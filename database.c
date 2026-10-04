/*
 * database.c
 * ------------------------------------------------------------------
 * Reading, writing and searching the users.txt database.
 */

#include "database.h"
#include "utils.h"
#include "password.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_MAX_FIELDS 8

static char g_db_path[512] = DB_DEFAULT_FILE;

/* ---------------------------------------------------------------- */
/* Path handling                                                    */
/* ---------------------------------------------------------------- */

void db_set_path(const char *path)
{
    utils_copy_str(g_db_path, sizeof g_db_path,
                   (path && *path) ? path : DB_DEFAULT_FILE);
}

const char *db_path(void)
{
    return g_db_path;
}

/* ---------------------------------------------------------------- */
/* Serialisation helpers                                            */
/* ---------------------------------------------------------------- */

/* Escapes nothing: fields must not contain '|'. */
static void split_fields(char *line, char *fields[], int max_fields)
{
    int count = 0;
    char *cursor = line;

    while (count < max_fields) {
        char *sep = strchr(cursor, '|');
        fields[count++] = cursor;
        if (!sep) {
            break;
        }
        *sep = '\0';
        cursor = sep + 1;
    }
    while (count < max_fields) {
        fields[count++] = NULL;
    }
}

static int parse_user_line(char *line, User *user)
{
    char *fields[DB_MAX_FIELDS];
    Role role;

    memset(user, 0, sizeof *user);

    if (!line || line[0] == '\0' || line[0] == '#') {
        return -1;
    }
    utils_trim(line);

    split_fields(line, fields, DB_MAX_FIELDS);
    if (!fields[0] || fields[0][0] == '\0') {
        return -1;
    }

    role = role_from_name(fields[2] ? fields[2] : "USER");
    if ((int)role >= (int)ROLE_COUNT) {
        role = ROLE_USER;              /* unknown role -> least privilege */
    }

    utils_copy_str(user->username, sizeof user->username, fields[0]);
    utils_copy_str(user->password_hash, sizeof user->password_hash,
                   fields[1] ? fields[1] : "");
    user->role = role;
    user->failed_attempts = (fields[3]) ? atoi(fields[3]) : 0;
    if (user->failed_attempts < 0) {
        user->failed_attempts = 0;
    }
    user->status = (fields[4] && utils_strcasecmp(fields[4], "LOCKED") == 0)
                       ? ACCOUNT_LOCKED
                       : ACCOUNT_ACTIVE;
    utils_copy_str(user->full_name, sizeof user->full_name,
                   (fields[5] && fields[5][0]) ? fields[5] : "-");
    utils_copy_str(user->security_question, sizeof user->security_question,
                   (fields[6] && fields[6][0]) ? fields[6] : "-");
    utils_copy_str(user->security_answer_hash, sizeof user->security_answer_hash,
                   (fields[7] && fields[7][0]) ? fields[7] : "-");

    /* Guard against values that do not fit their field. */
    if (strlen(fields[0]) >= sizeof user->username) {
        return -1;
    }
    return 0;
}

