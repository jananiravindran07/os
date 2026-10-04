import { useState } from 'react'
import { Check, ChevronDown, ChevronRight, Clipboard, FileCode2, FolderOpen, Terminal } from 'lucide-react'
import { SectionHeading } from '@/components/SectionHeading'

type CodeEntry = { name: string; purpose: string; snippet: string }
const files: CodeEntry[] = [
  { name: 'main.c', purpose: 'Starts the program, initializes the database and log, then displays and dispatches the main menu.', snippet: `int main(void) {\n    logger_init(LOG_DEFAULT_FILE);\n    db_init();\n    session_init();\n    main_menu();\n    return EXIT_SUCCESS;\n}` },
  { name: 'auth.c / auth.h', purpose: 'Registers accounts, checks credentials, handles login, logout, password changes and security-question recovery.', snippet: `int auth_register(void);\nint auth_login(void);\nvoid auth_logout(void);\nvoid auth_forgot_password(void);\nint auth_change_password(void);` },
  { name: 'user.c / user.h', purpose: 'Defines the User record, account roles and status, and provides profile and user-table helpers.', snippet: `typedef struct {\n    char username[MAX_USERNAME];\n    char password_hash[MAX_HASH];\n    Role role;\n    int failed_attempts;\n    AccountStatus status;\n} User;` },
  { name: 'password.c / password.h', purpose: 'Implements password policy, strength feedback, salted SHA-256 and PBKDF2 password hashing.', snippet: `int password_generate_salt(char *out, size_t out_size);\nint password_hash(const char *password,\n                  const char *salt_hex, int iterations,\n                  char *out, size_t out_size);\nint password_verify(const char *password,\n                    const char *stored_hash);` },
  { name: 'session.c / session.h', purpose: 'Creates the current session, records activity time, reports session status and ends idle sessions.', snippet: `if (difftime(time(NULL), last_activity)\n        >= SESSION_TIMEOUT_SECONDS) {\n    logger_write(username, EV_SESSION_TIMEOUT, NULL);\n    session_end(SESSION_END_TIMEOUT, username);\n}` },
  { name: 'access_control.c / access_control.h', purpose: 'Stores the role-to-resource permission matrix and reports ACCESS GRANTED or ACCESS DENIED.', snippet: `bool allowed = access_allowed(\n    session_role(), RES_VIEW_LOGS);\n\nif (allowed) {\n    access_show_resource(RES_VIEW_LOGS);\n}` },
  { name: 'admin.c / admin.h', purpose: 'Provides administrator tools to list, add, remove and update accounts, unlock users and read logs.', snippet: `if (user->status == ACCOUNT_LOCKED) {\n    user->status = ACCOUNT_ACTIVE;\n    user->failed_attempts = 0;\n    db_save_user(user);\n    logger_write(actor, EV_ACCOUNT_UNLOCKED,\n                 user->username);\n}` },
  { name: 'logger.c / logger.h', purpose: 'Writes timestamped authentication, access-control, session and account events to audit.log.', snippet: `time_t now = time(NULL);\nstruct tm *local = localtime(&now);\nstrftime(stamp, sizeof stamp,\n         "%Y-%m-%d %H:%M", local);\nfprintf(log, "%s | %s | %s\\n",\n        stamp, actor, event);` },
  { name: 'database.c / database.h', purpose: 'Reads, searches, inserts and updates user records stored in the pipe-separated users.txt file.', snippet: `while (fgets(line, sizeof line, file)) {\n    parse_user_record(line, &user);\n    if (strcmp(user.username, wanted) == 0)\n        return true;\n}` },
  { name: 'utils.c / utils.h', purpose: 'Collects shared input, string, formatting and timestamp helpers used by the other modules.', snippet: `bool utils_read_int(const char *prompt,\n                    int min, int max, int *out) {\n    /* display prompt, validate range */\n    return read_and_check(prompt, min, max, out);\n}` },
  { name: 'users.txt', purpose: 'Stores one user record per line: username, salted hash, role, failed attempts and account status.', snippet: `admin|pbkdf2_sha256$100000$salt$hash|ADMIN|0|ACTIVE\nrahul|pbkdf2_sha256$100000$salt$hash|USER|0|ACTIVE` },
  { name: 'audit.log', purpose: 'Keeps an append-only record of system start, login, access, lockout, role and session events.', snippet: `2026-10-04 17:10 | admin | LOGIN_SUCCESS\n2026-10-04 17:11 | rahul | ACCESS_DENIED` },
  { name: 'README.md', purpose: 'Explains the project, its C modules, build steps, security behavior and verification.', snippet: `gcc -std=c99 -Wall -Wextra -O2 \\\n    -o uac main.c auth.c user.c password.c \\\n    session.c access_control.c admin.c logger.c \\\n    database.c utils.c` },
]

