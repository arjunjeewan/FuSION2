# 04 · Reachability corridor

## Scope

This stage covers `fb_rch`: its handler in `Linux/src/main.cc:2109-2232`, `GraphManagerNew::bounded_forward_backward_reach` (`Linux/src/GraphManagerNew.cc:17759-18158`), the two copy helpers it calls, `duplicate_edge` (`9388-9503`) and `duplicate_node` (`9183-9284`), and the edge-class macros in `Linux/include/macros.h:69-77`. It starts from the graph that stage 2's `rgx` re-reads (S2.05, S2.26) and ends by handing a new corridor graph to stage 5's `pathz3`, which picks it up as graph `0`. Items S4.15, S4.16 and S4.21 follow that hand-over only as far as pathz3's reading of the corridor; the encoding is stage 5's. Stage 1 owns the token list (S1.05), dispatch and the help listing (S1.11, S1.12), the missing-`start` crash (S1.15), the launch directory (S1.26), failure policy (S1.28) and the compile-time flags (S1.29), and this stage cites those items. The `fb_rch` block of `macOS/src/main.cc` is identical to Linux's, and `GraphManagerNew.cc`, `GraphManagerNew.h` and `macros.h` are byte-identical (`cmp`), so this stage has no macOS difference. `bounded_reach` (`GraphManagerNew.cc:17354`) is reached only from two dead commands inside `#if 0`, `rch` (`main.cc:1992`) and `fwd_bkwd_rch` (`main.cc:3160`, `3204`), so it is out of scope.

Names and evidence used below:
- "the reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`. Its rgx graph 84 has 1134 nodes and 2529 edges, and its corridor, graph 86, has 115 nodes and 370 edges.
- "the full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/`. Its master graph 2 has 3100 nodes and 7400 edges, and its corridor, graph 4, has 592 nodes and 2138 edges.
- Both runs call `fb_rch 0 hsa1956 -1 hsa3576 -1 -1 10 0` (EGFR → CXCL8, bound 10).
- "the replay" is a Python re-implementation of the algorithm described below. It runs on each run's `run/merged_master_graph.xml`, with one node per entry that a relation reaches and one edge per relation (tool-written read, S2.12). It reproduces every count the runs print:
  - edges into the target: 11 and 18;
  - forward nodes: 354 and 1186;
  - forward graph: 376 nodes and 895 edges, and 1264 and 3535;
  - backward entries: 171 (the length of `fb_rch_back`) and 670;
  - corridor: 115 nodes and 370 edges with 3 isolated nodes, and 592 and 2138 with 41 isolated;
  - pathz3's `Up: 3` and `Up: 21`.

  It also reproduces two runs added on 2026-11-09, `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3` and `r7_epha2_curated` (289 nodes and 1246 edges; 306 and 1285). Counts not printed by FuSION come from the replay; FuSION 1 was not run.
- Batch counts cover all 89 `*.batch` files in the repo when this stage was drafted. 86 of them call `fb_rch`, in 170 lines. By the time it was verified, `resolve_wo_n7_e0.batch` had been added: 90 files, 87 calling `fb_rch` in 172 lines, 85 calling it twice, and 12 using flag 2.

## Items

### S4.01 · `fb_rch` command line

- **FuSION 1:** `Linux/src/main.cc:2109-2232`; help line `main.cc:904`; GUI template `Linux/frontend/src/gui4.java:2175-2176`
- **What it does:** The batch form is

  `fb_rch <graph-id> <source id>… -1 <target id>… -1 <excluded id>… -1 <bound> <edges_to_tgt>`

  - `<graph-id>` is parsed with `stoi`. `0` means the latest graph, `graph_id_count` (`main.cc:2119-2120`).
  - Each of the three lists is read token by token until a token that is exactly `-1`. The ids go into a `std::set<string>`, so repeats collapse and order is lost. An id is any string the input graph's id lookup knows (S4.02): `hsa1956`, `cpdC00076`, or a complex key such as `c_s_hsa4149_hsa4609_c_e`.
  - `<bound>` (S4.03) and `<edges_to_tgt>` (S4.05) are parsed with `stoi`. Tokens after the flag are ignored.
  - The command prints `Bound: <bound>`, runs the search and prints `Reachability Graph: <id>` on success (S4.17).
  - Each call registers two new graphs, the forward graph (S4.08) and the corridor. The corridor is created last, so afterwards `0` names it.

  Real line: `fb_rch 0 hsa1956 -1 hsa3576 -1 -1 10 0` (reference run, `run_console.log:238`).
  - All 170 batch lines have one source, one target and an empty exclusion list.
  - 84 files call `fb_rch` twice (S4.20). Two single-phase runs call it once: `runs/egfr_b10_gsea_fdr044_candidates/r7_epha2_curated` and `r8_phlda1_curated`.
  - The GUI template has eight slots in the same shape (graph, source, `-1`, target, `-1`, `-1`, bound, flag).

  Interactive mode prompts `Source node ids list ending with -1: `, `Target node ids list ending with -1: ` and `Excluded node ids list ending with -1: `, then reads the bound and the flag without prompts. S1.03 drops this mode.
- **Problems:**
  - A missing `-1` sends a list loop through the empty slots of the 1,000,000-slot token vector (S1.05). It inserts `""` each time and then reads past the vector's end. For example, `fb_rch 0 hsa1956 hsa3576 -1 -1 10 0` takes both genes as sources and gives an empty target list. It then takes `10`, `0` and every empty slot as excluded ids.
  - A missing or non-numeric graph id, bound or flag throws from `stoi` and aborts the run (S1.28).
  - The help listing names the command `fwd_bkwd_rch (or fb_rch)`, but only `fb_rch` is dispatched (S1.12). `fwd_bkwd_rch` is the name of a different, dead command inside `#if 0` (`main.cc:3121`).
  - Extra tokens are ignored silently.
