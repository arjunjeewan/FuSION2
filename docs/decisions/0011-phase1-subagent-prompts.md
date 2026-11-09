# 0011 · Phase 1 subagent prompts

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 005, Q1

## Context

[Decision 0010](0010-spec-reading.md) has one subagent per stage draft the spec. The user reviewed the draft prompts before any were sent: a shared brief (ground rules, the item template, granularity, the reply format) plus one section per stage.

## Options

1. Approve as drafted, aiming for about 10 to 30 items per stage (recommended)
2. The same prompts, aiming for 5 to 15 items per stage

## Decision

Option 1. The prompts as sent are in the prompt log: the shared brief in entry 006 and the stage sections in entries 007 to 014. The stage 8 section was widened after [decision 0014](0014-stage8-gui-scope.md).

## Consequences

- Every spec item has the same fields: FuSION 1 location, what it does, problems with evidence, a proposed keep, fix or drop, and the user's decision.
- Each subagent writes only its own `docs/spec/NN-*.md` and commits nothing.
