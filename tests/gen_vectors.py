"""Generates C test vectors for the SHA-256 / HMAC / PBKDF2 implementation
using Python's hashlib as a reference. Writes test_vectors.h.

Inputs are hex encoded with explicit lengths so that binary payloads
(including NUL bytes) survive the round trip into C."""

import hashlib
import hmac as hmaclib
import os

lines = ["/* Auto-generated test vectors (reference: Python hashlib / hmac). */",
         "#ifndef TEST_VECTORS_H",
         "#define TEST_VECTORS_H",
         ""]

# ---- SHA-256 ----
sha_inputs = [
    b"",
    b"abc",
    b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    b"a" * 55, b"a" * 56, b"a" * 63, b"a" * 64, b"a" * 65,
    b"a" * 1000,
    bytes(range(256)),
    bytes(range(255, -1, -1)),
    b"\x00" * 10,
]
lines.append("typedef struct { const char *msg_hex; int len; const char *sha256; } ShaVec;")
lines.append("static const ShaVec SHA_VECS[] = {")
for msg in sha_inputs:
    lines.append('    { "%s", %d,' % (msg.hex(), len(msg)))
    lines.append('      "%s" },' % hashlib.sha256(msg).hexdigest())
lines.append("};")
lines.append("#define SHA_VEC_COUNT ((int)(sizeof(SHA_VECS) / sizeof(SHA_VECS[0])))")
lines.append("")

# ---- HMAC-SHA256 ----
hmac_inputs = [
    (b"", b""),
    (b"key", b"The quick brown fox jumps over the lazy dog"),
    (b"\x0b" * 20, b"Hi There"),
    (b"Jefe", b"what do ya want for nothing?"),
    (bytes(range(100, 164)), b"Test Using Larger Than Block-Size Key - Hash Key First"),
    (b"secret", bytes(range(128))),
    (b"k", b""),
    (b"", b"message with empty key"),
    (b"\xff" * 64, b"\x00" * 3),
]
lines.append("typedef struct { const char *key_hex; int keylen; const char *msg_hex; int msglen; const char *hmac; } HmacVec;")
lines.append("static const HmacVec HMAC_VECS[] = {")
for key, msg in hmac_inputs:
    lines.append('    { "%s", %d,' % (key.hex(), len(key)))
    lines.append('      "%s", %d,' % (msg.hex(), len(msg)))
    lines.append('      "%s" },' % hmaclib.new(key, msg, hashlib.sha256).hexdigest())
lines.append("};")
lines.append("#define HMAC_VEC_COUNT ((int)(sizeof(HMAC_VECS) / sizeof(HMAC_VECS[0])))")
lines.append("")

# ---- PBKDF2-HMAC-SHA256 ----
pbkdf2_inputs = [
    (b"passwd", b"salt", 1, 64),
    (b"passwd", b"salt", 2, 64),
    (b"password", b"NaCl", 80000, 64),
    (b"", b"salt", 1, 32),
    (b"Hello@123", b"", 1, 32),
    (b"Hello@123", b"\x01\x02\x03\x04\x05\x06\x07\x08", 1000, 32),
    (b"correct horse battery staple", b"0123456789abcdef", 4096, 40),
    (b"Admin@123", b"\xff" * 16, 12345, 32),
    (b"\x00password", b"\x00salt", 7, 33),
    (b"Guest@2026", b"a" * 32, 2000, 65),
]
lines.append("typedef struct { const char *pw_hex; const char *salt_hex; int iter; int dklen; const char *dk; } Pbkdf2Vec;")
lines.append("static const Pbkdf2Vec PBKDF2_VECS[] = {")
for pw, salt, iter_, dklen in pbkdf2_inputs:
    dk = hashlib.pbkdf2_hmac("sha256", pw, salt, iter_, dklen).hex()
    lines.append('    { "%s",' % pw.hex())
    lines.append('      "%s",' % salt.hex())
    lines.append('      %d, %d,' % (iter_, dklen))
    lines.append('      "%s" },' % dk)
lines.append("};")
lines.append("#define PBKDF2_VEC_COUNT ((int)(sizeof(PBKDF2_VECS) / sizeof(PBKDF2_VECS[0])))")
lines.append("")
lines.append("#endif /* TEST_VECTORS_H */")

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_vectors.h")
with open(out, "w") as fh:
    fh.write("\n".join(lines) + "\n")
print("wrote", out)
print("  ", len(sha_inputs), "sha,", len(hmac_inputs), "hmac,", len(pbkdf2_inputs), "pbkdf2 vectors")