- **Proposed call:** fix. Keep the positional form for FuSION 1 batch files (decision 0017) and add names (decision 0018). Reject a list without its terminator, a missing argument or extra tokens, with a message.
- **Decision:** fix, as proposed (prompt log entry 057)

### S4.02 · Resolving the listed ids to nodes

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17775-17822`; lookups `797-808` (`get_rep_id_from_id`) and `876-887` (`get_nid_from_rep_id`)
- **What it does:**
  - Each source, target and excluded id is resolved in the input graph in two steps: `rep = id_to_rep_id_map[id]`, then `nid = rep_id_to_nid_map[rep]`.
  - As S2.03 describes, the first claim wins, and a member id resolves to its whole family node.
  - An id that is not in the lookup is skipped with a warning on stderr: `Source node <id> has no rep id in graph <gid>. Ignoring.` Unknown targets and excluded ids get the same warning, `Target node …` and `Excluded node … Ignoring exclusion`.
  - The resolved node ids are kept in vectors, not sets.
- **Problems:**
  - An unknown source or target only gets a warning. If no source or no target resolves, the call fails later with a message that blames the choice of edge types (S4.18, case 2).
  - Two listed ids for one node seed that node twice. It is dequeued and scanned twice, and its edges into a target are counted twice in the `N out of M` line (S4.17).
  - Exclusion works on nodes, so excluding one member id excludes its whole family node without a message.
  - These ids are resolved in the input graph. pathz3 resolves its own files against the corridor's rebuilt lookup, which can bind the same id to a different node (S4.15).
- **Proposed call:** fix. An unresolvable source or target is a command error (decision 0021). Resolve each id once, and report which node each id bound to (S2.03).
- **Decision:** fix, as proposed (prompt log entry 057)

### S4.03 · The bound and how path length is counted

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17769-17773` (b < 0 rejected), `17917-17921`, `18087-18091`, `18111`, `18132`; `Linux/include/macros.h:52` (`inf` is `INT_MAX`)
- **What it does:**
  - Length is the number of edges. A path of length k has k edges and k + 1 nodes, and the bound b admits every k ≤ b.
  - Only traversable edges count (S4.04), and edges have no weights. A complex node is an ordinary node: reaching it takes an edge into the complex itself, because component edges are never traversed (S4.04, S4.12).
  - KEGG `indirect effect` relations are not edges at all, because stage 2 discards them at read time (S2.11). An indirect step is therefore neither a hop nor part of any path.
  - b < 0 prints `Error: bound <b> is invalid` (stderr). b = 0 is accepted, although it can never keep an edge (S4.18).
  - pathz3 counts the same way: each present edge must increase node distance (`GraphManagerNew.cc:28106-28118`), and each target's distance is at most pathz3's own `path_bound` argument (`28121-28140`). Stage 5 owns that encoding.
  - Bounds in the batch files: 6 (21 files), 7 (7), 10 (31), 12 (2) and 15 (25).
  - The GUI calls the bound "Maximum Signalling Path Length" and offers 0 to 100 (`Linux/frontend/src/ReachPathBoundCard.java:23-26`).
- **Problems:**
  - The bound is not stored with the corridor, and pathz3 takes its own (S4.21).
  - The bound changes more than the search space. `macOS/model_example/RUNNING.md:125-127` warns that "a smaller bound is not a milder version of this run", because it shrinks the set of expression changes pathz3 sees (S4.16).
- **Proposed call:** keep. Port edge-count length, b ≥ 0, with b inclusive.
- **Decision:** keep (prompt log entry 057)

### S4.04 · Which edges can be traversed (`ONLY_INTERAC_EDGES_FOR_MINCUT`)

- **FuSION 1:** `Linux/include/macros.h:69-74` (`NOT_INTERAC_EDGE`), applied at `Linux/src/GraphManagerNew.cc:17883-17886`, `17968-17971`, `18076-18079` and `18141-18144`; `Linux/Makefile:10`
- **What it does:**
  - Every build defines `ONLY_INTERAC_EDGES_FOR_MINCUT`. An edge is therefore never followed or kept if any one of its subtypes is `component`, `association`, `dissociation` or `mapping`.
  - Every other edge can be traversed, whatever else it carries. That covers activation, inhibition, expression, repression, phosphorylation, dephosphorylation, ubiquitination, compound, missing and state.
  - Edges with no subtype at all are also traversable: 22 in the reference graph and 165 in the full graph. How they are signed is stage 5's question.
  - In the reference graph 1657 of 2529 edges are traversable; the other 872 include the 404 component edges. In the full graph 6462 of 7400 are traversable.
- **Problems:**
  - A signed relation that KEGG also marks as binding is dropped together with the binding. 21 edges in the reference graph and 40 in the full graph carry activation, inhibition or phosphorylation along with `association` or `dissociation`.
  - Only 8 of the 21 in the reference graph and 4 of the 40 in the full graph come from `path:hsa04062` (chemokine signalling), such as CXCL8 → the CXCR6-family node [activation, association] in the full graph. Most of the rest are ligand → receptor pairs from other pathways, such as hsa04722, hsa04672, hsa04666, hsa04310 and hsa04621.
  - The raw read stores each such relation in both directions (S2.10). Following them would therefore also create receptor → ligand "activations". In the reference graph the only one that enters CXCL8 is such a reverse copy, CXCR6-family → CXCL8. That is the case for the current rule.
  - The rule is a compile-time flag, and no output records it (S1.29). Its subtype names are KEGG's vocabulary (decision 0020).
- **Proposed call:** fix. Make the set of non-traversable subtypes a run setting, with today's four subtypes as the default, and record it in the run's output. This follows S1.29's decision for flags that change results.
- **Decision:** fix as proposed: a run setting with today's four subtypes as the default, recorded in the output. Signed bindings stay non-traversable by default (open question 4). (prompt log entry 057)

