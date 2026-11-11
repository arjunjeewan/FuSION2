# 06 · Results and plots

## Scope

This stage covers what FuSION 1 draws from a finished solve:

- `psp`/`plot_solution_paths`, which draws source-to-target solution paths for each Pareto point with Graphviz and writes a pathway-source CSV. That is the dispatch at `Linux/src/main.cc:5661-5689` and `GraphManagerNew::plot_solution_paths` at `Linux/src/GraphManagerNew.cc:8823-9144`.
- `pex`/`plot_explanation`, which is the dispatch at `Linux/src/main.cc:5691-5764` plus `Linux/supporting_files/plot_explanation.py` (744 lines, PyVis).
- `Linux/supporting_files/plot_po.sh`, the gnuplot Pareto plot that batch files run through `exec`, and the inline gnuplot lines that most batch files and both GUIs use instead.

It starts where stage 5 has written `temp_w_solutions.txt`, `temp_wo_solutions.txt`, the two `temp_*_relaxations.txt` files and the `.dat` files. Stage 5 owns how those files are produced and their full format; this stage describes only what the readers take from them.

Stage 1 owns:
- tokenising (S1.05);
- `exec` (S1.14);
- how the binary finds its helper scripts (S1.20, already decided);
- the interactive branches, which have been dropped (S1.03);
- the missing-`start` crash (S1.15);
- the launch directory (S1.26);
- failure policy (S1.28).

S3.14 compares pex's LFC parser with the others. Stage 8 owns the GUIs, which write only the inline gnuplot lines. Decision 0022 makes graph display a 2.0 feature to be designed after this review, so the items below describe FuSION 1 without designing that feature.

`macOS/src/main.cc` differs in one place in this stage: how pex finds its script (S6.11). `plot_explanation.py`, `plot_po.sh`, `GraphManagerNew.cc`, `GraphManagerNew.h` and `macros.h` are byte-identical between `Linux/` and `macOS/` (checked with `cmp`).

