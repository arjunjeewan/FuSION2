# 05 · Encoding and Pareto solve

## Scope

This stage covers `pathz3`/`get_path_with_z3`. That is its handler in `Linux/src/main.cc:4365-5073`, the helpers `make_smt_dump_dir_for` (`main.cc:866-887`) and `compare_pair_on_second_descending` (`main.cc:1002-1007`), and everything the handler reaches in `Linux/src/GraphManagerNew.cc`: the file readers (`18624-18819`), pre-processing whose results nothing reads (`2468-2518`, `8378-8398`, `8449-8540`, `16414-16455`), the encoding (`27684-28970`), the Pareto search (`29342-29883`) and the model printers (`30729-31017`). It also covers the bit-width macros in `Linux/include/macros.h:43-50`. It starts from the corridor that stage 4 hands over as graph `0` (S4.21). It ends with the files that stage 6's `psp`, `pex` and `plot_po.sh` read; how they are drawn is stage 6's. Stage 1 owns tokenising, `$` expansion and the 1,000,000-slot token list (S1.05, S1.07), the launch directory (S1.26), failure policy (S1.28) and compile-time flags (S1.29). Stage 3 owns the up and down lists (S3.09, S3.12). In this command, `macOS/src/main.cc` differs from Linux only in the optional-argument parser (S5.02). `GraphManagerNew.cc`, `GraphManagerNew.h` and `macros.h` are byte-identical (`cmp`).

Names and evidence used below:
- "The reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`: corridor of 115 nodes and 370 edges, EGFR → CXCL8, FSC JUN, bound 10.
- "The full run" is `runs/egfr_cxcl8_jun_b10_nr8_er8/`: corridor of 592 nodes and 2138 edges.
- "fdr050" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr050/`, a 50.7 h run.
- "The batch census" is a replay of FuSION 1's `let` and `$` expansion (S1.07, S1.08) over all 89 `*.batch` files in the repo. 86 of them call pathz3, in 170 lines.
- File paths under `run/` are relative to a run directory.
- FuSION 1 was not built or run. The one timing figure (S5.08) comes from a copy of one function, compiled the way FuSION is.

## Items

### S5.01 · `pathz3` command line

- **FuSION 1:** `Linux/src/main.cc:4365-4513` (batch parse `4482-4513`, interactive prompts `4421-4479`); help line `main.cc:905`
- **What it does:** The batch form has 28 positional tokens, then up to five optional ones (S5.02):

  `pathz3 <graph> <mode> <pairs> <e_lo> <n_lo> <e_hi> <n_hi> <bound> <prefix> <up> <down> <essential_nodes> <avoid_nodes> <essential_edges> <avoid_edges> <active> <inactive> <confirmed_up> <confirmed_down> <relaxed_nodes> <nonrelaxed_nodes> <relaxed_edges> <nonrelaxed_edges> <fold_change> <count> <explore> <timeout1> <timeout2> [<po_points>] [<coexp_csv> <coexp_thr> <frozen_thr> <enabled> <seed>]`

  - `<graph>` is parsed with stoi, and 0 means the latest graph (`4388-4389`). An unknown id prints `No such graph` and calls `exit(1)` (`4392-4396`), which ends the whole run.
  - `<mode>`: 0 computes the frontier and 1 tests given points (S5.28). Any other value prints `Invalid mode for pathz3`, with no newline, and the command ends (`4914-4918`).
  - The four relaxation bounds come in the order edge-lower, node-lower, edge-upper, node-upper (variables w, x, y, z; `4484-4487`, `4861-4864`).
  - `<bound>` is the path bound b (S5.11). `<prefix>` is the stem of the solutions, relaxations and mincuts files, used as given (`OUTPUT_FILES_DIR_PREFIX` is `""`, `macros.h:31`).
  - Fifteen input files follow: the connect pairs (S5.04) and 14 lists (S5.05, S5.06, S5.07). `<fold_change>` only names the dump directory (S5.23).
  - `<count>` and `<explore>` are covered in S5.26. `<timeout1>` and `<timeout2>` are milliseconds, parsed with stoul (S5.21).
  - `<po_points>` is present only in mode 1, and shifts the optional tokens by one.
  - Real line, from the reference run (`run_console.log:268`, paths shortened): `pathz3 0 0 …/run/connect_pairs 0 0 8 8 10 …/run/temp_w …/run/up_reg …/run/down_reg …/run/essential_nodes_file_w_hsa3725 …/run/empty_file …/run/empty_file …/run/empty_file …/run/active_nodes_file_w_hsa3725 …/run/empty_file …/run/empty_file …/run/empty_file …/run/empty_file …/run/nonrelaxed_nodes_file_w_hsa3725 …/run/empty_file …/run/empty_file …/inputs/egf_hela_t120_lfc.tsv 1 1 3600000 14400000 …/inputs/hsa_stringdb_coexpression_links_over400.csv 700 800 1 42`
  - Batch census:
    - All 170 lines use mode 0 and lower bounds 0 0.
    - Upper bounds (e, n) are (1, 1), (2, 2), (4, 4), (7, 7), (8, 8), (10, 5), (10, 10), (12, 3), (12, 5), (15, 3), (15, 15) and (20, 4), so the node bound reaches 15.
    - count/explore is 1/1 in 166 lines and 200/100 in 4.
    - 158 lines carry all five optional tokens, 6 carry three and 6 carry none.
    - These counts are as of the draft. A batch added on 2026-11-11, `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/resolve_wo_n7_e0/resolve_wo_n7_e0.batch`, adds a 171st line, with lower bounds (0, 7) and timeouts 3600000/86400000.
    - Eight of the 14 lists name `empty_file` in every line: essential and avoid edges, inactive, both confirmed lists, both relaxed lists and non-relaxed edges. The two exceptions are avoid-edge files in two lines (S5.06).
  - Interactive mode prompts for each value with `cin` and never reads the optional tokens, so it always runs with the constraints off and seed 42. S1.03 drops this mode.
- **Problems:**
  - The line has 28 to 34 positional tokens, and 8 of its lists are empty in every real batch.
  - A missing or non-numeric numeric token throws from stoi or stoul and aborts the run (S1.28). The token count is never checked, so a short line reads `""` (S1.05).
  - `No such graph` exits the process, while other commands' errors let the batch carry on.
  - The help line (`main.cc:905`) shows only `<args ...>` and the optional tokens, so the binary documents none of the 28 positional ones.
  - `main.cc:4372-4373` initialise two unused `size_t` variables from uninitialised ints. This is undefined behaviour, with no effect in practice.
- **Proposed call:** fix. Keep the positional form for FuSION 1 batches (decisions 0017 and 0023) and add names (decision 0018). Validate every argument before the solve starts (decision 0021), and make an unknown graph a command error.
- **Decision:** fix, as proposed (prompt log entry 065)

### S5.02 · Optional arguments: the Linux blank-slot bug and the macOS fix

- **FuSION 1:** Linux `Linux/src/main.cc:4515-4559`; macOS `macOS/src/main.cc:4516-4587`
- **What it does:** After the fixed tokens (index 29, or 30 in mode 1), batch mode reads up to five optional tokens:
  - coexpression CSV, default `""`;
  - coexpression threshold, stof, default -1;
  - frozen-edge threshold, stof, default -1;
  - enable flag, stoi, nonzero means on, default 0;
  - Z3 seed, stoi, default 42.

  The guard `cmd_index < token_cmd.size()` is always true (S1.05). After the parse, the command prints `[new-constraints] enabled=<0|1> z3_random_seed=<n>`.
  - **Linux:** a slot is read only if its token is non-empty. `cmd_index` advances only when the slot is taken, and for the numeric slots only when the conversion succeeds.
    - An empty token, which an empty-valued `$VAR` produces (S1.07), therefore stops the parse: every later slot re-reads the same empty token and keeps its default. A non-numeric token does the same through the `catch`.
    - Example: in `… $COEXPRESSION_CSV 700 800 1 42` with `$COEXPRESSION_CSV` defined empty, the CSV stays `""`, both thresholds stay -1 and the flag stays 0. The frozen-edge constraints are off, although 800 and 1 were given.
  - **macOS:** each slot consumes its token whether or not it is blank. A blank keeps only that slot's default, and a non-numeric value keeps the default but still advances. The same example gives CSV `""` (no coexpression pairs, S5.15), frozen threshold 800 and flag 1.
  - In both versions stoi accepts a numeric prefix, so `1x` reads as 1.
- **Problems:**
  - The Linux behaviour silently drops every argument after a blank one.
  - The census found no line where the two parsers differ: all 170 lines give the same five values under both. So no logged result is affected.
  - The case arises through the GUI template, which writes `let $COEXPRESSION_CSV %__a21__%` (`Linux/frontend/src/gui4.java:4175`). If the GUI can leave that field blank, the token is empty; stage 8 should confirm whether it can.
  - Both versions keep the default for a non-numeric value without a message.
- **Proposed call:** fix. Take the macOS behaviour, where each position consumes its token, and reject a non-numeric value with a message.
- **Decision:** fix as proposed: each position consumes its token, as on macOS, and a non-numeric value is an error (prompt log entry 065)

### S5.03 · The enable flag defaults to off, which disables constraints that older batches used

- **FuSION 1:** `Linux/src/main.cc:4530`, `4561-4563`; `Linux/src/GraphManagerNew.cc:27797-27805`, `28603-28604`, `28713-28714`
- **What it does:**
  - `new_constraints_enabled` gates both families at once. When it is 0, the CSV is not even loaded, and stderr gets `[new-constraints] disabled (coexpression + frozen-edge constraints skipped)`.
  - Each family also has its own off state: no CSV or a negative coexpression threshold loads no pairs, and a negative frozen threshold emits nothing.
  - Batch census: 126 lines pass flag 1 and 32 pass 0, for example the `*_off.batch` comparison runs.
- **Problems:**
  - Six lines pass a CSV and both thresholds but no flag:
    - `runs/real_experiments/experiment_1.batch:202` and `:238`;
    - `experiment_2.batch:201` and `:237`;
    - `auxilliary/comparison_results/batch_constrained.batch:70` and `:79`, under the comment "constraints ON".
  - They ran before the flag existed, and they did use the constraints: `runs/real_experiments/experiment_1/run.log:102` has `[coexpression] emitted 40 clauses`, and `auxilliary/comparison_results/debloated_constrained/run.log:88-89` has `[coexpression] emitted 55 clauses` and `[frozen-edge] emitted 31 clauses`.
  - Re-run today, the same lines solve without the constraints, and only a stderr line says so.
- **Proposed call:** fix, following decision 0021. A given flag still decides: 0 turns both families off, as the 32 `off` lines need. With no flag, a given CSV and threshold turn coexpression on, and a given frozen threshold turns frozen edges on. The run records which families it used.
- **Decision:** fix as proposed: a given flag decides; with no flag, the inputs given turn each family on; the run records the families used (prompt log entry 065)

### S5.04 · The connect-pairs file and the active source

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:18624-18650` (`read_pairs_file`), `27762-27765`, `27807-27809`; `Linux/src/main.cc:4579-4585`
- **What it does:**
  - The file has one pair per line, `<source id> <target id>`, separated by whitespace; anything after the second token is ignored.
  - Both ids go through the corridor's lookup (S4.15), and a pair with an unresolved end is skipped silently. Sources and targets are collected into two sets, and the command prints `<n> connect pairs`.
  - The first pair's source is the main source, and only it is asserted active (`__nodeactive_s`, `27807-27809`).
  - Real file, `run/connect_pairs` of the reference run: `hsa1956 hsa3576`. Every batch writes exactly one pair, with `exec echo $SRC_NODE $TGT_NODE > $CONNECT_PAIRS_FILE`.
