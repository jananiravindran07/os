/*
 * auth.c
 * ------------------------------------------------------------------
 * Credential handling: registration, login, logout, password change
 * and password recovery.
 */

#include "auth.h"
#include "database.h"
#include "logger.h"
#include "password.h"
#include "session.h"
#include "user.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- */
/* Shared helpers                                                   */
/* ---------------------------------------------------------------- */

static void load_users(UserList *list)
{
    memset(list, 0, sizeof(*list));
    (void)db_load(list);
}

void auth_print_password_policy(void)
{
    printf("Password policy:\n");
    printf("  - at least %d characters\n", PASSWORD_MIN_LENGTH);
    printf("  - at least one uppercase letter\n");
    printf("  - at least one lowercase letter\n");
    printf("  - at least one number\n");
    printf("  - preferably one special character (!@#$%%...)\n");
    printf("  - not a commonly used password\n");
}

static void report_strength(const char *password)
{
    PwReport report = password_analyze(password);

    printf("\nPassword analysis\n");
    printf("  Length        : %d\n", report.length);
    printf("  Uppercase     : %d\n", report.upper);
    printf("  Lowercase     : %d\n", report.lower);
    printf("  Numbers       : %d\n", report.digit);
    printf("  Special chars : %d\n", report.special);
    password_strength_bar(&report);
}

/* Reads and confirms a new password that satisfies the policy.
 * Returns 0 and copies the password into out on success. */
int auth_read_new_password(char *out, size_t out_size)
{
    char first[MAX_PASSWORD];
    char second[MAX_PASSWORD];
    char reason[160];
    int attempt = 0;

    for (attempt = 0; attempt < 3; attempt++) {
        utils_read_password("New password : ", first, sizeof first);
        utils_read_password("Confirm       : ", second, sizeof second);

        if (strcmp(first, second) != 0) {
            printf("\nPasswords do not match. Please try again.\n");
            continue;
        }
        if (!password_validate(first, reason, sizeof reason)) {
            printf("\nWeak password: %s\n", reason);
            auth_print_password_policy();
            continue;
        }

        report_strength(first);
        utils_copy_str(out, out_size, first);
        password_wipe(first, sizeof first);
        password_wipe(second, sizeof second);
        return 0;
    }

    printf("\nToo many unsuccessful attempts.\n");
    password_wipe(first, sizeof first);
    password_wipe(second, sizeof second);
    return -1;
}

/* Hashes a password into the target user record (random salt). */
int auth_hash_password(User *user, const char *password)
{
    char salt[PASSWORD_SALT_BYTES * 2 + 1];

    if (password_generate_salt(salt, sizeof salt) != 0) {
        return -1;
    }
    return password_hash(password, salt, PASSWORD_ITERATIONS,
                         user->password_hash, sizeof user->password_hash);
}

/* Hashes a security answer into the target user record. */
int auth_hash_security_answer(User *user, const char *answer)
{
    char normalised[MAX_PASSWORD];
    char salt[PASSWORD_SALT_BYTES * 2 + 1];
    int rc;

    password_normalise_answer(answer, normalised, sizeof normalised);
    if (password_generate_salt(salt, sizeof salt) != 0) {
        return -1;
    }
    rc = password_hash(normalised, salt, PASSWORD_ITERATIONS,
                       user->security_answer_hash,
                       sizeof user->security_answer_hash);
    password_wipe(normalised, sizeof normalised);
    return rc;
}

/* Security question presets for the registration / recovery flow. */
static const char *const SECURITY_QUESTIONS[] = {
    "Name of your first school?",
    "Name of your first pet?",
    "City you were born in?",
    "Favourite movie title?",
    "Name of your best friend?"
};
#define QUESTION_COUNT ((int)(sizeof(SECURITY_QUESTIONS) / sizeof(SECURITY_QUESTIONS[0])))

