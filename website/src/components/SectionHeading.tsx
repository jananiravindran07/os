import { useEffect, useRef, useState } from 'react'
import { useInView } from 'motion/react'
import { TextEffect } from '@/components/core/text-effect'

export function SectionHeading({ children, className = '' }: { children: string; className?: string }) {
  const ref = useRef<HTMLDivElement>(null)
  const visible = useInView(ref, { once: true, amount: 0.4 })
  const [reduced, setReduced] = useState(false)
  useEffect(() => {
    const query = window.matchMedia('(prefers-reduced-motion: reduce)')
    const update = () => setReduced(query.matches)
    update()
    query.addEventListener('change', update)
    return () => query.removeEventListener('change', update)
  }, [])

  return (
    <div ref={ref} className={`section-heading-wrap ${className}`}>
      {reduced
        ? <h2 className="section-heading">{children}</h2>
        : <TextEffect as="h2" per="word" preset="slide" speedReveal={3} speedSegment={2.5} trigger={visible} className="section-heading">{children}</TextEffect>}
    </div>
  )
}