- **Problems:**
  - A missing file only prints `File <name> could not be opened` on stderr.
  - With no resolvable pair, `connect_pairs_to_consider.front()` is called on an empty list (`27765`, `27820`). That is undefined behaviour, typically a crash, and it comes after `0 connect pairs` was printed.
  - With several pairs, only the first source is forced active; the source comment reads "current thing works only for one source-target pair" (`27762`). Several targets also open the crash of S5.10.
  - The pair repeats the corridor's source and target, which the batches keep in step by hand (S4.21).
- **Proposed call:** fix. Take the source and target from the corridor (S4.21) or from named arguments, and require them to resolve. Support one pair until multi-pair semantics are designed.
- **Decision:** fix, as proposed (prompt log entry 065)

### S5.05 · Node list files and what each one asserts

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:18688-18708` (`read_data_file_into_set_of_ints`), `28306-28411` (`get_query_assertions_as_constraints_with_z3`); `Linux/src/main.cc:4660-4692`
- **What it does:**
  - Every node list is read the same way: up, down, essential, avoid, active, inactive, confirmed up and down, relaxed, and non-relaxed.
  - Each list is read as whitespace-separated ids. Each id is resolved in the corridor (S4.15), unresolved ids are dropped silently, and the result is a set of node ids. A missing file prints `File <name> could not be opened.` and gives an empty set; the batches pass `empty_file` for "nothing".
  - Assertions:
    - essential gives `__node_v` (present); avoid gives `¬__node_v`;
    - active gives `__nodeactive_v`; inactive gives `¬__nodeactive_v`;
    - non-relaxed gives `¬__noderelax_v`;
    - the relaxed list is read and passed down, but nothing is asserted from it (no loop over it in `28306-28411`);
    - the confirmed lists change the observations (S5.07).
  - Real file, `run/essential_nodes_file_w_hsa3725` of the reference run: `hsa1956`, `hsa3576`, `hsa3725`, one per line.
  - Console: `Nodes to avoid:`, `Edges to avoid:`, `Nodes to be active:`, `Nodes to be inactive:`, `Nodes not to relax:` and `Edges not to relax:`, each with a count, then a blank line. Essential nodes, essential edges and the relaxed lists are not counted.
- **Problems:**
  - The relaxed-nodes list, and S5.06's relaxed-edges list, are accepted and ignored. Every batch passes the empty file for both.
  - The console gives counts, not names, so a family-node binding passes unnoticed (S2.03).
  - Contradictions such as an id that is both essential and avoided, or both active and inactive, are not checked; the solve is simply UNSAT.
  - A rep id with no node would insert node id -1 (`18704`), and the later `.at("__node_-1")` would throw. S4.15 found no such id, so this is latent.
- **Proposed call:** fix. Keep the list semantics and drop the relaxed-nodes and relaxed-edges inputs. Report which node each id bound to (S2.03), and reject contradictory lists before solving.
- **Decision:** fix, as proposed (prompt log entry 066)

### S5.06 · Edge list files

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:18739-18819` (`read_edge_data_file_into_set_of_ints`), assertions `28358-28374`, `28403-28410`; `Linux/src/main.cc:4664-4665`, `4683-4684`
- **What it does:**
  - The four edge lists are essential, avoid, relaxed and non-relaxed. Each line is `<source id> <target id> <subtypes>`.
  - The two ids are mapped to rep ids, and the line becomes the label `<src_rep>__<tgt_rep>__<subtypes>`. Every edge of the graph is labelled the same way, with its subtype list joined by `_` in stored order (`-` when it has none), and the edges whose labels appear in the file are selected.
  - Assertions: essential gives `__edge_e`, avoid gives `¬__edge_e`, non-relaxed gives `¬__edgerelax_e`, and relaxed gives nothing.
  - Real file: `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/run/avoid_edges_file_wo_hsa6774` has 10 lines such as `c_s_hsa6772_c_e hsa4170 expression`. The console then says `Edges to avoid: 3` (`run_console.log:547`), so 7 lines matched nothing, without a message.
- **Problems:**
  - The subtype field must match the stored order exactly: `activation_phosphorylation` can match where `phosphorylation_activation` does not. Nothing reports a line that matched no edge.
  - `edge_type` is declared outside the loop (`18753`), so a line with only two ids reuses the previous line's subtypes.
  - Each file costs a scan that labels every edge of the graph: four scans per call, eight per batch. This is small, but it is avoidable.
