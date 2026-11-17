# 0037 · Booking 1's scripts: Claude writes the case inventory and the fill script, subagents the rest

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 266

## Context

Booking 1 of the Phase 3 plan ([`docs/phase3/plan.md`](../phase3/plan.md), [decision 0036](0036-phase-3-method.md)) runs W1 to W6 and the experiments X02, X03, X05, X06 and X08. Their scripts all read one case inventory: the 39 KEGG cases and the 12 toy cases, each with its FuSION 1 run folder (batch, `inputs/`, `run/` outputs, console log), its artefacts and its path map. That is the data of plan item C4's `cases.json`, in section 12's format. FuSION 1's tree has grown since section 12's census (125 batch files on 2026-11-17, 107 then), but the 39 cases are fixed.

## Options

1. Claude writes the inventory and W1's fill script, which everything else reads; subagents then write W2 to W6 and the five experiments in parallel ([decision 0026](0026-maximum-parallel-subagents.md)) (recommended)
2. Claude writes everything, in order
3. Subagents write everything in parallel, the inventory included

## Decision

Option 1:

1. Claude writes `backend/tests/reference/cases.json` with the 51 cases, ahead of the rest of plan item C4, and W1's `fill_data_root.sh`.
2. Subagents then write W2 to W6 and X02, X03, X05, X06 and X08 in parallel, against the inventory. Each script comes with a dry-run check, and no subagent runs FuSION 1 or loads the workstation.
3. Claude reviews each script before the user sees its diff (decision 0036's loop).

## Consequences

- `cases.json`'s data exists before the reference runner that reads it; plan item C4 then adds the runner and `expected-differences.json`.
- Every subagent prompt is logged in full ([decision 0002](0002-prompt-log-scope.md)).
