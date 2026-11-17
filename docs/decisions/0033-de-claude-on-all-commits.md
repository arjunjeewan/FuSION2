# 0033 · Plain de-claude strips the Claude co-author lines from every commit, replacing GitHub's initial commit

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 255, Q1

## Context

The user asked Claude to run de-claude on the FuSION2 repo and push it to GitHub (prompt log entry 253). de-claude 2.2.1 (`/usr/local/bin/de-claude`) removes Claude attribution lines from commit messages by rewriting them with `git filter-branch`. Each of the 18 local commits ended with "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>". GitHub held only its own signed "Initial commit", `9914fe2`, which reaches `main` through the merge commit of [decision 0015](0015-github-initial-commit-merge.md). With no upstream set and an oldest commit that has no parent, de-claude rewrites every commit reachable from `HEAD`, `9914fe2` included. `git filter-branch` drops GitHub's signature, so that commit gets a new hash and the push has to replace it. Tests on two throwaway clones confirmed this, and showed that running de-claude's own filter script on the commits after `9914fe2` keeps it.

## Options

1. Run de-claude's own filter script through `git filter-branch`, as de-claude does, but only on the commits after `9914fe2` (range `9914fe2..HEAD`), so the push is a plain fast-forward (recommended)
2. Run de-claude unchanged, which replaces GitHub's initial commit with an unsigned copy, and force-push

## Decision

Option 2. Claude saved the old `main` as the local ref `refs/backup/main-before-de-claude` (`a78ac38`), which is never pushed, and ran `de-claude -y` (prompt log entry 256).

## Consequences

- Each local commit lost its co-author line and the blank line before it. The files, authors, dates and subjects are unchanged.
- All 19 commits have new hashes. GitHub's `9914fe2` is now `8ae909d`, the same commit without GitHub's signature, so the first push replaces it with `--force-with-lease` ([decision 0035](0035-claude-pushes-over-ssh.md)).
- Hashes cited before the rewrite name the old commits. Prompt log entry 256 maps each old hash to its new one, and decision 0015 has a note.
- The old history, co-author lines included, stays in this clone under `refs/backup/main-before-de-claude`.
- [Decision 0034](0034-co-author-line-kept-until-push.md) covers new commits.
