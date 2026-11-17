# 03 · Expression data

## Scope

This stage covers `cudf`/`create_up_down_file` (`Linux/src/main.cc:2629-2699`), which turns a log-fold-change (LFC) file into an up-regulated and a down-regulated gene list, and `Linux/supporting_files/suggest_thresholds.py`, which reads the same LFC file and reports candidate thresholds; nothing in FuSION 1 calls the script ([decision 0013](../decisions/0013-suggest-thresholds-in-scope.md)). It specifies the LFC file as cudf reads it, the two thresholds, the classification rule, the two lists cudf writes, and how entries of those lists become graph nodes. cudf itself matches nothing against a graph: matching happens when later commands read its files, so this stage names those readers and the lookup they share, and leaves what the solve does with the matched nodes to stage 5. Stage 1 owns tokenising (S1.05), failure handling (S1.28), the interactive branch (S1.03, dropped) and the reader of the merge-exception file that every batch builds from cudf's output (S1.17); stage 2 owns the id lookup (S2.03) and what the exception set does to merging (S2.13). Stage 4's `fb_rch` reads neither list. Stage 5's `pathz3` reads both lists and receives the LFC path; stage 6's `pex` and stage 7's `gsea` read the LFC file with parsers of their own, compared in S3.14. Stage 8's GUI writes the cudf line (`Linux/frontend/src/gui4.java:2166`, `4142-4143`). The cudf block is identical in `macOS/src/main.cc:2630-2700`, and `suggest_thresholds.py`, `GraphManagerNew.cc` and `XMLParser.cc` are byte-identical between `Linux/` and `macOS/` (checked with `cmp`).

Names used below:
- "the real run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`, with its batch file, its `run_console.log` and its `run/` folder;
- "the EGF file" is that run's `inputs/egf_hela_t120_lfc.tsv` (HeLa, EGF, t = 120); all 25 copies of that name under `runs/` are identical (same md5);
- "the TNFα file" is `runs/real_experiments/final_tnfa_p38i_combined.tsv`, read by 19 batch files;
- "the standalone copy" is cudf's loop and batch-mode argument handling copied verbatim into a small program, compiled with g++ 13.3 and `-std=c++0x` as in `Linux/Makefile:10`. It reproduces the real run's `run/up_reg` and `run/down_reg` byte for byte, and the TNFα lists in `runs/real_experiments/experiment7.1/`, `experiment5.0/`, `experiment_1_old/` and `experiment_2_old/`;
- `<TAB>` stands for a tab character.

FuSION 1 was not built or run. Counts come from that copy and from Python scripts that parse the files. FuSION 1's working tree changed while this stage was drafted: on 2026-11-09 `main.cc` and `GraphManagerNew.cc` gained a z3 timeout-labelling change, and nine batch files were added under `runs/egfr_b10_gsea_fdr044_candidates/`. Every location cited here was re-checked against the tree as it now stands, and the cudf block is unchanged from `HEAD`.

## Items

### S3.01 · `cudf` / `create_up_down_file`: the command and its arguments

- **FuSION 1:** `Linux/src/main.cc:2629-2699` (dispatch and handler), `main.cc:898` (help line), tokens from `main.cc:1151-1181` (S1.05); real batch line 145
- **What it does:** `cudf <lfc_file> <up_threshold> <down_threshold> <up_out> <down_out>`. `create_up_down_file` is the long name. In batch mode the five values are tokens 1 to 5 of the line (`main.cc:2643-2647`). The thresholds go through `stof` (S3.02) before any file is opened. The handler then opens the LFC file for reading and both outputs for writing, classifies every line (S3.06) and writes the two lists (S3.09). It never touches the graph manager, so it works before `start`, which is where all 87 batch files run it (the real batch runs cudf on line 145 and `start` on line 160). The real line after expansion (`run_console.log:6`) is `cudf /home/…/inputs/egf_hela_t120_lfc.tsv 1.01 -1.01 /home/…/run/up_reg /home/…/run/down_reg`. All 87 `*.batch` files under `runs/` and `macOS/model_example/`, and the three GUI scripts, carry the same line, `cudf $LOG_FOLD_CHANGE_FILE $UP_REG_THRESH $DOWN_REG_THRESH $UP_REG_FILE $DOWN_REG_FILE`. The batch files set ±1.01 (46 files), ±0.67 (22) or ±1.45 (19). The interactive branch (`main.cc:2633-2640`) reads the same five values from `cin` without prompts; S1.03 drops that mode.
- **Problems:**
  - The argument count is never checked. Tokens after the fifth are ignored. A missing threshold reads as `""` and aborts the whole run (S3.02). A missing output path reads as `""`, which fails to open only after the other output has been truncated (S3.10).
  - The help line lists `cudf` but not the long name (S1.12, already decided).
