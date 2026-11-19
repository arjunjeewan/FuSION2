# 07 · Results and outputs

## Scope

This section designs the `results` module (Choice C1): everything FuSION 2.0 writes from a solve that has finished, stopped, or been loaded from a parent run. It implements stage 6 (S6.01 to S6.26), the output side of stage 5 (S5.19's flags in outputs, S5.25 to S5.27, S5.30 and the frontier lines of S5.31), S5.13's sign-consistent routes and S2.31's ECrel marks.

It covers:
- the published form of a solve, `pathz3/results.json`: the frontier with certification flags, the witnesses keyed by stable node and edge keys, the observations and the corridor summary (E1, [decision 0027](../decisions/0027-graph-display-in-phase-5.md)), built from 06's `solve::Results`;
- the checks and analysis run on every result: the shared polarity rule (S6.14), sign-consistent routes (S5.13) and the FSC comparison (07.C7);
- the one explanation renderer (S6.26): its view model, its pages with one shared copy of the drawing library (S6.18), the path filter and the edge table (S6.04, S6.08), the static export (S6.07), and the run page that `run` writes when a run ends;
- the site report (S6.19);
- the built-in Pareto plot step, `plot_pareto`, drawn by gnuplot (S6.22 to S6.24, F1);
- FuSION 1's text formats (E1, S5.27);
- the console lines, events, settings and errors of all of these.

It leaves to other sections:
- 06: the search, its structured search log (S5.24), certification (S5.19, 06.C6), witnesses (S5.26), `solve::Results` with the case statuses (cross-section item 1.5), the per-check console lines and the timing line (S5.31);
- 05: corridor distances (S4.07), the corridor summary and the observation match (05.C9);
- 04: the per-gene values and how a value input is read (04.C9);
- 02: node and edge keys (02.C11), node labels and members, edge types, the polarity rule's functions (2B.1), and the symbol and pathway names, KEGG ones read from each KGML's root title (prompt log entry 205);
- 01: the command table, with each command's tool requirements (2A.1, 2B.15), and how FuSION 1's positional `pex`, `psp` and `exec mv` lines map onto this design (07.C9, 01.C11);
- 09: the run directory and its step directories (09.C2), `run.json`, the event envelope, `Files` and `OutputFile`, the check for output clashes, the lock, and writing the run page at the end of a run;
- 11: the dependency check (2A.1), the process helper (2A.11), the gnuplot runner and plot palette in `core` (2B.13), the bundled gnuplot and font (11.C9, 11.C10), and the compiled-in page assets (11.C11);
- 10 and Phase 5: how the frontend shows results ([decision 0027](../decisions/0027-graph-display-in-phase-5.md)).

Names and evidence used below:
- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`: one point (n 1, e 0) with the FSC JUN, none without it.
- "The full JUN run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/`: 6 points with JUN, 7 without, 13 pex pages.
- "fdr050" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr050/`.
- "The witness census" parsed all 181 `*_solutions.txt` files under `runs/` on 2026-11-12, with their relaxation files: 591 points with a witness and 14 without.
- "The prototype" converted the FuSION 1 dumps of the reference run, the full JUN run and fdr050 into this section's `results.json` and view formats, with a Python script, to measure sizes and check the rules on real witnesses. Its edge keys lacked the type that 02.C11 adds, so its sizes are a few per cent low.
- "The batch census" read the 116 `*.batch` files in the FuSION 1 repository on 2026-11-12, with `let` expanded.
- gnuplot 6.0 and Graphviz 14.1.2 were run on this machine for timings. FuSION 1 was not run.

Witness sizes in the witness census:

| Per point | median | 95th percentile | max |
|---|---|---|---|
| present nodes | 56 | 157 | 197 |
| present edges | 76 | 228 | 302 |
| present but inactive nodes | 1 | 4 | 6 |
| relaxed nodes | 3 | 7 | 12 |
| relaxed edges in the witness | 2 | 6 | 9 |
| relaxed edges outside the witness | 0 | 4 | 7 |
| points per solution file | 3 | 6 | 7 |

Corridors in the run logs reach 793 nodes and 3,328 edges (the b15 runs).

## Requirements carried in

Stage 6:
- S6.01: `psp` keeps its six arguments, named, and rejects an unknown mode, a bad window, missing or extra arguments, and equal output names for the two cases.
- S6.02: one documented reader of the solve's results; every recorded witness is read; a point without a witness is marked, not skipped or drawn empty.
- S6.03: selectors `all`, `least` and `<n>,<e>`, strict: `7,1x` is an error.
- S6.04: the path filter keeps its four modes, documented as walk-based.
- S6.05: a path view follows the witness's edges as the solver chose them and draws relaxed edges as relaxed, by the solver's polarity rule; sign-consistent routes come from a separate report.
- S6.06 and its open question 5: the explanation is every present node and edge of the witness, honoured inhibitions and inactive nodes included, with each node's activity and each edge honoured or relaxed.
- S6.07: a static export, with names and labels escaped, `dot` checked before the run, and the README corrected.
- S6.08, read through E1 (cross-section item 4B.33, prompt log entry 180): the edge table has one row per drawn edge listing all its edges, is a TSV whose fields hold no tab or line break, with `;`-separated lists, and takes names from the run's one pathway-name source.
- S6.09 and S6.20: a failure fails the command and reports its real cause; printed counts match what is drawn.
- S6.10: named arguments, each value passed as its own argument with no shell, required arguments checked, every site option settable from a batch.
- S6.11: one install-directory lookup, one list of helper dependencies, checked before the first step; 11 owns the mechanism (2A.1).
- S6.12, read through cross-section item 4A.13 (prompt log entry 168): a named file that cannot be read is an error, `0` means "not given", and the page draws the values the solve used.
- S6.13: keep the layout, levels included (hop distance from the source over all drawn pairs, unreachable nodes one level past the deepest), and the table; the table holds the relaxed nodes that are not in the witness and says why each is there.
- S6.14: one polarity rule, shared by the solver and the renderer, drawn as one edge per effect.
- S6.15: draw cases 1 to 3 (honoured, removed, flipped), never the twin; match relaxed edges by edge identity; show whether a relaxed edge is in the witness; relaxed edges outside the witness count.
- S6.16, amended by 07.C4 for the static page (prompt log entry 178): keep the colours and borders, make the legend match the drawing, correct the docs; inactive nodes are drawn faded on the static page, and the frontend's display stays Phase 5's.
- S6.17: the title comes from the run's real case and from symbols; "of several" only when known; the page is generated directly, not by patching a template.
- S6.18: the library ships once per output directory or site, and pages work offline.
- S6.19: keep the static report (run page, manifest, landing page) with every option a batch argument.
- S6.21: each case has an explicit "no PO point" result, which the renderers report as such.
- S6.22: a built-in Pareto plot step that takes the run's own results, ids and symbols and draws one or two curves.
- S6.23: keep the look; the subtitle lists only the constraints the run used (S5.03); the font is checked with the other dependencies.
- S6.24: the dodge for points on both curves is sized from the axis range.
- S6.25: 2.0 generates only the built-in plot step; old inline gnuplot lines still run through `exec` (S1.14).
- S6.26: one explanation renderer, with psp's path filter and edge table as options and a static export.
- Open question 2 (psp's SVG on a real dump) is Phase 3's; open question 3 (witness-less points) is designed for below.

Stage 5 and other stages:
- S5.12, S5.14, S5.16: the outputs list the counted and excluded observations, and state the modelling choices for absent nodes and exempt endpoints, the every-path-crosses-a-DE-node rule and the strict `>` score tests.
- S5.13: sign-consistent routes are reported separately from presence; self-loops leave the edge count.
- S5.17, extended by 06.C10 (prompt log entry 175): the FSC is a set of one or more ids; the cases are `in`, `out` and `free`, by default `in,out` with an FSC and `free` without; "in" means every FSC node present on a path with activity matching its observation (active when up-regulated or unobserved, inactive when down-regulated); "out" means absent and removed from the observations.
- S5.19, as 06.C6 implements it (prompt log entry 172): every point is certified or not, its blocking undecided cells are named, and the box's undecided cells are listed.
- S5.22: the seed is recorded in the outputs.
- S5.25, amended by 06.C20 (prompt log entry 174): the frontier is the set of Pareto-minimal witness counts, so a point always equals its witness's counts; the two-column file is kept; certification travels in the structured outputs.
- S5.26: the witness is the search's own model; more solutions only on request; the result files are always written, empty when there is no point.
- S5.27: nodes and edges are named by stable keys, and absent nodes carry no activity.
- S5.30, read through cross-section item 4A.12 (prompt log entry 177): every output of a solve goes to the run's directory; the case labels inside one `results.json` and the per-case file names meet "named by solve case".
- S5.31: the same milestones as FuSION 1, points in one order and labelled, no lines about unused work.
- S4.14, settled by 02.C11 (prompt log entry 159): a node's key is its rep id; an edge's key is its source key, target key, type and ordered subtypes, printed `src|tgt|type|sub1,sub2`.
- S2.31: ECrel edges are marked in the outputs.
- S1.19 with prompt log entry 205: KEGG pathway names come from each KGML's root title at run time, other names from the shipped map; lookups never insert and fall back to the id (S2.15, S1.18).
- S3.12 and S4.16, as 05.C9 implements them: unmatched genes are listed in two groups, outside the network and outside the corridor.
- S4.07: corridor distances are kept for the display.
- S1.14, amended by 01.C11 for six `exec mv` lines (prompt log entry 179): the recognised moves call this section's `legacy_limits` for `limits_timefile.txt`, so its destination gets FuSION 1's rendering, and `two_column` for `PO.dat` (prompt log entry 230).
- S1.26 and S8.21, read through cross-section items 2B.3 and 4A.11 (prompt log entry 169): a relative or omitted output path lands in the step's directory; an absolute one is honoured as well, the file kept in the step directory too, both recorded in `run.json`; the site export is the one write outside the run directory.
- S1.27: console numbers are printed with enough digits to be exact. S1.28: a failure stops the batch with a non-zero exit.
- S8.26, S8.27 and S8.29: the plot is drawn once to a file; every step is a pipeline block; the frontend shows each run's results.

Decisions, skeleton Choices and cross-section answers:
- [0017](../decisions/0017-batch-file-compatibility.md) and [0023](../decisions/0023-results-may-change-where-improved.md): old batch files still run; their outputs may change where 2.0 improves.
- [0018](../decisions/0018-batch-interface.md): named arguments and positional FuSION 1 lines.
- [0019](../decisions/0019-partial-runs.md): partial runs, so results may be partial, may lack a case, and may be read from a parent run.
- [0021](../decisions/0021-required-and-optional-inputs.md): inputs and tools are checked before the first step.
- [0022](../decisions/0022-graph-display.md) and [0027](../decisions/0027-graph-display-in-phase-5.md): the display must be efficient and seamless; Phase 5 designs it; Phase 2 gives it usable results.
- A1, as amended by 06.C19 (prompt log entry 174): Stop saves state and exits with a "stopped" status (3); a killed run's journal is used on resume.
- A2 with cross-section item 1.3 (prompt log entry 190): dotted `<area>.<event>` names, the key `type`, the envelope `v`, `seq`, `t`, `type` plus `step`, `line` and `point`; warnings only through `warning`.
- B1, C1, and 11.C6 with cross-section item 1.2 (prompt log entry 201): one static library per module; `results` depends on `solve`; the batch layer calls `publish_solve` after the solve step.
- D1 with cross-section item 2A.2 (prompt log entry 158): the solve step's saved artefact is 06's journal and `case.json`; `results.json` is a final output that `pex` and `plot_pareto` may also read from a parent run.
- E1: `run.json`, JSON results, TSV tables; FuSION 1's text formats only when a run setting asks, which is `legacy_prefix` (cross-section items 1.21 and 4A.9, prompt log entry 179).
- F1: C++ writes the export; gnuplot draws the Pareto image. G1: nlohmann/json. G3: Linux, macOS and Windows, native.
- Cross-section items accepted in prompt log entry 158: one `core` vocabulary, 09's (1.1: `core::Failure`, `Outcome<T>`, `Log`, `Events`, `Context`, `Sha256`, `stable_compare`); generic error codes from 09 and 01 (1.17); gnuplot 5.4 or later with `pngcairo`, 11's lookup order and the name `plot_pareto` (1.27); every write through 09's `OutputFile` with its Windows retry rule (1.28, 2A.6); 09's clash check by file identity (2A.7); 11's dependency check (2A.1) and process helper (2A.11); a results summary in `run.json` (2B.7); a gnuplot runner and the palette in `core` (2B.13); a lock for any file (2B.17); `pex` takes the solve's values (1.23).
- Cross-section items 1.5 (prompt log entry 177): 06 owns `solve::Results` and the case status, `not_requested` added; 07 builds `results::SolveResult` and maps the statuses to display words; one observation vocabulary of seven words. Item 1.4 (entry 177): 09.C2's step directories, with `pathz3/results.json`, `plot_pareto/`, `pex/` and the run page written once at the end. Item 3A.14 with 1.8 (entry 167): the only reported results are `corridor.empty`, `gsea.empty_selection` and `observations.none`; a solve's outcomes are case statuses. Item 1.16 (entry 190): registry keys `<module>.<name>`, bare names on the command line. 09.C14 (entry 186): failures are `core::Failure`, a stop is `core::Stopped`.

## Design

### Components

| Component | Responsibility | Spec |
|---|---|---|
| model | `SolveResult`: tables, cases, points, witnesses, observations, built from `solve::Results` | S5.19, S5.26, S5.27, E1, 1.5 |
| analyse | invariant checks, sign-consistent routes, FSC comparison | S6.14, S5.13, S5.19 |
| json | the writer and the one reader of `results.json` | S6.02, E1, 2A.2 |
| view | the one renderer's view model, point selectors, path filter | S6.03 to S6.06, S6.13 to S6.17, S6.26 |
| explain | pages, view files, edge table, static export, library copy, the run page's HTML | S6.07, S6.08, S6.17, S6.18 |
| pareto | the gnuplot script and the plot step | S6.22 to S6.24 |
| site | publishing a run into a site directory | S6.19 |
| legacy | the two-column file and FuSION 1's text formats | E1, S5.25, S5.27 |
| summary | console lines, events and the `run.json` summary | S5.31, A2, 2B.7 |

Dependencies follow 11.C6: `results` depends on `solve` (and through it on `corridor`, `expression`, `network`) and on `core`. It never sees `run` types: every step receives a `core::Context`, whose `Files` writes through `OutputFile` (1.1, 2A.6). The batch layer calls `publish_solve` after the solve step returns or stops (1.5).

### From `solve::Results` to `SolveResult`

06 owns the search's result: `solve::Results`, one `CaseResult` per case, each with its status, frontier (points with `certified`, `undecided_below` and witness indices), undecided cells, answers and witnesses with stable keys (1.5). This section builds its published form, `results::SolveResult`, from that and from what the display needs besides: node labels and members, edge types and pathways, per-gene values, corridor distances and the observation match. Tables hold every node and edge that a witness, an observation or the header refers to, so the source, the target and every FSC node are always there; witnesses are sorted index lists into them. Both cases of a solve share one corridor (S5.17), so they share the tables.

Nodes are identified by their key, the rep id, and edges by 02's key: source key, target key, type and ordered subtypes, printed `src|tgt|type|sub1,sub2` (02.C11, settling S4.14). 02.C5 merges exact duplicates when a network is read, so the key is unique in every network. Indices appear only in memory, beside keys.

```cpp
// backend/modules/results/include/fusion/results/model.hpp
namespace fusion::results {

using solve::Case;                              // in, out, free (06.C10)
using solve::CaseStatus;                        // complete, no_point, infeasible, no_de_gene,
                                                // stopped, budget_exhausted, not_requested (1.5)
using solve::Point;                             // {n, e}: n node relaxations, e edge relaxations
using NodeIx = std::uint32_t;                   // index into Tables::nodes
using EdgeIx = std::uint32_t;                   // index into Tables::edges

enum class Observed : std::uint8_t { none, up, down };

struct Member { std::string id, symbol; std::optional<double> lfc; Observed observed; };

struct NodeRecord {
  network::NodeKey key;                         // the rep id (S4.14)
  std::string label;                            // 07.C10: "EGFR", "RRAS2+5", "[STAT1+6]"
  network::NodeKind kind;                       // gene, complex, compound, other
  std::vector<Member> members;                  // sorted by id; values the solve used (04, 4A.13)
  Observed observed;                            // the class the solve used
  std::optional<std::int32_t> from_source, to_target;   // corridor distances (S4.07)
};

struct EdgeRecord {
  network::EdgeKey key;                         // source, target, type, ordered subtypes (02.C11)
  NodeIx source, target;
  std::string type;                             // "PPrel", "ECrel", "GErel", "PCrel" (S2.31)
  std::vector<std::string> subtypes;            // stored order
  network::Effect effect;                       // the one polarity rule, fixed at solve time (S6.14)
  std::vector<std::string> pathways;            // sorted ids
  bool frozen = false;                          // cannot be relaxed in this solve (S5.16)
};

struct Tables {
  std::vector<NodeRecord> nodes;                // sorted by key (core::stable_compare)
  std::vector<EdgeRecord> edges;                // sorted by key
  std::map<std::string, std::string> pathway_names;   // referenced ids only (S1.19, entry 205)
};

struct Route {                                  // S5.13: over selected edges whose effect is honoured
  bool exists = false;
  std::vector<NodeIx> nodes;                    // the first shortest route, neighbours taken in key order
  std::vector<NodeIx> fsc_on_route;             // the FSC nodes on some such route; in case only
};

struct Witness {                                // sorted, unique lists (06's Witness, re-indexed)
  std::vector<NodeIx> active, inactive;         // present nodes only (S5.27)
  std::vector<NodeIx> relaxed_present, relaxed_absent;   // relaxed nodes, by presence
  std::vector<EdgeIx> selected;
  std::vector<EdgeIx> relaxed_selected, relaxed_unselected;   // S5.13; never a self-loop
  Route route;                                  // filled by analyse()
  Point counts() const noexcept;                // relaxed nodes, relaxed edges
};

enum class Enumeration : std::uint8_t { not_requested, complete, limit_reached, stopped_unknown };

struct FrontierPoint {
  Point at;                                     // equals witnesses[0].counts() (06.C20)
  bool certified = false;                       // 06.C6
  std::vector<Point> undecided_below;           // the cells that block it
  std::vector<Witness> witnesses;               // [0] is the search's own model; more on request (06.C11)
  Enumeration enumeration = Enumeration::not_requested;
};

struct RequestedPoint {                         // 06.C9: points=n,e;… asks without a search
  Point at;
  solve::Outcome answer;                        // sat, unsat, or an unknown with its label
  std::string label;                            // TIMEOUT, UNDEFINED or stopped (S5.19)
  std::vector<Witness> witnesses;               // empty unless sat
};

struct UndecidedCell { Point at; std::string label; };   // last answer there: TIMEOUT, UNDEFINED, stopped, or not asked

struct CheckCounts { std::uint32_t sat = 0, unsat = 0, unknown = 0; };

enum class ObservationStatus : std::uint8_t {   // one vocabulary, also in 06's observations.tsv (1.5)
  counted,            // in the relaxation count (S5.12)
  forced,             // an FSC gene in the in case: held consistent, never relaxed (S5.17)
  exempt_endpoint,    // the source or the target (S5.12)
  removed_with_fsc,   // an FSC gene in the out case (S5.17)
  outside_encoding,   // in the corridor but not in the encoding (S5.10)
  outside_corridor,   // in the network but not in the corridor (S4.16, 05.C9)
  not_in_network      // in no node of the network (S3.12, 05.C9)
};

struct Observation { std::string gene; Observed observed; std::optional<NodeIx> node; ObservationStatus status; };

struct CaseResult {
  Case kind;
  CaseStatus status;                            // 06's, as given
  std::string label;                            // display only: "with FSC (JUN)" (1.6)
  std::string status_text;                      // display words for the status (below)
  std::string message;                          // 06's, for infeasible and the like
  Point requested, searched;                    // upper corners of the box, before and after 06's clamp (S5.07)
  std::vector<FrontierPoint> points;            // ascending n; empty for a points request
  std::vector<RequestedPoint> requested_points; // 06.C9
  std::vector<UndecidedCell> undecided;         // 06.C6
  CheckCounts checks;
  std::vector<Observation> observations;        // sorted by gene
};

struct CorridorSummary { std::uint32_t nodes = 0, edges = 0; std::int32_t bound = 0; std::string artefact; };

struct Header {
  std::string name;                             // "solve" by default, or a positional prefix (07.C9)
  std::string step;                             // "pathz3", "pathz3-2" (09.C2)
  NodeIx source, target;                        // always in Tables
  std::vector<NodeIx> fsc;                      // the FSC set (06.C10); empty without one
  std::int32_t bound;
  std::uint64_t seed;                           // S5.22
  std::optional<double> coexpression_threshold, frozen_threshold;   // only the families used (S5.03)
  std::vector<std::string> rules;               // modelling statements (S5.12, S5.14, S5.16)
  CorridorSummary corridor;                     // from 05
};

struct SolveResult { Header header; Tables tables; std::vector<CaseResult> cases; };

struct PublishInputs {                          // what the batch layer hands over after the solve step
  const solve::Results& results;                // 06 (1.5)
  const solve::Inputs& inputs;                  // endpoints, FSC set, box, settings (06)
  const corridor::Corridor& corridor;           // node and edge attributes, distances, summary (05)
  const expression::ObservationMatch& match;    // 05.C9's three groups
  const network::Maps& maps;                    // labels and pathway names (02, entry 205)
  std::string name;                             // the solve's name (07.C9)
};

SolveResult build(const PublishInputs&);        // O(V + E + Σ witness sizes)

}  // namespace fusion::results
```

06's statuses map to display words, used in the console, the run page, the plot key and the pages:

| 06's status | Display |
|---|---|
| `complete` | "6 Pareto points, all certified", or "7 Pareto points, 2 not certified" |
| `no_point` | "no Pareto point within the bounds", or, with undecided cells, "no point found; 3 cells undecided" |
| `infeasible` | "infeasible: " followed by 06's message |
| `no_de_gene` | "no differentially expressed gene in the corridor" |
| `stopped` | "stopped: partial frontier" |
| `budget_exhausted` | "budget used up: partial frontier" |
| `not_requested` | "not solved in this run" |

Case labels are for display only (1.6): `in` is "with FSC (JUN)", `out` is "without FSC (JUN)", and `free` is "unconstrained". An FSC set is listed, as in "with FSC (RAC1, RAC2+1)". A positional `pathz3` line names no FSC, so its one `free` case is labelled by the solve's name, for example "solve temp_w" (S6.17).

Memory is small. The largest witness in the census has 197 nodes and 302 edges, and the full JUN run's 13 points reference 143 nodes and 280 edges in all.

### Checks and analysis

`analyse` runs on every `SolveResult` before anything is written. All checks together cost O(Σ (V_w + E_w) log E_w) over the witnesses, which is microseconds at the sizes above.

```cpp
// backend/modules/results/include/fusion/results/analyse.hpp
namespace fusion::results {

struct Issue {
  enum class Kind : std::uint8_t { internal_error, warning } kind;
  std::string code, message;
};
std::vector<Issue> analyse(SolveResult&);       // checks, then routes

enum class Verdict : std::uint8_t {
  identical, with_cheaper, with_costlier, mixed, essential, with_impossible, no_points
};
struct Comparison { Verdict verdict; std::optional<std::int32_t> delta_n_at_e0; bool certified; };
std::optional<Comparison> compare_cases(const SolveResult&);   // only when in and out were both solved (07.C7)

}  // namespace fusion::results
```

The checks:
- **Shape.** Lists are sorted and unique; `active` and `inactive` are disjoint; both ends of every selected or relaxed edge are present. A failure is an internal error.
- **Polarity.** For every selected or relaxed edge, `network::is_relaxed(effect, source state, target state)` must equal the witness's flag. This is the one rule of S6.14, which 06's encoding also uses (S5.13: activation is honoured only when both ends are active, inhibition only when exactly one is). A mismatch is an internal error.
- **Counts.** 06.C20 makes the frontier the Pareto-minimal counts of the witnesses of all SAT answers (amending S5.25), so every point equals its first witness's counts; any difference is an internal error. The rule exists because of FuSION 1's evidence: in the witness census, 12 of the 591 witnesses had counts below their point, for example `runs/real_experiments/experiment4.0/temp_wo_relaxations.txt`, where the points (12, 9), (11, 13) and (10, 15) all had witnesses with counts (10, 9). All 12 are in five old runs under `runs/real_experiments/`; nine of their ten solves kept a limits file, and each records 6 to 23 unknown checks and no UNSAT check.
- **Draw cases.** A node pair cannot have the same effect both kept and relaxed (S6.15's twin). Under the rule, a kept activation needs both ends active, and then no activation on that pair can be relaxed. A twin is an internal error.
- **FSC** (S5.17 as extended by 06.C10). In the `in` case every FSC node is present, on a selected path, with activity matching its observation; in the `out` case every FSC node is absent and its observation has the status `removed_with_fsc`; `free` puts no condition on them. A violation is an internal error.

Internal errors do not lose data. `publish_solve` writes `results.json` with its `issues` list first, then throws a `core::Failure` with the code `results.internal` (S1.28), so a 50-hour solve is kept for inspection.

**Sign-consistent routes (S5.13).** For each witness, take the selected edges that are not relaxed. A breadth-first search from the source, with neighbours in key order, gives the first shortest route to the target. An FSC node is on a route when it is reachable from the source and the target is reachable from it over those edges; `fsc_on_route` lists every such FSC node. Cost: O(V_w + E_w). The prototype reproduces the case found in the FDR sweep: in fdr050 with JUN at (n 5, e 1), JUN is present and active but on no sign-consistent route. Its only in-edge is the kept MAPK1 → JUN, and MAPK1's only in-edge is the relaxed DUSP1 ⊣ MAPK1. The route found is EGFR → PTK2 → MAPK14 → CXCL1 → CXCR6+17 → JAK1+3 → [STAT1+6] → MYC → CXCL8.

**FSC comparison (07.C7).** It compares the `in` and `out` cases. Let f(e) be the least n among a case's points with e' ≤ e, or infinity. Over e in 0 to the box's upper e:
- `identical` when both cases have the same f;
- `with_cheaper` when f_in ≤ f_out everywhere and < somewhere, and `with_costlier` the other way round;
- `mixed` otherwise;
- `essential` when `in` has points and `out` has none, `with_impossible` the reverse, `no_points` when neither has any.

`delta_n_at_e0` is f_out(0) − f_in(0) when both are finite. The comparison is certified when both cases are `complete`, or `no_point` with no undecided cell, and every point is certified. Cost: O(P + E_box). The full JUN run gives `with_cheaper`, delta 1, not certified, because (n 8, e 0) and (n 7, e 1) without JUN are blocked by the undecided cell (n 7, e 0). The reference run gives `essential`, certified: FuSION 1's limits file records 4 UNSAT checks and no unknown one without JUN.

### `results.json` and the solve's other outputs

`publish_solve` writes into the solve step's directory, `pathz3/` (or `pathz3-2/` for a second solve; 09.C2 and cross-section item 1.4):
- `results.json`, the published form (07.C1). It is a final output of kind `results`. The solve step's saved artefact, which a resume and a load use, is 06's journal and `case.json` (2A.2). A later run's `pex` or `plot_pareto` may read a parent run's `results.json` with the one reader (S6.02).
- `<case>.dat` for each solved case, the two-column file (S5.25), described with the FuSION 1 formats below.
- FuSION 1's text files under `legacy/`, when `legacy_prefix` is set (E1, 1.21).

It then returns the summary that the batch layer records as the solve step's `details` in `run.json` (2B.7).

```cpp
// backend/modules/results/include/fusion/results/json.hpp
namespace fusion::results {
inline constexpr std::string_view results_format = "fusion.results";
inline constexpr int results_version = 1;
std::string to_json(const SolveResult&, std::span<const Issue>, const std::optional<Comparison>&);
SolveResult from_json(std::string_view);        // throws core::Failure: artefact.newer_format, or a parse error
}

// backend/modules/results/include/fusion/results/publish.hpp
namespace fusion::results {
// Called by the batch layer after the solve step returns or stops (1.5): build(), analyse(),
// compare_cases(), then results.json, the .dat files and, with legacy_prefix, FuSION 1's files,
// all through ctx.files (OutputFile); prints the frontier and emits the events below.
// Returns the summary for the step's run.json details (2B.7). Throws core::Failure
// (results.internal) after writing if analyse() found an internal error.
core::Json publish_solve(const PublishInputs&, core::Context& solve_step);
}
```

Format rules for `results.json`:
- Witnesses list node keys and edge keys, never indices (E1, S4.14). Each edge record also gives the key's four fields (`source`, `target`, `type`, `subtypes`), as 02.C11 asks of JSON.
- Object members are written in a fixed order and arrays are sorted by key with `core::stable_compare`, so equal results give equal bytes.
- Numbers use nlohmann/json's shortest round-trip form, which does not depend on the locale.
- The file holds no time, no absolute path and no host name; those are in `run.json` (E1).
- Indented by one space, so runs can be diffed line by line. 07 writes its JSON Schema, which 12 keeps (2A.10).

Abridged example, the reference run. The check counts are FuSION 1's, from its limits files; 2.0's will differ, because 06 clamps the box (S5.07).

```json
{
 "format": "fusion.results", "version": 1,
 "solve": {"name": "solve", "step": "pathz3", "source": "hsa1956", "target": "hsa3576", "fsc": ["hsa3725"],
  "bound": 10, "seed": 42,
  "constraints": {"coexpression": {"threshold": 700}, "frozen": {"threshold": 800}},
  "rules": ["every-path-crosses-a-de-node", "absent-down-is-consistent", "endpoints-exempt", "scores-strictly-above-threshold"]},
 "corridor": {"nodes": 115, "edges": 370, "bound": 10, "artefact": "fb_rch/corridor.fnet"},
 "pathways": {"path:hsa04010": "MAPK signaling pathway", "path:hsa05210": "Colorectal cancer", …},
 "nodes": [
  {"key": "hsa1956", "label": "EGFR", "kind": "gene", "observed": "none",
   "members": [{"id": "hsa1956", "symbol": "EGFR", "lfc": 0.1, "observed": "none"}],
   "distance": {"from_source": 0, "to_target": 4}},
  {"key": "hsa22800", "label": "RRAS2+5", "kind": "gene", "observed": "none",
   "members": [{"id": "hsa22800", "symbol": "RRAS2", "lfc": 0.16, "observed": "none"},
               {"id": "hsa22808", "symbol": "MRAS", "lfc": 0.0, "observed": "none"}, …],
   "distance": {"from_source": 2, "to_target": 4}},
  …
 ],
 "edges": [
  {"key": "hsa4214|hsa5609|PPrel|phosphorylation", "source": "hsa4214", "target": "hsa5609",
   "type": "PPrel", "subtypes": ["phosphorylation"], "effect": "activation",
   "pathways": ["path:hsa04010"], "frozen": false},
  …
 ],
 "cases": [
  {"case": "in", "label": "with FSC (JUN)", "status": "complete", "status_text": "1 Pareto point, certified",
   "box": {"requested": {"n": 8, "e": 8}, "searched": {"n": 1, "e": 8}},
   "checks": {"sat": 4, "unsat": 7, "unknown": 0}, "undecided": [],
   "observations": [
    {"gene": "hsa3576", "node": "hsa3576", "observed": "up", "status": "exempt_endpoint"},
    {"gene": "hsa3725", "node": "hsa3725", "observed": "up", "status": "forced"},
    {"gene": "hsa8660", "node": "hsa8660", "observed": "up", "status": "counted"},
    {"gene": "hsa1843", "node": null, "observed": "up", "status": "outside_corridor"}, …],
   "points": [
    {"n": 1, "e": 0, "certified": true, "undecided_below": [], "enumeration": "not_requested",
     "witnesses": [
      {"counts": {"n": 1, "e": 0},
       "active": ["hsa1956", "hsa22800", "hsa2885", "hsa3576", "hsa3725", "hsa4214", "hsa5599", "hsa5609", "hsa6654"],
       "inactive": [],
       "relaxed_nodes": [{"key": "hsa8660", "present": false}],
       "selected": ["hsa1956|hsa2885|PPrel|activation", "hsa4214|hsa5609|PPrel|activation", "hsa4214|hsa5609|PPrel|phosphorylation", …],
       "relaxed_edges": [],
       "route": {"exists": true, "fsc_on_route": ["hsa3725"],
        "nodes": ["hsa1956", "hsa2885", "hsa6654", "hsa22800", "hsa4214", "hsa5609", "hsa5599", "hsa3725", "hsa3576"]}}]}]},
  {"case": "out", "label": "without FSC (JUN)", "status": "no_point", "status_text": "no Pareto point within the bounds",
   "box": {"requested": {"n": 8, "e": 8}, "searched": {"n": 1, "e": 8}},
   "checks": {"sat": 0, "unsat": 4, "unknown": 0}, "undecided": [],
   "observations": [{"gene": "hsa3725", "node": "hsa3725", "observed": "up", "status": "removed_with_fsc"}, …],
   "points": []}
 ],
 "comparison": {"verdict": "essential", "delta_n_at_e0": null, "certified": true},
 "issues": []
}
```

Sizes measured on the prototype: the reference run 7,365 bytes; the full JUN run 194,624 bytes (137,225 unindented, 14,285 gzipped), with 143 nodes, 280 edges and 120 pathways for 13 points.

**Stopped and budget-limited solves.** A solve ended by a Stop request is published too, with `stopped` on its unfinished cases, just before the process exits with A1's "stopped" status (3); no later step runs. A solve ended by 06's whole-solve budget (06.C3) is published the same way with `budget_exhausted`, and the batch goes on: the plot and the pages run on its partial frontier, which is certified point by point and resumable (cross-section item 3A.14, prompt log entry 167).

**The summary in `run.json`** (2B.7). The batch layer records `publish_solve`'s return value as the solve step's `details`, so the frontend can list sessions without opening `results.json`. Full JUN run:

```json
{"id": "pathz3", "cmd": "pathz3", "line": 11, "status": "ok",
 "details": {"results": {
   "cases": {"in": {"status": "complete", "points": 6, "certified": 6, "least_total": 7},
             "out": {"status": "complete", "points": 7, "certified": 5, "least_total": 8}},
   "comparison": {"verdict": "with_cheaper", "delta_n_at_e0": 1, "certified": false}}},
 "outputs": [{"kind": "results", "format": "fusion.results", "format_version": 1, "path": "pathz3/results.json", "…": "…"}, …]}
```

`least_total` is the least n + e over the case's points, FuSION 1's "min relaxations" note on the site cards.

### Console output and events

The library prints nothing (B1). These lines go through `core::Log`, and the events through `core::Events`, which adds the envelope (`v`, `seq`, `t`, `type`) and `step`, `line` and `point` (1.3). Points are always written n first and labelled (S5.31), and numbers in full (S1.27).

After a solve, `publish_solve` prints the frontier. Full JUN run:

```
with FSC (JUN): 6 Pareto points, all certified
  n=2 e=6 | n=3 e=4 | n=4 e=3 | n=5 e=2 | n=6 e=1 | n=7 e=0
without FSC (JUN): 7 Pareto points, 2 not certified
  n=2 e=7 | n=3 e=5 | n=4 e=4 | n=5 e=3 | n=6 e=2 | n=7 e=1* | n=8 e=0*
  * not certified: blocked by the undecided cell n=7 e=0 (TIMEOUT)
comparison: forcing JUN in saves 1 node at e=0 (not certified)
wrote pathz3/results.json (194624 bytes)
```

FuSION 1 logged that check as UNDEFINED, before its 2026-11-09 label fix (`…_wo_…_limits_timefile.txt:74-75`). Under S5.19's rule as 06.C2 applies it, the 14,396 s check under a 14,400 s cap is a TIMEOUT.

Reference run, without JUN, with FuSION 1's check count: `without FSC (JUN): no Pareto point within the bounds (4 checks, all unsat)`. A case not solved prints `without FSC (JUN): not solved in this run`. The search's own lines and the timing line are 06's.

`pex` prints one line per page, with counts of what is drawn (S6.09): `pex: with FSC (JUN) n=1 e=0 -> pex/explain/in_n1_e0.html (9 nodes, 8 edges; 1 relaxed node not drawn)`. `plot_pareto` prints `plot_pareto: plot_pareto/pareto.png (with FSC 6 points, without FSC 7 points, 2 not certified)`.

Events this section defines, in 09's envelope and dotted style (shortened):

```json
{"v":1,"seq":57,"t":"2026-11-12T13:20:31.120Z","type":"results.frontier","step":"pathz3","line":11,"case":"out","status":"complete","points":[{"n":2,"e":7,"certified":true},{"n":8,"e":0,"certified":false}],"undecided":1}
{"v":1,"seq":58,"t":"2026-11-12T13:20:31.121Z","type":"results.comparison","step":"pathz3","line":11,"verdict":"with_cheaper","delta_n_at_e0":1,"certified":false}
```

Files need no event of their own: `OutputFile::commit()` records each in `run.json`, and `step.finished` lists them (09). A warning, such as a missing plot font (`deps.font_missing`, from 11's dependency check; 07.C8), goes through the `warning` event with its dotted code and message.

### The explanation view: the one renderer

The view model is the renderer's output. The static pages draw it, and the Phase 5 display can draw it without re-deriving polarity or draw cases (S6.26).

```cpp
// backend/modules/results/include/fusion/results/view.hpp
namespace fusion::results {

struct PointSelector { enum class Kind : std::uint8_t { all, least, one } kind = Kind::all; Point at; };
PointSelector parse_point_selector(std::string_view);   // "all", "least", "<n>,<e>"; "7,1x" -> core::Failure (S6.03)
std::vector<std::size_t> select_points(const CaseResult&, const PointSelector&);

enum class PathMode : std::uint8_t { subgraph, all, shortest, alt };
struct PathFilter {
  PathMode mode = PathMode::subgraph;
  std::int32_t window = 2;                      // alt only (S6.01)
  std::optional<network::NodeKey> from, to;     // psp's src and tgt; default the solve's source and target
};
PathFilter parse_path_filter(std::string_view);         // "subgraph", "all", "shortest", "alt", "alt:W" with W >= 0 (S6.01)

enum class Draw : std::uint8_t { honoured, removed, flipped };   // S6.15 cases 1 to 3
enum Role : std::uint8_t { role_source = 1, role_target = 2, role_fsc = 4 };

struct ViewNode { NodeIx node; bool present, active, relaxed; std::int32_t level; std::uint8_t roles; };

struct ViewEdge {                               // one drawn edge (S6.14, S6.15)
  NodeIx source, target;
  network::Effect effect;                       // the head drawn: the group's effect; when flipped, the kept effect
  std::optional<network::Effect> replaced;      // flipped only: the relaxed effect
  Draw draw;
  bool in_witness;                              // false: relaxed but not selected (S6.15)
  bool ecrel;                                   // a member edge is ECrel (S2.31)
  bool on_route;                                // on a sign-consistent route (S5.13)
  std::optional<std::int32_t> slack;            // through-length minus L over selected edges (S6.04)
  std::vector<EdgeIx> members;                  // every edge it stands for, both effects' when flipped (S6.08)
};

struct ParkedNode { NodeIx node; std::string reason; };    // S6.13

struct WitnessView {                            // one recorded witness, ready to draw
  Point counts;
  std::vector<ViewNode> nodes;                  // present nodes, sorted by key
  std::vector<ViewEdge> edges;                  // sorted by (source key, target key, effect)
  std::vector<ParkedNode> parked;               // relaxed nodes outside the witness, by label
  std::optional<std::int32_t> shortest;         // L
  std::string route_note;
};

struct PointView {                              // one view file: a point and every recorded witness (S6.02)
  std::string solve; Case kind; Point at;
  Enumeration enumeration; bool certified;
  std::string title, subtitle, certification_note, missing_reason;
  std::vector<WitnessView> witnesses;           // empty for a requested point that is not sat
};

PointView build_view(const SolveResult&, const CaseResult&, const FrontierPoint&);
PointView build_view(const SolveResult&, const CaseResult&, const RequestedPoint&);   // 06.C9
std::vector<bool> kept_by(const SolveResult&, const WitnessView&, const PathFilter&);   // per ViewEdge

}  // namespace fusion::results
```

`build_view` makes one `WitnessView` per recorded witness of the point:
1. **Nodes.** Every present node is drawn (S6.06), with its activity, its relaxation and its roles (source, target, FSC).
2. **Edges.** Selected and relaxed edges are grouped by ordered node pair. K is the set of effects of kept edges (selected, not relaxed) and R the set of effects of relaxed edges. If R is empty, one honoured edge per effect in K. If K is empty, one removed edge per effect in R. Otherwise the pair has exactly one kept and one relaxed effect (the twin check rules out the rest) and gets one flipped edge: it is drawn with K's head, names R's effect as `replaced`, and holds the records of both. A drawn edge is in the witness if any member is selected, and marked ECrel if any member's type is ECrel. Relaxed records are matched by edge key, never by (source, target, subtype) (S6.15).
3. **Levels** (S6.13, kept). Each present node's level is its hop distance from the source over every drawn pair, selected or relaxed, in the witness or not, as pex measured it over all its pairs. A node that no pair reaches goes one level past the deepest. The source is present and active in every case (06.C16) and always has a selected out-edge (S5.11), so FuSION 1's one-column collapse, which happened when the source had no drawn edge, cannot occur.
4. **Slack.** Breadth-first searches from the source and, backwards, from the target over the selected edges give d_s, d_t and L = d_t(source). An in-witness group (u, v) gets slack = d_s(u) + 1 + d_t(v) − L. Distances strictly increase along selected edges (S5.11), so here walks are simple paths and S6.04's walk caveat does not arise. Groups outside the witness have no slack.
5. **Route.** Groups on a sign-consistent route are marked from the witness's `Route`.
6. **Parked table.** The relaxed nodes that are absent, sorted by label, each with its reason, for example "absent from this explanation; up-regulated, so it counts as one node relaxation" (S6.13).
7. **Text** (S6.17). The title is `EGFR → CXCL8 · with FSC (JUN) · n 2, e 6`, from labels and the real case. The subtitle follows the enumeration: "One explanation found at this point; the solve did not look for others", "Explanation 1 of 3; there are exactly 3", "Explanation 1 of 5 shown; more exist", or "Explanation 1 of 2 shown; the search for more timed out". *Sprint (prompt log entry 318):* when the search for extra explanations stops at its limit, the subtitle says "Explanation 1 of 5 shown; the solve stopped at 5, so there may be more", because a search that stops at its limit never checks for another (S6.17). The certification note says "Certified Pareto-optimal" or names the blocking undecided cells (06.C6). A requested point that is not sat (06.C9) gets no `WitnessView`, and its page says why: "unsat at this point" or "undecided: TIMEOUT" (S6.02).

`kept_by` applies psp's rule (S6.04) to in-witness groups: `subgraph` keeps all groups, including those outside the witness; `all` keeps slack ≠ null; `shortest` keeps slack = 0; `alt:W` keeps slack ≤ W. Drawn nodes are the ends of kept groups plus the two endpoints, as in psp. The stored slack is for the solve's source and target. When psp names other endpoints, which FuSION 1's psp allowed, `kept_by` repeats the two searches from them, at O(V_w + E_w); an endpoint that is not present keeps nothing, and the point is reported as having no path, as psp skipped it. Because S5.11 puts every selected edge on a source-to-target path, `all` with the solve's endpoints keeps every in-witness group. That is a free test of 06's encoding.

Cost per witness: O((V_w + E_w) log E_w), dominated by the grouping sort. The prototype on the full JUN run gives 1,058 honoured, 38 removed and 0 flipped groups over 13 points, and L from 5 to 9 hops.

Drawing rules shared by the page and the static export:
- **Fill and borders (S6.16).** Up-regulated `#8CCB4E`, down-regulated `#F2A24C`, unobserved `#DEDEDE`. Source border `#1B5E20` and target border `#0D3B66`, both 4 px; default border `#555555`, 1 px. A relaxed node has a dotted `[2,2]` border at least 2 px wide. The hover adds the members with their LFC values, and the flags SOURCE, TARGET, FSC, relaxed or inactive.
- **Activity** (07.C4, amending S6.16 and decision 0027 for the static page; prompt log entry 178). A present, inactive node keeps its observation fill and is drawn faded, at 45 % opacity through RGBA fill and border colours, with "inactive in this explanation" in the hover and the legend. The frontend's display stays Phase 5's.
- **Edges (S6.14, S6.15).** Activation is a grey `#8a8a8a` arrow and inhibition a red `#C62828` T-bar, 2 px. Removed is dashed `[8,6]`, 3 px. Flipped is purple `#8E44AD`, dotted `[2,6]`, with the surviving effect's head. A group outside the witness is drawn like a removed one, 1.5 px at 45 % opacity, with the hover note "not part of this explanation; counted because both ends are present". An ECrel group carries the edge label `EC` and the hover note "ECrel: enzymes linked through a compound".
- **Layout (S6.13).** Hierarchical left to right by level: level separation 260, node spacing 150, tree spacing 220, physics off, boxes with font 24 and maximum width 230. Same-level edges are curved, alternating clockwise and anticlockwise with roundness 0.6. A fixed layout seed and key-sorted data make the drawing repeatable.
- **Legend.** Generated from the same style table as the drawing, so it cannot drift from it (S6.16).

### Explanation export

`pex` writes into its step directory, `pex/` (09.C2), for each selected case and point (07.C2):
- `explain/[<prefix>_]<case>_n<n>_e<e>.json`, the view model, with `<case>` one of `in`, `out` and `free` (1.6). The prefix is empty in the named form; positional lines pass their `out_prefix`'s base name, for example `pex`. A positional line's case in the name comes from its slot: the incl files give `in`, the excl files `out`.
- `explain/[<prefix>_]<case>_n<n>_e<e>.html`, a page shell of about 1 KB with the same JSON inline;
- with `edge_table=true`, `explain/…_edges.tsv` (S6.08);
- with `static=svg`, `explain/….dot` and `explain/….svg` (S6.07, 07.C5);
- `lib/`, the page library, once per `pex` step (S6.18).

When one `pex` line draws several solves that share a case name, each file name starts with its solve's name. A positional `out_prefix` that is an absolute path, such as `$WRITE_DIR/pex`, is honoured as well (2B.3): the page shells are also written under `<prefix>_<case>_n<n>_e<e>.html`, with a copy of the library in `<prefix>_lib/`, and both locations are recorded in `run.json`. `psp` writes the same files into its own step directory, `psp/`.

```cpp
// backend/modules/results/include/fusion/results/explain.hpp
namespace fusion::results {

struct ExplainOptions {
  std::vector<Case> cases;                      // empty: every solved case
  PointSelector points;                         // default all
  PathFilter paths;                             // the page's initial view, the edge table and the static export
  std::size_t witness = 1;                      // shown first; the page offers the others
  bool edge_table = false;                      // S6.08
  bool static_svg = false;                      // S6.07, through Graphviz dot (07.C5)
  std::string prefix;                           // file-name prefix; positional lines pass "pex" or "psp_w"
  std::optional<std::filesystem::path> absolute_prefix;   // an absolute out_prefix, honoured too (2B.3)
  std::optional<SiteOptions> site;              // S6.19
};

void explain(std::span<const SolveResult* const>, const ExplainOptions&, core::Context&);

std::string view_json(const PointView&, const SolveResult&);            // deterministic
std::string page_html(std::string_view view_json, std::string_view lib_href, std::string_view title);
std::string edge_table_tsv(const WitnessView&, const SolveResult&, const PathFilter&);
std::string dot_source(const WitnessView&, const SolveResult&, const PathFilter&);    // escaped (S6.07)

struct RunPageInputs {                          // what run gathers from run.json at the end of a run
  std::vector<const SolveResult*> solves;       // every results.json the run wrote or read
  std::vector<std::string> pages, plots;        // relative paths of the pages and Pareto images
};
std::string run_page_html(const RunPageInputs&);   // the run page; run writes it once (1.4)

}  // namespace fusion::results
```

**View file.** One file per point (`PointView`), self-contained: the point's texts, its case's observation counts by `ObservationStatus` (zeros left out), the pathway names used, the initial path mode, and one entry per recorded witness. Each witness entry holds its counts, L, route text, drawn nodes with members and LFC values, drawn edges with their member edges' keys, types, subtypes and pathways, and the parked table. Abridged, the reference run's only point (6,353 bytes in full in the prototype):

```json
{"format": "fusion.view", "version": 1, "solve": "solve", "case": "in", "label": "with FSC (JUN)", "n": 1, "e": 0,
 "title": "EGFR → CXCL8 · with FSC (JUN) · n 1, e 0",
 "subtitle": "One explanation found at this point; the solve did not look for others.",
 "certification": "Certified Pareto-optimal.", "enumeration": "not_requested", "paths": "subgraph",
 "observations": {"counted": 1, "forced": 1, "exempt_endpoint": 1, "outside_corridor": 28, "not_in_network": 14},
 "pathways": {"path:hsa04010": "MAPK signaling pathway", "path:hsa04722": "Neurotrophin signaling pathway", …},
 "witnesses": [
  {"counts": {"n": 1, "e": 0}, "shortest": 8,
   "route": "Sign-consistent route: EGFR → GRB2 → SOS1+1 → RRAS2+5 → MAP3K1 → MAP2K7+1 → MAPK8 → JUN → CXCL8 (8 steps); JUN is on it.",
   "nodes": [{"key": "hsa5609", "label": "MAP2K7+1", "observed": "none", "present": true, "active": true,
              "relaxed": false, "level": 5, "roles": [],
              "members": [{"id": "hsa5609", "symbol": "MAP2K7", "lfc": -0.18}, {"id": "hsa6416", "symbol": "MAP2K4", "lfc": 0.2}]}, …],
   "edges": [{"source": "hsa4214", "target": "hsa5609", "effect": "activation", "draw": "honoured",
              "in_witness": true, "ecrel": false, "on_route": true, "slack": 0,
              "members": [{"key": "hsa4214|hsa5609|PPrel|activation", "type": "PPrel", "subtypes": ["activation"], "pathways": ["path:hsa04722"]},
                          {"key": "hsa4214|hsa5609|PPrel|phosphorylation", "type": "PPrel", "subtypes": ["phosphorylation"], "pathways": ["path:hsa04010"]}]}, …],
   "parked": [{"key": "hsa8660", "label": "IRS2", "observed": "up", "lfc": 1.39,
               "reason": "absent from this explanation; up-regulated, so it counts as one node relaxation"}]}]}
```

The full JUN run's 13 views measure 39,714 to 63,581 bytes, 656,266 bytes in all. When a point has several recorded witnesses, the view holds all of them and the page offers a switch (S6.02).

**Page shell.** It is generated directly (S6.17) and fetches nothing: the data is inline, the library is a relative script, and nothing comes from a CDN (S6.18).

```html
<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EGFR → CXCL8 · with FSC (JUN) · n 1, e 0</title>
<link rel="stylesheet" href="../lib/fusion-explain.css">
<script src="../lib/vis-network.min.js"></script>
<script src="../lib/fusion-explain.js" defer></script>
</head><body>
<script type="application/json" id="fusion-view">{"format":"fusion.view","version":1,…}</script>
<noscript>This page needs JavaScript. The same explanation is in in_n1_e0.json.</noscript>
</body></html>
```

- Every `<` inside the embedded JSON is written as the JSON escape `\u003c`, so no symbol can close the script element.
- The page has no inline executable script, so a frontend serving it under a Content-Security-Policy needs no `script-src` exception. vis-network's standalone build injects its stylesheet through a `<style>` element, so a strict `style-src` would have to allow it (prompt log entry 177).
- `fusion-explain.js` (our renderer, estimated at about 20 KB; not yet written) reads `#fusion-view` and builds the title, notes, legend, path-mode selector, witness switch, graph, parked table, observation summary and, when enabled, the edge table. It refits the graph on load and on resize, as pex did.

**Library** (07.C3, 11.C11). `pex/lib/` holds `vis-network.min.js`, `fusion-explain.js`, `fusion-explain.css` and `LICENSES.txt`. All four are compiled into libfusion as byte arrays with their `core::Sha256`, at the versions section 11 pins, and written out byte for byte through `OutputFile`. The export therefore needs no install-directory lookup and cannot pick up a stray copy. One copy serves every page of the step (S6.18).

Sizes for the full JUN run: FuSION 1's 13 pex pages take 9,645,947 bytes. 2.0 takes 656,266 bytes of views, the same again inline in 13 shells, about 680 KB of library, and 194,624 bytes of `results.json`: about 2.2 MB in all.

**Run page.** `index.html` at the top of the run directory is static HTML with no script, estimated at 5 to 10 KB. `run` writes it once, when the run ends, from `run_page_html` (cross-section item 1.4), so no committed output is ever rewritten. It shows:
- the title, each solve's settings (source, target, FSC set, bound, constraints used, box, seed) and its Pareto image when present;
- the comparison sentence (07.C7);
- one panel per solve and case, listing points in FuSION 1's order (n + e, then n), each linked to its page and marked when not certified;
- for a case without points, its status in the display words above;
- per case, the observation summary and the modelling statements (S5.12, S5.14, S5.16);
- links to each `results.json`.

**Edge table** (S6.08, TSV per E1; prompt log entry 180). One row per drawn edge of the filtered witness view, sorted by source label, target label and effect, listing all its edges by key. Lists inside a field are joined by `;`, in member order. Fields never contain a tab, CR or LF: any control character becomes a space. The prototype's tables for the full JUN run's 13 points measure 10,129 to 17,201 bytes; longer key lists can bring a point to about 21 KB. Two rows from the reference run:

```
source	source_key	target	target_key	effect	draw	in_witness	types	subtypes	edge_keys	pathway_ids	pathway_names
EGFR	hsa1956	GRB2	hsa2885	activation	honoured	yes	PPrel	activation	hsa1956|hsa2885|PPrel|activation	path:hsa05210;path:hsa05215	Colorectal cancer;Prostate cancer
MAP3K1	hsa4214	MAP2K7+1	hsa5609	activation	honoured	yes	PPrel	activation;phosphorylation	hsa4214|hsa5609|PPrel|activation;hsa4214|hsa5609|PPrel|phosphorylation	path:hsa04010;path:hsa04722	MAPK signaling pathway;Neurotrophin signaling pathway
```

**Static export** (S6.07, on request, 07.C5). The DOT text is generated from the filtered witness view with the same colours and styles as the page: `rankdir=LR`; `arrowhead=normal` or `tee`; `style=dashed` or `dotted`; RGBA colours for faded nodes and edges; `label="EC"` on ECrel groups. Inside every quoted DOT string, `\` and `"` are escaped. 11's `core::run_process` (2A.11) runs `dot -Tsvg -o in_n2_e6.svg.tmp in_n2_e6.dot`, with an argument vector and no shell, in the `explain/` directory, so no path reaches a command line; the SVG is then committed through `OutputFile`. `dot` is found by 11's lookup (`FUSION_DOT`, then `PATH`; it is not bundled). Measured here on the full JUN run's (n 3, e 4) witness (86 nodes, 109 drawn edges, Graphviz 14.1.2): 0.04 s per run over five runs (a first, cold run took 0.21 s) and 63,827 bytes of SVG. The `.dot` file is kept beside the SVG, as psp did. Abridged, the reference run's point:

```
digraph "in_n1_e0" {
  rankdir=LR;
  node [shape=box, style=filled, fontname="Helvetica", color="#555555", fillcolor="#DEDEDE"];
  "hsa1956" [label="EGFR", color="#1B5E20", penwidth=4];
  "hsa3576" [label="CXCL8", fillcolor="#8CCB4E", color="#0D3B66", penwidth=4];
  "hsa3725" [label="JUN", fillcolor="#8CCB4E"];
  "hsa5599" -> "hsa3725" [color="#8a8a8a", arrowhead=normal, penwidth=2];
  "hsa3725" -> "hsa3576" [color="#8a8a8a", arrowhead=normal, penwidth=2];
}
```

### Pareto plot step

`plot_pareto` draws the run's own frontier (S6.22) into its step directory: `plot_pareto/pareto.png`, and the gnuplot script it ran, `plot_pareto/pareto.gp`. The script is self-contained, so `gnuplot pareto.gp` in that directory redraws the plot without a re-solve.

```cpp
// backend/modules/results/include/fusion/results/pareto.hpp
namespace fusion::results {
struct ParetoOptions { std::string font = "Liberation Sans"; };
double dodge_offset(std::int32_t edge_upper) noexcept;     // 0.10 * (edge_upper + 1) / 9
std::string gnuplot_script(std::span<const SolveResult* const>, const ParetoOptions&);   // pure, deterministic
void plot_pareto(std::span<const SolveResult* const>, const ParetoOptions&, core::Context&);
}
```

The script keeps plot_po.sh's look (S6.23): `pngcairo enhanced` 980×690 on `#fcfcfb`, the title and axis styling, blue `#2a78d6` circles for the `in` case and red `#e34948` squares for the `out` case, background-coloured halos under the markers, and a boxed key at top right. The colours come from the plot palette in `core` (2B.13), which 04's histogram shares. It differs in these ways:
- **Data inline.** Each case's points are datablocks inside the script (gnuplot 5.0 or later), so no temporary files are needed. This replaces plot_po.sh's `mktemp` and awk passes and their GNU-only `--suffix`.
- **One, two or three curves** (S6.22). A solve without an FSC gives one blue `free` curve; a `free` case beside `in` and `out` gets a grey third curve.
- **Labels from the run.** Source, target and FSC come from 02's labels and keys (07.C10), not typed by hand. Enhanced-text characters (`_ ^ { } @ & ~ \`) and double quotes in labels are escaped.
- **Subtitle** (S6.23). It lists only the constraint families the run used, with the strict test the solver applies (S5.15, S5.16): `FSC JUN (hsa3725) · bound 10 · coexpression > 700, frozen > 800 · n ≤ 8, e ≤ 8`. plot_po.sh printed `≥` and both thresholds whether used or not.
- **Dodge** (S6.24). A point present in both the `in` and the `out` case moves up by `dodge_offset(e_ub)` on the `in` curve and down on the other. The plot area is fixed by explicit screen margins (left 0.085, right 0.975, bottom 0.105, top 0.865), so a constant on-screen offset is 0.10 × (e_ub + 1) / 9 data units. That is exactly plot_po.sh's 0.10 at the bound of 8 that all its callers used, and 0.178 at 15.
- **Uncertified points** (07.C6) are drawn with open markers, by 06.C6's rule.
- **Cases without a curve** get a key entry in the display words above: "no Pareto point within the bounds" (S6.23's wording), "infeasible: …", "not solved in this run". A `stopped` or `budget_exhausted` case draws its points so far and adds "(partial)" to its label. A points request (06.C9) has no frontier; its sat points are drawn as markers without a line.
- **Axes** use the requested box, [−0.5, ub + 0.5] with integer tics, so plots in a sweep share their scales.

A gnuplot 6.0 prototype of this script on the full JUN run took 0.15 to 0.18 s and 43 MB to draw the PNG over repeated runs (0.05 s for SVG). Two runs gave byte-identical PNG files (same SHA-256). An excerpt:

```gnuplot
set encoding utf8
set terminal pngcairo enhanced size 980,690 font "Liberation Sans,13" background rgb "#fcfcfb"
set output 'pareto.png.tmp'
set lmargin at screen 0.085; set rmargin at screen 0.975; set bmargin at screen 0.105; set tmargin at screen 0.865
$out << EOD
2	7
3	5
EOD
$out_open << EOD
7	1
8	0
EOD
set xrange [-0.5:8.5]; set yrange [-0.5:8.5]
plot $in_line u 1:2 w lines ls 11 notitle, $out_line u 1:2 w lines ls 12 notitle, \
     $in u 1:2 w points ls 11 title "with FSC (JUN)", $out u 1:2 w points ls 12 title "without FSC (JUN)", \
     $out_open u 1:2 w points ls 14 notitle, keyentry w points pt 6 lc rgb "#52514e" title "open marker: not certified"
```

The step writes `pareto.gp` through `OutputFile`, then hands the script to core's gnuplot runner (2B.13), which runs gnuplot through `core::run_process` in the step directory with 11.C10's `FONTCONFIG_FILE` and `PANGOCAIRO_BACKEND=fc`, and checks both the exit status and that the image was written, since a gnuplot without the asked terminal warns, draws nothing and exits 0 (11). Only the relative names `pareto.png.tmp` and the script reach gnuplot, which avoids its string-escaping rules for Windows paths and non-ASCII directory names. The image is then committed as `pareto.png` through `OutputFile`.

**Requirements** (2A.1). `plot_pareto` declares in 01's command table: gnuplot 5.4 or later with the `pngcairo` terminal, and the font Liberation Sans. 11's check probes them once before the first step, looking in `FUSION_GNUPLOT`, then the bundle (gnuplot 6.0.5, 11.C9), then `PATH` (1.27). A missing gnuplot or terminal is the failure `deps.missing`. A missing font is the warning `deps.font_missing`, not a failure (07.C8): with the bundle it cannot happen, because 11.C10 ships Liberation Sans 2.1.5 with a `fonts.conf` that lists only the bundled fonts.

### Site report

`pex` with `site_dir` publishes its solves into a site directory (S6.19). Every option is a named argument, and the positional tokens 12 to 14 still work. This is the one write outside the run directory (2B.3). Layout:

```
<site>/index.html                   landing page, rebuilt from every <key>/manifest.json
<site>/lib/                         one copy of the page library for the whole site (S6.18)
<site>/<key>/index.html             the run page, with site-relative links
<site>/<key>/manifest.json
<site>/<key>/pareto.png
<site>/<key>/explain/*.html         page shells pointing at ../../lib/
```

```cpp
// backend/modules/results/include/fusion/results/site.hpp
namespace fusion::results {
struct SiteOptions {
  std::filesystem::path dir;
  std::string key;                              // default: the run directory's name
  std::string run_title;                        // default "<source> → <target>" from labels
  std::string takeaway;                         // default: the comparison sentence
  std::string site_title = "FuSION explanation subgraphs";
  std::optional<std::filesystem::path> pareto_png;   // positional token 14; default the solve's own plot
  bool replace = false;
};
void validate_site_key(std::string_view);       // batch check: batch.bad_value
void publish_site(std::span<const SolveResult* const>, const SiteOptions&, core::Context&);
}
```

Rules:
- **Shells are regenerated.** The site's page shells and run page are written from the view JSON with the site's relative paths; nothing edits HTML. Regenerating costs microseconds per page.
- **The key** must match `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`, must not be a Windows device name (`CON`, `PRN`, `AUX`, `NUL`, `COM0` to `COM9`, `LPT0` to `LPT9`, in any case, with or without an extension; the superscript-digit forms cannot pass the pattern) and must not end in a dot. It is checked before the run.
- **Replacing.** Publishing the same run again under its key replaces it. A key that holds another run, by `run_id` from `run.json` and compared case-insensitively for macOS and Windows file systems, is an error unless `replace=true`.
- **Locking and replacing.** The update takes the site's lock file through 09's `Lock`, which can lock any file (2B.17), and writes the new `<key>/` into `<key>.tmp-<n>/` through `OutputFile`, with 09's Windows retry rule (1.28). None of the three systems lets a directory be renamed over a non-empty one, so an existing `<key>/` is first renamed to `<key>.old-<n>/`, the new directory is renamed into place, and the old one is then deleted. A crash between those steps leaves the `.old` or `.tmp` directory; the next update, under the lock, finishes or undoes the interrupted step before anything else. The update then rebuilds `index.html` by writing a temporary file and renaming it. Two runs publishing at once therefore both appear on the landing page.
- **Old manifests.** The landing page also reads FuSION 1 manifests (no `format` field; fields `key`, `title`, `takeaway`, `png`, `incl`, `excl`), so adding a 2.0 run to the existing three-run site in `misc/FuSION-explanations` keeps its cards. That site's FuSION 1 pages keep their inline libraries.

Manifest (2.0):

```json
{"format": "fusion.site-run", "version": 1, "key": "jun_b10", "run_id": "r-20261112T184107Z-3f9a1c2e",
 "title": "EGFR → CXCL8 · FSC = JUN (bound 10)", "takeaway": "", "png": "pareto.png",
 "solves": [{"name": "solve", "cases": [
   {"case": "in", "label": "with FSC (JUN)", "status": "complete", "points": [[2, 6], [3, 4], [4, 3], [5, 2], [6, 1], [7, 0]], "uncertified": []},
   {"case": "out", "label": "without FSC (JUN)", "status": "complete", "points": [[2, 7], [3, 5], [4, 4], [5, 3], [6, 2], [7, 1], [8, 0]], "uncertified": [[7, 1], [8, 0]]}]}],
 "comparison": {"verdict": "with_cheaper", "delta_n_at_e0": 1, "certified": false}}
```

With no takeaway, the card shows the comparison sentence: "forcing JUN in saves 1 node at e = 0 (not certified)". The published site's hand-written takeaway for this run said the same in prose, including the uncertified corner.

### FuSION 1 text formats

**The two-column file** (S5.25, as amended by 06.C20) is written for every solve, whatever else is asked, as `pathz3/<case>.dat` (for example `in.dat`): one `n<TAB>e` line per point, n descending as in FuSION 1, with no filter, and an empty file when the case has no point (S5.26). Since 06.C20 the points are the Pareto-minimal witness counts. It costs about 100 bytes per case (24 and 28 bytes in the full JUN run), and `plot_po.sh` still runs on it.

**The other FuSION 1 files** are written only when the run setting `legacy_prefix` asks for them (E1; cross-section items 1.21 and 4A.9, prompt log entry 179). A positional `pathz3` line sets it from its prefix, and a named line or the `run` line sets it by name; 06's `pathz3` row owns the setting, and this section owns the formats. For a stem P, the solve writes `<P>_solutions.txt` and `<P>_relaxations.txt` (with several cases, `<P>_<case>_solutions.txt` and so on) into `pathz3/legacy/`, and also at P's own place when P is absolute, by 2B.3's rule. A batch's two positional `pathz3` lines, `$WRITE_DIR/temp_w` and `temp_wo`, therefore give FuSION 1's four familiar files.

**FuSION 1's moved files** (01.C11, amending S1.14 for six `exec mv` lines; prompt log entry 179). When the interpreter recognises `exec mv PO.dat <dest>` it calls `two_column` and writes the latest solve's two-column file at `<dest>`; for `exec mv limits_timefile.txt <dest>` it calls `legacy_limits` and writes FuSION 1's limits rendering there (prompt log entry 230), because the user's `compare_sweep.py` and `compare_candidates.py` parse that format.

The format is S5.27's, with its fixes:
- Column 1 holds the stable key (S4.14): the rep id on node lines, `src|tgt|type|subs` on edge lines. FuSION 1's parsers keep it as text.
- An absent node's activity is `NA`, which keeps the six fields that FuSION 1's parsers count. Absent nodes appear only in the relaxations file, as in FuSION 1.
- The subtype field joins subtypes with `_`, as FuSION 1 did, so FuSION 1's polarity split still works.
- An uncertified point's header gains a suffix that FuSION 1's parsers ignore, since they read fields 3 and 5 (S5.19): `Current PO point: 7 nodes, 1 edges relaxed (not certified: blocked by n=7 e=0, TIMEOUT)`.
- Only recorded witnesses are written, so there is no empty `Solution 2`. `No more solutions. Obtained unsat` follows only a complete enumeration.
- A case without points gets empty files, which exist (S5.26).
- The limits rendering takes 06's answers in order, three lines per check (`Relax bounds: <e> <n>`, or the `(node call)` or `(edge call)` form; `Time taken (sec): <cpu>`, written by `core::format_number`; `sat`, `unsat` or `unknown`), plus S5.19's TIMEOUT and UNDEFINED lines. That is everything the user's scripts read.

The reference run's `in` case, in `temp_w_solutions.txt`:

```
Current PO point: 1 nodes, 0 edges relaxed
Solution 1
hsa1956	hsa1956	EGFR	not_diff_expr	present	active
…
```

and in `temp_w_relaxations.txt`:

```
Current PO point: 1 nodes, 0 edges relaxed
Solution 1
hsa8660	hsa8660	IRS2	up_reg	not_present	NA
```

```cpp
// backend/modules/results/include/fusion/results/legacy.hpp
namespace fusion::results {
std::string two_column(const CaseResult&);                     // the .dat text (S5.25)
std::string legacy_solutions(const SolveResult&, Case);        // S5.27's format
std::string legacy_relaxations(const SolveResult&, Case);
std::string legacy_limits(const solve::CaseResult&);           // FuSION 1's rendering of 06's answers
void write_legacy(const SolveResult&, const solve::Results&, std::string_view legacy_prefix, core::Context&);
}
```

### Commands and settings

Section 01 owns the command table; this section supplies three entries, with their tool requirements (2A.1, 2B.15). No setting here changes a frontier or a witness. All are output settings, recorded in `run.json` like every other (E1). Registry keys are `results.<name>`, and the bare name is the argument (1.16).

| Command | Setting | Type | Default | Spec |
|---|---|---|---|---|
| `pex` / `plot_explanation` | `solve` | solve name(s) or step id | the latest solve | 07.C9 |
| | `cases` | list of `in`, `out`, `free` (positional: `both`, `incl`, `excl`) | every solved case | S6.10, 1.6 |
| | `points` | `all`, `least`, `<n>,<e>` | `all` | S6.03 |
| | `paths` | `subgraph`, `all`, `shortest`, `alt[:W]` (W ≥ 0, default 2) | `subgraph` | S6.04, S6.26 |
| | `witness` | integer ≥ 1 | 1 | S6.02 |
| | `edge_table` | bool | false | S6.08 |
| | `static` | `none`, `svg` | `none` | S6.07, 07.C5 |
| | `prefix` | file-name prefix | empty | S6.01 |
| | `site_dir`, `run_key`, `run_title`, `takeaway`, `site_title`, `replace`, `po_png` | path, string, string, string, string, bool, path | none, run directory name, `<source> → <target>`, comparison sentence, `FuSION explanation subgraphs`, false, the solve's plot | S6.19 |
| `psp` / `plot_solution_paths` | `solution`, `src`, `tgt`, `mode`, `points`, `prefix` (S6.01's six) | solve file, node id, node id, path mode, selector, prefix | all required | S6.01 |
| `plot_pareto` | `solve` | one or more solve names | the latest solve | S6.22 |
| | `font` | font family | `Liberation Sans` | S6.23 |

`legacy_prefix` (registry key `solve.legacy_prefix`) belongs to 06's `pathz3` row; its files are described above.

Tool requirements for 01's table: `plot_pareto` needs gnuplot 5.4 or later with `pngcairo`, and the font (07.C8's rule); `pex` and `psp` with `static=svg` need Graphviz `dot`.

How the old lines map:
- **`psp`** renders one case of the solve bound to `solution` (07.C9), with `paths=mode`, `points`, `static=svg`, `edge_table=true` and the prefix's base name. `src` and `tgt` are the path filter's endpoints, which FuSION 1's psp let differ from the solve's; one that is not present in a witness gives that point "no path", as in FuSION 1 (S6.04).
- **`pex`, positional.** Its four file tokens are bound to solves the same way (07.C9). Its `src` and `tgt` must equal the bound solve's source and target (07.C12). Its `lfc_file` and `pathmap_file` are checked as readable (S6.12, decision 0021); the page draws the values the solve used (4A.13), and the line may add 04.C9's reading arguments only to say how its `lfc_file` is read (1.23). When that value input (file and reading arguments) differs from the solve's by path, SHA-256 or arguments, a warning names both. Names come from the run's pathway names (S6.12, entry 205). `out_prefix`'s base name becomes `prefix`, and an absolute `out_prefix` is honoured as well (2B.3).

### Errors

Failures are `core::Failure` exceptions with a stable code (09.C14); they stop the batch with a non-zero exit (S1.28). Those marked "check" are found before the first step (decision 0021). Generic conditions take 09's and 01's codes (1.17).

| When | Code | Example message |
|---|---|---|
| check | `batch.bad_value` | `pex (line 269): points '7,1x' is not all, least or <n>,<e>` |
| check | `batch.bad_value` | `psp (line 12): path mode 'alt:-1': the window must be a whole number of at least 0` |
| check | `batch.bad_value` | `pex (line 270): run_key 'CON' is a reserved file name on Windows` |
| check | `batch.unmatched_file` (01) | `pex (line 269): …/temp_w_solutions.txt was not written by a pathz3 line of this batch` (07.C9) |
| check | `deps.missing` (11) | `plot_pareto (line 262) needs gnuplot 5.4 or later with the pngcairo terminal; none found (FUSION_GNUPLOT unset; no bundle; not on PATH)` |
| check | `input.unreadable` (09) | `pex (line 269): lfc_file …/egf_hela_t120_lfc.tsv cannot be read: No such file or directory` |
| check | `output.clash` (09) | two lines that would write one output, found by file identity (2A.7) |
| check, or before the step | `results.endpoint_mismatch` | `pex (line 269): src hsa1950 is not the source of solve temp_w (hsa1956)` (07.C12) |
| run | `results.no_such_point` | `pex: with FSC (JUN) has no point n=7 e=1; its points are n=2 e=6, n=3 e=4, …` |
| run | `results.gnuplot_failed`, `results.dot_failed` | `plot_pareto: gnuplot exited with status 1: "pareto.gp" line 9: invalid expression` (with the last stderr lines, 11) |
| run | `io.write` (09) | `cannot write pathz3/results.json: No space left on device` |
| run | `results.internal` | `internal error: edge hsa1843|hsa5594|PPrel|inhibition,dephosphorylation is relaxed in the witness, but its ends (active, inactive) honour it` |
| run | `artefact.newer_format` (09) | `…/pathz3/results.json: format fusion.results version 2 is newer than this FuSION reads (1)` |
| run | `results.site_key_taken` | `site …/FuSION-explanations: key 'jun_b10' holds another run; add replace=true to replace it` |
| run | `results.site_locked` | `site …/FuSION-explanations: locked by another run for 60 s` |

The endpoint check runs before the first step when the solve's endpoints are on its `pathz3` line; when they come from a file the batch writes (`exec echo … > $CONNECT_PAIRS_FILE`), it runs just before the `pex` step (01.C12).

Not failures, and not reported results either (cross-section items 3A.14 and 1.8, prompt log entry 167): a case's status `no_point`, `infeasible`, `no_de_gene`, `budget_exhausted` or `not_requested` is part of the results. The batch goes on, the plot and the pages run, the console, `results.json`, the run page and the plot key show the status in its display words, and `pex` draws nothing for a case without points and says why. FuSION 1 exited 1 when pex drew nothing at all; under S6.21 that is a result. A requested point that is not sat (06.C9) gets a page that says so (S6.02).

A Stop request is `core::Stopped` (09.C14): the solve's results are published with `stopped` cases, and the run ends with status 3 (A1).

Warnings go through `core::Log::warning` and the `warning` event: a missing plot font (`deps.font_missing`, from 11's dependency check; 07.C8), and a positional `pex` value input that differs from the solve's (07.C12).

### Files written in a run directory

09 owns the layout (09.C2); these are this section's files in it (cross-section item 1.4).

| Path | Written by | When | Size (full JUN run) |
|---|---|---|---|
| `pathz3/results.json` | `publish_solve`, after the solve step | every solve, also a stopped one | 194,624 bytes |
| `pathz3/<case>.dat` | `publish_solve` | every solve (S5.25) | 24 and 28 bytes |
| `pathz3/legacy/` | `publish_solve` | with `legacy_prefix` (1.21) | as FuSION 1 |
| `plot_pareto/pareto.png`, `pareto.gp` | `plot_pareto` | per call | 58 KB, 2 KB |
| `pex/explain/<case>_n<n>_e<e>.json` and `.html` | `pex` | per selected point | 40 to 64 KB each |
| `pex/explain/…_edges.tsv` | `pex edge_table=true` | per point | 10 to 21 KB |
| `pex/explain/….dot`, `….svg` | `pex static=svg` | per point | 64 KB of SVG |
| `pex/lib/` (4 files) | `pex` | once per `pex` step | about 680 KB |
| `index.html` | `run`, from `run_page_html` | once, when the run ends | 5 to 10 KB (estimate) |

Every file goes through 09's `OutputFile`: written to a temporary name and renamed when committed, with 09's Windows retry rule, so a crash or a Stop never leaves half a file (2A.6, 1.28). An external tool writes the temporary file itself (`pareto.png.tmp`, `in_n2_e6.svg.tmp`), and the step commits it. 09's check before the run refuses two outputs that are one file (2A.7). Paths inside files are relative and use `/`, so a run directory can be moved, zipped or published.

### Platforms

- **Child processes.** gnuplot and dot run through 11's `core::run_process` with an argument vector, never a shell (2A.11). Only fixed ASCII relative names appear as arguments, so spaces and non-ASCII characters in the run directory never need quoting; the working directory is passed as a wide string on Windows.
- **Line endings.** All text files are written in binary mode with LF line endings, on Windows too, so outputs are byte-identical across platforms (09.C16). gnuplot, browsers and the frontend read LF everywhere.
- **Renames.** 09's `OutputFile` replaces files with its Windows retry rule (1.28). A directory cannot be renamed over a non-empty one on any of the three systems, which the site's replace protocol allows for.
- **Long paths.** The longest name added here is `pex/explain/out_n15_e15_edges.tsv` (33 characters); 11's `long_path` covers deep run directories on Windows.
- **Fonts.** The bundled gnuplot draws with the bundled Liberation Sans through 11.C10's `fonts.conf` on every platform. With a system gnuplot, 11's check asks `fc-match` where fontconfig exists; macOS does not ship it. Whether the official Windows gnuplot honours `PANGOCAIRO_BACKEND=fc` is a Phase 4 check (11).
- **Browsers.** Pages never fetch a file, so they open from `file://` in Chrome, Edge, Firefox and Safari alike.

### Determinism

- Nothing here is random. Neighbours are taken in key order in every search, so routes and levels are repeatable, and the page fixes the layout seed.
- Nodes, edges, observations and pathway ids are sorted by key with `core::stable_compare`, independent of locale; points by n; cases as in, out, free.
- `results.json`, views, shells, TSVs, `pareto.gp`, DOT files and the legacy text contain no times and no absolute paths. Given the same `SolveResult`, they are byte-identical on every platform (09.C16).
- The PNG is byte-identical across runs with the same gnuplot and fonts; tests compare `pareto.gp`.
- The page assets are fixed by their compiled-in `core::Sha256`, recorded in `run.json` with gnuplot's and dot's versions (E1).

## Choices

### 07.C1 · Structured results: one normalised `results.json` per solve

- **Proposed:** one file per solve, `pathz3/results.json` (placement by cross-section item 1.4), holding all its cases, one table of every node and edge that a witness, an observation or the header refers to, and witnesses as sorted lists of node keys and edge keys (02.C11). The whole corridor stays in 05's artefact, referenced from the corridor summary. It is a final output; the solve's saved artefact is 06's journal and `case.json` (2A.2). This reads S5.30's "named by solve case" as met by the case labels inside the file and the per-case file names (`in.dat`, `in_n2_e6.html`), which cross-section item 4A.12 confirms (prompt log entry 177).
- **Alternatives:**
  - Each point carries complete node and edge records, so a reader needs only that point.
  - One results file per case, as S5.30 can be read literally; the comparison (07.C7) then needs a file of its own.
  - The whole corridor's tables inside `results.json`.
- **Why:** on the full JUN run the normalised file is 137,225 bytes (194,624 indented). Per-point records take 530,032 bytes (934,066 indented), because each node and edge is repeated at every point that uses it. One file per case takes 127,292 and 131,147 bytes indented, 258,439 in all, 33 % more, because the cases share most of their nodes and edges. Both cases share one corridor (S5.17), so one table serves both. The corridor itself (up to 793 nodes and 3,328 edges in the run logs) is already 05's artefact. The per-point form the display needs is the view model (07.C2).
- **Decision:** as proposed (prompt log entry 177)

### 07.C2 · Explanation pages: one page per point plus a run page, with the view as JSON beside each page and inline in it

- **Proposed:** for each point, a view file `pex/explain/<case>_n<n>_e<e>.json` and a page shell of about 1 KB with the same JSON inline, the library in `pex/lib/`. One run page, `index.html`, lists every point; `run` writes it once, when the run ends (cross-section item 1.4). Pages fetch nothing, so they work from `file://`.
- **Alternatives:**
  - One viewer page per run with every point inline and a point picker. That is one file, but the full JUN run would load 656,266 bytes of data to show one point, and links become fragments such as `#in-n2-e6`.
  - Pages that fetch their JSON. Chrome and Edge block `fetch` of `file://` URLs from a `file://` page, and Firefox does by default, so these pages would work only when served over HTTP.
  - HTML only, with no separate JSON. That halves the per-point bytes, but the Phase 5 display would have to parse JSON out of HTML or derive its own draw rules, a second renderer (S6.26).
- **Why:** FuSION 1 users know one page per point (`pex_incl_n2_e6.html`), and S6.19's run page links each point to its page. Each page loads only its own 40 to 64 KB. The duplicate inline copy costs 656 KB per run in the largest case measured, against FuSION 1's 9.6 MB of pages for the same run. The separate JSON is what the display can draw directly ([decision 0027](../decisions/0027-graph-display-in-phase-5.md)). Writing the run page once, at the end, keeps 09's rule that no committed output is rewritten.
- **Decision:** as proposed (prompt log entry 177)

### 07.C3 · Drawing library for the static page: vis-network 10.x, compiled into libfusion

- **Proposed:** vis-network's standalone build, at the release section 11 pins: 10.1.5 today, 653,713 bytes, licensed Apache-2.0 OR MIT. 11.C11 compiles it, `fusion-explain.js`, `fusion-explain.css` and `LICENSES.txt` into libfusion as byte arrays with their SHA-256 (cross-section item 3A.22; prompt log entry 177), and each `pex` step and each site gets one copy (S6.18). If Phase 3's check of the full JUN run's 13 points shows a visible layout change from 9.1.2, 9.1.2 is used instead.
- **Alternatives:**
  - Cytoscape.js 3.35.0 (MIT, 442,752 bytes) with a layered-layout extension (cytoscape-dagre and dagre, MIT). Its styling is richer and it can draw complexes as compound nodes, but S6.13's hierarchical layout would have to be rebuilt and tuned on a different engine.
  - No library: C++ computes a layered layout and writes SVG, with about 10 KB of our own script for zoom and hover. That means no third-party code and the smallest pages, but crossing reduction and block shifting would have to be written from scratch, and S6.07 already keeps Graphviz for static images.
  - vis-network 9.1.2, the exact copy inside FuSION 1's pages (468,813 bytes of script and 220,163 of CSS, 205,359 of which are 13 inline images).
  - The assets as files in the installation, which could be patched without a rebuild.
- **Why:** S6.13 to S6.16 keep pex's layout and drawing. pex drew with vis-network, so the same options reproduce them with no porting risk. The licence allows shipping with no conditions beyond the notice. Compiled in, the assets cannot go missing or drift. The view JSON does not depend on the library, so Phase 5 can choose another for the frontend without touching the backend.
- **Decision:** as proposed (prompt log entry 177)

### 07.C4 · Faded inactive nodes on the static page (amends S6.16 and decision 0027 for the static page; accepted in prompt log entry 178)

- **Proposed:** for the static page only, keep the observation fill (S6.16) and draw a present, inactive node faded, at 45 % opacity through RGBA fill and border colours, with "inactive in this explanation" in the hover and in the legend. The DOT export uses the same colours. The frontend's display stays Phase 5's. This amends S6.16 and decision 0027, which left how activity is drawn to Phase 5's display design.
- **Alternatives:**
  - S6.16 and decision 0027 as written: activity shows in the hover only, as in FuSION 1, until Phase 5.
  - Invert the node: white fill, with the observation colour moved to a thick border.
  - A text badge in the label, such as `CDK2 (off)`.
- **Why:** S6.06 requires the page to show each node's activity. Inactive present nodes are rare (median 1, maximum 6 per witness in the census), so a quiet cue is enough. Fading keeps the fill's meaning and the layout, and it reads correctly for S6.06's motivating case: a down-regulated gene switched off through an inhibition shows as a faded orange node at the end of a red T-bar. With a hover-only cue, the 302 present nodes that S6.06 found missing from 569 FuSION 1 renders would now be drawn but would look active until hovered.
- **Decision:** as proposed, amending S6.16 and decision 0027 for the static page (prompt log entry 178)

### 07.C5 · Static image export through Graphviz: only on request, not bundled

- **Proposed:** `static=svg` on `pex` (or any `psp` line) writes DOT and SVG per point through Graphviz `dot`. It is off by default, `dot` is checked before the run only when a batch asks for it, and section 11 does not bundle Graphviz (cross-section item 3A.23).
- **Alternatives:** on by default in the standard pipeline, so every run needs Graphviz and section 11 bundles it on three platforms.
- **Why:** S6.07 keeps the static export through `dot`; the user turned down a Graphviz-free export in that poll (prompt log entry 076). No batch has ever called psp, and the interactive page plus its view file already cover viewing. Graphviz is EPL-1.0, and its environment on this machine is 312 MB. Bundling and signing it on three platforms for a rarely used feature is out of proportion. When it is used, it costs about 0.04 s per point of 86 nodes.
- **Decision:** as proposed (prompt log entry 178)

### 07.C6 · Uncertified points on the Pareto plot

- **Proposed:** draw every point that 06.C6 leaves uncertified with an open marker of its curve's shape and colour, on the same halo, plus one key entry, "open marker: not certified".
- **Alternatives:** keep FuSION 1's markers and leave certification to `results.json`, the console and the pages.
- **Why:** the plot is the run's headline figure, and S5.19 makes provisional points a result to show. The published site's hand-written takeaway for the JUN run had to explain in prose that its (n 7, e 0) corner was "not machine-certified". By 06.C6's rule, 23 of 154 frontier points in 8 of FuSION 1's 34 bound-10 solves are uncertified (prompt log entry 172). The gnuplot 6.0 prototype draws it in the same 0.15 to 0.18 s, and the rest of S6.23's look is unchanged.
- **Decision:** as proposed (prompt log entry 178)

### 07.C7 · FSC comparison in the results

- **Proposed:** when the `in` and `out` cases are both solved, `results.json`, the console, the run page, the site card and the `run.json` summary (2B.7) carry a verdict (`identical`, `with_cheaper`, `with_costlier`, `mixed`, `essential`, `with_impossible`, `no_points`), `delta_n_at_e0`, and whether the verdict is certified.
- **Alternatives:** leave the comparison to the user's own scripts, as now.
- **Why:** S5.17 calls the comparison of the two curves the scientific result and notes that FuSION 1 knows nothing of it. The user's `compare_sweep.py` and `compare_candidates.py` each recompute it, and the FDR sweep was summarised in exactly these terms: forcing JUN "costs 1" at fdr 0.4, "saves 1" in the full graph, "identical" at 0.3 and 0.5, essential at 0.25. It costs O(P + E) on at most about 20 points, and the certification flag stops a provisional verdict passing as settled.
- **Decision:** as proposed (prompt log entry 178)

### 07.C8 · A missing plot font is a warning; the bundle ships the font

- **Proposed:** the bundled gnuplot draws with the bundled Liberation Sans 2.1.5 through 11.C10's `fonts.conf` (cross-section item 3A.21; prompt log entry 179). If the font is still not available, as can happen with a system gnuplot, 11's dependency check prints the warning `deps.font_missing` before the first step (prompt log entry 230) and gnuplot substitutes; fail only when gnuplot or its `pngcairo` terminal is missing.
- **Alternatives:**
  - Use whatever "Liberation Sans" resolves to on the system (usually Arial on Windows and macOS, with the same metrics), and warn on substitution.
  - A missing font fails the run before it starts.
- **Why:** S6.23 asks for the font to be checked so the substitution is no longer silent. A substitute changes only the plot's look, never a result, and failing a solve that may run 50 hours over a font is out of proportion. fontconfig does substitute silently: `fc-match` returns `Noto Sans` for an unknown family here. Liberation Sans is not installed by default on macOS or Windows, so shipping it makes the plot look the same everywhere.
- **Decision:** as proposed (prompt log entry 179)

### 07.C9 · How positional `pex` and `psp` lines reach a solve's results

- **Proposed:** each solve has a name, `solve` by default or the prefix of a positional `pathz3` line (`$WRITE_DIR/temp_w`). A positional file argument such as `$WRITE_DIR/temp_w_solutions.txt` binds to the solve with that prefix, in memory or loaded from a parent run's `results.json`. A file argument that matches no solve of the batch is an error before the run, section 01's `batch.unmatched_file` (prompt log entry 230). 01.C13's reuse of files written earlier in the run keeps only its other cases, `wgx` → `rgx` and `cudf` → `pathz3` (cross-section item 3A.30; prompt log entry 179).
- **Alternatives:** write FuSION 1's text dumps for every positional `pathz3` and parse them back with a second, legacy reader. That would also re-render old FuSION 1 run directories, but it brings back two readers of one result.
- **Why:** S6.02 asks for one documented reader. In the batch census, all 51 pex lines (in 51 of the 116 batch files) name files whose prefix is that of an earlier `pathz3` line in the same batch, so binding by name covers every real use with no parser. No batch calls psp.
- **Decision:** as proposed (prompt log entry 179)

### 07.C10 · Labels show family size

- **Proposed:** wherever a node is named (pages, routes, plot titles, tables, console), a merged box shows its representative symbol plus the number of other members, `MAP2K1+1` or `RRAS2+5`. A complex lists its members, `[STAT1+6]`. The hover lists every member with its LFC value.
- **Alternatives:** the representative's symbol alone, as in FuSION 1 (`RRAS2` for the six-gene RAS box).
- **Why:** a merged box's label is its lexically smallest member (S2.03, S2.07), so FuSION 1 printed the 36-gene receptor-tyrosine-kinase box as `TFG` and the RAS family as `RRAS2`. The user's candidate-run report had to add a hand-made relabelling table because route nodes were being read as single genes. In the reference run's one witness, 3 of 9 present nodes are boxes (RRAS2+5, MAP2K7+1, SOS1+1). Section 02 owns labels; this choice fixes what results needs from them.
- **Decision:** as proposed (prompt log entry 180)

### 07.C11 · Edge identity in results: 02's key with the type (settled by 02.C11; this choice lapses)

- **Proposed:** witnesses, views and edge tables name every edge by 02's key: source key, target key, type and ordered subtypes, printed `src|tgt|type|sub1,sub2`, with the four fields also given in JSON; indices appear only in memory, beside keys. This follows 02.C11, which settles S4.14's key (prompt log entry 159, cross-section items 3A.11 and 1.11), so this section's own choice lapses.
- **Alternatives:**
  - Run-stable edge ids, with S4.14's key without the type as an attribute: this section's earlier proposal, needed only if 02.C11 had been declined.
  - S4.14's key without the type, with an ordinal for repeats, such as `hsa5335|hsa5578|compound#2`. Witnesses stay readable, but the ordinal depends on corridor order and means nothing across runs.
- **Why:** without the type, the key is not unique. The 280 edges that the full JUN run's 13 witnesses use have 279 distinct such keys, because PLCG1 → PRKCA `compound` appears twice in the with-FSC witness at (n 2, e 6): once from hsa04012 and hsa04020, once, as ECrel, from hsa04650 and hsa04664. With the type all 280 are distinct, and 02.C5 merges exact duplicates when a network is read, so witnesses stay exact and the counts check holds.
- **Decision:** lapsed: 02.C11 puts the relation type in the edge key, which makes it unique (prompt log entry 159)

### 07.C12 · Positional `pex` arguments that 2.0 already knows

- **Proposed:** the positional `pex` form's `src` and `tgt` must name the bound solve's source and target, and anything else is an error before the run. `lfc_file` and `pathmap_file` are checked as readable (S6.12, decision 0021). The page draws the values the solve used (cross-section item 4A.13, prompt log entry 168); the line may add 04.C9's reading arguments only to say how its `lfc_file` is read (1.23), and a warning names both when that value input differs from the solve's. Pathway names come from the run's names, as S6.12 decided (entry 205).
- **Alternatives:**
  - Take the arguments as given, as FuSION 1 did: `src` and `tgt` mark the source and target and root the levels, and LFC values are parsed from the given file with the one reader, so the page can show values other than those the solve classified.
  - Ignore the four tokens with a warning.
- **Why:** in the batch census all 51 `pex` lines pass a `src` and `tgt` equal to their batch's connect pair, so the check costs no real batch anything, while a typo would otherwise relabel the drawing (S6.13's one-column collapse comes from a `src` that names no drawn node). Drawing the values the solve used keeps one meaning per gene in a run (S3.14). `psp` keeps FuSION 1's free endpoints: there `src` and `tgt` only set the path filter, so nothing is mislabelled.
- **Decision:** as proposed (prompt log entries 168 and 180)

## Interfaces with other sections

- **06 · Solve**
  - Gives `solve::Results` (1.5): per case, its status (with `not_requested`) and message, the frontier (points with `certified`, `undecided_below` and witness indices; 06.C6, 06.C20), the undecided cells, the answers in order (for the limits rendering), the witnesses with stable keys (06's `Witness`: `active`, `inactive`, `relaxed_nodes` with presence, `selected`, `relaxed_edges` with selection), extra witnesses and why their enumeration ended (06.C11), the answers of a points request (06.C9), and the observation report in the seven words of `ObservationStatus`.
  - Owns `legacy_prefix` in its `pathz3` row (1.21); this section writes the files.
  - The FSC set, the cases `in`, `out` and `free` (06.C10), and the endpoints present and active (06.C16) are 06's; `analyse` checks them.
- **05 · Corridor:** each node's distances from the source and to the target (S4.07); the corridor summary and artefact path; the observation match of 05.C9 (in the corridor, outside it, not in the network).
- **04 · Expression:** the values the solve used, per member gene; the identity of a value input (file plus reading arguments, 04.C9) for 07.C12's comparison.
- **02 · Network**
  - `NodeKey`, `EdgeKey` with its printed form `src|tgt|type|sub1,sub2` (02.C11), `NodeKind`, members and labels (07.C10), edge types (S2.31), pathway names, KEGG ones from each KGML's root title (entry 205).
  - `network::Effect network::effect_of(std::span<const std::string> subtypes)` and `bool network::is_relaxed(Effect, EndState source, EndState target)`, with EndState absent, inactive or active: the one rule that 06 encodes and this section checks (S6.14, 2B.1).
- **01 · Batch**
  - The command table entries for `pex`, `psp` and `plot_pareto`, with their tool requirements (2A.1, 2B.15); the batch-time checks under Errors; binding positional file arguments to solve names (07.C9), with `batch.unmatched_file` when nothing matches.
  - 01.C11's recognised moves call `two_column` for `PO.dat` and `legacy_limits` for `limits_timefile.txt`.
  - The batch layer calls `publish_solve` after the solve step returns or stops, and records its return value as the step's `details` (1.5, 2B.7).
- **09 · Runs:** the step directories (09.C2) and this section's placements in them (1.4); `core::Context`, `Files` and `OutputFile`, which must also commit a file an external tool wrote; absolute output paths honoured as well (2B.3); the clash check (2A.7); the event envelope and the `warning` event (1.3); `Lock` for any file (2B.17); `run` writing `index.html` from `run_page_html` when the run ends; the run id for site manifests.
- **10 · Frontend:** reads the solve step's `details` in `run.json` for session lists (2B.7), `results.json` (frontier, certification, comparison, observations), `plot_pareto/pareto.png`, the run page, and the view JSON or page shells. It never re-derives polarity or draw cases. The events `results.frontier` and `results.comparison` drive its live view.
- **11 · Build:** compiles in the page assets with their SHA-256 (11.C11); the dependency registry (2A.1); `core::run_process` (2A.11); the gnuplot runner and the plot palette in `core` (2B.13); the bundled gnuplot 6.0.5 (11.C9) and Liberation Sans with `fonts.conf` (11.C10); Graphviz left to the user (07.C5).
- **12 · Tests:** the hooks below, and the JSON Schemas for `fusion.results` and `fusion.view`, which this section writes and 12 keeps (2A.10).

## Test hooks

Library level (Catch2):
- **Build.** `build` on scripted `solve::Results` gives the expected tables, keys and display words for every one of 06's seven statuses, including a points request.
- **JSON.** `from_json(to_json(r)) == r`, and writing twice gives the same bytes. A newer version, an unknown format or broken JSON throws `core::Failure`. Every file validates against its schema (2A.10).
- **Polarity.** The full 3 × 3 × 2 table of `is_relaxed` (end states × end states × effect) matches FuSION 1's rule (`Linux/src/GraphManagerNew.cc:28552-28595`). A witness whose relaxed flag disagrees is an internal error.
- **Draw cases.** Hand pairs give honoured, removed and flipped. A random property test over end states shows the twin never arises from a consistent witness.
- **Path filter.** On a witness, `all` keeps exactly the in-witness groups. `shortest` and `alt:W` behave as expected on hand graphs. The selector and mode parsers reject `7,1x`, `alt:-1` and `alt:x`.
- **Routes.** Fixtures converted from FuSION 1 dumps reproduce fdr050 with JUN: (n 5, e 1) not on route and (n 6, e 0) on route.
- **Counts.** A point that differs from its witness's counts is an internal error (06.C20).
- **FSC** (06.C10). An `in` witness with an FSC node absent, or with activity not matching its observation, is an internal error; so is an `out` witness with one present. A two-gene FSC set is checked node by node.
- **Edge keys** (02.C11). The full JUN run's two PLCG1 → PRKCA `compound` edges stay two records with keys `hsa5335|hsa5578|PPrel|compound` and `hsa5335|hsa5578|ECrel|compound`.
- **Levels** (S6.13). On hand witnesses, levels follow hop distance over all drawn pairs, relaxed ones included, with unreachable nodes one level past the deepest.
- **View files.** A point with three recorded witnesses gives one view file with three witness entries; a requested point that is unsat gives one with none and its reason.
- **Comparison.** The full JUN run gives `with_cheaper`, delta 1, uncertified; the reference run gives `essential`, certified; the FOSB run gives `identical`.
- **Pareto script.** Golden `pareto.gp` files for one curve, two curves, three curves, identical curves (FOSB), a `no_point` case, an uncertified case, a `budget_exhausted` case and a points request. `dodge_offset(8) == 0.10` and `dodge_offset(15)` ≈ 0.178. Labels containing `_` and `"` are escaped.
- **Text formats.** A symbol `</script>` comes out escaped in a shell. TSV fields carry no tab or newline. DOT quotes `"` and `\`. No output contains an absolute path or a time.
- **Site.** Keys such as `CON`, `COM0`, `nul.txt`, `a/b` and a trailing dot are rejected. Two concurrent publishes both reach the landing page. A FuSION 1 manifest still gets its card. Replacing an existing `<key>/` works on all three systems, and an update killed between its two renames is finished or undone by the next one.
- **Legacy files.** Field counts (6 and 11) and header lines parse with FuSION 1's own parser logic (`Linux/supporting_files/plot_explanation.py:72-129`); Phase 3 can run that Python as a reference. `legacy_prefix` with an absolute stem writes the files in `pathz3/legacy/` and at the stem.

End to end (through `fusion`):
- The reference run's batch in 2.0 form gives `in` (n 1, e 0) certified, `out` `no_point`, verdict `essential`, one page, a two-entry plot key and a run page.
- Running a batch twice gives byte-identical step directories apart from the PNG on another platform.
- Every page opens from `file://` in a headless browser with no network request, and draws the node and edge counts in its view.
- A stopped solve leaves `pathz3/results.json` with `stopped` cases, and the process exits with status 3; a budget-limited solve leaves `budget_exhausted` cases, and the plot and pages still run.
- A FuSION 1 batch with two positional `pathz3` lines and `exec mv PO.dat …` and `exec mv limits_timefile.txt …` lines leaves the four `temp_w*` and `temp_wo*` files, the moved `.dat` files and FuSION 1-format limits files where the batch expects them.

Reference tests against FuSION 1 (decision 0023), with expected differences recorded:
- Pages draw honoured inhibitions and inactive nodes (S6.06): 1,192 edges and 302 nodes more over the 569 replayed renders; inactive nodes are faded (07.C4).
- Relaxed edges outside the witness are faded.
- File names say `in` and `out` instead of `incl` and `excl`.
- The plot subtitle uses `>` and lists only the constraints used.
- Points that FuSION 1 reported above their own witness's counts are replaced by those counts (06.C20).

## Open questions

1. **S6.08's CSV against E1's TSV.** *Closed:* TSV per E1, with no tab or line break in fields and `;` lists (prompt log entry 180, cross-section item 4B.33).
2. **Old batches move fixed-name files.** *Closed:* 01.C11 recognises the six moves (prompt log entry 179); they call `legacy_limits` for `limits_timefile.txt`, whose destination gets FuSION 1's rendering, and `two_column` for `PO.dat` (prompt log entry 230).
3. **S4.14's edge key is not unique.** *Closed:* 02.C11's key with the type settles S4.14, and 07.C11 lapses (prompt log entry 159).
4. **Points beaten by their own witness.** *Closed:* 06.C20 makes the frontier the Pareto-minimal witness counts, amending S5.25 (prompt log entry 174).
5. **07.C4 would amend S6.16 and decision 0027.** *Closed:* faded inactive nodes accepted for the static page, amending both (prompt log entry 178).
6. **07.C1 and S5.30.** *Closed:* one `results.json` per solve, with S5.30 read through cross-section item 4A.12 (prompt log entry 177).
7. **vis-network 10 against 9.1.2, and its injected stylesheet.** *Closed for Phase 2:* the layout check goes to Phase 3, with 9.1.2 as the fallback; a strict `style-src` in Phase 5 must allow the injected style (prompt log entry 177).
8. **Positional `pex`'s LFC file.** *Closed:* the page draws the values the solve used (cross-section item 4A.13, prompt log entry 168), and 07.C12 checks the given files (prompt log entry 180).
9. **Layout, case names and the run page against the parallel drafts.** *Closed:* 09.C2's step directories with cross-section item 1.4's placements, and the run page written once at the end (prompt log entry 177); case names `in`, `out` and `free` (prompt log entry 175).
