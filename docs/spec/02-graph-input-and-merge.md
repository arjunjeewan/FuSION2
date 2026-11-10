# 02 · Graph input and merging

## Scope

This stage covers the commands that build the signalling graph and report on it: `add`, `rgx`/`read_graph_xml`, `mff`/`merge_from_file`, `wgx`/`write_graph_xml`, `upfx`/`unique_prefix` and `sz`/`size`. It follows their handlers in `Linux/src/main.cc` into the KGML reader (`Linux/src/XMLParser.cc`) and into the graph model, merge code and XML writer in `Linux/src/GraphManagerNew.cc` and `Linux/include/GraphManagerNew.h`. It also covers how merging uses the maps that `start` loads (all_id_map, the merge-exception set, the symbol map, the pathway-name map) and mff's own map-file argument. Stage 1 owns dispatch, `start`, the readers of those four files (S1.15–S1.19), `mylog.txt` (S1.25), the fixed-name outputs in the launch directory (S1.26) and the compile-time flags (S1.29); this stage cites those items rather than repeating them. Stage 7's `gsea` writes the KGML list that mff reads. Stage 3's `cudf` writes the up- and down-regulated files that the batch concatenates into the exception file. Stage 4 takes over at `fb_rch`, which runs on the graph that rgx re-reads. Stage 8's GUI writes these batch lines. The two places where `macOS/src/main.cc` differs (pathz3 argument parsing, helper-script lookup) are outside these commands, and `XMLParser.cc`, `GraphManagerNew.cc` and both headers are byte-identical between `Linux/` and `macOS/` (checked with `cmp`). Names used below:
- "the real run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`, with its batch file, its `run_console.log` and its `run/` folder;
- "the 42-pathway merge" is that run's `run/merged_master_graph.xml` (1156 entries, 2529 relations), written by `mff` over the 42 KGMLs listed in `run/gsea_kgml_files.txt`;
- "the old master" is `runs/real_experiments/merged_kegg_master_graph_with_exp_score.xml` (5307 entries, 7517 relations), together with its six identical copies;
- "the 167 KGMLs" are the files in the real run's `inputs/list_of_kegg_xml_files.txt`: 163 in `auxilliary/hsa_pathways_with_exp_score/` and 4 in `updated_pathways/xml_files/`.

Counts come from Python scripts that parse these files; FuSION 1 was not run. `<TAB>` stands for a tab character.

## Items

### S2.01 · Graph store, graph ids and the `0` shorthand

