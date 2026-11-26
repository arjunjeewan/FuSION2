# C2 · The test kit, `fusion_testkit`

- **Status:** decided: Claude checked the note against section 12 and the code, and the user chose every open choice as proposed (prompt log entry 340)
- **Date:** 2026-11-26
- **Plan item:** C2 (`docs/phase3/plan.md`, Code table); P5 as amended in prompt log entry 330
- **Specification:** section 12, "The test kit" (`docs/architecture/12-tests.md:193-371`), "Merge then split" (741-759), 12.C9, 12.C14 and "Test hooks" (1203-1218)

The interface below is the one that C3 to C6 use. They name its types and functions as written here.

## 1. What section 12 asks

- **A test-only static library,** linked by the tests and `fusion-test` only (12.C2), with the pieces of the sketch at lines 201-360: data root and scratch directories, seeded generators, canonical forms and `diff`, FuSION 1's readers, the witness checker with `fusion1` and `fusion2` semantics (600-616), the oracles `exception_merge`, `walk_corridor`, `exhaustive_frontier` and `ScriptedOracle`, and the end-to-end helpers.
- **Independent oracles (195-197; 12.C14):** the checker, the merge oracle and the readers share no code with the modules they check.
- **Its own data model (197):** plain structs, with adapters from module types "added in Phase 4 as each module lands". The modules exist now, so the adapters come with C2.
- **Property tests (12.C9):** generators on `core::Rng`, never `<random>`'s distributions; a failing seed is printed and its input saved; `FUSION_TEST_SEED` for the weekly run.
- **S2.22 (741-759):** `split`, `merge_direct` and `exception_merge` compared in three layers: bounded exhaustive (T0, about 50 sets over at most 10 ids, every exception subset), random (T0, 1000 sets over up to 200 ids, 20 permutations each) and real (T2).
- **Test hooks (1203-1218),** among them: the checker accepts FuSION 1's 177 witnessed points under `fusion1` and rejects `r5`'s out witness at (4, 4) under `fusion2`.

## 2. What exists now

No kit exists. No C++ test reads `FUSION_TEST_DATA` or `FUSION_TEST_SEED`, and no test carries `[kegg]` or `[slow]`. The sprint gave each suite its own helpers:

| Helper | Where | Overlaps section 12's | Differs how |
|---|---|---|---|
| witness checker, exhaustive frontier | `unit/solve/kit.cpp:175-351`, `kit.hpp:52-63` | `check`, `exhaustive_frontier` | takes `solve::Problem`, the output of `make_case` (`kit.hpp:48`), so it trusts `make_case` and `bind_inputs` (trimming, counted set, FSC cases, complex rule); `fusion2` only; enumerates 3^N · 2^E for N ≤ 7, E ≤ 9 |
| `ScriptedOracle` | `kit.hpp:85-98`, `kit.cpp:376-407` | `ScriptedOracle` | a superset: adds `truth()`, `model`, `asked`, `witnesses`, which the 06.C5 and 06.C20 tests use |
| FuSION 1 readers | `kit.cpp:421-552` (`read_limits`, `read_dat`, `read_toy`); `unit/corridor/test_reference.cpp:51-182` (`F1Graph`, `F1Call`) | `f1::read_limits`, `read_dat`, `read_written_graph`, `read_console` | `read_limits` keeps the box and marks cache hits, which section 12's `Check` lacks; the solve search replay (`test_search.cpp:79`) needs both |
| walk oracle | `unit/corridor/test_properties.cpp:115-188` | `walk_corridor` | on its own index model; covers exclusions with complex closure and the edges-to-target rule, which section 12's signature has no parameter for |
| split check | `unit/merge/test_split.cpp:26-36, 142-216` | the S2.22 layers | `split` against `merge_direct` only, no second oracle; 3 hand-made sets over 7 ids at 5 thresholds (1920 comparisons) and 60 random sets over 12 ids with 8 exception sets each |
| pathway generator | `unit/merge/generator.hpp:18-71`, `Kgml` builder `support.hpp:125-166` | `pathways()` | on `core::Rng` already |
| scratch directories | 9 copies: `core/support.hpp:17`, `network/support.hpp:32`, `expression/support.hpp:26`, `enrichment/support.hpp:91`, `run/support.hpp:31`, `merge/support.hpp:29`, `corridor/corridor_support.hpp:56`, `results/fixture.hpp:151`, `solve/step.hpp:63` | `ScratchDir` | none is kept on failure or retries removal on Windows |
| recording step contexts | 8 copies (network, merge, corridor, expression, enrichment, results, solve, run) | none | not in section 12's kit |
| JSON Schema subset validator | `unit/results/test_schemas.cpp:19-76` | none | used only by results |

