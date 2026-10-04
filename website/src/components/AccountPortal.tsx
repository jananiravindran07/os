import { useEffect, useState, type FormEvent } from 'react'
import { KeyRound, LogIn, LogOut, ShieldCheck, UserPlus } from 'lucide-react'

type Role = 'admin' | 'user' | 'guest'
type Profile = { id: string; username: string; role: Role; status: 'active' | 'locked' }
type Session = { access_token: string; refresh_token: string; expires_at?: number; user: { id: string; email: string } }
type Resource = { name: string; required_role: Role | 'public'; description: string }

const env = import.meta.env
const supabaseUrl = env.VITE_SUPABASE_URL as string | undefined
const supabaseAnonKey = env.VITE_SUPABASE_ANON_KEY as string | undefined
const configured = Boolean(supabaseUrl && supabaseAnonKey)
const sessionKey = 'uac-supabase-session-v1'

async function supabase(path: string, options: { body?: unknown; token?: string; method?: string; auth?: boolean } = {}) {
  if (!configured) throw new Error('The account service is not configured yet. Complete the setup steps in the project README.')
  const url = (supabaseUrl as string).replace(/\/$/, '') + (options.auth ? '/auth/v1/' : '/rest/v1/') + path
  const response = await fetch(url, {
    method: options.method ?? (options.body ? 'POST' : 'GET'),
    headers: {
      apikey: supabaseAnonKey as string,
      Authorization: 'Bearer ' + (options.token ?? supabaseAnonKey),
      'Content-Type': 'application/json',
      Prefer: 'return=representation',
    },
    ...(options.body ? { body: JSON.stringify(options.body) } : {}),
  })
  const result = await response.json().catch(() => ({})) as Record<string, unknown>
  if (!response.ok) throw new Error(String(result.msg ?? result.message ?? result.error_description ?? result.error ?? 'The request failed. Please try again.'))
  return result
}

function getStoredSession(): Session | null {
  try { return JSON.parse(sessionStorage.getItem(sessionKey) ?? 'null') as Session | null } catch { return null }
}
function saveSession(value: Session | null) {
  if (value) sessionStorage.setItem(sessionKey, JSON.stringify(value))
  else sessionStorage.removeItem(sessionKey)
}
function passwordStrength(value: string) {
  return [value.length >= 10, /[a-z]/.test(value) && /[A-Z]/.test(value), /\d/.test(value), /[^A-Za-z0-9]/.test(value)].filter(Boolean).length
}
function assertStrongPassword(value: string) {
  if (passwordStrength(value) < 3) throw new Error('Use at least 10 characters and include uppercase, lowercase, a number, and a symbol.')
}

