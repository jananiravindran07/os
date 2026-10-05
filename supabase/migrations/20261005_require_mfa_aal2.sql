-- Require Supabase MFA assurance level 2 before accessing protected application data.
-- Run this once in Supabase SQL Editor for projects created before this migration.

create or replace function public.has_mfa_verified()
returns boolean
language sql stable
as $$
  select coalesce(auth.jwt()->>'aal' = 'aal2', false);
$$;

drop policy if exists "profiles readable by owner or admin" on public.profiles;
create policy "profiles readable by owner or admin"
on public.profiles for select to authenticated
using (public.has_mfa_verified() and (id = auth.uid() or public.is_site_admin()));

drop policy if exists "admins manage profiles" on public.profiles;
create policy "admins manage profiles"
on public.profiles for update to authenticated
using (public.has_mfa_verified() and public.is_site_admin())
with check (public.has_mfa_verified() and public.is_site_admin());

drop policy if exists "resources follow role access" on public.resources;
create policy "resources follow role access"
on public.resources for select
using (
  required_role = 'public'
  or (
    auth.role() = 'authenticated'
    and public.has_mfa_verified()
    and case required_role
      when 'guest' then public.current_profile_role() in ('guest', 'admin')
      when 'user' then public.current_profile_role() in ('user', 'admin')
      when 'admin' then public.current_profile_role() = 'admin'
      else false
    end
  )
);

drop policy if exists "users read their events and admins read all" on public.audit_events;
create policy "users read their events and admins read all"
on public.audit_events for select to authenticated
using (public.has_mfa_verified() and (user_id = auth.uid() or public.is_site_admin()));

drop policy if exists "users record their own events" on public.audit_events;
create policy "users record their own events"
on public.audit_events for insert to authenticated
with check (public.has_mfa_verified() and user_id = auth.uid());