Two findings:
- **Platform-dependent test inputs.** Solve's random tests draw through `std::uniform_int_distribution` (`test_encoding.cpp:111-112`, `test_search.cpp:63, 70, 144`), whose results differ between libstdc++, libc++ and MSVC. 12.C9 and 09.C15 exist to avoid that. Corridor's property test uses `rng() % n` on `std::mt19937_64` (`test_properties.cpp:41`), which is the same everywhere.
- **The 177 witnessed points** can be checked now: all 39 KEGG captures are in the data root (W3's `reports/w3/*/counts.tsv`), with each recorded case's witness files under `reference/<id>/f1/run/`.

## 3. Design

### Files

```
backend/tests/testkit/
  include/fusion/testkit/
    data.hpp scratch.hpp generate.hpp canon.hpp fusion1.hpp check.hpp oracles.hpp e2e.hpp run_facts.hpp
    adapt.hpp scripted_oracle.hpp step_context.hpp     (need libfusion)
    catch.hpp                                          (Catch2 helpers; header-only)
  src/*.cpp
backend/tests/unit/testkit/                            test_testkit: the kit's self-tests
```

### Interface

Section 12's sketch (lines 201-360) is the interface, with the changes and additions below (open choice C2.4 lists them as departures). Headers are `.hpp`, as every module's are; section 12 writes `.h`.

```cpp
namespace fusion::testkit {
// data.hpp
struct ManifestEntry { std::string path; std::uint64_t size; std::string sha256; std::string from; };  // + from
std::vector<ManifestEntry> load_manifest(const std::filesystem::path& data_manifest_json);
std::variant<DataRoot, std::string> require(std::span<const std::string> paths);   // the root, or why to skip
// verify() caches hashes by path, size and mtime in <root>/.fusion-test/hashes.tsv (12, Test data, rules)

// scratch.hpp
std::filesystem::path source_dir();                                   // backend/tests

// generate.hpp
std::uint64_t test_seed(std::uint64_t fixed);                         // FUSION_TEST_SEED, else fixed (12.C9)
struct TinyCase { CanonCorridor corridor; CaseSpec spec; };

// fusion1.hpp
namespace f1 {
struct Check { Point point; std::string step; CheckOutcome answer; double cpu_s; bool cached; };   // + cached
struct Limits { Point box_lo, box_hi; std::vector<Check> checks; std::vector<Point> found; };
Limits read_limits(const std::filesystem::path&);                     // was std::vector<Check>
std::vector<std::pair<std::string, std::string>> read_coexpression(const std::filesystem::path& csv,
                                                                   double threshold);   // S5.15, by column name
}
// check.hpp
//   CaseSpec gains `bool named = true;`: 06.C16's endpoint rule holds for the named form only.

// oracles.hpp
struct WalkRules {                                                     // S4.04 to S4.06, as the corridor suite tests them
  std::vector<std::string> exclude;
  enum class ToTarget { any, activation, expression } to_target = ToTarget::any;
  std::vector<std::string> skip_subtypes{"association", "component", "dissociation", "mapping"};
};
CanonCorridor walk_corridor(const CanonGraph&, std::span<const std::string> sources,
                            std::span<const std::string> targets, int bound, const WalkRules& = {});
Frontier exhaustive_frontier(const CanonCorridor&, const CaseSpec&, Semantics,
                             Point box_lo, Point box_hi);              // + box_lo (06.C20 clamps to it)

// e2e.hpp
RunResult run_fusion(const std::filesystem::path& exe, std::span<const std::string> args, const ScratchDir&,
                     std::span<const std::pair<std::string, std::string>> env = {});   // + env (11.C20)
Normaliser fusion_normaliser(const std::filesystem::path& run_dir, const std::filesystem::path& case_dir,
                             const std::optional<DataRoot>&);    // 09-runs.md:292's fields, 12's event masks
//   compare_tree compares only the files its rules' globs match; `unexpected` is a matched file with no golden.
//   Its path tokens apply to every compared file, and `events.jsonl` in `exact` mode is compared as one
//   stream per (step, point, case) with `seq` masked: the C5 note's choices C5.4 and C5.3.

// run_facts.hpp: a 2.0 run directory as kit types (results.json, run.json, the run's copied inputs)
struct SolveFacts {
  std::string step;                                                    // "pathz3", "pathz3-2"
  CanonCorridor corridor;                                              // results.json's nodes, with members
  struct Case { std::string name, status; Frontier frontier; std::vector<Assignment> witnesses; CaseSpec spec; };
  std::vector<Case> cases;
};
struct RunFacts { std::string status; int exit = 0; std::vector<SolveFacts> solves; };
RunFacts read_run(const std::filesystem::path& run_dir);
}  // namespace fusion::testkit

// fusion_testkit only (adapt.hpp, scripted_oracle.hpp, step_context.hpp)
namespace fusion::testkit::adapt {
CanonGraph canon(const network::Network&);
CanonGraph canon(const network::GraphView&);
CanonCorridor canon(const corridor::Corridor&);
CanonCorridor canon(const solve::CaseGraph&);                          // a gene node's key is its one member
CaseSpec spec(const solve::CaseSpec&, const solve::Settings&, solve::Case);
Assignment assignment(const solve::Witness&);                          // keys as canon(CaseGraph) writes them
Assignment assignment(const corridor::Corridor&, const solve::Witness&);
}
namespace fusion::testkit {
class ScriptedOracle;    // section 12's, plus the solve kit's truth(), model, asked and witnesses
class StepContext;       // a core::Context over a directory, recording log lines, events and created files
}
```

`catch.hpp` holds the Catch2 side, so the library itself does not depend on Catch2: `FUSION_REQUIRE_DATA(paths...)` calls `require()` and `SKIP`s with its reason; `FUSION_SEED(fixed)` prints the seed with `INFO`.

### Build

- `fusion_testkit_base` (static): everything in the first namespace block. It links `fusion::core`, `pugixml::pugixml` and `nlohmann_json` only, so an include of `fusion/network`, `merge`, `corridor` or `solve` from an oracle fails to compile (open choice C2.2).
- `fusion_testkit` (static): `adapt`, `ScriptedOracle` and `StepContext`, linking the base and `fusion::libfusion`.
- `test_testkit`: one Catch2 executable from `unit/testkit/`, added to the loop in `backend/tests/CMakeLists.txt:4`, labelled `unit;testkit` and, for the data-root tests, `kegg`.

### How the suites use it

- **solve:** `check` and `exhaustive_frontier` calls go through `adapt::canon(CaseGraph)`, `adapt::spec` and `adapt::assignment`, so the independent checker now re-derives what `make_case` derives. `ScriptedOracle` moves to the kit. `read_toy` stays in the solve suite and calls `f1::read_dat` and `f1::read_limits`. The random tiny cases come from `tiny_case()` and the staircases from `core::Rng`, at section 12's sizes (200 cases of 6 nodes and 8 edges per run; the suite now solves 80, `test_encoding.cpp:177`).
- **corridor:** the property test compares `compute()` with `walk_corridor()` through `adapt::canon`; `test_reference.cpp` reads FuSION 1's files with `f1::read_written_graph` and `f1::read_console`.
- **merge:** `test_split.cpp` adds `exception_merge` as the third side of every comparison, compared as `CanonGraph`s, and grows to section 12's sizes; the random sets come from `pathways()`. `dump()` stays, because 03.C8's golden digest is over it (`test_golden.cpp`).

### Self-tests

Section 12's test hooks, plus: `read_run` on a toy run; `require()` skipping on a missing root and on a corrupted file (a copy of the root's manifest with one hash changed, so the real root is never touched); `ScratchDir` kept after `keep()`; golden SHA-256s of `pathways()`, `value_file()` and `tiny_case()` at fixed seeds.