Names and evidence used below:
- **The full JUN run** is `runs/egfr_cxcl8_jun_b10_nr8_er8/`: EGFR → CXCL8, FSC JUN, bound 10. It has 6 Pareto (PO) points with the FSC and 7 without, and 13 pex pages.
- **The reference run** is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`. Its without-FSC solve found no PO point.
- **The pex replay** is a verbatim copy of `plot_explanation.py` lines 1-394 and 513-528, with `pyvis.network.Network` replaced by a stub that records each node and edge. It runs over the `temp_*_solutions.txt` files of every finished run under `runs/`.
  - Three runs in `runs/egfr_b10_gsea_fdr044_candidates/` were still running on 2026-11-11 and are left out.
  - That leaves 174 files and 569 renders, one per case and PO point.
  - The replay reproduces the counts pex printed, e.g. `(9 nodes, 8 edges)` at `run_console.log:378` of the reference run.
  - Logs written before 2026-07-21 also count nodes that the current script moves to a table (S6.13), e.g. 77 in the log against 75 + 2 for the DUSP1 run's with-FSC point (2,6).
- **The psp replay** is a Python re-implementation of `plot_solution_paths`. No batch file has ever called psp, and `runs/` holds no psp output (`.dot`, `.svg` or `_edge_sources.csv`).
- **Batch counts** cover the 89 `*.batch` files now in the repo.
- FuSION 1 itself was not run.
- Counts were taken on 2026-11-10 and 2026-11-11, and runs added later are not included. When this stage was verified, `runs/` held 281 pex pages and the repo 90 batch files.

## Items

### S6.01 · `psp` / `plot_solution_paths` command line

- **FuSION 1:** `Linux/src/main.cc:5661-5689` (batch tokens at `5679-5687`), help line `main.cc:907`; `Linux/src/GraphManagerNew.cc:8828-8850`; `macOS/src/main.cc:5689-5717` (identical)
- **What it does:** The batch form is

  `psp <solution_file> <src> <tgt> <path_mode> <po_selector> <out_prefix>`

  - The six tokens are taken as strings, `token_cmd[1]` to `token_cmd[6]`.
  - `<path_mode>` is `shortest`, `alt[:W]`, `all` or `subgraph` (S6.04).
    - `alt` without `:W` means W = 2. W is parsed with `atoi`, so `alt:x` gives 0 and `alt:-1` is accepted.
    - Any other mode prints `plot_solution_paths: unknown path_mode '<m>' (use shortest|alt[:W]|all|subgraph)` on stderr and returns.
  - `<src>` and `<tgt>` are compared literally with the rep-id column of the dump (S6.02).
  - `<po_selector>` is S6.03. `<out_prefix>` names the outputs (S6.07, S6.08).
  - One call reads one solution file, so with and without the FSC take two calls. They need different prefixes, or files for PO points that occur in both cases overwrite each other.
  - psp uses the graph manager only to look up pathway names, but it still crashes before `start` (S1.15).
  - The documented example is `psp $WRITE_DIR/temp_w_solutions.txt hsa1956 hsa3576 shortest all $WRITE_DIR/psp_w` (`macOS/model_example/RUNNING.md:217`).
- **Problems:**
  - It is not used. None of the 89 batch files calls psp, and no GUI writes it.
  - A missing token arrives as `""` (S1.05). A missing prefix, for example, writes `_n7_e0.dot` into the launch directory. Extra tokens are ignored.
  - A malformed or negative window passes silently. With `alt:-1` no path is drawn, only the source and target, unconnected (`GraphManagerNew.cc:9069-9070`), and every file is still written.
  - Errors leave the exit status unchanged (S1.28).
- **Proposed call:** fix. Keep the six arguments (named, decision 0018), and reject an unknown mode, a bad window, missing or extra arguments, and identical output names for the two cases. Whether psp survives as its own command is S6.26.
- **Decision:** fix, as proposed (prompt log entry 075)

### S6.02 · Reading the solution and relaxation dumps (psp and pex)

- **FuSION 1:** psp `Linux/src/GraphManagerNew.cc:8852-8927`; pex `Linux/supporting_files/plot_explanation.py:72-129`; writer (stage 5) `Linux/src/main.cc:4964-5060` and `GraphManagerNew.cc:30848-31017`
- **What it does:**
  - Both readers cut the file into blocks at `Current PO point: N nodes, M edges relaxed`. psp parses the header with `sscanf`; pex takes whitespace fields 3 and 5.
  - Both keep only the lines between `Solution 1` and the next `Solution` or header. Lines are split on tabs:
    - 6 fields make a node line: `nid  rep_id  symbol  up_reg|down_reg|not_diff_expr  present|not_present  active|inactive`;
    - 11 or more fields make an edge line: `eid  present|not_present  subtype  src_rep  src_sym  tgt_rep  tgt_sym  src_present  src_active|src_inactive  tgt_present  "tgt_active|tgt_inactive [pathway ids]"`;
    - every other line is skipped: blanks, `Solution 2`, `No more solutions. Obtained unsat`, and Z3 reason lines such as `timeout`.
  - Real lines from `runs/egfr_cxcl8_jun_b7_nr8_er8/run/temp_w_solutions.txt`, `<TAB>` standing for a tab:
    - line 3: `7480<TAB>hsa673<TAB>BRAF<TAB>not_diff_expr<TAB>present<TAB>active`
    - line 28: `18233<TAB>present<TAB>phosphorylation<TAB>hsa673<TAB>BRAF<TAB>hsa5604<TAB>MAP2K1<TAB>src_present<TAB>src_active<TAB>tgt_present<TAB>tgt_active [path:hsa04010 path:hsa04510]`
  - Pathway ids are the space-separated tokens between `[` and `]`. The subtype field joins an edge's subtypes with `_` (`GraphManagerNew.cc:30989-30990`), e.g. `activation_phosphorylation`.
  - The solutions file holds only present nodes and edges (`30902-30905`, `30998-31003`). The relaxations file lists every relaxed node and edge in the same formats, whatever its presence (`30907-30910`, `31005-31009`). On 2026-11-10 every node and edge line in the solution files under `runs/` was `present`. The 177 relaxation files dated 2026-11-10 or earlier held 1935 `not_present` node lines and 667 `not_present` edge lines (1859 and 652 in the 174 files of the pex replay).
  - Both readers key everything by rep id and never by nid. The nid shift between the two solves (S4.14) therefore affects neither, and that answers the check S4.14 asked of this stage.
  - psp keeps each node's rep id, symbol and activity, and each edge's subtype, endpoints, a both-active flag and pathways. pex also keeps the expression class, presence and eid.
- **Problems:**
  - Only the first witness is read. Later witnesses, written when `NUM_SOLNS_TO_EXPLORE` > 1, are ignored without a message.
  - The two readers treat a PO point that has no `Solution 1` block differently. Such a point arises when the witness solve ended `unknown` (writer `main.cc:5045-5050`) or the file is still being written.
    - psp creates a point's entry only from a node line. `all` therefore skips the point silently, and `<n>,<e>` reports it as "not in file".
    - pex creates the block at the header and draws an empty page (0 nodes, 0 edges) that does not mention the timeout.
    - 12 points in four finished runs under `runs/real_experiments/` have no witness. An example is `experiment_2/temp_w_solutions.txt:925-926`: `Current PO point: 0 nodes, 11 edges relaxed`, followed by `timeout`.
  - Avoidable cost: pathz3 holds the model in memory, writes it as text, and then two separate parsers, one in C++ and one in Python, read it back. Field positions are an unwritten contract; the pex docstring only says its reader "matches psp".
- **Proposed call:** fix. Give the renderers the solve's results through one documented reader. Keep rep-id keys, read every recorded witness, and mark a PO point whose witness is missing instead of skipping it or drawing an empty page.
- **Decision:** fix, as proposed (prompt log entry 075)

### S6.03 · Choosing PO points: `all`, `least` or `<n>,<e>`

- **FuSION 1:** psp `Linux/src/GraphManagerNew.cc:8935-8973`; pex `Linux/supporting_files/plot_explanation.py:513-528`, called at `543`
- **What it does:**
  - `all` selects every point in the file, in ascending (n, e) order.
  - `least` selects the one point with the smallest n + e, breaking ties towards the smaller n.
  - `<n>,<e>` selects that point. psp parses it with `sscanf("%d,%d")` and pex with `int()` on each part.
  - An unknown point prints `plot_solution_paths: PO point 7,1 not in file. Available: 2,6 3,4 …` (psp) or `plot_explanation: PO 7,1 not in solution file; available: …` (pex), and nothing is drawn for it. A malformed selector prints `bad po_selector` (psp) or `bad --po` (pex).
  - All 24 pex calls use `all`.
- **Problems:**
  - Points without a witness are in pex's set but not psp's (S6.02), so `least` can pick a point in pex whose page is empty.
  - psp's `sscanf` accepts trailing text (`7,1x` gives 7,1), while pex rejects it.
- **Proposed call:** keep. The three selectors are clear. The witness-less case is fixed in S6.02, and a malformed selector such as `7,1x` is an error, as in pex. (Corrected after polling: the draft said S1.05 already rejects malformed arguments, but S1.05 only lets a command tell a missing argument from an empty one; prompt log entry 076.)
- **Decision:** keep, strict: all, least and <n>,<e>, and a selector with trailing text such as `7,1x` is an error, as in pex (prompt log entries 075 and 076)

### S6.04 · psp: which edges are drawn in each path mode

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8982-9071`
- **What it does:** For each selected point:
  - The active nodes are the node lines marked `active`. The candidate edges are the edge lines whose two endpoint fields are `src_active` and `tgt_active`, whatever their subtype.
  - A breadth-first search over the candidate edges runs forward from the source, if it is active, giving `dsrc`. A second runs backward from the target, giving `dtgt`. `L = dtgt(src)` is the shortest source-to-target hop count.
  - If `L` is infinite and the mode is not `subgraph`, psp prints `plot_solution_paths: no active src->tgt path at PO (n,e) in <file> - skipping` and writes nothing for that point. `L` is infinite when no such path exists, or when the source or target is inactive, absent, or not given as a rep id.
  - An edge (u, v) is kept when `dsrc(u)` and `dtgt(v)` are finite, testing `through = dsrc(u) + 1 + dtgt(v)` by mode:
    - `shortest`: `through == L`;
    - `alt`: `through <= L + W`;
    - `all`: any finite `through`;
    - `subgraph`: every candidate edge, with no test.
  - In `subgraph` mode the drawn nodes are all active nodes, isolated ones included. In the other modes they are the endpoints of kept edges, plus the source and target when active.
  - Cost is two searches and one pass, O(V + E) per point.
  - psp replay, full JUN run with the FSC, point (2,6):
    - `shortest`: 6 nodes and 6 edges;
    - `alt` (W = 2): 6 and 6;
    - `all`: 76 and 103;
    - `subgraph`: 79 and 106.

    `L` ranges from 5 to 9 hops across that run's points, under its bound of 10.
- **Problems:**
  - `alt` and `all` work on walks, not simple paths. An edge counts if the shortest walk through it fits, even when no simple path does. For example, an edge back into the source from a node one hop away passes `alt:2`. This is the same issue as S4.11. The replay found no such edge in the full JUN run: in every mode, no drawn edge enters the source or leaves the target. So the issue is real but has not shown up in this data.
  - The choice of candidate edges is wrong in two ways, covered in S6.05 and S6.06.
- **Proposed call:** keep. Keep the distance filter and the four modes, and document `alt` and `all` as walk-based; enumerating simple paths would be exponential.
- **Decision:** keep, as proposed (prompt log entry 075)

