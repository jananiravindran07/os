import { Check, LockKeyhole, Shield, Sparkle, X } from 'lucide-react'
import { SectionHeading } from '@/components/SectionHeading'
import { RevealText } from '@/components/RevealText'

const roles = [
  { role: 'ADMIN', name: 'Administrator', intro: 'Steward of the system', permissions: ['View all users', 'Add and delete accounts', 'Unlock accounts', 'Change user roles', 'Review authentication logs', 'Access protected system resources'], note: 'Administrative powers stay with one trusted role.' },
  { role: 'USER', name: 'Signed-in user', intro: 'Everyday access', permissions: ['View own profile', 'Change password', 'Use normal resources', 'End their own session'], note: 'Can use normal resources, but cannot manage accounts.' },
  { role: 'GUEST', name: 'Guest', intro: 'A limited welcome', permissions: ['View public resources', 'Read basic information', 'End their session'], note: 'Protected resources stay closed to guest accounts.' },
]
const matrix = [
  ['Public Information', true, true, true],
  ['View Profile', true, true, true],
  ['Change Password', true, true, false],
  ['Normal Resource', true, true, true],
  ['User Management', true, false, false],
  ['Delete User', true, false, false],
  ['View Logs', true, false, false],
  ['System Settings', true, false, false],
] as const

export function Roles() {
  return (
    <section id="roles" className="content-section roles-section">
      <div className="section-shell">
        <p className="section-kicker">different accounts, different permissions</p>
        <SectionHeading>Three roles. Clear boundaries.</SectionHeading>
        <p className="roles-lede">Role-based access control gives each account the permissions it needs and keeps sensitive actions behind an administrator role.</p>
        <div className="role-grid">
          {roles.map((item, index) => (
            <article className={`role-card role-card-${item.role.toLowerCase()}`} key={item.role} style={{ ['--role-turn' as string]: `${(index - 1) * 1.4}deg` }}>
              <div className="role-card-top"><span className="role-icon"><Shield size={23}/></span><span className="role-label">{item.role}</span></div>
              <RevealText text={item.name} className="role-name"/>
              <p className="role-intro">{item.intro}</p>
              <span className="can-do-label">CAN DO</span>
              <ul>{item.permissions.map(permission => <li key={permission}><Sparkle size={13}/>{permission}</li>)}</ul>
              <div className="least-privilege"><LockKeyhole size={14}/><span>{item.note}</span></div>
            </article>
          ))}
        </div>

        <div className="roles-detail-grid">
          <div className="matrix-card">
            <div className="matrix-heading"><div><span className="section-kicker">permission matrix</span><h3>Who can reach what?</h3></div><Sparkle size={24}/></div>
            <div className="matrix-scroll"><table className="role-matrix"><thead><tr><th>Resource</th><th>ADMIN</th><th>USER</th><th>GUEST</th></tr></thead><tbody>{matrix.map(([resource, admin, user, guest]) => <tr key={resource}><th>{resource}</th>{[admin, user, guest].map((allowed, index) => <td key={index}>{allowed ? <><Sparkle size={15} fill="currentColor"/><span>YES</span></> : <><Sparkle size={15}/><span>NO</span></>}</td>)}</tr>)}</tbody></table></div>
          </div>
          <div className="access-examples">
            <article className="access-example access-example-denied"><div><X size={17}/><strong>ACCESS DENIED</strong></div><p>You do not have permission to access this resource.</p><span>Required Role: ADMIN · Your Role: USER</span></article>
            <article className="access-example access-example-granted"><div><Check size={17}/><strong>ACCESS GRANTED</strong></div><p>The role is allowed to use this resource.</p><span>Required Role: ANY · Your Role: USER</span></article>
          </div>
        </div>
      </div>
    </section>
  )
}