## 4. Checks it runs now (P5 as amended)

| Check | Tier | Needs |
|---|---|---|
| S2.22, bounded exhaustive and random layers, both oracles | T0, merge suite | nothing beyond the repository |
| S2.22, real layer: the 167 KGMLs split by 100 random subsets of the EGF up and down genes, against both oracles | T2, `[kegg]` in the merge suite | data root; a Release build (see Cost). The 7 pinned merges come with C4's pinned merge stage |
| W7: FuSION 1's merge on the generated sets with three exception sets each, then both oracles against it wherever no fix applies | W7 run, then T1 | `pathways()` written to `reference/synthetic/merge-<n>/inputs/`; the snapshot binary through `run_fusion1.py`; about 150 short runs, unbooked per the plan's order (item 3), after a load check |
| the checker on FuSION 1's 177 witnessed points, on mutations, and on `r5` at (4, 4) | `[kegg]`, `test_testkit` | data root: captures, witness files and list files of the 23 recorded cases |
| every witness solve's suites produce, through the independent checker | T0, solve suite | nothing more |

The checker on every witness of the reference cases comes with C4.

*As built (prompt log entry 349):* the two synthetic S2.22 layers overran T0's budget at section 12's sizes, so T0 runs a copy of each cut to fit it (the listed shapes alone, and the first 50 random sets), and the copies at section 12's sizes are tagged `[slow]`, like the real layer. The checker's `fusion1` semantics never count edges out of the target, as S5.10 says and FuSION 1's code does; section 12 records both. The three `[kegg]` self-tests that need W3's captures skip until C3.2 adds them to the data manifest.

## 5. Open choices