### S6.05 · Bug: psp draws relaxed inhibition edges as ordinary path edges

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8910-8913`, `8995-9002`, `9103`; the solver's rule at `GraphManagerNew.cc:28552-28595`
- **What it does:** psp's candidate edges are those with both endpoints active, whatever their sign. psp never reads the relaxations file, and it labels each edge with its raw subtype.
- **Problems:**
  - Under the solver's rule (`28552`), an edge carrying `inhibition` or `repression` whose two endpoints are both active is relaxed: it counts against the edge budget. psp nevertheless draws it as a signal-carrying hop.
  - Example from the psp replay, full JUN run, without the FSC, point (2,7). One shortest EGFR → CXCL8 path is EGFR → SRC → CDC42 → MAPK14 → TP53 → DUSP1 ⊣ MAPK1 → ETS1 → CXCL8.
    - Edge 28246, DUSP1 → MAPK1 `inhibition_dephosphorylation`, is one of that point's 7 relaxed edges in `temp_wo_relaxations.txt`.
    - psp would draw it as a plain path edge.
  - Each solution file of the three b10 full runs (JUN, DUSP1 and FOSB) has 3 to 5 distinct such edges. Summed over all of a run's PO points they come to 7 to 11 edge lines (with and without the FSC: 7/7, 11/9 and 8/8).
- **Proposed call:** fix. A path view must follow only edges whose effect the solution honours, or else mark relaxed edges as relaxed. Either way it must use the solver's polarity rule.
- **Decision:** fix: a path view follows the witness's edges as the solver chose them and draws relaxed edges as relaxed, by the solver's polarity rule; sign-consistent routes come from S5.13's separate report (prompt log entry 076)

### S6.06 · Bug: both renderers hide honoured inhibitions and the nodes they switch off

- **FuSION 1:** psp `Linux/src/GraphManagerNew.cc:8997-8998`; pex `Linux/supporting_files/plot_explanation.py:208-214`, `236-252`; the solver's rule at `GraphManagerNew.cc:28552-28595`
- **What it does:** Both renderers build the explanation from edges whose two endpoints are active. pex then adds the relaxed nodes and edges; psp adds nothing.
- **Problems:**
  - Under the solver's rule, an inhibition edge is honoured exactly when its two endpoints differ: active ⊣ inactive, or inactive ⊣ active. The solutions file lists those edges as present, yet neither renderer ever draws one. Neither draws a present inactive node either, unless it is the end of a relaxed edge.
  - Example, full JUN run, with the FSC, point (7,0):
    - CDKN1A ⊣ CDK2 (`src_active tgt_inactive`) and CDK2 ⊣ FOXO6 (`src_inactive tgt_active`) form a present double negative.
    - CDK2 (`hsa1017`) and ID1 (`hsa3397`) are present and inactive.
    - CDKN1A and FOXO6 appear in `run/pex_incl_n7_e0.html`, but CDK2, ID1 and the two inhibition edges do not.
  - Across the 569 renders of the pex replay, 1192 present edges that have an inactive end and are not relaxed go undrawn, and so do 302 present nodes.
  - A down-regulated gene explained through an inhibition therefore never shows its explanation.
- **Proposed call:** fix. Draw every present node and edge of the witness, showing each node's activity and whether each edge is honoured or relaxed.
- **Decision:** fix, as proposed. This answers open question 5: the explanation is every present element of the witness (prompt log entry 076)

### S6.07 · psp's DOT and SVG output

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9073-9112`; `Linux/include/macros.h:33` (`DOT_PATH_NAME "/usr/bin/dot "`, which psp does not use); `Linux/README.md:35`
- **What it does:** For each drawn point, psp writes `<out_prefix>_n<n>_e<e>.dot`:

  ```
  digraph G {
  rankdir=LR;
  label="shortest : hsa1956 -> hsa3576  [PO 4 nodes, 0 edges relaxed]";
  labelloc=top; labeljust=left; fontname="Helvetica";
  node [shape=box, style=filled, fillcolor="#EEEEEE", fontname="Helvetica"];
  edge [fontname="Helvetica", fontsize=9];
    "hsa1956" [label="EGFR", fillcolor="#A6E3A1"];
    "hsa1956" -> "hsa5335" [label="activation"];
  }
  ```

  - The source is filled `#A6E3A1` (green), the target `#F5A9A9` (red), and every other node `#EEEEEE`.
  - Nodes are written in rep-id order. There is one edge per distinct (source, target, subtype), so a pair carrying both `expression` and `activation` gets two parallel edges.
  - psp then runs `system("dot -Tsvg <dot> -o <svg>")`, finding `dot` on `PATH`. A non-zero status prints `plot_solution_paths: graphviz returned <rv> for <dot>`, and the CSV is still written. The `.dot` file stays on disk.
  - The label shows `alt` without its window.
- **Problems:**
  - File names reach the shell unquoted. A prefix containing a space breaks the command, and a shell metacharacter in it runs as shell syntax.
  - Labels are not escaped, so a `"` in a symbol breaks the DOT file.
  - Each point costs one `dot` process and a `.dot` file round trip.
  - A missing `dot` is reported only after the `.dot` has been written.
  - `Linux/README.md:35` says FuSION expects `dot` at `/usr/bin/dot` and that pex uses it. psp looks `dot` up on `PATH`, and pex does not use Graphviz at all.
  - The drawing has no expression colours, no polarity and no relaxations (S6.05, S6.26).
- **Proposed call:** fix. Keep a static export, but escape names and labels, check for `dot` before the run starts (decision 0021), and correct the README.
- **Decision:** fix, as proposed (prompt log entry 076)

### S6.08 · psp's edge-sources CSV

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:9114-9138`; name lookup `get_pathway_names_from_path_id`, `GraphManagerNew.cc:8788-8799`
- **What it does:**
  - psp writes `<out_prefix>_n<n>_e<e>_edge_sources.csv` with the header `edge_index,source_symbol,source_hsa,target_symbol,target_hsa,subtype,pathway_ids,pathway_names`.
  - There is one row per kept edge line, not deduplicated, sorted by source symbol, then target symbol, then subtype.
  - Every field after the index is wrapped in `"`. Pathway ids are joined with `;`.
  - Names come from `start`'s pathway-name map (S1.19), falling back to the id.
  - No real CSV exists. The edge on line 28 of the dump quoted in S6.02 would give `<i>,"BRAF","hsa673","MAP2K1","hsa5604","phosphorylation","path:hsa04010;path:hsa04510","MAPK_signaling_pathway;Focal_adhesion"`.
- **Problems:**
  - The CSV rows do not match the drawn edges. Two edge ids with the same (source, target, subtype) give two rows but one DOT edge. For the full JUN run, with the FSC, point (2,6), mode `all`, that is 104 rows for 103 drawn edges.
  - A `"` inside a field is not doubled, which would make the CSV invalid.
  - psp needs `start` only for this lookup, while pex reads the same map file itself.