static void prompt_security_question(char *out, size_t out_size)
{
    char buffer[MAX_QUESTION];
    char choice_prompt[48];
    int choice = 0;
    int i;

    snprintf(choice_prompt, sizeof choice_prompt, "Choice [1-%d]: ", QUESTION_COUNT + 1);

    for (;;) {
        printf("\nChoose a security question for password recovery:\n");
        for (i = 0; i < QUESTION_COUNT; i++) {
            printf("  %d. %s\n", i + 1, SECURITY_QUESTIONS[i]);
        }
        printf("  %d. Enter my own question\n", QUESTION_COUNT + 1);

        if (!utils_read_int(choice_prompt, 1, QUESTION_COUNT + 1, &choice)) {
            printf("Please enter a valid choice.\n");
            continue;
        }
        if (choice <= QUESTION_COUNT) {
            utils_copy_str(out, out_size, SECURITY_QUESTIONS[choice - 1]);
            return;
        }
        if (utils_read_line("Your question : ", buffer, sizeof buffer) &&
            strlen(buffer) >= 5) {
            utils_copy_str(out, out_size, buffer);
            return;
        }
        printf("The question must be at least 5 characters long.\n");
    }
}

const char *auth_result_message(AuthResult result)
{
    switch (result) {
    case AUTH_OK:              return "Login successful";
    case AUTH_NO_USER:         return "Invalid username or password";
    case AUTH_BAD_CREDENTIALS: return "Invalid username or password";
    case AUTH_LOCKED:          return "Account locked - contact the administrator";
    case AUTH_CANCELLED:       return "Login cancelled";
    default:                   return "Unknown result";
    }
}

bool auth_prompt_role(bool allow_admin, Role *out)
{
    char buffer[16];
    char choice_prompt[48];
    Role role;

    if (!out) {
        return false;
    }

    snprintf(choice_prompt, sizeof choice_prompt, "Choice [1-%d]: ", allow_admin ? 3 : 2);

    for (;;) {
        printf("\nSelect a role:\n");
        printf("  1. USER  (normal application access)\n");
        printf("  2. GUEST (public information only)\n");
        if (allow_admin) {
            printf("  3. ADMIN (full system access)\n");
        }
        printf("Note: %s\n", allow_admin
                   ? "administrators may grant any role."
                   : "self registration cannot create administrators (least privilege)");

        if (!utils_read_line(choice_prompt, buffer, sizeof buffer)) {
            return false;
        }

        if (strcmp(buffer, "1") == 0) {
            role = ROLE_USER;
        } else if (strcmp(buffer, "2") == 0) {
            role = ROLE_GUEST;
        } else if (strcmp(buffer, "3") == 0 && allow_admin) {
            role = ROLE_ADMIN;
        } else {
            printf("Invalid choice.\n");
            continue;
        }

        printf("Selected role: %s\n", role_name(role));
        if (utils_confirm("Confirm role? [y/n]: ")) {
            *out = role;
            return true;
        }
    }
}

/* ---------------------------------------------------------------- */
/* Registration                                                     */
/* ---------------------------------------------------------------- */

