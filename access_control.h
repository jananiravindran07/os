/*
 * access_control.h
 * ------------------------------------------------------------------
 * Role based access control (RBAC).
 *
 * Every protected resource of the system is mapped to the roles that
 * are allowed to use it. A user receives exactly the permissions of
 * their role and nothing more (least privilege).
 */

#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <stdbool.h>

#include "user.h"

/* ---------------------------------------------------------------- */
/* Protected resources                                              */
/* ---------------------------------------------------------------- */
typedef enum {
    RES_PUBLIC_INFO = 0,     /* public notices                 */
    RES_VIEW_PROFILE,        /* own profile                   */
    RES_CHANGE_PASSWORD,     /* own password                  */
    RES_NORMAL_RESOURCE,     /* normal application resource   */
    RES_USER_MANAGEMENT,     /* list / add users              */
    RES_DELETE_USER,         /* remove accounts               */
    RES_VIEW_LOGS,           /* audit log review              */
    RES_SYSTEM_SETTINGS,     /* system configuration          */
    RES_COUNT
} Resource;

/* ---------------------------------------------------------------- */
/* API                                                              */
/* ---------------------------------------------------------------- */

/* Name of a resource, e.g. "USER MANAGEMENT". */
const char *access_resource_name(Resource resource);

/* Role required for a resource, e.g. "ADMIN". */
const char *access_required_role(Resource resource);

/* Table lookup: is this role allowed to use the resource? */
bool access_allowed(Role role, Resource resource);

/* Full permission matrix printed as a table. */
void access_print_matrix(void);

/* Authorization check with user feedback and audit logging.
 * Prints ACCESS GRANTED / ACCESS DENIED and returns the decision. */
bool access_require(const char *actor, Role role, Resource resource);

/* Simulated payload of a protected resource. */
void access_show_resource(Resource resource);

/* Interactive "Access Resource" screen available to every role.
 * The permission matrix decides what the caller may open. */
void access_resource_picker(void);

#endif /* ACCESS_CONTROL_H */