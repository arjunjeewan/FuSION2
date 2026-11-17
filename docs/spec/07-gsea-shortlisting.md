# 07 · Pathway shortlisting

## Scope

This stage covers `gsea`/`gsea_select_pathways` (`Linux/src/main.cc:5766-5830`, help line `main.cc:899`). It ranks the genes of a log-fold-change (LFC) file by value, scores every candidate KEGG pathway by pre-ranked gene set enrichment analysis (GSEA), and writes the list of KGML files that `mff` merges next. The C++ handler only collects the arguments, shells out to `Linux/supporting_files/gsea_select_pathways.py` (243 lines, byte-identical to the macOS copy) and may pause. The script does the work with `prerank` from gseapy 1.3.1. gseapy is not part of FuSION 1, but its behaviour is part of this step, so its code is cited from the repo's virtual environment: "gseapy `gsea.py:493`" means `/home/ks-149-u24/Desktop/arjun/FuSION/.venv/lib/python3.12/site-packages/gseapy/gsea.py`, line 493 (live tree). The macOS handler (`macOS/src/main.cc:5793-5856`, help line `900`) differs only in how it finds the script (S1.20).

Hand-overs:
- **Before:** stage 1 owns the batch line's tokens (S1.05), `$` expansion (S1.07), the interactive branch (S1.03, dropped), the script lookup (S1.20) and failure handling in general (S1.28). Stage 3 owns the LFC file format and the comparison of its readers (S3.03, S3.14).
- **After:** stage 2's `mff` reads the shortlist (S2.16).
- **Outside:** stage 8's GUI never writes a `gsea` line; no Java source in FuSION 1 mentions it. The prototype `auxilliary/GSEA-with-python.py`, which fetched MSigDB's `c2.cp.kegg_legacy` gene sets, is called by nothing and is out of scope.

Names used below:
- "the real run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`, with its batch file (`gsea` at line 169, `mff` at 171), its `run_console.log` (the `gsea` output is lines 23-80) and its `run/` folder. Its table predates the keep list (S7.16).
- "the keep run" is `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr040_keep/` (`gsea` at batch line 171).
- "the 19 batches" are the 19 `*.batch` files under `runs/` that call `gsea`:
  - the real run and its b6 and b7 twins;
  - the three keep runs;
  - the four runs of `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/`;
  - the nine `runs/egfr_b10_gsea_fdr044_candidates/r*` runs of 2026-11-09.

  All 19 rank the same LFC file, `egf_hela_t120_lfc.tsv` (md5 `a1243fbc…`, 3054 lines), against the same 167-line candidate list (md5 `ac05b281…`). Their tables have identical score columns.
- "the copy" is a verbatim copy of the frozen script, run from a scratch directory with the repo's `.venv` Python 3.12.3 (gseapy 1.3.1, pandas 3.0.1, numpy 2.4.3), on the machine that made the real runs (Intel Xeon Gold 6140, with AVX-512). On the inputs of `runs/…_gsea_fdr_sweep/fdr025` it reproduces that run's shortlist and table byte for byte. Variants call `gseapy.prerank` as the script does, with one setting changed. FuSION 1 itself was not built or run.
- `<TAB>` stands for a tab character.

The 2026-11-09 change to Z3 result labelling touches none of this stage's code.

## Items

### S7.01 · `gsea` / `gsea_select_pathways`: the command and its arguments

- **FuSION 1:**
  - `Linux/src/main.cc:5766-5794` (dispatch and arguments), `main.cc:899` (help line); `macOS/src/main.cc:5793-5821`, `900`
  - tokens from `main.cc:1151-1181` (S1.05)
  - real batch line 169; keep run batch line 171
- **What it does:** `gsea <lfc_file> <kgml_list> <selector> <out_list> [<keep_ids>]`, with `gsea_select_pathways` as the long name.
  - **Arguments:** in batch mode the five values are tokens 1 to 5 of the line (`main.cc:5789-5793`):
    - the LFC file (S7.04);
    - the candidate list (S7.06);
    - the selector (S7.13);
    - the shortlist path (S7.15), which also sets the table's path (S7.16);
    - the keep list (S7.14), where an absent token or `0` means none.

    C++ checks none of them and hands them to the script unchanged (S7.02).
  - **Placement:** the handler never touches the graph manager, so it works before `start`. The real batch runs it after `start` (line 160) and right before `mff` (line 171), as 17 of the 19 batches do. In r7 and r8 an `exec echo` line that appends a curated KGML to the shortlist sits between the two (S7.15).
  - **Real lines:** after expansion (`run_console.log:23`): `gsea /home/…/inputs/egf_hela_t120_lfc.tsv /home/…/inputs/list_of_kegg_xml_files.txt fdr:0.25 /home/…/run/gsea_kgml_files.txt`. The keep run's line ends in `hsa1956,hsa3576,hsa3725`, which its batch builds as `$SRC_NODE,$TGT_NODE,$NODE_TO_TEST_FOR_SIGNIFICANCE`.
  - **Help and interactive mode:** the help line lists both names and `[keep_gene_ids,...]`. The interactive branch (`main.cc:5777-5786`) reads the same five values from `cin` behind prompts, with `0` for no keep list; S1.03 drops that mode.
- **Problems:** The argument count is never checked. A missing token arrives as `""` (S1.05) and is passed on in quotes:
  - with the selector missing, the script stops with exit status 2 (S7.13);
  - with the shortlist path missing, the whole analysis runs and prints its selection, then dies with `FileNotFoundError: [Errno 2] No such file or directory: ''` (exit 1, measured with the copy);
  - tokens after the fifth are ignored without a message.
- **Proposed call:** fix. Keep the four positional arguments, the optional keep list and both names, and report a wrong argument count before the analysis runs.
- **Decision:** fix, as proposed (prompt log entry 083)

### S7.02 · Running the script: the shell command, `python3` and library versions

- **FuSION 1:**
  - `Linux/src/main.cc:5796-5820`; `macOS/src/main.cc:5823-5846`
  - `gsea_select_pathways.py:53-67` and `185-193`
  - `Linux/README.md:41-47`, `macOS/README.md:58-66`
  - `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/run_sweep.sh:8`, `runs/egfr_b10_gsea_fdr044_candidates/run_all.sh:7`
  - gseapy `__init__.py:308-331` (defaults), `base.py:236-247` (thread cap)
