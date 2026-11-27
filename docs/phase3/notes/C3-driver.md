# C3 · The driver, `fusion-test`

- **Status:** decided: Claude checked the note against section 12 and the code, and the user chose every open choice as proposed (prompt log entry 341)
- **Date:** 2026-11-26
- **Plan item:** C3 (`docs/phase3/plan.md`, Code table); P2 and P5 (as amended in prompt log entry 330)
- **Specification:** section 12, "Repository layout" (`docs/architecture/12-tests.md:89-121`), "Test data" rules (179-183), "End-to-end tests" (388-431), "Files, settings, events and errors" (920-936), 12.C2 and 12.C8

C3 builds the executable, its shared plumbing, the `data` subcommand and the `e2e` engine. C4 fills `ref`, C6 fills `bench`, and C5 writes the end-to-end cases that `e2e` runs. Kit names are those of the C2 note.

## 1. What section 12 asks

- One C++ driver, `backend/tests/tools/fusion-test/`, with `data`, `e2e`, `ref` and `bench` subcommands, linking `libfusion` and the kit (12.C2, 12.C8). No Python at test time.
- Exit codes (lines 927-931): 0 no failure; 1 an unexpected difference, a failed prediction, a stale entry or a golden mismatch; 2 the harness could not run; 4 everything skipped, which CTest registers as the skip code.
- `fusion-test data init --from <FuSION tree>` copies the data root's files from a FuSION 1 tree and checks them against `data-manifest.json`; every file is hash-checked before use, with hashes cached by path, size and mtime; a missing or differing file skips its tests with the reason (lines 179-183).
- `fusion-test e2e` starts `fusion` per case in a fresh `ScratchDir`, compares the run directory with `expected/` after normalisation, and checks the exit status and stderr fragments; `--update <case>` rewrites `expected/`, shown as a diff before any commit (lines 390-431).
- Catch2 tags become CTest labels, so `ctest -L unit` selects a tier (line 118).
- Reports go outside any run directory; `--update` and `--record-seen` rewrite files for review (line 923).

## 2. What exists now

