# Phase 3 plan

- **Status:** Accepted; choices P1 to P6 decided in prompt log entries 262 and 263
- **Date:** 2026-11-17
- **Method:** [decision 0036](../decisions/0036-phase-3-method.md) (prompt log entry 261)

## Scope

Phase 3 is the reference tests ([decision 0003](../decisions/0003-phase-plan.md)). Section 12 of the architecture ([12-tests.md](../architecture/12-tests.md)) fixes what it holds: Phase 3 "builds the test kit, the data root and its reference store, the runner, the expected-differences list and the end-to-end fixtures, and runs the experiments". This plan orders that work, gives each item its inputs, machine time, outputs and what the user reviews, and lists the choices section 12 left open (P1 to P6).

Not in Phase 3:
- each module's code and unit tests (Phase 4), and the JSON Schemas, which each format's owner writes with its module (12, Repository layout);
- the end-to-end goldens: they are 2.0's own outputs, recorded with `fusion-test e2e --update` when `fusion` first runs the cases in Phase 4;
- `fake-fusion` and the frontend's tests (Phase 5);
- the golden search, at the end of Phase 4 (12.C1);
- X01, X04, X09 and X10, which only document FuSION 1's behaviour (12.C15).

## Rules

- **Bookings.** Every run that loads the workstation (36 cores, shared with other FuSION 1 work) is booked with the user, and never started beside the user's own runs (12, open question 6; prompt log entry 209). Before a booked run starts, Claude checks the load average and the running `fusion` and `z3` processes, and asks again if the machine is busy.
- **FuSION 1 stays read-only.** Its files are copied into the data root and run from there, each batch in its own scratch directory, because FuSION 1 writes fixed-name files to its launch directory (S1.26).
- **KEGG data never enters the repository** (S8.31, 12.C3). It lives in the data root, outside both repositories.
- **Code** follows Phase 4's loop ([decision 0036](../decisions/0036-phase-3-method.md)): a design note, the user's OK, code with tests, the user's review of the diff, then a commit. For capture scripts and experiment code, the work item below is the design note.
- **Findings come back as polls.** Each experiment that bears on a decision is reported, and the decision is polled: X11 on the Z3 pin, M01 on pruning, M03 on vis-network, M04 on the counter encoding, and the others on their spec questions.
- As in Phases 1 and 2: every prompt, poll and subagent prompt is logged ([decision 0002](../decisions/0002-prompt-log-scope.md)), every decision gets a record, and commits keep the co-author line until de-claude runs before a push ([decision 0034](../decisions/0034-co-author-line-kept-until-push.md)).

## Work items

### Captures and FuSION 1's references

| Id | Item | Inputs | Machine time | Outputs | The user reviews |
|---|---|---|---|---|---|
| W1 | Fill the data root and write its manifest (P1, P2) | FuSION 1's live tree, read only: the 163 + 4 KGMLs, the old master, the model example, the EGF file, the coexpression file, each case's `inputs/` and outputs, the 26 dumps X07 re-checks | minutes (150 to 200 MB) | the data root; `backend/tests/reference/data-manifest.json` | the manifest's file list |
| W2 | FuSION 1's binary: copy it with the snapshot's `supporting_files/`, check the live sources against the snapshot's `MANIFEST.sha256`, and rerun the five fastest recorded cases (12, FuSION 1 as the reference) | the live `Linux/fusion`; the snapshot | a few minutes | `fusion1/Linux/` in the data root; a note | the note |
| W3 | Capture batches for the 39 KEGG cases: each case's batch cut before its first `pathz3`, with `wgx` after `rgx` and after the `fb_rch` before it (12, Capture in Phase 3) | W1, W2; path maps | about 30 min | `reference/<case>/f1/capture/` in the data root | node and edge counts per case |
| W4 | Regenerate the 12 toy cases on relabelled copies, the 15 EGF-HeLa b6 cases and the model example | batches rebuilt from the GUI template; the old master | about 12 min | the toy outputs in `backend/tests/reference/synthetic/`; the rest in the data root | the regenerated files against the recorded ones |
| W5 | gseapy's references, and FuSION 1's `suggest_thresholds.py` report on the EGF file | FuSION 1's `.venv` (gseapy 1.3.1) | about 2 min | `backend/tests/fixtures/gsea/` | the fixture list |
| W6 | Copy the batch corpus and the three GUI scripts, verbatim | the live `runs/` and `macOS/model_example/` | seconds | `backend/tests/fixtures/fusion1-batches/` | the file list |
| W7 | FuSION 1's merge on the synthetic merge fixtures, with three exception sets each (12, Merge then split) | C2's generators; W2 | minutes (about 150 runs) | `backend/tests/reference/synthetic/` (about 300 KB) | the summary |