- **Proposed call:** fix. Require the subtype field, and compare it as a set, since a user cannot know the stored order (S2.04's ordered identity stays inside the graph). Report every line that matches no edge.
- **Decision:** fix, as proposed (prompt log entry 066)

### S5.07 · The observations and the node-bound clamp

- **FuSION 1:** `Linux/src/main.cc:4633-4634`, `4660-4679`, `4693-4714`; `Linux/src/GraphManagerNew.cc:18652-18686` (`read_data_file_into_map`)
- **What it does:**
  - `up_use = ((up − avoid) − confirmed_down) ∪ confirmed_up` and `down_use = (down − confirmed_up) ∪ confirmed_down`. Only the up set loses avoided nodes.
  - The command prints `Up: <|up_use|> Down: <|down_use|>`. `up_down_size` is `|up_use| + |down_use|`, so a node in both sets counts twice.
  - If the node-lower or node-upper bound exceeds `up_down_size`, it is lowered to it, and the command prints `Adjusting node relaxn bound to be consistent with max diff expr genes in reachable graph (<n>)`. The edge bounds are not clamped.
  - Before all this, both lists are read a first time into an `unordered_map` that nothing uses (S3.13).
  - Reference run: `Up: 3 Down: 0` with the FSC and `Up: 2 Down: 0` without it, because the avoided JUN leaves the up set. The node bound 8 becomes 3 and 2 (`run_console.log:281-282`, `350-351`).
- **Problems:**
  - `up_down_size` counts the source, the target and nodes outside the encoding (S5.10), none of which can be relaxed. In the reference run's with-FSC solve only IRS2 can be relaxed, because CXCL8 is the target and JUN is forced, yet the search window is n ∈ [0, 3] (`run/…_w_…_limits_timefile.txt:2`).
  - An avoided down-regulated node stays in the down set but costs nothing, since absence is consistent with down (S5.12). An avoided up-regulated node is removed. The outcome is the same, but nothing states the asymmetry.
  - The unused first read parses both lists a second time.
- **Proposed call:** fix. Keep the set arithmetic and drop the unused read. Clamp the node bound to the number of observations that can actually be relaxed, and print that number.
- **Decision:** fix, as proposed (prompt log entry 066)

### S5.08 · Work whose result nothing uses

- **FuSION 1:** `Linux/src/main.cc:4587-4613`, `4769-4836`; `Linux/src/GraphManagerNew.cc:8449-8540` (`convert_to_undirected_graph`), `2468-2518` (`preprocess_graph_for_mincut`), `8378-8398` (`fill_adj_matrix_for_graph`), `16414-16455` (`compute_transitive_closure`), `27750-27751`, `27838-27923`, `27949-28053`, `28911-28970`
- **What it does:**
  - Before encoding, pathz3:
    - registers an undirected copy of the corridor as a new graph, so `0` then names that copy (S4.20), and assigns sequence numbers on both graphs;
    - fills an n×n adjacency matrix on the corridor object and computes its transitive closure;
    - allocates four n×n int matrices for call and edge levels and counts, which the constraint pass updates and nothing prints (`Not printing matrices in the current run`);
    - opens `<prefix>_mincuts` and never writes to it (`run/temp_w_mincuts` is 0 bytes).
  - During encoding it builds a disjunction for each visited pair (`to_OR`, `right_expr`) and then discards it, because the `s.add` lines are commented out (`27990`, `28052`).
  - It also asserts two vacuous totals: a free `__nodebound ≤ |N|` with `Σ present nodes ≤ __nodebound`, and the same for edges (`28911-28970`). Every assignment satisfies them, because neither sum can exceed |N| or |E|.
- **Problems:**
  - Avoidable cost. The closure allocates a fresh n×n matrix at each of its n steps, so it takes O(n³) time.
  - A copy of the function, built like FuSION with `-g` and no `-O` (`Linux/Makefile:10`), took 0.23 s at n = 115 and 23.8 s at n = 592. So the full run spent about 24 s of each pathz3 call, 48 s per batch, on a matrix nothing reads.
  - The two vacuous totals put adders over all nodes and all edges into every query. They are two of the 1321 assertions in the reference run's 462 KB dump, 7.8 KB and 26 KB long.
  - The undirected copy doubles memory and shifts every later id (S4.14).
- **Proposed call:** drop. None of it changes any output, and the vacuous totals can go without changing satisfiability.
- **Decision:** drop, as proposed (prompt log entry 066)

### S5.09 · Solver, variables and bit widths

- **FuSION 1:** `Linux/src/main.cc:4720-4759`; `Linux/src/GraphManagerNew.cc:27684-27725`; `Linux/include/macros.h:43-50`
- **What it does:**
  - Each call makes one `z3::context` and one `z3::solver s(c, "QF_BV")`.
  - Variables for every corridor node v: the Booleans `__node_v` (present), `__nodeactive_v` and `__noderelax_v`, and the bit-vector `__nodedistance_v`.
  - Variables for every edge e: `__edge_e` (selected), `__edgerelax_e` and `__edgepathflag_e`.
  - Single bit-vectors: `__pathbound`, `__nodebound`, `__edgebound`, `__nodestatusrelaxbound` and `__edgestatusrelaxbound`.
  - v and e are the corridor's session-wide ids (S4.14); `__node_39128` is EGFR in the reference run.
  - Widths:
    - distance and path bound: `ceil(log2(b+1))`, which is 4 for b = 10;
    - node bound: `ceil(log2(|N|+1))`;
    - edge bound and edge-relaxation count: `ceil(log2(|E|+1))`, which is 9 for 370 edges;
    - node-relaxation count: `ceil(log2(up_down_size))+1`, which is 3 when there are 3 observations.
  - Variables are created for every node and edge, but only the ones that appear in assertions reach the solver. The reference dump declares variables for 108 of the corridor's 115 nodes and 364 of its 370 edges (`PO_constraints_nr_0_er_1.smt2`).
- **Problems:**
  - Bound values are built with `c.bv_val(value, width)`, which wraps modulo 2^width. The edge bound is never clamped (S5.07), so an upper edge bound of 2^width or more silently becomes a smaller bound. That can happen on a small corridor: 3 edges give width 2, so a bound of 4 becomes 0.
  - `up_down_size = 0` gives `ceil(log2(0))`, which is undefined (S3.10). With no observation in the corridor the encoding is malformed.
  - b = 0 gives width 0. Stage 4 already yields an empty corridor in that case (S4.18).
- **Proposed call:** fix. Keep the variable families and the bit-vector encoding. Clamp or reject an edge bound above |E|, and stop with a message when no observation reaches the corridor.
- **Decision:** fix, as proposed (prompt log entry 067)

### S5.10 · Which nodes and edges enter the encoding

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:27747-27810` (`generate_connectivity_constraints_with_z3`), `27812-28056` (`get_connection_constraints_using_mincut_with_z3`), `28064-28104`, `28330-28347`
- **What it does:** Constraints are posted only over two sets, `nids_to_consider` and `eids_to_consider`. They are built as follows:
  - a worklist starts with the connect pairs;
  - for a pair (u, t), every out-edge of u goes into the edge set and its endpoints into the node set, and each out-edge (u, w) with w ≠ t pushes the pair (w, t);
  - each pair is processed once, using a function-static set (S5.29);
  - the result is every edge reachable from a source along a path that does not pass through the target, plus the endpoints of those edges;
  - essential nodes join the node set whether or not they are reachable (`28330-28347`).

  In the reference run, 108 of the 115 corridor nodes and 364 of the 370 edges are in the sets. Among those left out are the 3 isolated complex members (S4.12) and the target's out-edges.
- **Problems:**
  - If a reached node w ≠ t has no out-edges, the code asserts `¬__connect_w_t` through `.at()` (`27925-27930`). No `__connect_` variable is ever created (`main.cc:4752` is commented out), so `std::out_of_range` aborts the run. A corridor cannot trigger this, because by S4.09's keep rule every reached node except the target has an out-edge. A non-corridor graph, or a second target without out-edges, would.
  - Nodes outside the sets get variables but no constraints, so they are never present.
  - Up- or down-regulated nodes outside the sets, such as isolated members, count in `Up:` and in the clamp (S5.07) but never in the relaxation count (S5.12). S4.12 says such a member "still counts as an observation to be explained", but in the objective it does not count.
  - The function's name and comments speak of mincuts, but the mincut call is commented out (`27884`). What runs is a forward reachability walk that rebuilds strings and z3 expressions for every pair.
- **Proposed call:** fix. Compute both sets with a plain forward search from the source that stops at the target, inside the corridor. List the observations that fall outside it, as S4.16 asks.
- **Decision:** fix, as proposed (prompt log entry 067)

### S5.11 · Path constraints under the bound

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28064-28141`, `28143-28304`
- **What it does:** Over the considered sets:
  - a selected edge implies that both its endpoints are present (`28098`);
  - a selected edge implies `dist(src) <u dist(tgt)`, compared unsigned (`28118`);
  - for every target, `dist(t) ≤u __pathbound`, and `__pathbound ≤u b` (`28121-28141`);
  - a present source has a selected out-edge, a present target has a selected in-edge, and every other present considered node has both (`28143-28304`); only considered edges count.

  Together these make the selected edges a DAG in which every present node lies on a selected path from a source to a target, and each such path has at most b edges, because distances strictly increase and the target's distance is at most b. The source's distance is free, not fixed at 0. Nothing here asserts that the source and target are present; the batches put both in the essential list (S5.17). A self-loop's distance constraint cannot hold, so a self-loop is never selected.
- **Problems:**
  - A considered node that is neither a source nor a target, and has no in-edges or no out-edges, trips `assert(!inlist.empty() && !outlist.empty())` (`28256`). Asserts are compiled in (S4.22). This happens when an isolated corridor node is named essential, since S5.10 adds essential nodes to the set.
  - Example, traced from the code and not run: if an FSC id binds to one of S4.12's isolated members, such as RELA in the full run's corridor, the run aborts.
- **Proposed call:** fix. Keep the constraints. An essential node that cannot lie on a path makes that case infeasible, reported with a message before solving, instead of aborting the run.
- **Decision:** fix, as proposed (prompt log entry 067)

### S5.12 · Node relaxations

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28456-28524` (`get_node_activation_status_relaxation_constraints_with_z3`)
- **What it does:** For each considered node v that is in `up_use` or `down_use` and is neither a source nor a target:
  - **up-regulated:** `__noderelax_v ⇔ ¬(__node_v ∧ __nodeactive_v)`, so an up gene is explained only if it is present and active;
  - **down-regulated:** `__noderelax_v ⇔ (__node_v ∧ __nodeactive_v)`, so a down gene is explained if it is absent, or present and inactive;
  - a node in both sets is treated as up, by the if/else order (S3.07);
  - the count `Σ ite(condition, 1, 0)` is asserted `≤u __nodestatusrelaxbound`.

  Real assertion from the reference dump, for IRS2 (up): `(assert (let (($x5255 (and __node_39181 __nodeactive_39181))) (let (($x5256 (not $x5255))) (= $x5256 __noderelax_39181))))`.

  The function also writes `diff_expr_nodes.txt` in the launch directory, one line per counted node, for example `[+] hsa8660 IRS2` (reference run).
- **Problems:**
  - Sources and targets are exempt, so the target's own expression is never part of the objective, and no output says so.
  - Absence is consistent with down-regulation but is a relaxation of up-regulation. An up gene on no path always costs 1, and a down gene on no path never does. This is a modelling choice that no output states.
  - `diff_expr_nodes.txt` has a fixed name and is truncated by each call, so only the last solve's list survives. No batch moves it (S1.26).
- **Proposed call:** fix. Keep the rule, and write the counted observations, and the excluded ones (source, target, outside the encoding), to the run's output.
- **Decision:** fix as proposed; the outputs also state the modelling choices for absent nodes and exempt endpoints (prompt log entry 067)

### S5.13 · Edge relaxations and edge signs

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28526-28599` (`get_edge_activation_status_relaxation_constraints_with_z3`), `1856-1874` (`edge_has_subtype`)
- **What it does:** For every considered edge e = (u, v):
  - **Inhibition family**, when the subtype list holds the token `inhibition` or `repression`: relaxed ⇔ both ends present and either both active or both inactive.
  - **Activation family**, every other edge: activation, expression, phosphorylation, dephosphorylation, ubiquitination, binding/association, component, `dummy_u_to_d`, or no subtype at all. Relaxed ⇔ both ends present and not both active.
  - So an activation edge between present nodes is consistent only if both ends are active, and an inhibition edge only if exactly one end is active.
  - The count `Σ ite(relaxed, 1, 0)` is asserted `≤u __edgestatusrelaxbound`.
  - The relaxation does not depend on `__edge_e`: an edge between two present nodes counts whether or not it is selected.
  - Multi-subtype edges keep their subtypes as separate tokens. The merged XML has one `<subtype>` element per subtype, and 1119 of the full run's 7400 relations have more than one. So the `inhibition_dephosphorylation` shown in the outputs is in the inhibition family.
  - Real case, full run: `18423 present inhibition_dephosphorylation hsa1843 DUSP1 hsa5595 MAPK3 src_present src_active tgt_present tgt_active …` appears in both `run/temp_w_solutions.txt:889` and `run/temp_w_relaxations.txt:76`.
- **Problems:**
  - Relaxations count every considered edge between two present nodes, so edges that no solution path can use still change the result. This settles stage 4's open question 1: off-path edges change results, not only cost.
    - An inhibitory self-loop on a present node always costs 1.
    - An activating self-loop costs 1 whenever its node is present and inactive.
    - An edge into the source, or a walk-only edge (S4.11), costs 1 whenever its edge rule fails: an activation edge when its ends disagree, or when both are present and inactive (`28585-28588`), and an inhibition edge when its ends agree.
    - The reference run has 5 considered self-loops and the full run 7, found as `(=> __edge_e (bvult d d))` in their dumps. How much they moved the measured frontiers was not measured.
  - The inhibition family is only {inhibition, repression}; stage 6's renderer has been aligned to that set. The 162 subtype-less edges of the old master (S2.04) and every `dummy_u_to_d` association count as activation.
  - The solver never reads the `dummy_u_to_d` tag, which settles stage 2's open question 5.
  - Connectivity (S5.11) ignores signs, so a forced node can attach through an inhibition edge that is itself relaxed. "FSC present and active" does not mean "FSC on a sign-consistent route".
- **Proposed call:** fix. Keep the sign rules and the rule that edges count whether or not they are selected. Leave self-loops out of the count, since an always-relaxed inhibitory self-loop is an artefact of two-state nodes. Report sign-consistent routes separately from presence. Whether only selected edges should count is open question 2.
- **Decision:** fix as proposed: keep the sign rules and keep counting every considered edge between present nodes, selected or not; leave self-loops out of the count; report sign-consistent routes separately. This settles S4.11: the corridor trims the cheap off-path edges. (prompt log entry 068)

### S5.14 · Every path passes a differentially expressed node

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28731-28909` (`get_every_path_has_diff_exp_node_constraints_with_z3`)
- **What it does:** Each considered edge has a flag `__edgepathflag_e`, and DE is `up_use ∪ down_use`. The rules:
  1. every selected in-edge of a target has its flag set;
  2. if a non-DE considered node has a selected in-edge with its flag clear, all its considered out-edges have their flags clear;
  3. a non-DE source has all its considered out-edges' flags clear;
  4. a DE node has all its considered out-edges' flags set.

  So every selected source-to-target path passes through a DE node before the target. Flags start clear at a non-DE source, stay clear through non-DE nodes, and must be set on arrival. A relaxed DE node still counts as DE. A DE source satisfies the rule by itself, and a direct edge from a non-DE source to the target can never be selected.
- **Problems:**
  - No user-facing text and no help line mentions this rule, yet the frontier depends on it.
  - Four function-static sets guard it (S5.29).
  - It is posted as one conjunction, which is 54 KB long in the reference dump.
- **Proposed call:** keep. Port the rule as it is, and document it in the help and the output.
- **Decision:** keep, and document the rule in the help and the run's output (prompt log entry 068)

### S5.15 · Coexpression constraints and their CSV

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28601-28680` (`load_coexpression_pairs_from_csv`), `28682-28709` (`get_coexpression_constraints_with_z3`); `Linux/src/main.cc:4561-4563`
- **What it does:**
  - **Reading.** The first line is skipped. Each later line needs at least four commas. Columns 3 and 4 hold the id cells, and the text after the fourth comma is the score, parsed with stof, which ignores trailing text; a line whose score is not a number is skipped.
  - Real header and line, from `additional_constraints/coexpression_csv/hsa_stringdb_coexpression_links_over400.csv` (each run copies it into `inputs/`): `node1,node2,hsa_id_node1,hsa_id_node2,coexpression_score` and `CD74,HLA-DMA,hsa:972,hsa:3108 hsa:3109 … hsa:3127,611`.
  - **Pairs.** A line is kept if its score is strictly above the threshold. Each id cell is split on whitespace and its `:` characters removed, and each token is resolved in the corridor (S4.15). Every cross pair (u, v) with u ≠ v is stored, unordered.
  - Console: `Loaded <n> coexpression pairs from <file> at threshold > <t>` on stdout, with t to two decimals (S1.27).
  - **Constraint.** For each pair with both nodes considered (S5.10): `(__node_u ∧ __node_v) ⇒ (__nodeactive_u = __nodeactive_v)`. stderr gets `[coexpression] emitted <k> clauses`.
  - Reference run: 1 pair loaded and 1 clause emitted. The batch comment "coexpression >700" matches the strict test; 2 of the file's 903 rows score exactly 700.
  - The cooccurrence CSVs under `additional_constraints/cooccurence_csv/` have no reader in the live code, and no batch names them.
- **Problems:**
  - The header is skipped unconditionally and the columns are positional. Malformed and unresolved rows are skipped without a count.
  - The file is parsed again by every pathz3 call, twice per batch.
- **Proposed call:** fix. Keep the constraint and the strict threshold, but find columns by header name, report skipped and unresolved rows, and parse the file once per run.
- **Decision:** fix, as proposed (prompt log entry 068)

### S5.16 · Frozen edges

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:28711-28729` (`get_frozen_edge_constraints_with_z3`); score storage `GraphManagerNew.cc:263-273` (`weight_f = 0.0f`) and `Linux/src/XMLParser.cc:1518-1524`
- **What it does:**
  - An edge's `experiment_score` is stored as its float weight. It is 0 when the KGML has no score, and the maximum over merged duplicates (S2.10, S2.20).
  - For each considered edge whose score is strictly above the threshold, the command asserts `¬__edgerelax_e`. stderr gets `[frozen-edge] emitted <k> clauses`. The reference run emits 97.
- **Problems:**
  - The test is strict `>`, but the batches say "frozen edges score >=800" (reference batch, line 81). One of the 2529 relations in the reference run's merged graph scores exactly 800, so it is not frozen.
  - As in S5.13, the constraint applies whether or not the edge is selected. A frozen edge between two present nodes forbids their disagreeing states, which can make a point UNSAT.
  - KGMLs without the attribute score 0 and can never be frozen, without notice. The four pathways the reference batch adds are like this (batch lines 165-166).
- **Proposed call:** fix. Keep the rule and the strict `>`, which matches the coexpression threshold and leaves results unchanged. Correct the batches' wording, state the test in the help and the output, and report how many considered edges have no score.
- **Decision:** fix, as proposed (prompt log entry 068)

### S5.17 · The FSC forced in or out

- **FuSION 1:** no FSC argument exists. The batches build the FSC cases from S5.05's lists, in reference batch lines 198-207 (with) and 233-239 (without); `Linux/src/main.cc:4667-4668`
- **What it does:**
  - **With the FSC**, the essential, active and non-relaxed lists each hold the source, the target and the FSC, and the avoid list is empty.
  - **Without the FSC**, those three lists hold the source and the target, and the avoid list holds the FSC.
  - So "in" means present, active and not relaxed. "Out" means absent and, for an up-regulated FSC, no longer an observation (S5.07).
  - The two solves run on two separately built corridors (S4.20). Console: `Nodes to be active: 3`, then `Nodes to avoid: 1`.
- **Problems:**
  - A down-regulated FSC cannot be forced in. Present and active makes its relaxation true (S5.12), and the non-relaxed list asserts it false, so every point of the with-FSC solve is UNSAT. This is traced from the code and was not run.
  - "In" forces presence and activity, not a role: the FSC may hang off a relaxed inhibition edge (S5.13).
  - An FSC that cannot lie on a path aborts the run (S5.11).
  - The comparison of the two curves is the scientific result, yet pathz3 knows nothing of it. The batch must keep 15 arguments in step across two pathz3 lines and two `fb_rch` lines.
- **Proposed call:** fix. Make the FSC a named argument of the solve. "In" means present and active; for a down-regulated FSC, leave out the non-relaxed assertion or refuse with a message. "Out" means absent and removed from the observations. Run both cases on one corridor.
- **Decision:** fix as proposed: the FSC is a named argument of the solve, and both cases run on one corridor. 'Out' means absent and removed from the observations. 'In' means present on a path, with activity matching its observation: active when up-regulated or unobserved, inactive when down-regulated (open question 5). (prompt log entry 069)

### S5.18 · How the Pareto frontier is enumerated

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:29349-29833` (`get_PO_points_within_limits`); call `Linux/src/main.cc:4876-4881`; `Linux/include/macros.h:79-80`
- **What it does:** The search relies on monotonicity: if (n, e) is SAT, so is every (n′ ≥ n, e′ ≥ e). It works on windows [nl, nu] × [el, eu], starting from [n_lo, n_hi] × [e_lo, e_hi]:
  1. **Diagonal search.** Test the midpoint of the window's two corners. On SAT, move the high corner to one below the point on both axes; otherwise move the low corner to one above it, clamped. The last SAT point is (nd, ed). If no point is SAT, the window holds no point and the call returns.
  2. **Smallest n.** With e ≤ ed pushed, binary-search n in [nl, nd] for the least SAT value N_h.
  3. **Smallest e.** With n ≤ nd pushed, binary-search e in [el, ed] for the least SAT value E_h.
  4. Record (n N_h, e ed) and (n nd, e E_h), or a single point when the two share a coordinate.
  5. Recurse into [nd+1, nu] × [el, E_h−1] and [nl, N_h−1] × [ed+1, eu].

  Each test pushes `__nodestatusrelaxbound ≤u n` and `__edgestatusrelaxbound ≤u e`, checks, and pops. A function-static matrix caches SAT and UNSAT per point; it is reset at the top-level call (`29352-29359`).
  - Reference run: 11 checks give the single with-FSC point (n 1, e 0), and 4 checks, all UNSAT, give no point without the FSC.
  - Full run, without the FSC: 7 points from 23 distinct dumped queries.
  - The lower bounds only shape the window. The result is the frontier inside the box, and the edge cap truncates it: in the full run the last point is (n 2, e 7) with e_hi 8.
- **Problems:**
  - With exact answers the windows cover the whole box, so the method is sound. With unknown answers it is not (S5.19).
  - Every check re-solves the same base formula with two bound assertions. Nothing is reused between points except Z3's own learned state, and the model found at each SAT point is discarded (S5.26).
  - `result` is allocated with `new` and never freed (`29537`, `29552`, `29795`, `29799`), and `Zero solutions found!!!` (`29805`) cannot be reached.
- **Proposed call:** keep. The algorithm is correct under exact answers; its weak points are the separate items S5.19, S5.20 and S5.26.
- **Decision:** keep (prompt log entry 069)

### S5.19 · Unknown results: TIMEOUT, UNDEFINED and uncertified points

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:29342-29347` (`z3_unknown_is_timeout`), `29475-29523`, `29626-29666`, `29739-29779`, `29868-29879`, `8180`, `8217-8225`; `Linux/src/main.cc:4733` (`macOS/src/main.cc:4761`). Code before the fix: git `224e817`, `Linux/src/GraphManagerNew.cc:29438` and the six other `reason.find("timeout")` tests (`29468`, `29586`, `29610`, `29696`, `29720`, and the mode-1 test at `29823`).
- **What it does:** A check returns sat, unsat or unknown. On unknown, the current code labels the point TIMEOUT if either:
  - `reason_unknown()` contains `timeout`; or
  - the check's wall time, in whole seconds from `time()`, is at least 99 % of the overall timeout that main passes in (`set_z3_overall_timeout_ms(timeout2)`).

  Otherwise the point is UNDEFINED. The command writes to the limits file and stdout either `TIMEOUT (<step> cache) at point e=<e> n=<n> (z3 reason: <r>). Consider increasing the solver timeout ($CONSTR_SOLVER_TIMEOUT1/$CONSTR_SOLVER_TIMEOUT2 in the batch file).` or `UNDEFINED (<step> cache; z3::unknown, reason: <r>) at point e=<e> n=<n>`, then the same again with `steering` in place of `cache`. Either way the point counts as not SAT: the diagonal search raises its low corner past it, and the binary searches treat it as UNSAT.
  - Runs before 2026-11-09 19:16 tested only the reason string. Z3 reported `unknown`, so every timeout was logged as UNDEFINED. Full run, `run/…_wo_…_limits_timefile.txt:74-75`: `Time taken (sec): 14396.2`, then `UNDEFINED (smallest-e cache; z3::unknown, reason: unknown) at point e=0 n=7`.
  - Runs after the fix log TIMEOUT, as in `runs/egfr_b10_gsea_fdr044_candidates/r0_cxcl8_jun/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10_limits_timefile.txt:108`.
  - The fix changed only the labels. The search treats both labels alike, so no frontier changes.
- **Problems:**
  - A point found next to an unknown answer is not certified Pareto-optimal, and nothing in `PO.dat`, the solutions file or the console says so.
  - Full run, without the FSC: (n 7, e 0) was unknown, so the reported (n 7, e 1) and (n 8, e 0) may be dominated by it (limits lines 69-89). fdr050 has the same situation at (n 2, e 7) and (n 5, e 0).
  - The 99 % rule is a heuristic over a one-second clock.
- **Proposed call:** fix. Keep the TIMEOUT/UNDEFINED distinction, and mark every frontier point whose optimality depends on an unknown answer, in the frontier output and in the solutions file.
- **Decision:** fix, as proposed (prompt log entry 069)

### S5.20 · Timed-out points are solved again

- **FuSION 1:** cache reads `Linux/src/GraphManagerNew.cc:29423-29432`, `29580-29589`, `29695-29704`; TIMEOUT writes `29480`, `29631`, `29744`
- **What it does:** The cache answers only SAT and UNSAT. A TIMEOUT or UNDEFINED state is stored, but when the search reaches that point again, from another step or another window, it solves the same query from scratch.
- **Problems:**
  - Avoidable cost. fdr050 made 9 unknown checks of about 4 h each:
    - with the FSC, (n 2, e 7) three times;
    - without it, (n 5, e 0) twice, (n 2, e 7) three times and (n 2, e 6) once.
  - Five of the nine repeated a query that had already timed out, which is about 20 of the run's 50.7 h.
  - The pattern continues after the fix: `runs/egfr_b10_gsea_fdr044_candidates/r0_cxcl8_jun/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10_limits_timefile.txt:108` and `:114` time out twice at (n 2, e 6).
- **Proposed call:** fix. Cache TIMEOUT and UNDEFINED like SAT and UNSAT within a solve, and keep that cache across a resumed solve for decision 0019.
- **Decision:** fix, as proposed (prompt log entry 070)

### S5.21 · Solver timeouts

- **FuSION 1:** `Linux/src/main.cc:4507-4508`, `4727-4736`
- **What it does:**
  - `timeout1` is set as `:solver2_timeout` and `timeout2` as `:timeout`, both in milliseconds, on the one solver. The code comments call them "timeout for incremental solver" and "overall timeout". Both apply to each `check()` separately, and nothing limits a whole solve.
  - Census values for timeout1/timeout2, in ms: 600000/3000000 (102 lines), 3600000/14400000 (34), 30000/150000 (26), 60000/300000 (4) and 15000/30000 (4).
  - Logged unknowns stop at the overall cap: 14,354 to 14,400 s for 14,400,000 ms, about 3000 s for 3,000,000 ms, and about 150 s for 150,000 ms (limits files under `runs/`).
- **Problems:**
  - A solve can run for many multiples of the cap. The full run's without-FSC solve took 57,248 s (15.9 h) under a 4 h cap (`run/timing.log`), and fdr050's took 124,355 s.
  - The limits file's "Time taken (sec)" is process CPU time (`clock()`), while the cap and the TIMEOUT test use wall time.
  - FuSION documents neither what the first timeout does nor how it interacts with the second (open question 1).
- **Proposed call:** fix. Keep a per-query timeout. Add an optional budget for the whole solve that stops cleanly and saves state for resuming (decision 0019), and log wall time.
- **Decision:** fix, as proposed (prompt log entry 070)

### S5.22 · Random seed

- **FuSION 1:** `Linux/src/main.cc:4531`, `4573-4577`, `4732`; `Linux/src/GraphManagerNew.cc:8179`, `8207-8215`, `29451-29452`, `29603-29604`, `29717-29718`
- **What it does:**
  - The seed defaults to 42 (optional token 5). It is set on the solver as `random_seed` and stored in the graph manager.
  - Each dump starts with `(set-option :smt.random-seed 42)` when the seed is 0 or more. The graph manager's default is -1 (`8179`), but pathz3 always sets the seed.
  - Reruns reproduce on this machine and Z3 build. The sweep's `fdr025/` rerun and the reference run have byte-identical with-FSC `.dat` and `temp_w_solutions.txt` files, and their limits files differ only in `Time taken` lines.
- **Problems:**
  - The smallest-e step writes its dumps without the seed line (`29717-29718`). In the reference run, `PO_constraints_nr_1_er_{0,1,2}.smt2` are 461,914 bytes against 461,947 for the others, except `PO_constraints_nr_1_er_4.smt2` at 481,472; the 33 bytes are the seed line and its newline.
  - The comment at `main.cc:4573-4576` claims that standalone Z3 on a dump "reproduces the in-process behaviour byte-for-byte". The dumps carry neither the timeouts nor the history of earlier checks, so that cannot hold in general, and it was never tested.
  - Apart from the dumps, the seed appears only in one console line.
- **Proposed call:** fix. Keep the seed and its default, record it in the run's outputs, and write it into every dump.
- **Decision:** fix, as proposed (prompt log entry 070)

### S5.23 · SMT dumps (`PO_constraints_nr_<n>_er_<e>.smt2`)

- **FuSION 1:** `Linux/src/main.cc:866-887` (`make_smt_dump_dir_for`), `4567-4571`; `Linux/src/GraphManagerNew.cc:29448-29454`, `29600-29606`, `29715-29719`
- **What it does:**
  - **Directory.** Each pathz3 call creates `<fold-change basename without extension>_smt-files_<YYYYmmdd_HHMMSS>/` in the launch directory, with mode 0755; an existing directory is reused. Any other mkdir failure prints `[smt-dump] could not create <dir> (errno=<n>); falling back to CWD`, and the dumps then go to the launch directory. The command prints `[smt-dump] writing SMT2 dumps to <dir>/`.
  - **Files.** Before every search check the cache does not answer, `s.to_smt2()` is written to `<dir>/PO_constraints_nr_<n>_er_<e>.smt2`. It holds the optional seed line, `; `, `(set-info :status unknown)`, the declarations, every assertion including the two pushed bounds, and `(check-sat)`.
  - Solution checks (S5.26) and mode 1 write no dumps.
  - Real example: the reference run's with-FSC directory `egf_hela_t120_lfc_smt-files_20260928_185105/` holds 11 files, ten of about 462 KB and `PO_constraints_nr_1_er_4.smt2` of 481 KB, each with 1425 declarations and 1321 assertions.
- **Problems:**
  - Avoidable cost. The dumps are always written, and FuSION never reads them. Each one re-serialises the whole formula.
  - The full run wrote 21 and 23 files of 2.73 to 2.89 MB, 57.5 MB and 65.6 MB in all. When this stage was drafted, `runs/` held 171 dump directories with 5.55 GB (172 when it was verified).
  - A point that is solved again (S5.20) overwrites its earlier dump. Two calls in one launch directory share a dump directory only if they start in the same second with the same fold-change base name (`main.cc:877-879`).
  - Variables are named by session ids (S4.14), so a dump means nothing without the run's solutions file.
- **Proposed call:** fix. Write dumps only under the debug option (S1.29), into the run's output directory, named by solve case and point. Reusing dumps (roadmap objective 4) is Phase 2's question.
- **Decision:** fix, as proposed: dumps only under the debug option, in the run's output directory, named by solve case and point; dump reuse (roadmap objective 4) is a Phase 2 question (prompt log entry 070)

### S5.24 · The limits file (`limits_timefile.txt`)

- **FuSION 1:** `Linux/src/main.cc:4851-4852`; writes throughout `Linux/src/GraphManagerNew.cc:29361-29829`
- **What it does:**
  - The file is `<unique_prefix>limits_timefile.txt` in the launch directory, truncated by every pathz3 call. The `upfx` prefix (S2.29) is joined on without a separator.
  - It traces the search, writing points as (e, n). The line types are:
    - window: `new call with node relax in [nl, nu] and edge relax in [el, eu] of type <0|1|2>`, and `Invalid window: Quitting search`;
    - check: `Relax bounds: <e> <n>`, `Relax bounds (node call): <e> <n>` and `Relax bounds (edge call): <e> <n>`, each followed by `Time taken (sec): <cpu>` and `sat`, `unsat` or `unknown`;
    - cache hit: `Point <e> <n> already sat` or `already unsat` (the diagonal cache prints `<n>already sat`, without the space, `29425`);
    - steering: `N_h: … E_h: …` and `N_l: … E_l: …`;
    - binary searches: `Finding smallest n`, `Soln lower <e> <n>`, `Finding smallest e` and `Soln upper <e> <n>`;
    - results: `One soln in window (<eu>, <nl>, <el>, <nu>)`, `Two solns in window (…)` and `Point 1: (<e>, <n>)`;
    - the TIMEOUT and UNDEFINED lines of S5.19.
  - Real excerpt, reference run: `Relax bounds: 4 1` / `Time taken (sec): 0.68651` / `sat` / `N_h: 0 E_h: 3`.
  - Every batch moves the file to `<src>_to_<tgt>_<w|wo>_<fsc>_<lfc>_b<b>_limits_timefile.txt`, except the two in `auxilliary/comparison_results/`, which use `po_w_limits_timefile.txt` and `po_wo_limits_timefile.txt`.
- **Problems:**
  - Points are (e, n) here but (n, e) in PO.dat and "n nodes, e edges" in the solutions file, and the window line mixes both orders.
  - This free text is the only record of unknown answers (S5.19) and of solver times.
  - Mode 1 truncates the file and writes nothing to it (S5.28).
- **Proposed call:** fix. Write a structured search log in the run's output directory: one record per check, with point, step, outcome, wall and CPU time, and reason, in a single point order.
- **Decision:** fix, as proposed (prompt log entry 071)

### S5.25 · `PO.dat` and the dominance filter

- **FuSION 1:** `Linux/src/main.cc:4855-4856`, `4920-4955`, `1002-1007`
- **What it does:**
  - After the search the command prints `<k> PO points found` and sorts the points by n descending, ties by e ascending.
  - It then walks the list. A point whose e equals the last kept point's e is printed `(<e> ,<n>) is dominated` and skipped. Any other point is printed `(<e> ,<n>) is not dominated` and written as `<n>\t<e>` to `<unique_prefix>PO.dat` in the launch directory.
  - With no point it prints `No point in PO curve` and ends the command (S5.26).
  - Real file, the full run's `run/…_wo_…_b10.dat`: `8\t0`, `7\t1`, `6\t2`, `5\t3`, `4\t4`, `3\t5`, `2\t7`.
  - An empty frontier leaves an empty file; the reference run's without-FSC `.dat` is 0 bytes.
  - Batches move the file to `<…>.dat`, which `plot_po.sh` and pex read (stage 6).
- **Problems:**
  - Of two points with equal e, the filter keeps the larger n, which is backwards.
  - It never fires: S5.18's windows give every point a distinct e, so the filter is dead weight.
  - The file has no header and no case label, and does not mark uncertified points (S5.19).
- **Proposed call:** fix. Drop the filter, since the search already yields a non-dominated set. Keep the two-column file for stage 6, and carry the certification flag in the structured outputs (S5.19).
- **Decision:** fix, as proposed (prompt log entry 071)

### S5.26 · Solutions at each frontier point

- **FuSION 1:** `Linux/src/main.cc:4964-5066`; `Linux/src/GraphManagerNew.cc:30729-30846` (`process_z3_model`)
- **What it does:** Points are visited in PO.dat's order. For each one, both bounds are pushed and the loop runs while `found ≤ count`:
  - **SAT:** read the model, print the solution if `found ≤ explore`, and add a blocking clause;
  - **UNSAT:** write `No more solutions. Obtained unsat` and stop;
  - **unknown:** write `reason_unknown()` to both files and stop.

  The blocking clause negates the conjunction of every Boolean `__node_`, `__nodeactive_`, `__noderelax_`, `__edge_` and `__edgerelax_` variable in the model, at its model value. With count = explore = 1, as in 166 of the 170 lines, each point costs two more checks and prints one solution plus an empty `Solution 2` header.
- **Problems:**
  - Avoidable cost. The search already found a model at every frontier point and discarded it, so each point is solved again.
    - In fdr050 the with-FSC and without-FSC solves spent 2226 s and 12,871 s outside the logged search checks: elapsed time from `run/timing.log` minus the limits files' check times.
    - Nearly all of that is this re-solving; the closure of S5.08 takes a few seconds at that corridor size.
  - The blocking clause includes don't-care variables, such as the activity of absent nodes. The reference run's relaxations file lists IRS2 as `not_present active`. A second solution can therefore differ from the first only in such a bit, and the empty `Solution 2` says nothing about alternative explanations.
  - With no frontier point the command ends before opening the solution files, so the reference run has no `temp_wo_solutions.txt` or `temp_wo_relaxations.txt`. The `[pathz3 timing]` line is not printed either.
- **Proposed call:** fix. Keep the model the search found at each point. Enumerate further solutions only on request, blocking only on present nodes, their activity and selected edges. Always write the solution files, empty when there is no point.
- **Decision:** fix, as proposed (prompt log entry 071)

### S5.27 · Solutions and relaxations files

- **FuSION 1:** `Linux/src/main.cc:4964-4976`, `5011-5013`, `5038-5060`; `Linux/src/GraphManagerNew.cc:30848-31017` (`print_variables_from_z3_model`)
- **What it does:** The files are `<prefix>_solutions.txt` and `<prefix>_relaxations.txt`; the batches use `$WRITE_DIR/temp_w` and `temp_wo` as prefixes. Each point starts with `Current PO point: <n> nodes, <e> edges relaxed`, then each solution with `Solution <k>`, followed by a node block and an edge block separated by blank lines. A blank line follows each point.
  - **Solutions file, nodes:** every present node, as `<nid>\t<rep id>\t<symbol or rep id>\t<up_reg|down_reg|not_diff_expr>\tpresent\t<active|inactive>`. Example: `39118\thsa3725\tJUN\tup_reg\tpresent\tactive`.
  - **Solutions file, edges:** every selected edge, as `<eid>\tpresent\t<subtypes joined by _>\t<src rep>\t<src symbol>\t<tgt rep>\t<tgt symbol>\tsrc_present\t<src_active|src_inactive>\ttgt_present\t<tgt_active|tgt_inactive> [<pathways, space-separated>]`. Example: `78864\tpresent\texpression\thsa3725\tJUN\thsa3576\tCXCL8\tsrc_present\tsrc_active\ttgt_present\ttgt_active [path:hsa04620 path:hsa05120 path:hsa05130 path:hsa05132 path:hsa05142]`.
  - **Relaxations file:** every node whose `__noderelax_` is true, in the same columns whether present or not (`39181\thsa8660\tIRS2\tup_reg\tnot_present\tactive`), then every edge whose `__edgerelax_` is true, selected or not, with `present` or `not_present` in the second column.
  - Nodes and edges appear in ascending corridor id (S4.14). For a node in both lists, up wins (S3.07). A missing symbol prints the rep id.
- **Problems:**
  - The ids are corridor ids, which differ between the two solves (S4.14).
  - For an absent node, `active` or `inactive` is an arbitrary model value (S5.26), printed as if it meant something.
  - The format is positional, tab-separated text with a bracketed, space-separated tail.
- **Proposed call:** fix. Keep both files for stage 6 until it settles its inputs. Name nodes and edges by stable keys (S4.14), and leave out the activity of absent nodes.
- **Decision:** fix, as proposed (prompt log entry 071)

### S5.28 · Mode 1: testing given points

- **FuSION 1:** `Linux/src/main.cc:4882-4913`; `Linux/src/GraphManagerNew.cc:29835-29883` (`is_sat_at_existing_PO_points`)
- **What it does:**
  - Reads `<n> <e>` per line from the file named after the timeouts. It checks each point under both bounds and stops at the first SAT one, printing `Sat point obtained on previous curve`; if none is SAT it prints `New curve completely separated from previous points`.
  - Per point it prints `Pair (e,n): <e> <n>`, `Time taken (sec): …`, `Sat at …` or `Unsat at …`, and any TIMEOUT or UNDEFINED line.
  - It writes no solutions, and the command ends (`continue`) without the timing line.
- **Problems:**
  - No batch uses it: all 170 lines are mode 0.
  - PO.dat and the limits file are opened, and so truncated, before the mode test (`4851-4856`). A mode-1 call therefore empties the previous solve's files.
  - A blank or short line reuses the previous line's values, because `n` and `e` are declared outside the loop. A non-numeric value sets them to 0, as C++11 extraction does on failure.
- **Proposed call:** drop. Decision 0019's "solve part of the frontier" is designed in Phase 2 and can cover this use.
- **Decision:** drop, as proposed (prompt log entry 072)

### S5.29 · Function-static state: pathz3 cannot run twice on one graph

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:27815`, `28067`, `28160`, `28200`, `28241`, `28311`, `28740`, `28776`, `28835`, `28875`; never-filled sets `28113`, `28124`, `28309`, `28310`, `28918`, `28948`; an unused static vector `27818`; matrix `29352`
- **What it does:**
  - Seven encoding functions keep `static` sets of the pairs, edges or nodes they have already encoded, and skip those items on later calls. Six more keep sets they never fill, and one function keeps a static vector it never uses (`27818`).
  - Node and edge ids are session-wide (S4.14), so on a new corridor nothing collides. That is why the batches' second `fb_rch` (S4.20) keeps the second solve correct.
- **Problems:**
  - A second pathz3 on the same graph would find every pair already in `written_pairs` (`27815`) and encode no edges at all. It would also skip the essential nodes (`28311`), the source and target edge rules and the path-flag rules, while the relaxation sums ranged over empty sets.
  - That second solve would most likely report the single point (n 0, e 0), with no error.
  - This settles stage 4's open question 2: no. It is traced from the code, and no batch does it.
- **Proposed call:** fix. No state survives a solve: each solve builds its encoding from its own inputs.
- **Decision:** fix, as proposed (prompt log entry 072)

### S5.30 · Where the outputs go

- **FuSION 1:** `Linux/src/main.cc:4565`, `4851-4856`, `4964-4967`; `Linux/src/GraphManagerNew.cc:27750-27751`, `28468-28469`; `Linux/include/macros.h:31`
- **What it does:**
  - `limits_timefile.txt` and `PO.dat` go to the launch directory, with the `upfx` prefix joined on.
  - `<prefix>_solutions.txt`, `_relaxations.txt` and `_mincuts` go wherever `<prefix>` points; the batches use `$WRITE_DIR/temp_w` and `temp_wo`.
  - `diff_expr_nodes.txt` and the dump directories go to the launch directory without the prefix.
  - Every batch moves the first two files after each solve with `exec mv`.
- **Problems:** S1.26's problems apply. Two runs in one directory overwrite each other's `PO.dat`, limits file and `diff_expr_nodes.txt`, and only two of the outputs use the `upfx` prefix. The run's own directory receives only what the batch moves there.
- **Proposed call:** fix. Every output of a solve goes to the run's output directory, named by solve case, and no fixed names are written to the launch directory (S1.26).
- **Decision:** fix, as proposed (prompt log entry 072)

### S5.31 · Console output

- **FuSION 1:** `Linux/src/main.cc:4558-4559`, `4571`, `4585`, `4686-4693`, `4712-4713`, `4763-4769`, `4878-4879`, `4920-4953`, `5014`, `5040`, `5071-5072`; `Linux/src/GraphManagerNew.cc:27804`, `27827-27828`, `28678-28679`, `28708`, `28728`, `29813`, `29824-29825`
- **What it does:** A successful call prints, in order (reference run, `run_console.log:270-296`):
  - setup: `[new-constraints] enabled=1 z3_random_seed=42`, `Loaded 1 coexpression pairs from <csv> at threshold > 700.00`, `[smt-dump] writing SMT2 dumps to <dir>/`, `1 connect pairs`, the six counts of S5.05 and a blank line, `Up: 3 Down: 0`, and the clamp line (S5.07);
  - encoding: `Beginning to generate constraints`, `Source: EGFR(hsa1956) Target: CXCL8(hsa3576)`, `[coexpression] emitted 1 clauses` and `[frozen-edge] emitted 97 clauses` (both on stderr), `Finished generating constraints` and `Not printing matrices in the current run`;
  - search: `Solving constraints to generate PO points...` and `This may take some time ... See limits_timefile.txt to check progress`, then each point as it is found, as (e, n), for example `(0, 1)`;
  - frontier: `1 PO points found` and `(0 ,1) is not dominated`;
  - solutions: `Solution 1` and `Solution 2`;
  - timing: `[pathz3 timing] Total wall-clock time: 13.63 s (13632.95 ms)`.

  The search also prints the TIMEOUT and UNDEFINED lines of S5.19.
- **Problems:**
  - Points are printed as (e, n), with inconsistent spacing.
  - The timing line is missing when the frontier is empty and in mode 1.
  - Some lines describe work nothing uses, such as `Not printing matrices`.
  - Messages go to stdout or stderr with no consistent rule.
- **Proposed call:** fix. Print the same milestones, with points in one order and labelled, always print the timing line, and remove the lines about unused work.
- **Decision:** fix, as proposed (prompt log entry 072)

## Open questions

1. **What exactly do `:solver2_timeout` and `:timeout` do in this Z3 configuration, and why does a 4 h timeout report `unknown` rather than `timeout`?** The code comments say "incremental solver" and "overall", and the logs show each unknown check stopping at the overall cap. *Would settle it:* run one of the full run's dumps with Z3's verbose and statistics output under both timeouts, with the Z3 version FuSION links. Or read that version's combined-solver source.
   *Still open, for phase 3.* No stage 5 decision depends on it; the answer tells Phase 2 how S5.21's per-query timeout maps onto Z3.
2. **Should edge relaxations count only selected edges instead of every considered edge between present nodes (S5.13, S5.16)?** This is a modelling choice: it decides whether an unused inhibition edge between two active genes is an inconsistency. *Would settle it:* the user's judgement, informed by a phase-3 comparison on the full run's corridor of both counting rules, plus a run with self-loops left out.
   *Resolved:* every considered edge between present nodes keeps counting, selected or not, and self-loops leave the count (S5.13; prompt log entry 068).
3. **Are the frontier points next to unknown answers really Pareto-optimal (S5.19)?** Examples: the full run's (n 7, e 1) and (n 8, e 0) next to the undecided (n 7, e 0), and fdr050's points near (n 2, e 7) and (n 5, e 0). *Would settle it:* re-check the undecided queries from their dumps with a longer timeout or another Z3 tactic.
   *Still open, for phase 3.* Either way, S5.19 marks such points as uncertified in the outputs.
4. **Can the GUI write an empty `$COEXPRESSION_CSV` (S5.02)?** *Would settle it:* stage 8's reading of how `gui4.java` fills `%__a21__%` when the field is blank.
   *Still open, for stage 8.*
5. **What should "FSC forced in" mean for a down-regulated FSC (S5.17)?** It could mean present and inactive, or present and active with its relaxation paid. *Would settle it:* the user's biological judgement.
   *Resolved:* present on a path, with activity matching its observation, so inactive for a down-regulated FSC (S5.17; prompt log entry 069).
6. **Does the `random_seed` setting reach every solver path Z3 uses here (S5.22)?** *Would settle it:* two runs of one corridor that differ only in the seed, comparing the limits files.
   *Still open, for phase 3.*
