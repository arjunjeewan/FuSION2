# 0038 · A first end-to-end version this week, built ahead of the rest of Phase 3 (amends 0003 and 0036)

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 279

## Context

On Tuesday 2026-11-17 the user asked for an end-to-end working version of FuSION 2.0 by the end of that week, to be iterated on later, with the build first and thorough implementations, because "most implementations have already been done in fusion 1" (prompt log entry 278). [Decision 0003](0003-phase-plan.md) puts Phase 3's reference tests before the backend, and [decision 0036](0036-phase-3-method.md) runs Phase 3 from its plan, with each piece of code through a design note, the user's OK, code with tests and a diff review.

## Options

Poll 279 asked four questions:

1. **Scope:** the backend and a minimal desktop app (recommended), or the backend CLI only.
2. **Phase 3:** build now with FuSION 1's references recorded alongside (recommended), or finish Phase 3 first.
3. **Method:** parallel subagents porting FuSION 1's code module by module (recommended), or Claude alone, module by module.
4. **Reviews:** one approval per module (recommended), one approval per day, or the full loop of decision 0036.

## Decision

The recommended option of each:

1. This week's version is the backend and a minimal app. The `fusion` CLI runs FuSION 1's batches end to end: KGML and master reading, the `mff` merge, `cudf`, `fb_rch`, `pathz3` on Z3 with the `in`, `out` and `free` FSC cases, results and the Pareto plot, GSEA as the exact gseapy port, and explanation pages. A minimal JavaFX app logs in, edits the default pipeline, runs it, shows progress and opens the results.
2. Phase 3's test kit, runner and experiments are paused; their scripts, written for booking 1, stay. Booking 1 keeps its captures and references (W1 to W6), so the build is checked against FuSION 1 from the start. The rest of Phase 3 resumes after the first version.
3. Claude sets up the build skeleton and every module's interface header from sections 01 to 09. One subagent per module then ports FuSION 1's working code into its module, applying the spec's fix decisions and its section's design, with unit tests. Claude integrates daily and runs the reference batches end to end.
4. During the sprint there are no separate design-note approvals. Each module is committed once its tests pass and the user has approved its diff summary. The user reviews the working version at the end of the week.

## Consequences

- Resume after a kill, partial runs, sweeps, the graph display, installers and the macOS and Windows builds come in later iterations, through the architecture's choices as decided.
- The diff approval before every commit (CLAUDE.md, [decision 0001](0001-repo-location-and-hosting.md)) stays.
- Phase 4's module order and its review loop apply again after the sprint, to what the sprint leaves unfinished.
