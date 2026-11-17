# 0034 · Commits keep the Claude co-author line; de-claude runs before each push

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 255, Q2

## Context

Every FuSION2 commit ended with "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" until [decision 0033](0033-de-claude-on-all-commits.md) stripped those lines. That left open whether new commits should still carry the line.

## Options

1. Drop the line from new commits, so later pushes need no clean-up and no hashes change again (recommended)
2. Keep the line in local commits, and run de-claude before each push

## Decision

Option 2.

## Consequences

- New commits still end with the co-author line. Before each push, de-claude runs on the unpushed commits, which strips the line and gives those commits new hashes.
- Once `main` tracks `origin/main` ([decision 0035](0035-claude-pushes-over-ssh.md)), de-claude with no flags scans only the unpushed commits, so pushed history is not rewritten again and those pushes need no force.
- A hash cited for an unpushed commit stops naming it once the commit is cleaned for its push.
