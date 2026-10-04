/*
 * user.h
 * ------------------------------------------------------------------
 * User information structures, role definitions and profile display.
 *
 * Demonstrates: structures, enumerations, arrays of characters and
 * passing structures by pointer.
 */

#ifndef USER_H
#define USER_H

/* ---------------------------------------------------------------- */
/* Limits (shared by every module that stores user data)            */
/* ---------------------------------------------------------------- */
#define MAX_USERNAME     32
#define MAX_FULL_NAME    64
#define MAX_PASSWORD     64
#define MAX_HASH        192
#define MAX_QUESTION     96
#define MAX_USERS       256

/* Consecutive failed logins that lock an account. */
#define MAX_FAILED_ATTEMPTS 3

/* ---------------------------------------------------------------- */
/* Roles (used by the access control matrix)                        */
/* ---------------------------------------------------------------- */
typedef enum {
    ROLE_ADMIN = 0,
    ROLE_USER  = 1,
    ROLE_GUEST = 2,
    ROLE_COUNT = 3          /* number of roles - also used as "invalid" */
} Role;

/* ---------------------------------------------------------------- */
/* Account status                                                   */
/* ---------------------------------------------------------------- */
typedef enum {
    ACCOUNT_ACTIVE = 0,
    ACCOUNT_LOCKED = 1
} AccountStatus;

/* ---------------------------------------------------------------- */
/* User record - one line of users.txt                              */
/* ---------------------------------------------------------------- */
typedef struct {
    char username[MAX_USERNAME];
    char password_hash[MAX_HASH];       /* PBKDF2-HMAC-SHA256 string       */
    Role role;
    int  failed_attempts;               /* consecutive failed logins       */
    AccountStatus status;
    char full_name[MAX_FULL_NAME];
    char security_question[MAX_QUESTION];
    char security_answer_hash[MAX_HASH];
} User;

/* ---------------------------------------------------------------- */
/* Helper functions                                                 */
/* ---------------------------------------------------------------- */
const char *role_name(Role role);
Role        role_from_name(const char *name);
const char *account_status_name(AccountStatus status);

/* Returns full name if stored, otherwise a capitalised username. */
const char *user_display_name(const User *user);

/* Prints a detailed profile of a single user. */
void user_print_profile(const User *user);

/* Table helpers used by the "View All Users" screens. */
void user_print_table_header(void);
void user_print_table_row(const User *user);
void user_print_table_footer(int total);

/* Loads the record of the signed in user from the database and prints
 * its profile. Also lists that user's most recent audit entries. */
void user_view_own_profile(void);

#endif /* USER_H */