- **FuSION 1:**
  - `Linux/src/GraphManagerNew.cc`: `48-52` (static counters), `8360-8371` (`get_graph`), `8405-8408` (`add_graph`), `8614-8623` (`add_graph_genesis`), `19517-19548` (`read_graph_xml_file`), `9810-9815` (a merge step allocates a graph), `20222-20254` (`destroy_graph`)
  - `Linux/src/main.cc`: `1594-1595` and `2593-2594` (graph `0`), `2446-2450` (mff's genesis record)
- **What it does:** The graph manager keeps every graph it creates in a map from integer graph id to graph. One static counter, `graph_id_count`, hands out graph ids. Each file read by rgx or mff takes the next id, and so does each step of an mff merge.
  - **Ids in the real run:** graphs 1–42 are the 42 pathway reads and 43–83 are the merge steps (` merging graphs: 1 and 2`, ` merging graphs: 43 and 3`, …, `New graph: 83`). The rgx re-read is graph 84 (`run_console.log:87-232`).
  - **Graph `0`:** passed to sz or wgx, it means the counter's current value, which is the last graph created.
  - **Node and edge ids:** two more static counters, shared by all graphs, hand these out. A node id is unique for the whole session and never reused, so the same gene has a different node id in every graph that holds it (each pathway graph, each merge step, the re-read). Later stages therefore have to compare nodes by key, not by node id. The manager records which graph owns each node id and edge id.
  - **Genesis record:** each graph also gets one, holding an operation string and operand graph ids. A read records `read_graph from file: <path>`. A merge step records `Merged graphs` with both operands. mff finally overwrites the merged graph's record with `Merged graphs from file <list>` and no operands.
- **Problems:**
  - Nothing is freed. The real run's 41 intermediate graphs stay in memory until exit, each one a full copy of the graph merged so far. The 163-pathway build made 162 of them (`runs/egf_hela/experiment/build_graph_egfr.log:519`: `New graph: 325`).
  - Genesis records are written but never read. The only command that prints them, `genesis`/`gen`, is inside `#if 0` (`main.cc:2572`).
- **Proposed call:** fix. Keep integer graph ids and `0` for the latest graph, but free merge intermediates and drop the genesis records, which no live command reads.
- **Decision:** fix, as proposed (prompt log entry 033)

### S2.02 · Node record

- **FuSION 1:** `Linux/include/GraphManagerNew.h:68-125` (`NodeNew`), `196-219` (per-graph maps); `Linux/src/GraphManagerNew.cc:67-261` (accessors), `547-550` (insert-only `add_to_map`), `889-912`, `934-950`, `975-1002`, `1104-1109`
- **What it does:** A node holds:
  - `node_id`, the session-wide integer (S2.01);
  - `rep_id`, the key the node is filed under in its graph, e.g. `hsa5594`, `cpdC00076` or `c_s_hsa4149_hsa4609_c_e` (S2.07, S2.09);
  - `all_id`, the ordered list of member ids, written without KGML's colon (`hsa5923`, not `hsa:5923`);
  - `node_type`, the KGML entry type copied verbatim (`gene`, `compound`, `group`; the never-run naming-map step would add `protein`, S2.14);
  - `components`, the node ids of a complex's members (groups only);
  - `display_names`, an ordered list of labels (S2.15, S2.25);
  - `inlist` and `outlist`, edge ids in creation order;
  - `other_value1`, the source node id when `duplicate_node` copies a node, and `excess` (later stages);
  - `description`, `compartment` and `pathways`, which no stage-2 code fills. Nodes carry no pathway list; pathway membership lives on edges.

  Each graph also keeps:
  - `NIDSg` and `EIDSg`, its node and edge ids in creation order, which fix wgx's output order (S2.25);
  - two string maps, `rep_id_to_nid_map` and `id_to_rep_id_map`. Both are insert-only (`std::map::insert`, `GraphManagerNew.cc:547-550`, `889-893`, `909-912`): the first value stored for a key stays, and later writes are silently ignored. S2.03 describes the lookup built on them.

  The real run's re-read graph has 1134 nodes: 952 gene, 96 group and 86 compound (`run_console.log:236`).
- **Problems:** None in the record itself. One avoidable cost: every accessor returns its vector by value (`get_node_ids`, `get_edge_ids`, `get_outlist`, `get_all_ids_of_node`, …), so each call copies the whole list (see S2.23).
- **Proposed call:** keep. Port the record as described; the fields no stage fills carry no behaviour.
- **Decision:** keep (prompt log entry 033)

### S2.03 · Looking up a gene id (first claim wins) and the family-node trap

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:797-808` (`get_rep_id_from_id`), `876-887` (`get_nid_from_rep_id`), `909-912` (`add_id_rep_id`); registrations at `Linux/src/XMLParser.cc:974`, `979`, `1001`, `1024`, `1033`; stage 4–5 callers such as `GraphManagerNew.cc:4081` and `4107`
- **What it does:** A gene id becomes a node in two steps: `rep = id_to_rep_id_map[id]` (empty if absent), then `nid = rep_id_to_nid_map[rep]` (-1 if absent). Every later stage resolves its source, target, FSC, up/down and constraint-file ids this way. Both maps are insert-only, so an id held by several nodes resolves to whichever node registered it first. In a read that is the first relation, in file order, that reaches an entry containing the id (S2.06).
- **Problems:**
  - **The old master (S2.27):** 1796 ids sit in more than one entry, at most 14 (EGFR `hsa1956` is in 12). HRAS `hsa3265` is in four entries: 137 (the 6-gene RAS family), 279 (the classical trio), 371 (the trio plus MRAS) and 2010 (HRAS alone). The first relation to reach any of them is the 81st in file order, and it reaches entry 137, whose lexically smallest id is `hsa22800` (RRAS2). So `hsa3265` resolves to the node labelled RRAS2, and the classical-RAS node can never be chosen by id. This matches the earlier invalid RAS run, where forcing HRAS forced RRAS2's node and HRAS's own node stayed free; the file confirms that finding. PRKCA's first claim is the clean classical-PKC trio (entry 132), which is why the PKC run was valid.
  - **Raw-KGML merges:** every id ends up in exactly one node. That holds in all ten `merged_master_graph.xml` files written by the GSEA runs (five distinct lists of 42, 56, 71, 87 and 90 pathways) and in FuSION's own 163-pathway build (`runs/egf_hela/experiment/merged_kegg_master_graph_with_exp_score.xml`). Lookups are therefore unambiguous, but the node found can be a whole family (S2.17): HRAS resolves to a 6-gene node labelled RRAS2, and EGF (`hsa1950`) to a 52-gene ligand node.
  - Nothing tells the user which node an id bound to, or that the node holds other genes. pathz3's `Nodes to be active: N` counts nodes, not which ones.
  - Later outputs label a node with its key, which is its lexically smallest member (S2.07). In the 42-pathway merge, 127 of the 972 gene nodes have a key with no entry in the symbol map, so they print as bare ids (PI3K prints as `hsa110117499`).
- **Proposed call:** fix. Keep the id-to-node lookup, but report the node's members whenever an id belongs to several nodes or to a node with other members, instead of choosing silently.
- **Decision:** fix, as proposed (prompt log entry 033)

### S2.04 · Edge record

- **FuSION 1:** `Linux/include/GraphManagerNew.h:127-194` (`EdgeNew`); `Linux/src/GraphManagerNew.cc:263-489`, `676-694` (`check_if_edge_already_created`), `1836-1849`, `1967-1977`
- **What it does:** An edge holds:
  - a session-wide `edge_id`, and the source and target node ids;
  - `edge_type`, the KGML relation type copied verbatim (`PPrel`, `GErel`, `PCrel`, `ECrel`; the naming-map step would add `mapping`);
  - `edge_subtypes`, an ordered list after normalisation (S2.10), possibly including `dummy_u_to_d`;
  - `edge_belongs_to_pathways`, an ordered list of pathway ids such as `path:hsa04010`;
  - `weight_f`, the float `experiment_score` (default 0).

  Edges are directed, and an undirected KGML interaction is stored as two edges (S2.10). Two edges are the same edge when they share source, target, type and the exact ordered subtype list; the test scans the source's out-list (S2.10, S2.20).

  The constructor also zeroes a four-entry `weight_tuple`. It leaves `weight`, `capacity`, `flow`, `other1`–`other4` and `deleted` uninitialised, and stage 2 never reads them.

  **The 42-pathway merge** has 2529 edges: PPrel 2083, GErel 241, PCrel 180, ECrel 25.
  - Subtypes: activation 840, dummy_u_to_d 640, association 448, inhibition 408, component 404, phosphorylation 257, expression 240, compound 45, dephosphorylation 37, missing 26, dissociation 20, ubiquitination 15, repression 1, state 1.
  - `experiment_score` is 0 on 1966 edges and positive on 563.
- **Problems:**
  - 22 edges in the 42-pathway merge have no subtype at all (PPrel 13, PCrel 9, e.g. TNFRSF1A → TRADD from `path:hsa04010`), so they carry no sign; the old master has 162. How the solver signs edges is stage 5's question.
  - The duplicate test is a linear scan that copies the out-list and every candidate's subtype list (`GraphManagerNew.cc:676` carries the comment `HIGHLY INEFFICIENT CODE`).
- **Proposed call:** keep. Port the fields and the exact-list identity, initialising every member.
- **Decision:** keep; whether the never-read fields stay is settled with stages 4 and 5 (open question 7) (prompt log entry 033)

### S2.05 · `rgx` / `read_graph_xml`

- **FuSION 1:** `Linux/src/main.cc:1341-1391`; `Linux/src/GraphManagerNew.cc:19517-19548` (`read_graph_xml_file`); `Linux/src/XMLParser.cc:621-722` (`fill_graph_from_xml_file`)
- **What it does:** Batch form `rgx <file> <y|n> <thres>`.
  - The second argument is the tool-written flag: a first character of `y` or `Y` selects the tool-written read (S2.12), anything else the raw KGML read.
  - The third is the split threshold (S2.07), parsed with `stoi`; every batch passes 1.

  rgx allocates the next graph id and fills the graph (S2.06–S2.11). Unless the read is tool-written, it adds component edges (S2.09). It then records the genesis and prints:
  - `Creating graph...`
  - `Read graph: 84 from file /home/…/run/merged_master_graph.xml`
  - `Done!`
  - `New graph id: 84`
  - `Time taken (in sec): 0.00`

  (`run_console.log:229-233`). A file with no relations also prints `<file> has no edges`, and still yields a graph. Interactive mode prompts `Tool-written: (y/n): ` and then `Split node threshold: ` (S1.03). The batch files contain 77 rgx lines: 75 are `rgx $MERGED_PATHWAYS_XML_FILE y $NODE_SPLIT_THRESHOLD`, which re-reads wgx output (S2.26), and 2 are `rgx $XML_FILE n $NODE_SPLIT_THRESHOLD`.
- **Problems:**
  - A missing, unreadable or malformed file makes libxml++'s `parse_file` throw (`XMLParser.cc:627`). Nothing catches the exception, so the whole run aborts (S1.28).
  - The failure branch in `read_graph_xml_file` (`--graph_id_count; return -1`, `GraphManagerNew.cc:19543-19547`) can never run, because `fill_graph_from_xml_file` always returns true (`XMLParser.cc:721`). If it did run, the half-built graph would stay registered (`add_graph` at `19511`) under an id the next graph reuses.
  - A missing threshold token makes `stoi("")` throw, which aborts the run.
  - The help line calls the flag `<merge_flag>` (S1.12).
  - Each read allocates one `XMLParser` and never frees it (`GraphManagerNew.cc:19524`).
- **Proposed call:** fix. Keep the arguments and console lines, but report a missing or malformed file or argument as a command error instead of aborting.
- **Decision:** fix as proposed; under [decision 0021](../decisions/0021-required-and-optional-inputs.md) the file is checked before the run starts. The reader is KGML-only; other sources plug in through Phase 2's mechanism ([decision 0020](../decisions/0020-plug-and-play-inputs.md)). (prompt log entry 034)

### S2.06 · What is read from a KGML file

- **FuSION 1:** `Linux/src/XMLParser.cc:621-722` (`fill_graph_from_xml_file`), `85-102` (`get_component_list_for_xml_node`), `875-948`, `1451-1479`; `Linux/src/main.cc:954-973` (`split_string`)
- **What it does:**
  - **Root `<pathway>`:** only `name` is used. In a raw read it becomes the pathway id of every edge, e.g. `<pathway name="path:hsa04010" org="hsa" number="04010"` (`auxilliary/hsa_pathways_with_exp_score/hsa04010.xml:4`).
  - **`<entry>`:** `id` (parsed with `atoi`), `name` and `type`.
    - Every `:` is deleted from `name`, which is then split on single spaces into a sorted set of ids. So `name="hsa:5923 hsa:5924"` gives `hsa5923` and `hsa5924`, `cpd:C00338` gives `cpdC00338`, and a group's `name="undefined"` gives `undefined`.
    - `type` is kept verbatim.
    - `<graphics name>` is used only for display names (S2.07).
    - For `type="group"`, the `<component id>` children list the members, e.g. `<entry id="287" name="undefined" type="group">` with `<component id="139"/>` and `<component id="286"/>` (`hsa04010.xml:668-673`).
  - **`<relation>`:** `entry1`, `entry2`, `type`, `experiment_score`, `pathways` (tool-written reads only, S2.12) and each `<subtype>` child's `name`. The subtype's `value` is ignored. Real line: `<relation entry1="45" entry2="41" type="PPrel" experiment_score="625">` with `<subtype name="activation" value="--&gt;"/>` (`hsa04010.xml:693-695`).
  - **Everything else is ignored:** `org`, `number`, `title`, links, coordinates and `<reaction>` elements.

  Nodes are made only while the reader walks the relations: in file order, entry1 before entry2, then that relation's edges (`XMLParser.cc:666-714`). An entry that no relation reaches, directly or as a member of a reached group, never becomes a node. Under `ASSERT_FLAG` a file with no `<entry>` fails an assert (`637`).

  `experiment_score` is not a KEGG attribute. The external script `auxilliary/add_experiment_score.py` stamps it on each (pathway, entry1, entry2) from a STRING experimental-score table, and writes `0` when the table has no row. 275 relations in the 167 KGMLs have no such attribute: exactly the relations of the four added pathways in `updated_pathways/xml_files/` (hsa04216, hsa05224, hsa05225, hsa05226). They read as 0.

  **Measured over the 167 KGMLs:**
  - relation types are PPrel 7745, GErel 1295, PCrel 1293 and ECrel 54;
  - there is no `maplink` relation, so `map` and `ortholog` entries never become nodes;
  - no relation or component names a missing entry, no entry id repeats within a file, and no group contains a group.
- **Problems:** Each case below crashes or silently goes wrong on a bad file. No current file triggers any of them.
  - A relation that names a missing entry stores a null pointer (`file_id_node_map[...]`, `XMLParser.cc:675-676`), which is dereferenced at `881`.
  - A component that names a missing entry dereferences `end()` (`1084`).
  - A group with no components fails an `assert` (`99`).
  - A repeated entry id silently replaces the earlier entry (`646`).
  - A group-cycle check exists (`104-138`), but its call is commented out (`657-661`), so a cyclic group recurses until the stack overflows.
  - Two spaces in a `name` produce an empty id `""` (`main.cc:954-973`).
- **Proposed call:** fix. Keep the subset of KGML that is accepted, and reject a file with dangling references, repeated ids or cyclic groups with a message.
- **Decision:** fix as proposed; under [decision 0020](../decisions/0020-plug-and-play-inputs.md) this is one input reader among others (prompt log entry 034)

### S2.07 · From an entry to its node(s): keys and the split threshold

- **FuSION 1:** `Linux/src/XMLParser.cc:875-1077` (`process_node_with_merge_exception`, non-group branch); `Linux/src/GraphManagerNew.cc:8702-8713` (`get_rep_id_for_id`), `8768-8786` (`get_display_names_from_rep_id`)
- **What it does:** The rule below covers a non-group entry. S is its id set, sorted in string order, and E is the merge-exception set (S2.13).
  1. Each id in E becomes its own node, keyed by itself (`958-962`, `1000-1008`).
  2. An id with an entry in `all_id_map` is keyed by the mapped id. This never happens in practice (S2.14).
  3. The remaining ids collapse into one node keyed by r = min(S \ E) when |S| > thres, where |S| also counts the ids in E (`899-900`, `934-940`). Otherwise each becomes its own node.

  **Node creation and reuse:**
  - A key that already names a node in the graph reuses that node.
  - A new node gets as `all_id` the collapsed ids (or its single id), and the entry's `type`.
  - Its display name is the symbol-map entry for its key, or the key itself when there is none. It never comes from the KGML `<graphics>` name.
  - When a later entry resolves to an existing node, that entry's `<graphics name>` is split on `", "` and appended to the node's display names, skipping duplicates (`1059-1073`).
  - Every id is registered in `id_to_rep_id_map` against its key, and the first registration wins (S2.03).

  The entry stands for the set of nodes it produced, and its relations fan out over that set (S2.10).

  **In practice:** every batch uses thres = 1, so each multi-gene box becomes one node minus its exception ids. Real case: `hsa04010.xml` entry 21 lists ten DUSPs. In the real run, `hsa1843`, `hsa1846`, `hsa1847` and `hsa1848` are in E and become single nodes. The other six become one node keyed `hsa11221`, because `"hsa11221" < "hsa1844"` as strings. The 42-pathway merge writes that node as entry 98, `name="hsa11221 hsa1844 hsa1849 hsa1850 hsa1852 hsa80824"`, and DUSP1 as entry 99, `name="hsa1843"`.

  **Other thresholds:** thres = 0 behaves like 1. A negative thres never collapses anything, because the size comparison is unsigned.
- **Problems:**
  - The key is the smallest id in string order, which has no biological meaning. S2.03 describes its effect on labels and lookups.
  - Node identity depends on E, which holds this run's differentially expressed genes. The same pathway list therefore gives a different graph at every LFC threshold, and no merged graph can be reused across runs (S2.22).
  - No batch uses a threshold other than 1.
- **Proposed call:** keep. Port the exception-first, collapse-above-threshold rule, with 1 as the default; how families ought to merge is roadmap objective 2.
- **Decision:** keep (prompt log entry 034)

### S2.08 · Bug: a later box with an existing node's key loses its other genes

- **FuSION 1:** `Linux/src/XMLParser.cc:966-983` (ids registered against the key), `991-1073` (only a newly created node receives `all_id`; the existing-node branch adds display names only)
- **What it does:** A multi-gene box may collapse to a key that already names a node in the same pathway graph. Its other ids are then registered in `id_to_rep_id_map`, but they are not added to that node's `all_id`. The cross-pathway merge (S2.17) and wgx (S2.25) carry only `all_id`, so those genes vanish from the merged graph.
- **Problems:**
  - In the 167 KGMLs, 46 multi-gene boxes share their key with a box reached earlier in the same file but list other genes.
  - In the 42-pathway merge, 11 ids from non-indirect relations of the source KGMLs are in no merged entry:
    - `hsa861` (RUNX1), `hsa7252` (TSHB) and `hsa5345` (SERPINF2);
    - `hsa2894`, `hsa2895` and `hsa2897`–`hsa2901` (glutamate-receptor subunits);
    - `cpdC22540`.
  - Replaying the rule over the 42 KGMLs predicts 25 candidate ids, which include all 11. The other 14 survive only because some other box carries them.
  - Trace for RUNX1, in `hsa05220.xml`:
    - entry 16, {`hsa:2122`} (MECOM), is reached first, at relation position 12;
    - entry 5, {`hsa:2122 hsa:861`} (the AML1-EVI1 box), is reached at position 14. It keys to `hsa2122`, finds that node, and RUNX1 is dropped.
  - Ids in E are immune, because they always get a node of their own.
- **Proposed call:** fix. Add every id of a box to the node the box resolves to.
- **Decision:** fix, as proposed (prompt log entry 034)

### S2.09 · Groups: complex nodes and component edges

- **FuSION 1:** `Linux/src/XMLParser.cc:42-62` (`cartestian_product`), `1078-1125`; `Linux/src/GraphManagerNew.cc:2421-2436` (`get_node_id_with_components`), `2438-2466` (`concatenate_sorted_rep_ids`), `3981-4071` (`add_component_edges_for_complexes`), `19528-19531`
- **What it does:** Each component of a group entry is resolved to its set of nodes, recursively (S2.07). The group stands for one complex per combination in the Cartesian product of those sets.
  - A combination reuses an existing group node with exactly the same component node ids, found by scanning all nodes.
  - Otherwise a new node is made:
    - type `group`;
    - key `c_s_` + the components' keys, sorted and joined by `_`, + `_c_e` (one distinct key gives `c_s_<key>_c_e`);
    - display name `[` + the first display name of each component, joined by `+`, + `]`.
  - The key is also written into the manager's `all_id_map` (`XMLParser.cc:1112`).

  Real written entry (42-pathway merge, lines 163-167): `<entry id="54" name="c_s_hsa4149_hsa4609_c_e" type="group">`, `<graphics name="[MYC+MAX], [MAX+MYC]"/>`, `<component id="52"/>`, `<component id="53"/>`.

  After a raw read, every group node gets two PPrel edges per component, both with subtypes `component`, `dummy_u_to_d`: one from component to complex and one back. Each carries the pathways of the graph's first edge. The 42-pathway merge has 404 such edges.
- **Problems:**
  - The pathway list is copied from `*(get_edge_ids().begin())` (`GraphManagerNew.cc:3998`). That dereferences an empty vector when a file yields a group but no edges, for example when every relation is indirect and discarded (S2.11).
  - Both directions get `dummy_u_to_d`, because the key-order test is commented out (`4023`, `4057`). An association pair tags exactly one direction (S2.10). What the tag means is stage 5's.
  - Tool-written reads never add component edges (S2.12). Complexes read from a file without them, such as the old master, have no edges to their members (S2.27).
  - After merging, complexes collect keys and members. Of the 96 group entries in the 42-pathway merge:
    - 47 list two or more `c_s_…_c_e` keys (e.g. `c_s_hsa1794_hsa63916_c_e c_s_hsa1794_hsa9844_c_e`);
    - 12 list a component twice;
    - 12 have only one distinct component.
  - `get_node_id_with_components` scans every node for each lookup (comment `HIGHLY INEFFICIENT CODE`, `2421`).
- **Proposed call:** fix. Keep complexes and their two-way component edges, but give component edges the file's own pathway id instead of copying the first edge's list.
- **Decision:** fix as proposed; what `dummy_u_to_d` means when both directions carry it is settled with stage 5 (open question 5) (prompt log entry 035)

### S2.10 · From a relation to edges

- **FuSION 1:** `Linux/src/XMLParser.cc:682-713` (fan-out), `1451-1602` (`process_edge`), `1476-1478` (the subtypes that make an edge undirected)
- **What it does:**
  1. Subtype names are normalised in place, keeping their order: `indirect effect` → `indirect`, `missing interaction` → `missing`, `state change` → `state`, `binding/association` → `association`. All other names are kept verbatim (`1459-1479`).
  2. A relation carrying `indirect` is dropped at this point (S2.11).
  3. One edge is attempted for each (source node, target node) pair in the product of the two entries' node sets (S2.07, S2.09).
  4. In a raw read, an attempt is skipped when the graph already has an edge with the same source, target and type whose subtype list equals this one, or equals this one plus `dummy_u_to_d` (`1493-1501`). The skipped relation's `experiment_score` is lost.
  5. A new edge gets the type, the subtypes, and `experiment_score` parsed with `stof`, which is 0 when absent or malformed. In a raw read, the root `name` is its only pathway.
  6. In a raw read, an edge whose subtypes include `association`, `dissociation` or `mapping` is undirected (`1526-1531`, `1551-1601`):
     - the direction whose source key is larger than its target key, in string order, gets `dummy_u_to_d` appended;
     - a reverse edge is added under the same duplicate test, tagged when its source key is the larger;
     - so exactly one direction of each pair carries the tag.

     Real written pair member: `<relation entry1="18" entry2="19" type="PPrel" pathways="path:hsa04010 path:hsa04722" experiment_score="625">`, with subtypes `association` and `dummy_u_to_d`.
- **Problems:**
  - A relation between two entries with the same key makes a self-loop, which gets neither a tag nor a reverse edge. The 42-pathway merge has 18 self-loops, e.g. the MDM2/MDM4 node's activation edge (`path:hsa04115`) and the SKP1/CUL1/RBX1 node's association edges. Most of them come from merging (S2.17).
  - Which direction is tagged depends on key string order, and that order can differ between pathways for the same pair (S2.21).
- **Proposed call:** keep.
- **Decision:** keep, except that a skipped duplicate's `experiment_score` still counts: the larger score is kept, as the cross-pathway merge does (S2.20). This can change which edges pass stage 5's frozen-edge threshold. (prompt log entry 035)

### S2.11 · Indirect-effect relations are discarded (compile-time)

- **FuSION 1:** `Linux/src/XMLParser.cc:1481-1490`; `Linux/Makefile:10` (`-DDISCARD_INDIRECT_EDGES`, S1.29)
- **What it does:** Every relation whose normalised subtypes contain `indirect` is dropped. That covers both KEGG spellings, `indirect effect` and `indirect`, and holds whatever else the relation carries (e.g. `activation` + `indirect effect`). Raw and tool-written reads both apply it. Both endpoint entries are already nodes when the check runs (`XMLParser.cc:682-683`), so an entry whose only relations are indirect becomes a node with no edges.
- **Problems:**
  - The source comment says the decomposed cascade exists elsewhere in the merged graph, "so reachability is preserved (verified against the master graph)" (`1482-1486`). That is false.
    - In the old master, 620 indirect relations cover 598 ordered entry pairs. For 181 of those pairs, no path remains once indirect relations are removed (counting association and dissociation in both directions).
    - The other 417 pairs keep paths of length 1 to 9; the counts by length are 82, 103, 72, 67, 37, 28, 15, 10 and 3.
    - In the 42-pathway merge, 204 of the 2763 source relations (7.4 %) are discarded. For 48 of the 186 that touch no group, no directed path joins their endpoint nodes in the merged graph.
  - Sign information goes with them: 176 of those 204 relations also carry another subtype.
  - The discard leaves 22 isolated nodes in the 42-pathway merge. One is STAR `hsa6770`, whose only relation in `hsa04913.xml` is a GErel with `expression` + `indirect effect`. wgx writes these nodes, and the rgx re-read drops them (1156 → 1134 nodes, S2.26).
  - The rule is fixed at compile time, and no output records it (S1.29).
- **Proposed call:** fix. Make indirect handling a documented run option recorded in the output, and decide it before any endpoint node is created. Whether to keep indirect edges as "1+" hops is a roadmap objective 2 question.
- **Decision:** fix, with both behaviours as a configuration option recorded in the output and applied before nodes are made: discard indirect-effect relations (the default), or keep them as "1+" hops (roadmap objective 2's design). Old batches need not give the same results where 2.0 improves ([decision 0023](../decisions/0023-results-may-change-where-improved.md)). (prompt log entry 035)

### S2.12 · Tool-written read (`y`)

- **FuSION 1:** `Linux/src/XMLParser.cc:1493`, `1496`, `1527`, `1533-1544`, `1551`; `Linux/src/GraphManagerNew.cc:19528-19531`
- **What it does:** `y` tells rgx and mff that wgx wrote the file (S2.25). It changes four things:
  - there is no duplicate test, so every relation and every fan-out pair becomes an edge;
  - undirected subtypes get neither a reverse edge nor a `dummy_u_to_d` tag;
  - no component edges are added;
  - each edge's pathways come from the relation's `pathways` attribute, split on single spaces into a sorted set.

  Keys, the threshold, the exception set and the indirect discard all still apply. A run whose exception set differs from the writing run's therefore splits the file's family entries again.
- **Problems:**
  - The flag is trusted, not checked. A file that wgx did not write gets no component edges, and its associations stay one-way. That is what happens to the old master in 65 batch files (S2.27).
  - A raw KGML read with `y` gets edges with no pathway, because it has no `pathways` attribute.
  - Pathway lists are re-sorted on every round trip.
  - Byte-identical duplicate relations are kept; the old master has 18 extra copies.
- **Proposed call:** fix. Read FuSION's own merged-graph format with a reader that recognises it, instead of trusting a flag on the KGML reader. Only that format has `pathways` attributes and component edges.
- **Decision:** fix as proposed; old batches may still pass `y`, and how the old master is read is S2.27's question (prompt log entry 035)

### S2.13 · Merge-exception set (`hsa_ids_not_to_be_merged.txt` and the run's exception file)

- **FuSION 1:** `Linux/src/XMLParser.cc:922-932`, `958-962`, `1000-1008`; reader at `Linux/src/GraphManagerNew.cc:8072-8110` (S1.17); real batch line 153
- **What it does:** An id in the set never joins a box node. It becomes its own node, keyed by itself, and is never added to another node's `all_id`. The cross-pathway merge never consults the set (no reference in `GraphManagerNew.cc:9804-10850`). Exception ids stay single there anyway, because no multi-gene node contains them.

  The batch builds the file itself: `exec cat $UP_REG_FILE $DOWN_REG_FILE $SRC_FILE $TGT_FILE $HSA_IDS_NOT_TO_BE_MERGED_FILE > $EXCEPTION_TO_NODE_MERGE_FILE`. In the real run, `run/exception_file.txt` has 50 lines:
  - the 43 up-regulated and 2 down-regulated genes;
  - `hsa1956` (the source) and `hsa3576` (the target), so `hsa3576`, which is also up-regulated, appears twice;
  - the three lines of `inputs/hsa_ids_not_to_be_merged.txt`: `hsa1432` (MAPK14), `hsa5594` (MAPK1) and `hsa5599` (MAPK8).
- **Problems:**
  - The FSC is not added. An FSC that is not differentially expressed therefore lands inside its family node, which is the RAS trap (S2.03). The real run is clean only because JUN (`hsa3725`) happens to be up-regulated.
  - The set comes from this run's LFC thresholds, so a merged graph cannot be reused across experiments. Every run re-reads and re-merges all its KGMLs: 3 s for 42 pathways, about 124 s for 163 (S2.22).
  - An exception id that sits inside a box still collects the box's `<graphics>` aliases as display names. EGFR's written label begins `EGFR, CSF1R, BANDDOS, C-FMS, CD115, …`, because EGFR sits in a 25-gene receptor box whose graphics name lists CSF1R's aliases (S2.25).
- **Proposed call:** fix. Keep the set's meaning, and always include the FSC being tested in it.
- **Decision:** fix as proposed: the FSC being tested joins the set automatically, along with the up and down genes, source and target that S1.17 adds. Merging must therefore know them; Phase 2 designs how. (prompt log entry 036)

### S2.14 · The inter-database map in merging: `all_id_map` keys and mff's naming-map step

- **FuSION 1:**
  - `all_id_map` keys: `Linux/src/XMLParser.cc:964-986`, `1010-1036`, `1112`; `Linux/src/GraphManagerNew.cc:8702-8733`, `10058`, `10194`
  - naming-map step: `Linux/src/main.cc:2384-2400` (map-file list); `Linux/src/GraphManagerNew.cc:8307-8341` (`read_naming_map`), `10623-10771` (expansion)
- **What it does:** One file feeds two different mechanisms (S1.16).
  - **`all_id_map`, loaded by `start`:** during a read, a non-exception id with an entry there is keyed by the mapped id instead of the box key (S2.07). Every new complex key is also written into this map (`XMLParser.cc:1112`, `GraphManagerNew.cc:10058`, `10194`).
  - **mff's map files:** mff reads each map file it is given into a symmetric map, so a line `a b` gives a ↔ b. After merging, for every id of the merged graph found in a map it adds:
    - a node of type `protein` for the id's `all_id_map` key, and one for each of its partners;
    - two `mapping` edges between them, with subtype `mapping` and pathway `" "`.

    It repeats on the newly made nodes until the edge count stops changing, which always takes at least two passes. The real run prints `Edges before: 2529`, `Edges after: 2529` and `Edges after: 2529` (`run_console.log:212-214`).
- **Problems:**
  - Neither mechanism has ever run. All 17 copies of `inter_db_mapping_file` are empty, and every mff passes that file (`Read map with 0 entries`, `run_console.log:85`).
  - The keying path is inconsistent. The node is keyed by the mapped id, but the box's ids are registered under min(S \ E) (`XMLParser.cc:1021-1025`), which may name no node.
  - The expansion has four faults:
    - It looks keys up in `all_id_map`, not in the merged graph. For an id inside a family node it would therefore create a second, duplicate node (`GraphManagerNew.cc:10662-10684`).
    - Its edges have no provenance, which the edge report then dereferences as an empty set (`10812`, `10827`; S2.24).
    - It copies each map once per pass (`10650`).
    - A one-file mff skips it entirely.
  - Writing complex keys into `all_id_map` mixes generated keys into the user's map.
- **Proposed call:** drop. Neither use has ever run; if a synonym map is wanted, it is a new feature under roadmap objective 2.
- **Decision:** drop both uses. Mapping ids across sources is designed afresh in Phase 2 for the plug-and-play inputs of [decision 0020](../decisions/0020-plug-and-play-inputs.md), and may reuse S1.16's two-column file, whose reader stays. (prompt log entry 036)

### S2.15 · How stage 2 uses the symbol and pathway-name maps

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8735-8753` (`get_display_names_of_all_ids_of_node`), `8768-8786` (`get_display_names_from_rep_id`), `8788-8799` (`get_pathway_names_from_path_id`); `Linux/src/XMLParser.cc:1004`, `1052`
- **What it does:** Stage 2 uses the symbol map (S1.18) for labels only:
  - a new node's display name is its key's symbol, or the key itself when there is none;
  - an exception node's display name is its symbol, or empty when there is none;
  - the edge report (S2.24) names a node by the symbols of all its ids, joined by `,`, falling back to each id.

  Only the edge report uses the pathway-name map (S1.19); it prints `MAPK_signaling_pathway` for `path:hsa04010`. Neither map affects the graph's structure.
- **Problems:**
  - Two lookups use `operator[]` (`XMLParser.cc:1004`, `GraphManagerNew.cc:8747`), which inserts an empty name for every unknown id. The comment at `GraphManagerNew.cc:8771-8776` records that this once made a second read in one session segfault in the complex-label code (`XMLParser.cc:1105`). The fallback for empty names now hides it.
  - 291 of the 2175 gene ids in the 42-pathway merge have no symbol in the 6334-line map.
- **Proposed call:** fix. Use lookups that never insert, and keep the fallback to the id.
- **Decision:** fix, as proposed (prompt log entry 038)

### S2.16 · `mff` / `merge_from_file`

- **FuSION 1:** `Linux/src/main.cc:2330-2479`; `Linux/src/GraphManagerNew.cc:10499-10850` (`merge_graphs`)
- **What it does:** Batch form `mff <list_file> xml <thres> <y|n> [<map_file> …] -1`.
  - **Arguments:**
    - `<list_file>` holds one path per line. Lines are read with `getline` and not trimmed.
    - `xml` is the only accepted format. Anything else prints `Unknown format` and then `No graphs to merge`.
    - Each listed file is read as rgx would read it, with the given threshold and flag (S2.05). Each read prints `Starting to read files` and `Read graph: <id> from file <path>`.
    - Map files are read up to the `-1` token (S2.14).
  - **Two or more graphs:** `merge_graphs` folds them left to right (S2.17), runs the naming-map step (S2.14) and writes the edge report (S2.24). It prints ` merging graphs: <a> and <b>` for each step, then `New graph: 83` and `Total time taken (in sec): 3.00` (`run_console.log:87-216`).
  - **One graph:** it prints `Only one graph. Returning same.`, returns that graph, and skips the naming step and the report.
  - **Two forms in the batch files:**
    - 13 batches run `mff $GSEA_KGML_LIST xml $NODE_SPLIT_THRESHOLD n $INTER_DB_MAP_FILE -1` over raw KGMLs (or `$LIST_OF_MERGED_KEGG_XML_FILE` in place of the GSEA list);
    - 65 run `mff $NEW_LIST_OF_MERGED_XML_FILE xml $NODE_SPLIT_THRESHOLD y $INTER_DB_MAP_FILE -1` over a one-line list that names the old master. That form is just a read (S2.27).
- **Problems:**
  - The help line ends the arguments with `<inter_db_map> <graph-id>` (`main.cc:900`), but the code expects map files ended by `-1`. Without the `-1`, the loop walks a million empty tokens (S1.05).
  - A blank line or a bad path in the list aborts the run (S2.05). So does a list with CRLF line endings, because every path then ends in `\r`.
  - A one-file mff writes no `edges_merged_filename`. The batch's next command, `exec mv edges_merged_filename …`, then fails with `mv: cannot stat 'edges_merged_filename': No such file or directory` (`runs/egf_hela/experiment/egfr_to_il8_fsc_jun_b6_t700_f800_nr4_er4_on/run.log:46`). In a directory that still holds an earlier run's file, it moves that stale file instead.
- **Proposed call:** fix. Keep `mff <list> <thres> <y|n>` with the same meaning, drop the format token and the map-file list, and check every listed path before reading any.
- **Decision:** fix as proposed, with old lines still running: the new form is `mff <list> <thres> <y|n>`, and FuSION 1's `xml` token and map-file list are still accepted with a warning that they are ignored. Every listed path is checked before the run starts ([decision 0021](../decisions/0021-required-and-optional-inputs.md)). (prompt log entry 038)

### S2.17 · Merging nodes across pathways

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10572-10621` (the left fold in `merge_graphs`), `9804-10228` (`merge_two_graphs_without_splitting_nodes`, map and node phases), `9183-9284` (`duplicate_node`). The similar `merge_two_graphs` (`9699-9786`) is never called by a live command.
- **What it does:** mff merges the graphs in list order: G = merge(G1, G2), then merge(G, G3), and so on, each step building a brand-new graph. One step, merge(A, B), runs as follows. M is a string map whose lookups insert an empty value for unknown keys (`operator[]`).
  1. **Seed** (`9824-9838`): for every node n of A, in creation order, set M[id] = n.rep for each id of n, and M[n.rep] = n.rep.
  2. **Fold in B** (`9850-9903`): for each node n of B, let S be the union of the member lists of the B nodes that n's ids resolve to in B, and let r = M[min S], which is empty if unset.
     - If r is set, map to r every id in S, every id of A's node for r and every id of B's node for r, and set M[r] = r.
     - Otherwise map every id in S to itself.
  3. **Complex keys** (`9905-9976`): for each group node of A, then of B, compute k = `c_s_`, then each value in the sorted set of M[component key] followed by `_`, then `c_e`. Set M[group key] = k and M[k] = k.
  4. **Nodes** (`9984-10228`): for each node of A, then of B, and for each of its ids, follow M from the id until a key maps to itself; call the result r.
     - If the new graph has no node under r, a group gets a new complex node keyed r. Any other node is copied with `duplicate_node`, or an earlier copy of the same source node is reused. r is then registered for it (`add_rep_id_for_node`), which also overwrites the node's own `rep_id` field.
     - The id is then added to the node's `all_id` if missing, and the source node's display names are added if missing.
     - Before a node of B is copied, a type conflict with A aborts the step (S2.19).
  5. **Edges** follow; see S2.20.

  The effect is that boxes sharing any non-exception id are unioned, transitively and across all pathways. In every recorded raw merge, each id ends up in exactly one node (S2.03). After the rgx re-read (S2.26), a merged node is keyed by its lexically smallest member; the PI3K node, for example, is keyed `hsa110117499`.
- **Problems:**
  - **Over-merging.** A single box that pairs two families fuses them in every pathway.
    - The 42-pathway merge has a 19-gene node holding GNA12, GNA13 and every G-beta and G-gamma subunit, bridged by one KGML box, `hsa:2768 hsa:55970` (GNA12 + GNG12). The same merge also has:
      - a 52-gene growth-factor node with EGF, the FGFs, VEGFs and PDGFs, INS, IGF1/2 and HGF;
      - a 44-gene chemokine node and a 26-gene calcium-channel node;
      - a 6-gene RAS node (RRAS2, MRAS, HRAS, KRAS, NRAS, RRAS) and a 6-gene PKC node (PRKCA, PRKCB, PRKCG, PRKCD, PRKCQ, PRKCE).
    - The 87-pathway merge (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr040_keep`) has a 161-gene GPCR node spanning serotonin, dopamine, adrenergic, muscarinic and chemokine receptors. FuSION's own 163-pathway build has a 143-gene one.
    - An explanation that runs through such a node does not say which member acted.
  - **The result depends on list order.** Step 2 resets ids to themselves when a box's smallest id is new. `duplicate_node` copies all of a source node's ids whatever M says. Together they let an id sit in two nodes until a later step re-joins them. The crash in S2.18 appeared for one order of six pathways and not for another.
  - **It is hard to reproduce exactly.** A node's `rep_id` field is overwritten once per member. The A pass finds nodes through `get_rep_id_from_id`, while the B pass uses the key directly (`10007` against `10144`).
  - **Cost:** see S2.22.
  - Correction to an earlier finding: the "harsh failure mode" previously cited at `9744-9751` is in the dead `merge_two_graphs`. The live check is at `10097-10115` (S2.19).