- **Proposed call:** fix. Write one row per drawn edge listing all its edge ids, quote fields to the CSV standard, and take names from the run's one pathway-name map.
- **Decision:** fix, as proposed (prompt log entry 077)

### S6.09 · psp's messages and failures

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8841`, `8848`, `8931`, `8960`, `8966`, `9036`, `9112`, `9140-9142`
- **What it does:**
  - Each drawn point prints `plot_solution_paths: PO (n,e) mode=<mode> -> <svg> (<k> nodes, <m> edges); sources -> <csv>`.
  - The errors are `cannot open solution file <f>`, `no PO points parsed from <f>`, the selector and mode errors (S6.01, S6.03), the missing-path skip (S6.04) and the Graphviz status (S6.07).
  - All of these go to stderr, and the batch carries on.
- **Problems:**
  - `<m>` counts CSV rows, not drawn edges (S6.08).
  - Output streams are not checked. With an unwritable prefix the DOT file is never written, `dot` fails on the missing file, and psp prints `graphviz returned 512` followed by its success line (`GraphManagerNew.cc:9110-9112`, `9140`).
  - No failure changes the exit status (S1.28).
- **Proposed call:** fix. Make a failure fail the command, following S1.28's decision, and print counts that match what is drawn.
- **Decision:** fix, as proposed (prompt log entry 077)

### S6.10 · `pex` / `plot_explanation` command line

- **FuSION 1:** `Linux/src/main.cc:5691-5764` (batch tokens at `5716-5726`, command string at `5742-5759`), help line `main.cc:908`; `macOS/src/main.cc:5719-5791` (the same apart from S6.11)
- **What it does:** The batch form is

  `pex <incl_soln> <incl_relax> <excl_soln> <excl_relax> <lfc_file> <pathmap_file> <src> <tgt> <both|incl|excl> <all|least|<n>,<e>> <out_prefix> [<site_dir> <run_key> [<po_png>]]`

  - The dispatcher builds `python3 "<exe_dir>/supporting_files/plot_explanation.py" --incl-soln "…" --incl-relax "…" --excl-soln "…" --excl-relax "…" --lfc "…" --pathmap "…" --src <src> --tgt <tgt> --cases <c> --po <p> --out-prefix "…"` and runs it with `system()`.
  - It adds `--site-dir` when token 12 is non-empty, `--run-key` when token 13 is non-empty, and `--po-png` when token 14 is neither empty nor `0`.
  - A file argument of `0` means "none".
  - pex does not need `start`.
  - The reference run's line is `run_console.log:375`: `pex …/run/temp_w_solutions.txt …/run/temp_w_relaxations.txt …/run/temp_wo_solutions.txt …/run/temp_wo_relaxations.txt …/inputs/egf_hela_t120_lfc.tsv …/inputs/hsa_path_to_path_name_map.txt hsa1956 hsa3576 both all …/run/pex`.
  - 24 batch files call pex:
    - 22 use `both all`;
    - the single-solve runs `r7_epha2_curated` and `r8_phlda1_curated` pass `0 0` for the without-FSC files and `incl all`.

    No batch passes the site tokens.
- **Problems:**
  - The tests `token_cmd.size() > 13` and `> 14` (`5724-5725`) are always true (S1.05). The optional tokens work only because empty strings are skipped afterwards.
  - `--src`, `--tgt`, `--cases` and `--po` are not quoted, and quoted values are not escaped.
    - An empty source gives `--src --tgt …`. argparse rejects it with exit status 2, which pex reports as `renderer returned 512`.
    - A `"` or `$` in a path is interpreted by the shell.
  - `--run-title`, `--takeaway` and `--site-title` can be set only by running the script directly (S6.19).
  - Extra tokens are ignored.
- **Proposed call:** fix. Use named arguments (decision 0018), pass each value as its own argument without a shell, reject missing required arguments, and make every site option settable from a batch.
- **Decision:** fix, as proposed (prompt log entry 077)

### S6.11 · Finding the helpers, and what they need installed (Linux and macOS differ)

- **FuSION 1:**
  - Linux `Linux/src/main.cc:5728-5740`.
  - macOS `macOS/src/main.cc:31` and `5756-5767`.
  - `Linux/src/GraphManagerNew.cc:9109` (`dot`).
  - The reference batch's `plot_po.sh` line, line 262.
  - `Linux/README.md:35-47`, `macOS/README.md:49-66`.
- **What it does:**
  - **pex**:
    - The script is `<directory of the running binary>/supporting_files/plot_explanation.py`.
    - Linux finds that directory with `readlink("/proc/self/exe")`, which resolves symlinks.
    - macOS uses `_NSGetExecutablePath`, which returns the path the binary was started by. That path is unresolved and may be relative.
    - On both systems, a failed lookup falls back to `.`. S1.20 owns this lookup and has already decided to fix it.
    - The interpreter is the first `python3` on `PATH`, and it needs `pyvis`. The repo's `.venv` has pyvis 0.3.2 on Python 3.12.3. The system `/usr/bin/python3` on this machine lacks pyvis.
    - Besides pyvis and its dependencies (jinja2, networkx, jsonpickle, and ipython, which pyvis 0.3.2 imports at `pyvis/network.py:10`), the script uses only the standard library.
    - The repo has no requirements file. The READMEs say `pip install pyvis gseapy`.
  - **psp** needs `dot` on `PATH`. On this machine `dot` exists only in the conda environment `/home/ks-149-u24/miniconda3/envs/graphviz/bin` (Graphviz 14.1.2), which `run_sweep.sh` puts on `PATH`.
  - **plot_po.sh** is not located by the binary at all. Each batch names it by an absolute path and runs it with `bash` through `exec`. It needs `bash`, `awk`, `mktemp`, and `gnuplot` with the `pngcairo` terminal (6.0 on this machine).
  - Nothing is checked before the run. A missing pyvis shows up only at the end, as `plot_explanation: renderer returned 256 (is python3 with pyvis installed? script: …)`.
- **Problems:**
  - Three conventions coexist for finding helpers (S1.20).
  - The Linux README is wrong about `/usr/bin/dot` and about pex using Graphviz.
  - Dependencies are discovered only when their step runs, which goes against decision 0021's up-front validation.
  - The macOS copy of `plot_po.sh` uses GNU `mktemp --suffix` (open question 1).
- **Proposed call:** fix. Adopt S1.20's single install-directory lookup, list every helper's dependencies in one place, and check them before the first step runs.
- **Decision:** fix, as proposed (prompt log entry 077)