- **Proposed call:** fix. Keep the five positional arguments and both names, and report a wrong argument count as an error before any file is touched.
- **Decision:** fix as proposed; the arguments can also be given by name ([decision 0018](../decisions/0018-batch-interface.md)) (prompt log entry 049) *Phase 2 (prompt log entry 169):* extended by 04.C11: in the named form, `up_out` and `down_out` may be left out, and the lists then go to the step directory.

### S3.02 · Threshold parsing

- **FuSION 1:** `Linux/src/main.cc:2632` (`float up_threshold, down_threshold`), `main.cc:2644-2645` (`stof`); no `try` around dispatch (S1.28)
- **What it does:** Each threshold is parsed with `std::stof` into a 32-bit float. `stof` reads the longest prefix that is a number and ignores the rest, so `1.01x` reads as 1.01 and `0x1p0` as 1.0. It accepts `nan`, `inf` and `-inf`. A token with no numeric prefix, such as `abc` or the `""` of a missing argument, throws `std::invalid_argument`. A value outside float range, such as `1e40`, throws `std::out_of_range`. Nothing catches either, so the run aborts with status 134 (S1.28) before cudf opens any file. Checked with the standalone copy: `1.01x` gives a normal up list; `nan` and `inf` give an empty up list; `abc`, `""` and `1e40` print `terminate called after throwing an instance of 'std::invalid_argument'` (or `'std::out_of_range'`) and exit 134.
- **Problems:**
  - A typo that starts with a number is accepted silently: `1.0l` reads as 1.0.
  - `nan` makes every comparison false, so the list comes out empty with no message. `inf` as the up threshold (or `-inf` as the down threshold) empties that list, and the opposite sign puts every gene in it.
  - One malformed value kills the whole batch, not just this command.
- **Proposed call:** fix. Accept a threshold only when the whole token is a finite number, and otherwise fail with an error that names the argument, as S1.28 decides for failures.
- **Decision:** fix, as proposed (prompt log entry 049) *Phase 2 (prompt log entry 167):* refined by 04.C2: a threshold must match one decimal grammar, parsed with fast_float, so `0x1p0` is refused.

### S3.03 · The LFC input file

- **FuSION 1:** `Linux/src/main.cc:2650` (open), `main.cc:2671-2679` (read loop); `Linux/src/XMLParser.cc:881-888` (KGML ids lose their colons)
- **What it does:** The file is plain text, read line by line with `getline`. A last line without a final newline is still read. From each line cudf takes the first whitespace-delimited token as the gene id and the second as the value (`stringstream(s) >> gene_name >> fold_change`):
  - spaces and tabs both separate, leading whitespace is skipped, and anything after the second token is ignored;
  - the `\r` of a CRLF line is whitespace, so it never reaches the id or the value;
  - the id is copied verbatim, with no change to case, colons or prefixes;
  - the value is read by `operator>>(float&)` in the C locale (FuSION never changes locale): an optional sign, `.` as the decimal point, an optional exponent, and only the longest numeric prefix of the token, so `1.5abc` reads as 1.5 and `1,5` as 1;
  - values are used as given, with no log transform and no range or scale check;
  - there is no header, comment or missing-value convention (S3.04, S3.05).

  Real lines (EGF file, lines 1-2): `hsa1000<TAB>0.12` and `hsa10000<TAB>-0.18`. The EGF file has 3054 lines with LF endings, exactly two tab-separated fields per line, values with two decimals (190 distinct values, from −1.56 to +5.19), no repeated id, and every id of the form `hsa<digits>`. The TNFα file has 74 lines with CRLF endings and no final newline, values from −2.53 to +94.57, and 70 distinct ids (S3.08). Users also edit LFC files by hand: `runs/egfr_b10_gsea_fdr044_candidates/r7_epha2_curated/inputs/egf_hela_t120_lfc_plus_phlda1.tsv` (2026-11-09) is the EGF file with `hsa22822<TAB>4.03` appended. `macOS/model_example/inputs/egf_hela_t120_lfc.tsv` is a different, 115-line file under the same name.
- **Problems:**
  - The id column is tied to KEGG ([decision 0020](../decisions/0020-plug-and-play-inputs.md)). Later steps match it exactly against the network's ids (S3.12), which in FuSION 1 are KEGG gene ids with the colon removed (`hsa3725`). The KGML reader strips colons from KGML names (`XMLParser.cc:883-886`), but nothing strips them from the LFC file. `hsa:3725`, the symbol `JUN` and the bare Entrez id `3725` all pass through cudf and then match nothing, with no message.
- **Proposed call:** keep. Port the two-column, whitespace-separated format with extra columns ignored and LF or CRLF line ends; how ids from other sources map onto network ids is the plug-and-play question that decision 0020 leaves to Phase 2.
- **Decision:** keep (prompt log entry 049) *Phase 2 (prompt log entries 167 and 169):* settled and extended: a value must match 04.C2's grammar as a whole token, with no longest-prefix reading, and 04.C10 accepts a lone CR and a UTF-8 byte-order mark and refuses UTF-16 and NUL bytes.

