# 0035 · Claude pushes when the user asks, over this machine's SSH key (amends 0006)

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 255, Q3

## Context

[Decision 0006](0006-github-repo-setup.md) gave `origin` the HTTPS address `https://github.com/arjunjeewan/FuSION2.git` and had the user run every push, because this machine had no GitHub credentials. On 2026-11-17 the user asked Claude to push (prompt log entry 253). HTTPS still cannot push from here: `git ls-remote origin` stops at the username prompt. But github.com is now in `~/.ssh/known_hosts`, and this machine's SSH key reaches the repo: a read-only `git ls-remote` over SSH showed GitHub's `main` at `9914fe2` and nothing else.

## Options

1. Switch `origin` to `git@github.com:arjunjeewan/FuSION2.git`, and Claude pushes with `git push -u origin main`, forcing with a lease where [decision 0033](0033-de-claude-on-all-commits.md) replaced GitHub's commit (recommended)
2. The user pushes, and decision 0006 stands

## Decision

Option 1. Claude still pushes only when the user says so ([decision 0001](0001-repo-location-and-hosting.md)).

## Consequences

- `origin` uses SSH, for fetches too, so pushes from this clone go over SSH whoever runs them.
- The first push replaces GitHub's `9914fe2` with `8ae909d` under `--force-with-lease=main:9914fe2`, which refuses if GitHub's `main` has moved. After it, `main` tracks `origin/main`.
- No credential passes through Claude: the key stays in `~/.ssh`, and Claude does not read it.
