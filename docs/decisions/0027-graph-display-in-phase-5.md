# 0027 · The graph display is designed in Phase 5

- **Status:** Accepted. It amends the timing in [decision 0022](0022-graph-display.md).
- **Date:** 2026-11-12
- **Asked in:** prompt log entry 102

## Context

[Decision 0022](0022-graph-display.md) made the graph display a crucial 2.0 feature and planned to work out its design after the stage 6 and stage 8 reviews, feeding Phase 2. Those reviews have since settled much of what it must show and do:

- the whole witness, with each node's activity and each edge honoured or relaxed (S6.05, S6.06, S6.15);
- one explanation renderer, with psp's path filter and edge-source table as options and a static export (S6.07, S6.26);
- sign-consistent routes reported separately from presence (S5.13);
- each run's results shown in the frontend and linked from its session (S8.29);
- a working pipeline editor whose default pipeline is the standard run (S8.18).

## Options

1. Options paper, then polls: research subagents measure what must be drawn and compare approaches, then each choice is polled (recommended)
2. The user describes the display first
3. Defer to Phase 5

## Decision

Option 3. The graph display is designed in Phase 5, together with the frontend screens. Phase 2 goes ahead without it.

## Consequences

- Decision 0022's requirement stands: the display must be efficient and seamless.
- Phase 2 must still give the backend's results a form the display can use, but it does not choose the display's approach.
