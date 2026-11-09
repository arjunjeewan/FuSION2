# 0019 · Partial runs

- **Status:** Accepted
- **Date:** 2026-11-09
- **Asked in:** prompt log entries 019 and 020 (Q2)

## Context

The user said some users may not need the entire solve, or may want to complete a partial solve (entry 019). Claude asked which kinds of partial run 2.0 must support, several allowed.

## Options

1. Stop after any step: run part of the pipeline, such as merge and corridor without a solve, keeping each step's output on disk
2. Start from a saved step: begin from saved results, such as a merged graph or a corridor, without redoing earlier steps
3. Resume a stopped solve: continue an interrupted `pathz3` from the Pareto points it already found
4. Solve part of the frontier: only chosen Pareto points, or only the with-FSC or without-FSC case

## Decision

All four.

## Consequences

- Every step's output must be saveable and loadable, so a run can stop after it or start from it.
- `pathz3` must save enough state to resume, and must accept a restriction to chosen Pareto points or to one FSC case. Stage 5's review records what FuSION 1 supports today; Phase 2 designs the rest.
