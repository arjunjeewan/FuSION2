# 0028 · How Phase 2 is run

- **Status:** Accepted
- **Date:** 2026-11-12
- **Asked in:** prompt log entry 103

## Context

Phase 2 designs FuSION 2.0's architecture ([decision 0003](0003-phase-plan.md)): the C++ backend's modules, data model and run directory; the batch language's named arguments, sweeps and partial runs ([decisions 0018](0018-batch-interface.md) and [0019](0019-partial-runs.md)); the solver layer; how the Spring Boot frontend talks to the backend; build and platforms; and the hooks Phase 3's reference tests need. Phase 1 also left questions for it, among them the named-argument and sweep syntax, plug-and-play inputs ([decision 0020](0020-plug-and-play-inputs.md)), id mapping (S2.14), where plots are drawn (S6.22), the form of `suggest_thresholds`, SMT dump reuse (S5.23), GSEA's language (S7.02) and the pipeline editor's default pipeline (S8.18).

## Options

1. Skeleton first, then parallel sections (recommended)
2. One author, section by section in dependency order
3. Poll Phase 1's deferred questions first, then write the architecture around the answers

## Decision

Option 1:

1. Claude drafts the top-level skeleton (process model, module boundaries, data flow) and polls each of its choices.
2. Subagents then draft each module's section in parallel against the skeleton, as in [decision 0026](0026-maximum-parallel-subagents.md).
3. A verification subagent checks each section.
4. Claude polls every choice a section makes, one question per choice, before it is committed.

## Consequences

- The skeleton is the contract the section drafts follow; a section that needs to change it comes back to the user as a poll.
- Every subagent prompt is logged in the prompt log, as in Phase 1.