- **What it does:**
  - **The command:** C++ builds `python3 "<exe_dir>/supporting_files/gsea_select_pathways.py" "<lfc_file>" "<kgml_list>" "<selector>" "<out_list>"`. It appends ` --keep "<keep_ids>"` when the keep token is neither empty nor `0`, and runs the string with `system()`, which hands it to `/bin/sh -c` and waits. `exe_dir` is the binary's directory (S1.20).
  - **The interpreter:** `python3` is whichever one is first on `PATH`. Both sweep launchers put the repo's virtual environment first (`export PATH=$REPO/.venv/bin:…`). That environment has Python 3.12.3 with gseapy 1.3.1, pandas 3.0.1 and numpy 2.4.3. The READMEs say `pip install pyvis gseapy` (`macOS/README.md:65`: `pip3 install pyvis gseapy`) with no versions, and the repo has no requirements file.
  - **The call:** the script imports gseapy and pandas and calls `gp.prerank(rnk=…, gene_sets=…, permutation_num=1000, min_size=15, max_size=500, outdir=None, seed=42)`. Every other setting is gseapy's default: `weight=1.0`, `ascending=False`, `threads=4` (capped at the CPU count minus 1) and `method="permutation"`. With `outdir=None`, gseapy writes no files.
- **Problems:**
  - Nothing pins or records the library versions, and the result depends on them:
    - the order of tied values comes from numpy's sort and changes with the CPU (S7.05);
    - pandas 3 removed an argument that gseapy 1.3.1 still passes for infinite values, so an `inf` in the LFC file now crashes the step (S7.04);
    - other gseapy versions were not tried.
  - `.venv/bin/activate:45` sets `VIRTUAL_ENV=/home/ks-149-u24/Desktop/FuSION/.venv`. The environment was created at that path (`.venv/pyvenv.cfg`) and moved, and the path no longer exists, so activating the environment does not put its `python3` on `PATH`. Only the launchers' explicit `PATH` does.
  - The arguments pass through `sh` inside double quotes, so a `$`, backquote, `"` or `\` in a path or keep list is interpreted by the shell instead of being passed on.
- **Proposed call:** fix. Whatever runs GSEA in 2.0 pins its library versions, records them with each result, and gets its arguments without a shell. S1.20 leaves the choice of helper language to Phase 2.
- **Decision:** fix, as proposed (prompt log entry 083)

### S7.03 · Failure: exit status, message, and `mff` merging a stale or empty list

- **FuSION 1:**
  - `gsea_select_pathways.py:172-175`, `194-196`, `227-239`; `Linux/src/main.cc:5817-5820`
  - downstream: `main.cc:2405-2437` (`mff`), `1594-1600` (`sz`), the `wgx` block (`Graph id … is not present`); S1.28
- **What it does:** The script's exit status is:
  - 0 when at least one pathway is chosen, by GSEA or by the keep list;
  - 1 when none is. The shortlist is still written, empty, and stderr gets `no pathway passed '<selector>'; loosen the selector`;
  - 2 for a bad selector term, before any file is read (S7.13);
  - 1 with `error running GSEA: <message>` when gseapy raises `ValueError`;
  - 1 with a Python traceback for any other error. Measured with the copy:
    - a missing LFC file or candidate list (`FileNotFoundError`);
    - an `inf` value (`TypeError`, S7.04);
    - an LFC file of one line (`AttributeError`);
    - no candidate with 15 to 500 ranked genes (`LookupError`, S7.09);
    - an empty candidate list (`Exception: Error parsing gmt parameter for gene sets`).

    By the code, a malformed KGML ends the same way, with `xml.etree.ElementTree.ParseError`.

  Except when nothing is chosen, a failure writes neither the shortlist nor the table, so files of those names from an earlier run stay as they were.

  When `system()` returns non-zero, C++ prints `gsea_select_pathways: no shortlist produced, script returned <rv> (is python3 with gseapy installed? script: <path>)` on stderr. `<rv>` is the raw wait status: 256 for exit 1, 512 for exit 2, and 32512 when `sh` finds no `python3`. Then it carries on. It pauses if stdin is a terminal (S7.18), and the batch's next line runs `mff` on the list path.
- **Problems:**
  - `mff` merges whatever the list path holds:
    - **A stale list:** re-running a batch in its own `$WORK_DIR` with the selector mistyped as `FDR:0.3` (exit 2) merges the previous run's shortlist. The script's argparse error and C++'s `no shortlist produced, script returned 512` are printed, but nothing says that `mff` then merges a stale list. The run still ends with status 0 (S1.28).
    - **No file, or an empty one:** `mff` prints `No graphs to merge`, `size 0` prints `No such graph`, and `wgx 0` prints `Graph id 0 is not present` and writes nothing. `rgx` then reads any `merged_master_graph.xml` that an earlier run left behind.
  - The message shows the wait status rather than the exit code, and blames a missing gseapy for every failure, a bad selector included.
  - No real run failed here: no file under `runs/` contains `no shortlist produced`.
- **Proposed call:** fix. Treat a script failure or an empty selection as a `gsea` failure, which stops the batch before `mff` (S1.28 already stops a batch at its first failure). The message gives the exit code and the script's own reason.
- **Decision:** fix, except that an empty selection is a reported result ("no pathway passed '<selector>'"), not a failure: that run's later steps are skipped, so one strict cut-off doesn't halt a sweep, as S4.18 decided for an empty corridor. A script failure stops the batch before `mff` (S1.28), with the exit code and the script's own reason (prompt log entry 083)

### S7.04 · The LFC file as `gsea` reads it

- **FuSION 1:** `gsea_select_pathways.py:86-96` (`read_lfc`), `178`, `198`; gseapy `gsea.py:479-523` (`_load_ranking`), `559`; S3.03, S3.14
- **What it does:**
  - **Reading:** `read_lfc` splits each line on any whitespace. The first token is the gene id, and the second, read with Python's `float()`, is the value; tokens after the second are ignored.
    - A line is skipped without a message when it has fewer than two tokens or `float()` rejects its second token: `NA`, `1,5`, `1.5abc`, or a header such as `gene<TAB>lfc`.
    - `float()` accepts `nan`, `inf`, exponents and underscores (`1_000` reads as 1000).
    - A repeated id keeps its last value, at the position of its first line.
  - **The ranking metric:** the values are used as the ranking metric unchanged. The up and down thresholds and cudf's lists play no part here.
  - **gseapy's handling:**
    - it drops NaN values, warning `Input gene rankings contains NA values(gene name and ranking value), drop them all!` (`gsea.py:496-501`);
    - for an infinite value it calls `Series.replace(-np.inf, method="ffill")` (`gsea.py:510-513`). pandas 3.0.1 rejects that call, so the step dies with `TypeError: NDFrame.replace() got an unexpected keyword argument 'method'`;
    - it needs at least two genes. A one-line file dies even earlier, with an `AttributeError`.
  - **Measured with the copy**, on the EGF file plus one line:
    - `hsa999991<TAB>nan` gives the same 42 pathways as the real run;
    - `hsa999992<TAB>inf` gives exit 1 and no shortlist;
    - a header line changes nothing.

    All 3054 lines of the real file parse (`run_console.log:29`: `3054 genes ranked by log-fold change, 1000 permutations`).
  - **Matching:** ids are compared exactly with the gene-set ids, which have the form `hsa1956` (S7.08).
- **Problems:**
  - S3.14's table says that `gsea` keeps `nan` and `inf`. `read_lfc` does, but gseapy then drops NaN and dies on infinity.
  - `gsea`, `cudf` and `pex` parse the file by different rules (S3.14), so one gene can be ranked by one value and classified by another.
  - Ids in any other form (`hsa:1956`, `EGFR`, `1956`) match no gene set. If no id matches at all, every set has fewer than 15 ranked genes and the step ends in a `LookupError` traceback (S7.09). Both sides of the match are KEGG ids ([decision 0020](../decisions/0020-plug-and-play-inputs.md)).
- **Proposed call:** fix, as S3.14 proposes. Read the file by the one set of rules that stage 3 settles (S3.03 to S3.05, S3.08). Then `gsea` ranks the same values `cudf` classifies, and that reader reports a non-finite value instead of letting it crash the GSEA library.
- **Decision:** fix, as proposed (prompt log entry 083)

### S7.05 · Bug: tied values are ordered by numpy's unstable sort, so the result depends on the CPU

- **FuSION 1:** gseapy `gsea.py:493-494` (`rank_metric.sort_values(by=…, ascending=False, inplace=True)` with pandas' default `kind="quicksort"`), `gsea.py:516-520` (the warning); real `run_console.log:25-26`
- **What it does:**
  - **Sorting and ties:** gseapy orders the genes by value, largest first, with pandas' default sort, which is numpy's unstable quicksort. Genes with equal values end up in whatever order that sort leaves them. The enrichment score depends on that order (S7.10), because within a block of equal values the running sum meets the set's genes and the other genes in that order.
  - **How many ties:** the EGF file has 3054 genes but only 190 distinct values, since values have two decimals.
    - 2994 genes share their value with another gene. The largest blocks are 145 genes at exactly 0.00 and 71 at −0.06.
    - gseapy counts 2864 repeats and warns in every run: `[WARNING] Duplicated values found in preranked stats: 93.78% of genes` / `The order of those genes will be arbitrary, which may produce unexpected results.`
    - Against a stable sort, which keeps ties in file order (here, gene-id order), 2859 of the 3054 genes sit at a different rank.
  - **Dependence on the CPU:** on x86, numpy 2.4 picks its sort code by CPU features. The copy, rerun with numpy limited by `NPY_DISABLE_CPU_FEATURES`, gives three different tie orders and three different results from the same inputs and library versions:

  | Ranking | First genes of the 0.00 block | Max change in ES, NES | fdr:0.25 | fdr:0.4 | fdr:0.44 | fdr:0.5 |
  |---|---|---|---|---|---|---|
  | numpy with AVX-512 (the real runs) | `hsa2560, hsa23710, hsa2563` | – | 42 | 71 | 87 | 90 |
  | numpy with AVX2 only (`X86_V4` disabled) | `hsa7137, hsa3983, hsa23624` | 0.36, 1.37 | 44 | 72 | 86 | 90 |
  | numpy SSE baseline (`X86_V3 X86_V4` disabled) | `hsa7185, hsa2560, hsa374291` | 0.37, 1.21 | 44 | 80 | 85 | 91 |
  | stable sort, ties by gene id | – | 0.35, 1.23 | 44 | 79 | 86 | 91 |

  The AVX2 and SSE variants change the ES of 159 of the 166 tested pathways, and the stable sort changes 158; every variant changes the NES of all 166. Notable changes:
  - In all three variants, Renal cell carcinoma and Thyroid cancer join the fdr:0.25 shortlist; with AVX2 only, their FDR goes from 0.251 to 0.241 and from 0.253 to 0.205.
  - With AVX2 only, Platelet activation's NES flips from +0.58 to −0.79.
  - With ties by gene id, fdr:0.44 gains ECM-receptor interaction and loses Bacterial invasion of epithelial cells and Leishmaniasis.
- **Problems:**
  - "Reruns are byte-identical" holds only on one class of CPU. All 19 real tables (2026-09-28 to 2026-11-09) are identical because they ran on the same AVX-512 Xeon. A machine without AVX-512, such as many desktop CPUs, gets a different shortlist from the same batch file. Apple silicon, which the macOS build targets, was not tested.
  - The tie order is arbitrary, not a property of the data, yet it moves NES by up to 1.37 and the shortlist by up to nine pathways.
- **Proposed call:** fix. Order tied values by gene id, ascending, so that the ranking, every score and the shortlist come out the same on every machine. This changes the real runs' shortlists (fdr:0.25 gives 44 instead of 42), which [decision 0023](../decisions/0023-results-may-change-where-improved.md) allows.
- **Decision:** fix, as proposed (prompt log entry 084)

### S7.06 · The candidate list file

- **FuSION 1:** `gsea_select_pathways.py:99-116` (`read_kgml_list`), `179-182`; `mff`'s reader `Linux/src/main.cc:2405-2416` (S2.16)
- **What it does:**
  - **Format:** one KGML path per line.
    - Each line is trimmed of leading and trailing whitespace, so a CRLF line loses its `\r`.
    - Blank lines are skipped.
    - A path that is not an existing file is skipped. One warning on stderr counts the skipped paths and names the first: `warning: <n> KGML file(s) in <list> not found, skipped (first: <path>)`.
    - Relative paths resolve against the launch directory (S1.26).
    - There is no comment syntax, so a `#…` line counts as a missing file.

    Each listed file is parsed in full with `xml.etree.ElementTree` (S7.07, S7.08). The format is the one `mff` reads (S2.16).
  - **Real list:** all 19 batches pass `inputs/list_of_kegg_xml_files.txt`, identical to `auxilliary/list_of_kegg_xml_files.txt`. It holds 167 absolute paths: the 163 KGMLs of `auxilliary/hsa_pathways_with_exp_score/`, then four from `updated_pathways/xml_files/` (Ferroptosis hsa04216, Breast cancer hsa05224, Hepatocellular carcinoma hsa05225, Gastric cancer hsa05226). Line 1 is `/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score/hsa03320.xml`.
