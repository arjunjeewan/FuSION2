# 03 · Merge

## Scope

This section designs the `merge` module (Choice C1). It covers:

- the explicit, order-independent union of pathway files into one network, with every merged node's members listed (S2.17, S2.18, S2.08, S2.09);
- edge merging across pathways, undirected interactions and the maximum score (S2.20, S2.21, S2.10), as the merge drives section 02's builder;
- node-type conflicts, which stop the run (S2.19);
- the merge-exception set and how the merge learns it: the up and down genes, the confirmed genes, the source, the target, the FSC, the not-merged file and FuSION 1's `start` file (S1.17, S2.13, S8.22);
- merge reuse: each pathway list merged once without exceptions, the exception ids split out afterwards, and runs on the same pathways sharing one merge (S2.22, S8.23), with the keys and the store's entries;
- the `mff`/`merge_from_file` step (S2.16), whose output later steps use in memory (S2.26), and the optional provenance report (S2.24).

It leaves to other sections:

- **02:** the network model and its builder, which merges edges on identity, sets the tag and sorts (02.C10); the builder calls and header fields the merge needs (cross-section item 2B.11); the readers and their documents (`PathwayDoc`); the relation filters (S2.11, S2.31); the S2.07 rule for one entry (`split_entry`); the list reader with `kgml_dir` (02.C12); `sniff`; FuSION's network file with embedded sources (02.C4) and `add`'s marked output (02.C17); the legacy reader (02.C5); labels, pathway names, the id rule (02.C3) and the binding report (S2.03).
- **01:** the command table and the `mff` handler (01.C9); named arguments; the look-ahead facts that default `mff`'s source, target, FSC and confirmed lists (01.C17); latest-value defaults (01.C16); sweeps; the check before the run, with its note `batch.no_fsc` for a batch that names no FSC (2A.9); `load` (01.C6); and FuSION 1's move of `edges_merged_filename` (01.C11).
- **04:** the observations, that is the up and down genes after the Not DE override (04.C4).
- **09:** the core vocabulary (cross-section item 1.1); the run directory and the `mff` step's directory (09.C2); `run.json`; the events envelope (1.3); the settings registry (1.16); the shared store's root, publishing and the bypass for unclean builds (09.C13); and the rule that the store keys on rules versions while resume fingerprints on the build id (3A.3).
- **12:** the harnesses that run the hooks listed under Test hooks, among them the independent oracle `exception_merge` (12.C14).

Names used below:

- "the reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/` (EGFR → CXCL8, FSC JUN, 42 pathways);
- the lists are named by their pathway count, and all are `gsea_kgml_files.txt` files of FuSION 1 runs:
  - 42: the reference run;
  - 56 and 71: `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr030` and `fdr040`;
  - 87: `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr040_keep`, and `runs/egfr_b10_gsea_fdr044_candidates/r0_cxcl8_jun` with its `_string125` rerun;
  - 88: two lists, that of `r7` to `r16` (as in `r9_epha2_mek_avoid`) and that of `r17` to `r26`;
  - 90: `fdr_sweep/fdr050`;
- "the 167 KGMLs" are the reference run's `inputs/list_of_kegg_xml_files.txt`, and "the 163 KGMLs" all of `auxilliary/hsa_pathways_with_exp_score/`;
- E is the exception set; a **box** is an entry that names ids and is not a group; a **group** is a group entry; a **kept relation** is one that section 02's filters keep;
- "the prototype" is a Python 3.12 re-implementation of this design, run on the real KGMLs on this machine (Xeon Gold 6140, 2.30 GHz) to measure sizes and costs and to check the split. It is not part of either repository. FuSION 1 was not run; its numbers come from its logs and written files.

## Requirements carried in

**The exception set**

- S1.17: the first-token-per-line reader stays. 2.0 adds `cudf`'s up and down genes and the source and target to the set itself; old batches that still build the file keep working.
- S2.13: the FSC being tested joins the set automatically, with S1.17's genes. Merging must know them.
- S8.22: the backend builds the set from `cudf`'s lists, the source, the target, the FSC and the not-merged file; the frontend supplies only those inputs. 03.C2 adds the confirmed lists (accepted in prompt log entry 165).
- S2.07: an id in the set becomes its own node; the rest of a box collapses into one node when the box holds more ids than the split threshold, counting the set's ids; the default threshold is 1 (also S8.06). 03.C5 amends its "keep" for single-file reads (accepted in prompt log entry 166).
- S5.17: the FSC is a named argument of the solve, and both FSC cases run on one corridor.
- S8.15: Not DE removes a gene from the observed up and down lists.

**The union**

- S2.17: an explicit, order-independent union in one pass, with every merged node's members listed in the output. Whether families should be unioned at all is revisited under roadmap objective 2.
- S2.18: with the explicit union there are no key chains to follow.
- S2.08: every id of a box joins the node the box resolves to.
- S2.09: complexes stay, one per combination of the components' nodes, keyed `c_s_…_c_e`, each with two-way component edges that carry the file's own pathway.
- S2.11: indirect-effect relations are handled by a run setting applied before nodes are made, discarding them by default.
- S2.14: no `all_id_map` keying and no naming-map step.
- S2.19: a node-type conflict is an error naming the node and both pathways, and the run stops (S1.28).
- S2.01: integer graph ids with `0` for the latest; merge intermediates are freed.
- S2.23: none of FuSION 1's dead work (occurrence maps, `MAPK_merged_ids`, overwritten display names, edge-id copies).

**Edges**

- S2.04: two edges are the same edge when source, target, type and the exact ordered subtype list match.
- S2.10: subtypes are normalised; a relation fans out over its two entries' nodes; an association, dissociation or mapping relation also gets the reverse edge; a duplicate's larger `experiment_score` is kept.
- S2.20: matching edges merge, with pathways unioned and the larger score kept.
- S2.21: subtype lists are compared without `dummy_u_to_d`, and the tag is computed once, on the merged graph.
- S5.13: the solver never reads the tag.

**Reuse, cost and saved steps**

- S2.22: one pass with hashed indexes. Each pathway list is merged once without the run's exceptions and the exception ids are split out afterwards, so runs on the same pathways share one merge. Phase 3 must show that the split gives the same graph as merging with the exceptions, read as 2.0's own exception-aware semantics (12 open question 5, closed in prompt log entry 208).
- S8.23: the graph is built once in memory, saved with the run, and a later run of the session can start from it.
- S2.26: no `wgx`/`rgx` round trip inside a run; the in-memory graph gets the round trip's normalisation, and the written file is a saved step.
- S7.19: kept as it is. Prompt log entry 087 (Q3) declined one parse of the candidate KGMLs shared between `gsea` and `mff`, so `mff` parses its own files (cross-section item 4A.5, prompt log entry 158).
- Decision 0019: a run can stop after any step or start from a saved one.
- D1 and S5.29: the batch interpreter keeps the step values in memory, and no step keeps state between calls.
- S1.26 and S8.21: every output of a run goes in the run's own directory, which is never deleted or reused. They stand: the shared store is read as a cache, not an output (prompt log entry 166).

**The command and its inputs**

- S2.16: `mff <list> <thres> <y|n>`; FuSION 1's `xml` token and map-file list are accepted with a warning that they are ignored; every listed path is checked before the run starts.
- S7.06 and S7.07: `gsea` reads candidate lists with `mff`'s reader and checks, and two candidates with one pathway id are an error naming both files.
- S7.03: an empty GSEA selection is a reported result, and the run's later steps, `mff` included, are skipped.
- S2.24: an optional provenance report, written to a path given to the command when it is asked for.
- S2.25 and S2.12: the merged graph is written in FuSION's own merged-graph format, marked as such, and readers recognise that format instead of trusting the `y` flag.
- S2.03: when an id lands in a node with other genes, the lookup reports the node's members (section 02); the merge supplies the members.
- S2.28: `sz` also prints how many nodes hold more than one gene, and the largest such node.
- S1.27: numbers on the console are printed with enough digits to be exact.
- S2.27, widened by 02.C5: every unmarked file that FuSION 1 wrote is read the FuSION 1 way, kept apart from the raw-KGML pipeline, and flagged as legacy.
- S2.30, amended by 02.C17: `add`'s output carries 2.0's mark (accepted in prompt log entry 163).
- S1.15, amended by cross-section item 2B.4: `start` is optional, and a batch without it uses the shipped maps (prompt log entry 163).
- S8.08 and S8.24: a KGML list or a pre-merged graph, chosen explicitly, with additional edges in both modes.
- S8.31: the shipped KGML list resolves against the user's own KGML directory.
- Decisions 0020 (any pathway source plugs in), 0021 (inputs checked before the run; a named but unreadable optional input stops it), 0017 and 0023 (old batches run; results may change where 2.0 improves).

**Settled in the architecture polls (decision 0031)**, beyond the choices below:

