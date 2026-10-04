import { TextEffect } from '@/components/core/text-effect'
import { useEffect, useState } from 'react'

export function Footer() {
  const [reduced, setReduced] = useState(false)
  useEffect(() => {
    const query = window.matchMedia('(prefers-reduced-motion: reduce)')
    const update = () => setReduced(query.matches)
    update()
    query.addEventListener('change', update)
    return () => query.removeEventListener('change', update)
  }, [])
  return <footer className="site-footer"><div className="footer-shell"><p className="footer-project">User Authentication and Access Control System using C</p><p className="footer-subject">Operating Systems</p><div className="footer-divider" aria-hidden="true"><span>✦</span><i/><span>✦</span><i/><span>✦</span></div>{reduced ? <p className="footer-tagline">verify who you are. decide what you can touch.</p> : <TextEffect as="p" per="char" preset="fade" speedReveal={3} speedSegment={3} className="footer-tagline">verify who you are. decide what you can touch.</TextEffect>}</div></footer>
}