- **Problems:** `gsea` and `mff` read this format by different rules.
  - `gsea` skips blank lines, missing files and `\r`, with one warning. `mff` keeps the `\r` and aborts on a blank line or bad path (S2.16).
  - So `gsea` quietly cleans a list that `mff` would reject, and a candidate whose file is missing drops out of the merge with only a warning. Measured with the copy: a list with a blank line, a missing path and three CRLF lines gives a clean six-line shortlist and one warning.
- **Proposed call:** fix. Read candidate lists with the same reader and checks as `mff`'s list, which S2.16 checks path by path before the run starts ([decision 0021](../decisions/0021-required-and-optional-inputs.md)). Then a missing candidate stops the run instead of silently leaving the shortlist.
- **Decision:** fix, as proposed (prompt log entry 084)

### S7.07 · Bug: candidates that share a pathway id collapse into one

- **FuSION 1:** `gsea_select_pathways.py:111` and `115` (the dict keyed by pathway id), `228-230`
- **What it does:**
  - **Identity:** a candidate's id is the KGML root's `name` attribute without `path:` (`path:hsa04010` becomes `hsa04010`), or the file path when `name` is missing. Its title is the root's `title`, or else the id.
  - **Collapse:** candidates are stored in a dict keyed by that id, so when two files have the same id, the second replaces the first. The shortlist and the table then hold only the later file, at the earlier file's position, and the earlier file disappears without a message. That follows from Python's dict semantics. With the copy, a list naming the same five files twice gives five tested pathways and a five-line shortlist; `mff` would have read ten files.