export function AccountPortal() {
  const [mode, setMode] = useState<'login' | 'register' | 'recover' | 'reset' | 'change'>('login')
  const [session, setSession] = useState<Session | null>(null)
  const [profile, setProfile] = useState<Profile | null>(null)
  const [resources, setResources] = useState<Resource[]>([])
  const [people, setPeople] = useState<Profile[]>([])
  const [email, setEmail] = useState('')
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [newPassword, setNewPassword] = useState('')
  const [role, setRole] = useState<'user' | 'guest'>('user')
  const [busy, setBusy] = useState(false)
  const [message, setMessage] = useState('')
  const [error, setError] = useState('')
  const [recoveryToken, setRecoveryToken] = useState('')

  useEffect(() => {
    const hash = new URLSearchParams(window.location.hash.replace(/^#/, ''))
    const token = hash.get('access_token')
    if (token && hash.get('type') === 'recovery') {
      setRecoveryToken(token)
      setMode('reset')
      window.history.replaceState(null, '', window.location.pathname + '#account')
    }
    setSession(getStoredSession())
  }, [])

  useEffect(() => {
    if (!session) { setProfile(null); setResources([]); setPeople([]); return }
    let cancelled = false
    async function load() {
      try {
        const rows = await supabase('profiles?select=id,username,role,status&id=eq.' + encodeURIComponent(session.user.id), { token: session.access_token }) as Profile[]
        const own = rows[0]
        if (!own || own.status === 'locked') {
          saveSession(null); setSession(null)
          throw new Error(own?.status === 'locked' ? 'This account is locked. Contact the site administrator.' : 'Your profile is still being created. Check your email, then sign in again.')
        }
        if (cancelled) return
        setProfile(own)
        const visible = await supabase('resources?select=name,required_role,description&order=name.asc', { token: session.access_token }) as Resource[]
        if (!cancelled) setResources(visible)
        await supabase('audit_events', { token: session.access_token, body: { action: 'session_started' } })
        if (own.role === 'admin') {
          const allPeople = await supabase('profiles?select=id,username,role,status&order=username.asc', { token: session.access_token }) as Profile[]
          if (!cancelled) setPeople(allPeople)
        }
      } catch (reason) {
        if (!cancelled) setError(reason instanceof Error ? reason.message : 'Could not load your account.')
      }
    }
    void load()
    return () => { cancelled = true }
  }, [session])

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault(); setBusy(true); setError(''); setMessage('')
    try {
      if (mode === 'register') {
        assertStrongPassword(password)
        const result = await supabase('signup', { auth: true, body: { email: email.trim(), password, data: { username: username.trim(), role } } }) as { access_token?: string; refresh_token?: string; expires_in?: number; user?: { id: string; email: string } }
        if (result.access_token && result.refresh_token && result.user) {
          const next = { access_token: result.access_token, refresh_token: result.refresh_token, expires_at: Date.now() / 1000 + (result.expires_in ?? 3600), user: result.user }
          saveSession(next); setSession(next); setMessage('Your account is ready.')
        } else {
          setMessage('Check your email to confirm your account, then return here to sign in.')
          setMode('login')
        }
      } else if (mode === 'login') {
        const result = await supabase('token?grant_type=password', { auth: true, body: { email: email.trim(), password } }) as Session & { expires_in?: number }
        const next = { ...result, expires_at: Date.now() / 1000 + (result.expires_in ?? 3600) }
        saveSession(next); setSession(next); setMessage('')
      } else if (mode === 'recover') {
        const redirect = window.location.origin + window.location.pathname + '#account'
        await supabase('recover?redirect_to=' + encodeURIComponent(redirect), { auth: true, body: { email: email.trim() } })
        setMessage('If that email has an account, a password reset link is on its way.')
      } else if (mode === 'reset') {
        assertStrongPassword(newPassword)
        await supabase('user', { auth: true, method: 'PUT', token: recoveryToken, body: { password: newPassword } })
        setRecoveryToken(''); setMode('login'); setPassword(''); setNewPassword('')
        setMessage('Password updated. Sign in with your new password.')
      } else if (mode === 'change') {
        assertStrongPassword(newPassword)
        await supabase('user', { auth: true, method: 'PUT', token: session?.access_token, body: { password: newPassword } })
        await supabase('audit_events', { token: session?.access_token, body: { action: 'password_changed' } })
        setPassword(''); setNewPassword(''); setMode('login')
        setMessage('Password changed successfully.')
      }
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Something went wrong. Please try again.')
    } finally { setBusy(false) }
  }

  async function logout() {
    setBusy(true); setError('')
    try {
      if (session) await supabase('logout', { auth: true, method: 'POST', token: session.access_token })
    } catch { /* Clear this browser session even if the network is unavailable. */ }
    saveSession(null); setSession(null); setProfile(null); setPeople([]); setMode('login'); setBusy(false)
    setMessage('You are signed out.')
  }

  async function setAccountStatus(person: Profile, status: 'active' | 'locked') {
    if (!session) return
    setError(''); setMessage('')
    try {
      await supabase('profiles?id=eq.' + encodeURIComponent(person.id), { method: 'PATCH', token: session.access_token, body: { status } })
      setPeople(items => items.map(item => item.id === person.id ? { ...item, status } : item))
      setMessage(person.username + ' is now ' + status + '.')
      await supabase('audit_events', { token: session.access_token, body: { action: 'account_' + status + ':' + person.username } })
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Could not update the account.') }
  }

  const setView = (next: typeof mode) => { setMode(next); setError(''); setMessage(''); setPassword(''); setNewPassword('') }
  const meter = <small className="account-hint">Password strength: {['too weak', 'weak', 'fair', 'good', 'strong'][passwordStrength(mode === 'change' || mode === 'reset' ? newPassword : password)]}. Use 10+ characters with mixed case, a number and a symbol.</small>

  return (
    <section id="account" className="content-section account-section">
      <div className="section-shell">
        <p className="section-kicker">online account system</p>
        <h2 className="section-heading">Create an account. Sign in. Use your access.</h2>
        <p className="account-lede">Accounts are stored by Supabase and work across devices. Resource permissions are enforced by database rules.</p>
        {!configured && <div className="account-alert" role="status">Setup is not complete yet. Add the Supabase project URL and public anon key to the GitHub Actions variables before the site can accept accounts.</div>}
        <div className="account-layout">
          <div className="account-panel">
            {profile ? <>
              <div className="account-welcome"><ShieldCheck size={23}/><div><strong>Welcome, {profile.username}</strong><span>Role: {profile.role} · Account: {profile.status}</span></div></div>
              <div className="account-actions"><button type="button" className="account-button" onClick={() => setView('change')}><KeyRound size={16}/>Change password</button><button type="button" className="account-button secondary" onClick={() => void logout()} disabled={busy}><LogOut size={16}/>Sign out</button></div>
              <h3 className="account-subheading">Resources available to you</h3>
              {resources.length ? <ul className="resource-list">{resources.map(item => <li key={item.name}><strong>{item.name}</strong><span>{item.description}</span><small>{item.required_role} access</small></li>)}</ul> : <p className="account-hint">No resources are available to this role.</p>}
              {profile.role === 'admin' && <><h3 className="account-subheading">Account access control</h3><div className="people-list">{people.map(person => <div className="person-row" key={person.id}><span><strong>{person.username}</strong><small>{person.role} · {person.status}</small></span><button type="button" className="account-button small" onClick={() => void setAccountStatus(person, person.status === 'locked' ? 'active' : 'locked')}>{person.status === 'locked' ? 'Unlock' : 'Lock'}</button></div>)}</div></>}
            </> : <>
              <div className="account-tabs">{([['login', 'Sign in'], ['register', 'Create account'], ['recover', 'Forgot password']] as const).map(([key, label]) => <button type="button" key={key} aria-pressed={mode === key} onClick={() => setView(key)}>{label}</button>)}</div>
              <form className="account-form" onSubmit={submit}>
                {mode !== 'reset' && <label>Email<input required type="email" autoComplete="email" value={email} onChange={event => setEmail(event.target.value)}/></label>}
                {mode === 'register' && <><label>Username<input required minLength={3} maxLength={32} autoComplete="username" value={username} onChange={event => setUsername(event.target.value)}/></label><label>Role<select value={role} onChange={event => setRole(event.target.value as 'user'|'guest')}><option value="user">User</option><option value="guest">Guest</option></select></label></>}
                {(mode === 'login' || mode === 'register') && <label>Password<input required type="password" autoComplete={mode === 'login' ? 'current-password' : 'new-password'} value={password} onChange={event => setPassword(event.target.value)}/></label>}
                {(mode === 'change' || mode === 'reset') && <label>New password<input required type="password" autoComplete="new-password" value={newPassword} onChange={event => setNewPassword(event.target.value)}/></label>}
                {(mode === 'register' || mode === 'change' || mode === 'reset') && meter}
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !configured}>{mode === 'login' ? <LogIn size={17}/> : <UserPlus size={17}/ >}{busy ? 'Please wait…' : mode === 'login' ? 'Sign in' : mode === 'register' ? 'Create account' : mode === 'recover' ? 'Send reset link' : 'Save password'}</button>
                {mode === 'change' && <button type="button" className="account-text-button" onClick={() => setView('login')}>Cancel password change</button>}
              </form>
            </>}
          </div>
          <aside className="account-side"><strong>How this works</strong><p>Your password is handled by Supabase Auth. Only a public browser key is used on this page; database row-level security protects profiles and resources.</p><p>Password recovery uses an email link. An administrator can lock or unlock accounts from this panel after being assigned the admin role in the Supabase SQL setup.</p><p className="account-hint">Your old “apple” account was in the local C program. It will need to be registered here with an email address.</p></aside>
        </div>
      </div>
    </section>
  )
}
