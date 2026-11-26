# C6 · The benchmark harness and the traceability test

- **Status:** decided: Claude checked the note against section 12 and the code, and the user chose every open choice as proposed (prompt log entries 344 and 345)
- **Date:** 2026-11-26
- **Plan item:** C6 (`docs/phase3/plan.md`, Code table); P5 as amended in prompt log entry 330
- **Specification:** section 12, "Benchmarks" (`docs/architecture/12-tests.md:811-849`), 12.C13, 12.C17, "Repository layout" (lines 113, 118-119) and the harness's test hook (line 1218)

`fusion-test bench` is a subcommand of the C3 driver. Kit names are those of the C2 note.

## 1. What section 12 asks

- **The cases** of the table at lines 813-825, from KGML reading and merges to GSEA, the corridor, solve checks and the whole batch, plus synthetic-scale cases in CI.
- **The harness (lines 827-840; 12.C13):** each sample in its own child process; one warm-up, then the median and minimum of 5 runs; wall time, CPU time, and peak memory by `wait4` on the child (line 798). The sketch's `fusion::bench` types and functions.
- **History:** JSON lines in the data root's `bench/history.jsonl`, keyed by a machine fingerprint. A case is flagged above 1.25 times the median of its last 5 entries. Budgets from the module sections go in `bench/budgets.json` and are checked separately.
- **A quiet machine:** the load average is recorded, with a warning above 1.0; workstation runs are booked (prompt log entry 209).
- **Test hook:** two children run in turn each report their own peak (line 1218).
- **Traceability (lines 118-119; 12.C17):** a test prints the spec items no test's tags cover. It only reports during Phase 4, and fails afterwards except for the items of `traceability-exempt.txt`.
- **P5 as amended** runs the benchmarks of sections 01, 02, 03 and 08 now, on Linux.

## 2. What exists now

- No harness, no `backend/tests/bench/`, no traceability test and no exempt list.
- `core::Process` already measures what section 12 asks of a child, except its CPU time. `ProcessResult` gives `wall` and `peak_rss_bytes` (`backend/modules/core/include/fusion/core/process.hpp:31-38`), the latter from `wait4` on the child's pid (`core/src/process_posix.cpp:207-221, 386-391`). The same `rusage` holds `ru_utime` and `ru_stime`, but `ProcessResult` does not return them.
- `run.json` records the run's `resources` (`wall_s`, `cpu_s`, `peak_rss_bytes`) and each step's `wall_s`. In the sprint's Debug run of `jun-b6-fdr025`: `gsea` 3.675 s, `mff` 0.779 s, `fb_rch` 0.036 s, `pathz3` 0.435 s, peak 54 MB. Its `versions.fusion.build_type` is `Debug`.
- `CMakePresets.json`'s `release` has tests off, so no Release `fusion-test` can be built today (C3.3 adds `ci-linux`).
- **Budgets in the module sections:**

  | Section | Budget | Where |
  |---|---|---|
  | 01 | lexing and expansion of the real batch under 1 ms; its full check, with the 167-file list, under 50 ms on warm caches | `01-batch-language.md:1265` |
  | 02 | all 273 KGMLs at threshold 15000, and the GUI run's legacy build (the old master at threshold 15000: 8,139 nodes, 82,188 edges, `02-network.md:379`), "within a stated time and peak memory … such as under 1 s and under 64 MB" | `02-network.md:1195` |
  | 03 | the 42-, 87-, 90-, 163- and 167-pathway lists, from scratch, from the store and split. No 2.0 budget is stated: FuSION 1's baselines, and an estimate of under 0.1 s from scratch and a few ms per split | `03-merge.md:845`, `:356` |
  | 04 | the read time of the probe table and the query time of the EGF file, recorded; no number | `04-expression.md:904` |
  | 05 | the corridor of the 82k graph in 10 ms | `05-corridor.md:592` |
  | 08 | the real inputs on one thread, with the check's KGML parse, at most 0.5 s; 10,000 permutations at most 3 s; peak memory at most 50 MB | `08-enrichment.md:714-717` |

- **Disagreements:**
  - Section 02's 273 KGMLs (`FuSION/updated_pathways/xml_files/`, 15 MB) are not in the data root, whose manifest has the 167 KGMLs.
  - Section 05's 82k graph is FuSION 1's file of the GUI run (`FuSION/runs/egf_hela_hsa1956_cxcl8_b6/merged_master_graph.xml`, 13.4 MB), also not in the root. 2.0's legacy read of the old master at threshold 15000 gives the same graph (`02-network.md:951`), and the old master is in the root.
  - Section 12's own table names neither input: it uses the 167 KGMLs and the full run's 3100-node graph.
