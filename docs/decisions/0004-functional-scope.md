# 0004 · Functional scope

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 002, Q4

## Context

The survey of FuSION 1 shown with the poll, word for word (file links removed):

> - **Backend (C++):** about 94k lines of tracked source under `Linux/`. GraphManagerNew.cc alone is 31k lines and main.cc is 7.9k. `macOS/` is a second copy that has drifted from it.
> - **Live commands:** about 15 still run after the debloat:
>   - rgx, mff and wgx (read, merge and write KEGG graphs)
>   - fb_rch (reachability corridor)
>   - pathz3 and get_assg_z3 (Z3 Pareto solve)
>   - cudf, rmap, upfx and sz
>   - psp, pex and gsea
>
>   About 100 more command names sit in disabled `#if 0` code. Examples are ApproxMC counting, mincut, dominators and the SBML/Reactome readers.
> - **Helpers outside C++:** GSEA uses gseapy, explanation graphs use PyVis, Pareto plots use gnuplot, and threshold suggestion is a Python script.
> - **Frontend:** a Java Swing app of about 10k lines that writes batch scripts for `fusion -b`.

## Options

1. Live set, legacy listed: port everything the current build runs, and give each disabled command one line in the spec (recommended)
2. Live set and all legacy: also port the roughly 100 disabled commands
3. Live set only: leave the disabled code out of the spec entirely

## Decision

Option 3. FuSION 2.0 ports only what the current FuSION 1 build runs. Code inside FuSION 1's `#if 0` regions is not inventoried.

## Consequences

- The live command list above comes from a scan of FuSION 1's `main.cc` and is provisional. The phase 1 spec confirms it command by command.
- Correction found before the first commit: the survey's scan skipped only `#if 0` regions. A rescan that also strips `//` and `/* */` comments shows:
  - `get_assg_z3` is commented out and marked dead (main.cc:5935), so it is not live;
  - `add` (plaintext edge file to KEGG-format XML, main.cc:5841) is live but was missing from the survey.

  Corrected live set: add, rgx, mff, wgx, upfx, sz, fb_rch, cudf, pathz3, psp, pex and gsea, plus the script commands let, start, exec, exit and help.
- Two scope questions the scan could not settle, to be polled in phase 1:
  - helper scripts the binary never calls, such as `suggest_thresholds.py`;
  - which of the three Java GUIs under FuSION 1's `Linux/` is the live frontend: `frontend/`, `frontend1/` or `GUIForExecutable.java`.
