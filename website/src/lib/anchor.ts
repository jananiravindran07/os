import type { MouseEvent } from 'react'

export function navigateToSection(event: MouseEvent<HTMLAnchorElement>, href: string, afterNavigate?: () => void) {
  event.preventDefault()
  afterNavigate?.()
  const id = href.replace(/^#/, '')
  const target = document.getElementById(id)
  if (!target) return
  if (window.location.hash !== href) window.history.pushState(null, '', href)
  target.scrollIntoView({ behavior: window.matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth', block: 'start' })
}
