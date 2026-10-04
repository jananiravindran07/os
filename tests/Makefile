# Makefile - verification suite (optional, not part of the application)
#
#   make crypto     build and run the SHA-256 / HMAC / PBKDF2 vector tests
#   make firstrun   verify the first-run seeding of the default admin
#   make e2e        run the end to end interactive tests
#   make all-checks run every suite (crypto, firstrun, e2e)
#   make clean      remove test binaries
#
# The C vector tests check the project's crypto code against reference
# vectors. test_vectors.h was generated with Python's hashlib; run
# `python3 gen_vectors.py` to regenerate it.

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O2
PYTHON  = python3

APP_DIR = ..

ifeq ($(OS),Windows_NT)
    T_CRYPT = test_crypto.exe
else
    T_CRYPT = test_crypto
endif

.PHONY: crypto firstrun e2e all-checks clean

all-checks: crypto firstrun e2e

crypto: $(T_CRYPT)
	./$(T_CRYPT)

$(T_CRYPT): test_crypto.c test_vectors.h $(APP_DIR)/password.c $(APP_DIR)/utils.c
	$(CC) $(CFLAGS) -I$(APP_DIR) -o $@ test_crypto.c $(APP_DIR)/utils.c

firstrun:
	$(PYTHON) test_first_run.py

e2e:
	$(PYTHON) test_e2e.py

clean:
	$(RM) $(T_CRYPT)