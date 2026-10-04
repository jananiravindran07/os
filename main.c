/*
 * main.c
 * ------------------------------------------------------------------
 * User Authentication and Access Control System
 * Operating Systems mini project - command line interface.
 *
 * Start-up sequence:
 *   1. initialise the audit log
 *   2. initialise the user database (creates users.txt if missing)
 *   3. initialise the session subsystem
 *   4. show the main menu and dispatch to the dashboards
 *
 * Main menu  -> authentication -> session -> access control -> audit log
 */

#include "access_control.h"
#include "admin.h"
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
/* Dashboards                                                       */
/* ---------------------------------------------------------------- */

static void user_dashboard(void)
{
    int choice = 0;

    for (;;) {
        utils_print_header("USER DASHBOARD");
        printf("Welcome, %s\n\n", session_display_name());
        printf("1. View Profile\n");
        printf("2. Change Password\n");
        printf("3. Access Resource\n");
        printf("4. View Permission Matrix\n");
        printf("5. Session Status\n");
        printf("0. Logout\n");

        if (!utils_read_int("Enter your choice: ", 0, 5, &choice)) {
            if (utils_input_ended()) {
                printf("\nInput stream closed - leaving the dashboard.\n");
                return;
            }
            printf("Invalid choice. Please enter a number from 0 to 5.\n");
            utils_pause(NULL);
            continue;
        }

        /* The timeout is evaluated after the input has been read so that
         * a command entered after a long idle period is rejected instead
         * of being executed with an expired session. */
        if (session_check_and_reap()) {
            return;
        }
        session_touch();

        switch (choice) {
        case 1:
            user_view_own_profile();
            break;
        case 2:
            (void)auth_change_password();
            break;
        case 3:
            access_resource_picker();
            break;
        case 4:
            access_print_matrix();
            break;
        case 5:
            session_print_summary();
            break;
        case 6:
        default:
            return;
        }
        utils_pause(NULL);
    }
}

static void guest_dashboard(void)
{
    int choice = 0;

    for (;;) {
        utils_print_header("GUEST DASHBOARD");
        printf("Welcome, %s (public access only)\n\n", session_display_name());
        printf("1. View Profile\n");
        printf("2. Access Resource\n");
        printf("3. View Permission Matrix\n");
        printf("0. Logout\n");

        if (!utils_read_int("Enter your choice: ", 0, 3, &choice)) {
            if (utils_input_ended()) {
                printf("\nInput stream closed - leaving the dashboard.\n");
                return;
            }
            printf("Invalid choice. Please enter a number from 0 to 3.\n");
            utils_pause(NULL);
            continue;
        }

        if (session_check_and_reap()) {
            return;
        }
        session_touch();

        switch (choice) {
        case 1:
            user_view_own_profile();
            break;
        case 2:
            access_resource_picker();
            break;
        case 3:
            access_print_matrix();
            break;
        case 4:
        default:
            return;
        }
        utils_pause(NULL);
    }
}

/* Dispatches the signed in user to the dashboard of their role. */
static void run_dashboard(void)
{
    switch (session_role()) {
    case ROLE_ADMIN:
        admin_menu();
        break;
    case ROLE_USER:
        user_dashboard();
        break;
    case ROLE_GUEST:
        guest_dashboard();
        break;
    default:
        printf("Unknown role - returning to the main menu.\n");
        session_end(SESSION_END_SHUTDOWN, session_username());
        break;
    }
}

/* ---------------------------------------------------------------- */
/* Banner and help                                                  */
/* ---------------------------------------------------------------- */

static void print_banner(void)
{
    printf("\n");
    printf("==========================================\n");
    printf("   USER AUTHENTICATION AND\n");
    printf("      ACCESS CONTROL SYSTEM\n");
    printf("   Operating Systems Mini Project - C\n");
    printf("==========================================\n");
}

static void print_help(void)
{
    char now[32];

    utils_print_header("HELP / SECURITY NOTES");
    utils_timestamp(now, sizeof now);
    printf("System time    : %s\n", now);
    printf("Authentication\n");
    printf("  Passwords are never stored as plain text.\n");
    printf("  Hashing  : PBKDF2-HMAC-SHA256, %d iterations, 128 bit salt\n",
           PASSWORD_ITERATIONS);
    printf("  Policy   : minimum %d chars, mixed case, digits\n",
           PASSWORD_MIN_LENGTH);
    printf("  Lockout  : %d failed attempts locks the account\n",
           MAX_FAILED_ATTEMPTS);
    printf("  Session  : idle timeout after %d seconds\n",
           SESSION_TIMEOUT_SECONDS);
    printf("\nFiles\n");
    printf("  User database : %s\n", db_path());
    printf("  Audit log     : %s\n", logger_path());
    printf("\nDefault administrator account (created on first run):\n");
    printf("  username : admin      password : Admin@123\n");
    printf("  Change this password immediately after the first login.\n");
}

/* ---------------------------------------------------------------- */
/* Main menu                                                        */
/* ---------------------------------------------------------------- */

static int main_menu(void)
{
    int choice = 0;

    for (;;) {
        print_banner();
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Forgot Password\n");
        printf("4. Help / Security Notes\n");
        printf("5. Exit\n\n");

        if (!utils_read_int("Enter your choice: ", 0, 5, &choice)) {
            if (utils_input_ended()) {
                printf("\nInput stream closed - exiting the program.\n");
                return 0;
            }
            printf("\nInvalid choice. Please enter a number from 1 to 5.\n");
            utils_pause(NULL);
            continue;
        }

        switch (choice) {
        case 1:
            (void)auth_register();
            utils_pause(NULL);
            break;

        case 2:
            if (auth_login() == 0) {
                run_dashboard();
                if (session_is_active()) {
                    auth_logout();
                    utils_pause(NULL);
                }
            } else {
                utils_pause(NULL);
            }
            break;

        case 3:
            auth_forgot_password();
            utils_pause(NULL);
            break;

        case 4:
            print_help();
            utils_pause(NULL);
            break;

        case 5:
        default:
            printf("\nThank you for using the system. Goodbye!\n\n");
            return 0;
        }
    }
}

int main(void)
{
    int seeded;

    printf("Initialising the user authentication system...\n");

    /* 1. Audit log */
    if (logger_init(LOG_DEFAULT_FILE) != 0) {
        fprintf(stderr, "Fatal: cannot open the audit log file '%s'.\n",
                LOG_DEFAULT_FILE);
        return EXIT_FAILURE;
    }

    /* 2. User database (creates the default administrator when empty) */
    db_set_path(DB_DEFAULT_FILE);
    seeded = db_init();
    if (seeded < 0) {
        fprintf(stderr, "Fatal: cannot initialise the user database '%s'.\n",
                DB_DEFAULT_FILE);
        return EXIT_FAILURE;
    }
    if (seeded > 0) {
        printf("Created a new user database '%s'.\n", db_path());
        printf("Default administrator: admin / Admin@123 - change it now.\n\n");
    }

    /* 3. Session subsystem */
    session_init();

    logger_write("system", EV_SYSTEM_START, NULL);

    main_menu();

    /* Clean shutdown: end any session that is still open. */
    if (session_is_active()) {
        session_end(SESSION_END_SHUTDOWN, session_username());
    }
    logger_write("system", EV_SYSTEM_EXIT, NULL);

    return EXIT_SUCCESS;
}