static int write_user_line(FILE *fp, const User *user)
{
    const char *name = (user->full_name[0] && utils_strcasecmp(user->full_name, "-") != 0)
                           ? user->full_name
                           : "-";
    const char *question =
        (user->security_question[0] && utils_strcasecmp(user->security_question, "-") != 0)
            ? user->security_question
            : "-";
    const char *answer =
        (user->security_answer_hash[0] &&
         utils_strcasecmp(user->security_answer_hash, "-") != 0)
            ? user->security_answer_hash
            : "-";

    if (fprintf(fp, "%s|%s|%s|%d|%s|%s|%s|%s\n",
                user->username,
                user->password_hash,
                role_name(user->role),
                user->failed_attempts,
                account_status_name(user->status),
                name, question, answer) < 0) {
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Dynamic list management                                          */
/* ---------------------------------------------------------------- */

static int list_reserve(UserList *list, size_t needed)
{
    size_t capacity;
    User *grown;

    if (list->items && needed <= list->capacity) {
        return 0;
    }

    capacity = list->capacity ? list->capacity : 8;
    while (capacity < needed) {
        capacity *= 2;
    }

    /* Dynamic memory: the array grows with realloc. */
    if (list->items) {
        grown = (User *)realloc(list->items, capacity * sizeof(User));
    } else {
        grown = (User *)malloc(capacity * sizeof(User));
    }
    if (!grown) {
        return -1;
    }

    list->items = grown;
    list->capacity = capacity;
    return 0;
}

void db_free(UserList *list)
{
    if (!list) {
        return;
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ---------------------------------------------------------------- */
/* Load / save                                                      */
/* ---------------------------------------------------------------- */

long db_load(UserList *list)
{
    FILE *fp;
    char line[1024];
    long loaded = 0;

    if (!list) {
        return -1;
    }

    list->count = 0;

    fp = fopen(g_db_path, "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof line, fp)) {
        User record;

        if (parse_user_line(line, &record) != 0) {
            continue;                   /* skip blank / malformed lines */
        }
        if (list_reserve(list, list->count + 1) != 0) {
            fclose(fp);
            return -1;
        }
        list->items[list->count++] = record;
        loaded++;
    }

    fclose(fp);
    return loaded;
}

int db_save(const UserList *list)
{
    FILE *fp;
    char temp_path[600];
    size_t i;

    if (!list) {
        return -1;
    }
    if (snprintf(temp_path, sizeof temp_path, "%s.tmp", g_db_path) < 0) {
        return -1;
    }

    fp = fopen(temp_path, "w");
    if (!fp) {
        return -1;
    }

    fputs("# users.txt - one record per line\n", fp);
    fputs("# username|password_hash|role|failed_attempts|status|full name|question|answer hash\n", fp);

    for (i = 0; i < list->count; i++) {
        if (write_user_line(fp, &list->items[i]) != 0) {
            fclose(fp);
            remove(temp_path);
            return -1;
        }
    }

    fclose(fp);

    /* Replace the live file with the freshly written one. */
    remove(g_db_path);
    if (rename(temp_path, g_db_path) != 0) {
        remove(temp_path);
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* CRUD                                                             */
/* ---------------------------------------------------------------- */

int db_add(UserList *list, const User *user)
{
    if (!list || !user || user->username[0] == '\0') {
        return -1;
    }
    if (db_find(list, user->username) != NULL) {
        return -1;                      /* duplicate */
    }
    if (list_reserve(list, list->count + 1) != 0) {
        return -1;
    }
    list->items[list->count++] = *user;
    return 0;
}

int db_remove(UserList *list, const char *username)
{
    size_t i;

    if (!list || !username) {
        return -1;
    }
    for (i = 0; i < list->count; i++) {
        if (utils_strcasecmp(list->items[i].username, username) == 0) {
            /* Shift the remaining records left (array based delete). */
            size_t j;
            for (j = i; j + 1 < list->count; j++) {
                list->items[j] = list->items[j + 1];
            }
            list->count--;
            return 0;
        }
    }
    return -1;
}

User *db_find(UserList *list, const char *username)
{
    size_t i;

    if (!list || !username) {
        return NULL;
    }
    for (i = 0; i < list->count; i++) {
        if (utils_strcasecmp(list->items[i].username, username) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

int db_count_role(const UserList *list, Role role)
{
    size_t i;
    int count = 0;

    if (!list) {
        return 0;
    }
    for (i = 0; i < list->count; i++) {
        if (list->items[i].role == role) {
            count++;
        }
    }
    return count;
}

void db_print_all(const UserList *list)
{
    size_t i;

    if (!list || list->count == 0) {
        printf("\nNo user accounts are stored yet.\n\n");
        return;
    }

    user_print_table_header();
    for (i = 0; i < list->count; i++) {
        user_print_table_row(&list->items[i]);
    }
    user_print_table_footer((int)list->count);
}

/* ---------------------------------------------------------------- */
/* Initialisation                                                   */
/* ---------------------------------------------------------------- */

int db_init(void)
{
    FILE *fp;
    UserList existing;
    UserList list;
    User admin;
    long existing_count;
    char salt[PASSWORD_SALT_BYTES * 2 + 1];
    char hash[MAX_HASH];
    char answer_salt[PASSWORD_SALT_BYTES * 2 + 1];
    char answer_hash[MAX_HASH];

    /* Seed the default administrator when the database is missing or
     * when it does not contain a single record yet. */
    memset(&existing, 0, sizeof existing);
    existing_count = db_load(&existing);
    if (existing_count > 0) {
        db_free(&existing);
        return 0;                       /* database already populated */
    }
    db_free(&existing);

    memset(&admin, 0, sizeof admin);
    utils_copy_str(admin.username, sizeof admin.username, "admin");
    utils_copy_str(admin.full_name, sizeof admin.full_name, "System Administrator");
    admin.role = ROLE_ADMIN;
    admin.status = ACCOUNT_ACTIVE;
    admin.failed_attempts = 0;

    if (password_generate_salt(salt, sizeof salt) != 0 ||
        password_hash("Admin@123", salt, PASSWORD_ITERATIONS,
                      hash, sizeof hash) != 0) {
        fprintf(stderr, "[database] unable to hash the default password\n");
        return -1;
    }
    utils_copy_str(admin.password_hash, sizeof admin.password_hash, hash);

    utils_copy_str(admin.security_question, sizeof admin.security_question,
                   "Name of your first school?");
    if (password_generate_salt(answer_salt, sizeof answer_salt) != 0 ||
        password_hash("vasavi", answer_salt, PASSWORD_ITERATIONS,
                      answer_hash, sizeof answer_hash) != 0) {
        fprintf(stderr, "[database] unable to hash the default answer\n");
        return -1;
    }
    utils_copy_str(admin.security_answer_hash, sizeof admin.security_answer_hash,
                   answer_hash);

    memset(&list, 0, sizeof list);
    if (db_add(&list, &admin) != 0) {
        db_free(&list);
        return -1;
    }

    /* Create the file even when writing the list fails. */
    fp = fopen(g_db_path, "a");
    if (fp) {
        fclose(fp);
    }

    if (db_save(&list) != 0) {
        db_free(&list);
        return -1;
    }

    db_free(&list);
    return 1;                           /* default administrator created */
}