- **Traceability now** (a source scan made for this note, `TEST_CASE` names and tags in `backend/tests/unit/`):
  - 209 items in `docs/spec/0*.md`, 16 of them decided "drop";
  - spec items written as tags cover 76; with the spec ids written in test names, 146;
  - 63 items are cited nowhere, 26 of them in stage 8 (the frontend's workflow) and 12 of them drops.
- **The convention is uneven:**
  - corridor, enrichment, merge and solve tag spec items (136 test cases);
  - network, expression, results, run, core and batch write them only in the name, as in `"… (S3.03)"` (104 test cases);
  - 283 test cases cite none;
  - batch and corridor carry no `[unit]` (97 test cases);
  - the frontend's 19 JUnit test files mention 74 spec items in comments and names, none as a JUnit `@Tag`.

## 3. Design

### Files

```
backend/tests/bench/cases.json            one entry per case
backend/tests/bench/budgets.json          one entry per budget, with its section and line
backend/tests/tools/fusion-test/bench/    measure.cpp, history.cpp, machine.cpp, child.cpp (C6.2)
backend/tests/traceability.cmake          the traceability test, a CMake script
backend/tests/traceability-exempt.txt     <item> <reason>, one per line
```

### Benchmark cases now (sections 01, 02, 03 and 08)

| Case | Input (data root unless noted) | Kind (C6.2) | Budget |
|---|---|---|---|
| `batch-lex`, `batch-check` | the reference run's batch, path-mapped as C4 prepares it, with its 167-file list | call | 1 ms; 50 ms |
| `kgml-read-167` | `kegg/`, the 167 candidates | call | none (12's row) |
| `network-273-t15000` | the 273 KGMLs (C6.1) | call | 1 s, 64 MB ("such as") |
| `legacy-read` | `legacy/merged_kegg_master_graph_with_exp_score.xml`, at threshold 1 (the full run's graph 2) | call | none; FuSION 1's about 1 s recorded as baseline (12's row) |
| `legacy-read-t15000` | the same file at threshold 15000 (the GUI run's 82k graph) | call | 1 s, 64 MB ("such as") |
| `merge-<n>-scratch`, `-store`, `-split` for n = 42, 87, 90, 163, 167 | the shortlists in `reference/<id>/f1/run/gsea_kgml_files.txt`, the 163 folder, the 167 list; the reference exception sets | call | none; FuSION 1's 3 s, 22 to 53 s, 24 s and 122 to 126 s recorded as baselines |
| `gsea-real`, `gsea-10k` | `expression/egf_hela_t120_lfc.tsv` and the 167 candidates, one thread | batch | 0.5 s and 50 MB; 3 s |
| `synthetic-1x`, `synthetic-4x` | `pathways()` at the old master's size and 4 times it (repository) | call | none; CI smoke run with 1 repeat, label `bench` |

The corridor cases (section 12's full-run graph, section 05's 82k graph with its 10 ms budget), and the encoding, solve-check, whole-batch and Z3 re-pin cases, come later: P5 names 01, 02, 03 and 08, and the solve's cases wait for the Z3 pin (X11, booking 2). The corridor cases need no new input.

### Harness

- **`measure(exe, args, warmups, repeats)`** as sketched. One `core::Process` per sample. Wall time is taken on the parent's steady clock, peak memory from `ProcessResult::peak_rss_bytes`, CPU time from C6.3.
- **Before each case,** `getloadavg()` is recorded in `Result::load_1m`, with a warning above 1.0.
- **Builds:** a build that is not Release is refused with exit 2, unless `--allow-debug` is given; then its results go to a history keyed by build type and no budget is judged.
- **`Machine`:** the CPU model, threads, OS, compiler and build type, and the Z3 version and commit from `fusion --version --json`. The fingerprint is its hash.
- **History:** `append` writes section 12's line to `$FUSION_TEST_DATA/bench/history.jsonl`. `regressed` compares the median with the last 5 entries of the same fingerprint and case.
- **Budgets:** checked after the samples; a missed budget is exit 1 and a reported finding.
- **Self-test:** the test hook, with a child that holds 200 MB, then one that holds 20 MB; each must report its own peak.

### Traceability

`traceability.cmake` runs as CTest test `traceability`, labelled `unit`. It:
1. reads the 209 ids from the headings `### S<stage>.<nn>` of `docs/spec/0*.md`;
2. runs every `test_<module> --list-tags` and `fusion-test e2e --list` (C3), and collects the spec-item tags;
3. removes the items in `traceability-exempt.txt`;
4. prints the uncovered items by stage.

It exits 0 while `FUSION_TRACEABILITY_STRICT` is `OFF`, and 1 for an uncovered item once it is `ON` (12.C17). The exempt list starts with the drops that leave nothing to test. Claude proposes which of the 16 those are, each with its spec decision as the reason, for the user's review.

## 4. Checks it runs now (P5 as amended)

| Check | Needs |
|---|---|
| the benchmarks of 01, 02, 03 and 08 above, a first history entry each, and the budgets judged | the `ci-linux` Release build (C3.3); the data root, verified; the 273 KGMLs (C6.1); a quiet machine, booked with the user. About 21 of the user's FuSION 1 jobs held the machine when Phase 3 resumed (load 24 on 36 cores, prompt log entry 328) |
| the traceability report | nothing; it prints 146 of 209 covered after C6.4's retag |

A missed budget comes back to the user as a poll, as the plan's Rules ask of findings.

## 5. Open choices

### C6.1 · The 273 KGMLs, which the data root lacks
- **Proposed:** copy them into the root (15 MB of KEGG data, outside the repository), with manifest entries, as W1 copied the rest.
- **Alternatives:**
  - Measure section 02's case on the 167 KGMLs at threshold 15000 (113,162 edges, `03-merge.md:357`), as section 12's table would suggest, and record the change against section 02's hook.
  - Leave the case out until someone asks.
- **Why:** the budget was set on that input. The 273 KGMLs make the largest plausible network (`02-network.md:193`), so it is the case the budget exists for.
- **Decision:** as proposed (prompt log entry 344)

### C6.2 · How a case runs in its child
- **Proposed:** two kinds. `batch` runs `fusion -b <case batch>`. `call` runs `fusion-test bench --child <case>`, a hidden mode that makes one library call N times in the child, prints its own timings, and lets the parent measure the process.
- **Alternatives:**
  - `batch` only, with step times taken from `run.json`.
  - `call` only.
- **Why:** budgets of 1 ms and 50 ms, and merges of tens of milliseconds, are smaller than a process start and a run directory. GSEA's budget includes the check's parse, which only the whole `fusion` path has.
- **Decision:** as proposed (prompt log entry 344)

### C6.3 · The child's CPU time
- **Proposed:** add `cpu_s` to `core::ProcessResult`, from the `rusage` that `wait4` already fills (POSIX), and later from `GetProcessTimes` on Windows. This is a few lines in `core`, which is section 11's module.
- **Alternatives:** no `core` change: `batch` cases read `run.json`'s `resources.cpu_s` and `call` children print `core::cpu_seconds()`.
- **Why:** `measure` stays the generic call section 12 sketches, and CPU comes from the same `wait4` as peak memory. The alternative leaves `core` untouched, at the cost of two ways to read one number.
- **Decision:** as proposed (prompt log entry 344)

### C6.4 · Spec items as tags
- **Proposed:** one mechanical retag commit. The spec ids that the 104 test names cite become tags too, with the names unchanged, and `[unit]` goes on the 97 batch and corridor test cases. Traceability then reads tags only.
- **Alternatives:**
  - The test also reads spec ids from test names (`--list-tests`), with no retag.
  - Tags only, with no retag, so the report starts at 76 covered.
- **Why:** section 12 makes spec items tags, and so CTest labels, which `ctest -L S2.22` selects. Names drift unseen, and the missing `[unit]` already makes `ctest -L unit` skip 97 tests (C3 note).
- **Decision:** as proposed (prompt log entry 345)

### C6.5 · Which tests count
- **Proposed:** the backend's Catch2 tags and the end-to-end cases' tags now. The frontend's JUnit tests join through `@Tag("S8.21")` when Phase 5 resumes them, so stage 8's items show as uncovered until then.
- **Alternatives:**
  - Scan the frontend's test sources for spec ids now.
  - Exempt stage 8 until Phase 5.
- **Why:** a tag is a declaration that a test covers an item; a comment is not. Exempting stage 8 would hide 26 items.
- **Decision:** as proposed (prompt log entry 345)

### C6.6 · The switch at the end of Phase 4
- **Proposed:** a CMake option, `FUSION_TRACEABILITY_STRICT`, `OFF` until the commit that ends Phase 4 turns it on.
- **Alternatives:** a date in the script; a flag line in the exempt list.
- **Why:** the end of Phase 4 is the user's review (decision 0003), not a date, and a reviewed commit is how it shows in the repository.
- **Decision:** as proposed (prompt log entry 345)

## 6. Cost

- **Size:** about 8 files and 1,100 lines: `measure`, history and `regressed` about 350, the child's calls about 300, the machine fingerprint and load about 100, `cases.json` and `budgets.json` about 150, the traceability script about 150, and the self-tests about 100. C6.3 adds a few lines to `core`. C6.4's retag changes about 200 `TEST_CASE` lines in 10 files.
- **Run time:** about 25 cases (15 of them the merge lists in three modes), each 1 warm-up and 5 samples of well under a few seconds in Release (section 03's and 08's estimates; GSEA at 10,000 permutations at most 3 s). About 5 to 10 min on a booked, quiet machine. The traceability test takes a few seconds in T0. The synthetic smoke cases take under a minute.
