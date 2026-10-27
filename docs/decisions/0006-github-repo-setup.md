# 0006 · Creating and pushing the GitHub repo

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 003, Q3

## Context

[Decision 0001](0001-repo-location-and-hosting.md) adds a private GitHub repo under `arjunjeewan`, created and pushed only after a separate confirmation. This machine has no GitHub credentials: no `gh`, no git credential helper, and github.com is not in `~/.ssh/known_hosts`.

## Options

1. The user creates an empty private repo `arjunjeewan/FuSION2` on github.com (no README or license); Claude adds it as `origin`; the user pushes (recommended)
2. The user adds `~/.ssh/id_ed25519.pub` to GitHub and creates the repo; Claude adds github.com's host key, sets an SSH origin and pushes
3. Claude installs the GitHub CLI in `~/.local/bin`; the user runs `gh auth login` once; Claude creates the repo and pushes
4. Stay local, and ask again at the end of phase 1

## Decision

Option 1. `origin` is `https://github.com/arjunjeewan/FuSION2.git`, the same HTTPS form FuSION 1's remote uses.

## Consequences

- No GitHub credentials are stored on this machine or seen by Claude, so the user runs every push.
