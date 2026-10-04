/*
 * logger.h
 * ------------------------------------------------------------------
 * Audit logging module.
 *
 * Every security relevant event (login, logout, lockout, access
 * attempt, password change, ...) is appended to audit.log so that an
 * administrator can review the history of the system.
 */

#ifndef LOGGER_H
#define LOGGER_H

/* Default audit log file. */
#define LOG_DEFAULT_FILE "audit.log"

/* ---------------------------------------------------------------- */
/* Event types                                                      */
/* ---------------------------------------------------------------- */
typedef enum {
    EV_SYSTEM_START = 0,
    EV_SYSTEM_EXIT,
    EV_REGISTER_SUCCESS,
    EV_REGISTER_FAILED,
    EV_LOGIN_SUCCESS,
    EV_LOGIN_FAILED,
    EV_ACCOUNT_LOCKED,
    EV_ACCOUNT_UNLOCKED,
    EV_LOGOUT,
    EV_SESSION_TIMEOUT,
    EV_ACCESS_GRANTED,
    EV_ACCESS_DENIED,
    EV_PASSWORD_CHANGED,
    EV_PASSWORD_RESET,
    EV_USER_ADDED,
    EV_USER_DELETED,
    EV_ROLE_CHANGED,
    EV_LOG_VIEWED
} EventType;

/* ---------------------------------------------------------------- */
/* Logging API                                                      */
/* ---------------------------------------------------------------- */

/* Opens the audit log in append mode. Call once at start-up. */
int         logger_init(const char *path);

/* Currently used audit log path. */
const char *logger_path(void);

/* Human readable event name, e.g. "LOGIN_SUCCESS". */
const char *logger_event_name(EventType event);

/* Appends one audit entry. Returns 0 on success. */
int logger_write(const char *actor, EventType event, const char *detail);

/* Prints the most recent events (tail > 0), or everything if tail <= 0. */
void logger_view(int tail);

/* Total number of entries currently stored in the log. */
long logger_total_events(void);

#endif /* LOGGER_H */