# 02 · Network model, inputs and id mapping

## Scope

- **Module:** `network` (Choice C1). It owns:
  - the graph model: nodes with member ids, family boxes and complexes; edges with type, ordered subtypes, pathways, score and flags; stable keys (`NodeKey`, `EdgeKey`) and node kinds; subgraph views; the id lookup;
  - the one polarity rule of S6.14 (`effect_of`, `is_relaxed`), which 06 encodes and 07 checks and draws;
  - the readers: KGML (S2.05, S2.06), FuSION's own network file (S2.12, S2.25), the legacy read of FuSION 1-written files (S2.27), and edge tables, which include `add`'s plain edge list (S2.30);
  - the plug-and-play network input of [decision 0020](../decisions/0020-plug-and-play-inputs.md): format detection, the exchange format, input lists and the KGML directory (S8.31);
  - the id rule and the id map (S2.14, S1.16); the symbol and pathway-name maps (S1.18, S1.19, S2.15); KEGG pathway names read from each KGML's root title; the `Maps` that `start` sets;
  - the writer (`wgx`, S2.25, and every saved network, S2.26), `add` (S2.30), `sz` (S2.28) and the query-only `labels` command;
  - the relation filters for indirect effects, ECrel and maplink (S2.11, S2.31).
- **Spec items implemented here:** S2.01 to S2.12, S2.14, S2.15, the tag of S2.21, S2.23, S2.25 to S2.31; the readers of S1.16, S1.18 and S1.19; S4.14 and S4.15 (stable keys, one lookup); the keys of S5.27; S6.14's rule.
- **Left to other sections:**
  - 01 Batch: command syntax and the command table, the graph store with `0` and `#LASTGRAPHNUM` (S2.01, S1.09), `load`, the `run` line, the query mode, printing to the console, and reusing files the run wrote (01.C13).
  - 03 Merge: the union of boxes that share an id (S2.17), also for a single KGML read (03.C5); the exception set (S1.17, S2.13); split, reuse and the shared store (S2.22); node-type conflicts (S2.19); edge merging across pathways (S2.20); the merge report (S2.24); `mff` itself (S2.16), with its rule for the legacy master's list (03.C9).
  - 04 Expression: the value reader, which applies this section's id rule and `IdMap`.
  - 05 Corridor: the corridor, built as a view on a network (02.C9).
  - 07 Results: how keys, labels and flags appear in results.
  - 08 Enrichment: GSEA, which takes each candidate's gene set from this section's readers.
  - 09 Runs: core's vocabulary, ids and stable orderings (cross-section item 2A.5), the run directory and its input copies, run.json, the events envelope, `OutputFile` and the output-clash check, and the settings registry.

Evidence used below:
- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/` (42 pathways, raw KGML merge). "The full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/` (old master, legacy read). "The GUI run" is `runs/egf_hela_hsa1956_cxcl8_b6/` (old master at split threshold 15000, S8.06).
- "The replay" is a set of Python scripts in this section's scratch folder (`graph_stats.py`, `build2.py`, `legacy_replay.py`, `legacy_roundtrip.py`, `bind_census.py`). They parse the real files; FuSION 1 was not run. `build2.py` replays the 2.0 build this section and 03 describe: relation filters at their defaults, the exception split, the union of boxes, complexes, component edges carrying their document's pathway, and canonical order. The legacy replay reproduces FuSION 1's printed counts exactly (below), which checks it.
- "The 273 KGMLs" are all of `FuSION/updated_pathways/xml_files/` (every KEGG human pathway, 14.6 MB). "The 167 KGMLs" are the list in `auxilliary/list_of_kegg_xml_files.txt` (6.9 MB).

## Requirements carried in

- **Skeleton.**
  - B1: the library reports through the logger and event interface and never writes to the console.
  - C1: `network` holds the model, the KGML, merged-graph, legacy and edge-list readers, the writer, the plug-and-play network input and id mapping. C1 gives `core` the ids and the stable orderings; 09 owns them and adopts this section's outline (2A.5, prompt log entry 158).
  - D1: steps pass typed values in memory; graphs keep integer ids with `0` for the latest; every step's output can be saved and loaded.
  - E1: run.json records every setting with its value and source, and each input's path, size and SHA-256; structured results are JSON and tables TSV.
  - A2: progress and warnings go to `events.jsonl`.
  - G1: C++20, CMake, pugixml for XML. G3: Linux, macOS and Windows, all native.
- **Decisions.**
  - [0020](../decisions/0020-plug-and-play-inputs.md): the network may come from any pathway source, plug and play.
  - [0021](../decisions/0021-required-and-optional-inputs.md): inputs are checked before the run; a named file that cannot be read stops the run.
  - [0019](../decisions/0019-partial-runs.md): a run can stop after, or start from, any saved step.
  - [0017](../decisions/0017-batch-file-compatibility.md), [0018](../decisions/0018-batch-interface.md) and [0023](../decisions/0023-results-may-change-where-improved.md): FuSION 1 batch lines still run, with names as well as positions; results may change where 2.0 improves.
  - [0027](../decisions/0027-graph-display-in-phase-5.md): results must be in a form the Phase 5 display can use.
- **Graph store and records.**
  - S2.01: integer graph ids, `0` for the latest; merge intermediates freed; no genesis records.
  - S2.02: the node record is kept, including the unfilled description, compartment and pathways; 02.C16 amends it for three fields (entry 163).
  - S2.04: the edge record is kept, with the exact ordered-subtype identity and every member initialised; 02.C14 drops the fields nothing reads (entry 162).
- **Lookup.**
  - S2.03: keep the id-to-node lookup, but report the node's members whenever an id belongs to several nodes or to a node with other members.
  - S4.02, S4.15: resolve each id once per run, against one lookup, report what it bound to, and carry that binding into the corridor.
  - S3.12: the lookup is exact; unmatched genes are listed.
- **Reading KGML.**
  - S2.05: rgx keeps its arguments and console lines; a bad file or argument is a command error; the reader is KGML-only and other sources plug in through Phase 2's mechanism.
  - S2.06: the same KGML subset is read; dangling references, repeated entry ids and cyclic groups are rejected with a message.
  - S2.07: exception ids first, then the rest of a box collapses above the split threshold, default 1. 03.C5 amends it for single-file reads, which become one-file merges (entry 166).
  - S2.08: every id of a box joins the node the box resolves to.
  - S2.09: complexes, with two-way component edges carrying the file's own pathway id.
  - S2.10: subtype normalisation, fan-out, undirected pairs, self-loops kept; a skipped duplicate's score still counts (the larger is kept); the score is 0 when absent or malformed.
  - S2.11: indirect effects are a recorded setting, applied before nodes are made: discard (default) or keep as "1+" hops.
  - S2.13: the FSC being tested joins the exception set automatically, with S1.17's up and down genes, source and target. 03 assembles the set.
  - S2.31: ECrel edges are a setting, kept by default, and outputs mark them.
- **Formats.**
  - S2.12: FuSION's own format is recognised by a reader, not by trusting a flag; old batches may still pass `y`.
  - S2.25: the merged-graph format is kept, with escaped values, display names from the symbol map, and a mark saying it is FuSION's format and not KGML.
  - S2.26: later stages run on the in-memory graph; the written file is a saved step.
  - S2.27: a compatibility reader reads the old master the way FuSION 1 does (first-claim lookups, no component edges, one-way associations), kept apart from the raw pipeline and flagged as legacy; 02.C5 widens its scope (entry 159).
  - S2.30: `add` keeps its command, checks types against KEGG's spelling and rejects malformed lines; a plain edge list is a candidate exchange format; 02.C17 amends "keeps its output format" (entry 163).
  - S2.21: subtype lists are compared without `dummy_u_to_d`, and the tag is computed once on the merged graph. S5.13: the solver never reads the tag.
- **Maps.**
  - S2.14: both FuSION 1 uses of the inter-database map are dropped; id mapping is designed here and may reuse S1.16's two-column file.
  - S1.16: the two-column reader is kept as it is: the first two whitespace-separated tokens of each line, a later line winning, extra tokens ignored. A blank line maps `""` to `""`, and a line with one token maps its id to `""` (`Linux/src/GraphManagerNew.cc:8014-8015`).
  - S1.18, S1.19: one reader for both label maps: the name is the rest of the line after the first tab, further tabs become `_`, blank lines are skipped, a later line wins, and an id with no name falls back to itself.
  - S2.15: lookups never insert.
  - S1.15: `start` loads the four files, and a named `start` file that cannot be read stops the run before it starts. Cross-section item 2B.4 amends it: `start` is optional (entry 163).
  - S8.13, S8.31: the reference maps ship with their source and date; no KGMLs ship; a KGML list is resolved against the user's own KGML directory.
- **Keys, copies and the rule.**
  - S4.14, S5.27: outputs name nodes and edges by stable keys, and any internal id means the same node or edge in every step of a run. 02.C11 settles the edge key (entry 159).
  - S4.13: the corridor copy's duplicate-merge rule, which 02.C5 moves to reading (entry 159).
  - S4.20, S5.29: the corridor is computed once, the solve leaves it unchanged, and nothing survives between solves.
  - S5.06: edge-list files are matched as subtype sets.
  - S5.13, S6.14: one polarity rule, shared by the solver and the renderer.
- **Architecture polls** ([decision 0031](../decisions/0031-polling-the-architecture-sections.md); prompt log entries 158 to 210), by cross-section item or choice:
  - 1.1: core's vocabulary is 09's: `core::Failure` (with `UsageError` and `InstallError`), `core::Report` and `core::Outcome<T>`, `core::Log` and `core::Events`, `core::Context` for every step, a 32-byte `core::Sha256`, one `core::stable_compare` (entry 158).
  - 1.3: events have dotted names, the key `type`, and 09's envelope; warnings go only through `warning` events with a dotted code (entry 190).
  - 1.16 and 2B.2: a setting's registry key is `<module>.<name>`, its argument the bare name; the `run` line takes run-level settings by name, `fusion --set` overrides them, and `--commands` exports them (entry 190).
  - 1.17: generic conditions use 09's and 01's codes (`input.*`, `io.write`, `artefact.*`, `output.clash`, `batch.*`, `deps.missing`); a module keeps codes for its own faults; one `network.flag_mismatch` (entry 158).
  - 1.20: tables write numbers with 09's `format_number`, in Python's repr layout (entry 180).
  - 1.22: one run-level `bindings.tsv`, written by this section's binding table, with 06's `line` and `note` columns (entry 158).
  - 1.24 with 03.C6: two files with one pathway id in a list fail, with one code, `list.duplicate_pathway`, for `gsea` and `mff` (entry 166).
  - 1.25: an `unscored` edge flag; the score stays 0 for comparisons (entry 158).
  - 2A.4: `load` restores a saved network exactly; `rgx` compares and rebuilds; `mff` has no `from_saved` (entry 164).
  - 2A.6 and 2A.7: every write goes through 09's `OutputFile`, and 09's check finds output clashes by file identity (entry 158).
  - 2B.1: this section adds `effect_of`, `is_relaxed`, `NodeKey` and `NodeKind`; 2B.6: a query-only `labels` command; 2B.10: a list a step wrote records the base its lines resolve against; 2B.11, 2B.12, 2B.14 and 2B.18: the builder calls and header fields 03 needs, each list line's text with its path, `Maps`, and `normalise_subtype` (entry 158).
  - 2B.3: a relative or omitted output path lands in the step's directory; an absolute one is honoured as well (entry 169).
  - 2B.5: the shipped KGML list `data/kegg_pathways.txt` holds FuSION 1's 167 candidates as bare pathway ids (entry 204).
  - 3A.8: `fusion --query` reads one batch line on standard input and answers in JSON, with no run directory (entry 168).
  - 3A.19 with 04.C2: one decimal grammar, parsed with fast_float, for values, thresholds, real arguments and edge scores (entry 167).
  - 4A.5: the full pre-run parse is shared between `gsea` and `mff` by neither, as S7.19 decided (entry 158).
  - 01.C13: a file the run wrote, unchanged since (identity and SHA-256), is read back from memory (entry 189).
  - 05.C3: `fb_rch`'s ids are checked against the entry ids of the network inputs before the first step, from this section's pre-run parse (entry 170).
  - 07.C10: a node's label shows its family size (entry 180).
  - 08.C3 and 08.C4: a GSEA candidate is one network file in any format this section reads; its relation share counts relations kept under the run's filters (entry 181).
  - 09.C3 with 1.13: inputs are copied into the run and hashed first; the check parses the copy, and the step uses that parse (entry 184). 09.C4: every step saves its output (entry 184). 09.C14: failures are `core::Failure` exceptions, reported results `core::Outcome<T>`, a stop `core::Stopped` (entry 186).
  - Section 11's open question 4: the shipped maps are rebuilt from clear sources (NCBI Gene symbols; Reactome names with attribution) and recorded in `data/SOURCES.md`; KEGG pathway names are not shipped but read at run time from each KGML root's title (entry 205).
  - This section's open questions 1, 6 and 8: the legacy reader keeps S2.08's loss (entry 163); KGMLs are documentation only, and the check names every listed pathway missing from the folder (entry 164); legacy reads follow S2.13 when the FSC is known (entry 164).