### S6.12 · pex's other inputs: optional files, the LFC file and pathway names

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:83-84`, `132-165`, `534-539`, `696-697`
- **What it does:**
  - Every file argument is optional. `0`, an empty string, or a path that is not a regular file all count as absent, without a message.
  - What happens when each is absent:
    - a missing solutions file prints `plot_explanation: no PO points parsed from <file> (skipping <case> case)` on stderr;
    - a missing relaxations file silently drops the overlay;
    - a missing LFC file makes the hover show `lfc: n/a`;
    - a missing map shows pathway ids instead of names.
  - LFC file: the first two whitespace fields of each line, read with `float()`. Unparsable lines are skipped and a repeated gene keeps its last value (compared with the other readers in S3.14).
  - Pathway map: each line is split at the first tab, else at ` => `, else at the first space. The name is stripped, an empty name falls back to the id, and later lines win. Unlike `start`'s reader (S1.19), names with inner tabs survive.
- **Problems:**
  - A mistyped path silently removes part of the output. Decision 0021 says a named but unreadable file stops the run.
  - The LFC and pathway-map files are parsed a second time, in Python, with rules that differ from the C++ readers (S3.14, S1.19).
- **Proposed call:** fix. A named file that cannot be read is an error (decision 0021), and `0` means "not given". Reuse the run's one LFC reader (S3.14) and its one pathway-name map (S1.19).
- **Decision:** fix, as proposed (prompt log entry 078)

### S6.13 · pex: which nodes are drawn, the layout and the relaxed-node table

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:176-194` (`_bfs_levels`), `197-285`, `375-392`, `440-464` (`_parked_table_html`)
- **What it does:**
  - Edges are grouped into ordered (source, target) pairs. A pair comes either from a solution edge whose endpoints are both flagged active and both in the active node set, or from any edge in the relaxations block.
  - Symbols and expression classes are taken first from the relaxation lines, then from the solution lines, so the solution's values win.
  - The draw set is the active nodes, the relaxed nodes, and every endpoint of a pair.
  - A node that belongs to no pair is "parked": it is removed from the graph and listed in a collapsible table at the bottom of the page, headed `Relaxed-away nodes (no surviving edge): <k>`. The table has columns symbol, hsa, log-FC and expression, sorted by symbol.
  - Layout:
    - Each node's level is its hop distance from the source over all pairs; unreachable nodes go one level past the deepest.
    - vis.js draws a hierarchical left-to-right layout with level separation 260, node spacing 150 and tree spacing 220, and physics off.
    - Nodes are boxes with font size 24 and a maximum width of 230.
    - Edges between two nodes on the same level are curved, alternating clockwise and anticlockwise with roundness 0.6.
  - pex replay: 1969 parked nodes in 569 renders. For example, the full JUN run's with-FSC point (7,0) draws 54 nodes and 69 edges and parks 7 nodes.
- **Problems:**
  - The table's heading and legend call every parked node relaxed, but 84 parked nodes in the replay were never relaxed. One example is BCL2 (`hsa596`) in `runs/egf_hela/experiment/egf_to_il8_fsc_nfkb1_b15`, with the FSC, point (5,10). Such a node is active but its only edges are honoured inhibitions (S6.06).
  - If the source is not a rep id in the dump, every node gets level 1 and the drawing collapses into one column. No run hits it for that reason, but the same collapse happens when the source has no drawn edge. In `runs/egf_hela/experiment/egfr_to_il8_fsc_nfkb1_b6_t700_f800_nr4_er4_on`, without the FSC, at point (0,1), EGFR's only edge is the honoured inhibition EGFR ⊣ CTNND1. EGFR is therefore parked and listed as relaxed-away, though it was never relaxed, and all 6 drawn nodes get level 1 (`plot_explanation.py:181-193`, `248-252`).
- **Proposed call:** fix. Keep the layout and the table, and make the table say why each node is in it: relaxed, or without drawn edges.
- **Decision:** fix, as proposed. With S6.06's decision, honoured inhibitions are drawn, so the table holds the relaxed nodes that are not in the witness (prompt log entry 078)

### S6.14 · pex: edge polarity, one edge per effect

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:48-65`, `287-334`; the solver at `Linux/src/GraphManagerNew.cc:28552` and `1856-1874` (`edge_has_subtype`)
- **What it does:**
  - `_effect(subtype)` returns `inh` when any `_`-separated part of the dumped subtype is `inhibition` or `repression`, and `act` otherwise.
  - The solver asks whether the edge's subtype list contains `inhibition` or `repression` (`28552`), so the two rules agree. `dephosphorylation` and `ubiquitination` on their own count as activation.
  - Within a pair, records are deduplicated by subtype, with their pathways merged, and drawn as at most one edge per effect:
    - `act` is a grey `#8a8a8a` arrow;
    - `inh` is a red `#C62828` T-bar;
    - solid edges are 2 wide and dashed edges 3.
  - The hover shows `<s> -> <t>`, a note, `subtypes: …`, and `pathways:` with one name per line.
  - Lead check: the lead that the renderer and solver disagree describes the script before 2026-07-21. `INHIBIT_SUBTYPES` has been exactly `{"inhibition", "repression"}` since then (line 56).
- **Problems:** The rule exists twice, once in C++ and once in Python, linked only by a comment.
- **Proposed call:** keep, with the polarity rule defined in one place that both the solver and the renderer use.
- **Decision:** keep, as proposed: one polarity rule shared by the solver and the renderer (prompt log entry 078)

### S6.15 · pex: how relaxed edges are drawn (removed, flipped, twin)

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:215-234`, `328-373`; the solver's rule at `Linux/src/GraphManagerNew.cc:28552-28595`
- **What it does:**
  - A both-active solution edge counts as relaxed when its (source, target, subtype) appears in the relaxations block. Every edge in that block counts as relaxed, `not_present` ones included.
  - Each pair is then drawn one of four ways:
    1. No relaxed record: one solid edge per effect.
    2. Only relaxed records: one dashed `[8,6]` edge per effect, with the note `<effect> effect REMOVED by relaxation`.
    3. The kept and relaxed effects differ: one purple `#8E44AD` dotted `[2,6]` edge with the surviving effect's arrowhead and the note `net effect FLIPPED <gone> -> <surviving> by relaxation`.
    4. The same effect is both kept and relaxed: the kept edge solid, plus a dashed twin curved aside (`curvedCW`, roundness 0.35).
  - The pex replay drew 45198 solid edges (case 1), 1917 dashed ones (case 2), 8 flips (case 3) and no twins (case 4).
- **Problems:**
  - Case 4 is the lead's "dashed curved twin", and it can no longer happen.
    - Under the solver's rule (`28572-28592`), an activation-family edge is relaxed unless both its ends are active.
    - So a kept edge with both ends active is always activation-family, and a relaxed edge on the same pair must be inhibition-family. That is case 3.
    - Case 4 last fired when dephosphorylation still counted as inhibition.
  - A relaxed edge that is not part of the witness (`not_present`; 667 such lines in the relaxation files dated 2026-11-10 or earlier) is drawn exactly like a relaxed edge of the explanation.
  - Relaxed status is matched on (source, target, subtype), not on the edge id. Two edges with the same triple share one status. That is harmless today, because the solver's rule gives them the same status anyway.