- 1.1, 1.17, 2A.9, 4A.5, 2B.10 to 2B.12 (prompt log entry 158): 09's core vocabulary; generic error codes from 09 and 01, with one `network.flag_mismatch`; one no-FSC note, from 01's check (`batch.no_fsc`, named in prompt log entry 230); no parse shared between `gsea` and `mff`; a step's list uses the resolved paths of the shortlist it takes; 02 adds the builder calls and header fields the merge needs, and returns each list line's text with its path.
- 02.C5 and 02.C15 (prompt log entry 159): the legacy reader reproduces FuSION 1's graph 2 for every unmarked FuSION 1 file; every network input is fully parsed before the run.
- 02.C10 and 02.C12 (prompt log entry 162): canonical order; list entries resolve against `kgml_dir`, else the list's folder.
- 02 open question 8 and cross-section item 2A.4 (prompt log entry 164): a known FSC joins E in legacy reads too; `load` restores a saved network exactly, `rgx` compares and rebuilds, and `mff` has no `from_saved`.
- 01.C17 and 03 open question 1 (prompt log entry 165): `mff`'s exception arguments default to the look-ahead facts; FuSION 1 batches keep their own exception file.
- 3A.19 (prompt log entry 167): one decimal grammar, parsed with fast_float, for values, thresholds, real arguments and edge scores.
- 2B.3 (prompt log entry 169): a relative or omitted output path lands in the step's directory; an absolute one is honoured as well.
- 06.C10 (prompt log entry 175): `fsc=` takes one or more ids, forced in or out together (extending S5.17).
- 09.C2 and 09.C3 (prompt log entries 177 and 184): one directory per step, named by its command; inputs copied and hashed first, then parsed from the copy.
- 01.C11 (prompt log entry 179): `exec mv edges_merged_filename <dest>` writes the latest merge's report, and nothing, with a note, after a one-file `mff` over the old master.
- 09.C14 (prompt log entry 186): a failure is a `core::Failure` exception, a reported result is part of a `core::Outcome<T>`, a stop is `core::Stopped`.
- 01.C13 and 01.C16 (prompt log entry 189): a file written earlier in the run comes back from memory while it is unchanged; an argument defaults to the latest value of its kind.
- 1.16, 2B.2 and 1.3 (prompt log entry 190): registry keys `<module>.<name>`, `run.cache` in place of a merge switch, run-level settings on the `run` line; dotted event names under the key `type`.
- 11.C6 and 1.2 (prompt log entry 201): one static library per module; `merge` links `network` and `expression`.
- 11 open question 4 (prompt log entry 205): a KEGG pathway's name comes from its KGML's root `title`.
- 12.C14 (prompt log entry 208): two oracles for S2.22, `merge_direct` and the test kit's `exception_merge`, with FuSION 1 as a cross-check.
- Prompt log entry 230: the check's codes `batch.no_fsc` (a positional batch names no FSC, 2A.9), `batch.mff_before_cudf` and `batch.bad_argument`; and section 02's readings, among them pathway names (a KGML's root title, then the pathway-name map, then the id) and unscored relations for unparseable scores.

**Skeleton:** A2 (events in `events.jsonl`), B1 (library, never the console), C1 (this is `merge`), D1 (typed values in memory, every step saveable and loadable), E1 (`run.json`, JSON results, TSV tables with a header), G1 (C++20), G3 (Linux, macOS and Windows, native).

## Design

### The step

Section 01's `mff` handler (01.C9) resolves the step's inputs and calls `merge_step`. One `mff` runs six stages:

1. **Read the list.**
   - The list defaults to the latest `gsea` shortlist (01.C16), whose resolved paths the step uses (2B.10). If the shortlist file changed after `gsea` wrote it, as `r7` and `r8` append a curated KGML with `exec echo … >>`, it is read again from disk, with a note (01.C13).
   - A named list is read by section 02's `read_input_list` (02.C12), which returns each line's text and path (2B.12).
   - Each file's size and SHA-256 are the ones `run.json` records (E1), computed once while the run copies its inputs (09.C3).
2. **Key** the merge from the digests and the build settings (03.C8).
3. **Get the base merge**, the merge without exceptions: from memory, else from the shared store, else by building it (03.C3).
   - To build it, `mff` uses the parse of its own inputs: the check before the run parsed their copies (02.C15, 09.C3), and the step takes that parse.
   - `gsea`'s parse of the same files is never shared (S7.19, 4A.5).
4. **Assemble E** from the run's inputs (03.C1, 03.C2).
5. **Split:** apply E to the base merge.
6. **Register** the result as the next graph id, write the saved step and any report, and emit events.

When the list exists before the run starts, the check of decision 0021 also runs stages 2 and 3, so type conflicts stop the batch before any step. The base merge then waits in memory for the step. A list that an earlier step writes, such as `gsea`'s shortlist, is merged when `mff` runs.

### Terms

- **Units of a box under E** (S2.07), as section 02's `split_entry` computes them: when the box holds more ids than the threshold, each of its ids in E forms a unit alone and the rest form one unit; otherwise every id forms a unit alone. The count includes the ids in E, as in FuSION 1 (`Linux/src/XMLParser.cc:899-900`). A negative threshold never collapses, and 0 behaves like 1 (S2.07).
- **Node:** a connected component of ids, where the ids of each unit are connected. Its key is its smallest member in byte order, the rule of FuSION 1's re-read (S2.26). Its type is its boxes' type.
- **Complex:** the distinct component nodes of one combination of a group's components. Its key is `c_s_`, the component keys sorted and joined by `_`, then `_c_e` (S2.09).
- **Base merge:** the merge with E empty, plus the provenance that links each node to its boxes, each complex to its group entries and each edge to its relations.

### The union (S2.17)

`merge_direct(docs, settings, E)` is the whole algorithm in one pass. It is the first of the two oracles that the split is tested against (12.C14). Steps 1 to 4 are this section's; steps 5 to 7 feed section 02's `NetworkBuilder`, which merges edges on identity, sets the tag and sorts.

1. **Reached entries.** For each document, the relations that `kept_relations` keeps (S2.11, S2.31) and the entries they reach, directly or as components of a reached group (S2.06). Filtered relations make no nodes.
2. **Units and the type table.** `split_entry` gives each reached box's units. Each id takes the type of the first box that holds it, in document order; a box of another type is `merge.type_conflict`.
3. **Union-find** over the interned ids, with union by rank and path halving: the ids of each unit are joined. O(N α(N)), where N is the number of box-id incidences: 5,449 for the 42-pathway list and 23,370 for the 167 KGMLs.
4. **Nodes:** one per component, members sorted, key the smallest member. Every single-id node whose id is in E is marked as an exception node (`mark_exception`).
5. **Complexes:** for each reached group, the Cartesian product of its components' node sets. A box component's set is the nodes of its units; a group component's set is its complexes. `add_complex` reuses a complex already made. O(Σ products).
6. **Edges:** for each kept relation and each pair (s, t) of its entries' node sets, `add_edge(s, t, type, subtypes, pathway, score)`, and for an undirected relation also (t, s) unless s = t.
   - The builder merges a repeat on identity (S2.04), unions pathways and keeps the larger score (S2.10, S2.20).
   - With provenance on, it records the relation in section 02's `Provenance`, as a reference of kind `relation`.
   - Each complex gets its two-way component edges per distinct component, with the pathway of each group that made it (S2.09). Each group entry behind a complex goes into `Provenance` as a reference of kind `group` (2B.11). The boxes behind each node are the merge's own table, which the split needs.
   - O(F) expected, where F is the total fan-out.
7. **Finish:** the builder sets the tag once (S2.21) and sorts nodes and edges canonically (02.C10).
   - The tag goes on every component edge, both ways, as in FuSION 1 (S2.09), and on every undirected edge s → t with s ≠ t whose source key is larger than its target key. Self-loops stay untagged (S2.10).
   - In the reference run's network, 228 of the 456 undirected edges between distinct nodes carry it: exactly one direction of each pair.

What this guarantees:

- Each id is in exactly one gene or compound node, by construction. Stage 2's open question 4 asked this of FuSION 1.
- The result does not depend on the order of the list, the order inside a file or hash-map iteration. With 02.C10's order, five shuffles of the 42-pathway list gave identical output in the prototype.
- Keys match those of FuSION 1's re-read graph for all 1,113 nodes that the reference run's two graphs share, complexes included. The re-read recomputes a complex's key from its components (`Linux/src/XMLParser.cc:1098-1099`, `Linux/src/GraphManagerNew.cc:2438-2466`). The stale keys that FuSION 1 wrote into the names of 10 complexes (S2.18) therefore never reach later stages. A key moves only where S2.08 or S2.11 changes a node's members (below).
- No node is isolated: a node exists only when a kept relation reaches one of its boxes, and every complex has component edges.
  - The prototype finds 0 isolated nodes in the 42- and 167-pathway networks. FuSION 1 wrote 22 for the reference run, and its re-read dropped them (S2.11, S2.26).
  - So the in-memory network already has the round trip's normal form (S2.26): keys are smallest members, pathway lists are sorted, isolated nodes are absent, and labels come from the symbol map when written (02.C16).

**Agreement with FuSION 1.** The prototype's network was compared with FuSION 1's `run/merged_master_graph.xml` in four runs. Nodes were matched by their member sets, as Phase 3 will match them (03 open question 2, closed), complexes by their components', and edges by their endpoints, type and subtypes without the tag.

| Run | FuSION 1 nodes, edges | 2.0 nodes, edges | Edges between nodes in both graphs | Of these, different |
|---|---|---|---|---|
| reference (42) | 1134, 2529 | 1129, 2509 | 2351 | 0 |
| fdr040_keep (87) | 1666, 4890 | 1657, 4844 | 4564 | 0 |
| fdr044 r0 (87) | 1728, 5004 | 1721, 4957 | 4706 | 0 |
| fdr050 (90) | 1763, 5019 | 1756, 4972 | 4720 | 0 |

FuSION 1's node counts leave out the isolated nodes. Every score agrees. The other differences all come from decided fixes, and are expected differences under decision 0023 (prompt log entry 164):

- **S2.08:** 11 ids that FuSION 1 lost now join nodes (13 in the fdr040_keep run), RUNX1 joining MECOM's node among them. They also widen unions: the glutamate-receptor nodes of 7 and 4 genes become one 18-gene node, and three compound nodes become one.
- **S2.11, applied before nodes are made:** a box reached only through indirect relations no longer joins unions. In the reference run:
  - the `hsa04010.xml` box {NFKB1, NFKB2, RELA, RELB} (entry 33) is reached only through two indirect relations, so NFKB2 and RELB stay apart from NFKB1 and RELA;
  - BAK1 and BAX, one node in FuSION 1 through `hsa05132.xml` entry 808, become two;
  - GNAL, GNAO1, HK1, HK2, HK3, HKDC1 and VDAC1 leave the network, and so do the complexes [HK1, HK2, HK3, HKDC1 + VDAC1] (`hsa05131.xml`, entry 1204) and [CD14 + TLR2, TLR4] (`hsa05146.xml`, entry 293).
- **S2.11 also moves a key.** In the three 87- and 90-pathway runs measured (fdr040_keep, fdr044 r0 and fdr050), the receptor-tyrosine-kinase node loses TFG, TPM3, TPR, CCDC6 and NCOA4.
  - Their oncogenic fusion boxes with NTRK1 and RET (`hsa05200.xml` entries 151 and 184, `hsa05216.xml` entries 29 and 30) are reached only through indirect relations.
  - The node's key moves from `hsa10342` (TFG) to `hsa1436` (CSF1R), and its complex from `c_s_hsa10342_c_e` to `c_s_hsa1436_c_e`.
  - FuSION 1's solution files name the old key, for example 3 lines of `runs/egfr_b10_gsea_fdr044_candidates/r0_cxcl8_jun/run/temp_w_solutions.txt`. No alias is kept: S5.06 reports any line of an old node or edge file that names a moved key (prompt log entry 164).
- **S2.21:** 6, 7, 12 and 12 tagged and untagged duplicates disappear.
- **S2.09:** 6 component edges per run carry a different pathway list.

### Complexes (S2.09)

- A complex whose components fall into one node after the union keeps one component, as in FuSION 1: 12 of the reference run's 93 complexes.
- With threshold 1 and the reference run's E, no group makes more than 2 complexes. The 42-pathway network has 93 complexes and the 167-pathway one 277.
- With threshold 15000, FuSION 1's GUI default (S8.06), nothing collapses, so products and fan-outs grow: up to 117 complexes from one group and 36,907 edges for the 42-pathway list, and 621 and 113,162 for the 167 KGMLs. The algorithm handles both. FuSION 1's one recorded GUI run, at threshold 15000 on the legacy master, wrote 82,188 relations (S8.23).

### Node-type conflicts (S2.19)

- **Rule:** an id held by boxes of two types. Unions follow shared ids, so this is the same as a node made of boxes of two types.
- **When:** at stage 3, while the base merge is built. The rule does not depend on E, so a base merge that exists can always be split, and a stored one never fails later. Because the check before the run parses every network input (02.C15), it finds the conflict before any step.
- **Message:** it names the id, both types and both boxes: `mff: node-type conflict: cpdC00076 is a compound in path:hsa04020 (hsa04020.xml, entry 35) and a gene in my_edges (my_edges.tsv, line 12)`. That example, calcium named in a user's edge table, is the case section 02 foresees: an edge table without a type column gives its endpoints the type `gene`.
- **Measured:** no id has two types in the 167 KGMLs, and FuSION 1's warning never fired (S2.19).

### The exception set (S1.17, S2.13, S8.22)

`assemble_exceptions` builds E from the inputs below, with every id put through section 02's id rule (02.C3). It records each id's reasons for `exceptions.tsv`.

| Reason | Source in 2.0 | FuSION 1 |
|---|---|---|
| `up`, `down` | `Observations::observed()` of the latest `cudf` (01.C16, 04): its lists after Not DE (04.C4). None when no `cudf` runs before `mff`, which section 01's check warns about (`batch.mff_before_cudf`) | `exec cat` of `up_reg` and `down_reg` |
| `confirmed_up`, `confirmed_down` | the solve's confirmed-list files, which 01's look-ahead collects from later `pathz3` lines (03.C2), read like the not-merged file | never added |
| `source`, `target` | the corridor's endpoints, from 01's look-ahead facts or `mff`'s arguments (03.C1) | `src_file`, `tgt_file` |
| `fsc` | the FSC being tested (S5.17), one or more ids (06.C10), from 01's look-ahead facts or `mff`'s argument (03.C1) | never added: the RAS trap (S2.03) |
| `not_merged` | the not-merged file, first token per line, blank lines skipped | `hsa_ids_not_to_be_merged.txt` |
| `start_file` | `start`'s argument 2, when a batch has a `start` line (optional since 2B.4) | the whole set |

- **The reference run,** written with named arguments (`fsc=hsa3725` and the not-merged file): E holds 49 ids. They are 43 up and 2 down, EGFR (source), CXCL8 (target, also up), JUN (FSC, also up), and MAPK14, MAPK1 and MAPK8 (not merged).
  - 35 of them are in the network. 14 of these have a base node of their own, and 21 sit in a node with other ids before the split. Among those 21:
    - EGFR is in a 26-gene receptor node keyed `hsa1436` (CSF1R);
    - CXCL8 is in a 47-gene chemokine node;
    - JUN and MAPK8 are in a 6-gene node with FOS, MAPK9, MAPK10 and SP1;
    - DUSP1 is in the 10-gene DUSP node.
  - Run as its FuSION 1 batch, E holds the same 49 ids: the `start` file's, with the up and down genes added from `cudf` (S1.17). JUN's reasons are then `up` and `start_file`. The batch names no FSC on any line, so its own exception file applies, and section 01's check notes that the FSC is unknown (`batch.no_fsc`; 2A.9, prompt log entry 165).
- **Legacy reads:** section 02's legacy reader applies E with `split_entry` too, and a known FSC joins E there as well (02 open question 8, prompt log entry 164).
- **The FSC:** the fdr044 candidate runs added their FSCs (RHOA, RAC1, CDC42, STAT3, CREB1) to the not-merged file by hand, giving 8 ids (`r0_cxcl8_jun/inputs/hsa_ids_not_to_be_merged.txt`). With S2.13 the FSC joins by itself.
- **Other node lists** (essential, avoid, active, inactive, non-relaxed) do not join E. Runs use them on whole families on purpose: `r9_epha2_mek_avoid` avoids the MEK node and `r11_epha2_raf_forced` forces the RAF node. Section 02's binding report says which node such an id binds to (S2.03).
- **In a sweep** each point has its own facts, so its own E, and each point's split starts from the same base merge.

### Merge once, split afterwards (S2.22)

**Why the base merge keeps provenance.** Deleting E's ids from the base merge's nodes is wrong. In the reference run, the base merge has one node {FOS, JUN, MAPK8, MAPK9, MAPK10, SP1}, keyed `hsa2353`. It is built from:

- the AP-1 boxes {FOS, JUN} of 8 pathways;
- 21 JNK boxes {MAPK8, MAPK9, MAPK10} in 17 pathways;
- `hsa05161`'s box {JUN, MAPK8, MAPK9, MAPK10} and `hsa05224`'s box {FOS, JUN, SP1}.

With E, where JUN is the FSC and MAPK8 is not merged, the merge gives {FOS, SP1} and {MAPK9, MAPK10}, because every link between the two sides ran through JUN or MAPK8. Deleting JUN and MAPK8 from the base node leaves {FOS, MAPK9, MAPK10, SP1} as one node. This shortcut gets 3 node identities wrong in each of the 42-, 87- and 90-pathway runs, and 6 for the 167 KGMLs. Section 02 reached the same conclusion for saved networks (02.C4).

**The split.** `split(base, E)` reuses the base merge for everything E does not touch:

1. A is the set of base nodes with two or more members that hold an id of E.
2. The affected boxes are the boxes with a unit in A, found through the provenance. The affected groups are the groups with an affected component, followed to a fixpoint. The affected relations are those with an affected entry.
3. The ids of A are joined again from the affected boxes' units under E, with steps 3 and 4 of the union. A box that does not collapse can have units in several nodes; its units outside A keep their base nodes.
4. Steps 5 to 7 recompute the affected groups' complexes and the affected relations' edges. Every other node and edge is copied from the base merge, through the builder's `copy_node` and `copy_edge` (2B.11), which keep each edge's provenance.
5. Every single-id node whose id is in E is marked as an exception node, including those copied unchanged: 14 in the reference run, whose base node already held only that id.

**Why it is exact.** A collapsing box's ids lie in one base node. A base node without ids of E is made only of boxes without ids of E, whose units and unions are the same under E, so the node reappears unchanged. So do the complexes and edges that touch only such nodes. Everything else is recomputed by the code `merge_direct` runs.

**Cost.** O(affected incidences + affected fan-out), plus a copy of the rest. E's ids sit in the largest families (chemokines, receptors, ligands), so the split touches 8 to 14 % of the boxes and 15 to 27 % of the relations:

| List | Affected nodes | Affected boxes | Affected relations |
|---|---|---|---|
| 42 | 12 | 220 of 2,902 | 396 of 2,559 |
| 87 (fdr040_keep) | 17 | 767 of 6,600 | 1,276 of 6,265 |
| 88 (`r7` to `r16`) | 20 | 899 of 6,450 | 1,611 of 6,035 |
| 167 | 17 | 1,012 of 10,550 | 1,614 of 9,704 |

**Evidence for Phase 3.** S2.22's equivalence is read as 2.0's own exception-aware semantics, and is tested with two oracles (12.C14, prompt log entry 208):

- `split` must equal `merge_direct`, and both must equal the test kit's `exception_merge`. `exception_merge` is written from S2.07 to S2.21, with its own KGML reading and no code shared with `merge` or `network`, so a fault in the code that `split` and `merge_direct` share cannot pass unseen.
- Section 12 checks them in three layers: every exception subset of about 50 small synthetic sets; 1,000 random cases per run and 100,000 weekly; and the 7 distinct real merges of the 24 valid runs, with the 167 KGMLs split by 100 random subsets of the EGF up and down genes.
- All three are cross-checked against FuSION 1's merge under the recorded expected differences (section 12's XD-03 to XD-06 and XD-16). The agreement table above is a first run of that cross-check.