int auth_register(void)
{
    UserList users;
    User record;
    char username[MAX_USERNAME];
    char full_name[MAX_FULL_NAME];
    char question[MAX_QUESTION];
    char password[MAX_PASSWORD];
    char answer[MAX_PASSWORD];
    char answer_confirm[MAX_PASSWORD];
    char detail[160];
    Role role = ROLE_USER;

    utils_print_header("USER REGISTRATION");
    printf("Create a new account to access the system.\n");

    /* ---- username ---- */
    for (;;) {
        if (!utils_read_line("Username    : ", username, sizeof username)) {
            printf("\nRegistration cancelled (end of input).\n");
            return -1;
        }
        utils_str_tolower(username);
        if (!utils_is_valid_username(username)) {
            printf("Invalid username.\n");
            printf("  - 3 to 31 characters\n");
            printf("  - start with a lowercase letter\n");
            printf("  - use a-z, 0-9, '_', '.', '-'\n");
            continue;
        }
        break;
    }

    load_users(&users);
    if (db_find(&users, username) != NULL) {
        printf("\nThe username '%s' is already taken.\n", username);
        logger_write(username, EV_REGISTER_FAILED, "reason=username taken");
        db_free(&users);
        return -1;
    }
    db_free(&users);

    /* ---- full name ---- */
    utils_read_line("Full name   : ", full_name, sizeof full_name);
    if (full_name[0] == '\0') {
        snprintf(full_name, sizeof full_name, "%s", username);
    }

    /* ---- role ---- */
    if (!auth_prompt_role(false, &role)) {
        printf("\nRegistration cancelled.\n");
        return -1;
    }

    /* ---- password ---- */
    auth_print_password_policy();
    if (auth_read_new_password(password, sizeof password) != 0) {
        printf("\nRegistration cancelled.\n");
        return -1;
    }

    /* ---- security question / answer ---- */
    prompt_security_question(question, sizeof question);

    for (;;) {
        if (!utils_read_line("Security answer : ", answer, sizeof answer)) {
            printf("\nRegistration cancelled.\n");
            password_wipe(password, sizeof password);
            return -1;
        }
        utils_read_line("Confirm answer  : ", answer_confirm, sizeof answer_confirm);
        if (strlen(answer) < 3) {
            printf("The answer must be at least 3 characters long.\n");
            continue;
        }
        if (strcmp(answer, answer_confirm) != 0) {
            printf("Answers do not match.\n");
            continue;
        }
        break;
    }

    /* ---- create the record ---- */
    memset(&record, 0, sizeof record);
    utils_copy_str(record.username, sizeof record.username, username);
    utils_copy_str(record.full_name, sizeof record.full_name, full_name);
    utils_copy_str(record.security_question, sizeof record.security_question, question);
    record.role = role;
    record.status = ACCOUNT_ACTIVE;
    record.failed_attempts = 0;

    if (auth_hash_password(&record, password) != 0) {
        printf("\nUnable to hash the password.\n");
        password_wipe(password, sizeof password);
        return -1;
    }
    if (auth_hash_security_answer(&record, answer) != 0) {
        printf("\nUnable to hash the security answer.\n");
        password_wipe(password, sizeof password);
        return -1;
    }

    /* (the policy check inside auth_read_new_password() passed already) */

    load_users(&users);
    if (db_add(&users, &record) != 0) {
        printf("\nRegistration failed - the account already exists.\n");
        logger_write(username, EV_REGISTER_FAILED, "reason=duplicate account");
        db_free(&users);
        password_wipe(password, sizeof password);
        return -1;
    }
    if (db_save(&users) != 0) {
        printf("\nRegistration failed - unable to write %s.\n", db_path());
        logger_write(username, EV_REGISTER_FAILED, "reason=database write error");
        db_free(&users);
        password_wipe(password, sizeof password);
        return -1;
    }
    db_free(&users);

    printf("\n==========================================\n");
    printf("        REGISTRATION SUCCESSFUL\n");
    printf("==========================================\n\n");
    utils_print_kv("Username", record.username);
    utils_print_kv("Role", role_name(record.role));
    utils_print_kv("Status", account_status_name(record.status));
    utils_print_kv("Password", "stored as a salted PBKDF2-SHA256 hash");
    printf("\nYou can now log in with your new account.\n");

    logger_write(record.username, EV_REGISTER_SUCCESS,
                 (snprintf(detail, sizeof detail, "role=%s", role_name(record.role)),
                  detail));

    password_wipe(password, sizeof password);
    password_wipe(answer, sizeof answer);
    password_wipe(answer_confirm, sizeof answer_confirm);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Login                                                            */
/* ---------------------------------------------------------------- */

static void print_locked_alert(const User *user)
{
    printf("\n==========================================\n");
    printf("        SECURITY ALERT\n");
    printf("==========================================\n\n");
    printf("Maximum login attempts exceeded.\n\n");
    utils_print_kv("Account", user->username);
    utils_print_kv("Status", account_status_name(user->status));
    printf("\nPlease contact the administrator to unlock this account.\n\n");
    printf("==========================================\n");
}

int auth_login(void)
{
    UserList users;
    User *record;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    AuthResult result = AUTH_CANCELLED;
    char detail[160];

    utils_print_header("LOGIN");

    if (!utils_read_line("Username: ", username, sizeof username)) {
        printf("\n%s\n", auth_result_message(AUTH_CANCELLED));
        return -1;
    }
    utils_str_tolower(username);

    utils_read_password("Password: ", password, sizeof password);

    printf("\nAuthenticating...\n");

    load_users(&users);
    record = db_find(&users, username);

    /* ---- unknown user ---- */
    if (record == NULL) {
        printf("\nInvalid username or password.\n");
        logger_write(username, EV_LOGIN_FAILED, "reason=unknown username");
        result = AUTH_NO_USER;
        goto finish;
    }

    /* ---- locked account ---- */
    if (record->status == ACCOUNT_LOCKED) {
        print_locked_alert(record);
        logger_write(record->username, EV_LOGIN_FAILED, "reason=account locked");
        result = AUTH_LOCKED;
        goto finish;
    }

    /* ---- verify password ---- */
    if (password_verify(password, record->password_hash) != 1) {
        record->failed_attempts++;

        if (record->failed_attempts >= MAX_FAILED_ATTEMPTS) {
            record->status = ACCOUNT_LOCKED;
            if (db_save(&users) != 0) {
                printf("Warning: unable to persist the account status.\n");
            }
            print_locked_alert(record);
            logger_write(record->username, EV_LOGIN_FAILED, "reason=bad password");
            logger_write(record->username, EV_ACCOUNT_LOCKED,
                         (snprintf(detail, sizeof detail, "attempts=%d",
                                   record->failed_attempts), detail));
            result = AUTH_LOCKED;
            goto finish;
        }

        if (db_save(&users) != 0) {
            printf("Warning: unable to persist the attempt counter.\n");
        }
        printf("\nInvalid username or password.\n\n");
        printf("Failed Attempts: %d/%d\n",
               record->failed_attempts, MAX_FAILED_ATTEMPTS);
        logger_write(record->username, EV_LOGIN_FAILED, "reason=bad password");
        result = AUTH_BAD_CREDENTIALS;
        goto finish;
    }

    /* ---- authenticated ---- */
    record->failed_attempts = 0;
    record->status = ACCOUNT_ACTIVE;
    if (db_save(&users) != 0) {
        printf("Warning: unable to reset the attempt counter.\n");
    }

    session_start(record);
    snprintf(detail, sizeof detail, "role=%s", role_name(record->role));
    logger_write(record->username, EV_LOGIN_SUCCESS, detail);

    printf("\n==========================================\n");
    printf("        LOGIN SUCCESSFUL\n");
    printf("==========================================\n\n");
    printf("Welcome, %s\n", user_display_name(record));
    session_print_summary();
    result = AUTH_OK;

finish:
    password_wipe(password, sizeof password);
    db_free(&users);
    if (result == AUTH_OK) {
        return 0;
    }
    return -1;
}

void auth_logout(void)
{
    char username[MAX_USERNAME];

    if (!session_is_active()) {
        return;
    }
    utils_copy_str(username, sizeof username, session_username());

    printf("\n==========================================\n");
    printf("              LOGOUT\n");
    printf("==========================================\n\n");
    printf("Goodbye, %s. Your session has been terminated.\n",
           session_display_name());
    printf("Session ID %s closed.\n", session_session_id());

    session_end(SESSION_END_LOGOUT, username);
}

/* ---------------------------------------------------------------- */
/* Forgot password                                                  */
/* ---------------------------------------------------------------- */

void auth_forgot_password(void)
{
    UserList users;
    User *record;
    char username[MAX_USERNAME];
    char answer[MAX_PASSWORD];
    char new_password[MAX_PASSWORD];

    utils_print_header("FORGOT PASSWORD");
    printf("Recover access using your security question.\n\n");

    if (!utils_read_line("Username: ", username, sizeof username)) {
        printf("\n%s\n", auth_result_message(AUTH_CANCELLED));
        return;
    }
    utils_str_tolower(username);

    load_users(&users);
    record = db_find(&users, username);

    if (record == NULL) {
        /* Do not disclose whether the account exists. */
        printf("\nNo account matching that username was found.\n");
        logger_write(username, EV_LOGIN_FAILED, "reason=password recovery, unknown user");
        db_free(&users);
        return;
    }

    if (record->security_question[0] == '\0' ||
        utils_strcasecmp(record->security_question, "-") == 0 ||
        record->security_answer_hash[0] == '\0') {
        printf("\nNo security question is configured for '%s'.\n", record->username);
        printf("Please contact the administrator.\n");
        db_free(&users);
        return;
    }

    printf("\nSecurity question:\n  %s\n", record->security_question);
    utils_read_password("Your answer : ", answer, sizeof answer);

    {
        char normalised[MAX_PASSWORD];
        password_normalise_answer(answer, normalised, sizeof normalised);
        if (password_verify(normalised, record->security_answer_hash) != 1) {
            printf("\nIncorrect answer. Password was not changed.\n");
            logger_write(record->username, EV_LOGIN_FAILED,
                         "reason=password recovery, bad answer");
            password_wipe(answer, sizeof answer);
            db_free(&users);
            return;
        }
        password_wipe(normalised, sizeof normalised);
    }

    printf("\nAnswer accepted. Choose a new password.\n");
    auth_print_password_policy();
    if (auth_read_new_password(new_password, sizeof new_password) != 0) {
        password_wipe(answer, sizeof answer);
        db_free(&users);
        return;
    }

    if (auth_hash_password(record, new_password) != 0) {
        printf("\nUnable to hash the new password.\n");
    } else {
        /* A successful recovery also clears a previous lockout. */
        record->failed_attempts = 0;
        record->status = ACCOUNT_ACTIVE;

        if (db_save(&users) != 0) {
            printf("\nPassword changed but unable to write %s.\n", db_path());
        } else {
            printf("\n==========================================\n");
            printf("        PASSWORD RESET SUCCESSFUL\n");
            printf("==========================================\n\n");
            printf("Account   : %s\n", record->username);
            printf("Status    : %s\n", account_status_name(record->status));
            printf("You can now log in with your new password.\n");
            logger_write(record->username, EV_PASSWORD_RESET, "method=security question");
        }
    }

    password_wipe(answer, sizeof answer);
    password_wipe(new_password, sizeof new_password);
    db_free(&users);
}

/* ---------------------------------------------------------------- */
/* Change password                                                  */
/* ---------------------------------------------------------------- */

int auth_change_password(void)
{
    UserList users;
    User *record;
    char current[MAX_PASSWORD];
    char new_password[MAX_PASSWORD];
    char detail[160];

    if (!session_is_active()) {
        printf("You must be logged in to change your password.\n");
        return -1;
    }

    utils_print_header("CHANGE PASSWORD");

    utils_read_password("Current password : ", current, sizeof current);

    load_users(&users);
    record = db_find(&users, session_username());
    if (record == NULL) {
        printf("\nYour account could not be found in %s.\n", db_path());
        password_wipe(current, sizeof current);
        db_free(&users);
        return -1;
    }

    if (password_verify(current, record->password_hash) != 1) {
        printf("\nCurrent password is incorrect.\n");
        logger_write(record->username, EV_PASSWORD_CHANGED, "result=denied (bad current password)");
        password_wipe(current, sizeof current);
        db_free(&users);
        return -1;
    }
    password_wipe(current, sizeof current);

    auth_print_password_policy();
    if (auth_read_new_password(new_password, sizeof new_password) != 0) {
        db_free(&users);
        return -1;
    }

    if (password_verify(new_password, record->password_hash) == 1) {
        printf("\nThe new password must be different from the current one.\n");
        password_wipe(new_password, sizeof new_password);
        db_free(&users);
        return -1;
    }

    if (auth_hash_password(record, new_password) != 0) {
        printf("\nUnable to hash the new password.\n");
        password_wipe(new_password, sizeof new_password);
        db_free(&users);
        return -1;
    }

    if (db_save(&users) != 0) {
        printf("\nUnable to write %s - the password was not changed.\n", db_path());
        password_wipe(new_password, sizeof new_password);
        db_free(&users);
        return -1;
    }

    printf("\nPassword updated successfully.\n");
    snprintf(detail, sizeof detail, "by=%s", record->username);
    logger_write(record->username, EV_PASSWORD_CHANGED, detail);

    password_wipe(new_password, sizeof new_password);
    db_free(&users);
    return 0;
}