import { Navbar } from '@/components/Navbar'
import { Hero } from '@/components/Hero'
import { Overview } from '@/components/Overview'
import { Features } from '@/components/Features'
import { CodeStructure } from '@/components/CodeStructure'
import { Roles } from '@/components/Roles'
import { Flow } from '@/components/Flow'
import { OsConcepts } from '@/components/OsConcepts'
import { Footer } from '@/components/Footer'
import './App.css'

export default function App() {
  return <><Navbar /><main><Hero /><Overview /><Features /><Roles /><Flow /><CodeStructure /><OsConcepts /></main><Footer /></>
}