### S3.04 · Bug: a value that is not a number counts as 0

- **FuSION 1:** `Linux/src/main.cc:2679`; `Linux/Makefile:10` (`-std=c++0x`)
- **What it does:** When the second token is present but has no numeric prefix, the float extraction fails and, under C++11, stores 0. Header lines, missing-value markers and non-finite values therefore all read as an LFC of 0, with no message: `gene<TAB>lfc`, `hsa1<TAB>NA`, `hsa2<TAB>nan`, `hsa3<TAB>inf` and `hsa4<TAB>-`. This confirms the note at `suggest_thresholds.py:12-14`.
- **Problems:**
  - With the real thresholds (±0.67, ±1.01, ±1.45) a 0 lands in neither list, so the error stays invisible. With a threshold that admits 0, the line becomes a gene. At an up threshold of 0 the header `gene<TAB>lfc` writes `gene` to the up list (checked with the standalone copy). With both thresholds at 0.0, as in the GUI-written `Linux/output_script.txt:31-32`, every `NA` gene lands in both lists.
  - A measured 0 and an unreadable value cannot be told apart.
  - The Python readers take `nan` and `inf` as NaN and infinity, so they disagree with cudf on these lines (S3.14).
  - No real LFC file contains such a line.
- **Proposed call:** fix. Skip and report, with its line number, any line whose value is not a finite number, so a header or missing value can never be classified; well-formed files give the same lists as now ([decision 0017](../decisions/0017-batch-file-compatibility.md)).
- **Decision:** fix, as proposed (prompt log entry 049)

### S3.05 · Bug: a line without a value reuses the previous line's gene or value

- **FuSION 1:** `Linux/src/main.cc:2671-2672` (`gene_name` and `fold_change` are declared once, outside the loop), `main.cc:2679`
- **What it does:** When a line runs out before a token is read, that extraction does nothing, and the variable keeps what the previous line put in it:
  - a blank or whitespace-only line, including a lone `\r` in a CRLF file, leaves both the gene and the value unchanged, so the previous gene is classified again and, if it passed a threshold, written a second time;
  - a line with an id but no value, such as `hsa9`, or `hsa3:2.0` with no whitespace, gives the new id the previous line's value;
  - a blank first line classifies an empty id with an uninitialised value, which is undefined behaviour.

  Checked with the standalone copy at ±1: the lines `hsa1<TAB>2.0`, blank, `hsa2<TAB>0.1` write `hsa1` to the up list twice; the lines `hsa1<TAB>2.0`, `hsa9`, `hsa2<TAB>0.1` write `hsa1` and `hsa9`.
- **Problems:** A gene with no measurement can be reported as up- or down-regulated, and a blank line duplicates an entry, all without a message. No real LFC file has a blank or one-field line: every line of the EGF, TNFα, model-example and PHLDA1 files has exactly two fields.
- **Proposed call:** fix. Skip and report any line that lacks an id or a value, and never carry a value over from another line.
- **Decision:** fix: a line with an id but no value is skipped and reported by line number, blank lines are skipped without a message, and no value is ever carried over (prompt log entry 051)

### S3.06 · Classifying a gene: `>=` up, `<=` down, applied independently

- **FuSION 1:** `Linux/src/main.cc:2681-2688`
- **What it does:** A line's gene goes to the up list when `value >= up_threshold`, and to the down list when `value <= down_threshold`. These are two separate `if`s, so the tests are independent, and both bounds are inclusive. A value between the thresholds goes to neither list. The value itself is then discarded: only the id is written. This is the rule `suggest_thresholds.py:16-17` describes.

  Both sides are 32-bit floats parsed from decimal text, so a value written exactly as a threshold counts. The EGF file has `hsa6662<TAB>1.01` (SOX9), which is in the real run's up list, and three genes at exactly 0.67 (`hsa10365`, `hsa1849`, `hsa7852`), which are up at ±0.67. On the EGF file the rule gives 43 up and 2 down at ±1.01 (the real run's lists) and 77 up and 13 down at ±0.67. On the TNFα file it gives 35 up and 7 down lines at ±1.45, repeats included (S3.08).
- **Problems:** None found in the rule itself. A port must parse values and thresholds at the same precision. Float 1.01 is 1.00999999046, so comparing a float value with a double threshold of 1.01 drops SOX9. With doubles on both sides the real files, which have at most two decimals, give the same lists. A value within float rounding of a threshold would differ: `1.0099999999` is up at 1.01 in FuSION 1 but not with doubles.
- **Proposed call:** keep. Port the inclusive, independent rule, with values and thresholds parsed at one precision.
- **Decision:** keep, with values and thresholds both as doubles; real files give identical lists, and [decision 0023](../decisions/0023-results-may-change-where-improved.md) allows the rounding-edge difference (prompt log entry 051)