- No driver. `backend/tests/CMakeLists.txt` builds one Catch2 executable per module and registers it with `catch_discover_tests(... ADD_TAGS_AS_LABELS DISCOVERY_MODE PRE_TEST)` (line 11). Catch2 3.16.1's `catch_discover_tests` already sets `SKIP_RETURN_CODE 4` (`build/dev/_deps/catch2-src/extras/Catch.cmake:218-219`), so a Catch2 `SKIP` and `fusion-test`'s code 4 mean the same to CTest.
- Six end-to-end tests of the staged home are CMake scripts, labelled `e2e;home` (`cmake/FusionInstall.cmake:64-87`). With the 523 Catch2 cases they make the 529 tests.
- **Labels are uneven:** the 55 batch and 42 corridor test cases carry no `[unit]` tag, so `ctest -L unit` misses 97 of the 523. C6's open choice C6.4 retags them.
- `backend/tests/experiments/capture/fill_data_root.sh` filled the root under P2, and wrote the manifest: 2,063 files, 1,394 MB. The manifest's `from` paths start with `FuSION/` (2,049) or `FuSION1-reference-2026-11-10/` (14), so they are relative to the folder that holds both trees, not to a FuSION tree as section 12 words it.
- **745 files in the root (80.7 MB) are in no manifest:** W3's 117 capture files, W4's regenerated outputs (547 files under the regenerated cases' `f1/run/` and 16 console logs) and the 65 files of `reports/`. The runner and the kit's `[kegg]` tests read the captures, so as things stand they would use files nothing has hashed.
- Presets (`CMakePresets.json`): `dev` (Debug, tests on) and `release` (Release, tests off). Section 11 also plans one `ci-*` preset per platform (`11-build.md:200`); none exists, and `.github/workflows/` does not exist yet.
- The sprint's runs used `python3` scripts in Claude's scratchpad and `xargs -P` (`sprint/run_t1_t2_r4.sh`), at `nice 19` beside the user's jobs (prompt log entry 326). C4 replaces them.

## 3. Design

### Files

```
backend/tests/tools/fusion-test/
  main.cpp                 subcommand dispatch, exit codes
  options.hpp/.cpp         the command line; FUSION_TEST_DATA; the fusion to run
  data.hpp/.cpp            data verify | init | record
  e2e/case.hpp/.cpp        case.json (format in the C5 note)
  e2e/engine.hpp/.cpp      run, check, compare, update
  ref/                     C4
  bench/                   C6
  CMakeLists.txt           target fusion-test; CTest registration
backend/tests/unit/fusion-test/   test_fusion_test: options, exit codes, data on a toy root, the engine on toy cases
```

### Command line

```
fusion-test data verify  [--data DIR]                   every manifest file, through the kit's hash cache
fusion-test data init    --from DIR [--data DIR]        copy missing files, read only, then verify
fusion-test data record  --origin TEXT PATH... [--data DIR]   hash generated files into the manifest (C3.2)
fusion-test e2e   [--case ID]... [--update ID]... [--list] [--keep]
fusion-test ref   ...                                   C4
fusion-test bench ...                                   C6
common: --fusion EXE   default: the build's staged fusion, $<TARGET_FILE:fusion>, which finds its home beside it
        --data DIR     default: FUSION_TEST_DATA
```

- `--from DIR` names the folder holding `FuSION/` and the snapshot, as the manifest's `from` paths need. `init` opens sources read only, copies (never links: Windows needs a privilege for symbolic links, 12 line 157), never overwrites a file that differs, and stops on one, as `fill_data_root.sh` does. `fill_data_root.sh` stays as P2's one-off; `init` is how a second machine fills its own root (12.C1's T2 "by hand on a Mac and a Windows machine").
- `--list` prints each case with its tags and state, for C6's traceability test.
- A user who shares the workstation runs `nice -n 19 fusion-test ...`; children inherit the priority, so no option is needed.

### The `e2e` engine

For each case: a fresh `ScratchDir`; the case's batch and `inputs/` copied in; `testkit::run_fusion(exe, args, scratch, env)`, with `--run-dir run` appended unless the case's own arguments place the run (`--run-dir`, `--root`), stdin from the null device and the case's timeout (default 120 s); then, in order:
1. exit status, `run.json`'s `status`, and for a failure the batch line and the stderr fragment (12 line 411);
2. no two files of the run directory differ only in case (12, Platforms, line 805);
3. `testkit::compare_tree(run, expected, normaliser)`, where the normaliser is `testkit::fusion_normaliser(...)` plus the case's own rules.

A failing case keeps its scratch directory and prints its path (`ScratchDir::keep()`); a passing one is removed. `--update ID` writes the normalised files that the case's rules match into `expected/` and prints what changed; nothing is committed without the user's review of the diff (CLAUDE.md).

### CTest registration (open choice C3.1)

- `e2e: <case>`, one test per `e2e/cases/*/case.json` (globbed with `CONFIGURE_DEPENDS`), command `fusion-test e2e --case <id>`, labels `e2e` plus the case's tags, `SKIP_RETURN_CODE 4`.
- `ref: <id>`, one test per T1 case of `cases.json`, read at configure time with CMake's `string(JSON)`, labels `ref;t1`.
- `ref: t2`, one test running the whole tier with one report, labels `ref;t2;kegg;slow`.
- T3 is not registered: it runs only as a booked `fusion-test ref --tier t3`.
- Test presets keep plain runs short: `dev` excludes the labels `slow` and `bench`; a `t2` preset selects `kegg|t2`.

## 4. Checks it runs now (P5 as amended)

- `fusion-test data verify` on the root: P2's re-check against the manifest. It reads the manifest's 1.39 GB once (1.47 GB once C3.2 adds the generated files), then a stat per file.
- `fusion-test data record` for the 745 generated files, so that W3's captures and W4's outputs are hash-checked like the copied files (C3.2). The manifest change is a reviewed diff.
- The end-to-end cases, as C5 adds them, on Linux.

## 5. Open choices

### C3.1 · How the driver's work appears in CTest
- **Proposed:** one CTest test per end-to-end case and per T1 reference case; T2 as one test with one report, outside the `dev` test preset; T3 not in CTest.
- **Alternatives:**
  - One CTest test per subcommand and tier (`e2e`, `ref t1`, `ref t2`).
  - One CTest test per case in every tier, T2 and T3 included.
- **Why:** per-case tests run in parallel under `ctest -j`, and CI's log names the case that failed. T2 needs its findings in one `report.json` (12, Reports), and starts about 15 minutes of solves on 8 workers, so a bare `ctest` on a machine with `FUSION_TEST_DATA` set must not start it. T3 is booked with the user (12.C1; prompt log entry 209).
- **Decision:** as proposed (prompt log entry 341)

### C3.2 · The files the data root generates
- **Proposed:** `fusion-test data record` adds W3's, W4's and W5's outputs to `data-manifest.json`, each with its work item and report as `from` (for example `"W3 reports/w3/20261117T183723"`).
- **Alternatives:**
  - A second manifest for generated files.
  - Leave them unhashed, and hash only what was copied from FuSION 1.
- **Why:** section 12 says the manifest lists every file of the root and every file is hash-checked before use (lines 159, 180). The captures are the runner's main input, so they need the check most. One manifest keeps one rule.
- **Decision:** as proposed (prompt log entry 341)

### C3.3 · A Release build with tests
- **Proposed:** add section 11's `ci-linux` preset now (Release, tests on, as `ci.yml` will build it, `11-build.md:659`) and use it locally for T2, the S2.22 real layer and the benchmarks.
- **Alternatives:**
  - A local-only `release-tests` preset.
  - Turn tests on in `release`, against section 11's preset table (`11-build.md:200`).
  - Run everything in the Debug `dev` build.
- **Why:** the benchmarks must measure a Release build, and the merge layers are projected at minutes in Debug (C2 note, Cost). Section 11 already plans the preset, so adding it early invents nothing.
- **Decision:** as proposed (prompt log entry 341)

### C3.4 · Departures from section 12
- **Proposed:** accept: `data init --from` names the folder holding both FuSION trees; `data record` and `e2e --list` are added; the e2e engine passes `--run-dir run` to every case that does not place its own run.
- **Alternatives:** follow section 12's wording, and rewrite every manifest `from` to be relative to the FuSION tree.
- **Why:** 14 manifest files come from the snapshot, not the live tree, so one root has to name both.
- **Decision:** as proposed (prompt log entry 341)
- **As built (prompt log entries 361, 362 and 368):** the user accepted, as a group, four further departures:
  - one of the 745 unmanifested files is W2's report, recorded as `W2 reports/w2/20261117T183307` beside W3's 157, W4's 576 and W5's 11;
  - `ci-linux` is Release with tests on and warnings as errors, without LTO, because the build has no LTO option yet;
  - the kit's normaliser also masks the events' `ms`, a time;
  - the `ref:` CTest tests are registered by C4, with its subcommand.

  The user also decided:
  - the normaliser masks every `run.json` output's `size` and `sha256`, and `case.json`'s `wall_s` and `cpu_clock_s`;
  - a case with `processes` above 1 starts that many `fusion`s with no `--run-dir`: each names its own run directory, and each run is compared with the case's one `expected/`;
  - a case without `expected/` checks only its exit status, `run.json`'s status and its stderr fragment, as C5.1 says;
  - a case with both `processes` above 1 and `actions` is refused, and comes back as a decision when a case needs it;
  - run lists each step's outputs by path, so parallel solve cases cannot change `run.json` (commit 370aea8).

## 6. Cost

- **Size:** about 12 files and 1,300 lines (options and dispatch about 300, `data` about 350, the e2e engine about 450, CMake registration about 80, the driver's own tests about 150), plus the `ci-linux`, `dev` and `t2` preset entries.
- **Run time:** `data verify` reads 1.39 to 1.47 GB the first time (core's SHA-256 speed is unmeasured; expect under a minute in Release) and takes milliseconds afterwards. The engine adds a process start per end-to-end case, well inside T1's 5 min (C5 note, Cost).
