/*
 * database.h
 * ------------------------------------------------------------------
 * File based user database (users.txt).
 *
 * Record format (fields separated by '|'):
 *
 *   username|password_hash|ROLE|failed_attempts|STATUS|full name|question|answer hash
 *
 * The first five fields are the mandatory fields. The remaining
 * optional fields support the profile screen and the "Forgot
 * Password" security question flow.
 *
 * This module demonstrates file handling (fopen/fgets/fprintf/fclose),
 * dynamic memory management (realloc growth) and record search.
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>

#include "user.h"

#define DB_DEFAULT_FILE "users.txt"

/* Growing array of user records. */
typedef struct {
    User  *items;         /* dynamically allocated array of User      */
    size_t count;         /* number of records in use                */
    size_t capacity;      /* allocated slots                         */
} UserList;

/* ---------------------------------------------------------------- */
/* Setup                                                            */
/* ---------------------------------------------------------------- */

/* Selects the database file used by the module (default users.txt). */
void db_set_path(const char *path);
const char *db_path(void);

/* Creates the database if it is missing or empty and seeds the default
 * administrator account.
 * Returns 1 when the default administrator was created, 0 when the
 * database already held records, -1 on error. */
int  db_init(void);

/* Releases the memory held by a UserList. */
void db_free(UserList *list);

/* ---------------------------------------------------------------- */
/* CRUD operations                                                  */
/* ---------------------------------------------------------------- */

/* Loads every record into the list (resizing it as needed).
 * Returns the number of records loaded, or -1 on error. */
long db_load(UserList *list);

/* Writes the list back to the database file (atomically via a
 * temporary file and rename). Returns 0 on success. */
int  db_save(const UserList *list);

/* Adds a record. The list takes ownership of a copy of the record. */
int  db_add(UserList *list, const User *user);

/* Removes a record by username. Returns 0 when removed, -1 otherwise. */
int  db_remove(UserList *list, const char *username);

/* Case insensitive lookup. Returns NULL when not found. */
User *db_find(UserList *list, const char *username);

/* Number of accounts holding the given role (used to protect the
 * last remaining administrator). */
int  db_count_role(const UserList *list, Role role);

/* Prints every record as a table. */
void db_print_all(const UserList *list);

#endif /* DATABASE_H */