"""First-run check: verifies the shipped (comment only) users.txt is seeded
with the default administrator and that the documented credentials work."""

import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)

failures = []
checks = 0


def check(label, condition, extra=""):
    global checks
    checks += 1
    print(("  PASS " if condition else "  FAIL ") + label + (" " + extra if extra else ""))
    if not condition:
        failures.append(label)


def main():
    exe = os.path.join(PROJECT, "uac.exe" if os.name == "nt" else "uac")
    if not os.path.exists(exe):
        print("build first")
        return 1

    work = tempfile.mkdtemp(prefix="uac_firstrun_")
    shutil.copy(exe, os.path.join(work, os.path.basename(exe)))
    # Ship exactly what the repository contains: a comment only users.txt
    shutil.copy(os.path.join(PROJECT, "users.txt"), os.path.join(work, "users.txt"))
    shutil.copy(os.path.join(PROJECT, "audit.log"), os.path.join(work, "audit.log"))

    proc = subprocess.Popen([os.path.join(work, os.path.basename(exe))], cwd=work,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, bufsize=0)
    fd = proc.stdout.fileno()
    chunks = queue.Queue()
    pending = [""]

    def reader():
        while True:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            if not data:
                break
            chunks.put(data.decode("utf-8", "replace"))

    threading.Thread(target=reader, daemon=True).start()

    def expect(pattern, timeout=30):
        rx = re.compile(pattern)
        deadline = time.time() + timeout
        while True:
            m = rx.search(pending[0])
            if m:
                pending[0] = pending[0][m.end():]
                return m.group(0)
            remaining = deadline - time.time()
            if remaining <= 0:
                raise AssertionError("timeout for %r pending=%r" % (pattern, pending[0][-300:]))
            pending[0] += chunks.get(timeout=remaining)

    def send(text=""):
        proc.stdin.write((text + "\n").encode())
        proc.stdin.flush()

    expect(r"Enter your choice:")          # the program has initialised by now

    raw = open(os.path.join(work, "users.txt")).read()
    records = [l for l in raw.splitlines() if l and not l.startswith("#")]
    check("default admin seeded on first run",
          any(l.startswith("admin|pbkdf2_sha256$100000$") for l in records),
          str(records[:1]))
    check("seeded admin has role ADMIN and is ACTIVE", "|ADMIN|0|ACTIVE|" in raw)
    check("no plain text password in the seeded records",
          not any("Admin@123" in l for l in records))

    print("  logging in with the documented default credentials...")
    send("2")
    expect(r"Username:")
    send("admin")
    expect(r"Password:")
    send("Admin@123")
    expect(r"LOGIN SUCCESSFUL")
    check("default credentials work", True)
    expect(r"Enter your choice:")
    send("0")
    expect(r"Press Enter")
    send("")
    expect(r"Enter your choice:")
    send("5")
    expect(r"Goodbye")
    proc.stdin.close()
    proc.wait(timeout=15)

    log = open(os.path.join(work, "audit.log")).read()
    check("audit log keeps the shipped entry", "2026-10-04 17:08 | system | SYSTEM_START" in log)
    check("login appended to the audit log", "LOGIN_SUCCESS" in log)
    check("no password in the audit log", "Admin@123" not in log)

    shutil.rmtree(work, ignore_errors=True)
    print("\n%d checks, %d failures -> %s" %
          (checks, len(failures), "FIRST RUN OK" if not failures else "FAILURES"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())