W2's binary. The live binary was rebuilt on 2026-11-16 at 17:29. On 2026-11-17 every source file in the snapshot's manifest still matched the live tree: `Linux/src/GraphManagerNew.cc` had been touched but not changed. The rebuild therefore compiled the snapshot's sources, and W2's five reruns confirm that it behaves as the binary that recorded the cases. The live `gsea_select_pathways.py` changed on 2026-11-16: a generated KGML whose entries carry a `member` attribute now contributes only its `member="yes"` genes. W2 copies the snapshot's script, as section 12 says, and KEGG's KGMLs carry no such attribute. If the live sources differ from the manifest when W2 runs, Claude stops and asks.

### Code

| Id | Item | Outputs | The user reviews |
|---|---|---|---|
| C1 | The build skeleton and the backend pieces the kit needs (P3) | `backend/` as 11.C14 lays it out; `cmake/FusionVersions.cmake` (11.C4) | design note, then the diff |
| C2 | The test kit, `fusion_testkit` (12, The test kit): data root and scratch directories, seeded generators, canonical forms and `diff`, the readers of FuSION 1's files, the witness checker, the oracles (`exception_merge`, `walk_corridor`, `exhaustive_frontier`, `ScriptedOracle`) and the end-to-end helpers, with self-tests | `backend/tests/testkit/` | design note, then the diff |
| C3 | The driver, `fusion-test`, with its `data`, `e2e`, `ref` and `bench` subcommands (12.C8) | `backend/tests/tools/fusion-test/` | design note, then the diff |
| C4 | The reference runner (12, Reference-test runner); `cases.json` with the 12 synthetic and 39 KEGG cases and their path maps; `expected-differences.json`, XD-01 onward (12.C6) | `backend/tests/reference/` | design note, then the diff |
| C5 | The end-to-end case definitions and their inputs (12, End-to-end tests) | `backend/tests/e2e/cases/` | design note, then the diff |
| C6 | The benchmark harness with `bench/cases.json` and `budgets.json` (12.C13); the traceability test and its exempt list, reporting only during Phase 4 (12.C17) | `backend/tests/bench/`, `backend/tests/` | design note, then the diff |
| C7 | Experiment code: M01's capped pruning, M02's prototype of 06's search, M04's rewriter of the relaxation counters | `backend/tests/experiments/` | the work item, then the diff |

C1 to C6 run in order; builds are short and need no booking. W7 follows C2.

### Experiments and measurements (12.C15)

| Id | Question | Needs | Machine time | Informs |
|---|---|---|---|---|
| X02 | graph 83 against graph 84 (stage 2 question 1, stage 4 question 5; 02 open question 7) | W1 to W3 | about 25 min | whether S2.26's normalisation must copy every re-read effect |
| X03 | each id in one node in FuSION 1's merges (stage 2 question 4) | W1, W2 | about 20 min | the comparability of FuSION 1's merges |
| X05 | 57 or 58 ids change node (stage 4 question 3) | W3 | seconds | XD-08's prediction |
| X06 | what `:solver2_timeout` and `:timeout` do (stage 5 question 1) | the full run's dump; Z3 4.8.12's source | about 10 min | section 06's per-query timeout |
| X08 | does the seed reach the solver (stage 5 question 6; 06 open question 7) | W1, W2 | about 25 min | section 06's seed parameters; the determinism tests |
| X11 | Z3 4.8.12, 4.16.0 and 5.3.3 on 73 real dumps, in both of 11's modes, with a 600 s cap per check; beside it, which reason 5.3.3 gives for an exhausted `rlimit` (06 open question 11) | P4's builds | about 5 h on 16 workers | 11.C2's pin, which Phase 4 waits for |
| M04 | a balanced adder tree and a sorting network against the adder chains, on X11's dumps (06 open question 6) | C7's rewriter; P4's 5.3.3 | about 1.5 h on 16 workers | section 06's counter encoding |
| X07 | decide the 26 undecided queries of the 24 valid runs from their dumps, each fresh, under a 24 h cap (stage 5 question 3): pass 1 with Z3 4.8.12, pass 2 with the pinned 5.3.3 | W1; P4's 5.3.3 for pass 2 | up to 24 h per pass, on up to 26 threads | the certification of the reference frontiers |
| M01 | a capped exact pruning to simple paths on section 05's 29 configurations (05 open question 2) | W3; C7 | at most the cap per configuration | whether 2.0 prunes |
| M02 | the queries that models' own counts answer without Z3, on the T2 cases' corridors (06.C5) | W3; C7 | about 40 min | 06.C5's gain |
| M03 | vis-network 9.1.2 against 10.x on the full JUN run's 13 explanation pages (07 open question 7) | W1; a headless browser, installed with the user's OK | under 1 h | 07.C3 and 11.C11 |

