/*
 * password.h
 * ------------------------------------------------------------------
 * Password security module.
 *
 *  - SHA-256, HMAC-SHA256 and PBKDF2-HMAC-SHA256 implementations
 *    (self contained, no external crypto library required)
 *  - Password validation and strength analysis
 *  - Password hashing and verification
 *
 * Stored format of a hash:
 *   pbkdf2_sha256$<iterations>$<salt-hex>$<derived-key-hex>
 *
 * Passwords are never stored or compared as plain text.
 */

#ifndef PASSWORD_H
#define PASSWORD_H

#include <stddef.h>

#define PASSWORD_MIN_LENGTH       8
#define PASSWORD_SALT_BYTES      16          /* 128 bit salt         */
#define PASSWORD_ITERATIONS  100000          /* PBKDF2 work factor   */
#define PASSWORD_DERIVED_BYTES   32          /* 256 bit derived key  */

/* ---------------------------------------------------------------- */
/* Strength analysis                                                */
/* ---------------------------------------------------------------- */
typedef enum {
    PW_VERY_WEAK = 0,
    PW_WEAK,
    PW_MEDIUM,
    PW_STRONG
} PwStrength;

typedef struct {
    int length;
    int upper;
    int lower;
    int digit;
    int special;
    int score;              /* 0 (worst) .. 8 (best) */
    PwStrength strength;
} PwReport;

/* Validates a candidate password against the system password policy.
 * Returns 1 when acceptable, otherwise 0 and fills err with a reason. */
int password_validate(const char *password, char *err, size_t err_size);

/* Scores a password and reports which character classes it contains. */
PwReport password_analyze(const char *password);

/* "VERY WEAK" / "WEAK" / "MEDIUM" / "STRONG" */
const char *password_strength_label(PwStrength strength);

/* Prints a simple textual strength bar. */
void password_strength_bar(const PwReport *report);

/* ---------------------------------------------------------------- */
/* Hashing                                                          */
/* ---------------------------------------------------------------- */

/* Generates a new random salt in hexadecimal form (out needs 2*bytes+1). */
int password_generate_salt(char *out, size_t out_size);

/* Derives a hash: pbkdf2_sha256$<iterations>$<salt-hex>$<key-hex> */
int password_hash(const char *password, const char *salt_hex,
                  int iterations, char *out, size_t out_size);

/* Verifies a password against a stored hash string.
 * Returns 1 on match, 0 on mismatch, -1 on malformed hash. */
int password_verify(const char *password, const char *stored_hash);

/* Normalises a security answer (trim + lowercase) before hashing. */
void password_normalise_answer(const char *in, char *out, size_t out_size);

/* Securely removes a secret from memory. */
void password_wipe(char *secret, size_t len);

#endif /* PASSWORD_H */