- **Problems:** The two KGML directories share 163 pathways. `auxilliary/hsa_pathways_with_exp_score/` holds 163 KGMLs and `updated_pathways/xml_files/` holds 273, of which 163 have the same file names and root `name`s. Two of those 163 pairs have different gene sets (hsa04080 and hsa04141, one gene each). A candidate list that draws on both directories therefore silently loses one version of each shared pathway. No real list does this: the 167 ids are distinct.
- **Proposed call:** fix. Stop with an error that names both files when two candidates share a pathway id.
- **Decision:** fix, as proposed (prompt log entry 084)

### S7.08 · Gene sets: every gene entry of each KGML

- **FuSION 1:** `gsea_select_pathways.py:11-17` (docstring), `110-115`, `187`; gseapy `base.py:460-520` (`load_gmt`); `CHANGELOG.md:70-74` (live tree)
- **What it does:**
  - **Construction:** a candidate's gene set is every name of every `type="gene"` entry in its KGML, with the colon removed (`hsa:1956` becomes `hsa1956`) and repeats dropped. Group, compound, ortholog and map entries add nothing directly; a group's member genes have gene entries of their own. Relations play no part, so a gene with no edge in the pathway still counts. gseapy then keeps the set's genes that are in the ranking (S7.09).
  - **The real sets:** across the 167 candidates, the sets hold 8 to 533 genes (median 103), and every gene-entry name has the `hsa:` prefix.
  - **Why not MSigDB:** the script builds its sets from the KGMLs, not the prototype's MSigDB `c2.cp.kegg_legacy` sets. Its docstring says those match only 90 of the 167 KGMLs and lack the Ras, PI3K-Akt, NF-kappa B and TNF signaling pathways (`11-17`). That was not re-measured, because it needs the MSigDB download. As a result, every tested set is exactly one file that `mff` can merge.
- **Problems:**
  - GSEA scores membership, while `mff` takes a pathway's relations. Across the 167 candidates, 3301 of the 21,205 gene memberships (16%) are genes on no relation of their pathway, of any type.
  - At least five of the 42 pathways that pass fdr:0.25 do so mostly on such genes, counting a group's components as on its relations:
    - Systemic lupus erythematosus (hsa05322): 21 of its 143 genes are on one of its 7 relations;
    - Asthma (hsa05310): 6 of its 32 genes are on its 3 relations;
    - Viral carcinogenesis (hsa05203): 6 of its 205 genes are on its 3 relations;
    - Transcriptional misregulation in cancer (hsa05202): 19 of its 201 genes are on its 9 relations;
    - Malaria (hsa05144): 11 of its 50 genes are on its 6 relations.
  - The ids are KEGG's on both sides of the match, which [decision 0020](../decisions/0020-plug-and-play-inputs.md) flags for plug-and-play inputs.
- **Proposed call:** keep. Taking the sets from the candidate files is what makes every tested set one mergeable file. Scoring only genes that sit on relations would be a change of method, and Phase 2 decides how other pathway sources supply gene sets.
- **Decision:** keep the gene sets, and add a table column giving each pathway's share of genes that sit on its relations (prompt log entry 084)

### S7.09 · Size filter: 15 to 500 ranked genes

- **FuSION 1:** `gsea_select_pathways.py:61-66`, `141-142`, `213-220`; gseapy `base.py:484-520`
- **What it does:**
  - **The rule:** gseapy tests a set only when the number of its genes in the ranking is at least 15 (`MIN_SIZE`), at most 500 (`MAX_SIZE`) and below the total number of ranked genes. Both bounds are constants in the script. The prototype used a minimum of 1 to fit its 8-gene demo; 15 is GSEA's default (`61-63`).
  - **On the real inputs:** 166 of the 167 candidates are tested. The exception, Sulfur relay system (hsa04122), has 6 ranked genes (`run_console.log:30`: `166 of 167 candidate pathways tested (the rest have fewer than 15 or more than 500 ranked genes)`).
  - **Untested pathways:** an untested pathway has no scores and the selector never picks it. It gets a table row only when the keep list chooses it (S7.14, S7.16).
  - **No set in range:** gseapy raises `LookupError` (`base.py:517`), which the script does not catch. The result is a traceback with gseapy's hints, exit 1 and no shortlist (measured with a list holding only hsa04122).