- **Proposed call:** fix. Port the merge as an explicit, order-independent union of boxes that share a non-exception id, which gives the measured result without the chain-following, and list every merged node's members in the output. Whether families should be unioned at all is roadmap objective 2.
- **Decision:** fix as proposed: an explicit, order-independent union in one pass, with every merged node's members listed in the output. Whether families should be unioned at all is revisited under roadmap objective 2. (prompt log entry 038)

### S2.18 · Bug: the merge crashes on chained keys (fixed only in the working tree)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9788-9802` (`final_rep_id`), `10249-10250`, `10354-10355`; uncommitted (`git diff HEAD -- Linux/src/GraphManagerNew.cc`); the macOS copy is identical
- **What it does:** M chains (A → B → C) when pathways box overlapping families under different keys. The node phase follows a chain to its end. The committed edge phase followed only one step, so it could land on a key the merged graph never registered, get node id -1 and fail `assert(new_source_nid != -1)`. The working tree adds `final_rep_id`, which follows the chain the same way the node phase does, and uses it in both edge loops. The earlier session that wrote the fix reported:
  - a minimal crashing list: hsa04010, hsa04012, hsa04014, hsa04015, hsa04020, then hsa04024;
  - wgx output byte-identical to the committed code wherever that did not crash: 2350 entries and 7189 relations for all 163 pathways, 1156 and 2529 for the FDR 0.25 list.
