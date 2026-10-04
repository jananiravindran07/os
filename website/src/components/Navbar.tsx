import { useState } from 'react'
import { Menu, X } from 'lucide-react'
import { navigateToSection } from '@/lib/anchor'

const links = [
  ['Overview', '#overview'], ['Features', '#features'], ['Roles', '#roles'],
  ['Flow', '#flow'], ['Code', '#code'], ['OS Concepts', '#os-concepts'],
]

export function Navbar() {
  const [open, setOpen] = useState(false)
  return (
    <header className="site-header">
      <nav className="navbar" aria-label="Main navigation">
        <a className="brand" href="#home" aria-label="User Authentication System home">
          <span className="brand-sparkle" aria-hidden="true">✦</span>
          <span className="brand-name">User Authentication<br className="brand-break" /> System</span>
        </a>
        <button className="menu-toggle" aria-label={open ? 'Close navigation' : 'Open navigation'} aria-expanded={open} onClick={() => setOpen(!open)}>
          {open ? <X size={21} /> : <Menu size={21} />}
        </button>
        <div className={`nav-links${open ? ' is-open' : ''}`}>
          {links.map(([label, href]) => <a key={label} href={href} onClick={event => navigateToSection(event, href, () => setOpen(false))}>{label}</a>)}
        </div>
      </nav>
    </header>
  )
}
