/*
 * admin.c
 * ------------------------------------------------------------------
 * User management and system supervision for the ADMIN role.
 */

#include "admin.h"
#include "access_control.h"
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

static void load_users(UserList *list)
{
    memset(list, 0, sizeof(*list));
    (void)db_load(list);
}

/* Guards every administrative action. */
static bool require_admin(Resource resource)
{
    if (!session_is_active()) {
        printf("\nNo active session - please log in first.\n");
        return false;
    }
    if (!access_require(session_username(), session_role(), resource)) {
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------- */
/* 1. View all users                                                */
/* ---------------------------------------------------------------- */

void admin_view_users(void)
{
    UserList users;

    if (!require_admin(RES_USER_MANAGEMENT)) {
        return;
    }

    load_users(&users);
    printf("\n          USER MANAGEMENT - ALL ACCOUNTS\n\n");
    db_print_all(&users);

    printf("Role distribution:\n");
    printf("  Administrators : %d\n", db_count_role(&users, ROLE_ADMIN));
    printf("  Users          : %d\n", db_count_role(&users, ROLE_USER));
    printf("  Guests         : %d\n", db_count_role(&users, ROLE_GUEST));
    printf("\nDatabase file : %s\n", db_path());

    db_free(&users);
}

/* ---------------------------------------------------------------- */
/* 2. Add user                                                      */
/* ---------------------------------------------------------------- */

void admin_add_user(void)
{
    UserList users;
    User record;
    User *existing;
    char username[MAX_USERNAME];
    char full_name[MAX_FULL_NAME];
    char password[MAX_PASSWORD];
    char detail[160];
    Role role = ROLE_USER;

    if (!require_admin(RES_USER_MANAGEMENT)) {
        return;
    }

    utils_print_subheader("ADD NEW USER");
    printf("An administrator may assign any role, including ADMIN.\n");

    for (;;) {
        if (!utils_read_line("Username  : ", username, sizeof username)) {
            printf("\nCancelled.\n");
            return;
        }
        utils_str_tolower(username);
        if (!utils_is_valid_username(username)) {
            printf("Invalid username (3-31 chars, start with a letter, a-z 0-9 _ . -).\n");
            continue;
        }
        break;
    }

    utils_read_line("Full name : ", full_name, sizeof full_name);

    if (!auth_prompt_role(true, &role)) {
        printf("\nCancelled.\n");
        return;
    }

    auth_print_password_policy();

    /* The password policy is identical for admin created accounts. */
    if (auth_read_new_password(password, sizeof password) != 0) {
        printf("\nCancelled.\n");
        return;
    }

    memset(&record, 0, sizeof record);
    utils_copy_str(record.username, sizeof record.username, username);
    utils_copy_str(record.full_name, sizeof record.full_name,
                   full_name[0] ? full_name : username);
    record.role = role;
    record.status = ACCOUNT_ACTIVE;
    record.failed_attempts = 0;

    if (auth_hash_password(&record, password) != 0) {
        printf("\nUnable to hash the password.\n");
        password_wipe(password, sizeof password);
        return;
    }

    load_users(&users);
    existing = db_find(&users, username);
    if (existing != NULL) {
        printf("\nThe account '%s' already exists.\n", username);
        db_free(&users);
        password_wipe(password, sizeof password);
        return;
    }
    if (db_add(&users, &record) != 0 || db_save(&users) != 0) {
        printf("\nUnable to add the account (check write permissions on %s).\n",
               db_path());
        db_free(&users);
        password_wipe(password, sizeof password);
        return;
    }
    db_free(&users);

    printf("\nUser '%s' created with role %s.\n", record.username, role_name(record.role));
    snprintf(detail, sizeof detail, "target=%s role=%s", record.username, role_name(role));
    logger_write(session_username(), EV_USER_ADDED, detail);

    password_wipe(password, sizeof password);
}

/* ---------------------------------------------------------------- */
/* 3. Delete user                                                    */
/* ---------------------------------------------------------------- */

void admin_delete_user(void)
{
    UserList users;
    User *victim;
    char username[MAX_USERNAME];
    char detail[160];
    long total;

    if (!require_admin(RES_DELETE_USER)) {
        return;
    }

    utils_print_subheader("DELETE USER");
    printf("This permanently removes an account from %s.\n\n", db_path());

    if (!utils_read_line("Username to delete: ", username, sizeof username)) {
        printf("\nCancelled.\n");
        return;
    }
    utils_str_tolower(username);

    load_users(&users);
    victim = db_find(&users, username);
    if (victim == NULL) {
        printf("\nNo account named '%s' exists.\n", username);
        db_free(&users);
        return;
    }

    printf("\nAccount to remove:\n");
    user_print_profile(victim);

    /* Never allow the last administrator to be deleted or demoted. */
    if (victim->role == ROLE_ADMIN && db_count_role(&users, ROLE_ADMIN) <= 1) {
        printf("This is the only administrator account - it cannot be deleted.\n");
        db_free(&users);
        return;
    }
    if (utils_strcasecmp(victim->username, session_username()) == 0) {
        printf("You cannot delete the account you are logged in with.\n");
        db_free(&users);
        return;
    }

    if (!utils_confirm("\nConfirm deletion? [y/n]: ")) {
        printf("Deletion cancelled.\n");
        db_free(&users);
        return;
    }

    if (db_remove(&users, username) != 0) {
        printf("\nUnable to remove the record.\n");
        db_free(&users);
        return;
    }
    total = (long)users.count;
    if (db_save(&users) != 0) {
        printf("\nRecord removed in memory but writing %s failed.\n", db_path());
        db_free(&users);
        return;
    }
    db_free(&users);

    printf("\nAccount '%s' deleted (%ld accounts remain).\n", username, total);
    snprintf(detail, sizeof detail, "target=%s", username);
    logger_write(session_username(), EV_USER_DELETED, detail);
}

/* ---------------------------------------------------------------- */
/* 4. Change user role                                              */
/* ---------------------------------------------------------------- */

void admin_change_user_role(void)
{
    UserList users;
    User *target;
    char username[MAX_USERNAME];
    char detail[160];
    Role new_role;

    if (!require_admin(RES_USER_MANAGEMENT)) {
        return;
    }

    utils_print_subheader("CHANGE USER ROLE");

    if (!utils_read_line("Username: ", username, sizeof username)) {
        printf("\nCancelled.\n");
        return;
    }
    utils_str_tolower(username);

    load_users(&users);
    target = db_find(&users, username);
    if (target == NULL) {
        printf("\nNo account named '%s' exists.\n", username);
        db_free(&users);
        return;
    }

    printf("\nCurrent role of '%s' : %s\n", target->username, role_name(target->role));
    if (!auth_prompt_role(true, &new_role)) {
        db_free(&users);
        printf("\nCancelled.\n");
        return;
    }
    if (new_role == target->role) {
        printf("\nThe role is unchanged.\n");
        db_free(&users);
        return;
    }
    if (target->role == ROLE_ADMIN && db_count_role(&users, ROLE_ADMIN) <= 1) {
        printf("The only administrator cannot be demoted.\n");
        db_free(&users);
        return;
    }

    {
        char old_role_name[16];
        utils_copy_str(old_role_name, sizeof old_role_name, role_name(target->role));
        target->role = new_role;
        if (db_save(&users) != 0) {
            printf("\nUnable to write %s.\n", db_path());
            db_free(&users);
            return;
        }
        snprintf(detail, sizeof detail, "target=%s %s->%s", target->username,
                 old_role_name, role_name(new_role));
    }
    db_free(&users);

    printf("\nRole of '%s' changed to %s.\n", username, role_name(new_role));
    printf("Note: a role change takes effect at the user's next login.\n");
    logger_write(session_username(), EV_ROLE_CHANGED, detail);
}

/* ---------------------------------------------------------------- */
/* 5. Unlock account                                                */
/* ---------------------------------------------------------------- */

void admin_unlock_account(void)
{
    UserList users;
    User *target;
    char username[MAX_USERNAME];
    char detail[160];
    long locked = 0;
    size_t i;

    if (!require_admin(RES_USER_MANAGEMENT)) {
        return;
    }

    utils_print_subheader("UNLOCK ACCOUNT");

    load_users(&users);
    for (i = 0; i < users.count; i++) {
        if (users.items[i].status == ACCOUNT_LOCKED) {
            locked++;
        }
    }

    if (locked == 0) {
        printf("\nNo accounts are currently locked.\n");
    } else {
        printf("\nLocked accounts:\n");
        for (i = 0; i < users.count; i++) {
            if (users.items[i].status == ACCOUNT_LOCKED) {
                user_print_table_row(&users.items[i]);
            }
        }
    }

    if (!utils_read_line("\nUsername to unlock: ", username, sizeof username)) {
        db_free(&users);
        printf("\nCancelled.\n");
        return;
    }
    utils_str_tolower(username);

    target = db_find(&users, username);
    if (target == NULL) {
        printf("\nNo account named '%s' exists.\n", username);
        db_free(&users);
        return;
    }
    if (target->status != ACCOUNT_LOCKED && target->failed_attempts == 0) {
        printf("\nThe account '%s' is already active with no failed attempts.\n",
               target->username);
        db_free(&users);
        return;
    }

    target->status = ACCOUNT_ACTIVE;
    target->failed_attempts = 0;
    if (db_save(&users) != 0) {
        printf("\nUnable to write %s.\n", db_path());
        db_free(&users);
        return;
    }
    db_free(&users);

    printf("\n==========================================\n");
    printf("        ACCOUNT UNLOCKED\n");
    printf("==========================================\n\n");
    utils_print_kv("Account", target->username);
    utils_print_kv("Status", account_status_name(target->status));
    printf("Failed login attempts reset to 0.\n");

    snprintf(detail, sizeof detail, "target=%s", target->username);
    logger_write(session_username(), EV_ACCOUNT_UNLOCKED, detail);
}

/* ---------------------------------------------------------------- */
/* 6. View authentication logs                                      */
/* ---------------------------------------------------------------- */

void admin_view_logs(void)
{
    int tail = 0;

    if (!require_admin(RES_VIEW_LOGS)) {
        return;
    }

    utils_print_subheader("AUTHENTICATION LOGS");
    printf("Total audit entries: %ld\n", logger_total_events());
    printf("  (press Enter to show all, or type a number to show the last N)\n");

    if (!utils_read_int("Entries to show: ", 0, 10000, &tail)) {
        tail = 0;
    }
    logger_view(tail);
}

/* ---------------------------------------------------------------- */
/* 7. Protected resources                                           */
/* ---------------------------------------------------------------- */

void admin_protected_resources(void)
{
    if (!require_admin(RES_SYSTEM_SETTINGS)) {
        return;
    }

    utils_print_subheader("PROTECTED SYSTEM RESOURCES");

    access_print_matrix();
    access_show_resource(RES_USER_MANAGEMENT);
    access_show_resource(RES_SYSTEM_SETTINGS);
}

/* ---------------------------------------------------------------- */
/* Dashboard                                                        */
/* ---------------------------------------------------------------- */

void admin_menu(void)
{
    int choice = 0;

    if (!session_is_active()) {
        printf("No active session.\n");
        return;
    }

    for (;;) {
        utils_print_header("ADMIN DASHBOARD");
        printf("Welcome, %s\n\n", session_display_name());
        printf("1. View All Users\n");
        printf("2. Add User\n");
        printf("3. Delete User\n");
        printf("4. Change User Role\n");
        printf("5. Unlock Account\n");
        printf("6. View Authentication Logs\n");
        printf("7. View Protected Resources\n");
        printf("8. Access Resource (RBAC demo)\n");
        printf("9. View My Profile\n");
        printf("0. Logout\n");

        if (!utils_read_int("Enter your choice: ", 0, 9, &choice)) {
            if (utils_input_ended()) {
                printf("\nInput stream closed - leaving the dashboard.\n");
                return;
            }
            printf("Invalid choice. Please enter a number from 0 to 9.\n");
            utils_pause(NULL);
            continue;
        }

        /* Reject a command entered after the session went idle. */
        if (session_check_and_reap()) {
            return;
        }
        session_touch();

        switch (choice) {
        case 1:
            admin_view_users();
            break;
        case 2:
            admin_add_user();
            break;
        case 3:
            admin_delete_user();
            break;
        case 4:
            admin_change_user_role();
            break;
        case 5:
            admin_unlock_account();
            break;
        case 6:
            admin_view_logs();
            break;
        case 7:
            admin_protected_resources();
            break;
        case 8:
            access_resource_picker();
            break;
        case 9:
            user_view_own_profile();
            break;
        case 0:
        default:
            return;
        }
        utils_pause(NULL);
    }
}