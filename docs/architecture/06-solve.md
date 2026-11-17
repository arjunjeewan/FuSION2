# 06 · Solve

## Scope

- **Module:** `solve` (Choice C1). It implements spec stage 5 (S5.01 to S5.31):
  - building each FSC case from one corridor;
  - the Z3 encoding and the Z3 layer;
  - the Pareto search with its answer cache;
  - per-query timeouts, the resource limit, the whole-solve budget and Stop;
  - resume and partial solves ([decision 0019](../decisions/0019-partial-runs.md));
  - the seed, debug dumps, certification and witnesses.
- **Starts from** the corridor that section 05 hands over (S4.21), with the observations and their match (05.C9). **Ends with** `solve::Results`: one entry per case, with its status, frontier, certification flags, witnesses, search log and observation report (cross-section item 1.5, prompt log entry 177).
- **Left to other sections:**
  - 01: the `pathz3` command line, argument binding (01.C1), sweeps, the debug option, and `fusion --resume`.
  - 02: the graph model, node and edge keys (02.C11), the id lookup and the `BindingTable` that writes `bindings.tsv` (1.22), edge scores and the `unscored` flag (1.25), and the polarity rule's functions (`effect_of`, `is_relaxed`).
  - 04: reading per-gene values and classifying them (cudf, S3.06).
  - 05: the corridor, its parameters, its trimming (05.C2) and the observation match (05.C9).
  - 07: `results::SolveResult`, built from `solve::Results`, and every published output: `pathz3/results.json` (07.C1), the `.dat` files, the Pareto plot, explanation pages, sign-consistent routes (S5.13) and FuSION 1's text files (`legacy_prefix`, 1.21).
  - 09: `core`'s vocabulary (1.1), the run directory and step directories (09.C2), `run.json`, the event envelope (1.3), the Stop watcher, `OutputFile` and journals (2A.6, 09.C6), and resume by parent run (09.C5).
  - 11: the Z3 version (5.3.3, gated on Phase 3's X11; 11.C2) and how Z3 is linked (11.C17).

Names and evidence used below:

- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`: FuSION 1's corridor has 115 nodes and 370 edges, 2.0's trimmed corridor 108 and 358 (05.C2); its solves took 13.7 s with the FSC and 601.6 s without (`run/timing.log`).
- "The full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/`: FuSION 1's corridor has 592 nodes and 2138 edges, 2.0's 547 and 2117; 4044 s with the FSC and 57,248 s (15.9 h) without.
- "fdr050" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr050/`: 317 nodes and 1262 edges, 182,500 s (50.7 h) of solving.
- "The b10 set" is the 17 bound-10 runs that have a `timing.log`: the full run, fdr025 to fdr050 of the sweep, `egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr040_keep`, the DUSP1 and FOSB runs, and r0 to r8 of `runs/egfr_b10_gsea_fdr044_candidates/`. That is 32 solves and 1,238,521 s (344.0 h) of solving. All 17 use timeouts of 3,600,000 and 14,400,000 ms.
- "The snapshot" is the 195 limits files under `runs/` last written before 2026-11-12 17:00 IST. Runs started later (r9 to r26, the `*_string125` runs and the r6 re-solve) are left out, because they were still running or had been stopped mid-search.
- "The replay" re-runs a FuSION 1 search with the z3 4.8.12 binary on this machine. It loads the base formula of a real dump, then for each logged check does `push`, the two bound assertions, `check-sat` and `pop`, in the logged order, under `(set-logic QF_BV)`, which is what `z3::solver(c, "QF_BV")` builds in FuSION 1 (`Linux/src/main.cc:4721-4736`). FuSION 1 nests one bound in an outer push during its binary searches (below); the replay pushes both per check.
- "The port" is a line-by-line Python port of FuSION 1's search (`GraphManagerNew.cc:29349-29833`). Fed the reference run's answers, it reproduces that run's 11 checks in order.
- Z3 5.3.3 is the pinned version (11.C2, entry 173). Every Z3 measurement here was made on 4.8.12, which FuSION 1 links; Phase 3's X11 repeats them on 5.3.3. FuSION 1 was not built or run. Z3 timings ran as one `nice`d process on a shared 36-core machine with 11 to 18 FuSION 1 runs going, so absolute times are indicative.

## Requirements carried in

Stage 5:

- **S5.01:** positional FuSION 1 lines still work, names are added, every argument is validated before solving, and an unknown graph is a command error.
- **S5.02:** each positional slot consumes its token, and a non-numeric value is an error.
- **S5.03:** a given enable flag decides; otherwise the inputs given turn each constraint family on; the families used are recorded.
- **S5.04:** source and target come from the corridor or named arguments, must resolve, and only one pair is supported.
- **S5.05:** the node lists keep their meaning, so a non-relaxed observation stays consistent; the relaxed-nodes and relaxed-edges inputs are dropped; each id's binding is reported; contradictory lists are rejected before solving. The one change: an avoided gene now also forbids every complex that contains it (entry 229).
- **S5.06:** the edge-line subtype field is required and compared as a set; unmatched lines are reported.
- **S5.07:** the up and down set arithmetic stays, with one read of each list; the node bound is clamped to the number of relaxable observations, which is printed.
- **S5.08:** no undirected copy, closure, unused matrices, mincut file, discarded disjunctions or vacuous totals.
- **S5.09:** the variable families and the bit-vector encoding stay. An edge bound above |E| is clamped with a warning (06.C14, settling "clamp or reject", entry 176). No DE gene in the corridor is the case status `no_de_gene` (06.C15, settling "stop with a message", entry 167).
- **S5.10:** the encoded sets come from a plain forward search from the source that stops at the target; observations outside them are listed.
- **S5.11:** the path constraints stay; an essential node that cannot lie on a path makes its case infeasible, reported before solving.
- **S5.12:** the node-relaxation rule stays; counted and excluded observations go to the output, with the modelling choices stated.
- **S5.13:** the sign rules stay; every considered edge between present nodes counts, selected or not; self-loops leave the count; sign-consistent routes are reported separately (by 07).
- **S5.14:** every selected path crosses a DE gene; the rule is kept and documented in the help and the output.
- **S5.15:** coexpression keeps its constraint and strict threshold, finds columns by header, reports skipped rows, and parses the file once per run.
- **S5.16:** frozen edges keep the strict `>`, state it, and report considered edges with no score.
- **S5.17:** the FSC is a named argument; both cases run on one corridor. "Out" is absent and removed from the observations. "In" is present on a path with activity matching its observation: active when up-regulated or unobserved, inactive when down-regulated.
  - Extended by 06.C10 (entry 175): the FSC may be a set, and a third case, `free`, has no FSC constraint.
  - Extended by entry 170's rule, which entry 229 widens to the avoid lists: avoiding a gene, as an out-case FSC or on an avoid list, also forbids every complex that contains it.
- **S5.18:** the search is kept as it is.
- **S5.19:** TIMEOUT and UNDEFINED stay distinct; every frontier point whose optimality depends on an unknown answer is marked.
- **S5.20:** TIMEOUT and UNDEFINED are cached like SAT and UNSAT, and the cache survives a resume. Amended by 06.C8 (entry 174): a TIMEOUT is asked again under a longer timeout or a larger resource limit.
- **S5.21:** a per-query timeout, an optional whole-solve budget with a clean resumable stop, and wall time logged.
- **S5.22:** seed 42 by default, recorded in the outputs and in every dump.
- **S5.23:** dumps only under the debug option, in the run's directory, named by case and point; reuse is this section's question.
- **S5.24:** a structured search log: one record per check with point, step, outcome, wall and CPU time and reason, in one point order.
- **S5.25:** no dominance filter, because the search yields the frontier; the two-column file stays for stage 6 (07 writes it); certification lives in the structured outputs. Amended by 06.C20 (entry 174): the frontier is the Pareto-minimal witness counts.
- **S5.26:** the search's model is kept; more solutions only on request, blocking only on present nodes, their activity and selected edges; results are always written, empty when there is no point.
- **S5.27:** stable keys in place of corridor ids; no activity for absent nodes.
- **S5.28:** mode 1 is dropped; partial solves cover its use.
- **S5.29:** nothing survives between solves.
- **S5.30:** every output goes to the run's directory, named by case. The case directories and the case labels inside `results.json` meet it (4A.12, entry 177).
- **S5.31:** the same console milestones, points labelled and in one order, the timing line always printed, no lines about unused work.

Other stages:

- **S1.26, S1.28, S1.29:** outputs in the run directory; failures stop the batch with a non-zero exit; result-changing settings are run settings; debug output sits behind one option.
- **S2.11, S2.31:** indirect-effect edges (if kept, as "1+" hops) and ECrel edges reach the solve as ordinary edges. They need no encoding change, only their keys in the witnesses so 07 can mark them.
- **S3.07, S3.08:** a gene is never both up and down.
- **S4.06, S4.10, S4.11, S4.14, S4.15, S4.16, S4.20, S4.21:**
  - excluded genes, whose complexes go too (entry 170);
  - one corridor per solved pair, trimmed of self-loops and of edges into the source or out of the target (05.C2);
  - stable keys, with the edge key including its relation type (02.C11, entry 159);
  - one id binding per run;
  - a corridor that the solve never changes;
  - the solve's bound equal to the corridor's (05.C7, entry 171).
- **S4.18, S7.03:** a Report skips the point's later steps. The only Reports are `corridor.empty`, `gsea.empty_selection` and `observations.none`. A solve makes no Report: its outcomes are case statuses (3A.14 and 1.8, entry 167).
- **S6.02, S6.21:** results come through one documented form, every witness is readable, a point without a witness is marked, and "no point" is an explicit result.
- **S6.05, S6.06, S6.14, S6.15:** a witness holds every present node with its activity and every selected edge; relaxed edges that are not selected are part of the result; one polarity rule.
- **S8.06:** it decides the frontend's session defaults only (4A.4, entry 173). The backend's defaults are 06.C2 and 06.C17, which the frontend offers as a one-click suggestion (10.C13).
- **S8.10, S8.12, S8.17, S8.25:** the solver dialog maps onto the per-query timeout, the budget and solutions on request; the FSC is optional, and a run without one is a single solve (the `free` case); every optional argument reaches the solve; the default pipeline solves both cases on one corridor.

Decisions, the skeleton and the Phase 2 polls:

- [0017](../decisions/0017-batch-file-compatibility.md), [0023](../decisions/0023-results-may-change-where-improved.md): old batches run; results may change where 2.0 improves.
- [0018](../decisions/0018-batch-interface.md): named arguments and sweeps.
- [0019](../decisions/0019-partial-runs.md): stop after or start from any step, resume a stopped solve, solve part of the frontier or one case.
- [0021](../decisions/0021-required-and-optional-inputs.md): inputs checked before the first step; a named but unreadable optional input stops the run.
- [0027](../decisions/0027-graph-display-in-phase-5.md): results in a form the display can use.
- Skeleton:
  - A1: Stop interrupts the running Z3 check, saves state and exits "stopped", without POSIX signals. Its "killing the process stays the last resort, with no saved state" is amended by 06.C19 (entry 174): a killed or crashed run's journal is used on resume.
  - A2: progress events.
  - B1: a library that never writes to the console.
  - C1: this module.
  - D1: typed values in memory, each step saveable and loadable.
  - E1: `run.json`, JSON results, TSV tables.
  - F1: all C++.
  - G1: C++20 and Z3's C++ API at a pinned version.
  - G3: Linux, macOS and Windows, all native.
- Cross-section answers (entries 158 to 210):
  - 1.1: `core`'s vocabulary is 09's: `core::Failure` (with `UsageError` and `InstallError`), `Report` and `Outcome<T>`, `Log` and `Events`, `Context`, `Sha256` and `stable_compare`.
  - 1.3: dotted event names under the key `type`. The envelope is `v`, `seq`, `t` and `type`, plus `step`, `line` and `point` inside a step. Warnings go only through `warning`.
  - 1.4 and 09.C2: step directories, with `pathz3/results.json` and the case directories `pathz3/in/`, `pathz3/out/` and `pathz3/free/`; debug files only under `debug/<step>/`.
  - 1.7 and 01.C1: `pathz3`'s names are owned by 06:
    - `node_min`, `node_max`, `edge_min`, `edge_max`;
    - `timeout`, `incremental_timeout`, `budget`, as durations with units in named form;
    - `coexpression_csv`, `coexpression_threshold`, `frozen_threshold`.
  - 1.16: the registry key is `<module>.<name>`, so `solve.seed`; the bare name is the argument.
  - 1.21: one setting, `legacy_prefix`, asks for FuSION 1's text files.
  - 1.22: one run-level `bindings.tsv`, written by 02's `BindingTable`, with 06's `line` and `note` columns.
  - 2A.6 and 2A.7: every write goes through 09's `OutputFile`; 09's check finds clashes by file identity.
  - 2B.8: a per-check resource limit, `rlimit`.
  - 09.C10: a `solve.check.started` event before each check.
  - 09.C14: failures are `core::Failure`, and a stop is `core::Stopped`.
  - 11.C17: Z3 is a shared library, version-checked at start-up.

## Design

### Components

1. **`build_case`.** Turns the corridor, the observations, the lists, the FSC and a case into an immutable `Problem`, or into a skipped case (`infeasible`, `no_de_gene`).
2. **The encoder.** Asserts a `Problem` into a Z3 context.
3. **`CaseSolver`.** One `z3::context` and one solver per case, used by one thread. It runs checks, reads models and reasons, writes dumps and can be interrupted from another thread.
4. **`pareto_search`.** FuSION 1's algorithm (S5.18). It asks an `Oracle` for answers, so tests can drive it without Z3.
5. **`AnswerCache`.** Every answer, unknowns included (S5.20), with monotone lookups (06.C5).
6. **`assemble`.** The frontier from the witnesses' counts (06.C20), certification flags and undecided points (06.C6).
7. **Witnesses.** Model extraction and extra solutions (S5.26).
8. **The driver.** It runs the cases, threads, budget, Stop, resume and reuse, and writes the case files and events through `core::Context`.

Nothing is `static`. Every object lives inside one `solve()` call (S5.29).

### Building a case

For each requested case, `build_case` does the following, in this order.

1. **Endpoints and bound.** Source, target and path bound come from the corridor (S4.21, S5.04). A positional line's connect-pairs file and bound must match them, or the command fails naming both (05.C7, entry 171).
2. **Encoded sets (S5.10).**
   - One breadth-first search from the source over corridor out-edges, never expanding the target. The encoded nodes N are the reached nodes plus every essential node. The encoded edges E are the traversed edges, without self-loops (below).
   - Cost: O(|V| + |E|).
   - On 2.0's trimmed corridors this is the whole corridor (05.C2: 108 nodes and 358 edges in the reference run, 547 and 2117 in the full run). It stays the definition, so a corridor from another source is still encoded correctly. FuSION 1 encoded 108 nodes and 364 edges in the reference run, 5 of them self-loops, and 551 nodes and 2138 edges in the full run (`declare-fun` counts in their dumps).
3. **Lists (S5.05, S5.06).**
   - Node ids resolve through the run's `BindingTable` (02; S4.15). An id that does not resolve is dropped, as in FuSION 1, with a warning.
   - Edge lines match corridor edges by source, target and subtype set. An edge line has no relation-type field, so a line can match edges of two types between the same genes; it then selects both, as FuSION 1's label match did, and its note says so. A line without a subtype field is a failure (S5.06 requires it). A line that matches no edge is a warning.
   - Every id and edge line gets a row in the run's top-level `bindings.tsv`, through the `BindingTable` (1.22).
   - A gene on an avoid list forbids its node and every complex in N whose members include it (entry 229). That covers `avoid_nodes` and the avoid list of a FuSION 1 positional line, and matches what step 5 does for an out-case FSC.
     - The avoid lists therefore lose FuSION 1's node-only meaning.
     - Old results change where an avoided gene sits in a complex, which decision 0023 allows. Section 12's XD-19 records the one known case: in r5's out case at (n 4, e 4), FuSION 1's witness goes STAT1 → [STAT1+STAT3] → MYC with STAT3 avoided.
   - These are failures (S5.05's contradictions):
     - an id that is both essential and avoided, or both active and inactive;
     - a gene on both confirmed lists, which the set arithmetic below would make both up and down (S3.07);
     - an FSC that is also on an avoid list.
4. **Observations (S5.07, S5.12, S5.17).**
   - The case's sets are `up = ((up − avoid) − confirmed_down) ∪ confirmed_up` and `down = (down − confirmed_up) ∪ confirmed_down`, with the gene ids resolved to corridor nodes.
   - The out case also removes the FSC nodes from both sets.
   - DE, used by S5.14, is `up ∪ down` in the corridor.
   - Every observed gene of the run gets one of 07's seven roles (1.5) in `observations.tsv`:
     - `counted`: an observation in N that is not the source or target, not held by a list and not an FSC node. Only these can be relaxed.
     - `forced`: held by a list or by the FSC rule. That is, on the non-relaxed list (held consistent, below), on an avoid list, or an in-case FSC node.
     - `exempt_endpoint`: the source or the target.
     - `removed_with_fsc`: an out-case FSC node.
     - `outside_encoding`: in the corridor but not in N.
     - `outside_corridor` and `not_in_network`: from 05's match (05.C9).
5. **FSC constraints (S5.17, 06.C10).**
   - in: each FSC node is present, active if up-regulated or unobserved, and inactive if down-regulated.
   - out: each FSC node is absent, and so is every complex in N whose members include an FSC node. A complex cannot form without a member (entry 170, extending S5.17); the avoid lists follow the same rule (entry 229, step 3). The members come from the corridor's member lists (S4.12).
   - free: nothing.
6. **Endpoint defaults (06.C16).** In the named form the source and target are present and active, as every FSC case of the batches lists them (S5.17) and the default pipeline writes them (S8.25). Without presence, the empty assignment would satisfy every path rule. Positional lines pass their lists as written. Their source is still forced active, as FuSION 1 does (`Linux/src/GraphManagerNew.cc:27807-27809`), which S5.04 left unchanged.
7. **Clamps.**
   - Both node bounds are clamped to the number of relaxable observations R_n, the `counted` rows (S5.07).
   - Both edge bounds are clamped to the number of counted edges R_e (06.C14).
   - Each clamp is the warning `solve.bound_clamped`, naming the old and new values.
8. **Pre-checks.** Each skips the case's checks:
   - An essential node, the in-case FSC included, makes the case `infeasible` (S5.11) when it is not in N, or lacks an encoded in-edge (unless it is the source) or an encoded out-edge (unless it is the target). An example is an FSC bound to an isolated complex member.
   - No DE gene in the corridor makes the case `no_de_gene` (06.C15).
9. **Families (S5.03).**
   - The coexpression pairs are the cross pairs of the table rows scoring strictly above the threshold, with both ends in N, deduplicated as unordered pairs.
   - The frozen edges are the edges of E whose score is strictly above the threshold.
   - The edges of E that carry 02's `unscored` flag are counted and reported (S5.16, 1.25).
10. **Fingerprint.** A `core::Sha256` over a canonical serialisation of everything that can change an answer:
    - node and edge keys in index order, polarity per edge, scores;
    - the source, target and path bound;
    - the DE set and the observation roles;
    - every list after binding, the FSC and the case;
    - the coexpression pairs and frozen edges;
    - the encoder version and the seed.

    It is left out of this fingerprint, because answers do not depend on it:
    - timeouts, `rlimit` and the budget;
    - the box, `points`, `solutions`, `retry` and `threads`.

    09 states the rule for resume once (3A.3, entry 166): a step's fingerprint holds the build id, which pins the Z3 version (11.C2), so a resumed solve always runs the same Z3 (06.C18). The case fingerprint guards the journal inside it.

All collections are vectors in corridor index order (S4.14). Every set operation sorts first, with `core::stable_compare`, so the encoding and its Z3 terms are built in the same order on every platform.

### Encoding

The variable families of S5.09 stay. Each variable is named by its index in the case (`p17`, `a17`, `r17`, `d17`, `s42`, `x42`, `f42`), and `names.tsv` maps indices to node keys and to 02's edge keys (`src|tgt|type|subtypes`) for dumps (06.C13).

| Family | For | Width or type |
|---|---|---|
| present `p_v`, active `a_v` | each node of N | Bool |
| relaxed `r_v` | each `counted` observation | Bool |
| distance `d_v` | each node of N | bit-vector, `bit_width(b)`: 4 for b = 10 |
| selected `s_e`, relaxed `x_e`, path flag `f_e` | each edge of E | Bool |
| node count C_n | one | `bit_width(max(R_n, 1))`: 1 for the reference run's single counted observation, where FuSION 1 used 3 |
| edge count C_e | one | `bit_width(R_e)`: 9 for the reference run's 358 edges, 12 for the full run's 2117 |

`bit_width(k)` is the number of bits needed for k, so a clamped bound can never wrap (S5.09).

**Self-loops** are left out of E altogether. The corridor already trims them (05.C2, S4.11); the distance rule below forbids selecting one (S5.11); and S5.13 leaves them out of the count. So no `x_e`, frozen or list constraint can bind one. FuSION 1 froze 4 of the reference run's 5 self-loops (`(not __edgerelax_e)` in its dump), and a frozen inhibitory self-loop forces its node absent.

The constraint families, all ported from FuSION 1:

- **Paths (S5.11).**
  - `s_e ⇒ p_u ∧ p_v`, and `s_e ⇒ d_u <u d_v`.
  - `d_t ≤u b` for the target, asserted directly, without FuSION 1's `__pathbound`.
  - A present source has a selected out-edge, a present target a selected in-edge, and every other present node both.
- **Node relaxation (S5.12).** For a `counted` observation v: up gives `r_v ⇔ ¬(p_v ∧ a_v)`; down gives `r_v ⇔ (p_v ∧ a_v)`. Then `C_n = Σ ite(r_v, 1, 0)`.
- **Edge relaxation (S5.13).**
  - `network::is_relaxed` (02's polarity rule, S6.14) gives, for the inhibition family, `x_e ⇔ p_u ∧ p_v ∧ (a_u = a_v)`; for every other edge, `x_e ⇔ p_u ∧ p_v ∧ ¬(a_u ∧ a_v)`.
  - Then `C_e = Σ ite(x_e, 1, 0)` over E.
  - An ECrel (S2.31) or indirect (S2.11) edge is encoded like any other edge of its family.
- **DE on every path (S5.14).** FuSION 1's four rules, one assertion per edge or node instead of one 332 KB conjunction:
  - a selected in-edge of the target has its flag set;
  - at a non-DE node, `(∨ (s_i ∧ ¬f_i)) ⇒ ∧ ¬f_o` over its in-edges i and out-edges o;
  - a non-DE source clears its out-flags;
  - a DE node sets its out-flags.
- **Coexpression (S5.15).** `(p_u ∧ p_v) ⇒ (a_u = a_v)` for each pair.
- **Frozen edges (S5.16).** `¬x_e`.
- **Node lists (S5.05).**
  - essential `p_v`, active `a_v`, inactive `¬a_v`.
  - Avoid asserts `¬p_v`, and `¬p_c` for every complex c in N whose members include v (entry 229), not the node alone as in FuSION 1.
  - Non-relaxed, on an observation, asserts the consistency that FuSION 1's `¬__noderelax_v` implied: `p_v ∧ a_v` for up, `¬(p_v ∧ a_v)` for down. On any other node it does nothing, as in FuSION 1. The batches put the exempt source and target there, and `bindings.tsv` notes it.
- **Edge lists (S5.06).** essential `s_e`, avoid `¬s_e`, non-relaxed `¬x_e`.
- **Bounds.** A check at (n, e) holds `C_n ≤u n` and `C_e ≤u e`, pushed as FuSION 1 pushes them (below), with no `__nodestatusrelaxbound` or `__edgestatusrelaxbound` variables in between.

Size and cost:

- The assertions are O(|N| + |E| + pairs). The two sums are adder chains of R_n and R_e terms, so after bit-blasting C_e is O(R_e · w_e) gates. Phase 3 compares a balanced adder tree and a sorting network with them, beside X11 (entry 210).
- FuSION 1's without-FSC dump of the full run is 2,766,822 bytes, with 8093 declarations and 7325 assertions. Its four largest assertions are:
  - the edge-relaxation definitions with their sum, 562,709 bytes;
  - the DE rules, 332,338 bytes;
  - the two vacuous totals that S5.08 drops, 162,501 and 41,863 bytes.
- 2.0 drops the vacuous totals and the `__nodebound`, `__edgebound` and `__pathbound` variables. The formula changes, so witnesses and timings may change. Answers cannot, because the dropped parts constrain nothing (decision 0023).

### The Z3 layer

- **Version and linking.**
  - Z3 5.3.3 is pinned and recorded in `run.json` (11.C2, entry 173). The pin is gated: Phase 3's X11 runs the real dumps through 4.8.12, 4.16.0 and 5.3.3, and the user re-decides if 5.3.3 is slower than 4.8.12 at the median.
  - Z3 is a shared library beside `fusion` on every platform, found through the run path. `solve::verify_z3()` checks its version at start-up and throws `core::InstallError` on a mismatch (11.C17, entry 203).
  - In 11's comparison of 4.8.12 and 4.15.3, answers agreed wherever both finished, but every SAT model differed, so witnesses change with any version.
- **One case, one context, one thread (06.C1).** A `CaseSolver` owns a `z3::context` and a `z3::solver(ctx, "QF_BV")`. That is Z3's combined solver: an incremental solver first, then, after the incremental phase limit, the non-incremental QF_BV tactic. Z3 4.8.12's own description (`z3 -pm:combined_solver`) is "combines two solvers: non-incremental (solver1) and incremental (solver2)".
- **Solver parameters:**
  - `timeout` = the per-query timeout (06.C2);
  - `combined_solver.solver2_timeout` = the incremental phase limit (06.C2);
  - `rlimit` = the per-check resource limit, when not 0 (2B.8);
  - `random_seed` = the seed (S5.22);
  - `ctrl_c` = false, so Ctrl-C reaches only `fusion`'s handler (11.C18 and 09.C8, entry 185). 4.8.12 has no such parameter.
- **What the two timeouts do.** This answers spec stage 5's open question 1. Measured in the replay:
  - With `solver2_timeout` at 5 s, a check that needs about 40 s incrementally logs `(combined-solver "solver 2 failed, trying solver1")`, restarts in the non-incremental tactic and returns unsat.
  - With 2 s and an overall `timeout` of 6 s, it returns unknown. Read before `pop`, its reason is `timeout`; after `pop`, the CLI reports the state of the last check as not known.
  - FuSION 1 reads the reason after `pop` (`Linux/src/GraphManagerNew.cc:29461` check, `29465` pop, `29477` reason) and gets `unknown`. That is why every 4 h timeout before the 2026-11-09 fix was labelled UNDEFINED (S5.19), and why the labels after it still say `z3 reason: unknown`.
- **Push pattern.** 2.0 pushes as FuSION 1's search does, so a case's solver sees the same assertion history:
  - each diagonal check pushes both bounds (`GraphManagerNew.cc:29438-29465`);
  - the smallest-n search pushes `e ≤ ed` once, then `n ≤ c` for each check (`29572-29675`);
  - the smallest-e search pushes `n ≤ nd` once, then `e ≤ c` for each check (`29687-29788`).
- **A check (`CaseSolver::check`):**
  1. Build the context and encoding on first use.
  2. Push the moving bound or bounds, and write the dump if debugging.
  3. Check, timing wall with `steady_clock` and CPU with the thread's CPU clock.
  4. On sat, read the model (its counts, and the witness).
  5. On unknown, read `reason_unknown()` and decide the outcome (06.C2):
     - `stopped` if this solve interrupted the check, for Stop or the budget;
     - else `timeout` if the reason contains `timeout`, or if `rlimit` is set and the reason names a resource limit or is `canceled`. On 4.8.12 an exhausted `rlimit` reports `canceled`, measured on the reference run's (n 0, e 1) with `rlimit` 2,000,000;
     - else `undefined`, with the reason kept.
  6. Pop.

  FuSION 1's 99 %-of-a-one-second-clock rule (`GraphManagerNew.cc:29342-29347`) goes.
- **Interrupt.** `CaseSolver::interrupt()` calls `z3::context::interrupt()`, that is `Z3_interrupt`, which "can be used to interrupt: solvers, simplifiers and tactics" (`/usr/include/z3_api.h:1661-1667` on this machine). The C++ header adds: "This is a soft interruption: there is no guarantee the object will actually stop" (`/usr/include/z3++.h:226-230`).
  - The checking thread is blocked inside `check()`, so the call comes from another thread, through the `core::StopCallback` the solve registers (09).
  - Its latency was not measured; Phase 3's label test covers it.
  - An interrupted context is not reused, because Stop and the budget both end the case.
- **Platform notes (G3):**
  - **CPU time.** FuSION 1's `clock()` is process CPU time on POSIX but wall time on Windows, and with two cases in threads only per-thread time is meaningful. `core` supplies the thread's CPU time: `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` on Linux and macOS, `GetThreadTimes` on Windows.
    - `GetThreadTimes` advances in scheduler ticks of about 15.6 ms, so a check shorter than a tick reads 0 CPU there. 178 of the b10 set's 582 checks took under 1 s.
    - `case.json` records the clock's resolution (`cpu_clock_s`). `QueryThreadCycleTime` counts cycles, which are not seconds when the clock speed varies, so it is not used.
  - **Signals.** The solve installs none (09.C8).
- **Memory, measured.**
  - From scratch, the z3 4.8.12 CLI needs 51 to 80 MB peak to solve the reference run's dumps.
  - FuSION 1's whole process peaked at 318,920 KiB (fdr025, 115 nodes) and 1,508,252 KiB (fdr050, 317 nodes), and at 1,100,900 to 1,982,592 KiB on the r0 to r8 corridors (289 to 319 nodes, 1246 to 1314 edges; `/usr/bin/time -v` in `run_time.txt`). After hours of solving, most of that is Z3's learned state.

### Search and cache

- `pareto_search` is S5.18's algorithm line for line:
  - the diagonal search;
  - the smallest n, then the smallest e, as binary searches;
  - one or two points per window;
  - recursion into `[nd+1, nu] × [el, E_h−1]` and `[nl, N_h−1] × [ed+1, eu]` (`GraphManagerNew.cc:29828-29829`).

  Unknown answers steer it as FuSION 1's UNSAT answers do. The windows are fixed, so for a given box and the same answers the sequence of queries is fixed. Resume and the tests rely on that.
- Cost: O(k · log(max(N, E))) queries for k frontier points. The b10 set made 582 Z3 checks in 32 solves (4 to 26 per solve) and 95 cache hits.
- **The cache (06.C5)** is a dense array over the box (at most 16 × 21 cells in the census) holding the index of each answer, plus two staircases: the minimal proven-SAT points and the maximal proven-UNSAT points. A query is answered, in this order:
  1. **by a proof:** an earlier SAT or UNSAT answer at that point, or one implied by monotonicity: SAT if some proven-SAT point, a model's own counts included, is ≤ it on both axes; UNSAT if it is ≤ some proven-UNSAT point;
  2. **by an earlier TIMEOUT or UNDEFINED answer at that point** (S5.20), unless 06.C8 re-asks it;
  3. **by Z3.**

  A proof found after an unknown answer therefore wins over it. `stopped` answers are never cached.
- **Evidence for the unknown cache.** In the b10 set, 17 checks re-asked an unknown point. 13 timed out again (187,161 s); 3 returned unsat (37,873 s) and 1 sat (5738 s). So S5.20 saves 230,771 s, but it also leaves 4 points undecided that a second try settled, such as (n 2, e 6) in r2, r3 and r4. 06.C8 re-asks them under a longer timeout.

### Frontier, certification and undecided points

`assemble` runs after the search, or after a stop. Its cost is O(|box| · |answers|), under a millisecond.

- **Frontier (06.C20, amending S5.25).** The Pareto-minimal counts among the witnesses of all SAT answers in the box, each with that witness. With exact answers this is the search's own point set. With unknowns, a point whose own witness has lower counts is replaced by those counts, and `found_at` keeps the search's point. FuSION 1's witnesses dominated their own points 12 times in 591, all in `runs/real_experiments/`. For example, `experiment_2_old` without the FSC reported (n 15, e 1) with a witness of counts (7, 1).
- **Certified (06.C6).** A frontier point (n, e) is certified when each lower neighbour inside the box, (n−1, e) and (n, e−1), is proven UNSAT, directly or by monotonicity. Otherwise `undecided_below` lists the undecided cells that block it (S5.19).
- **Undecided.** The cells of the box that are neither proven SAT nor proven UNSAT. A missing frontier point can only lie among them.
- **Measured on FuSION 1's answers.**
  - 154 frontier points in the 34 bound-10 solves (the b10 set plus the reference run): 23 uncertified, in 8 solves.
  - Full run without the FSC: (8, 0) and (7, 1) are uncertified, because (7, 0) is undecided.
  - fdr050: (2, 8) with the FSC; (6, 0), (5, 1) and (2, 8) without.
  - r0 without JUN: none of its 5 points is certified, because of 4 unknown answers.

### Witnesses and extra solutions

- **The search's model is the witness (S5.26).** It is read right after the check, before `pop`. Variables are evaluated through their handles with model completion, so no names are parsed (FuSION 1 parses names, `GraphManagerNew.cc:30729-30846`).
- **Witness content (S5.27, S6.06, S6.15).**
  - present nodes by activity: absent nodes carry no activity;
  - selected edges;
  - relaxed nodes, each with presence;
  - relaxed edges, each marked selected or not.

  Nodes are named by node key and edges by 02's edge key (02.C11). The counts C_n and C_e are evaluated too.
- **Size and cost.** Extraction evaluates |N| + |E| Booleans per SAT answer, about 2700 on the full run. Full run, without the FSC: 52 to 78 present nodes and 65 to 98 selected edges per point, about 6 KB of JSON per witness. Witnesses are kept for every SAT answer, because the search does not yet know which will be frontier points: about 7 per case (215 sat answers in the b10 set's 32 solves).
- **Why it matters.** In r5 without STAT3, the search proved (n 7, e 0) sat in 0.76 s. FuSION 1's separate witness check at that point then timed out, so the point had no witness (`run/temp_wo_solutions.txt` says `timeout`). A pinned re-solve in a new process took 442.6 s. r6 at (2, 8) failed the same way (`runs/egfr_b10_gsea_fdr044_candidates/README.md:175-180` for r5, `181-184` for r6). In the b10 set, time outside the search, mostly that re-solving, was 184,253 s, 14.9 % of the total.
- **More solutions (06.C11).** With `solutions = k > 1`, after the search each frontier point is checked again under its bounds, blocking every witness found so far, until there are k witnesses, an unsat ("no more") or an unknown. The blocking clause is

  `¬( ∧_{v∈N} (p_v = P_v) ∧ ∧_{v present} (a_v = A_v) ∧ ∧_{e∈E} (s_e = S_e) )`,

  that is, about |N| + |present| + |E| literals: 2700 to 2800 for the full run. Relaxation flags are left out because they follow from presence and activity. Extra checks use the per-query limits and appear in the search log as phase `extra`.

### Timeouts, the resource limit, the budget and Stop

- **Per query (S5.21, 06.C2):** Z3's own `timeout`, with the incremental phase limit inside it. Logged per check: the timeout used, `rlimit`, wall, CPU, outcome and reason.
- **Resource limit (2B.8, entry 176):** `rlimit` gives each check a count of Z3's own steps, 0 meaning none. It is deterministic where wall-clock timeouts are not. Tests that need unknown answers use it or the scripted oracle, never wall-clock timeouts. Wall-clock timeouts stay the default for real runs.
- **Budget (06.C3):** optional wall time for the whole solve command, all cases together. A monitor thread sleeps until the deadline, then interrupts every running check. Those checks become `stopped`; each unfinished case gets the status `budget_exhausted`, with its journal already on disk. That is a case status, not a Report (3A.14, entry 167): the batch, the Pareto plot and the pages still run, and the solve can be resumed.
- **Stop (A1):** 09's watcher sets the run's stop token, and the `core::StopCallback` the solve registered interrupts every running check. Each unfinished case gets the status `stopped`, with its journal already on disk.
  - `solve()` returns the partial `Results` rather than throwing.
  - The `pathz3` handler has 07 publish them (1.5) and then throws `core::Stopped`, so the run ends stopped (09.C14).
  - The in-flight check's work is lost; a resume re-asks that point.
- **Latency.** In the replay, Z3's own 6 s timeout returned about 0.2 s late. `interrupt()` was not timed.
- **Portability.** `std::thread`, `std::condition_variable`, `core`'s stop token and `z3::context::interrupt()` behave the same on all three platforms. No signals are used. `std::jthread` and `std::stop_token` are avoided because libc++ 15, which G1 allows, lacks them (09).

### Cases in parallel

- With `threads = 0`, the default (06.C4), each case gets its own thread and context. FuSION 1 already gave each case its own context: one `z3::context` per `pathz3` call (`main.cc:4721`). So each case sees the same sequence of checks in the same solver, and timings and witnesses are as in a sequential run. They differ only through CPU contention near a timeout.
- A run with an FSC therefore uses two cores, or three with `free`. 01.C4's `--jobs auto` and the frontend's queue limit (10.C20) count one core per case (3A.9, entry 173).
- Estimated from the b10 set's logs, with each check taking its logged time:

  | Run | FuSION 1 (both cases) | 2.0, cases one after the other | 2.0, cases in parallel |
  |---|---|---|---|
  | full run | 61,292 s (17.0 h) | 43,205 s | 40,162 s (11.2 h) |
  | fdr050 | 182,500 s (50.7 h) | 95,436 s | 68,304 s (19.0 h) |
  | fdr040 | 4140 s | 3828 s | 2777 s |
  | DUSP1 | 20,017 s | 15,352 s | 11,276 s |
  | FOSB | 11,361 s | 9800 s | 5966 s |
  | r0 (JUN) | 161,496 s | 108,674 s | 84,033 s |
  | **b10 set, 17 runs** | **1,238,521 s (344.0 h)** | **823,498 s (228.7 h)** | **640,102 s (177.8 h)** |

  "One after the other" removes the re-checks of unknown points (S5.20) and the witness re-solving (S5.26). "In parallel" also overlaps the two cases.
- **Within a case the search stays sequential.** On paper, running the two binary searches and the two child windows in parallel would cut the b10 total to 470,021 s. That needs a context per window, and a fresh context loses the incremental state:
  - In the replay of the reference run's with-FSC search, 8 of the 9 checks after the first hard one (43.5 s) took 3 to 12 ms each; FuSION 1 took 0.3 to 2.7 ms. The ninth, a sat check, took 3.7 s (FuSION 1: 1.8 s).
  - From scratch, the reference run's without-FSC point (n 2, e 6) took 258.0 s; FuSION 1 answered it in 0.000127 s, incrementally.
  - The full run's (n 1, e 1), (3, 3), (1, 8) and, without the FSC, (4, 2) did not finish within 60 s from scratch. In FuSION 1 they took 0.035 to 3.67 s.

### Resume, reuse and partial solves

- **State on disk (06.C7).** Each case writes `case.json` through `OutputFile` at its start and end, and appends one record per Z3 answer to `journal.jsonl` through `core::Journal`. Each record is on disk before the next check starts (09.C6: flush, then `fsync` or `FlushFileBuffers`), and a SAT record carries its witness. Stop and the budget therefore need no extra saving step. A record torn by a crash is detected and dropped. The journal and `case.json` are the solve step's saved artefact (2A.2).
- **Resume.** A resume is 09's: the new run names the stopped run as its parent (`fusion --resume`, 01), gets its own directory (S1.26), and the solve step receives the parent's journal records through `Files::resumed()`.
  - 09.C5 also reruns a completed solve from its journal when its may-change settings differ, such as a wider box.
  - A killed or crashed run's journal is used in the same way, and only the check in progress is lost (06.C19, amending A1).
  - The resume then runs as follows:
    1. The case fingerprint is checked; a journal for another problem is not used.
    2. The answers are imported, and the search runs from the start. Every imported answer is served from the cache in microseconds, so the search reaches the first unanswered query without storing its recursion.
    3. The context is built only when a query needs Z3. A completed case therefore costs no check.
    4. Imported answers appear in the new log with source `imported`.
    5. The resumed case starts with a cold context, without the learned clauses of the solve it continues. Its first checks can therefore be far slower (258 s against 0.0001 s for one measured point), a known cost of resuming (entry 210).
- **Unknowns on resume (06.C8, amending S5.20).**
  - `retry=automatic` is the default: a TIMEOUT answer is asked again when the resumed `timeout` is longer, or the resumed `rlimit` larger, than the limit it ran out under.
  - `retry=unknown` re-asks every unknown answer.
  - `retry=none` keeps them all, as S5.20 decided.
- **Partial solves (06.C9, decision 0019):**
  - `cases=` picks one or more cases.
  - The box (`node_min`, `node_max`, `edge_min`, `edge_max`) restricts the search.
  - `points=n,e;…` asks only those points, with no search. Each answer is computed or taken from the cache, and SAT points get up to `solutions` witnesses. This covers mode 1's use (S5.28) and single-point re-solves, such as the user's `resolve_wo_n7_e0` and `resolve_wo_n2_e8`, in one context.
- **Loading a saved solve (D1).** `load_saved` rebuilds the full `Results` from each case's journal and `case.json` without Z3, so a run can start from a saved solve (decision 0019). The cost is O(answers).
- **Reuse (06.C12, roadmap objective 4).** What is reused is answers and witnesses, never SMT dumps:
  - resume, as above;
  - `answers_from`: the journals of any earlier solve with the same fingerprint, for example to widen the box or ask for more solutions in a new batch;
  - **free to in and out.** When a solve's cases include `free`, that case runs first, and its answers seed the other two. With F the FSC nodes and δ the number of up-regulated nodes in F:
    - UNSAT_free(n, e) ⇒ UNSAT_in(n, e).
    - A free witness with every node of F present and consistent is an in witness at the same counts.
    - A free witness with every node of F absent, and every complex containing a node of F absent (entry 170's rule), is an out witness at (n − δ, e). The complexes of genes on an avoid list must be absent too (entry 229). Every free witness already has them absent, because the lists apply to every case.
    - UNSAT_free(n + δ, e) ⇒ UNSAT_out(n, e).

    These hold because in adds constraints to free, out adds `¬p_f` for each f in F and each complex holding one, and an absent node cannot change any edge, path-flag or coexpression term. In free, each absent up-regulated member of F costs one node relaxation that out does not count; complexes are never observations. Seeding happens before in and out start, so it is deterministic.

    The user already applies this by hand: r7 and r8's witnesses settled forcing PI3K, FAK, JUN, FOSB or CXCL1 and avoiding RAF, so those tests were never run (`runs/egfr_b10_gsea_fdr044_candidates/README.md:84-86`).
- **Why not dumps.**
  - A dump answers only its own point, which the cache answers in microseconds.
  - Reading one back costs a parse of up to 2.9 MB on the b10 runs, and up to 26.9 MB under `runs/` (`smt_dumps/final_tnfa_p38i_combined_smt-files_*`).
  - Solving it from scratch loses the incremental state (258 s against 0.0001 s above).
  - On 2026-11-12 at about 18:30, `runs/` held 199 dump directories with 2270 files and 5,800,825,397 bytes.

### Debug dumps (S5.23, 06.C13)

Under the debug option only, written through `Files::create_debug`, so they land in `debug/pathz3/<case>/dumps/` (1.4):

- `n<n>_e<e>.smt2` before each Z3 check (S5.23), plus `_x<k>` for extra solutions. Each starts with comment lines for the FuSION and Z3 versions, case, point, seed and limits, then `(set-logic QF_BV)`, the seed options and `to_smt2()`.
- `names.tsv`: index to node key or edge key.
- `base.smt2` once, and `session.smt2`: the ordered push, bounds, check and pop lines of every query. Together they replay the solve incrementally, which the per-point dumps cannot.
  - FuSION 1's source comment says a standalone dump reproduces the in-process check (S5.22). That does not hold: the reference run's without-FSC (n 2, e 6) took 258 s from its dump against 0.0001 s in the session.

The file names are ASCII with no `:`, which Windows forbids.

### Public interface (C++20, module `solve`)

```cpp
namespace fusion::solve {

// ---- settings: registry keys solve.<name> (1.16), recorded in run.json (E1) ----
enum class Case : std::uint8_t { in, out, free };                     // 06.C10; names in files and JSON (1.6)

struct Point {                                                         // (n, e), n always first
  int n = 0;                                                           // node relaxations
  int e = 0;                                                           // edge relaxations
  friend constexpr auto operator<=>(const Point&, const Point&) = default;
};
constexpr bool below_or_equal(Point a, Point b) noexcept;              // a.n <= b.n && a.e <= b.e

struct Box { Point lo{0, 0}; Point hi{8, 8}; };                        // node_min..edge_max; 06.C17

enum class Retry : std::uint8_t { automatic, unknown, none };          // 06.C8

struct Families {                                                      // S5.03, S5.15, S5.16
  std::optional<bool> enabled;                                         // the constraints flag, if given
  std::optional<double> coexpression_threshold;                        // strict >
  std::optional<double> frozen_threshold;                              // strict >
};

struct Settings {                                                      // filled from core::Settings
  std::vector<std::string> fsc;                                        // gene ids; empty = none
  std::vector<Case> cases;                                             // empty = {in, out} or {free}
  Box box;
  std::vector<Point> points;                                           // non-empty = these points only
  std::chrono::milliseconds timeout{14'400'000};                       // 4h (06.C2)
  std::chrono::milliseconds incremental_timeout{3'600'000};            // 1h (06.C2)
  std::chrono::milliseconds budget{0};                                 // 0 = none (06.C3)
  std::uint64_t rlimit = 0;                                            // Z3 steps per check; 0 = none (2B.8)
  std::uint32_t seed = 42;                                             // S5.22
  int solutions = 1;                                                   // 06.C11
  unsigned threads = 0;                                                // 0 = one per case (06.C4)
  Retry retry = Retry::automatic;                                      // 06.C8
  std::optional<std::filesystem::path> answers_from;                   // 06.C12
  Families families;
  bool dumps = false;                                                  // the debug option (S1.29)
};

// ---- inputs ----
struct NodeLists {                                                     // S5.05, ids as written
  std::vector<std::string> essential, avoid, active, inactive, non_relaxed,
                           confirmed_up, confirmed_down;
};
struct EdgeLine {                                                      // S5.06
  std::string source, target;
  std::vector<std::string> subtypes;                                   // required; compared as a set
  int line = 0;
};
struct EdgeLists { std::vector<EdgeLine> essential, avoid, non_relaxed; };

class CoexpressionTable;                                               // parsed once per run (S5.15)
CoexpressionTable read_coexpression(const std::filesystem::path&, core::Context&);  // throws core::Failure

struct Inputs {
  const corridor::Corridor& corridor;                                  // 05; read only (S4.20)
  const expression::Observations& observations;                        // 04; up and down sets
  const expression::ObservationMatch& match;                           // 05.C9: outside_corridor, not_in_network
  network::BindingTable& bindings;                                     // 02; the run's bindings.tsv (1.22)
  NodeLists nodes;
  EdgeLists edges;
  const CoexpressionTable* coexpression = nullptr;
};

// ---- one case ----
enum class Role : std::uint8_t {                                       // 07's seven words (1.5)
  counted, forced, exempt_endpoint, removed_with_fsc,
  outside_encoding, outside_corridor, not_in_network
};
enum class CaseStatus : std::uint8_t {                                 // owned here (1.5); not Reports (1.8)
  complete, no_point, infeasible, no_de_gene, stopped, budget_exhausted, not_requested
};

struct Problem;                                                        // immutable
struct Skipped { CaseStatus status; std::string message; };            // infeasible or no_de_gene
std::variant<Problem, Skipped> build_case(const Inputs&, const Settings&, Case, core::Context&);
core::Sha256 fingerprint(const Problem&);

// ---- answers and witnesses ----
enum class Phase   : std::uint8_t { diagonal, smallest_n, smallest_e, point, extra };
enum class Outcome : std::uint8_t { sat, unsat, timeout, undefined, stopped };
enum class Source  : std::uint8_t { z3, cache, model, transfer, imported };

struct Answer {
  std::uint32_t check = 0;                                             // the case's counter (1.3)
  Point point;
  Phase phase;
  Source source;
  Outcome outcome;
  double wall_s = 0;
  double cpu_s = 0;
  std::chrono::milliseconds timeout{};                                 // the limits it ran under
  std::uint64_t rlimit = 0;
  std::string reason;                                                  // read before pop
  std::optional<Point> model;                                          // the model's own counts
  std::optional<std::uint32_t> witness;                                // index into witnesses
};

struct Witness {                                                       // S5.26, S5.27
  Point counts;
  std::vector<network::NodeKey> active, inactive;                      // present nodes only
  std::vector<network::EdgeKey> selected;                              // 02.C11 keys
  std::vector<std::pair<network::NodeKey, bool>> relaxed_nodes;        // bool: present
  std::vector<std::pair<network::EdgeKey, bool>> relaxed_edges;        // bool: selected
};

class Oracle {                                                         // Z3, or a script in tests
public:
  virtual ~Oracle() = default;
  virtual Answer ask(Point, Phase) = 0;
};

class SearchObserver {
public:
  virtual ~SearchObserver() = default;
  virtual void on_started(Point, Phase) = 0;                           // solve.check.started
  virtual void on_answer(const Answer&) = 0;                           // solve.check, journal
  virtual void on_point(Point) = 0;                                    // solve.point
};

class AnswerCache {                                                    // S5.20, 06.C5
public:
  explicit AnswerCache(Box);
  std::optional<Answer> lookup(Point) const;                           // proofs, then unknowns
  void record(const Answer&);
  std::span<const Answer> answers() const noexcept;
};

std::vector<Point> pareto_search(Box, AnswerCache&, Oracle&, SearchObserver&);   // S5.18

struct FrontierPoint {
  Point point;
  bool certified = false;                                              // 06.C6
  std::vector<Point> undecided_below;
  std::optional<Point> found_at;                                       // the search's point, if replaced (06.C20)
  std::uint32_t witness = 0;
  std::vector<std::uint32_t> more;                                     // solutions 2..k
};
struct Frontier {
  std::vector<FrontierPoint> points;                                   // ascending n
  std::vector<Point> undecided;
};
Frontier assemble(Box, std::span<const Answer>, std::span<const Witness>);

// ---- Z3 ----
void verify_z3();                                                      // throws core::InstallError (11.C17)
std::string z3_version();                                              // Z3_get_full_version()

class CaseSolver {                                                     // one context, one thread
public:
  CaseSolver(const Problem&, const Settings&, core::Context&);         // context built lazily
  Answer check(Point, Phase);
  std::optional<Witness> next_solution(Point, std::span<const Witness> seen);
  void interrupt() noexcept;                                           // from another thread
};

// ---- the step ----
struct ObservationRow { std::string gene, symbol; char observed; Role role; };
struct CaseStats;                                                      // sizes, counts, times

struct CaseResult {
  Case kase;
  CaseStatus status;
  std::string message;
  Frontier frontier;                                                   // empty for points requests
  std::vector<Answer> log;                                             // every query, in order
  std::vector<Witness> witnesses;
  std::vector<ObservationRow> observations;
  CaseStats stats;
};
struct Results { std::vector<CaseResult> cases; };                     // 07 builds SolveResult from it

Results load_saved(const std::filesystem::path& step_dir);             // journal replay, no Z3 (D1)

// Never carries a Report: a solve's outcomes are case statuses (1.8). On a stop it returns the
// partial Results; the pathz3 handler publishes them (1.5) and then throws core::Stopped.
core::Outcome<Results> solve(const Inputs&, const Settings&, core::Context&);
}  // namespace fusion::solve
```

- `core::Context`, `core::Failure`, `core::InstallError`, `core::Outcome`, `core::Sha256`, `core::Journal` and `core::StopCallback` are 09's (1.1).
- `network::NodeKey`, `network::EdgeKey` and `network::BindingTable` are 02's.
- `corridor::Corridor` is 05's, and `expression::Observations` and `ObservationMatch` are 04's.
- `solve()` writes only through `ctx.files`, emits only through `ctx.events` and logs only through `ctx.log`.

### Files written

All paths are in the run directory. Every file is written in binary mode with `\n` line ends through 09's `OutputFile` or `Journal` (2A.6). 09's pre-run check finds clashes by file identity (2A.7).

- `bindings.tsv`, at the top level (1.22): the run's one binding file, written by 02's `BindingTable`, with 02's columns plus 06's `line` and `note`. The solve adds a row for each list id and edge line it binds. Edge-line rows use 02's statuses: `own` for one matching edge, `ambiguous` for several, `absent` for none.

  ```
  role	given	id	status	node	label	size	members	also_in	line	note
  fsc	hsa3725	hsa3725	own	hsa3725	JUN	1	JUN			
  essential_nodes	hsa1956	hsa1956	own	hsa1956	EGFR	1	EGFR		3	
  nonrelaxed_nodes	hsa3576	hsa3576	own	hsa3576	CXCL8	1	CXCL8		2	endpoint: exempt, no effect
  avoid_edges	c_s_hsa6772_c_e hsa4170 expression		absent						1	matches no corridor edge
  ```

- `pathz3/<case>/case.json`: the case's settings, fingerprint, sizes, constraint families, status and times. It is written at the start and replaced at the end. Example for the reference run's in case, with sizes from 05.C2's corridor (times illustrative):

  ```json
  {"case": "in", "fsc": ["hsa3725"], "status": "complete", "message": "",
   "fingerprint": "sha256:9c1e…", "encoder": 1, "z3": "Z3 5.3.3.0", "seed": 42,
   "box": {"requested": {"n": [0, 8], "e": [0, 8]}, "used": {"n": [0, 1], "e": [0, 8]}},
   "encoded": {"nodes": 108, "edges": 358, "counted": 1, "de": 3},
   "families": {"coexpression": {"threshold": 700, "pairs": 1},
                "frozen": {"threshold": 800, "edges": 92, "unscored": 0}},
   "limits": {"timeout_ms": 14400000, "incremental_timeout_ms": 3600000, "budget_ms": 0, "rlimit": 0},
   "checks": {"z3": 8, "cache": 0, "model": 1}, "wall_s": 12.4, "cpu_clock_s": 1e-9,
   "rules": ["every selected path crosses a DE gene before the target (S5.14)",
             "an absent up-regulated gene is relaxed; an absent down-regulated gene is not (S5.12)",
             "source and target are exempt from node relaxation (S5.12)",
             "frozen: score > threshold; coexpression: score > threshold (S5.15, S5.16)"]}
  ```

  - The 92 frozen edges are FuSION 1's 97 less the 4 frozen self-loops and the frozen edge into the source, which the trimmed corridor drops.
  - `unscored` counts the encoded edges with 02's `unscored` flag; the reference run's value is illustrative.
  - `rules` states the modelling rules that S5.12, S5.14 and S5.16 ask the output to state.

- `pathz3/<case>/observations.tsv` (S5.12, S5.17), in 07's seven roles (1.5):

  ```
  gene	symbol	observed	role
  hsa8660	IRS2	up	counted
  hsa3725	JUN	up	forced
  hsa3576	CXCL8	up	exempt_endpoint
  hsa1026	CDKN1A	up	outside_corridor
  ```

- `pathz3/<case>/search_log.tsv` (S5.24): one row per query, Z3 checks and cache answers alike, n before e throughout.
  - It is written whole when the case ends or stops. While a solve runs, the `solve.check` events and the journal carry the same records.
  - Example rows: the full run's without-FSC search, with FuSION 1's answers. FuSION 1 logged only CPU time, so the wall times are illustrative.

  ```
  check	phase	n	e	source	outcome	wall_s	cpu_s	timeout_ms	rlimit	model_n	model_e	reason
  1	diagonal	4	4	z3	sat	2057.3	2057.1	14400000	0	4	4
  2	diagonal	1	1	z3	unsat	0.034	0.034	14400000	0
  …
  7	smallest_n	3	4	cache	unsat	0	0	14400000	0
  …
  16	smallest_e	7	0	z3	timeout	14400.2	14396.2	14400000	0			timeout
  ```

- `pathz3/<case>/journal.jsonl` (06.C7): a header record, then one record per Z3 answer, each on disk before the next check (09.C6).
  - The example is the reference run's in case. Its order is the port's: on the clamped box, n in [0, 1], the search's first checks are (0, 4) and (1, 6). FuSION 1, whose box was n in [0, 3], started at (1, 4).
  - Times and model counts are illustrative.

  ```
  {"header": 1, "case": "in", "fingerprint": "sha256:9c1e…", "encoder": 1, "seed": 42, "box": [[0, 0], [1, 8]]}
  {"check": 1, "n": 0, "e": 4, "phase": "diagonal", "outcome": "unsat", "wall_s": 9.7, "cpu_s": 9.7, "timeout_ms": 14400000, "rlimit": 0}
  {"check": 2, "n": 1, "e": 6, "phase": "diagonal", "outcome": "sat", "wall_s": 0.7, "cpu_s": 0.7, "timeout_ms": 14400000, "rlimit": 0, "model": [1, 3], "witness": {"active": ["hsa1956", "hsa3725", "hsa3576", …], "inactive": [], "selected": ["hsa3725|hsa3576|GErel|expression", …], "relaxed_nodes": [["hsa8660", false]], "relaxed_edges": [["…", true], …]}}
  ```

  A b10 case averages 18 Z3 answers and 7 witnesses (582 and 215 in 32 solves), so the file stays under 60 KB on the full run.

- `debug/pathz3/<case>/dumps/`: debug only, as above.

07 builds its outputs from `solve::Results` after the batch layer calls `publish_solve` (1.5): `pathz3/results.json` (07.C1), `pathz3/in.dat` and `pathz3/out.dat`, and FuSION 1's text files in `pathz3/legacy/` under `legacy_prefix` (1.21). The solve writes none of them. Every requested case writes its files even when it ends `no_point`, `infeasible`, `no_de_gene`, `stopped` or `budget_exhausted`, so a missing file never stands for "no solution" (S6.21). A case that was not requested (`not_requested`) has no directory.

### Settings

Every setting is recorded in `run.json` with its value and source (E1). The registry key is `solve.<name>`, and on the command line the bare name is the argument; 01's `--commands` exports the table for the frontend (1.16, entry 190). The names are owned here (1.7, entry 187). "On resume" is 09's policy: a `must match` setting is part of the step's fingerprint, and a `may change` setting is not.

| Name | Type | Default | Changes results | On resume |
|---|---|---|---|---|
| `fsc` | gene ids | none | yes | must match |
| `cases` | list of `in`, `out`, `free` | `in,out` with an FSC, `free` without | yes | must match |
| `node_min`, `node_max` | int ≥ 0 | 0, 8 (06.C17) | yes | may change |
| `edge_min`, `edge_max` | int ≥ 0 | 0, 8 (06.C17) | yes | may change |
| `points` | list of `n,e` | none | yes | may change |
| `timeout` | duration (`4h`, `90s`, `500ms`; bare ms in positional slots) | 4h (06.C2) | yes, through unknowns | may change |
| `incremental_timeout` | duration | 1h (06.C2) | yes, through unknowns | may change |
| `budget` | duration, 0 = none | 0 | yes, through completeness | may change |
| `rlimit` | unsigned count of Z3 steps per check, 0 = none | 0 (2B.8) | yes, through unknowns | may change |
| `seed` | int ≥ 0 | 42 | witnesses and timing | must match |
| `solutions` | int ≥ 1 | 1 (06.C11) | adds witnesses | may change |
| `threads` | int ≥ 0 | 0 (one per case) | no | may change |
| `retry` | `automatic`, `unknown`, `none` | `automatic` (06.C8) | yes | may change |
| `answers_from` | step directory | none | yes | may change |
| node lists: `essential_nodes`, `avoid_nodes`, `active_nodes`, `inactive_nodes`, `nonrelaxed_nodes`, `confirmed_up`, `confirmed_down` | file | none | yes | must match |
| edge lists: `essential_edges`, `avoid_edges`, `nonrelaxed_edges` | file | none | yes | must match |
| `coexpression_csv`, `coexpression_threshold` | file, number | none | yes | must match |
| `frozen_threshold` | number | none | yes | must match |
| `constraints` | 0 or 1, optional | absent: inputs decide (S5.03) | yes | must match |
| `legacy_prefix` | path stem | none | no; asks 07 for FuSION 1's text files (1.21) | may change |

- `avoid_nodes`: every listed gene also forbids every complex that contains it (entry 229), so the list no longer means the node only, as it did in FuSION 1.
- The lower bounds default to 0 because all 170 census lines use 0 and 0 (S5.01).
- The frontend's pipeline requires the bounds and the timeouts, and offers these defaults as a one-click suggestion (10.C13).
- The path bound, source and target are the corridor's and are recorded with the corridor (S4.21).

### FuSION 1 positional lines

01 parses the tokens (S5.01, S5.02, 01.C1). This section fixes their meaning:

| Token(s) | 2.0 meaning |
|---|---|
| `<graph>` | the corridor; an unknown id is a failure |
| `<mode>` | must be 0; 1 is a failure naming `points=` (S5.28) |
| `<pairs>` | must match the corridor's source and target |
| `<e_lo> <n_lo> <e_hi> <n_hi>` | `edge_min`, `node_min`, `edge_max`, `node_max` |
| `<bound>` | must equal the corridor's bound (05.C7) |
| `<prefix>` | `legacy_prefix`: FuSION 1's text files in `pathz3/legacy/`, and at the prefix's own path when it is absolute (1.21, 2B.3). The prefix also names the solve, so a positional `pex` binds to it (07.C9). |
| `<up> <down>` | read with 04's list reader |
| the 12 list files | the node and edge lists. Relaxed nodes and relaxed edges are accepted and ignored, with a warning if non-empty (S5.05). The avoid list forbids each listed gene's complexes too (entry 229). |
| `<fold_change>` | recorded only; it named FuSION 1's dump directory |
| `<count> <explore>` | `solutions = explore`; `count` is ignored with a warning (06.C11) |
| `<timeout1> <timeout2>` | `incremental_timeout`, `timeout`, in bare milliseconds, as in FuSION 1 (01.C1) |
| the 5 optional tokens | `coexpression_csv`, `coexpression_threshold`, `frozen_threshold`, `constraints`, `seed` |

- A positional line has no FSC argument, so it is one `free` case with its lists, in `pathz3/free/` (1.4), as in FuSION 1. Its results may change only where 2.0 improves (decision 0023).
- A positional without-FSC line avoids its FSC through the avoid list, which now also forbids the gene's complexes (entry 229). So it gives the same answer as a named FSC's out case.
  - Its result changes where an avoided gene sits in a complex.
  - Section 12's XD-19 records the one known case: r5's out case at (n 4, e 4), whose FuSION 1 witness uses [STAT1+STAT3].
- FuSION 1's batches still call `fb_rch` twice. 05.C4 registers the second corridor as the first (entry 171), but the two solves still cannot share answers, because their fingerprints differ by the lists.
- The trap of a down-regulated FSC forced "in" through lists (S5.17) remains for positional lines. The named FSC avoids it.

### Events (A2)

Dotted names under the key `type`. `core::Events` adds the envelope: `v`, `seq` and `t`, and `step`, `line` and `point` inside a step (1.3, entry 190).

- `solve.case.started`: `case`, `fsc`, `nodes`, `edges`, `counted`, `box`, `resumed_answers`.
- `solve.check.started` (09.C10, entry 185): `case`, `check`, `phase`, `n`, `e`, before each Z3 check.
- `solve.check`, after each query: `case`, `check`, `phase`, `n`, `e`, `source`, `outcome`, `wall_s`, `cpu_s`, `reason`; for sat, `model_n` and `model_e`. Example:

  ```json
  {"v": 1, "seq": 57, "t": "2026-07-18T00:12:47.031Z", "type": "solve.check", "step": "pathz3", "line": 243, "case": "out", "check": 16, "phase": "smallest_e", "n": 7, "e": 0, "source": "z3", "outcome": "timeout", "wall_s": 14400.2, "cpu_s": 14396.2, "reason": "timeout"}
  ```

- `solve.point`: `case`, `n`, `e`, when the search records a point.
- `solve.case.finished`: `case`, `status`, `points`, `certified`, `undecided`, `checks`, `wall_s`.
- Warnings go only through 09's `warning` event, from `ctx.log.warning(code, message)`, with dotted codes:
  - `solve.bound_clamped`, `solve.edge_line_unmatched`, `solve.list_id_unresolved`, `solve.nonrelaxed_not_observed`;
  - `solve.unscored_edges`, `solve.coexpression_rows_skipped`, `solve.relaxed_list_ignored`, `solve.count_ignored`.
- A solve makes no Report, so its `step.finished` carries none; its case statuses are in `solve.case.finished` and in `results.json`.

### Console (S5.31)

Through `ctx.log`; the library never prints (B1). Points are always `n=…, e=…`. Example for the reference run's in case, with the trimmed corridor's sizes and illustrative times:

```
[solve] cases in, out on EGFR -> CXCL8, bound 10: corridor 108 nodes, 358 edges
[solve in] FSC JUN (hsa3725) present, active; 1 counted observation (IRS2); node bound 8 -> 1
[solve in] encoded 108 nodes, 358 edges; coexpression 1 pair (> 700); frozen 92 edges (> 800)
[solve in] check n=0 e=4 (diagonal): unsat in 9.7 s
[solve in] check n=1 e=6 (diagonal): sat in 0.7 s; model n=1 e=3
[solve in] point n=1 e=0, certified
[solve in] complete: 1 point, 1 certified; 9 queries, 8 to Z3, 1 from a model; 12.4 s
[solve] done in 610.2 s wall
```

### Errors

Failures are `core::Failure` exceptions with dotted codes, which stop the batch (09.C14, S1.28). Generic conditions use 09's and 01's codes (1.17). Case statuses are results, not failures and not Reports (1.8).

| Condition | Kind | Code, or example message |
|---|---|---|
| invalid or missing argument, unknown case name, a box with min > max | failure before any step (0021) | 01's argument codes (01.C1) |
| positional mode 1 | failure | `solve.mode_removed` |
| unknown graph, or a graph that is not a corridor | failure | `solve.not_a_corridor` |
| a pair or bound that does not match the corridor | failure (05.C7) | `solve.bound_mismatch`: `bound 10 differs from the corridor's 7` |
| unreadable named input (list, CSV) | failure (0021) | 09's `input.missing`, `input.unreadable` |
| an edge-list line without a subtype field | failure (S5.06, 0021) | `solve.edge_line_no_subtype` |
| an FSC id that does not resolve | failure (S5.17, S8.12) | `solve.fsc_unresolved` |
| contradictory lists, a gene on both confirmed lists included | failure (S5.05, S3.07) | `solve.contradictory_lists`: `hsa3725 is both essential and avoided` |
| `answers_from` names a solve of another problem | failure | `solve.answers_mismatch` |
| a journal or `case.json` changed since it was recorded | failure | 09's `artefact.changed` |
| a Z3 exception | failure; the journal is on disk | `solve.z3` (09) |
| a Z3 library of another version | failure at start-up (11.C17) | `core::InstallError` |
| a list id that does not resolve; an edge line that matches no edge; a non-relaxed id that is not an observation | warning, with a row in `bindings.tsv`; the lists keep FuSION 1's meaning (S5.05, S5.06), apart from the avoid lists' complexes (entry 229) | `solve.list_id_unresolved`, `solve.edge_line_unmatched`, `solve.nonrelaxed_not_observed` |
| no point in the box | case status `no_point` | `[solve out] no point within n<=8, e<=8` |
| an essential node that cannot lie on a path | case status `infeasible` | `[solve in] infeasible: RELA cannot lie on a path from EGFR to CXCL8` |
| no DE gene in the corridor | case status `no_de_gene` (06.C15) | `[solve] no differentially expressed gene in the corridor; no path can cross one (S5.14)` |
| budget used up | case status `budget_exhausted` (06.C3) | `[solve out] budget 24h used: 5 points, 3 certified, 4 undecided cells; resumable` |
| unknown answers | flags on the frontier (S5.19) | `[solve out] point n=7 e=1, not certified: n=7 e=0 undecided` |
| Stop | case status `stopped`; the handler throws `core::Stopped` (A1, 09.C14) | `[solve] stopped by request after 14 checks; resumable` |

## Choices

### 06.C1 · Z3 solver configuration

- **Proposed:** keep FuSION 1's configuration. Each case gets one `z3::solver(ctx, "QF_BV")`, Z3's combined solver: an incremental solver first, then, after the incremental phase limit, the non-incremental QF_BV tactic. Bounds are pushed and popped as FuSION 1's search does: both bounds for each diagonal check; in each binary search, the fixed bound once and the moving bound per check (`GraphManagerNew.cc:29438-29465`, `29572-29675`, `29687-29788`). Base assertions are added once.
- **Alternatives:**
  - Bounds as assumption literals (`check(assumptions)`). This is incremental too, but Z3 then always uses the incremental solver and never falls back to the tactic. It would allow unsat cores over the two bounds.
  - A fresh solver per check. Each check would then depend only on the formula, point and seed, so window-level parallelism and resumes would be exact; but every check would start cold.
- **Why:**
  - Incremental state is worth up to six orders of magnitude here. In the replay, 8 of the 9 checks after the first hard one took 3 to 12 ms each. From scratch, the reference run's without-FSC (n 2, e 6) took 258.0 s against FuSION 1's 0.000127 s. The full run's (1, 1), (3, 3), (1, 8) and (4, 2) did not finish within 60 s from scratch, against 0.035 to 3.67 s in FuSION 1.
  - The fallback matters too. In the b10 set, 53 checks (7 sat, 46 unsat, 344,567 s) finished between 1 h and 4 h, after the 1 h incremental phase gave up, so the tactic settled them.
  - The assumption variant replayed the reference run in 53.5 s against 47.9 s for push and pop. That is the same pattern, but without the fallback.
  - All of this was measured on 4.8.12. X11 repeats it on the pinned 5.3.3 (11.C2).
- **Decision:** as proposed (prompt log entry 173)

### 06.C2 · Default timeouts and the TIMEOUT label

- **Proposed:**
  - Backend defaults of `timeout` 4h per query and `incremental_timeout` 1h, the values of every b10 run. The frontend's pipeline requires both values and offers these as a suggestion (10.C13).
  - TIMEOUT when Z3's reason, read before `pop`, contains `timeout`, or when an `rlimit` ran out; `stopped` when this solve interrupted the check; UNDEFINED otherwise.
- **Alternatives:**
  - No backend defaults: both required on every `pathz3` line too.
  - The census's commonest pair, 600,000 and 3,000,000 ms (102 of 170 lines).
  - FuSION 1's label rule: wall time at least 99 % of the cap, on a one-second clock.
  - The solve's own watchdog for per-query timeouts too, interrupting Z3 itself.
- **Why:**
  - All 17 b10 runs, the most recent and the longest, use 1 h and 4 h. S8.06 decides only the frontend's session defaults (4A.4).
  - The replay shows Z3 reports `timeout` before `pop` and only loses it after, so the label needs no clock heuristic.
  - A watchdog for every query would rely on Z3 accepting a reused context after `interrupt()`, which is untested. Z3's own timer is what FuSION 1's 39 b10 timeouts used, and it left the context usable.
- **Decision:** as proposed (prompt log entry 173)

### 06.C3 · When the whole-solve budget runs out

- **Proposed:** each unfinished case gets the status `budget_exhausted`, and the batch goes on. The frontier so far is certified point by point, undecided cells are listed, and the solve can be resumed. It is a case status, not a Report (1.8).
- **Alternatives:** treat it like Stop: the run ends with the stopped status.
- **Why:**
  - A budget exists so that one slow point cannot hold a whole sweep. The full run's without-FSC solve took 15.9 h under a 4 h per-query cap, and fdr050's took 34.5 h.
  - Ending the run would also skip the plots and explanations of the cases that did finish.
- **Decision:** as proposed (prompt log entry 167)

### 06.C4 · Solve the cases in parallel

- **Proposed:** one thread and one Z3 context per case, all cases at once (`threads = 0`). `threads = 1` restores FuSION 1's order. Within a case the search stays sequential. `--jobs auto` (01.C4) and the frontend's queue limit (10.C20) count one core per case (3A.9).
- **Alternatives:**
  - Sequential cases, as in FuSION 1.
  - Also run the two binary searches and the two child windows of each window in parallel, each in its own context.
- **Why:**
  - On the b10 set's logs, overlapping the cases cuts 823,498 s to 640,102 s. The full run goes from 43,205 s to 40,162 s, and fdr050 from 95,436 s to 68,304 s.
  - FuSION 1 already used one context per case, so per-case histories, witnesses and timings are unchanged.
  - The cost is a second core and a second Z3 heap. FuSION 1's processes, which solved one case at a time, peaked at 0.3 to 1.9 GiB on these corridors.
  - Per-query timeouts are wall-clock, so oversubscribed cores can turn a finished check into a timeout. Counting cores per case prevents that.
  - Window-level parallelism would give 470,021 s only if checks kept their warm times. A new context per window loses the incremental state, which costs up to 258 s against 0.0001 s per check, so that gain is not credible.
- **Decision:** as proposed (prompt log entry 173)

### 06.C5 · Answer queries from monotonicity and from models' own counts

- **Proposed:** the cache answers a query SAT when some model already found has counts ≤ the query on both axes, and UNSAT when the query is ≤ a proven-UNSAT point, besides exact hits. The search's steps are unchanged; some of its queries just need no Z3 call.
- **Alternatives:** FuSION 1's exact-point cache, with unknowns added (S5.20).
- **Why:**
  - A SAT model proves its own counts. At the reference run's first check, (n 1, e 4), the model's counts depended on the solver run:
    - a push replay with seed 42 gave (1, 3), which answers none of that solve's later checks;
    - seed 7 gave (1, 1), which answers two of the three smallest-e checks;
    - the non-incremental solver gave (1, 0), which answers all three.
  - FuSION 1's own witnesses, solved at exactly a point's bounds, had lower counts than the point for 12 of 591 recorded points, so models below their bounds do occur.
  - Monotone inference from exact answers alone would have answered 25 of the snapshot's 2540 checks, 13 s in all. Any gain comes from model counts, and on these data it is unmeasured; Phase 3 can measure it by replaying the reference corridors.
  - It never changes an answer. It does change which checks run, and so the solver's later history.
- **Decision:** as proposed (prompt log entry 172)

### 06.C6 · Certification and undecided points

- **Proposed:** a frontier point is certified when both lower neighbours inside the box are proven UNSAT, directly or by monotonicity. `undecided_below` names what blocks a point, and the box's undecided cells are listed.
- **Alternatives:**
  - Flag a point uncertified when any unknown answer occurred in its window.
  - Flag the whole frontier when any answer was unknown.
- **Why:**
  - The rule is exactly S5.19's "depends on an unknown answer", and cheap: at most 336 cells.
  - On FuSION 1's answers it marks 23 of 154 frontier points in 8 of 34 bound-10 solves, with the blocking point named. For example, (8, 0) and (7, 1) of the full run are blocked by (7, 0).
  - The window heuristic misses dependencies across windows, and flagging the whole frontier says nothing about which point to re-check.
- **Decision:** as proposed (prompt log entry 172)

### 06.C7 · Resume by replaying the journal

- **Proposed:** the state is `journal.jsonl` (every Z3 answer, with witnesses), made durable per check through `core::Journal` (09.C6), plus `case.json`. A resume imports the answers and reruns the search from the start, served from the cache, until the first unanswered query.
- **Alternatives:**
  - Serialise the search's recursion stack and windows at Stop.
  - Pin windows by hand, as the user's `resolve_wo_*` batches do.
- **Why:**
  - The search is deterministic given its answers, so the answers are the whole state.
  - Replay costs microseconds per answer; a b10 case averages 18.
  - Nothing has to be written at the moment of a Stop.
  - The same files are the solve step's saved artefact, so a run can start from a saved solve (D1, decision 0019, 2A.2) without Z3.
  - Writing a stack would add a second format that must match the algorithm exactly.
- **Decision:** as proposed (prompt log entry 174)

### 06.C8 · Re-asking unknown answers on resume (amends S5.20; accepted in prompt log entry 174)

- **Proposed:** amend S5.20.
  - `retry=automatic`, the default, asks a TIMEOUT answer again when the resumed `timeout` is longer, or the resumed `rlimit` larger, than the limit it ran out under.
  - `retry=unknown` re-asks every unknown answer.
  - `retry=none` keeps them all.
  - Otherwise cached unknowns stand, as S5.20 says.
- **Alternatives:**
  - S5.20 as decided: cached unknowns always stand on resume, and only a new solve without the journal re-asks them.
  - A narrower amendment: no automatic re-ask, but an explicit `retry=unknown`.
- **Why:**
  - S5.20 stops futile repeats: 13 of the b10 set's 17 re-checks timed out again, costing 187,161 s.
  - A timeout under 4 h says nothing about 24 h, which is how the user re-solved r6's (2, 8).
  - The other 4 re-checks settled their points (3 unsat, 1 sat), which shows that a retry can pay.
- **Decision:** as proposed, amending S5.20 (prompt log entry 174)

### 06.C9 · Partial solves

- **Proposed:** `cases=` picks cases; the box (`node_min` to `edge_max`) restricts the search; `points=n,e;…` asks only the listed points, in one context, with up to `solutions` witnesses each and no search.
- **Alternatives:**
  - Box only: one solve command per pinned point, as in FuSION 1 batches.
  - A separate `solve_points` command.
- **Why:**
  - Decision 0019 asks for chosen points and single cases.
  - Several pinned boxes would each build a context and start cold. `points=` keeps one warm context and one cache.
  - A separate command would duplicate the solve's inputs and settings.
- **Decision:** as proposed (prompt log entry 175)

### 06.C10 · The FSC as a set, and the free case (extends S5.17; accepted in prompt log entry 175)

- **Proposed:**
  - `fsc=` takes one or more ids, forced in or out together.
  - The cases are `in`, `out` and `free`. The default is `in,out` with an FSC, and `free` without one (S8.12's "single solve").
  - The names `in`, `out` and `free` are used in arguments, file names and JSON (`pathz3/in/`, `in.dat`, `in_n1_e0.html`). 07's labels ("with FSC (JUN)") are for display only (1.6).
  - `both` leaves the named form (`cases=in,out`) and stays only as FuSION 1's positional `pex` token.
- **Alternatives:**
  - Exactly one FSC id, with `in` and `out` only. Knockout sets and unconstrained arms are then written as single-case solves with lists.
  - 07's `with`, `without` and `single` names in files and JSON.
- **Why:** the candidate runs use all three shapes:
  - r3 and r4 avoid 2 and 4 nodes as one arm; r2 names two ids, of which only one resolves (`runs/egfr_b10_gsea_fdr044_candidates/README.md:66-69`);
  - r4's first phase is unconstrained;
  - r7 and r8 are the free arms against which r9 to r16 are read.

  With `free` in the same solve, the transfers of 06.C12 apply automatically.
- **Decision:** as proposed, extending S5.17 (prompt log entry 175)

### 06.C11 · One setting for extra solutions

- **Proposed:** `solutions = k` (default 1) keeps k distinct witnesses per frontier point. FuSION 1's `explore` maps to it; `count` is accepted and ignored with the warning `solve.count_ignored`.
- **Alternatives:** keep both: `solutions` kept, plus `count_solutions` to count up to c without keeping them.
- **Why:**
  - 166 of 170 census lines pass 1 and 1, which costs FuSION 1 two extra checks per point and prints an empty `Solution 2`. With k = 1 the search's model costs nothing.
  - FuSION 1 never reported a count, only headers, so dropping `count` loses no output a batch reads.
- **Decision:** as proposed (prompt log entry 175)

### 06.C12 · Reuse: answers and witnesses, not SMT dumps (roadmap objective 4)

- **Proposed:**
  - No dump is ever read back.
  - Reuse goes through answers and witnesses: resume; `answers_from` for any solve with the same fingerprint; and, inside one solve with a `free` case, the sound free-to-in and free-to-out transfers above.
  - A free witness transfers to out only with the FSC's complexes absent too (entry 170's rule), and with the complexes of every avoided gene absent, as every case's witness has them (entry 229).
- **Alternatives:**
  - Read dumps of nearby points.
  - Unsat cores over assumption bounds, which needs 06.C1's assumption alternative and loses the tactic fallback.
  - Warm starts from nearby models (initial-value hints), which 4.8.12 lacks; X11 can say whether 5.3.3 has them.
- **Why:**
  - A dump answers only its own point, at the cost of a parse of up to 2.9 MB on the b10 runs (26.9 MB under `runs/`) and a cold solve (258 s against 0.0001 s).
  - Answers are a few hundred bytes, and witnesses about 6 KB.
  - The transfers are what the user already does by hand: r7 and r8's witnesses settled six node tests (forcing PI3K, FAK, JUN, FOSB or CXCL1, and avoiding RAF), which were then not run.
- **Decision:** as proposed (prompt log entry 175)

### 06.C13 · Debug dumps: index names, plus a replayable session

- **Proposed:**
  - Per-point dumps as decided (S5.23), in `debug/pathz3/<case>/dumps/` (1.4).
  - Variables named by case index, with `names.tsv` for node and edge keys.
  - `base.smt2` and `session.smt2` per case, which replay the incremental solve.
- **Alternatives:**
  - Per-point dumps only, with variables named by stable keys, such as `|p hsa1956|`.
- **Why:**
  - A per-point dump solved alone does not reproduce the in-process check (258 s against 0.0001 s). The session script does: the replay built this way reproduced FuSION 1's pattern of millisecond follow-up checks.
  - Index names keep the dumps near FuSION 1's size, where key names would repeat 40-byte edge keys at every use.
- **Decision:** as proposed (prompt log entry 176)

### 06.C14 · Edge bound above the counted edges

- **Proposed:** clamp both edge bounds to R_e, the number of counted edges, with the warning `solve.bound_clamped`, as S5.07 does for nodes. This settles S5.09's "clamp or reject".
- **Alternatives:** reject the solve with a message.
- **Why:**
  - Any point with e ≥ R_e is equivalent to e = R_e, so clamping loses nothing.
  - Rejecting would stop a sweep whose fixed box of 8 meets a small corridor, such as S5.09's three-edge example.
- **Decision:** as proposed (prompt log entry 176)

### 06.C15 · No differentially expressed gene in the corridor

- **Proposed:** the case status `no_de_gene`, with an explicit empty frontier; the batch goes on. It is a case status, not a Report (1.8), and settles S5.09's "stop with a message".
- **Alternatives:** a failure that stops the batch.
- **Why:**
  - Without a DE gene, S5.14 makes every point UNSAT, so the answer is known without a check.
  - It is the same kind of outcome as S4.18's empty corridor, which a bound sweep must survive.
- **Decision:** as proposed (prompt log entry 167)

### 06.C16 · Source and target present and active by default

- **Proposed:** in the named form, the source and target are present and active without being listed. Positional lines pass their lists as written, with the source still forced active, as in FuSION 1.
- **Alternatives:**
  - Present only, with their activity left to the solver.
  - Nothing implied: the batch or the frontend lists them, as FuSION 1's batches do.
- **Why:**
  - Every FSC case of the batches lists both endpoints as essential and active (S5.17), and the default pipeline writes them (S8.25).
  - Without presence, the empty assignment satisfies every path rule.
  - The endpoints are exempt from node relaxation (S5.12), so forcing them active costs no node relaxation. It only fixes which edge signs at them are honoured.
- **Decision:** as proposed (prompt log entry 176)

### 06.C17 · Default upper bounds of the box

- **Proposed:** the backend defaults `node_max` and `edge_max` to 8. The frontend's pipeline requires them and offers 8 and 8 as a suggestion (10.C13).
- **Alternatives:**
  - No default: both required on every `pathz3` line.
  - The census maxima, 15 nodes and 20 edges.
- **Why:**
  - Every b10 run uses 8 and 8. S8.06 decides only the frontend's session defaults (4A.4).
  - A larger box adds checks to every solve, and the clamps of S5.07 and 06.C14 already shrink it on small corridors.
- **Decision:** as proposed (prompt log entry 173)

### 06.C18 · A resumed solve runs the same Z3 version

- **Proposed:** follow 09, where the rule is stated once (3A.3): resume fingerprints on the build id, and the build pins the Z3 version (11.C2). So a resume under another build reruns the solve from the start. The case fingerprint therefore leaves the Z3 version out.
- **Alternatives:** let a resume continue under another Z3 version with a warning, since answers do not depend on the version; only witnesses, timings and unknowns do.
- **Why:**
  - A result whose witnesses come from two Z3 versions cannot be reproduced by rerunning either. In 11's comparison, every SAT model differed between 4.8.12 and 4.15.3.
  - The cost is losing a journal across an upgrade.
- **Decision:** as proposed (prompt log entry 166)

### 06.C19 · Resuming after a kill (amends skeleton A1; accepted in prompt log entry 174)

- **Proposed:** amend A1's "killing the process stays the last resort, with no saved state". A killed or crashed run's journal is used on resume, like a stopped run's; only the check in progress is lost.
- **Alternatives:** A1 as decided: a killed run has no saved state, and its solve restarts from the start.
- **Why:**
  - The journal is on disk after every check anyway (09.C6), so the state exists.
  - On 2026-11-12, the eight FSC tests r9 to r16 started at 17:05:16 and were all ended at 18:06:28 by SIGTERM (`status.log`: `exit=143`), with 1 to 8 checks done each.
  - FuSION 1's single solves ran up to 15.9 h (the full run) and 34.5 h (fdr050), which a restart would repeat.
- **Decision:** as proposed, amending skeleton A1 (prompt log entry 174)

### 06.C20 · A point dominated by its own witness (amends S5.25; accepted in prompt log entry 174)

- **Proposed:** amend S5.25. The reported frontier is the set of Pareto-minimal counts among the witnesses of all SAT answers, so a point dominated by its own witness is replaced by the witness's counts, with that witness, and `found_at` keeps the search's point. With exact answers this equals the search's points.
- **Alternatives:** S5.25 as decided: report the search's points, and mark a dominated one `dominated_by_witness`.
- **Why:**
  - S5.25 drops the filter because the search yields a non-dominated set. That holds with exact answers but not with unknowns.
  - FuSION 1's witnesses dominated their own points 12 times in 591, all in `runs/real_experiments/`. For example, `experiment_2_old` without the FSC reported (n 15, e 1) with a witness of counts (7, 1).
  - A replaced point is proven SAT by its witness, and is certified by the rule of 06.C6 like any other.
- **Decision:** as proposed, amending S5.25 (prompt log entry 174)

## Interfaces with other sections

- **01 Batch language:**
  - Uses: `pathz3`'s names as owned here (1.7), with `answers_from` and `free` in 01's row (2B.9); 01.C1's binding, with durations in units in named form and bare milliseconds in positional slots; the positional mapping (S5.01, S5.02); the debug option as `Settings::dumps`; `points=` lists; `fusion --resume` and parent runs.
  - The `pathz3` handler, in the batch layer, fills `solve::Settings` from `core::Settings` and calls `solve()`. It has 07 publish the results whether the solve returned or stopped (1.5), and then throws `core::Stopped` if the run was stopped (09.C14).
  - `--jobs auto` counts one core per case (3A.9).
  - Gives: help text stating the S5.14 rule, the strict `>` thresholds (S5.15, S5.16), the case meanings (S5.17) and the units of every limit.
- **02 Network:**
  - Needs: `NodeKey`, and `EdgeKey` with its text `src|tgt|type|subtypes` (02.C11); `effect_of` and `is_relaxed` (2B.1, S6.14); the `unscored` flag (1.25); edge subtypes as a set for list matching (S5.06); the indirect and ECrel marks (S2.11, S2.31).
  - Needs too: the `BindingTable`, with a call that adds 06's edge-line rows and fills the `line` and `note` columns (1.22), and the id lookup's normalisation of coexpression cells such as `hsa:972`.
- **04 Expression:** needs `Observations` (up and down gene sets after cudf and the Not-DE override), `ObservationMatch` as 05 computes it (05.C9), and the list reader for positional `<up> <down>` files (S3.09).
- **05 Corridor:**
  - Needs: the corridor's nodes and edges in a stable index order, its source, target and bound (S4.21), each complex's member list (S4.12, for the complex rule of entries 170 and 229), and a guarantee that it is read-only.
  - Shares the rule of entry 170, widened by entry 229: 05 removes an excluded gene's complexes; 06 forbids the complexes of an out-case FSC and of every gene on an avoid list.
  - Gives: the encoded-set computation (S5.10) as the solve's own; 05.C7's bound check is enforced on both sides.
- **07 Results:**
  - Gives: `solve::Results`, per case: status (with `not_requested`), frontier points with `certified`, `undecided_below`, `found_at` and witness indices, the undecided cells, witnesses with node and edge keys, extra witnesses, observation rows in the seven roles, and the search log.
  - 07 builds `results::SolveResult` from it, maps the statuses to its display words, and writes `pathz3/results.json`, `in.dat` and `out.dat`, and FuSION 1's text files under `legacy_prefix` (1.21). Its labels are for display only (1.6).
  - 07 shows `stopped` and `budget_exhausted` frontiers as incomplete, marks uncertified points (07.C6) and replaced points (`found_at`), and handles a `points` request, which has answers but no frontier.
- **09 Runs:**
  - Needs: `core::Context` with `Files::create`, `create_debug`, `journal` and `resumed` (2A.6, 09.C6); the stop token and `StopCallback`; the event envelope (1.3); a thread CPU clock in `core` beside `cpu_seconds()`; and the parent's journal records on resume, including a killed run's (06.C19).
  - Gives: `load_saved` as the solve step's loader (D1, 2A.2), and the resume policy of each setting (above).
  - 09 states the resume fingerprint rule once (3A.3), which 06.C18 follows.
- **10 Frontend:**
  - The solver dialog (S8.10) maps to `timeout`, `budget` and `solutions`, and `incremental_timeout` is an advanced field. The pipeline requires the timeouts and bounds and suggests 06's defaults (10.C13).
  - The relaxation page maps to `node_min` to `edge_max`; the node-id page maps to `fsc` and `cases`.
  - Progress comes from `solve.check.started` and `solve.check`. The queue counts one core per case (10.C20).
- **11 Build and platforms:**
  - Needs: Z3 5.3.3 as a shared library with `solve::verify_z3()` at start-up (11.C2, 11.C17), and `ctrl_c=false` on every solver (11.C18).
  - X11 must repeat this section's 4.8.12 measurements on 5.3.3: the combined solver's fallback (06.C1), the reason before `pop` and the reason for an exhausted `rlimit` (06.C2), and the incremental speed-up. It can also say whether 5.3.3 offers initial-value hints (06.C12).
- **12 Tests:** the hooks below; tests that need unknown answers use `rlimit` or the scripted oracle (2B.8, 12.C12). XD-19 records the expected difference that entry 229's rule may bring at r5's out case, (n 4, e 4).

## Test hooks

- **Search replay without Z3.** For each of the snapshot's 195 limits files, drive `pareto_search` with an `Oracle` that returns each point's first logged answer.
  - Where no re-ask of an unknown point changed its answer, the query sequence must equal FuSION 1's, minus its re-asks, and the points must equal the `.dat` file. The port already does this for the reference run.
  - In the 5 files where a re-ask turned SAT, the two searches part at that point. There are 7 such re-asks: r6 without CREB1 at (n 2, e 8), and six in four files of `runs/real_experiments/`. They are recorded as expected differences (decision 0023).
- **Synthetic oracles.** Random monotone SAT regions in random boxes up to 16 × 21:
  - the frontier must equal the brute-force minimal set;
  - with injected unknowns, `certified` and `undecided` must equal a brute-force check;
  - with model counts below the queried points, answers must be unchanged and checks fewer (06.C5), and the frontier must take the witness's counts (06.C20);
  - a proof recorded after an unknown answer must win over it.
- **Witness checker.** A Z3-free C++ checker recomputes, from a `Problem` and a witness: selected edges form source-to-target paths of at most b edges, every rule of S5.11 to S5.17 holds, and both counts equal the witness's. Run it on every witness Phase 3 produces.
- **Encoder unit tests** on hand-made graphs, one per constraint family. Among them:
  - a down-regulated FSC "in" is SAT (S5.17);
  - an out-case FSC forbids a complex that contains it (entry 170);
  - an avoided gene forbids a complex that contains it, through `avoid_nodes` and through a positional line's avoid list alike (entry 229);
  - a non-relaxed up-regulated observation is present and active in every witness (S5.05);
  - a frozen self-loop constrains nothing (05.C2, S5.13);
  - an isolated essential node gives `infeasible` (S5.11);
  - an edge bound of 4 on three edges clamps to 3 (06.C14).
- **Reference runs end to end.** The reference run (13.7 s and 601.6 s in FuSION 1) and fdr030 (12.6 min): frontiers equal FuSION 1's `.dat`, except where 05.C2's trimming moves them (decision 0023); witnesses valid by the checker. A positional run that avoids a complex member is compared under section 12's XD-19, which records r5's out case at (n 4, e 4) (entry 229).
- **Determinism.** The same corridor twice gives identical `journal.jsonl` apart from times. Cases sequential (`threads=1`) against parallel give identical per-case answers and witnesses. One `rlimit` gives the same answers on all three platforms (2B.8).
- **Stop, kill and resume.** A test hook sets the stop token after k checks, and another kills the process after k checks. Either way, the resumed solve must give the same answers for the first k queries (source `imported`) and a frontier equal to an uninterrupted run's.
- **Fingerprint.** Changing any encoding input changes it; changing a limit, the box, `retry`, `threads` or `solutions` does not.
- **Labels.** A small hard instance with a 1 s timeout gives `timeout` with reason `timeout`; with a small `rlimit` it gives `timeout` too. An `interrupt()` from another thread gives `stopped`, and that answer is not cached; its latency is measured here.
- **Transfers.** On random small problems, with FSC sets of one to four nodes, some inside complexes, and avoid lists that name complex members, in and out solved alone and solved after free give identical answers (06.C12).
- **CPU clock.** On Windows, a 1 ms check reads 0 CPU and `case.json` gives the clock's resolution.

## Open questions

1. **06.C8 would amend S5.20.** Closed by prompt log entry 174: accepted.
2. **06.C19 would amend skeleton A1.** Closed by prompt log entry 174: accepted.
3. **06.C20 would amend S5.25.** Closed by prompt log entry 174: accepted.
4. **"Stop with a message" (S5.09).** Closed by prompt log entry 167: `no_de_gene` is a case status (06.C15).
5. **Resume is cold.** Closed by prompt log entry 210: recorded as a known cost of resuming, with no design change.
6. **The counter encoding.** Closed by prompt log entry 210: Phase 3 compares a balanced adder tree and a sorting network with the adder chains, beside X11, and any change comes back as a decision.
7. **Does the seed reach every Z3 path?** This is spec stage 5's open question 6, still for Phase 3. The design sets `random_seed` on the solver, as FuSION 1 does, and records it everywhere. At the reference run's first check, seed 42 and seed 7 gave models with different counts, (1, 3) and (1, 1).
8. **Spec stage 5's open question 1.** Answered here and recorded with 06.C2 in prompt log entry 173: the two timeouts are the combined solver's incremental phase limit and the per-check cap, and `reason: unknown` came from reading the reason after `pop`. X11 re-checks it on 5.3.3.
9. **Uncertified frontiers in published runs.** Closed by prompt log entry 172, which put the finding to the user: by 06.C6's rule, r0's without-JUN frontier and r6's without-CREB1 frontier have no certified point.
10. **Does entry 170's complex rule reach the avoid lists?** Closed by prompt log entry 229: yes, every gene on an avoid list forbids every complex that contains it.
11. **Which reason does 5.3.3 give for an exhausted `rlimit`?** 4.8.12 says `canceled`, as for an interrupt; the label rule reads it only when `rlimit` is set and this solve did not interrupt the check. X11 confirms it on 5.3.3.
