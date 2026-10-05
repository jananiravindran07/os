import { useEffect, useState, type FormEvent } from 'react'
import { FirebaseError } from 'firebase/app'
import { EmailAuthProvider, createUserWithEmailAndPassword, onAuthStateChanged, reauthenticateWithCredential, sendEmailVerification, sendPasswordResetEmail, signInWithEmailAndPassword, signOut, updatePassword, updateProfile, type Auth, type User } from 'firebase/auth'
import { addDoc, collection, doc, getDoc, getDocs, query, serverTimestamp, setDoc, updateDoc, where, type Firestore, type Timestamp } from 'firebase/firestore'
import { KeyRound, LogIn, LogOut, ShieldCheck, UserPlus } from 'lucide-react'
import { firebaseAuth, firebaseConfigured, firestore } from '@/lib/firebase'
import './AccountPortal.css'

type Role = 'admin' | 'user' | 'guest'
type Profile = { id: string; email: string; username: string; role: Role; status: 'active' | 'locked' }
type Resource = { name: string; required_role: Role | 'public'; description: string }
type AuditEvent = { id: string; action: string; created_at: Timestamp | null }
type Mode = 'login' | 'register' | 'recover' | 'change'

const resourceCatalog: Resource[] = [
  { name: 'Public guide', required_role: 'public', description: 'Available to every visitor.' },
  { name: 'Member workspace', required_role: 'user', description: 'Available to signed-in users and administrators.' },
  { name: 'Guest area', required_role: 'guest', description: 'Available to signed-in guests and administrators.' },
  { name: 'Administration', required_role: 'admin', description: 'Available only to administrators.' },
]

function passwordStrength(value: string) {
  return [value.length >= 10, /[a-z]/.test(value) && /[A-Z]/.test(value), /\d/.test(value), /[^A-Za-z0-9]/.test(value)].filter(Boolean).length
}

function assertStrongPassword(value: string) {
  if (passwordStrength(value) < 3) throw new Error('Use at least 10 characters and include uppercase, lowercase, a number, and a symbol.')
}

function errorMessage(reason: unknown) {
  if (reason instanceof FirebaseError) {
    if (reason.code === 'auth/invalid-credential' || reason.code === 'auth/wrong-password' || reason.code === 'auth/user-not-found') return 'Email or password is incorrect.'
    if (reason.code === 'auth/email-already-in-use') return 'An account already exists for this email address.'
    if (reason.code === 'auth/weak-password') return 'Choose a stronger password.'
    if (reason.code === 'auth/too-many-requests') return 'Too many attempts. Try again later.'
    if (reason.code === 'auth/unauthorized-continue-uri') return 'Add this website domain to Firebase Authentication authorized domains.'
    if (reason.code === 'permission-denied') return 'Firebase rejected this request. Check that the Firestore security rules are deployed.'
  }
  return reason instanceof Error ? reason.message : 'Something went wrong. Please try again.'
}

async function recordEvent(userId: string, action: string) {
  if (!firestore) return
  await addDoc(collection(firestore, 'audit_events'), { user_id: userId, action, created_at: serverTimestamp() })
}

async function loadEvents(userId: string) {
  if (!firestore) return []
  const snapshot = await getDocs(query(collection(firestore, 'audit_events'), where('user_id', '==', userId)))
  return snapshot.docs.map(item => ({ id: item.id, ...item.data() } as AuditEvent))
    .sort((left, right) => (right.created_at?.toMillis() ?? 0) - (left.created_at?.toMillis() ?? 0))
    .slice(0, 8)
}