- **Problems:**
  - A candidate that is neither tested nor kept leaves no row in the table. Only the count line shows that it existed.
  - The no-candidate case ends in an uncaught traceback (S7.03).
- **Proposed call:** fix. Keep 15 to 500, and give every candidate a row in the table, an untested one with its ranked-gene count and no scores. Report "no candidate has 15 to 500 ranked genes" as a `gsea` failure (S7.03) instead of a traceback.
- **Decision:** fix, as proposed: a no-candidate case is a `gsea` failure, unlike S7.03's empty selection (prompt log entry 085)

### S7.10 · The enrichment score, leading edge, `Tag %` and `Gene %`

- **FuSION 1:** gseapy `gsea.py:552-627` (`run`; the scores come from the compiled `prerank_rs`, `gsea.py:604`) with `weight=1.0`; the script copies the results into its table (`gsea_select_pathways.py:200-202`)
- **What it does:** Let N be the number of ranked genes, in gseapy's order (S7.05), with values r_1 ≥ … ≥ r_N, and let S be the set's m ranked genes.
  - **Running sum:** after rank i the running sum is `P_hit(i) − P_miss(i)`, where
    - `P_hit(i) = Σ_{j ≤ i, j ∈ S} |r_j| / Σ_{j ∈ S} |r_j|`;
    - `P_miss(i) = (number of j ≤ i not in S) / (N − m)`.
  - **ES:** the running sum at the first rank k where its absolute value is largest.
  - **Leading edge:** for ES ≥ 0, the set's genes at ranks 1 to k, in ranking order. For ES < 0, the set's genes at ranks k to N, listed from the bottom of the ranking up.
  - **Columns:** `Tag %` is `<leading-edge size>/<m>`. `Gene %` is k/N for ES ≥ 0 and (N − k + 1)/N for ES < 0, as a percentage with two decimals. `Lead_genes` joins the leading edge with `;`.

  Recomputing all this from the ranking gseapy builds reproduces the 166 real ES values to within 9 × 10⁻¹⁶, and every leading edge, `Tag %` and `Gene %` exactly. Real example, table row 1: Bladder cancer, `ES 0.7467262550167671`, `Tag % 10/39`, `Gene % 6.12%`, `Lead_genes hsa3576;hsa7057;hsa1026;…`. Of the 166 tested pathways, 128 have a positive ES and 38 a negative one.
- **Problems:** None found in the formula. The tie order that feeds it is S7.05.
- **Proposed call:** keep.
- **Decision:** keep (prompt log entry 085)

### S7.11 · Significance: 1000 permutations with seed 42, NES, nominal p, FDR and FWER

- **FuSION 1:** `gsea_select_pathways.py:64`, `67`, `185-193`; gseapy `gsea.py:552-627` (compiled `prerank_rs`), `__init__.py:308-331`; gseapy's Python reference formulas in `algorithm.py:568-700`
- **What it does:**
  - **Permutations:** gseapy's prerank permutes gene labels. In each of 1000 permutations the genes are shuffled over the fixed list of values and every set's ES is recomputed, which gives each set 1000 null scores. The shuffles depend only on the seed, 42. Removing four candidates or reversing the candidate order leaves every ES, NES and nominal p unchanged, and 1, 4 or 8 threads give identical tables (measured).
  - **The statistics,** as gseapy's Python reference code states them:
    - **NES** is ES divided by the mean of the set's null scores of the same sign, taken as positive: `ES / mean(null ≥ 0)` for ES ≥ 0 and `ES / |mean(null < 0)|` for ES < 0.
    - **Nominal p**, for ES ≥ 0, is the share of the set's non-negative null scores that are ≥ ES. For ES < 0 it is the share of its negative null scores that are < ES. The real values fit this rule: Complement and coagulation cascades has `0.007352941176470588`, which is 1/136, and Nicotine addiction has `0.007168458781362007`, which is 2/279. It is 0 when no null score is as extreme, as for Bladder cancer (`0.0`).
    - **FDR q**, for NES* ≥ 0, is the share of all tested sets' non-negative normalised null scores that are ≥ NES*, divided by the share of the non-negative observed NES that are ≥ NES*, capped at 1. For NES* < 0 the same holds on the negative side. The null scores are pooled over every tested set, so a pathway's FDR depends on the others (S7.12).
    - **FWER p** comes from the compiled core; FuSION 1 only copies it into the table.

  Real row: Bladder cancer, `NES 2.064414339961141`, `NOM p-val 0.0`, `FDR q-val 0.0009217452670540877`, `FWER p-val 0.001`.
- **Problems:**
  - The fixed seed hides Monte Carlo noise. With seeds 1 to 20, fdr:0.25 selects 41 to 46 pathways: 38 pass with every seed and 8 with some. ErbB signaling passes with 4 of the 20 seeds. With 10,000 permutations (11.6 s instead of 1.3 s), fdr:0.25 selects 43.
  - Byte-identical NES, p and FDR values need gseapy's own random stream. A reimplementation gets statistically equivalent but different numbers, so pathways near a cut-off would change.
- **Proposed call:** keep. Port the procedure with seed 42 and 1000 permutations. The measured noise is for the user to weigh against the cost of more permutations.
- **Decision:** keep the procedure, with the seed (default 42) and the permutation count (default 1000) as run settings recorded in the output, like S1.29's other result-changing settings (prompt log entry 085) *Phase 2 correction (prompt log entry 183):* gseapy's core, which made FuSION 1's numbers, differs from the Python reference code quoted above in two places: for ES < 0 the nominal p counts null scores ≤ ES, and NES uses ES itself as the mean when no null score has the needed sign. The FWER formula is given in 08-enrichment.md.

### S7.12 · A pathway's FDR depends on the other candidates

