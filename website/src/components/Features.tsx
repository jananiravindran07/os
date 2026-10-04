import { KeyRound, LockKeyhole, ShieldCheck, UserRoundPlus, LogOut, ShieldAlert, FileText, UsersRound, Timer, RotateCcw, Sparkles } from 'lucide-react'
import { SectionHeading } from '@/components/SectionHeading'

type FeatureGroup = { title: string; icon: typeof KeyRound; cards: { name: string; description: string; icon: typeof KeyRound }[] }
const groups: FeatureGroup[] = [
  {
    title: 'Authentication', icon: KeyRound, cards: [
      { name: 'User Registration', description: 'Create an account with a username, password and role, then save it in the user database.', icon: UserRoundPlus },
      { name: 'User Login', description: 'Verify credentials and show a clear success or invalid-credentials message.', icon: ShieldCheck },
      { name: 'Logout', description: 'End the active session, clear the current user and return to the main menu.', icon: LogOut },
    ],
  },
  {
    title: 'Password Security', icon: LockKeyhole, cards: [
      { name: 'Password Hashing', description: 'Store a salted PBKDF2-SHA256 hash instead of the original password.', icon: KeyRound },
      { name: 'Password Strength', description: 'Check length and character variety before accepting a new password.', icon: ShieldCheck },
      { name: 'Change Password', description: 'Verify the current password before saving a replacement.', icon: RotateCcw },
      { name: 'Forgot Password', description: 'Recover access by answering the account’s security question.', icon: LockKeyhole },
    ],
  },
  {
    title: 'Security', icon: ShieldAlert, cards: [
      { name: 'Failed Login Tracking', description: 'Count incorrect attempts and show progress toward the limit of three.', icon: Sparkles },
      { name: 'Account Lockout', description: 'Lock an account after three failed login attempts.', icon: ShieldAlert },
      { name: 'Account Unlock', description: 'Let an administrator restore access to a locked account.', icon: ShieldCheck },
      { name: 'Role-Based Access', description: 'Use ADMIN, USER and GUEST roles to control protected actions.', icon: UsersRound },
      { name: 'Sessions & Audit Log', description: 'Track active sessions, timeouts and security events in the log.', icon: Timer },
      { name: 'File-Based Database', description: 'Keep account records in users.txt and audit events in audit.log.', icon: FileText },
    ],
  },
]

export function Features() {
  return (
    <section id="features" className="content-section features-section">
      <div className="section-shell">
        <p className="section-kicker">the pieces that make it work</p>
        <SectionHeading>Security, one layer at a time</SectionHeading>
        <p className="features-lede">Each part has one job, from checking a password to deciding whether a resource is available.</p>
        <div className="feature-groups">
          {groups.map(({ title, icon: GroupIcon, cards }) => (
            <div className="feature-group" key={title}>
              <h3 className="feature-group-title"><GroupIcon size={19} strokeWidth={1.8} />{title}</h3>
              <div className={`feature-grid feature-grid-${title.toLowerCase().replaceAll(' ', '-')}`}>
                {cards.map(({ name, description, icon: FeatureIcon }, index) => {
                  return (
                    <article className="feature-card" key={name} style={{ ['--card-turn' as string]: `${(index % 3 - 1) * 1.3}deg` }}>
                      <div className="feature-card-top"><FeatureIcon size={23} strokeWidth={1.7} /><span className="feature-index">{String(index + 1).padStart(2, '0')}</span></div>
                      <h4>{name}</h4>
                      <p>{description}</p>
                    </article>
                  )
                })}
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  )
}
