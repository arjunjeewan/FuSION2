# 0007 · Spec format

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 004, Q1

## Context

Phase 1 writes a spec of FuSION 1, one pipeline stage at a time ([decision 0003](0003-phase-plan.md)).

## Options

1. Markdown, one file per stage under `docs/spec/`; each requirement a numbered item citing FuSION 1 file:line, with known bugs flagged (recommended)
2. One file, `docs/spec/SPEC.md`, covering every stage
3. A Claude Doc per stage for comments in the browser, with the approved text copied into `docs/spec/`

## Decision

Option 1. Each stage is one file `docs/spec/NN-<stage>.md`. Each requirement is a numbered item that cites FuSION 1 file:line and flags known bugs.

## Consequences

- Every requirement has a stable ID that later decisions, code and tests can cite.
