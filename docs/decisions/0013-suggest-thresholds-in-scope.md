# 0013 · suggest_thresholds.py is in scope

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 005, Q3

## Context

[Decision 0004](0004-functional-scope.md) left open whether helper scripts that the binary never calls are in scope. `Linux/supporting_files/suggest_thresholds.py` reads the same log-fold-change file as `cudf` and reports where quantile, MAD and SD cut-offs would fall. Nothing calls it. It is the result of FuSION 1 roadmap objective 5.

## Options

1. In scope, specified in stage 3 alongside `cudf` (recommended)
2. Out of scope, because the binary never runs it

## Decision

Option 1.
