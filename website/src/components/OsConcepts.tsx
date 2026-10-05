import { AtSign, FileStack, KeyRound, LockKeyhole, ShieldCheck, Timer, UserRoundCog, UserRoundPlus } from 'lucide-react'
import type { LucideIcon } from 'lucide-react'
import { RevealText } from '@/components/RevealText'
import { SectionHeading } from '@/components/SectionHeading'

const concepts: { title: string; icon: LucideIcon; description: string }[] = [
  { title: 'Authentication', icon: KeyRound, description: 'Prove an account’s identity with a username and password.' },
  { title: 'Authorization', icon: AtSign, description: 'Decide whether an authenticated account may perform an action.' },
  { title: 'Access Control', icon: ShieldCheck, description: 'Apply permissions consistently to protected parts of a system.' },
  { title: 'Least Privilege', icon: LockKeyhole, description: 'Give each account only the permissions it needs.' },
  { title: 'User Management', icon: UserRoundPlus, description: 'Create accounts and keep their roles and statuses up to date.' },
  { title: 'Session Management', icon: Timer, description: 'Track a signed-in account and end sessions that are no longer active.' },
  { title: 'File Management', icon: FileStack, description: 'Read and update persistent records in files.' },
  { title: 'Security', icon: UserRoundCog, description: 'Use multiple safeguards to reduce account misuse.' },
]

export function OsConcepts() {
  return (
    <section id="os-concepts" className="content-section os-section">
      <div className="section-shell">
        <p className="section-kicker">the operating systems ideas behind the code</p>
        <SectionHeading>OS concepts, made practical</SectionHeading>
        <div className="os-concept-grid">
          {concepts.map(({ title, icon: Icon, description }, index) =>
            <article className="os-card" key={title} style={{ ['--os-turn' as string]: `${(index % 3 - 1) * 1.3}deg` }}>
              <div className="os-card-mark"><Icon size={23}/><span>{String(index + 1).padStart(2, '0')}</span></div>
              <RevealText text={title} className="os-card-title"/>
              <p className="os-description">{description}</p>
            </article>
          )}
        </div>
      </div>
    </section>
  )
}