### Checks that need 2.0's own code

The sections ask Phase 3 to measure or confirm some things that only 2.0's code can show: the benchmarks of sections 01, 02, 03 and 08; S2.22's split equivalence (03); the witness checker on every witness 2.0's solve produces (06); the Windows replace check of `OutputFile` (09); and section 10's platform confirmations (macOS file watching on the pinned JDK, `HostServices` with `file:` URIs on each platform, Windows argument quoting, no console window for `fusion.exe`, jpackage's job object, and bcrypt's timing in Java). When they run is P5.

### The spec's open questions left to Phase 3

| Question | Answered by |
|---|---|
| stage 1, question 3 | not run (X01) |
| stage 2, question 1 | X02 |
| stage 2, question 4 | X03 |
| stage 2, question 6 | not run (X04) |
| stage 4, question 3 | X05 |
| stage 4, question 5 | X02 |
| stage 5, question 1 | X06 |
| stage 5, question 3 | X07 |
| stage 5, question 6 | X08 |
| stage 6, question 2 | not run (X09) |
| stage 7, question 2 | the cross-platform tests (Phase 4) |
| stage 8, question 4 | not run (X10); 10.C16's test covers 2.0 |

## Order and bookings

1. P1 to P6 are polled. Nothing runs before they are decided (decision 0036).
2. **Booking 1, about 2.5 h, light on cores:** W1 to W6, then X02, X03, X05, X06 and X08.
3. **Code, no booking:** C1 to C7, each through the loop; W7 once C2 exists.
4. **Booking 2, about 7.5 h on 16 cores:** build the Z3 versions (P4), then X11 with the `rlimit` check, then M04. X11 must finish before Phase 4.
5. **Booking 3, up to 24 h on up to 26 threads:** X07, pass 1.
6. **Booking 4, up to 24 h:** X07, pass 2.
7. **Booking 5, under 2 h plus M01's capped runs:** M01, M02 and M03. M01's cap is set in its work item.
8. The findings are polled, and the user reviews Phase 3 (P6).

In all, about 60 h of booked machine time plus M01's capped runs; 48 h of it is X07's two passes at their cap.

## Choices

### P1 · Where the data root lives
- **Proposed:** `/home/ks-149-u24/Desktop/arjun/FuSION2-test-data`, beside the FuSION and FuSION2 folders. Claude sets `FUSION_TEST_DATA` on each command; adding it to a shell profile is the user's call.
- **Alternatives:**
  - `~/fusion-test-data`.
  - A folder inside FuSION2 that git ignores.
- **Why:** outside both repositories, so KEGG data is never one `git add` away from FuSION2 (S8.31). It sits on the same disk as FuSION 1, which has 376 GB free for 150 to 200 MB plus the experiments' outputs and the Z3 builds. A folder inside the repository needs a `.gitignore` entry, and one slip publishes KEGG files.
- **Decision:** as proposed (prompt log entry 262)

### P2 · How the data root is first filled
- **Proposed:** a one-off script, `backend/tests/experiments/capture/fill_data_root.sh`, copies section 12's files from FuSION 1's live tree, read only, and writes `data-manifest.json` with each file's size, SHA-256 and origin. Once C3's `fusion-test data init` exists, it re-checks the root against that manifest.
- **Alternatives:** build `fusion-test`'s `data` subcommand first (C1 to C3), then fill the root; the captures wait for it.
- **Why:** decision 0036 puts the captures first, and tests rely on the manifest, not on the tool that wrote it: every file is hash-checked before use (12, Test data). A shell copy with `sha256sum` needs no build.
- **Decision:** as proposed (prompt log entry 262)

