# 0005 · CLAUDE.md carries the process rules

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 003, Q1

## Context

Claude's memory is stored per project folder. A session started inside FuSION2 does not load the memory kept for the FuSION 1 folder, so it would not know the process rules.

## Options

1. Add a `CLAUDE.md` holding the process rules to the repo (recommended)
2. Keep the rules only in Claude's memory for the FuSION 1 folder

## Decision

Option 1. `CLAUDE.md` holds the text previewed in the poll, word for word.

## Consequences

- Every Claude session started in FuSION2 loads the rules.
- Changing the process means editing `CLAUDE.md` in a reviewed commit.
