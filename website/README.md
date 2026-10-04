# User Authentication & Access Control — Project Website

A responsive project presentation for the Operating Systems mini-project written in C. It explains the project's features, access roles, authentication flow, C modules and OS concepts. The flow animation, role matrix and code explorer are presentation elements; the site does not simulate sign-in or read or modify the C program's `users.txt` and `audit.log` files.

## Run locally

```sh
cd website
npm install
npm run dev
```

Vite prints the local preview URL after it starts. Create a production build with `npm run build`.

## Folder structure

```text
website/
├── public/
│   └── pixel-cursor.svg
├── src/
│   ├── components/
│   │   ├── core/text-effect.tsx  # motion-primitives text animation
│   │   ├── ui/button.tsx         # shadcn/ui button
│   │   ├── Navbar.tsx            # page navigation
│   │   ├── Hero.tsx              # animated introduction
│   │   ├── Overview.tsx          # project goals and summary
│   │   ├── Features.tsx           # implemented security features
│   │   ├── Roles.tsx              # permissions and access matrix
│   │   ├── Flow.tsx               # sign-in and authorization flow
│   │   ├── CodeStructure.tsx      # selectable C modules and examples
│   │   ├── OsConcepts.tsx         # OS concepts mapped to modules
│   │   ├── Footer.tsx
│   │   ├── SectionHeading.tsx     # in-view heading animation
│   │   └── RevealText.tsx         # in-view title animation
│   ├── lib/
│   │   ├── anchor.ts              # smooth anchor navigation
│   │   └── utils.ts               # shadcn class helper
│   ├── App.tsx
│   ├── App.css
│   ├── index.css                  # four-color theme tokens and base styles
│   └── main.tsx
├── index.html                    # page metadata and Google Fonts
├── components.json
├── package.json
├── package-lock.json
├── tsconfig.json                  # @/* TypeScript alias
├── tsconfig.app.json
└── vite.config.ts
```

## Website sections

The page is organized as Navbar, Hero, Overview, Features, Roles, Flow, Code Structure, OS Concepts and Footer. The text animations respect the visitor's reduced-motion preference. The interactive code explorer and permission cards explain the actual C project without changing its files.

## How the website maps to the C modules

| Website section | C project files it explains |
|---|---|
| Overview | `main.c`, `auth.c`, `access_control.c` |
| Features | `auth.c`, `password.c`, `session.c`, `admin.c`, `logger.c`, `database.c` |
| Roles and access matrix | `user.c`, `user.h`, `access_control.c`, `access_control.h` |
| Authentication flow | `main.c`, `auth.c`, `password.c`, `session.c`, `access_control.c` |
| Code structure | All modules: `main.c`, `auth.c`, `user.c`, `password.c`, `session.c`, `access_control.c`, `admin.c`, `logger.c`, `database.c`, `utils.c` |
| OS concepts | `session.c`, `database.c`, `logger.c`, `user.c`, `access_control.c` |

The page's file tree, code samples and file-format examples are explanatory snapshots. The actual C source and data files remain in the parent project folder.
