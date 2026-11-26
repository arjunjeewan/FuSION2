# C4 · The reference runner, `cases.json` and `expected-differences.json`

- **Status:** decided: Claude checked the note against section 12 and the code, and the user chose every open choice as proposed (prompt log entry 342)
- **Date:** 2026-11-26
- **Plan item:** C4 (`docs/phase3/plan.md`, Code table); decision 0037 (shared `cases.json` and `run_fusion1.py`); P5 as amended in prompt log entry 330
- **Specification:** section 12, "Reference-test runner" (`docs/architecture/12-tests.md:433-740`), "Determinism" (773-790), 12.C4 to 12.C7, 12.C16, and the runner's test hooks (1210-1214)

Kit names are those of the C2 note; `fusion-test`'s command line and CTest registration are the C3 note's.

## 1. What section 12 asks

- **`fusion-test ref`** in C++, linking `libfusion` and the kit, with the sketch's types and functions (lines 437-496). It runs the 12 synthetic and 39 KEGG cases of lines 498-556, each with its `inputs/` copied into a scratch directory that is `$CASE`.
- **Three modes (566-576):** chained (the batch through `fusion` after the path map), pinned (each stage at library level on FuSION 1's recorded input, so a difference belongs to its stage), and verify (T3). A chained difference counts as expected only if upstream pinned differences reach it.
- **Comparisons (578-622):** a rule per stage, from classify to solve; witnesses judged by the kit's checker under rules R1 to R4; an unknown answer is inconclusive, never failed.
- **Expected differences (624-676; 12.C6):** 27 entries, each with `applies_when` evaluated per case, `allows`, an optional checked `predicts`, and `seen_in`; a stale entry fails until `--record-seen` rewrites the list, reviewed as a diff.
- **Verification (678-697; 12.C7):** at most 2k + 1 fresh single-point checks per frontier, each in its own worker process.
- **Reports (699-732):** `report.json` and `summary.tsv` outside any run directory; a failing case keeps its run directory.
- **T2 (line 83):** the 23 fast KEGG cases, the non-solve stages of all 39, and repeat-run determinism, in 15 min.

## 2. What exists now

- **`backend/tests/reference/cases.json`:** all 51 cases, written by `backend/tests/experiments/capture/make_cases.py`, in section 12's form plus an `origin` block that the script's docstring says the runner may ignore (`make_cases.py:23-27`). `batch` is relative to `backend/tests/fixtures/`. The toy batches name `/fusion-test/case`, mapped to `$CASE`.
- **`run_fusion1.py`** (decision 0037) prepares a case the way the runner must: the path map applied as text, longest `from` first, to the batch and to every `inputs/` file whose name contains `list` (`run_fusion1.py:96-100, 168-173`); a refusal when a path into FuSION 1's tree or snapshot is left (`:111-114`); a fresh folder under the root's `scratch/`.
- **The data root** holds all 39 captures (W3) and W2's confirmation that the snapshot binary reruns five cases byte for byte (`reports/w2/20261117T183307/report.txt`).
- **Claude's scratch harness** (`/tmp/claude-1000/-home-ks-149-u24-Desktop-arjun-FuSION2/b9b5d617-7cc2-4860-8cba-af70fbd07c02/scratchpad/sprint/`), which C4 replaces:
  - `run_fusion2.py` runs a case's batch through 2.0's `fusion` with `run_fusion1.py`'s preparation, and compares the `.dat` files by FuSION 1's file name, byte for byte;
  - `run_pinned.py` pins FuSION 1's GSEA shortlist by editing the batch;
  - `run_t1_t2_r4.sh` runs the cases three at a time.

  Round 4: 66 of 70 frontiers match. The other 4 are XD-07 (`egfhela-nfkb1-b6-r5-12` in, `model-example` out) and XD-01 (`jun-b7-fdr040keep`, both), and the last matches with FuSION 1's shortlist pinned (prompt log entries 306, 326).
- **What 2.0 writes for a FuSION 1 batch.** Each positional `pathz3` line is one `free` case in its own step, `pathz3` then `pathz3-2`. `run.json` records each step's settings, including `solve.legacy_prefix` (`…/run/temp_w`). 01.C11 writes FuSION 1's `.dat` and limits files natively into the batch's `$WRITE_DIR`. `results.json` holds the corridor's nodes with their members, each case's points, flags and witnesses.
- **Gaps:**
  - no `expected-differences.json`;
  - `points=` (06.C9) is not ported: `solve.unsupported` (`backend/modules/solve/src/solve.cpp:452-453`); neither are `answers_from=` (06.C12) nor loading a saved solve (06.C7, `solve.cpp:544`);
  - the toy batches carry the GUI template's unfilled placeholders (`fixtures/toy-batches/*.batch:70-72`, `%__a21__%` to `%__a23__%`), which S5.02's decision makes an error. The sprint edits them away in two different ways: the harness drops `pathz3`'s optional tail (`run_fusion2.py:18-21`) and the batch suite erases the placeholders (`unit/batch/test_interpreter.cpp:391-406`);
  - section 12's examples name cases `jun-b10-full` and `jun-b10-keep040`; `cases.json` calls them `jun-b10-old` and `jun-b10-fdr040keep`. The runner uses `cases.json`'s ids;
  - section 12's XD-07 `seen_in` example lists three T3 cases only, but the sprint saw XD-07 in two T2 cases;
  - XD-01's predicted shortlist sizes cover fdr:0.25, 0.4, 0.44 and 0.5; the sprint found that the keep-list case `jun-b7-fdr040keep` selects 90 pathways where FuSION 1 selected 87 (prompt log entry 306), which the entry should add.

## 3. Design

### Files

```
backend/tests/tools/fusion-test/ref/
  case.hpp/.cpp        load_cases, PathMap, preparing a case's folder
  expect.hpp/.cpp      load_expectations; the predicates and predictions
  chained.cpp          the batch through fusion
  pinned.cpp           each stage at library level
  verify.cpp           verification_points and the point workers
  facts.cpp            FuSION 1's and 2.0's facts per stage, as kit types
  rules.cpp            R1 to R4
  report.cpp           write(): report.json, summary.tsv
  run.cpp              run(): workers, outcomes
backend/tests/reference/expected-differences.json      the 27 entries
backend/tests/reference/cases.json                      as now, plus C4.1's edits
backend/tests/unit/fusion-test/ref/                      the runner's self-tests
```

The types are section 12's, with the additions of C4.4. `make_cases.py` stays the generator of `cases.json`, so every field is reproducible.

### Preparing a case

The case folder is `<out>/cases/<id>/<mode>/`, under the report directory, which defaults to `$FUSION_TEST_DATA/reports/ref/<UTC time>/`. The runner copies the case's `inputs/` (from the root, or from `reference/synthetic/<id>/inputs/` for a toy), applies the path map with `run_fusion1.py`'s rules, refuses leftovers in the same way, and then applies the case's edits (C4.1). The rules are restated in C++ rather than shared, and `cases.json` is the one contract both runners read.

### Chained

`fusion -b case.batch --run-dir run` in the case folder, through `testkit::run_fusion`, with stdin from the null device. `testkit::read_run(run)` gives the solves. Each FuSION 1 solve maps to the 2.0 step whose `solve.legacy_prefix` ends in its arm: `temp_w` for `in`, `temp_wo` for `out`, and the one solve for `free` (`r7`, `r8`). Stage facts come from the run's files: `cudf/up_reg` and `down_reg`, `mff/exceptions.tsv`, the `gsea/` table and shortlist, `mff/merged_graph.xml` through `network::read_fusion_network` and `adapt::canon`, the corridor from `results.json`, `fb_rch/observations_match.tsv`, the solve's counts and frontiers. The `.dat` byte check of the scratch harness stays as the first solve fact.

### Pinned

In-process, with a `testkit::StepContext` per stage, each stage on FuSION 1's recorded input (lines 569-573):

| Stage | Input from FuSION 1 | 2.0 call | Compared with |
|---|---|---|---|
| classify | the LFC file, the `cudf` line's thresholds | expression's reader and classifier | `up_reg`, `down_reg`: sets |
| exceptions | FuSION 1's `up_reg` and `down_reg`, the `start` file, the endpoints | `merge::assemble_exceptions` | `exception_file.txt`: sets |
| GSEA | LFC file, candidate list | enrichment's step on one thread | `.gsea.tsv`, shortlist: exact as doubles (12, GSEA agreement) |
| merge | FuSION 1's shortlist and exception file | `merge::merge_direct`; `network::read_legacy` for old-master cases | `graph_reread.xml` via `f1::read_written_graph`: canonical equality |
| corridor | `graph_reread.xml` through 2.0's legacy reader | `corridor::compute` | `corridor.xml`, console counts |
| observations, solve inputs | as above | `corridor::match_observations`, the solve's counts | console's `Up:`, clause counts |
| solve | 2.0's corridor from the row above, FuSION 1's up and down lists and list files, the FSC named | `solve::bind_inputs`, `make_case`, `solve_cases` | `.dat`, limits answers, witnesses: R1 to R4 |

Each stage's arguments come from the case's own batch, parsed and bound by the batch module as its corpus test does (`unit/batch/test_check.cpp:364-388`), with no file checks. The 16 T3 cases run every row but the last. This replaces the scratch harness's batch-level GSEA pin: the pinned merge row starts from FuSION 1's shortlist directly.

### Witness rules

R1 to R4 as section 12 states them (lines 610-616), all through `testkit::check`:
- R1 runs on every 2.0 witness, chained and pinned, under `fusion2` on the corridor it came from. FuSION 1's own witnesses were checked once, by C2's self-test of all 177.
- R2 re-scores each FuSION 1 witness (`testkit::from_f1`) under `fusion2` on 2.0's corridor; R3 scores each 2.0 witness under `fusion1` on FuSION 1's captured corridor.
- FuSION 1's case spec comes from its arm's list files (`f1::read_ids`), `up_reg` and `down_reg`, `f1::read_coexpression` at the line's threshold, and the frozen threshold.
- Assignments use canonical keys, so they move between the two corridors. Where the checker rejects one on the other side (XD-03, XD-08, XD-16, XD-19), the finding is "not applicable", with the checker's reason.

### Verify (T3)

`verification_points(frontier, box_lo, box_hi)` as section 12 defines it. Each point is one worker process, `fusion-test ref --check-point <spec.json>` (C4.2), which builds the pinned solve's case and calls `solve::CaseSolver::check(point, Phase::point)` once, in a fresh context. A `sat` answer's witness must pass R1. T3 is not run in Phase 3: it is about 12 h on 16 workers and is booked with the user (12.C1). C4 checks verify mode on the T1 cases.

### Expected differences

- The file is section 12's format. `applies_when` names one predicate from a closed list, with its arguments, and each predicate is one C++ function over the case's facts. There are about 22 kinds, one per row of section 12's "Applies when" column: `always`, `stage_runs` (XD-01), `raw_merge` (XD-03 to XD-06, XD-16), `fusion1_witness_relaxes_any` (XD-07), `id_in_several_nodes` (XD-08), `fsc_down` (XD-10), and so on. An unknown name is exit 2.
- `predicts` has four kinds: `rescore_bound` (R2's bound; XD-07, XD-17), `counts` (XD-17's corridor sizes from section 05's replay), `unchanged` (XD-21) and `selection_size` (XD-01's 44, 79, 86 and 91 pathways).
- By section 12's table, 13 entries apply to none of the 24 recorded runs and are tested on synthetic cases. The table did not assess the 15 regenerated EGF-HeLa cases or the model example, so the first T2 run settles those.
- **Stale** applies to the cases a run covered: a T2 run cannot make an entry stale for a T3 case.
- `seen_in` starts from section 12's measurements; `--record-seen` after the first T1 and T2 runs adds what the sprint found, as a reviewed diff.

### Determinism (T2)

A second chained run of every T1 and T2 case (C4.3), compared byte for byte with the first after `testkit::fusion_normaliser` (12, Determinism, "Repeat").

### Self-tests

The test hooks (1210-1214): on a toy case with injected differences, the runner reports an unexpected difference, an allowed one, a failed prediction and a stale entry. Also: preparation refuses a leftover FuSION 1 path; the arm-to-step mapping on a toy run; `verification_points` on hand-made staircases.

## 4. Checks it runs now (P5 as amended)

| Check | Needs | Expected from the sprint |
|---|---|---|
| T1: the 12 toys, chained and pinned, with R1 to R4 and repeat runs | nothing outside the repository | every frontier equal (round 4) |
| T2: the 23 fast KEGG cases, chained and pinned; the non-solve stages of the 16 T3 cases; repeat runs | data root, verified (C3); the `ci-linux` Release build (C3.3); a booking, since it loads 8 cores for about 15 min | 32 of 35 chained cases equal; `egfhela-nfkb1-b6-r5-12` and `model-example` under XD-07; `jun-b7-fdr040keep` under XD-01 chained and equal pinned |
| the witness checker on every witness 2.0's solve produces in T1 and T2 | the kit's checker (C2) | none seen yet: the sprint compared frontiers only |
| the first `--record-seen` | the two runs above | a reviewed diff of `seen_in` |

## 5. Open choices

### C4.1 · Batch lines that 2.0 refuses by decision
- **Proposed:** `cases.json` gains an `edits` list per case, applied after the path map, each with a reason, for example `{"drop_pathz3_tail": "%__a21__%", "why": "S5.02: the GUI template's unfilled placeholders are an error in 2.0"}`. The report lists every edit applied. `make_cases.py` writes them for the 12 toys.
- **Alternatives:**
  - An expected difference that allows the toys' chained runs to fail; chained toys then compare nothing.
  - Fill the placeholders in `fixtures/toy-batches/`, so the batches differ from the ones FuSION 1 ran in W4.
- **Why:** an edit keeps the frozen batch as FuSION 1 ran it and makes the deviation visible in the report. One rule replaces the sprint's two different ones.
- **Decision:** as proposed (prompt log entry 342)

### C4.2 · Single-point checks for verification
- **Proposed:** a worker mode of `fusion-test` calls `solve::CaseSolver::check` on the pinned case, one point per process.
- **Alternatives:** `fusion … points=n,e`, as section 12 says, once 06.C9 is ported; T3 then waits for that port.
- **Why:** the worker gives what section 12 wants from `points=`, a fresh context per point and crash isolation, without an unported feature. Its case is the pinned solve's, built from FuSION 1's inputs, which a FuSION 1 batch would rebuild through every earlier stage. The worker can call `points=` once that exists, if the user prefers.
- **Decision:** as proposed (prompt log entry 342)

### C4.3 · Which cases the repeat check covers
- **Proposed:** every T1 and T2 case.
- **Alternatives:**
  - Leave out `jun-b10-fdr025` and `jun-b10-fdr030`, whose FuSION 1 solves took 615 s and 738 s.
  - T1 only.
- **Why:** section 12 names no subset. With every case repeated, T2 is about 5,900 s of case time in Debug (Cost), which 8 workers finish in about 12 to 14 min if the long runs start first: inside 15 min, though not by much. Leaving out the two long ones saves about 1,000 s of case time, which matters only if the first booked run overruns.
- **Decision:** as proposed (prompt log entry 342)

### C4.4 · Departures from section 12's sketch
- **Proposed:** accept: `Stage::observations` (section 12's comparison table has the row, its enum does not); `Case::origin` and `Case::edits`; `Options::cases` and `Options::modes`, to run a subset; the `--check-point` worker; `cases.json`'s case ids in place of the section's example ids.
- **Alternatives:** follow the sketch literally.
- **Why:** each addition serves a row or a rule that section 12 already states.
- **Decision:** as proposed (prompt log entry 342)

## 6. Cost

- **Size:** about 15 files and 2,800 lines of C++ (pinned stages about 700, facts about 500, predicates and predictions about 500, R1 to R4 about 300, the rest about 800), `expected-differences.json` at about 700 lines, a `make_cases.py` change for the edits, and about 300 lines of self-tests.
- **Run time:**
  - **T1:** about 25 s of case time per mode in Debug (round 4's 12 toys), so about 1 min with pinned and repeat runs, seconds of wall time on several workers.
  - **T2:** round 4's chained runs took 1,862 s of case time in Debug; pinned runs and repeats add about as much again each, and the T3 non-solve stages a few minutes: about 5,900 s in all. On 8 workers that is 12 to 14 min, bounded below by `jun-b10-fdr025`'s 737 s per run (Debug, at nice 19 beside the user's jobs). A Release build should be faster.
  - **T3:** 312 checks, about 12 h on 16 workers (12, How long); not run in Phase 3.
