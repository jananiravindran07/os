import { useEffect, useRef, useState } from 'react'
import { useInView } from 'motion/react'
import { TextEffect } from '@/components/core/text-effect'

export function RevealText({ text, as = 'h3', className = '' }: { text: string; as?: keyof React.JSX.IntrinsicElements; className?: string }) {
  const ref = useRef<HTMLDivElement>(null)
  const visible = useInView(ref, { once: true, amount: 0.55 })
  const [reduced, setReduced] = useState(false)
  useEffect(() => {
    const query = window.matchMedia('(prefers-reduced-motion: reduce)')
    const update = () => setReduced(query.matches)
    update()
    query.addEventListener('change', update)
    return () => query.removeEventListener('change', update)
  }, [])
  const Tag = as
  return <div ref={ref} className="reveal-text-wrap">{reduced ? <Tag className={className}>{text}</Tag> : <TextEffect as={as} per="char" preset="fade" speedReveal={3} speedSegment={3} trigger={visible} className={className}>{text}</TextEffect>}</div>
}