In the prototype, the split equals `merge_direct` with the real E of the 42-, 87- (both), 88- and 90-pathway lists and of the 167 KGMLs, and in 240 random cases:

- the cases used random sublists of the 167 KGMLs, thresholds 0, 1, 2, 3 and 15000, and 1 to 1000 random exception ids;
- the prototype's first version failed one case in 60, at threshold 2: it gave a non-collapsing box's units outside A new nodes, which step 3 now forbids. Phase 3 must therefore test thresholds other than 1.

### Reuse: keys and the store (03.C3, 03.C4, 03.C8)

**The key** (03.C8) is the SHA-256 of a text like this one, for the reference run:

```text
fusion-base-merge 1
network.split_threshold=1
network.indirect=discard
network.ecrel=keep
network.maplink=skip
00c875a9599edee8e6900e35e16f509f8d9d98f520824d75048d51700bf72e05
0bb8b61b4b04a004b97e8cec8299ec6c3d4343ae37972dcfd3bfe2978c9fa9ab
…  (one line per input, 42 here)
```

- Every line, the last included, ends with LF. This text gives `0e91f9a911c19a7f98fd415ffd56bc95ae6fd52128c70bc586557d35978c74d5`.
- **The first line** carries the rules version, which is bumped whenever the reader's or the merge's rules change (see Test hooks).
- **Then come** section 02's build settings, by their registry keys (1.16), and the id map's digest when the run gives one (02.C3), since it changes the documents' ids.
- **Then one line per input,** sorted, each line once:
  - each line is the file's SHA-256;
  - an edge table takes its default pathway id from its file name (02.C2, 2B.12), so its line also carries that pathway id. Identical tables under two names then stay two inputs with two pathways.
