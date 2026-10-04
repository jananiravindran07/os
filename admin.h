/*
 * admin.h
 * ------------------------------------------------------------------
 * Administrator operations: user management, account unlocking, role
 * management, audit log review and access to protected resources.
 *
 * Every function in this module performs an access control check
 * first (role must be ADMIN), so it is safe to call from anywhere.
 */

#ifndef ADMIN_H
#define ADMIN_H

/* Interactive administrator dashboard. */
void admin_menu(void);

/* Individual operations (each verifies the ADMIN role itself). */
void admin_view_users(void);
void admin_add_user(void);
void admin_delete_user(void);
void admin_change_user_role(void);
void admin_unlock_account(void);
void admin_view_logs(void);
void admin_protected_resources(void);

#endif /* ADMIN_H */