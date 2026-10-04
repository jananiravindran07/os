import { createClient } from 'https://esm.sh/@supabase/supabase-js@2.49.1'

const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, apikey, content-type, x-client-info',
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
  'Content-Type': 'application/json',
}

Deno.serve(async request => {
  if (request.method === 'OPTIONS') return new Response('ok', { headers: cors })
  if (request.method !== 'POST') return new Response(JSON.stringify({ message: 'Method not allowed' }), { status: 405, headers: cors })

  const url = Deno.env.get('SUPABASE_URL')
  const anonKey = Deno.env.get('SUPABASE_ANON_KEY')
  const serviceKey = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')
  if (!url || !anonKey || !serviceKey) return new Response(JSON.stringify({ message: 'Authentication service is not configured.' }), { status: 500, headers: cors })

  try {
    const input = await request.json()
    const email = String(input.email ?? '').trim().toLowerCase()
    const password = String(input.password ?? '')
    if (!email || !password) return new Response(JSON.stringify({ message: 'Email and password are required.' }), { status: 400, headers: cors })

    const admin = createClient(url, serviceKey, { auth: { persistSession: false } })
    const { data: profile, error: lookupError } = await admin
      .from('profiles')
      .select('id,status,failed_attempts')
      .eq('email', email)
      .maybeSingle()
    if (lookupError) throw lookupError
    if (profile?.status === 'locked') return new Response(JSON.stringify({ message: 'This account is locked after three failed sign-in attempts. Contact an administrator.' }), { status: 423, headers: cors })

    const authResponse = await fetch(url + '/auth/v1/token?grant_type=password', {
      method: 'POST',
      headers: { apikey: anonKey, Authorization: 'Bearer ' + anonKey, 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password }),
    })
    const authBody = await authResponse.json().catch(() => ({}))
    if (!authResponse.ok) {
      if (profile && (authResponse.status === 400 || authResponse.status === 401)) {
        const attempts = Math.min(Number(profile.failed_attempts ?? 0) + 1, 3)
        await admin.from('profiles').update({ failed_attempts: attempts, status: attempts >= 3 ? 'locked' : 'active' }).eq('id', profile.id)
        const message = attempts >= 3
          ? 'Three sign-in attempts failed. This account is locked; contact an administrator.'
          : 'Email or password is incorrect. ' + (3 - attempts) + ' attempt(s) remain.'
        return new Response(JSON.stringify({ message }), { status: attempts >= 3 ? 423 : 401, headers: cors })
      }
      return new Response(JSON.stringify(authBody), { status: authResponse.status, headers: cors })
    }

    if (profile) await admin.from('profiles').update({ failed_attempts: 0 }).eq('id', profile.id)
    return new Response(JSON.stringify(authBody), { status: 200, headers: cors })
  } catch (error) {
    return new Response(JSON.stringify({ message: error instanceof Error ? error.message : 'Sign-in failed.' }), { status: 500, headers: cors })
  }
})