- **Other.** S1.26 (outputs in the run's directory), S1.27 (numbers printed exactly), S1.28 (a failure stops the batch with a non-zero exit), S1.29 (result-changing flags become recorded run settings), S2.23 (dead work dropped), S2.28 (`sz` also counts multi-gene nodes and names the largest), S2.29 (`upfx` dropped).

## Design

### Components

- **core's ids** (09 owns them, 2A.5): `core::Interner` interns every id, key, type, subtype and pathway id, so the model holds 32-bit handles, `core::GeneId` for entity ids and node keys and `core::Sym` for other strings. Handles follow first appearance, so nothing is ever ordered by them. Every order compares the strings, with `core::stable_compare` (byte order).
- **`IdMap`** applies the one id rule (02.C3) to every id any input names.
- **`LabelMap`** holds the symbol map or the pathway-name map (one reader). **`Maps`** bundles the id map and the two label maps that `start` sets, or the shipped defaults (2B.4, 2B.14).
- **Readers**, chosen by `sniff` (02.C1):
  - `read_kgml` and `read_edge_table` produce a `PathwayDoc`: the file's entries and relations as read, before any build rule;
  - `read_fusion_network` and `read_legacy` produce a `Network` directly.
- **`NetworkBuilder`** turns nodes and relations into a `Network` in normal form. 03's merge drives it, for one file (`rgx` on a KGML or an edge table, 03.C5) or many (`mff`).
- **`Network`** is immutable once built and shared as `std::shared_ptr<const Network>`.
- **`GraphView`** is a whole network or a subset of it with the network's own indices (02.C9). The corridor is one.
- **`IdIndex`** (per network) and **`BindingTable`** (per run) implement the lookup and its report.
- **The polarity rule**: `effect_of` and `is_relaxed` (2B.1, S6.14).
- **Writer**, **`add`**, **`summarize`** (for `sz`) and **`labels_answer`** (for the `labels` query).

### The graph model

- **Node.**
  - `key`: the stable key (S4.14), a `NodeKey`. A gene or compound node's key is its smallest member in byte order (S2.07), for example `hsa11221` for the DUSP box. A complex's key is `c_s_` + its distinct component keys, sorted and joined by `_`, + `_c_e` (S2.09), for example `c_s_hsa4149_hsa4609_c_e`.
  - `type`: the source's entry type, verbatim (`gene`, `compound`, `group`, …), and its `NodeKind`: `gene`, `complex` (type `group`), `compound`, or `other` for any other type (2B.1).
  - `members`: the canonical ids it holds, sorted and unique. A complex's only member is its key.
  - `components`: a complex's component nodes, in key order.
  - flags: `complex`, `exception` (a node an exception id made, S2.13), `family` (two or more members).
  - S2.02's `description`, `compartment` and `pathways` are kept as an optional side table, empty unless a reader fills them. `node_id` becomes the stable index, and `inlist` and `outlist` the adjacency arrays: the same information, stored differently. `display_names`, `other_value1` and `excess` are not stored (02.C16).
- **Edge.**
  - `src`, `tgt`: node indices.
  - `type`: verbatim (`PPrel`, `GErel`, `PCrel`, `ECrel`, or another source's term).
  - `subtypes`: the ordered, normalised list (S2.04, S2.10), without `dummy_u_to_d`, which is a flag (S2.21).
  - `pathways`: sorted, unique pathway ids.
  - `score`: `experiment_score` as a `double`, 0 when the edge is unscored. A double is used so that a non-integer score from another source compares with its threshold exactly.
  - flags: `dummy_u_to_d`, `component`, `undirected` (both edges of an association pair), `ecrel`, `indirect`, `self_loop`, `legacy`, and `unscored` (1.25): no relation behind the edge carries a usable score. The score then stays 0 for every comparison (S2.10), and 06 can count such edges apart in S5.16's report.
- **Identity.** An edge is identified by (source key, target key, type, ordered subtypes), with the tag left out (S2.04, S2.21). A network never holds two edges with the same identity; adding one merges pathways, keeps the larger score, and clears `unscored` when either side is scored (S2.10, S2.20).
- **Normal form.** Every `Network`, whatever its reader, has:
  - node keys as above, members sorted, pathway sets sorted;
  - unique edge identities;
  - nodes and edges in canonical order (02.C10).

  A network that the builder makes from documents (KGML, edge tables, 03's merges), and a 2.0 file saved from one, also has:
  - each canonical id in exactly one node, which 03's union guarantees (stage 2 open question 4);
  - undirected relations as two-way pairs, with `dummy_u_to_d` set once: on the direction whose source key is larger in byte order (S2.10), and on both directions of a component pair (S2.09);
  - component edges for every complex.

  This is the normalisation that FuSION 1's `wgx` → `rgx` round trip performed (S2.26). In 2.0 it holds by construction, so no re-read is needed.

  Two kinds of network keep FuSION 1's shape instead. A legacy network (S2.27) can hold an id in several nodes, and has only the reverse edges, component edges and tags that its file already held as relations: none in the old master, all of them in a FuSION 1 `wgx` output. An `add` transcription (S2.30) has one-way associations and no tags.
- **Provenance** (optional). When 03 builds with provenance on, the network keeps the `PathwayDoc`s it came from, the (document, relation) pairs behind each edge, and the (document, group entry) pairs behind each complex (2B.11). The merge report (S2.24) and the embedded sources of 02.C4 use it.
- **Metadata** (`NetworkInfo`): the reader used; each source's path (left out in store entries, 03.C4), size, SHA-256, pathway id and KGML title; the build settings; the exception set it was built with; the id map's digest; the legacy flag; and, for a merge, 03's merge header (base-merge key, base mark, rules version). 09 copies these into run.json.

### The polarity rule (S6.14)

- **`effect_of(subtypes)`**: `inhibition` when the list holds `inhibition` or `repression`; `activation` otherwise, including an edge with no subtype, `component`, `association`, `dummy_u_to_d` and `indirect` alone (S5.13).
- **`is_relaxed(effect, source, target)`**, with each end `absent`, `inactive` or `active`:
  - an edge with an absent end is never relaxed;
  - an activation edge between present ends is relaxed unless both ends are active;
  - an inhibition edge between present ends is relaxed when both ends are active or both inactive.

  This is FuSION 1's rule (`Linux/src/GraphManagerNew.cc:28526-28599`, as S5.13 describes it). The full table has 3 × 3 × 2 rows. Leaving self-loops out of the count (S5.13) is the solve's step, not this function's.
- 06 encodes the rule, and 07 checks every witness against it and draws by it, so the solver and the renderer cannot disagree (S6.26).

### Memory layout

02.C8 proposes interned struct-of-arrays storage with compressed adjacency:
- an edge record of 32 bytes: `src`, `tgt`, `type`, subtype-list id, pathway-set id (4 bytes each), `score` (8), flags (2), padding (2);
- a node record of 28 bytes: `key`, `type`, a member range, a component range, flags;
- out- and in-adjacency as offset arrays plus edge-index arrays, 8 bytes per edge and 8 per node;
- subtype lists and pathway sets interned once each.

Measured sizes (counts from the files; the 2.0-rule rows from the replay):

| Graph | Nodes | Edges | Distinct subtype lists | Distinct pathway sets |
|---|---|---|---|---|
| 42-pathway merge, FuSION 1 (reference run's `run/merged_master_graph.xml`) | 1,156 | 2,529 | 30 | 215 |
| 42 KGMLs, 2.0 rules, no exceptions | 1,106 | 2,291 | 25 | 207 |
| 42 KGMLs, 2.0 rules, the run's 49 exception ids (the reference run's network) | 1,129 | 2,509 | 25 | 214 |
| 167 KGMLs, 2.0 rules | 2,297 | 6,759 | 38 | 836 |
| 273 KGMLs, 2.0 rules | 2,955 | 10,187 | 42 | 1,174 |
| Full run, legacy read (graph 2, `run.log:56-57`) | 3,100 | 7,400, of which 6,632 distinct | 34 | 724 |
| GUI run, legacy read at threshold 15000 (`mylog.txt:3-4`) | 8,139 | 82,188, of which 71,620 distinct | 34 | 724 |
| TNFα run (`real_experiments/hsa7124_to_hsa4792_…_b10/merged_master_graph.xml`) | 8,237 | 85,550 | 44 | 744 |
| 273 KGMLs, threshold 15000 (no box collapses) | 10,028 | 149,039 | 42 | 1,788 |

The 2.0 rows use the defaults: indirect effects discarded, ECrel kept, maplink skipped. Keeping maplink would add 75 `map` nodes to the 273-KGML network (02.C7).

- The "about 8,000 nodes and 82,000 edges" case is the GUI's default split threshold of 15000 (S8.06) applied to the old master: no box collapses, so each relation fans out over every member pair, and groups become Cartesian products (3,179 complexes). The replay reproduces it exactly: 8,139 nodes (4,710 genes, 3,179 complexes, 250 compounds) and 82,188 edges.
- The largest plausible network is all 273 KGMLs with no collapse: about 10,000 nodes and 149,000 edges. In this layout that is about 4.8 MB of edge records, 1.3 MB of adjacency and well under 1 MB of nodes, members, strings and indexes, so about 7 MB in all. Building it also holds a transient hash of edge identities, about 24 bytes per edge (3.6 MB).
- The reference run's network fits in about 0.2 MB.
- For comparison, FuSION 1's `EdgeNew` declares 2 strings, 4 vectors and 12 scalars (`Linux/include/GraphManagerNew.h:127-194`): about 216 bytes with libstdc++, plus heap blocks for its subtype, pathway and weight-tuple lists and an `unordered_map` node. That is roughly 400 bytes per edge, or about 33 MB per copy of the GUI run's graph, and FuSION 1 keeps several copies (S2.01, S4.14).
- Memory is not the constraint at these sizes. What the layout buys is no per-step copies, no linear scans (S2.04, S2.22), cache-friendly loops for 05 and 06, and 32-bit indices that scale linearly to networks far larger than KEGG.

### Stable keys and order

- **Node key:** as above. It is stable within a run and identical in every step after the network is built (S4.14).
- **Edge key** (02.C11): the identity fields, printed as `src|tgt|type|subtype,subtype`, for example `hsa5906|hsa369|PPrel|activation` (the RAP1 box → the RAF box, from `path:hsa04010` and `path:hsa04722`) or `hsa4149|c_s_hsa4149_hsa4609_c_e|PPrel|component`. JSON outputs also give the four fields apart, and indices appear only beside keys. The type is needed. Without it, 3 edges of the reference run's network collide (2,509 → 2,506 distinct identities in the 2.0 build; 2,523 → 2,520 in FuSION 1's merge), and 35 of the full run's graph (6,632 → 6,597).
- **Order** (02.C10): nodes are sorted by key, and edges by (source index, target index, type, subtype list). Keys, types and subtypes are compared as strings in byte order, never as interned handles. `EdgeKey`'s own ordering gives the same order. The same inputs in any list order therefore give the same network (`content_hash`) and, since the file header also lists its inputs in canonical order, byte-identical files. That checks S2.17's order independence directly.
- **Indices:** `NodeIdx` and `EdgeIdx` are positions in that order. Every view of a network uses the network's indices (02.C9). So within a run, from the split network onwards, node 531 (GRB2 in the reference run's network) is the same node in the network, the corridor, both solves and every output.

### Views

- A `GraphView` is a network plus an optional pair of bitsets, one over its nodes and one over its edges.
- The corridor (05.C1) is a view. It costs N + E bits, about 20 KB for the largest network above, and never copies a node or an edge. So:
  - it cannot drift from the network;
  - the id lookup is the network's (S4.15);
  - the solve cannot write into it (S4.20, S5.29), because `Network` is `const`.
- Iterating a view's out-edges of v walks the network's out-edges of v and tests a bit: O(deg(v)).
- Because a network's edge identities are unique, S4.13's duplicate merge, which 02.C5 moves to reading, never has to run at the corridor.

### Ids, maps and labels

- **The id rule** (02.C3). Every id that any input names passes through `IdMap::canonical` once, at read time. That covers KGML entry names, edge-table cells, values (04), node and edge list files and coexpression ids (06), the not-merged file (03), and source, target and FSC arguments (05, 06). The rule:
  1. delete every `:`, the convention FuSION 1 already applies to KGML names (`Linux/src/XMLParser.cc:885`) and to coexpression ids (`Linux/src/GraphManagerNew.cc:28650`), but not to LFC files or node lists (S3.03);
  2. if a map applies and the id is in its left column, replace it by the right column, once, with no chaining.

  Ids are otherwise case-sensitive and matched exactly (S3.12). Pathway ids (`path:hsa04010`) are labels of edges, not entity ids, and keep their colon.
- **The id map** is one `IdMap` type with one file layout, S1.16's `<foreign id> <network id>`, read by S1.16's reader exactly: the first two whitespace-separated tokens of each line, a later line winning, extra tokens ignored.
  - A blank line maps `""` to `""`, which no id ever matches. A line with one token maps its id to `""`, so that id then matches no node. Both are FuSION 1's behaviour (`Linux/src/GraphManagerNew.cc:8014-8015`). 2.0 only adds a warning for each one-token line, which changes no result.
  - The id rule's colon deletion applies to both columns, as to every id, so `hsa:3725` works on either side. The reader also warns about left ids that repeat with a different right id.
  - **Which map applies.** The run-wide map is `start`'s first argument, or `id_map=` by name, and applies to every input. A value input's own `id_map=` (04) names either an S1.16 file or the keyword `symbols`, the symbol map read backwards, and replaces the run-wide map for that input (cross-section item 1.12). Without `start` there is no run-wide map (2B.4).
  - **Ids a map lacks** (prompt log entry 229). The run-wide map passes an id it lacks through unchanged, as a synonym map should, so a network or list id missing from it keeps its own spelling. A value input's own map drops the ids it lacks, because that input is keyed by foreign ids, and 04 reports them; with pass-through, 04's probe table would bring 9,989 unmapped symbols into GSEA's ranking and the threshold statistics. `IdMap::lookup` tells the two cases apart: it returns the mapped id, or nothing when the map lacks the id.
  - Each map's SHA-256 is recorded (E1). All 51 copies of `inter_db_mapping_file` under FuSION 1's tree are empty (S2.14 counted 17 when it was written), so old batches map nothing and behave as before.
- **Reverse symbols.** `id_map=symbols` builds an `IdMap` from the symbol map read backwards (symbol → id), for value files keyed by gene symbols. In FuSION 1's 6,334-line map only 8 of 6,326 symbols name two ids (for example `ICOS`: `hsa29851` and `hsa102723996`). Those are reported and resolved by the later-line rule.
- **Maps.** `start` sets the id map, the symbol map and the pathway-name map. `start` is optional (2B.4): without it, or for a file it leaves out, the shipped maps apply, and run.json records each map by hash. The shipped maps (section 11's `data/`) are rebuilt from sources with clear terms and recorded in `data/SOURCES.md`: the symbol map from NCBI Gene, keyed by canonical ids such as `hsa3725`, and the pathway-name map with Reactome's names and Reactome's attribution. They hold no KEGG pathway names (section 11's open question 4, entry 205).
- **Label maps.** The symbol map (S1.18) and the pathway-name map (S1.19) share one reader, with the decided rule: the name is the rest of the line after the first tab, further tabs become `_`, blank lines are skipped and a later line wins. Lookups never insert (S2.15). A missing name falls back to the id.
  - No comment syntax is added. A line that carries a shipped map's source and date (S8.31), such as `#source<TAB>NCBI Gene gene_info, 2026-11-01`, needs none: its key `#source` matches no id, so it is inert.
- **Pathway names** (the rule of entry 205, read as accepted in entry 230). A pathway read from a KGML takes its name from that KGML's root `title`: `path:hsa04010` is `MAPK signaling pathway`, as `hsa04010.xml` says. Each of the 167 and the 273 KGMLs carries a `title`. Every other pathway id (Reactome ids, the legacy master's ids, an edge table's pathways) takes the pathway-name map's entry, and an id with neither prints as itself (S1.19). The titles travel with the network: each source in `NetworkInfo` and in a saved file's header records its title.
- **Node labels** (07.C10, entry 180). Labels are derived when something is printed, from the symbol map, by S2.15's and S2.25's rules. The network does not store them (02.C16), so a new symbol map never changes a network's content hash.
  - A single-member node shows its key's symbol, or the key itself: `EGFR`.
  - A family node shows its representative's symbol, the key's, plus the number of other members: `RRAS2+5` for the 6-gene RAS box, `MAP2K1+1` for MEK1/2. FuSION 1 printed the RAS box as `RRAS2` and a 36-gene receptor box as `TFG`.
  - A complex shows its key's symbol when the map has one, else `[` + its components' labels joined by `+` + `]`, a box among them as symbol plus count: `[MAX+MYC]`, or `[STAT1+6]` for a complex of the 7-gene STAT box (accepted in prompt log entry 230).
  - Reports and hovers list every member's label (S2.03, S2.17). In the reference run's merge, 127 of 972 gene keys had no symbol in FuSION 1's map (S2.03), so PI3K printed as `hsa110117499`.

### The id lookup and the family-node trap

- **`IdIndex`** maps each canonical id to the nodes that hold it.
  - In every non-legacy network there is exactly one holder.
  - In a legacy network an id may have several holders, kept in FuSION 1's claim order, with the first claim first (S2.03).

  Built in O(Σ members); a lookup is O(1).
- **`resolve(given)`** applies the id rule and returns a `Binding` with a status:
  - `own`: the id's node holds only that id;
  - `family`: the id's node holds other ids too, for example EGF (`hsa1950`) in the 52-gene ligand node of the reference run;
  - `ambiguous`: legacy only; several nodes hold the id, and the first claim is bound. HRAS (`hsa3265`) binds to the RRAS2-labelled 6-gene node `hsa22800`, which is the RAS trap;
  - `absent`: no node holds the id.
- **`BindingTable`** (one per run) resolves every (role, id) once, against the run's network, and keeps the result. 04, 05 and 06 ask it rather than the index, so an id means one node in every step (S4.15). The caller decides what an absent id means: an unresolvable source or target is a command error (S4.02), while an unmatched up gene is listed (S3.12). 06 adds its edge-line rows through it (1.22).
- **Where the trap still bites.** S2.13 puts the source, target, FSC, up and down genes and the not-merged ids in the exception set, so each of them always has a node of its own. Legacy reads follow S2.13 too when the FSC is known (entry 164). The trap remains for every other named id: excluded nodes (S4.06), the node and edge list files and the coexpression ids (06).
  - Of the 702 distinct ids in the coexpression CSV the runs pass (`hsa_stringdb_coexpression_links_over400.csv`), the full run's legacy graph binds 333 to their own node, 247 to a family node and 109 ambiguously, and 13 are absent.
  - The 2.0 build of the reference run's 42 KGMLs, with its exception set, binds 115 to their own node and 250 to a family node, and 337 are absent. FuSION 1's graph 84 of the same run gives 114, 253 and 335.

  So about a third land in a family node. `bindings.tsv` (02.C13) and one summary event per role make this visible.

### Reading network inputs

#### Format detection

`sniff(path, flag)` reads at most the first 64 KiB.
- **XML** (the first non-blank byte is `<`), by the root `<pathway>` element:
  - root attribute `format="fusion-network"`: FuSION's own file, at the version in `version`;
  - else, root has an `org` attribute (KGML requires `org` and `number`; FuSION 1's tool-written files have neither): KGML;
  - else: a legacy file that FuSION 1 wrote. This covers the old master, FuSION 1's `wgx` outputs and FuSION 1's `add` output.
  - Any other root element is an error naming the file.
- **Text:** blank lines and lines starting with `#` are skipped first. If the first remaining line's tab-separated fields include `source` and `target`, the file is a headered edge table. If it has exactly four whitespace-separated fields, it is `add`'s form. Anything else is an error.
- An explicit `format=` argument overrides detection.
- FuSION 1's `y|n` flag is accepted and compared with the content. When they disagree, `sniff` raises the one warning `network.flag_mismatch`, naming the reader it used (S2.12, 1.17). For example, `y` on a raw KGML would, in FuSION 1, have given edges with no pathway. The 110 `rgx … y` lines and 64 `mff … y` lines under `runs/`, and one of each under `macOS/model_example/`, name tool-written files wherever the file still exists, so none of them warns.

#### KGML

- **Parse.** pugixml loads the file; the DOM is dropped after conversion.
  - From the root: `name`, the pathway id, and `title`, the pathway's name (entry 205). A root without a `name` takes the file stem as its id (2B.12); none of the 273 KGMLs lacks one.
  - From each `<entry>`: `id`, `name` with `:` deleted and split on runs of spaces, so two spaces no longer make an empty id (S2.06), `type`, and the `<component id>` children.
  - From each `<relation>`: `entry1`, `entry2`, `type`, each `<subtype name>` normalised by `normalise_subtype` (S2.10's table, 2B.18), and `experiment_score`.
  - **Scores** follow the one decimal grammar of 04.C2, parsed with fast_float (3A.19, entry 167): a score counts only when its whole text matches `[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?`. An absent score, or one that does not match, leaves the relation unscored: its score is 0 for every comparison, as S2.10 decided for absent or malformed scores, and its edge carries `unscored` (1.25). A score that does not match also raises the warning `network.bad_score`, and the read goes on (accepted in prompt log entry 230). FuSION 1 read `625x` as 625 by its numeric prefix (`Linux/src/XMLParser.cc:1519-1522`); 2.0 reads it as unscored.
  - `<graphics>`, coordinates, links and `<reaction>` are ignored. Node labels come from the symbol map (S2.25).
- **Checks** (S2.06), each a failure naming the file and the element:
  - a relation or component naming a missing entry;
  - a repeated entry id;
  - a group with no components;
  - a cyclic group, found by a three-colour depth-first search in O(entries + components).

  pugixml never loads DTDs or external entities, so a downloaded KGML cannot make FuSION read another file.
- **Result.** A `PathwayDoc` holding every entry and every relation, with ids canonical. Its `gene_ids()` are every `type="gene"` entry's ids, which are the file's GSEA gene set (S7.08, 08.C3); 05.C3 checks `fb_rch`'s ids against every entry's ids.
- **Cost.** O(file bytes). Measured with Python's expat-based parser, an upper bound for C++: 0.24 s for the 167 KGMLs and 0.51 s for the 273. The whole 2.0 build in the replay (parse, union, fan-out) takes 0.33 s and 0.64 s. FuSION 1's `mff` took 122–126 s for 163 pathways (S2.22).

#### Relation filters

Applied when a document is built, before any node is made. An entry becomes a node only if a kept relation reaches it, directly or as a component of a reached group (S2.06). The filters:
- **`network.indirect`** (decided by S2.11: a recorded setting, applied before nodes are made).
  - `discard` (the decided default) drops every relation whose subtypes include `indirect`. The real run's 42 KGMLs lose 204 of 2,763 relations; the 167 lose 683; the 273 lose 1,062.
  - `keep` keeps them. Their edges carry the `indirect` subtype and flag, 05 counts each as one hop, and 07 prints a path that uses one as `e+` (roadmap objective 2's "1+").
- **`network.ecrel`** (decided by S2.31: a setting, kept by default, with ECrel edges marked in the outputs). `keep` or `exclude`. ECrel edges carry the `ecrel` flag for the marking. `exclude` acts at the same point as the indirect filter (02.C6). The real 42 KGMLs have 31 ECrel relations after the indirect discard.
- **`network.maplink`.** `skip` (default, 02.C7) or `keep`.

The parsed `PathwayDoc` keeps every relation, so one parse serves every setting. `kept_relations(doc, filters)` is also what 08's relation share counts (08.C4).

#### Building

03's merge turns documents into a network; 03 owns the union. A single KGML or edge table read by `rgx`, and a one-file `mff`, run the same merge (03.C5, which amends S2.07 for single-file reads; entry 166). This section gives the merge the rules and the builder:
- **`split_entry(entry, E, thres)`** applies S2.07 to one entry. Ids in the exception set become single nodes. The rest form one box when the entry holds more than `thres` ids, counting exception ids; otherwise each is single. `thres < 0` never collapses, and `0` behaves like 1, as in FuSION 1. It lives in `network`, because both 03's union and this section's legacy reader use it, and `merge` depends on `network`, not the reverse.
- **03** unions the boxes that share an id, within and across documents. This also gives S2.08's fix by construction: a box whose smallest id already names a node adds all its ids to that node.
- **Complexes:** one per combination in the Cartesian product of the components' node sets (S2.09), keyed and labelled as above.
- **Edges:** each kept relation fans out over the product of its two entries' node sets. `add_edge` merges an edge whose identity already exists, keeping the larger score and the union of pathways (S2.10, S2.20).
  - A relation whose subtypes include `association`, `dissociation` or `mapping` also adds the reverse edge, and both get the `undirected` flag (S2.10).
  - A relation between two entries with the same key makes a self-loop, flagged (S2.10; 05 trims it, S5.13 leaves it out of the count).
- **Component edges:** two PPrel edges per (complex, component), subtypes `component`, the `dummy_u_to_d` flag on both, and the document's own pathway id (S2.09).
- **Copies for 03's split** (2B.11): `copy_node` copies a node from another network with its members, and `copy_edge` an edge with its flags, pathways, score and provenance, so that the split can copy the untouched part of a base merge.
- **`finish()`** sets the tag once (S2.21), sorts nodes and edges (02.C10), builds the adjacency and the id index, and drops the identity hash.
- **Cost.** Fan-out O(Σ over relations of |N(e1)| × |N(e2)|) with O(1) expected identity tests. That is 2,509 edges for the reference run's network, and 149,039 for the 273 KGMLs with no collapse. Sorting is O(N log N + E log E). Memory as in the layout above.

#### Edge tables: the exchange format and `add`

The exchange format (02.C2) is a UTF-8, tab-separated table with a header. Any other pathway source plugs in by producing it.

```
# SIGNOR export converted for FuSION, 2026-11-12
source	target	type	subtypes	pathways	score	source_type	target_type
hsa1956	hsa2885	PPrel	activation,phosphorylation	signor:SIGNOR-EGFR	932
hsa5906,hsa5908	hsa673	PPrel	activation	path:hsa04010	625
hsa4149+hsa4609	hsa1029	GErel	expression	path:hsa05200	0
cpdC00076	hsa5530	PCrel	activation	path:hsa04020	0	compound	gene
cpdC00752+hsa7056+hsa10544	hsa5624	PCrel	activation	path:hsa04610	0	compound+gene+gene	gene
```

The first rows are illustrative. The last transcribes relation 682 → 410 of `hsa04610.xml`: a complex of a compound with THBD and PROCR, activating PROC.

- **Columns.** `source` and `target` are required; the others may be left out, in any order. Trailing empty cells may be omitted.
- **Defaults:** type `PPrel`, no subtypes, the file's stem as pathway id, unscored, node types `gene`. An unknown column name is an error, so a typo cannot be ignored silently.
- **Endpoint cells** use KGML's entry structure inline:
  - `,` joins the ids of one box, like a multi-gene KGML entry;
  - `+` joins the components of a complex, each a single id or a box.

  Ids pass the id rule. No id in the 273 KGMLs contains either character, and neither do Entrez, Ensembl or UniProt ids.
- **`source_type` and `target_type`** give node types. One value applies to every id in the cell. A complex cell may instead give one type per component, joined by `+` in the same order, as in the last row. This is needed: 7 of the 334 complexes built from the 273 KGMLs mix compound and gene components, for example `c_s_cpdC00752_hsa10544_hsa7056_c_e`, the first node of the reference run's network.
- **`subtypes`** is an ordered, comma-joined list, normalised as in KGML. **`pathways`** is comma-joined. **`score`** follows the same grammar as a KGML score; an empty cell is unscored.
- **Each row is one relation**, and builds exactly as a KGML relation would: boxes collapse above the split threshold, exception ids split out, associations become two-way pairs, complexes get component edges, and the relation filters apply.
- **Gene set.** The table's `gene_ids()` are every endpoint id of type `gene`: the file's GSEA gene set (08.C3).
- **`add`'s form** (S2.30): headerless lines of exactly four whitespace-separated fields, `source target type subtype`. It is read by the same reader.
  - Blank lines and `#` lines are skipped.
  - A line with any other number of fields is rejected with its line number. FuSION 1 skipped short lines silently, and its standalone tool duplicated the previous edge.
  - Types are checked against KEGG's spelling: `PPrel`, `GErel`, `PCrel`, `ECrel`, `maplink`. A case variant such as `PPRel` is an error that names the right spelling. Any other type, and any subtype outside KGML's vocabulary plus `component`, is a warning.
- **The `add` command** keeps its form: `add <edges_file> <out_xml>`. It transcribes the rows: one `gene` entry per id, labelled with the id, one relation per row, `pathways="KEGG"`, no score. It applies no build rule, so an association row stays one-way, as FuSION 1's `add` output read with `y` did. It writes FuSION's mark (02.C17), so 2.0 reads its output as its own file, not as legacy.

#### FuSION network files

- **The reader** loads the file as saved: entries as nodes and relations as edges, through the builder, with no box or relation rule. Embedded sources become the network's `Provenance` documents, and each `<input>`'s title is kept.
- **`load`** (01.C6) restores the network exactly as saved, as decision 0019's start from a saved step (2A.4).
- **`rgx`** on such a file compares the recorded build settings, exception set and id-map digest with the run's (`saved_network_diff`, 2A.4):
  - **equal:** the network is used as saved. This is the fast path for every saved step and for the 110 old `rgx … y` lines under `runs/` that re-read the file `wgx` just wrote. A file this run wrote, unchanged since (identity and SHA-256), is not even parsed: 01.C13 hands back the in-memory network (S2.26);
  - **different settings or exceptions, sources embedded (02.C4):** 03's split rebuilds the network from the embedded documents with the run's values, for example when a new FSC sits inside a saved family node (S2.13, S2.22);
  - **different, no embedded sources, or a different id map:** a failure naming each difference (`network.settings_mismatch`). The embedded documents hold ids already mapped, so they cannot be re-mapped.
- **In an `mff` list** (03.C7), a network file that embeds its sources contributes its documents, as if its original files were listed. 03 decides when a file without sources may join.
- **Store entries** (03.C4) are the same format with 03's `<merge>` header element (base-merge key, `base="true"`, rules version), an empty exception set and no input paths.
- A file that records no build settings, such as `add`'s output, is used as saved.

#### The legacy reader

02.C5 sets its scope and method: every unmarked file that FuSION 1 wrote is read so as to reproduce what FuSION 1's later stages saw, FuSION 1's graph 2.
1. **Read 1.** FuSION 1's tool-written rules (S2.12): relations in file order, `entry1`'s nodes, then `entry2`'s, then the fan-out.
   - Fan-out follows FuSION 1's node-creation order (`std::set<int>` of node ids, `Linux/src/XMLParser.cc:700-701`).
   - S2.07 is applied without union: the first box to name a key creates its node, and later boxes with that key add nothing. That is S2.08's loss, which legacy reads keep (entry 163).
   - Ids are registered by first claim.
   - There is no duplicate test, no reverse edge, no tag and no component edge.
   - Each edge's pathways come from the `pathways` attribute; `dummy_u_to_d` subtypes in FuSION 1-written files become the flag.
2. **Replay of `wgx` → `rgx y`, in memory.**
   - Indirect relations are dropped (their entries were nodes in read 1).
   - Nodes reached by no kept relation, directly or as a component of a kept complex, are dropped.
   - Ids are re-claimed in written-relation order: each edge's source node, then its target node, components before their complex.
3. **Exact duplicate edges are merged** with S4.13's rule (pathways unioned, larger score), so identities stay unique. FuSION 1 merged them at the corridor copy, so the corridor and the solve see the same edges.

The exception set is the run's, which follows S2.13: a known FSC gets its own node in legacy reads too (entry 164). A FuSION 1 batch that names no FSC keeps its own `start` exception file, and 01's check says so (2A.9).

Checked against FuSION 1's own logs:
- **Full run, threshold 1:** 3,100 nodes (2,516 genes, 380 complexes, 204 compounds), 198 isolated, 7,400 edges. That is graph 2 in `run.log:56-57`, with members and bindings identical to the replayed re-read of `run/merged_master_graph.xml` for every id. After step 3, 6,632 edges remain.
- **GUI run, threshold 15000:** 8,139 nodes (4,710 genes, 3,179 complexes, 250 compounds) and 82,188 edges, as in its `mylog.txt:3-4`. After step 3, 71,620 edges remain.
- **A single pass without step 2** differs from graph 2 for 194 ids: 125 that graph 2 lacks, 2 that only graph 2 has, and 67 bound to other nodes. It also differs in the members of 8 nodes.

Beside the legacy master, an `mff` list may hold edge tables and `add`'s marked output. Their ids bind to the master's nodes by first claim (S2.03), with no union; 03.C9 owns the rule (entry 163). The network gets `legacy=true`, and every saved file, event and run.json entry derived from it says so (S2.27).

#### Input lists and the KGML directory

`read_input_list(list, base)` serves `mff` and `gsea` (02.C12):
- One entry per line. Surrounding whitespace and `\r` are trimmed, and blank lines are skipped. FuSION 1 aborted on a blank line or CRLF (S2.16).
- **Base.** An absolute path is used as is. A relative path resolves against the base: `kgml_dir` when one is given; else, for a list that a step wrote (`gsea`'s shortlist), the base that step recorded in run.json (2B.10); else the list file's own folder.
- A bare pathway id (`hsa04010` or `path:hsa04010`: no folder, no extension) becomes `<base>/hsa04010.xml`. The shipped list `data/kegg_pathways.txt` names FuSION 1's 167 candidates this way and resolves against each user's own KGML folder (S8.31, 2B.5).
- Each line comes back as its trimmed text and its resolved path (2B.12), so `gsea` can repeat the text in its shortlist (S7.15).
- **Checks before the run** (S2.16, 0021). Every listed file must exist. The check names every missing one in one failure (`input.missing`), not only the first, so a user missing KGMLs sees the whole list (this section's open question 6, entry 164). Two entries naming the same file (`std::filesystem::equivalent`, which also catches case-only differences on macOS and Windows) are read once, with a warning. Two files declaring the same pathway id fail, naming both: `list.duplicate_pathway`, for `gsea` and `mff` alike (1.24, 03.C6).
- All 167 entries of FuSION 1's list are absolute paths, so old lists resolve as before.

#### Checks before the run

0021 has the backend check every input before the first step.
- 09 first copies each input into the run's `inputs/` and hashes it in the same read. The check then fully parses the copies of the network inputs, and each step uses the parse of its own inputs (09.C3 with 1.13; 02.C15). The recorded SHA-256 is therefore the hash of the bytes parsed.
- `gsea` and `mff` share no parse: each step reads only its own inputs, as S7.19 decided (4A.5). `gsea` parses its candidates; `mff` parses its list.
- 05.C3 checks every `fb_rch` id against the documents' entry ids before the first step, and 03 checks node types early; both use this parse.
- The check-only mode (09.C12, `fusion --check`) parses the originals, without copies, and the frontend validates through it.
- The cost is the parse time above: under a second for every case measured.
- A list that `gsea` writes during the run cannot be checked up front. Its candidate list is checked instead, and each shortlisted path is one of those already parsed by the check of `gsea`'s inputs.

### Writing: `wgx`, saved networks and `add`

`wgx <graph|0> <file> <tag>` keeps its form (S2.25) and writes FuSION's network format, version 1 (02.C4). The same writer saves every step's network (09.C4): `rgx` saves `rgx/network.xml`, `load`'s restored network needs no copy, and 03's `mff` saves `mff/merged_graph.xml`.
- The writer serialises to a stream; the step writes it through 09's `OutputFile` (a temporary name, then a rename, with 09's Windows retry rule; 2A.6).
- Output paths follow 09's rule (2B.3): a relative or omitted path lands in the step's directory; an absolute path is honoured as well, with the file also kept in the step's directory and both recorded in run.json. 09's check refuses an output that clashes with an input or another output, by file identity (2A.7).

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pathway name="KEGG" format="fusion-network" version="1">
  <fusion writer="FuSION 2.0.0" reader="kgml" legacy="false">
    <merge key="b41c6f0e…" base="false" rules="1"/>
    <setting name="network.split_threshold" value="1"/>
    <setting name="network.indirect" value="discard"/>
    <setting name="network.ecrel" value="keep"/>
    <setting name="network.maplink" value="skip"/>
    <input path="/home/u/kegg/hsa04010.xml" sha256="ce0b9f2863d1749510a1e97ab804fe50b0380391c3d1f93cbcba1bd44e53a6d3" pathway="path:hsa04010" title="MAPK signaling pathway"/>
    <exceptions ids="hsa1026 hsa1390 hsa1432 hsa1843 hsa1846 hsa1847 hsa1848 hsa1956 hsa3576 hsa3725 hsa5594 hsa5599"/>
  </fusion>
  <entry id="75" key="c_s_hsa4149_hsa4609_c_e" name="c_s_hsa4149_hsa4609_c_e" type="group">
    <graphics name="[MAX+MYC]"/>
    <component id="697"/>
    <component id="723"/>
  </entry>
  <entry id="294" key="hsa11221" name="hsa11221 hsa1844 hsa1849 hsa1850 hsa1852 hsa80824" type="gene">
    <graphics name="DUSP10, DUSP2, DUSP7, DUSP8, DUSP9, DUSP16"/>
  </entry>
  <entry id="396" key="hsa1843" name="hsa1843" type="gene">
    <graphics name="DUSP1"/>
  </entry>
  <relation entry1="531" entry2="345" type="PPrel" pathways="path:hsa04010 path:hsa04722" experiment_score="625">
    <subtype name="association"/>
    <subtype name="dummy_u_to_d"/>
  </relation>
  <source pathway="path:hsa04010" title="MAPK signaling pathway" sha256="ce0b9f28…">
    <e id="21" type="gene" ids="hsa11221 hsa1843 hsa1844 hsa1846 hsa1847 hsa1848 hsa1849 hsa1850 hsa1852 hsa80824"/>
    <r e1="45" e2="41" type="PPrel" score="625" sub="activation"/>
  </source>
</pathway>
```

The example is shortened from the replay of the reference run's network (1,129 entries, 2,509 relations). `exceptions` shows 12 of its 49 ids, and the merge key is abbreviated. The entry numbers are the real canonical ones: entry 1 is `c_s_cpdC00752_hsa10544_hsa7056_c_e`, MAX and MYC are 697 and 723, entry 531 is GRB2 (`hsa2885`) and entry 345 the 24-gene receptor node keyed `hsa1436`. The tag sits on this direction because `hsa2885` sorts after `hsa1436`.

- **Entries** are numbered from 1 in canonical order. `name` lists the members sorted, so a re-read keys every node the same (S2.25 found 28 entries that FuSION 1 wrote unsorted). `key` is explicit. `<graphics name>` holds every member's symbol, or the complex label (S2.25).
- **Relations** carry `pathways` (sorted, space-joined) and `experiment_score`. A whole score is written as an integer, FuSION 1's form, and any other as the shortest text that reads back exactly. An unscored edge has no `experiment_score`, which FuSION 1 also reads as 0, so the flag survives a round trip. The tag is written as a `<subtype>` element, as FuSION 1 did, and read back as the flag.
- **Escaping:** attribute values are escaped by pugixml when it saves (S2.25's latent fault).
- **The header** records what 09 needs and what `rgx` compares: settings, inputs with their SHA-256 and title, the exception set, the legacy flag, and 03's `<merge>` element for a merge. `<input>` elements are written in canonical order (pathway id, then SHA-256), as are `<source>` sections, so a file does not depend on list order. Input paths are the paths as named and differ between machines; a store entry leaves them out (03.C4). The network's identity is its `content_hash`, which leaves the header out.
- **`<source>` sections** embed each input's relations and every entry that some relation reaches, directly or as a component (02.C4). An entry that no relation reaches can never become a node under any setting, so these are all a rebuild needs. For the 42 KGMLs they add 348 KB, next to FuSION 1's 540 KB merged file; for the 167 KGMLs, 1.3 MB; for the 273 KGMLs, 2.1 MB. That is about 18 % of the raw KGML bytes.
- **FuSION 1 compatibility.** FuSION 1 reads only the root's `entry` and `relation` children (`Linux/src/XMLParser.cc:635`, `666`) and ignores unknown attributes. Its `rgx … y` can therefore read a 2.0 file, which lets Phase 3 run FuSION 1's later stages on a 2.0 merge.
- **Views.** A view (a corridor) can be written too. Complex components outside the view are written with `in_view="false"`, so the file stays self-contained. 05's saved corridor (`corridor.json`, 05.C6) instead references the network by its `content_hash`.
- **Cost.** O(output bytes): FuSION 1's files of these graphs are 540 KB for the reference run and 13.4 MB for the GUI run.

### `sz`

`summarize(view)` counts nodes by type (`gene`, `group`, `compound`, other), isolated nodes, and edges (S2.28). It also counts gene nodes with two or more members and names the largest. For the reference run's network as the replay builds it, the batch layer prints:

```
Number of nodes: 1129 (952 genes, 93 complexes, 84 compounds, 0 others) 0 isolated
Number of edges: 2509
Multi-gene nodes: 334; largest hsa125467750 (52 genes)
```

FuSION 1 printed 1134 nodes (952 genes, 96 complexes, 86 compounds) and 2529 edges for its graph 84 (`run_console.log:236-237`).

### `labels` (query only)

`labels [map=<file>]` serves the frontend's gene lookup (2B.6, 10.C15). It runs only through `fusion --query`, which reads the line on standard input and writes one JSON answer to standard output, with no run directory and no events (3A.8). Without `map`, it reads the shipped symbol map. The answer lists the map's id-label pairs in byte order of id, with the map's path and SHA-256:

```json
{"map": {"path": "share/fusion/data/symbols.tsv", "sha256": "5b0e…"},
 "labels": [["hsa1", "A1BG"], ["hsa10", "NAT2"], ["hsa100", "ADA"], ["hsa1000", "CDH2"]]}
```

The answer is shortened, and the shipped file's name is section 11's. The `map` argument is not a run setting: a query makes no run and records nothing. It is read with the same reader as every symbol map, so the frontend's suggestions match what the backend binds.

### Settings

All are recorded in run.json with their source (E1). A registry key is `<module>.<name>`, and the argument is the bare name (1.16). The run-level settings are set on the `run` line by key (`run network.indirect=keep`) or by `fusion --set network.indirect=keep`, and `--commands` exports them (2B.2). The values and defaults of `network.indirect` and `network.ecrel` are decided by S2.11 and S2.31.

| Key | Argument | Type | Default | Given by | Changes results | Spec |
|---|---|---|---|---|---|---|
| `network.split_threshold` | `split_threshold` | integer | 1 | `rgx`, `mff` (`<thres>`, positional) | yes | S2.07 |
| `network.tool_written` | `tool_written` | `y` \| `n` | none | `rgx`, `mff` (positional) | no; compared with the content | S2.12 |
| `network.indirect` | run-level | `discard` \| `keep` | `discard` | `run` line, `--set` | yes | S2.11 (decided) |
| `network.ecrel` | run-level | `keep` \| `exclude` | `keep` | `run` line, `--set` | yes | S2.31 (decided), 02.C6 |
| `network.maplink` | run-level | `skip` \| `keep` | `skip` | `run` line, `--set` | yes | 02.C7 |
| `network.id_map` | `id_map` | path | none | `start` argument 1, or by name | yes | S1.16, 02.C3 |
| `network.symbols` | `symbols` | path | the shipped symbol map | `start` argument 3 | labels only | S1.18, S8.13, 2B.4 |
| `network.pathway_names` | `pathway_names` | path | the shipped pathway-name map | `start` argument 4 | labels only | S1.19, S8.13, 2B.4 |
| `network.kgml_dir` | `kgml_dir` | path | none | `mff`, `gsea` | no; resolved paths and hashes are recorded | S8.31, 02.C12 |
| `network.format` | `format` | `auto` \| `kgml` \| `fusion` \| `legacy` \| `edges` | `auto` | `rgx` | only when it overrides detection | 02.C1 |
| `network.embed_sources` | `embed_sources` | boolean | `true` | `wgx` | no | 02.C4 |

### Files written in a run directory

| File | Written by | Format |
|---|---|---|
| `rgx/network.xml` | `rgx`, as its saved step (09.C4) | FuSION network format, version 1 |
| the path given to `wgx`, for example `wgx/merged_master_graph.xml` (and the absolute path as well, if one is given) | `wgx` | FuSION network format, version 1 |
| the path given to `add`, in `add/` | `add` | FuSION network format, version 1, a transcription |
| `bindings.tsv`, at the run directory's top level | the run's `BindingTable`, written once when the run ends, whatever its status, through 09's `Run::create_run_file("bindings.tsv")` (prompt log entries 230 and 244) | TSV with a header (02.C13) |

03's `mff/merged_graph.xml` is written by this section's writer in 03's step.

`bindings.tsv`, from the full run's legacy graph. The bindings are real; the `essential` row is a hypothetical use of HRAS. `line` is the line in the file that named the id (CDKN1A is line 54 of the LFC file, ITGAL first appears on line 139 of the coexpression CSV), and `note` holds 06's remarks, such as an edge line that matched no edge (1.22):

```
role	given	id	status	node	label	size	members	also_in	line	note
source	hsa1956	hsa1956	own	hsa1956	EGFR	1	EGFR			
target	hsa3576	hsa3576	own	hsa3576	CXCL8	1	CXCL8			
fsc	hsa3725	hsa3725	own	hsa3725	JUN	1	JUN			
up	hsa1026	hsa1026	own	hsa1026	CDKN1A	1	CDKN1A		54	
coexpression	hsa:3683	hsa3683	family	hsa3683	ITGAL+3	4	ITGAL,ITGAM,ITGB1,ITGB2		139	
coexpression	hsa:5104	hsa5104	ambiguous	hsa3053	SERPIND1+3	4	SERPIND1,SERPINC1,SERPINA5,hsa5270	hsa5104	44	
coexpression	hsa:54776	hsa54776	absent							878	
essential	hsa3265	hsa3265	ambiguous	hsa22800	RRAS2+5	6	RRAS2,MRAS,HRAS,KRAS,NRAS,RRAS	hsa3265	1	
```

### Events

The events go through 09's `core::Events`, which adds the envelope (`v`, `seq`, `t`, and `step`, `line` and `point` inside a step; 1.3).

```json
{"type":"network.read","path":"inputs/list_of_kegg_xml_files/hsa04010.xml","format":"kgml","pathway":"path:hsa04010","entries":134,"relations":169,"kept":164,"dropped":{"indirect":5,"ecrel":0,"maplink":0},"unscored":0,"ms":2}
{"type":"network.graph","graph":1,"reader":"kgml","legacy":false,"nodes":1129,"genes":952,"complexes":93,"compounds":84,"others":0,"isolated":0,"edges":2509,"multi_gene":334,"largest":{"key":"hsa125467750","size":52}}
{"type":"network.bindings","role":"coexpression","ids":702,"own":115,"family":250,"ambiguous":0,"absent":337}
{"type":"network.written","graph":1,"path":"mff/merged_graph.xml","nodes":1129,"edges":2509,"bytes":901377,"ms":9}
{"type":"warning","code":"network.flag_mismatch","message":"rgx was given y, but inputs/hsa04010.xml is KGML; read as KGML"}
```

The counts are real: `hsa04010.xml`, and the replay of the reference run's network, with its exceptions, and of its coexpression ids. Times and byte counts are illustrative.

### Errors

A failure is a `core::Failure` exception with a stable code (09.C14). The interpreter catches it once per command and stops the batch with a non-zero exit (S1.28). Failures are found before the run whenever the input exists then (0021). This module returns no reported result (`core::Report`); those are an empty corridor, an empty selection and no gene past the thresholds (prompt log entry 167). Readers of long lists check the step's stop token between files and throw `core::Stopped` after a stop request.

Generic conditions take 09's and 01's codes (1.17):

| Code | When |
|---|---|
| `input.missing`, `input.unreadable`, `input.not_file` | a named network input, list entry, map or `add` input cannot be read; a list names every missing entry at once |
| `artefact.changed`, `artefact.newer_format` | a saved network that changed since its run recorded it, or one written by a newer FuSION |
| `io.write` | a write, rename or copy fails |
| `output.clash` | an output that is also an input or another output, by file identity (09's check) |
| `batch.bad_argument`, `batch.bad_value` | an unknown, duplicate or missing argument, a positional after a named one, or a wrong number of positional tokens; a value not of its type (01's binder; S2.05; prompt log entry 230) |
| `batch.*` | a graph id with no graph yet (01's check, S1.15) |

This module's own failures:

| Code | When |
|---|---|
| `network.unknown_format` | detection fails, or `format=` names an unknown reader |
| `network.parse` | malformed XML or TSV, with the line |
| `network.dangling_ref`, `network.duplicate_entry`, `network.empty_group`, `network.cyclic_group` | S2.06's structural faults |
| `network.bad_row` | an edge-table row with the wrong field count, an unknown column, or a type cell that does not match its endpoint's components |
| `network.type_spelling` | `PPRel` and other case variants of KEGG types |
| `network.settings_mismatch` | `rgx` on a network file built under other settings or another id map, which cannot be rebuilt |
| `list.duplicate_pathway` | two files in one list with one pathway id (1.24) |

Warnings never stop a run (`warning` events with a dotted code):
- `network.empty`: a file whose relations are all filtered out, or that has none, gives an empty graph (FuSION 1 printed `<file> has no edges`, `XMLParser.cc:717`);
- `family`, `ambiguous` and `absent` bindings, summarised in `network.bindings` (the caller decides whether an absent id is a failure, as S4.02 does for source and target);
- `network.unknown_type`, `network.unknown_subtype`;
- `network.flag_mismatch` (raised by `sniff`);
- `list.duplicate_entry`: one file listed twice, read once;
- `network.bad_score`: a score that does not match the grammar, read as unscored;
- `network.id_map_line`: an id-map line with one token; `network.id_map_repeat`: a left id with two right ids;
- skipped maplink relations, counted in `network.read`.

### Portability

- **Paths.** Paths are `std::filesystem::path`. Batch text is UTF-8 and is converted through `char8_t` (C++20). On Windows, pugixml is given the wide path, so non-ASCII folders work.
  - List entries may use `/` on every platform, and `\` only on Windows.
  - Duplicate detection uses `std::filesystem::equivalent`, not string comparison, because macOS and Windows file systems are case-insensitive by default.
  - Paths longer than 260 characters on Windows need both the executable's `longPathAware` manifest entry and the system's `LongPathsEnabled` policy (section 11).
- **Line endings and encoding.** Every text reader accepts LF and CRLF, and skips a UTF-8 byte-order mark. Every writer writes UTF-8 without a mark and LF only, in binary mode, so the same content gives the same bytes on all three platforms. A network file's header still records each machine's own input paths.
- **Numbers.** Scores are parsed with fast_float (11.C5), never the locale; `std::from_chars` for doubles is missing from macOS's libc++ at the targeted version. A non-whole score is written as the shortest text that reads back exactly; floating-point `std::to_chars` needs a macOS deployment target of 13.3 or later, which section 11 sets or replaces.
- **No processes, shells, signals or locks.** This module reads and writes files only, and every write goes through 09's `OutputFile`.

### Determinism

- **Order.** Nodes and edges are in canonical order (02.C10), and the file header lists inputs in canonical order, so files, events and every downstream iteration are independent of list order and platform.
- **Comparison.** Orders compare strings, never interned handles, with `core::stable_compare`: unsigned-byte order on every platform.
- **Hashing.** Hash containers use core's fixed hash function, never `std::hash`, whose values differ between libstdc++, libc++ and MSVC. No output or decision ever iterates a hash container.
- **Legacy.** The legacy reader follows FuSION 1's creation and claim order exactly, then sorts.
- **No randomness.**
- **Records.** Reader names, the format version, input hashes, KGML titles and the id map's digest are recorded (E1). `Network::content_hash()` covers the model fields only, not labels, titles or paths, so Phase 3 can compare networks across runs and machines.

### Interface outline

Headers under `network/` in `libfusion`, namespace `fusion::network`. 09 owns `core::Interner`, `core::GeneId`, `core::Sym`, `core::stable_compare` and the fixed hash, adopting this section's earlier outline (2A.5), and the rest of core's vocabulary (1.1): `core::Context` (log, events, stop token, settings, files, cache, step), `core::Failure`, `core::Outcome<T>`, `core::Sha256`, `core::OutputFile` and `core::Json`.

```cpp
namespace fusion::network {

using core::GeneId;                                     // an interned entity id or node key (03, 04 use it)
using core::Sym;                                        // other interned strings: types, subtypes, pathways
using NodeIdx = std::uint32_t;                          // position in canonical node order
using EdgeIdx = std::uint32_t;                          // position in canonical edge order

// ---- keys, kinds and the polarity rule (S4.14, S6.14; items 2B.1, 2B.18) ----
struct NodeKey {                                        // "hsa1956", "c_s_hsa4149_hsa4609_c_e"
  std::string value;
  auto operator<=>(const NodeKey&) const = default;     // byte order
};
struct EdgeKey {                                        // 02.C11: unique within a network
  NodeKey source, target;
  std::string type;                                     // "PPrel", "GErel", "PCrel", "ECrel", ...
  std::vector<std::string> subtypes;                    // ordered, without dummy_u_to_d
  auto operator<=>(const EdgeKey&) const = default;     // byte order; equals the canonical edge order
  std::string text() const;                             // "src|tgt|type|a,b"
  static EdgeKey parse(std::string_view text);
};
enum class NodeKind : std::uint8_t { gene, complex, compound, other };
NodeKind kind_of(std::string_view entry_type) noexcept; // "gene", "group", "compound", anything else

enum class Effect : std::uint8_t { activation, inhibition };
enum class EndState : std::uint8_t { absent, inactive, active };
Effect effect_of(std::span<const std::string> subtypes);              // inhibition iff inhibition or repression
bool is_relaxed(Effect, EndState source, EndState target) noexcept;   // S5.13's 3 x 3 x 2 table
std::string_view normalise_subtype(std::string_view name) noexcept;   // S2.10: "indirect effect" -> "indirect", ...

// ---- maps and labels (02.C3; items 1.12, 2B.4, 2B.14) ----
class LabelMap {                                        // S1.18, S1.19: one reader
 public:
  static LabelMap read(const std::filesystem::path&, core::Context&);
  std::optional<std::string_view> find(std::string_view id) const noexcept;   // never inserts (S2.15)
  std::string_view label_or_id(std::string_view id) const noexcept;
  const core::Sha256& digest() const noexcept;
};
class IdMap {                                           // S1.16's layout: <foreign id> <network id>
 public:
  static IdMap none();
  static IdMap read(const std::filesystem::path& two_column, core::Context&);
  static IdMap reverse_symbols(const LabelMap& symbols, core::Context&);    // id_map=symbols
  GeneId canonical(std::string_view raw, core::Interner&) const;  // ':' removed, mapped once; an id the
                                                                  // map lacks passes through (run-wide map)
  std::optional<GeneId> lookup(std::string_view raw, core::Interner&) const;  // ':' removed; empty when the
                                                                  // map lacks the id (a value input's own map
                                                                  // drops it, and 04 reports it; entry 229)
  const core::Sha256& digest() const noexcept;
};
struct Maps {                                           // what start sets; the shipped files otherwise
  IdMap ids;
  LabelMap symbols, pathway_names;
};
Maps shipped_maps(core::Context&);                      // section 11's data/, no id map

class IdSet;                                            // canonical ids, sorted by content (exceptions)

// ---- model ----
enum class NodeFlag : std::uint16_t { complex = 1, exception = 2, family = 4 };
enum class EdgeFlag : std::uint16_t { dummy_u_to_d = 1, component = 2, undirected = 4, ecrel = 8,
                                      indirect = 16, self_loop = 32, legacy = 64, unscored = 128 };
enum class IndirectMode : std::uint8_t { discard, keep };
enum class EcrelMode    : std::uint8_t { keep, exclude };
enum class MaplinkMode  : std::uint8_t { skip, keep };

struct RelationFilters {                                // S2.11, S2.31, 02.C7; 08's draft calls it EdgeSettings
  IndirectMode indirect = IndirectMode::discard;
  EcrelMode ecrel = EcrelMode::keep;
  MaplinkMode maplink = MaplinkMode::skip;
  auto operator<=>(const RelationFilters&) const = default;
};
struct BuildSettings {
  int split_threshold = 1;                              // S2.07
  RelationFilters filters;
  auto operator<=>(const BuildSettings&) const = default;
};

struct Node { GeneId key; Sym type; std::uint32_t members_first, members_count,
              comps_first, comps_count; std::uint16_t flags; };
struct Edge { NodeIdx src, tgt; Sym type; std::uint32_t subtypes, pathways;
              std::uint16_t flags; double score; };     // score 0 when unscored

enum class Format { kgml, fusion_network, legacy, edge_table };
struct SourceRef {
  std::optional<std::filesystem::path> path;            // as named; left out in store entries (03.C4)
  std::uint64_t size;
  core::Sha256 sha256;
  Format format;
  std::string pathway;                                  // root name, else the file stem (item 2B.12)
  std::string title;                                    // KGML root title, the pathway's name (entry 205)
};
struct MergeHeader { std::string key; bool base = false; int rules = 1; };  // 03's <merge> element
struct NetworkInfo {
  std::string reader;
  std::vector<SourceRef> sources;                       // canonical order
  BuildSettings settings;
  std::vector<std::string> exceptions;                  // sorted by content
  core::Sha256 id_map;
  bool legacy = false;
  std::optional<MergeHeader> merge;
};

class PathwayDoc;
struct Provenance {                                     // optional; filled by 03
  enum class Kind : std::uint8_t { relation, group };   // a relation behind an edge; a group behind a complex
  struct Ref { std::uint32_t doc, item; Kind kind; };
  std::vector<std::shared_ptr<const PathwayDoc>> docs;
  std::vector<std::uint32_t> edge_first, node_first;    // offsets into the two tables
  std::vector<Ref> edge_refs, node_refs;
};

class IdIndex {                                         // hash table with core's fixed hash
 public:
  std::span<const NodeIdx> holders(GeneId) const noexcept;   // O(1); first claim first
};

class Network {                                         // immutable; shared as shared_ptr<const Network>
 public:
  std::span<const Node> nodes() const noexcept;
  std::span<const Edge> edges() const noexcept;
  std::span<const GeneId> members(NodeIdx) const noexcept;
  std::span<const NodeIdx> components(NodeIdx) const noexcept;
  std::span<const EdgeIdx> out_edges(NodeIdx) const noexcept;   // ascending EdgeIdx
  std::span<const EdgeIdx> in_edges(NodeIdx) const noexcept;
  std::span<const Sym> subtypes(EdgeIdx) const noexcept;        // ordered
  std::span<const Sym> pathways(EdgeIdx) const noexcept;        // sorted by content
  bool has_subtype(EdgeIdx, std::string_view) const noexcept;
  NodeKey key(NodeIdx) const;
  EdgeKey key(EdgeIdx) const;
  NodeKind kind(NodeIdx) const noexcept;
  Effect effect(EdgeIdx) const noexcept;
  std::optional<NodeIdx> find_node(const NodeKey&) const noexcept;   // O(1)
  std::optional<EdgeIdx> find_edge(const EdgeKey&) const noexcept;   // binary search in out_edges: O(log deg)
  std::vector<EdgeIdx> find_edges(NodeIdx src, NodeIdx tgt,
                                  std::span<const std::string_view> subtype_set) const;  // S5.06
  std::string label(NodeIdx, const Maps&) const;        // "EGFR", "RRAS2+5", "[STAT1+6]" (07.C10)
  std::string pathway_label(std::string_view pathway, const Maps&) const;   // KGML title, map, id
  const IdIndex& ids() const noexcept;
  const NetworkInfo& info() const noexcept;
  const Provenance* provenance() const noexcept;
  core::Sha256 content_hash() const;                    // model fields only
};

class NetworkBuilder {
 public:
  explicit NetworkBuilder(NetworkInfo);
  NodeIdx add_node(Sym type, std::span<const GeneId> members);     // key = smallest by content
  void add_members(NodeIdx, std::span<const GeneId> ids);
  NodeIdx add_complex(std::span<const NodeIdx> components);        // S2.09 key
  void mark_exception(NodeIdx);
  EdgeIdx add_edge(NodeIdx src, NodeIdx tgt, Sym type, std::span<const Sym> subtypes,
                   std::span<const Sym> pathways, std::optional<double> score,
                   EdgeFlag extra = {});                             // merges on identity
  void add_component_edges(NodeIdx complex, Sym pathway);           // S2.09
  NodeIdx copy_node(const Network& from, NodeIdx);                  // with members (item 2B.11)
  EdgeIdx copy_edge(const Network& from, EdgeIdx, NodeIdx src, NodeIdx tgt);   // flags, pathways,
                                                                     // score, provenance
  void set_claim_order(std::span<const std::pair<GeneId, NodeIdx>> claims);   // legacy only
  void attach(Provenance);
  Network finish(core::Context&) &&;                    // tag once, sort, adjacency, IdIndex
};

// ---- views (02.C9, 05.C1) ----
class Bitset;                                           // fixed-size bit vector
class GraphView {
 public:
  explicit GraphView(std::shared_ptr<const Network>);
  GraphView(std::shared_ptr<const Network>, Bitset nodes, Bitset edges);
  const Network& network() const noexcept;
  bool has_node(NodeIdx) const noexcept;
  bool has_edge(EdgeIdx) const noexcept;
  std::size_t node_count() const noexcept;
  std::size_t edge_count() const noexcept;
  template <class F> void for_each_node(F&&) const;               // ascending index
  template <class F> void for_each_edge(F&&) const;
  template <class F> void for_each_out_edge(NodeIdx, F&&) const;
  template <class F> void for_each_in_edge(NodeIdx, F&&) const;
};

// ---- lookup and bindings.tsv (S2.03, S4.15; 02.C13; item 1.22) ----
enum class BindStatus : std::uint8_t { own, family, ambiguous, absent };
struct Binding { std::string given; GeneId id; BindStatus status; NodeIdx node; std::uint32_t holders; };
struct BindingRow {                                     // one line of bindings.tsv
  std::string role, given;
  std::optional<Binding> binding;                       // empty for 06's edge lines
  std::uint32_t line = 0;                               // 0: not from a file
  std::string note;                                     // 06's remark
};
class BindingTable {
 public:
  BindingTable(std::shared_ptr<const Network>, const Maps&);
  const Binding& bind(std::string_view role, std::string_view given, std::uint32_t line = 0);
  void add_row(BindingRow);                             // 06's edge-line rows
  struct Counts { std::uint32_t ids, own, family, ambiguous, absent; };
  Counts counts(std::string_view role) const;
  void write(std::ostream&) const;                      // once, at the run's end, into the stream of
                                                        // 09's Run::create_run_file("bindings.tsv")
};

// ---- documents and readers ----
struct Entry { std::uint32_t local_id; Sym type; std::uint32_t ids_first, ids_count,
               comps_first, comps_count; };
struct Relation { std::uint32_t entry1, entry2; Sym type; std::uint32_t subtypes, pathways;
                  std::optional<double> score; std::uint32_t line; };   // empty score: unscored
class PathwayDoc {
 public:
  const SourceRef& source() const noexcept;             // pathway id and title included
  std::span<const Entry> entries() const noexcept;      // every entry
  std::span<const Relation> relations() const noexcept;
  std::span<const GeneId> ids(const Entry&) const noexcept;
  std::span<const std::uint32_t> components(const Entry&) const noexcept;
  std::span<const Sym> subtypes(const Relation&) const noexcept;
  const Entry* entry(std::uint32_t local_id) const noexcept;
  std::vector<GeneId> gene_ids() const;                 // the file's GSEA gene set (S7.08, 08.C3)
  const core::Sha256& id_map() const noexcept;          // the map its ids went through
};

struct Sniff { Format format; int version; };
Sniff sniff(const std::filesystem::path&, std::optional<bool> tool_written,
            core::Context&);                            // raises network.flag_mismatch
std::shared_ptr<const PathwayDoc> read_kgml(const std::filesystem::path&, const IdMap&, core::Context&);
std::shared_ptr<const PathwayDoc> read_edge_table(const std::filesystem::path&, const IdMap&, core::Context&);
std::shared_ptr<const Network> read_fusion_network(const std::filesystem::path&,
                                                   core::Context&);   // as saved (load); sources as Provenance
std::vector<std::string> saved_network_diff(const NetworkInfo&, const BuildSettings&,
                                            const IdSet& exceptions, const IdMap&);  // rgx; empty: as saved
std::shared_ptr<const Network> read_legacy(const std::filesystem::path&, const IdMap&,
                                           const BuildSettings&, const IdSet& exceptions,
                                           core::Context&);

class StepDocs {                                        // one per step: the check fills it from the copies
 public:                                                // (09.C3); never shared between steps (S7.19)
  std::shared_ptr<const PathwayDoc> get(const std::filesystem::path& copy, const IdMap&, core::Context&);
};

std::vector<std::uint32_t> kept_relations(const PathwayDoc&, const RelationFilters&);  // also 08.C4
struct EntrySplit { std::vector<GeneId> singles, box; };
EntrySplit split_entry(const PathwayDoc&, const Entry&, const IdSet& exceptions, int split_threshold);

struct ListLine { std::string text; std::filesystem::path path; };          // item 2B.12
struct PathList { std::filesystem::path list, base; std::vector<ListLine> lines; };
std::filesystem::path list_base(const std::filesystem::path& list,
                                const std::optional<std::filesystem::path>& kgml_dir,
                                const std::optional<std::filesystem::path>& recorded);   // 02.C12, item 2B.10
PathList read_input_list(const std::filesystem::path& list, const std::filesystem::path& base,
                         core::Context&);               // input.missing names every missing file

// ---- writers, summaries and the labels query ----
struct WriteOptions { std::string root_name = "KEGG"; bool embed_sources = true; bool store_entry = false; };
void write_fusion_network(const GraphView&, std::ostream& out, const Maps&,
                          const WriteOptions&);         // out: a core::OutputFile's stream
void add_edges(const std::filesystem::path& edges, std::ostream& out, const IdMap&,
               core::Context&);                         // S2.30, 02.C17
struct Summary { std::uint32_t nodes, genes, complexes, compounds, others, isolated, edges,
                 multi_gene; NodeIdx largest; std::uint32_t largest_size; };
Summary summarize(const GraphView&);
std::vector<std::string> summary_lines(const Summary&, const GraphView&, const Maps&);   // sz
core::Json labels_answer(const LabelMap& symbols, const SourceRef& map_file);   // the labels query

}  // namespace fusion::network
```

## Choices

### 02.C1 · How network inputs from any source plug in

- **Proposed:** both.
  - One reader interface, with four built-in readers: KGML, FuSION's network file, the legacy read of FuSION 1-written files, and edge tables.
  - One documented exchange format, the edge table (02.C2). Any other pathway source plugs in by producing it, with a converter outside the backend or an export from the source.
  - Format detection from the content, with `format=` to override.
  - A further built-in reader (SIF, GPML, BioPAX, SBML-qual) can be added later behind the same interface, with nothing else changing.
- **Alternatives:**
  - Built-in readers only: one C++ reader per source, so each new source waits for a FuSION release.
  - Run-time plugins (shared libraries loaded by name): sources added without rebuilding FuSION, but a binary interface to keep stable on three platforms and compilers (G3).
  - The exchange format only: impossible as stated, because S2.05 and S2.06 keep a KGML reader.
- **Why:**
  - The common non-KEGG sources (SIGNOR, OmniPath, Reactome's functional interactions, STRING) already export tab-separated relation tables. Mapping their columns onto the table's named columns is a small conversion.
  - The readers that C1 lists cover every file the 114 batch files under `runs/` and `macOS/model_example/` read: 49 raw-KGML `mff` lines and 65 one-file `mff … y` lines.
  - Content detection replaces S2.12's trusted flag.
- **Decision:** as proposed (prompt log entry 160)

### 02.C2 · The exchange format: a headered edge table with inline boxes and complexes

- **Proposed:** the table described under "Edge tables" above.
  - Named columns: `source`, `target`, `type`, `subtypes`, `pathways`, `score`, `source_type`, `target_type`.
  - Comma-joined lists, `,` for box members and `+` for complex components. A type cell holds one type, or one per component joined by `+`.
  - Scores follow the one decimal grammar (3A.19); an empty cell is unscored (1.25).
  - `add`'s four-field lines accepted as they are.
  - Each row builds exactly as a KGML relation does.
- **Alternatives:**
  - FuSION's network XML as the exchange format: lossless and already needed, but converters must write numbered entries and relations, which is harder from R or a spreadsheet.
  - SIF (Cytoscape's simple interaction format): widely written, but it has no subtypes list, pathways or scores, and no families.
  - Separate node and edge tables: explicit, but two files per input, and every list entry would need a pair.
  - A configurable reader (column names and a vocabulary map given per file): reads source exports unchanged, but adds a second configuration language.
- **Why:**
  - One row per relation with inline boxes carries everything the model holds for a KGML relation, so a KGML and its table transcription build the same network. That is a test hook. Per-component types are needed for it: 7 of the 334 complexes built from the 273 KGMLs mix compound and gene components.
  - Lists are needed: 1,437 of the 9,704 kept relations in the 167 KGMLs carry two or three subtypes (312 of 2,559 in the reference run's 42), for example `activation,phosphorylation`. One subtype per row would split each into parallel edges (S2.20).
  - The user kept `add` at one subtype per line (S2.30, entry 044). The four-field form stays exactly so; only the headered form takes lists.
- **Decision:** as proposed (prompt log entry 160)

### 02.C3 · One id rule and one id map for every input

This choice also settles 04.C1's map layout and cross-section item 1.12, polled with it.

- **Proposed:**
  - Every id from every input passes one function at read time: delete `:`, then apply a map once.
  - One `IdMap` type and file layout: S1.16's two-column file `<foreign id> <network id>`, a later line winning, recorded by hash.
  - The run-wide map is `start`'s first argument or `id_map=` by name, and applies to every input. A value input's own `id_map=` (04) names either an S1.16 file or the keyword `symbols`, the symbol map read backwards, and replaces the run-wide map for that input.
  - The run-wide map passes an id it lacks through unchanged. A value input's own map drops the ids it lacks, and 04 reports them; `IdMap::lookup` says whether an id was mapped (prompt log entry 229).
- **Alternatives:**
  - One map per input in 04's draft layout (`<network id> <foreign id>`), one-to-many allowed: exact when sources overlap in confusing ways, but more arguments, a second layout, and the same id can mean different things in different inputs.
  - No mapping in the backend: users convert files beforehand. FuSION 1's position, and the cause of S3.03's silent non-matches (`hsa:3725`, `JUN` and `3725` all match nothing).
  - Strip colons only from KEGG-style prefixes: narrower, but leaves the same id in two spellings across inputs.
- **Why:**
  - FuSION 1 already deletes colons in two readers (`XMLParser.cc:885`, `GraphManagerNew.cc:28650`) and not in the others. The coexpression CSV the runs pass names `hsa:972`, while LFC files use `hsa1000`.
  - One rule at the boundary means every module sees one namespace.
  - The empty map that every FuSION 1 run passes changes nothing.
  - The reverse symbol map is nearly unambiguous: 8 of 6,326 symbols in FuSION 1's map. With `id_map=symbols`, 04's probe table (22,283 rows keyed by symbol) gives 3,050 of the EGF file's 3,054 ids: the earlier ids of ICOS, CSNK1E, ATG4B and PDE6G are lost, because a later map line wins. The lists at ±1.01 are unchanged, 43 up and 2 down genes (corrected in prompt log entry 230).
- **Decision:** as proposed (prompt log entry 160)

### 02.C4 · FuSION's network file: the form of the mark, the header and embedded sources

S2.25 has decided the rest: keep `wgx`'s XML format and element structure, escape attribute values, take display names from the symbol map, and mark the file as FuSION's own rather than KGML. This choice covers only what S2.25 left open, and was polled with 03.C4 (the shared store's base merge is the same file) and 03.C7 (network files in an `mff` list).

- **Proposed:**
  - The mark is `format="fusion-network" version="1"` on the root `<pathway>`, and KGML's DOCTYPE is dropped.
  - Members are listed sorted, with an explicit `key` attribute.
  - A `<fusion>` header records the build settings, the inputs with their SHA-256 and KGML title, the exception set, the legacy flag and, for a merge, 03's `<merge>` element. Inputs and sources are written in canonical order (pathway id, then SHA-256).
  - `<source>` sections embed each input's relations and every entry a relation reaches, so a saved network can be rebuilt under other settings or exceptions without its original files. `embed_sources=false` turns them off.
  - The shared store's base merge (03.C4) is the same format, with an empty exception set, `base="true"` and no input paths. A network file in an `mff` list contributes its embedded documents, as if its original files were listed (03.C7).
- **Alternatives:**
  - A new root element (`<fusion-network>`) as the mark: cleaner, but FuSION 1 could no longer read 2.0's networks, which Phase 3 can use.
  - A FuSION DOCTYPE as the only mark: no new attribute, but the mark then sits outside the element tree, where pugixml keeps it only with its non-default `parse_doctype` option.
  - No embedded sources: the header records input paths and hashes, and a rebuild re-reads them. Files are smaller, but a saved step depends on KGMLs that may have moved, against A1's "each run is reproducible from its own directory"; 03.C4 would then need separate tables.
  - Embed every entry, not only those a relation reaches: the documents would be complete for GSEA too, at 441 KB, 1.58 MB and 3.13 MB instead of 348 KB, 1.3 MB and 2.1 MB.
- **Why:**
  - S2.22 and S2.13 need the exceptions split out of a saved merge whenever a new FSC sits in a family node. That needs the boxes and relations, not only the merged nodes: a union made through an exception id must be undone (the {FOS, MAPK9, MAPK10, SP1} case of 03).
  - The embedded sources cost about 18 % of the raw KGML bytes (348 KB for the 42 KGMLs, 2.1 MB for all 273). An entry no relation reaches can never become a node, so leaving it out loses nothing a rebuild needs.
  - FuSION 1 reads only root-level `entry` and `relation` children (`XMLParser.cc:635`, `666`), so the header and sources do not disturb it.
- **Decision:** as proposed (prompt log entry 160). *Sprint (prompt log entry 309):* the network module's reader needs five more things to rebuild a network exactly, and the format carries them: a `flags` attribute on relations; `flags="exception"` on entries; `<claim>` elements that keep the legacy reader's holder order; `size` and `format` attributes on `<input>`; and an `<id_map>` element. FuSION 1 still reads 2.0's files, because it reads only the `entry` and `relation` elements and ignores attributes it does not know.

### 02.C5 · The legacy reader: which files, and how exactly it reproduces FuSION 1

- **Proposed:**
  - Read every unmarked file that FuSION 1 wrote (no 2.0 mark and no `org` on the root) with the legacy reader, whatever `y|n` says: the old master, FuSION 1's `wgx` outputs and FuSION 1's `add` output.
  - Reproduce FuSION 1's graph 2 exactly: read 1, then the in-memory replay of `wgx` → `rgx y`, keeping S2.08's loss (this section's open question 1, entry 163).
  - Then merge exact duplicate edges with S4.13's rule, so edge identities stay unique.
  - This widens S2.27's "this one file" and moves S4.13's duplicate merge from the corridor copy to reading; accepted in prompt log entry 159.
  - The run's exception set applies, so a known FSC gets its own node (S2.13; this section's open question 8, entry 164). Edge tables and `add`'s marked output may share the master's `mff` list and bind to its nodes by first claim, with no union (03.C9, entry 163).
- **Alternatives:**
  - Only the old master, recognised by its SHA-256 (S2.27's "this one file"). FuSION 1-written masters would then go through the 2.0 network reader, which does not reproduce FuSION 1's claims.
  - A plain single-pass tool-written read with no replay: simpler, but it differs from graph 2 for 194 ids and the members of 8 nodes, so published runs are not reproduced like for like.
  - Keep duplicates as parallel edges with equal identities (FuSION 1's `sz` count of 7,400): exact counts, but stable edge keys stop being unique, and 05 must merge them at the corridor.
- **Why:**
  - Of the 65 one-file `mff … y` batches under `runs/` and `macOS/model_example/`, 56 read the old master (checked by SHA-256, once paths from before FuSION 1's reorganisation are mapped to `runs/`), 7 read masters that FuSION 1's own `wgx` wrote under `runs/egf_hela/experiment/`, and 2 name files that no longer exist.
  - The replay reproduces graph 2 exactly on the full run (3,100 nodes, 198 isolated, 7,400 edges, every binding equal) and on the GUI run at threshold 15000 (8,139 nodes, 82,188 edges).
  - Merging duplicates (768 on the full run, 10,568 on the GUI run) changes `sz`'s edge count but not the corridor or the solve, because FuSION 1 merged them at the corridor copy (S4.13: 4,126 → 3,535 forward edges on the full run).
- **Decision:** as proposed, widening S2.27 and moving S4.13's merge to reading (prompt log entry 159)

### 02.C6 · Where ECrel exclusion applies

S2.11 and S2.31 have decided both settings: indirect effects are discarded by default or kept as "1+" hops, recorded, and applied before nodes are made; ECrel edges are kept by default, can be excluded by a run setting, and are marked in the outputs. The only open point is where `exclude` acts.

- **Proposed:** at the same point as the indirect filter. ECrel relations are dropped when a document is built, before any node is made, so an enzyme that only ECrel relations reach never becomes a node. The parsed document keeps every relation, so changing the setting never forces a re-parse.
- **Alternatives:**
  - At the corridor, as a non-traversable type next to S4.04's subtypes: the network keeps the edges for display, but the enzymes stay in the network as untraversed nodes, and S3.12's "absent from the network" list then counts them as present.
  - Per command, as arguments of `rgx` and `mff`: finer control, but one run could then mix networks built under different rules.
- **Why:**
  - S2.11 fixes the point for indirect effects; doing ECrel the same way gives one place and one rule for relation filters.
  - The real 42 KGMLs have 31 ECrel relations after the indirect discard.
  - As run settings, both are recorded once in run.json and in each saved network's header, which `rgx` compares (02.C4).
- **Decision:** as proposed (prompt log entry 161)

### 02.C7 · maplink relations

- **Proposed:** skip KGML `maplink` relations by default, with the run setting `network.maplink=keep` to read them as edges, and count the skipped relations in `network.read`.
- **Alternatives:**
  - Read them as edges, as FuSION 1 does with every relation type (S2.31). S2.06 keeps "the subset of KGML that is accepted".
  - Drop them with no setting.
- **Why:**
  - None of the 167 KGMLs any run used has a maplink relation, so no recorded result changes.
  - The full KEGG human set has 723, in 24 of the 273 KGMLs, mostly metabolic maps (221 in `hsa00250`).
  - They link genes or complexes to `map` entries, which stand for whole other pathways: 384 gene → map, 208 map → gene, 129 map → map and 2 group → map, all with subtype `compound`. That subtype is traversable (S4.04), so a corridor could route through a node that stands for a whole pathway.
  - Keeping them adds 75 `map` nodes and 667 edges to the 273-KGML network (3,034 nodes and 10,854 edges instead of 2,955 and 10,187).
  - A list built from the full KEGG set, which GSEA candidates may come from (stage 7), would meet them.
- **Decision:** as proposed (prompt log entry 161)

### 02.C8 · Memory layout: interned struct-of-arrays with compressed adjacency

- **Proposed:**
  - Nodes and edges as plain arrays of fixed-size records (28 and 32 bytes).
  - Every string interned to a 32-bit handle; subtype lists and pathway sets interned once each.
  - Out- and in-adjacency as offset and index arrays.
  - Ids and node keys found through hash tables with core's fixed hash, O(1); an edge found by binary search in its source's sorted out-edges, O(log deg).
  - Immutable after `finish()`.
- **Alternatives:**
  - One object per node and edge with string and vector members (FuSION 1's shape, about 400 bytes per edge): simplest to write, but 10 times the memory, pointer chasing in every loop, and copies are expensive.
  - A graph library (Boost.Graph's `adjacency_list`): ready-made algorithms, but a large dependency for three platforms, no interning, and its descriptors are not stable across copies.
- **Why:**
  - The measured range runs from 2,291 edges (the 42 KGMLs) to 82,188 (the GUI run) and about 149,000 (all 273 KGMLs with no collapse). The proposed layout holds the largest in about 7 MB.
  - Distinct subtype lists number 25 to 44 and distinct pathway sets 207 to 1,788, so interning them shrinks each edge to two 4-byte ids.
  - The real gains are no per-step copies and no linear scans; S2.04 and S2.22 recorded FuSION 1's `HIGHLY INEFFICIENT CODE` scans and a 122–126 s merge.
- **Decision:** as proposed (prompt log entry 161)

### 02.C9 · Derived graphs as views over one network

This choice was polled with 05.C1, which builds the corridor as such a view.

- **Proposed:** a corridor, or any other subset, is a `GraphView`: the network plus node and edge bitsets, using the network's indices. Nothing is copied, and the network is `const`.
- **Alternatives:**
  - Compact copies with new indices and a map back (FuSION 1's pattern without its session-wide counters): denser loops over a 115-node corridor, but every output must translate indices, and copies can drift.
  - Copy-on-write subgraphs: a view until something changes it. Nothing in 2.0's pipeline changes a corridor (S4.20), so the extra machinery buys nothing.
- **Why:**
  - S4.14 asks that any internal id mean the same node in every step. With views that holds by construction: EGFR is one index in the network, the corridor and both solves, where FuSION 1 gave it 3273, 7596 and 10044.
  - S4.15's single lookup, S4.20's unchanged corridor and S5.29's no shared state follow too, and both FSC solves share one read-only object.
  - A view of the largest network costs about 20 KB.
- **Decision:** as proposed (prompt log entry 161)

### 02.C10 · Canonical order of nodes and edges

- **Proposed:** sort nodes by key in byte order, and edges by (source index, target index, type, subtype list). Number file entries in that order.
- **Alternatives:**
  - First-appearance order (FuSION 1's creation order): files read in their own order, but a merge's output then depends on list order, and S2.17's order independence can only be checked by comparing graphs, not bytes.
  - Sort for writing only, and keep creation order in memory: stable files, but iteration order, and so tie-breaks in 05 and 06, would still follow the input order.
- **Why:**
  - Sorting costs O(N log N + E log E), a few milliseconds at 150,000 edges.
  - It makes every later step, and every file, independent of the order of the KGML list and of the platform.
  - It turns stage 2 open question 4 and S2.22's split equivalence into byte comparisons Phase 3 can make.
- **Decision:** as proposed (prompt log entry 162)

### 02.C11 · The stable edge key

- **Proposed:**
  - An edge's key is (source key, target key, type, ordered subtypes), without the tag, printed `src|tgt|type|sub1,sub2`.
  - JSON outputs also give the four fields separately.
  - Indices appear only alongside keys.
  - This settles S4.14's key ("source, target and subtypes") by adding the type; accepted in prompt log entry 159, where 07.C11's run-stable edge ids lapse.
- **Alternatives:**
  - S4.14's wording, source, target and subtypes, plus 07.C11's run-stable edge ids: shorter keys, but not unique, so identity rests on ids.
  - An opaque 64-bit hash of the fields: compact, but unreadable in a TSV and useless for grep.
  - The index alone: compact and unique within a run (02.C9), but it means nothing in another run.
- **Why:**
  - Leaving out the type merges 3 edges in the reference run's network and 35 in the full run's graph.
  - `|` occurs in no KEGG, Entrez, Ensembl or UniProt id, or in the 1,969 complex keys of FuSION 1's symbol map, while `_`, which FuSION 1's edge-file labels used (S5.06), occurs in every complex key.
  - Every output (S5.27), 07's results and the Phase 5 display (0027) need the same key.
- **Decision:** as proposed, amending S4.14's wording (prompt log entry 159)

### 02.C12 · Input lists and the KGML directory

- **Proposed:**
  - List entries are trimmed, and blank lines skipped. Each line comes back as its text and its resolved path (2B.12).
  - Relative entries resolve against `kgml_dir` when given; else, for a list a step wrote, the base that step recorded in run.json (2B.10); else the list file's folder.
  - A bare pathway id becomes `<base>/<id>.xml`, so the shipped `data/kegg_pathways.txt` (167 bare ids, 2B.5) resolves against each user's folder.
  - Duplicates are found by file identity and read once, with a warning. Two files with one pathway id fail, as `list.duplicate_pathway` (1.24, 03.C6). The check names every missing file at once (this section's open question 6).
  - `kgml_dir` is a named argument of `mff` and `gsea`. The frontend fills it from the user's settings, and no environment variable is read.
- **Alternatives:**
  - Relative to the launch directory (FuSION 1): breaks as soon as the frontend starts runs from their own directories.
  - Relative to the run directory: lists would have to be copied into each run.
  - A `FUSION_KGML_DIR` environment variable: convenient, but hidden state that run.json would have to capture separately.
- **Why:**
  - S8.31 ships a KGML list without KGMLs, "resolved against the user's own KGML directory", which needs a base folder and bare names.
  - All 167 entries of FuSION 1's list are absolute, so nothing old changes.
  - File-identity checks catch the case-only duplicates that macOS and Windows allow.
  - A shortlist repeats its candidates' lines as given (S7.15), so it needs the base they were resolved against.
- **Decision:** as proposed (prompt log entry 162)

### 02.C13 · The binding report

- **Proposed:**
  - One run file, `bindings.tsv`, at the run directory's top level, with one row per (role, id) the first time it is bound: the given and canonical id, the status (`own`, `family`, `ambiguous`, `absent`), the node's key, label, size and member labels, any other holders, and 06's `line` and `note` (1.22). 06 adds its edge-line rows through the table.
  - The table is written once, when the run ends, whatever its status, through 09's top-level file writer `Run::create_run_file` (accepted in prompt log entry 230; named in entry 244).
  - One `network.bindings` summary event per role, emitted as each step ends.
  - The console prints the summary only.
- **Alternatives:**
  - Events only, one per family or ambiguous binding: no new file, but the coexpression CSV alone gives 356 such events on the full run, and the frontend would have to rebuild the table.
  - Console warnings only (FuSION 1 had none): easiest, but lost in a log of hours.
- **Why:**
  - S2.03, S4.02 and S3.12 all ask for the binding to be shown.
  - The real measure: a third of the 702 coexpression ids land in family nodes, and 109 are ambiguous on the full run.
  - One TSV row per id is small: about 750 rows for a whole run.
- **Decision:** as proposed (prompt log entry 162)

### 02.C14 · Edge fields that nothing reads (S2.04, open question 7)

- **Proposed:** the edge record carries none of FuSION 1's `weight`, `weight_tuple`, `capacity`, `flow`, `other1`–`other4`, `other`, `deleted` and `reaction_desc`. This closes stage 2's open question 7.
- **Alternatives:** keep them as unused, initialised fields for parity with S2.04's record.
- **Why:**
  - In the live paths, only `duplicate_edge` copies `weight` and `weight_tuple` (`Linux/src/GraphManagerNew.cc:9495`, `9497`), and pathz3's undirected copy copies `weight` (`8525`). The undirected copy's results are unused (S5.08).
  - Every other read is in code that no live command reaches. Most sit in the Gomory–Hu mincut routines: the largest share in `compute_weighted_min_cut_for_ghtree_relabel_to_front` (`12867`, 36 reads outside comments), 22 in `compute_min_cut_for_ghtree_relabel_to_front` (`11980`), and more in `compute_min_cut_for_ghtree_relabel_to_front_2` (`13627`) and `compute_min_cut_for_ghtree` (`14538`). Their entry points `compute_gh_tree` (`15185`), `compute_gh_tree_2` (`15357`) and `get_connectivity` (`24304`) are called only from `#if 0` regions of `main.cc` (`3664`, `3771`, `6044`), or not at all. The rest are in the disabled display commands (S1.23), in `#if 0` regions, or in accessors and copies. The single hit inside pathz3's encoding range is a comment (`27936`).
  - Nothing that changes a result reads them. Keeping them would double the edge record.
- **Decision:** as proposed (prompt log entry 162)

### 02.C15 · How deeply network inputs are checked before a run

- **Proposed:**
  - The pre-run check (0021) fully parses every network input and list entry, with every S2.06 check. It parses the copies in `inputs/`, which 09 makes and hashes first (09.C3 with 1.13).
  - Each step keeps the parse of its own inputs for that step. `gsea` and `mff` share none, as S7.19 decided (4A.5).
  - 05.C3's id check and 03's early type check use this parse.
  - The frontend validates through the check-only mode (09.C12), which parses the originals.
- **Alternatives:** check only that each file exists and is readable, and sniff its format; parse at the step. 05.C3 and 03's early checks would then need their own reads.
- **Why:**
  - A full parse finds a malformed KGML or a dangling reference before a run that may last 50 hours (fdr050).
  - It costs at most the parse time measured above, under a second for all 273 KGMLs, and the parse is then not repeated.
  - Holding the documents costs about 2 MB for the 273 KGMLs. They hold every entry, as 08's gene sets need: 37,767 entries with 67,853 ids, and 17,670 relations.
- **Decision:** as proposed (prompt log entry 159)

### 02.C16 · Node fields that 2.0 does not store

This choice amends S2.02, which kept FuSION 1's node record; accepted in prompt log entry 163.

- **Proposed:**
  - `display_names` are not stored. Labels are derived from the symbol map when printed, by S2.15's and S2.25's rules, with 07.C10's family size: the key's symbol or the key, `+N` for the other members of a family, `[A+B]` for a complex.
  - `other_value1`, the id of the node a copy came from, is not stored. Views (02.C9) keep one index per node, so there is nothing to point back to.
  - `excess` is not stored. Only mincut code that no live command reaches uses it (see 02.C14).
  - `description`, `compartment` and `pathways` stay, as an optional side table (S2.02's own reason: a future location check may fill `compartment`).
- **Alternatives:** keep every S2.02 field, storing display names at build time from the same symbol map.
- **Why:**
  - Within a run the symbol map does not change between build and print, so derived labels equal stored ones; the difference is that a saved network then never depends on the map, and its `content_hash` stays the same under a newer symbol map.
  - FuSION 1's stored display names were the source of S2.25's fault: they collected other genes' KGML aliases, so EGFR's label began `EGFR, CSF1R, BANDDOS, …`.
  - Storing them would cost a range per node (8 bytes of record) plus the strings, for no reader.
- **Decision:** as proposed, amending S2.02 (prompt log entry 163)

### 02.C17 · `add` writes FuSION's marked format

This choice amends S2.30's "keep its output format"; accepted in prompt log entry 163, together with 03.C9 admitting edge tables and `add`'s marked output beside the legacy master (cross-section item 3A.27).

- **Proposed:** `add` writes the same elements and attributes as FuSION 1's `add` (one `gene` entry per id, labelled with the id; one relation per line; `pathways="KEGG"`; no score), with the mark of 02.C4 on the root and without the DOCTYPE `Additional edges added`. 2.0 then reads it as FuSION's own file, as a transcription with no build rule.
- **Alternatives:** write FuSION 1's layout exactly. 2.0 then detects it as an unmarked FuSION 1-written file and reads it with the legacy reader, so any network merged with it is flagged legacy.
- **Why:**
  - The GUI's flow merges `add`'s output with the master (S2.26, S8.23), and S8.08 keeps the additional-edges step in both modes, so the output has to be readable next to other inputs.
  - Nothing reads `add`'s output except FuSION's own readers; the in-binary `add` has no recorded output (S2.30), so no file on disk changes.
  - FuSION 1's `rgx … y` would still read the marked file, since it ignores unknown attributes.
- **Decision:** as proposed, amending S2.30 (prompt log entry 163)

## Interfaces with other sections

- **01 Batch.**
  - Its command table holds `start` (optional, 2B.4), `rgx`, `mff`'s `split_threshold`, `tool_written` and `kgml_dir`, `wgx`, `sz`, `add`, and the query-only `labels` (2B.6). The `y|n` flag and `mff`'s ignored `xml` token and map files pass through for warnings.
  - The graph store (D1) holds `std::shared_ptr<const Network>` and `GraphView` values under integer ids, with `0` for the latest. `load` restores a saved network through `read_fusion_network`, exactly as saved (2A.4).
  - It prints the console lines this module returns (S2.05's `Read graph: …`, `sz`'s lines).
  - It hands back the in-memory network for a file the run wrote, unchanged since by identity and SHA-256 (01.C13).
  - The `run` line and `--set` carry `network.indirect`, `network.ecrel` and `network.maplink` (2B.2); `--query` runs `labels` (3A.8).
- **03 Merge.**
  - It consumes `PathwayDoc`s, and `Network`s for FuSION-network and legacy inputs, and produces `Network`s through `NetworkBuilder`.
  - It uses `split_entry`, `kept_relations`, the complex key, `add_edge`, `copy_node`, `copy_edge`, `Provenance` with its two kinds of reference, and the `<merge>` header (2B.11, 03.C4).
  - It owns the union, also for a one-file read (03.C5), the exception set, split and reuse, type conflicts, the merge report and the rule for the legacy master's list (03.C9). Its `mff` step saves `mff/merged_graph.xml` with this section's writer.
  - Its `StepDocs` holds only the parse of `mff`'s own inputs (S7.19). The list reader raises `list.duplicate_pathway` for it (03.C6).
  - The rules version in its store key covers this section's reader and build rules, so a change to either bumps it (03.C8).
- **04 Expression.**
  - It applies `IdMap::canonical` to value ids, with the run-wide map or the value input's own `id_map=`, an S1.16 file or `symbols` (1.12). It shares `core::GeneId`.
  - It uses the `BindingTable` for S3.12's two groups: absent from the network, or bound but outside the corridor (`GraphView::has_node`).
  - It names this section's `Network` and `LabelMap` where its draft said `Graph` and `Names` (2B.1).
- **05 Corridor.**
  - It builds the corridor as a `GraphView` (05.C1) and saves `corridor.json` against the network's `content_hash` (05.C6).
  - It reads subtypes and flags (`component`, `undirected`, `ecrel`, `indirect`, `self_loop`) for S4.04, S4.05 and S4.11, and `normalise_subtype` for `skip_subtypes` (2B.18).
  - It resolves endpoints and exclusions through the `BindingTable` (S4.02, S4.15), and checks `fb_rch`'s ids against the parsed documents before the first step (05.C3).
  - Unique edge identities, with duplicates merged at reading (02.C5), mean the corridor needs no merge step.
- **06 Solve.**
  - It names nodes and edges by `NodeKey` and `EdgeKey`, encodes `effect_of` and `is_relaxed` (S6.14), and uses `find_edges` for S5.06's edge files.
  - It binds node-file and coexpression ids through the `BindingTable`, and adds its edge-line rows with `line` and `note` (1.22). Coexpression ids get the colon rule.
  - It counts `unscored` edges apart in S5.16's frozen-edge report (1.25).
  - It may solve the two FSC cases in parallel, since networks and views are immutable.
- **07 Results.** It uses `NodeKey`, `EdgeKey` and its text, `NodeKind`, `Network::label` (07.C10), `pathway_label` with KGML titles, member lists for family nodes (S2.03, S2.17), the `ecrel`, `indirect` and `legacy` flags, `is_relaxed` for its polarity check, and `bindings.tsv`.
- **08 Enrichment.**
  - It reads the candidate list with `read_input_list` (its `PathList`, with each line's text, 2B.12), records the base it resolved against (2B.10), and takes `kgml_dir`.
  - Each candidate's gene set is `PathwayDoc::gene_ids()` in any format this section reads (08.C3), and its relation share counts `kept_relations` under the run's `RelationFilters` (08.C4; its draft's `EdgeSettings`).
  - It parses its own candidates and shares no parse with `mff` (S7.19). It defines `Shortlist`.
- **09 Runs.**
  - It owns `core::Interner`, `core::GeneId`, `core::Sym`, `core::stable_compare` and the fixed hash (2A.5), and the vocabulary of 1.1.
  - It copies and hashes inputs before the check parses them (09.C3), and records `SourceRef`s, KGML titles and the id map's digest in run.json.
  - It provides `OutputFile` for every write (2A.6), and `Run::create_run_file(name)` for a file at the run directory's top level, through which `bindings.tsv` is written once at the run's end (1.22, entries 230 and 244). It also provides the output-clash check (2A.7) and the output-path rule (2B.3).
  - Its settings registry holds this section's `network.*` keys (1.16), and its envelope wraps this section's events (1.3).
- **10 Frontend.** It runs `labels` through `--query` for its gene lookup (10.C15), stores the user's `kgml_dir`, and validates through the check-only mode.
- **11 Build.** It supplies pugixml and fast_float (11.C5); a macOS deployment target of 13.3 or later for floating-point `std::to_chars`, or a replacement; the long-path manifest entry on Windows, and documentation of the `LongPathsEnabled` policy; the rebuilt maps with `data/SOURCES.md`; and `data/kegg_pathways.txt` (2B.5).
- **12 Tests.** It tests the legacy reader against graph 2's counts (1.30), and the hooks below.

## Test hooks

- **Legacy exactness.**
  - The full run's old master with its exception file gives 3,100 nodes (2,516 genes, 380 complexes, 204 compounds), 198 isolated, 7,400 edges before the duplicate merge and 6,632 after.
  - Bindings equal FuSION 1's for every id, for example `hsa3265` → `hsa22800` and `hsa1386` → `hsa1386`, with `hsa10488` also holding it. The 125 ids of S2.08's loss stay absent.
  - The GUI run at threshold 15000 gives 8,139 nodes and 82,188 edges (71,620 after the merge).
- **KGML rejections.** Synthetic files with a dangling relation, a dangling component, a repeated entry id, an empty group, a cyclic group and a double space in a name each fail with the right code.
- **Scores.** `625` reads as 625; `625x` and `abc` read as unscored, each with `network.bad_score`; an absent score is unscored without a warning. An unscored edge round-trips through a written file, and merging it with a scored copy of the same identity clears the flag.
- **Detection table.** A raw KGML, a 2.0 file, a FuSION 1 `wgx` output, the old master, a FuSION 1 `add` output, a headered table, a four-field table with leading blank and `#` lines, and an unknown file each reach the right reader. `y|n` mismatches raise one `network.flag_mismatch`.
- **Round trips.**
  - Write then read gives an equal `content_hash`; `load` restores it exactly, and `rgx` with other settings rebuilds from the embedded sources.
  - A KGML and its edge-table transcription build equal networks, including `hsa04610.xml`'s compound-and-gene complex.
  - FuSION 1's `rgx … y` reads a 2.0 file, so Phase 3 can run FuSION 1 downstream of a 2.0 merge.
- **Order and platform independence.** The 42 KGMLs in shuffled list orders give byte-identical files. Files written from the same inputs on Linux, macOS and Windows have the same `content_hash` and identical bytes apart from the header's input paths. Every id is in exactly one node (stage 2 open question 4).
- **Example values.** The reference run's network has 1,129 nodes and 2,509 edges; its entry 1 is `c_s_cpdC00752_hsa10544_hsa7056_c_e`, and GRB2, MAX and MYC are entries 531, 697 and 723.
- **S2.08.** The ids FuSION 1 lost from the 42-pathway merge (RUNX1 `hsa861`, TSHB `hsa7252`, SERPINF2 `hsa5345`, the glutamate-receptor subunits, `cpdC22540`) are present.
- **Polarity.** The 3 × 3 × 2 table of `is_relaxed` matches FuSION 1's rule; `effect_of` gives inhibition for `inhibition,dephosphorylation` and activation for no subtype.
- **Labels.** The 6-gene RAS box prints `RRAS2+5`; a complex of the 7-gene STAT box prints `[STAT1+6]`; `path:hsa04010` prints `MAPK signaling pathway` from `hsa04010.xml`'s title, and a pathway id with no title and no map entry prints as itself.
- **Lists.** A list naming three missing KGMLs fails once, naming all three. `updated_pathways/xml_files/hsa04010.xml` and `auxilliary/hsa_pathways_with_exp_score/hsa04010.xml` in one list fail with `list.duplicate_pathway`. A shortlist read back from a saved run resolves against its recorded base.
- **Id rule.** `hsa:3725` and `hsa3725` bind the same node. A later id-map line wins and is reported. A one-token id-map line maps its id to nothing, with a warning (S1.16). `id_map=symbols` reports 8 ambiguous symbols on FuSION 1's map. An id the run-wide map lacks passes through unchanged, while `lookup` on a value input's own map returns nothing for it, so 04 drops and reports it (entry 229); on the probe table, `id_map=symbols` keeps 3,050 ids.
- **Filters.** The 42 KGMLs drop 204 indirect relations by default. `keep` flags them. `ecrel=exclude` removes ECrel edges. The 273 KGMLs skip 723 maplink relations, and no `map` node appears; with `maplink=keep` there are 75.
- **Binding census.** The 702 coexpression ids bind 333 own, 247 family, 109 ambiguous and 13 absent on the full run's legacy graph, and 115 own, 250 family and 337 absent on the reference run's 2.0 network.
- **`labels`.** The query on a map returns one pair per row, in byte order of id, and writes no file.
- **Benchmark.** All 273 KGMLs at threshold 15000 (about 10,000 nodes and 149,000 edges) and the GUI legacy graph build within a stated time and peak memory, recorded for Phase 3, such as under 1 s and under 64 MB.

## Open questions

1. **Does S2.08's fix apply to the legacy reader?** *Settled:* no; legacy reads keep S2.08's loss and reproduce graph 2 exactly, and the fix applies to the raw pipeline (prompt log entry 163).
2. **S2.27 names "this one file", but 7 of the 65 old-master batches read masters that FuSION 1's own `wgx` wrote.** *Settled by 02.C5:* every unmarked FuSION 1-written file is read as legacy (prompt log entry 159).
3. **S4.14's edge key ("source, target and subtypes") is not unique.** *Settled by 02.C11:* the key includes the type (prompt log entry 159).
4. **Should the id map allow one id to map to several?** *Settled:* one `IdMap` in S1.16's layout with a later line winning; a value input may use `id_map=symbols` (prompt log entry 160, with 04.C1 and cross-section item 1.12).
5. **`add` writes FuSION's marked format, not FuSION 1's unmarked layout.** *Settled by 02.C17,* amending S2.30 (prompt log entry 163).
6. **KGMLs are not shipped (S8.31), and nothing in 2.0 fetches them.** *Settled:* documentation only; the check before a run names every listed pathway missing from the folder (prompt log entry 164).
7. **Stage 2 open question 1 (graph 83 against graph 84) narrows.** In 2.0 the raw pipeline builds the normal form directly, and the legacy reader replays the re-read exactly. Phase 3 still has to show that the solve's results agree. *Still open, for Phase 3.*
8. **S2.13 and S2.27 pull apart on legacy reads.** *Settled:* legacy reads follow S2.13 when the FSC is known; unchanged FuSION 1 batches name no FSC and keep their own exception file (prompt log entry 164).
9. **02.C16 would amend S2.02.** *Settled by 02.C16* (prompt log entry 163).
