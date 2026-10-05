import { useEffect, useState, type FormEvent } from 'react'
import { Eye, EyeOff, KeyRound, LogIn, LogOut, ShieldCheck, UserPlus } from 'lucide-react'
import './AccountPortal.css'

type Role = 'admin' | 'user' | 'guest'
type Profile = { id: string; username: string; role: Role; status: 'active' | 'locked'; failed_attempts?: number }
type Session = { access_token: string; refresh_token: string; expires_at?: number; user: { id: string; email: string } }
type AuthSession = Session & { expires_in?: number }
type Resource = { name: string; required_role: Role | 'public'; description: string }
type AuditEvent = { action: string; created_at: string }
type MfaFactor = { id: string; status?: string; factor_type?: string; type?: string; totp?: { qr_code?: string; secret?: string } }
type Mode = 'login' | 'register' | 'confirm-email' | 'mfa-setup' | 'mfa-challenge' | 'recover' | 'reset' | 'change'

const env = import.meta.env
const supabaseUrl = env.VITE_SUPABASE_URL as string | undefined
const supabaseAnonKey = env.VITE_SUPABASE_ANON_KEY as string | undefined
const configured = Boolean(supabaseUrl && supabaseAnonKey)
const sessionKey = 'uac-supabase-session-v1'

async function supabase(path: string, options: { body?: unknown; token?: string; method?: string; auth?: boolean; fn?: boolean } = {}) {
  if (!configured) throw new Error('The account service is not configured yet. Complete the setup steps in the project README.')
  const url = (supabaseUrl as string).replace(/\/$/, '') + (options.fn ? '/functions/v1/' : options.auth ? '/auth/v1/' : '/rest/v1/') + path
  const response = await fetch(url, {
    method: options.method ?? (options.body ? 'POST' : 'GET'),
    headers: {
      apikey: supabaseAnonKey as string,
      ...(options.token ? { Authorization: 'Bearer ' + options.token } : {}),
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
function tokenAssuranceLevel(token: string) {
  try {
    const payload = token.split('.')[1].replace(/-/g, '+').replace(/_/g, '/')
    return (JSON.parse(atob(payload)) as { aal?: string }).aal ?? 'aal1'
  } catch { return 'aal1' }
}
function sessionWithExpiry(value: AuthSession): Session {
  return { ...value, expires_at: value.expires_at ?? Date.now() / 1000 + (value.expires_in ?? 3600) }
}
function passwordStrength(value: string) {
  return [value.length >= 10, /[a-z]/.test(value) && /[A-Z]/.test(value), /\d/.test(value), /[^A-Za-z0-9]/.test(value)].filter(Boolean).length
}
function assertStrongPassword(value: string) {
  if (passwordStrength(value) < 3) throw new Error('Use at least 10 characters and include uppercase, lowercase, a number, and a symbol.')
}

function PasswordField({ label, value, onChange, autoComplete }: { label: string; value: string; onChange: (value: string) => void; autoComplete: string }) {
  const [visible, setVisible] = useState(false)
  return <label>{label}<span className="password-input-wrap"><input required type={visible ? 'text' : 'password'} autoComplete={autoComplete} value={value} onChange={event => onChange(event.target.value)}/><button type="button" className="password-visibility" aria-label={visible ? 'Hide password' : 'Show password'} aria-pressed={visible} onClick={() => setVisible(show => !show)}>{visible ? <EyeOff size={17}/> : <Eye size={17}/>}</button></span></label>
}

export function AccountPortal() {
  const [mode, setMode] = useState<Mode>('login')
  const [session, setSession] = useState<Session | null>(null)
  const [pendingSession, setPendingSession] = useState<Session | null>(null)
  const [mfaFactorId, setMfaFactorId] = useState('')
  const [mfaChallengeId, setMfaChallengeId] = useState('')
  const [mfaQrCode, setMfaQrCode] = useState('')
  const [mfaSecret, setMfaSecret] = useState('')
  const [mfaCode, setMfaCode] = useState('')
  const [profile, setProfile] = useState<Profile | null>(null)
  const [resources, setResources] = useState<Resource[]>([])
  const [people, setPeople] = useState<Profile[]>([])
  const [events, setEvents] = useState<AuditEvent[]>([])
  const [email, setEmail] = useState('')
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [newPassword, setNewPassword] = useState('')
  const [confirmationCode, setConfirmationCode] = useState('')
  const [role, setRole] = useState<'user' | 'guest' | 'admin'>('user')
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
    const saved = getStoredSession()
    if (saved && tokenAssuranceLevel(saved.access_token) !== 'aal2') {
      saveSession(null)
      setMessage('Please sign in again to complete two-step verification.')
      return
    }
    setSession(saved)
  }, [])

  useEffect(() => {
    const activeSession = session
    if (!activeSession) { setProfile(null); setResources([]); setPeople([]); return }
    let cancelled = false
    async function load(currentSession: Session) {
      try {
        if (currentSession.expires_at && currentSession.expires_at < Date.now() / 1000 + 60) {
          const fresh = await supabase('token?grant_type=refresh_token', { auth: true, body: { refresh_token: currentSession.refresh_token } }) as unknown as AuthSession
          const renewed = sessionWithExpiry(fresh)
          if (!cancelled && tokenAssuranceLevel(renewed.access_token) === 'aal2') { saveSession(renewed); setSession(renewed) }
          else if (!cancelled) { saveSession(null); setSession(null); setMessage('Please sign in again to complete two-step verification.') }
          return
        }
        const rows = await supabase('profiles?select=id,username,role,status,failed_attempts&id=eq.' + encodeURIComponent(currentSession.user.id), { token: currentSession.access_token }) as unknown as Profile[]
        const own = rows[0]
        if (!own || own.status === 'locked') {
          saveSession(null); setSession(null)
          throw new Error(own?.status === 'locked' ? 'This account is locked. Contact the site administrator.' : 'Your profile is still being created. Check your email, then sign in again.')
        }
        if (cancelled) return
        setProfile(own)
        const visible = await supabase('resources?select=name,required_role,description&order=name.asc', { token: currentSession.access_token }) as unknown as Resource[]
        if (!cancelled) setResources(visible)
        await supabase('audit_events', { token: currentSession.access_token, body: { action: 'session_started' } })
        const recent = await supabase('audit_events?select=action,created_at&order=created_at.desc&limit=8', { token: currentSession.access_token }) as unknown as AuditEvent[]
        if (!cancelled) setEvents(recent)
        if (own.role === 'admin') {
          const allPeople = await supabase('profiles?select=id,username,role,status,failed_attempts&order=username.asc', { token: currentSession.access_token }) as unknown as Profile[]
          if (!cancelled) setPeople(allPeople)
        }
      } catch (reason) {
        if (!cancelled) setError(reason instanceof Error ? reason.message : 'Could not load your account.')
      }
    }
    void load(activeSession)
    return () => { cancelled = true }
  }, [session])

  async function beginSecondFactor(rawSession: AuthSession) {
    const current = sessionWithExpiry(rawSession)
    const factorResult = await supabase('factors', { auth: true, token: current.access_token }) as unknown as { all?: MfaFactor[]; totp?: MfaFactor[]; factors?: MfaFactor[] }
    const factors = Array.isArray(factorResult.all) ? factorResult.all : Array.isArray(factorResult.totp) ? factorResult.totp : Array.isArray(factorResult.factors) ? factorResult.factors : []
    const verified = factors.find(factor => (factor.factor_type ?? factor.type) === 'totp' && factor.status === 'verified')
    setPendingSession(current)
    if (verified) {
      const challenge = await supabase('factors/' + encodeURIComponent(verified.id) + '/challenge', { auth: true, token: current.access_token, body: {} }) as unknown as { id: string }
      setMfaFactorId(verified.id)
      setMfaChallengeId(challenge.id)
      setMfaQrCode('')
      setMfaSecret('')
      setMfaCode('')
      setMode('mfa-challenge')
      setMessage('Enter the current code from your authenticator app.')
      return
    }
    for (const factor of factors.filter(item => (item.factor_type ?? item.type) === 'totp' && item.status === 'unverified')) {
      await supabase('factors/' + encodeURIComponent(factor.id), { auth: true, token: current.access_token, method: 'DELETE' })
    }
    const enrolled = await supabase('factors', { auth: true, token: current.access_token, body: { factor_type: 'totp', friendly_name: 'Website two-step verification' } }) as unknown as MfaFactor
    if (!enrolled.id || !enrolled.totp?.qr_code || !enrolled.totp.secret) throw new Error('Supabase did not return an authenticator setup code. Please try signing in again.')
    setMfaFactorId(enrolled.id)
    setMfaChallengeId('')
    setMfaQrCode(enrolled.totp.qr_code)
    setMfaSecret(enrolled.totp.secret)
    setMfaCode('')
    setMode('mfa-setup')
    setMessage('')
  }

  async function completeSecondFactor() {
    if (!pendingSession || !mfaFactorId) throw new Error('Your sign-in step expired. Please sign in again.')
    let challengeId = mfaChallengeId
    if (!challengeId) {
      const challenge = await supabase('factors/' + encodeURIComponent(mfaFactorId) + '/challenge', { auth: true, token: pendingSession.access_token, body: {} }) as unknown as { id: string }
      challengeId = challenge.id
    }
    const result = await supabase('factors/' + encodeURIComponent(mfaFactorId) + '/verify', {
      auth: true,
      token: pendingSession.access_token,
      body: { challenge_id: challengeId, code: mfaCode.trim() },
    }) as unknown as AuthSession
    if (!result.access_token || !result.refresh_token || !result.user) throw new Error('The code was not accepted. Check your authenticator app and try the current code.')
    const verified = sessionWithExpiry(result)
    if (tokenAssuranceLevel(verified.access_token) !== 'aal2') throw new Error('Supabase did not confirm the second factor. Please try again.')
    saveSession(verified)
    setPendingSession(null)
    setSession(verified)
    setMode('login')
    setMfaCode('')
    setMfaQrCode('')
    setMfaSecret('')
    setMessage('Two-step verification complete.')
  }

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault(); setBusy(true); setError(''); setMessage('')
    try {
      if (mode === 'register') {
        assertStrongPassword(password)
        const result = await supabase('signup', { auth: true, body: { email: email.trim(), password, data: { username: username.trim(), role } } }) as unknown as AuthSession
        if (result.access_token && result.refresh_token && result.user) {
          await beginSecondFactor(result)
        } else {
          setConfirmationCode('')
          setMode('confirm-email')
          setMessage(role === 'admin'
            ? 'Enter the six-digit code sent to your email. Admin access must be granted separately by the site owner.'
            : 'Enter the six-digit code sent to your email to confirm your account.')
        }
      } else if (mode === 'confirm-email') {
        const result = await supabase('verify', { auth: true, body: { type: 'signup', email: email.trim(), token: confirmationCode.trim() } }) as unknown as AuthSession
        await beginSecondFactor(result)
      } else if (mode === 'login') {
        const result = await supabase('login', { fn: true, body: { email: email.trim(), password } }) as unknown as AuthSession
        await beginSecondFactor(result)
      } else if (mode === 'mfa-setup' || mode === 'mfa-challenge') {
        if (!/^\d{6}$/.test(mfaCode.trim())) throw new Error('Enter the six-digit code from your authenticator app.')
        await completeSecondFactor()
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
        const currentSession = session
        if (!currentSession) throw new Error('Please sign in again before changing your password.')
        await supabase('login', { fn: true, body: { email: currentSession.user.email, password } })
        await supabase('user', { auth: true, method: 'PUT', token: currentSession.access_token, body: { password: newPassword } })
        await supabase('audit_events', { token: currentSession.access_token, body: { action: 'password_changed' } })
        setEvents(await supabase('audit_events?select=action,created_at&order=created_at.desc&limit=8', { token: currentSession.access_token }) as unknown as AuditEvent[])
        setPassword(''); setNewPassword('')
        setMessage('Password changed successfully.')
      }
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Something went wrong. Please try again.')
    } finally { setBusy(false) }
  }

  async function resendConfirmationCode() {
    setBusy(true); setError(''); setMessage('')
    try {
      await supabase('resend', { auth: true, body: { type: 'signup', email: email.trim() } })
      setMessage('A new confirmation code has been sent. Check your inbox and spam folder.')
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Could not resend the code.') }
    finally { setBusy(false) }
  }

  async function logout() {
    setBusy(true); setError('')
    try {
      if (session) {
        await supabase('audit_events', { token: session.access_token, body: { action: 'logout' } })
        await supabase('logout', { auth: true, method: 'POST', token: session.access_token })
      }
    } catch { /* Clear this browser session even if the network is unavailable. */ }
    saveSession(null); setSession(null); setProfile(null); setPeople([]); setMode('login'); setBusy(false)
    setMessage('You are signed out.')
  }

  async function setAccountStatus(person: Profile, status: 'active' | 'locked') {
    if (!session) return
    setError(''); setMessage('')
    try {
      await supabase('profiles?id=eq.' + encodeURIComponent(person.id), { method: 'PATCH', token: session.access_token, body: { status, failed_attempts: status === 'active' ? 0 : 3 } })
      setPeople(items => items.map(item => item.id === person.id ? { ...item, status, failed_attempts: status === 'active' ? 0 : 3 } : item))
      setMessage(person.username + ' is now ' + status + '.')
      await supabase('audit_events', { token: session.access_token, body: { action: 'account_' + status + ':' + person.username } })
      setEvents(await supabase('audit_events?select=action,created_at&order=created_at.desc&limit=8', { token: session.access_token }) as unknown as AuditEvent[])
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Could not update the account.') }
  }

  const setView = (next: Mode) => { setMode(next); setError(''); setMessage(''); setPassword(''); setNewPassword('') }
  const cancelMfa = () => { setPendingSession(null); setMfaFactorId(''); setMfaChallengeId(''); setMfaCode(''); setMfaQrCode(''); setMfaSecret(''); setMode('login'); setError(''); setMessage('Sign in again when you are ready to continue.') }
  const meter = <small className="account-hint">Password strength: {['too weak', 'weak', 'fair', 'good', 'strong'][passwordStrength(mode === 'change' || mode === 'reset' ? newPassword : password)]}. Use 10+ characters with mixed case, a number and a symbol.</small>
  const qrImage = mfaQrCode ? 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(mfaQrCode) : ''

  return (
    <section id="account" className="content-section account-section">
      <div className="section-shell">
        <p className="section-kicker">online account system</p>
        <h2 className="section-heading">Create an account. Sign in. Use your access.</h2>
        <p className="account-lede">Accounts are stored by Supabase and work across devices. Sign-ins use an authenticator code as a second step.</p>
        {!configured && <div className="account-alert" role="status">Setup is not complete yet. Add the Supabase project URL and public anon key to the GitHub Actions variables before the site can accept accounts.</div>}
        <div className="account-layout">
          <div className="account-panel">
            {mode === 'confirm-email' ? <>
              <h3 className="account-subheading">Confirm your email</h3>
              <p className="account-hint">Enter the six-digit code sent to {email}. After confirmation, you’ll set up two-step verification with an authenticator app.</p>
              <form className="account-form" onSubmit={submit}>
                <label>Email confirmation code<input required inputMode="numeric" autoComplete="one-time-code" maxLength={6} pattern="[0-9]{6}" value={confirmationCode} onChange={event => setConfirmationCode(event.target.value.replace(/\D/g, '').slice(0, 6))}/></label>
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !configured}>{busy ? 'Please wait…' : 'Confirm email'}</button>
                <button type="button" className="account-text-button" onClick={() => void resendConfirmationCode()} disabled={busy}>Resend code</button>
              </form>
            </> : mode === 'mfa-setup' || mode === 'mfa-challenge' ? <>
              <h3 className="account-subheading">{mode === 'mfa-setup' ? 'Set up two-step verification' : 'Enter your authenticator code'}</h3>
              {mode === 'mfa-setup' ? <>
                <p className="account-hint">Scan this QR code with an authenticator app such as Google Authenticator, Microsoft Authenticator, Authy, or Apple Passwords. You’ll need a fresh code from the app each time you sign in.</p>
                {qrImage && <img className="mfa-qr" src={qrImage} alt="QR code for authenticator app setup"/>}
                <p className="account-hint">If you can’t scan it, enter this setup key in your authenticator app:</p>
                <code className="mfa-secret">{mfaSecret}</code>
              </> : <p className="account-hint">Open your authenticator app and enter the current six-digit code for this website.</p>}
              <form className="account-form" onSubmit={submit}>
                <label>Six-digit authenticator code<input required inputMode="numeric" autoComplete="one-time-code" maxLength={6} pattern="[0-9]{6}" value={mfaCode} onChange={event => setMfaCode(event.target.value.replace(/\D/g, '').slice(0, 6))}/></label>
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !configured}>{busy ? 'Please wait…' : mode === 'mfa-setup' ? 'Enable two-step verification' : 'Verify and sign in'}</button>
                <button type="button" className="account-text-button" onClick={cancelMfa}>Cancel</button>
              </form>
            </> : profile ? mode === 'change' ? <>
              <h3 className="account-subheading">Change your password</h3>
              <form className="account-form" onSubmit={submit}>
                <PasswordField label="Current password" autoComplete="current-password" value={password} onChange={setPassword}/>
                <PasswordField label="New password" autoComplete="new-password" value={newPassword} onChange={setNewPassword}/>
                {meter}
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !configured}><KeyRound size={17}/>{busy ? 'Please wait…' : 'Save password'}</button>
                <button type="button" className="account-text-button" onClick={() => setView('login')}>Cancel</button>
              </form>
            </> : <>
              <div className="account-welcome"><ShieldCheck size={23}/><div><strong>Welcome, {profile.username}</strong><span>Role: {profile.role} · Account: {profile.status} · Two-step verification on</span></div></div>
              <div className="account-actions"><button type="button" className="account-button" onClick={() => setView('change')}><KeyRound size={16}/>Change password</button><button type="button" className="account-button secondary" onClick={() => void logout()} disabled={busy}><LogOut size={16}/>Sign out</button></div>
              {error && <p className="account-alert error" role="alert">{error}</p>}
              {message && <p className="account-alert success" role="status">{message}</p>}
              <h3 className="account-subheading">Resources available to you</h3>
              {resources.length ? <ul className="resource-list">{resources.map(item => <li key={item.name}><strong>{item.name}</strong><span>{item.description}</span><small>{item.required_role} access</small></li>)}</ul> : <p className="account-hint">No resources are available to this role.</p>}
              <h3 className="account-subheading">Recent security activity</h3>
              {events.length ? <ul className="resource-list">{events.map((event, index) => <li key={event.created_at + index}><strong>{event.action.replaceAll('_', ' ')}</strong><small>{new Date(event.created_at).toLocaleString()}</small></li>)}</ul> : <p className="account-hint">No recent events.</p>}
              {profile.role === 'admin' && <><h3 className="account-subheading">Account access control</h3><div className="people-list">{people.map(person => <div className="person-row" key={person.id}><span><strong>{person.username}</strong><small>{person.role} · {person.status} · {person.failed_attempts ?? 0} failed</small></span><button type="button" className="account-button small" disabled={person.id === profile.id} onClick={() => void setAccountStatus(person, person.status === 'locked' ? 'active' : 'locked')}>{person.status === 'locked' ? 'Unlock' : 'Lock'}</button></div>)}</div></>}
            </> : <>
              <div className="account-tabs">{([['login', 'Sign in'], ['register', 'Create account'], ['recover', 'Forgot password']] as const).map(([key, label]) => <button type="button" key={key} aria-pressed={mode === key} onClick={() => setView(key)}>{label}</button>)}</div>
              <form className="account-form" onSubmit={submit}>
                {mode !== 'reset' && <label>Email<input required type="email" autoComplete="email" value={email} onChange={event => setEmail(event.target.value)}/></label>}
                {mode === 'register' && <>
                  <label>Username<input required minLength={3} maxLength={32} pattern="[A-Za-z0-9_.-]+" title="Use letters, numbers, dots, underscores, or hyphens." autoComplete="username" value={username} onChange={event => setUsername(event.target.value)}/></label>
                  <label>Role<select value={role} onChange={event => setRole(event.target.value as 'user'|'guest'|'admin')}><option value="user">User</option><option value="guest">Guest</option><option value="admin">Admin (owner approval required)</option></select></label>
                  {role === 'admin' && <small className="account-hint">For security, public sign-ups cannot grant themselves admin access. Your account will start as a regular user; the site owner must assign the admin role.</small>}
                </>}
                {mode === 'login' && <PasswordField label="Password" autoComplete="current-password" value={password} onChange={setPassword}/>}
                {mode === 'register' && <><PasswordField label="Password" autoComplete="new-password" value={password} onChange={setPassword}/>{meter}</>}
                {(mode === 'change' || mode === 'reset') && <><PasswordField label="New password" autoComplete="new-password" value={newPassword} onChange={setNewPassword}/>{meter}</>}
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                {mode === 'login' && error && <button type="button" className="account-text-button" onClick={() => setView('recover')}>Forgot your password?</button>}
                <button className="account-button" type="submit" disabled={busy || !configured}>{mode === 'login' ? <LogIn size={17}/> : <UserPlus size={17}/ >}{busy ? 'Please wait…' : mode === 'login' ? 'Sign in' : mode === 'register' ? 'Create account' : mode === 'recover' ? 'Send reset link' : 'Save password'}</button>
              </form>
            </>}
          </div>
          <aside className="account-side"><strong>How this works</strong><p>Your password is handled by Supabase Auth. Sign-in also requires a time-based code from an authenticator app. Database row-level security enforces the second-factor check before protected resources are available.</p><p>Password recovery uses an email link. Admin privileges are granted by the site owner, never by public sign-up.</p></aside>
        </div>
      </div>
    </section>
  )
}