- **Problems:**
  - The fix exists only as an uncommitted change. A build from the committed tree crashes on some pathway lists, depending on which pathways are listed and in what order.
  - `final_rep_id` uses `operator[]` and returns `""` for an unknown id instead of failing.
  - The complex-key phase and the complex-member lookup still follow one step (`9928`, `9964`, `10043`, `10179`). That cannot crash, but it can key a complex by a stale member key. Not measured.
- **Proposed call:** keep. Port the fixed behaviour, in which edge endpoints resolve through the whole chain.
- **Decision:** keep the fixed behaviour; with S2.17's explicit union there are no chains to follow (prompt log entry 038)

### S2.19 · A node-type conflict drops a whole pathway

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10097-10115`; `merge_graphs` at `10614-10617`
- **What it does:** Before copying each node of B, the step looks up A's node for the same key. If that node exists and its type differs, the step:
  - prints `Warning: Node types do not match for node <key>. Not merging graph <id>.`;
  - deletes the partly built new graph and takes back its graph id;
  - returns A unchanged.

  mff carries on with the next file. The skipped pathway is missing from the result and from the genesis list.
- **Problems:** One conflicting entry silently removes every node and edge of a pathway, with nothing more than a warning on stderr. This has never fired in a recorded run: no log under `runs/` or `auxilliary/` contains the warning, because node keys of different KGML types never coincide in these files.
- **Proposed call:** fix. Treat a type conflict as an error that names the node and both pathways.
- **Decision:** fix as proposed; the run stops at it (S1.28) (prompt log entry 039)

### S2.20 · Merging edges across pathways

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10230-10432`; `676-694`
- **What it does:** Each edge of A, then of B, is mapped into the new graph by resolving both endpoint keys through M to the end of the chain (S2.18). It then matches an existing edge with the same endpoints, the same type and the same ordered subtype list, or that list plus `dummy_u_to_d`.
  - **On a match:** the incoming edge's pathways that are not yet listed are appended, `experiment_score` becomes the larger of the two, and the incoming edge's provenance (the per-pathway edge ids it came from) is added.
  - **Otherwise:** a new edge copies type, subtypes, pathways and score.

  Real merged edge: `<relation entry1="18" entry2="19" type="PPrel" pathways="path:hsa04010 path:hsa04722" experiment_score="625">`.

  **Measured on the 42-pathway merge:** 2529 edges on 2370 ordered node pairs, of which 140 pairs carry more than one edge (at most 3):
  - 66 where one subtype set contains another; the RAF node (BRAF, RAF1, ARAF) → the MEK node (MAP2K1, MAP2K2) carries `phosphorylation`, `activation` + `phosphorylation` and `activation` as three edges;
  - 30 where the types differ;
  - 9 where one edge has no subtype;
  - 6 that differ only by `dummy_u_to_d` (S2.21);
  - 40 others.

  4 pairs carry both an `activation` or `expression` edge and an `inhibition` or `repression` edge, e.g. NKD1/NKD2 → DVL1–3 with `activation` and `inhibition`, both from `path:hsa04310`. There are 18 self-loops (S2.10). The old master, merged by a different tool, has 417 multi-edge pairs (at most 18 edges) and 24 polarity pairs by this definition; an earlier note reported 28 using a broader one.
