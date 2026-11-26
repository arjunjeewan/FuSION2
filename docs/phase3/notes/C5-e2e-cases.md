# C5 · The end-to-end cases and their inputs

- **Status:** decided: Claude checked the note against section 12 and the code, and the user chose every open choice as proposed (prompt log entries 343 and 344)
- **Date:** 2026-11-26
- **Plan item:** C5 (`docs/phase3/plan.md`, Code table; its Scope puts the goldens in Phase 4); P5 as amended in prompt log entry 330
- **Specification:** section 12, "End-to-end tests" (`docs/architecture/12-tests.md:388-431`), "Platforms" (792-809), "Files, settings, events and errors" (920-936), 12.C11 and 12.C12

The engine that runs these cases is C3's `fusion-test e2e`. The normaliser and `compare_tree` are the kit's (C2).

## 1. What section 12 asks

- **A case** is `e2e/cases/<case>/` with `case.json`, a batch, `inputs/` and `expected/` (line 98). `case.json` gives the arguments, the expected status and exit, a failure's batch line and stderr fragment, tags, and one compare rule per output (lines 393-411).
- **Three compare modes (412-416):** `exact`, after masking section 09's volatile fields, absolute paths and FuSION's version; `tsv_masked`; and `results`, where witnesses pass `check()`. One golden set serves all three platforms (12.C11). A self-test proves the masks hide nothing else.
- **The case list (417-430),** and the platform tests of lines 805-809.
- **Unknown answers** come from `rlimit`, never from wall-clock limits (12.C12).
- **Goldens** are 2.0's own outputs, written by `--update` and reviewed as a diff (line 431).
- **Inputs** are synthetic: made-up ids, or FuSION 1's relabelled toy network (lines 127-137; decision 0020).

## 2. What exists now

- No case under `backend/tests/e2e/`.
- **End to end already:** the six home tests (`cmake/FusionInstall.cmake:64-87`, labels `e2e;home`) cover the staged home and its four failure codes.
- **In-process, not through a child:** the batch suite runs many of section 12's scenarios through the library (`unit/batch/test_interpreter.cpp:225-511`): a reported result skips later lines, the first failure exits 1, a stop exits 3, `exec` and the fixed-name moves, window plots skipped, an input made by the batch checked just before its step, a check error before any run directory, the toy batch as written. Also `test_cli.cpp` (usage errors, `-h`, `--commands`, `--check --json`). The end-to-end cases check the same promises through `fusion`'s process, with goldens.
- **Inputs to reuse:** FuSION 1's toy networks relabelled to `G1` to `G22`, with ids `hsa1` to `hsa22` as section 12 allows (`reference/synthetic/*/inputs/`, three distinct networks); `unit/network/data/tst00001.xml`, a synthetic KGML with `t` ids; the kit's `pathways()` and `value_file()` (C2).
- **Not ported yet,** so the cases that need them cannot pass:

  | Feature | Decision | Where 2.0 refuses it |
  |---|---|---|
  | `sweep` | 01.C3 | `batch.unknown_command`, `backend/modules/batch/src/check.cpp:482-484` |
  | `load` (start from a saved step) | 01.C6 | the same lines |
  | `--resume` | 01.C6, 09.C5 | `run.resume_unavailable`, `batch/src/handlers/cli_run.cpp:192` |
  | `points=` | 06.C9 | `solve.unsupported`, `solve/src/solve.cpp:452-453` |
  | the bundled gnuplot | 11.C20 | not built: bundled tools come with packaging (prompt log entry 307) |
  | Windows paths, `posix-shell` cases under busybox-w32 | G3, 01.C10 | no Windows build |