- **FuSION 1:** `gsea_select_pathways.py:185-193` (one `prerank` call over all candidates); gseapy `__init__.py:338-340`; `CHANGELOG.md:95-98` (live tree)
- **What it does:** All candidates go into one `prerank` call, and the q-value pools the null scores and observed NES of every tested set (S7.11). gseapy's docstring notes it: "If multiple gene sets are provided, the FDR null distribution will be based on the combined gene sets." A pathway's ES, NES and nominal p depend only on its own genes, but its FDR moves when other pathways are added or removed. Measured with the copy:
  - **Four fewer candidates:** dropping the four `updated_pathways` KGMLs from the 167-line list changes the FDR of 154 of the 162 pathways left, by at most 0.0071 (`CHANGELOG.md:95-98` records the same). At fdr:0.4, GnRH signaling (0.406 → 0.399) and Type II diabetes mellitus (0.402 → 0.396) then pass, although neither is related to the four removed.
  - **The shortlist alone:** GSEA on just the 42 pathways shortlisted at fdr:0.25 gives all 42 an FDR of at most 0.068.
- **Problems:** A cut-off means different things for different candidate lists. One pathway can pass or fail depending on which unrelated pathways were offered with it.
- **Proposed call:** keep. This is GSEA's definition of the q-value, and the table lists every tested candidate, so each result can be read against its own list.
- **Decision:** keep (prompt log entry 085)

### S7.13 · The selector: `fdr:Q`, `pval:P`, `nes:X`, `top:N`, comma-joined

- **FuSION 1:** `gsea_select_pathways.py:24-32` (docstring), `69-83` (`parse_select`), `119-130` (`apply_select`), `172-175`, `200`, `203`; `runs/egfr_b10_gsea_fdr044_candidates/README.md` ("Why fdr:0.44")
- **What it does:**
  - **Parsing:** the selector is split on `,`. Each term is `key:value`, where the key is exactly `fdr`, `pval`, `nes` or `top`, in lower case. `top` takes its value through `int()` and the others through `float()`.
  - **Bad terms:** any other key, an empty term or a value Python cannot parse ends the script before anything is read. It prints argparse's usage line and `error: bad selector term '<term>' (use fdr:Q, pval:P, nes:X or top:N)`, and exits with status 2. Measured with `FDR:0.25`, `fdr:`, `fdr:0.25,` (its empty last term), `top:2.5` and an empty selector.
  - **Applying the terms:** the terms are applied in turn to the tested pathways, sorted by |NES|, largest first:
    - `fdr:Q` keeps FDR q ≤ Q;
    - `pval:P` keeps nominal p ≤ P;
    - `nes:X` keeps |NES| ≥ X;
    - `top:N` keeps the first N.

    Both signs of NES count. The keep list adds its pathways afterwards (S7.14).
  - **Real selectors:** all 19 batches use a single `fdr:` term: `fdr:0.25` (4 batches), `fdr:0.3` (1), `fdr:0.4` (4), `fdr:0.44` (9) and `fdr:0.5` (1). The fdr044 README chose 0.44 because every cut-off from 0.429 to 0.454 selects the same 87 pathways.
- **Problems:**
  - Values are not range-checked. All of these were measured with the copy:
    - `top:-5` keeps all but the last five (161 of 166, through pandas `head(-5)`);
    - `top:0` and `fdr:nan` keep none (exit 1, S7.03);
    - `nes:-1` and `fdr:inf` keep all;
    - `fdr:0_5` reads as 5.0 and keeps all 166.
  - Term order matters once a `top:` term bites: `top:10,fdr:0.06` keeps 7 pathways, while `fdr:0.06,top:10` keeps 8.
  - FDR does not fall monotonically with |NES|: in 278 same-sign pairs, the pathway with the smaller |NES| has the smaller FDR.
    - So `fdr:` and `nes:` cut-offs are not interchangeable, and from fdr:0.4 up a selection is not a prefix of the |NES| order.
    - Example: at fdr:0.41, GnRH signaling (NES +1.186, FDR 0.406) and Tight junction (+1.190, 0.409) would pass, but Estrogen signaling (+1.194, 0.412) would not (keep run `run_console.log:108-110`).
- **Proposed call:** fix. Keep the four keys and the in-turn rule, but accept a term only when its value is finite and in range: `fdr` and `pval` from 0 to 1, `nes` at least 0, `top` a positive integer. Any other term fails with an error that names it.
- **Decision:** fix, order-independent: keep the four keys, accept only finite, in-range values (`fdr` and `pval` 0 to 1, `nes` at least 0, `top` a positive integer) with an error naming any bad term, and apply the thresholds before `top:N`, so term order never matters (prompt log entry 086)

### S7.14 · The keep list

- **FuSION 1:**
  - `Linux/src/main.cc:5793`, `5815-5816`
  - `gsea_select_pathways.py:34-39` (docstring), `169-170`, `176`, `205-211`, `222`
  - keep run batch line 171, `run_console.log:34`; `CHANGELOG.md:99-107` (live tree)
- **What it does:**
  - **Passing it on:** unless the fifth token is empty or `0`, C++ passes it as `--keep "<ids>"`. The script splits it on `,` and drops empty items.
  - **Choosing:** each candidate whose gene set holds at least one keep id is chosen, whatever its GSEA result, even when it was too small or too large to test. The gene set here is all of the KGML's gene ids, ranked or not (S7.08).
  - **Matching:** ids are matched exactly, so they must have the `hsa1956` form. An id found in no candidate prints `warning: --keep <id> is in none of the candidate pathways` on stderr and is otherwise ignored.
  - **Output:** the console reports `plus <n> kept for containing a --keep gene (candidate pathways per gene: <id> <count>, …)` and adds a `kept for` column (S7.17). The table gets `kept_for` (S7.16).
  - **Real use:** three batches use it, all with `$SRC_NODE,$TGT_NODE,$NODE_TO_TEST_FOR_SIGNIFICANCE`. In the keep run, fdr:0.4 picks 71 pathways and the keep list adds 16, for 87. EGFR is in 33 of the 167 candidates, CXCL8 in 21 and JUN in 37 (`run_console.log:34`).
  - **Why it exists:** GSEA scores transcription, so the pathway the stimulus signals through can score low. ErbB signaling, which EGF switches on by phosphorylation, has FDR 0.259 and misses fdr:0.25.
- **Problems:** A wrong id form (`hsa:1956`, `EGFR`) gets only the warning, and the shortlist silently lacks the pathways it was meant to keep. Measured: `hsa:1956` matches 0 candidates. The warning is right for a valid id that is in no KGML, such as PHLDA1, which r8 adds through a curated KGML appended after `gsea` (S7.15).
- **Proposed call:** keep.
- **Decision:** keep (prompt log entry 086)

### S7.15 · The shortlist file

