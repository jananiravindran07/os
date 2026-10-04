/* Verification of the project's SHA-256 / HMAC / PBKDF2 code against
 * vectors produced by Python's hashlib. Temporary test, not shipped. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "test_vectors.h"
#include "password.c"

static int failures = 0;
static int checks = 0;

static void report(const char *label, const char *got, const char *expected)
{
    checks++;
    if (strcmp(got, expected) != 0) {
        printf("  FAIL %s\n    got      %s\n    expected %s\n", label, got, expected);
        failures++;
    }
}

static void hexify(const unsigned char *in, size_t len, char *out)
{
    static const char *d = "0123456789abcdef";
    size_t i;
    for (i = 0; i < len; i++) {
        out[i * 2]     = d[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = d[in[i] & 0xF];
    }
    out[len * 2] = '\0';
}

static size_t unhex(const char *hex, unsigned char *out, size_t max)
{
    size_t len = strlen(hex) / 2;
    size_t i;
    for (i = 0; i < len && i < max; i++) {
        unsigned int byte;
        char pair[3];
        pair[0] = hex[i * 2];
        pair[1] = hex[i * 2 + 1];
        pair[2] = '\0';
        sscanf(pair, "%x", &byte);
        out[i] = (unsigned char)byte;
    }
    return len;
}

int main(void)
{
    char got[600];
    unsigned char buf[4096];
    int i;

    printf("=== SHA-256 (FIPS 180-2 / reference vectors) ===\n");
    for (i = 0; i < SHA_VEC_COUNT; i++) {
        sha256_ctx ctx;
        unsigned char digest[32];
        size_t len = unhex(SHA_VECS[i].msg_hex, buf, sizeof buf);
        size_t k;

        if ((int)len != SHA_VECS[i].len) {
            printf("  FAIL vector %d length mismatch\n", i + 1);
            failures++;
        }

        sha256_init(&ctx);
        for (k = 0; k < len; k++) {          /* one byte at a time */
            sha256_update(&ctx, buf + k, 1);
        }
        sha256_final(&ctx, digest);
        hexify(digest, 32, got);
        printf("  vector %2d len %3d -> %s\n", i + 1, SHA_VECS[i].len, got);
        report("sha256", got, SHA_VECS[i].sha256);
    }

    printf("\n=== HMAC-SHA256 (RFC 4231 / reference vectors) ===\n");
    for (i = 0; i < HMAC_VEC_COUNT; i++) {
        unsigned char key[2048];
        unsigned char msg[2048];
        unsigned char digest[32];
        size_t keylen = unhex(HMAC_VECS[i].key_hex, key, sizeof key);
        size_t msglen = unhex(HMAC_VECS[i].msg_hex, msg, sizeof msg);

        hmac_sha256(key, keylen, msg, msglen, digest);
        hexify(digest, 32, got);
        printf("  vector %2d (key %3d, msg %3d) -> %s\n", i + 1,
               HMAC_VECS[i].keylen, HMAC_VECS[i].msglen, got);
        report("hmac", got, HMAC_VECS[i].hmac);
    }

    printf("\n=== PBKDF2-HMAC-SHA256 (RFC 7914 / reference vectors) ===\n");
    for (i = 0; i < PBKDF2_VEC_COUNT; i++) {
        unsigned char pw[2048];
        unsigned char salt[2048];
        unsigned char dk[512];
        size_t pwlen = unhex(PBKDF2_VECS[i].pw_hex, pw, sizeof pw);
        size_t saltlen = unhex(PBKDF2_VECS[i].salt_hex, salt, sizeof salt);

        pbkdf2_hmac_sha256((const char *)pw, pwlen, salt, saltlen,
                           PBKDF2_VECS[i].iter, dk,
                           (size_t)PBKDF2_VECS[i].dklen);
        hexify(dk, (size_t)PBKDF2_VECS[i].dklen, got);
        printf("  vector %2d (c=%5d, dkLen=%2d) -> %s\n", i + 1,
               PBKDF2_VECS[i].iter, PBKDF2_VECS[i].dklen, got);
        report("pbkdf2", got, PBKDF2_VECS[i].dk);
    }

    printf("\n=== Public hash API round trip ===\n");
    {
        char salt[64];
        char hash[MAX_HASH];
        char hash2[MAX_HASH];

        password_generate_salt(salt, sizeof salt);
        password_hash("Hello@123", salt, 1000, hash, sizeof hash);
        printf("  stored format : %s\n", hash);

        checks++;
        if (password_verify("Hello@123", hash) != 1) {
            printf("  FAIL verify(correct password)\n"); failures++;
        } else { printf("  verify(correct password) -> OK\n"); }

        checks++;
        if (password_verify("Hello@124", hash) != 0) {
            printf("  FAIL verify(wrong password)\n"); failures++;
        } else { printf("  verify(wrong password)   -> rejected\n"); }

        checks++;
        if (password_verify("Hello@123", "garbage") != -1) {
            printf("  FAIL malformed hash rejected\n"); failures++;
        } else { printf("  malformed hash           -> rejected\n"); }

        checks++;
        if (password_verify("Hello@123", "pbkdf2_sha256$1000$zz$zz") != -1) {
            printf("  FAIL bad salt rejected\n"); failures++;
        } else { printf("  bad salt / short fields   -> rejected\n"); }

        {
            char salt2[64];
            password_generate_salt(salt2, sizeof salt2);
            password_hash("Hello@123", salt2, 1000, hash2, sizeof hash2);
            checks++;
            if (strcmp(hash, hash2) == 0) {
                printf("  FAIL salt must be random per account\n"); failures++;
            } else { printf("  random salt per account  -> OK\n"); }
        }
    }

    printf("\n=== Password policy ===\n");
    {
        char err[160];
        struct { const char *pw; int valid; const char *label; } cases[] = {
            { "Ab@12345",  1, "8 chars, all classes" },
            { "abc123",    0, "too short" },
            { "abcdefgh",  0, "no upper/digit" },
            { "Password",  0, "commonly used" },
            { "aaaaaaaa",  0, "single repeated char" },
            { "Str0ng!Passw0rd#2026", 1, "long and complex" }
        };
        size_t c;
        for (c = 0; c < sizeof cases / sizeof cases[0]; c++) {
            int valid = password_validate(cases[c].pw, err, sizeof err);
            PwReport r = password_analyze(cases[c].pw);
            checks++;
            if (valid != cases[c].valid) {
                printf("  FAIL policy '%s' -> %d (expected %d) %s\n",
                       cases[c].label, valid, cases[c].valid, err);
                failures++;
            } else {
                printf("  %-22s %-6s %-10s score %d\n", cases[c].label,
                       valid ? "ACCEPT" : "REJECT",
                       password_strength_label(r.strength), r.score);
            }
        }
    }

    printf("\n=== Work factor timing ===\n");
    {
        char salt[64];
        char hash[MAX_HASH];
        clock_t t0, t1;
        password_generate_salt(salt, sizeof salt);
        t0 = clock();
        password_hash("Hello@123", salt, PASSWORD_ITERATIONS, hash, sizeof hash);
        t1 = clock();
        printf("  PBKDF2 with %d iterations: %.0f ms\n", PASSWORD_ITERATIONS,
               1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC);
    }

    printf("\n%d checks, %d failures -> %s\n", checks, failures,
           failures == 0 ? "ALL TESTS PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}