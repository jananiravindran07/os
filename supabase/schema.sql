-- Supabase schema for the online authentication website.
-- Run this once in Supabase Dashboard > SQL Editor.
create extension if not exists pgcrypto;

create table if not exists public.profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  email text not null unique,
  username text not null unique,
  role text not null default 'user' check (role in ('admin', 'user', 'guest')),
  status text not null default 'active' check (status in ('active', 'locked')),
  failed_attempts integer not null default 0 check (failed_attempts between 0 and 3),
  created_at timestamptz not null default now()
);

create or replace function public.current_profile_role()
returns text
language sql stable security definer
set search_path = public
as $$
  select role from public.profiles where id = auth.uid() and status = 'active';
$$;

create or replace function public.is_site_admin()
returns boolean
language sql stable security definer
set search_path = public
as $$
  select coalesce(public.current_profile_role() = 'admin', false);
$$;

create or replace function public.create_profile_for_new_auth_user()
returns trigger
language plpgsql security definer
set search_path = public
as $$
declare
  requested_role text;
  requested_username text;
begin
  requested_role := case when new.raw_user_meta_data->>'role' = 'guest' then 'guest' else 'user' end;
  requested_username := trim(coalesce(new.raw_user_meta_data->>'username', split_part(new.email, '@', 1)));
  if length(requested_username) < 3 or length(requested_username) > 32
     or requested_username !~ '^[A-Za-z0-9_.-]+$' then
    requested_username := 'user_' || substr(replace(new.id::text, '-', ''), 1, 12);
  end if;
  insert into public.profiles (id, email, username, role)
  values (new.id, lower(new.email), requested_username, requested_role);
  return new;
end;
$$;

drop trigger if exists on_auth_user_created_profile on auth.users;
create trigger on_auth_user_created_profile
after insert on auth.users
for each row execute procedure public.create_profile_for_new_auth_user();

create or replace function public.has_mfa_verified()
returns boolean
language sql stable
as $
  select coalesce(auth.jwt()->>'aal' = 'aal2', false);
$;

alter table public.profiles enable row level security;
drop policy if exists "profiles readable by owner or admin" on public.profiles;
create policy "profiles readable by owner or admin"
on public.profiles for select to authenticated
using (public.has_mfa_verified() and (id = auth.uid() or public.is_site_admin()));
drop policy if exists "admins manage profiles" on public.profiles;
create policy "admins manage profiles"
on public.profiles for update to authenticated
using (public.has_mfa_verified() and public.is_site_admin())
with check (public.has_mfa_verified() and public.is_site_admin());

create table if not exists public.resources (
  id bigint generated always as identity primary key,
  name text not null unique,
  required_role text not null check (required_role in ('public', 'guest', 'user', 'admin')),
  description text not null
);
insert into public.resources (name, required_role, description) values
  ('Public guide', 'public', 'Available to every visitor.'),
  ('Member workspace', 'user', 'Available to signed-in users and administrators.'),
  ('Guest area', 'guest', 'Available to signed-in guests and administrators.'),
  ('Administration', 'admin', 'Available only to administrators.')
on conflict (name) do update set required_role = excluded.required_role, description = excluded.description;

alter table public.resources enable row level security;
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

create table if not exists public.audit_events (
  id bigint generated always as identity primary key,
  user_id uuid not null default auth.uid() references auth.users(id) on delete cascade,
  action text not null,
  created_at timestamptz not null default now()
);
alter table public.audit_events enable row level security;
drop policy if exists "users read their events and admins read all" on public.audit_events;
create policy "users read their events and admins read all"
on public.audit_events for select to authenticated
using (public.has_mfa_verified() and (user_id = auth.uid() or public.is_site_admin()));
drop policy if exists "users record their own events" on public.audit_events;
create policy "users record their own events"
on public.audit_events for insert to authenticated
with check (public.has_mfa_verified() and user_id = auth.uid());

-- After creating your own account, promote it from the SQL Editor:
-- update public.profiles set role = 'admin' where email = 'YOUR_EMAIL';
