# 0002 · What the prompt log records

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 002, Q2

## Context

The user asked for every prompt in the FuSION 2.0 thread to be tracked, and for every decision to be run past them and reviewed.

## Options

1. User prompts word for word with timestamps, plus each poll and its answer, in one append-only `docs/process/PROMPTS.md`; each decision also gets its own record under `docs/decisions/` (recommended)
2. Everything in option 1, plus the full text of any prompt Claude gives a subagent
3. User prompts only, with no decision records

## Decision

Option 2. `docs/process/PROMPTS.md` logs user prompts, polls (options word for word, plus the answer chosen) and subagent prompts. `docs/decisions/` holds one record per decision, indexed in `docs/decisions/README.md`.

## Consequences

- Every generated file traces back to the instruction behind it, including work handed to subagents.
- Log entries are never edited. A correction is a new entry.