- **FuSION 1:** `gsea_select_pathways.py:227-230`, `234`; read by `mff` (`Linux/src/main.cc:2405-2416`, S2.16); real `run/gsea_kgml_files.txt`
- **What it does:**
  - **Writing:** the file is written only after GSEA succeeded (S7.03), created or truncated, at the path given as argument 4. Relative paths resolve against the launch directory (S1.26).
  - **Format:** one line per chosen pathway: the KGML path exactly as the candidate list gave it after trimming (S7.06), followed by `\n`. Lines keep the candidate-list order, so `mff` merges the chosen files in the same relative order as it would the full list. There is no header. When nothing is chosen, the file is empty (0 bytes).
  - **Real files:** the real run's file has 42 lines; the first is `/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score/hsa04010.xml`. The keep run's has 87, ending in `…/updated_pathways/xml_files/hsa05226.xml`.
  - **Downstream use:** each of the 19 batches passes the file to `mff $GSEA_KGML_LIST xml $NODE_SPLIT_THRESHOLD n $INTER_DB_MAP_FILE -1`, 17 of them on the next command line. Users also work on it directly:
    - r7 and r8 append a curated, non-KEGG KGML with `exec echo $CURATED_KGML >> $GSEA_KGML_LIST` (r7 batch line 94), so r7's list has 88 lines;
    - `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/resolve_wo_n7_e0/resolve_wo_n7_e0.batch` (written 2026-11-11) merges the r5 run's saved list instead of running `gsea`. Its line 55 calls it "the original run's gsea selection (87 of 166), reused rather than recomputed".
- **Problems:** None found in the format. The hand-over to `mff` is a file round trip, which [decision 0019](../decisions/0019-partial-runs.md) keeps anyway as a saveable step output.
- **Proposed call:** keep.
- **Decision:** keep (prompt log entry 086)

### S7.16 · The results table `<stem>.gsea.tsv`

- **FuSION 1:** `gsea_select_pathways.py:41-44` (docstring), `198-203`, `213-224`, `231-232`, `235`; gseapy `base.py:811` (rows by |NES|)
- **What it does:**
  - **Path and writer:** the table is written next to the shortlist, at `os.path.splitext(<out_list>)[0] + ".gsea.tsv"` (`run/gsea_kgml_files.txt` gives `run/gsea_kgml_files.gsea.tsv`), and only when GSEA succeeded. pandas writes it tab-separated, with a header row, no index column and `\n` line ends. Floats are written at full precision, booleans as `True`/`False`, and missing values as empty fields.
  - **Columns:**
    - `Term`: the pathway id;
    - `pathway_name`: the KGML title;
    - `genes`: the set's genes present in the LFC file;
    - `ES`, `NES`, `NOM p-val`, `FDR q-val`, `FWER p-val`, `Tag %`, `Gene %`, `Lead_genes` (S7.10, S7.11);
    - `selected`: chosen by GSEA or by the keep list;
    - `kept_for`: the keep ids, only for pathways chosen by the keep list alone;
    - `kgml`: the file path.
  - **Rows:** every tested pathway, by |NES|, largest first. After them come the chosen pathways that could not be tested, in candidate order, with empty score fields.
  - **Real lines** (keep run; long paths and lists shortened with `…`):
    - `hsa04610<TAB>Complement and coagulation cascades<TAB>67<TAB>0.5711811532198225<TAB>1.7000926859823016<TAB>0.007352941176470588<TAB>0.05639405133885464<TAB>0.414<TAB>6/67<TAB>2.23%<TAB>hsa2152;hsa5329;hsa5328;hsa1604;hsa5054;hsa7035<TAB>True<TAB><TAB>/home/…/hsa04610.xml`
    - `hsa05140<TAB>Leishmaniasis<TAB>73<TAB>0.37309099093070164<TAB>1.1461241075332327<TAB>0.2777101096224117<TAB>0.42824060291414123<TAB>1.0<TAB>7/73<TAB>8.71%<TAB>hsa5743;hsa3725;…<TAB>True<TAB>hsa3725<TAB>/home/…/hsa05140.xml`
    - `hsa04744<TAB>Phototransduction<TAB>24<TAB>-0.35073362757962556<TAB>-1.1118098608085027<TAB>0.310580204778157<TAB>0.6371380483789727<TAB>0.982<TAB>6/24<TAB>9.66%<TAB>hsa1258;…<TAB>False<TAB><TAB>/home/…/hsa04744.xml`

    No real table has an untested row. The copy writes one for Sulfur relay system when it is kept for MPST: `hsa04122<TAB>Sulfur relay system<TAB>6<TAB><TAB><TAB><TAB><TAB><TAB><TAB><TAB><TAB>True<TAB>hsa4357<TAB>/home/…/hsa04122.xml`.
  - **Format history:** the tables written before the keep list existed, the real run's and its b6 and b7 twins' (2026-09-28 18:51), have no `kept_for` column. All later tables have it.
- **Problems:**
  - Nothing reads the table back. The docstring says it is "for picking a different threshold without re-running" (`43-44`), but no command selects from a saved table. The FDR sweep ran GSEA four times and the fdr044 set nine times, and all 19 tables have the same score columns.
  - A candidate that is neither tested nor kept has no row (S7.09).
  - `genes` counts the parsed LFC ids, including any NaN gene that gseapy then dropped (S7.04).
- **Proposed call:** keep.
- **Decision:** keep, with the changes from S7.04, S7.08 and S7.09 (prompt log entry 086)

### S7.17 · Console report

- **FuSION 1:** `gsea_select_pathways.py:138-160` (`print_selection`), `180-182`, `207-209`, `225`, `234-237`; gseapy `gsea.py:516-520`; real `run_console.log:23-80`
- **What it does:**
  - **stderr:** gseapy's tie warning (S7.05) with a timestamp, its NaN warning when the file holds NaN values (S7.04), and the script's warnings: missing KGMLs (S7.06), keep ids (S7.14) and an empty selection (S7.03).
  - **stdout:** a blank line, then (real run, lines 28-36):
    ```
    GSEA pre-ranked on /home/…/inputs/egf_hela_t120_lfc.tsv
      3054 genes ranked by log-fold change, 1000 permutations
      166 of 167 candidate pathways tested (the rest have fewer than 15 or more than 500 ranked genes)
      NES over tested pathways: -1.719 to +2.064

    selected 42 of 166 tested pathways (fdr:0.25)
        #  pathway   name                                                       genes      ES     NES   NOM p   FDR q
      -------------------------------------------------------------------------------------------------------------
        1  hsa05219  Bladder cancer                                                39  +0.747  +2.064   0.000   0.001
    ```
    The table has one row per chosen pathway. After a blank line come `  shortlist  -> <out_list>  (42 KGML files, in <kgml_list> order)` and `  full table -> <table>` (lines 79-80).
  - **With a keep list:** the `plus <n> kept …` line follows the `selected` line, and the table gains a `kept for` column.
  - **Rows:** numbered on from 1, in this order:
    - the pathways GSEA chose, by |NES|;
    - the kept ones, by |NES|;
    - the untested kept ones last, with `-` for their scores.
  - **Layout:** the name column is as wide as the longest name shown. ES and NES print with their sign and three decimals, and p and FDR with three decimals.