### S3.07 · No check that the up threshold is above the down threshold

- **FuSION 1:** `Linux/src/main.cc:2644-2645`, `main.cc:2681-2688`; downstream `Linux/src/GraphManagerNew.cc:28476` and `28498`, `GraphManagerNew.cc:30867-30873`
- **What it does:** The two thresholds are used as given. When `up_threshold <= down_threshold`, every value in `[up_threshold, down_threshold]` is written to both lists, without a warning.
- **Problems:** A real GUI-written script sets both thresholds to 0.0 (`Linux/output_script.txt:31-32`). On the EGF file that puts 1630 genes up and 1569 down, and 145 of them, every gene at exactly 0.00, in both. When pathz3 later finds a node in both sets, it treats it as up only, both in the node-relaxation constraints (`GraphManagerNew.cc:28476` before `28498`) and when labelling solutions (`30867` before `30871`). The down entry is ignored without a message (stage 5).
- **Proposed call:** fix. Reject `up_threshold <= down_threshold` with an error before anything is written.
- **Decision:** fix as proposed; a FuSION 1 GUI script with both thresholds at 0.0 then stops with this error (prompt log entry 051)

### S3.08 · Repeated gene ids are classified line by line

- **FuSION 1:** `Linux/src/main.cc:2677-2689`
- **What it does:** cudf keeps no record of the ids it has seen. A repeated id is classified once per line, so it can be written to the same list several times or, if its values disagree, to both lists.
- **Problems:**
  - A real case: the TNFα file lists `hsa5806` three times (9.82, 9.82, 1.95) and `hsa3280` (HES1) twice (−1.48 both times). So `runs/real_experiments/experiment7.1/up_reg` holds `hsa5806` three times, and that folder's `down_reg` holds `hsa3280` twice. `hsa57018` also appears twice, at 0.
  - Repeats that disagree, such as `hsa7<TAB>2.0` and `hsa7<TAB>-2.0`, put the gene in both lists. The solver then reads it as up only (S3.07), while `gsea` and `pex` keep the last value, −2.0 (S3.14).
  - pathz3's reader removes repeats (S3.12), so the line count of a list overstates its genes.
- **Proposed call:** fix. Report repeated ids, write each gene at most once per list, and stop with an error when an id's lines put it in different classes.
- **Decision:** fix as proposed; the same rule binds every reader of the LFC file (S3.14) (prompt log entry 051) *Phase 2 (prompt log entries 165 and 167):* narrowed: Not DE genes are set to none before classification (04.C4), and with `repeats=max_abs` each id is reduced to its largest-magnitude value first, so its rows cannot conflict (04.C3).

### S3.09 · The up and down list format

- **FuSION 1:** `Linux/src/main.cc:2651-2652`, `main.cc:2683`, `2687`, `2691-2693`
- **What it does:** The lists are two plain-text files, at the paths given as arguments 4 and 5. Relative paths resolve against the launch directory (S1.26). Each file is created or truncated when cudf starts. For every input line that passes a threshold, cudf writes the gene id exactly as read, followed by `\n` (`std::endl`, which also flushes). There is no header, value, count or trailing whitespace, and CRLF input still gives LF output. Entries follow the input order, repeats included (S3.05, S3.08). A list with no genes is an empty, 0-byte file. In the real run, `run/up_reg` has 43 lines (342 bytes), starting `hsa1026`, `hsa1390`, `hsa1545`, and `run/down_reg` is `hsa1942` and `hsa3399` (16 bytes). pathz3's other node lists (essential, avoid, active, inactive, confirmed up and down, relaxed and non-relaxed nodes; stage 5) use the same one-id-per-line format and go through the same reader (S3.12).
- **Problems:** None found beyond the repeats of S3.08.
- **Proposed call:** keep. Port the format; under [decision 0019](../decisions/0019-partial-runs.md) the two lists stay a saveable step output that a later run can start from.
- **Decision:** keep (prompt log entry 053)

### S3.10 · Failure handling: outputs are truncated before the input is checked

