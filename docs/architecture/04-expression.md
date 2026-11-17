# 04 · Expression

## Scope

This section designs the `expression` module (Choice C1). It covers:

- the one reader of per-gene values, used by every command that reads them (S3.03 to S3.05, S3.08, S3.14, S7.04), and how values from any data modality plug in ([decision 0020](../decisions/0020-plug-and-play-inputs.md));
- classification into the up and down lists, `cudf` (S3.01, S3.02, S3.06 to S3.11), with the Not DE override (S8.15);
- the `Observations` value that the merge, the corridor step and the solve receive (D1);
- the report of observed genes that the network or the corridor leaves out, in two groups (S3.12, S4.16);
- threshold suggestion as a C++ command, with its histogram (S3.15 to S3.19, F1);
- the backend side of the frontend's live preview (S8.14) and of "accept a suggestion" (S8.06).

It leaves to other sections:

- 01: the batch syntax; the command table, where the handlers of `cudf` and `suggest_thresholds` live and bind their arguments (01.C9); the check before the run; `fusion --query`'s command line; how a reported result skips the rest of a point (01.C15);
- 02: gene ids, the id rule and `IdMap` (02.C3), the label maps and the `labels` query (2B.6), the node lookup (S2.03), `Network`, `GraphView` and `NodeKey`;
- 03: the exception set built from the observations and the confirmed lists (S1.17, S2.13, S8.22, 03.C2);
- 05: the corridor, and `fb_rch`, which matches the observations and writes the match report (05.C9);
- 06: how the solve turns observations into node sets (S5.07), the counted observations (S5.12) and the FSC's activity (S5.17);
- 07: the explanation export, which uses the values the solve used (07.C12);
- 08: GSEA's ranking of the values (S7.04, S7.05);
- 09: the run directory and its step directories, input copies, `run.json`, events, the settings registry, saved steps, `OutputFile` and the clash check (09.C2, 09.C3, 2A.6, 2A.7);
- 10: the preview screens, the stored overrides and batch generation;
- 11: fast_float (11.C5), compiler flags, gnuplot and its lookup, and core's process layer, beside which the gnuplot runner sits (2B.13); 12: fixtures.

Names used below:

- "the EGF file" is `runs/egf_hela/lfc/egf_hela_t120_lfc.tsv`. All 50 files of that name under `runs/`, this one included, are byte-identical.
- "the PHLDA1 copy" is `runs/egfr_b10_gsea_fdr044_candidates/r7_epha2_curated/inputs/egf_hela_t120_lfc_plus_phlda1.tsv`: the EGF file with `hsa22822<TAB>4.03` appended (S3.03).
- "the TNFα file" is `runs/real_experiments/final_tnfa_p38i_combined.tsv`, and "the treated TNFα file" is `auxilliary/IITB_ACTREC_old_data/IITB_ACTREC_old_data/final_tnfa_treated.tsv`.
- "the model example" is `macOS/model_example/inputs/egf_hela_t120_lfc.tsv`, a 115-line file.
- These five are the distinct real two-column value files: 6,372 lines in all.
- "the probe table" is `runs/egf_hela/lfc/HeLa_log2FC_all_timepoints.csv`: the user's microarray log2 fold changes, one row per probe and one column per time point. `runs/egf_hela/scripts/compute_lfc.py` wrote it and the EGF file from the same intensities, `runs/egf_hela/HeLa_normalized_exponentiated_gene_expression.csv` (`compute_lfc.py:26`, `112-113`).
- "the symbol map" is FuSION 1's, the reference run's `inputs/hsa_to_gene_symbol_map.txt` (6334 lines). 2.0 ships a map rebuilt from NCBI Gene (prompt log entry 205), so counts made with it may differ slightly.
- "the reference run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`.
- Paths under `runs/`, `auxilliary/` and `macOS/` are relative to `/home/ks-149-u24/Desktop/arjun/FuSION/`. FuSION 1 code is cited at the frozen snapshot ([decision 0024](../decisions/0024-frozen-fusion1-snapshot.md)).
- Counts come from Python scripts that read the files. FuSION 1 was not run. "The census" is the 114 batch files under `runs/` and `macOS/model_example/` that run `cudf`, as of 2026-11-12.

## Requirements carried in

Decisions:

- [0020](../decisions/0020-plug-and-play-inputs.md): per-gene values from any data modality must be plug and play. The mechanism is Phase 2's.
- [0021](../decisions/0021-required-and-optional-inputs.md): inputs are checked before the first step. A named input that is missing or unreadable stops the run. An optional input that is given but empty counts as an empty list. The frontend validates too.
- [0017](../decisions/0017-batch-file-compatibility.md), [0018](../decisions/0018-batch-interface.md), [0023](../decisions/0023-results-may-change-where-improved.md): FuSION 1's positional `cudf` line still runs. Arguments can be named. Results may change where 2.0 improves, and each such difference is recorded for Phase 3.
- [0019](../decisions/0019-partial-runs.md): the up and down lists are a saved step that a later run can start from.
- [0013](../decisions/0013-suggest-thresholds-in-scope.md): threshold suggestion is in scope.

Skeleton choices: A1 (the frontend runs `fusion` as a child process for each run; read-only calls that make no run directory are outside it, cross-section item 4A.2); A2 (`events.jsonl`); B1 (the library reports through the logger and event interface, never the console); C1 (all of this is `expression`); D1 (values become observations in memory; step outputs can be saved and loaded); E1 (settings and input hashes in `run.json`; JSON results; TSV tables with a header); F1 (threshold suggestion is a C++ command; gnuplot stays; no Python); G1 (C++20, nlohmann/json); G3 (Linux, macOS and Windows, native).

Stage 3:

- S3.01: `cudf <lfc_file> <up_threshold> <down_threshold> <up_out> <down_out>`, long name `create_up_down_file`, also by name. A wrong argument count is an error before any file is touched.
- S3.02: a threshold is accepted only when the whole token reads as a finite number; otherwise the error names the argument.
- S3.03: two whitespace-separated columns, extra columns ignored, LF or CRLF line ends, ids verbatim. Values are used as given, with no log transform and no scale check. Stage 3's open question 2 is resolved: the TNFα file holds log fold changes (prompt log entry 055).
- S3.04: a line whose value is not a finite number is skipped and reported by line number.
- S3.05: a line with an id but no value is skipped and reported by line number. Blank lines are skipped silently. No value is ever carried over.
- S3.06: up when value ≥ up threshold, down when value ≤ down threshold, the two tests independent and inclusive, values and thresholds as doubles.
- S3.07: up ≤ down is an error before anything is written.
- S3.08: repeated ids are reported, each gene is written at most once per list, and lines that put one id in different classes are an error. The rule binds every reader.
- S3.09: the list format (one id per line, LF, input order, no header; an empty list is a 0-byte file), written on every `cudf`, and a saveable step.
- S3.10: check the input and both outputs before truncating anything, stop on failure, and refuse outputs that coincide with each other or with the input.
- S3.11: print the count per list and the skipped and repeated lines.
- S3.12: the exact id-to-node lookup stays. Unmatched genes are listed in two groups: absent from the merged network, and in the network but outside the corridor.
- S3.13: the list format both list readers rely on stays. The exception set is built in memory (S1.17).
- S3.14: one parser for every command that reads the file.
- S3.15: the advisory report stays. Its form is decided here, and F1 has made it a C++ command.
- S3.16: suggestion reads by the same parser, so its counts are the ones `cudf` writes.
- S3.17: the seven criteria, plus ±k·SD about 0 for k = 2 and 3.
- S3.18: counts are over the file's genes, not the network's. With S3.08 and S3.16 they are distinct genes, as `cudf` writes them.
- S3.19: the histogram stays. A requested plot that is not written is a failure. Where plots are drawn is decided here.

Other stages:

