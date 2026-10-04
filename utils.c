/*
 * utils.c
 * ------------------------------------------------------------------
 * Implementation of the shared helper routines.
 */

#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

/* Set as soon as the input stream is exhausted. */
static int g_input_ended = 0;

/* ---------------------------------------------------------------- */
/* Console helpers                                                  */
/* ---------------------------------------------------------------- */

void utils_clear_screen(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void utils_print_line(void)
{
    printf("------------------------------------------\n");
}

void utils_print_header(const char *title)
{
    const int width = 42;
    int len;
    int pad;

    if (!title) {
        title = "";
    }
    len = (int)strlen(title);
    pad = (len < width) ? (width - len) / 2 : 0;

    printf("\n==========================================\n");
    printf("%*s%s\n", pad, "", title);
    printf("==========================================\n\n");
}

void utils_print_subheader(const char *title)
{
    if (!title) {
        return;
    }
    printf("\n--- %s %.*s\n", title,
           (int)(38 - strlen(title) > 0 ? 38 - (int)strlen(title) : 0), "");
}

void utils_print_kv(const char *key, const char *value)
{
    printf("%-16s: %s\n", key, value ? value : "-");
}

void utils_print_kv_int(const char *key, long value)
{
    printf("%-16s: %ld\n", key, value);
}

void utils_pause(const char *message)
{
    char sink[16];

    printf("\n%s", message ? message : "Press Enter to continue...");
    fflush(stdout);
    if (fgets(sink, sizeof sink, stdin) == NULL) {
        clearerr(stdin);
    }
}

/* ---------------------------------------------------------------- */
/* Input helpers                                                    */
/* ---------------------------------------------------------------- */

int utils_read_line(const char *prompt, char *buf, size_t size)
{
    size_t len;

    if (!buf || size == 0) {
        return 0;
    }
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buf, (int)size, stdin) == NULL) {
        buf[0] = '\0';
        g_input_ended = 1;
        clearerr(stdin);
        return 0;                    /* EOF */
    }

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else if (len + 1 == size) {
        /* Line was longer than the buffer: drain the remainder. */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* discard */
        }
    }

    utils_trim(buf);
    return 1;
}

int utils_input_ended(void)
{
    return g_input_ended;
}

int utils_stdin_is_tty(void)
{
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;

    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!GetConsoleMode(handle, &mode)) {
        return 0;                    /* not a console (redirected input) */
    }
    return (mode & ENABLE_LINE_INPUT) ? 1 : 0;
#else
    return isatty(STDIN_FILENO) ? 1 : 0;
#endif
}

void utils_read_password(const char *prompt, char *buf, size_t size)
{
    size_t i = 0;

    if (!buf || size == 0) {
        return;
    }
    buf[0] = '\0';

    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    /* Non interactive input (pipes, scripts, redirected files):
     * there is nobody watching the screen, so read a normal line. */
    if (!utils_stdin_is_tty()) {
        if (fgets(buf, (int)size, stdin)) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
                buf[--len] = '\0';
            }
        } else {
            buf[0] = '\0';
            g_input_ended = 1;
            clearerr(stdin);
        }
        return;
    }

#ifdef _WIN32
    {
        int c;
        while ((c = _getch()) != '\r' && c != '\n' && c != EOF && c != -1) {
            if (c == '\b') {
                if (i > 0) {
                    i--;
                    printf("\b \b");
                    fflush(stdout);
                }
                continue;
            }
            if (c == 0 || c == 0xE0) {   /* arrow / function keys */
                (void)_getch();
                continue;
            }
            if (i + 1 < size && c >= 32 && c < 127) {
                buf[i++] = (char)c;
                putchar('*');
            }
        }
    }
#else
    {
        struct termios old_term;
        struct termios new_term;
        int have_term = 0;
        int c;

        if (tcgetattr(STDIN_FILENO, &old_term) == 0) {
            new_term = old_term;
            new_term.c_lflag &= (tcflag_t)~ECHO;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) == 0) {
                have_term = 1;
            }
        }

        while ((c = getchar()) != '\n' && c != EOF) {
            if (c == '\b' || c == 127) {
                if (i > 0) {
                    i--;
                    printf("\b \b");
                    fflush(stdout);
                }
                continue;
            }
            if (i + 1 < size && c >= 32 && c < 127) {
                buf[i++] = (char)c;
                putchar('*');
            }
        }

        if (have_term) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
        }
    }
#endif

    putchar('\n');
    buf[i] = '\0';
}

