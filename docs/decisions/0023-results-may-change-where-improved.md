# 0023 · Old batch results may change where 2.0 improves on FuSION 1

- **Status:** Accepted. It amends [decision 0017](0017-batch-file-compatibility.md).
- **Date:** 2026-11-10
- **Asked in:** prompt log entry 035, Q3 (S2.11)

## Context

[Decision 0017](0017-batch-file-compatibility.md) said a FuSION 1 batch file that worked must give the same result in 2.0, with changes only where FuSION 1 was buggy. Answering S2.11 (indirect-effect relations), the user wrote: "both fixes are good, should be configuration with default being discarding indirect effects. old batches need not give the same results provided we are improving so much".

## Decision

FuSION 1 batch files still run in 2.0, but their results may change wherever 2.0 improves on FuSION 1, not only where FuSION 1 had a bug.

## Consequences

- An improvement can be chosen even when it changes an old batch's result. The poll question then says that it does.
- Phase 3 reference tests compare against FuSION 1 only where no such improvement applies, and record each expected difference.
