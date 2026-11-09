# 0008 · Spec stages and their order

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 004, Q2

## Context

The stages shown with the poll, word for word:

> 1. **Batch language and run setup:** `fusion -b`, the script commands let, start, exec, exit and help, and the files in `supporting_files/` (config, paths, node and edge type lists, colour files).
> 2. **Graph input and merging:** add, rgx, mff, wgx, upfx and sz, including the known merge inconsistencies.
> 3. **Expression data:** cudf (from LFC data to the up/down file and its thresholds).
> 4. **Reachability corridor:** fb_rch.
> 5. **Encoding and Pareto solve:** pathz3. This covers the Z3 constraints, relaxations, FSC, the coexpression and frozen constraints, timeouts and SMT dumps.
> 6. **Results and plots:** psp, pex (`plot_explanation.py`) and `plot_po.sh`.
> 7. **Pathway shortlisting:** gsea (`gsea_select_pathways.py`).
> 8. **Frontend workflow:** the live Swing GUI.

## Options

1. Approve, in pipeline order, so each stage builds on the data structures the stage before it defines (recommended)
2. Solver first: start with stage 5, then go back to stage 1

## Decision

Option 1.

## Consequences

- Stages are drafted in parallel ([decision 0010](0010-spec-reading.md)), so this order sets the order of review and approval.
