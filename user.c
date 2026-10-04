/*
 * user.c
 * ------------------------------------------------------------------
 * Presentation of user information: role names, account status and
 * profile / table output.
 */

#include "user.h"
#include "database.h"
#include "logger.h"
#include "session.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Role name lookup table - an array of string pointers. */
static const char *const ROLE_NAMES[ROLE_COUNT] = {
    "ADMIN",
    "USER",
    "GUEST"
};

const char *role_name(Role role)
{
    if ((int)role < 0 || (int)role >= (int)ROLE_COUNT) {
        return "INVALID";
    }
    return ROLE_NAMES[role];
}

Role role_from_name(const char *name)
{
    int i;

    if (!name) {
        return ROLE_COUNT;
    }
    for (i = 0; i < (int)ROLE_COUNT; i++) {
        if (utils_strcasecmp(name, ROLE_NAMES[i]) == 0) {
            return (Role)i;
        }
    }
    return ROLE_COUNT;
}

const char *account_status_name(AccountStatus status)
{
    switch (status) {
    case ACCOUNT_ACTIVE: return "ACTIVE";
    case ACCOUNT_LOCKED: return "LOCKED";
    default:             return "UNKNOWN";
    }
}

const char *user_display_name(const User *user)
{
    static char fallback[MAX_FULL_NAME];
    size_t i;

    if (!user) {
        return "unknown";
    }
    if (user->full_name[0] != '\0') {
        return user->full_name;
    }

    /* Capitalise the username: "janani" -> "Janani". */
    utils_copy_str(fallback, sizeof fallback, user->username);
    for (i = 0; fallback[i] != '\0'; i++) {
        fallback[i] = (char)toupper((unsigned char)fallback[i]);
    }
    return fallback;
}

void user_print_profile(const User *user)
{
    if (!user) {
        return;
    }

    utils_print_line();
    printf("            USER PROFILE\n");
    utils_print_line();
    utils_print_kv("Username", user->username);
    utils_print_kv("Full Name", user->full_name[0] ? user->full_name : "(not provided)");
    utils_print_kv("Role", role_name(user->role));
    utils_print_kv("Status", account_status_name(user->status));
    utils_print_kv_int("Failed Logins", user->failed_attempts);
    utils_print_kv_int("Lockout at", MAX_FAILED_ATTEMPTS);
    utils_print_kv("Password", "stored as PBKDF2-SHA256 hash");
    utils_print_line();
}

void user_print_table_header(void)
{
    printf("\n%-14s %-8s %-8s %-7s %-24s\n",
           "USERNAME", "ROLE", "STATUS", "FAILED", "FULL NAME");
    printf("--------------------------------------------------\n");
}

void user_print_table_row(const User *user)
{
    if (!user) {
        return;
    }
    printf("%-14s %-8s %-8s %-7d %-24s\n",
           user->username,
           role_name(user->role),
           account_status_name(user->status),
           user->failed_attempts,
           user->full_name[0] ? user->full_name : "-");
}

void user_print_table_footer(int total)
{
    printf("--------------------------------------------------\n");
    printf("Total accounts: %d\n\n", total);
}

void user_view_own_profile(void)
{
    UserList users;
    User *record;

    if (!session_is_active()) {
        printf("\nNo active session - please log in first.\n");
        return;
    }

    printf("\n");

    memset(&users, 0, sizeof users);
    if (db_load(&users) < 0) {
        printf("\nUnable to read %s.\n", db_path());
        return;
    }

    record = db_find(&users, session_username());
    if (record == NULL) {
        printf("\nYour account could not be found in %s.\n", db_path());
        db_free(&users);
        return;
    }

    user_print_profile(record);

    printf("Security question : %s\n",
           (record->security_question[0] &&
            utils_strcasecmp(record->security_question, "-") != 0)
               ? record->security_question
               : "(not configured)");
    printf("Audit trail       : %s\n", logger_path());

    db_free(&users);
    logger_write(session_username(), EV_ACCESS_GRANTED, "VIEW PROFILE");
}