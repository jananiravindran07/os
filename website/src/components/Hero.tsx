import { useEffect, useState } from 'react'
import { ArrowDown, ArrowUpRight, LockKeyhole, Sparkles } from 'lucide-react'
import { TextEffect } from '@/components/core/text-effect'
import { navigateToSection } from '@/lib/anchor'

function AnimatedText({ text, per, preset = 'fade', delay = 0, as = 'p', className = '' }: {
  text: string; per: 'char' | 'word'; preset?: 'blur' | 'fade' | 'scale' | 'slide'; delay?: number; as?: keyof React.JSX.IntrinsicElements; className?: string
}) {
  const [reduced, setReduced] = useState(false)
  useEffect(() => {
    const query = window.matchMedia('(prefers-reduced-motion: reduce)')
    const update = () => setReduced(query.matches)
    update()
    query.addEventListener('change', update)
    return () => query.removeEventListener('change', update)
  }, [])
  if (reduced) {
    const Tag = as
    return <Tag className={className}>{text}</Tag>
  }
  return <TextEffect as={as} per={per} preset={preset} delay={delay} speedReveal={3} speedSegment={3} className={className}>{text}</TextEffect>
}

function Sparkle({ className = '' }: { className?: string }) {
  return <svg className={`sparkle ${className}`} viewBox="0 0 48 48" aria-hidden="true"><path d="M24 1.5c3.1 12.4 8.1 17.4 22.5 22.5C32.1 29.1 27.1 34.1 24 46.5 20.9 34.1 15.9 29.1 1.5 24 15.9 18.9 20.9 13.9 24 1.5Z" /></svg>
}

function Badge() {
  return (
    <div className="hero-stamp" aria-label="User authentication and access control">
      <svg className="stamp-type" viewBox="0 0 140 140" role="img" aria-hidden="true">
        <defs><path id="stamp-circle" d="M70,70 m-49,0 a49,49 0 1,1 98,0 a49,49 0 1,1 -98,0" /></defs>
        <text><textPath href="#stamp-circle">user authentication　•　access control　•　</textPath></text>
      </svg>
      <span className="stamp-center"><LockKeyhole size={23} strokeWidth={1.8} /><Sparkles size={13} /></span>
    </div>
  )
}

function BookLockIllustration() {
  return (
    <svg className="book-illustration" viewBox="0 0 340 300" fill="none" aria-label="Line illustration of a lock on a stack of books" role="img">
      <g stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
        <path d="M56 232h228c5 0 8 5 5 9l-9 17H55l-8-17c-2-4 3-9 9-9Z"/><path d="M52 258h231v17H52z"/><path d="M74 199h205c5 0 8 5 5 9l-9 17H70l-8-17c-2-4 5-9 12-9Z"/><path d="M74 199c4 7 8 17 7 26M269 199c-4 7-7 17-7 26"/>
        <path d="M131 119V90a39 39 0 0 1 78 0v29"/><rect x="111" y="115" width="118" height="93" rx="17"/><circle cx="170" cy="155" r="9"/><path d="m170 164-7 20h14l-7-20Z"/>
        <path d="M54 191c-19-14-22-33-12-48 9 10 16 24 12 48ZM285 183c22-16 23-35 13-50-9 10-15 26-13 50ZM298 132c-9-12-6-24 4-32 6 9 7 21-4 32Z"/>
        <path d="M244 67c-13-9-14-18-5-26 9 8 11 17 5 26ZM241 56c12-11 12-22 2-30"/>
      </g>
      <path d="M24 1.5c3.1 12.4 8.1 17.4 22.5 22.5C32.1 29.1 27.1 34.1 24 46.5 20.9 34.1 15.9 29.1 1.5 24 15.9 18.9 20.9 13.9 24 1.5Z" transform="translate(26 34) scale(.42)" fill="currentColor" stroke="none"/>
      <path d="M24 1.5c3.1 12.4 8.1 17.4 22.5 22.5C32.1 29.1 27.1 34.1 24 46.5 20.9 34.1 15.9 29.1 1.5 24 15.9 18.9 20.9 13.9 24 1.5Z" transform="translate(283 78) scale(.32)" fill="currentColor" stroke="none"/>
    </svg>
  )
}

export function Hero() {
  return (
    <section id="home" className="hero-section">
      <div className="hero-backdrop" aria-hidden="true"><span className="orbit orbit-a"/><span className="orbit orbit-b"/></div>
      <Sparkle className="float-star float-star-a"/><Sparkle className="float-star float-star-b"/><Sparkle className="float-star float-star-c"/>
      <div className="hero-inner">
        <div className="hero-copy">
          <h1 className="hero-title">
            <AnimatedText as="span" text="User Authentication" per="char" preset="blur" className="title-line title-line-one" />
            <AnimatedText as="span" text="& Access Control" per="char" preset="blur" delay={0.38} className="title-line title-line-two" />
          </h1>
          <AnimatedText as="p" text="an operating systems mini-project, written in C" per="word" delay={0.76} className="hero-tagline" />
          <AnimatedText as="p" text="Verify who you are. Decide what you can touch." per="char" delay={1.05} className="hero-subtitle" />
          <div className="hero-actions">
            <a className="button button-light" href="#overview" onClick={event => navigateToSection(event, '#overview')}>Explore the project <ArrowUpRight size={17} /></a>
            <a className="button button-outline" href="#code" onClick={event => navigateToSection(event, '#code')}>Explore the code <ArrowDown size={16} /></a>
          </div>
          <div className="tech-pills" aria-label="Technology used">
            {['C', 'GCC', 'CLI', 'File-based storage', 'RBAC'].map((item) => <span key={item}>{item}</span>)}
          </div>
        </div>
        <div className="hero-art" aria-hidden="false">
          <Badge />
          <BookLockIllustration />
        </div>
      </div>
      <div className="hero-bottom"><a href="#overview" aria-label="Scroll to project overview" onClick={event => navigateToSection(event, '#overview')}><ArrowDown size={17}/></a></div>
    </section>
  )
}