int utils_read_int(const char *prompt, int min, int max, int *out)
{
    char buf[UTILS_LINE_MAX];
    char *end = NULL;
    long value;

    if (!utils_read_line(prompt, buf, sizeof buf)) {
        return 0;
    }
    if (buf[0] == '\0') {
        return 0;
    }

    value = strtol(buf, &end, 10);
    if (end == buf || (end && *end != '\0')) {
        return 0;
    }
    if (value < min || value > max) {
        return 0;
    }
    if (out) {
        *out = (int)value;
    }
    return 1;
}

int utils_confirm(const char *prompt)
{
    char buf[16];

    for (;;) {
        if (!utils_read_line(prompt, buf, sizeof buf)) {
            return 0;
        }
        if (buf[0] == 'y' || buf[0] == 'Y') {
            return 1;
        }
        if (buf[0] == 'n' || buf[0] == 'N' || buf[0] == '\0') {
            return 0;
        }
        printf("Please answer 'y' or 'n'.\n");
    }
}

/* ---------------------------------------------------------------- */
/* String helpers                                                   */
/* ---------------------------------------------------------------- */

void utils_trim(char *s)
{
    size_t start = 0;
    size_t end;

    if (!s) {
        return;
    }
    end = strlen(s);
    while (s[start] != '\0' && isspace((unsigned char)s[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)s[end - 1])) {
        end--;
    }
    if (start > 0) {
        memmove(s, s + start, end - start);
    }
    s[end - start] = '\0';
}

void utils_str_toupper(char *s)
{
    if (!s) {
        return;
    }
    for (; *s != '\0'; s++) {
        *s = (char)toupper((unsigned char)*s);
    }
}

void utils_str_tolower(char *s)
{
    if (!s) {
        return;
    }
    for (; *s != '\0'; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

int utils_strcasecmp(const char *a, const char *b)
{
    if (!a || !b) {
        return (a == b) ? 0 : 1;
    }
    while (*a != '\0' && *b != '\0') {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) {
            return ca - cb;
        }
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int utils_is_valid_username(const char *s)
{
    size_t len;
    size_t i;

    if (!s) {
        return 0;
    }
    len = strlen(s);
    if (len < 3 || len >= 32) {
        return 0;
    }
    if (!islower((unsigned char)s[0])) {
        return 0;                    /* usernames start with a lowercase letter */
    }
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (!islower((unsigned char)c) && !isdigit((unsigned char)c) &&
            c != '_' && c != '.' && c != '-') {
            return 0;
        }
    }
    return 1;
}

void utils_copy_str(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int utils_random_hex(char *out, size_t bytes)
{
    static const char *hexdigits = "0123456789abcdef";
    static int seeded = 0;
    size_t i;

    if (!out || bytes == 0) {
        return -1;
    }

    if (!seeded) {
        unsigned long seed = (unsigned long)time(NULL);
#ifdef _WIN32
        seed ^= (unsigned long)clock() * 2654435761UL;
#else
        seed ^= (unsigned long)getpid() * 2246822519UL;
#endif
        seed ^= (unsigned long)(size_t)out;
        srand((unsigned int)seed);
        seeded = 1;
    }

    for (i = 0; i < bytes; i++) {
        unsigned char byte = (unsigned char)(rand() & 0xFF);
        /* Mix neighbouring byte for slightly better distribution. */
        byte = (unsigned char)(byte ^ (unsigned char)(rand() >> 8));
        out[i * 2] = hexdigits[(byte >> 4) & 0x0F];
        out[i * 2 + 1] = hexdigits[byte & 0x0F];
    }
    out[bytes * 2] = '\0';
    return 0;
}

/* ---------------------------------------------------------------- */
/* Time helpers                                                     */
/* ---------------------------------------------------------------- */

void utils_timestamp(char *out, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (!out || size == 0) {
        return;
    }
#ifdef _WIN32
    tm_now = *localtime(&now);
#else
    localtime_r(&now, &tm_now);
#endif
    strftime(out, size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

void utils_timestamp_minute(char *out, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_now;

    if (!out || size == 0) {
        return;
    }
#ifdef _WIN32
    tm_now = *localtime(&now);
#else
    localtime_r(&now, &tm_now);
#endif
    strftime(out, size, "%Y-%m-%d %H:%M", &tm_now);
}

/* ---------------------------------------------------------------- */
/* File helpers                                                     */
/* ---------------------------------------------------------------- */

int utils_file_exists(const char *path)
{
    FILE *fp;

    if (!path) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

long utils_count_lines(const char *path)
{
    FILE *fp;
    long count = 0;
    int c;
    int last = '\n';

    if (!path) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
            count++;
        }
        last = c;
    }
    if (last != '\n') {
        count++;                     /* last line without newline */
    }
    fclose(fp);
    return count;
}