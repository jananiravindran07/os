import { Sparkle } from 'lucide-react'
import { SectionHeading } from '@/components/SectionHeading'

const objectives = [
  'User authentication', 'User management', 'Password security', 'Session management',
  'Role-based access control', 'Authorization', 'Access control', 'Least privilege',
  'Account lockout', 'Audit logging',
]

export function Overview() {
  return (
    <section id="overview" className="content-section overview-section">
      <div className="section-shell overview-shell">
        <p className="section-kicker">a small system with a clear purpose</p>
        <SectionHeading>What is this project?</SectionHeading>
        <p className="overview-intro">A C-based system that verifies identity with a username and password, then decides which resources a person may access based on their role.</p>
        <p className="overview-detail">It brings together the everyday building blocks of account security: authentication, user management, password protection, sessions, authorization, least privilege, account lockout and audit logging.</p>
        <div className="objective-grid" aria-label="Project objectives">
          {objectives.map((item, index) => (
            <div className="objective-pill" key={item}>
              <Sparkle size={17} aria-hidden="true" />
              <span>{item}</span>
              <span className="objective-number">{String(index + 1).padStart(2, '0')}</span>
            </div>
          ))}
        </div>
        <div className="overview-note"><span aria-hidden="true">✦</span> Authentication answers who you are. Authorization decides what you can do.</div>
      </div>
    </section>
  )
}