### S4.05 · Edges into a target: the `edges_to_tgt` flag

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17889-17913` (search), `17973-17983` (forward copy); `Linux/include/macros.h:76-77`
- **What it does:** The flag acts only on edges whose head is one of the targets:
  - `1` drops such an edge if it has subtype `expression` or `repression`;
  - `2` drops it if it has `activation`, `inhibition` or `phosphorylation`;
  - any other value, including `0`, applies no filter.

  A dropped edge is neither followed nor kept. Over the edges it examines from nodes within the bound, the forward search counts the edges that enter a target (M) and the ones the flag drops (N). It prints `N out of M edges to target excluded due to choice of activation/expression edges to target.` (S4.17).
  - The batch files name the values with `let $ONLY_ACT_EDGES_TO_TARGET 1`, `let $ONLY_EXPR_EDGES_TO_TARGET 2` and `let $BOTH_ACT_EXPR_EDGES_TO_TARGET 0`.
  - The GUI offers "Activation edges only", "Expression edges only" and "No restriction" under "Final Edges in pathways restricted to:" (`ReachPathBoundCard.java:75-80`).
  - Use: 75 batch files pass 0, 11 pass 2 (7 of the 19 batches in `runs/real_experiments/`, and four of the 2026-11-09 candidate runs), and none passes 1.
  - Real flag-2 line: `fb_rch 0 hsa1956 -1 hsa1969 -1 -1 10 2` → `2 out of 4 edges to target excluded …` (`r7_epha2_curated/run_console.log`). The two activation edges into EPHA2 are dropped, and the curated ERK1/2 → EPHA2 expression edges (`path:hsa99901`) are kept.

  Effect at bound 10, from the replay:

  | Graph | Flag 0 | Flag 1 | Flag 2 |
  |---|---|---|---|
  | reference | 0 of 11 dropped; 115 nodes, 370 edges | 9 of 11; 103 nodes, 330 edges | 2 of 11; 115 nodes, 368 edges |
  | full | 0 of 18 dropped; 592 nodes, 2138 edges | 16 of 18; 533 nodes, 1956 edges | 2 of 18; 592 nodes, 2136 edges |
- **Problems:**
  - The names promise more than the rule does. Flag 2 ("expression edges only") keeps every edge into the target that lacks activation, inhibition and phosphorylation. That includes subtype-less, dephosphorylation, ubiquitination, compound, missing and state edges.
  - Flag 1 ("activation edges only") also drops an edge that carries activation together with expression.
  - In every graph measured, the reference and full runs and the 2026-11-09 candidate runs, each traversable edge into the target carries a single subtype: expression, activation or inhibition. No run so far therefore shows the difference.
  - A value other than 0, 1 or 2, such as a typo `3`, silently means no filter.
- **Proposed call:** fix. Accept only 0, 1 and 2, or their names, and make each rule match its name: keep only expression/repression edges into a target, or keep only activation/inhibition/phosphorylation edges. On every graph measured this gives the same corridors as today.
- **Decision:** fix, as proposed (prompt log entry 058)

### S4.06 · Excluded nodes

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17809-17822`, `17849`, `17915`, `18028`, `18082-18084`
- **What it does:** An excluded node is never seeded as a source or target and never entered, so it has no edges in the corridor. Exclusion applies to the whole node an id resolves to (S4.02).
- **Problems:**
  - No batch line and no GUI script passes an excluded node (S4.01).
  - An excluded node can still enter the forward graph and the corridor as an isolated member of a copied complex (S4.12). pathz3 then sees it.
  - The batches exclude the FSC in pathz3, through its avoid file, not here. Both solves therefore run on the same corridor, but not on the same observations: pathz3 removes avoided nodes from the up set (`main.cc:4668`), and the FSC, JUN, is up-regulated. The reference run prints `Up: 3` and then `Up: 2` (`run_console.log:281`, `350`), and the full run 21 and then 20 (`run.log:101`, `189`). Excluding the FSC here instead would change which expression changes pathz3 counts.
  - Excluding the only target makes the call fail with a message that does not name the exclusion as the cause (S4.18, cases 2 and 3).
- **Proposed call:** fix. Keep the list, and never let an excluded node into the corridor as a complex member.
- **Decision:** fix, as proposed (prompt log entry 058)

### S4.07 · Forward search from the sources

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17824-17856` (setup), `17858-17929` (search), `17940-17953`
- **What it does:**
  - The search is breadth-first. It uses traversable edges (S4.04) minus the edges into targets that the flag drops (S4.05), and starts from every resolved, non-excluded source at distance 0.
  - For each examined edge u → v with v not excluded, ds(v) = min(ds(v), ds(u) + 1). v is queued if ds(v) ≤ b and v is neither queued nor done.
  - A node at distance b is still dequeued and scanned. Its successors get b + 1 recorded but are not queued.
  - The dequeued nodes form the forward set F, and `fwd nodes <|F|>` is printed: 354 in the reference run, 1186 in the full run.
  - ds is the exact shortest distance from the nearest source in this filtered graph.
- **Problems:** The avoidable costs are small at these sizes:
  - Queue membership is tested with `std::find` over the whole queue on every relaxation (`17921`), about 0.92 million comparisons in the full run, since `std::find` stops at a hit, or about 1.2 million counting the backward search's test at `18091`. A "first reached" test would do, as the dead `bounded_reach` uses (`17620`).
  - `vector<int> nids = graph->get_node_ids()` (`17942`) copies the node list and is never used. The re-filter on `distances_from_source <= bound` (`17945-17952`) can never remove a node.
  - Every adjacency and subtype accessor returns a copy of its vector (`get_outlist` at `2119-2132`, `edge_has_subtype` at `1856-1873`). `NOT_INTERAC_EDGE` calls `edge_has_subtype` up to four times per edge.
- **Proposed call:** keep. Port the search and its distances; the costs above are implementation details, not behaviour.
- **Decision:** keep, and also write each corridor node's distances to the nearest source and target, for inspection and for the graph display ([decision 0022](../decisions/0022-graph-display.md)) (prompt log entry 058)

### S4.08 · The forward graph: an intermediate copy

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17824-17828` (allocation), `17954-18007`
- **What it does:**
  - Every input edge with both ends in F that is traversable and not dropped by the flag is copied, in ascending edge id order, into a new registered graph. There is no distance test per edge.
  - The copy goes through `duplicate_edge` (S4.13), so it merges duplicate edges and brings in complex members (S4.12).
  - It prints `eids <input edge count>`, `edges <edges selected>` and `Forward reachable graph id: <id> has <n> nodes <m> edges `, with a trailing space. Reference run: `edges 895`, then `Forward reachable graph id: 85 has 376 nodes 895 edges`. Full run: `edges 4126`, then `… id: 3 has 1264 nodes 3535 edges`.
  - The backward search (S4.09) runs over this graph. Nothing uses it afterwards, but it stays registered until the run ends.
