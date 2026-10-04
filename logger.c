/*
 * logger.c
 * ------------------------------------------------------------------
 * File based audit logging using fopen / fputs / fclose.
 *
 * Log line format:
 *   2026-10-04 17:10 | janani | LOGIN_SUCCESS
 *   2026-10-04 17:20 | unknown | LOGIN_FAILED | reason=bad password
 */

#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENT_NAME 24

static char g_log_path[512] = LOG_DEFAULT_FILE;

/* Event name table indexed by EventType. */
static const char *const EVENT_NAMES[] = {
    "SYSTEM_START",
    "SYSTEM_EXIT",
    "REGISTER_SUCCESS",
    "REGISTER_FAILED",
    "LOGIN_SUCCESS",
    "LOGIN_FAILED",
    "ACCOUNT_LOCKED",
    "ACCOUNT_UNLOCKED",
    "LOGOUT",
    "SESSION_TIMEOUT",
    "ACCESS_GRANTED",
    "ACCESS_DENIED",
    "PASSWORD_CHANGED",
    "PASSWORD_RESET",
    "USER_ADDED",
    "USER_DELETED",
    "ROLE_CHANGED",
    "LOG_VIEWED"
};

int logger_init(const char *path)
{
    FILE *fp;

    utils_copy_str(g_log_path, sizeof g_log_path,
                   (path && *path) ? path : LOG_DEFAULT_FILE);

    /* Create the file if it does not exist yet. */
    fp = fopen(g_log_path, "a");
    if (!fp) {
        return -1;
    }
    fclose(fp);
    return 0;
}

const char *logger_path(void)
{
    return g_log_path;
}

const char *logger_event_name(EventType event)
{
    size_t count = sizeof(EVENT_NAMES) / sizeof(EVENT_NAMES[0]);

    if ((size_t)event >= count) {
        return "UNKNOWN_EVENT";
    }
    return EVENT_NAMES[event];
}

int logger_write(const char *actor, EventType event, const char *detail)
{
    FILE *fp;
    char stamp[32];

    fp = fopen(g_log_path, "a");
    if (!fp) {
        fprintf(stderr, "[audit] cannot open %s for writing\n", g_log_path);
        return -1;
    }

    utils_timestamp_minute(stamp, sizeof stamp);
    fprintf(fp, "%s | %s | %s", stamp,
            (actor && *actor) ? actor : "unknown",
            logger_event_name(event));
    if (detail && *detail) {
        fprintf(fp, " | %s", detail);
    }
    fputs("\n", fp);

    fclose(fp);
    return 0;
}

void logger_view(int tail)
{
    FILE *fp;
    char line[512];
    long index = 0;
    long first_to_print = 1;
    long total;

    if (tail <= 0) {
        utils_print_subheader("COMPLETE AUDIT LOG");
    } else {
        utils_print_subheader("RECENT AUTHENTICATION LOG");
    }

    fp = fopen(g_log_path, "r");
    if (!fp) {
        printf("\nNo audit log found at '%s'.\n\n", g_log_path);
        return;
    }

    total = utils_count_lines(g_log_path);
    if (tail > 0 && total > tail) {
        first_to_print = total - tail + 1;
    }

    printf("\n");
    while (fgets(line, sizeof line, fp)) {
        utils_trim(line);
        index++;
        if (index >= first_to_print) {
            printf("%4ld | %s\n", index, line);
        }
    }
    fclose(fp);

    if (total == 0) {
        printf("  (the audit log is empty)\n");
    }

    printf("\nShowing %ld of %ld entries from %s\n\n",
           (tail > 0 && total > tail) ? tail : total, total, g_log_path);
    logger_write("admin", EV_LOG_VIEWED, NULL);
}

long logger_total_events(void)
{
    return utils_count_lines(g_log_path);
}