- **Problems:** None in the rule itself. What it produces reaches the solver: parallel edges, polarity conflicts and self-loops, where each parallel edge is one more edge that can be relaxed. That is stage 5's concern. The duplicate test is linear (S2.04).
- **Proposed call:** keep. Port the exact-list matching, with pathways unioned and the maximum score.
- **Decision:** keep (prompt log entry 039)

### S2.21 · Bug: one undirected interaction can survive both with and without the dummy tag

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10267-10275`, `10371-10379`; the same pattern in `Linux/src/XMLParser.cc:1493-1501` and `1554-1561`
- **What it does:** The duplicate test accepts an existing edge whose subtype list is the incoming one plus `dummy_u_to_d`. It does not accept the reverse: an incoming tagged edge never matches an existing untagged one.
- **Problems:** Within each pathway, key string order decides which direction is tagged (S2.10). The same merged node pair can have different keys in different pathways, so one association can survive as two parallel edges in the same direction, one tagged and one not. The 42-pathway merge has 6 such ordered pairs. One is the 24-gene receptor-tyrosine-kinase node (entry 19) → GRB2 (entry 18): `association` from `path:hsa04010`, and `association`, `dummy_u_to_d` from `path:hsa04722`.
- **Proposed call:** fix. Compare subtype lists with `dummy_u_to_d` removed, and compute the tag once, on the merged graph.
- **Decision:** fix, as proposed (prompt log entry 039)

### S2.22 · Cost of merging

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10572-10621` (the fold), `9850-9903` (calls to `get_all_ids_for_rep_id` at `9858`, `9876` and `9887`), `934-950`, `9183-9284`, `676-694`, `2421-2436`
- **What it does:** Merging k pathways takes k − 1 steps. Each step copies the whole graph merged so far, plus the next pathway, into a new graph (S2.01). Within a step, each id of B leads to up to three calls of `get_all_ids_for_rep_id`, and each call scans every node of a graph. Every edge copy also runs the linear duplicate test (S2.04).
- **Problems:** Time grows much faster than the pathway count. mff's `Total time taken` in the recorded logs:

  | Pathways | Time (s) |
  |---|---|
  | 42 | 3 |
  | 56 | 11 |
  | 71 | 17 |
  | 87 | 53 |
  | 90 | 24 |
  | 163 | 122–126 |

  The logs are `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr0{25,30,40,50}/run_console.log`, `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr040_keep/run_console.log`, and `runs/egf_hela/experiment/graph_build.log` and `build_graph_egfr.log`; the two 163-pathway logs predate the indirect discard. The whole merge is repeated for every run, because the exception set changes the result (S2.13). The build is also unoptimised (S1.29).
