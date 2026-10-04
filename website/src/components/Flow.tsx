import { useState } from 'react'
import { ArrowRight, Play, Sparkle } from 'lucide-react'
import { SectionHeading } from '@/components/SectionHeading'

const steps = ['Enter Username', 'Enter Password', 'Search User Database', 'Username Exists?', 'Verify Password', 'Password Correct?', 'Account Active?', 'Create Session', 'Load Role', 'Access Control', 'Grant Access']
const architecture = ['User', 'CLI / UI', 'Authentication Module', 'Password Verification', 'User Database · users.txt', 'Session Management', 'Access Control', 'Protected Resources', 'audit.log']

export function Flow() {
  const [playing, setPlaying] = useState(false)
  const play = () => {
    setPlaying(false)
    window.requestAnimationFrame(() => setPlaying(true))
    window.setTimeout(() => setPlaying(false), 5000)
  }
  return (
    <section id="flow" className="content-section flow-section">
      <div className="section-shell">
        <p className="section-kicker">from sign-in to the right resource</p>
        <SectionHeading>Follow the authentication flow</SectionHeading>
        <p className="flow-lede">The program verifies the account first, then checks its role before it opens a protected resource.</p>
        <div className={`flow-diagram${playing ? ' flow-playing' : ''}`}>
          <div className="flow-topline"><span>SUCCESS PATH</span><button onClick={play}><Play size={14} fill="currentColor"/> Play flow</button></div>
          <div className="flow-path" aria-label="Authentication success path">
            {steps.map((step, index) => <div className={`flow-step${index === steps.length - 1 ? ' flow-step-final' : ''}`} key={step} style={{ ['--step-index' as string]: index }}><span className="flow-step-number">{String(index + 1).padStart(2, '0')}</span><strong>{step}</strong>{index < steps.length - 1 && <ArrowRight className="flow-arrow" size={15}/>}</div>)}
            {playing && <Sparkle className="flow-traveler" size={22} fill="currentColor" aria-hidden="true"/>}
          </div>
          <div className="failure-branch"><span className="branch-label">FAILURE BRANCH</span><div className="branch-node">Wrong credentials</div><ArrowRight size={15}/><div className="branch-node">Increase failed attempts</div><ArrowRight size={15}/><div className="branch-node">Limit reached?</div><div className="branch-outcomes"><span><b>NO</b> · Try again</span><span><b>YES</b> · Lock account</span></div></div>
        </div>

        <div className="architecture-block"><div className="architecture-heading"><span className="section-kicker">inside the system</span><h3>Layered architecture</h3><p>Each module hands its result to the next part of the program.</p></div><div className="architecture-flow">{architecture.map((layer, index) => <div className="architecture-item" key={layer}><span>{layer}</span>{index < architecture.length - 1 && <ArrowRight size={16}/>}</div>)}</div></div>
      </div>
    </section>
  )
}