const concepts = ['Structures', 'Functions', 'Arrays', 'Strings', 'Pointers', 'File handling', 'Conditionals', 'Loops', 'Enumerations', 'Time functions', 'Dynamic memory', 'Modular programming', 'fopen()', 'fclose()', 'fprintf()', 'fscanf()', 'fgets()', 'fputs()']
const compileCommand = 'gcc -std=c99 -Wall -Wextra -O2 -o user_auth_system main.c auth.c user.c password.c session.c access_control.c admin.c logger.c database.c utils.c && ./user_auth_system'

export function CodeStructure() {
  const [selected, setSelected] = useState(files[0])
  const [copied, setCopied] = useState(false)
  const [treeOpen, setTreeOpen] = useState(true)
  const copyCommand = async () => {
    await navigator.clipboard.writeText(compileCommand)
    setCopied(true)
    window.setTimeout(() => setCopied(false), 1800)
  }

  return (
    <section id="code" className="content-section code-section">
      <div className="section-shell">
        <p className="section-kicker">a small, modular C project</p>
        <SectionHeading>Explore the code</SectionHeading>
        <p className="code-lede">Each module has a focused responsibility. Select a file to see where its work fits.</p>
        <div className="code-explorer">
          <aside className="file-tree">
            <div className="tree-title"><FolderOpen size={16}/> PROJECT FILES</div>
            <button className="tree-folder" onClick={() => setTreeOpen(!treeOpen)} aria-expanded={treeOpen}>{treeOpen ? <ChevronDown size={15}/> : <ChevronRight size={15}/>} UserAuthenticationSystem/</button>
            {treeOpen && <div className="tree-files">{files.map(file => <button className={selected.name === file.name ? 'tree-file is-selected' : 'tree-file'} key={file.name} onClick={() => setSelected(file)}><FileCode2 size={14}/>{file.name}</button>)}</div>}
          </aside>
          <article className="file-detail">
            <div className="file-detail-top"><span><FileCode2 size={15}/>{selected.name}</span><span className="file-language">C / TEXT</span></div>
            <div className="file-purpose"><span>PURPOSE OF THIS FILE</span><p>{selected.purpose}</p></div>
            <pre className="code-snippet"><code>{selected.snippet}</code></pre>
          </article>
        </div>

        <div className="code-info-grid">
          <div className="code-info-card"><div className="panel-title"><span>C</span><strong>C CONCEPTS USED</strong></div><div className="concept-cloud">{concepts.map(item => <span key={item}>{item}</span>)}</div></div>
          <div className="code-info-card"><div className="panel-title"><FileCode2 size={15}/><strong>FILE FORMATS</strong></div><div className="format-block"><b>users.txt · pipe separated</b><p><span>Username</span><span>Password Hash</span><span>Role</span><span>Failed Login Attempts</span><span>Account Status</span></p><code>admin|pbkdf2_sha256$100000$salt$hash|ADMIN|0|ACTIVE</code></div><div className="format-block"><b>audit.log · event record</b><code>2026-10-04 17:10 | admin | LOGIN_SUCCESS</code></div></div>
        </div>

        <div className="compile-card"><div><span className="compile-kicker"><Terminal size={15}/> COMPILE &amp; RUN</span><code>{compileCommand}</code></div><button onClick={() => void copyCommand()} aria-label="Copy compile command">{copied ? <Check size={16}/> : <Clipboard size={16}/>} {copied ? 'Copied' : 'Copy'}</button></div>
      </div>
    </section>
  )
}