- **Proposed call:** fix. Build the same merged graph in a single pass over all pathways, with hashed indexes from id to node and from edge key to edge, instead of k − 1 full copies and linear scans.
- **Decision:** fix as proposed, and reuse merges: each pathway list is merged once without the run's exceptions, and the exception ids are split out afterwards, so runs on the same pathways share one merge. Phase 3 must show that the split gives the same graph as merging with the exceptions. (prompt log entry 039)

### S2.23 · Dead work inside the live paths

- **FuSION 1:**
  - occurrence counts: `Linux/src/GraphManagerNew.cc:10533-10568`, `10582-10612`, `10791-10792`
  - `MAPK_merged_ids`: `Linux/src/main.cc:1374-1375`, `2365-2373`; `Linux/src/XMLParser.cc:928-930`
  - `duplicate_node` display names: `Linux/src/GraphManagerNew.cc:9252-9262`
  - `num_edges` and `count`: `Linux/src/XMLParser.cc:699-711`
- **What it does:** Four computations whose results nothing uses:
  - `merge_graphs` builds `node_repid_to_occurrence_map` and `edge_repstr_to_occurrence_map` for every graph, constructing a key string per edge, and names two output files it never writes.
  - `MAPK_merged_ids` is always empty, so its search for each id, and the map it fills, do nothing.
  - `duplicate_node` adds each display name cut at its last comma, then replaces the whole list with the uncut names.
  - After every edge, `fill_graph_from_xml_file` copies the graph's entire edge-id vector just to store its size in an unused variable.
