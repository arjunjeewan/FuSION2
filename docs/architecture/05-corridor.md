# 05 · Corridor

## Scope

This section designs the `corridor` module (Choice C1): the `fb_rch` step, which cuts out of a network the nodes and edges that lie on bounded walks from one source gene to one target gene, with each node's distance to both. It implements every stage 4 item (S4.01 to S4.22), the corridor side of S3.12 and S4.16, and the trimming that S5.13 settled for S4.11. It also fixes what the solve receives (S4.21, S5.10, S5.11).

It leaves to other sections:
- the batch syntax, the command table, the graph store with its ids and `0`, the run line and `--set`, the debug option, FuSION 1's `exec mv` lines (01.C11), and skipping later lines after a reported result (01.C15);
- the network model: node and edge indices in canonical order (02.C10), keys (02.C11), labels, subtype names, views (`GraphView`, 02.C9), the run's `BindingTable` and its `bindings.tsv` (02.C13), the reading of FuSION 1's files (02.C5), the relation filters (02.C6), the pre-run parse of network inputs (02.C15) and the network's content hash (02); the exception split that gives the source and target nodes of their own (03, S2.13);
- the observations value and the matcher that places observed genes (04);
- the encoding that consumes the corridor (06), and the display (07 and Phase 5, decision 0027);
- the run directory, `core::Context` with its files, settings, log and events, the settings registry, run.json, the error conventions, `core::Sha256` and `core::stable_compare` (09); the test harness (12).

Names and evidence used below:
- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`: a 1134-node, 2529-edge network. "The full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/`: 3100 nodes and 7400 edges. Both cut EGFR (`hsa1956`) → CXCL8 (`hsa3576`) at bound 10, as in stage 4. The reference batch calls `fb_rch` at lines 191 and 227.
- "experiment5.0" is `runs/real_experiments/experiment5.0/`: TNF (`hsa7124`) → NFKBIA (`hsa4792`) at bound 7, on a 3159-node, 8051-edge network.
- "The 82k graph" is `runs/egf_hela_hsa1956_cxcl8_b6/merged_master_graph.xml`, the largest merged graph under `runs/`: 8139 nodes and 82,188 edges after the re-read, and a corridor of 430 nodes and 3648 edges at bound 6 (its `mylog.txt`).
- "The log census", taken on 2026-11-12 at 19:37: 112 run logs under `runs/` call `fb_rch`, 193 times, in 29 distinct configurations. A configuration is one set of arguments with one printed forward set and corridor. With `macOS/` and `auxilliary/` there are 117 logs, 203 calls and 31 configurations. Runs were still being added that day.
- "The replay" is a Python re-implementation of `fb_rch` written for this section and not committed. It reads each run's `merged_master_graph.xml` as stage 4's replay does. It reproduces the forward-set and corridor counts that FuSION 1 printed in all 29 configurations, and the 82k graph's corridor. FuSION 1 was not run.
  - The replay's 2.0 numbers apply S2.11's default, which discards indirect-effect relations. Four of the networks, used by 11 of the 29 configurations, were written by a FuSION 1 build that kept them: 655 to 755 each. With S2.11's other option, which keeps them as "1+" hops, those networks are as FuSION 1 ran them; where that changes a number, it is given too.
- "The batch census", taken the same day: 116 `*.batch` files under `runs/`, `macOS/` and `auxilliary/`. 113 call `fb_rch`, in 206 lines, 93 of them twice. Every line has one source, one target and no exclusion. 80 files pass flag 0 and 33 pass flag 2; none passes 1. Bounds are 6 (21 files), 7 (7), 10 (58), 12 (2) and 15 (25). 111 files move the trace files with `exec mv fb_rch …` and `exec mv fb_rch_back …`.

## Requirements carried in

The command:
- S4.01: the positional form `fb_rch <graph> <sources…> -1 <targets…> -1 <excluded…> -1 <bound> <edges_to_tgt>` still runs (decisions 0017, 0023), names are added (decision 0018), and a missing terminator, a missing argument or an extra token is rejected with a message.
- S4.02: an unresolvable source or target is a command error (decision 0021); each id is resolved once; the node each id bound to is reported (S2.03).
- S4.03: length counts edges; b ≥ 0; b is inclusive. S8.09 has the frontend refuse b < 1; the backend still accepts 0.
- S4.04 and S4.22: the non-traversable subtypes are a run setting, by default `component`, `association`, `dissociation` and `mapping`, recorded with the result (S1.29). One such subtype makes an edge non-traversable, as FuSION 1's `NOT_INTERAC_EDGE` does (`Linux/include/macros.h:69-74`). `DUP_CHECK_FLAG_SET` and `DUMMY_EDGE_FLAG` are dropped.
- S4.05: the edges-to-target flag accepts 0, 1, 2 or names; 1 keeps only activation, inhibition and phosphorylation edges into the target, and 2 only expression and repression edges (FuSION 1: `macros.h:76-77`, `Linux/src/GraphManagerNew.cc:17889-17913`). S4.05 lists FuSION 1's dropping of an activation-and-expression edge under flag 1 as a fault, so an edge that carries both kinds passes both rules.
- Decision 0021: inputs are validated before the first step runs (05.C3); 01.C12 checks an input that the batch itself makes just before its step (prompt log entry 188).

