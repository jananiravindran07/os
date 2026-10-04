/*
 * session.h
 * ------------------------------------------------------------------
 * Session management.
 *
 * The session keeps the state of the currently logged in user:
 * username, role, session id, login time and last activity time.
 * A session is terminated by logout, by idle timeout or when the
 * program exits.
 */

#ifndef SESSION_H
#define SESSION_H

#include <time.h>
#include <stdbool.h>

#include "user.h"

/* Idle timeout in seconds (2 minutes). */
#define SESSION_TIMEOUT_SECONDS 120

/* Reason codes for ending a session. */
typedef enum {
    SESSION_END_LOGOUT = 0,
    SESSION_END_TIMEOUT,
    SESSION_END_SHUTDOWN,
    SESSION_END_REPLACED
} SessionEndReason;

typedef struct {
    bool active;
    char username[MAX_USERNAME];
    char full_name[MAX_FULL_NAME];
    Role role;
    char session_id[33];
    time_t started_at;
    time_t last_activity;
} Session;

/* ---------------------------------------------------------------- */
/* API                                                              */
/* ---------------------------------------------------------------- */

/* Prepares the session subsystem (called once at start-up). */
void session_init(void);

/* Creates an active session for a successfully authenticated user. */
void session_start(const User *user);

/* Ends the current session and writes the corresponding audit entry. */
void session_end(SessionEndReason reason, const char *actor);

/* True while a user is logged in. */
bool session_is_active(void);

/* Accessors for the active session. */
const char *session_username(void);
Role        session_role(void);
const char *session_session_id(void);
const char *session_display_name(void);

/* Refreshes the idle timer (call after every user interaction). */
void session_touch(void);

/* Seconds elapsed since the last activity. */
long session_idle_seconds(void);

/* True when the session exceeded SESSION_TIMEOUT_SECONDS. */
bool session_has_expired(void);

/* Ends an expired session, prints the notice and logs it. */
bool session_check_and_reap(void);

/* Prints the "Username / Role / Session" summary box. */
void session_print_summary(void);

#endif /* SESSION_H */