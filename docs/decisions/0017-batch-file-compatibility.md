# 0017 · Compatibility with FuSION 1 batch files

- **Status:** Accepted. It applies because [decision 0018](0018-batch-interface.md) keeps the batch language. Amended by [decision 0023](0023-results-may-change-where-improved.md): results may also change where 2.0 improves on FuSION 1.
- **Date:** 2026-11-09
- **Asked in:** prompt log entry 018, Q2

## Context

FuSION 1 runs from batch files: 81 of them in the repo (78 `*.batch` files under `runs/` and `macOS/model_example/`, plus three GUI scripts). Stage 1 of the spec proposes fixes to the batch language, such as longest-match `$` expansion.

## Options

1. Yes, except where FuSION 1 was buggy: a batch file that worked in FuSION 1 gives the same result in 2.0, and fixes change behaviour only where FuSION 1 went wrong, as with `$PLOT_FILE_WO_GENE` (recommended)
2. No, a clean break: the language may change its syntax freely, and old batch files are converted once

## Decision

Option 1.

## Consequences

- Every stage 1 fix must leave the results of a correctly working FuSION 1 batch file unchanged.
