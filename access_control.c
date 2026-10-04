/*
 * access_control.c
 * ------------------------------------------------------------------
 * The permission matrix and the authorization checks.
 *
 *                 ADMIN     USER      GUEST
 *   ---------------------------------------------
 *   Public Information  YES   YES       YES
 *   View Profile        YES   YES       YES
 *   Change Password     YES   YES       NO
 *   Normal Resource     YES   YES       YES
 *   User Management     YES   NO        NO
 *   Delete User         YES   NO        NO
 *   View Logs           YES   NO        NO
 *   System Settings     YES   NO        NO
 */

#include "access_control.h"
#include "logger.h"
#include "password.h"
#include "session.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *required_role;
    bool allowed[ROLE_COUNT];
} PermissionRow;

/* The access control list. Index order matches Role. */
static const PermissionRow MATRIX[RES_COUNT] = {
    { "PUBLIC INFORMATION", "ANY",      { true,  true,  true  } },
    { "VIEW PROFILE",        "ANY",      { true,  true,  true  } },
    { "CHANGE PASSWORD",     "ANY",      { true,  true,  false } },
    { "NORMAL RESOURCE",     "ANY",      { true,  true,  true  } },
    { "USER MANAGEMENT",     "ADMIN",    { true,  false, false } },
    { "DELETE USER",         "ADMIN",    { true,  false, false } },
    { "VIEW LOGS",           "ADMIN",    { true,  false, false } },
    { "SYSTEM SETTINGS",     "ADMIN",    { true,  false, false } }
};

const char *access_resource_name(Resource resource)
{
    if ((int)resource < 0 || (int)resource >= (int)RES_COUNT) {
        return "UNKNOWN RESOURCE";
    }
    return MATRIX[resource].name;
}

const char *access_required_role(Resource resource)
{
    if ((int)resource < 0 || (int)resource >= (int)RES_COUNT) {
        return "ADMIN";
    }
    return MATRIX[resource].required_role;
}

bool access_allowed(Role role, Resource resource)
{
    if ((int)role < 0 || (int)role >= (int)ROLE_COUNT) {
        return false;
    }
    if ((int)resource < 0 || (int)resource >= (int)RES_COUNT) {
        return false;
    }
    return MATRIX[resource].allowed[role];
}

void access_print_matrix(void)
{
    int r;
    int c;

    printf("\n%-22s %-9s %-9s %-9s\n", "RESOURCE", "ADMIN", "USER", "GUEST");
    printf("-------------------------------------------------\n");
    for (r = 0; r < (int)RES_COUNT; r++) {
        printf("%-22s", MATRIX[r].name);
        for (c = 0; c < (int)ROLE_COUNT; c++) {
            printf(" %-9s", MATRIX[r].allowed[c] ? "YES" : "NO");
        }
        printf("\n");
    }
    printf("-------------------------------------------------\n");
    printf("YES = permitted   NO = denied (least privilege)\n\n");
}

bool access_require(const char *actor, Role role, Resource resource)
{
    bool allowed = access_allowed(role, resource);
    const char *name = access_resource_name(resource);

    utils_print_line();
    printf("User            : %s\n", actor ? actor : "unknown");
    printf("Role            : %s\n", role_name(role));
    printf("Resource        : %s\n", name);
    utils_print_line();
    printf("Checking permissions...\n\n");

    if (allowed) {
        printf("ACCESS GRANTED\n\n");
        printf("The role '%s' is permitted to use %s.\n",
               role_name(role), name);
        logger_write(actor, EV_ACCESS_GRANTED, name);
        return true;
    }

    printf("==========================================\n");
    printf("          ACCESS DENIED\n");
    printf("==========================================\n\n");
    printf("You do not have permission\n");
    printf("to access this resource.\n\n");
    printf("Required Role : %s\n", access_required_role(resource));
    printf("Your Role     : %s\n\n", role_name(role));
    printf("==========================================\n");

    logger_write(actor, EV_ACCESS_DENIED, name);
    return false;
}

void access_show_resource(Resource resource)
{
    printf("\n");
    utils_print_line();
    printf("      PROTECTED RESOURCE CONTENT\n");
    utils_print_line();

    switch (resource) {
    case RES_PUBLIC_INFO:
        printf("Public notice:\n");
        printf("  This Operating Systems project demonstrates\n");
        printf("  authentication, authorization and access control.\n");
        break;
    case RES_VIEW_PROFILE:
        printf("Profile records of the signed in user are visible.\n");
        break;
    case RES_NORMAL_RESOURCE:
        printf("Normal application data (read/write allowed).\n");
        break;
    case RES_USER_MANAGEMENT:
        printf("User management console unlocked:\n");
        printf("  - create accounts\n");
        printf("  - list accounts\n");
        printf("  - change roles\n");
        printf("  - unlock accounts\n");
        break;
    case RES_DELETE_USER:
        printf("Account removal tool unlocked.\n");
        break;
    case RES_VIEW_LOGS:
        printf("Audit log console unlocked.\n");
        break;
    case RES_SYSTEM_SETTINGS:
        printf("System configuration unlocked:\n");
        printf("  Password policy      : %d+ chars, mixed case, digits\n",
               PASSWORD_MIN_LENGTH);
        printf("  Max failed logins    : 3\n");
        printf("  Session timeout      : %d seconds\n", SESSION_TIMEOUT_SECONDS);
        printf("  Hashing algorithm    : PBKDF2-HMAC-SHA256\n");
        printf("  PBKDF2 iterations    : %d\n", PASSWORD_ITERATIONS);
        break;
    case RES_CHANGE_PASSWORD:
        printf("Password change form unlocked for your own account.\n");
        break;
    default:
        printf("(no content defined)\n");
        break;
    }

    utils_print_line();
}

void access_resource_picker(void)
{
    int choice = 0;
    Resource resource;
    int i;

    if (!session_is_active()) {
        printf("You must be logged in to access resources.\n");
        return;
    }

    utils_print_subheader("ACCESS RESOURCE");
    printf("User: %s\nRole: %s\n\n", session_username(), role_name(session_role()));
    printf("Available resources:\n");
    for (i = 0; i < (int)RES_COUNT; i++) {
        printf("  %d. %-22s (requires %s)\n", i + 1,
               access_resource_name((Resource)i),
               access_required_role((Resource)i));
    }
    printf("  0. Back\n");

    if (!utils_read_int("\nSelect a resource: ", 0, (int)RES_COUNT, &choice)) {
        printf("Invalid choice.\n");
        return;
    }
    if (choice == 0) {
        return;
    }

    resource = (Resource)(choice - 1);
    if (access_require(session_username(), session_role(), resource)) {
        access_show_resource(resource);
    }
}