- S1.05 (quoted tokens), S1.12 (one command table supplies the names and the help), S1.26 (outputs under the run's directory), S1.27 (console numbers with enough digits to be exact), S1.28 (the batch stops at the first failure, with a non-zero exit), S1.29 (result-changing settings are run settings; debug outputs sit behind one option).
- S1.17, S2.13, S8.22: the backend builds the exception set from `cudf`'s lists, the source, the target, the FSC and the not-merged file; 03.C2 adds the confirmed lists.
- S2.03 (report the node an id binds to and its members), S2.14 (id mapping across sources is designed in Phase 2 and may reuse S1.16's two-column reader).
- S4.16 (write the observed genes outside the corridor, with the corridor's parameters), S4.18 and S7.03 (empty results are reported results, so sweeps carry on).
- S5.05 (the relaxed-nodes input is dropped), S5.07 (the solve's set arithmetic with the confirmed lists and avoided nodes; the unused first read is dropped), S5.12 (the solve keeps the relaxation rule, in which a node in both the up and the down set is treated as up, and writes the counted and excluded observations), S5.17 (the FSC's activity follows its observation).
- S6.12 (the explanation export reuses the run's one LFC reader), S7.04 (`gsea` ranks the values `cudf` classifies; the reader reports non-finite values).
- S8.06 (no default thresholds: the user sets them or accepts a suggestion), S8.14 (the preview uses the backend's parser and shows exactly what `cudf` will produce), S8.15 (override files in the run directory; every kind the solve accepts is passed to it; Not DE removes the gene from the observed up and down lists), S8.30 (checks before a run).

Answers from the architecture polls (prompt log entries 158 to 210) that bind this section beyond its own choices:

- 1.1 and 09.C14 (entries 158, 186): 09's vocabulary. A failure is a `core::Failure` exception; a reported result is a `core::Report{code, message}` inside `core::Outcome<T>`; a stop is `core::Stopped`. Steps report through `core::Log` and `core::Events`, receive a `core::Context`, and use `core::Sha256` and `core::stable_compare`.
- 1.2 and 11.C6 (entries 158, 201): one static library per module. `expression` links `core` and `network`, and `merge` links `expression`. The `cudf` and `suggest_thresholds` handlers live in `batch`, the matcher takes a `network::GraphView`, and the gnuplot runner and the plot palette sit in `core` (2B.13).
- 1.3 (entry 190): dotted event names under the key `type`. The envelope is `v`, `seq`, `t` and `type`, and `core::Events` adds `step`, `line` and `point` inside a step. Warnings go only through `warning`, and reported results only through `step.finished`'s `report`.
- 1.4 and 09.C2 (entry 177): one directory per step, named by its command: `cudf/`, `suggest_thresholds/`, `fb_rch/`. Debug files go only in `debug/<step>/`.
- 1.12 with 02.C3 (entry 160): one `IdMap` type and file layout, 02's. A value input's `id_map=` names an S1.16 file or the keyword `symbols`, and replaces the run-wide map for that input.
- 1.13 and 09.C3 (entry 184): every input is copied into `inputs/` and hashed first; the check parses the copy, and the step uses that parse.
- 1.16 (entry 190): setting keys are `<module>.<name>`; on a command line the bare name is the argument.
- 1.17 (entry 158): generic conditions take 09's and 01's codes (`input.*`, `io.write`, `artefact.*`, `output.clash`, `batch.*`, `deps.missing`). This module keeps codes only for its own faults.
- 1.20 (entry 180): tables write numbers with `core::format_number`, the shortest exact digits in Python's repr layout; JSON is as nlohmann/json writes it.
- 1.27 (entry 158): gnuplot 5.4 or later with `pngcairo`, looked for in `FUSION_GNUPLOT`, then the bundle, then `PATH`.
- 2A.6 and 2A.7 (entry 158): every write goes through 09's `OutputFile`. 09's check refuses output clashes by file identity, as this section designed it.
- 2B.1 (entry 158): 02 supplies `NodeKey` and `NodeKind`, and its `Network` and `LabelMap`.
- 2B.3 and 4A.11 (entry 169): a relative or omitted output path lands in the step directory. An explicit absolute path is honoured as well, with the file also kept in the step directory and both recorded in `run.json`.
- 3A.13 with 03.C2 (entry 165): the confirmed up and down genes join the exception set (amends S8.22). The frontend offers no Relaxed override (entry 165).
- 01.C13 (entry 189): a file written earlier in the run is reused from memory when its identity and SHA-256 match.
- 01.C15 and 4A.6 (entry 167): a reported result skips every later line of its run or sweep point.
- 01.C16 (entry 189): an argument that takes observations defaults to the latest `cudf` result.
- 05.C9 (entry 172): `fb_rch` matches the observations and writes `observations_match.tsv`.
- 06.C15 (entry 167): no DE gene in the corridor is a case status of the solve, not a failure.
- 07.C12 and 4A.13 (entry 168): the explanation export uses the values the solve used. `pex` reads a value file through this reader only for a positional FuSION 1 line that names one (1.23, entry 158).
- 09.C16 (entry 186): no build contracts `a*b + c` into a fused multiply-add, and outputs are byte-identical across platforms.
- 11.C5 (entry 167): fast_float parses every decimal.
- 4A.7 (entry 168): numbers a user may copy into a batch are printed exactly (S1.27); S7.17's GSEA table keeps three decimals, as the one recorded exception.

## Design

### Components

| Component | What it does | Spec |
|---|---|---|
| value reader | one parser for per-gene values: layouts, line rules, ids, repeats, report | S3.03–S3.05, S3.08, S3.14, 0020 |
| classifier | `cudf`'s work: thresholds, up, down or none, Not DE, the saved lists | S3.01, S3.02, S3.06–S3.11, S8.15 |
| observations | the typed value the merge, corridor step and solve take | D1, S5.07, S5.17 |
| matcher | observed genes against the network and the corridor | S3.12, S4.16, S2.03 |
| suggester | `suggest_thresholds`'s work: statistics, ten rows, histogram | S3.15–S3.19 |
| query answers | the JSON that `fusion --query` returns for the preview and the suggestion | S8.06, S8.14 |

The handlers that bind the batch arguments of `cudf` and `suggest_thresholds` live in `batch` (01.C9). They turn bound arguments into this module's typed requests, call the functions below, and write through 09's `Files` (1.2).

### The value reader

One function, `read_values`, reads per-gene values for every consumer: `cudf`, `suggest_thresholds`, `gsea` (section 08), a positional `pex` line that names a value file (1.23), and the frontend's queries. Otherwise the explanation export uses the values the solve used (07.C12). FuSION 1 had five parsers with different rules: `cudf` (`Linux/src/main.cc:2677-2689`), `suggest_thresholds.py:67-81`, `gsea_select_pathways.py:86-96`, `plot_explanation.py:132-143` and the GUI's preview (`Linux/frontend/src/ThresholdFilterPanel.java:496-520`). In the reference run three of them parsed the same file (S3.14).

**When it reads** (04.C9, 09.C3). A value input is a file plus its reading arguments. Section 09 copies each input file into `inputs/` and hashes it. The check before the run (0021) then parses the copy, once per distinct value input, and every step that names the input uses that parse (1.13). So every failure of the reader is found before the first step. The check-only mode (09.C12) and the query mode (04.C8) read the original file instead.

**Bytes.**
- The file is read whole, in binary mode, and its SHA-256 is taken from the same bytes, so `cudf.json` names exactly what was parsed. In a run, that is the hash 09 recorded for the copy. The largest real value file is the probe table, 3,076,532 bytes; the EGF file is 41,571.
- A UTF-8 byte-order mark at the start is dropped. A UTF-16 byte-order mark, or any NUL byte, is the failure `values.encoding`, whose message names the cause (04.C10).
- Ids are byte strings, compared exactly. Bytes that are not valid UTF-8 stay as they are, and are replaced only when written to JSON.

**Lines.** LF and CRLF end a line (S3.03), and so does a lone CR (04.C10). The last line needs no terminator; the two TNFα files have none. Lines are numbered from 1.

**Layouts** (04.C1):
- *Pairs*, the default, is FuSION 1's format (S3.03). There is no header. Fields are separated by runs of spaces and tabs. Field 1 is the id and field 2 the value; later fields are ignored. All five real two-column files have exactly two tab-separated fields on every line.
- *Table* is chosen by giving `value_column`. The first non-blank line is a header. `id_column` (default: the first column) and `value_column` pick columns by exact header name. The separator is a tab if the header has one, else a comma if it has one, else runs of spaces. With tabs or commas a field may be in double quotes, `""` inside quotes is one quote, and quotes never span lines. Spaces around an unquoted field are trimmed.

**Line rules**, applied in this order in both layouts:
1. A blank line, or one of spaces and tabs only, is skipped silently (S3.05).
2. A missing or empty id field skips the line as `missing_id`.
3. A missing or empty value field skips it as `missing_value` (S3.05).
4. An unterminated quote skips it as `bad_quote` (table layout only).
5. The value must match, as a whole token, the decimal grammar `[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?` (04.C2): an optional sign, then digits with an optional point and fraction, or a point and digits, then an optional exponent. Otherwise the line is skipped as `not_a_number`. `NA`, `nan`, `inf`, `-`, `1,5`, `1.5abc`, `1_000`, `0x1p0` and a header word all land here (S3.04).
6. fast_float (11.C5) converts a matching token to the correctly rounded double, the same on every platform (S3.06). A value whose magnitude overflows a double is `not_finite`. A nonzero value below the smallest normal double (2.2250738585072014e-308) reads as 0, whatever the library returns, and the reader tells overflow from underflow by the sign of the token's decimal exponent.
7. Otherwise the line gives an id, a value and a line number.

Every skipped line except a blank one is reported with its number, its reason and its first 60 bytes. Each line is parsed into fresh fields, so FuSION 1's carry-over of an earlier line's id or value cannot happen (S3.05). In the table layout, a header that lacks `id_column` or `value_column`, or has one of them twice, is the failure `values.column`. A file in which no line gives a value is the failure `values.empty`: the wrong file, the wrong layout, or a map that matches none of its ids.

**Ids** (04.C1, 02.C3). Every id passes section 02's id rule once, at read time: every `:` is deleted, then a map is applied once, with no chaining (02.C3). So `hsa:3725` in a value file now matches `hsa3725`; S3.03 left id mapping to Phase 2 (decision 0020).
- By default the map is the run's: S1.16's file, given with `start` or by name. An id it lacks passes through unchanged, as for every other input.
- A value input may name its own map. `id_map=<file>` is an S1.16 file read by 02's reader (`<foreign id> <network id>`, a later line winning). `id_map=symbols` is the run's symbol map read backwards (symbol → id). Either replaces the run's map for that input (1.12). An input that names its own map is keyed by foreign ids, so an id the map lacks is dropped: counted, listed once, and reported as `values.unmapped_ids` (prompt log entry 229). The reader asks 02's `IdMap::lookup` for each id, which returns the mapped id, or nothing when the map lacks it. That is not an error. Most genes of a genome-wide table are outside the KEGG maps, as are the symbols of 15,552 of the probe table's 22,283 rows.
- The map is one-to-one. In the symbol map, 8 of 6,326 symbols name two ids, such as `ICOS` (`hsa102723996`, then `hsa29851`); the later line wins, and 02's reader reports them. Several foreign ids may still map to one network id, which then repeats.
- An empty `id_map` file is an empty map (0021). Every id is then dropped, and the read ends in `values.empty`, whose message names the map.

**Repeated ids** (04.C3). An id that occurs on more than one line, after the id rule, is kept once, at its first line's position, with every line's value and number. The report lists each repeated id with its lines and values. One value per id, the *representative*, is the one with the largest magnitude; on a tie the earlier line wins. It is what GSEA ranks, what the suggestion's statistics and counts use, and what the preview shows. Classification uses every line (S3.08), unless `repeats=max_abs` reduces each id to its representative first.

The two TNFα files have the same three repeated ids. In the TNFα file they are `hsa57018` on lines 10 and 11 (0 twice), `hsa3280` on 34 and 35 (−1.48 twice) and `hsa5806` on 61, 62 and 66 (9.82, 9.82, 1.95). Its 74 lines give 70 genes, and the representative of `hsa5806` is 9.82; in the treated TNFα file it is 11.61. The other three real files have no repeats.

**Data structures.** `GeneValues` is immutable. Per gene, in first-occurrence order, it stores the interned id (sections 02 and 09), the representative and an offset. Per line it stores the line number and the value, in two flat parallel arrays grouped by gene; a gene's offset and the next gene's offset bound its lines in both arrays. An index from interned id to position makes lookup O(1); if ids are interned as consecutive integers, the index is a plain vector. For the EGF file this is 3054 × 16 bytes per gene plus 3054 × 12 bytes per line, about 86 KB. The file buffer is freed after parsing.

**The probe table, read as 04.C1 proposes,** with `id_column=Gene_Symbol value_column=HeLa.EGF.120 id_map=symbols repeats=max_abs`:
- It gives 3,050 of the EGF file's 3,054 ids, every value equal after rounding to two decimals, and the same 43 up and 2 down genes at ±1.01.
- The four ids it lacks are the earlier ids of `ICOS`, `CSNK1E`, `ATG4B` and `PDE6G` (`hsa102723996`, `hsa1454`, `hsa23205`, `hsa5148`). `compute_lfc.py` gave the symbol's value to both of its ids (`compute_lfc.py:90-91`); 02's later-line rule keeps one.
- Its `values` object, whose fields every JSON output of this section shares:

```json
{"path": "inputs/HeLa_log2FC_all_timepoints.csv", "bytes": 3076532,
 "sha256": "ea334b21cf6662bf9e7c11a53e0115e03ee35cfbbd6af32440c2db0a001aeed8",
 "layout": "table", "separator": "comma", "id_column": "Gene_Symbol", "value_column": "HeLa.EGF.120",
 "id_map": "symbols", "repeats": "max_abs"}
```

- Its `read` object counts 22,284 lines: 1 header, 1,227 skipped as `missing_id` (rows without a symbol), and 21,056 parsed. Of those, 15,552 are unmapped (9,989 distinct symbols), and the other 5,504 rows give 3,050 genes, 1,357 of them repeated.

### Classification (`cudf`)

`cudf` takes the run's already-read values (04.C9), classifies them and saves the two lists. FuSION 1's handler is `Linux/src/main.cc:2629-2699`.

1. Thresholds go through the same decimal grammar as values (04.C2), and the command table's rule `less(down, up)` enforces S3.07 (01.C9).
2. Not DE (04.C4): each id in the `not_de` list is set to none, whatever its lines say. The report lists the ids this changed, those already none, and those not in the values (a warning). An empty `not_de` file is an empty list (0021).
3. Every line of every other gene is classified: up if v ≥ up, down if v ≤ down, otherwise none (S3.06). Since up > down, the three classes are disjoint.
4. A repeated id whose lines fall in different classes, none included, is the failure `observations.conflict`. The message names the id and its lines and values (S3.08). With `repeats=max_abs` each id has one value and cannot conflict.
5. Each list holds each id once, in first-occurrence order (S3.08, S3.09). It is written with LF only, through 09's `OutputFile`, which writes a temporary file and renames it, so an interrupted run never leaves a half-written list (S3.10, 2A.6).

The check before the run performs steps 1 to 4 (they take milliseconds), so bad thresholds, conflicts and an empty result are known before the first step. Step 5 runs with the step.

**Outputs.**
- In the positional form, `up_out` and `down_out` are required (S3.01). In the named form they may be left out (04.C11).
- Where the lists land follows 09's rule (2B.3). A relative or omitted name lands in `cudf/`, as `up_reg` and `down_reg` by default. An absolute path, as FuSION 1 batches give (`$WRITE_DIR/up_reg`), is written as well, with the file also kept in `cudf/` under its own name and both recorded in `run.json`. The reference batch's line 153 then still `cat`s the lists from `$WRITE_DIR`.
- Section 09's check refuses an output that coincides with another output or with an input, with the code `output.clash` (S3.10, 2A.7). It works by file identity, as this section designed it:
  - outputs against inputs and outputs that already exist, by `std::filesystem::equivalent`, which is exact on every file system, case-insensitive mounts and Unicode spellings on APFS included;
  - two outputs that do not exist yet, by their `weakly_canonical` paths, ignoring case on Windows and macOS, and again by identity just before the second is written.

  So no existing file is ever truncated by a clash.

Real results:
- The EGF file at ±1.01 gives 43 up and 2 down genes, byte-identical to the reference run's `run/up_reg` (342 bytes, starting `hsa1026`, `hsa1390`, `hsa1545`) and `run/down_reg` (`hsa1942`, `hsa3399`; 16 bytes). At ±0.67 it gives 77 and 13.
- The TNFα file at ±1.45 gives 33 up and 6 down genes. FuSION 1 wrote 35 and 7 lines, repeats included (S3.06, S3.08). This is an expected difference under 0023.

Console, through `core::Log` (S3.11). The EGF file:

```
cudf: inputs/egf_hela_t120_lfc.tsv: 3054 lines, 3054 genes, 0 skipped, 0 repeated
cudf: up (>= 1.01) 43 genes, down (<= -1.01) 2 genes -> cudf/up_reg, cudf/down_reg
```

The TNFα file:

```
cudf: inputs/final_tnfa_p38i_combined.tsv: 74 lines, 70 genes, 0 skipped, 3 repeated: hsa57018 (lines 10, 11), hsa3280 (34, 35), hsa5806 (61, 62, 66)
cudf: up (>= 1.45) 33 genes, down (<= -1.45) 6 genes -> cudf/up_reg, cudf/down_reg
```

The console names at most ten skipped lines or repeated ids; `cudf.json` holds all of them. The help line for `cudf`, from the command table (S1.12), states the rule: up when value ≥ up, down when value ≤ down, both inclusive, up above down.

### Observations

`Observations` is the typed value that D1 passes from `cudf` to later steps:
- `up()` and `down()`: the ids, each once, in input order;
- `observed()`: up then down. Section 03 adds these to the exception set, with the confirmed up and down genes (03.C2), the source, target, FSC and not-merged ids (S1.17, S2.13, S8.22);
- `regulation(id)`: up, down or none. The solve uses it for the FSC's activity (S5.17);
- the thresholds and the ids Not DE removed, when known.

It holds network ids, not nodes. The solve resolves them in the corridor and applies S5.07's arithmetic with the confirmed lists and the avoided FSC (section 06). `fb_rch` and the solve take the latest `cudf` result unless an argument names another (01.C16). Values are not copied in; a consumer that needs them takes `GeneValues`.

FuSION 1 batches pass pathz3 the two list files (S5.01, tokens 10 and 11). When those are the files `cudf` wrote earlier in the same run, unchanged, the interpreter passes the in-memory value (01.C13). Otherwise `load_lists` reads them by S3.12's rules (whitespace-separated ids, `\r` as whitespace, repeats dropped) and returns `Observations` without thresholds. An id in both files is up, as S5.12 treats a node in both sets, and the warning `lists.overlap` names it; lists that `cudf` wrote never overlap (S3.07).

### The match report (S3.12, S4.16)

`match_observations` takes the observations and the corridor as a `network::GraphView` (1.2). It puts each observed id in one of three groups:
- `not_in_network`: the view's network has no node for it (section 02's exact lookup);
- `outside_corridor`: it has a node, and the view does not contain it;
- `in_corridor`.

It also records the `network::NodeKey` and member count of the node the id binds to, so a binding to a family node shows (S2.03). `fb_rch` calls it with the corridor's view (05.C9), writes `fb_rch/observations_match.tsv` through `save_match`, and prints the three counts; the corridor's parameters are in its `corridor.json`. Called with a view of the whole network, it gives the first group alone.

The reference run, recomputed from its merged graph and lists: of 45 observed genes, 14 are not in the network (13 up, among them FOSB, CREM and EGR1, and ID3 down), 28 are outside the corridor (27 up, and EFNA1 down), and 3 are in it (CXCL8, JUN and IRS2). For the full run the counts are 1, 24 and 20 (prompt log entry 172). FuSION 1 printed only `Up: 3 Down: 0`. The corridor is not saved in FuSION 1, so the three in-corridor genes come from the run's `Up: 3` line, its solutions file and `diff_expr_nodes.txt`.

### Threshold suggestion (`suggest_thresholds`)

`suggest_thresholds` reads the values with the one reader (S3.16) and works on the representatives, one per gene. These are the genes `cudf` would write (S3.08, S3.18): the TNFα file counts 70, not FuSION 1's 74.

Statistics, over the n representatives sorted in ascending order:
- min and max;
- mean = Σv / n and the population SD = √(Σ(v − mean)² / n), both summed in ascending order in double;
- the median (the mean of the two middle values when n is even) and MAD = median of |v − median|, shown with 1.4826 · MAD;
- the tail ratio, when neither extreme is 0 (S3.15).

Rows, in this order:
- `current`, when `current_up` and `current_down` are given;
- `quantile 1%`, `quantile 2.5%`, `quantile 5%`: up = Q(1 − p) and down = Q(p), where Q interpolates linearly at position p · (n − 1), as `Linux/supporting_files/suggest_thresholds.py:84-91` does;
- `MAD k=2`, `MAD k=3`: median ± k · 1.4826 · MAD;
- `SD k=2`, `SD k=3`: mean ± k · SD;
- `SD about 0 k=2`, `SD about 0 k=3`: ±k · SD, the rule the batches use (S3.17; the reference batch's lines 67-68 derive ±1.01 as 3 · SD).

Each row gives `n_up` = #{v ≥ up} and `n_down` = #{v ≤ down}, by binary search on the sorted representatives, and `conflicts` (04.C6): the repeated ids whose lines straddle one of the row's cuts, on which `cudf` would stop (S3.08). With `repeats=max_abs` it is always 0.

Every number of the console report is printed in the shortest form that reads back to the same double (S1.27), `%DE` included: these are numbers a user may copy into a batch (4A.7). So a row copied into `cudf` gives exactly the row's counts. For example, `0.6667499999999973` below is the double that the 2.5 % interpolation yields; the exact quantile is 0.66675. The report keeps S3.15's four parts: the file and gene count, the distribution, the table, and the three explanatory lines.

The histogram (04.C7) has 80 equal bins over [min, max], as in S3.19. If every value is equal, there is one bin and the x range is [v − 0.5, v + 0.5], so gnuplot never gets FuSION 1's empty range. The bins always go into `thresholds.json`. With `plot`, core's gnuplot runner (2B.13) draws the PNG in S3.19's style (`suggest_thresholds.py:149-220`):
- `pngcairo`, 980 × 620, on `#fcfcfb`, in the plot palette that `core` holds for this plot and the Pareto plot;
- the candidate cuts as dotted lines, merged after rounding to four decimals;
- the current cuts as solid lines, blue `#2a78d6` and red `#e34948`, labelled `up +1.01` and `down -1.01`;
- empty bins left out, since the y axis is logarithmic.

The script carries the data inline, as a data block, so no temporary file is made. The runner needs gnuplot 5.4 or later with `pngcairo` (1.27). It checks gnuplot's exit status and that the PNG was written, and returns that result without raising anything. When a requested plot was not written, the step fails with `expression.gnuplot_failed` (S3.19), as section 07's `plot_pareto` fails with `results.gnuplot_failed` (prompt log entry 230).

### The preview and suggestion queries

The frontend asks for the preview (S8.14) and the suggestion table (S8.06) through `fusion --query` (04.C8). It writes one batch line to standard input and reads one JSON answer from standard output. The line is exactly the `cudf` or `suggest_thresholds` line the frontend will put in the batch, so the preview uses the same arguments as the run.

- The query makes no run directory, writes no files and emits no events. Its log lines go to standard error, because standard output carries the answer.
- `cudf` and `suggest_thresholds` are marked in 01's command table as able to run without a run directory. In query mode their handlers compute and answer, and save nothing.
- A1 governs how runs are executed; read-only calls that make no run directory are outside it (4A.2).
- The Not DE list is a file, as in the batch: the session's copy in section 10's data directory.

Request, with FOSB (1.04) in the session's Not DE file (`<data dir>` is section 10's data directory):

```
cudf values=/home/u/fusion/egf_hela_t120_lfc.tsv up=1.01 down=-1.01 not_de=<data dir>/sessions/7f3c/overrides/not_de.txt
```

The answer has `cudf.json`'s fields, plus `ok`, `query` and `genes`, one record per gene in file order. In the example, `…` marks fields shown in full in `cudf.json` below, and the rest of `genes`:

```json
{"ok": true, "query": "cudf",
 "values": {"path": "/home/u/fusion/egf_hela_t120_lfc.tsv", "layout": "pairs", …},
 "read": {"lines": 3054, "header": 0, "blank": 0, "parsed": 3054, "genes": 3054, …},
 "thresholds": {"up": 1.01, "down": -1.01},
 "not_de": {"path": "<data dir>/sessions/7f3c/overrides/not_de.txt", "removed": ["hsa2354"], "unchanged": [], "unknown": []},
 "n_up": 42, "n_down": 2, "n_none": 3010,
 "genes": [{"id": "hsa1000", "value": 0.12, "class": "none"},
           {"id": "hsa10000", "value": -0.18, "class": "none"},
           {"id": "hsa2354", "value": 1.04, "class": "none", "not_de": true},
           {"id": "hsa3725", "value": 2.41, "class": "up"}, …]}
```

- Symbols are not in the answer. The frontend adds them from section 02's `labels` query, which it already runs for gene lookups (2B.6, 10.C15); they change no class.
- An error answers `{"ok": false, "query": "cudf", "error": {"code": "observations.conflict", "message": "…"}}` with exit status 1, so the frontend shows, before the run, the message the run would print (0021, S8.30).
- A `suggest_thresholds` query answers with `thresholds.json`'s content.

Measured on this machine (Linux, x86-64, 36 cores, load average about 20 during the runs): starting a process costs 0.9 to 1.3 ms (three timings of 200 runs of `/bin/true`). An awk stand-in that parses the EGF file, classifies it and prints the answer's counts and `genes` list (139 KB of compact JSON) takes 18 ms per call, process start included. Windows was not measured. Starting a process costs more there, and the frontend debounces typing (section 10).

### Batch commands

The handlers are `batch`'s (01.C9); the argument names are this section's.

`cudf`, long name `create_up_down_file` (S3.01):

| Position | Name | Type | Default | Notes |
|---|---|---|---|---|
| 1 | `values` | path | required | the per-gene value file (S3.03) |
| 2 | `up` | number | required | 04.C2's grammar (S3.02) |
| 3 | `down` | number | required | rule `less(down, up)` (S3.07) |
| 4 | `up_out` | output path | required positionally; `up_reg` in `cudf/` when named arguments leave it out (04.C11) | 2B.3 |
| 5 | `down_out` | output path | as `up_out`, with `down_reg` | 2B.3 |
| | `id_column` | string | the first column | table layout (04.C1) |
| | `value_column` | string | none: pairs layout | 04.C1 |
| | `id_map` | path, or `symbols` | none: the run's map | 04.C1, 1.12 |
| | `repeats` | `classify_each`, `max_abs` | `classify_each` | 04.C3 |
| | `not_de` | path, one id per line | none | S8.15, 04.C4 |

The positional form takes exactly the five arguments (S3.01). All 114 census batches use the same line, `cudf $LOG_FOLD_CHANGE_FILE $UP_REG_THRESH $DOWN_REG_THRESH $UP_REG_FILE $DOWN_REG_FILE`, with ±1.01 (73), ±0.67 (22) or ±1.45 (19). Section 10's generated batches write `cudf values=$LFC up=$UP down=$DOWN not_de=$FUSION_BATCH_DIR/overrides/not_de.txt`.

`suggest_thresholds`, short name `sth` (04.C6):

| Position | Name | Type | Default | Notes |
|---|---|---|---|---|
| 1 | `values` | path | required | as for `cudf` |
| | `current_up`, `current_down` | number | none | both or neither; the `current` row |
| | `plot` | output path | none: no plot | the PNG (2B.3); needs gnuplot, checked before the run (11, `deps.missing`) |
| | `id_column`, `value_column`, `id_map`, `repeats` | | | as for `cudf` |

### Interface outline

```cpp
// From other sections (names as their outlines give them):
//   core::Context, Files, OutputFile, Failure, Report, Outcome<T>, Sha256, Interner, GeneId, Json (09)
//   core gnuplot runner and plot palette (2B.13, beside 11's process layer)
//   network::IdMap, Maps, LabelMap, Network, GraphView, NodeKey (02)

namespace fusion::core {
// The decimal grammar of 04.C2, converted by fast_float (11.C5). In core, because the batch's
// real arguments (01) and edge scores (02) use it too (3A.19).
enum class DecimalStatus : std::uint8_t { ok, not_a_number, not_finite };
struct Decimal { DecimalStatus status; double value; };
Decimal parse_decimal(std::string_view token) noexcept;
}

namespace fusion::expression {

// ---------- the one reader (S3.14) ----------
enum class RepeatRule : std::uint8_t { classify_each, max_abs };              // 04.C3

struct ValueInput {                                       // a file and its reading arguments (04.C1, 04.C9)
    std::filesystem::path path;                           // as named; 09 supplies the checked copy
    std::optional<std::string> id_column;                 // table layout; default: the first column
    std::optional<std::string> value_column;              // given => table layout
    std::optional<std::string> id_map;                    // an S1.16 file, or "symbols" (1.12)
    RepeatRule repeats = RepeatRule::classify_each;
    bool operator==(const ValueInput&) const = default;
};

enum class SkipReason : std::uint8_t { missing_id, missing_value, bad_quote, not_a_number, not_finite };
struct SkippedLine { std::uint32_t line; SkipReason reason; std::string text; };   // first 60 bytes
struct RepeatedId  { core::GeneId id; std::vector<std::uint32_t> lines; std::vector<double> values; };

struct ValueReport {                                      // the "read" object of every JSON output
    std::uint64_t bytes = 0;
    core::Sha256 sha256;                                  // of exactly the bytes parsed
    std::uint32_t lines = 0, header = 0, blank = 0, parsed = 0;
    std::vector<SkippedLine> skipped;                     // file order
    std::vector<RepeatedId> repeated;                     // first-occurrence order
    std::uint32_t unmapped_lines = 0;
    std::vector<std::string> unmapped_ids;                // distinct, first-occurrence order
};

class GeneValues {                                        // immutable, one entry per gene
public:
    std::size_t size() const noexcept;
    core::GeneId id(std::size_t i) const noexcept;        // first-occurrence order
    double value(std::size_t i) const noexcept;           // the representative (04.C3)
    std::span<const double> line_values(std::size_t i) const noexcept;         // file order
    std::span<const std::uint32_t> line_numbers(std::size_t i) const noexcept; // same order
    std::optional<std::size_t> find(core::GeneId) const noexcept;              // O(1)
    const ValueInput& input() const noexcept;
    const ValueReport& report() const noexcept;
};

// Reads one value input: 09's checked copy in a run, the original in --check and --query.
// The map is the input's own (id_map=; ids IdMap::lookup lacks are dropped) or else the run's (02.C3).
// Throws core::Failure: values.encoding, values.empty, values.column.
GeneValues read_values(const std::filesystem::path& file, const ValueInput&,
                       const network::Maps& run_maps, core::Interner&, core::Context&);
GeneValues parse_values(std::string_view bytes, const ValueInput&, const network::IdMap& map,
                        bool own_map, core::Interner&, core::Context&);

class ValueInputs {                                       // one per run, held by batch (04.C9)
public:
    const GeneValues& read(const ValueInput&, const std::filesystem::path& file,
                           const network::Maps&, core::Interner&, core::Context&);  // at the check; once each
    const GeneValues& at(const ValueInput&) const;                                  // by the steps
};

// ---------- classification: cudf ----------
enum class Regulation : std::int8_t { down = -1, none = 0, up = 1 };

struct Thresholds { double up; double down; };            // up > down: the table's rule (S3.07)
Regulation regulation_of(double value, Thresholds) noexcept;              // >= up, <= down (S3.06)

class Observations {                                      // network ids, not nodes
public:
    std::span<const core::GeneId> up() const noexcept;    // each id once, input order
    std::span<const core::GeneId> down() const noexcept;
    std::span<const core::GeneId> observed() const noexcept;               // up, then down
    Regulation regulation(core::GeneId) const noexcept;   // none when not observed
    bool empty() const noexcept;
    std::optional<Thresholds> thresholds() const noexcept;                // none when loaded from lists
    std::span<const core::GeneId> not_de_removed() const noexcept;
};

struct ClassifyRequest {
    Thresholds thresholds;
    std::span<const core::GeneId> not_de;                 // applied before classification (04.C4)
};
// Report "observations.none" when nothing passes (04.C5); throws core::Failure observations.conflict.
core::Outcome<Observations> classify(const GeneValues&, const ClassifyRequest&, core::Context&);

// ---------- the saved step (S3.09, decision 0019) ----------
struct ListNames { std::string up = "up_reg"; std::string down = "down_reg"; };  // as the batch gave them (2B.3)
void save_observations(const Observations&, const GeneValues&, const ListNames&,
                       core::Context&);                   // the lists and cudf.json, through Files
std::vector<core::GeneId> read_id_list(const std::filesystem::path&, const network::IdMap&,
                                       core::Interner&);  // S3.12's rules
Observations load_lists(const std::filesystem::path& up, const std::filesystem::path& down,
                        const network::IdMap&, core::Interner&, core::Context&);   // an id in both is up (S5.12)

// ---------- observed genes against the network and the corridor (S3.12, S4.16) ----------
enum class MatchGroup : std::uint8_t { not_in_network, outside_corridor, in_corridor };
struct ObservationMatch {
    core::GeneId id;
    Regulation regulation;
    MatchGroup group;
    std::optional<network::NodeKey> node;                 // the node the id binds to (S2.03)
    std::uint32_t members = 0;                            // that node's member ids
};
std::vector<ObservationMatch> match_observations(const Observations&, const network::GraphView& corridor);
void save_match(std::span<const ObservationMatch>, const GeneValues*, const network::LabelMap& symbols,
                core::Context&);                          // fb_rch/observations_match.tsv (05.C9)

// ---------- threshold suggestion (S3.15 to S3.19) ----------
inline constexpr double mad_to_sd = 1.4826;
inline constexpr std::uint32_t histogram_bins = 80;      // S3.19
struct Distribution { std::size_t n; double min, max, mean, sd, median, mad; };   // population sd, raw mad
struct CandidateRow { std::string criterion; double up, down; std::size_t n_up, n_down, conflicts; };
struct Histogram { double lo; double width; std::vector<std::uint32_t> counts; };
struct Suggestion { Distribution distribution; std::vector<CandidateRow> rows; Histogram histogram; };

Suggestion suggest_thresholds(const GeneValues&, std::optional<Thresholds> current);
void save_suggestion(const Suggestion&, const GeneValues&, core::Context&);   // thresholds.tsv, thresholds.json
std::string histogram_script(const Suggestion&, std::optional<Thresholds> current);  // for core's runner,
                                                          // which returns a result; the step raises
                                                          // expression.gnuplot_failed (S3.19)

// ---------- answers for fusion --query (04.C8) ----------
core::Json cudf_answer(const GeneValues&, const Observations&);           // writes nothing
core::Json suggestion_answer(const Suggestion&, const GeneValues&);

}  // namespace fusion::expression
```

### Algorithms and cost

B is the file's bytes, L its lines, G its genes after the id rule and repeats, R the suggestion's rows (at most 10), k the observed genes.

| Operation | Time | Memory | Measured size |
|---|---|---|---|
| read and hash | O(B) | O(B), freed after parsing | EGF file 41,571 B; probe table 3,076,532 B |
| parse lines | O(L) | O(L) | 3,054 lines; 22,284 lines |
| id rule | O(L) hash lookups | O(G) | 21,056 parsed rows of the probe table give 3,050 genes |
| repeats and representatives | O(L) | none extra | EGF 0 repeated; probe table 1,357 genes with 2 to 11 rows |
| classify and Not DE | O(L + \|not_de\|) | O(G) | |
| save lists | O(up + down) | none | 358 bytes for the reference run |
| suggest | O(G log G) for the two sorts (values, deviations), O(R log G) for counts, O(L · R) for conflicts, O(G) for the histogram | O(G) | |
| match | O(k) lookups | O(k) | 45 observed in the reference run |
| preview query | read + classify + O(G) JSON | O(G) | 139 KB of JSON for the EGF file |

### Files written

In the step directory that 09.C2 gives each call:

| File | Written by | When |
|---|---|---|
| `cudf/up_reg`, `cudf/down_reg` | `cudf` | always (S3.09), under the names the batch gives; an absolute path gets the file too (2B.3) |
| `cudf/cudf.json` | `cudf` | always: the read and classification report, and the saved step's metadata |
| `suggest_thresholds/thresholds.tsv` | `suggest_thresholds` | always: the candidate rows |
| `suggest_thresholds/thresholds.json` | `suggest_thresholds` | always: distribution, rows, histogram |
| `suggest_thresholds/<plot>` | `suggest_thresholds` | with `plot`, under the name it gives |
| `debug/cudf/values.tsv`, `debug/suggest_thresholds/values.tsv` | either command, through `create_debug` | under the debug option only (S1.29): the values as read |
| `debug/suggest_thresholds/thresholds.gp` | `suggest_thresholds` | under the debug option, with `plot`: the gnuplot script |
| `fb_rch/observations_match.tsv` | `fb_rch`, through `save_match` | when the run has observations (05.C9) |

Every JSON output of this section shares two objects:
- `values` describes the input: `path`, `bytes`, `sha256`, `layout` (`pairs` or `table`), `separator` (`whitespace` for pairs; `tab`, `comma` or `spaces` for tables), `id_column`, `value_column`, `id_map`, `repeats`;
- `read` reports the reading: `lines`, `header`, `blank`, `parsed`, `genes`, `skipped` (an array of `{line, reason, text}`), `repeated` (an array of `{id, lines, values}`) and `unmapped` (`{lines, ids}`).

Counts elsewhere are named `n_…`. Tables write numbers with `core::format_number` (1.20), and JSON as nlohmann/json writes it.

`cudf/up_reg` (the reference run's first three lines):

```
hsa1026
hsa1390
hsa1545
```

`cudf/cudf.json`, EGF file:

```json
{
  "command": "cudf",
  "values": {"path": "inputs/egf_hela_t120_lfc.tsv", "bytes": 41571,
             "sha256": "777689a645f48d766fb2f821bb93ae31d0a68d3ec6ab7767970163389f33432d",
             "layout": "pairs", "separator": "whitespace", "id_column": null, "value_column": null,
             "id_map": null, "repeats": "classify_each"},
  "read": {"lines": 3054, "header": 0, "blank": 0, "parsed": 3054, "genes": 3054,
           "skipped": [], "repeated": [], "unmapped": {"lines": 0, "ids": []}},
  "thresholds": {"up": 1.01, "down": -1.01},
  "not_de": {"path": null, "removed": [], "unchanged": [], "unknown": []},
  "n_up": 43, "n_down": 2, "n_none": 3009
}
```

For the TNFα file, `read.repeated` is:

```json
[{"id": "hsa57018", "lines": [10, 11], "values": [0.0, 0.0]},
 {"id": "hsa3280", "lines": [34, 35], "values": [-1.48, -1.48]},
 {"id": "hsa5806", "lines": [61, 62, 66], "values": [9.82, 9.82, 1.95]}]
```

A skipped header line in a pairs file would appear in `read.skipped` as `{"line": 1, "reason": "not_a_number", "text": "gene\tlfc"}`.

`suggest_thresholds/thresholds.tsv`, EGF file with `current_up=1.01 current_down=-1.01`:

```
criterion	up	down	n_up	n_down	n_de	conflicts
current	1.01	-1.01	43	2	45	0
quantile 1%	1.17	-0.47	33	34	67	0
quantile 2.5%	0.6667499999999973	-0.37	77	86	163	0
quantile 5%	0.41	-0.3	159	156	315	0
MAD k=2	0.355824	-0.355824	204	89	293	0
MAD k=3	0.533736	-0.533736	105	20	125	0
SD k=2	0.7098328861083146	-0.6357267957350342	69	13	82	0
SD k=3	1.046222806569152	-0.9721167161958715	39	2	41	0
SD about 0 k=2	0.6727798409216744	-0.6727798409216744	74	13	87	0
SD about 0 k=3	1.0091697613825117	-1.0091697613825117	43	2	45	0
```

The first eight rows match S3.17's recomputed table. `SD about 0 k=3` gives the 43 up and 2 down genes of the ±1.01 cut in use. In the TNFα file, `MAD k=2` and `MAD k=3` each show one conflict: `hsa5806` (9.82 and 1.95) straddles +3.81 and +5.39.

`suggest_thresholds/thresholds.json`, EGF file (`…` marks the `values` and `read` objects shown in `cudf.json`, the other nine rows and the other 72 bins):

```json
{"command": "suggest_thresholds",
 "values": {"path": "inputs/egf_hela_t120_lfc.tsv", …},
 "read": {"lines": 3054, …},
 "distribution": {"n": 3054, "min": -1.56, "max": 5.19, "mean": 0.03705304518664024,
                  "sd": 0.3363899204608372, "median": 0.0, "mad": 0.12, "mad_scaled": 0.177912,
                  "wider_tail": "up", "tail_ratio": 3.326923076923077},
 "rows": [{"criterion": "current", "up": 1.01, "down": -1.01, "n_up": 43, "n_down": 2, "n_de": 45, "conflicts": 0}, …],
 "histogram": {"lo": -1.56, "width": 0.084375, "counts": [1, 0, 1, 0, 0, 0, 0, 3, …]}}
```

`fb_rch/observations_match.tsv`, reference run (shortened; rows by group, up before down, then input order; `node` is 02's `NodeKey`, shown as the gene id for single-gene nodes):

```
id	symbol	regulation	value	group	node	members
hsa1390	CREM	up	1.52	not_in_network		0
hsa3399	ID3	down	-1.36	not_in_network		0
hsa1026	CDKN1A	up	1.26	outside_corridor	hsa1026	1
hsa1942	EFNA1	down	-1.56	outside_corridor	hsa1942	1
hsa3576	CXCL8	up	5.19	in_corridor	hsa3576	1
hsa3725	JUN	up	2.41	in_corridor	hsa3725	1
hsa8660	IRS2	up	1.39	in_corridor	hsa8660	1
```

`debug/cudf/values.tsv`, debug only, TNFα file (one row per gene; repeated ids list all their lines):

```
id	value	lines
hsa56999	1.69	1
hsa5806	9.82	61,62,66
```

### Settings

These change results. They are recorded in `run.json` with their value and source (E1), under the registry key `expression.<name>` (1.16); on a command line the bare name is the argument. The four reading arguments belong to the value input, so they are recorded once per input (04.C9). Output names (`up_out`, `down_out`, `plot`) are recorded with the batch line and the outputs, and change no result.

| Key | Type | Default | Used by |
|---|---|---|---|
| `expression.values` | input file | required | `cudf`, `suggest_thresholds` |
| `expression.id_column` | text | the first column | table layout |
| `expression.value_column` | text | none: pairs layout | |
| `expression.id_map` | input file, or `symbols` | none: the run's map | |
| `expression.repeats` | `classify_each`, `max_abs` | `classify_each` | |
| `expression.up`, `expression.down` | real | required, no default (S8.06) | `cudf` |
| `expression.not_de` | input file | none | `cudf` |
| `expression.current_up`, `expression.current_down` | real | none | `suggest_thresholds` |

`gsea` takes the same reading arguments under its own keys (08, 1.23).

### Events

Payload fields only; the envelope is 09's (`v`, `seq`, `t`, `type`, with `step`, `line` and `point` inside a step; 1.3).

| `type` | Emitted | Fields |
|---|---|---|
| `expression.read` | when the check reads a value input, with the step and line of the first command that names it | path, layout, lines, genes, n_skipped, n_repeated, n_unmapped |
| `expression.classified` | after `cudf` classifies | thresholds, n_up, n_down, n_not_de_removed |
| `expression.suggested` | after `suggest_thresholds` | genes, n_rows, plot |
| `expression.matched` | by `fb_rch`, after matching (05.C9) | n_observed, n_not_in_network, n_outside_corridor, n_in_corridor |
| `warning` | skipped lines, repeated ids, unmapped ids, a file read two ways, unknown Not DE ids, overlapping lists | code, message (09) |

The reported result `observations.none` is not an event of its own: it is `step.finished`'s `report` (1.3).

```
{"v":1,"seq":4,"t":"2026-11-12T13:14:00.512Z","type":"expression.read","step":"cudf","line":145,"path":"inputs/egf_hela_t120_lfc.tsv","layout":"pairs","lines":3054,"genes":3054,"n_skipped":0,"n_repeated":0,"n_unmapped":0}
{"v":1,"seq":21,"t":"2026-11-12T13:14:01.030Z","type":"expression.classified","step":"cudf","line":145,"thresholds":{"up":1.01,"down":-1.01},"n_up":43,"n_down":2,"n_not_de_removed":0}
{"v":1,"seq":5,"t":"2026-11-12T13:14:00.513Z","type":"warning","step":"cudf","line":145,"code":"values.repeated_ids","message":"3 repeated ids, each kept once: hsa57018 (lines 10, 11), hsa3280 (34, 35), hsa5806 (61, 62, 66)"}
{"v":1,"seq":22,"t":"2026-11-12T13:14:01.031Z","type":"step.finished","step":"cudf","line":145,"status":"reported","report":{"code":"observations.none","message":"no gene passes up >= 5.2 or down <= -5.2"}}
```

(The first two lines are the EGF reference run's; the warning comes from a TNFα batch, and the last line from a threshold sweep's point at ±5.2.)

### Errors and reported results

Failures stop the batch with a non-zero exit (S1.28). "Check" means the check before the run (0021), which reads every value input and classifies every `cudf`. Generic conditions take 09's and 01's codes (1.17); this module's own faults keep theirs.

| Code | Kind | Raised when | Example message |
|---|---|---|---|
| `batch.bad_argument` (01) | failure, at the check | the positional form does not have its five arguments, or an argument is unknown, duplicated or missing (S3.01; prompt log entry 230) | `cudf: expected 5 positional arguments, got 4 (batch line 145) [batch.bad_argument]` |
| `batch.bad_value` (01) | failure, at the check | a threshold is not, as a whole token, a number by 04.C2's grammar (S3.02), or the rule `less(down, up)` fails (S3.07) | `cudf: up threshold "1.0l" is not a number (batch line 145) [batch.bad_value]` |
| `input.missing`, `input.unreadable`, `input.not_file` | failure, at the check | the value file, the `id_map` file or the `not_de` file is missing or unreadable (0021) | `cudf: cannot read /home/u/x.tsv: No such file or directory [input.unreadable]` |
| `output.clash` | failure, at 09's check, or just before the second list | an output coincides with another output or with an input (S3.10, 2A.7) | `cudf: up_out and down_out are the same file /home/u/run/up_reg [output.clash]` |
| `io.write` | failure, at the step | a write or rename fails, including over a file another program holds open on Windows (09's retry rule, 1.28) | |
| `deps.missing` | failure, at the check | `plot` is given and no gnuplot 5.4 or later with `pngcairo` is found (11) | |
| `values.encoding` | failure, at the check | UTF-16, or a NUL byte (04.C10) | `cudf: x.xlsx is not a text file (NUL byte at offset 5); save it as CSV or TSV [values.encoding]` |
| `values.empty` | failure, at the check | no line gives a value | `cudf: no line of x.csv holds an id and a number; is it a table (value_column=…)? [values.empty]` |
| `values.column` | failure, at the check | the header lacks a named column, or has it twice | |
| `observations.conflict` | failure, at the check | a repeated id's lines fall in different classes (S3.08) | `cudf: hsa5806 is up on lines 61, 62 and none on line 66 (up >= 2, down <= -2) [observations.conflict]` |
| `expression.gnuplot_failed` | failure, raised by the step, at the step | core's runner (2B.13) reports that gnuplot did not write a requested plot (S3.19); the runner itself raises nothing (prompt log entry 230) | `suggest_thresholds: gnuplot exited with status 1 and wrote no thresholds.png: line 14: invalid expression [expression.gnuplot_failed]` |
| `observations.none` | reported result | no gene passes the thresholds (04.C5) | `cudf: no gene passes up >= 5.2 or down <= -5.2` |
| `values.skipped_lines`, `values.repeated_ids`, `values.unmapped_ids`, `values.read_two_ways`, `not_de.unknown_ids`, `lists.overlap` | warning | the run carries on | |

### Portability

- Files are read and written in binary mode. The reader handles CR itself, and the lists are LF on every platform (S3.09).
- Byte-order marks, UTF-16 and binary files are handled as 04.C10 has it.
- Paths are `std::filesystem::path` values built from UTF-8, so non-ASCII Windows paths work. Output clashes are found by file identity (2A.7), so case-insensitive and Unicode-normalising file systems are covered.
- Every write goes through 09's `OutputFile`, a temporary file and then a rename, with 09's retry rule for Windows, where a rename over a file another program holds open fails (1.28). 2.0 writes into a new run directory (S8.21), where the target does not exist yet; only an absolute path an old batch names can meet an open file.
- gnuplot is found by 11's lookup (`FUSION_GNUPLOT`, then the bundle, then `PATH`; 1.27). The PNG path goes into the script as a single-quoted gnuplot string, with any `'` doubled, because gnuplot processes backslash escapes in double-quoted strings and Windows paths contain backslashes. The plot font is 11's (11.C10).
- The query reads its batch line from standard input, so the line is not split a second time as a Windows command line, where paths with spaces or quotes go wrong. Standard output is set to binary mode on Windows.
- Decimals are parsed by fast_float (11.C5), locale-free and correctly rounded on all three platforms. Numbers are written by 09's `core::format_number` and by nlohmann/json.

### Determinism

- Parsing is correctly rounded, the same on every platform, with no locale. Underflow reads as 0 by the reader's own rule, not the library's.
- Lists, reports and JSON arrays follow first-occurrence file order. A repeated id's lines are in file order, and the match report is in a fixed group order.
- The representative's tie goes to the earlier line.
- Statistics are summed in ascending order in double. `long double` is never used: it is 80-bit on x86-64 Linux and macOS, 128-bit on Linux aarch64, and the same as `double` on Apple silicon and MSVC.
- No build contracts `a*b + c` into a fused multiply-add (09.C16), so the SD's last bits are the same on every machine, as the byte-identical promise needs.
- Numbers are written in the shortest form that reads back exactly, so one double prints as one text everywhere.
- The bytes hashed are the bytes parsed, and the steps use what the check parsed (1.13, 04.C9).

## Choices

### 04.C1 · Plug-and-play values: a header-table layout and a per-input id map

- **Proposed:** one reader for per-gene values from any modality, as decision 0020 asks, leaving the mechanism to Phase 2. S3.03's two-column format stays the default, unchanged; giving `value_column` reads a table with a header (tabs, commas or spaces, detected from the header; quoted fields), whose id and value columns are picked by name. This extends S3.03 with a table layout. Ids follow section 02's one id rule (02.C3): `:` deleted, then one `IdMap`, in 02's S1.16 layout (`<foreign id> <network id>`, a later line winning). A value input's `id_map=` names an S1.16 file or the keyword `symbols` (the run's symbol map read backwards), and replaces the run's map for that input; ids that map lacks are dropped and reported, while the run-wide map passes them through (prompt log entry 229). Values are used as given (S3.03). Accepted, with 02.C3 and cross-section item 1.12, in prompt log entry 160.
- **Alternatives:**
  - Per-input maps in this section's first layout, `<network id> <foreign id>`, one-to-many allowed, so an ambiguous symbol gives its value to every id it names (the draft's proposal).
  - Two-column files only, converted outside FuSION, as today. The user wrote `compute_lfc.py` for this.
  - A built-in reader per tool's output: DESeq2, edgeR, limma, MaxQuant and so on.
  - A separate `convert_values` command that writes a two-column file, which `cudf` and `gsea` then read.
  - Ids kept as given and translated at node lookup instead of at read time.
- **Why:**
  - Measured on the user's own data: the probe table (22,283 rows keyed by symbol, 8 columns), read with `id_column=Gene_Symbol value_column=HeLa.EGF.120 id_map=symbols repeats=max_abs`, gives 3,050 of the EGF file's 3,054 ids. Every value is equal after rounding to two decimals, and the up and down genes at ±1.01 are the same 43 and 2 (as sets; the lists follow the table's row order, while the EGF file is sorted by id). No script is needed.
  - The four ids it lacks are the earlier ids of `ICOS`, `CSNK1E`, `ATG4B` and `PDE6G`. The draft's one-to-many layout kept them, as `compute_lfc.py` does, and gave all 3,054; 02's later-line rule keeps one id per symbol, and one map type serves every input. These corrected numbers replace entry 160's "the same 3054 ids", and the answer stands (prompt log entry 230).
  - On the way, the read skips and reports the 1,227 rows without a symbol and drops the 15,552 rows whose symbol the map lacks.
  - At ±0.67 the table gives 74 up genes where the EGF file has 77: `hsa10365` (0.6675) and `hsa1849` (0.6686) reach 0.67 only through the file's rounding to two decimals, and `hsa1454` is CSNK1E's earlier id.
  - Every real two-column file reads unchanged under the default layout (5 distinct files, 6,372 lines).
  - Tool outputs differ in their columns, not in kind: DESeq2's `log2FoldChange`, edgeR's and limma's `logFC`, the probe table's `HeLa.EGF.120`. Choosing a column covers them all. A reader per tool is one more code path per tool to keep up.
  - A convert step adds a file and a step but no capability; the debug option's `values.tsv` already shows what was read.
  - Translating at read time puts the lists, GSEA's ranking and the exception set in network ids, which all three need (S3.09, S7.04, S1.17).
- **Decision:** as proposed (prompt log entry 160)

### 04.C2 · One decimal grammar for values, thresholds, real arguments and scores

- **Proposed:** a value or threshold counts only when the whole token matches `[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?`. A value that does not match is skipped and reported (S3.04); a threshold that does not match is an error (S3.02). The same grammar serves every real argument of the batch language and edge scores (3A.19), through `core::parse_decimal`, which converts with fast_float 8.3.3 (11.C5). This refines S3.02's "finite number", so `0x1p0` is refused, and settles S3.03's prefix reading; accepted in prompt log entry 167.
- **Alternatives:**
  - `cudf`'s longest numeric prefix for values, as S3.03 describes FuSION 1: `1,5` reads as 1, `1.5abc` as 1.5 and `1_000` as 1.
  - For thresholds, any spelling the C++ library accepts, as `stof` did: `0x1p0` reads as 1.
- **Why:**
  - No real input changes. All 6,372 value tokens of the 5 real two-column files, and all 22,283 values of the probe table's `HeLa.EGF.120` column, match. Every census threshold is ±0.67, ±1.01 or ±1.45.
  - The prefix rule turns a decimal comma into a wrong value without a message: `1,5`, which a spreadsheet in a European locale writes for 1.5, becomes 1. With plug-and-play inputs from other labs (0020), that is the likely mistake.
  - One grammar for all means that a threshold copied from a value, or from a suggestion row, always parses.
  - fast_float is the one locale-free, correctly rounded parser available on all three platforms; `std::from_chars` for doubles is missing from macOS's libc++ at the targeted version (11.C5).
- **Decision:** as proposed, refining S3.02 and settling S3.03's reading (prompt log entry 167)

### 04.C3 · Repeated ids: the largest magnitude as the one value, and an opt-in collapse

- **Proposed:**
  - By default, S3.08 as decided: every line is classified, and lines in different classes stop the run.
  - Where one value per gene is needed (GSEA's ranking, the suggestion and the preview), it is the value with the largest magnitude, the earlier line winning a tie. GSEA has no thresholds, so a batch that runs `gsea` without `cudf` only reports the repeats.
  - `repeats=max_abs` reduces each id to that value before classification, for tables with several rows per gene: probes, transcripts, phosphosites. The collapsed ids are reported. For the runs that use it, this amends S3.08: a gene's rows can no longer conflict. Accepted in prompt log entry 167.
- **Alternatives:**
  - The first line's value, the last line's (FuSION 1's `gsea` and `pex`, S3.14), or the mean.
  - No collapse mode: rows per gene are merged outside FuSION, and S3.08 applies everywhere.
- **Why:**
  - In the probe table, read by 04.C1, 1,357 of the 3,050 genes have several rows, up to 11. With S3.08's default, 21 genes have rows in different classes at ±1.01 (43 at ±0.67, 11 at ±1.45), so `cudf` would stop.
  - The user's own pipeline keeps the largest |LFC| per gene (`compute_lfc.py:92-94`), and with it the probe table gives the EGF file's values. The first row gives 38 up genes instead of 43, the mean 31, and the last row 28; the last-row rule drops JUN, the reference run's FSC.
  - Among the two-column files only the two TNFα files have repeats, the same three ids in each. `hsa5806` is 9.82 (11.61 in the treated file) by largest magnitude or first line, and 1.95 by last line. No batch passes either file to `gsea` or `pex` (S3.14).
  - One rule for both purposes means that GSEA ranks the value `cudf` classified whenever classification succeeds.
- **Decision:** as proposed, amending S3.08 for runs with repeats=max_abs (prompt log entry 167)

### 04.C4 · Not DE applies in `cudf`, before classification

- **Proposed:** `cudf` takes the Not DE list (`not_de`, one id per line; section 10 writes it as `overrides/not_de.txt`, S8.15) and sets those genes to none before their lines are classified, so S3.08's conflict check skips them. The lists on disk, the observations, the exception set and the preview then all leave them out. This narrows S3.08's stop to genes the user has not overridden; accepted, with 03.C2 (confirmed genes join the exception set, amending S8.22), in prompt log entry 165.
- **Alternatives:**
  - Apply Not DE after the conflict check, so a conflicting gene stops the run even when marked Not DE.
  - The solve applies Not DE in S5.07's set arithmetic, beside the confirmed lists, and `cudf`'s lists keep the genes.
- **Why:**
  - Not DE should make a gene behave exactly as if its value lay between the thresholds. Applied in `cudf`, it does: the gene also stays out of the exception set (S8.22), so it is merged like any other gene that is not differentially expressed.
  - Applied in the solve, the gene would still be split out of its family box as if observed, and the saved lists would disagree with what the solve used.
  - Applied first, an explicit override settles a gene whose lines disagree, as `hsa5806`'s do in the TNFα file at ±2.
  - The preview shows the override through the same function `cudf` runs (S8.14).
  - The other override kinds stay where S8.15 put them: confirmed up and down, active, inactive and non-relaxed go to the solve (section 06), and the confirmed lists also reach the exception set (03.C2). The frontend offers no Relaxed override, since S5.05 dropped the input it would feed (prompt log entry 165).
- **Decision:** as proposed, narrowing S3.08 (prompt log entry 165)

### 04.C5 · No gene past the thresholds is a reported result

- **Proposed:** when no gene is up or down, `cudf` still writes its two empty lists (S3.09) and succeeds, returning the `core::Report` `observations.none`. As for every reported result, every later line of that run or sweep point is skipped, and the run ends ok with exit status 0 (01.C15). The three reported results are `corridor.empty`, `gsea.empty_selection` and this one; outcomes inside a solve, no DE gene in the corridor included, are case statuses (06.C15). Accepted, with cross-section items 3A.14 and 1.8, in prompt log entry 167.
- **Alternatives:** a failure that stops the batch (S1.28), even in a threshold sweep.
- **Why:**
  - Strict thresholds can select nothing, as a strict FDR can (S7.03). The EGF file selects nothing for any up threshold above 5.19 together with a down threshold below −1.56. In a threshold sweep that point should not halt the others.
  - FuSION 1 never logged `Up: 0 Down: 0` (S3.10), and its encoding breaks on it (S5.09), so the case has to be explicit either way.
  - A file in which no line gives a value stays a failure (`values.empty`): that is a wrong input, not a result.
- **Decision:** as proposed (prompt log entry 167)

### 04.C6 · The form of threshold suggestion

- **Proposed:** F1 has made threshold suggestion a C++ command. It is a batch command, `suggest_thresholds` (short name `sth`), which also answers `fusion --query` (04.C8). It prints S3.15's report, writes `thresholds.tsv` and `thresholds.json`, and with `plot` draws the histogram. Its table gains a `conflicts` column: the repeated ids that straddle a row's cut, where `cudf` would stop (S3.08). A suggestion reaches `cudf` as numbers: the user, or the frontend's "use this row" (S8.06), copies a row's exact values into `up` and `down`, which stay numbers (S3.02). Numbers a user may copy into a batch are printed exactly (S1.27), and S7.17's GSEA table keeps three decimals as the one recorded exception (4A.7). This adds a column to S3.15's table; accepted in prompt log entry 168.
- **Alternatives:**
  - `cudf` also accepts a criterion, say `thresholds=sd0:3`, computes the numbers itself and records both in `run.json`.
  - The report keeps FuSION 1's fixed decimals, amending S1.27 for it.
  - The table without the `conflicts` column, as S3.15 printed it.
- **Why:**
  - The batch keeps explicit numbers, as all 114 census batches have them, and S3.02 makes a threshold a number.
  - Exact printing (S1.27) makes copying safe: the copied row gives the row's counts. The batches rounded 3 · SD, 1.0091697613825117, to 1.01 by hand. On the two-decimal EGF file that gives the same 43 and 2 genes, but it need not in general.
  - The criterion form would suit a sweep over several value files, such as time points, where each file gets its own cut. It is a new feature with its own checks, so it is offered as an alternative, not proposed.
  - The `conflicts` column shows which cuts would stop `cudf`. In the TNFα file, `MAD k=2` and `MAD k=3` have one each; in the other real files, every row has 0.
- **Decision:** as proposed, adding a column to S3.15 (prompt log entry 168)

### 04.C7 · The histogram: a gnuplot PNG on request, bins as data

- **Proposed:** the 80 bins of S3.19 always go into `thresholds.json`. The PNG is drawn only when `plot` is given, in S3.19's style, through the gnuplot runner and plot palette in `core` that the Pareto plot also uses (2B.13). A file whose values are all equal gets one bin over [v − 0.5, v + 0.5], so the plot is always written. A requested plot that gnuplot did not write fails with `expression.gnuplot_failed`, raised by the step (S3.19; prompt log entry 230). The frontend draws its own chart from the bins (Phase 5) and never waits for gnuplot. Accepted in prompt log entry 168.
- **Alternatives:**
  - C++ writes the histogram as SVG, with no gnuplot.
  - Only the frontend draws it, and batches get no image.
- **Why:**
  - F1 keeps gnuplot for the Pareto plot, at about 0.06 s per PNG (measured for F1). The histogram is one more plot of the same family and palette (S3.19).
  - An SVG writer would be a second plotting path to keep in the same look, which F1 declined for the Pareto plot.
  - Batch users keep the PNG that S3.19 kept, and the frontend gets data it can draw interactively.
  - FuSION 1's all-equal case gave gnuplot an empty x range and a 0-byte PNG (S3.19).
- **Decision:** as proposed (prompt log entry 168)

### 04.C8 · The preview: `fusion --query`, one batch line in and JSON out

- **Proposed:** for the preview and the suggestion, the frontend runs `fusion --query`. It sends, on standard input, the exact `cudf` or `suggest_thresholds` line it will put in the batch, and reads one JSON answer from standard output. The query runs the same library functions as the batch command, writes no files, makes no run directory and emits no events; its log lines go to standard error. Only commands marked as able to run without a run directory may run this way. Section 02's `labels` line serves the frontend's gene lookups through the same mode (2B.6, 10.C15). A1 is read as governing how runs are executed: read-only calls that make no run directory (`--check`, `--commands`, `--version`, `--stop`, `--query`) are outside it, so this choice does not amend A1. Accepted, with cross-section items 3A.8, 1.10 and 4A.2, in prompt log entry 168.
- **Alternatives:**
  - A1 read strictly: the frontend runs each preview as a one-line batch in a new run directory and reads `cudf.json`.
  - A JSON request on standard input instead of a batch line.
  - The frontend fetches the parsed values once and classifies them in Java as the user types.
  - One long-lived `fusion` process per frontend session, answering queries line by line.
- **Why:**
  - S8.14 asks for a preview that is exactly what `cudf` will produce. The same function on the same line makes it so by construction. Classifying in Java brings back a second implementation of S3.06 to S3.08 and of Not DE: the fifth-parser problem S8.14 fixed.
  - A run per preview leaves a run directory for every pause in typing, and run directories are never deleted or reused (S8.21). One session of threshold trials would leave dozens.
  - A batch line keeps the batch language the only input format; a JSON request would be a second one.
  - It is cheap. Starting a process costs about 1 ms here, and an awk stand-in that prints the proposed answer for the EGF file (139 KB) takes 18 ms per call, start included. A long-lived process would save milliseconds at the cost of a second protocol and a process to supervise.
  - On standard input, the line is not split again as a Windows command line, where paths with spaces or quotes go wrong.
- **Decision:** as proposed (prompt log entry 168)

### 04.C9 · Each value input is read once per run, keyed by its file and reading arguments

- **Proposed:** a value input is a file plus its reading arguments: `id_column`, `value_column`, `id_map` and `repeats`. Every command that reads per-gene values takes the same arguments: `cudf`, `suggest_thresholds`, `gsea`, and a positional `pex` line that names a value file (1.23). The check before the run parses 09's copy of each distinct input once (1.13), and every command that names it shares the result (D1). One file read with different arguments is two inputs. That is allowed, so two time points of one table can both be used, and the warning `values.read_two_ways` names the arguments that differ. The explanation export otherwise uses the values the solve used (07.C12, cross-section item 4A.13). Accepted in prompt log entry 168.
- **Alternatives:**
  - One set of arguments per file, with a mismatch stopping the run before it starts. This would forbid two columns of one table in one run (04.C1).
  - A separate `values` command declares each input once, and later commands refer to it by name.
  - Each command reads with its own arguments, with no warning.
- **Why:**
  - S3.14's fix is one parser with one set of rules for every reader, which holds by construction. What is left is an accidental difference in arguments, such as an `id_map` given to `cudf` and forgotten for `gsea`. The warning names it.
  - Deliberate differences exist. The r7 and r8 batches (2026-11-09) give `gsea` the original EGF file while `cudf` classifies the PHLDA1 copy. Keying on the file and its arguments keeps that, and two time points of the probe table, possible.
  - FuSION 1 batches pass the path alone to `cudf`, `gsea` and `pex` (reference batch lines 145, 169 and 269). That is one input, read once, where FuSION 1 parsed it three times.
  - Parsing at the check means that every failure of the reader is found before the first step (0021); section 08 expects `values.empty` there too. The bytes parsed are the bytes hashed into `run.json`, even if the original is edited during the run.
  - A `values` command adds syntax, and an ordering rule that old batches do not follow.
- **Decision:** as proposed (prompt log entry 168)

### 04.C10 · Encodings and line ends beyond S3.03

- **Proposed:** the reader also accepts a lone CR as a line end, and drops a UTF-8 byte-order mark at the start of the file. A UTF-16 byte-order mark or a NUL byte is the failure `values.encoding`, with a message naming the cause. This extends S3.03, whose format has LF or CRLF line ends, takes ids verbatim and treats a lone CR as whitespace; accepted in prompt log entry 169. Batch files follow the same UTF-8 rule (01.C19).
- **Alternatives:** S3.03 as written. LF or CRLF only, with a lone CR as whitespace, as in FuSION 1. A byte-order mark stays part of the first id. UTF-16 and binary files are read as text.
- **Why:**
  - No real file is affected. The five real two-column files have LF (3) or CRLF (2) line ends, and none has a byte-order mark or a NUL byte; nor does the probe table.
  - Windows and Mac tools produce all three:
    - Excel's "CSV UTF-8" starts with a byte-order mark;
    - its "Unicode Text" export is UTF-16;
    - an `.xlsx` file is a ZIP archive with a NUL as its sixth byte;
    - classic Mac text uses lone CRs.
  - As written, each of these fails without a message: a lone-CR file reads as one line and one gene, a byte-order mark makes the first id match nothing, and a UTF-16 file reads as garbage.
  - G3 makes Windows and macOS first-class, and 0020 asks for inputs from other labs' tools.
- **Decision:** as proposed, extending S3.03 (prompt log entry 169)

### 04.C11 · `cudf`'s outputs may be left out in the named form

- **Proposed:** in the named form, `up_out` and `down_out` may be left out. The lists are then `up_reg` and `down_reg` in `cudf/`, by 09's rule for an omitted output (2B.3). The positional form keeps its five required arguments. This extends S3.01, which has all five required; accepted in prompt log entry 169.
- **Alternatives:** both outputs required in the named form too, as S3.01 has them.
- **Why:**
  - 2.0 puts every output in the run's own directory (S1.26, S8.21). FuSION 1 batches name the list paths only to keep runs apart in the launch directory. For the same reason they move other outputs with `exec mv`: 8 of the reference batch's 34 `exec` lines (S1.26).
  - The frontend's generated batches (S8.20) then need no paths for the lists.
  - Explicit paths still work. A relative one lands in `cudf/`, and an absolute one is written as well, with a copy kept in `cudf/` (2B.3).
- **Decision:** as proposed, extending S3.01 (prompt log entry 169)

## Interfaces with other sections

- **01 Batch language:**
  - the `cudf`/`create_up_down_file` and `suggest_thresholds`/`sth` rows of the command table, with the argument names above; exactly five positionals for `cudf` (S3.01); the rule `less(down, up)` (S3.07, 01.C9); both commands marked as able to run in `--query`;
  - the handlers, in `batch`: they bind arguments, read the run's `ValueInputs`, call `classify` or `suggest_thresholds`, call `save_observations`, `save_suggestion` and core's gnuplot runner, raise `expression.gnuplot_failed` when the runner reports a requested plot unwritten, and return the `Outcome` (01.C9, 1.2);
  - the check before the run, which calls `ValueInputs::read` for each value input and `classify` for each `cudf` (04.C9);
  - `fusion --query`: one batch line on standard input, the JSON answer on standard output, the log on standard error (04.C8);
  - the skip of every later line after `observations.none` (01.C15); the latest-`cudf` default for observations (01.C16); in-memory reuse of the lists by identity and SHA-256 (01.C13);
  - the look-ahead (01.C17), which collects the confirmed lists for 03 (03.C2).
- **02 Network:**
  - `core::GeneId` interning and the id rule, `IdMap::canonical` (02.C3);
  - `IdMap::lookup`, which returns the mapped id or nothing, so that a value input's own map can drop and report the ids it lacks, while the run-wide map passes them through (04.C1, prompt log entry 229);
  - `IdMap::reverse_symbols` for `id_map=symbols`, built from the run's symbol map in `network::Maps`;
  - `LabelMap` for the match report's symbols; the `labels` query that gives the frontend its symbols (2B.6);
  - `Network`'s id lookup, `GraphView::has_node` and `NodeKey`, for the matcher (S2.03, S4.14, 2B.1).
- **03 Merge:** `Observations::observed()` gives the up and down genes for the exception set (S1.17, S2.13, S8.22). Not DE genes are not in it (04.C4); the confirmed genes are, from 01's look-ahead (03.C2).
- **05 Corridor:** `fb_rch` takes the latest observations (01.C16), calls `match_observations` with `corridor.view()` and `save_match`, and prints the three counts (05.C9). The corridor's parameters are in 05's `corridor.json`.
- **06 Solve:**
  - takes `Observations` (ids and `regulation`), resolves them in the corridor and applies S5.07 with the confirmed lists and the avoided FSC;
  - uses `regulation(fsc)` for S5.17;
  - uses `load_lists` for FuSION 1's positional list paths, unless 01.C13 passes the in-memory value; an id in both lists is up, as S5.12 has it;
  - no DE gene in the corridor is a case status (06.C15), not a reported result like 04.C5.
- **07 Results:** the explanation export uses the values the solve used (07.C12); a positional `pex` line that names a value file reads it through `read_values` (1.23). The histogram uses core's gnuplot runner and palette, shared with `plot_pareto` (2B.13).
- **08 Enrichment:**
  - `gsea` reads its values through `read_values`, with its own `expression::ValueInput` (1.23), and ranks `GeneValues`: one representative per gene, in network ids (04.C3);
  - tie order by gene id is 08's (S7.05);
  - 08's sums rely on the same build flags (09.C16).
- **09 Runs:**
  - copies and hashes each value file into `inputs/` before the check parses the copy (09.C3, 1.13);
  - the step directories `cudf/` and `suggest_thresholds/`, debug files through `create_debug` in `debug/cudf/` and `debug/suggest_thresholds/`, and `fb_rch/observations_match.tsv` (09.C2);
  - `Files` and `OutputFile` for every write; the rule for omitted, relative and absolute output paths (2B.3); the clash check by file identity (2A.7);
  - `cudf` as a saved step, with the lists and `cudf.json`, and the loader `load_lists`;
  - the settings above under `expression.<name>`; the event payloads above inside 09's envelope; `warning` events through `core::Log`; the `Report` code `observations.none`.
- **10 Frontend:**
  - calls `fusion --query` with the batch line it will submit, and debounces typing;
  - keeps the session's Not DE file for the preview, and writes `overrides/not_de.txt` and the `not_de` argument into the batch (S8.15);
  - adds symbols to the preview from 02's `labels` query (2B.6);
  - shows the backend's error messages before a run (S8.30);
  - takes thresholds from the user or a suggestion row, never from a default (S8.06).
- **11 Build:** fast_float 8.3.3 for `core::parse_decimal` (11.C5); the flags that keep floating point byte-identical (09.C16); gnuplot 5.4 or later with `pngcairo` and its lookup (1.27); the plot font (11.C10); `deps.missing` when `plot` is given and gnuplot is missing.
- **12 Tests:** the hooks below, and the expected differences under 0023:
  - the TNFα lists written once per gene: 33 and 6, against FuSION 1's 35 and 7 lines;
  - the float-to-double edge of S3.06;
  - tokens such as `1,5` skipped, and `0x1p0` refused as a threshold (04.C2);
  - `hsa:3725` in a value file now matching `hsa3725` (02.C3);
  - the suggestion's counts per gene: 70 genes, not 74 rows.

## Test hooks

- `parse_values` on in-memory text, one case per row of S3.14's table and per skip reason: a header, a blank line, `hsa9`, `NA`, `nan`, `inf`, `1,5`, `1_000`, `1.5abc`, `1.`, repeats. Also CRLF without a final newline (as in the TNFα file), a lone CR, a UTF-8 byte-order mark, UTF-16, a NUL byte, `1e400` (not finite), `1e-400` (read as 0) and `hsa:3725` (read as `hsa3725`).
- Classification:
  - SOX9 (`hsa6662`, 1.01) is up at 1.01;
  - the EGF file at ±1.01 gives byte-identical copies of the reference run's `run/up_reg` and `run/down_reg`, and 77 and 13 genes at ±0.67;
  - up ≤ down fails, as does the TNFα file at ±2 (on `hsa5806`), unless `hsa5806` is in `not_de`;
  - nothing past ±5.2 on the EGF file returns `observations.none`, and the rest of the point is skipped.
- Table layout, on the probe table with `id_map=symbols` and `max_abs`:
  - 3,050 of the EGF file's 3,054 ids, all four others being the earlier ids of ambiguous symbols, and its values after rounding to two decimals;
  - at ±1.01, the same 43 up and 2 down genes as sets, in the table's row order;
  - at ±0.67, 74 up genes, where the rounded EGF file has 77;
  - 1,227 rows skipped as `missing_id`, and 15,552 dropped as unmapped;
  - a header without the named column fails.
- Clashes (with 09): two spellings of one new output on a case-insensitive file system, and an output naming an input, are refused, and no existing file is truncated.
- Suggestion: the EGF table above, to the last digit; the TNFα conflicts; a file whose values are all equal still draws a plot.
- Preview: for random thresholds and Not DE lists, the query's classes equal the lists `cudf` writes from the same batch line. This is the property S8.14 asks for.
- Match: the reference run's 14, 28 and 3, and the full run's 1, 24 and 20.
- Determinism: lists, TSV and JSON byte-identical on Linux, macOS and Windows (09.C16).
- Performance: a benchmark records the read time of the probe table and the query time of the EGF file.

## Open questions

1. **The amendments this section proposed** are all decided: 04.C1 extends S3.03 with a table layout (entry 160); 04.C2 refines S3.02 and settles S3.03's prefix reading (entry 167); 04.C3's `max_abs` amends S3.08 for the runs that use it (entry 167); 04.C4 narrows S3.08 (entry 165); 04.C6 adds a column to S3.15 (entry 168); 04.C10 extends S3.03 and 04.C11 extends S3.01 (entry 169). 04.C8 no longer amends A1 (entry 168). *Closed.*
2. **S3.15 and S1.27 on the report's numbers.** *Closed by prompt log entry 168 (4A.7):* copyable numbers are exact, and S7.17's GSEA table keeps three decimals.
3. **A1 and the query mode.** *Closed by prompt log entry 168 (4A.2):* A1 covers runs only, and `fusion --query` takes one batch line.
4. **Should confirmed overrides join the exception set?** *Closed by prompt log entry 165:* yes (03.C2, amending S8.22).
5. **S3.08 for GSEA, which has no thresholds.** *Closed by prompt log entry 167:* GSEA ranks the representative (04.C3).
6. **The Relaxed override has no target.** *Closed by prompt log entry 165:* the frontend offers neither the Relaxed override nor the relaxed-edges file.
7. **Absolute output paths in FuSION 1 batches.** *Closed by prompt log entry 169 (2B.3):* honoured, with a copy kept in the step directory.
8. **Is S5.09's stop a failure or a reported result?** *Closed by prompt log entry 167:* no DE gene in the corridor is a case status (06.C15).
9. **Does an id that a value input's own map lacks pass through, or drop?** *Closed by prompt log entry 229:* as proposed, a value input's own map (`id_map=<file>` or `symbols`) drops the ids it lacks and reports them, and the run-wide map passes them through (02.C3); 02's `IdMap::lookup` says whether an id was mapped.