- **Proposed call:** fix. Keep cases 1 to 3 and drop case 4. Match relaxed edges by edge id, and show whether each relaxed edge is part of the witness. Whether `not_present` edges should count as relaxed at all is stage 5's question (open question 4).
- **Decision:** fix, as proposed. Relaxed `not_present` edges count as relaxed (S5.13, open question 4) and are shown as outside the witness (prompt log entry 078)

### S6.16 · pex: node colours, and descriptions that say otherwise

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:8-12` (docstring), `32-39`, `168-173`, `254-285`, `396-437` (legend); `macOS/model_example/RUNNING.md:226`
- **What it does:**
  - Nodes are filled by the expression class in the dump: `up_reg` `#8CCB4E` (green), `down_reg` `#F2A24C` (orange), anything else `#DEDEDE` (grey).
  - Borders:
    - the default is `#555555`, width 1;
    - the source is `#1B5E20` and the target `#0D3B66`, both width 4;
    - a relaxed node gets a dotted `[2,2]` border at least 2 wide.
  - The hover shows `<symbol>  (<rep id>)`, `lfc: <2 decimals or n/a>`, `expr: <class>`, and the flags SOURCE, TARGET, relaxed or inactive.
  - A legend at the top shows the same scheme, though its dash patterns (`6,4`, `2,3`) and 3 px rings differ from what is drawn (`[8,6]`, `[2,6]`, 4 px; `plot_explanation.py:417-426`).
- **Problems:**
  - The docstring (line 8) says "up-regulated = red, down-regulated = blue", and `RUNNING.md:226` says nodes are "red/blue for up/down-regulated". The code draws green and orange, which confirms the lead.
  - The fill shows the observed expression class, not the solved activity, which appears only in the hover.
  - psp uses a different scheme (S6.26).
- **Proposed call:** keep. Port the drawn colours and legend as they are; the docstring and `RUNNING.md` are what is wrong, not the code.
- **Decision:** keep, with the legend made to match what is drawn and the docs corrected; how solved activity is shown visually is left to decision 0022's design (prompt log entry 079)

### S6.17 · pex: the HTML page

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:198-200`, `467-510` (`RESPONSIVE_CSS`, `FIT_JS`, `_finalize_html`), `531-558` (`render_case`)
- **What it does:**
  - One page is written per case and point: `<out_prefix>_<incl|excl>_n<n>_e<e>.html`.
  - pyvis builds the page with `Network(height="1000px", width="100%", directed=True, cdn_resources="in_line")`.
  - Regular expressions then edit it:
    - they remove pyvis's `<center><h1>`, the Bootstrap link and script, and the `node_modules` references;
    - they add `RESPONSIVE_CSS`, which sizes the canvas to the window;
    - after `<body>` they add the title, the subtitle and the legend;
    - before `</body>` they add the relaxed-node table and `FIT_JS`, which calls `network.fit()` on load and on resize.
  - The title is `Explanation subgraph (<with FSC|without FSC>) : <src> &rarr; <tgt> &mdash; PO <n> nodes, <e> edges relaxed`.
  - The subtitle is always `Showing one representative solution (of several) found at this PO point.`
  - A real title, from `runs/egfr_b10_gsea_fdr044_candidates/r7_epha2_curated/run/pex_incl_n7_e0.html`: `Explanation subgraph (with FSC) : hsa1956 &rarr; hsa1969 &mdash; PO 7 nodes, 0 edges relaxed`.
- **Problems:**
  - Every `incl` page says "with FSC". `r7_epha2_curated` and `r8_phlda1_curated` are single-solve runs with no FSC (their active-nodes file is `active_nodes_file_w_target_only`).
  - The title names the source and target by rep id, not by symbol.
  - "(of several)" is hard-coded. When pathz3 writes `No more solutions. Obtained unsat` after `Solution 1`, the witness is unique; two toy dumps under `runs/test_examples/` show this.
  - Editing pyvis's template with regular expressions ties the page to the exact markup of pyvis 0.3.2.
- **Proposed call:** fix. Build the title from the run's real case and from symbols, claim "of several" only when it is known, and generate the page directly instead of patching a library template.
- **Decision:** fix, as proposed (prompt log entry 079)

### S6.18 · Cost: every pex page carries its own copy of vis.js

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:198-200` (`cdn_resources="in_line"`), `486-496`
- **What it does:** Each page inlines vis-network's JavaScript and CSS, so it works offline as a single file.
- **Problems:**
  - On 2026-11-11, `runs/` held 238 pex pages, 174.5 MB in all, each over 700 KB.
  - In the reference run's 707 KB page, one inline script (468 KB) and one inline style (220 KB) are the library: about 97% of the file. The graph data is about 7 KB.
  - The public site in `misc/FuSION-explanations` holds 37 such pages, 27 MB.
- **Proposed call:** fix. Ship the library once per output directory or site, or serve it from the 2.0 frontend, so that pages still work offline.
- **Decision:** fix, as proposed (prompt log entry 079)