- **Findings for the normaliser:**
  - **Absolute paths outside `run.json`:** `events.jsonl`'s `network.read` events carry absolute `path`s, in named runs too (211 in the app's run, `scratch/app-ref-1/runs/app-ref_20261119-145456/` in the data root), and `legacy.written` carries `dest`. In a run of a positional `pathz3` line, `results.json`'s `solve.name` is the batch's absolute legacy prefix (the sprint's `scratch/f2-toy-hsa13-wni-tsv-b10-4` run). Section 12 masks only `run.json`'s paths and `run.started`'s `dir`. Section 11's identity test "relies on result files that hold no absolute path" (`11-build.md:575`).
  - **Event order:** `seq` rises across threads (`unit/run/test_run.cpp:487`), and a named `pathz3` runs its `in` and `out` cases in parallel (06.C4; `solve.threads` 0 means one per case). In the app's run the check events of the two cases interleave: the case changes twice in 25 of them. A byte-exact `events.jsonl` would therefore differ from run to run.
  - **Schemas:** results' two schemas sit in `unit/results/schemas/` (`results.schema.json`, `view.schema.json`), not in `backend/tests/schemas/`, where section 12 keeps the one copy (line 121). The plan leaves the schemas to each format's owner (plan, Scope). Until they move, the end-to-end cases validate nothing against a schema.

## 3. Design

### Layout and format

```
backend/tests/e2e/
  inputs/                 shared input sets: toy/ (the relabelled toy network and its value file),
                          gsea/ (generated KGMLs with t ids and 15 to 40 genes, a value file),
                          long-solve/ (a generated corridor whose solve takes several seconds),
                          odd-ids/ (ids with ':' and '*', nodes named CON and AUX), fake-tools/ (scripts)
  cases/<case>/case.json, case.batch, inputs/ (optional), expected/
```

`case.json` is section 12's, with these optional fields:

```json
{
  "inputs": ["toy"],
  "env": {"FUSION_GNUPLOT": "<CASE>/fake-tools/gnuplot-no-font"},
  "timeout_s": 120,
  "processes": 2,
  "actions": [{"on_event": "solve.check.started", "fusion": ["--stop", "<RUN>"]}],
  "pending": {"waits_for": "01.C3", "refused_with": "batch.unknown_command"}
}
```

- `inputs`: shared sets copied beside the case's own `inputs/`.
- `env`: variables for the child, with the path tokens expanded.
- `processes`: how many `fusion` processes start at once in one launch directory.
- `actions`: what the engine does when an event appears in `events.jsonl`, here a stop request.
- `pending`: C5.2.

### The cases

| Group | Cases | Now or pending |
|---|---|---|
| solve | `cases=in,out` on one corridor (S4.20, 06.C10); an FSC of two ids; no FSC, the `free` case alone | now |
| reported results | an empty corridor (`corridor.empty`), an empty selection (`gsea.empty_selection`), no gene past the thresholds (`observations.none`): later lines skipped with `step.skipped`, `ok`, exit 0 (S4.18, S7.03, 04.C5, 01.C15) | now, one batch each |
| sweeps | the same three as sweeps; a sweep whose failing point stops only itself and exits 1 (01.C5) | pending 01.C3 |
| partial runs | stop after a step (`exit` after it; 01.C6's first row) | now |
| | a stop request during a solve: `stopped`, exit 3 (A1), sent by an action when `solve.check.started` appears | now, on `long-solve/` |
| | start from a saved step; resume after a stop; kill, then resume from the journal (06.C19); resume under a longer timeout (06.C8); `points=` (06.C9) | pending 01.C6, 09.C5, 06.C9 |
| failures | a missing input and an unreadable one (0021); `batch.bad_value`; each `batch.bad_argument` form; an unknown command; a failing `exec` (S1.28); `-b` with a missing file (S1.01); `-h`; the dropped `-d` (S1.02, S1.03); a usage error exits 2 (01.C14); all of them once more as `--check --json` diagnostics with `command`, `setting` and `path` (1.9) | now |
| FuSION 1 forms | a FuSION 1-style toy batch with the shortest and longest positional `pathz3` lines, `mff … xml … -1`, `rgx … y`, the six fixed-name moves and the limits file in FuSION 1's rendering (01.C11), noted `batch.no_fsc` (2A.9); a positional `pex` with `both` (1.6); a `pex` file no `pathz3` wrote, `batch.unmatched_file` (07.C9); an input made by an earlier `exec` line (01.C12). The `exec` cases carry the tag `posix-shell` | now |
| tools and home | a window-plot `exec gnuplot -p`, skipped with a note when headless (entry 190); fake tools through `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT`, recorded under `run.json`'s `environment`, with a fake gnuplot that has no plot font (`deps.font_missing`) and one that writes no histogram (`expression.gnuplot_failed`) (11.C20, 07.C8, entry 230) | now |
| | `exec gnuplot` with no system gnuplot finds the bundled one | pending 11.C20 |
| paths and ids | spaces and non-ASCII characters; CRLF inputs; ids with `:` or `*`; `CON` and `AUX`, which must never become file names | now |
| | drive letters, paths over 260 characters | pending G3 (Windows) |
| run directory | the debug option on and off (S1.29, S4.19, S5.23, 1.4); two processes in one launch directory (S1.26); an absolute output path, written there and kept in the step directory, both in `run.json` (2B.3) | now |

That is about 30 cases now and 11 pending. Each carries its spec items and decisions as tags, for C6's traceability test.

### Compare rules

- Every case compares `run.json` and `events.jsonl` in `exact` mode, through `testkit::fusion_normaliser` (C2), which masks `09-runs.md:292`'s `run.json` fields, section 12's event fields (line 412) and FuSION's own version, with C5.3's event rule and C5.4's paths.
- Solve cases add `pathz3/results.json` in `results` mode, `pathz3/*/search_log.tsv` in `tsv_masked` mode (`wall_s`, `cpu_s`, `source`, `model_n`, `model_e`; the columns 2.0 writes, as in the sprint's runs), and the other text outputs in `exact` mode.
- Rendered outputs (PNG, the explanation pages) are not compared.
- `expected/` holds only the files the rules match. A toy run is 524 KB with 228 KB of compared text, so a case's golden is tens to a few hundred KB.

### Self-tests

The normaliser's mutation test (line 416): a changed unmasked field is a difference, a changed masked one is not. The pending rule: a pending case whose feature now works fails as stale, so it gets its golden when its feature lands.

## 4. Checks it runs now (P5 as amended)

The about 30 runnable cases, on Linux, in T1, with their goldens recorded by `--update` and reviewed as a diff (C5.1). Each pending case runs too: it must still be refused with its recorded code, and is reported as skipped. The macOS and Windows runs wait for those builds (P5 as amended).

## 5. Open choices

### C5.1 · When the goldens are recorded
- **Proposed:** now, as each case lands, for the runnable cases; each `--update` is a reviewed diff.
- **Alternatives:** in Phase 4, as the plan's Scope says ("when `fusion` first runs the cases in Phase 4").
- **Why:** the plan put the goldens in Phase 4 because `fusion` did not exist yet. It runs now, and a case without a golden checks only its exit status. Amended P5 runs 2.0's checks as each harness lands.
- **Decision:** as proposed (prompt log entry 343)

### C5.2 · Cases that wait for an unported feature
- **Proposed:** write them now, with `pending`. The engine checks that `fusion` still refuses them with the recorded code, reports them as skipped, and fails a pending case as stale once its feature works.
- **Alternatives:** write each case when its feature is ported.
- **Why:** the case is designed while section 12 is fresh, the skip is visible in every run, and a newly ported feature cannot go untested by oversight.
- **Decision:** as proposed (prompt log entry 343)

### C5.3 · `events.jsonl` under parallel cases
- **Proposed:** `exact` mode compares `events.jsonl` as one stream per (step, sweep point, case), each in its own order, with `seq` masked.
- **Alternatives:**
  - Run the cases' solves with `--set solve.threads=1`, so events keep one order; the default threading is then untested end to end.
  - Compare only the sequence of event types per step.
- **Why:** the order inside a case is deterministic, but the interleaving of parallel cases is not (section 2). Per-case streams keep section 12's byte comparison wherever order is defined, and test the default `threads`.
- **Decision:** as proposed (prompt log entry 343)

### C5.4 · Absolute paths in result files
- **Proposed:** the normaliser writes `<RUN>`, `<CASE>` and `<DATA>` in every compared file, not only in `run.json`. `results.json`'s absolute `solve.name` goes to the results module's owner as a finding against section 11's identity rule.
- **Alternatives:**
  - Change `results.json` first, so it names a positional line's prefix relative to the run (a results-module change).
  - Mask `solve.name` and the two event fields.
- **Why:** the tokens keep the goldens portable without hiding the field, and the module question stays a separate decision.
- **Decision:** as proposed (prompt log entry 343)

### C5.5 · The six home tests
- **Proposed:** keep them as CMake scripts.
- **Alternatives:** rewrite them as end-to-end cases.
- **Why:** they edit a copy of the staged home, which `case.json` cannot express without a setup step, and they pass. The fake-tools case adds what they lack: `FUSION_GNUPLOT`, `FUSION_DOT` and the `environment` record.
- **Decision:** as proposed (prompt log entry 344)

## 6. Cost

- **Size:** about 41 case folders, each a `case.json` of about 25 lines, a batch of 10 to 60 lines and 2 to 8 golden files; 5 shared input sets of about 15 files. About 200 files in all, under 3 MB of goldens. No KEGG data.
- **Run time:** the sprint's toy runs took about 2 s each in Debug (round 4: 24.7 s for 12), and the stop case a few seconds more. About 1.5 min of CPU for the runnable cases, under 30 s of wall time on 8 parallel CTest jobs, inside T1's 5 min.
