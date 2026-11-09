# 0009 · How spec items are reviewed

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 004, Q3

## Context

Each spec item gets a keep, fix or drop call ([decision 0003](0003-phase-plan.md)).

## Options

1. Straight ports listed as keep; every proposed fix or drop, and anything ambiguous, gets its own poll question; the stage file is then approved as a whole (recommended)
2. Every spec item gets its own poll question, straight ports included
3. The user fills in a Decision field in the file, and Claude checks and records the calls

## Decision

Option 2. Every spec item, straight ports included, gets its own poll question with a recommended call first. The answer is written into the item.

## Consequences

- A poll holds at most four questions, so a stage of n items takes about n/4 polls.
