/*
 * auth.h
 * ------------------------------------------------------------------
 * Authentication: registration, login, logout, password change and
 * the forgot-password recovery flow.
 *
 * Authentication flow implemented in auth_login():
 *
 *   read username -> read password -> search users.txt
 *        -> username exists? -> verify password hash
 *        -> account ACTIVE?  -> create session -> load role
 *        -> access control
 */

#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stddef.h>

#include "user.h"

/* MAX_FAILED_ATTEMPTS (the lockout threshold) is defined in user.h. */

/* Outcome of an authentication attempt. */
typedef enum {
    AUTH_OK = 0,          /* authenticated, session created       */
    AUTH_NO_USER,         /* unknown username                      */
    AUTH_BAD_CREDENTIALS, /* wrong password                        */
    AUTH_LOCKED,          /* account locked                        */
    AUTH_CANCELLED        /* user aborted the form                 */
} AuthResult;

/* ---------------------------------------------------------------- */
/* API                                                              */
/* ---------------------------------------------------------------- */

/* Self service registration. Returns 0 on success. */
int  auth_register(void);

/* Interactive login. Returns 0 when a session was created. */
int  auth_login(void);

/* Ends the active session (if any) and returns to the main menu. */
void auth_logout(void);

/* Security question based password reset. */
void auth_forgot_password(void);

/* Change password of the signed in user. */
int  auth_change_password(void);

/* Human readable result message. */
const char *auth_result_message(AuthResult result);

/* Role selection helper used by registration and admin tools. */
bool auth_prompt_role(bool allow_admin, Role *out);

/* Shared helpers (also used by the administrator tools). */

/* Prints the password policy of the system. */
void auth_print_password_policy(void);

/* Reads and confirms a new password that satisfies the policy.
 * Returns 0 on success and copies it into out. */
int  auth_read_new_password(char *out, size_t out_size);

/* Hashes a password (random salt) into the record's password_hash. */
int  auth_hash_password(User *user, const char *password);

/* Hashes a security answer into the record's security_answer_hash. */
int  auth_hash_security_answer(User *user, const char *answer);

#endif /* AUTH_H */