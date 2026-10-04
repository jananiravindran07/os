/*
 * session.c
 * ------------------------------------------------------------------
 * Implementation of the session state machine.
 */

#include "session.h"
#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static Session g_session;

/* Process id, used to make the session identifier unique. */
static long current_pid(void)
{
#ifdef _WIN32
    return (long)GetCurrentProcessId();
#else
    return (long)getpid();
#endif
}

void session_init(void)
{
    memset(&g_session, 0, sizeof g_session);
    g_session.role = ROLE_COUNT;       /* "no session" */
}

void session_start(const User *user)
{
    if (!user) {
        return;
    }

    memset(&g_session, 0, sizeof g_session);
    utils_copy_str(g_session.username, sizeof g_session.username, user->username);
    utils_copy_str(g_session.full_name, sizeof g_session.full_name,
                   user->full_name[0] ? user->full_name : user->username);
    g_session.role = user->role;
    g_session.active = true;
    g_session.started_at = time(NULL);
    g_session.last_activity = g_session.started_at;

    /* A pseudo session identifier (shown in the dashboard / audit log). */
    snprintf(g_session.session_id, sizeof g_session.session_id, "SESS-%08lX-%04lX",
             (unsigned long)g_session.started_at, (unsigned long)current_pid() & 0xFFFFUL);
}

void session_end(SessionEndReason reason, const char *actor)
{
    EventType event = EV_LOGOUT;
    char username[MAX_USERNAME];

    if (!g_session.active) {
        return;
    }

    utils_copy_str(username, sizeof username, g_session.username);

    switch (reason) {
    case SESSION_END_TIMEOUT:
        event = EV_SESSION_TIMEOUT;
        break;
    case SESSION_END_SHUTDOWN:
    case SESSION_END_REPLACED:
    case SESSION_END_LOGOUT:
    default:
        event = EV_LOGOUT;
        break;
    }

    logger_write((actor && *actor) ? actor : username, event,
                 (reason == SESSION_END_TIMEOUT) ? "reason=idle timeout" : NULL);

    memset(&g_session, 0, sizeof g_session);
    g_session.role = ROLE_COUNT;
}

bool session_is_active(void)
{
    return g_session.active;
}

const char *session_username(void)
{
    return g_session.active ? g_session.username : "none";
}

Role session_role(void)
{
    return g_session.active ? g_session.role : ROLE_COUNT;
}

const char *session_session_id(void)
{
    return g_session.active ? g_session.session_id : "--------";
}

const char *session_display_name(void)
{
    return g_session.active ? g_session.full_name : "Guest";
}

void session_touch(void)
{
    if (g_session.active) {
        g_session.last_activity = time(NULL);
    }
}

long session_idle_seconds(void)
{
    if (!g_session.active) {
        return 0;
    }
    return (long)(time(NULL) - g_session.last_activity);
}

bool session_has_expired(void)
{
    if (!g_session.active) {
        return false;
    }
    return session_idle_seconds() >= SESSION_TIMEOUT_SECONDS;
}

bool session_check_and_reap(void)
{
    char username[MAX_USERNAME];
    long idle;

    if (!g_session.active) {
        return false;
    }
    if (!session_has_expired()) {
        return false;
    }

    utils_copy_str(username, sizeof username, g_session.username);
    idle = session_idle_seconds();

    printf("\n==========================================\n");
    printf("        SESSION EXPIRED\n");
    printf("==========================================\n\n");
    printf("User       : %s\n", username);
    printf("Idle time  : %ld seconds (limit %d seconds)\n",
           idle, SESSION_TIMEOUT_SECONDS);
    printf("Reason     : inactivity timeout\n\n");
    printf("Please log in again to continue.\n");

    session_end(SESSION_END_TIMEOUT, username);
    return true;
}

void session_print_summary(void)
{
    utils_print_line();
    utils_print_kv("Username", session_username());
    utils_print_kv("Role", role_name(session_role()));
    utils_print_kv("Session", session_is_active() ? "ACTIVE" : "CLOSED");
    utils_print_kv("Session ID", session_session_id());
    utils_print_kv_int("Idle seconds", session_idle_seconds());
    utils_print_kv_int("Timeout", SESSION_TIMEOUT_SECONDS);
    utils_print_line();
}