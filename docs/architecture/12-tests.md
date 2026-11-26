# 12 · Test architecture

## Scope

- **Modules:** none of its own (skeleton Choice C1). This section designs the test tree, the test kit and the harnesses that every module's tests use:
  - library-level tests per module, in Catch2 (B1, G1);
  - end-to-end tests that run `fusion` on small fixtures;
  - the reference-test runner, which runs FuSION 1 batch files through 2.0 and compares the results with FuSION 1's outputs against a recorded list of expected differences ([decision 0023](../decisions/0023-results-may-change-where-improved.md));
  - determinism and cross-platform tests (G3);
  - benchmarks on real-size inputs;
  - the Phase 3 experiments that the spec left open.
- **Work by phase** ([decision 0003](../decisions/0003-phase-plan.md)):
  - Phase 3 builds the test kit, the data root and its reference store, the runner, the expected-differences list and the end-to-end fixtures, and runs the experiments. Its suites are written against the interfaces of sections 01 to 09 and go green as Phase 4 implements them.
  - Phase 4 adds each module's unit tests with its code.
  - Phase 5 tests the frontend screens and the graph display ([decision 0027](../decisions/0027-graph-display-in-phase-5.md)). This section fixes the contracts that frontend tests share with the backend, and how they run: the runner against `fake-fusion`, the four modules other than `ui` (accounts, sessions, pipeline and runner) without a display, and the JavaFX screens under Xvfb on Linux CI (section 10's desktop design; prompt log entry 209).
- **Left to other sections:** CI service configuration and build presets (11); the formats being compared (07, 09); each module's own invariants (02 to 09); the frontend runner (10).

## Requirements carried in

Decisions:
- [0003](../decisions/0003-phase-plan.md): Phase 3 saves FuSION 1 outputs from reference runs (the toy examples, EGF-HeLa, EGFR → CXCL8 b7) so that 2.0 can be checked against them. The frontend starts only after the backend has been checked against FuSION 1.
- [0017](../decisions/0017-batch-file-compatibility.md) as amended by [0023](../decisions/0023-results-may-change-where-improved.md): FuSION 1 batch files still run, but results may change wherever 2.0 improves. Reference tests compare with FuSION 1 only where no improvement applies, and record each expected difference.
- [0018](../decisions/0018-batch-interface.md): the batch language stays, with named arguments, FuSION 1's positional lines and sweeps; the end-to-end cases use all three.
- [0019](../decisions/0019-partial-runs.md): stop after a step, start from a saved step, resume a solve, solve part of the frontier. All four are tested, and the runner uses the last three.
- [0020](../decisions/0020-plug-and-play-inputs.md): network and per-gene inputs are plug and play, so fixtures cover each input form and use ids that are not KEGG ids.
- [0021](../decisions/0021-required-and-optional-inputs.md): inputs are checked before the first step; a named but unreadable optional input stops the run; the frontend validates the same cases. Amended by 01.C12 (prompt log entry 188): an input that the batch itself makes is checked again just before its step.
- [0024](../decisions/0024-frozen-fusion1-snapshot.md): the FuSION 1 reference is the snapshot at `/home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10` (352 files, `MANIFEST.sha256`). Run data and KGMLs come from the live tree.
- [0027](../decisions/0027-graph-display-in-phase-5.md): results must be display-ready; display tests are Phase 5's.

Skeleton choices:
- **A1:** Stop is a platform-neutral stop request, and a stopped solve resumes. Amended by 06.C19 (prompt log entry 174): a killed or crashed run's journal is used on resume too, so a kill loses only the check in progress.
- **A2:** progress events go to `events.jsonl`, with dotted names (`run.started`, `step.finished`, `solve.check.started`; cross-section item 1.3, prompt log entry 190).
- **B1:** tests call `libfusion` directly and also run `fusion` end to end.
- **C1:** ten backend modules; `core` owns seeded random numbers and stable orderings. Amended by 11.C19 (prompt log entry 204): `core` also holds build information, the install directory, the platform layer (processes, paths, CPU time) and the dependency registry.
- **D1:** steps pass typed values in memory, and every step's output saves and loads.
- **E1:** `run.json` records versions, every setting with its source, and input hashes; results are JSON and tables TSV; FuSION 1's text formats are written only on request (one setting, `legacy_prefix`; cross-section item 1.21, prompt log entry 179).
- **F1:** GSEA is reimplemented in C++; gnuplot draws the Pareto image. F1 said its numbers agree with gseapy "statistically but not byte for byte". 08.C1 amended that (prompt log entry 159): the C++ GSEA reproduces gseapy 1.3.1 bit for bit, and Phase 3 tests exact equality.
- **G1:** C++20, CMake, Catch2, pinned dependency versions recorded in `run.json`. Amended by 11.C22 (prompt log entry 200): GCC 13 and Clang 17 at the oldest.
- **G2:** the frontend is Java 21 and Spring Boot, built with Gradle, so its tests are JUnit tests run by Gradle. Amended by 11.C21 (prompt log entry 200): Spring Boot 4.1. 10.C1 makes it a JavaFX desktop application (entry 192), so its screens are tested under Xvfb on Linux CI.
- **G3:** Linux, macOS and Windows, all native.
- **H1:** this section answers "the hooks Phase 3 needs: library-level tests, deterministic outputs, and a reference-test runner that compares 2.0 with FuSION 1 runs against a recorded list of expected differences".

Spec items:
- **Stage 1:** S1.01 to S1.04 (command line, missing file, end of file without `exit`); S1.14 (a failing `exec` is a failure); S1.26 (outputs in the run's directory); S1.27 (console numbers exact); S1.28 (the batch stops at the first failure, with a non-zero exit); S1.29 (result-changing flags become recorded settings; one debug option).
- **Stage 2:** S2.13 (the FSC joins the exception set by itself); S2.17 (order-independent union); S2.22 ("Phase 3 must show that the split gives the same graph as merging with the exceptions"); stage 2 open question 4 (each id in exactly one node, as a property test); S2.26 (in-memory normalisation instead of the re-read, open question 1); S2.27 (the legacy reader, so the published runs compare like for like); S2.08, S2.09, S2.10, S2.11 and S2.21 (fixes that change merged graphs).
- **Stage 3:** S3.06 (values and thresholds as doubles; rounding-edge differences allowed); S3.07; S3.14 (one LFC parser).
- **Stage 4:** S4.11 with S5.13 (the corridor trims self-loops and edges into a source or out of a target); S4.14 (stable keys); S4.15 (ids resolved once per run); S4.18 and S7.03 (an empty corridor or an empty selection is a reported result); S4.20 (the solve leaves the corridor unchanged).
- **Stage 5:** S5.13 (self-loops leave the count); S5.17 (FSC semantics); S5.19 (certification flags); S5.20 (unknown answers cached, also across a resume); S5.22 (seed recorded; reruns reproduce on one machine and Z3 build); S5.23 (dumps only under debug); S5.24 (a structured search log, one record per check, with wall and CPU time); S5.26 (the search's models are kept); S5.29 (no state between solves).
- **Stage 6:** S6.14 (one polarity rule); S6.21 (an explicit "no PO point" result).
- **Stage 7:** S7.05 (ties ordered by gene id); S7.11 (the seed, default 42, and the permutation count, default 1000, are run settings recorded in the output); S7.12 (FDR pooled over the candidates).
- **Stage 8:** S8.21 (a run directory is never reused); S8.28 and S8.30 (runner and validation); S8.31 (no KGMLs in the repository).

Open questions the spec left to Phase 3: stage 1 question 3; stage 2 questions 1, 4 and 6; stage 4 questions 3 and 5; stage 5 questions 1, 3 and 6; stage 6 question 2; stage 7 question 2 (whole results on a Mac); stage 8 question 4.

Answers of the Phase 2 polls that the tests follow (prompt log entries 158 to 210), beyond those named above:
- **Batch (01):** six fixed-name `exec mv` lines are recognised and written natively, amending S1.14 (01.C11, entry 179); window-plot `exec` lines are skipped when headless, amending S1.14 and S6.25 (entry 190); after a reported result every later line of that run or sweep point is skipped, widening S4.18 (01.C15, entry 167); a failing sweep point stops only itself, and the sweep exits 1 (01.C5, entry 188); `fusion --check --json` prints 01's diagnostics with 09's fields (cross-section item 1.9, entries 158 and 186).
- **Network and merge (02, 03):** the stable edge key is source, target, type and subtypes (02.C11, settling S4.14, entry 159); the legacy reader reads every unmarked FuSION 1-written file and reproduces graph 2, merging exact duplicate edges at read time, widening S2.27 and moving S4.13's merge (02.C5, entry 159); a single KGML read is a one-file merge, amending S2.07 (03.C5, entry 166).
- **Expression (04):** one decimal grammar for values, thresholds and real arguments, refining S3.02 (04.C2, entry 167); repeated ids keep the largest magnitude in `max_abs` runs, amending S3.08 (04.C3, entry 167); no gene past the thresholds is a third reported result, `observations.none` (04.C5 with cross-section item 1.8, entry 167); value files may use a lone CR and a UTF-8 byte-order mark, extending S3.03 (04.C10, entry 169).
- **Corridor (05):** self-loops and edges into a source or out of a target are trimmed during the search, as the reading of S4.11 (05.C2, entry 170); the walk rule stands, and a capped exact pruning to simple paths is measured in Phase 3 (05 open question 2, entry 170); excluding or avoiding a gene also removes or forbids every complex that contains it, extending S4.06 and S5.17 (entry 170), for every gene on an avoid list as well as a named out case's FSC (06 open question 10, entry 229); `fb_rch` writes `corridor.json` and `observations_match.tsv` (05.C6, 05.C9, entries 171 and 172).
- **Solve (06):** the FSC is a set, with cases `in`, `out` and `free` in arguments, file names and JSON, extending S5.17 (06.C10, cross-section items 1.6 and 4B.29, entry 175); `points=` asks single points (06.C9, entry 175); TIMEOUT answers are re-asked on resume under a longer timeout, amending S5.20 (06.C8, entry 174); a killed run's journal is used on resume, amending A1 (06.C19, entry 174); the frontier is the Pareto-minimal witness counts, amending S5.25 (06.C20, entry 174); an edge bound above the counted edges is clamped with a warning (06.C14, entry 176); source and target are present and active in the named form (06.C16, entry 176); `rlimit` gives a deterministic per-check budget (cross-section item 2B.8, entry 176); Z3 5.3.3 is pinned, gated on experiment X11 (11.C2, cross-section item 3A.26, entry 173); a resumed case starts cold, a known cost (06 open question 5, entry 210).
- **Results (07):** 06's case names, with 07's labels for display only (1.6, entry 175); vis-network 10.x compiled into `libfusion`, with 9.1.2 as the fallback if Phase 3's layout check sees a change (07.C3, 11.C11, cross-section item 3A.22, entry 177); FuSION 1's text files only under `legacy_prefix` (1.21, entry 179).
- **Enrichment (08):** GSEA reproduces gseapy 1.3.1 bit for bit, amending F1 (08.C1, entry 159), on one thread (08.C6, entry 182); an all-zero draw gets the step 1/m and a warning, and an undefined FWER is 1.0 (08.C7, entry 182); equal |NES| are ordered by pathway id (08.C9, entry 182).
- **Runs (09):** one directory per step, one `results.json` per solve, and a run page written once at the run's end (09.C2, 07.C1, cross-section items 3A.15, 1.4 and 4A.12, entry 177); failures are `core::Failure` exceptions, reported results `core::Outcome<T>`, stops `core::Stopped` (09.C14, entry 186); random numbers outside GSEA come from `core::Rng` (09.C15, cross-section item 3A.17, entry 186); results are byte-identical on all three platforms (09.C16, cross-section items 3A.18 and 1.18, entry 186); dotted event names (1.3, entry 190).
- **Frontend (10):** runs outlive the frontend (10.C16, entry 185); the desktop design tests its four modules other than `ui` without a display (10.C11, entry 198).
- **Build (11):** GitHub Actions run Linux on every push, and macOS, Windows, the identity test, the oldest-GCC build and the reproducibility check on pull requests into `main`, weekly and on demand, with actions pinned by commit SHA (11.C15, entry 203); `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT` are recorded in `run.json`, and `exec` lines get the bundled tools directory appended to `PATH` (11.C20, entry 204); open questions 5, 6, 7 and 9 and the `set term png` check are Phase 3 and 4 checks (entry 205).

## Design

### Evidence used

- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/` (FuSION 1's corridor 115 nodes, 370 edges, 2.0's 108 and 358 under XD-17 and XD-18; pathz3 615 s). "The full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/` (FuSION 1's corridor 592 nodes, 2138 edges, 2.0's 547 and 2117; pathz3 61,292 s).
- "The 24 valid runs" are the run directories under `runs/` whose solution files were written on or after 2026-07-16, by builds whose results match the snapshot's (see "Reference cases"). Their pathz3 calls took 1,241,580 s (344.9 h) in all, from `run/timing.log` and the `[pathz3 timing]` console lines.
- "The batch corpus" is the 107 `*.batch` files under `runs/` and `macOS/model_example/` at 18:19 on 2026-11-12: 1,300,313 bytes, 24,787 lines, all with LF endings. Three GUI scripts in `Linux/` come with it. The set grows while FuSION 1 is in use: by 18:29 the tree held 116 batch files (7 new `*_string125` runs, and 2 in `auxilliary/comparison_results/`), the count sections 07 and 09 use; section 01's corpus adds the three GUI scripts, for 119. Phase 3 freezes the copy it takes, with its own manifest, so the tests do not move with the tree.
- "The snapshot binary" is the live `Linux/fusion`: 19,525,272 bytes, SHA-256 `22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50`, built 2026-11-09 19:20. Of the snapshot's 352 manifest entries, all 351 source files have the same SHA-256 in the live tree; the 352nd, `README.txt`, exists only in the snapshot. It links Z3 4.8.12 (`libz3-4 4.8.12-3.1build1`).
- Counts come from Python scripts over these files. Nothing was built or run. The workstation is an Intel Xeon Gold 6140 with 18 cores, 36 threads and 124 GB. While this section was drafted, up to 18 FuSION 1 jobs ran on it at once (`runs/egfr_b10_gsea_fdr044_candidates/status.log`): `r17` to `r26` from 18:20, the seven `*_string125` reruns from 18:26, and `r6`'s `resolve_wo_n2_e8`. Jobs `r9` to `r16` had been stopped at 18:06 (exit 143).

### Test tiers

| Tier | What runs | Data | Where | When | Budget |
|---|---|---|---|---|---|
| T0 · unit | library and property tests per module; test-kit self-tests; batch-corpus parse | repository | every local build; CI on Linux x86-64 for every push, and on macOS arm64 and x86-64 and Windows x86-64 on pull requests into `main`, weekly and on demand (11.C15, prompt log entry 203) | every build and push | 1 min per platform |
| T1 · end to end | `fusion` on synthetic fixtures; synthetic reference cases (toy networks, synthetic merges, synthetic GSEA) against recorded FuSION 1 and gseapy outputs | repository | as T0 | as T0 | 5 min per platform |
| T2 · reference, fast | the 23 KEGG reference cases whose FuSION 1 solve took at most 738 s; the non-solve stages of all 39 KEGG cases; repeat-run determinism | data root | workstation; by hand on a Mac and a Windows machine holding a copy of the data root | before committing a change to `merge`, `corridor`, `solve` or `enrichment` | 15 min |
| T3 · reference, full | the 16 slow KEGG cases, by frontier verification: against FuSION 1's frontiers until the golden search, then against 2.0's reviewed frontiers (12.C16) | data root | workstation, scheduled with the user on a quiet machine (prompt log entry 209) | after a change to the encoding, the corridor or the merge; before a release, together with the comparison with FuSION 1 | 24 h |
| B · benchmarks | real-size steps; synthetic-scale cases in CI | data root, repository | workstation; CI smoke run | on demand | 30 min |

Once, at the end of Phase 4, a golden search runs 2.0's full Pareto search on all 39 KEGG cases and records 2.0's own frontiers (up to about two days, bounded by the slowest case). Section 11's CI already runs every test under AddressSanitizer and UndefinedBehaviorSanitizer on each push (a Linux Clang debug build), and its pull-request and weekly workflow adds the cross-platform identity test, the oldest-GCC build and the reproducibility check (11.C15). This section adds a weekly ThreadSanitizer run of the tests of the stop watcher and of FSC cases solved in parallel (06.C4). GSEA runs on one thread (08.C6), so it needs none. The frontend's screen tests run under Xvfb on Linux CI.

### Repository layout

The backend's tests live in `backend/tests/` and the frontend's in `frontend/src/test/`, as section 11's layout places them:

```
backend/tests/
  CMakeLists.txt                  every test registered with CTest, labelled by tier
  testkit/                        fusion_testkit: test-only static library
  unit/<module>/                  one Catch2 executable per module: test_core … test_run
  e2e/cases/<case>/               case.json, batch file, inputs/, expected/
  reference/
    cases.json                    reference cases: batch, tier, path map, FuSION 1 artefacts
    expected-differences.json     the recorded list of decision 0023
    data-manifest.json            every data-root file: path, size, SHA-256
    synthetic/<case>/             synthetic cases with their FuSION 1 outputs
  bench/cases.json                benchmark cases
  bench/budgets.json              time and memory budgets from the module sections
  fixtures/
    synthetic/                    hand-written KGML-format pathways, value files, gene sets
    fusion1-batches/              the batch corpus and the three GUI scripts, verbatim
    gsea/                         synthetic rankings and sets, with gseapy 1.3.1 results
  experiments/                    X02, X03, X05 to X08, X11 and M01 to M04: batch files, scripts, notes
  schemas/                        JSON Schemas: run.json, events.jsonl, results.json, and the output of
                                  fusion --check --json and fusion --commands
  traceability-exempt.txt         spec items the traceability test may skip, each with a reason
  tools/fusion-test/              driver: data, e2e, ref and bench subcommands
  tools/fake-fusion/              native stand-in for fusion, for section 10's process tests
```

- Each Catch2 test case carries tags: its tier (`[unit]`, `[property]`, `[e2e]`, `[kegg]`, `[ref]`, `[slow]`, `[bench]`), its module, and the spec items it covers (`[S2.22]`). CTest receives the tags as labels, so `ctest -L unit` selects a tier.
- A traceability test lists every test's tags and the 209 item ids in `docs/spec/`, and prints the items that no test covers. When it starts to fail is 12.C17.
- Section 11's `.gitattributes` (`* text=auto eol=lf`) keeps LF fixtures and goldens LF in a Windows checkout (G3). Deliberate CRLF fixtures are marked `-text`, so they stay CRLF. One is FuSION 1's own: `example/with_node_included.tsv` has CRLF line endings and space separators.
- `schemas/` is the one copy of the JSON Schemas, kept here and written by each format's owner: 09 for `run.json` and `events.jsonl`, 07 for `results.json`, 01 for the output of `--commands` and `--check --json` (cross-section item 2A.10, prompt log entry 158). The backend's tests read them in place, and the frontend's Gradle tests read them from `../backend/tests/schemas/`.

### Test data and the KEGG licence

S8.31 keeps KGMLs out of the repository. Tests therefore use two kinds of data.

**In the repository (synthetic, or free of KEGG content):**
- hand-written KGML-format pathways with made-up ids (`t1`, `t2`, …) and neutral labels (`G1`, …);
- FuSION 1's three toy networks, relabelled. Each has 22 entries and 25 relations:
  - `example/report_example.xml` (17 `activation`, 8 `inhibition`), used by 3 of the 12 toy runs;
  - `example/report_example_with_pathways.xml` (18 and 7, with `pathways` attributes), used by 7;
  - `example/with_node_included.xml` (17 and 8, with `pathways` attributes), used by 2.

  Their structure is hand-made, but their `<graphics>` names are KEGG aliases (`TNFRSF11A, CD265, FEO, …`), so the copies replace them. The value files come too: `report_example.tsv`, `report_example.csv` (tab-separated despite its name) and `with_node_included.tsv` (spaces, CRLF). The last keeps the typo `has16`, a test of an id that matches no node;
- FuSION 1's outputs on those synthetic inputs, regenerated in Phase 3;
- the batch corpus and the GUI scripts (no KEGG content);
- synthetic GSEA rankings and gene sets, with gseapy's results.

The product ships rebuilt maps: a symbol map from NCBI Gene and Reactome's pathway names, with their sources in `data/SOURCES.md`. KEGG pathway names come at run time from each KGML's root title (prompt log entry 205). It also ships `data/kegg_pathways.txt`, the 167 candidate pathway ids, which resolve against the user's own KGML folder (entry 204). Synthetic tests use the shipped files. A `[kegg]` test checks that the shipped 167 ids all resolve against the data root's `kegg/` folders. The KEGG reference cases use the maps in their own `inputs/`, as their batches name them.

**Outside the repository: the data root**, found through the environment variable `FUSION_TEST_DATA`:

```
kegg/hsa_pathways_with_exp_score/      163 KGMLs, 6.7 MB    (FuSION/auxilliary/…)
kegg/updated_pathways/                 hsa04216, hsa05224, hsa05225, hsa05226, 0.18 MB
legacy/merged_kegg_master_graph_with_exp_score.xml           1,945,801 bytes
model_example/                         dummy_master_graph.xml (125 entries, 479 relations; KGML data
                                       under S8.31, entry 206), inputs
expression/egf_hela_t120_lfc.tsv       41,571 bytes, 3054 lines
constraints/hsa_stringdb_coexpression_links_over400.csv
fusion1/Linux/fusion                   the snapshot binary, with supporting_files/
reference/<case>/inputs/               the case's own inputs/ folder, as its batch names it
reference/<case>/f1/                   FuSION 1 outputs and capture artefacts per case
reference/dumps/                       the 26 SMT dumps that X07 re-checks, 46.3 MB
```

Each case's `inputs/` is a copy of its FuSION 1 run's `inputs/` folder, whatever the files' origin. For `r7` and `r8` that includes the curated pathway `hsa99901_curated_erk_targets.xml` (1897 bytes, the same SHA-256 in both runs) and `egf_hela_t120_lfc_plus_phlda1.tsv`. The runner copies a case's `inputs/` into the case's scratch directory before it runs; copies rather than links, because Windows needs a privilege for symbolic links.

The whole root comes to an estimated 150 to 200 MB. `backend/tests/reference/data-manifest.json` lists every file:

```json
{
  "schema": 1,
  "root_env": "FUSION_TEST_DATA",
  "files": [
    {"path": "kegg/hsa_pathways_with_exp_score/hsa04010.xml", "size": 69314,
     "sha256": "ce0b9f2863d1749510a1e97ab804fe50b0380391c3d1f93cbcba1bd44e53a6d3",
     "from": "FuSION/auxilliary/hsa_pathways_with_exp_score/hsa04010.xml"},
    {"path": "legacy/merged_kegg_master_graph_with_exp_score.xml", "size": 1945801,
     "sha256": "9cb47389e7a42bdaa0dc329fd0d3b7ee16a049a309998479c8cdcdff4325fa24",
     "from": "FuSION/runs/real_experiments/merged_kegg_master_graph_with_exp_score.xml"},
    {"path": "expression/egf_hela_t120_lfc.tsv", "size": 41571,
     "sha256": "777689a645f48d766fb2f821bb93ae31d0a68d3ec6ab7767970163389f33432d",
     "from": "FuSION/runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/inputs/egf_hela_t120_lfc.tsv"}
  ]
}
```

Rules:
- Every file is hash-checked before use. Hashes are cached by path, size and modification time, so a repeat check costs a `stat` per file.
- If the root is missing or a file differs, the test is skipped with that reason, never passed. Tests that need the root carry `[kegg]`.
- `fusion-test data init --from <FuSION tree>` copies the files from a FuSION 1 tree and checks them against the manifest.
- The KGMLs cannot be fetched from KEGG instead. FuSION 1's files carry an `experiment_score` attribute stamped by `auxilliary/add_experiment_score.py` (S2.06), which KEGG's files do not have.

### FuSION 1 as the reference

- **Sources** are the snapshot (decision 0024). **Binary:** Phase 3 copies the snapshot binary into the data root, next to a copy of the snapshot's `Linux/supporting_files/`, because `gsea` finds its script beside the binary (S7.02). It then confirms two things:
  - the 351 source files of the manifest still match the live tree (true on 2026-11-12; `README.txt`, the 352nd entry, exists only in the snapshot);
  - reruns of the five fastest recorded cases reproduce their `.dat`, solutions and relaxations files byte for byte, as the fdr025 rerun did (S5.22).
- **Running FuSION 1:** each batch runs in its own scratch directory, because FuSION 1 writes fixed-name files to the launch directory (S1.26). `PATH` starts with FuSION 1's `.venv/bin`, which holds gseapy 1.3.1 (S7.02).
- **Rebuilding FuSION 1** is not needed: X04, the one experiment that would, is not run (12.C15, prompt log entry 208). If a later question needs it, it uses a writable copy and a serial `make`. `Linux/Makefile:56` lists `create_build`, which runs `rm -rf build` (`:64-65`), beside the object targets, so `make -j` can delete objects mid-build.

### The test kit

`fusion_testkit` is a static library that only tests link; neither `libfusion` nor `fusion` contains it. Two principles:
- **Independent oracles.** The witness checker, the merge oracle and the readers of FuSION 1's files share no code with the modules they check. The merge oracle reads KGML with its own small pugixml reader. The checker implements the polarity rule from the spec's table (S5.13), not by calling the solve's rule (S6.14).
- **Own data model.** Canonical forms, cases and assignments are plain structs, so the kit works in Phase 3 before the modules exist. Adapters from module types (`network::Network`, `corridor::Corridor`, `solve::Results`) are added in Phase 4 as each module lands. Type names follow the owning sections.

The kit uses `core` as every module does: `core::Rng` for random numbers (09.C15), 11's `core::Process` to start `fusion` (cross-section item 2A.11), and `core::Failure` for its own errors (09.C14).

```cpp
// backend/tests/testkit/include/fusion/testkit/*.h  (test-only)
namespace fusion::testkit {

// --- data and scratch space -------------------------------------------------
struct DataRoot {
  std::filesystem::path root;
  static std::optional<DataRoot> from_env();                  // FUSION_TEST_DATA
  std::filesystem::path at(std::string_view relative) const;
};
struct ManifestEntry { std::string path; std::uint64_t size; std::string sha256; };
struct DataCheck { std::vector<std::string> missing, mismatched; bool ok() const; };
DataCheck verify(const DataRoot&, std::span<const ManifestEntry>);

class ScratchDir {                                            // short unique path, kept on failure
 public:
  explicit ScratchDir(std::string_view test_name);
  const std::filesystem::path& path() const;
  void keep();
  ~ScratchDir();                                              // retries removal on Windows sharing errors
};

// --- seeded generators (core::Rng: mt19937_64 with core's own distributions) -
struct PathwayShape {
  int entries = 30, relations = 40, max_box = 6, groups = 3;
  double shared_id = 0.2, undirected = 0.15, indirect = 0.05, ecrel = 0.03, duplicate = 0.05;
};
struct SyntheticPathway { std::string id; std::string kgml; };   // KGML-format text, made-up ids
std::vector<SyntheticPathway> pathways(core::Rng&, int count, const PathwayShape&);
std::string value_file(core::Rng&, std::span<const std::string> ids, double tie_share, bool crlf);
struct TinyCase;                                             // corridor, observations, case spec
TinyCase tiny_case(core::Rng&, int nodes, int edges, int bound);

// --- canonical forms ----------------------------------------------------------
struct CanonNode {
  std::string key;               // sorted member ids joined by '+'; a complex: "{" component keys "}"
  std::vector<std::string> members;                          // sorted member ids
  std::string type;
};
struct CanonEdgeKey {
  std::string src, tgt;                                      // CanonNode keys, never indices
  std::string type;
  std::vector<std::string> subtypes;                         // sorted, without dummy_u_to_d
  auto operator<=>(const CanonEdgeKey&) const = default;
};
struct CanonEdge {
  CanonEdgeKey key;
  bool dummy_tag;
  std::vector<std::string> pathways;                         // sorted
  double score;
};
struct CanonGraph {
  std::vector<CanonNode> nodes;                              // sorted by key
  std::vector<CanonEdge> edges;                              // sorted by key
  std::string text() const;                                  // one LF-terminated line per element
  std::string sha256() const;
};
struct CanonCorridor { CanonGraph graph; std::vector<std::string> sources, targets; int bound; };
struct GraphDiff {
  std::vector<std::string> only_a_nodes, only_b_nodes;       // node keys
  std::vector<CanonEdgeKey> only_a_edges, only_b_edges;
  std::vector<std::pair<CanonEdge, CanonEdge>> changed_edges;  // same key, other fields differ
  bool empty() const;
};
GraphDiff diff(const CanonGraph& a, const CanonGraph& b);    // sorted merge on keys

struct Point { int n, e; auto operator<=>(const Point&) const = default; };
enum class CheckOutcome { sat, unsat, unknown, not_checked };
struct Frontier { std::vector<Point> points; std::vector<bool> certified; };

// --- readers of FuSION 1 files -------------------------------------------------
namespace f1 {
struct NodeState { std::string rep; bool present, active; };
struct EdgeKey { std::string src_rep, tgt_rep; std::vector<std::string> subtypes; };
struct Witness {
  std::vector<NodeState> present_nodes;
  std::vector<EdgeKey> selected_edges, relaxed_edges;
  std::vector<std::string> relaxed_nodes;
};
struct PointRecord { Point point; std::optional<Witness> witness; std::string reason; };
struct Check { Point point; std::string step; CheckOutcome answer; double cpu_s; };
struct ConsoleFacts {
  std::vector<std::pair<int, int>> up_down, corridor_sizes;
  std::vector<int> coexpression_clauses, frozen_clauses;
};
std::vector<Point> read_dat(const std::filesystem::path&);
std::vector<PointRecord> read_solutions(const std::filesystem::path& solutions,
                                        const std::filesystem::path& relaxations);
std::vector<Check> read_limits(const std::filesystem::path&);
CanonGraph read_written_graph(const std::filesystem::path&);   // wgx output, tool-written rules
std::vector<std::string> read_ids(const std::filesystem::path&);  // up_reg, down_reg, shortlists
struct GseaRow;
std::vector<GseaRow> read_gsea_table(const std::filesystem::path&);
ConsoleFacts read_console(const std::filesystem::path&);
}  // namespace f1

// --- independent witness checker ------------------------------------------------
enum class Semantics { fusion1, fusion2 };
struct EdgeSpec {                                                 // S5.06: subtypes compared as a set
  std::string src, tgt;
  std::vector<std::string> subtypes;
};
struct CaseSpec {
  std::vector<std::string> up, down, confirmed_up, confirmed_down;          // S5.07
  std::vector<std::string> essential, avoid, active, inactive, nonrelaxed;  // S5.05
  std::vector<EdgeSpec> essential_edges, avoid_edges, nonrelaxed_edges;     // S5.06
  std::vector<std::pair<std::string, std::string>> coexpressed;   // pairs above threshold (S5.15)
  std::optional<double> frozen_threshold;                         // S5.16
  std::vector<std::string> fsc;                                   // fusion2: a set (06.C10)
  enum class Case { in, out, free } solve_case = Case::free;      // fusion2: 06.C10's case names
};
struct Assignment {                                               // keys, so it moves between corridors
  std::vector<std::pair<std::string, bool>> present;              // node key, active
  std::vector<CanonEdgeKey> selected;
};
struct Verdict { bool valid; Point cost; std::vector<std::string> violations; };
Verdict check(const CanonCorridor&, const CaseSpec&, const Assignment&, Semantics);
Assignment from_f1(const CanonCorridor&, const f1::Witness&);

// --- oracles --------------------------------------------------------------------
struct MergeOracleSettings {
  int threshold = 1;                                                 // S2.07
  bool keep_indirect = false;                                        // S2.11 default: discard
  bool keep_ecrel = true;                                            // S2.31 default: keep
};
CanonGraph exception_merge(std::span<const std::filesystem::path> kgmls,
                           const std::set<std::string>& exceptions,
                           const MergeOracleSettings&);   // S2.07 to S2.21, shares no code with merge
CanonCorridor walk_corridor(const CanonGraph&, std::span<const std::string> sources,
                            std::span<const std::string> targets, int bound);
Frontier exhaustive_frontier(const CanonCorridor&, const CaseSpec&, Semantics, Point box_hi);

class ScriptedOracle final : public solve::Oracle {                 // section 06's search interface
 public:
  ScriptedOracle(std::vector<solve::Point> staircase,               // minimal SAT points
                 std::map<solve::Point, solve::Outcome> overrides); // e.g. timeout, undefined
  solve::Answer ask(solve::Point, solve::Phase) override;            // monotone, or the override
  int calls(solve::Point) const;                                     // S5.20: never above 1
};

// --- end to end -------------------------------------------------------------------
enum class Compare {
  exact,                       // bytes, after the JSON and path masks
  tsv_masked,                  // bytes, with the named columns blanked
  results,                     // frontier and flags exact; witnesses must pass check() at their point
};
struct FileRule { std::string glob; Compare mode; std::vector<std::string> masked_columns; };
struct Normaliser {
  std::vector<std::string> json_pointers;                    // volatile fields (section 09)
  std::vector<std::pair<std::filesystem::path, std::string>> path_tokens;  // <RUN>, <CASE>, <DATA>
  std::vector<FileRule> rules;                               // one per compared output
};
struct RunResult { int exit_code; std::string out, err; std::filesystem::path run_dir; };
RunResult run_fusion(const std::filesystem::path& exe, std::span<const std::string> args,
                     const ScratchDir&);                     // through 11's core::Process
struct TreeDiff { std::vector<std::string> missing, unexpected, differing; bool empty() const; };
TreeDiff compare_tree(const std::filesystem::path& actual, const std::filesystem::path& expected,
                      const Normaliser&);
}  // namespace fusion::testkit
```

| Operation | Time | Memory |
|---|---|---|
| canonicalise a graph | O(M log M + V log V + E log E), M = member ids | O(V + E + M) |
| `diff` | O((V + E) · k): one sorted merge over node keys and one over edge keys, k the key length; edges name nodes by key, so no index remapping is needed when the node sets differ | O(V + E) |
| `check` one witness | O(V + E + P): longest selected path into each target, DE-on-path dynamic programme, relaxation recount, P coexpression pairs | O(V + E) |
| `walk_corridor` | O(V · d^b) explicit walks; used for V ≤ 12, b ≤ 5 | O(V · b) |
| `exhaustive_frontier` | O(3^V · 2^E · (V + E)): absent, active or inactive per node, every edge subset; V ≤ 6, E ≤ 8 is 186,624 assignments (per run), V ≤ 7, E ≤ 10 is 2.2 million (weekly) | O(V + E) |
| `exception_merge` | O(Σ entries + fan-out pairs) per call, union-find over boxes | O(V + E) |

On the largest real graph, the old master's 5307 entries and 7517 relations, canonicalisation and diff take milliseconds; a witness check on the full run's 592-node corridor takes microseconds.

### Library-level tests per module

| Module | Tested | Oracle or property |
|---|---|---|
| `core` | the settings registry: every setting has a name, type and default, and appears in `run.json` with its source (E1); stable orderings; `core::Failure`, `core::Outcome<T>` and `core::Stopped` (09.C14); 11's process layer and the gnuplot runner (11.C19, cross-section item 2B.13) | `core::Rng` (09.C15): the 10,000th output of a default-constructed `std::mt19937_64` is 9981545732273789042, as the standard requires, and golden first values of `Rng::stream(42, 0)`, of a bounded draw and of a shuffle are equal on all three platforms. Its distributions are `core`'s own code, because `<random>`'s differ between libstdc++, libc++ and the MSVC library. GSEA's generator (08.C1, cross-section item 1.19): `Xoshiro256pp(0)` starts 5987356902031041503, 7051070477665621255, 6633766593972829180, rand 0.9.2's own test values |
| `batch` | tokens, quoting, comments, `$` expansion, `let` (S1.05 to S1.08); named and positional forms give the same command (0018); sweeps; the failure policy (S1.28); end of file acts as `exit` (S1.04); the six fixed-name moves written natively (01.C11); window-plot lines skipped when headless (entry 190); inputs the batch makes rechecked before their step (01.C12) | **batch corpus:** every batch file of the frozen copy (107 at this census) and `Linux/script.txt` pass syntax and argument validation. `Linux/output_script.txt:31-32` sets both thresholds to 0.0 and must fail S3.07's check. `Linux/advtempscript.txt` holds `%__12__%` placeholders and is excluded. The output of `fusion --check --json` and of `fusion --commands`, cross-argument rules included, validates against its schema (cross-section item 1.9, 01.C9) |
| `network` | KGML rejections (S2.06); keys and threshold (S2.07); S2.08; complexes and component edges (S2.09); fan-out, undirected pairs, tags (S2.10); indirect and ECrel settings (S2.11, S2.31); family bindings reported (S2.03); KEGG pathway names from each KGML's root title (entry 205) | write then read is the identity on the canonical form, with escaping (S2.25); edge keys with the type are unique (02.C11; without the type, 3 edges collide in the reference merge and 35 in the full run); `[kegg]`: the legacy reader reproduces the full run's graph 2 from the old master, 3,100 nodes (198 isolated) and 7,400 edges, 6,632 after the duplicate merge (02.C5; cross-section item 1.30) |
| `merge` | edge merge (S2.20, S2.21, S2.10); a type conflict stops the run (S2.19); the report (S2.24); a single KGML read is a one-file merge (03.C5: in the 167 KGMLs, 309 ids sit in two boxes of one pathway, in 71 files) | order independence over random permutations (S2.17); each id in exactly one node (stage 2 question 4); merge then split equals both oracles (see below) |
| `expression` | one parser for every reader (S3.14) and one decimal grammar (04.C2): `NA`, `nan`, `inf`, `1_000`, `1,5` and `0x1p0` skipped and reported, a header, CRLF, a lone CR, a UTF-8 byte-order mark, repeated ids, missing values (S3.04, S3.05, S3.08, 04.C10); a UTF-16 byte-order mark or a NUL byte fails as `values.encoding` (04.C10); repeated ids keep the largest magnitude under `max_abs` (04.C3); inclusive bounds in doubles (S3.06); up threshold above down (S3.07); no gene past the thresholds is the reported result `observations.none`, with two empty lists (04.C5) | `[kegg]`: threshold suggestions equal FuSION 1's `suggest_thresholds.py` report on the EGF file, plus S3.17's ±k·SD about 0 rows |
| `corridor` | traversable subtypes (S4.04); edges into a target (S4.05); exclusions, which also remove every complex that contains the excluded gene (S4.06 as extended in entry 170); distances (S4.07); complex members (S4.12); stable keys (S4.14); one binding per id (S4.15); empty corridor reported (S4.18); ids checked before the first step and at the step (05.C3) | `walk_corridor` on 1000 random graphs, with trimming during the search (05.C2); corridor fingerprint equal before and after a solve (S4.20). `[kegg]`: the reference corridor has 108 nodes and 358 edges, the set the solve encodes (keeping, then trimming, would leave 112 and 363); `observations_match.tsv` places 3, 28 and 14 genes for the reference run and 20, 24 and 1 for the full run (05.C9) |
| `solve` | FSC sets and the cases `in`, `out` and `free`, including a down-regulated FSC and free's transfers, where a free witness transfers to out only with the FSC's complexes absent (06.C10, 06.C12); every avoided gene, on an avoid list or as an out case's FSC, forbids every complex that contains it (entries 170 and 229); `points=` (06.C9); `solutions=k` (06.C11); endpoints present and active in the named form (06.C16); edge bound clamped with a warning (06.C14); seed in every dump and in `run.json` (S5.22); dumps only under debug, with `names.tsv` and a `session.smt2` that replays the solve (06.C13); `rlimit`, recorded as `solve.rlimit` (2B.8); cases in parallel give the same per-case answers and witnesses as `threads=1` (06.C4) | `exhaustive_frontier` against Z3 on 200 random tiny cases per run (6 nodes, 8 edges), and larger ones in the weekly run; every model passes `check` (rule R1 below); the reported frontier is the Pareto-minimal witness counts (06.C20). The Pareto search on `ScriptedOracle` over random staircases in boxes up to 20 × 20: the exact staircase, no point checked twice (S5.20), the cache never changing an answer (06.C5), and each uncertified point naming its blocker (06.C6). Resume from the journal equals an uninterrupted solve, after a stop and after a kill (06.C7; 06.C19 amends A1); under a longer timeout, TIMEOUT answers are asked again (06.C8 amends S5.20). Two solves in one process equal two processes (S5.29). `[kegg]`: 06.C6's rule applied to FuSION 1's recorded answers leaves 23 of 154 frontier points uncertified, in 8 of 34 bound-10 solves |
| `results` | one `results.json` per solve, holding all its cases (07.C1); a view file and a page shell per point, named by case (`in_n1_e0.json`, `in.dat`; 07.C2, 1.6); stable keys (S4.14, S5.27); "no PO point" (S6.21); FuSION 1 text writers byte-exact on fixed inputs, under `legacy_prefix` (1.21); the Pareto plot step's gnuplot script text, not the image | the renderer and the solve call one polarity rule, and it equals the spec's table (S6.14); the page assets compiled into `libfusion` match their pinned SHA-256, vis-network 10.x among them (07.C3, 11.C11) |
| `enrichment` | ties by gene id in byte order (S7.05, 08.C2); equal \|NES\| ordered by pathway id (08.C9); selector parsing and term-order independence (S7.13); keep list (S7.14); a row for every candidate (S7.09); empty selection reported (S7.03); no candidate of size 15 to 500 fails (S7.09); permutation counts outside 1 to 100,000 fail the check as `batch.bad_value` (08.C8; the code from prompt log entry 230); an all-zero draw gets the step 1/m and a warning, and an undefined FWER is 1.0 (08.C7) | exact equality with gseapy (08.C1; see below); identical bytes for any candidate order; the shipped notices include gseapy's BSD-3-Clause and rand's MIT texts (08.C1) |
| `run` | a run directory is never reused (S8.21); one directory per step, and the run page written once, at the run's end (09.C2, cross-section item 1.4); statuses `ok`, `failed` and `stopped` (1.31); every setting recorded with its source (E1); dotted events (1.3); every write through `OutputFile` (1.28); each journal record durable before the next check (09.C6); checks before the first step (0021) | for each step k, stop after k then start from k equals the full run (D1, 0019); two runs at once in one launch directory do not interfere (S1.26); a run continues and stays resumable when the frontend exits (10.C16) |

### End-to-end tests

- **Driver.** `fusion-test e2e` starts `fusion` as a child process for each case, in a fresh `ScratchDir`. It then compares the run directory with `expected/` after normalisation, and checks the exit status and the stderr fragments the case names.
- **Case file:**

  ```json
  {
    "case": "toy22-fsc-t13-b10",
    "args": ["-b", "case.batch"],
    "expect": {"status": "ok", "exit": 0},
    "tags": ["e2e", "solve", "S5.17", "S5.19"],
    "compare": [
      {"glob": "run.json", "mode": "exact"},
      {"glob": "events.jsonl", "mode": "exact"},
      {"glob": "fb_rch/observations_match.tsv", "mode": "exact"},
      {"glob": "pathz3/results.json", "mode": "results"},
      {"glob": "pathz3/*/search_log.tsv", "mode": "tsv_masked",
       "masked_columns": ["wall_s", "cpu_s", "source", "model_n", "model_e"]},
      {"glob": "pathz3/*/observations.tsv", "mode": "exact"}
    ]
  }
  ```

  The paths follow 09.C2's step directories and cross-section item 1.4: `pathz3/results.json` holds every case of the solve (07.C1), and `pathz3/in/`, `pathz3/out/` and `pathz3/free/` hold each case's files (06). The statuses are 09's `ok`, `failed` and `stopped`, with exit statuses 0, 1 and 3 (cross-section item 1.31). A failure case names the batch line instead: `"expect": {"status": "failed", "exit": 1, "line": 14, "stderr": "no such file"}`.
- **Normaliser.** In `exact` mode it masks only the fields that section 09 lists as volatile (cross-section item 1.14). In `run.json` those are the run id and name, process id, labels, times and resources, launch directory and command line, the `platform`, `install` and `environment` blocks, absolute paths (written as `<RUN>`, `<CASE>` and `<DATA>`), tool paths and the stop record. In `events.jsonl` they are the envelope's `t`, every time field, `run.started`'s `pid`, `dir` and `fusion`, and the model-dependent fields of `solve.check` events (`source`, `model_n`, `model_e`), as in the search log. *Phase 3 (prompt log entry 349):* also `run.started`'s `run`, `name` and `parent`, which change in every run. The normaliser also masks FuSION's own version and commit (`build_id`, `versions.fusion`), so the goldens survive a commit. The Z3 version stays, and paths are written with `/`. Everything else is compared byte for byte. The other two modes follow 12.C11, because the goldens serve all three platforms:
  - `tsv_masked` blanks named columns. In the search log those are the times, and the columns that depend on the model Z3 returns (`source`, `model_n`, `model_e`); the points asked and their outcomes stay.
  - `results` compares the frontier and its certification flags exactly. Each witness must pass `check()` with a cost equal to its point, but is not compared byte for byte; the repeat test compares witnesses on one machine.

  A self-test changes one unmasked field and requires a reported difference.
- **Cases:**
  - the toy network with several FSCs: `cases=in,out` on one corridor (S4.20), an FSC of two ids, and a solve with no FSC, which is the `free` case alone (06.C10);
  - a bound sweep in which one bound gives an empty corridor, a selector sweep in which one selection is empty, and a threshold sweep in which one point puts no gene past the thresholds: every later line of that point is skipped with a `step.skipped` event, the point ends `ok` with exit 0, and the other points run on (S4.18, S7.03, 04.C5, 01.C15);
  - a sweep in which one point fails: only that point stops, the others run to their end, and the sweep exits 1 (01.C5);
  - each kind of partial run (0019), single points through `points=` among them (06.C9); a stop request written while the solve runs, then a resume; a kill during a solve, then a resume from the journal, which loses only the check in progress (06.C19 amends A1); a resume under a longer timeout, which asks TIMEOUT answers again (06.C8 amends S5.20);
  - failures: missing and unreadable inputs (0021), a bad number (`batch.bad_value`), an unknown, repeated or missing argument, a positional token after a named one, or a wrong number of positional tokens (`batch.bad_argument`, entry 230), an unknown command, a failing `exec` (S1.28), `fusion -b` with a missing file (S1.01), `-h`, and the dropped `-d` (S1.02, S1.03); a usage error exits 2 (01.C14); the same problems reported by `fusion -b <file> --check --json` as 01's `diagnostics`, with 09's `command`, `setting` and `path` fields (1.9);
  - FuSION 1 forms: positional `pathz3` with 28 to 34 tokens, `mff … xml … -1`, `rgx … y` (S2.16, S5.01), the six fixed-name `exec mv` lines, the limits file in FuSION 1's rendering (01.C11), a positional `pex` line with `both`, which only the positional form keeps (1.6), a positional `pex` file that no `pathz3` line of the batch wrote, which fails the check as `batch.unmatched_file` (07.C9), and an input made by an earlier `exec` line, checked just before its step (01.C12). The check notes each FuSION 1 batch that names no FSC as `batch.no_fsc` (2A.9). Cases with POSIX `exec` lines carry `[posix-shell]`; on Windows they run under the bundled busybox-w32 shell (01.C10);
  - a window-plot `exec gnuplot -p` line, skipped with a note when no display exists (entry 190); and an `exec gnuplot` line on a machine with no system gnuplot, which finds the bundled one appended to `PATH` (11.C20);
  - a fake home and fake tools given through `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT`: the run uses them, and the case asserts that `run.json` records each under `environment`, a block the normaliser otherwise masks (11.C20). Scripted fake tools also give a missing plot font, warned as `deps.font_missing` before the first step while the plot is still drawn (07.C8), and a histogram that gnuplot did not write, which fails its step as `expression.gnuplot_failed` (entry 230);
  - paths with spaces, non-ASCII characters, Windows drive letters and lengths over 260 characters; CRLF inputs;
  - ids containing `:` or `*`, and the Windows reserved names `CON` and `AUX`, which must never become file names;
  - the debug option, with and without: debug files appear only under `debug/<step>/` (S1.29, S4.19, S5.23, cross-section item 1.4);
  - two `fusion` processes in one launch directory;
  - an absolute output path in a batch: the file is written there and kept in the step directory too, both recorded in `run.json` (cross-section item 2B.3).
- **Golden updates.** `fusion-test e2e --update <case>` rewrites `expected/` from the normalised outputs. The diff is shown before any commit (CLAUDE.md).

### Reference-test runner

`fusion-test ref` runs the reference cases. It is C++ and links `libfusion` and the test kit (12.C8).

```cpp
// backend/tests/tools/fusion-test/ref/*.h  (test-only)
namespace fusion::reftest {
enum class Tier { t1, t2, t3 };
enum class Stage { classify, exceptions, gsea, merge, corridor, solve_inputs, solve };
enum class Mode { chained, pinned, verify };
struct PathMap { std::string from, to; };          // applied to batch tokens and to list files
struct SolveArtefacts {                            // one FSC case
  std::filesystem::path dat, limits;
  std::optional<std::filesystem::path> solutions, relaxations;   // absent when there is no point
};
struct Fusion1Artefacts {                          // paths relative to reference/<id>/ in the data root
  std::filesystem::path up, down, exceptions, console;
  std::optional<std::filesystem::path> gsea_table, shortlist;    // GSEA cases only
  std::filesystem::path reread, corridor;                        // from the capture batch
  std::map<std::string, SolveArtefacts> solve;                   // "in", "out" or "free" (06.C10)
};
struct Case {                                      // one entry of cases.json, field for field
  std::string id;
  std::filesystem::path batch;
  Tier tier;
  std::string source;                              // "recorded 2026-09-28", "regenerated"
  std::vector<PathMap> path_map;
  Fusion1Artefacts fusion1;
  std::map<std::string, double> fusion1_seconds;   // "in", "out" or "free"
};
struct Entry {                                     // one expected difference
  std::string id, why;
  std::vector<std::string> spec, allows, seen_in;
  std::vector<Stage> stages;
  nlohmann::json applies_when;
  std::optional<nlohmann::json> predicts;
};
enum class Outcome { match, expected_difference, unexpected_difference, prediction_failed,
                     stale, inconclusive, skipped, not_runnable };
struct Finding {
  std::string case_id, solve_case;                 // solve_case: "in", "out", "free" or empty
  Mode mode;
  Stage stage;
  Outcome outcome;
  std::vector<std::string> entries;
  nlohmann::json detail;
};
struct Options {
  Tier max_tier;
  int workers;
  testkit::DataRoot data;
  std::filesystem::path out;
  bool record_seen = false;
};
struct Report { std::vector<Finding> findings; int exit_code() const; };

std::vector<Case> load_cases(const std::filesystem::path& cases_json);
std::vector<Entry> load_expectations(const std::filesystem::path& expected_differences_json);
std::vector<testkit::Point> verification_points(const testkit::Frontier&,
                                                testkit::Point box_lo, testkit::Point box_hi);
Report run(std::span<const Case>, std::span<const Entry>, const Options&);
void write(const Report&, const std::filesystem::path& out_dir);    // report.json, summary.tsv
}  // namespace fusion::reftest
```

#### Reference cases

| Group | Cases | Source | FuSION 1 pathz3 time | Tier |
|---|---|---|---|---|
| Toy network, FSCs `hsa5` to `hsa18`, b10 and b15 | 12 | regenerated (outputs date from 2025) | milliseconds | T1 |
| EGF-HeLa b6, old master: ERK1, JUN, NFKB1 × 5 settings | 15 | regenerated (recorded 2026-06-18 and 2026-06-29, before the score fix of 2026-07-09) | 13.5 to 107.8 s each, 724 s in all (`timing.log`) | T2 |
| macOS model example (reduced master) | 1 | regenerated (its batch has a placeholder `$WORK_DIR`) | 0.46 s of checks | T2 |
| JUN b6, old master | 1 | recorded 2026-07-17 | 28 s | T2 |
| JUN b6 and b7, GSEA fdr:0.25 and fdr:0.4 with keep list | 4 | recorded 2026-09-28 | 0.75, 1.05, 7.7, 35.9 s | T2 |
| JUN b10, GSEA fdr:0.25 and fdr:0.3 | 2 | recorded 2026-09-28, 2026-10-14 | 615, 738 s | T2 |
| JUN b7, old master; JUN b10, fdr:0.4 | 2 | recorded | 2369, 4140 s | T3 |
| JUN, FOSB, DUSP1 b10, old master | 3 | recorded 2026-07-17 | 61,292, 11,361, 20,017 s | T3 |
| JUN b10, fdr:0.4 with keep list; fdr:0.5 | 2 | recorded | 74,534, 182,500 s | T3 |
| fdr:0.44 candidates `r0` to `r8` | 9 | recorded 2026-11-10 by the snapshot binary | 29,990 to 187,623 s | T3 |

- That makes 12 synthetic and 39 KEGG cases. The sweep's `fdr025/` repeats the reference run byte for byte (S5.22) and is not a separate case.
- **Case names.** FuSION 1's with-FSC and without-FSC solves are compared with 2.0's `in` and `out` cases, and a single solve with `free` (06.C10). The candidate runs use every shape (prompt log entry 175): `r3` and `r4` avoid 2 and 4 nodes as one arm, `r2` names two ids, and `r7` and `r8` are single solves with no FSC. Their pinned solves therefore use FSC sets and the `free` case.
- **Why 2026-07-16.** The FuSION 1 changelog (`CHANGELOG.md`, live tree) lists every later change as output-preserving:
  - 2026-09-28: the merge chain fix, "byte-identical graphs" where it did not crash (lines 123-133);
  - 2026-11-09: timeout labels only (lines 31-42);
  - the undated iterations 24 to 27: renderers and paths (lines 163-197).

  Earlier builds differ. Indirect relations were kept before 2026-06-23 (`54ced64`, line 248), and the discard applies to tool-written reads too (`Linux/src/XMLParser.cc:1481-1490`). `experiment_score` was lost in the `wgx`/`rgx` round trip before 2026-07-09 (`95387e0`, line 239).
- **Distinct inputs.** The 19 GSEA runs among the 24 (18 cases, since the sweep's `fdr025/` repeats the reference run) use 7 distinct pathway lists: 42, 56, 71, 87 (fdr:0.4 with keep list), 87 (fdr:0.44), 88 and 90 pathways. The lists of `r7` and `r8` differ only in the folder of the same curated XML, whose SHA-256 is equal. All 24 use 3 distinct exception sets (49, 54 and 55 ids), so there are 7 distinct raw merges. Every FSC given as an id is already in its run's exception file: JUN, FOSB and DUSP1 are up-regulated, and the candidate runs list RHOA, CDC42, RAC1, STAT3 and CREB1 by hand. S2.13's change therefore touches none of them, and synthetic cases cover it.

`cases.json`, one entry:

```json
{
  "id": "jun-b10-fdr025",
  "batch": "fusion1-batches/runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch",
  "tier": "T2",
  "source": "recorded 2026-09-28",
  "path_map": [
    {"from": "/home/ks-149-u24/Desktop/arjun/FuSION/runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025", "to": "$CASE"},
    {"from": "/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score", "to": "$DATA/kegg/hsa_pathways_with_exp_score"},
    {"from": "/home/ks-149-u24/Desktop/arjun/FuSION/updated_pathways/xml_files", "to": "$DATA/kegg/updated_pathways"},
    {"from": "/home/ks-149-u24/Desktop/arjun/FuSION/Linux/supporting_files", "to": "$DATA/fusion1/Linux/supporting_files"}
  ],
  "fusion1": {
    "up": "f1/run/up_reg", "down": "f1/run/down_reg", "exceptions": "f1/run/exception_file.txt",
    "console": "f1/run_console.log",
    "gsea_table": "f1/run/gsea_kgml_files.gsea.tsv", "shortlist": "f1/run/gsea_kgml_files.txt",
    "reread": "f1/capture/graph_reread.xml", "corridor": "f1/capture/corridor.xml",
    "solve": {
      "in":  {"dat": "f1/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10.dat",
              "limits": "f1/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10_limits_timefile.txt",
              "solutions": "f1/run/temp_w_solutions.txt", "relaxations": "f1/run/temp_w_relaxations.txt"},
      "out": {"dat": "f1/run/hsa1956_to_hsa3576_wo_hsa3725_egf_hela_t120_lfc.tsv_b10.dat",
              "limits": "f1/run/hsa1956_to_hsa3576_wo_hsa3725_egf_hela_t120_lfc.tsv_b10_limits_timefile.txt"}
    }
  },
  "fusion1_seconds": {"in": 13.7, "out": 601.6}
}
```

The case keys are 06.C10's names (cross-section item 1.6); FuSION 1's own file names keep its `w` and `wo`.

`$CASE` is the case's scratch directory, which the runner fills with a copy of `reference/<id>/inputs/`; `$DATA` is the data root. The paths under `fusion1` are relative to `reference/<id>/`. The path map is needed because the batch corpus names paths that no longer exist. For example, the corpus holds 62 references to `…/FuSION/IITB_ACTREC_old_data/…` and 53 to `…/FuSION/EGF_HeLa/experiment`, from before the `runs/` reorganisation.

#### Capture in Phase 3

FuSION 1 never saves its corridor (S4.21). Phase 3 therefore runs a capture batch for every KEGG case with the snapshot binary. The capture batch is the case's batch, cut before the first `pathz3`, with two lines added:
- `wgx 0 $WRITE_DIR/graph_reread.xml KEGG` after `rgx`;
- `wgx 0 $WRITE_DIR/corridor.xml KEGG` after the `fb_rch` that precedes the first `pathz3`.

The batches' second `fb_rch`, before the without-FSC solve, repeats the first with the same arguments; it gave the same corridor counts in all 83 logs that have both (S4.20). One corridor per case is therefore enough. No FuSION 1 code changes. Each capture takes at most about a minute: merges took 1 to 53 s, GSEA 3.3 s. The corridor file lists nodes in copy order, which is the order in which ids are claimed (S4.15), so it also answers stage 4's question 3 (X05).

#### Modes

- **Chained.** The FuSION 1 batch file runs through `fusion` as written, after the path map. Every stage is 2.0's own, so an early difference cascades. Old batch lines run because of four answers: the six fixed-name moves are written natively, the limits file in FuSION 1's rendering (01.C11); absolute output paths are honoured as well as kept in the step directory (2B.3); window-plot lines are skipped on the headless workstation (entry 190); and other `exec gnuplot` lines find the bundled gnuplot when no system one exists (11.C20).
- **Pinned.** Each stage runs at library level on FuSION 1's recorded input to that stage:
  - classify and GSEA on the LFC file and the candidate list;
  - the merge on FuSION 1's shortlist;
  - the corridor on FuSION 1's re-read graph;
  - the solve on 2.0's corridor of that graph, with FuSION 1's up and down lists and the FSC named, so each FuSION 1 solve becomes 2.0's `in`, `out` or `free` case (06.C10).

  A pinned difference therefore belongs to its own stage. A chained difference is accepted only if the pinned differences upstream of it reach its input.
- **Verify.** Frontier verification for T3 (see "Frontier verification").

#### Comparison levels

| Stage | FuSION 1 facts | 2.0 facts | Rule |
|---|---|---|---|
| classify | `up_reg`, `down_reg` | observations | set equality |
| exception set | `run/exception_file.txt` | the set 2.0 builds (S1.17, S2.13, 03.C2); an unchanged FuSION 1 batch names no FSC, which the check notes as `batch.no_fsc` (2A.9), and keeps its own exception file (03 open question 1, entry 165) | set equality |
| GSEA | `.gsea.tsv`, shortlist | table, shortlist | see "GSEA agreement" |
| merge | `graph_reread.xml` | merged graph after the split | canonical equality |
| corridor | `corridor.xml`, `New graph … has 115 nodes 370 edges` | `fb_rch/corridor.json` (05.C6) | canonical equality and counts, under XD-17 and XD-18 |
| observations | `Up: 3 Down: 0`, the observed genes inside the corridor | the in-corridor rows of `fb_rch/observations_match.tsv` (05.C9) | equal counts, under XD-17 |
| solve inputs | `[coexpression] emitted 1 clauses`, `[frozen-edge] emitted 97 clauses` | the solve's reported counts | equality, under XD-17 |
| solve | `.dat`, limits answers, witnesses | `pathz3/results.json`, each case's `search_log.tsv` | rules R1 to R4 |

New facts that FuSION 1 never had, such as certification flags and blockers (S5.19, 06.C6) or S7.08's relation-share column, are never differences. Rendered outputs (Pareto image, explanation pages) are compared only against 2.0's own goldens.

#### Witness checker and re-scoring

A witness is checked, not compared for equality, because the solver's models depend on its history. In `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/`, the point (n 7, e 0) was answered in three ways:
- `sat` in 0.76 s during the search (`run/…_wo_…_limits_timefile.txt:83-85`);
- the witness re-solve at the same point hit the 4 h cap (`run/temp_wo_solutions.txt:2` reads `timeout`);
- a fresh single check took 442.6 s (`resolve_wo_n7_e0/run/limits_timefile.txt`).

The checker evaluates an assignment under one of two semantics:
- **fusion1:** S5.10 to S5.16 as FuSION 1 implements them: self-loops, edges into the source and edges out of the target counted, nodes kept only through them included, one FSC, and the complexes of an avoided gene allowed. *Phase 3 (prompt log entry 349):* edges out of the target are never considered, so never counted: FuSION 1's walk never expands the target (S5.10; `GraphManagerNew.cc:27960-28045`), and the checker follows S5.10.
- **fusion2:** the decided 2.0 rules:
  - S5.13: self-loops out of the count;
  - S4.11 as 05.C2 reads it: trimmed edges, and the nodes kept only through them, are absent;
  - S4.12: a complex member is a node only when kept in its own right;
  - S5.17 as 06.C10 extends it: FSC sets, and the cases `in`, `out` and `free`;
  - poll 170's rule, which entry 229 extends to avoid lists: every avoided gene, whether on an avoid list or an out case's FSC, also forbids every complex that contains it;
  - 06.C16: in the named form, the source and the target are present and active.

The rules:
- **R1 validity.** Every witness passes under its own semantics. A 2.0 witness costs exactly its point, because 2.0's frontier is the Pareto-minimal witness counts (06.C20, amending S5.25). A FuSION 1 witness may cost less than its point, which 06.C20 would replace by the witness's counts. On the 24 valid runs none does: all 177 witnessed points have relaxation counts equal to their point (measured from the relaxations files). The 12 FuSION 1 points of 591 whose witnesses dominate them all lie in `runs/real_experiments/` (entry 174). Two points have no witness, because their witness check timed out: `r5`'s out case at (7, 0), and `r6`'s at (2, 8).
- **R2 2.0 is no worse.** For each FuSION 1 witness, its fusion2 cost must be weakly dominated by a 2.0 frontier point, unless 2.0's answers in that region are unknown.
- **R3 FuSION 1 is no worse on its own terms.** For each 2.0 witness, its fusion1 cost must be weakly dominated by a certified FuSION 1 point, unless FuSION 1's answer there was unknown.
- **R4 exact match.** Where no applicable expected difference touches the case, the certified points of both frontiers are equal, and definite answers agree point by point.
- An unknown answer on either side makes the point inconclusive. It is reported, not failed.
- R2 and R3 need the witness to be a valid assignment on the other side's corridor. When an upstream difference makes it invalid there, the re-score is reported as not applicable, with the checker's reason. Examples: a binding changed by XD-08, a node whose members changed under XD-03 or XD-16, or a complex of an avoided gene under XD-19, in either mode.

**Example, the full run's out case.** FuSION 1's frontier is (8, 0), (7, 1), (6, 2), (5, 3), (4, 4), (3, 5), (2, 7). Two expected differences re-score its witnesses:
- XD-07: the witnesses at (7, 1), (5, 3) and (4, 4) each relax one self-loop; at (7, 1) it is NFKB1 ⊣ NFKB1 (`run/temp_wo_relaxations.txt:26`);
- XD-17: AREG, relaxed at every point (`run/temp_wo_relaxations.txt:4` at (8, 0)), lies within the bound only on walks that pass back through EGFR, so it leaves the corridor (05.C2).

Re-scored under fusion2, the seven witnesses cost (7, 0), (6, 0), (5, 2), (4, 2), (3, 3), (2, 5) and (1, 7). So by R2, 2.0's out curve must reach (6, 0), (4, 2), (3, 3), (2, 5) and (1, 7) or better. FuSION 1 left (7, 0) undecided after 14,396 s. The in case's frontier, (7, 0), (6, 1), (5, 2), (4, 3), (3, 4), (2, 6), has no self-loop relaxation but relaxes AREG at every point, so its bound is (6, 0), (5, 1), (4, 2), (3, 3), (2, 4), (1, 6). FuSION 1's two curves differ by one edge at every n from 2 to 7; if both bounds are tight, 2.0's differ by one edge only at n = 5, 2 and 1, and meet everywhere else. That changes how the FSC test reads for this run. Across the 24 valid runs, XD-07 moves 6 of 179 frontier points: these three, and (3, 3), (2, 5) of `…_gsea_fdr040_keep` and (2, 8) of fdr050, all in the out case. XD-17 moves 44: every point of the JUN, FOSB and DUSP1 b10 old-master cases, which relax AREG (37 points), and of the JUN b7 old-master case, which relaxes HBEGF (7 points).

#### Expected differences

`backend/tests/reference/expected-differences.json` holds the list that decision 0023 asks for. Example entry:

```json
{
  "id": "XD-07",
  "spec": ["S5.13"],
  "why": "Self-loops leave the edge-relaxation count, so a witness that relaxed one costs less in 2.0.",
  "stages": ["solve"],
  "applies_when": {"fusion1_witness_relaxes_any": ["self_loop"]},
  "allows": ["solve.frontier", "solve.witness"],
  "predicts": {"rule": "rescore_bound", "semantics": "fusion2"},
  "seen_in": ["jun-b10-full:out", "jun-b10-keep040:out", "jun-b10-fdr050:out"]
}
```

- `applies_when` is evaluated per case, so an entry cannot hide a difference in a case it does not explain.
- `allows` names the facts that may differ.
- `predicts`, when present, is checked; a failed prediction is a failure. `rescore_bound` is rule R2: it bounds 2.0's frontier from above, and does not fix it exactly.
- `seen_in` lists where the difference occurs. A listed case that no longer shows it is reported as stale and fails until the list is updated (`fusion-test ref --record-seen`, reviewed as a diff).

There are 27 initial entries: 16 from the spec's decisions, XD-15 from G1 with section 11's Z3 pin (11.C2), and XD-19 to XD-28 from the changes accepted in polls 158 to 205 (12.C6, prompt log entry 207). XD-18 (S4.12) joined the spec's entries in this revision, from section 05's corridor counts. XD-02, 2.0's own permutation stream (F1 with S7.11), was withdrawn when 08.C1's exact port was accepted (entry 159); its id is not reused.

| Id | Spec | Change | Applies when | Effect on the reference cases |
|---|---|---|---|---|
| XD-01 | S7.05, 08.C2 | ties ordered by gene id, compared in byte order (entry 181) | every `gsea` | fdr:0.25, 0.4, 0.44 and 0.5 select 44, 79, 86 and 91 pathways instead of 42, 71, 87 and 90 (S7.05; section 08's reference port). With 08.C1's exact port, every GSEA number that differs from FuSION 1's tables traces to this order. Chained runs of all 18 GSEA cases merge other lists, so their later stages may differ |
| XD-03 | S2.08 | box genes no longer lost | raw merges; legacy reads keep the loss (02 open question 1, entry 163) | 11 ids that FuSION 1 lost join nodes in the 42-pathway merge (`hsa861`, `hsa7252`, `hsa5345`, `hsa2894`, `hsa2895`, `hsa2897` to `hsa2901`, `cpdC22540`), 13 in fdr040_keep's. The unions they bridge widen: the glutamate-receptor nodes of 7 and 4 genes become one of 18, and three compound nodes become one (section 03, entry 164) |
| XD-04 | S2.21 | tag compared once | raw merges | 6, 7, 12 and 12 tagged and untagged duplicates become single edges in the 42-, 87- (fdr040_keep, `r0`) and 90-pathway merges (section 03) |
| XD-05 | S2.10 | a skipped duplicate's score still counts | raw merges | some scores rise, and frozen-clause counts may rise |
| XD-06 | S2.09 | component edges carry their own file's pathway | raw merges | 6 component edges per merge carry a different pathway list (section 03) |
| XD-07 | S5.13 | self-loops out of the count | a FuSION 1 witness relaxes a self-loop | 6 of 179 points; re-scoring bounds 2.0's frontier (above) |
| XD-08 | S4.15 | ids resolved once per run | an id in several nodes | old-master cases: 57 or 58 ids of the full run's corridor change node (S4.15; X05 settles which) |
| XD-09 | S2.13 | the FSC joins the exception set | the FSC is missing from FuSION 1's exception file | none of the 24; synthetic cases only |
| XD-10 | S5.17 | a down-regulated FSC "in" is inactive | the FSC is down-regulated | none of the 24 |
| XD-11 | S5.26 | a witness at every SAT frontier point | always | witnesses differ in general; `r5` (7, 0) and `r6` (2, 8) gain witnesses |
| XD-12 | S5.09, 06.C14 | both edge bounds clamped to R_e, the number of counted edges, with a warning (entry 176) | FuSION 1's upper edge bound is 2^width or more | none of the 24: their edge bound is 8, and their corridors have 134 to 2138 edges |
| XD-13 | S5.03 | given inputs switch constraints on | a `pathz3` line without the flag | the six lines of `experiment_1`, `experiment_2` and the comparison batches; none of the 39 |
| XD-14 | S3.06 | doubles instead of floats | a value within float rounding of a threshold | none in the EGF file |
| XD-15 | G1, 11.C2 | Z3 5.3.3 instead of 4.8.12 (entry 173) | every case, unless X11 sends the pin back to the user | witnesses, check times and unknown answers; certified points must still match |
| XD-16 | S2.11 | indirect relations dropped before nodes are made | raw merges | a box reached only through indirect relations no longer joins unions (section 03, entry 164). In the reference run, NFKB2 and RELB stay apart from NFKB1 and RELA, BAK1 and BAX become two nodes, and GNAL, GNAO1, HK1 to HK3, HKDC1 and VDAC1 leave. In the 87- and 90-pathway runs the receptor-tyrosine-kinase node loses TFG, TPM3, TPR, CCDC6 and NCOA4, so its key moves from `hsa10342` to `hsa1436` and its complex from `c_s_hsa10342_c_e` to `c_s_hsa1436_c_e`. Nodes are matched by member sets, never by key |
| XD-17 | S4.11, 05.C2 | the corridor's searches never follow self-loops, edges into a source or edges out of a target, so the nodes kept only through them leave too (entry 170) | always | with XD-18: 108 nodes and 358 edges for the reference run against FuSION 1's 115 and 370, 547 and 2117 for the full run against 592 and 2138, and 253 and 1037 for the JUN b7 old master against 269 and 1049 (section 05's replay). Observed genes kept only through trimmed edges leave the corridor, so every point of four old-master cases moves down by 1 in n: AREG in the JUN, FOSB and DUSP1 b10 cases (37 points), HBEGF in the JUN b7 case (7 points). Predicted: the replay's counts, and the re-scoring bound (above). `seen_in` is filled by `--record-seen` from the Phase 3 captures |
| XD-18 | S4.12 | a complex member is a corridor node only when kept in its own right | FuSION 1's corridor holds isolated members | 3 of the reference run's 115 corridor nodes, 41 of the full run's 592 and 14 of the JUN b7 old master's 269 (section 05) |
| XD-19 | S4.06, S5.17 (entries 170 and 229) | excluding a gene also removes every complex that contains it, and avoiding a gene, on an avoid list or as an out case's FSC, forbids them | an excluded or avoided id sits in a complex | no case excludes an id. `r5`'s out case at (4, 4): FuSION 1's witness routes STAT1 → [STAT1+STAT3] → MYC with STAT3 avoided (`egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/run/temp_wo_solutions.txt:300` and `:362-363`). In both modes that witness is invalid, pinned with STAT3 named as the FSC and chained through `r5`'s own avoid list, so R2 does not apply there and 2.0's point may be costlier. No other out witness of the 24 valid runs uses a complex of an avoided id |
| XD-20 | S5.25, 06.C20 (entry 174) | the frontier is the Pareto-minimal witness counts | a FuSION 1 witness costs less than its point | none of the 24: every witnessed point has counts equal to its point; 12 of 591 FuSION 1 points, all in `runs/real_experiments/`, would move |
| XD-21 | S2.27, S4.13, 02.C5 (entry 159) | the legacy reader merges exact duplicate edges as it reads, where FuSION 1 merged them at the corridor copy | FuSION 1's graph holds exact duplicates | the legacy-read cases (the old master and the model example's reduced master); in the full run, graph 2's 7,400 edges become 6,632 (768 merged). Predicted: corridors and solves unchanged |
| XD-22 | S2.07, 03.C5 (entry 166) | a single KGML read is a one-file merge, so ids in two boxes of one pathway share a node | a single KGML-format file is read | none: no id sits in two boxes of the three toy networks, and no KEGG case reads a single KGML; synthetic cases only |
| XD-23 | S3.02, S3.03, 04.C2 (entry 167) | one decimal grammar: a value that is a number only by its prefix, such as `1,5`, is skipped and reported, where FuSION 1 read it as 1 | a value token that matches only as a prefix | none: all 6,372 value tokens of the five two-column files match |
| XD-24 | S3.08, 04.C3 (entry 167) | where one value per gene is needed, the largest magnitude, not FuSION 1's last line; `repeats=max_abs` collapses rows before classification | a value file repeats an id | none: the reference cases' value files have unique ids |
| XD-25 | S3.03, 04.C10 (entry 169) | a lone CR ends a line, and a UTF-8 byte-order mark is dropped | a value file with either | none: the real files have LF or CRLF line ends and no byte-order mark |
| XD-26 | 02.C7 (entry 161) | maplink relations skipped by default | a KGML with maplink relations | none: no KGML of any reference case has one |
| XD-27 | 08.C7 (entry 182) | an all-zero draw gets the step 1/m with a warning, and an undefined FWER is 1.0, where gseapy's arithmetic gives NaN | a draw whose hits all have value 0 | none on the real inputs (about 10⁻²⁰ for a 15-gene draw); one synthetic fixture with 80 % zeros, kept out of the exact gseapy comparison |
| XD-28 | 08.C9 (entry 182) | equal \|NES\| ordered by pathway id, where FuSION 1's unstable sort left them in no defined order | two candidates with equal \|NES\| | none: no two of the 167 candidates tie (the closest \|NES\| differ by 4.2 × 10⁻⁶); synthetic twins only |

#### Frontier verification

To confirm a reference frontier F with k points, sorted by n, inside the box [n_lo, n_hi] × [e_lo, e_hi] that FuSION 1 searched (the first line of its limits file), it is enough to check at most 2k + 1 points with fresh, independent checks:
- `sat` at each point of F, with a witness that passes R1;
- `unsat` at each inner corner of the staircase: (n_{i+1} − 1, e_i − 1) for each pair of neighbouring points, plus (n₁ − 1, e_hi) and (n_hi, e_k − 1) when they lie inside the box.

By monotonicity (S5.18), the unsat corners make every point outside F's up-set unsat, so this proves that F is the frontier in the box. An empty F needs only (n_hi, e_hi). The runner asks each point through 06.C9's `points=n,e` (decision 0019's "solve part of the frontier"), in a worker process with its own Z3 context, so a crash or the memory of one check does not affect the others.

A fresh context starts cold, the cost that 06's open question 5 records for a resumed case (prompt log entry 210). From scratch, the reference run's out case at (n 2, e 6) took 258.0 s, against FuSION 1's 0.000127 s inside its incremental search (06.C1). The check times below use FuSION 1's time at each point's first check, so they understate verification wherever FuSION 1 answered from a warm context. A cold check can also time out where FuSION 1's answered; that point is then inconclusive, not failed.

Measured over the 24 valid runs (46 cases):

| | FuSION 1 search | Verification |
|---|---|---|
| checks | 703 (262 sat, 402 unsat, 39 unknown) | 360, all of which FuSION 1 had checked; 23 were unknown |
| check time | 1,057,265 s (293.7 h) | 702,742 s (195.2 h), using FuSION 1's time at each point's first check |
| longest single check | 14,400 s (the 4 h cap) | 14,400 s |
| wall time, all cases at once | bounded by the slowest sequential search: 149,664 s (41.6 h, `r6`'s out case) | bounded by the cap and by the total over the workers: about 12 h with 16 workers, 6 to 9 h with 32 |

A verification that fails at some point triggers a full 2.0 search for that case, whose frontier is then judged by R1 to R4. Whether verification switches to 2.0's own reviewed frontiers after the golden search is 12.C16. The ten slow b10 runs that have a `run_time.txt` (`r0` to `r8` and fdr050) peaked at 1.1 to 2.0 GB each, and the sweep's fdr025, fdr030 and fdr040 at 0.32 to 0.52 GB. So even 32 workers of the largest size fit in 124 GB.

#### Reports

`fusion-test ref` writes a report directory outside any run directory. `report.json` (2.0's values illustrative):

```json
{
  "runner": "fusion-test 0.1.0",
  "fusion": {"version": "2.0.0-dev+g1a2b3c4", "z3": "5.3.3"},
  "fusion1": {"binary_sha256": "22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50",
              "snapshot": "FuSION1-reference-2026-11-10"},
  "data_manifest_sha256": "…",
  "tier": "T2",
  "findings": [
    {"case": "jun-b6-fdr025", "mode": "pinned", "stage": "solve", "solve_case": "in",
     "outcome": "match", "fusion1": [[0, 0]], "fusion2": [[0, 0]]},
    {"case": "jun-b10-full", "mode": "pinned", "stage": "solve", "solve_case": "out",
     "outcome": "expected_difference", "entries": ["XD-07", "XD-17"],
     "fusion1": [[8,0],[7,1],[6,2],[5,3],[4,4],[3,5],[2,7]],
     "predicted_bound": [[6,0],[4,2],[3,3],[2,5],[1,7]]}
  ],
  "summary": {"match": 31, "expected_difference": 6, "unexpected_difference": 0,
              "prediction_failed": 0, "stale": 0, "inconclusive": 2, "skipped": 0}
}
```

`summary.tsv` has one line per finding (values illustrative):

```
case	mode	stage	solve_case	outcome	entries	detail
jun-b6-fdr025	pinned	solve	in	match	-	1 point
jun-b10-full	pinned	solve	out	expected_difference	XD-07,XD-17	7 of 7 points move
```

Each case also keeps its 2.0 run directory and its diffs while it fails; they are deleted once it passes.

#### How long the reference suite takes

- **T2:** about 2151 s of FuSION 1 solve time in all: 74 s for the five fastest recorded cases, 724 s for the EGF-HeLa b6 cases (their `timing.log` files; the console's `[pathz3 timing]` lines miss the with-FSC solves of the four NFKB1 `nr4` cases, which found no point), and 615 s and 738 s for fdr:0.25 and fdr:0.3. Run as full searches on 8 workers, the tier is bounded by its slowest case, fdr:0.3 at 738 s in FuSION 1. Within the tier, one check alone took 600.6 s: the reference run's out case at (1, 4). Non-solve stages take seconds per case. Budget: 15 min.
- **T3:** 312 verification checks over the 16 slow cases, about 702,000 s of check time. That is about 12 h on 16 workers and is never less than the 4 h cap. Budget: 24 h.
- **Golden search** (once): bounded by the slowest case. FuSION 1 took 187,623 s (52.1 h) on `r6`; 2.0 skips repeated timeouts (S5.20, 2 of `r6`'s 9 unknown checks repeated a point) and witness re-solves (S5.26).
- **Re-running every recorded search instead** would cost 1,241,580 s (344.9 h) of FuSION 1 time, and at least 52.1 h of wall time even with every case in parallel.

### Merge then split, and one node per id (S2.22, stage 2 question 4)

Section 03 designs `split(base, E)` and a direct merge, `merge_direct(R, E)`, and asks this harness to show that both give byte-identical canonical output. `split` recomputes the affected part with the same code that `merge_direct` runs, and both read `network`'s pathway records. A bug in that shared code would therefore pass the comparison unseen. The test kit adds `exception_merge`, a second oracle written from S2.07 to S2.21 that shares no code with `merge` or `network` (12.C14). Three layers of checks, from cheap and exhaustive to real:
- **Bounded exhaustive (T0).** About 50 synthetic pathway sets, each over an id universe of at most 10 ids, are tested against every exception subset: up to 1024 subsets per set. The property: the canonical forms of `split`, of `merge_direct` and of `exception_merge` are equal. The sets cover:
  - two boxes that share only an exception id (the bridge must break);
  - chains A ∩ B and B ∩ C, the shape that crashed FuSION 1 (S2.18);
  - complexes whose components hold exception ids (Cartesian product, S2.09);
  - relations between two boxes that both hold exceptions (fan-out, S2.10);
  - undirected relations whose tag direction changes after the split (S2.21);
  - self-loops made or removed by a split, and duplicates that collapse or separate;
  - indirect and ECrel relations under both settings (S2.11, S2.31);
  - one-file sets with an id in two boxes of the same pathway, since a single KGML read is a one-file merge (03.C5);
  - thresholds 0, 1, 2 and large; section 03's prototype failed only at threshold 2.

  *Phase 3 (prompt log entry 349):* at this size the layer takes about 10 s in a Release build and 79 s in Debug, which with the random layer overruns T0's budget. T0 runs the listed shapes alone, 32 sets and 2,832 comparisons (1.6 s in Release, 10 s in Debug); the copy with the generated sets, about 50 in all, is tagged `[slow]`, like the real layer.
- **Random (T0).** 1000 generated pathway sets with exception sets, over up to 200 ids and biased towards members of multi-id nodes and complex components, as section 03 asks; the weekly run uses 100,000 with a new seed. The same inputs also check order independence (20 permutations each, S2.17) and that every id lies in exactly one node. *Phase 3 (prompt log entry 349):* at this size the layer takes about 67 s in a Release build and 545 s in Debug, over T0's budget. T0 runs the first 50 sets (3.8 s in Release, 23 s in Debug); the copy with all 1000 is tagged `[slow]`.
- **Real (T2).** The 7 distinct pinned merges of the 24 valid runs are compared with FuSION 1's re-read graphs under XD-03 to XD-06 and XD-16. The 167 KGMLs are also split by 100 random subsets of the EGF up and down genes and compared with both oracles.

Phase 3 also runs FuSION 1's own merge on the synthetic sets with three exception sets each: none, a DE-like set, and a bridging set. That is about 150 runs, whose canonical outputs (about 300 KB) go into the repository. This checks both oracles against FuSION 1 wherever no fix applies. X03 runs the same one-node property on FuSION 1 with random real lists.

Cost: each synthetic merge takes microseconds. The real layer runs 8 merges (the 7 pinned lists of 42 to 90 KGMLs, and the 167 KGMLs once), 100 splits, and each oracle 100 times on the 167 KGMLs. FuSION 1 needed 122 to 126 s for one 163-pathway merge (S2.22); section 03's single pass sets 2.0's cost.

### GSEA agreement with gseapy (S7.11)

08.C1's exact port of gseapy 1.3.1's random stream was accepted, amending skeleton Choice F1's "statistically but not byte for byte" (prompt log entry 159). The test is therefore exact equality, and the statistical test this section first designed (12.C10) lapses. Section 08's reference port already reproduced gseapy on the real inputs with ties by gene id: every column of all 166 rows, compared as doubles.
- **References** (recorded once in Phase 3, capture step 4 below). gseapy 1.3.1 from FuSION 1's `.venv` runs through section 08's harness: the ranking is passed ordered by value, then by gene id in byte order (08.C2), with `ascending=None`, so gseapy keeps that order (gseapy `gsea.py:493`). Runs:
  - the synthetic fixtures (rankings of 50, 500 and 3054 genes; the last copies the EGF file's ties: 190 distinct values, blocks of 145 at 0.00 and 71 at −0.06), seeds 42 and 1 to 20, 1000 permutations each;
  - the EGF file and the 167 candidates, at the same seeds.

  That takes about 2 min. Synthetic results go into the repository; EGF results go into the data root.
- **Exact equality.** Every statistic (ES, NES, nominal p, FDR q, FWER p) is equal as a double, and `Tag %`, `Gene %` and `Lead_genes` are equal as text, for every set and seed. Selections are then equal too: at seed 42, 44 pathways at fdr:0.25 (XD-01). Section 08 measured why this is stronger than a statistical band: between seeds, NES moved by up to 0.082 and FDR q by up to 0.373, so a porting error of that size would pass a band.
- **Outside gseapy's arithmetic.** Fixtures with a draw whose hits all have value 0 stay out of the exact comparison, because gseapy's arithmetic gives NaN there (08.C7). One synthetic fixture with 80 % zeros checks 2.0's own rule instead: each hit of such a draw gets the step 1/m, a warning gives the number of such draws, and an undefined FWER is written as 1.0 (XD-27). Synthetic twin candidates check that equal \|NES\| are ordered by pathway id (08.C9, XD-28).
- **One thread.** GSEA runs on one thread (08.C6), so no thread-count test is needed. The generator's golden values are in `core`'s row above.

### Determinism

- **Repeat.** The same build on the same machine run twice gives identical bytes after masking volatile fields. This covers witnesses, which must reproduce on one machine and Z3 build (S5.22), and threaded steps. Cases solved in parallel (06.C4) must give the same per-case answers and witnesses as `threads=1` whenever no wall-clock limit is reached (section 06's hook).
- **Order.** Permuting the inputs gives identical canonical results: pathway list order (S2.17), entry and relation order within a KGML, LFC line order without repeated ids (repeats have their own test, S3.08), candidate order (S7.12), node-list order, sweep order.
- **Seeds.** The same seed gives identical bytes, and other seeds change only what they should. GSEA ES never depends on the seed. `core::Rng`'s streams, seeded from (seed, stream id) through SplitMix64, give the same numbers whatever the thread count; a test draws one set of streams on 1 and on 8 threads (09.C15). Z3's seed is written in every dump and in `run.json`; X08 settles which parameter reaches the solver (stage 5 question 6).
- **Platforms.** One golden set for all three platforms (12.C11, with 09.C16's byte-identical promise; entry 186):
  - canonical outputs of every deterministic stage are byte-identical, GSEA's tables included;
  - the SMT-LIB text of every query in a debug run is byte-identical, which proves Z3 receives the same formula;
  - frontiers and definite answers match;
  - witnesses are checked by R1 on each platform, and are compared across platforms only as a report.
- **Environment.** Tests also run under `LC_ALL=de_DE.UTF-8` and `tr_TR.UTF-8` (decimal comma, dotless i), under a German locale on Windows, and with `TZ` set away from UTC. Outputs must not change. CRLF inputs must give the same values as LF inputs. Output files never contain `\r`.
- **No hidden state.** Running the same steps twice in one process equals two processes (S5.29). The corridor fingerprint is equal before and after a solve (S4.20).

The cross-platform tests rest on four things, which sections 09 and 11 decided (09.C16, 11.C7, 09.C15 and 11.C5; entries 167 and 186):
- **No floating-point contraction.** `-ffp-contract=off` on GCC and Clang, and MSVC without `/fp:contract`, with 11.C7's other flags: `-O2` with link-time optimisation, `/fp:precise` on MSVC, no `-ffast-math`, `-Ofast` or `-march=native`, the baseline x86-64 and arm64 instruction sets, and the same flags for Z3 where it is built from source. Apple silicon has FMA instructions and x86-64's baseline does not, so contraction would change GSEA's last bits, which S1.27's exact console numbers would expose.
- **No platform maths on result paths.** Every result path avoids the platform's `exp`, `log` and `pow`, or uses one correctly rounded implementation, because maths libraries differ in the last bit and a pathway exactly at the FDR cut-off could otherwise flip (09.C16).
- **Number text without the locale.** Printing uses `std::to_chars`, which on macOS needs a deployment target of 13.3 (section 11). Parsing uses fast_float, because libc++ has floating-point `std::from_chars` only from LLVM 20 (11.C5, with 04.C2's grammar).
- **`core`'s own RNG distributions** (09.C15), with golden values on all three platforms (above).

### Platforms (G3)

| Concern | Linux | macOS | Windows |
|---|---|---|---|
| CI (11.C15) | x86-64, every push | arm64 and x86-64, on pull requests into `main`, weekly and on demand | x86-64, MSVC or clang-cl (section 11), as macOS |
| starting `fusion` from tests | 11's `core::Process`: `posix_spawn` | `core::Process`: `posix_spawn` | `core::Process`: `CreateProcessW` with its own quoting rules |
| peak memory of a child | `wait4` on the child's pid, its `rusage.ru_maxrss` in KB; not `getrusage(RUSAGE_CHILDREN)`, which keeps the largest child reaped so far | `wait4`, `ru_maxrss` in bytes | `GetProcessMemoryInfo` on the child's handle before it is closed, `PeakWorkingSetSize` |
| file names | case-sensitive | case-insensitive by default | case-insensitive; reserved names such as `CON`; `<`, `>`, `:`, `"`, `/`, `\`, `?`, `*` and the vertical bar invalid |
| deleting files | immediate | immediate | fails while another process holds the file open; virus scanning delays it |
| line endings | LF | LF | Git may convert to CRLF; `.gitattributes` prevents it |
| `exec` lines | `/bin/sh` | `/bin/sh` | the busybox-w32 `sh` installed with FuSION (01.C10); `[posix-shell]` cases |

Tests that follow from the table:
- No two files of a run directory may differ only in case.
- A process must be able to read `events.jsonl` while `fusion` writes it, on every platform (A2). Windows share modes make this a real test.
- Replacing an output that another process holds open succeeds through 09's `OutputFile` retry, and readers that open files with delete sharing never block it (cross-section item 1.28).
- A stop request, then a resume, must work the same on all three (A1).
- `ScratchDir` retries removal on Windows sharing errors and never fails a test over cleanup.

### Benchmarks

| Case | Input | FuSION 1 baseline |
|---|---|---|
| read KGMLs | the 167 candidates, 6.9 MB | Python parse in `gsea` 0.24 s (S7.19) |
| merge | the 42-, 87- and 167-pathway lists | 3 s, 53 s, 122 to 126 s for 163 (S2.22) |
| split | each reference exception set on the 167-pathway merge | merge repeated per run (S2.13) |
| legacy read | the old master, 5307 entries, 7517 relations | about 1 s (`run.log`, full run) |
| GSEA | 3054 genes, 167 candidates, 1000 permutations | 1.3 s on 4 threads, 3.7 s on 1, 318 MB peak (S7.19) |
| corridor | the full run's 3100-node, 7400-edge graph, b10 | not timed by FuSION 1 |
| encode | the full run's corridor: 547 nodes and 2117 edges in 2.0, 592 and 2138 in FuSION 1 (XD-17) | 23.8 s of unused closure alone (S5.08); dumps of 2.7 to 2.9 MB per query |
| solve checks | a fixed set of single points from the reference cases that took 10 to 200 s in FuSION 1 | their FuSION 1 check times |
| whole batch | the reference run's batch and inputs | 10 min 58 s, 318,920 kB peak, from the sweep's identical rerun (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr025/run_time.txt`; the reference run has no `run_time.txt`) |
| Z3 re-pin | the real dumps through two or three Z3 builds, in both modes | X11's measurements, section 11's re-pin benchmark |
| synthetic scale (CI) | generated graphs of 1× and 4× the old master | none |

- **Harness.** `fusion-test bench` runs each case in its own child process, so peak memory is the case's own. It records the median and minimum of 5 runs after one warm-up: wall time from a steady clock, CPU time, and peak resident memory.

  ```cpp
  // backend/tests/tools/fusion-test/bench/*.h  (test-only)
  namespace fusion::bench {
  struct Sample { double wall_s, cpu_s; std::uint64_t peak_rss_bytes; };
  struct Machine { std::string cpu, os, compiler, build, commit, z3; int threads; };
  struct Result { std::string case_id; std::vector<Sample> samples; Sample median, min; double load_1m; };
  Result measure(const std::filesystem::path& exe, std::span<const std::string> args,
                 int warmups = 1, int repeats = 5);                 // one child process per sample
  void append(const std::filesystem::path& history, const Machine&, const Result&);
  bool regressed(const std::filesystem::path& history, const Machine&, const Result&,
                 double factor = 1.25);                              // against the last 5 entries
  }  // namespace fusion::bench
  ```
- **History.** Results are appended as JSON lines to `bench/history.jsonl` in the data root, keyed by a machine fingerprint (values illustrative):

  ```json
  {"time":"2026-12-01T10:12:03Z","machine":{"cpu":"Intel Xeon Gold 6140","threads":36,"os":"Ubuntu 24.04","compiler":"gcc 13.3","build":"Release"},"commit":"1a2b3c4","z3":"5.3.3","case":"merge-167","repeats":5,"wall_s":{"median":0.41,"min":0.39},"cpu_s":{"median":0.40},"peak_rss_mb":112,"load_1m":0.4}
  ```

- **Flags.** A case is flagged when its median exceeds 1.25 times the median of the last 5 entries for the same machine. Budgets in `bench/budgets.json` come from the module sections and are checked separately.
- **Quiet machine.** The harness records the load average and warns above 1.0, because the workstation is shared. Benchmarks on the workstation, like T3 and Phase 3's long experiments, are scheduled with the user on a quiet machine, and never started beside the user's own runs without asking (prompt log entry 209).

### Phase 3 experiments

Seven experiments run (12.C15, prompt log entry 208), about 30 h of wall time on the workstation, scheduled with the user on a quiet machine (entry 209):

| Id | Open question | Method | Cost | Informs |
|---|---|---|---|---|
| X02 | stage 2, 1 and stage 4, 5: graph 83 against graph 84 | copy of the reference batch with `fb_rch` and `pathz3` on 83; compare corridors and frontiers | about 25 min | whether S2.26's normalisation must copy every re-read effect; entries |
| X03 | stage 2, 4: each id in one node in FuSION 1's merges | 100 random lists of 10 to 60 of the 167 KGMLs, two orders each, through `mff` and `wgx`; check the written XML | about 20 min | comparability of FuSION 1's merges; an entry if violated |
| X05 | stage 4, 3: 57 or 58 ids change node | count first claims in the full run's captured corridor | seconds after capture | XD-08's prediction |
| X06 | stage 5, 1: what `:solver2_timeout` and `:timeout` do | run the full run's 2.9 MB dump with `z3 -v:10 -st` and small values of both timeouts; read 4.8.12's combined-solver source | about 10 min | section 06's per-query timeout |
| X07 | stage 5, 3: are points beside unknown answers optimal | decide the 26 undecided queries of the 24 valid runs from their dumps (46.3 MB), each fresh, with seed 42 set (smallest-e dumps lack it, S5.22), under a 24 h cap: Z3 4.8.12, then the pinned 5.3.3 once it is installed (11.C2, entry 173) | 24 h wall per version, 26 workers | certifies reference frontiers, turning up to 23 inconclusive verification points into decided ones; extends section 11's 47-dump comparison of 4.8.12 and 4.15.3 to the hardest queries |
| X08 | stage 5, 6: does the seed reach the solver | fdr:0.3 (738 s) with seeds 42 and 7; compare limits files and witnesses | about 25 min | section 06's seed parameters; determinism tests |
| X11 | section 11's Z3 gate (11.C2, cross-section item 3A.26), not a spec question | run the real dumps through Z3 4.8.12, 4.16.0 and 5.3.3, in both of 11's modes (a lone `check-sat`, and with `(push 1)` first), with a 600 s cap per check. The dumps are the 47 of section 11's comparison (the reference run's 15, fdr050's 32) and X07's 26. It also repeats section 06's 4.8.12 measurements on 5.3.3: the combined solver's fallback, the reason read before `pop`, the reason for an exhausted `rlimit`, and the incremental speed-up | up to 73 × 3 × 2 checks of 600 s, about 73 h of check time; about 5 h on 16 workers | whether 5.3.3 is slower than 4.8.12 at the median, in which case the user re-decides 11.C2 before Phase 4 (entry 173) |

Four measurements accepted in the polls run beside them (entries 208 and 210), a few hours more:

| Id | From | Method | Cost | Informs |
|---|---|---|---|---|
| M01 | 05 open question 2 (entry 170) | a capped exact pruning to simple paths in C++, keeping a node whenever the cap is hit, on section 05's 29 configurations: the nodes and edges it removes, the observed genes among them (IRS2 in the reference run) and the time against the cap | at most the cap per configuration | the user's decision on whether 2.0 prunes; until then the walk rule stands, in 05 and in the test kit's `walk_corridor` |
| M02 | 06.C5 (entry 172) | replay the searches of the T2 cases on their captured corridors with a prototype of 06's search, and count the queries that models' own counts answer without Z3, against the 25 of the snapshot's 2,540 checks (13 s) that monotone inference from exact answers alone would answer | about 40 min, the T2 cases' solve time | the cache's real gain |
| M03 | 07 open question 7 (entry 177) | load the full JUN run's 13 FuSION 1 explanation pages with vis-network 9.1.2 and with the pinned 10.x in a headless browser, and compare node positions and screenshots | under 1 h | whether 9.1.2 replaces 10.x (07.C3, 11.C11) |
| M04 | 06 open question 6 (entry 210) | X11's 73 dumps with both relaxation counters rewritten as a balanced adder tree and as a sorting network, run on 5.3.3 as X11 runs them, against the adder chains | up to 73 × 2 checks of 600 s, about 24 h of check time; about 1.5 h on 16 workers | any change of encoding comes back to the user as a decision |

Four experiments are not run, because they only document FuSION 1's behaviour (entry 208):

| Id | Open question | Method, if wanted later | Why 2.0 does not need it |
|---|---|---|---|
| X01 | stage 1, 3: `fusion -d`, `-b` with a missing file, no `exit` | the snapshot binary with `< /dev/null` under `timeout 10`; under 1 min | the spec says no 2.0 decision depends on it |
| X04 | stage 2, 6: is the `final_rep_id` fix output-preserving | build git `224e817` and the snapshot; diff `wgx` output for the 163- and 42-pathway lists; about 15 min | the reference is the fixed snapshot |
| X09 | stage 6, 2: does psp's SVG step work | `psp` on the full run with `dot` from the `graphviz` conda environment on `PATH`; under 1 min | S6.26 replaces psp |
| X10 | stage 8, 4: does the backend die when the GUI exits | fdr:0.3's batch with stdout into a pipe whose reader exits; about 15 min | 2.0's Stop works differently (A1), and 10.C16's test covers 2.0 |

Stage 7 question 2 (whole results on a Mac) is answered by the cross-platform tests, not by FuSION 1. X11 needs Z3 4.16.0 and 5.3.3 installed on the workstation; only 4.8.12 (the system package) and 4.15.3 are there now. X11 must finish before Phase 4 starts.

Phase 4 checks recorded with section 11 (entry 205), each run once on a real machine:
- one plot on Windows, to see whether the official Windows gnuplot honours `PANGOCAIRO_BACKEND=fc` and `FONTCONFIG_FILE`; otherwise the font is installed per user (11 open question 5);
- busybox-w32's `date +%N` and its `bash` applet, once on Windows (11 open question 7, with 01's open question 6);
- `objdump -T` on OpenJFX 21.0.13's Linux libraries, against the glibc 2.28 floor (11 open question 9);
- the cairo-only bundled gnuplot accepts `set term png`, which 71 of the corpus's `exec gnuplot` lines use (11.C20, entry 204).

11's open question 6, Z3 5.3.3's speed on FuSION's dumps, is X11.

Capture work in Phase 3:
1. Copy the data root and check the manifest; copy the snapshot binary; rerun five fast cases to confirm the binary.
2. Capture batches for all 39 KEGG cases (about 30 min).
3. Regenerate the 12 toy cases (seconds, on relabelled copies, from batch files rebuilt from the GUI template), the 15 EGF-HeLa b6 cases (about 12 min: 724 s of solves) and the model example.
4. Record the gseapy references (about 2 min) and FuSION 1's `suggest_thresholds.py` report on the EGF file.
5. Run FuSION 1's merge on the synthetic merge fixtures (about 150 runs).
6. Copy the batch corpus into `backend/tests/fixtures/fusion1-batches/`.

### Frontend contracts

- **Schemas.** `backend/tests/schemas/` holds the one copy of the JSON Schemas, each written by its format's owner (cross-section item 2A.10, prompt log entry 158): 09's `run.json` and `events.jsonl`, 07's `results.json`, and 01's output of `fusion --commands` and `fusion --check --json`, the latter 01's diagnostics with 09's fields (1.9). The backend's tests check every such file they produce against them. The frontend's JUnit tests validate the T1 golden files against the same schemas, so both sides test one contract.
- **Batch generation.** Every pipeline the frontend can generate (the default pipeline, and each block alone) is passed to `fusion -b <file> --check --json` (section 01, decision 0021) in a cross-language test. A second test keeps the two sweep forms in step: the plain batches that Submit makes, one per swept value, equal the points of the exported sweep file (10.C21, entry 187).
- **Directory names.** One table of labels and their reduced directory names is read by the C++ test of 09's rule and by the frontend's JUnit test, so the two copies of 1.26's rule agree (entry 158).
- **Runs outlive the frontend.** The frontend starts a run through its runner and exits; the run goes on, and after a restart the frontend reattaches through `run.lock` and the run stays resumable (10.C16, 09.C9; entry 185). This test closes open question 8.
- **How the frontend's tests run** (section 10's desktop design, entry 209): the runner against `fake-fusion`; the four modules other than `ui` (accounts, sessions, pipeline and runner) without a display; the JavaFX screens under Xvfb on Linux CI.
- **`fake-fusion`.** Section 10's process tests launch this stand-in in place of `fusion`, on all three platforms. It is a small C++ program in `backend/tests/tools/fake-fusion/`, built with the tests and never shipped. It links `core` and `run`, so the `run.json`, `events.jsonl` and `run.lock` it writes are the real formats, with dotted event names (1.3), and they pass the schemas. A JSON-lines script, given as its batch file, tells it what to do:

  ```json
  {"after_ms": 0, "event": {"type": "step.started", "step": "pathz3"}}
  {"after_ms": 0, "spawn_child": true}
  {"after_ms": 100, "event": {"type": "solve.check.started", "step": "pathz3", "case": "in", "check": 1, "n": 0, "e": 4}}
  {"after_ms": 500, "on_stop": {"action": "honour", "within_ms": 200}}
  {"after_ms": 3000, "exit": 0}
  ```

  That covers section 10's cases: following a run, a stop honoured within the grace period or ignored (to force 09.C7's exit), a child process that a Kill must also end, scripted exit statuses (0, 1, 2, 3 and 130) and crashes, and a held `run.lock` for reattachment. On Windows the child is an ordinary child process; ending the whole tree is the runner's job (section 10).

### Files, settings, events and errors

- **Run directories:** this section writes none. The e2e driver and the runner create 2.0 run directories through `fusion` in scratch space and delete them once their case passes.
- **Other files:** the reference report (`report.json`, `summary.tsv`), the benchmark history (`bench/history.jsonl`), and the e2e goldens and `seen_in` lists that `--update` and `--record-seen` rewrite for review.
- **Settings:** it adds none. Tests that need unknown answers set section 06's `rlimit`, registry key `solve.rlimit`: Z3's resource limit per check, 0 meaning none, recorded in `run.json` (cross-section item 2B.8, prompt log entry 176; 12.C12).
- **Events:** it emits none, and reads `events.jsonl`, whose names are dotted (`run.started`, `step.skipped`, `solve.check.started`; 1.3).
- **Outcomes:** pass, fail, skipped (no data root, or a hash mismatch), inconclusive (unknown answers) and stale (an expected difference no longer seen).
- **`fusion-test` exit codes:**
  - 0: no failure;
  - 1: an unexpected difference, a failed prediction, a stale entry or a golden mismatch;
  - 2: the harness could not run (unreadable `cases.json` or manifest);
  - 4: everything skipped. CTest registers 4 as the skip code.
- **2.0's own statuses as test assertions:**
  - a failure (S1.28) is asserted by status `failed`, exit 1, error code, batch line and message (09.C14, 01.C14);
  - a reported result (`corridor.empty`, `gsea.empty_selection` or `observations.none`; cross-section item 1.8, entry 167) must skip the point's later lines with `step.skipped` events, end the point `ok` with exit 0, and appear in `step.finished`'s `report` and in `run.json`'s `reports` (01.C15);
  - a stop is asserted by status `stopped` and exit 3;
  - in a reference case, a failure exit is a test failure unless an entry allows it.

## Choices

### 12.C1 · Test tiers, their budgets and where they run
- **Proposed:** five tiers:
  - T0 unit and T1 end to end: repository data only; every local build; in CI on Linux for every push, and on macOS (arm64 and x86-64) and Windows on pull requests into `main`, weekly and on demand (11.C15, prompt log entry 203); 1 and 5 min per platform;
  - T2 fast reference: data root, workstation, before committing a change to `merge`, `corridor`, `solve` or `enrichment`; 15 min;
  - T3 full reference by verification: before a release and after encoding, corridor or merge changes, scheduled with the user on a quiet machine (entry 209); 24 h;
  - B benchmarks: on demand.

  Plus a once-off golden search at the end of Phase 4, and a weekly ThreadSanitizer run beside section 11's per-push AddressSanitizer and UndefinedBehaviorSanitizer build.
- **Alternatives:**
  - One suite, run in full before every commit.
  - Reference tests only at the end of each phase.
- **Why:** FuSION 1's solve times over the 24 valid runs range from 0.75 s to 187,623 s: five under 40 s, five from 615 s to 4140 s, fourteen above 11,000 s. T3 alone needs about 12 h on 16 workers. Running everything per commit would stall all work for a day; running the reference only at phase ends would let a merge or encoding regression go unseen for weeks. KEGG data exists only on the workstation (12.C3), so T2 and T3 cannot run in hosted CI. The macOS and Windows cadence is 11.C15's, which follows section 11's billing measurement: macOS minutes cost $0.062 against $0.006 on Linux, 10.3 times as much.
- **Decision:** as proposed (prompt log entry 206)

### 12.C2 · Test layout, executables and conventions
- **Proposed:** a `backend/tests/` tree, where section 11's layout puts the backend's tests, with:
  - a test-only `fusion_testkit` library;
  - one Catch2 executable per module, registered with `catch_discover_tests`;
  - tags turned into CTest labels for tiers, plus spec-item tags (`[S2.22]`) and a traceability test over the 209 items (when it fails is 12.C17);
  - one `fusion-test` driver with `data`, `e2e`, `ref` and `bench` subcommands;
  - `fake-fusion` in `tools/`, and one copy of the JSON Schemas in `schemas/`, each written by its format's owner and also read by the frontend's tests (cross-section item 2A.10, prompt log entry 158). Together they close open question 2 (entry 206).
- **Alternatives:**
  - One test executable for everything.
  - Test files beside their sources (`backend/modules/merge/src/merge_test.cpp`).
- **Why:** `catch_discover_tests` runs each test case in its own process, so crash isolation does not depend on the split; per-module executables mean that editing `merge`'s tests relinks only `test_merge`. A separate tree keeps test-only code, and the oracles that must not share code with the modules, out of the module sources. Spec tags make "every decided item has a test" checkable over 209 items instead of by memory. Section 10 expects `fake-fusion` and the schemas from this section; one copy of the schemas stops the two sides drifting apart.
- **Decision:** as proposed (prompt log entry 206)

### 12.C3 · Test data under the KEGG licence
- **Proposed:** the repository holds only synthetic data and files without KEGG content:
  - fixtures, including FuSION 1's toy network relabelled;
  - FuSION 1's outputs on synthetic inputs;
  - the batch corpus (1.30 MB).

  Everything derived from KEGG lives in a data root outside the repository, found through `FUSION_TEST_DATA` and hash-checked against `backend/tests/reference/data-manifest.json`. That covers the KGMLs, the old master, the model example's 479-relation reduced master, which counts as KGML data under S8.31 (open question 4, closed in prompt log entry 206), and FuSION 1's outputs on KEGG data. Tests that need the root are skipped, with the reason, when it is missing or differs.
- **Alternatives:**
  - Download KGMLs from KEGG's REST service at test time.
  - Keep the data in a private repository or submodule.
- **Why:**
  - S8.31 keeps KGMLs out of the repository. A reduced master built from KEGG relations is the same kind of data.
  - Downloads cannot reproduce FuSION 1's inputs: its KGMLs carry an `experiment_score` stamped by `add_experiment_score.py` (S2.06). KEGG's current files also differ from the 2026-03-17 copies.
  - A private repository is still a hosted copy of KEGG data.
  - The root is small (an estimated 150 to 200 MB) and is built in one command from the FuSION 1 tree.
  - Keeping the batch corpus in the repository lets CI parse it on Windows and macOS.
- **Decision:** as proposed (prompt log entry 206)

### 12.C4 · Where FuSION 1's reference outputs come from
- **Proposed:** a hybrid:
  - reuse the 24 runs recorded on or after 2026-07-16;
  - regenerate the toy cases, the 15 EGF-HeLa b6 cases and the model example with the snapshot binary;
  - add capture batches (no `pathz3`) that write each case's re-read graph and corridors with `wgx`.
- **Alternatives:**
  - Regenerate every case with the snapshot binary.
  - Use only the recorded outputs.
- **Why:**
  - The 24 runs hold 1,241,580 s (344.9 h) of solve time. Regenerating them costs as much again, and takes at least 52.1 h of wall time even with every run in parallel.
  - Their builds match the snapshot in results, per FuSION 1's changelog; five reruns in Phase 3 confirm it.
  - Older recorded runs differ: indirect relations kept before 2026-06-23 (`Linux/src/XMLParser.cc:1481-1490` applies to tool-written reads too), `experiment_score` lost before 2026-07-09.
  - Recorded outputs alone lack corridors (S4.21), which the pinned solve and the checker need. A capture takes at most about a minute per case.
- **Decision:** as proposed (prompt log entry 206)

### 12.C5 · How results are compared with FuSION 1
- **Proposed:**
  - canonical fact sets per stage;
  - two modes, chained (the batch through `fusion`) and pinned (each stage at library level on FuSION 1's recorded input);
  - frontiers compared exactly on definite answers;
  - witnesses judged by an independent checker under `fusion1` and `fusion2` semantics, with rules R1 to R4; `fusion2` follows the polls' answers (05.C2's trimming, poll 170's complex rule, 06.C10's cases, 06.C16's endpoints and 06.C20's frontier);
  - unknown answers reported as inconclusive.
- **Alternatives:**
  - Textual diffs of output files.
  - Frontier-only comparison.
  - Witness equality.
- **Why:**
  - E1 changes every output format, so textual diffs fail on every case.
  - Witness equality fails on correct runs: one point of `r5` was answered in 0.76 s, timed out at 4 h, and took 442.6 s, depending on the solver's history.
  - Frontier-only comparison accepts invalid witnesses and cannot explain a moved point. The re-scoring rule explains the 6 points that XD-07 moves and the 44 that XD-17 moves: it gives an upper bound that 2.0's frontier must meet, not the frontier itself.
  - Pinning localises the cascades of XD-01, which can change every downstream stage of the 18 GSEA cases when chained.
  - The checker is validated on real data: all 177 witnessed FuSION 1 points have counts equal to their point.
- **Decision:** as proposed (prompt log entry 207)

### 12.C6 · The recorded list of expected differences
- **Proposed:** `backend/tests/reference/expected-differences.json` in the repository. Each entry has an id, spec items, a reason, an applicability predicate, the facts it allows, an optional prediction, and the cases where it is seen. The runner fails on a difference no applicable entry allows, on a failed prediction and on a stale entry. There are 27 initial entries: 16 from the spec's decisions, XD-15 from G1 with 11.C2, and XD-19 to XD-28 for the changes accepted in polls 158 to 205, as the answer to this choice asked (prompt log entry 207). XD-02 went with 08.C1's exact port (entry 159).
- **Alternatives:**
  - A free-text list checked by hand.
  - Per-case golden overrides with no stated reason.
  - The same structure in TOML or YAML.
- **Why:**
  - Decision 0023 asks for each difference to be recorded. A machine-checked list stops one entry from masking an unrelated regression in another case.
  - Stale detection keeps the list true as 2.0 changes.
  - JSON uses nlohmann/json, already a dependency (G1); TOML or YAML would add a parser for the sake of comments, which the `why` field replaces.
- **Decision:** as proposed (prompt log entry 207)

### 12.C7 · The slow tier by frontier verification
- **Proposed:** verify each slow case's reference frontier of k points with at most 2k + 1 independent single-point checks through 06.C9's `points=`, run in parallel worker processes. Run a full search only when verification finds a difference, and once to record 2.0's own frontiers.
- **Alternatives:** re-run each case's full Pareto search every time.
- **Why:** over the 24 valid runs, verification needs 360 checks against the search's 703, and 702,742 s of check time against 1,057,265 s. The checks are independent, so the wall time falls from the 41.6 h of the slowest sequential search to about 12 h on 16 workers, never below the 4 h cap. It uses 06.C9's `points=`, decision 0019's single-point check, which 2.0 has anyway. Each check starts cold, the known cost of 06's open question 5 (entry 210), so a check that FuSION 1 answered warm can take longer or time out; such a point is inconclusive, not failed.
- **Decision:** as proposed (prompt log entry 207)

### 12.C8 · Language of the reference runner and drivers
- **Proposed:** C++, in `fusion-test`, linking `libfusion` and the test kit. gseapy values are recorded once in Phase 3, so no Python runs at test time.
- **Alternatives:**
  - Python scripts.
  - CMake scripts driving CTest.
- **Why:** pinned runs must call the library stage by stage (B1); Python would need bindings to `libfusion`, or a CLI path for every pinned input. One C++ toolchain already exists on all three platforms (G3), while F1 removed Python from the product. CMake scripts cannot compare canonical graphs or run the checker.
- **Decision:** as proposed (prompt log entry 207)

### 12.C9 · Property-based testing
- **Proposed:** the test kit's own seeded generators on `core::Rng`, iterated by Catch2. `core::Rng` is 09.C15's: `std::mt19937_64` with `core`'s own bounded integers, uniform doubles and shuffle, and SplitMix64 streams (cross-section item 3A.17, accepted with this choice in prompt log entry 186). A failing seed is printed, and the failing input is saved under the test's scratch directory, so it can be added as a fixture. No shrinking. The seed defaults to a fixed value; `FUSION_TEST_SEED` changes it for the weekly runs.
- **Alternatives:**
  - RapidCheck, with shrinking and a Catch2 integration.
  - Hand-written example tables only.
- **Why:** the generators are size-bounded (at most 12 nodes for corridors, 10 ids for exhaustive merges, 20 × 20 boxes for the search), so failing cases are already small. `core`'s RNG gives the same inputs on every platform. RapidCheck would be a further pinned dependency to build on three platforms. Tables alone would miss the interactions that S2.17 and S2.22 depend on.
- **Decision:** as proposed (prompt log entry 186)

### 12.C10 · The statistical agreement test with gseapy (lapsed)
- **Lapsed.** 08.C1's exact port was accepted, amending skeleton Choice F1 (prompt log entry 159), and that answer records that this choice lapses: the GSEA test is exact equality as doubles (see "GSEA agreement with gseapy"). The lapsed proposal stays below as the record of what was replaced.
- **Proposed (lapsed):** test F1's "statistically but not byte for byte" in three tiers, against gseapy 1.3.1 run with the gene-id tie order:
  - ES, leading edge, `Tag %` and `Gene %`, which use no random numbers, agree within 1 × 10⁻¹²;
  - NES, nominal p, FDR and FWER agree within 5 standard errors, both as 20-seed means and against a 100,000-permutation run at seed 42;
  - selections agree, except for pathways whose FDR lies within 5 standard errors of the cut-off.
- **Alternatives:**
  - Compare only shortlists.
  - A tighter band, 3 standard errors.
- **Why:**
  - S7.10 reproduced ES within 9 × 10⁻¹⁶, so the first tier can be exact in all but rounding.
  - Over seeds 1 to 20, gseapy itself selects 41 to 46 pathways at fdr:0.25 (S7.11), so selection differences inside the noise band are not bugs.
  - At 5 standard errors the two-sided false-alarm chance is 5.7 × 10⁻⁷ per comparison, about 4 × 10⁻⁴ over 664 comparisons (166 sets, 4 statistics). At 3 it is 2.7 × 10⁻³ per comparison, so most runs of the suite would see at least one false alarm.
  - Shortlists alone would miss a wrong NES normalisation that leaves selections unchanged.
  - The band's weakness is real: between seeds, NES moved by up to 0.082 and FDR q by up to 0.373 (section 08), so a porting error of that size passes. That is the case for 08.C1.
- **Decision:** lapsed: 08.C1's exact port makes the agreement test exact (prompt log entry 159)

### 12.C11 · One golden set for all three platforms
- **Proposed:** byte-identical canonical outputs on Linux, macOS and Windows for every deterministic stage, for the SMT-LIB text of every query, and for frontiers. Witnesses and times are excluded. This is cross-section item 1.18's one promise, accepted with 09.C16 and 11.C7 (prompt log entry 186). It rests on:
  - floating-point contraction turned off, with 11.C7's flags;
  - every result path avoiding the platform's `exp`, `log` and `pow`, or using one correctly rounded implementation (09.C16);
  - `core`'s own RNG distributions (09.C15);
  - locale-free number parsing and printing;
  - LF-only output.
- **Alternatives:**
  - Goldens per platform.
  - Agreement within a tolerance across platforms, the promise 1.18 set against this one.
- **Why:** S7.05 showed one library's sort producing different shortlists on different CPUs. FMA contraction differs between arm64 and x86-64 builds, `<random>` distributions differ between the three standard libraries, and maths libraries differ in the last bit, so a pathway exactly at the FDR cut-off could flip. Goldens per platform would hide exactly the drift that G3 must prevent, and would triple every review. A tolerance would need a band for every number, and would still let a selection at the cut-off differ.
- **Decision:** as proposed (prompt log entry 186)

### 12.C12 · Deterministic solver budgets in tests
- **Proposed:** tests that need unknown answers use a per-check Z3 resource limit, or `ScriptedOracle`; no test asserts on wall-clock timeouts. Section 06 adds the limit as `rlimit` (cross-section item 2B.8, accepted with this choice in prompt log entry 176): Z3's resource limit per check, an unsigned count of Z3's own steps, 0 by default meaning none, registry key `solve.rlimit` (1.16), recorded in `run.json`, and changeable on resume like the timeouts. Wall-clock timeouts stay the default for real runs. Phase 3 checks that one limit gives the same answers on all three platforms. The same answer closes section 10's open question 4.
- **Alternatives:**
  - Small wall-clock timeouts.
  - Keep unknown answers out of end-to-end tests.
- **Why:** under a wall-clock cap, the answer depends on machine speed and load; this workstation ran up to 18 FuSION 1 jobs at once while this section was drafted. S5.19's flags and S5.20's cache must still be tested end to end with real Z3. A resource limit counts Z3's own steps, which do not depend on the clock.
- **Decision:** as proposed (prompt log entry 176)

### 12.C13 · Benchmark harness
- **Proposed:** `fusion-test bench`:
  - real-size cases from the data root, plus synthetic-scale cases in CI;
  - each case in its own child process, median of 5 runs after a warm-up;
  - wall time, CPU time and peak memory;
  - a JSON-lines history per machine, flagged at 1.25 times the median of the last 5 entries;
  - budgets from the module sections checked separately;
  - runs on the workstation scheduled with the user on a quiet machine (prompt log entry 209).
- **Alternatives:**
  - Google Benchmark.
  - Catch2's `BENCHMARK` alone.
- **Why:** the costs that matter are whole steps on real inputs: merges of 3 to 126 s, GSEA at 318 MB, 23.8 s of closure, 1.1 to 2.0 GB per slow b10 run. Peak memory per step needs a separate process, measured with `wait4` (see "Platforms"). Google Benchmark is a further dependency built for micro-benchmarks in one process, and Catch2's `BENCHMARK` keeps neither history nor memory.
- **Decision:** as proposed (prompt log entry 208)

### 12.C14 · Oracles for S2.22's equivalence
- **Proposed:** two oracles. Section 03's `merge_direct` is the first, as section 03 designs it. The test kit adds a second, `exception_merge`, written from S2.07 to S2.21 with its own KGML reading and no code shared with `merge` or `network` (about 300 lines). Both are checked in three layers:
  - against every exception subset of small synthetic sets;
  - against 1000 random cases per run, and 100,000 in the weekly run;
  - on the 7 real merges, cross-checked against FuSION 1's merge on synthetic sets and real lists under XD-03 to XD-06 and XD-16.

  This reads S2.22's "merging with the exceptions" as 2.0's own exception-aware semantics, which closes open question 5 (prompt log entry 208).
- **Alternatives:**
  - `merge_direct` alone, as section 03 proposes.
  - FuSION 1's merge as the only oracle.
- **Why:**
  - `split` recomputes the affected part with the code `merge_direct` runs, and both read `network`'s pathway records (section 03). A bug in that shared code would make both wrong in the same way, and the comparison would pass. The independent oracle catches that class of bug.
  - FuSION 1 can check only the inputs it is run on, and it differs from 2.0 by five fixes, so on its own it cannot show equivalence across exception sets.
  - A small oracle checks all 1024 subsets of a 10-id universe in milliseconds, and costs about 300 lines of test-only code.
- **Decision:** as proposed (prompt log entry 208)

### 12.C15 · Which Phase 3 experiments to run
- **Proposed:** run X02, X03, X05, X06, X07, X08 and X11, which inform 2.0's design, certify the references or gate the Z3 pin: about 30 h of wall time on the workstation. Most of it is X07's run with Z3 4.8.12, which occupies up to 26 threads for at most 24 h, and X11's, about 5 h on 16 workers. X11 must finish before Phase 4 starts (11.C2, prompt log entry 173). Run X07's second pass with the pinned Z3 5.3.3 once it is installed; XD-15 applies to every case. X01, X04, X09 and X10 (about 30 min in all) only document FuSION 1's behaviour and are not run. Beside the seven run the measurements accepted in polls 170, 172 and 177 (M01 to M03) and 06's counter-encoding comparison (M04, entry 210), and section 11's Phase 4 checks of poll 205 are recorded with them (entry 208).
- **Alternatives:**
  - Run all eleven.
  - Run only the cheap ones and X11, and skip X07.
- **Why:** the spec says no 2.0 decision depends on X01; X04 and X09 concern code that 2.0 replaces; X10's FuSION 1 answer does not carry over, because 2.0's Stop works differently (A1). X07 turns up to 23 inconclusive verification points into decided ones, and the `r5` re-solve showed that a fresh check can settle in 7.4 min what timed out at 4 h. X11 is the measurement that section 11's pin waits for.
- **Decision:** as proposed (prompt log entry 208)

### 12.C16 · What the slow tier verifies against after the golden search
- **Proposed:** once the golden search at the end of Phase 4 has run and the user has reviewed its frontiers, T3 verifies each case against 2.0's own reviewed frontier. The comparison with FuSION 1 (R1 to R4 under the expected-differences list) then runs before each release, and whenever the list changes.
- **Alternatives:**
  - Always verify against FuSION 1's frontiers, under the expected-differences list.
  - Switch case by case, as each case's differences are explained.
- **Why:** against FuSION 1's frontiers, every case where a point moved fails verification at that point. That is 3 cases for XD-07, 4 for XD-17 (every one of their points), and with Z3 5.3.3 (XD-15) possibly more. Each failure triggers a full search, the slow path of 41.6 h for the slowest case. 2.0's reviewed frontiers make verification a plain pass or fail. The risk is a wrong frontier accepted in review, which the release-time FuSION 1 comparison would still catch.
- **Decision:** as proposed (prompt log entry 208)

### 12.C17 · When the traceability test starts to fail
- **Proposed:** the traceability test only reports during Phase 4. From the end of Phase 4 it fails for any spec item without a tagged test, except items listed with a reason in `backend/tests/traceability-exempt.txt`, at first the pure drops.
- **Alternatives:**
  - Fail from the start of Phase 3, so every item needs a test before its code exists.
  - Never fail; report only.
- **Why:** Phase 4 builds one module at a time (decision 0003), so failing earlier would keep the suite red for weeks and hide real failures. By the end of Phase 4 every module exists, and an untested decision is then a gap rather than work in progress. Of the 209 items, 16 have decisions that begin with "drop"; most leave nothing to test. Report-only for ever would let gaps stay unnoticed.
- **Decision:** as proposed (prompt log entry 209)

## Interfaces with other sections

What this section needs:
- **01 Batch:**
  - a library call that parses and validates a batch file, with file-existence checks optional (for the batch corpus);
  - `fusion -b <file> --check --json`, printing 01's diagnostics with 09's `command`, `setting` and `path` fields (cross-section item 1.9), and `fusion --commands`, with the schemas of both outputs (2A.10);
  - 09's statuses `ok`, `failed` and `stopped`, and the exit statuses 0, 1, 2, 3 and 130 (01.C14);
  - 01.C11's six fixed-name moves, written natively with the limits file in FuSION 1's rendering, and window-plot lines skipped when headless (entries 179 and 190), which chained mode relies on;
  - busybox-w32's `sh` for `exec` on Windows (01.C10).
- **02 Network:**
  - read access to member ids, types, components, edge subtypes, pathways and scores, for canonical forms;
  - the legacy reader usable on any unmarked FuSION 1-written file, reproducing graph 2 with exact duplicates merged (02.C5): re-read graphs and corridors;
  - error identifiers for each KGML rejection.
- **03 Merge:** `build_base_merge`, `split` and `merge_direct` callable separately at library level, as section 03 designs them; the report.
- **04 Expression:** a parser that returns per-line diagnostics; classification, with `observations.none` as a reported result (04.C5); threshold suggestions at library level.
- **05 Corridor:**
  - building from any graph and parameters, including a graph read from a FuSION 1 corridor file;
  - a corridor fingerprint, `corridor.json` and `observations_match.tsv` (05.C6, 05.C9);
  - the distances.
- **06 Solve:**
  - the `Oracle` interface under `pareto_search` (`Answer ask(Point, Phase)`, with 06's `Outcome` values), so `ScriptedOracle` can drive it;
  - single-point checks through 06.C9's `points=n,e;…`, also from a fresh context in a worker process, for verification;
  - `rlimit`, registry key `solve.rlimit` (cross-section item 2B.8, entry 176);
  - SMT-LIB text of each query under debug;
  - witnesses exported as full assignments (presence and activity of every present node, selected edges);
  - the structured search log;
  - stop and resume through a token at library level;
  - cases in parallel giving the same per-case answers and witnesses as `threads=1` (06.C4);
  - poll 170's complex rule for every avoided gene, avoid lists included (its open question 10, closed in entry 229), which sets XD-19's `applies_when`.
- **07 Results:** the schema of `results.json` (2A.10); FuSION 1 text writers under `legacy_prefix` (1.21); the certification flag on each point; case-named files (1.6).
- **08 Enrichment:**
  - the exact port of gseapy 1.3.1's stream (08.C1, entry 159), which sets the GSEA test;
  - section 08's gseapy harness, reused to record the references;
  - 08.C7's rule for all-zero draws, and 08.C9's order for equal \|NES\|;
  - null scores available under debug.
- **09 Runs:**
  - the run-directory root settable by tests;
  - the list of volatile `run.json` and event fields, and the schemas of `run.json` and `events.jsonl` (2A.10);
  - an event sink that tests can capture;
  - the saved-step registry API;
  - the stop-request API;
  - `core::Failure`, `core::Outcome<T>` and `core::Stopped` (09.C14);
  - `core::Rng` and number formatting, platform-independent (09.C15, 09.C16).
- **10 Frontend:** JUnit tests that validate the T1 goldens against `backend/tests/schemas/`; its process tests run against this section's `fake-fusion`, found through a build property that points at the built binary; the four modules other than `ui` tested without a display and the screens under Xvfb (entry 209); the cross-language tests of batch generation, sweep forms (10.C21) and directory names (1.26); reattachment after the frontend exits (10.C16).
- **11 Build:**
  - `backend/tests/` and `frontend/src/test/` in the layout, with `backend/tests/schemas/` read by both sides;
  - `fake-fusion` built with the backend's tests, before the frontend's Gradle tests run;
  - Catch2 3.16.1 with CTest labels;
  - the per-push sanitizer build, plus a weekly ThreadSanitizer run;
  - 11.C7's flags with no floating-point contraction, fast_float (11.C5), and the macOS 13.3 deployment target;
  - section 11's `.gitattributes`;
  - 11.C15's workflows, which 12.C1's cadence follows;
  - `FUSION_TEST_DATA` passed to tests, and `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT` honoured and recorded (11.C20);
  - Z3 4.16.0 and 5.3.3 installed on the workstation for X11, and the pinned 5.3.3 for X07's second pass and T3;
  - the Phase 4 checks of entry 205.

What this section gives: the test kit (data root, scratch directories, generators, canonical forms, FuSION 1 readers, checker, oracles, normaliser), the tag and label conventions, `fusion-test`, `fake-fusion`, the one copy of the JSON Schemas, the data manifest, the reference cases, the expected-differences list, X11's measurements for 11.C2's gate, and M01 to M04's measurements for sections 05, 06 and 07.

## Test hooks

How the test architecture itself is tested:
- The checker under `fusion1` semantics must accept all 177 witnessed FuSION 1 points of the 24 valid runs, with costs equal to their points. It must reject mutated witnesses (one edge deselected, one activity flipped). Under `fusion2` semantics it must reject `r5`'s out witness at (4, 4) for its [STAT1+STAT3] complex, both with STAT3 named as the FSC and with `r5`'s own avoid list (XD-19).
- Canonical forms: equal graphs built in different orders give equal text; every single-field mutation is detected by `diff`.
- The normaliser's mutation self-test (see "End-to-end tests").
- Generators: golden SHA-256 of generated fixtures for fixed seeds, identical on all three platforms.
- The runner on a synthetic case with injected differences must report:
  - an unexpected difference;
  - an allowed one;
  - a failed prediction;
  - a stale entry.
- `ScriptedOracle` and `exhaustive_frontier` must agree on random tiny cases.
- A data root with one corrupted file must skip the dependent tests with the hash reason, and never pass them.
- `fake-fusion`: every file it writes passes the schemas, and on each platform it honours or ignores a stop request as scripted, within the stated time.
- The benchmark harness: two children run one after the other report their own peak memory, not the larger of the two (the `wait4` rule).

## Open questions

1. **Old batch files and fixed-name outputs.** Closed in prompt log entry 209: 01.C11 writes the six fixed-name moves natively, amending S1.14 (entry 179); positional `pex` lines bind to their solve by prefix (07.C9, entry 179); window plots are skipped when headless (entry 190); the `exec bash …/plot_po.sh` lines run FuSION 1's script through the path map.
2. **Schema ownership.** Closed: this section keeps the one copy, and each format's owner writes its schema (cross-section item 2A.10, entry 158; 12.C2, entry 206).
3. **A setting for tests.** Closed: section 06 adds `rlimit` (cross-section item 2B.8, entry 176; closed in entry 209).
4. **The model example's reduced master.** Closed: it is KGML data under S8.31 and stays in the data root (12.C3, entry 206).
5. **S2.22's wording.** Closed: 2.0's own exception-aware semantics, with FuSION 1's merge as a cross-check (12.C14, entry 208).
6. **The workstation.** Closed: T3, Phase 3's long experiments and the benchmarks are scheduled with the user on a quiet machine, and never started beside the user's own runs without asking (entry 209).
7. **Z3 version.** Closed: Z3 5.3.3 is pinned, gated on X11 (11.C2, entry 173), so XD-15 applies to every case (closed in entry 209).
8. **Stage 8 question 4, in 2.0.** Closed by 10.C16: a run continues and stays resumable when the frontend exits, which the `run` tests and the frontend contracts check (entry 209).
9. **Exact or statistical GSEA agreement.** Closed: 08.C1's exact port was accepted, amending F1, so the test is exact equality and 12.C10 lapses (entry 159; closed in entry 209).