- **Not in the key:** paths, list order, E, the symbol and pathway-name maps, and FuSION's own version.
- **One rule, stated in section 09** (3A.3, prompt log entry 166): the store keys on rules versions, and resume fingerprints on the build id (09.C5).

**What sharing there is.** On 2026-11-12 the logs under `runs/` held 49 raw-KGML merges by FuSION 1, which took 1,481 s in all.

- There were only 10 distinct list contents. The two 163-pathway builds of `runs/egf_hela/experiment/` count as one content, since they read the same 163 paths (which have since moved).
- Merging each content once would have taken 335 to 351 s: the sum of each content's fastest and slowest logged times.

Keys built from paths would miss part of this and confuse another part:

- 20 fdr044 runs (`r7` to `r26`) each list their own copy of a curated KGML under `inputs/`, such as `hsa99901_curated_erk_targets.xml`, so two groups of 10 lists with the same content name 20 different paths.
- The seven `_string125` reruns name the same file names in `auxilliary/hsa_pathways_with_exp_score_v12.5/`. These files have the same structure (5,004 edges) but different scores, so they rightly get their own key.

**Where merges live (03.C3, accepted in prompt log entry 166).**

1. **Memory:** the batch interpreter holds one `MergeStore` per `fusion` process and passes it to each `mff` step, so sweep points and repeated `mff` lines share a merge.
   - This follows D1, which has the interpreter hold step values in memory.
   - The `merge` module keeps no state of its own between calls (S5.29).
   - An entry is immutable and keyed by its content, so it cannot change a later result.
2. **The shared store,** in each user's cache directory (09.C13): `$XDG_CACHE_HOME/fusion` or `~/.cache/fusion` on Linux, `~/Library/Caches/FuSION` on macOS and `%LOCALAPPDATA%\FuSION\Cache` on Windows. `run.cache_dir` or `FUSION_CACHE_DIR` moves it, and `run.cache=false` and builds with uncommitted changes bypass it; the step's `core::Context` then carries no `Cache`.
   - Each entry is one file, `1/<key>.xml` in `Cache::dir("merge")`, so `merge/1/<key>.xml` under the store's root, holding the base merge in section 02's network format with its sources embedded (03.C4).
   - It is written once: staged as `merge/1/.tmp-<key>-<pid>-<n>.xml`, then renamed into place by `core::Cache::publish`, and the first writer wins.
   - An entry records its inputs' SHA-256, pathway ids and titles, not their paths, which are each run's own and go in its `run.json`. Two entries with one key therefore hold the same network and documents, and can differ only in the writer's version. Whichever is kept serves every later run.
   - A reader checks the entry's recorded key and sources; a bad entry is bypassed with the warning `merge.store_invalid`.
   - Nothing is evicted automatically, and deleting the store loses nothing.
3. **The run directory:** the run's `merged_graph.xml` embeds its sources (02.C4) and records its base merge key. A later run can start from it with no other file, even after the store is cleared (A1), in three ways (2A.4, prompt log entry 164):
   - `load` restores it exactly as saved (01.C6, decision 0019);
   - `rgx` compares its recorded settings, exception set and id-map digest with the run's (section 02's `saved_network_diff`). When the settings or E differ, it rebuilds through the merge: `MergeStore::from_embedded` gives the base merge, from the store or from the embedded documents, and `split` applies the run's E. A file without sources, or built under another id map, cannot be rebuilt, which is section 02's `network.settings_mismatch`;
   - an `mff` list may name it (03.C7).

The store is a cache, not a run output. Every output of the run, including the documents a rebuild needs, goes in the run's own directory, so S1.26 and S8.21 stand (03 open question 3, closed in prompt log entry 166).

### Kinds of source in a list

Section 02's `sniff` tells each file's kind. The merge treats them as follows:

- **KGML and edge tables** (02.C2, including `add`'s four-field lines, S2.30): parsed documents, merged as above. The additional-edges file of S8.24 is one of these.
- **FuSION network files** (02.C4): their embedded documents join the union as if the original files were listed. The key is computed from the documents' recorded SHA-256 and pathway ids, so the store can serve the base merge (03.C7). The documents' ids are already mapped, so a file built under another id map is refused, as `rgx` refuses it (`network.settings_mismatch`).
  - `add`'s output is such a file (02.C17). It records no build settings and every node holds one id, so it joins as it stands: a transcription, with no build rule.
- **Legacy files** (02.C5: the old master and the other unmarked files FuSION 1 wrote): never unioned and never stored. Section 02's `read_legacy` builds the graph with `split_entry` and first-claim lookups (S2.12). 03.C9 says what may share its list.

The `y|n` argument, section 02's `tool_written`, stays (S2.16), but the kind comes from `sniff` (S2.12). A flag that disagrees with the kind is the warning `network.flag_mismatch`, raised by `sniff` (1.17).

### Cost

FuSION 1's `mff` times come from the logs and have a resolution of one second (`time()`, `Linux/src/main.cc:2376`, `2468-2469`).

The prototype's times, in CPython, are medians of five runs in one session. The machine's load average was about 14 on 36 cores, and an earlier session measured up to twice these times.

| Pathways | FuSION 1 runs | FuSION 1 `mff` (s) | KGML bytes | Boxes, groups, relations kept | Prototype: parse, base merge, split (s) |
|---|---|---|---|---|---|
| 42 | 4 | 3 | 1.99 MB | 2,902, 150, 2,559 | 0.075, 0.022, 0.011 |
| 56 | 1 | 11 | 2.85 MB | 4,367, 292, 4,056 | 0.131, 0.036, 0.022 |
| 71 | 1 | 17 | 3.51 MB | 5,433, 328, 5,131 | 0.143, 0.047, 0.050 |
| 87 (fdr040_keep) | 3 | 53 | 4.17 MB | 6,600, 372, 6,265 | 0.179, 0.082, 0.036 |
| 87 (fdr044) | 10 | 22–25 | 4.13 MB | 6,447, 384, 6,033 | 0.191, 0.085, 0.041 |
| 87 (`_string125`) | 7 | 34–39 | 4.14 MB | 6,447, 384, 6,033 | 0.197, 0.057, 0.039 |
| 88 (`r7` to `r16`) | 10 | 23–26 | 4.13 MB | 6,450, 384, 6,035 | 0.191, 0.056, 0.040 |
| 88 (`r17` to `r26`) | 10 | 26–27 | 4.14 MB | 6,454, 384, 6,040 | 0.173, 0.083, 0.040 |
| 90 | 1 | 24 | 4.29 MB | 6,709, 421, 6,187 | 0.197, 0.061, 0.039 |
| 163 | 2 | 122–126 | 6.68 MB | 10,208, 584, 9,440 | 0.298, 0.093, 0.057 |
| 167 | – | – | 6.85 MB | 10,550, 608, 9,704 | 0.324, 0.132, 0.074 |

- The FuSION 1 runs in the table add up to the 49 merges counted above.
- The two 163-pathway FuSION 1 builds (`runs/egf_hela/experiment/graph_build.log`, `build_graph_egfr.log`) predate the indirect discard. The prototype's 163 row discards indirect relations.
- **The C++ estimate:** Expat alone tokenises the 6.85 MB of the 167 KGMLs in 0.14 s here, and pugixml is commonly several times faster. The union and the edges are hash-map work over about 23,000 incidences and 10,000 relations. So a merge from scratch should take under 0.1 s and a split a few milliseconds. This is an estimate, which the benchmark hook (Test hooks) measures in Phase 3.
- **Memory:** a few MB for the 167 KGMLs. In section 02's layout (02.C8), the largest network measured, all 273 KGMLs without collapse (about 149,000 edges), takes about 7 MB, so threshold 15000 on the 167 KGMLs (113,162 edges) stays below that.
- **Disk:** a store entry or a run's `merged_graph.xml` is the network plus its embedded sources, about 0.9 MB for the 42 KGMLs. FuSION 1's merged file of that network is 540 KB, and section 02 counts 348 KB of embedded sources.

### Interface outline

`merge` links `network` and `expression` (1.2, 11.C6), so its interface uses no `batch`, `enrichment` or `run` type: section 01's handler turns the look-ahead facts, the latest shortlist and the latest observations into the plain values below.

```cpp
// Types from other sections:
//   core (09's vocabulary, 1.1): GeneId, Sha256, Failure (thrown), Stopped, Report, Outcome<T>,
//     Context {log, events, stop, settings, files, cache, step}, Cache, OutputFile
//   network (02): PathwayDoc, SourceRef, BuildSettings, IdSet, IdMap, Maps, Network, NodeIdx,
//     NetworkBuilder, Provenance, StepDocs, split_entry, kept_relations
//   expression (04): Observations

namespace fusion::merge {

// ---------- the exception set (S1.17, S2.13, S8.22) ----------
enum class Reason : std::uint16_t {
    up = 1u << 0, down = 1u << 1, confirmed_up = 1u << 2, confirmed_down = 1u << 3,
    source = 1u << 4, target = 1u << 5, fsc = 1u << 6, not_merged = 1u << 7, start_file = 1u << 8,
};
class ExceptionSet {
public:
    void add(core::GeneId, Reason);
    bool contains(core::GeneId) const noexcept;
    std::uint16_t reasons(core::GeneId) const noexcept;        // bit set of Reason
    const network::IdSet& ids() const noexcept;                // what split_entry takes
    core::Sha256 digest() const;                               // over the sorted ids, for run.json
};
struct ExceptionInputs {                                       // filled by section 01's handler
    const expression::Observations* observations = nullptr;   // observed(): up, then down (01.C16);
                                                               // null when no cudf ran
    std::span<const core::GeneId> sources, targets, fscs;      // defaults from 01.C17; the FSC is a set (06.C10)
    std::vector<std::filesystem::path> confirmed_up, confirmed_down;   // 03.C2; files, defaults from 01.C17
    std::optional<std::filesystem::path> not_merged;            // first token per line (S1.17), like every file here
    std::optional<std::filesystem::path> start_file;            // FuSION 1's start argument 2
};
// Throws core::Failure: input.missing, input.unreadable.
ExceptionSet assemble_exceptions(const ExceptionInputs&, const network::IdMap&, core::Context&);

// ---------- keys (03.C8) ----------
inline constexpr std::uint32_t rules_version = 1;              // in every key; see Test hooks
struct MergeKey {
    core::Sha256 value;
    std::string hex() const;
    auto operator<=>(const MergeKey&) const = default;
};
// One line per input: its SHA-256, and for an edge table the default pathway it takes from its name.
MergeKey merge_key(std::span<const network::SourceRef> inputs, const network::BuildSettings&,
                   const network::IdMap&);

// ---------- the base merge (S2.17, S2.22) ----------
using DocPtr = std::shared_ptr<const network::PathwayDoc>;
struct EntryRef { std::uint32_t doc; std::uint32_t entry; };   // document index, entry id

class BaseMerge {                                    // immutable; held by the interpreter's MergeStore
public:
    const MergeKey& key() const noexcept;
    const network::BuildSettings& settings() const noexcept;
    std::span<const DocPtr> docs() const noexcept;
    std::shared_ptr<const network::Network> network() const noexcept;   // E = {}; its Provenance holds
                                                                         // relations and group entries (2B.11)
    std::span<const EntryRef> boxes_of(network::NodeIdx) const noexcept;  // node -> its boxes, for the split
};
// Throws core::Failure: merge.type_conflict (S2.19). Throws core::Stopped between documents.
std::shared_ptr<const BaseMerge> build_base_merge(std::vector<DocPtr>, const network::BuildSettings&,
                                                  MergeKey, core::Context&);

// ---------- the split, and the oracle it is tested against (12.C14) ----------
struct SplitStats {
    std::uint32_t exception_ids, in_network, split_out;   // split_out: left a multi-id base node
    std::uint32_t affected_nodes, affected_boxes, affected_groups, affected_relations;
};
struct MergedNetwork {
    std::shared_ptr<const network::Network> network;      // canonical order (02.C10)
    ExceptionSet exceptions;                               // as applied; their nodes marked
    MergeKey base;
    SplitStats stats;
};
MergedNetwork split(const BaseMerge&, const ExceptionSet&);
std::shared_ptr<const network::Network> merge_direct(std::span<const DocPtr>, const network::BuildSettings&,
                                                     const ExceptionSet&);   // throws merge.type_conflict

// ---------- reuse (03.C3) ----------
class MergeStore {                                   // owned by the batch interpreter (D1)
public:
    enum class Source : std::uint8_t { memory, store, embedded, built };
    struct Hit { std::shared_ptr<const BaseMerge> merge; Source source; };
    // Memory, then the shared store (ctx.cache; null: bypassed), then build from the documents.
    // The inputs carry the SHA-256 the run computed while copying them (09.C3). Never changes the result.
    Hit get_or_build(std::span<const network::SourceRef> inputs, const network::BuildSettings&,
                     const network::IdMap&, network::StepDocs&, core::Context&);
    // The base merge of a saved network's embedded documents: for rgx's rebuild (2A.4) and for a
    // network file in an mff list (03.C7). Throws core::Failure: merge.no_sources (an mff list member).
    Hit from_embedded(const network::Network& saved, const network::BuildSettings&, core::Context&);
};

// ---------- reports (S2.24), written through 09's OutputFile (2A.6); labels and names from Maps ----------
void write_exceptions(const MergedNetwork&, const BaseMerge&, const network::Maps&, core::OutputFile&);
void write_edge_report(const MergedNetwork&, const network::Maps&, core::OutputFile&);   // also for 01.C11

// ---------- the step that section 01's mff handler calls ----------
struct MergeRequest {
    std::vector<network::SourceRef> inputs;                 // resolved by the handler (02.C12, 2B.10)
    network::BuildSettings build;                           // split threshold and filters (02)
    ExceptionInputs exceptions;
    std::optional<std::string> report;                      // S2.24; placed by 2B.3's rule
};
// The merge has no reported result of its own, so the Outcome never carries a Report.
core::Outcome<MergedNetwork> merge_step(const MergeRequest&, MergeStore&, network::StepDocs&,
                                        core::Context&);

}  // namespace fusion::merge
```

Section 02 adds what the merge needs from it (2B.11, prompt log entry 158):

- `split_entry` and `kept_relations` as described in section 02;
- `NetworkBuilder::copy_node`, which copies a node with its members, and `copy_edge`, which copies an edge with its flags, pathways, score and provenance, so that `split` can copy the untouched part of a base merge;
- `Provenance` references of two kinds: `relation`, behind an edge, and `group`, behind a complex;
- the `<merge>` header element (`MergeHeader`: the base merge key, the base-merge mark and the rules version), with an empty exception set for a base merge (03.C4), and a `SourceRef` whose path is left out in store entries.

The split network keeps the provenance it copies, so `write_edge_report` needs only the `MergedNetwork`.

All 10,112 `experiment_score` values in the 163 KGMLs are integers from 0 to 999.

### Files written

Every file is written through section 09's `Files::create` and `OutputFile`, a temporary name and then a rename (2A.6), in the `mff` step's directory (`mff/`, 09.C2):

| File | When | Content |
|---|---|---|
| `merged_graph.xml` | always | the run's network, in section 02's format, with members listed (S2.17), sources embedded, E and the base merge key in the header |
| `exceptions.tsv` | always | E, with reasons and what each id became |
| `merged_edges.tsv` | `report=` given | the provenance report (S2.24); a relative or omitted name lands in `mff/`, and an absolute path is honoured as well, the file also kept in `mff/` and both recorded in `run.json` (2B.3) |

FuSION 1's `exec mv edges_merged_filename <dest>` writes the latest merge's report to `<dest>` (01.C11), whether or not `report=` was given, since the merged network keeps its provenance. A legacy read merges nothing and has no report, so the move writes nothing, with section 01's note.

The shared store holds one file per base merge, `merge/1/<key>.xml`. It is in the same format, with an empty exception set, `base="true"`, and no input paths. The header of the reference run's base merge, in section 02's layout (shortened):

```xml
<pathway name="KEGG" format="fusion-network" version="1">
  <fusion writer="FuSION 2.0.0" reader="kgml" legacy="false">
    <merge key="0e91f9a911c19a7f98fd415ffd56bc95ae6fd52128c70bc586557d35978c74d5" base="true" rules="1"/>
    <setting name="network.split_threshold" value="1"/>
    <setting name="network.indirect" value="discard"/>
    <setting name="network.ecrel" value="keep"/>
    <setting name="network.maplink" value="skip"/>
    <input sha256="ce0b9f2863d1749510a1e97ab804fe50b0380391c3d1f93cbcba1bd44e53a6d3" pathway="path:hsa04010" title="MAPK signaling pathway"/>
    <exceptions ids=""/>
  </fusion>
  …
</pathway>
```

The examples below are the reference run written with named arguments, as in the exception-set section above. In `exceptions.tsv`, SOX9 is in no pathway of the list:

```text
id	label	reasons	node	base_node	base_size
hsa1843	DUSP1	up	hsa1843	hsa11221	10
hsa1956	EGFR	source	hsa1956	hsa1436	26
hsa3576	CXCL8	up,target	hsa3576	hsa10344	47
hsa3725	JUN	up,fsc	hsa3725	hsa2353	6
hsa5599	MAPK8	not_merged	hsa5599	hsa2353	6
hsa6662	SOX9	up			0
```

`merged_edges.tsv` (S2.24) has one row per merged edge and relation it came from, and one per component edge and group entry that made its complex:

- nodes are given by key and label;
- a KEGG pathway's name is its KGML's root `title` (02, prompt log entries 205 and 230);
- each row gives the relation's or group's entries and their ids, so a family node's edge can be traced to the box that made it;
- the reference run gives 3,729 rows for 2,509 edges (3,143 relation rows and 586 component rows), against FuSION 1's 3,585 lines for 2,529 edges.

```text
source	target	type	subtypes	source_label	target_label	pathway	pathway_name	entry1	entry2	entry1_ids	entry2_ids	score
hsa5906	hsa369	PPrel	activation	RAP1A	ARAF	path:hsa04010	MAPK signaling pathway	45	41	hsa5906 hsa5908	hsa673	625
hsa5906	hsa369	PPrel	activation	RAP1A	ARAF	path:hsa04722	Neurotrophin signaling pathway	57	59	hsa5906 hsa5908	hsa673	625
```

### Settings

Registry keys follow 1.16 (`<module>.<name>`), and on the `mff` line the bare name is the argument. All are recorded in `run.json` with their source (E1).

| Key | Argument | FuSION 1 | Type | Default | Changes results | Spec |
|---|---|---|---|---|---|---|
| `merge.not_merged` | `not_merged` | inside `start`'s file | path | none | yes, through E | S1.17, S8.22 |
| `merge.source`, `merge.target`, `merge.fsc` | `source`, `target`, `fsc` | written into files by `exec` | gene ids | 01's look-ahead facts (01.C17, 03.C1) | yes, through E | S2.13 |
| `merge.confirmed_up`, `merge.confirmed_down` | `confirmed_up`, `confirmed_down` | none | paths | the files of later `pathz3` lines, from 01's look-ahead (03.C2) | yes, through E | S5.07, S8.15 |
| `merge.report` | `report` | `edges_merged_filename`, after every multi-file merge | output name | none | no | S2.24, 2B.3 |

- `mff`'s `list` (section 02's argument) defaults to the latest shortlist, and its `observations` to the latest `cudf`'s (01.C16). A name or a path overrides either.
- The split threshold and the relation filters are section 02's `network.split_threshold`, `network.indirect`, `network.ecrel` and `network.maplink`. The merge applies them, and they enter the key.
- Whether the shared store is used is section 09's `run.cache`, with `run.cache_dir` for its place (1.16). A batch sets them on its `run` line, for example `run cache=false`, and `fusion --set` overrides that (2B.2). The merge has no switch of its own.
- `run.json` also records, in the step's `details` (section 09), the merge key, where the base merge came from (`memory`, `store`, `embedded` or `built`), and E's size and digest.