The search:
- S4.07: breadth-first distances from the source, and each corridor node's distances to the source and the target are written out, for inspection and for the display (decisions 0022, 0027).
- S4.08: no intermediate forward graph; the backward search runs on the forward set inside the input graph.
- S4.09: keep rule: node v when ds(v) + dt(v) ≤ b, edge u → v when ds(u) + 1 + dt(v) ≤ b (`GraphManagerNew.cc:18111`, `18132`).
- S4.10 and S5.04: the corridor is built only for the pairs the solve will solve, and the solve supports one pair, taking its source and target from the corridor. So a corridor has one source and one target.
- S4.11 as settled by S5.13: self-loops, edges into a source and edges out of a target are trimmed; 05.C2 reads "trim" as trimming during the search (cross-section item 4B.25).
- S4.06: exclusions stay, and an excluded node never enters the corridor, not even as a complex member. A new rule extends it: excluding a gene also removes every complex that contains it (prompt log entry 170, closing this section's open question 3). In the solve, every gene on an avoid list, the out case's FSC included, likewise forbids every complex that contains it (prompt log entry 229, extending S5.17).
- S4.12: a complex keeps its member list, but a member becomes a corridor node only if it is kept in its own right.
- S4.13: duplicate edges (same source, target, type and ordered subtype list) are merged, with pathways unioned and the larger score kept. 02.C5 moves this merge to reading (prompt log entry 159), so no duplicate reaches the corridor.
- S2.11 and 02.C6: indirect-effect relations are discarded by default, or kept as "1+" hops, and ECrel exclusion acts at the same point, before nodes are made (prompt log entry 161); the corridor sees the network that results.

Ids and the hand-over:
- S4.14: outputs name nodes and edges by stable keys, and an internal id that does appear means the same thing in every step of a run. 02.C11 settles the edge key as source, target, type and subtypes (prompt log entry 159).
- S4.15: one id lookup per run; the corridor carries its bindings and builds no lookup of its own. 02.C9 makes every derived graph a view of the network (prompt log entry 161).
- S4.20, S5.17 and S5.29: the corridor is computed once, both FSC cases are solved on it, and the solve leaves it unchanged.
- S4.21: the corridor records its parameters (sources, targets, bound, flag, exclusions and non-traversable subtypes), can be saved and loaded (decision 0019), and the solve checks its own bound and pair against them.
- S5.10, S5.11 and S5.13: the solve considers the edges reachable from the source without passing the target; a present node lies on a selected path of at most b edges with strictly increasing distances; every considered edge between two present nodes counts as relaxed or not, selected or not, self-loops excepted.
- 01.C16: an argument that takes a value of some kind defaults to the latest value of that kind: `0` for graphs (S2.01), the latest `cudf` result for observations (prompt log entry 189).

Results, failures and outputs:
- S4.16 and S3.12: the observed genes are reported in two unmatched groups, absent from the network and outside the corridor, with the corridor's parameters.
- S4.17: the same counts as FuSION 1, each labelled with what it counts; the forward-graph count goes with S4.08.
- S4.18: a failure stops the run (S1.28), leaves no graph and names its cause; an empty corridor is a reported result ("no path within b"), so a bound sweep carries on. 01.C15 widens S4.18's "its solve is skipped" to every later line of the run or sweep point (prompt log entry 167).
- S4.19: trace files only under the debug option, in the run's output directory, naming nodes by key.
- 09.C14: a failure is a `core::Failure` exception, a reported result is part of a step's `core::Outcome<T>`, and a stop is `core::Stopped` (prompt log entry 186).
- Prompt log entry 158: one core vocabulary, section 09's (cross-section item 1.1: `core::Failure`, `Report` and `Outcome<T>`, `core::Context`, a 32-byte `core::Sha256`, one `stable_compare`); core's ids and orderings owned by 09 after 02's outline (2A.5); generic conditions use 09's and 01's codes, and modules keep codes only for their own faults (1.17).
- Prompt log entry 190: the settings registry's key is `<module>.<name>`, and a batch line takes the bare name; the run line takes any run-level setting by name, `fusion --set` overrides it, and `--commands` exports the settings (1.16, 2B.2). Event names are dotted, the key is `type`, and reported results go only through `step.finished`'s `report{code, message}` (1.3).
- Prompt log entry 229: a step's own argument wins for that step, then `fusion --set`, then the `run` line, then the registry default.
- Prompt log entry 230: the check's argument errors are `batch.bad_argument` (an unknown, duplicate or missing argument, a positional after a named one, or a wrong number of positional tokens), and a value not of its type is `batch.bad_value`.
- S1.26 (outputs only in the run directory), S1.27 (console numbers exact), S2.01 (graph ids, `0` for the latest graph; the corridor takes the next id, S4.08).
- Decisions 0019 (every step saveable and loadable), 0020 (inputs from any source, so nothing beyond the defaults assumes KEGG), 0023 (results may change where 2.0 improves).
- Skeleton: B1 (the library reports through the logger and events), D1 (network, observations and endpoints to corridor; typed values in memory; saved steps), E1 (run.json, JSON results including the corridor summary, TSV tables), A2 (`events.jsonl`), G3 (Linux, macOS and Windows).

## Design

### What the step computes

The input is an immutable network (section 02), the run's `BindingTable` (02), a request (one source, one target, exclusions, bound, edges-to-target rule, skipped subtypes) and, when the run has them, the observations (section 04). The output is an immutable corridor value and its files. The batch layer registers the value under the next graph id, so `0` names it afterwards (S2.01, S4.08). In the reference batch the network is graph 2 and the corridor graph 3 (section 01).

Let T be the traversal graph: the network's edges minus
1. every edge with a skipped subtype (S4.04);
2. every edge into the target that the edges-to-target rule drops (S4.05);
3. every edge with an excluded end. The excluded nodes are the nodes the excluded ids bind to, and every complex that contains one of them (S4.06, extended in prompt log entry 170);
4. every self-loop, every edge into the source and every edge out of the target (S4.11, 05.C2).

ds(v) is the length of a shortest source → v path in T, and dt(v) of a shortest v → target path. The corridor's edges are the edges u → v of T with ds(u) + 1 + dt(v) ≤ b, and its nodes are their endpoints. Put differently, an edge is kept exactly when it lies on a walk of at most b edges from the source to the target that never re-enters the source, never leaves the target, never touches an excluded node and uses only traversable edges.

Consequences:
- Every edge of every simple source-to-target path of at most b edges is kept, because such a path is a walk in T. So no solution path loses an edge.
- No node is isolated: a complex member enters only as the end of a kept edge (S4.12), and an excluded member never does (S4.06).
- Every corridor edge is reachable from the source inside the corridor without passing the target, because the shortest walk to its tail is kept too. So the corridor equals the set the solve considers (S5.10). The replay confirms it in all 29 configurations, under both indirect-effect settings.
- The corridor is a view of the network, not a copy (05.C1, 02.C9). Node and edge attributes, keys and the id bindings stay in the network and its `BindingTable`.
- A relation kept as a "1+" hop (S2.11) is an ordinary one-edge step here. Its flag on the edge lets outputs show lengths through it as `e+`.
- The walk rule stands. Pruning to simple paths would also drop nodes such as IRS2, which lies on no simple path within the reference run's bound yet is that run's one relaxed gene at (n 1, e 0). Phase 3 measures a capped exact pruning, and the user then decides (prompt log entry 170).

The edges-to-target rule, following S4.05: `activation` keeps an edge into the target that carries activation, inhibition or phosphorylation; `expression` keeps one that carries expression or repression; `any` keeps every traversable edge. An edge that carries both kinds passes both rules, and one that carries neither passes neither. Traversable edges carrying both kinds are rare: 0 in the reference network, 4 in the full network (for example TBX21 → IL4 `[inhibition, expression]`, GErel, `path:hsa05321`), 2 in each fdr044 network checked and 6 in the 82k graph. In all 29 configurations every traversable edge into the target carries one kind only, so no measured corridor depends on it.

Excluded complexes: a complex cannot form without its member, so a complex that contains an excluded node, directly or through a complex inside it, is excluded too. Excluding one gene of a family node already excludes the whole node (S4.02), and with it every complex that contains that node. The step reports each complex removed this way, naming the id that caused it. The solve applies the same rule to every gene on an avoid list, the out case's FSC included: each forbids every complex that contains it (section 06, prompt log entry 229). No measured corridor changes: the reference network holds one JUN complex, [ESR1+NCOA3+JUN], and the full network two, adding [JUN+FOSB]; none lies in its 2.0 corridor, so excluding JUN gives 106 nodes and 331 edges in the reference run and 546 and 2068 in the full run, with or without the rule.

### Data structures

The corridor value holds:
- a `network::GraphView` (02.C9): the network, shared read-only, plus one bit per network node and one per network edge. N + E bits: 11 KB for the 82k graph;
- `ds`, `dt`: two arrays of 32-bit distances indexed by the network's node index, meaningful for corridor nodes only. 8 bytes per network node: 65 KB for the 82k graph;
- the request, the bindings of every listed id (02's `Binding` with its role), the excluded nodes with the id that caused each, the counts, the empty reason, the shortest path and the digest.

Corridor nodes and edges are named by the network's own indices (02.C10 orders them by key), so "corridor order" is the network's index order restricted to the view. Edge type, subtypes, pathways and score are read from the network. A node's out-edges in the corridor are its network out-edges whose bit is set: O(deg) per node.

The value has only const accessors and no mutable or lazily filled members, so several threads may read it at once (S4.20, S5.29). During the search, a queue over the network's nodes adds 4 bytes per network node.

### Interface

Module `corridor`, namespace `fusion::corridor`. `network::Network`, `NodeIdx`, `GraphView`, `Binding` and `BindingTable` are section 02's; `core::Context`, `SettingSpec`, `Outcome`, `Report`, `Failure` and `Sha256` are section 09's (cross-section item 1.1); `expression::ObservationMatch` and `match_observations` are section 04's.

```cpp
// include/fusion/corridor/corridor.hpp
namespace fusion::corridor {

enum class EdgesToTarget : std::uint8_t { any = 0, activation = 1, expression = 2 };   // S4.05

struct Request {                                    // the fb_rch arguments (S4.01, 05.C8)
    std::vector<std::string> source;                // must bind to one node (S4.10, S5.04)
    std::vector<std::string> target;                // must bind to one node
    std::vector<std::string> exclude;               // may be empty (S4.06)
    std::uint32_t bound = 0;                        // edges, inclusive (S4.03)
    EdgesToTarget edges_to_target = EdgesToTarget::any;
    std::vector<std::string> skip_subtypes{"component", "association", "dissociation", "mapping"};  // S4.04
};

enum class Role : std::uint8_t { source, target, excluded };
struct Bound { Role role; network::Binding binding; };   // 02's given id, status, node, holders

struct Excluded {                                   // S4.06, extended in prompt log entry 170
    network::NodeIdx node;
    std::string because;                            // the excluded id that removed it
    bool complex_of_excluded;                       // true: a complex containing an excluded node
};

enum class EmptyReason : std::uint8_t { none, beyond_bound, edges_to_target, no_path };   // S4.18

struct Counts {                          // the counts S4.17 keeps, plus the first one
    std::uint32_t traversable_edges;     // network edges with no skipped subtype
    std::uint32_t into_target;           // edges into the target from forward-set nodes (FuSION 1's M)
    std::uint32_t into_target_dropped;   // of those, dropped by edges_to_target (FuSION 1's N)
    std::uint32_t forward_set;           // nodes with ds <= b
    std::uint32_t backward_set;          // forward-set nodes with dt <= b
};

class Corridor {                         // immutable once built
public:
    const network::GraphView& view() const noexcept;      // membership and adjacency (02.C9)
    const network::Network& network() const noexcept;
    const Request& request() const noexcept;
    std::span<const Bound> bindings() const noexcept;
    std::span<const Excluded> excluded() const noexcept;
    network::NodeIdx source() const noexcept;
    network::NodeIdx target() const noexcept;
    std::uint32_t bound() const noexcept;                 // the solve's bound must equal it (05.C7)

    bool empty() const noexcept;
    EmptyReason empty_reason() const noexcept;
    std::optional<std::uint32_t> shortest_path() const noexcept;   // edges; known even when > bound
    const Counts& counts() const noexcept;

    std::uint32_t ds(network::NodeIdx) const noexcept;    // S4.07; corridor nodes only
    std::uint32_t dt(network::NodeIdx) const noexcept;
    const core::Sha256& digest() const noexcept;          // of the canonical content
};

std::span<const core::SettingSpec> settings();            // keys corridor.<name> (1.16)

// Up-front check (05.C3): the listed ids that appear in no network input. Called by the
// pre-run check with the canonical entry ids that 02's pre-run parse found (02.C15).
std::vector<std::string> ids_missing_from_inputs(const Request&,
                                                 const std::unordered_set<std::string>& input_ids);

// The step. Throws core::Failure for the errors that stop the batch (S1.28, 09.C14). An empty
// corridor is a value with a core::Report whose code is "corridor.empty" (S4.18). The step is
// too short to poll for a stop, so it never throws core::Stopped. Writes no file.
core::Outcome<std::shared_ptr<const Corridor>>
compute(std::shared_ptr<const network::Network> network, network::BindingTable& bindings,
        const Request& request, core::Context& ctx);

// Saved step (decision 0019, 05.C6). `matched` is section 04's match_observations() result for
// this corridor (05.C9), empty when the run has no observations. Files go through
// ctx.files.create(); the traces (S4.19) through ctx.files.create_debug(), which section 09
// enables only under the debug option.
void save(const Corridor&, std::span<const expression::ObservationMatch> matched, core::Context& ctx);
std::shared_ptr<const Corridor> load(std::shared_ptr<const network::Network> network,
                                     network::BindingTable& bindings,
                                     const std::filesystem::path& corridor_json, core::Context& ctx);

}  // namespace fusion::corridor
```

### Algorithm and cost

N and E are the network's node and edge counts, n and m the corridor's, and σ the number of subtypes on an edge: 0 to 3 in every measured network, from a vocabulary of 13 to 16 names.

1. **Resolve** every listed id through the run's `BindingTable` (S4.02, S4.15). Repeated ids, and ids that land on one node, collapse with a warning. Then the checks of the next section run. Cost: one hash lookup per id.
2. **Exclude.** The excluded set is the nodes the excluded ids bind to, closed upwards over complex membership: every complex that has an excluded node among its components is excluded too, and so on through nested complexes (prompt log entry 170). One pass over the network's complexes builds the closure, O(N + C) for C component links.
3. **Prepare the edge test.** The `skip_subtypes` names go through the network's subtype normaliser (S2.10), so `binding/association` means `association`, and become a boolean table over the network's subtype ids. The two edges-to-target classes become two more tables. Cost O(S) for S subtype names.
4. **Forward search** (FuSION 1: `GraphManagerNew.cc:17858-17929`). Breadth-first from the source over out-edges that pass the test of T, in the network's edge order. It records ds and expands nodes with ds < b. If the target is not within b, the search goes on past b until it reaches the target or runs out, so the empty report can give the shortest path. Cost O(N + E·σ) at most; the reference run's network has 348 nodes reachable from EGFR at any distance, against 346 within 10.
5. **Counts** (S4.17). `into_target` counts the edges into the target that pass every test but the edges-to-target rule and start at a forward-set node (ds ≤ b), as FuSION 1's M does (`GraphManagerNew.cc:17899`, printed at `17933`); `into_target_dropped` counts those the rule drops, as N does. They come from one scan of the target's in-edges, O(in-degree). With the same network, both equal FuSION 1's printed counts in all 29 configurations. S2.11's default lowers them where an indirect relation entered the target: 21 → 18 (N 20 → 17 with flag 2) for TNF → NFKBIA, and 19 → 18 or 14 → 13 for EGFR or EGF → CXCL8 on the older networks.
6. **Empty check.** If ds(target) > b, the corridor is empty and the step stops here (S4.18). The reason is:
   - `edges_to_target` when the rule dropped an edge u → target with ds(u) + 1 ≤ b, so that without the rule the target would be within b;
   - otherwise `beyond_bound` when the target was reached, first at d > b; the report gives d;
   - otherwise `no_path`: the target cannot be reached at all under these settings.

   The test ds(u) + 1 ≤ b needs no subtraction, so b = 0 is safe with unsigned distances.
7. **Backward search** (FuSION 1: `18052-18099`). Breadth-first from the target over in-edges that pass the test and whose tail has ds ≤ b. It records dt and expands nodes with dt < b. It scans every in-edge of each node it expands, inside the forward set or not, so it costs O(N_x + E_x·σ) for the N_x expanded nodes and their E_x in-edges; at most O(N + E·σ).
8. **Keep.** For every node v with ds(v) + dt(v) ≤ b, set the bit of each in-edge u → v that passes the test and has ds(u) + 1 + dt(v) ≤ b, and the bits of both ends. Cost O(E_x).
9. **Duplicates.** None reach the corridor: 02.C5 merges edges of equal identity when it reads a network, with S4.13's rule (prompt log entry 159). Raw-KGML networks have none (S2.10, S2.20). FuSION 1's own files, which the legacy reader reads, keep them: 768 in the full run's network and 10,568 in the 82k graph.
10. **Digest.** SHA-256 over the canonical text of the network's content hash, the request and the corridor's node and edge keys in index order, with ds and dt. Cost O(n + m) bytes, under 0.6 MB measured.

The whole step is O(N + E·σ) time, and O(N + E) bits plus 12 bytes per network node of memory. The replay, in unoptimised Python with dictionaries, takes 4.5 to 41 ms on the 29 measured networks and about 0.2 s on the 82k graph. A C++ version over arrays does the same linear passes, so it needs no progress events and no stop polling: Stop (A1) acts between steps.

Measured sizes, FuSION 1 against 2.0 (05.C2). The network columns are FuSION 1's counts, with the indirect relations they hold. FuSION 1's corridors are its own printed counts, which the replay reproduces. 2.0's come from the replay under S2.11's default, with the "1+" value after it where that differs. Edges are counted after merging duplicates.

| Run (under `runs/`) | Network nodes / edges (indirect) | b, flag | Forward set, FuSION 1 → 2.0 | FuSION 1 corridor: nodes (isolated) / edges | 2.0 corridor: nodes / edges |
|---|---|---|---|---|---|
| `egfr_cxcl8_jun_b6_nr8_er8_gsea_fdr025` | 1134 / 2529 (0) | 6, 0 | 279 → 276 | 39 (0) / 134 | 39 / 131 |
| reference run | 1134 / 2529 (0) | 10, 0 | 354 → 346 | 115 (3) / 370 | 108 / 358 |
| `egfr_b10_gsea_fdr044_candidates/r7_epha2_curated` | 1729 / 5008 (0) | 10, 2 | 650 → 650 | 306 (12) / 1285 | 294 / 1271 |
| `egfr_cxcl8_jun_b7_nr8_er8` | 3100 / 7400 (0) | 7, 0 | 1138 → 1138 | 269 (14) / 1049 | 253 / 1037 |
| full run | 3100 / 7400 (0) | 10, 0 | 1186 → 1186 | 592 (41) / 2138 | 547 / 2117 |
| experiment5.0 | 3159 / 8051 (734) | 7, 0 | 1218 → 1087 | 223 (17) / 957 | 71 / 188; "1+": 196 / 832 |
| `real_experiments/experiment7.1` | 3159 / 8051 (734) | 15, 0 | 1238 → 1175 | 773 (55) / 3150 | 638 / 2381; "1+": 718 / 3115 |
| `egf_hela/experiment/egfr_to_il8_fsc_jun_b15_t700_exhaustive` | 3192 / 8438 (755) | 15, 0 | 1265 → 1202 | 793 (57) / 3328 | 679 / 2634; "1+": 736 / 3303 |
| the 82k graph | 8139 / 82,188 (0) | 6, 0 | 3309 → 3309 | 430 (24) / 3648 | 406 / 3637 |

Across the 29 configurations, FuSION 1's corridors hold 39 to 793 nodes and 134 to 3328 edges. 2.0's hold 39 to 679 nodes and 131 to 2634 edges under the default, and 39 to 736 and 131 to 3303 with "1+".

### Resolution and validation

Before the first step (05.C3, decision 0021): every source, target and excluded id must appear among the entry ids of the network inputs the batch names. An id found in none of them stops the run before anything runs.

When the step runs, in this order:
1. Every source and target id must bind (S4.02), and so must every excluded id (05.C5).
2. After collapsing, there must be one source node and one target node (S4.10 with S5.04), different from each other (05.C5).
3. Neither endpoint may be excluded, directly or as a complex that contains an excluded node (05.C5, prompt log entry 170).
4. A `skip_subtypes` name that the batch gave and that no edge of the network carries gets a warning. That catches typos in a plug-and-play vocabulary (decision 0020). The defaults are never warned about: no network built by 2.0 has `mapping` edges, because S2.14 drops the step that made them.

Bindings come from the run's `BindingTable` and are reported as S2.03 asks, with 02's statuses (`own`, `family`, `ambiguous`, `absent`); the run-level `bindings.tsv` lists them (02.C13), and `corridor.json` repeats the corridor's own as its parameters (S4.21). The source and target are in the merge-exception set in both the raw and the legacy pipeline (S2.13), so each binds to a node of its own (02). Excluded ids are not: excluding one gene of a family node excludes the whole node, with a warning that lists its genes.

Section 01's check finds the argument errors of S4.01 and S4.05 (terminators, argument count, an integer bound ≥ 0, a known edges-to-target value) before the batch starts, and so a `graph` argument that names no graph or names a corridor, since every line's value kind is known then (01.C16).

### Failures, reported results and warnings

Failures stop the batch (S1.28). They are thrown as `core::Failure` before the corridor is registered, so they leave no graph behind (S4.18). Section 01 prefixes each message with the batch file and line. Following cross-section item 1.17, conditions that 01's or 09's generic checks find keep the generic codes: `batch.bad_argument` for argument errors and `batch.bad_value` for value-kind errors (prompt log entry 230), `input.*` for an unreadable saved corridor, and `artefact.changed` and `artefact.newer_format` for a saved corridor that changed or comes from a newer FuSION. The codes below are this module's own faults.

| Code | Cause | Example message |
|---|---|---|
| `corridor.id_not_in_inputs` | before the first step: an id appears in no network input (05.C3) | `target hsa35766 appears in none of the 167 network inputs` |
| `corridor.unknown_id` | a source, target or excluded id binds to no node of the built network (S4.02, 05.C5) | `target hsa3567 is in the inputs but not in network 2` |
| `corridor.one_pair` | more than one source node or target node (S4.10, S5.04) | `2 target nodes (hsa3576, hsa1969); a corridor has one source and one target` |
| `corridor.same_node` | source and target bind to one node (05.C5) | `source hsa1956 and target hsa1956 are the same node` |
| `corridor.endpoint_excluded` | an endpoint is excluded, directly or as a complex containing an excluded node (05.C5) | `target hsa3576 is also in the exclusion list` |
| `corridor.network_mismatch` | a saved corridor was cut from another network, or names a key or binding that the run's network lacks | `corridor.json was cut from network 3f0c…, but network 2 is 9a71…` |

An empty corridor is a reported result: `compute` returns the empty corridor with a `core::Report` whose code is `corridor.empty` (09.C14). The batch layer then skips every later line of the run or sweep point, and the run still exits 0 (01.C15, widening S4.18's wording; prompt log entry 167).

| Reason | When | Example message |
|---|---|---|
| `beyond_bound` | the target is first reached at d > b, b = 0 included | `no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3: the shortest path has 4 edges` |
| `edges_to_target` | the rule dropped an edge that would have reached the target within b, and no kept edge does | `no path within bound <b>: edges_to_target=<rule> dropped the <k> edges into <target> that lie within the bound` |
| `no_path` | the target cannot be reached under these settings at any length | `no path from <source> to <target> over traversable edges` |

Warnings let the step carry on, through the `warning` event with a dotted code and a message (1.3): `corridor.family_binding` (an excluded id binds to a node holding other genes, S2.03), `corridor.ambiguous_binding` (legacy network, first claim, S2.27), `corridor.repeated_id` (S4.02), `corridor.excluded_complex` (a complex removed because it contains an excluded node, prompt log entry 170) and `corridor.unused_skip_subtype`.

### Settings

All are arguments of each `fb_rch` call. Each is registered as a `core::SettingSpec` under the key `corridor.<name>`, and a batch line names it bare (`bound=10`; cross-section item 1.16). They are recorded in run.json with their source (default, position or name, E1), and in `corridor.json`. Names are 05.C8's.

`corridor.skip_subtypes` is the run setting that S4.04 asks for. The run line sets it for every `fb_rch` of the run (`run corridor.skip_subtypes=component,association,dissociation`), and `fusion --set corridor.skip_subtypes=…` overrides the run line (2B.2). An `fb_rch` line's own `skip_subtypes=` wins for that line, then `fusion --set`, then the run line, then the default (prompt log entry 229). So a batch that gives two `fb_rch` lines different values keeps both.

| Registry key | FuSION 1 position | Type | Default | Meaning |
|---|---|---|---|---|
| `corridor.graph` | 1 | graph id | the latest graph, `0` (01.C16) | the network to cut |
| `corridor.source` | list ended by `-1` | gene ids | required | must bind to one node |
| `corridor.target` | list ended by `-1` | gene ids | required | must bind to one node |
| `corridor.exclude` | list ended by `-1` | gene ids | empty | nodes never entered, with every complex that contains one (S4.06, prompt log entry 170) |
| `corridor.bound` | after the lists | integer ≥ 0 | required | edges, inclusive (S4.03) |
| `corridor.edges_to_target` | last | `any`, `activation`, `expression` (or 0, 1, 2) | `any` | rule for edges into the target (S4.05) |
| `corridor.skip_subtypes` | none (named only) | list of subtype names | `component,association,dissociation,mapping` | non-traversable subtypes (S4.04); the line's own value wins, then `fusion --set`, then the run line, then this default (prompt log entry 229) |
| `corridor.observations` | none (named only) | observations value | the latest `cudf` result, if any (01.C16) | matched for the report only (05.C9); changes no corridor |

### Files

A call that computes a corridor writes two files into its step directory, which section 09 names `fb_rch/` (`fb_rch-2/` for a second call), and a third when the run has observations. A call that reuses an earlier corridor writes none (05.C4). All are UTF-8 with `\n` line ends, written through `ctx.files.create()`, which gives 09's `OutputFile`.

**`corridor.json`**, the saved step and the corridor summary (E1, 05.C6). Nodes and edges are listed in the network's index order (02.C10: nodes by key in byte order; edges by source, target, type and subtypes). Excerpt from the reference run as 2.0 would cut it:

```json
{
  "format": "fusion-corridor", "format_version": 1, "graph": 3,
  "network": {"graph": 2, "content_hash": "3f0c…", "nodes": 1134, "edges": 2529},
  "request": {"source": ["hsa1956"], "target": ["hsa3576"], "exclude": [], "bound": 10,
              "edges_to_target": "any", "skip_subtypes": ["association", "component", "dissociation", "mapping"]},
  "bindings": [{"role": "source", "given": "hsa1956", "node": "hsa1956", "label": "EGFR", "status": "own"},
               {"role": "target", "given": "hsa3576", "node": "hsa3576", "label": "CXCL8", "status": "own"}],
  "excluded_nodes": [],
  "status": "ok", "empty_reason": null, "shortest_path": 4,
  "counts": {"traversable_edges": 1657, "into_target": 11, "into_target_dropped": 0,
             "forward_set": 346, "backward_set": 164, "nodes": 108, "edges": 358},
  "observations": {"in_corridor": 3, "outside_corridor": 28, "not_in_network": 14},
  "nodes": [
    {"key": "hsa110117499", "label": "hsa110117499", "type": "gene", "genes": 13, "ds": 1, "dt": 3},
    {"key": "hsa1956", "label": "EGFR", "type": "gene", "genes": 1, "ds": 0, "dt": 4},
    {"key": "hsa3576", "label": "CXCL8", "type": "gene", "genes": 1, "ds": 4, "dt": 0},
    {"key": "hsa3725", "label": "JUN", "type": "gene", "genes": 1, "ds": 4, "dt": 1}
  ],
  "edges": [
    {"source": "hsa1956", "target": "hsa2885", "type": "PPrel", "subtypes": ["activation"],
     "pathways": ["path:hsa05210", "path:hsa05215"], "score": 994},
    {"source": "hsa3725", "target": "hsa3576", "type": "GErel", "subtypes": ["expression"],
     "pathways": ["path:hsa04620", "path:hsa05120", "path:hsa05130", "path:hsa05132", "path:hsa05142"], "score": 0},
    {"source": "hsa3725", "target": "hsa3576", "type": "PPrel", "subtypes": ["activation"],
     "pathways": ["path:hsa04621"], "score": 0}
  ],
  "digest": "ba0c16be…"
}
```

An edge's source, target, type and subtypes are its key (02.C11). `excluded_nodes` lists every node the exclusions removed, each with the id that caused it and whether it is a complex of an excluded node; excluding JUN in the reference run would give `[{"node": "hsa3725", "because": "hsa3725"}, {"node": "c_s_hsa2099_hsa3725_hsa8202_c_e", "because": "hsa3725", "complex": true}]`. `observations` is `null` when the run has none. An empty corridor has `"status": "empty"`, its reason, the shortest path when known, and empty `nodes` and `edges`. The replay's files in this format measure 60 KB for the reference run, 348 KB for the full run and 586 KB for the 82k graph's corridor, the largest.

**`corridor_nodes.tsv`**, one row per corridor node, for people (S4.07). Sorted by ds, then dt, then key, so it reads from the source outwards. 3 KB for the reference run, 16 KB for the full run.

```
key	label	type	genes	ds	dt
hsa1956	EGFR	gene	1	0	4
hsa110117499	hsa110117499	gene	13	1	3
hsa2885	GRB2	gene	1	1	5
hsa25759	SHC2	gene	4	1	6
hsa387	RAC1	gene	5	2	2
```

**`observations_match.tsv`**, section 04's file and format (05.C9), written when the run has observations. The step fills it with `match_observations` for this corridor, so each observed gene is `in_corridor`, `outside_corridor` or `not_in_network`. Section 04 shows the reference run's rows.

**Debug traces** (S4.19), only under the debug option: `forward.tsv` and `backward.tsv` in `debug/fb_rch/`, through `ctx.files.create_debug()`. One row per node the search dequeued, with the nodes it queued, all by key. For FuSION 1's `exec mv fb_rch …` and `exec mv fb_rch_back …` lines, section 01 writes these two files to the named destinations under the debug option, and nothing otherwise (01.C11). The first rows of the reference run's FuSION 1 traces (`run/fb_rch`, `run/fb_rch_back`) would read:

```
order	node	distance	queued
0	hsa1956	0	hsa110117499,hsa2885,hsa25759
```

```
order	node	distance	queued
0	hsa3576	0	c_s_hsa4790_c_e,hsa3663,hsa2353,hsa3725,hsa4790,hsa1432,hsa5600,hsa126961,hsa11335
```

### Console output

The step's log lines (S4.17), which `fusion` prints after the command echo (S1.10). Reference run:

```
Corridor from EGFR (hsa1956) to CXCL8 (hsa3576), bound 10, edges_to_target any
  Traversable edges: 1657 of 2529 (skipping component, association, dissociation, mapping)
  Edges into the target from the forward set: 11; dropped by edges_to_target: 0
  Forward set: 346 nodes within 10 edges of the source
  Backward set: 164 of them within 10 edges of the target
  Corridor, graph 3: 108 nodes, 358 edges; shortest path 4 edges
  Observed genes: 3 in the corridor, 28 outside it, 14 not in the network
```

The full run ends `Corridor, graph 3: 547 nodes, 2117 edges; shortest path 3 edges` and `Observed genes: 20 in the corridor, 24 outside it, 1 not in the network`. With exclusions, a line such as `Excluded: JUN (hsa3725), and the complex [ESR1+NCOA3+JUN] that contains it` follows the bindings. An empty corridor ends with its report, for example `no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3: the shortest path has 4 edges`, then `Graph 3 is an empty corridor; the later lines of this run are skipped.` (01.C15). A reused corridor prints `Corridor, graph 4: same as graph 3` (05.C4).

### Events

Section 09 owns the envelope (`v`, `seq`, `t`, `type`) and the generic step events; `core::Events` adds `step`, `line` and `point` to every event emitted inside a step (cross-section item 1.3). This step adds one `corridor.summary` event, a `warning` event per warning, and section 04's match event when it matches observations. Reference run, with `v`, `seq` and `t` left out:

```json
{"type": "step.started", "step": "fb_rch", "line": 191, "point": "", "cmd": "fb_rch"}
{"type": "corridor.summary", "step": "fb_rch", "line": 191, "point": "", "graph": 3, "nodes": 108, "edges": 358, "shortest_path": 4, "forward_set": 346, "backward_set": 164, "digest": "ba0c16be…"}
{"type": "step.finished", "step": "fb_rch", "line": 191, "point": "", "status": "ok", "wall_s": 0.004, "outputs": ["fb_rch/corridor.json", "fb_rch/corridor_nodes.tsv", "fb_rch/observations_match.tsv"]}
```

An empty corridor gives zero counts in `corridor.summary`. The result itself goes only in `step.finished`, with status `reported` and `"report": {"code": "corridor.empty", "message": "no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3: the shortest path has 4 edges"}`, and each later line then gives a `step.skipped` event (01.C15). A reused corridor gives `"same_as": 3` in `corridor.summary` and no outputs. A failure gives section 09's `error` event and status `failed`.

### Saving, loading and reuse

- **Save** (decision 0019). `save()` writes the files above. They are small, and they hold outputs a user asked for (S4.07, S4.16), so they are always written.
- **Load.** To start from a saved corridor, or to resume a stopped solve (01.C6 and section 09), the run first loads the network the corridor was cut from, then calls `load()`. 09's registry has already checked the file's SHA-256 (`artefact.changed`) and format version (`artefact.newer_format`). `load()` checks the network's content hash, maps every node and edge key to the network's indices, and re-binds every given id through the run's `BindingTable`, which must give the same node (S4.15); any mismatch is `corridor.network_mismatch`. It then recomputes the digest. It does not rerun the search, so a resumed solve runs on exactly the corridor it started on, even under a later FuSION version (05.C6).
- **Reuse within a run** (05.C4). When an `fb_rch` has the same network and the same normalised request as an earlier corridor of the run, the command registers that corridor again instead of computing a new one.

### Determinism and portability

- Nothing is random. The corridor depends only on the network's content and the request. The search follows the network's edge order, but distances and kept sets do not depend on it.
- Output order never comes from a hash map, whose iteration order differs between libstdc++, libc++ and Microsoft's library. Files follow the network's canonical index order (02.C10) or, for the node table, an explicit sort; keys compare with `core::stable_compare`, byte by byte (cross-section items 1.1 and 2A.5).
- Numbers are written with core's exact number format or nlohmann/json, so no locale enters any file. Files are written in binary mode with `\n`, so they are byte-identical on Linux, macOS and Windows; readers accept `\r\n`.
- `corridor.json` holds no path: it names its network by graph id and content hash, so a run directory copied to another machine still loads.
- The step starts no process, uses no shell and handles no signal.

## Choices

### 05.C1 · The corridor is a view with its distances

- **Proposed:** The corridor is section 02's `GraphView` (02.C9): the network, shared read-only, plus one bit per network node and per network edge. On top of the view it keeps its request, the bindings from the run's `BindingTable`, the excluded nodes, ds and dt as two arrays over the network's nodes, the counts, the empty reason and a digest. It copies no node or edge, keeps no lookup of its own and no second index: the network's indices name corridor nodes in every step. Once built it never changes. This is 02.C9 applied to the corridor, so one answer covers both.
- **Alternatives:**
  - A copied subgraph with its own indices, as FuSION 1 copies (S4.13). Every network command (`sz`, the writer, a later `fb_rch`) would work on it unchanged, and its saved form would stand alone. But it needs the network's lookup either rebuilt or copied, and every output must translate its ids back.
  - The view plus a compact adjacency over dense corridor positions, for the solve's arrays.
- **Why:**
  - S4.14 and S4.15 hold by construction. A corridor node is a network node, so EGFR has one index in the network, the corridor and both solves. FuSION 1 gives it 3273, 7596 and 10044 (S4.14).
  - A rebuilt lookup rebinds 57 of the 1399 ids the full run's corridor resolves (S4.15), for example ATF2 to the 10-gene CREB3-family node.
  - Dense positions buy nothing measurable. A view walks a node's network out-edges and tests a bit, O(deg); the largest measured corridor has 3637 edges, and the distance arrays over the network cost 65 KB for the 82k graph.
  - Read-only by construction: `Network` is const and the corridor has only const accessors, so both FSC solves, and any solves run in parallel, share one object (S4.20, S5.29).
- **Decision:** as proposed (prompt log entry 161)

### 05.C2 · Trim during the search, not after the keep rule

- **Proposed:** The searches never follow a self-loop, an edge into the source or an edge out of the target (S4.11, as S5.13 settled it), and the keep rule runs on what remains. The corridor is then exactly the edges on walks of at most b edges that never re-enter the source and never leave the target. This is a reading of S4.11's "trim", not an amendment of it (cross-section item 4B.25; prompt log entry 170).
- **Alternatives:**
  - Keep, then trim: run the keep rule of S4.09 on the untrimmed edges, then delete the three edge classes. Nodes that were kept only through those edges stay, cut off from the source or from the target. This is the literal reading of S4.11's decision, and it keeps FuSION 1's counts for those nodes.
  - Trim to simple paths: also drop every node and edge that lies on no simple path within b. That goes beyond S4.11's decision and is exponential; Phase 3 measures a capped version first, and the user then decides (prompt log entry 170).
- **Why:**
  - The cost is the same: the trim is three comparisons in the edge test, and both versions are linear.
  - No solution path loses an edge, because a simple source-to-target path never re-enters the source or leaves the target.
  - Keeping then trimming leaves nodes that can never be present. A present node lies on a selected path from the source to the target with strictly increasing distances (S5.11), so it lies on a simple path of at most b edges, and these nodes lie only on walks through the source or the target.
  - How often, measured on the 29 configurations:
    - Under S2.11's default, trimming first removes them in 5 configurations, 2 to 5 nodes and 2 to 15 edges each: the reference run, the full run, `egfr_cxcl8_jun_b7_nr8_er8`, experiment7.1 and `real_experiments/experiment4.1`.
    - With "1+", also in 5, with experiment5.0 in place of experiment7.1: up to 10 nodes and 101 edges, 11 % of the 933 edges experiment5.0's solve would encode.
  - Observed genes among them cost a node relaxation at every point, in both FSC cases, because an up-regulated gene that is not present is relaxed (S5.12). The recorded witnesses show it:
    - AREG in the full run, at all 6 points with the FSC and all 7 without (`run/temp_w_relaxations.txt`, `temp_wo_relaxations.txt`). AREG is 6 edges from EGFR. Its route to CXCL8 back through EGFR has 4 edges, which fits the bound of 10; its shortest route that avoids EGFR has 5, which does not.
    - HBEGF in `egfr_cxcl8_jun_b7_nr8_er8`, at all 4 and all 3 points.
    - Both networks hold no indirect relations, so both cases hold under the default.
    - DUSP1 and JUNB in experiment5.0, the only relaxed genes of its single point (n 2, e 0) in both cases. FuSION 1's build kept that network's indirect relations, so this case holds only with "1+". Under the default, experiment5.0's corridor shrinks to 71 nodes and 188 edges, and trimming first changes nothing there.
  - Trimming first reports these genes as outside the corridor (05.C9). So every point of those curves moves down in n by 1 (AREG, HBEGF) or 2 (DUSP1 and JUNB, with "1+"), and experiment5.0's becomes (n 0, e 0). That assumes the recorded frontiers are exact (S5.19). The shapes and the FSC comparison do not change. Old results change, which decision 0023 allows.
  - Trimming first also drops edges that join two nodes which stay, because their only bounded walks pass through the source or the target: 5 of the 34 extra edges under the default (1 in experiment7.1, 4 in experiment4.1), and 74 of 120 with "1+" (71 in experiment5.0). They lie on no simple path within b, and no recorded witness relaxes any of them.
  - The corridor then equals the set the solve considers (S5.10) in all 29 configurations. Keeping then trimming does not: in the reference run it leaves 112 nodes and 363 edges, of which the solve considers 108 and 358, so the corridor shown to a user and the one encoded would differ.
- **Decision:** as proposed, as the reading of S4.11's trim (prompt log entry 170)

### 05.C3 · Ids are checked before the first step as well as at the step

- **Proposed:** Two checks. Before the first step, every source, target and excluded id of every `fb_rch` line must appear among the entry ids of the network inputs the batch names: the KGML candidate list, a merged-graph or legacy file, an edge table, or a saved network that the batch loads. The ids come from section 02's pre-run parse (02.C15). An id found in none of them is a failure, `corridor.id_not_in_inputs`, before anything runs (decision 0021). A network input that the batch itself makes is scanned just before its step, as 01.C12 checks such inputs (prompt log entry 188). When the step runs, S4.02's binding check runs again against the built network.
- **Alternatives:**
  - Check only when the step runs, as FuSION 1 does and as this section's earlier draft did.
  - Check only before the first step. That is not enough: a network built during the run depends on GSEA's selection.
- **Why:**
  - Decision 0021 has the backend validate all inputs before the first step runs, so that a typo cannot cost a long run. A mistyped target, such as `hsa35766` for `hsa3576`, otherwise surfaces only after GSEA and the merge.
  - The check is cheap. Scanning the entry ids of the 167 candidate KGMLs (6.7 MB, 6,938 distinct ids) takes 0.22 s in Python, and nothing extra with 02.C15's pre-run parse.
  - The step-time check is still needed. An id can sit in a pathway that GSEA does not select, in an entry that no relation reaches (S2.06), or in one reached only by indirect relations that S2.11's default discards.
  - The frontend already accepts only ids found in the symbol map (S8.12); batch users get no such check otherwise.
- **Decision:** as proposed (prompt log entry 170)

### 05.C4 · A repeated `fb_rch` with the same arguments reuses the corridor

- **Proposed:** When an `fb_rch` has the same network (by content hash) and the same normalised request as an earlier corridor of the run, the command registers that corridor again under the new graph id, prints `Corridor, graph 4: same as graph 3`, and writes no files; its run.json entry points at the first call's files. A different network or any changed argument computes a new corridor.
- **Alternatives:**
  - Recompute: an equal but separate corridor, with its own identical files in `fb_rch-2/`.
- **Why:**
  - S4.20 decided that the corridor is computed once and both solves use it. 93 of the 113 FuSION 1 batch files that call `fb_rch` call it twice with the same arguments, and in every logged pair both corridors have the same size (S4.20). Reuse honours the decision without changing those batches.
  - Time does not decide it, since a corridor costs milliseconds. Reuse gives one object and one set of files, so the two solves provably read the same corridor.
  - It is safe, because the corridor never changes once built (05.C1).
- **Decision:** as proposed (prompt log entry 171)

### 05.C5 · Three id problems stop the step

- **Proposed:** These are failures (S1.28), reported before any search: an excluded id that binds to no node; a source or target that is also excluded, directly or as a complex that contains an excluded node (prompt log entry 170); a source and a target that bind to the same node.
- **Alternatives:**
  - Warn and drop an unknown excluded id, as FuSION 1 does (`Excluded node <id> has no rep id in graph <gid>. Ignoring exclusion`, `Linux/src/GraphManagerNew.cc:17815`).
  - Let an exclusion win over an endpoint, which gives an empty corridor.
  - Report a source equal to the target as an empty corridor, so a sweep carries on.
- **Why:**
  - Decision 0021's reasoning applies to ids as it does to files: a typo must not silently change a run that may take hours. No batch excludes anything yet (S4.01), so nothing old breaks.
  - FuSION 1 handles an excluded endpoint by failing later with a message that blames the edge types (S4.06, S4.18 cases 2 and 3).
  - The source and target each get a node of their own (S2.13), so they share a node only when both name one gene, possibly through two spellings that 02's id rule maps to one id. That is a mistake, and with trimming a corridor from a node to itself is always empty; an empty result in a sweep would hide it.
- **Decision:** as proposed (prompt log entry 171)

### 05.C6 · Saved form: `corridor.json` naming its network, plus a node table

- **Proposed:** A call that computes a corridor writes `corridor.json` (request, bindings, excluded nodes, the network's content hash, counts, nodes with ds and dt, edges with their attributes, and a digest) and `corridor_nodes.tsv` (for people), beside section 04's `observations_match.tsv` (05.C9). Loading needs the network the corridor was cut from: the loader checks its content hash, maps keys back to its indices and re-binds the given ids, without rerunning the search.
- **Alternatives:**
  - A self-contained file that also embeds member genes and the network's whole id lookup, so that a solve could start from the corridor alone. The lookup has 2246 ids (about 53 KB) in the reference network and 4837 (about 116 KB) in the full one. Section 02 also offers writing a view as a merged-graph file with `in_view` marks.
  - Save only the request and the digest, and recompute the corridor on load (at most 41 ms in the replay).
  - JSON only, without the node table.
- **Why:**
  - One source of node and edge attributes, and one id binding per run (S4.15). A second lookup inside the file would be another place where a binding can differ.
  - A resumed solve must run on the identical corridor (decision 0019). The digest guarantees that. Recomputing would not, once a later version changes the search, as 05.C2 changes FuSION 1's.
  - The network is always available when a corridor is loaded: it is the step before, saved in the same or an earlier run directory (section 09).
  - Sizes: 60 KB, 348 KB and 586 KB for the reference run, the full run and the 82k graph's corridor.
  - S4.07 asks for distances "for inspection and for the graph display": the JSON serves the display, and E1 puts tables in TSV, which a spreadsheet opens.
- **Decision:** as proposed (prompt log entry 171)

### 05.C7 · The solve's bound must equal the corridor's

- **Proposed:** The solve takes its path bound from the corridor. A bound given to the solve, as FuSION 1's positional `pathz3` line always gives one, must equal the corridor's, or the solve fails with a message that names both (S4.21). Section 06 makes the same check.
- **Alternatives:**
  - Allow a smaller solve bound b′ and solve on the corridor cut at b, as FuSION 1 does.
  - Allow a smaller b′ and have the solve cut a sub-corridor at b′ first.
- **Why:**
  - Off-path edges count (S5.13). A corridor cut at b holds edges, and observed genes, that lie only on walks longer than b′. Solving at b′ on it gives a different result from solving on a corridor cut at b′, which is what a user means by "bound b′".
  - All 206 batch lines pass one `$REACH_PATH_BOUND` to `fb_rch` and `pathz3` (S4.21), so no old batch fails.
  - A larger solve bound cannot be met: FuSION 1 silently caps it (S4.21).
  - The sub-corridor alternative is correct but adds a second way to cut a corridor that nothing needs: a bound sweep reruns `fb_rch`, which costs milliseconds.
- **Decision:** as proposed (prompt log entry 171)

### 05.C8 · Names for `fb_rch` and its arguments

- **Proposed:** The command keeps the name `fb_rch`, with `corridor` as its long name. Its arguments are `graph`, `source`, `target`, `exclude`, `bound`, `edges_to_target` (values `any`, `activation` and `expression`, or 0, 1 and 2), `skip_subtypes` and `observations`; their registry keys are `corridor.<name>` (cross-section item 1.16).
- **Alternatives:**
  - `to_target` in place of `edges_to_target`, as this section's first draft named it.
  - `both` in place of `any` for value 0, after FuSION 1's `$BOTH_ACT_EXPR_EDGES_TO_TARGET`.
  - `non_traversable_subtypes` in place of `skip_subtypes`.
  - `fb_rch` only, with no long name.
- **Why:**
  - Positional FuSION 1 lines keep working whatever the names (decision 0017).
  - `edges_to_target` is FuSION 1's own name: the help line's `<edges_to_tgt>` (`Linux/src/main.cc:904`) and the `$EDGES_TO_TARGET` that 111 batch files define.
  - `any` says what 0 does: every edge passes, subtype-less ones included. `both` suggests a filter that passes two kinds.
  - The frontend's three labels, "Activation edges only", "Expression edges only" and "No restriction" (S8.09), map one to one.
  - These names also fix those in section 01's command table and in sections 09's and 10's examples (prompt log entry 172).
- **Decision:** as proposed (prompt log entry 172)

### 05.C9 · The corridor step reports where every observed gene falls

- **Proposed:** `fb_rch` takes the run's observations, by default the latest `cudf` result (01.C16). It calls section 04's `match_observations` with the network and the new corridor, and writes 04's `observations_match.tsv` in its step directory. Each observed gene is then in the corridor, in the network but outside the corridor, or not in the network: S3.12's two unmatched groups and S4.16's report, with the corridor's parameters beside them in `corridor.json`. The three counts also go on the console. Observations never change the corridor.
- **Alternatives:**
  - The solve does the matching and writes the report, since it has both the corridor and the observations, and `fb_rch` takes no observations.
- **Why:**
  - After a run that stops after the corridor (decision 0019), a user sees, before a solve of hours, which observations a bound or a pathway list leaves out. A smaller bound changes the observations, not only the search space (S4.03, S4.16).
  - FuSION 1 printed only `Up: 3 Down: 0` for the reference run's 45 listed genes. 2.0 prints 3 in the corridor, 28 outside it and 14 not in the network; for the full run, 20, 24 and 1.
  - Section 04 designs the matcher and its file for this step, so nothing is duplicated.
  - The solve still reports what it does with the genes inside the corridor, such as the exempt source and target and an avoided FSC (S5.10, S5.12).
- **Decision:** as proposed (prompt log entry 172)

## Interfaces with other sections

**01 · Batch language and interpreter**
- Needs: the command-table entry for `fb_rch` and `corridor` with 05.C8's names, positions, types and defaults, and the syntax checks of S4.01 and S4.05 before the batch starts (decision 0021), including a `graph` argument that names no graph or a corridor (`batch.bad_value`, 1.17 and prompt log entry 230).
- Needs: the graph store. The corridor takes the next graph id, and `0` names it. Before computing, `fb_rch` looks for an equal corridor to reuse (05.C4).
- Needs: 01.C16's latest-value defaults, which give `graph` the latest graph and `observations` the latest `cudf` result; with no observations, nothing is matched.
- Needs: the run line and `fusion --set` for the run-level `corridor.skip_subtypes` (2B.2), with entry 229's precedence: a line's own argument, then `--set`, then the run line, then the default.
- Needs: the pre-run check to call `ids_missing_from_inputs` for every `fb_rch` line, with the ids of the point's network inputs, and again just before its step for an input the batch makes (05.C3, 01.C12).
- Needs: after a `corridor.empty` report, every later line of the run or sweep point is skipped with a `step.skipped` event, and the run exits 0 (01.C15, widening S4.18's wording).
- Needs: 01.C11 for FuSION 1's `exec mv fb_rch …` and `exec mv fb_rch_back …` lines (111 files). In all 111 files the first such line comes after the last `fb_rch`, and in 109 after a `pathz3` too. Without 01.C11, S4.19 and S1.28 together would stop those batches at that line.

**02 · Network model, inputs and id mapping**
- Needs: an immutable network shared as `std::shared_ptr<const Network>`; indices in canonical order (02.C10); out- and in-edges in index order; tail, head, type, ordered subtype ids, pathways and score per edge; keys, labels, node types and complex components; the content hash.
- Needs: `GraphView` (02.C9), the run's `BindingTable` with its four statuses (S2.03, S4.15) and its `bindings.tsv` (02.C13).
- Needs: the entry ids of every network input from the pre-run parse (02.C15), for 05.C3.
- Needs: each complex's component list, so the step can close the excluded set over complexes (prompt log entry 170); the solve uses the same closure for every gene on an avoid list (prompt log entry 229).
- Needs: subtype names normalised to FuSION's vocabulary (S2.10) by every reader, with `normalise_subtype` exported (2B.18), so the default `skip_subtypes` and the edges-to-target classes apply to every source (decision 0020).
- Needs: duplicate edges merged on reading with S4.13's rule (02.C5); the corridor has no merge step.
- Agrees: the edge key includes the type (02.C11). Within corridors, leaving it out would merge 15 keys in the full run's corridor and 32 in the 82k graph's, for example GNAS → ADCY1 `[activation]` as PPrel and PCrel, and PLCG1 → PRKCA `[compound]` as PPrel and ECrel.
- The ECrel setting (S2.31) and the indirect-effect setting (S2.11) act when the network is built (02.C6), so the corridor sees only what they leave.
- Gives back: `sz` on a corridor prints its counts through the view; a corridor has no isolated nodes.

**03 · Merge**
- Needs: the source, the target and the FSC in the exception set (S2.13), through section 01's look-ahead, so that each endpoint binds to a node of its own.

**04 · Expression**
- Needs: the observations value, with its `Regulation` per gene, after the frontend's Not DE overrides (S8.15); `match_observations` and the writer of `observations_match.tsv`, which this step calls with its corridor (05.C9).
- Gives: a membership test for nodes in the corridor (`Corridor::view()`).

**06 · Solve**
- Gives: the `Corridor` above: the view (membership and adjacency in index order), source, target, bound, the bindings, the excluded nodes, ds and dt, and a read-only object that both FSC cases and any parallel solves share (S4.20, S5.29). An empty corridor never reaches the solve.
- With 05.C2, S5.10's encoded set is the whole corridor (proved above, equal in all 29 configurations); section 06 keeps its search as the definition, which stays correct for any corridor.
- The solve takes source, target and bound from the corridor; a pair or bound given to it must match (S5.04, 05.C7).
- An FSC or essential node outside the corridor makes that case infeasible, which the solve reports (S5.11, S5.17). Every gene on an avoid list, the out case's FSC included, also forbids every complex that contains it (prompt log entries 170 and 229, extending S5.17).
- ds and dt are valid bounds for the solve's distance variables: a present node v has dist(v) ≥ dist(source) + ds(v) and dist(v) ≤ b − dt(v). Section 06 may assert them to help Z3.
- The corridor's digest can key resume state and caches (S5.20).

**07 · Results and outputs**
- Gives: stable keys for nodes and edges (02.C11), ds and dt per node for the display's layering (S4.07, decision 0027), and `corridor.json` as a display-ready graph.

**09 · Runs**
- Needs: `core::Context` (files, settings, log, events), `ctx.files.create()` and `create_debug()` with 09's `OutputFile`, `core::Outcome` and `Report`, `core::Failure`, `core::Sha256` and `core::stable_compare`, the `corridor` artefact kind in run.json with its registry checks (`artefact.changed`, `artefact.newer_format`), the step directory `fb_rch/`, and registry keys `corridor.<name>`.
- Gives: the `corridor.summary` event type and the `corridor.*` codes above, which section 09's tables should list.

**10 · Frontend architecture**
- The restriction page's three choices map onto `edges_to_target`'s values (05.C8, S8.09). `skip_subtypes` keeps its default unless a pipeline block sets it; `--commands` exports it (2B.2). The frontend refuses b < 1 (S8.09); the backend still accepts 0 and reports an empty corridor.

**12 · Test architecture**
- See Test hooks.

## Test hooks

Library tests (Catch2) on small synthetic networks:
- the keep rule: a diamond, a node exactly at distance b, b = 0, and a corridor equal to its walk definition;
- trimming: a self-loop, an edge into the source, an edge out of the target, a node whose only route within the bound passes back through the source (the AREG case), and a node reached only through the target (the CXCR6-family case of the reference run);
- `edges_to_target` in all three modes, an edge carrying both kinds (passes both), an edge carrying neither (passes neither), and the two counts at a tail exactly b edges from the source;
- exclusions: an excluded node and its edges are absent; a complex that contains an excluded gene is absent, and so is a complex nested around it; an endpoint that is such a complex fails as `corridor.endpoint_excluded`; excluding a gene of a family node excludes the node with a warning;
- complexes: members are never added unless kept (S4.12);
- each empty reason, with the shortest path, and each failure code, with nothing registered; `ids_missing_from_inputs` on a typo;
- reuse: a second equal request returns the same object and writes nothing (05.C4);
- determinism: the same network built from inputs in another order gives byte-identical `corridor.json` and the same digest.

Property tests on random small networks:
- every edge of every simple source-to-target path of at most b edges is kept, checked by brute force;
- every kept edge lies on a walk of at most b edges in T;
- the corridor equals the set S5.10's search encodes;
- save then load gives the same digest; the corridor never holds an excluded node; its digest is the same before and after a solve (S4.20).

Reference tests (decision 0023, section 12): the 29 configurations of the log census.
- Baseline: FuSION 1's printed forward-set, target-edge and corridor counts.
- Under "1+", which keeps the networks as FuSION 1 ran them, the expected differences are recorded: isolated members gone (S4.12), trimmed edges, the nodes of 05.C2, and the observed genes that leave the corridor (AREG, HBEGF, DUSP1 and JUNB). The two target-edge counts must equal FuSION 1's.
- Under the default, the 11 configurations on older networks also change because indirect relations are discarded. The replay's numbers above are the expected values.

Phase 3 measurement (prompt log entry 170): a capped exact pruning to simple paths in C++, keeping a node whenever the cap is hit, run on the 29 configurations. It reports the nodes and edges removed, the observed genes among them (IRS2 in the reference run), and the time against the cap; the user then decides whether 2.0 prunes.

Benchmarks: the corridor of the 82k graph (8139 nodes, 82,188 edges). The proposed budget is 10 ms, recorded on all three platforms.

End to end: a bound sweep b = 0 to 10 on the reference network. Bounds 0 to 3 report `beyond_bound` with shortest path 4, and each such point skips its later lines (01.C15) while the sweep carries on; from 4 up the solve runs. At b = 4 the corridor has 7 nodes and 9 edges.

Hooks: `compute()` writes no file; `save()` and `load()` are separate; a debug-build `check_invariants(const Corridor&)` checks the view against the keep rule and the distances, for the tests to call.

## Open questions

All three are closed.

1. **Where duplicate edges are merged.** Closed: 02.C5 moves S4.13's merge to reading (prompt log entry 159), so the corridor has no merge step.
2. **Nodes on walks but on no simple path.** Closed: the walk rule stands; Phase 3 measures a capped exact pruning, and the user then decides (prompt log entry 170).
3. **Excluding a gene, and the complexes that contain it.** Closed: excluding a gene also removes every complex that contains it, a new rule extending S4.06 (prompt log entry 170); in section 06 every gene on an avoid list, the out case's FSC included, forbids them, extending S5.17 (prompt log entries 170 and 229).
