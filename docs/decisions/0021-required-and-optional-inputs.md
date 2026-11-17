# 0021 · Required and optional inputs

- **Status:** Accepted
- **Date:** 2026-11-09
- **Asked in:** prompt log entries 021 (Q1) and 022 (Q1)

## Context

Answering S1.01 in entry 021, the user wrote: "not all files are mandatory for solve, so missing/unreadable/empty file should stop the solve only if the solve is impossible. the mode of solve should be predetermined based on inputs. for example any solve should have both lfc and pathway files defined, but if coexp csv is not provided, the solve can still take place, similarly for all the additional edge, list of edges, exceptions etc."

From that, the rule:

- Before anything runs, the inputs each requested step needs are checked. A missing required input stops the run; any solve needs an LFC file and a network.
- The optional features a run uses follow from the inputs it is given, and are settled before it starts.
- An optional input that is not given turns its feature off.
- An optional input that is given but empty counts as an empty list, as with FuSION 1's empty `inter_db_mapping_file`.

That left one case open: an optional input that is named but missing or unreadable.

## Options

1. Stop before running, so a typo in an optional path cannot cost a long run without that feature (recommended)
2. Warn and carry on without the feature, because the solve is still possible

## Decision

Option 1. The user added: "ideally there should be robust frontend validation for these cases as well."

## Consequences

- The backend validates all inputs before the first step runs.
- The Spring Boot frontend validates the same cases before it submits a run (phase 5). Phase 2 decides how the two share one set of rules.

## Phase 2 note

Amended in Phase 2 by 01.C12 (prompt log entry 188): an input that the batch itself makes is checked just before the step that reads it.