- **Problems:** These cost time and produce nothing. The edge-id copy is quadratic in a file's edge count: one read of the old master's 7400 edges makes about 27 million integer copies.
- **Proposed call:** drop.
- **Decision:** drop (prompt log entry 040)

### S2.24 · The merged-edge report `edges_merged_filename`

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:10798-10845`; real batch line 176, `exec mv edges_merged_filename $WRITE_DIR/merged_edges_file`
- **What it does:** After merging two or more graphs, mff writes a tab-separated file called `edges_merged_filename` into the launch directory. Each merged edge, in edge creation order, gets one line with at least seven fields:
  1. the merged edge's source, subtypes and target;
  2. the source, subtypes and target of the first per-pathway edge it came from;
  3. the names of that edge's pathways.

  Each further edge it came from gets a line of its own, whose first three fields are empty.
  - Nodes are named by the symbols of all their ids joined by `,`, falling back to each id.
  - Subtypes are joined by `,`, or written as `-` when there are none.
  - Pathways are named through the pathway-name map.

  Real lines (`run/merged_edges_file:5-6`):
  - `RAP1A,RAP1B<TAB>activation<TAB>BRAF,RAF1,ARAF<TAB>RAP1A,RAP1B<TAB>activation<TAB>BRAF<TAB>MAPK_signaling_pathway`
  - `<TAB><TAB><TAB>RAP1A,RAP1B<TAB>activation<TAB>BRAF<TAB>Neurotrophin_signaling_pathway`

  The real file has 3585 lines for 2529 edges, 1056 of them continuation lines.
- **Problems:**
  - The fixed name in the launch directory is not covered by upfx, so concurrent runs in one directory overwrite each other's report (S1.26).
  - A one-file mff writes nothing, and the batch's `mv` then fails or moves a stale file (S2.16).
  - Nothing reads the file. Apart from its writer, the only reference is the GUI's `mv` line (`Linux/frontend/src/gui4.java:4326`).
  - Every name is built with `operator[]` lookups of every member id (S2.15).
- **Proposed call:** fix. Keep it as an optional provenance report, written to a path given to the command whenever it is asked for.
- **Decision:** fix, as proposed (prompt log entry 040)

### S2.25 · `wgx` / `write_graph_xml` and the merged-graph XML format

- **FuSION 1:** `Linux/src/main.cc:2585-2627`; `Linux/src/GraphManagerNew.cc:7895-7938` (`write_to_xml_file`), `2892-2918` (`write_node_to_xml_file`), `2920-2953` (`write_edge_to_xml_file`)
- **What it does:** Batch form `wgx <graph_id|0> <file> <pathway_tag>`; every batch uses `wgx 0 <file> KEGG`. The file starts with `<?xml version="1.0"?>`, then `<!DOCTYPE pathway SYSTEM "http://www.kegg.jp/kegg/xml/KGML_v0.7.1_.dtd">`, then `<pathway name="KEGG">`, so the tag becomes the root name.
  - **Nodes** become entries in creation order, numbered from 1.
    - `name` is the node's `all_id` joined by spaces, in list order.
    - `type` is the node type.
    - `<graphics name>` holds the display names joined by `", "`.
    - A complex lists `<component id>` children by their new numbers.

    Real entry: `<entry id="4" name="hsa22800 hsa22808 hsa3265 hsa3845 hsa4893 hsa6237" type="gene">`, then `<graphics name="RRAS2, NS12, TC21..., HRAS, C-BAS/HAS, …"/>`.
  - **Edges** become relations in creation order.
    - Attributes: `entry1`, `entry2`, `type`, `pathways` (joined by spaces) and `experiment_score`. The score is written as an integer when it is whole, and in default float format otherwise.
    - One `<subtype name="…"/>` child is written per subtype.

    Real relation: `<relation entry1="1" entry2="2" type="PPrel" pathways="path:hsa04010" experiment_score="0">`, then `<subtype name="activation"/>`.

  wgx prints `1156 nodes written to xml file` and `2529 edges written to xml file`. A bad graph id prints `Graph id <n> is not present`, and a path it cannot write prints `Error: couldn't open file <path>`. In interactive mode it prompts for the file and for the pathway name.
- **Problems:**
  - Nothing is XML-escaped. No current name contains `&`, `<` or `"` (checked), so this is latent.
  - The labels mix in other genes' aliases, because entries append their `<graphics>` names (S2.07). EGFR's entry 20 reads `EGFR, CSF1R, BANDDOS, C-FMS, …`, and JUN's ends `FOS, C-FOS, p55..., p39...`. The rgx re-read rebuilds labels from the symbol map, so only someone reading the XML sees this.
  - `name` keeps list order, not sorted order. 28 gene entries in the 42-pathway merge do not list their smallest id first, so their key changes on the re-read (S2.26).
  - Isolated nodes are written and then dropped by the re-read (S2.11).
  - The DOCTYPE claims KGML 0.7.1, but the file is not KGML: it has `pathways` attributes, component edges and `dummy_u_to_d` subtypes.
  - A node with no ids fails an `assert` (`2896`).
- **Proposed call:** fix. Keep the format, which is what the `y` read consumes, but escape attribute values and write display names from the symbol map.
- **Decision:** fix as proposed, and mark the file as FuSION's merged-graph format rather than KGML, so that S2.12's reader can recognise it (prompt log entry 040)

### S2.26 · The `mff` → `wgx` → `rgx` round trip

- **FuSION 1:** real batch lines 171-183 (mff, size, wgx, `exec mv`, rgx, size); `Linux/output_script.txt:149-202` (a GUI-written script)
- **What it does:** Every batch that merges writes the merged graph and reads it straight back: 75 batches run `wgx 0 $MERGED_PATHWAYS_XML_FILE KEGG`, then `rgx $MERGED_PATHWAYS_XML_FILE y $NODE_SPLIT_THRESHOLD`. Later stages run on the re-read graph (`#LASTGRAPHNUM`, S1.09).

  The GUI's script does two round trips, in this order:
  1. `mff` over the raw KGMLs;
  2. `wgx` the master;
  3. `exec add` for the user's extra edges;
  4. `mff … y` over the master plus the extra-edges file;
  5. `wgx` again;
  6. `rgx … y`.

  In the real run, the re-read:
  - drops the 22 isolated nodes, so nodes go from 1156 to 1134 while edges stay at 2529 (`run_console.log:219-237`);
  - re-keys every node by the smallest id of its written `name`;
  - sorts each edge's pathway list;
  - rebuilds display names from the symbol map;
  - applies the exception set and the threshold again;
  - gives the graph a new id, 84 instead of 83.
- **Problems:**
  - It is a file round trip between two steps of one run: 539688 bytes are written and parsed again in the real run.
  - In the 65 old-master batches, the master is parsed twice: once by the one-file `mff … y`, once by `rgx … y`.
  - The round trip is also an undocumented normalisation step: afterwards the keys, the id map and the labels agree with each other. Whether later stages would give the same answers on graph 83 is unknown (open question 1).
- **Proposed call:** fix. Keep writing the merged graph as an artefact, but run the later stages on the in-memory graph after applying the same normalisation, instead of re-parsing it.
- **Decision:** fix as proposed; the written file also serves as a saved step for partial runs ([decision 0019](../decisions/0019-partial-runs.md)) (prompt log entry 040)

### S2.27 · The pre-merged master read by 65 batches

- **FuSION 1:** no FuSION 1 code. The file `runs/real_experiments/merged_kegg_master_graph_with_exp_score.xml` and its six identical copies (five in run `inputs/` folders, one in `runs/egf_hela/`) are produced by `auxilliary/merge_master_graph.py` (in a gitignored directory), and read through S2.16 and S2.12.
- **What it does:** 65 of the 78 batch files that call mff never merge KGMLs. They read this file with a one-file `mff … y`. Among them are the published b10 runs, such as `runs/egfr_cxcl8_jun_b10_nr8_er8`.
  - **How the file was made:** the script collapses entries across the 163 pathways by their exact (type, name) string, joins relation pathways and takes the maximum `experiment_score`. It keeps KEGG's raw subtype spellings (`binding/association`, `indirect effect`) and adds no component edges and no `dummy_u_to_d` tags.
  - **Contents:** 5307 entries (gene 3596, group 598, compound 481, ortholog 396, map 236) and 7517 relations.
  - **Read result:** with `y` and threshold 1 it gives 3161 nodes, 259 of them isolated, and 7400 edges (`runs/egfr_cxcl8_jun_b10_nr8_er8/run.log:38-39`).

  The same file name also belongs to a different file: `runs/egf_hela/experiment/merged_kegg_master_graph_with_exp_score.xml`, which `build_graph.batch` wrote with FuSION's own mff and wgx (2391 entries).