- **Problems:** The gene count is the number of ids parsed, including any NaN value that gseapy then drops. The EGF file plus a `nan` line reports `3055 genes ranked`, although 3054 are ranked. S7.04's fix removes this.
- **Proposed call:** keep.
- **Decision:** keep (prompt log entry 087) *Phase 2 (prompt log entry 183):* amended by 08.C10: the tie warning's second sentence becomes 'Tied genes are ordered by gene id.'

### S7.18 · The review pause

- **FuSION 1:** `Linux/src/main.cc:5822-5829`; `macOS/src/main.cc:5848-5855`; `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/run_sweep.sh:14`, `runs/egfr_b10_gsea_fdr044_candidates/run_all.sh:13`; `CHANGELOG.md:65-67` (live tree)
- **What it does:**
  - **The prompt:** after the script returns, whether or not it succeeded, FuSION checks whether stdin is a terminal (`isatty(STDIN_FILENO)`). If it is, FuSION prints `Review the pathways selected above. Press Enter to merge them, Ctrl-C to abort: ` and reads one line. Any input, or end of file, continues; Ctrl-C at the prompt ends FuSION.
  - **Why it is in C++:** POSIX `system()` makes FuSION ignore SIGINT while the script runs, so Ctrl-C during GSEA ends only Python, and FuSION goes on.
  - **When it is skipped:** without a terminal on stdin (`nohup`, `&`, `< /dev/null` or a pipe) there is no pause. Both sweep launchers redirect stdin from `/dev/null`, and no file under `runs/` contains the prompt.
- **Problems:**
  - FuSION also pauses after a failure, offering to "merge them" when nothing was produced (S7.03).
  - A batch started in a terminal without redirection blocks at this line until someone presses Enter. It is the only such stop in a batch run.
  - Ctrl-C while the script runs kills only Python. The batch then carries on to the pause or, without a terminal, straight to `mff` (S7.03).
- **Proposed call:** drop. [Decision 0019](../decisions/0019-partial-runs.md) lets a run stop after any step, which gives the same review point: stop after `gsea`, read the table, then start again from the saved shortlist. S1.03 already drops interactive input.
- **Decision:** drop, as proposed: review by stopping after `gsea` (decision 0019) or in the frontend (prompt log entry 087)

### S7.19 · Cost of the step

- **FuSION 1:** the whole script; `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr025/run_time.txt`; `CHANGELOG.md:76-81` (live tree)
- **What it does:** Measured with the copy on the real inputs, one call takes 3.3 s wall and 13.4 s CPU, with a peak of 318 MB. The wall time splits into:
  - 1.7 s to start Python and import gseapy and pandas;
  - 0.24 s to parse the 167 KGMLs;
  - 1.3 s for GSEA itself on 4 threads (3.7 s on 1 thread, 0.75 s on 8).

  The FDR-sweep fdr025 run's whole-run peak is 318,920 kB (`run_time.txt`), within 1 MB of the script's own peak, so the GSEA child is probably the largest process of that 11-minute run. The shortlist pays for itself: the CHANGELOG measured a b7 EGFR→CXCL8 batch at 7 s with the fdr:0.25 shortlist, against 3 min 14 s with all 163 KGMLs (`CHANGELOG.md:76-81`).
- **Problems:**
  - Every run recomputes the same analysis. The 19 batches give the same inputs and get identical score columns, about a minute of work in all, which is small next to merges and solves of minutes to 50 hours. The one batch that reuses a saved shortlist does so by hand (S7.15).
  - The candidate KGMLs are parsed here in Python, and the shortlisted ones again by `mff` in C++ (stage 2), which also re-reads the shortlist from disk.
- **Proposed call:** keep. The step is cheap, and starting a run from a saved shortlist under [decision 0019](../decisions/0019-partial-runs.md) removes the repeats.
- **Decision:** keep (prompt log entry 087)

## Open questions

1. **The compiled core's formulas.** The exact FDR and FWER formulas in gseapy 1.3.1's compiled core (`gse.cpython-312-x86_64-linux-gnu.so`), and how its permutations are drawn, could not be read. S7.11 gives the formulas of gseapy's Python reference code (`algorithm.py:568-700`). Those fit the real nominal p values, and the measured invariances fit label permutations shared by all sets. Settle by reading gseapy 1.3.1's Rust source. A reimplementation that has to match FuSION 1's numbers exactly also needs its random number generator.
   *Still open, for phase 2:* it matters only if 2.0 reimplements GSEA instead of calling gseapy (S7.02).
2. **Apple silicon.** The tie order, and so the shortlist, on Apple silicon (the macOS build) was not measured (S7.05). Settle by running the copy on a Mac.
   *No longer affects 2.0:* S7.05 breaks ties by gene id, so the tie order no longer depends on the machine. Phase 3 can still compare whole results on a Mac.
3. **Unpinned versions.** Results under the gseapy, pandas and numpy versions that today's unpinned `pip install pyvis gseapy` installs were not measured (S7.02). Settle by running the copy in a fresh environment and comparing its table with the real one.
   *No longer affects 2.0:* S7.02 pins and records the library versions.
4. **The MSigDB claim.** The docstring's claim that MSigDB `c2.cp.kegg_legacy` matches only 90 of the 167 candidate KGMLs (S7.08) was not re-measured. Settle by fetching the collection, as `auxilliary/GSEA-with-python.py` does, and matching its sets against the candidates.
   *No longer needed:* S7.08 keeps the gene sets from the candidate KGMLs.
