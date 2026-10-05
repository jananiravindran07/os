# Online account setup

The website includes an online registration, sign-in, password recovery, role-based resource access and admin lock/unlock panel. It uses Supabase Auth and Postgres. GitHub Pages hosts the interface; Supabase stores accounts and enforces access.

## One-time setup

1. Create a Supabase project at https://supabase.com/dashboard.
2. Open **SQL Editor**, paste in the repository file `supabase/schema.sql`, and run it.
3. Deploy the login function from a terminal with the Supabase CLI installed:
   ```sh
   npx supabase login
   npx supabase link --project-ref YOUR_PROJECT_REF
   npx supabase functions deploy login
   ```
   The function uses the project secrets Supabase provides to Edge Functions. Never put the service-role key in the website or GitHub.
4. In **Authentication → URL Configuration**, set the Site URL to `https://jananiravindran07.github.io/os` and add `https://jananiravindran07.github.io/os/*` to the allowed redirect URLs. This is needed for password-reset links.
5. In **Authentication → Email Templates → Confirm signup**, make sure the template includes the six-digit confirmation token with `{{ .Token }}`. Replace link-based confirmation text with wording that tells the user to enter the code on the website. Password recovery remains a link.
6. In the GitHub repository, open **Settings → Secrets and variables → Actions**:
   - Add repository variable `VITE_SUPABASE_URL` with the Project URL.
   - Add repository secret `VITE_SUPABASE_ANON_KEY` with the project's publishable/anon key.
7. Merge the website changes into `main`. The Pages workflow will build with those settings and publish the account section.
8. If you later change either Actions value, start a fresh run from **Actions → Build and deploy website → Run workflow**; saving a secret alone does not start a new Pages build.

For local development, copy `.env.example` to `.env.local` in the `website` folder and fill in the same two values. Never use a service-role key for either value.

## Make an administrator

Register and confirm your own email address through the site. Then run this in the Supabase SQL Editor, replacing the email:

```sql
update public.profiles
set role = 'admin'
where email = lower('you@example.com');
```

Administrators can see the account list and lock or unlock accounts. The Edge Function blocks sign-in after three consecutive bad passwords. Row-level security also prevents a locked account from reading protected resources. Roles are assigned by the database; visitors cannot register themselves as admins. The Admin option shown during public registration is a request only and starts the account as a regular user until the owner promotes it.

## Two-step verification and existing projects

New users confirm their email with the six-digit signup code, then enroll a TOTP authenticator app (Google Authenticator, Microsoft Authenticator, Authy, or Apple Passwords). They must enter a fresh authenticator code for each sign-in. This is authenticator-based MFA; Supabase does not provide email as the second factor.

For projects where `schema.sql` has already been run, also run `supabase/migrations/20261005_require_mfa_aal2.sql` in **Supabase Dashboard → SQL Editor**. This adds a database-side requirement for AAL2 before protected tables can be read or changed.

## Notes

- Password reset links are sent by Supabase Auth. Configure a custom SMTP provider in Supabase before opening registrations broadly; the built-in mail service is for limited testing.
- The public anon key is designed to be present in the browser. Database security comes from the SQL row-level security policies. Keep the service-role key private.
- This changes online auth storage from the C program's local `users.txt` to Supabase. The command-line program remains separate.