### S6.19 · pex site mode

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:561-670`, `686-693`, `699-739`; `Linux/src/main.cc:5698-5701`, `5754-5759`
- **What it does:** With `--site-dir D`:
  - Pages go to `D/<run-key>/`, using `run` when no key is given. The prefix is forced to `pex`, and `--out-prefix` is ignored.
  - pex writes `D/<key>/index.html`. It has a title, the Pareto image copied from `--po-png` as `pareto.png`, and one column per case listing the PO points sorted by (n + e, n), each linked to its page.
  - It writes `D/<key>/manifest.json`. Real example, `misc/FuSION-explanations/jun_b10/manifest.json`: `{"key": "jun_b10", "title": "EGFR → CXCL8 · FSC = JUN (bound 10)", "takeaway": "…", "png": "pareto.png", "incl": [[2, 6], [3, 4], …], "excl": [[2, 7], …]}`. The file stores `→` and `·` as the escapes `\u2192` and `\u00b7`; they are shown decoded here.
  - It then rebuilds `D/index.html` from every `D/*/manifest.json`. That page has one card per run, carrying a thumbnail, the title, and the takeaway or else `with FSC min k relaxations; without FSC min k`.
  - The default run title is `<src> → <tgt>`.
  - It prints `plot_explanation: site updated at D/index.html  (run '<key>', landing now lists <k> experiment(s))`.
  - No batch uses site mode. The three-run public site was built by running the script directly with `--run-title` and `--takeaway`.
- **Problems:**
  - The title and takeaway cannot be set from a batch (S6.10).
  - `--out-prefix` is silently ignored in this mode.
  - The manifest file is never explicitly closed.
- **Proposed call:** fix. Keep the static report (run page, manifest, landing page) and make every option a batch argument. Whether the 2.0 frontend replaces it is for decision 0022's design.
- **Decision:** fix, as proposed (prompt log entry 079)

### S6.20 · pex's messages and exit status

- **FuSION 1:** `Linux/supporting_files/plot_explanation.py:555-556`, `720-724`; `Linux/src/main.cc:5760-5763`
- **What it does:**
  - Each page prints `plot_explanation: <case> PO (n,e) -> <file>  (<k> nodes, <m> edges)`, where the counts exclude parked nodes. A final line follows: `plot_explanation: wrote <total> interactive HTML file(s)`.
  - If nothing is drawn, the script prints `plot_explanation: nothing rendered` on stderr and exits 1. The dispatcher then prints `plot_explanation: renderer returned 256 (is python3 with pyvis installed? script: …)`.
- **Problems:**
  - The dispatcher prints the raw wait status (256 for exit 1, 512 for argparse's exit 2) and the same pyvis hint for every cause.
  - A run in which one case is skipped still exits 0.
  - The batch continues regardless (S1.28).
- **Proposed call:** fix. Report the actual cause, and make a failure fail the command, following S1.28's decision.
- **Decision:** fix, as proposed (prompt log entry 080)

### S6.21 · Bug: a solve without PO points leaves the renderers a missing or stale file

- **FuSION 1:** writer `Linux/src/main.cc:4855-4856` (the PO `.dat` is truncated first), `4951-4955` (`continue` when no point was found) and `4964-4967` (the solutions and relaxations files are opened only after that); `Linux/supporting_files/plot_explanation.py:83-84`, `534-538`; `Linux/supporting_files/plot_po.sh:52-53`
- **What it does:**
  - When pathz3 finds no PO point, it leaves an empty `.dat` file but neither creates nor truncates `<prefix>_solutions.txt` and `<prefix>_relaxations.txt`.
  - In the reference run, the without-FSC solve printed `0 PO points found` and `No point in PO curve` (`run_console.log:360-361`).
    - `plot_po.sh` then labels that curve `without FSC (JUN): no solution within bounds`.
    - pex prints `plot_explanation: no PO points parsed from …/temp_wo_solutions.txt (skipping excl case)` (`run_console.log:377`).
- **Problems:**
  - If an earlier run left a `temp_wo_solutions.txt` in the same directory, pex and psp draw that old file as if it were current, while the Pareto plot says "no solution". The outputs of one run then contradict each other without any warning.
  - "File missing" and "no solution" look the same to the renderers.
- **Proposed call:** fix. Stage 5 always writes each case's results, including an explicit "no PO point" result, and the renderers report that result as such.
- **Decision:** fix, as proposed (prompt log entry 080)

### S6.22 · `plot_po.sh`: arguments and how batches call it

- **FuSION 1:** `Linux/supporting_files/plot_po.sh:16-30`, `39-40`; the reference batch, line 262
- **What it does:**
  - The usage is `plot_po.sh WITH.dat WITHOUT.dat OUT.png SRC TGT FSC FSC_ID BOUND COEXP FROZEN NODE_UB EDGE_UB`: 12 positional arguments under `set -eu`.
  - Each `.dat` holds one `n<TAB>e` line per PO point (stage 5).
  - The reference batch calls it as `exec cd $WRITE_DIR; bash /home/ks-149-u24/Desktop/arjun/FuSION/Linux/supporting_files/plot_po.sh $WRITE_DIR/$PO_POINTS_WITH_FSC.dat $WRITE_DIR/$PO_POINTS_WITHOUT_FSC.dat $PLOT_FILE EGFR CXCL8 JUN $NODE_TO_TEST_FOR_SIGNIFICANCE $REACH_PATH_BOUND $COEXP_THRESH $FROZEN_THRESH $NODE_RELAX_UB $EDGE_RELAX_UB`.
  - 22 batch files call it, all with bounds of 8 and 8.
  - Two more mention it only in a comment: `;; Single solve: the PO plot is drawn by the report (plot_po.sh needs two arms).` (`r7_epha2_curated.batch:126`).
  - The source, target and FSC labels are typed by hand. All 22 match their ids, e.g. JUN/`hsa3725`, DUSP1/`hsa1843`, and `'Rho family'`/`rhoKO`.
- **Problems:**
  - All 22 files contain an absolute path into one user's checkout.
  - The labels are typed by hand instead of taken from the symbol map, so they can drift from the ids.
  - A variable with an empty value disappears when the shell splits words. Every later argument shifts left, and `set -u` stops at the missing 12th.
  - Only two curves can be drawn, so single-solve runs have no plot.
  - `mktemp --suffix` is a GNU option (open question 1).
  - The exit status is ignored (S1.14).
- **Proposed call:** fix. Make the Pareto plot a built-in step that takes the run's own results, ids and symbols, and draws one curve or two.
- **Decision:** fix, as proposed: a built-in Pareto plot step that takes the run's own results, ids and symbols and draws one or two curves; where and with what it is drawn is decided in Phase 2 (prompt log entry 080)

### S6.23 · `plot_po.sh`: the Pareto plot

- **FuSION 1:** `Linux/supporting_files/plot_po.sh:34-37`, `46-78`
- **What it does:**
  - gnuplot draws a `pngcairo enhanced` image, 980×690, in Liberation Sans 13 on `#fcfcfb`.
  - The title is `{/:Bold <SRC> → <TGT>     ·     Pareto frontier}`. Below it, a subtitle reads `FSC <FSC> (<FSC_ID>)    ·    bound <B>    ·    coexp ≥ <C>, frozen ≥ <F>    ·    ≤ <NUB> node / ≤ <EUB> edge`.
  - Axes:
    - x is labelled "Node relaxations" and y "Edge relaxations";
    - each range is [−0.5, UB + 0.5] with integer tics;
    - only the left and bottom borders are drawn, and the grid is `#e1e0d9`;
    - the key sits top right in a box.
  - Curves:
    - with the FSC: line style 11, blue `#2a78d6`, width 2.4, circles (`pt 7 ps 1.5`);
    - without it: line style 12, red `#e34948`, squares (`pt 5 ps 1.4`).
  - Each marker sits on a halo of the background colour (`ps 2.3` and `ps 2.2`), and points are joined in file order.
  - An empty `.dat` gets a key entry `…: no solution within bounds` instead of a curve.
  - Real output: `runs/egfr_cxcl8_fosb_b10_nr8_er8/run/PO_hsa1956_to_hsa3576_fsc_hsa2354_egf_hela_t120_lfc.tsv_b10.png`.
- **Problems:**
  - The subtitle prints the coexpression and frozen thresholds whether or not the run enabled those constraints.
  - If Liberation Sans is missing, fontconfig substitutes another font without a message.
- **Proposed call:** keep. Port the look as the 2.0 Pareto plot: palette, markers, halos and the key entry for an empty curve.
- **Decision:** keep the look, with the subtitle listing only the constraints the run used (S5.03) and the font checked with the other dependencies (S6.11) (prompt log entry 080)

### S6.24 · `plot_po.sh`: points present on both curves are moved apart

- **FuSION 1:** `Linux/supporting_files/plot_po.sh:32`, `41-44`
- **What it does:**
  - Two awk passes move every point that is present in both files: by +0.10 in y on the with-FSC curve and by −0.10 on the without-FSC curve. Other points keep their integer coordinates.
  - The test is `FILENAME==ARGV[1]` rather than `FNR==NR`, so an empty first file does not swallow the second.
  - The FOSB run's two curves are identical. A reproduction of the two awk lines gives `7 0.1, 6 1.1, … 2 6.1` and `7 -0.1, 6 0.9, … 2 5.9`. The PNG shows two parallel lines whose markers touch, which confirms the lead.
- **Problems:** The offset is in data units, so its size on screen shrinks as `EDGE_UB` grows. It is tuned for 8, the value all 22 calls pass. With a larger bound the markers overlap again; with a smaller one they separate.
- **Proposed call:** fix. Keep the offset, but size it from the axis range so the markers touch at any bound.
- **Decision:** fix, as proposed (prompt log entry 081)

### S6.25 · Inline gnuplot lines in batch files and GUI templates

- **FuSION 1:** `Linux/frontend/src/gui4.java:4462`, `4470`; `Linux/advtempscript.txt:337`, `345` (frontend1's template); the plot inside pathz3 at `Linux/src/main.cc:4957-4962`
- **What it does:**
  - 62 of the 89 batch files contain 130 inline `exec … gnuplot -p -e "…"` lines:
    - 56 open an on-screen window: no `set term`, `-p` to keep it open, with-FSC in blue and without-FSC in red, drawn with `lp`;
    - 60 write `$PLOT_FILE` with `set term png size 900,650`, a title, and `pt 7` and `pt 5` markers;
    - 12 write `$PLOT_FILE_WO_GENE`, which S1.07 breaks;
    - 2 others write `pareto.png`.
  - Both GUIs write only the plain pair: a window line and a `set term png` line. No GUI writes psp, pex or `plot_po.sh`.
  - pathz3 draws nothing itself. Its `system("gnuplot -p -e \"set term png; set output 'PO.png'; plot 'PO.dat' with linespoints\"")` sits inside a `/* … */` block marked `Commented by Supratik: Nov 19, 2019 … Plotting of PO curves is now automatically taken care of from input script`. The earlier note that pathz3 has a live gnuplot fallback is wrong.
- **Problems:**
  - Three Pareto-plot styles coexist.
  - The window lines need a display: under the GUI each run pops up a window, and a headless run can only fail to open it.
  - Each plot is a hand-written shell command with no error check (S1.14).
- **Proposed call:** drop. The 2.0 GUI emits the built-in plot step (S6.22). Old batch files keep working, because `exec` passes these lines to the shell (S1.14).
- **Decision:** drop, as proposed (prompt log entry 081)

### S6.26 · Two renderers that disagree about one solution

- **FuSION 1:** psp `Linux/src/GraphManagerNew.cc:8823-9144`; pex `Linux/supporting_files/plot_explanation.py:197-393`
- **What it does:** psp and pex draw the same witness differently:
  - **Nodes:** psp applies a path filter to the both-active edges. pex draws the whole both-active subgraph plus the relaxed overlay.
  - **Edges:** psp draws one edge per subtype, pex one per effect. In the reference run at (1,0), MAP3K1 → MAP2K7 and MAPK8 → JUN each carry `phosphorylation` and `activation`, while JUN → CXCL8 carries only `expression` (`run/temp_w_solutions.txt:14-19`), and the totals are 10 edges in psp's drawing against 8 in pex's.
  - **Colours:** psp fills the source green (`#A6E3A1`), the target red (`#F5A9A9`) and everything else grey. pex fills nodes by expression class and rings the source dark green and the target dark blue.
  - **Relaxations:** psp ignores them (S6.05); pex overlays them.
  - **Output:** psp writes a static SVG and a CSV; pex writes interactive HTML.
  - **Tools:** psp needs Graphviz; pex needs Python and pyvis, and has its own parser (S6.02).
  - **Use:** psp is called by no batch file; pex by 24.