- **Problems:**
  - Avoidable cost. In the full run the copy holds 2.1 times the corridor's nodes (1264 against 592) and 1.7 times its edges (3535 against 2138). It takes a graph id and fresh node and edge ids, and it is never freed (S2.01).
  - The copy is not needed. For every node that ends up kept, all nodes on its shortest path to a target lie in F. Running the backward search on F inside the input graph therefore changes no kept node or edge.
  - Because of the copied complex members, this graph's node count (376) differs from |F| (354), so the two "forward" counts printed disagree.
- **Proposed call:** drop. Run the backward search on F within the input graph. The corridor is unchanged, and its graph id becomes one lower; no batch file hard-codes a graph id.
- **Decision:** drop, as proposed (prompt log entry 058)

### S4.09 · Backward search and the keep rule

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:18015-18050` (seeding), `18052-18099` (search), `18103-18116` (node rule), `18118-18136` (edge rule), `18138-18149` (copy), `18156-18157`
- **What it does:**
  1. Each non-excluded target is looked up in the forward graph by rep id. A target that is missing prints `Target <rep id> not forward reachable from any of the given sources ` (stderr) and is skipped. If every target is missing, the call fails (S4.18).
  2. A breadth-first search runs backwards along in-edges of the forward graph, starting from the targets at distance 0. It gives dt(v), the distance from v to the nearest target, and queues nodes while dt ≤ b.
  3. A node v reached by this search is kept if ds(v) + dt(v) ≤ b.
  4. An edge u → v of the forward graph is kept if both ends are kept and ds(u) + 1 + dt(v) ≤ b.
  5. The kept edges are copied, in ascending forward-graph edge id order, into the corridor graph (S4.13), which is returned. Reference run: `New graph 86 has 115 nodes 370 edges`.

  An edge is therefore kept exactly when it lies on some walk of at most b edges from a source to a target, and a node exactly when it lies on such a walk.

  Nodes enter the corridor only as ends of kept edges, or as complex members (S4.12). A kept node with no kept edge is therefore left out; that can happen only to a node that is both a source and a target.

  The backward search processes 171 nodes in the reference run (`fb_rch_back` has 171 entries) and 670 in the full run.
- **Problems:** None found in the rule itself. Walks versus simple paths is S4.11.
- **Proposed call:** keep.
- **Decision:** keep (prompt log entry 060)

### S4.10 · Several sources or targets

- **FuSION 1:** the code of S4.07 and S4.09, where ds and dt are distances to the nearest source and the nearest target
- **What it does:** With several sources and targets, the corridor is the union of the corridors of every (source, target) pair, cross pairs included. For a node that is both a source and a target, the corridor holds the cycles through it of at most b edges.
- **Problems:**
  - pathz3 solves only for the pairs in its connect-pairs file (`read_pairs_file`, `GraphManagerNew.cc:18624-18650`), which need not be all pairs. The corridor can therefore hold paths for pairs that no solve asks about.
  - No batch line uses more than one source or one target (S4.01).
- **Proposed call:** keep. The union contains each pair's corridor, so it is never too small.
- **Decision:** fix: build the corridor only for the (source, target) pairs that pathz3 will solve (prompt log entry 060)

### S4.11 · Walks, not simple paths

- **FuSION 1:** the keep rule of S4.09 (`Linux/src/GraphManagerNew.cc:18111`, `18132`)
- **What it does:** The keep rule is a test on walks. ds(u) and dt(v) are each shortest, but the two shortest paths may share nodes, and nothing excludes cycles.
- **Problems:**
  - The corridor keeps edges that no simple source-to-target path within the bound uses. An exhaustive search over each corridor in the replay finds 31 such edges of 370 (8.4 %) in the reference run, and 54 of 2138 (2.5 %) in the full run. They are:
    - self-loops: 5 in the reference run and 11 in the full run, for example MDM2 (`hsa4193`) → itself [activation];
    - edges into the source: 1 and 5, for example AREG (`hsa374`) → EGFR;
    - edges out of the target: 1 and 0;
    - the rest, whose only bounded walks repeat a node.
  - pathz3 requires every present edge to increase distance (S4.03), so none of these edges can be part of a solution path. Whether they still cost time or constrain the solve is stage 5's question.
  - The GUI's script comment, "Retain only those nodes that appear in some path of length <= $REACH_PATH_BOUND" (`gui4.java:140`), overstates what is kept.
- **Proposed call:** keep. The rule never drops an edge that a bounded simple path uses, and exact simple-path pruning is exponential in general. If stage 5 shows that off-path edges matter, self-loops and edges into a source or out of a target are the cheap cases to trim.
- **Decision:** deferred until stage 5 settles S5.13. If off-path edges still count as relaxed, trim self-loops and edges into a source or out of a target; if not, keep the walk rule as it is. (prompt log entry 060) *Settled by S5.13 (prompt log entry 068):* off-path edges keep counting as relaxed, so the corridor trims self-loops and edges into a source or out of a target.

### S4.12 · Complex members are copied with their complex

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9267-9281` (`duplicate_node`, group branch), reached from `duplicate_edge` at `9404-9422`
- **What it does:**
  - Copying a node of type `group` also copies every component node, recursively, and records the new component ids on the complex.
  - Members come along whether or not they lie on a bounded walk. Component edges are never traversable (S4.04), so a member that is not kept in its own right has no edges in the corridor.
  - Members' ids are registered in the corridor's id lookup like any other node's (S4.15).
  - Reference corridor: 3 of its 115 nodes are such isolated members: DOCK2 and the ELMO1/ELMO2 node, both from [DOCK2+ELMO1], and TIRAP.
  - Full corridor: 41 of 592, for example RXRA, TAB2, RELA, the integrins ITGAL, ITGB1, ITGB2, ITGB3, ITGA4 and ITGAV, TSC1, RPTOR and MLST8.
  - These are the isolated nodes that `sz` reports right after `fb_rch`: `Number of nodes: 115 (104 genes, 9 complexes, 2 compounds, 0 others) 3 isolated` (reference run, `run_console.log:250`) and `… 41 isolated` (full run, `run.log:70`).
  - The forward graph picks up 22 and 78 members the same way (S4.08).