### Events

Each event uses section 09's envelope (1.3): `v`, `seq`, `t` and `type`, with the `step`, `line` and `point` that `core::Events` adds to every event inside a step. Each merge event also carries `ms`, the time its stage took.

```json
{"v":1,"seq":14,"t":"2026-11-12T13:14:03.120Z","type":"merge.base","step":"mff","line":171,"point":"","ms":38,"key":"0e91f9a9…","source":"built","pathways":42,"boxes":2902,"relations":2559,"nodes":1106,"edges":2291}
{"v":1,"seq":15,"t":"2026-11-12T13:14:03.124Z","type":"merge.split","step":"mff","line":171,"point":"","ms":4,"exceptions":49,"in_network":35,"split_out":21,"affected_nodes":12,"affected_boxes":220,"nodes":1129,"edges":2509}
{"v":1,"seq":16,"t":"2026-11-12T13:14:03.131Z","type":"merge.done","step":"mff","line":171,"point":"","ms":7,"graph":1,"nodes":1129,"genes":952,"complexes":93,"compounds":84,"edges":2509,"multi_gene_nodes":334,"largest":{"key":"hsa125467750","genes":52}}
```

- The times in these examples are illustrative.
- `multi_gene_nodes` counts gene nodes with two or more members (S2.28): 334 in the reference run. The 12 compound nodes with several ids are not counted.
- Warnings go only through the `warning` event, with a dotted `code` and a `message` (1.3), such as `merge.store_invalid` or section 09's `store.unavailable`.
- The merge makes no reported result, so its `step.finished` never carries a `report`.

### Errors

Failures are `core::Failure` exceptions with a stable code, which the interpreter catches once per command (09.C14). Generic conditions take section 09's and 01's codes (1.17):

- a missing or unreadable list member, not-merged file, confirmed-list file or `start` file is `input.missing`, `input.unreadable` or `input.list_member`, found by the check before the run (0021);
- a failed write is `io.write`;
- a store that cannot be created or written is the warning `store.unavailable`;
- output clashes are `output.clash`, and argument errors section 01's `batch.bad_argument` and other `batch.*` codes;
- a `y` or `n` flag that disagrees with a file's kind is the warning `network.flag_mismatch`, raised by section 02's `sniff`;
- a FuSION 1 batch that names no FSC gets section 01's check note `batch.no_fsc` (2A.9), and `mff` before any `cudf` its warning `batch.mff_before_cudf`.

The merge's own codes:

| Code | Kind | When |
|---|---|---|
| `merge.empty_list` | failure | a list given directly names no file. An empty GSEA selection never reaches `mff` (S7.03) |
| `list.duplicate_pathway` | failure, before the run | two different KGMLs in one list declare one pathway id, raised by the one list reader for `gsea` and `mff` alike (03.C6, 1.24) |
| `merge.type_conflict` | failure | S2.19, with the message above |
| `merge.legacy_mixed` | failure, before the run | a legacy file shares its list with another KGML or network file (03.C9) |
| `merge.no_sources` | failure | a FuSION network file in an `mff` list has no embedded sources and cannot be split exactly (03.C7). `rgx` on such a file fails with section 02's `network.settings_mismatch` |
| `merge.store_invalid` | warning | a store entry fails its checks; the run builds its own merge |

The merge makes no reported result of its own:

- the empty GSEA selection belongs to `gsea` (S7.03);
- a network without a path is the corridor's to report (S4.18).

A stop request is honoured between documents (`core::Stopped`). Ids of E that are in no box are listed in `exceptions.tsv` and counted in `merge.split`; they are not warnings.

### Console

Three lines per `mff`, with exact numbers (S1.27):

- `mff: 42 pathway files; base merge 0e91f9a9… built: 1106 nodes, 2291 edges`;
- `mff: exception set: 49 ids (43 up, 2 down, source, target, FSC, 3 not merged); 35 in the network, 21 split out of family nodes`;
- `mff: graph 1: 1129 nodes (952 genes, 93 complexes, 84 compounds), 2509 edges; 334 nodes hold more than one gene, the largest hsa125467750 with 52`.

### Portability (G3)