- **FuSION 1:** `Linux/src/main.cc:2650-2669`, `main.cc:2695-2698`
- **What it does:** All three files are opened before any check, so both outputs are created or truncated first. If any of the three fails to open, cudf prints `Error: Couldn't open file <path>` for each failure, then `Skipping generation of up and down regulated files.`, all on stderr. It writes nothing, and the batch carries on with status 0 (S1.28).
- **Problems:**
  - A wrong LFC path empties the lists left by an earlier run, and the batch then runs with no observations. The merge-exception file built from the lists loses every expressed gene (S3.13). pathz3 sees `Up: 0 Down: 0`, and its node-relaxation bit width `ceil(log2(0))+1` (`Linux/include/macros.h:49`) is then undefined (stage 5). No real log shows `Up: 0 Down: 0`.
  - A missing or unwritable output path still truncates the other output: in the standalone copy, an empty `<down_out>` emptied an existing up list.
  - Clashing paths destroy data without a message. With one path for both lists, the two streams overwrite each other: the input `hsa1 2.0`, `hsa22 -2.0`, `hsa333 3.0`, `hsa4444 -3.0` left the single file `hsa22hhsa4444`. With the up path equal to the LFC path, the LFC file is truncated to 0 bytes before it is read.
- **Proposed call:** fix. Check the LFC file and both output paths before truncating anything, stop the run on failure as [decision 0021](../decisions/0021-required-and-optional-inputs.md) requires, and refuse output paths that coincide with each other or with the input.
- **Decision:** fix, as proposed (prompt log entry 053)

### S3.11 · Console output

- **FuSION 1:** `Linux/src/main.cc:2680` (a commented-out per-gene trace)
- **What it does:** On success cudf prints nothing. The only console trace is the command echo (S1.10). In the real run, `run_console.log:6-7` is the echoed line and its dashes, and line 8 is already the next command.
- **Problems:** The number of genes written, and the effect of the thresholds, stay invisible until pathz3 prints `Up: N Down: M`. That count comes after graph matching (S3.12): 3 and 0 in the real run, against the 43 and 2 that cudf wrote.
- **Proposed call:** fix. Print how many genes went to each list and, with S3.04, S3.05 and S3.08, how many lines were skipped or repeated.
- **Decision:** fix, as proposed (prompt log entry 053)

