# Online account setup

The website uses Firebase Authentication for verified email/password accounts and password-reset emails. Firestore stores account profiles and security activity; `firestore.rules` protects that data. GitHub Pages hosts the website.

## One-time setup

1. Create a Firebase project at https://console.firebase.google.com/ and register a Web app.
2. In **Authentication → Sign-in method**, enable **Email/Password**.
3. In **Authentication → Settings → Authorized domains**, add the domain that hosts the site (for example, `jananiravindran07.github.io`).
4. In **Authentication → Templates**, set the password-reset action URL/continue URL to your deployed website. Firebase sends verification and password-reset emails; customize the sender/template there as needed.
5. Create a Firestore database, then publish the repository's `firestore.rules` in **Firestore Database → Rules**.
6. In **Project settings → General → Your apps**, copy the web app values into the GitHub repository's **Settings → Secrets and variables → Actions → Variables**:
    - `VITEFIREBASEAPIKEY` = `apiKey`
    - `VITEFIREBASEAUTHDOMAIN` = `authDomain`
    - `VITEFIREBASEPROJECTID` = `projectId`
    - `VITEFIREBASEAPPID` = `appId`
7. For local development, copy `.env.example` to `.env.local` in the `website` folder and fill in those same values.
8. Deploy the website. Create an account, verify the email from the inbox, then sign in. Use **Forgot password** to send a reset email to the entered address.

Firebase web configuration is public browser configuration, not a secret. Firestore access is controlled by the published security rules; never loosen those rules to public read/write.

## Administrator access

New accounts can choose the `user` or `guest` role. To promote your own account, find its document under Firestore `profiles/{Firebase Auth UID}` and change `role` to `admin`. Administrators can then lock or unlock other account profiles from the website. A locked profile is denied access to the protected profile and activity data by the Firestore rules.

Online accounts are separate from the C program's local `users.txt`. Existing local accounts need to register with an email address.