- **No processes, shells or signals.** The exception set is built in memory; FuSION 1 needed an `exec cat` (S1.17).
- **Paths and line endings** follow section 02's list reader (02.C12). The not-merged, confirmed-list and `start` files accept LF and CRLF. Every output is written in binary mode with LF, so the files are byte-identical on all three systems.
- **Duplicates by content.** Keys use SHA-256, so case-insensitive file systems (the macOS and Windows defaults) cannot hide a repeated file or invent one.
- **The store needs no lock.** `core::Cache::publish` renames a staged file into place, and the first writer wins (09.C13). Concurrent runs may build the same entry twice, which costs little in 2.0.
- **Path length.** Store paths are short (`merge/1/<64 hex digits>.xml` under the store's root), well inside Windows' 260-character limit for 09.C13's default roots.

### Determinism

- Nothing is random.
- Keys and the canonical order (02.C10) use byte comparison (`core::stable_compare`, 1.1), never locale collation.
- Hash maps serve lookups only; every output is sorted.
- The merge only compares scores, keeping the larger, so it parses and formats none. Section 02's reader parses them with the one decimal grammar through fast_float (3A.19, prompt log entry 167), and all 10,112 scores in the 163 KGMLs are integers from 0 to 999.
- The key holds the rules version but not FuSION's version (03.C8). FuSION's and the libraries' versions are recorded in `run.json` (E1).
- If documents are ever parsed on several threads, they are placed by list index, so the result does not depend on scheduling.

## Choices

### 03.C1 · How the merge learns the source, target and FSC

- **Proposed:** section 01's look-ahead (01.C17), as cross-section item 3A.1 resolved it (prompt log entry 165).
  - `mff` keeps its arguments `source=`, `target=` and `fsc=`, where `fsc` is one or more ids (06.C10). Each defaults to the facts that section 01's check collects for each point from the batch's own later `fb_rch` and `pathz3` lines (`BatchFacts`). An explicit argument wins.
  - The look-ahead also collects the confirmed up and down lists from later `pathz3` lines, for 03.C2.
  - The up and down genes come from the latest `cudf` observations, or from a named one (01.C16).
  - A FuSION 1 batch names no FSC on any line, so it passes its set through `start` (S1.17). That set already holds the up and down genes, the source and the target, and section 01's check notes the missing FSC (`batch.no_fsc`, 2A.9).
- **Alternatives:**
  - Run parameters, this section's first proposal. A `params` line, such as `params source=hsa1956 target=hsa3576 fsc=hsa3725`, is stated once, and its values default every argument of the same name in `mff`, `fb_rch` and `pathz3`.
  - Explicit arguments only. Every `mff` line repeats the source, target and FSC that `fb_rch` and `pathz3` also carry. Nothing is implicit, but three lines must be kept in step by hand, which S4.21 and S5.17 list as a FuSION 1 problem.
  - Deferred split. `mff` returns the base merge, and the corridor step splits it with its own endpoints. Then graph `0` after `mff` is a network that no step solves on: `wgx 0` would write the AP-1 and JNK families fused (see the split above). The corridor step would also need the FSC, which belongs to the solve (S5.17).
- **Why:**
  - S1.17, S2.13 and S8.22 require the merge to know these ids before it runs.
  - Every real batch has one source and one target (S4.01's census of all `fb_rch` lines) and tests at most one FSC; `r7` and `r8` of the fdr044 runs test none (S8.12).
  - The whole batch is bound before anything runs, so the check already knows the later lines. Each id then appears once, on the line that uses it, in the frontend's default pipeline (S8.20) and in an FSC sweep, and each sweep point gets its own E and its own split of one shared base merge.
- **Decision:** as proposed (prompt log entry 165)

### 03.C2 · Confirmed up and down genes join the exception set

- **Proposed:** the genes the solve takes as confirmed up or confirmed down (S5.07; the frontend's "Above Up" and "Below Down" overrides, S8.15) join E with the reasons `confirmed_up` and `confirmed_down`. They reach `mff` through section 01's look-ahead (03.C1). Not DE genes are already out of the observations, and so out of E (04.C4, which narrows S3.08). This amends S8.22, whose list of sources does not include them; accepted in prompt log entry 165, with 04.C4.
- **Alternatives:** E holds only `cudf`'s lists after Not DE, with the source, target, FSC and not-merged ids, exactly as S8.22 lists.
- **Why:**
  - A confirmed gene is an observation the solve must explain (S5.07: `up_use` includes `confirmed_up`).
  - Inside a family box it would mark the whole family node, which is the trap S2.03 describes.
  - No FuSION 1 batch passes a non-empty confirmed list: both name `empty_file` in every line of S5.01's census of `pathz3` lines. No old result changes.
  - Since 2026-11-09, 20 candidate batches (`r7` to `r26`) have edited a copy of the LFC file by hand so that PHLDA1 counts as up. A confirmed-up override can replace that edit only if confirmed genes are split out.
- **Decision:** as proposed, amending S8.22 (prompt log entry 165)

### 03.C3 · Where reusable merges live

- **Proposed:** three tiers:
  - memory, held by the batch interpreter for one process;
  - a shared, content-addressed store in each user's cache directory (09.C13), written once per key, used automatically and safe to delete;
  - each run's `merged_graph.xml`, which embeds its sources and records its base merge key, so a later run can start from it (0019, S8.23).

  The store is read as a cache, not a run output, so S1.26 and S8.21 stand (accepted in prompt log entry 166).
- **Alternatives:**
  - No shared store. Reuse happens within a process (sweeps) and by starting from a saved network; the frontend matches keys across a session's runs, as S8.23 describes. There is no shared directory to manage, and a new batch on the same pathways rebuilds.
  - A store in the runs root (`<root>/.fusion-store`), shared by everyone who uses that root, such as a lab server's users.
  - A store of parsed documents only, with the union redone on load. This is safe even if a rules change misses its version bump, and costs a few milliseconds more per run, but it is no longer the "merged once" of S2.22.
- **Why:**
  - S2.22 asks that runs on the same pathways share one merge. The store does that for every run, without one run having to name another's output.
  - The real runs share heavily: FuSION 1's 49 merges had 10 distinct contents, taking 1,481 s where 335 to 351 s would have done.
  - Content keys catch byte-identical copies, such as the fdr044 runs' copied curated KGML, and keep the v12.5 rescoring apart.
  - In 2.0 the time saved per run is small. A store hit reads one file of about 1 to 3 MB instead of parsing up to 6.85 MB of KGML and building, so tens of milliseconds by estimate. The case for the store is S2.22, not speed. Its main cost is one shared directory on three systems, which write-once files keep free of locks.
  - **The skeleton's rules.** The memory tier is a value the interpreter holds, as D1 provides, and the `merge` module keeps no state between calls (S5.29). The store holds no run output: every output, including the documents a rebuild needs, is in the run directory (S1.26, S8.21).
- **Decision:** as proposed (prompt log entry 166)

### 03.C4 · Format of a saved base merge

- **Proposed:** section 02's network file (02.C4) with its sources embedded, an empty exception set, the merge key in the header and no input paths. Store entries and the run's saved network then share one format, one writer and one reader. Accepted with 02.C4 and 03.C7 in prompt log entry 160.
- **Alternatives:**
  - A directory holding `manifest.json` and six TSV tables with headers: `pathways`, `boxes`, `groups`, `relations`, `nodes` and `edges`.
    - This would be needed if 02.C4's embedded sources were declined.
    - It is smaller: 0.37 MB for 42 pathways and 1.30 MB for 167 in the prototype, writing ids space-joined and subtypes comma-joined, one row per box, group, relation, node or edge.
    - It loads with a plain TSV reader.
  - One JSON file with the same content: 0.50 and 1.69 MB, and a slower DOM parse.
- **Why:**
  - The split needs the boxes and relations, not only the merged nodes (the {FOS, MAPK9, MAPK10, SP1} case above), and 02.C4 already embeds exactly those.
  - One format avoids a second writer, reader and version.
  - It costs about 18 % of the KGML bytes (section 02's measurement), and pugixml parses it in milliseconds, so a store hit stays cheaper than a rebuild.
- **Decision:** as proposed (prompt log entry 160)

### 03.C5 · A single KGML read is a one-file merge

- **Proposed:** `rgx` on a KGML, and a one-file `mff`, run the same union and split as any merge. Ids that sit in two boxes of one pathway then share a node, as they already do after a merge of two or more files. Section 02's draft assumes this. This amends S2.07's "keep" for single-file reads, whose nodes FuSION 1 makes per box key, with first-claim ids (S2.03); accepted in prompt log entry 166.
- **Alternatives:** keep FuSION 1's single-file read: one node per box key, with the first claim winning for an id in two boxes (S2.03).
- **Why:**
  - It gives one rule for every graph built from KGMLs, and one code path.
  - In the 167 KGMLs, 309 ids sit in two different boxes of the same pathway, in 71 files.
  - FuSION 1 already unions such boxes whenever two or more files are merged; the prototype matches its multi-file merges, as above.
  - Only the two `auxilliary/comparison_results` batches read a single KGML, the test file `test_example/realistic_mapk/mapk.xml`. FuSION 1's 65 one-file `mff` batches read legacy files, which section 02 reads apart (02.C5).
- **Decision:** as proposed, amending S2.07 (prompt log entry 166)

### 03.C6 · Two KGMLs with one pathway id in an `mff` list

- **Proposed:** the one list reader fails before the run, naming both files, with one code, `list.duplicate_pathway`, for `gsea` and `mff` alike, as S7.07 decided for `gsea`'s candidates (S7.06: one reader). The same file listed twice is read and merged once, with a warning. Accepted with cross-section item 1.24 in prompt log entry 166.
- **Alternatives:**
  - A warning, as section 02's list reader first proposed for every list. Both files are merged, their edges share one pathway id, and neither the pathway lists nor the provenance report can tell the two versions apart.
  - Merge both silently, as FuSION 1 does.
- **Why:**
  - The KGML directories hold the same pathway ids with different contents:
    - `updated_pathways/xml_files/` shares 163 file names and ids with `auxilliary/hsa_pathways_with_exp_score/`, two of them with different genes (S7.07);
    - `auxilliary/hsa_pathways_with_exp_score_v12.5/` has the same files with different scores.

    A list that mixes them is almost certainly a mistake.
  - The same rule for `gsea` and `mff` means a list that passes the GSEA step also merges.
  - No real list repeats an id.
- **Decision:** as proposed (prompt log entry 166)

### 03.C7 · FuSION network files in an `mff` list, and rebuilding a saved network

- **Proposed:**
  - A FuSION network file that embeds its sources (02.C4) contributes its documents to the union, as if its original files were listed. The result equals merging those files with the run's E, and the base merge can come from the store by key.
  - When `rgx` reads such a file and its recorded settings or exception set differ from the run's, section 02's compare-and-rebuild calls the merge the same way (`MergeStore::from_embedded`, then `split`); `load` restores a file exactly, and `mff` has no `from_saved` argument (2A.4, prompt log entry 164).
  - A file without embedded sources joins only when a per-node split is exact: when its exception set and settings equal the run's, or when it records no build settings and every node holds one id, as in `add`'s marked output (02.C17). Otherwise it is `merge.no_sources`, as `rgx` fails on such a file (section 02's `network.settings_mismatch`). A file built under another id map is refused too, since its documents' ids are already mapped.
  - Accepted with 02.C4 and 03.C4 in prompt log entry 160.
- **Alternatives:**
  - Take a file's nodes as boxes, as FuSION 1 does with `y` (S2.12), and warn. This splits per node, which cannot undo unions made through exception ids.
  - Refuse network files in lists, and only start from them.
  - Keep `from_saved` as an `mff` argument as well, which gives three ways to start from a saved network, two of them in `mff`.
- **Why:**
  - The per-node split is wrong on real data: 3 node identities in the reference run, such as {FOS, MAPK9, MAPK10, SP1}.
  - The frontend's pre-merged mode (S8.08) with additional edges (S8.24) is a list of one network file and one edge table. With embedded sources that merge is exact.
  - FuSION 1's own merged files are legacy files, which section 02 reads apart (02.C5), so this rule touches only files 2.0 wrote.
- **Decision:** as proposed (prompt log entry 160)

### 03.C8 · What the merge key holds

- **Proposed:** the SHA-256 of these lines:
  - a format line with the rules version;
  - section 02's build settings, by registry key, and the id map's digest when the run gives one;
  - one line per input, sorted, each line once: its SHA-256, plus, for an edge table, the default pathway id it takes from its file name (02.C2).

  Paths, list order, E, the label maps and FuSION's own version stay out. Section 09 states the rule once: the store keys on rules versions, and resume fingerprints on the build id (09.C5; cross-section item 3A.3, accepted in prompt log entry 166).
- **Alternatives:**
  - Add FuSION's version, as resume does. Every release then starts an empty store, which is safe even if a rule changes without a new rules version.
  - Key by paths and modification times. This needs no hashing, but it misses copies, which are common in the real runs, and sees an edit only through its time stamp.
- **Why:**
  - A release that changes no merge or reader rule keeps its entries. A forgotten version bump is caught twice: the golden test (Test hooks) fails when a fixture's merge changes, and builds with uncommitted changes never touch the store (09.C13).
  - Content keys find the real sharing: the 20 fdr044 runs that each copied the same curated KGML, and the seven `_string125` reruns, whose identical file names hide different scores.
  - Two identical edge tables under two names give two pathway ids (02.C2), so their line carries the pathway as well; otherwise the key would treat two different merges as one.
  - Hashing costs nothing extra, since `run.json` records every input's SHA-256 anyway (E1), computed while the run copies its inputs (09.C3).
- **Decision:** as proposed (prompt log entry 166)

### 03.C9 · A legacy file in an `mff` list

- **Proposed:** the version accepted with 02.C17 (cross-section item 3A.27, prompt log entry 163).
  - A legacy file (02.C5: the old master, or another unmarked file FuSION 1 wrote) may share its list with edge tables and with `add`'s marked output, which 02.C17 makes a FuSION network file (amending S2.30; accepted in prompt log entry 163).
  - Their ids bind to the legacy graph's nodes by first claim (S2.03), and an id the legacy graph lacks becomes a new one-id node. Nothing is unioned.
  - Any other KGML or network file beside a legacy file is `merge.legacy_mixed`.
- **Alternatives:**
  - Edge tables only, this section's earlier proposal. `add`'s marked output would then be refused beside a legacy file, although additional edges written by `add` are what the GUI's pre-merged mode makes.
  - `add` writes FuSION 1's exact, unmarked layout, which is read as legacy. 03.C9 then admits it, and any network merged with it is flagged legacy, in list mode too.
  - A legacy file strictly alone. Additional edges then need a 2.0 network, although S8.08 and S8.24 ask for them in both modes.
  - Union the legacy file's entries with the other sources like any merge. By S2.17's account of the fold, that is what FuSION 1 would have done to a list of the master and an `add` output, which no run ever merged (S8.08, S8.24): it unions boxes that share an id, so it would have joined the master's overlapping entries, such as HRAS's four. That departs from S2.27's "the FuSION 1 way".
- **Why:**
  - S2.27 keeps the legacy read apart from the raw pipeline, so it reproduces exactly the graph FuSION 1's later stages saw (02.C5).
  - The GUI's pre-merged mode with additional edges needs exactly this, since S8.08 and S8.24 keep that step in both modes.
  - All 65 FuSION 1 batches that read a legacy master name it alone.
  - First-claim binding is how FuSION 1 resolves any id against the master (S2.03).
- **Decision:** as proposed (prompt log entry 163)

## Interfaces with other sections

- **01 Batch language:**
  - The command table needs rows for `mff`/`merge_from_file`:
    - positional `mff <list> <thres> <y|n>`;
    - FuSION 1's `mff <list> xml <thres> <y|n> <maps…> -1`, whose `xml` token and map files are ignored with a warning (S2.16);
    - the named arguments of the settings table, section 02's `split_threshold`, `tool_written` and `kgml_dir`, and `list` and `observations`, which default to the latest shortlist and observations (01.C16). There is no `from_saved` (2A.4).
  - The `mff` handler lives in `batch` (01.C9). It resolves the list or the latest shortlist's paths (2B.10), fills `ExceptionInputs` from `BatchFacts` (01.C17), which also collects the confirmed-list files (03.C2), and calls `merge_step`.
  - The check before the run runs stages 1 to 3 when the list exists (02.C15), notes a FuSION 1 batch that names no FSC (`batch.no_fsc`, 2A.9), and warns when `mff` comes before any `cudf` (`batch.mff_before_cudf`).
  - 01.C11's move of `edges_merged_filename` calls `write_edge_report` on the latest `MergedNetwork`. After a legacy read there is no report, and the move writes nothing, with 01's note.
  - The interpreter owns one `MergeStore` per process and passes it to each `mff` step (D1), so sweep points share base merges.
  - A reported empty selection (S7.03) skips `mff`.
  - `start`'s exception file feeds `ExceptionInputs::start_file`.
  - `mff` registers exactly one graph id (S2.01).
  - `load` restores a saved network exactly (01.C6, 2A.4).
  - The `run` line carries run-level settings such as `cache` (2B.2).
- **02 Network:**
  - The merge uses `PathwayDoc`, `kept_relations`, `split_entry`, `NetworkBuilder` (`add_node`, `add_members`, `add_complex`, `mark_exception`, `add_edge`, `add_component_edges`, `attach`, `finish`, and 2B.11's `copy_node` and `copy_edge`), `Provenance`, `StepDocs`, `read_input_list` (with each line's text, 2B.12), `sniff`, `saved_network_diff` and `read_legacy`.
  - 2B.11's `<merge>` header element (`MergeHeader`) and a `SourceRef` without a path, for store entries (03.C4).
  - `Provenance` holds the relations behind each edge and the group entries behind each complex (2B.11). The base merge keeps its own table from each node to its boxes.
  - `StepDocs`: `mff` uses only the parse of its own inputs, and 02.C15 shares no parse between `gsea` and `mff` (4A.5).
  - The one list reader raises `list.duplicate_pathway` for `gsea` and `mff` (03.C6, 1.24), and `sniff` raises `network.flag_mismatch` (1.17).
  - A one-file `rgx` on a KGML is a merge (03.C5). `rgx`'s rebuild of a saved network calls `MergeStore::from_embedded` and `split`, and fails with `network.settings_mismatch` when no rebuild is possible (2A.4, 03.C7).
  - The reports take labels and pathway names from `Maps`; a KEGG pathway's name is its KGML's title (prompt log entry 205).
  - The legacy reader takes E as an `IdSet` (S2.12), a known FSC included (02 open question 8), and edge tables and `add`'s marked output in the same list attach through its lookup (03.C9).
  - The rules version in the key covers section 02's reader and build rules too, so a change to either must bump it.
- **04 Expression:** `Observations::observed()` gives the up and down genes after Not DE (04.C4), in network ids (`core::GeneId`). `merge` links `expression` for this type (1.2).
- **05 Corridor:**
  - Node keys come from the merge.
  - The corridor's source and target are in E, so they are nodes of their own. An endpoint not in E (a FuSION 1 batch) is shown by section 02's binding report.
- **06 Solve:**
  - The FSC (S5.17) and the confirmed lists (03.C2) reach `mff` from the later `pathz3` lines through section 01's look-ahead.
  - The solve can check that the FSC is in `MergedNetwork::exceptions`, and warn when it binds to a node with other members.
- **07 Results:** each edge's pathways come from the network; the provenance report is this section's.
- **08 Enrichment:**
  - It shares the list reader through section 02, with `list.duplicate_pathway` (1.24), and its shortlist is `mff`'s default list (01.C16), taken by its resolved paths (2B.10).
  - Nothing parsed passes between `gsea` and `mff` (S7.19, 4A.5), but the store can serve `mff` without a parse.
- **09 Runs:**
  - The core vocabulary (1.1): `core::Failure`, `core::Stopped`, `core::Outcome<T>`, `core::Context`, `core::Events`, `core::Cache`, `core::OutputFile`, `core::Sha256` and `core::stable_compare`.
  - The store's root, `Cache::publish`, the bypass for unclean builds and `run.cache` (09.C13), and the rule of 3A.3, stated once in section 09.
  - The `mff/` step directory (09.C2), writes through `Files::create` (2A.6), and output-path placement (2B.3).
  - The step's `details` in `run.json`: the merge key, the base merge's source, and E's digest and size. The inputs' SHA-256 are computed once (09.C3), for both `run.json` and the key.
  - The events envelope and the `warning` event (1.3); the registry keys `merge.*` and `run.cache` (1.16).
  - The saved-step entry for the merge: `merged_graph.xml`.
- **10 Frontend:**
  - The frontend passes the not-merged file, the source, the target and the FSC; its confirmed overrides reach `mff` through the look-ahead (03.C2); `kgml_dir` is section 02's.
  - Its pre-merged mode names a saved network (03.C7), or a legacy file with edge tables or `add`'s output (03.C9).
  - Reuse needs no session logic while `run.cache` is on.
- **11 Build:** `fusion_merge` links `network` and `expression` (1.2, 11.C6); SHA-256 comes from `core`, which `run.json` needs anyway; the merge adds no library.
- **12 Tests:** the hooks below, with the independent oracle `exception_merge` (12.C14).

## Test hooks

- **Library-level (Catch2):**
  - `merge_direct` on synthetic documents: chains of overlapping boxes, overlaps inside one file, groups of groups, single-component complexes, self-loops, undirected pairs, component edges, and a type conflict and its message.
  - `assemble_exceptions` with every reason.
  - `merge_key`: list order and paths do not change it; an edge table renamed does.
  - Synthetic documents need no KGML, which S8.31 keeps out of the repository.
- **Properties** (section 12's harness, 12.C14), in its three layers: every exception subset of small synthetic sets; 1,000 random cases per run and 100,000 weekly; and the real merges, with the 167 KGMLs split by random subsets of the EGF up and down genes:
  - `split(base(R), E)`, `merge_direct(R, E)` and the test kit's `exception_merge(R, E)` give byte-identical canonical networks, exception marks included. The cases need thresholds 0, 1, 2, 3 and 15000, and E biased towards members of multi-id nodes and group components. The prototype's first version failed only at threshold 2.
  - Order independence.
  - Each id in exactly one gene or compound node (stage 2's open question 4).
  - No isolated node.
  - Listing a file twice changes nothing.
  - A stored and reloaded base merge splits to the same network as a fresh one.
  - `split` with an empty E returns the base network.
- **Reference** (section 12's runner, decision 0023):
  - The real merges are compared with FuSION 1's re-read graphs (graph 84), matching nodes by member sets (prompt log entry 164), and edges between shared nodes by key, score and pathways.
  - The expected differences are section 12's XD-03 to XD-06 and XD-16: S2.08's recovered ids and the unions they widen; S2.21's duplicates (6, 7, 12, 12 in the four runs measured); S2.10's scores; S2.09's component pathways (6); and S2.11's boxes reached only through indirect relations, with the nodes they split or drop and the keys they move, such as `hsa10342` to `hsa1436`. Also expected: 22 isolated nodes no longer written, and single-file unions (03.C5).
  - The prototype found no other difference in four runs.
  - Keys are compared with the re-read graph's, not with the names FuSION 1 wrote, whose complex keys are stale in 10 entries of the reference run.
- **Golden:** the canonical digest of a fixture's base merge is pinned. When it changes, the test fails until `rules_version` changes too, so the store cannot serve merges made under old rules.
- **Store:**
  - Two processes building one key at once on each system both succeed and leave one entry.
  - A truncated entry is bypassed with `merge.store_invalid`.
  - An unusable store only warns (`store.unavailable`), and `run.cache=false` reads and writes nothing.
- **Platforms:** the same inputs give byte-identical `merged_graph.xml` and `exceptions.tsv` on Linux, macOS and Windows.
- **Benchmarks** (12.C13): the 42-, 87-, 90-, 163- and 167-pathway lists, from scratch, from the store and split. FuSION 1's baselines are 3 s, 22–53 s and 24 s for the first three. For 163 pathways it took 122–126 s, but before the indirect discard; FuSION 1 never merged the 167 list.
- **End to end:**
  - A FuSION 1-style batch (`start` with an exception file, `mff … xml 1 n <map> -1`, `wgx`, `rgx … y 1`) gives the same network as the named form with the same E.
  - An FSC sweep reports `"source":"memory"` for every point after the first.
  - `load` of a saved `merged_graph.xml` restores it exactly; `rgx` of it under another FSC rebuilds it, and the result equals a fresh merge with that FSC.

## Open questions

All four are closed.

1. **FuSION 1 batches never name their FSC.** Closed in prompt log entry 165 (keep as designed): such a batch keeps its own `start` exception file, and section 01's check notes the missing FSC (`batch.no_fsc`, 2A.9); this also closed 01 open question 5.
2. **S2.08 and S2.11 change which genes form a node, and so some keys.** Closed in prompt log entry 164: the changes are expected differences (decision 0023), S5.06 reports any line of an old file that names a moved key, no aliases are kept, and Phase 3 matches old and new graphs by member sets.
3. **The shared store and S1.26 and S8.21.** Closed in prompt log entry 166: the store is read as a cache, not an output, and S1.26 and S8.21 stand.
4. **Choices that amend decided items.** Closed: 03.C2 amends S8.22 (accepted in prompt log entry 165), and 03.C5 amends S2.07 (accepted in prompt log entry 166).