### S3.12 · How a list entry becomes a graph node (done by the readers, not by cudf)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:18688-18708` (`read_data_file_into_set_of_ints`), `GraphManagerNew.cc:797-808` (`get_rep_id_from_id`), `876-887` (`get_nid_from_rep_id`), `9243-9248` (`duplicate_node` copies member ids into a subgraph); callers `Linux/src/main.cc:4660-4661`; counts printed at `main.cc:4693`
- **What it does:** cudf writes every gene that passes a threshold, whether or not any graph contains it. pathz3 takes the two lists as tokens 10 and 11 of its line (`main.cc:4490-4491`) and reads each file as whitespace-separated tokens (`ifs >> id`), so several ids per line, blank lines and `\r` are all accepted. Each token is looked up in the solved graph's id-to-key map. The lookup is an exact, case-sensitive `std::map` lookup (S2.03). A token that is absent is skipped silently; otherwise the key's node id goes into a `std::set<int>`, which removes repeats. All member ids of a multi-gene node resolve to that node, so such an id marks the whole node (S2.03). The solved graph is graph 0, the most recent one (`main.cc:4388-4389`), which in every real batch is the `fb_rch` corridor. A corridor's id map holds only the ids of the nodes copied into it (`GraphManagerNew.cc:9243-9248`), so a gene outside the corridor never matches. After the stage-5 adjustments for the avoid and confirmed lists (`main.cc:4668-4679`), pathz3 prints the node counts: `Up: 3 Down: 0` in the real run's with-FSC solve (`run_console.log:281`), and `Up: 2 Down: 0` without the FSC (`run_console.log:350`), because the FSC, JUN, is itself up-regulated and is avoided.
- **Problems:** Unmatched entries vanish without a message. In the real run cudf wrote 43 up and 2 down genes. Of these, 30 and 1 occur in `run/merged_master_graph.xml`, and 3 and 0 reach the solve. Nothing lists the other 42.
- **Proposed call:** fix. Keep the exact id-to-node lookup, and report how many entries of each list match no node of the graph being solved, and which ones.
- **Decision:** fix: keep the exact lookup, and list unmatched genes in two groups, those absent from the merged network and those in the network but outside the corridor (stage 4's S4.16 covers the corridor side too) (prompt log entry 053)

### S3.13 · The other readers of the lists: pathz3's unused first read, and the merge-exception file

- **FuSION 1:** `Linux/src/main.cc:4633-4634` and `Linux/src/GraphManagerNew.cc:18652-18686` (`read_data_file_into_map`); real batch lines 153 and 160; `GraphManagerNew.cc:8087-8110` (exception-file reader, S1.17); `Linux/src/XMLParser.cc:925`, `958-962`, `1000-1008` (S2.13)
- **What it does:**
  - Before the read in S3.12, pathz3 first reads both lists with `read_data_file_into_map` (`main.cc:4634`), into an `unordered_map` from node key to `true` (up) or `false` (down). Because `insert` keeps the first entry, an id in both lists stays `true`. Nothing uses the map afterwards: `data_map` has no other reference in the live code.
  - All 87 batch files and the three GUI scripts build the merge-exception file from the lists with `exec cat $UP_REG_FILE $DOWN_REG_FILE $SRC_FILE $TGT_FILE $HSA_IDS_NOT_TO_BE_MERGED_FILE > $EXCEPTION_TO_NODE_MERGE_FILE` (real batch line 153), and `start` reads that file on line 160 (S1.17). Its reader takes the first token of each line, so it relies on cudf writing one id per line. An exception id never joins a family node (S2.13), so the thresholds also decide which genes are split out of their KGML boxes. In the real run, 19 of the 45 listed genes sit in at least one multi-gene entry of the 42 merged KGMLs.
  - `fb_rch` (stage 4), `psp` and `pex` do not read the lists. `pex` takes each node's class from pathz3's solutions file.
- **Problems:**
  - The first read is dead work: two file parses and one lookup per entry, whose result is discarded.
  - Because the thresholds change the exception set, a merged graph cannot be reused across thresholds or experiments. This is S2.13's coupling, not a defect in cudf.
- **Proposed call:** keep. Port the list format that both readers rely on; dropping the unused read belongs to stage 5, and S1.17 has already moved the exception-set use into memory.
- **Decision:** keep; stage 5's review decides the unused read (prompt log entry 054)

### S3.14 · The other readers of the LFC file parse it differently

- **FuSION 1:** cudf `Linux/src/main.cc:2677-2689`; `Linux/supporting_files/suggest_thresholds.py:67-81`; `Linux/supporting_files/gsea_select_pathways.py:86-96` (stage 7); `Linux/supporting_files/plot_explanation.py:132-143` (stage 6); pathz3 `Linux/src/main.cc:4504`, `4569`, `866-887` (stage 5)
- **What it does:** The real run parses the LFC file three times, with three different parsers: cudf (batch line 145), `gsea` (line 169, Python) and `pex` (line 269, Python). pathz3 also receives the path, as token 24 of its line, but never opens it. It uses only the base name, without extension, to name its SMT dump directory `<name>_smt-files_<YYYYmmdd_HHMMSS>` (`main.cc:866-887`, `4569`), for example `egf_hela_t120_lfc_smt-files_20260928_185105`. The path is also passed down at `main.cc:4765`, but no live code reads it. The readers treat the same lines as follows (checked with the standalone copy and with verbatim copies of the three Python functions):

  | Line | cudf | suggest_thresholds.py | gsea, pex |
  |---|---|---|---|
  | `gene<TAB>lfc` (header) | 0 | skipped, counted as unparseable | skipped |
  | blank | previous gene again (S3.05) | skipped | skipped |
  | `hsa9` (no value) | previous line's value | skipped, counted | skipped |
  | `hsa1<TAB>NA` | 0 | skipped, counted | skipped |
  | `hsa2<TAB>nan`, `hsa3<TAB>inf` | 0 | NaN, infinity kept | NaN, infinity kept (gseapy then drops NaN and stops on infinity, S7.04) |
  | `hsa4<TAB>1,5` | 1 | skipped, counted | skipped |
  | `hsa5<TAB>1_000` | 1 | 1000 | 1000 |
  | `hsa6<TAB>1.5abc` | 1.5 | skipped, counted | skipped |
  | `hsa7` twice (2.0, then −2.0) | in both lists | both rows counted | −2.0 (last wins) |

  Stage 8 found a fifth parser, the GUI's threshold preview (S8.14): it counts only lines with exactly two fields, one non-numeric value on such a line aborts the whole preview, and a repeated id keeps its last value.

- **Problems:** One file can mean different things to different steps of the same run: a gene can be ranked by gsea, shown by pex and classified by cudf with three different values. No real run is affected, because the EGF files are clean and no batch passes the TNFα file, the only real file with repeats, to `gsea` or `pex`. The three parses of a 3054-line file cost nothing that matters; the issue is consistency.
- **Proposed call:** fix. Every consumer of the LFC file applies one set of parsing rules, the ones settled in S3.03 to S3.05 and S3.08.
- **Decision:** fix as proposed: one LFC parser, used by every command that reads the file (prompt log entry 054)

### S3.15 · `suggest_thresholds.py`: invocation and report

- **FuSION 1:** `Linux/supporting_files/suggest_thresholds.py:1-41` (docstring), `118-146` (`print_report`), `223-243` (`main`); `CHANGELOG.md:125-136`
- **What it does:** `python3 suggest_thresholds.py LFC_FILE [--current UP DOWN] [--plot OUT.png]`. It uses only the Python standard library, plus gnuplot for `--plot`. It reads the LFC file (S3.16), prints a report on stdout and exits 0. A missing file exits 1 with `error: no such file: <path>`. A file with no parseable line exits 1 with ``error: no parseable `<gene> <lfc>` lines in <path>``. Bad options exit 2 (argparse). The report has four parts:
  - `LFC file : <path>`, then `genes    : <n>`, followed by `(<k> unparseable line(s) skipped)` when k > 0;
  - a `distribution` block: min and max, mean and population SD, median and raw MAD with the 1.4826-scaled MAD, and, when neither extreme is 0, how many times further one tail reaches than the other;
  - the `candidate thresholds` table (S3.17), with columns `criterion up down #up #down #DE %DE`;
  - three fixed lines that contrast quantile cuts with MAD and SD cuts.

  Recomputed from the EGF file with `--current 1.01 -1.01` (the script was not run): `min / max      -1.560 / +5.190`, `mean / sd      +0.0371 / 0.3364`, `median / MAD   +0.0000 / 0.1200   (MAD*1.4826 = 0.1779)`, `tail extents   up tail reaches 3.3x further than the other`. Nothing in FuSION 1 calls the script. The only references to it are its two copies, `CHANGELOG.md:125-136` and a notes file under `.claude/`; no batch file, command or GUI panel uses it.
- **Problems:** A suggested pair reaches cudf only by hand. The report is written to be read, and has no machine-readable form that a batch or the GUI could take.
- **Proposed call:** keep. Port the advisory report as it is; whether it becomes a FuSION command or stays a separate tool is a Phase 2 question.
- **Decision:** keep; its form is a Phase 2 decision (prompt log entry 054) *Phase 2 (prompt log entry 168):* extended by 04.C6: the table gains a `conflicts` column, and numbers a user may copy into a batch are printed exactly.

### S3.16 · `suggest_thresholds.py` reads the LFC file differently from cudf

- **FuSION 1:** `Linux/supporting_files/suggest_thresholds.py:67-81` (`read_lfc`), `suggest_thresholds.py:11-17` (docstring), `239` (sort)
- **What it does:** A line with fewer than two tokens is skipped, and counted as unparseable unless it is blank. Otherwise the second token goes through Python's `float()`, and a `ValueError` skips the line and counts it. Every parsed line becomes one row, so a repeated id counts once per line: `genes` is 74 for the TNFα file, which has 70 distinct ids.
- **Problems:**
  - The docstring says "Counting mirrors cudf exactly" (line 16). That holds only when every line is `<id> <number>`. Header, `NA`, blank and one-field lines are skipped here, while cudf classifies them as 0 or with a stale value. `1,5` and `1.5abc` are skipped here, while cudf reads 1 and 1.5. `1_000` is 1000 here and 1 in cudf (S3.14).
  - `nan` and `inf` are accepted, and one NaN breaks the analysis. `sorted()` cannot order it (`[3.0, nan, 1.0, 2.0]` stays in that order), the mean and SD become NaN, and the median and quantiles become unreliable. The SD rows then print `nan` thresholds with zero counts, and the other rows are wrong. cudf reads the same line as 0.
- **Proposed call:** fix. Read the file with the same rules as cudf (S3.14), so the counts are the ones cudf would write.
- **Decision:** fix, as proposed (prompt log entry 054)

### S3.17 · `suggest_thresholds.py`: the candidate criteria

- **FuSION 1:** `Linux/supporting_files/suggest_thresholds.py:84-91` (`quantile`), `94-96` (`counts`), `99-115` (`build_candidates`), `63` (`MAD_TO_SD`)
- **What it does:** The rows, in order, are `current` (only with `--current`), then:
  - `quantile 1%`, `2.5%` and `5%`: up = Q(1 − p) and down = Q(p), where Q interpolates linearly between the sorted values at position p·(n − 1);
  - `MAD k=2` and `k=3`: median ± k · 1.4826 · median(|v − median|);
  - `SD k=2` and `k=3`: mean ± k · population SD.

  Counts use cudf's rule (`>=` up, `<=` down) over all parsed rows. `#DE` is `#up` + `#down`, and `%DE` is `#DE`/n. Recomputed for the EGF file:

  ```
  criterion            up     down     #up   #down     #DE     %DE
  current          +1.010   -1.010      43       2      45    1.5%
  quantile 1%      +1.170   -0.470      33      34      67    2.2%
  quantile 2.5%    +0.667   -0.370      77      86     163    5.3%
  quantile 5%      +0.410   -0.300     159     156     315   10.3%
  MAD k=2          +0.356   -0.356     204      89     293    9.6%
  MAD k=3          +0.534   -0.534     105      20     125    4.1%
  SD k=2           +0.710   -0.636      69      13      82    2.7%
  SD k=3           +1.046   -0.972      39       2      41    1.3%
  ```
- **Problems:**
  - The SD rows are centred on the mean and the MAD rows on the median, but the thresholds the batches use are centred on 0. The real batch derives ±1.01 as "3*SD of this dataset's t=120 log2FC distribution (SD=0.336, 3*SD=1.009 -> 1.01)" (real batch lines 67-68), which is a row the script never prints. Its own SD k=3 row is +1.046 / −0.972, which gives 39 up genes, not 43. The four genes in between include FOSB (`hsa2354`, 1.04), the FSC of `runs/egfr_cxcl8_fosb_b10_nr8_er8/`, which would not count as up-regulated under that row. `CHANGELOG.md:135-136` calls ±1.01 "effectively an SD k=3 cut (+1.046 / −0.972), yielding 43 up / 2 down", but that row yields 39 and 2.
  - The EGF file has only 190 distinct values for 3054 genes, so a quantile cut cannot hit its share exactly: 1% gives 33 up and 34 down, about 1.1% each. Tied data makes this unavoidable, and it needs no fix.
- **Proposed call:** keep. Port the seven criteria as defined; which centre the SD rule should use is an open question below.
- **Decision:** keep the seven criteria, and add ±k·SD about 0 (k = 2, 3), the rule the batches use, so the table shows both centres and the cut in use (prompt log entry 055)

### S3.18 · `suggest_thresholds.py` counts genes in the file, not genes the solve sees

- **FuSION 1:** `Linux/supporting_files/suggest_thresholds.py:94-96`, `140-143`
- **What it does:** `#up` and `#down` count rows of the LFC file. The script has no graph, so it cannot tell which genes the network contains.
- **Problems:** The counts overstate, by an order of magnitude, what a threshold changes in the solve. For the real run's ±1.01 the table shows 43 and 2; of these, 30 and 1 are in the merged graph and 3 and 0 reach pathz3 (S3.12).
- **Proposed call:** keep. Counting what the solve sees needs the corridor, which exists only after `fb_rch`, and S3.11 and S3.12 report those counts where they are known.
- **Decision:** keep (prompt log entry 055)

### S3.19 · `suggest_thresholds.py --plot`: the histogram

- **FuSION 1:** `Linux/supporting_files/suggest_thresholds.py:149-220` (`render_plot`), `51-64` (palette, `NBINS`), `242-243`
- **What it does:** The plot needs `gnuplot` on `PATH`. Without it the script prints `warning: gnuplot not found on PATH -- skipping --plot` on stderr. Otherwise it puts all values into 80 equal-width bins over [min, max], drops empty bins (the y axis is logarithmic), writes the bins to a temporary data file and pipes a script to gnuplot. The output is a `pngcairo` image, 980×620, in Liberation Sans on `#fcfcfb`, with:
  - grey boxes for the bins;
  - every candidate threshold except `current` as a dotted vertical line, with duplicates merged after rounding to four decimals;
  - with `--current`, the up cut as a solid blue (`#2a78d6`) line and the down cut as a solid red (`#e34948`) line, labelled `up +1.01` and `down -1.01`;
  - the title `Log-fold-change distribution` and the subtitle `<n> genes · dotted lines = candidate cuts · solid = in use`.

  On success it prints `plot written: <path>`. If gnuplot fails, it prints `error: gnuplot failed:` and gnuplot's stderr. The palette is the one `plot_po.sh` uses (stage 6).
- **Problems:**
  - The exit status is 0 even when the requested plot was not written, because `main` ignores the result of `render_plot` (line 243).
  - A file whose values are all equal gives zero-width bins and `set xrange [v:v]`. gnuplot rejects that range: a minimal reproduction printed `Can't plot with an empty x range!`, exited 1 and left a 0-byte PNG.
- **Proposed call:** fix. Keep the plot, and exit non-zero when a requested plot is not written.
- **Decision:** fix as proposed; where 2.0 draws plots (gnuplot or the frontend) is a Phase 2 question (prompt log entry 055)

## Open questions

- **Which centre should an SD-based threshold use?** The batches cut at ±3·SD about 0 (real batch lines 67-68). `suggest_thresholds.py` prints mean ± k·SD and median ± k·MAD, and `CHANGELOG.md:135-136` treats the two as the same. The choice changes real results: FOSB (1.04) is up at ±1.01 but not at +1.046. The code cannot settle this; the user's choice of method for roadmap objective 5 would.
  *Resolved:* the report shows both centres, mean ± k·SD and ±k·SD about 0 (S3.17).
- **Is the TNFα file on a log scale?** Its values reach +94.57 (CXCL8), and its 19 batches cut at ±1.45. cudf assumes nothing about scale, so nothing in the code can tell. The source of the file would settle it. It comes from `auxilliary/IITB_ACTREC_old_data/` (next to `Final.xlsx`), and nothing in the repo records how it was made. The answer matters for whether 2.0 should check the scale of per-gene values (decision 0020).
  *Resolved:* the user confirmed the file holds log fold changes, so +94.57 is a genuine outlier (prompt log entry 055).
