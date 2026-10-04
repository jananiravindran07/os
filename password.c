/*
 * password.c
 * ------------------------------------------------------------------
 * Cryptographic password hashing (SHA-256 -> HMAC -> PBKDF2) plus
 * password policy / strength checking.
 *
 * PBKDF2-HMAC-SHA256 with a random 128 bit salt and 100000 iterations
 * is used, which is the same construction recommended by current
 * password storage guidelines (NIST SP 800-132 / OWASP).
 */

#include "password.h"
#include "utils.h"
#include "user.h"      /* MAX_PASSWORD and friends */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* ================================================================== */
/* SHA-256                                                            */
/* ================================================================== */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    unsigned char buffer[64];
    size_t buflen;
} sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ ((~(x)) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define BSIG1(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SSIG0(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static void sha256_transform(sha256_ctx *ctx, const unsigned char *data)
{
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        m[i] = SSIG1(m[i - 2]) + m[i - 7] + SSIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + BSIG1(e) + CH(e, f, g) + K256[i] + m[i];
        uint32_t t2 = BSIG0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx)
{
    ctx->state[0] = 0x6a09e667UL;
    ctx->state[1] = 0xbb67ae85UL;
    ctx->state[2] = 0x3c6ef372UL;
    ctx->state[3] = 0xa54ff53aUL;
    ctx->state[4] = 0x510e527fUL;
    ctx->state[5] = 0x9b05688cUL;
    ctx->state[6] = 0x1f83d9abUL;
    ctx->state[7] = 0x5be0cd19UL;
    ctx->bitlen = 0;
    ctx->buflen = 0;
}

static void sha256_update(sha256_ctx *ctx, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i = 0;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = p[i];
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(sha256_ctx *ctx, unsigned char out[32])
{
    size_t i = ctx->buflen;

    ctx->bitlen += (uint64_t)ctx->buflen * 8;

    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) {
            ctx->buffer[i++] = 0x00;
        }
        sha256_transform(ctx, ctx->buffer);
        i = 0;
    }
    while (i < 56) {
        ctx->buffer[i++] = 0x00;
    }

    ctx->buffer[56] = (unsigned char)((ctx->bitlen >> 56) & 0xFF);
    ctx->buffer[57] = (unsigned char)((ctx->bitlen >> 48) & 0xFF);
    ctx->buffer[58] = (unsigned char)((ctx->bitlen >> 40) & 0xFF);
    ctx->buffer[59] = (unsigned char)((ctx->bitlen >> 32) & 0xFF);
    ctx->buffer[60] = (unsigned char)((ctx->bitlen >> 24) & 0xFF);
    ctx->buffer[61] = (unsigned char)((ctx->bitlen >> 16) & 0xFF);
    ctx->buffer[62] = (unsigned char)((ctx->bitlen >> 8) & 0xFF);
    ctx->buffer[63] = (unsigned char)(ctx->bitlen & 0xFF);
    sha256_transform(ctx, ctx->buffer);

    for (i = 0; i < 8; i++) {
        out[i * 4]     = (unsigned char)((ctx->state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = (unsigned char)((ctx->state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = (unsigned char)((ctx->state[i] >> 8) & 0xFF);
        out[i * 4 + 3] = (unsigned char)(ctx->state[i] & 0xFF);
    }

    memset(ctx, 0, sizeof(*ctx));
}

/* ================================================================== */
/* HMAC-SHA256                                                        */
/* ================================================================== */

static void hmac_sha256(const unsigned char *key, size_t keylen,
                        const unsigned char *msg, size_t msglen,
                        unsigned char out[32])
{
    unsigned char k[64];
    unsigned char ipad[64];
    unsigned char opad[64];
    unsigned char inner[32];
    sha256_ctx ctx;
    size_t i;

    memset(k, 0, sizeof k);
    if (keylen > 64) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, keylen);
        sha256_final(&ctx, k);
    } else {
        memcpy(k, key, keylen);
    }

    for (i = 0; i < 64; i++) {
        ipad[i] = (unsigned char)(k[i] ^ 0x36);
        opad[i] = (unsigned char)(k[i] ^ 0x5C);
    }

    sha256_init(&ctx);
    sha256_update(&ctx, ipad, 64);
    sha256_update(&ctx, msg, msglen);
    sha256_final(&ctx, inner);

    sha256_init(&ctx);
    sha256_update(&ctx, opad, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, out);

    memset(k, 0, sizeof k);
    memset(ipad, 0, sizeof ipad);
    memset(opad, 0, sizeof opad);
}

/* ================================================================== */
/* PBKDF2-HMAC-SHA256                                                */
/* ================================================================== */

static void pbkdf2_hmac_sha256(const char *password, size_t passlen,
                               const unsigned char *salt, size_t saltlen,
                               int iterations,
                               unsigned char *out, size_t outlen)
{
    unsigned char *salt_block;
    unsigned char u[32];
    unsigned char t[32];
    uint32_t block_index = 1;
    size_t produced = 0;

    if (iterations < 1) {
        iterations = 1;
    }

    salt_block = (unsigned char *)malloc(saltlen + 4);
    if (!salt_block) {
        memset(out, 0, outlen);
        return;
    }
    memcpy(salt_block, salt, saltlen);

    while (produced < outlen) {
        size_t take;
        size_t j;

        /* INT(32-BE) block counter appended to the salt. */
        salt_block[saltlen]     = (unsigned char)((block_index >> 24) & 0xFF);
        salt_block[saltlen + 1] = (unsigned char)((block_index >> 16) & 0xFF);
        salt_block[saltlen + 2] = (unsigned char)((block_index >> 8) & 0xFF);
        salt_block[saltlen + 3] = (unsigned char)(block_index & 0xFF);

        hmac_sha256((const unsigned char *)password, passlen,
                    salt_block, saltlen + 4, u);
        memcpy(t, u, 32);

        for (j = 1; j < (size_t)iterations; j++) {
            size_t k;
            hmac_sha256((const unsigned char *)password, passlen, u, 32, u);
            for (k = 0; k < 32; k++) {
                t[k] ^= u[k];
            }
        }

        take = (outlen - produced < 32) ? (outlen - produced) : 32;
        memcpy(out + produced, t, take);
        produced += take;
        block_index++;
    }

    memset(u, 0, sizeof u);
    memset(t, 0, sizeof t);
    free(salt_block);
}

/* ================================================================== */
/* Hex helpers                                                       */
/* ================================================================== */

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out)
{
    static const char *digits = "0123456789abcdef";
    size_t i;

    for (i = 0; i < len; i++) {
        out[i * 2]     = digits[(bytes[i] >> 4) & 0x0F];
        out[i * 2 + 1] = digits[bytes[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static int hex_to_bytes(const char *hex, unsigned char *out, size_t out_size)
{
    size_t len;
    size_t i;

    if (!hex || !out) {
        return -1;
    }
    len = strlen(hex);
    if (len != out_size * 2) {
        return -1;
    }
    for (i = 0; i < out_size * 2; i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return -1;
        }
    }
    for (i = 0; i < out_size; i++) {
        char pair[3];
        pair[0] = hex[i * 2];
        pair[1] = hex[i * 2 + 1];
        pair[2] = '\0';
        out[i] = (unsigned char)strtoul(pair, NULL, 16);
    }
    return 0;
}

/* Constant time comparison - avoids leaking information by timing. */
static int secure_compare(const char *a, const char *b)
{
    size_t len = strlen(a);
    size_t i;
    unsigned char diff = 0;

    if (len != strlen(b)) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }
    return diff == 0;
}

/* ================================================================== */
/* Public API - policy and strength                                  */
/* ================================================================== */

int password_validate(const char *password, char *err, size_t err_size)
{
    size_t len;
    int has_upper = 0;
    int has_lower = 0;
    int has_digit = 0;
    int has_special = 0;
    size_t i;

    if (err && err_size > 0) {
        err[0] = '\0';
    }
    if (!password) {
        utils_copy_str(err, err_size, "Password is missing.");
        return 0;
    }

    len = strlen(password);
    if (len < PASSWORD_MIN_LENGTH) {
        snprintf(err, err_size, "Password must be at least %d characters long.",
                 PASSWORD_MIN_LENGTH);
        return 0;
    }
    if (len >= MAX_PASSWORD) {
        utils_copy_str(err, err_size, "Password is too long.");
        return 0;
    }

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)password[i];
        if (isupper(c))      { has_upper = 1; }
        else if (islower(c)) { has_lower = 1; }
        else if (isdigit(c)) { has_digit = 1; }
        else if (isalnum(c)) { /* other letters/digits */ }
        else                 { has_special = 1; }
    }

    if (!has_upper || !has_lower) {
        utils_copy_str(err, err_size,
                       "Password must contain both uppercase and lowercase letters.");
        return 0;
    }
    if (!has_digit) {
        utils_copy_str(err, err_size, "Password must contain at least one number.");
        return 0;
    }
    if (has_upper + has_lower + has_digit + has_special < 3) {
        utils_copy_str(err, err_size,
                       "Password is too simple - add special characters.");
        return 0;
    }

    /* Reject the most obvious weak choices. */
    if (utils_strcasecmp(password, "password") == 0 ||
        utils_strcasecmp(password, "admin") == 0 ||
        utils_strcasecmp(password, "admin123") == 0 ||
        utils_strcasecmp(password, "12345678") == 0) {
        utils_copy_str(err, err_size, "This password is commonly used - choose another.");
        return 0;
    }

    return 1;
}

PwReport password_analyze(const char *password)
{
    PwReport r;
    size_t len;
    size_t i;
    int all_same = 1;

    memset(&r, 0, sizeof r);
    if (!password) {
        r.strength = PW_VERY_WEAK;
        return r;
    }

    len = strlen(password);
    r.length = (int)len;

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)password[i];
        if (isupper(c))      { r.upper++; }
        else if (islower(c)) { r.lower++; }
        else if (isdigit(c)) { r.digit++; }
        else                 { r.special++; }
    }
    if (len > 1 && password[0] == password[1]) {
        all_same = 0;
    }
    for (i = 1; i < len && all_same; i++) {
        if (password[i] != password[i - 1]) {
            all_same = 0;
        }
    }

    /* Score: one point per character class, bonus for length. */
    r.score = (r.upper > 0) + (r.lower > 0) + (r.digit > 0) + (r.special > 0);
    if (len >= 8)  { r.score += 1; }
    if (len >= 12) { r.score += 2; }
    if (len >= 16) { r.score += 1; }
    if (len < PASSWORD_MIN_LENGTH) { r.score -= 2; }
    if (len > 0 && all_same)       { r.score -= 3; }

    if (r.score >= 7)      { r.strength = PW_STRONG; }
    else if (r.score >= 5) { r.strength = PW_MEDIUM; }
    else if (r.score >= 3) { r.strength = PW_WEAK; }
    else                   { r.strength = PW_VERY_WEAK; }

    return r;
}

const char *password_strength_label(PwStrength strength)
{
    switch (strength) {
    case PW_STRONG:    return "STRONG";
    case PW_MEDIUM:    return "MEDIUM";
    case PW_WEAK:      return "WEAK";
    case PW_VERY_WEAK: return "VERY WEAK";
    default:           return "UNKNOWN";
    }
}

void password_strength_bar(const PwReport *report)
{
    int filled;
    int i;

    if (!report) {
        return;
    }
    filled = (int)((report->score > 8 ? 8 : report->score) * 2);
    printf("%-10s [", password_strength_label(report->strength));
    for (i = 0; i < 16; i++) {
        putchar(i < filled ? '#' : '-');
    }
    printf("]  score %d/8\n", report->score);
}

/* ================================================================== */
/* Public API - hashing                                              */
/* ================================================================== */

int password_generate_salt(char *out, size_t out_size)
{
    if (!out || out_size < PASSWORD_SALT_BYTES * 2 + 1) {
        return -1;
    }
    return utils_random_hex(out, PASSWORD_SALT_BYTES);
}

int password_hash(const char *password, const char *salt_hex,
                  int iterations, char *out, size_t out_size)
{
    unsigned char salt[PASSWORD_SALT_BYTES];
    unsigned char derived[PASSWORD_DERIVED_BYTES];
    char derived_hex[PASSWORD_DERIVED_BYTES * 2 + 1];
    int written;

    if (!password || !salt_hex || !out) {
        return -1;
    }
    if (hex_to_bytes(salt_hex, salt, sizeof salt) != 0) {
        return -1;
    }
    if (iterations <= 0) {
        iterations = PASSWORD_ITERATIONS;
    }

    pbkdf2_hmac_sha256(password, strlen(password), salt, sizeof salt,
                       iterations, derived, sizeof derived);
    bytes_to_hex(derived, sizeof derived, derived_hex);

    written = snprintf(out, out_size, "pbkdf2_sha256$%d$%s$%s",
                       iterations, salt_hex, derived_hex);

    password_wipe((char *)derived, sizeof derived);
    password_wipe(derived_hex, sizeof derived_hex);

    return (written > 0 && (size_t)written < out_size) ? 0 : -1;
}

int password_verify(const char *password, const char *stored_hash)
{
    char prefix[32];
    char salt_hex[PASSWORD_SALT_BYTES * 2 + 1];
    char expected[PASSWORD_DERIVED_BYTES * 2 + 1];
    char computed[MAX_HASH];
    int iterations = 0;
    int rc = -1;

    if (!password || !stored_hash) {
        return -1;
    }
    if (sscanf(stored_hash, "%31[^$]$%d$%32[^$]$%64s",
               prefix, &iterations, salt_hex, expected) != 4) {
        return -1;
    }
    if (strcmp(prefix, "pbkdf2_sha256") != 0) {
        return -1;
    }
    if (iterations < 1 || iterations > 10000000) {
        return -1;
    }
    if (strlen(salt_hex) != PASSWORD_SALT_BYTES * 2 ||
        strlen(expected) != PASSWORD_DERIVED_BYTES * 2) {
        return -1;
    }

    if (password_hash(password, salt_hex, iterations, computed, sizeof computed) != 0) {
        return -1;
    }

    /* Compare the complete canonical hash string in constant time so
     * that neither the salt, the work factor nor the derived key can be
     * probed by measuring execution time. */
    rc = secure_compare(stored_hash, computed) ? 1 : 0;

    password_wipe(computed, sizeof computed);
    return rc;
}

void password_normalise_answer(const char *in, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    utils_copy_str(out, out_size, in ? in : "");
    utils_trim(out);
    utils_str_tolower(out);
}

void password_wipe(char *secret, size_t len)
{
    volatile char *p;
    size_t i;

    if (!secret) {
        return;
    }
    p = (volatile char *)secret;
    for (i = 0; i < len; i++) {
        p[i] = '\0';
    }
}