export function AccountPortal() {
  const [mode, setMode] = useState<Mode>('login')
  const [session, setSession] = useState<User | null>(null)
  const [authReady, setAuthReady] = useState(!firebaseAuth)
  const [profile, setProfile] = useState<Profile | null>(null)
  const [profileLoading, setProfileLoading] = useState(false)
  const [resources, setResources] = useState<Resource[]>([])
  const [people, setPeople] = useState<Profile[]>([])
  const [events, setEvents] = useState<AuditEvent[]>([])
  const [email, setEmail] = useState('')
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [newPassword, setNewPassword] = useState('')
  const [role, setRole] = useState<'user' | 'guest'>('user')
  const [busy, setBusy] = useState(false)
  const [message, setMessage] = useState('')
  const [error, setError] = useState('')

  useEffect(() => {
    if (!firebaseAuth) return
    return onAuthStateChanged(firebaseAuth, user => {
      setSession(user)
      setAuthReady(true)
      setProfileLoading(Boolean(user) && mode !== 'register')
    })
  }, [mode])

  useEffect(() => {
    if (mode === 'register') return
    const activeSession = session
    const database = firestore
    const auth = firebaseAuth
    if (!activeSession || !database || !auth) return
    let cancelled = false
    async function loadAccount(currentUser: User, databaseClient: Firestore, authClient: Auth) {
      try {
        const snapshot = await getDoc(doc(databaseClient, 'profiles', currentUser.uid))
        if (!snapshot.exists()) throw new Error('Your account profile is missing. Sign out, then create your account again or contact the administrator.')
        const data = snapshot.data()
        const own: Profile = {
          id: currentUser.uid,
          email: currentUser.email ?? '',
          username: String(data.username ?? currentUser.displayName ?? currentUser.email ?? 'user'),
          role: (data.role ?? 'user') as Role,
          status: data.status === 'locked' ? 'locked' : 'active',
        }
        if (own.status === 'locked') {
          if (!cancelled) setError('This account is locked. Contact the site administrator.')
          await signOut(authClient)
          return
        }
        const ownEvents = await loadEvents(currentUser.uid)
        const allPeople = own.role === 'admin'
          ? (await getDocs(collection(databaseClient, 'profiles'))).docs.map(item => ({ id: item.id, ...item.data() } as Profile))
          : []
        if (cancelled) return
        setProfile(own)
        setResources(resourceCatalog.filter(item => item.required_role === 'public' || item.required_role === own.role || own.role === 'admin'))
        setEvents(ownEvents)
        setPeople(allPeople)
        await recordEvent(currentUser.uid, 'session_started')
      } catch (reason) {
        if (!cancelled) setError(errorMessage(reason))
      } finally {
        if (!cancelled) setProfileLoading(false)
      }
    }
    void loadAccount(activeSession, database, auth)
    return () => { cancelled = true }
  }, [session, mode])

  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault(); setBusy(true); setError(''); setMessage('')
    try {
      if (!firebaseAuth || !firestore) throw new Error('Firebase is not configured. Complete the setup steps in the website README.')
      if (mode === 'register') {
        assertStrongPassword(password)
        if (!/^[A-Za-z0-9_.-]{3,32}$/.test(username.trim())) throw new Error('Use 3–32 letters, numbers, dots, underscores or hyphens for the username.')
        const credential = await createUserWithEmailAndPassword(firebaseAuth, email.trim(), password)
        await updateProfile(credential.user, { displayName: username.trim() })
        const profileRef = doc(firestore, 'profiles', credential.user.uid)
        const existing = await getDoc(profileRef)
        if (!existing.exists()) await setDoc(profileRef, {
          email: credential.user.email ?? email.trim(), username: username.trim(), role, status: 'active', created_at: serverTimestamp(),
        })
        await sendEmailVerification(credential.user)
        setMessage('Verification email sent. Confirm your email address before signing in.')
        await signOut(firebaseAuth)
        setMode('login'); setPassword('')
      } else if (mode === 'login') {
        const credential = await signInWithEmailAndPassword(firebaseAuth, email.trim(), password)
        if (!credential.user.emailVerified) {
          await signOut(firebaseAuth)
          throw new Error('Verify your email using the link we sent before signing in.')
        }
        setPassword('')
      } else if (mode === 'recover') {
        const continueUrl = window.location.origin + window.location.pathname
        await sendPasswordResetEmail(firebaseAuth, email.trim(), { url: continueUrl, handleCodeInApp: false })
        setMessage('If an account exists for that email, a password reset link is on its way.')
      } else if (mode === 'change') {
        assertStrongPassword(newPassword)
        if (!session?.email) throw new Error('Sign in again before changing your password.')
        const credential = EmailAuthProvider.credential(session.email, password)
        await reauthenticateWithCredential(session, credential)
        await updatePassword(session, newPassword)
        await recordEvent(session.uid, 'password_changed')
        setEvents(await loadEvents(session.uid))
        setPassword(''); setNewPassword(''); setMessage('Password changed successfully.')
      }
    } catch (reason) {
      setError(errorMessage(reason))
    } finally { setBusy(false) }
  }

  async function logout() {
    setBusy(true); setError('')
    try {
      if (session) await recordEvent(session.uid, 'logout')
      if (firebaseAuth) await signOut(firebaseAuth)
      setMode('login'); setMessage('You are signed out.')
    } catch (reason) { setError(errorMessage(reason)) }
    finally { setBusy(false) }
  }

  async function setAccountStatus(person: Profile, status: 'active' | 'locked') {
    if (!session || !firestore) return
    setError(''); setMessage('')
    try {
      await updateDoc(doc(firestore, 'profiles', person.id), { status })
      setPeople(items => items.map(item => item.id === person.id ? { ...item, status } : item))
      setMessage(person.username + ' is now ' + status + '.')
      await recordEvent(session.uid, 'account_' + status + ':' + person.username)
      setEvents(await loadEvents(session.uid))
    } catch (reason) { setError(errorMessage(reason)) }
  }

  const setView = (next: Mode) => { setMode(next); setError(''); setMessage(''); setPassword(''); setNewPassword('') }
  const meter = <small className="account-hint">Password strength: {['too weak', 'weak', 'fair', 'good', 'strong'][passwordStrength(mode === 'change' ? newPassword : password)]}. Use 10+ characters with mixed case, a number and a symbol.</small>

  return (
    <section id="account" className="content-section account-section">
      <div className="section-shell">
        <p className="section-kicker">online account system</p>
        <h2 className="section-heading">Create an account. Sign in. Use your access.</h2>
        <p className="account-lede">Email accounts are verified and securely managed by Firebase Authentication. Firestore rules protect account profiles and security activity.</p>
        {!firebaseConfigured && <div className="account-alert" role="status">Setup is not complete yet. Add the Firebase web app configuration to the GitHub Actions variables before the site can accept accounts.</div>}
        <div className="account-layout">
          <div className="account-panel">
            {(!authReady || profileLoading) ? <p className="account-hint">Loading account…</p> : profile ? mode === 'change' ? <>
              <h3 className="account-subheading">Change your password</h3>
              <form className="account-form" onSubmit={submit}>
                <label>Current password<input required type="password" autoComplete="current-password" value={password} onChange={event => setPassword(event.target.value)}/></label>
                <label>New password<input required type="password" autoComplete="new-password" value={newPassword} onChange={event => setNewPassword(event.target.value)}/></label>
                {meter}
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !firebaseConfigured}><KeyRound size={17}/>{busy ? 'Please wait…' : 'Save password'}</button>
                <button type="button" className="account-text-button" onClick={() => setView('login')}>Cancel</button>
              </form>
            </> : <>
              <div className="account-welcome"><ShieldCheck size={23}/><div><strong>Welcome, {profile.username}</strong><span>Role: {profile.role} · Account: {profile.status}</span></div></div>
              <div className="account-actions"><button type="button" className="account-button" onClick={() => setView('change')}><KeyRound size={16}/>Change password</button><button type="button" className="account-button secondary" onClick={() => void logout()} disabled={busy}><LogOut size={16}/>Sign out</button></div>
              {error && <p className="account-alert error" role="alert">{error}</p>}
              {message && <p className="account-alert success" role="status">{message}</p>}
              <h3 className="account-subheading">Resources available to you</h3>
              {resources.length ? <ul className="resource-list">{resources.map(item => <li key={item.name}><strong>{item.name}</strong><span>{item.description}</span><small>{item.required_role} access</small></li>)}</ul> : <p className="account-hint">No resources are available to this role.</p>}
              <h3 className="account-subheading">Recent security activity</h3>
              {events.length ? <ul className="resource-list">{events.map(item => <li key={item.id}><strong>{item.action.replaceAll('_', ' ')}</strong><small>{item.created_at?.toDate().toLocaleString() ?? 'Just now'}</small></li>)}</ul> : <p className="account-hint">No recent events.</p>}
              {profile.role === 'admin' && <><h3 className="account-subheading">Account access control</h3><div className="people-list">{people.map(person => <div className="person-row" key={person.id}><span><strong>{person.username}</strong><small>{person.role} · {person.status}</small></span><button type="button" className="account-button small" disabled={person.id === profile.id} onClick={() => void setAccountStatus(person, person.status === 'locked' ? 'active' : 'locked')}>{person.status === 'locked' ? 'Unlock' : 'Lock'}</button></div>)}</div></>}
            </> : session ? <>
              <p className="account-alert error" role="alert">{error || 'Your account profile could not be loaded.'}</p>
              <button type="button" className="account-button secondary" onClick={() => void logout()} disabled={busy}><LogOut size={16}/>Sign out</button>
            </> : <>
              <div className="account-tabs">{([['login', 'Sign in'], ['register', 'Create account'], ['recover', 'Forgot password']] as const).map(([key, label]) => <button type="button" key={key} aria-pressed={mode === key} onClick={() => setView(key)}>{label}</button>)}</div>
              <form className="account-form" onSubmit={submit}>
                <label>Email<input required type="email" autoComplete="email" value={email} onChange={event => setEmail(event.target.value)}/></label>
                {mode === 'register' && <><label>Username<input required minLength={3} maxLength={32} pattern="[A-Za-z0-9_.-]+" title="Use letters, numbers, dots, underscores, or hyphens." autoComplete="username" value={username} onChange={event => setUsername(event.target.value)}/></label><label>Role<select value={role} onChange={event => setRole(event.target.value as 'user'|'guest')}><option value="user">User</option><option value="guest">Guest</option></select></label></>}
                {(mode === 'login' || mode === 'register') && <label>Password<input required type="password" autoComplete={mode === 'login' ? 'current-password' : 'new-password'} value={password} onChange={event => setPassword(event.target.value)}/></label>}
                {mode === 'change' && <label>New password<input required type="password" autoComplete="new-password" value={newPassword} onChange={event => setNewPassword(event.target.value)}/></label>}
                {(mode === 'register' || mode === 'change') && meter}
                {error && <p className="account-alert error" role="alert">{error}</p>}
                {message && <p className="account-alert success" role="status">{message}</p>}
                <button className="account-button" type="submit" disabled={busy || !firebaseConfigured}>{mode === 'login' ? <LogIn size={17}/> : <UserPlus size={17}/>}{busy ? 'Please wait…' : mode === 'login' ? 'Sign in' : mode === 'register' ? 'Create account' : 'Send reset link'}</button>
              </form>
            </>}
          </div>
          <aside className="account-side"><strong>How this works</strong><p>Firebase Authentication verifies email accounts and handles passwords. Forgot password sends a reset link directly to the address entered.</p><p>Firestore security rules protect profiles and activity. An administrator can lock or unlock accounts here after their role is assigned in Firebase Console.</p><p className="account-hint">Your old “apple” account was in the local C program. Register it here with an email address.</p></aside>
        </div>
      </div>
    </section>
  )
}
