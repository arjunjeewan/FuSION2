# 0018 · FuSION 2.0's interface: a batch language with named arguments and sweeps

- **Status:** Accepted
- **Date:** 2026-11-09
- **Asked in:** prompt log entries 018 (Q1), 019 and 020 (Q1)

## Context

Asked whether 2.0 should keep FuSION 1's batch language, the user asked for the alternatives (entry 018). Claude laid out four and recommended a declarative job file (entry 019). The user answered that pipeline order must stay very flexible, because some users don't need the entire solve or want to complete a partial solve, and the batch framework allows that; that option 2's readability is an improvement; and that the backend's two inputs should be plug and play.

## Options

1. FuSION 1's batch language with the stage 1 fixes
2. The batch language with named arguments, positional lines still accepted
3. A declarative job file with a converter for FuSION 1 batch files (recommended in entry 019)
4. Embedded Lua scripting

In entry 020 Claude put back its reading of the answer as option 2 (recommended), or option 2 plus sweeps.

## Decision

Option 2 plus sweeps:

- one command per line, so any subset of steps runs in any order;
- arguments by name (`name=value`) or by FuSION 1's positions ([decision 0017](0017-batch-file-compatibility.md));
- a simple parameter list, so one file can run a bound or FSC sweep instead of cloned files.

## Consequences

- Stage 1's language items stay in play, and their calls apply to this language.
- Phase 2 designs the named-argument and sweep syntax.
- Decision 0017 applies, because the batch language stays.