- **Problems:**
  - pathz3 resolves expression data through the corridor's lookup (S4.16). An up- or down-regulated member that lies on no path therefore still counts as an observation to be explained. None of the 3 or the 41 is in its run's `up_reg` or `down_reg` file, so no measured result is affected.
  - Excluded nodes re-enter the corridor this way (S4.06).
  - Each member also gets solver variables (stage 5).
  - pathz3's undirected copy (`Linux/src/main.cc:4588`) does read a corridor complex's member list: it goes through `duplicate_edge` and `duplicate_node`, which read and copy the members (`Linux/src/GraphManagerNew.cc:9202`, `9267-9281`). That is why pathz3's undirected graph 5 has 592 nodes in S4.14's sum. The other readers are graph building and merging, the XML writer (`write_node_to_xml_file`, `2892-2899`), the display-side `print_all_graph_info` (`3402`), and the mincut code, which uses residual-graph component lists as scratch storage (for example `Initialize_preflow`, `27459-27472`).
- **Proposed call:** fix. Keep each corridor complex's member list, but add a member as a corridor node only if it is kept in its own right.
- **Decision:** fix, as proposed (prompt log entry 060)

### S4.13 · Copying attributes and merging duplicate edges

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9388-9503` (`duplicate_edge`), `9183-9284` (`duplicate_node`), `676-694` (`check_if_edge_already_created`). Both copies use the default `duplicate_check_flag = true` (`Linux/include/GraphManagerNew.h:597-598`).
- **What it does:**
  - **Nodes** are matched by rep id. A node not yet in the new graph is created with the old node's:
    - type and rep id;
    - `all_id` list, each id registered in the new graph's lookup, where the first claim wins (S4.15);
    - display names and description;
    - `other_value1`, set to its node id in the graph it was copied from.

    Complexes bring their members with them (S4.12).
  - **Edges** are merged when the new graph already has an edge with the same source, target, type and exactly the same ordered subtype list, or that list plus `dummy_u_to_d`. The merge appends any missing pathway ids and keeps the larger float weight (`experiment_score`). The integer weight and the weight tuple stay those of the first copy.

    Otherwise a new edge is created with the type, the ordered subtypes, the pathway list, the integer and float weights and the weight tuple.
  - In the full run the 4126 selected forward edges become 3535, so 591 were merged. Its input graph holds 768 exact duplicate relations, because tool-written reads keep duplicates (S2.12). The reference graph has none.
- **Problems:**
  - The corridor is the first place where duplicates disappear. `sz` edge counts before and after `fb_rch` are therefore not comparable, and a merged edge's pathway list is a union. If stage 2 removes duplicates at read time (S2.12), this merge never fires.
  - Dead work: `duplicate_node` appends each display name cut at its last comma (`9241-9250`), then overwrites the list with the uncut names (`9251`). A corridor node's `other_value1` points into the discarded forward graph, and nothing reads it.
  - The duplicate test scans the source's out-list and copies subtype lists along the way (`676` carries the comment `HIGHLY INEFFICIENT CODE`).
- **Proposed call:** keep. Port the attribute copy and the merge rule as described.
- **Decision:** keep (prompt log entry 062)

### S4.14 · Node and edge ids change with every copy

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:51-52` (counters), `67-70` (`NodeNew` takes `++node_id_counter`), `263-265` (`EdgeNew`), `9229`, `9467`
- **What it does:**
  - Node and edge ids come from two session-wide counters, so every copy gives every node and edge a new id. Across graphs, a node is identified only by its rep id.
  - `fb_rch` copies twice, into the forward graph and the corridor. The batches run it twice (S4.20), with pathz3's undirected copy in between.
  - Reference run: EGFR is node 37592 in graph 84 (`run/fb_rch`, first entry) and node 39128 in its corridor (`run/temp_w_solutions.txt`).
  - Full run: EGFR is 3273 in graph 2, 7596 in the phase-1 corridor (graph 4) and 10044 in the phase-2 corridor (graph 7).
  - Copying is deterministic, so within that run every node shifts by the same 2448: 592 (graph 4) + 592 (pathz3's undirected graph 5) + 1264 (forward graph 6). Edge ids shift as well; EGFR → STAT5A is edge 18462 in phase 1.
  - Corridor nodes are created in the order of the kept edges, by ascending forward-graph edge id, with each source before its target and members straight after their complex. pathz3 lists nodes by ascending id, so this is also the order of `temp_*_solutions.txt`.
- **Problems:**
  - An id from one graph means nothing in another, yet the outputs mix them. `fb_rch` prints input-graph ids, `fb_rch_back` mixes rep ids with input-graph ids (S4.19), and pathz3 prints corridor ids, which differ between the two solves.
  - Any comparison between the two solves has to go through rep ids. Stage 6 should check pex and psp for this.
  - The only link back, `other_value1`, points to the discarded forward graph (S4.13).
- **Proposed call:** fix. Outputs name nodes and edges by stable keys: the rep id for a node, and source, target and subtypes for an edge. Any internal id that does appear means the same node or edge in every step of a run.
- **Decision:** fix, as proposed (prompt log entry 062)

### S4.15 · The corridor rebuilds the id lookup

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9244-9248` (`add_id_rep_id` for each copied id), `909-912` (insert-only). pathz3's readers are `18624-18650` (`read_pairs_file`), `18652-18686` (`read_data_file_into_map`) and `18688-18708` (`read_data_file_into_set_of_ints`), called at `Linux/src/main.cc:4583`, `4634` and `4660-4684`.
- **What it does:** The corridor's id lookup holds exactly the `all_id` lists of the nodes copied into it, registered in copy order, with the first claim winning. pathz3 resolves its connect pairs and every node file against this lookup, not against the input graph's. The node files are the up, down, essential, avoid, active, inactive, confirmed, relaxed and non-relaxed lists.
- **Problems:**
  - An id held by several nodes can bind to a different node in the corridor than in the graph `fb_rch` read. That happens when the input graph's first claimer is outside the corridor, or when the copy order differs.
  - Replaying rgx's claim order on the full run (relation order, components first; `Linux/src/XMLParser.cc:667-683`, `1083-1087`) predicts that 57 of the 1399 ids the corridor resolves change node: 50 because the first claimer is outside the corridor, and 7 because of copy order. Claiming ids in entry-file order instead gives 58 (50 + 8).
  - Example: ATF2 (`hsa1386`) has its own node in the master graph (entry 174), but that node is not in the corridor. In the corridor, `hsa1386` resolves to the 10-gene CREB3-family node (entry 432, key `hsa10488`). Naming ATF2 as the FSC, or as an essential or avoided node, would then force or avoid the whole family node.
  - Raw-KGML merges put every id in one node (S2.03), and the reference run has no such id.
  - An id registered in the input lookup but missing from its node's `all_id` (S2.08) does not resolve in the corridor at all.
- **Proposed call:** fix. Resolve every id once per run, against one lookup, and carry that binding into the corridor.
- **Decision:** fix, as proposed (prompt log entry 062)

### S4.16 · The corridor decides which expression changes pathz3 sees

- **FuSION 1:** pathz3's readers as in S4.15; `Linux/src/main.cc:4693-4714` (the `Up: … Down: …` line and the relaxation-bound clamp)
- **What it does:**
  - pathz3 drops every up- or down-regulated id that does not resolve in the corridor. Genes off the corridor are neither explained nor relaxed.
  - pathz3 then lowers its node-relaxation bounds to the number of genes left.
  - Reference run: of 45 changed genes (43 up, 2 down), 3 remain: `Up: 3 Down: 0`, then `Adjusting node relaxn bound to be consistent with max diff expr genes in reachable graph (3)` (`run_console.log:281-282`). The requested bound of 8 becomes 3.
  - Full run: 21 remain (`run.log:101`). The replay predicts both counts.
  - The bound and the pathway list therefore change the observations, not only the search space, as `RUNNING.md:125-127` warns.
  - Both solves use the same corridor, but the without-FSC solve also drops the FSC from the up set when the FSC is itself up-regulated, as JUN is (S4.06): `Up: 3` and then `Up: 2` in the reference run.
- **Problems:** No output lists the expression changes the corridor left out, so a user cannot tell "explained" from "never considered".
- **Proposed call:** fix. Keep the rule, and write out the up- and down-regulated genes that fall outside the corridor, together with the corridor's parameters.
- **Decision:** fix as proposed; these are S3.12's 'outside the corridor' genes (prompt log entry 062)

### S4.17 · Console output

- **FuSION 1:** `Linux/src/main.cc:2210`, `2226`; `Linux/src/GraphManagerNew.cc:17933`, `17953`, `17957`, `17989`, `18007`, `18156`
- **What it does:** A successful call prints to stdout. Reference run, `run_console.log:240-247`:

  ```
  Bound: 10
  0 out of 11 edges to target excluded due to choice of activation/expression edges to target.
  fwd nodes 354
  eids 2529
  edges 895
  Forward reachable graph id: 85 has 376 nodes 895 edges 
  New graph 86 has 115 nodes 370 edges
  Reachability Graph: 86
  ```

  - `fwd nodes` is |F| (S4.07), and `eids` is the input graph's edge count.
  - `edges` is the number of edges selected for the forward graph, before merging.
  - The forward-graph line counts nodes after members are added and edges after merging (S4.08).
  - The last two lines give the corridor.
  - Warnings on stderr are in S4.02 and S4.09, and failures in S4.18.
- **Problems:**
  - M in the second line counts edges into a target examined from any node within the bound, including edges whose target ends up beyond it. It also counts twice for a source listed twice (S4.02).
  - Three different node counts are printed with no label saying what each counts.
- **Proposed call:** fix. Print the same counts, each labelled with what it counts: the forward set, the forward graph including complex members, and the corridor.
- **Decision:** fix as proposed; the forward-graph count goes with S4.08 (prompt log entry 063)

### S4.18 · Failures, and what graph `0` names afterwards

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17763-17773`, `17933-17938`, `18024-18050`; `Linux/src/main.cc:2224-2231`
- **What it does:** In each case below, `bounded_forward_backward_reach` returns -1, `main` prints `Error: reachability didn't work` (stderr), and the batch carries on (S1.28).
  1. There is no graph with that id (`Error: there is no graph for id <gid>`), or b < 0. Nothing is created, so `0` still names the input graph.
  2. The flag dropped every examined edge into a target (M = N). This includes M = 0: no source or no target resolved, or no node within distance b has a traversable edge into a target. stderr gets `All edges to target excluded due to choice of activation/expression edges to target.  Cannot reach target.` The empty forward graph is already registered, so `0` names an empty graph.
  3. No target is in the forward graph, for example because it is first reached at distance b + 1, or because every target is excluded. stdout gets `None of the targets reachable from source. Exiting.`, although nothing exits. Both graphs are registered and the corridor is empty, so `0` names an empty graph.

  Two more cases return success with an empty corridor (`New graph <id> has 0 nodes 0 edges`):
  - b = 0 when a target is already in the forward graph, for example when a source is also a target, because no edge can satisfy ds(u) + 1 + dt(v) ≤ 0. With source and target distinct, b = 0 fails instead: no target enters F, so the call fails as case 2 or case 3. In the replay, EGFR → CXCL8 fails as case 2 and EGFR → GRB2 as case 3.
  - A target that is in the forward graph only as a copied complex member (S4.12). It has no in-edges there, so nothing is kept. This case is traced from the code and has not been seen in a run.

  No logged run hits any of these cases.
- **Problems:**
  - Case 2 blames the edge-type choice even when the flag dropped nothing.
  - After a failure, the batch's next `pathz3 0` runs on the input graph (case 1) or on an empty graph (cases 2 and 3, and the two empty successes), because nothing checks the result.
  - "Exiting." is false.
- **Proposed call:** fix. A failure leaves no graph behind and stops the run (S1.28's decision). The message names the real cause: no source resolved, the target is not reachable within the bound, or every edge into the target was dropped by the flag. An empty corridor counts as a failure.
- **Decision:** fix, except that an empty corridor is a reported result ("no path within b"), not a failure: its solve is skipped, so one small bound doesn't halt a bound sweep ([decision 0018](../decisions/0018-batch-interface.md)). Real failures leave no graph behind, stop the run (S1.28) and name their cause. (prompt log entry 063)

### S4.19 · Trace files `fb_rch` and `fb_rch_back`

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:17858`, `17867-17868`, `17925`, `18054`, `18061-18062`, `18095`
- **What it does:** Two files with fixed names are written in the launch directory (S1.26) and truncated by every call. `upfx`'s prefix does not apply to them (S2.29).
  - `fb_rch` has one entry per dequeued forward node, each starting with a newline, so the file begins with an empty line and has no final newline. An entry is `<k>: <input-graph node id>: `, followed by `<node id>(<rep id>): ` for each node it queued. Lines 2 and 3 of the reference run's file:
    - `0: 37592: 37705(hsa110117499): 37590(hsa2885): 37704(hsa25759): `
    - `1: 37705: 37666(hsa10000): 37710(hsa3702): 37714(hsa57580): …`
  - `fb_rch_back` has the same layout for the backward search, except that the dequeued node is given by rep id and the queued nodes by input-graph node id. Its first entry: `0: hsa3576 37765(c_s_hsa4790_c_e): 38185(hsa3663): 37661(hsa2353): …`
  - The reference run's files have 354 and 171 entries.
  - 84 batch files move both files to `$WRITE_DIR` after their last call, so a two-call batch loses the first call's files. The two `auxilliary/comparison_results/` batches never move them.
  - A call that fails before the backward search leaves the previous call's `fb_rch_back` in place.
  - Nothing reads either file.
- **Problems:** These are debug traces written on every run, with mixed kinds of ids (S4.14) and contents that are overwritten or stale as described above.
- **Proposed call:** fix. Write them only under the debug option (S1.29), into the run's output directory (S1.26), naming nodes by rep id.
- **Decision:** fix, as proposed (prompt log entry 063)

### S4.20 · The corridor is computed twice per batch

- **FuSION 1:** batch pattern `fb_rch 0 …` before phase 1 and `fb_rch $MERGED_GRAPH_NUM …` before phase 2 (for example the reference batch); GUI writer `Linux/frontend/src/gui4.java:4352-4354`, `4406-4415`; pathz3's undirected copy `Linux/src/main.cc:4588`, `Linux/src/GraphManagerNew.cc:8449-8464`
- **What it does:**
  - 84 batch files, and the GUI writer, call `fb_rch` again before phase 2 with the same arguments on the saved merged-graph id. The GUI's script comment says "Reconstructing the same graph as was done earlier.", but its second call takes its inputs from a separate block (`gui4.java:4407`, `4513-4542`), so there the arguments match only if the user enters them the same way twice.
  - In all 75 logs with two calls when this stage was drafted, and all 83 when it was verified, both corridors have the same node and edge counts.
  - The second call is needed only because pathz3 registers an undirected copy of the corridor, so `0` no longer names the corridor.
  - The fresh copy also gives the second solve a graph that pathz3 has not written to. pathz3 stores an adjacency matrix (`fill_adj_matrix_for_graph`, `8378-8398`) and insert-only sequence maps (`preprocess_graph_for_mincut`, `2468-2518`) in the graph object.
- **Problems:**
  - Avoidable cost: a second forward graph and corridor (1264 + 592 nodes in the full run) and a second set of ids, which causes the shift between phases in S4.14.
  - Whether pathz3 could safely run twice on one corridor is stage 5's question. Some mincut helpers also keep function-static state, such as `static vector<vector<bool>> edge_exist(N + 1, …)` (`GraphManagerNew.cc:12053`).
- **Proposed call:** fix. Compute the corridor once and let both solves use it; decision 0019 also needs the corridor to be saveable.
- **Decision:** fix as proposed; 2.0's solve must leave the corridor unchanged (prompt log entry 063)

### S4.21 · What `fb_rch` hands to pathz3, and what it does not

- **FuSION 1:** `Linux/src/main.cc:4388-4391` (graph `0`), `4583` (connect pairs), `4488` (pathz3's own `path_bound`); `Linux/src/GraphManagerNew.cc:18624-18650`
- **What it does:**
  - **Handed over:** the corridor graph object, reachable as graph `0` right after the call or by its printed id. That includes its nodes and edges with their attributes (S4.13), its id lookup (S4.15) and its creation order (S4.14).
  - **Not handed over:** the bound, the sources and targets, the flag, the exclusion list, the traversable-edge rule and the distances ds and dt, which are discarded on return.
  - pathz3 takes its own path bound and reads its endpoints from the connect-pairs file. A pair whose source or target does not resolve in the corridor is skipped silently, and pathz3 prints `<n> connect pairs`.
  - The batches keep the two in step by hand: they pass the same `$REACH_PATH_BOUND` to both commands and write `connect_pairs` from the same `$SRC_NODE` and `$TGT_NODE`.
  - No batch writes the corridor to disk; only the trace files of S4.19 are kept.
- **Problems:**
  - Nothing checks that pathz3's bound and endpoints match the corridor's.
  - If pathz3's bound is larger, the corridor silently caps it. If its pair names nodes outside the corridor, the pair is dropped.
  - Decision 0019 requires a run to stop after the corridor step or start from a saved corridor, but FuSION 1's corridor exists only in memory.
- **Proposed call:** fix. The corridor records its parameters (sources, targets, bound, flag, exclusions and non-traversable subtypes), can be saved and loaded, and pathz3 checks its own bound and pairs against them.
- **Decision:** fix, as proposed (prompt log entry 064)

### S4.22 · Compile-time flags in this stage

- **FuSION 1:** `Linux/Makefile:10`; `Linux/src/GraphManagerNew.cc:17883`, `17968`, `18076`, `18141` (`ONLY_INTERAC_EDGES_FOR_MINCUT`), `9273-9277`, `9408-9421` (`DUP_CHECK_FLAG_SET`), `9434-9441` (`DUMMY_EDGE_FLAG`)
- **What it does:** S1.29's decision asks each stage to confirm what kind of flag each of its flags is.
  - `ONLY_INTERAC_EDGES_FOR_MINCUT` changes results (S4.04).
  - `DUP_CHECK_FLAG_SET` only decides whether `duplicate_edge` passes its `duplicate_check_flag` on to `duplicate_node`. `fb_rch` calls `duplicate_edge` with the default `true`, so both branches behave the same.
  - `DUMMY_EDGE_FLAG` adds the "same subtypes plus `dummy_u_to_d`" merge test. That never fires in `fb_rch`, because no traversable edge carries the tag: 0 of the reference graph's 640 tagged edges are traversable, and the full graph has no tagged edges.
  - `NDEBUG` is not defined, so the `assert(nid > 0)` checks in `fb_rch` (`17789`, `17804`, `17819`, `17853`, `17923`, `18083`, `18093`, `18110`, `18131`) are active. They cannot fire on a consistent graph.
- **Problems:** None beyond S4.04.
- **Proposed call:** drop the two no-effect flags for this stage. `DUP_CHECK_FLAG_SET` and `DUMMY_EDGE_FLAG` do not change `fb_rch`'s output, so the corridor copy needs neither branch. `ONLY_INTERAC_EDGES_FOR_MINCUT` becomes the run setting of S4.04. Stage 5 confirms the two flags for its own copies.
- **Decision:** fix, as proposed (prompt log entry 064)

## Open questions

1. **Does off-path content in the corridor change pathz3's results, or only its cost (S4.11, S4.12)?** This covers isolated complex members, walk-only edges and self-loops. *Would settle it:* stage 5's reading of whether constraints are posted on nodes and edges that no chosen path uses, and whether an up- or down-regulated node with no edges must be relaxed. A phase-3 reference test would also settle it: run pathz3 on the full run's corridor with the 41 isolated members and 54 walk-only edges removed, and compare the `PO.dat` files.
   *Resolved by stage 5:* off-path edges change results, because every considered edge between present nodes counts whether or not it is selected (S5.13), so S4.11 trims self-loops and edges into a source or out of a target. Isolated complex members stay outside the encoding unless named essential, so they never enter the objective and count only in the Up and Down totals and the node-bound clamp (S5.10).
2. **Can pathz3 run twice on one corridor object (S4.20)?** pathz3 writes the adjacency matrix and insert-only sequence maps into the graph object, and some mincut helpers keep function-static state sized on the first call. *Would settle it:* stage 5, or a reference run that calls `pathz3` twice on one corridor id and compares the result with the current two-`fb_rch` pattern.
   *Resolved:* stage 5's draft says no, because of function-static state; S4.20 decided that 2.0 computes the corridor once and that its solve must leave the corridor unchanged.
3. **Is the claim-order count in S4.15 (57 ids) right?** It comes from a replay that follows rgx's claim order (relation order, components first); claiming in entry-file order would give 58. *Would settle it:* a phase-3 test that dumps the corridor's id lookup from the binary, or a run that names ATF2 (`hsa1386`) in an essential file and checks which node is forced.
   *Still open, for phase 3.*
4. **Should KEGG's signed binding relations be traversable, for example in the direction the KGML records only (S4.04)?** *Would settle it:* the user's biological judgement; the replay can measure how the corridor changes.
   *Resolved for now:* S4.04 makes the non-traversable subtypes a run setting, and signed bindings stay non-traversable by default.
5. **Does the corridor change if `fb_rch` runs on the in-memory merged graph instead of the re-read one?** This is stage 2's open question 1, applied here, since the replay used only re-read graphs. *Would settle it:* the phase-3 comparison that open question proposes, run on graph 83 and graph 84 of the reference run.
   *Still open, for phase 3.*
