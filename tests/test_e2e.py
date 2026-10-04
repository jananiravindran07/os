"""End-to-end driver for the User Authentication and Access Control System.

Spawns uac.exe, waits for specific prompts and sends input, asserting that
the expected output appears. Verifies the interactive flows plus the
resulting users.txt / audit.log state.
"""

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

# The tests run against a throw-away copy of the executable in a temporary
# directory, so the project's own users.txt / audit.log are never touched.
WORKDIR = tempfile.mkdtemp(prefix="uac_e2e_")
APP = os.path.join(PROJECT, "uac.exe" if os.name == "nt" else "uac")
if not os.path.exists(APP):
    APP = os.path.join(PROJECT, "uac")
USERS = os.path.join(WORKDIR, "users.txt")
AUDIT = os.path.join(WORKDIR, "audit.log")

failures = []
checks = 0


def check(label, condition, extra=""):
    global checks
    checks += 1
    if condition:
        print("  PASS %s" % label)
    else:
        print("  FAIL %s %s" % (label, extra))
        failures.append(label)


class App:
    def __init__(self):
        self.proc = subprocess.Popen(
            [APP], cwd=WORKDIR, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=0)
        self.fd = self.proc.stdout.fileno()
        self.chunks = queue.Queue()
        self.transcript = []
        self.pending = ""          # text received but not yet consumed
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        """Read raw chunks: prompts have no trailing newline, so line
        based reading would block until the next newline arrives."""
        while True:
            try:
                data = os.read(self.fd, 4096)
            except OSError:
                break
            if not data:
                break
            self.chunks.put(data.decode("utf-8", "replace"))

    def expect(self, pattern, timeout=30):
        """Consume output until `pattern` (regex) is seen."""
        rx = re.compile(pattern)
        deadline = time.time() + timeout
        while True:
            m = rx.search(self.pending)
            if m:
                cut = m.end()
                self.transcript.append(self.pending[:cut])
                self.pending = self.pending[cut:]
                return m.group(0)
            remaining = deadline - time.time()
            if remaining <= 0:
                raise AssertionError(
                    "timeout waiting for %r; pending output: %r"
                    % (pattern, self.pending[-400:]))
            try:
                self.pending += self.chunks.get(timeout=remaining)
            except queue.Empty:
                raise AssertionError(
                    "timeout waiting for %r; pending output: %r"
                    % (pattern, self.pending[-400:]))

    def send(self, text=""):
        self.proc.stdin.write((text + "\n").encode())
        self.proc.stdin.flush()

    def expect_menu(self):
        self.expect(r"Enter your choice:")

    def logout_to_menu(self):
        """Leaves the app waiting at the main menu, with its prompt still
        pending so that expect_menu()/login() can consume it."""
        self.expect(r"Enter your choice:")
        self.send("0")           # logout from the dashboard
        self.expect(r"Press Enter to continue")
        self.send("")
        self.expect(r"USER AUTHENTICATION AND")   # banner, prompt stays pending

    def close(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()


def login(app, username, password):
    app.expect_menu()
    app.send("2")
    app.expect(r"Username:")
    app.send(username)
    app.expect(r"Password:")
    app.send(password)


def main():
    if not os.path.exists(APP):
        print("Build the project first (gcc -o uac ... ), expected: %s" % APP)
        return 1

    shutil.copy(APP, os.path.join(WORKDIR, os.path.basename(APP)))
    for path in (USERS, AUDIT):
        if os.path.exists(path):
            os.remove(path)

    app = App()
    print("=== 1. Registration (USER) ===")
    app.expect_menu()
    app.send("1")
    app.expect(r"Username    :")
    app.send("testuser")
    app.expect(r"Full name   :")
    app.send("Test User")
    app.expect(r"Choice \[1-2\]")
    app.send("1")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"New password")
    app.send("Str0ng@Pass1")
    app.expect(r"Confirm       :")
    app.send("Str0ng@Pass1")
    app.expect(r"Choice \[1-6\]")
    app.send("1")
    app.expect(r"Security answer")
    app.send("vasavi")
    app.expect(r"Confirm answer")
    app.send("vasavi")
    app.expect(r"REGISTRATION SUCCESSFUL")
    app.expect(r"Press Enter")
    app.send("")

    db = open(USERS).read()
    check("user record created", "testuser|" in db)
    check("password not stored in clear text", "Str0ng@Pass1" not in db)
    check("password stored as pbkdf2 hash", "pbkdf2_sha256$100000$" in db)

    print("\n=== 2. Weak password rejection ===")
    app.expect_menu()
    app.send("1")
    app.expect(r"Username    :")
    app.send("weakuser")
    app.expect(r"Full name   :")
    app.send("Weak User")
    app.expect(r"Choice \[1-2\]")
    app.send("1")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"New password")
    app.send("abc")
    app.expect(r"Confirm       :")
    app.send("abc")
    app.expect(r"Weak password")
    app.expect(r"New password")
    app.send("Str0ng@Pass1")
    app.expect(r"Confirm       :")
    app.send("Str0ng@Pass1")
    app.expect(r"Choice \[1-6\]")
    app.send("1")
    app.expect(r"Security answer")
    app.send("vasavi")
    app.expect(r"Confirm answer")
    app.send("vasavi")
    app.expect(r"REGISTRATION SUCCESSFUL")
    app.expect(r"Press Enter")
    app.send("")

    print("\n=== 3. Duplicate username rejection ===")
    app.expect_menu()
    app.send("1")
    app.expect(r"Username    :")
    app.send("testuser")
    app.expect(r"already taken")
    app.expect(r"Press Enter")
    app.send("")

    print("\n=== 4. Invalid username format ===")
    app.expect_menu()
    app.send("1")
    app.expect(r"Username    :")
    app.send("A!")
    app.expect(r"Invalid username")
    app.expect(r"Username    :")
    app.send("testuser2")
    app.expect(r"Full name   :")
    app.send("")
    app.expect(r"Choice \[1-2\]")
    app.send("2")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"New password")
    app.send("Gu3st@Pass")
    app.expect(r"Confirm       :")
    app.send("Gu3st@Pass")
    app.expect(r"Choice \[1-6\]")
    app.send("1")
    app.expect(r"Security answer")
    app.send("vasavi")
    app.expect(r"Confirm answer")
    app.send("vasavi")
    app.expect(r"REGISTRATION SUCCESSFUL")
    app.expect(r"Press Enter")
    app.send("")
    db = open(USERS).read()
    check("guest account created", "testuser2|" in db and "|GUEST|" in db)

    print("\n=== 5. GUEST access control (denied) ===")
    login(app, "testuser2", "Gu3st@Pass")
    app.expect(r"GUEST DASHBOARD")
    app.expect(r"Enter your choice:")
    app.send("2")                       # Access Resource
    app.expect(r"Select a resource:")
    app.send("3")                       # CHANGE PASSWORD -> must be denied
    app.expect(r"ACCESS DENIED")
    app.expect(r"Required Role")
    app.expect(r"Press Enter")
    app.send("")
    app.expect(r"Enter your choice:")
    app.send("2")                       # Access Resource
    app.expect(r"Select a resource:")
    app.send("5")                       # USER MANAGEMENT -> denied
    app.expect(r"ACCESS DENIED")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 6. GUEST access control (granted) ===")
    login(app, "testuser2", "Gu3st@Pass")
    app.expect(r"Enter your choice:")
    app.send("2")
    app.expect(r"Select a resource:")
    app.send("1")                       # PUBLIC INFORMATION -> granted
    app.expect(r"ACCESS GRANTED")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 7. USER access control ===")
    login(app, "testuser", "Str0ng@Pass1")
    app.expect(r"USER DASHBOARD")
    app.expect(r"Enter your choice:")
    app.send("3")
    app.expect(r"Select a resource:")
    app.send("5")                       # USER MANAGEMENT -> denied for USER
    app.expect(r"ACCESS DENIED")
    app.expect(r"Press Enter")
    app.send("")
    app.expect(r"Enter your choice:")
    app.send("3")
    app.expect(r"Select a resource:")
    app.send("4")                       # NORMAL RESOURCE -> granted
    app.expect(r"ACCESS GRANTED")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 8. Failed login tracking (1/3, 2/3) ===")
    login(app, "testuser", "Wr0ng@Pass")
    app.expect(r"Invalid username or password")
    app.expect(r"Failed Attempts: 1/3")
    app.expect(r"Press Enter")
    app.send("")
    login(app, "testuser", "Wr0ng@Pass")
    app.expect(r"Failed Attempts: 2/3")
    app.expect(r"Press Enter")
    app.send("")
    db = open(USERS).read()
    check("attempt counter persisted", "|USER|2|ACTIVE|" in db)

    print("\n=== 9. Account lockout (3/3) ===")
    login(app, "testuser", "Wr0ng@Pass")
    app.expect(r"SECURITY ALERT")
    app.expect(r"LOCKED")
    app.expect(r"Press Enter")
    app.send("")
    db = open(USERS).read()
    check("account marked LOCKED in database", "|USER|3|LOCKED|" in db)

    print("\n=== 10. Locked account cannot log in ===")
    login(app, "testuser", "Str0ng@Pass1")
    app.expect(r"SECURITY ALERT")
    app.expect(r"Press Enter")
    app.send("")

    print("\n=== 11. Unknown username ===")
    login(app, "ghost", "whatever1")
    app.expect(r"Invalid username or password")
    app.expect(r"Press Enter")
    app.send("")

    print("\n=== 12. Administrator login and user list ===")
    login(app, "admin", "Admin@123")
    app.expect(r"ADMIN DASHBOARD")
    app.expect(r"Enter your choice:")
    app.send("1")
    app.expect(r"USER MANAGEMENT - ALL ACCOUNTS")
    app.expect(r"USERNAME")
    app.expect(r"testuser")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 13. Administrator unlocks the account ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("5")                        # Unlock Account
    app.expect(r"UNLOCK ACCOUNT")
    app.expect(r"testuser")
    app.expect(r"Username to unlock:")
    app.send("testuser")
    app.expect(r"ACCOUNT UNLOCKED")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()
    db = open(USERS).read()
    check("counter reset after unlock",
          re.search(r"^testuser\|pbkdf2_sha256\$\d+\$\w+\$\w+\|USER\|0\|ACTIVE\|", db, re.M) is not None)

    print("\n=== 14. Unlocked account can log in again ===")
    login(app, "testuser", "Str0ng@Pass1")
    app.expect(r"USER DASHBOARD")
    app.logout_to_menu()

    print("\n=== 15. Admin adds a user ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("2")                        # Add User
    app.expect(r"ADD NEW USER")
    app.expect(r"Username  :")
    app.send("rahul")
    app.expect(r"Full name :")
    app.send("Rahul Kumar")
    app.expect(r"Choice \[1-3\]")
    app.send("1")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"New password")
    app.send("Rahul@Pass1")
    app.expect(r"Confirm       :")
    app.send("Rahul@Pass1")
    app.expect(r"created with role USER")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()
    db = open(USERS).read()
    check("admin created account", "rahul|" in db)

    print("\n=== 16. Admin changes a role to GUEST ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("4")                        # Change User Role
    app.expect(r"Username:")
    app.send("rahul")
    app.expect(r"Choice \[1-3\]")
    app.send("2")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"Role of 'rahul' changed to GUEST")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()
    db = open(USERS).read()
    check("role changed to GUEST",
          re.search(r"^rahul\|pbkdf2_sha256\$\d+\$\w+\$\w+\|GUEST\|0\|ACTIVE\|", db, re.M) is not None)

    print("\n=== 17. Admin deletes a user ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("3")                        # Delete User
    app.expect(r"Username to delete:")
    app.send("rahul")
    app.expect(r"Confirm deletion")
    app.send("y")
    app.expect(r"deleted")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()
    db = open(USERS).read()
    check("user deleted", "rahul|" not in db)

    print("\n=== 18. Last administrator protection ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("3")
    app.expect(r"Username to delete:")
    app.send("admin")
    app.expect(r"only administrator")
    app.expect(r"Press Enter")
    app.send("")
    app.expect(r"Enter your choice:")
    app.send("4")                        # demote
    app.expect(r"Username:")
    app.send("admin")
    app.expect(r"Choice \[1-3\]")
    app.send("2")
    app.expect(r"Confirm role")
    app.send("y")
    app.expect(r"cannot be demoted")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 19. Change password (as user) ===")
    login(app, "testuser", "Str0ng@Pass1")
    app.expect(r"Enter your choice:")
    app.send("2")                        # Change Password
    app.expect(r"Current password")
    app.send("Wr0ng@Pass")               # wrong current password
    app.expect(r"Current password is incorrect")
    app.expect(r"Press Enter")
    app.send("")
    app.expect(r"Enter your choice:")
    app.send("2")
    app.expect(r"Current password")
    app.send("Str0ng@Pass1")
    app.expect(r"New password")
    app.send("N3w@Passw0rd")
    app.expect(r"Confirm       :")
    app.send("N3w@Passw0rd")
    app.expect(r"Password updated successfully")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 20. Login with the new password, old one rejected ===")
    login(app, "testuser", "Str0ng@Pass1")
    app.expect(r"Invalid username or password")
    app.expect(r"Press Enter")
    app.send("")
    login(app, "testuser", "N3w@Passw0rd")
    app.expect(r"USER DASHBOARD")
    app.logout_to_menu()

    print("\n=== 21. Forgot password via security question ===")
    app.expect_menu()
    app.send("3")
    app.expect(r"Username:")
    app.send("testuser")
    app.expect(r"Security question")
    app.expect(r"Your answer")
    app.send("wronganswer")
    app.expect(r"Incorrect answer")
    app.expect(r"Press Enter")
    app.send("")
    app.expect_menu()
    app.send("3")
    app.expect(r"Username:")
    app.send("testuser")
    app.expect(r"Your answer")
    app.send("VASAVI")                   # case insensitive
    app.expect(r"New password")
    app.send("Reset@Pass9")
    app.expect(r"Confirm       :")
    app.send("Reset@Pass9")
    app.expect(r"PASSWORD RESET SUCCESSFUL")
    app.expect(r"Press Enter")
    app.send("")
    login(app, "testuser", "Reset@Pass9")
    app.expect(r"USER DASHBOARD")
    app.logout_to_menu()

    print("\n=== 22. Audit log review (admin only) ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("6")
    app.expect(r"AUTHENTICATION LOGS")
    app.expect(r"Entries to show:")
    app.send("0")
    app.expect(r"LOGIN_SUCCESS")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 23. Admin protected resources ===")
    login(app, "admin", "Admin@123")
    app.expect(r"Enter your choice:")
    app.send("7")
    app.expect(r"PROTECTED SYSTEM RESOURCES")
    app.expect(r"RESOURCE")
    app.expect(r"SYSTEM SETTINGS")
    app.expect(r"Press Enter")
    app.send("")
    app.expect(r"Enter your choice:")
    app.send("8")
    app.expect(r"Select a resource:")
    app.send("8")
    app.expect(r"ACCESS GRANTED")
    app.expect(r"Press Enter")
    app.send("")
    app.logout_to_menu()

    print("\n=== 24. Help screen and exit ===")
    app.expect_menu()
    app.send("4")
    app.expect(r"HELP / SECURITY NOTES")
    app.expect(r"PBKDF2-HMAC-SHA256")
    app.expect(r"Press Enter")
    app.send("")
    app.expect_menu()
    app.send("5")
    app.expect(r"Goodbye")
    app.close()

    print("\n=== 25. Audit log file content ===")
    log = open(AUDIT).read()
    for event in ["SYSTEM_START", "REGISTER_SUCCESS", "REGISTER_FAILED",
                  "LOGIN_SUCCESS", "LOGIN_FAILED", "ACCOUNT_LOCKED",
                  "ACCOUNT_UNLOCKED", "LOGOUT", "ACCESS_GRANTED",
                  "ACCESS_DENIED", "PASSWORD_CHANGED", "PASSWORD_RESET",
                  "USER_ADDED", "USER_DELETED", "ROLE_CHANGED",
                  "LOG_VIEWED", "SYSTEM_EXIT"]:
        check("audit contains %s" % event, event in log)

    lines = [l for l in log.strip().splitlines() if l]
    check("audit entries are timestamped",
          all(re.match(r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2} \| ", l) for l in lines))
    check("audit has no plain text password",
          "Str0ng@Pass1" not in log and "Reset@Pass9" not in log
          and "Admin@123" not in log)

    db = open(USERS).read()
    check("database has no plain text password",
          "Pass1" not in db and "Pass9" not in db and "Admin@123" not in db)
    check("every record has 8 fields",
          all(len(l.split("|")) == 8 for l in db.splitlines()
              if l and not l.startswith("#")))
    check("final accounts: admin, testuser, weakuser, testuser2",
          sorted(l.split("|")[0] for l in db.splitlines()
                 if l and not l.startswith("#")) == ["admin", "testuser", "testuser2", "weakuser"])
    check("no account left locked", "|LOCKED|" not in db)

    print("\n%d checks, %d failures -> %s" % (checks, len(failures),
          "ALL E2E CHECKS PASSED" if not failures else "FAILURES: %s" % failures))
    shutil.rmtree(WORKDIR, ignore_errors=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())