# 0015 · Reconciling with GitHub's initial commit

- **Status:** Accepted
- **Date:** 2026-11-09
- **Asked in:** prompt log entry 016, Q1

## Context

When the user created `arjunjeewan/FuSION2` ([decision 0006](0006-github-repo-setup.md)), GitHub added its own first commit, `9914fe2`, holding an MIT `LICENSE` ("Copyright (c) 2026 Arjun Jeewan"). A later `git pull` fetched it but could not merge it, because its history is unrelated to the local `main` (`289b0bc`). Nothing local had reached GitHub.

## Options

1. Merge `origin/main` into `main` with `--allow-unrelated-histories`: one merge commit that adds only `LICENSE`, so every hash the logs cite stays valid (recommended)
2. Rebase `main` onto `origin/main` for a straight history, giving `289b0bc` a new hash
3. Keep `main` as it is and force-push over the remote, losing `LICENSE`

## Decision

Option 1, as the merge commit "Merge GitHub's initial commit (LICENSE)". The user then pushes `main`.

## Consequences

- FuSION 2.0 is MIT-licensed, like FuSION 1 ("Copyright (c) 2023 Supratik Chakraborty"). If FuSION 2.0 ever copies FuSION 1 files, the MIT license asks that FuSION 1's notice travel with them.
