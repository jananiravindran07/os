import { useState } from 'react'
import { AtSign, ChevronDown, FileStack, KeyRound, LockKeyhole, ShieldCheck, Timer, UserRoundCog, UserRoundPlus } from 'lucide-react'
import type { LucideIcon } from 'lucide-react'
import { RevealText } from '@/components/RevealText'
import { SectionHeading } from '@/components/SectionHeading'

const concepts: { title: string; icon: LucideIcon; description: string; inProject: string }[] = [
  { title: 'Authentication', icon: KeyRound, description: 'Prove an account’s identity with a username and password.', inProject: 'auth.c verifies the submitted password against its stored salted hash.' },
  { title: 'Authorization', icon: AtSign, description: 'Decide whether an authenticated account may perform an action.', inProject: 'The access-control module checks the account role before showing a resource.' },
  { title: 'Access Control', icon: ShieldCheck, description: 'Apply permissions consistently to protected parts of a system.', inProject: 'A role-to-resource matrix returns ACCESS GRANTED or ACCESS DENIED.' },
  { title: 'Least Privilege', icon: LockKeyhole, description: 'Give each account only the permissions it needs.', inProject: 'USER and GUEST accounts cannot manage users or view administrator logs.' },
  { title: 'User Management', icon: UserRoundPlus, description: 'Create accounts and keep their roles and statuses up to date.', inProject: 'ADMIN can list, add, delete, unlock and change the role of accounts.' },
  { title: 'Session Management', icon: Timer, description: 'Track a signed-in account and end sessions that are no longer active.', inProject: 'session.c stores the current account and checks its idle timeout.' },
  { title: 'File Management', icon: FileStack, description: 'Read and update persistent records in files.', inProject: 'database.c reads and writes users.txt; logger.c appends audit.log entries.' },
  { title: 'Security', icon: UserRoundCog, description: 'Use multiple safeguards to reduce account misuse.', inProject: 'Password hashing, failed-login tracking, lockout and audit records work together.' },
]

export function OsConcepts() {
  const [expanded, setExpanded] = useState<string[]>([])
  const toggle = (title: string) => setExpanded(items => items.includes(title) ? items.filter(item => item !== title) : [...items, title])
  return (
    <section id="os-concepts" className="content-section os-section">
      <div className="section-shell">
        <p className="section-kicker">the operating systems ideas behind the code</p>
        <SectionHeading>OS concepts, made practical</SectionHeading>
        <div className="os-concept-grid">
          {concepts.map(({ title, icon: Icon, description, inProject }, index) => {
            const open = expanded.includes(title)
            return <article className={`os-card${open ? ' is-expanded' : ''}`} key={title} style={{ ['--os-turn' as string]: `${(index % 3 - 1) * 1.3}deg` }}>
              <div className="os-card-mark"><Icon size={23}/><span>{String(index + 1).padStart(2, '0')}</span></div>
              <RevealText text={title} className="os-card-title"/>
              <p className="os-description">{description}</p>
              <div className="in-project"><b>In this project:</b><p>{inProject}</p></div>
              <button className="os-toggle" aria-expanded={open} onClick={() => toggle(title)}>{open ? 'Show less' : 'See how it connects'}<ChevronDown size={14}/></button>
            </article>
          })}
        </div>
      </div>
    </section>
  )
}
