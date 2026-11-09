# 0022 · Graph display is a FuSION 2.0 feature

- **Status:** Accepted. The design is still to be worked out with the user.
- **Date:** 2026-11-09
- **Asked in:** prompt log entry 028, Q4 (S1.23)

## Context

S1.23 asked about `supporting_files/color_files/`, which only FuSION 1's disabled graph-display commands read. The user answered: "the graph display feature is actually crucial once implemented efficiently and seamlessly. need to ideate on the right way to develop this. the color files themselves are replaceable so no need to retain them."

## Decision

- The colour files are dropped (S1.23).
- FuSION 2.0 has a graph display feature, and it must be efficient and seamless.
- How to build it is worked out with the user before it is designed. [Decision 0004](0004-functional-scope.md) keeps FuSION 1's disabled display commands out of the spec, so they are not the starting point unless the user asks for them.

## Consequences

- The stage 6 spec (psp and pex, today's solution-path and explanation plots) and the stage 8 spec (the GUI) are inputs to that work.
- Timing, chosen in prompt log entry 030 (Q4): the work happens after the stage 6 and stage 8 reviews and feeds Phase 2.
