/*
 * utils.h
 * ------------------------------------------------------------------
 * Common helper functions used across the User Authentication and
 * Access Control System.
 *
 * Demonstrates: strings, character handling, file checks, time
 * functions and portable console input (including hidden password
 * input on both Windows and POSIX systems).
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stddef.h>

/* Maximum length of a line read from the console. */
#define UTILS_LINE_MAX 256

/* ---------------------------------------------------------------- */
/* Console / screen helpers                                         */
/* ---------------------------------------------------------------- */
void utils_clear_screen(void);
void utils_print_header(const char *title);
void utils_print_subheader(const char *title);
void utils_print_line(void);
void utils_print_kv(const char *key, const char *value);
void utils_print_kv_int(const char *key, long value);
void utils_pause(const char *message);

/* ---------------------------------------------------------------- */
/* Input helpers                                                    */
/* ---------------------------------------------------------------- */

/* Reads a visible line. Returns 1 on success, 0 on EOF/error. */
int utils_read_line(const char *prompt, char *buf, size_t size);

/* Reads a line without echoing it (used for passwords). */
void utils_read_password(const char *prompt, char *buf, size_t size);

/* True when stdin is an interactive console. Password entry falls back
 * to plain line reading when the input is redirected (scripts, pipes). */
int utils_stdin_is_tty(void);

/* True once the end of the input stream has been reached (EOF or closed
 * pipe). Menu loops use this to terminate instead of spinning forever. */
int utils_input_ended(void);

/* Reads an integer inside [min, max]. Returns 1 on success. */
int utils_read_int(const char *prompt, int min, int max, int *out);

/* Yes/No question. Returns 1 for yes, 0 for no. */
int utils_confirm(const char *prompt);

/* ---------------------------------------------------------------- */
/* String helpers                                                   */
/* ---------------------------------------------------------------- */
void utils_trim(char *s);
void utils_str_toupper(char *s);
void utils_str_tolower(char *s);
int  utils_strcasecmp(const char *a, const char *b);
int  utils_is_valid_username(const char *s);
void utils_copy_str(char *dst, size_t dst_size, const char *src);

/* Fills out with 2*bytes hexadecimal characters. Returns 0 on success. */
int  utils_random_hex(char *out, size_t bytes);

/* ---------------------------------------------------------------- */
/* Time helpers                                                     */
/* ---------------------------------------------------------------- */
void utils_timestamp(char *out, size_t size);        /* YYYY-MM-DD HH:MM:SS */
void utils_timestamp_minute(char *out, size_t size);  /* YYYY-MM-DD HH:MM   */

/* ---------------------------------------------------------------- */
/* File helpers                                                     */
/* ---------------------------------------------------------------- */
int  utils_file_exists(const char *path);
long utils_count_lines(const char *path);

#endif /* UTILS_H */