### P3 · What Phase 3 builds of the backend itself
- **Proposed:** C1 builds:
  - the build skeleton: 11.C14's layout, 11.C6's targets, and 11.C4's pinned fetches of Catch2, pugixml, nlohmann/json and fast_float;
  - the three `core` pieces the kit uses: `core::Rng` (09.C15), `core::Process` (11.C19, cross-section item 2A.11) and `core::Failure` (09.C14);
  - the public headers of the modules of sections 01 to 09, declarations only, so that the runner and the suites compile now and go green as Phase 4 implements each module.
- **Alternatives:**
  - The skeleton and the three `core` pieces only; code that needs a module stays out of the build until Phase 4 adds its header.
  - No backend code: the kit carries its own stand-ins for the three pieces, replaced by `core` in Phase 4.
- **Why:** section 12 says Phase 3's suites "are written against the interfaces of sections 01 to 09 and go green as Phase 4 implements them", and that the kit uses `core` as every module does. Code kept out of the build goes stale unseen, and stand-ins duplicate `core` until they are swapped out. Headers add no behaviour, so Phase 4 still builds each module through its own loop; a header that a Phase 4 design note changes takes its suites with it.
- **Decision:** as proposed (prompt log entry 262)

### P4 · Where the Z3 versions for X07 and X11 come from
- **Proposed:** build Z3 4.8.12, 4.16.0 and 5.3.3 from their official source archives, each checked against a pinned SHA-256, with one compiler and one set of release flags, as 11.C3 builds Linux's Z3. They go under the data root's `tools/z3-<version>/`, not on `PATH`, with no `sudo`. X11 compares the three builds. X07's first pass uses the system 4.8.12 that FuSION 1 links, and its second pass the 5.3.3 build. Each build takes roughly 10 to 20 min, inside booking 2.
- **Alternatives:**
  - The official Linux release binaries for 4.16.0 and 5.3.3, beside the system 4.8.12: no builds, but X11 would compare three differently built binaries.
  - Z3's Python wheels.
- **Why:** X11 is a speed gate, so the three versions should differ only in Z3's own code. The 5.3.3 source build is also the Z3 that 2.0 links on Linux (11.C3).
- **Decision:** as proposed (prompt log entry 262)

### P5 · When the checks that need 2.0's own code run
- **Proposed:** Phase 3 builds their harnesses, fixtures and budgets, and each check runs when its code exists:
  - in Phase 4, as each module lands: the benchmarks of 01, 02, 03 and 08; S2.22's split equivalence against both oracles and W7's FuSION 1 merges; the witness checker on every witness 2.0's solve produces; 09's Windows replace check;
  - in Phase 5, with the frontend: section 10's platform confirmations.

  This plan keeps them listed, and each is ticked off where it runs.
- **Alternatives:**
  - Throwaway prototypes in Phase 3 that measure them now, as M01 and M02 do for their questions.
  - Section 10's platform confirmations as small probe programs on GitHub's macOS and Windows runners in Phase 3; the rest when the code exists.
- **Why:** these checks test 2.0's code, or the platform calls that code makes, and neither exists before Phases 4 and 5; a prototype would measure the prototype. Probes on hosted runners would spend macOS minutes, 10.3 times the price of Linux's (12.C1), on answers that Phase 4's and Phase 5's CI gets once the real code runs. M01 and M02 differ: their answers decide a design before Phase 4 writes it.
- **Decision:** as proposed (prompt log entry 263)

### P6 · When Phase 3 ends
- **Proposed:** when:
  - every work item is done, or moved with the user's OK;
  - X11 has run, and the user has confirmed or re-decided the Z3 pin (11.C2), which Phase 4 waits for;
  - every finding that bears on a decision has been polled;
  - the kit, driver, runner, manifest, case lists and fixtures are committed after the user's diff reviews.

  The user then reviews Phase 3 (decision 0003).
- **Alternatives:** end Phase 3 once everything but X07 is done, and run X07's two passes during Phase 4.
- **Why:** Phase 4 checks each module against these references, so they and the kit come first. X07 certifies reference frontiers that T3 compares against, and T3 cannot run before Phase 4's solve module exists, so the alternative works; it trades the phase order for up to two days of earlier progress.
- **Decision:** as proposed (prompt log entry 263)