### C2.1 · Which sprint helpers move into the kit
- **Proposed:** move or replace the pieces that section 12 names: solve's checker, exhaustive frontier, `ScriptedOracle` and FuSION 1 readers; corridor's walk oracle and FuSION 1 readers; merge's random generator. Three suites change (solve, corridor, merge). Scratch directories, recording contexts, the schema validator and the module-specific builders (`solvekit::Graph`, merge's `Kgml`, results' `Catalogue`) stay where they are.
- **Alternatives:**
  - The kit is new code beside the helpers, and no suite changes.
  - Move everything shared: all ten suites switch to the kit's `ScratchDir` and `StepContext`.
- **Why:** the solve kit's checker takes `make_case`'s output, so a wrong counted set or a wrong FSC case passes it unseen. Section 12's checker re-derives them, which is the point of independence. Two checkers of S5.11 to S5.17 would drift. The scratch and context copies work and differ in small ways (corridor's records creation order, enrichment's fails on the n-th create), so moving them is churn without a test gain.
- **Decision:** as proposed (prompt log entry 340)

### C2.2 · Independence enforced by the build
- **Proposed:** two libraries, `fusion_testkit_base` (core, pugixml and nlohmann/json only) and `fusion_testkit` (adapters), as above.
- **Alternatives:** one library, with independence kept by review.
- **Why:** section 11 already makes layering a compile error (`backend/CMakeLists.txt:3`). The same costs one more `add_library` here and turns section 12's principle into something the build checks.
- **Decision:** as proposed (prompt log entry 340)

### C2.3 · A recording `StepContext` in the kit
- **Proposed:** add it. C4's pinned mode calls library stages, and each needs a `core::Context`. The module suites keep their own copies (C2.1).
- **Alternatives:**
  - C4 writes its own context inside `fusion-test`.
  - The kit's context replaces the eight copies now.
- **Why:** one context for the runner and for later suites, without touching suites that work.
- **Decision:** as proposed (prompt log entry 340)

### C2.4 · Departures from section 12's sketch
- **Proposed:** accept, as a group: `.hpp` headers; `ManifestEntry::from`; `require()`, `source_dir()`, `test_seed()`; `f1::Limits` with the box and cache hits; `f1::read_coexpression`; `CaseSpec::named`; `WalkRules`; `box_lo` on `exhaustive_frontier`; `env` on `run_fusion`; `fusion_normaliser()` and `read_run()`; `compare_tree` limited to matched files.
- **Alternatives:** follow the sketch literally; solve's search replay and corridor's exclusion tests then keep their own code.
- **Why:** each change carries something a sprint test already needs, or that section 12's prose asks for and its sketch leaves out (the seed variable, the skip rule, the fake-tool environment, the masks).
- **Decision:** as proposed (prompt log entry 340)
- **As built (prompt log entry 349):** the user accepted these further departures as a group:
  - `read_run` completes each solve's corridor from the `fb_rch` step's `corridor.json` and the run's network file, because `results.json` lists only the nodes its witnesses name;
  - `manifest_path()` and a `require()` that takes a manifest; `ScratchDir::kept()`; `exception_merge_texts()` for generated sets; a timeout on `run_fusion`; `normalise_file()` and `matched_files()` for C5's golden updates; `keep_failing_input()` for 12.C9's saved inputs; an `adapt::spec` that takes the case graph, for edge lists;
  - masks for `run.started`'s `run` and `name`, which change in every run, and, by the same poll's fourth question, its `parent`.

## 6. Cost

- **Size:** about 30 files and 3,600 lines: the base about 2,700 (checker with both semantics about 400, `exception_merge` about 300 as 12.C14 estimates, FuSION 1 readers about 450, canonical forms and `diff` about 300, generators about 300, `read_run` and the e2e helpers about 500), the adapters, oracle and context about 400, and the self-tests about 500. The solve, corridor and merge suites lose about 550 lines and gain about 200.
- **Run time:** the kit's self-tests take seconds, and the `[kegg]` witness test under a minute.
- **The S2.22 layers are the risk.** Today's exhaustive test takes 3.34 s for 1920 comparisons in the Debug build (sprint log `ctest_dev.log`), 1.74 ms each. Section 12's sizes are up to 51,200 comparisons, plus a third oracle: about 2 min in Debug, and the random layer about as much again. T0's budget is 1 min per platform, and CI runs T0 in a Release build (section 11, `ci.yml`), which nothing has measured yet. The real layer runs 100 splits and 200 oracle merges of the 167 KGMLs; at the Debug `mff`'s 0.78 s for 44 KGMLs (run.json of the sprint's `jun-b6-fdr025` run, which merged 2.0's own shortlist), that is about 10 min in Debug, so it is meant for a Release build. Both are measured when the code lands, and any overrun comes back to the user as a poll.