- **Problems:** A reader comparing the two sees different explanations of one solution. The two codebases each carry a copy of the dump reader and the polarity rule.
- **Proposed call:** fix. Have one explanation renderer in 2.0 that offers psp's path filter (S6.04) and edge-source table (S6.08) as options of pex's view. How it is built is for decision 0022's design.
- **Decision:** fix, as proposed: one explanation renderer, with psp's path filter (S6.04) and edge-source table (S6.08) as options of pex's view and a static export (S6.07); how it is built is for decision 0022's design (prompt log entry 081)

## Open questions

1. **Does `plot_po.sh` run on macOS?** Line 39 calls `mktemp --suffix=.dat`. As far as I know that option is GNU-only, and BSD `mktemp` may reject it, which under `set -e` would stop the script before gnuplot runs. The macOS copy is byte-identical, and no macOS batch calls it (the model example uses inline gnuplot). Running `mktemp --suffix=.dat` once on the user's Mac would settle it.
   *No longer needed:* S6.22 replaces `plot_po.sh` with a built-in plot step, so whether this script runs on macOS no longer affects 2.0 (prompt log entry 080).
2. **Does psp's SVG step work on a real dump?** `runs/` holds no psp output, and this stage replayed psp's logic without running Graphviz. Running psp once on the full JUN run with `dot` on `PATH` would settle it.
   *Still open, for phase 3.*
3. **Are witness-less PO points (S6.02) still produced after the 2026-11-09 fix to Z3 result labelling?** The 12 found here come from older runs. Stage 5's review of the witness loop (`main.cc:4998-5058`) settles it.
   *Answered:* yes. `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3`, without the FSC, point (7,0), finished on 2026-11-11 with the binary rebuilt after the fix. It ends in `timeout` with no witness (`run/temp_wo_solutions.txt:1-2`), and pex drew nothing for it (`run_console.log:612`).
4. **Should relaxed `not_present` edges count as relaxed?** They make up 667 of the edge lines in the relaxation files dated 2026-11-10 or earlier, because relaxation depends only on the endpoint states (`GraphManagerNew.cc:28552-28595`). Stage 5's review of that constraint settles what "e edges relaxed" is meant to count, and so what S6.15 should draw.
   *Resolved by stage 5:* yes. Every considered edge between present nodes counts, selected or not (S5.13), so S6.15 draws them and shows that they are outside the witness (prompt log entry 078).
5. **Is the explanation every present element of the witness?** S6.06 assumes it is, honoured inhibitions and inactive nodes included. The user's confirmation, together with stage 5's definition of "present", settles it.
   *Resolved:* yes. The explanation is every present node and edge of the witness, honoured inhibitions and inactive nodes included (S6.06; prompt log entry 076).
