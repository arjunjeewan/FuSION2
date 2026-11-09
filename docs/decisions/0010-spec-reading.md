# 0010 · Who reads FuSION 1 for the spec

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 004, Q4

## Options

1. Claude, stage by stage, so the detail stays in the main conversation (recommended)
2. Subagents in parallel, one per stage, with each prompt logged in full

## Decision

Option 2. One subagent per stage drafts that stage's file. Every subagent prompt is logged in full in the prompt log ([decision 0002](0002-prompt-log-scope.md)).

## Consequences

- Before polling a stage's items, Claude re-reads every FuSION 1 location the stage file cites.
