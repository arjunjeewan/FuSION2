# 0001 · Repo location and hosting

- **Status:** Accepted. The GitHub step was confirmed in [decision 0006](0006-github-repo-setup.md).
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 002, Q1

## Context

FuSION 2.0 gets its own repository, separate from FuSION 1 (`/home/ks-149-u24/Desktop/arjun/FuSION`, remote `supratik1/FuSION`).

## Options

1. `/home/ks-149-u24/Desktop/arjun/FuSION2`, local only (recommended)
2. `/home/ks-149-u24/Desktop/arjun/misc/FuSION2`, local only
3. The folder from option 1, plus a private GitHub repo under `arjunjeewan`

## Decision

Option 3. The local repo is `/home/ks-149-u24/Desktop/arjun/FuSION2` on branch `main`. The GitHub repo is created and pushed to only after the user confirms that step separately.

## Consequences

- This machine cannot push to GitHub yet: `gh` is not installed, git has no credential helper or stored credentials, and SSH to github.com fails host-key verification because github.com is not in `~/.ssh/known_hosts`.