- **Problems:**
  - Because the read is tool-written, the 598 groups get no edges to their members and associations stay one-way (S2.12). The raw pipeline has both, so results from the two pipelines are not comparable.
  - 1796 ids sit in more than one entry (at most 14), and lookups take the first one (S2.03; the RAS trap). 1716 entries are in no relation. 18 byte-identical duplicate relations and 162 relations without a subtype survive.
  - The earlier findings this stage was given (HRAS in 4 nodes, 417 multi-edge pairs, 162 untyped relations) describe this file, not current mff output. Current raw merges place every id in exactly one node (S2.03, S2.17).
  - Its generator lives outside the repo's tracked code, and two different files share its name.
- **Proposed call:** drop. FuSION 2.0 builds its merged graph from raw KGMLs; whether to keep reading this file in order to reproduce published runs is open question 2.
- **Decision:** keep, as a compatibility reader. 2.0 can read this one file the way FuSION 1 does (S2.12's tool-written read: first-claim lookups, no component edges, one-way associations), kept apart from the raw-KGML pipeline and flagged as legacy in the output. The published runs can then be re-run in 2.0, and Phase 3 can compare them like for like. This settles open question 2. (prompt log entry 042)

### S2.28 · `sz` / `size`

- **FuSION 1:** `Linux/src/main.cc:1581-1643`; `Linux/src/GraphManagerNew.cc:4266-4277` (`is_isolated_node`)
- **What it does:** Batch form `sz <graph_id|0>`, where `0` means the latest graph. It prints two lines:
  - `Number of nodes: <n> (<g> genes, <c> complexes, <p> compounds, <o> others) <i> isolated`;
  - `Number of edges: <e>`.

  Genes, complexes and compounds count the node types `gene`, `group` and `compound`, others counts every other type, and isolated counts nodes with no edges in or out. Real output: `Number of nodes: 1156 (972 genes, 96 complexes, 88 compounds, 0 others) 22 isolated`, then `Number of edges: 2529` (`run_console.log:219-220`). Under `DEBUG_FLAG` the same counts, minus the isolated count, also go to the debug log (S1.25). An unknown id prints `No such graph` on stderr.
- **Problems:** A missing or non-numeric argument throws from `stoi` and aborts the run. Otherwise none found.
- **Proposed call:** keep.
- **Decision:** keep, and also print how many nodes hold more than one gene, and the largest such node (prompt log entry 042)

### S2.29 · `upfx` / `unique_prefix`

- **FuSION 1:** `Linux/src/main.cc:1331-1340`; `Linux/src/GraphManagerNew.cc:8187-8190`; read at `main.cc:4851` and `4855`
- **What it does:** `upfx <prefix>` closes the debug log and reopens it, truncated, as `<prefix>_mylog.txt` in the launch directory. It also stores the prefix in the graph manager, and pathz3 later puts the prefix on two of its outputs, `limits_timefile.txt` and `PO.dat` (stage 5). Nothing in stage 2 uses it. A source comment says it was added to stop concurrent runs racing on fixed file names.
- **Problems:**
  - No batch file or GUI emits it.
  - It covers only three of the fixed-name outputs; `edges_merged_filename`, `fb_rch`, `fb_rch_back` and the SMT dump directories stay unprefixed (S1.26).
  - Before `start` it dereferences a null graph manager (S1.15).
  - In interactive mode it reads no argument, so the prefix is empty (S1.03).
- **Proposed call:** drop. Nothing uses it, and it does not prevent the race it was written for.
- **Decision:** drop; S1.25's per-run log name and S1.26's per-run output directory cover its purpose (prompt log entry 042)

### S2.30 · `add`: plaintext edge list to KGML-style XML

- **FuSION 1:** `Linux/src/main.cc:5842-5934`; the original standalone tool is `macOS/add.cpp`, and a prebuilt `Linux/add` sits next to the binary
- **What it does:** `add <edges_file> <out_xml>` reads lines of the form `src_id tgt_id edge_type edge_subtype`, separated by whitespace. Lines with fewer than four tokens are skipped, and extra tokens are ignored. Two warnings are printed:
  - `Adding an edge of type <t> (different from "PPRel")` for every type other than `PPRel`;
  - `Adding an edge of subtype <s> (different from "inhibition" or "activation")` for every other subtype.

  Ids are numbered from 0 in order of first appearance. The output looks like this (reconstructed from the code):

  ```
  <?xml version="1.0"?>
  <!DOCTYPE pathway SYSTEM "Additional edges added">
  <pathway name="KEGG">
  	<entry id="0" name="hsa1956" type="gene">
  	  <graphics name="hsa1956"/>
  	</entry>
  	<relation entry1="0" entry2="1" type="PPrel" pathways="KEGG">
  	 <subtype name="activation"/>
  	</relation>
  </pathway>
  ```

  Entries are written in string order of id. Each entry has type `gene` and its own id as label, and there is no `experiment_score`. The `pathways` attribute marks the file as meant for a `y` read: the GUI's script merges it with the master through `mff … y` (S2.26). No batch file uses the in-binary `add`, which is why the output above is reconstructed rather than quoted. The GUI script calls the standalone tool instead, through `exec add …` (`Linux/output_script.txt:164`).
- **Problems:**
  - The type check compares against `PPRel`, but KEGG and every downstream comparison spell it `PPrel`. A correct `PPrel` line therefore triggers the warning, and a `PPRel` line is written verbatim as a type no other code recognises.
  - Each edge can have only one subtype; `activation,phosphorylation` would be written as a single unknown subtype name.
  - A file it cannot open only prints an error, and nothing is XML-escaped.
  - The standalone tool does not skip a short line. It re-adds the previous line's edge instead, so a trailing blank line duplicates the last edge.
- **Proposed call:** fix. Keep the command and its output format, but check types against KEGG's spelling and reject malformed lines with a message.
- **Decision:** fix as proposed. A plain edge list like this is also a candidate exchange format for the plug-and-play inputs of [decision 0020](../decisions/0020-plug-and-play-inputs.md), in Phase 2. (prompt log entry 044)

## Open questions

1. **Would the later stages give the same results on the in-memory merged graph as on the re-read one (S2.26)?** The re-read normalises keys, the id map and labels, and drops isolated nodes, but nothing shows whether fb_rch and pathz3 depend on that. *Would settle it:* in phase 3 reference testing, run fb_rch and pathz3 on graph 83 and on graph 84 of the real run and diff their outputs.
   *Still open, for phase 3.*
2. **Must FuSION 2.0 reproduce the published runs that read the old master (S2.27)?** Those runs depend on first-claim lookups (S2.03), complexes without component edges and one-way associations, none of which the raw pipeline has. *Would settle it:* the user's choice. If yes, FuSION 2.0 needs a reader that reproduces S2.12's behaviour for that one file.
   *Resolved:* yes, through a compatibility reader kept apart from the raw pipeline (S2.27).
3. **Was the transitive union of boxes intended (S2.17)?** The function's name, `merge_two_graphs_without_splitting_nodes`, suggests that merging overlapping boxes was deliberate. The 52-, 143- and 161-gene nodes suggest its reach was never checked. *Would settle it:* the user, or FuSION 1's authors, as part of roadmap objective 2.
   *Resolved for now:* the union is kept as an explicit, order-independent union (S2.17); whether families should be unioned at all is revisited under roadmap objective 2.
4. **Does every raw merge end with each id in exactly one node (S2.03, S2.17)?** It holds in every recorded raw merge, but the code does not guarantee it (step 2 resets ids, and `duplicate_node` copies all of a node's ids). *Would settle it:* a phase 3 property test that merges random pathway lists in random orders with FuSION 1 and checks the written XML.
   *Still open, for phase 3.* With S2.17's explicit union, 2.0 guarantees it by construction.
5. **What does `dummy_u_to_d` mean on component edges, where both directions carry it (S2.09), compared with one direction on association pairs (S2.10)?** *Would settle it:* stage 5's account of how the solver uses the tag.
   *Still open, for stage 5.*
6. **Is the `final_rep_id` fix really output-preserving (S2.18)?** The byte-identical comparison comes from the session that wrote the fix; I could not rerun it. *Would settle it:* build the committed and working trees in phase 3 and diff wgx output for the 163- and 42-pathway lists.
   *Still open, for phase 3.*
7. **Do any later stages read the edge fields the constructor leaves uninitialised (`weight`, `capacity`, `flow`, `other1`–`other4`, `deleted`; S2.04)?** *Would settle it:* stages 4 and 5.
   *Still open, for stages 4 and 5.*
