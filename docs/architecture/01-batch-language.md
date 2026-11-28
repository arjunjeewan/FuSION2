# 01 · Batch language and interpreter

## Scope

This section designs the `batch` module and the `fusion` executable (Choices B1 and C1). `batch` reads a batch file, splits its lines into tokens, expands variables, binds positional and named arguments through one command table, checks the whole file and its inputs before any step runs, runs the lines over values held in memory, runs sweeps, and gives the four partial runs of [decision 0019](../decisions/0019-partial-runs.md) their commands. `fusion` parses its command line and calls `batch`.

It implements spec stage 1 (S1.01 to S1.29), graph ids and `#LASTGRAPHNUM` (S1.09, S2.01), FuSION 1's old `mff` lines (S2.16), the reported results that end a run early (S4.18, S7.03, and 04.C5's `observations.none`), and the batch files the frontend writes (S8.18 to S8.20, S8.25 to S8.27).

It leaves to other sections:
- what each command does, and the names and defaults of its arguments: 02 (`start`, `rgx`, `wgx`, `sz`, `add`, `labels`), 03 (`mff`), 04 (`cudf`, `suggest_thresholds`), 05 (`fb_rch`), 06 (`pathz3`, resume state, partial frontiers), 07 (`psp`, `pex`, `plot_pareto`, FuSION 1's text formats) and 08 (`gsea`). The command table below lists the owners' names;
- the run directory's layout and names, `run.json`, the event set and its envelope, saved steps, the stop file, the run log, the settings registry and the shared vocabulary of failures, reports and contexts: 09. This section decides how a batch gives a run its label and root, and which options and commands reach those features;
- the pipeline model and the content of the default pipeline: 10. This section shows that the language can express it;
- packaging a POSIX shell for Windows, the process layer, the dependency check and the install home: 11.

Names and evidence used below:
- "The real batch" is stage 1's: `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch`. It has 273 lines and 13,614 bytes: 79 comments, 77 `let` lines, 67 blank lines and 50 commands, 34 of them `exec`. It holds 330 `$` signs.
- "The corpus" is the 116 `*.batch` files in the FuSION 1 tree on 2026-11-12 at 18:29, plus the three GUI scripts in the snapshot (`Linux/output_script.txt`, `Linux/script.txt` and `Linux/advtempscript.txt`): 119 files and 27,104 lines. The count keeps growing: `runs/egfr_b10_gsea_fdr044_candidates/*_string125` was being generated, as another set of cloned files, while this section was written.
- "The replay" is a Python re-implementation of FuSION 1's front end (`Linux/src/main.cc:1148-1300`: tokens split on spaces only, `;;` comments, `$` expansion by the first key in byte order, `let` keys expanded) and of 2.0's rules (longest match, literal keys). It was run over the corpus, and every corpus count below comes from it.
- Run times and peak memory come from the `run_time.txt` files that `/usr/bin/time -v` wrote for the two parallel sweeps under `runs/`.
- "This machine" has 36 logical CPUs (18 cores with 2 threads each) and 124 GiB of memory. Its `/bin/sh` is dash.
- "Entry N" is prompt log entry N in `docs/process/PROMPTS.md`, where the architecture polls quote each question and answer.
- FuSION 1 was not run.

## Requirements carried in

The batch file and the command line:
- **S1.01:** `-b <file>` reads the whole file first. A missing, unreadable or empty file, or no file name, stops with a message and a non-zero status. Data inputs follow [decision 0021](../decisions/0021-required-and-optional-inputs.md).
- **S1.02:** `-h` prints the usage and the command listing on stdout and exits 0. Usage after a bad invocation goes to stderr, with a non-zero status.
- **S1.03:** there is no interactive prompt, and `-d` is an unknown option.
- **S1.04:** the end of the file acts as `exit`.
- **S1.13:** `exit` ends the run; its status follows S1.28.
- **S1.20:** the install directory is found once at startup, in the same way on every platform, and serves every bundled file.
- **S1.21 to S1.24:** no `config.txt`, encoding tables, colour files or unused startup objects; so no 450 leading spaces, and no logger lines at exit.
- **S1.27:** numbers on the console are printed with enough digits to be exact, which includes the wall times this section prints.

Lines, tokens and variables:
- **S1.05:** runs of spaces and tabs, and a trailing `\r`, separate tokens. The token list fits the line, and a missing argument can be told from an empty one. A double-quoted token may contain spaces. `exec` lines bypass the quoting.
- **S1.06:** a line whose first token starts with `;;` is a comment, and blank lines are skipped; neither is echoed. `#` is not special.
- **S1.07:** at each `$`, the longest defined key that is a prefix wins. No delimiter is needed, each value is inserted once and never rescanned, and an unmatched `$` passes through. A `$NAME` that matches no key gives a warning, except on `exec` lines. Every token, the command name included, is expanded before dispatch.
- **S1.08:** a `let` key is taken literally. A redefinition is an error that names the key, and so are extra tokens after the value. The value is expanded when the `let` runs; a missing value is empty.
- **S1.09:** a `let` value of exactly `#LASTGRAPHNUM` is the id of the latest graph at that moment. When there is none, that is an error and the variable stays undefined.
- **S2.01:** graph ids are integers, and `0` names the latest graph. Merge intermediates are freed.

Commands:
- **S1.10:** the echo keeps its format: the expanded tokens, each followed by one space, a newline and a line of 31 dashes, flushed before the command runs. `let` lines, comments and blank lines are not echoed. The frontend follows `events.jsonl` instead (A2).
- **S1.11:** command names match exactly and case-sensitively, with the short aliases.
- **S1.12:** one command table drives dispatch, help and the argument names.
- **S1.14:** `exec` passes the rest of the line to the shell exactly as written after `$` expansion. A non-zero status is reported with the line number and counts as a failure (S1.28).
- **S1.15:** `start` keeps its four files. A named `start` file that cannot be read stops the run before it starts.
- **S1.17, S2.13, S8.22:** 2.0 adds the up and down genes, the source, the target and the FSC to the merge-exception set itself, and old batches that still build the exception file keep working.
- **S2.16:** the new form is `mff <list> <thres> <y|n>`. FuSION 1's `xml` token and map-file list are accepted, with a warning that they are ignored. Every listed path is checked before the run.
- **S2.26:** later steps use the in-memory merged graph after the same normalisation, instead of parsing the file that `wgx` wrote.
- **S2.27:** the pre-merged old master stays readable the way FuSION 1 reads it, through a one-file `mff … y` or `rgx … y`, kept apart from the raw-KGML pipeline and flagged as legacy.
- **S2.29:** `upfx` is dropped.
- **S6.25:** old batch files keep their inline gnuplot lines, which `exec` passes to the shell (S1.14).
- **S3.01, S3.02, S4.01, S5.01, S5.02, S6.01, S6.03, S6.10, S7.01:** each command keeps its FuSION 1 positional form and gains names ([decision 0018](../decisions/0018-batch-interface.md)). A missing argument, extra tokens, a `-1` list without its terminator and a number that is not a whole numeric token are errors. Each position consumes its token, so an empty token keeps only that slot's default.
- **S5.28:** `pathz3`'s mode 1 is dropped.
- **S7.18:** there is no pause after `gsea`; review is done by stopping after it.

Failures, results and outputs:
- **S1.25:** one debug log per run, with a per-run name that can be set before `start`.
- **S1.26, S5.30, S8.21:** every output goes to the run's own directory, never to the launch directory. A run directory is never deleted or reused. Partial runs find saved steps there. Phase 2 designs how a batch names it.
- **S1.28:** errors are caught per command and reported on stderr with the batch line number. The batch stops at the first failure and exits non-zero; a failing `exec` counts. Partial runs restart from the last good step.
- **S1.29:** every flag that changes results becomes a run setting recorded with the result. Debug outputs sit behind one debug option, which the frontend does not show.
- **S4.18:** an empty corridor is a reported result ("no path within b"). Its solve is skipped, so one small bound does not halt a bound sweep. Real failures leave no graph behind and stop the run.
- **S7.03:** an empty GSEA selection is a reported result, and that run's later steps are skipped. A failure of the step itself stops the batch.
- **S4.19, S5.23:** corridor traces and SMT dumps are written only under the debug option.

Batch files from the frontend:
- **S8.18, S8.19, S8.20:** the frontend turns every drawn pipeline into a batch file, with named arguments, every value checked and quoted where needed. The standard run is the default pipeline. The block library matches the real commands.
- **S8.25, S8.26, S8.27:** the default pipeline solves both FSC cases with the named FSC on one corridor, draws the Pareto plot with the built-in step, and includes `gsea` and `pex`.

Decisions and skeleton choices:
- **[0017](../decisions/0017-batch-file-compatibility.md), [0023](../decisions/0023-results-may-change-where-improved.md):** FuSION 1 batch files keep running. Their results may change where 2.0 improves, and each such change is recorded as an expected difference.
- **[0018](../decisions/0018-batch-interface.md):** one command per line, so any subset of steps runs in any order; arguments by name (`name=value`) or by FuSION 1's positions; a simple parameter list lets one file run a sweep.
- **[0019](../decisions/0019-partial-runs.md):** stop after any step, start from a saved step, resume a stopped solve, solve part of the frontier.
- **[0021](../decisions/0021-required-and-optional-inputs.md):** all inputs are checked before the first step. A missing required input, or an optional input that is named but missing or unreadable, stops the run. An optional input that is not given turns its feature off; one that is given but empty counts as an empty list.
- **A1:** `fusion` runs as the frontend's child process, one per run. The frontend writes the batch file into a new run directory. Stop is a platform-neutral request, after which the run saves its state and exits with a "stopped" status.
- **A2:** structured events go to `events.jsonl`; the human-readable console output stays on stdout.
- **B1, C1:** all behaviour sits in `libfusion`; `fusion` is a thin executable. This section is the `batch` module.
- **D1:** steps pass typed values in memory, and each step's output can be saved and loaded.
- **E1:** `run.json` records every setting with its value and its source: a default, a position or a name.
- **F1:** no Python remains in the backend; gnuplot draws the Pareto image.
- **G1:** C++20, with nlohmann/json.
- **G3:** Linux, macOS and Windows, all native. This section settles which shell `exec` uses on Windows.

Amendments and readings decided in the architecture polls, which this design follows:
- **S1.07** is amended by Choice 01.C2's `$$` (entry 187).
- **S1.14** is amended by Choice 01.C11 for FuSION 1's six fixed-name moves (entry 179), and by Choice 01.C20 for window plots on a machine without a display (entry 190).
- **S1.15** is amended by row 2B.4: `start` is optional, and without it the maps are the shipped ones (S8.13; entry 163). With decision 0021 and S2.16, it is also amended by Choice 01.C12 for inputs the batch makes (entry 188).
- **S1.26 and S8.21** are read by rows 2B.3 and 4A.11: a relative or omitted output path lands in the step's directory; an absolute one is honoured as well, the file also kept in the step directory and both recorded in `run.json`; section 07's site export is the one other write outside the run directory (entry 169).
- **S1.28** is read per sweep point by Choice 01.C5 (row 4B.5, entry 188).
- **S3.02** is refined, and S3.03's prefix reading settled, by 04.C2's one decimal grammar, parsed with fast_float (11.C5; entry 167).
- **S3.08** is narrowed by 04.C4: a gene marked Not DE is removed in `cudf` before classification, so it never reaches the exception set (entry 165).
- **S4.18's wording** is widened by Choice 01.C15: a reported result skips every later line of its point (row 4A.6, entry 167).
- **S5.17** is extended by 06.C10: the FSC is a set of ids, and the cases are `in`, `out` and `free` (entry 175).
- **S6.25** is amended by Choice 01.C20 for window plots (entry 190).
- **S8.22** is amended by 03.C2: genes the solve takes as confirmed up or down join the exception set (entry 165).
- **E1** is read by rows 1.21 and 4A.9: FuSION 1's text files are written when the one setting `legacy_prefix` asks for them (entry 179).
- **A1** is read three ways: it governs how runs are executed, so read-only calls that make no run directory (`--check`, `--commands`, `--version`, `--stop`, `--query`) are outside it (row 4A.2, entry 168); Stop is the `stop` file only, of A1's two channels (09.C7, entry 184); and signal handlers only make the same request (09.C8, entry 185).

## Design

### Components

| Component | What it does | Spec |
|---|---|---|
| source reader | reads the batch file once, with its SHA-256 | S1.01 |
| lexer | lines, tokens, quotes, comments, the raw text of `exec` lines | S1.05, S1.06, S1.14 |
| `Variables` | `let`, longest-match `$` expansion, the four predefined `$FUSION_` variables | S1.07 to S1.09 |
| `CommandTable` | every command: names, arguments, positional forms, rules, tool needs, handler, checker | S1.11, S1.12 |
| binder | positional and named tokens to typed values, each with its source | 0018, E1 |
| planner | `sweep` lines to points; the `run` line | 0018, S1.26 |
| checker | the whole file, for every point, and every input, before anything runs | 0021 |
| interpreter | runs one point's lines over a `Workspace` of in-memory values | D1, S1.28 |
| `Shell` | runs `exec` lines and captures their output into the run log | S1.14, G3 |
| FuSION 1 adapter | positional forms, ignored tokens, fixed-name moves, window plots | 0017, 0023 |
| sweep coordinator | one child `fusion` process per point | 0018 |
| query mode | one read-only batch line from stdin, a JSON answer on stdout | S8.14 |
| `fusion` | parses the command line and calls the above | B1 |

### Reading a batch file

- `fusion -b <file>` reads the file once into memory (S1.01), so editing it during a run has no effect. 09 copies it into the run's `inputs/` with its SHA-256 (09.C3).
- Lines end at `\n`. One `\r` before it, or at the end of the file, is dropped, so files saved with CRLF on Windows work. No corpus file contains `\r`.
- The file must be UTF-8, and a leading UTF-8 byte-order mark is skipped. An invalid byte sequence is an error that names the line (`batch.bad_value`), because paths must convert to UTF-16 on Windows (Choice 01.C19). The corpus is ASCII except for two files with a `—` in a comment (`runs/real_experiments/experiment_7_on.batch` and `experiment_7_off.batch`).
- A missing or unreadable file, no file name, or a file with no command line at all stops `fusion` with a message on stderr and exit status 2 (S1.01).

### Tokens and quoting

Tokens follow S1.05 and S1.06, with the quoting rules of Choice 01.C2:
- Tokens are separated by runs of spaces and tabs.
- A `"` opens a quoted section anywhere in a token, and the next `"` closes it. Inside, `""` stands for one `"`, and spaces, tabs, `=`, `,` and `;;` are ordinary characters. The quotes themselves are removed.
- A backslash is an ordinary character everywhere, so Windows paths need no escapes.
- `$` is expanded inside quotes too, and `$$` stands for one literal `$`, except on `exec` lines, where `$$` belongs to the shell. This amends S1.07, under which `$$` passed through unchanged (Choice 01.C2). No corpus line contains `$$`.
- A value cannot hold a line break.
- An unterminated quote is an error (`batch.bad_value`).
- Each token records whether part of it was quoted, and where its first unquoted `=` is. That decides named arguments (Choice 01.C1).
- A line whose first token, before expansion, starts with `;;` is a comment. A line of spaces and tabs only is blank, so a tab-only line no longer dispatches the tab as a command (S1.06).
- No non-`exec` token in the corpus contains `"`, so quoting changes no FuSION 1 line.

```
let $IN "C:/Users/Arjun Jeewan/FuSION/inputs"
cudf values=$IN/egf_hela_t120_lfc.tsv up=1.01 down=-1.01
```

The first line defines `$IN` as a path with a space in it. In the second, `values=$IN/egf_hela_t120_lfc.tsv` stays one argument, because expansion never splits a token.

### `exec` lines

- When the expanded first token is `exec`, the rest of the line, from the first character after the blanks that follow `exec`, is taken exactly as written. It is `$`-expanded (longest match, no warnings, `$$` left for the shell) and handed to the shell as one argument (S1.14). Spacing inside shell quotes survives: the real batch's line 244, `exec echo "[egfr-cxcl8-jun] pathz3 wo  FSC  : start $(date)" >> $WRITE_DIR/timing.log`, keeps its double spaces, which FuSION 1 collapsed.
- The shell is a POSIX `sh` on every platform (Choice 01.C10). It runs through section 11's process layer in the batch file's directory, where relative input paths also resolve (Choice 01.C2). In every launch seen, that is FuSION 1's launch directory or a directory that holds only outputs.
- Its stdin is the null device: a batch run never reads stdin (09.C7). Its stdout and stderr are captured, and `fusion` passes every line through its log, so the run log holds the shell's output after the flushed echo (row 1.15, 09.C11; entry 185). The console shows the same lines.
- Its `PATH` is the user's, with FuSION's bundled tools directory appended, so a system gnuplot, as FuSION 1 used, still comes first, and a machine without one finds the bundled build (11.C20; entry 204).
- A non-zero status is a failure (S1.14, S1.28): `error: egfr.batch:153: exec: shell exited with status 1: cat /…/up_reg … [exec.status]`.
- A stop request during an `exec` line ends the shell and everything it started (A1), and the run stops with exit status 3. On resume, that line runs again in full.
- FuSION 1's moves of its fixed-name outputs are handled before the shell sees them (Choice 01.C11, which amends S1.14 for those lines).
- A FuSION 1 window plot, a line that runs `gnuplot -p` without `set term`, is skipped with a note when no display is available: no `DISPLAY` or `WAYLAND_DISPLAY` on Linux, or an SSH session on macOS. With a display it runs as in FuSION 1 (Choice 01.C20, which amends S1.14 and S6.25 for these lines). The corpus has 59 such lines: one in each of 56 `*.batch` files, and one in each GUI script, after `cd $WRITE_DIR;`. Run here with no display, gnuplot 6.0 printed a Qt error and waited until a 20 s timeout killed it (status 124).
- The corpus's other 77 inline gnuplot lines `set term png`. Section 11 checks in Phase 4 that its cairo-only bundled gnuplot accepts that terminal (entries 204 and 205).

### Variables

- **`let <$key> [<value>]`** (S1.08):
  - The key is literal. It is `$` followed by a letter or `_`, then letters, digits and `_`. All 104 distinct keys in the corpus fit.
  - The value is one token, expanded when the `let` runs, so the variable holds a snapshot. A missing value is empty. Extra tokens are an error (`batch.bad_argument`).
  - A second definition of a key is an error that names both lines (`batch.redefined`). A key may not start with `$FUSION_`, which is reserved for the predefined variables (`batch.bad_value`).
  - `let` lines are not echoed.
- **Expansion** (S1.07): at each `$`, the longest defined key that is a prefix of the rest of the text is replaced by its value, and scanning resumes after the value. Keys are held in a byte trie, so a match costs O(length of the key), and a line costs at worst O(its length × the longest key).
  - The real batch has 77 keys and 330 `$` signs. The Python replay expands it in 11 ms here, and in 17 ms in the verifier's run; C++ is expected to take well under a millisecond, which Phase 3 measures. FuSION 1 spent about 3.4 s per pass building 1,000,000-slot token vectors (S1.05).
  - Longest match changes the expansion of 32 lines in 14 corpus files. Twelve are the `runs/real_experiments/` batches with `$PLOT_FILE` and `$PLOT_FILE_WO_GENE` (S1.07). The other two are `auxilliary/comparison_results/batch_baseline.batch` and `batch_constrained.batch`, where `$ACTIVE_W` hid `$ACTIVE_WO`, and likewise `$NONRELAX_WO` and `$ESSENTIAL_WO`. There FuSION 1 wrote and read `…/active_wO` consistently, so only file names change, not the solve's inputs. All 14 files go on decision 0023's list of expected differences.
  - A `$NAME` that matches no key gives the warning `batch.undefined_variable`, except on `exec` lines, where `$` usually belongs to the shell (S1.07). No real corpus batch triggers it; only two placeholder lines of the GUI template do.
- **`#LASTGRAPHNUM`** (S1.09): a `let` value of exactly `#LASTGRAPHNUM` becomes the id of the latest graph. Ids are deterministic, one per graph a line makes (below), so the checker computes them before the run. With no graph yet, it is an error (`batch.bad_value`). The corpus has 116 such lines.
- **Predefined variables** (Choice 01.C18), defined before line 1 and recorded in `run.json`. Their paths use `/` on every platform, so they can go into `exec` lines, where the POSIX shell treats `\` as an escape. No corpus file defines a `$FUSION_` key.

  | Variable | Value |
  |---|---|
  | `$FUSION_RUN_DIR` | absolute path of this run's directory (with a `run` line, defined from the line after it) |
  | `$FUSION_BATCH_DIR` | absolute path of the batch file's directory |
  | `$FUSION_POINT` | the sweep point's label, empty without a sweep |
  | `$FUSION_HOME` | the directory of FuSION's bundled files (S1.20), section 11's home, such as `$FUSION_HOME/data/hsa_to_gene_symbol_map.txt`; the `FUSION_HOME` environment variable overrides it, and `run.json` records the override (11.C20) |

### Commands and arguments

**The command table** (S1.12, Choice 01.C9). One static table lists every command: its name and aliases, owning module, summary, arguments, positional forms, the values it needs and produces, its cross-argument rules, the external tools it needs, its handler and its checker. The table drives dispatch, `help`, `fusion -h`, the argument names of named arguments, the checker, `fusion --commands` (the frontend's block library, S8.19) and the record of bound arguments in `run.json` (E1).
- An argument names its settings-registry key, `<module>.<name>` (`solve.seed`, `gsea.seed`, `network.indirect`), and the registry's `SettingSpec` gives its type, default, range and help (row 1.16, entry 190). On a command's line the argument is the bare name (`pathz3 seed=42`).
- A command declares its tools, with minimum versions and features, such as gnuplot 5.4 with `pngcairo` for `plot_pareto` and a POSIX `sh` for `exec`. Section 11 owns the check (`deps.missing`); 09's check calls it (rows 2A.1 and 2B.15, entry 158).
- `fusion --commands` exports the commands, their arguments with the registry's defaults, the rules, the tool needs and the run-level settings, so the frontend can build blocks and bind settings from it (09.C12, entry 186; row 2B.2, entry 190). This section writes the JSON Schema of that output and of `--check --json`'s, which section 12 keeps (row 2A.10).

**Named and positional arguments** ([decision 0018](../decisions/0018-batch-interface.md), Choice 01.C1):
- A token is named when it starts, unquoted, with one of the command's argument names followed by `=`. The name is literal, and the value after the first `=` is expanded and may be quoted.
- Positional tokens come first, in the order of the command's positional form; named tokens follow, in any order.
- A positional token after a named one, a name given twice, a name already filled by position, a name the command does not have, or a wrong number of positional tokens is an error, `batch.bad_argument`. For an unknown name, the message lists the command's argument names, as S1.11 lists the commands.
- A setting that a step can give itself and that is also run-level, such as `fb_rch`'s `skip_subtypes`, takes the step's own argument for that step, then `fusion --set`, then the `run` line, then the registry default (entry 229).
- An empty value (`coexpression_csv=`, or an empty token from an empty variable) means "not given". An optional argument then keeps its default or turns its feature off (decision 0021); a required one is an error. This is S5.02's rule that a blank keeps only its slot's default.
- No non-`exec` token in the corpus looks like `name=…`, and no `let` value contains `=`, so the rule cannot misread a FuSION 1 line.

**Types.** Every value must be a whole token of its type (S3.02, S5.02). The types are 09's `core::Type`, and the values 09's `core::Value` (row 1.1):

| `core::Type` | Accepts | Example |
|---|---|---|
| `integer` | optional sign and digits, range-checked; a graph id is an integer, and `0` names the latest graph (S2.01) | `bound=10`, `graph=0` |
| `real` | 04.C2's one decimal grammar, `[+-]?([0-9]+(\.[0-9]*)?\|\.[0-9]+)([eE][+-]?[0-9]+)?`, read by `core::parse_decimal` through fast_float (11.C5), locale-free; so no `nan`, `inf`, `0x1p0` or trailing text (entry 167) | `up=1.01` |
| `boolean` | `y`, `yes`, `true`, `1` / `n`, `no`, `false`, `0` | `tool_written=y` |
| `duration` | named form needs a unit: `ms`, `s`, `min`, `h`, `d`; positional FuSION 1 slots take bare milliseconds (Choice 01.C1); stored as whole milliseconds (09) | `timeout=4h` (FuSION 1: `14400000`) |
| `choice` | one of the listed words; positional forms may also list FuSION 1's numbers | `edges_to_target=any` (FuSION 1: `0`) |
| `text` | any token, which the owning module validates: node ids, a selector, a point list | `source=hsa1956`, `selector=fdr:0.25`, `points=7,0;6,1` |
| `input_file` | an input path, relative to the batch file's directory (Choice 01.C2) | `values=$IN/egf_hela_t120_lfc.tsv` |
| `input_list` | an input path whose lines name more inputs, such as a KGML list | `candidates=$IN/list_of_kegg_xml_files.txt` |
| `output_name` | an output path: relative or omitted, it lands in the step's directory; absolute, it is honoured as well, the file also kept in the step directory and both recorded in `run.json` (rows 2B.3 and 4A.11, entry 169) | `up_out=$WRITE_DIR/up_reg` |

Two marks in an argument's `ArgSpec` add what binding needs beyond the type:
- `list`: the argument takes several values, comma-separated in named form (`source=hsa1956,hsa1957`, `cases=in,out`) and as FuSION 1's `-1`-terminated slots in positional form; its value is a list of strings. A point list is one `text` value of `n,e` points joined by `;`, which 06 reads (06.C9); `pex` and `psp` take `all`, `least` or one `<n>,<e>` (S6.03).
- `takes`: the kind of in-memory value the argument names (a network or corridor, observations, a shortlist or results). It defaults to the latest value of that kind (Choice 01.C16), and the check simulates it.

**Positional forms.** Each command lists one or more positional forms, made of slots:
- `one` takes one token;
- `list` takes tokens up to a token that is exactly `-1`; a missing terminator is an error (S4.01);
- `ignored` takes tokens and drops them with a warning (mff's `xml` token and map-file list, S2.16);
- an `optional` tail takes what is left, slot by slot (S5.02);
- a slot may name a "none" token, such as `0` for pex's files and gsea's keep list, or `-1` for `start`'s exception file.

The binder picks the form whose shape fits the line: `mff` with `xml` as its second argument is FuSION 1's form, and otherwise the 2.0 form.

**The commands.** Argument names are the owning sections' (the section is shown); the owner decides each command's arguments and their defaults.

| Command (aliases) | Owner | FuSION 1 positional form, and corpus lines | Named arguments | Value it makes |
|---|---|---|---|---|
| `help` (`h`) | 01 | `help` | `command` | none |
| `let` | 01 | `let <$key> [<value>]`: 8,290 | none | a variable |
| `sweep` | 01 | new (Choice 01.C3) | `<$var> <value>… [label=]`, or `<$var>=<value>… [label=]` | points |
| `run` | 01 | new (Choice 01.C7) | `root`, `label`, `log` and the `run` module's other settings by bare name; any other run-level setting by its key, such as `network.indirect=keep` (row 2B.2) | the run directory's root and label, run-level settings |
| `load` | 01, 09 | new (Choice 01.C6) | `path`, `run`, `step` | the saved values, restored exactly |
| `exec` | 01 | `exec <shell text>`: 3,917 | none | none |
| `exit` | 01 | `exit`: 119; extra tokens are ignored with a warning (S1.13) | none | none |
| `start` | 02 | `start <id_map> <exception_file> <symbol_map> <pathway_name_map>`: 119; optional, with the shipped maps when it is absent and for any file it leaves out (row 2B.4) | `id_map`, `symbols`, `pathway_names` (02); the exception file is positional only, and 2.0 batches give `mff not_merged=` instead (03) | maps |
| `cudf` (`create_up_down_file`) | 04 | `cudf <lfc> <up> <down> <up_out> <down_out>`: 117 | `values`, `up`, `down`, `up_out` and `down_out` (both optional when named, 04.C11), `not_de`, and the reading arguments `id_column`, `value_column`, `id_map`, `repeats` (04.C9) | observations |
| `suggest_thresholds` (`sth`) | 04 | new | `values`, `current_up`, `current_down`, `plot` and the reading arguments (04.C6) | a report |
| `labels` | 02 | new, query only (row 2B.6) | `map`, the shipped symbol map by default | id-label pairs, as JSON |
| `gsea` (`gsea_select_pathways`) | 08 | `gsea <lfc> <kgml_list> <selector> <out_list> [<keep>]`: 19 (16 with 4 tokens, 3 with 5) | `values`, `candidates`, `selector`, `shortlist`, `keep`, and by name only `seed` and `permutations` (08.C11); `kgml_dir` (02); the reading arguments (04.C9) | shortlist |
| `mff` (`merge_from_file`) | 03 | `mff <list> xml <thres> <y\|n> <map>… -1`: 118, all with 6 tokens; in the 116 `*.batch` files, 65 with `y` over the old master (S2.27) and 49 with `n`; 2.0 form `mff <list> <thres> <y\|n>` | `list`, `split_threshold`, `tool_written`, `kgml_dir` (02); `source`, `target`, `fsc`, `confirmed_up`, `confirmed_down`, `not_merged`, `report`, `observations` (03) | network |
| `rgx` (`read_graph_xml`) | 02 | `rgx <file> <y\|n> <thres>`: 116, 114 with `y` and 2 with `n` | `file`, `tool_written`, `split_threshold`, `format` (02) | network |
| `wgx` (`write_graph_xml`) | 02 | `wgx <graph> <file> <tag>`: 118 | `graph`, `out`, `tag`, `embed_sources` (02) | a file |
| `sz` (`size`) | 02 | `sz <graph>`: 445 | `graph` | none |
| `add` | 02 | `add <edges> <out_xml>`: 0 | `edges`, `out` | a file |
| `fb_rch` (`corridor`) | 05 | `fb_rch <graph> <src>… -1 <tgt>… -1 <excl>… -1 <bound> <edges_to_tgt>`: 212, all 8 tokens | `graph`, `source`, `target`, `exclude`, `bound`, `edges_to_target` (`any`, `activation` or `expression`; positionally 0, 1 or 2), `skip_subtypes`, `observations` (05.C8) | corridor (a graph id) |
| `pathz3` (`get_path_with_z3`) | 06 | 28 positional tokens and up to 5 optional ones (S5.01, S5.02): 210 (10 with 28 tokens, 6 with 31, 194 with 33) | `graph`, `fsc` (one or more ids), `cases` (`in`, `out`, `free`; `in,out` by default with an FSC, `free` without), `node_min`, `node_max`, `edge_min`, `edge_max`, `points`, `timeout`, `incremental_timeout`, `budget`, `rlimit`, `seed`, `solutions`, `threads`, `retry`, `answers_from`, `coexpression_csv`, `coexpression_threshold`, `frozen_threshold`, `constraints`, the node and edge lists (06; row 1.7), and `legacy_prefix` (row 1.21) | results |
| `psp` (`plot_solution_paths`) | 07 | `psp <soln> <src> <tgt> <mode> <po> <prefix>`: 0; files bind by prefix (07.C9) | 07's | files |
| `pex` (`plot_explanation`) | 07 | `pex <incl_soln> <incl_relax> <excl_soln> <excl_relax> <lfc> <pathmap> <src> <tgt> <cases> <po> <prefix> [<site> <key> [<png>]]`: 51, all 11 tokens; files bind by prefix (07.C9); FuSION 1's `both`, `incl` and `excl` stay positional `<cases>` tokens only | 07's, such as `solve` (the latest by default), `cases` (every solved case by default) and `points` | files |
| `plot_pareto` | 07 | new, the Pareto plot step (S6.22) | `solve` (one or more solves; the latest by default) and `font` (07) | a file |
| `upfx` (`unique_prefix`) | dropped (S2.29) | `upfx <prefix>`: 0 | none | an error |

New command names collide with nothing: no corpus line starts with `sweep`, `run`, `load` or `labels`, and none is one of section 09's reserved directory names (`inputs`, `debug`, `points`, `overrides`). A dropped command is an unknown command (S1.11, S2.29), and its error says why it was dropped: "upfx was dropped in FuSION 2.0; each run has its own directory (S2.29)". `pathz3`'s positional mode `1` is an error, because S5.28 dropped it; the corpus has none.

**Values between steps** (D1). The interpreter keeps a `Workspace`:
- the maps, 02's `Maps`: the files `start` names, and the shipped symbol and pathway-name maps for any it leaves out or when there is no `start`, in which case there is no id map (row 2B.4, entry 163);
- a `GraphStore` of networks and corridors by integer id, where `0` is the latest (S2.01). Each line that makes a graph takes exactly one id: `rgx`, `mff`, `fb_rch`, and `load <file>` of a network or corridor. `load run=… step=…` takes one id for each network and corridor it restores, in the order the earlier run made them, so ids stay deterministic and the check predicts them. Merge intermediates, FuSION 1's forward graph (S4.08) and pathz3's undirected copy (S5.08) take none. In the real batch, `mff` makes graph 1 and `rgx` graph 2, so `#LASTGRAPHNUM` gives 2 where FuSION 1 gave 84, and the two `fb_rch` corridors are 3 and 4. The corpus never names a graph id other than through `0` or a variable: `fb_rch` has 116 lines with `0` and 96 with a variable, and `pathz3`, `sz` and `wgx` always use `0`;
- the latest observations (`cudf`), shortlist (`gsea`, 08's `Shortlist`) and solve results (`pathz3`, 06's `solve::Results`). After a solve step returns or stops, the batch layer calls 07's `publish_solve`, which builds the published results from them (row 1.5, entry 177).

An argument that takes one of these kinds defaults to the latest value of that kind (Choice 01.C16). So `mff split_threshold=1` merges the shortlist that the preceding `gsea` chose, `fb_rch` takes the latest `cudf` observations (05.C9), and `pex` draws the latest results. A line whose needed value does not exist yet, such as `pathz3` before any corridor, is a check error (`batch.bad_argument`).

**What merging keeps apart** (S1.17, S2.13, S8.22; Choice 01.C17). The exception set is the not-merged file, the up and down genes, the confirmed up and down genes (03.C2, which amends S8.22; entry 165), the source, the target and the FSC.
- The up and down genes come from the latest `cudf` observations when `mff` runs (03.C1). A gene marked Not DE is removed in `cudf`, before classification, so it reaches neither the lists nor the exception set (04.C4, which narrows S3.08; entry 165). All 114 corpus batch files that call `mff` run `cudf` before it. A batch that merges before any `cudf` gets the warning `batch.mff_before_cudf`.
- The source, the target, the FSC and the confirmed lists appear only on later `fb_rch` and `pathz3` lines. Because the whole file is bound before anything runs, the checker collects them for each point (`BatchFacts`). Section 03's `mff` arguments `source`, `target`, `fsc`, `confirmed_up` and `confirmed_down` default to them; an explicit argument wins (row 3A.1, entry 165).
- A positional FuSION 1 batch names no FSC on any line: its FSC sits in the list files that its `exec` lines build. Such a batch keeps the exception file its `start` line names, which reproduces FuSION 1, and the check's one note, `batch.no_fsc` (row 2A.9, entry 230), says that the FSC is unknown and that naming it with `fsc=` applies S2.13 (03 open question 1, entry 165).

**Files written earlier in the run** (S2.26, Choice 01.C13). When a step writes a file that a later line may read, the interpreter keeps the value with the file's identity and SHA-256. A later input that names the same, unchanged file gets the value from memory. So `rgx` of the merged graph that `wgx` just wrote applies 02's normalisation to the in-memory network instead of parsing it, and `pathz3` reads `cudf`'s lists from memory. Positional `pex` and `psp` lines do not go through files: their file arguments bind by prefix to the solve that wrote them, and a file that matches no solve's prefix is an error before the run, `batch.unmatched_file` (07.C9, entry 179).

### Checking before anything runs

Every `-b` run starts with the full check ([decision 0021](../decisions/0021-required-and-optional-inputs.md)), and `--check` runs only the check, every check a real run makes, without a run directory and without copying inputs (09.C12, entry 186). For each point it:
1. lexes every line, then evaluates `let`, `sweep` and `run` lines in order, binds each command line, and simulates graph ids and value kinds;
2. calls each command's checker. The checker reports problems, such as `cudf`'s thresholds with up not above down (S3.07), and which optional features its inputs turn on, such as coexpression constraints because a CSV and a threshold are given. These features are 0021's "mode of solve, settled before it starts", and they are printed before the run;
3. checks every input. In a real run 09 copies and hashes each input into `inputs/` first, and the check parses the copy (09.C3, row 1.13; entry 184); `--check` reads the originals. A path is produced in the batch when an earlier line declares it as an output, or when its text occurs in an earlier `exec` line. A command's checker declares every file it will write, derived names included, such as a positional `pathz3`'s `<prefix>_solutions.txt`. A produced input is checked, copied and hashed just before its step runs (Choice 01.C12, which amends decision 0021, S1.15 and S2.16 for such inputs). Every other input must exist, be a regular file and be readable before anything starts;
4. checks the tools each command line needs, such as gnuplot 5.4 with `pngcairo` for `plot_pareto` and `sh` for `exec`, through section 11's `core::deps::Registry` (`deps.missing`), probing each tool once (rows 2A.1 and 2B.15);
5. checks that no two outputs clash, by file identity, across steps and across points (09's check, `output.clash`; row 2A.7);
6. reports every problem at once, each with its code (see Errors), sorted by point and line. Any error ends `fusion` with status 1 before a step runs (09.C14); warnings are printed, and the run goes on.

`--check --json` prints the problems as `diagnostics`, each with `severity`, `line`, `column`, `point`, `code`, `command`, `setting`, `path` and `message`, plus `features`; the C++ type is 09's `core::CheckReport` (row 1.9, entry 158).

Cost: simulating a point is linear in its lines; each distinct path is checked once with one `stat` and one open, and the results are cached across points. `mff`'s list adds one check per listed file: 167 files and 6,853,555 bytes for the real batch's candidate list. Section 09 measured 17 ms for the whole check of the reference run's 174 input files.

### Running a point

The interpreter runs the lines in order:
- A comment or blank line is skipped. A `let` line defines its variable.
- A command line is echoed (S1.10), and `step.started` is emitted. Any produced input of the line is checked, copied and hashed. Then the handler runs, with 09's `core::Context`.
- **Failure** (S1.28): the handler throws a `core::Failure`, or an `exec` exits non-zero. The interpreter catches it once and calls 09's `Run::fail`: the message goes to stderr with the file and line, an `error` event and `step.finished` carry its code and message, and the run ends with status `failed` and exit status 1 (09.C14).
- **Reported result** (S4.18, S7.03): the handler's `core::Outcome` carries a `core::Report`. There are three: `corridor.empty` (`fb_rch`), `gsea.empty_selection` (`gsea`) and `observations.none` (`cudf` with no gene past the thresholds, 04.C5) (rows 3A.14 and 1.8, entry 167). The message, the owner's, is printed (`egfr.batch:191: result: no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3: the shortest path has 4 edges`, from 05), and `step.finished` has status `reported` and the `report`. Every later line of that point is then skipped, each with a `step.skipped` event (Choice 01.C15). The run's status is `ok`, its `run.json` lists the report, and it exits 0.
- **Case statuses** are not reports. A case with no point (S6.21), an infeasible case (S5.11), no DE gene in the corridor (06.C15) and an exhausted whole-solve budget (06.C3) are statuses inside the solve's results, and the batch, the Pareto plot and the pages go on (entry 167).
- **Stop** (A1): the `stop` file appears in the run directory, which 09's watcher checks every 0.5 s (09.C7), or a signal handler sets the same flag (09.C8). The step throws `core::Stopped` at a safe point, with its work journaled (06, 09.C6), and the run ends with status `stopped` and exit status 3. If the main thread has not ended 30 s after the request, 09's watcher records `forced: true` and calls the `fusion` executable's grace hook, which ends the process with status 3.
- The end of the file, or `exit`, ends the run with status `ok` (S1.04, S1.13).

Statuses follow section 09: a step ends `ok`, `reported`, `skipped`, `failed`, `stopped` or `loaded`, and a run `ok`, `failed` or `stopped`.

### Sweeps

A batch with `sweep` lines runs once per point, as if the file had been cloned with each point's values (Choices 01.C3, 01.C4, 01.C5).

```
;; GSEA cut-off sweep: replaces the four cloned files and run_sweep.sh of
;; runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/
run root=/home/ks-149-u24/FuSION2-runs label=egfr_cxcl8_jun_b10_fdr_sweep
sweep $FDR 0.25 0.3 0.4 0.5 label=fdr
let $IN /home/ks-149-u24/FuSION2-runs/inputs
cudf values=$IN/egf_hela_t120_lfc.tsv up=1.01 down=-1.01
gsea values=$IN/egf_hela_t120_lfc.tsv candidates=$IN/list_of_kegg_xml_files.txt selector=fdr:$FDR shortlist=gsea_kgml_files.txt
mff split_threshold=1 not_merged=$IN/hsa_ids_not_to_be_merged.txt
fb_rch source=hsa1956 target=hsa3576 bound=10
pathz3 fsc=hsa3725 node_max=8 edge_max=8 timeout=4h coexpression_csv=$IN/hsa_stringdb_coexpression_links_over400.csv coexpression_threshold=700 frozen_threshold=800
plot_pareto
pex
```

Rows set several variables together, as the bound and FSC runs did. The `label` comes last, because it is a named argument (Choice 01.C1):

```
sweep $FSC=hsa3725 $BOUND=10 $TIMEOUT=4h    label=jun_b10
sweep $FSC=hsa3725 $BOUND=6  $TIMEOUT=50min label=jun_b6
sweep $FSC=hsa1843 $BOUND=10 $TIMEOUT=4h    label=dusp1_b10
sweep $FSC=hsa2354 $BOUND=10 $TIMEOUT=4h    label=fosb_b10
```

- Points are numbered in product order, with the first dimension outermost. A point's label is the `label` given, or else built from its values, such as `fdr0.25`. Labels are reduced by row 1.26's rule, which section 09 states: per Unicode code point, each run of characters outside `[A-Za-z0-9._-]` becomes one `-`, and leading and trailing `-` are trimmed. They are then cut to 32 characters. A point runs in `points/<NN>-<label>/` inside the sweep's directory (09.C2), such as `points/01-fdr0.25/`. The number keeps directories unique and avoids Windows' reserved names (`CON`, `NUL`, `COM1` …) and collisions on case-insensitive file systems.
- Each point is a complete run, with its own run directory, `run.json`, `events.jsonl`, saved steps and run log. `$FUSION_RUN_DIR` names the point's directory.
- The coordinator writes `points.tsv` in the sweep's directory and keeps it up to date:

  ```
  index	label	$FDR	status	reports	exit	wall_s	dir
  1	fdr0.25	0.25	ok		0	658.1	points/01-fdr0.25
  2	fdr0.3	0.3	ok		0	758.68	points/02-fdr0.3
  3	fdr0.4	0.4	ok		0	4166	points/03-fdr0.4
  4	fdr0.5	0.5	ok		0	182534	points/04-fdr0.5
  ```

  The wall times are the four FuSION 1 runs' (`run_time.txt`), printed in their shortest exact form (S1.27). A `status` is `pending` or `running`, or one of 09's run statuses: `ok`, `failed` or `stopped`. `reports` lists the codes of the point's reported results, such as `corridor.empty`.
- A reported result ends only that point, with status `ok` and exit 0, and the other points carry on (S4.18, S7.03). A failure ends its point with exit 1; Choice 01.C5 settles what happens to the others.
- The sweep exits 0 when every point ends `ok`, 1 when any point failed, and 3 when it was stopped.

### Partial runs

The four partial runs of [decision 0019](../decisions/0019-partial-runs.md) map onto the language like this (Choice 01.C6):

| Partial run | How |
|---|---|
| stop after any step | put `exit` after it. Every step's output is saved in its step directory as the step ends (09.C4), so nothing else is needed |
| start from a saved step | `load <file>` restores one saved value exactly as saved. `load run=<dir> step=<step>` restores everything that was current after that step of an earlier run: maps, observations, networks, corridors, shortlist and results (row 2A.4, entry 164). `rgx` of a saved network file compares its recorded settings and exception set with the run's and rebuilds when they differ (02); `mff` has no `from_saved` argument |
| resume a stopped solve | `fusion --resume <run dir>` starts a new run whose parent is that run, on the parent's own batch copy (09.C5). Steps whose fingerprints match a step the parent finished are loaded, the stopped solve continues from its journal (06), and later lines run. `fusion -b <corrected batch> --resume <run dir>` does the same with a corrected batch, which is S1.28's restart from the last good step. `exec` lines run again, because a shell line has no fingerprint |
| solve part of the frontier | `pathz3`'s `cases` (`in`, `out`, `free`), the box (`node_min`, `node_max`, `edge_min`, `edge_max`), and `points=<n>,<e>[;…]`, which asks only the listed points, in one context, with up to `solutions` witnesses each and no search (06.C9, entry 175) |

`step` names a step of the earlier run as section 09 names step directories (`fb_rch`, or `fb_rch-2` for a second `fb_rch`), or by its batch line (`step=229`). For each kind of value, `load` takes the latest artefact up to that step from the run's `run.json` through 09's `Registry`, which verifies its SHA-256 and copies it like any other input (09.C3). The check fails when the run, the step or an artefact is missing, unfinished or changed.

The 2026-11-11 re-solve of one point of `r5_mcl1_stat3` took a 124-line batch, with 91 command and `let` lines, which rebuilt the 87-pathway merge (22 s) and both corridors before its one 454 s query (`resolve_wo_n7_e0/run_console.log`, `run/timing.log`). In 2.0 it is three lines:

```
load run=/home/ks-149-u24/FuSION2-runs/r5_mcl1_stat3_20261109-195056 step=fb_rch
pathz3 fsc=hsa6774 cases=out points=7,0 timeout=24h avoid_edges=stat_to_mcl1_edges.txt
pex cases=out points=7,0
```

### Naming the run directory

Section 09 names a run directory `<label>_<YYYYMMDD-HHMMSS>` in local time inside a root, with `-2`, `-3` on a collision, created atomically and never deleted or reused, and records a separate run id, `r-<UTC time>-<8 hex digits>`, in `run.json` (09.C1, S8.21). This section decides where the label and the root come from (Choice 01.C7):
- a `run` line, before the first step line: `run root=/home/u/fusion-runs label=egfr_cxcl8_jun_b10`. Its values may use variables defined above it;
- on the command line, `--root` and `--label`, which override the `run` line;
- by default, the label is the batch file's stem and the root is the batch file's directory.

`--run-dir <dir>` adopts a directory instead, as the frontend does (A1): `fusion` claims it with 09's `run.lock` and refuses one that has already been used. The frontend's label is the session's name, with the swept value appended for a sweep, and it records the session id with `--tag session=<id>` (10.C18, entry 184). `$FUSION_RUN_DIR` holds the directory's absolute path from the line after the `run` line, or from line 1 when there is none. In a sweep, the directory belongs to the sweep, and each point runs in `points/<NN>-<label>/` inside it (09.C2).

**Run-level settings** (rows 1.16 and 2B.2, entry 190). The `run` line also carries the batch's run-level settings: the `run` module's by bare name (`log`, `cache`, `cache_dir`, `jobs`), and any other by its key (`run network.indirect=keep corridor.skip_subtypes=component,association,dissociation`). `fusion --set <key>=<value>` overrides the `run` line, and `run.json` records each value with its source. Entry 229 settled two more points:
- `debug` is the one run-level setting the `run` line does not take. Debugging stays on the command line (Choice 01.C8), and a `run` line that names `debug` gets `batch.bad_argument`, as any line does for an argument it does not take.
- Precedence: a step's own argument wins for that step, then `fusion --set`, then the `run` line, then the registry default. So two `fb_rch` lines that set `skip_subtypes` differently keep both, and `--set corridor.skip_subtypes=…` changes only the `fb_rch` lines that do not set it.

### The debug option

`--debug` on the command line is the one debug option of S1.29 (Choice 01.C8). It sets section 09's `debug` setting, which is off by default, and the frontend never passes it. It turns on:
- debug records in the run log, which is always written and holds every console line and the output of `exec` lines (09.C11, entry 185). The log's per-run name is 09's `log` setting, given by `run log=<name>` or `--log <name>` and fixed before any line runs, which is S1.25's "set before `start`";
- the corridor traces (S4.19);
- the SMT dumps (S5.23), which no step ever reads back (06.C12);
- the modules' self-checks that are too slow for normal runs (FuSION 1's `ASSERT_FLAG`).

It changes no result. `run.json` records it, and sweep points inherit it.

### FuSION 1 batch files

FuSION 1 batch files run unchanged (decisions 0017, 0023). The replay shows what that takes:

| FuSION 1 construct | In 2.0 | Corpus |
|---|---|---|
| positional lines of the 11 commands used | bound through each command's FuSION 1 form | 1,644 non-`exec` command lines, besides five lines of the GUI template that start with its placeholder `%__c__%` |
| `start` with the batch's own exception file | read as before; it replaces the shipped maps, and its exception file keeps FuSION 1's set (S1.17) | 119 lines |
| `mff <list> xml <thres> <y\|n> <map> -1` | accepted; the `xml` token and map list are ignored with a warning (S2.16) | 118 lines |
| a one-file `mff … y` over the old master (S2.27), then `exec mv edges_merged_filename …` | the legacy read; a one-file `mff` merges nothing and has no merge report (S2.16), so the move writes nothing, with a note (Choice 01.C11) | 65 batch files |
| `rgx <file> <y\|n> …` | accepted (S2.12) | 116 lines, 114 with `y` |
| `let … #LASTGRAPHNUM` | latest graph id; ids are numbered afresh | 116 lines |
| keys that are prefixes of other keys | longest match changes expansions (S1.07) | 32 lines in 14 files |
| absolute output paths, such as `cudf`'s `$WRITE_DIR/up_reg` | honoured, and the file also kept in the step directory (row 2B.3), so later `exec` lines find it | every batch |
| `exec mv <fixed name> <dest>` | written natively (Choice 01.C11) | 879 lines in all 119 files |
| `gnuplot -p` window plots without `set term` | skipped with a note when no display is available (Choice 01.C20) | 59 lines in 59 files |
| every other `exec` line | POSIX shell, status checked (S1.14), output in the run log, bundled tools last on `PATH` | 2,979 lines |
| empty tokens from empty variables | "not given" for that slot (S5.02) | 16 in `Linux/output_script.txt` |
| a positional `pathz3` | one solve, as the lists say. Its `<prefix>` sets `legacy_prefix`, so 07 writes the prefix's FuSION 1 text files in `pathz3/legacy/` and, the prefix being absolute, at the prefix's own path (row 1.21) | 210 lines |
| a positional `pex` | its files bind by prefix to the solves that wrote them (07.C9); none is parsed back | 51 lines |

What the 3,917 `exec` lines do, by their first commands after expansion:

| Job | Lines | Files | Done in 2.0 by |
|---|---|---|---|
| build an input file (`echo`, `cat`, `printf` into a file) | 1,481 | 119 | the backend's own lists and exception set (S1.17, S5.04, S5.17, S8.22) |
| timing (`date`, `awk`, `echo >> timing.log`) | 910 | 110 | events and `run.json` |
| move a fixed-name output | 879 | 119 | the run directory (S1.26); Choice 01.C11 for old lines |
| Pareto plot (`gnuplot`, `plot_po.sh`) | 165 | 94 | the built-in plot step (S6.22) |
| remove a stale file before rewriting it (`if [ -f … ] ; then /bin/rm …; fi`), 78 of them an avoid file | 152 | 81 | new run directories (S8.21) |
| `touch empty_file` | 119 | 119 | an optional input left out (0021) |
| `mkdir -p` the write directory | 117 | 117 | the run directory |
| `echo` to the console | 90 | 78 | console, run log and events |
| other: the three GUI scripts' no-op `exec cd …;` (S8.22) and `exec add` (S8.24) | 4 | 3 | the built-in `add` |

So a 2.0 batch needs no `exec` at all, and the frontend never writes one (S8.22, S8.26, S8.27). `exec` stays for old batches and for power users. 396 of the timing lines use `date +%s.%N`, which macOS's BSD `date` does not support, and 115 of the 116 corpus `*.batch` files hard-code `/home/…` paths in `let` lines. The one exception is `macOS/model_example/run_model_example.batch`, which has `/ABSOLUTE/PATH/TO` placeholders. Old batches therefore run where they were written; on another machine their paths need editing first.

The real batch in 2.0, line by line:
- line 94, `exec mkdir -p …`, and lines 151 to 154, which build `src_file`, `tgt_file`, `connect_pairs`, the exception file and `empty_file`: through the shell, with their output in the run log;
- line 145, `cudf`: FuSION 1's form. It makes the observations and writes `up_reg` and `down_reg` at the batch's absolute paths and in `cudf/` (row 2B.3), so line 153's `cat` finds them;
- line 160, `start`: FuSION 1's form. It reads the exception file that line 153 built, a produced input that is checked, copied and hashed just before line 160 runs;
- line 169, `gsea`: makes the shortlist and writes `gsea_kgml_files.txt`;
- line 171, `mff … xml 1 n … -1`: warns about `xml` and the map list, merges the shortlist from memory, and makes graph 1;
- line 174, `wgx`: writes `merged_master_graph.xml` and remembers it;
- line 176, `exec mv edges_merged_filename …`: writes the merge report (Choice 01.C11);
- line 182, `rgx … y 1`: finds the file `wgx` wrote, unchanged, normalises the in-memory network (S2.26), and makes graph 2;
- line 185: `$MERGED_GRAPH_NUM` becomes `2`;
- lines 191 and 227, `fb_rch`: make corridors 3 and 4;
- lines 214 and 247, `pathz3`: FuSION 1's form, one solve each, named by their prefixes `$WRITE_DIR/temp_w` and `temp_wo`. Their prefixes set `legacy_prefix`, so `temp_w_solutions.txt` and the other text files appear in `pathz3/legacy/` and `pathz3-2/legacy/` and in `$WRITE_DIR`;
- lines 220, 221, 253 and 254, the moves of `limits_timefile.txt` and `PO.dat`: write FuSION 1's limits rendering and the frontier of the solve just before;
- lines 230 and 231, the moves of `fb_rch` and `fb_rch_back`: notes only, unless `--debug`;
- line 262, `plot_po.sh`: through the shell;
- line 269, `pex`: FuSION 1's form; its four files bind by prefix to the two solves (07.C9);
- line 271, the move of `mylog.txt`: a note only, unless `--debug`;
- line 273, `exit`.

The run gives one warning (line 171, `batch.ignored_tokens`), the check's note `batch.no_fsc` that the batch names no FSC (row 2A.9), and eight move notes. Each echoed line keeps FuSION 1's format, and differs only where 2.0 differs: the 450 leading spaces are gone (S1.24), the second `fb_rch` reads `fb_rch 2 …` where FuSION 1 printed `fb_rch 84 …`, and `exec` lines keep their own spacing (line 244).

### The frontend's batch files

The frontend writes a batch file for every Submit (S8.18 to S8.20) and starts `fusion -b <run dir>/run.batch --run-dir <run dir> --tag session=<id>` in the run directory, with its console discarded (A1, 10.C17, 10.C18). A sweep drawn in the editor runs as one run per value, and only an exported batch uses the `sweep` syntax (10.C21). The default pipeline (S8.25 to S8.27) can be written like this. It has no `start` line, because the shipped maps are the default (row 2B.4), and its `pex` draws every solved case:

```
;; FuSION 2.0 default pipeline, written by the frontend (session 7f3c2a, run 12)
let $IN "C:\Users\Arjun Jeewan\FuSION\inputs"
cudf values=$IN\egf_hela_t120_lfc.tsv up=1.01 down=-1.01
gsea values=$IN\egf_hela_t120_lfc.tsv candidates=$IN\list_of_kegg_xml_files.txt selector=fdr:0.25 shortlist=gsea_kgml_files.txt
mff split_threshold=1 not_merged=$IN\hsa_ids_not_to_be_merged.txt
fb_rch source=hsa1956 target=hsa3576 bound=10 edges_to_target=any
pathz3 fsc=hsa3725 node_max=8 edge_max=8 timeout=4h coexpression_csv=$IN\hsa_stringdb_coexpression_links_over400.csv coexpression_threshold=700 frozen_threshold=800 seed=42
plot_pareto
pex
```

With an FSC, `pathz3` solves the cases `in` and `out` by default (06.C10), as S8.25 asks. Rules for the batch files the frontend writes (section 10 owns the generator):
- arguments are named;
- input paths are absolute and may be native, because backslashes are ordinary characters (Choice 01.C2); outputs are left out where the command allows, or given as bare file names, such as `gsea`'s required `shortlist` (08), so they land in the step directories (row 2B.3);
- every `$` in a value is written `$$`, quoted or not, because `$` is expanded everywhere; a value is quoted when it holds a space, a tab or a `"`, or is empty, and inside the quotes a `"` is written `""` (Choice 01.C2);
- no `exec` lines;
- `fusion --check --json` validates the file before submitting, with exactly the backend's rules (0021, S8.30, 09.C12);
- `fusion --commands` supplies the block library, the cross-argument rules and the run-level settings (S8.19, Choice 01.C9);
- gene lookups and the threshold preview go through `fusion --query`: the exact `cudf` or `suggest_thresholds` line the batch will hold, or 02's `labels [map=<file>]` line (10.C15, row 2B.6).

### The `fusion` command line

| Option | Does |
|---|---|
| `-b <file>` | checks, then runs a batch file (S1.01) |
| `-h [<command>]` | usage and the command listing, or one command's arguments, on stdout; exit 0 (S1.02) |
| `--check` | runs every check a real run makes; runs no step, creates no directory and copies no input (09.C12) |
| `--json` | with `--check` or `--version`: machine-readable output on stdout |
| `--root <dir>`, `--label <text>`, `--log <name>` | where the run directory goes, its label, and the run log's name (Choice 01.C7, 09.C1, 09.C11) |
| `--set <key>=<value>` | sets a run-level setting, overriding the `run` line (row 2B.2) |
| `--run-dir <dir>` | adopts a directory that the frontend made (A1, 09) |
| `--tag <key>=<value>` | a label recorded in `run.json`, such as `session=<id>` (09's `labels`, 10.C18) |
| `--resume <run dir>` | a new run whose parent is that run (09.C5); with `-b`, on a corrected batch; with `--run-dir`, in a directory the frontend made (Choice 01.C6, 10) |
| `--stop <run dir>` | creates the `stop` file in a running run's directory (09.C7) |
| `--point <i,j,…>` | runs only these sweep points, by number or label |
| `--jobs <n>` or `auto` | the core budget for sweep points, one core per case (default `auto`, Choice 01.C4) |
| `--debug` | the one debug option (Choice 01.C8) |
| `--query` | reads one batch line on stdin and writes one JSON answer on stdout; the same library functions as the batch command, but no files, no run directory and no events, with log lines on stderr; only commands that need no run directory (entry 168) |
| `--commands` | prints the command table as JSON |
| `--version` | the versions as `run.json` records them, and, with `--json`, what section 11 lists: the home directory, the bundled files checked against their hashes, and the external tools |

`--check`, `--commands`, `--version`, `--stop` and `--query` make no run directory and fall outside A1, which governs how runs are executed (row 4A.2, entry 168).

*Phase 3 (prompt log entry 387):* `--stop` writes the `stop` file only into a directory holding a `run.lock` that a live run holds (09.C9).
- **Refused:** a directory without a `run.lock` is not a run directory, and a run whose lock is free has ended. Both exit with status 1, and nothing is written.
- **A file system that cannot lock:** the file is written, with a note that liveness could not be checked.

Exit statuses (Choice 01.C14):

| Status | Meaning |
|---|---|
| 0 | the batch ran to its end, reported results included; in a sweep, every point did |
| 1 | a failure (09.C14): a batch syntax or check error, an install or dependency error at startup (`install.*`, `deps.*`, through 09's `error` event; row 2B.16), an unusable run directory, a failing step, an internal error, or a failed sweep point |
| 2 | a usage error: an unknown or incomplete option, or a `-b` file that is missing, unreadable or empty (S1.01, S1.02); nothing is created |
| 3 | stopped on request (A1, 09.C7); `--resume` continues |
| 130 | a second Ctrl-C ended the process at once (09.C8) |

In FuSION 1 a killed run cannot be continued, because FuSION 1 has no resume (decision 0019 asks for one): the eight runs of `runs/egfr_b10_gsea_fdr044_candidates/run_fsc_tests.sh` all ended with status 143 (SIGTERM) on 2026-11-12 at 18:06:28, 61 minutes in (`status.log`). In 2.0, SIGTERM and Ctrl-C request the same clean stop (09.C8), so such a run ends with status 3 and can be resumed.

`fusion -h` prints, on stdout:

```
Run as "fusion -b <batch_file>"

Options:
  -b <file>              check, then run a batch file
  -h [<command>]         this help, or one command's arguments
  --check                check the batch file and its inputs; run nothing
  ...

Commands:
  help (h)               [<command>]
  let                    <$name> [<value>]
  sweep                  <$name> <value>... [label=<prefix>] | <$name>=<value>... [label=<text>]
  ...
  fb_rch (corridor)      <graph> <source>... -1 <target>... -1 <exclude>... -1 <bound> <edges_to_target>
                         graph= source= target= exclude= bound= edges_to_target= skip_subtypes= observations=
  ...
```

`fusion -b egfr.batch --check --json` prints:

```
{"ok": false, "batch": "/home/u/exp/egfr.batch", "sha256": "5f0c…", "points": 1,
 "diagnostics": [
  {"severity": "error", "line": 160, "column": 7, "point": null, "code": "input.missing",
   "command": "start", "setting": "symbols", "path": "/home/u/exp/inputs/hsa_to_gene_symbol_map.txt",
   "message": "start: symbols file /home/u/exp/inputs/hsa_to_gene_symbol_map.txt does not exist"},
  {"severity": "warning", "line": 171, "column": 23, "point": null, "code": "batch.ignored_tokens",
   "command": "mff", "setting": null, "path": null,
   "message": "mff: FuSION 1's 'xml' token and map-file list are ignored"}],
 "features": [
  {"line": 214, "command": "pathz3", "feature": "coexpression", "on": true,
   "because": "a CSV and a threshold are given"}]}
```

On the console, the same diagnostics read `error: egfr.batch:160: start: symbols file … does not exist [input.missing]`, in section 09's form (09.C14).

### C++ interface outline

The outline uses section 09's vocabulary (row 1.1, entry 158): `core::Failure` as the base exception, with `core::UsageError` and `core::InstallError` derived from it; `core::Report` and `core::Outcome<T>` for step results; `core::Stopped`; `core::Log` and `core::Events`; `core::Context` for every step; `core::Sha256`; one `core::stable_compare`; and the registry's `core::SettingSpec`, `core::Type`, `core::Source` and `core::Value`.

`libfusion`, module `batch`:

```cpp
namespace fusion::batch {

// --- source and lexing (S1.01, S1.05, S1.06, S1.14) ---
struct SourceFile {
  std::filesystem::path path;            // absolute
  core::Sha256 sha256;                   // of the bytes read
  std::vector<std::string> lines;        // split on '\n', one '\r' dropped, BOM skipped
};
SourceFile read_source(const std::filesystem::path& file);    // throws core::UsageError (exit 2)

struct Token {
  std::string text;                      // quotes removed, '$' not yet expanded
  bool quoted = false;                   // some part was inside "..."
  std::optional<std::size_t> name_end;   // unquoted '=' after [a-z][a-z0-9_]*
  int column = 0;
};
enum class LineKind { blank, comment, command };
struct Line {
  int number = 0;                        // 1-based
  LineKind kind = LineKind::blank;
  std::vector<Token> tokens;             // all tokens; unused once the expanded first token is "exec"
  std::string rest;                      // the text after the first token, as written; used for exec
  std::optional<int> unterminated_quote; // column; an error unless the line turns out to be exec
};
Line lex(int number, std::string_view text);   // knows no variables: exec is decided after expansion

// --- variables (S1.07 to S1.09) ---
class Variables {
 public:
  bool define(std::string key, std::string value, int line);  // false: already defined
  enum class Mode { argument, exec };   // exec: "$$" kept for the shell, no warnings
  struct Expansion { std::string text; std::vector<std::string> unmatched; };
  Expansion expand(std::string_view text, Mode mode) const;   // longest match, one pass
  std::vector<std::tuple<std::string, std::string, int>> in_order() const;  // for run.json
 private:
  struct TrieNode;
  std::unique_ptr<TrieNode> root_;
};
}
```

The command table and binding:

```cpp
namespace fusion::batch {
enum class ValueKind { maps, network, corridor, observations, shortlist, results };

struct ArgSpec {
  std::string_view name;                 // the bare name on a command line, snake_case
  std::string_view setting;              // registry key "<module>.<name>": its core::SettingSpec
                                         // gives the core::Type, need, default, range and help (row 1.16)
  bool list = false;                     // several values: commas when named, -1 slots when positional
  std::optional<ValueKind> takes;        // names an in-memory value; the latest by default (01.C16)
};
enum class SlotKind { one, list, ignored, optional_tail };
struct Slot {
  SlotKind kind;
  std::string_view arg;                  // empty for ignored slots
  std::string_view none_token;           // "0" or "-1" where FuSION 1 used one
};
struct PositionalForm {
  std::string_view origin;               // "FuSION 1" or "2.0"
  std::span<const Slot> slots;
  bool (*fits)(std::span<const std::string> tokens);   // e.g. mff: tokens[2] == "xml"
  std::string_view warning;              // printed when this form is used
};
struct Effects {
  std::span<const ValueKind> needs;      // must exist, or be loaded, before the line
  std::optional<ValueKind> produces;
  bool creates_graph = false;            // takes the next graph id
  bool needs_run_dir = true;             // false: allowed in --query
};
enum class RuleKind { less, less_or_equal, requires, excludes };
struct Rule { RuleKind kind; std::string_view a, b; };   // e.g. less(down, up), S3.07

struct BoundArgs;  struct Workspace;  class CheckContext;

struct CommandSpec {
  std::string_view name;
  std::span<const std::string_view> aliases;
  std::string_view owner;                // C1 module
  std::string_view summary;
  std::span<const ArgSpec> args;
  std::span<const PositionalForm> forms;
  Effects effects;
  std::span<const Rule> rules;                    // enforced, and exported by --commands
  std::span<const core::deps::Requirement> tools; // 11's tool needs (rows 2A.1, 2B.15); a checker
                                                  // adds a need that depends on arguments
  core::Outcome<void> (*run)(const BoundArgs&, Workspace&, core::Context&);   // a Report skips
                                                                            // the point's later lines
  void (*check)(const BoundArgs&, CheckContext&);
  std::string_view dropped;              // non-empty: dropped in 2.0 (upfx, S2.29); the error's reason
};
class CommandTable {
 public:
  const CommandSpec* find(std::string_view name_or_alias) const;  // exact (S1.11)
  std::span<const CommandSpec> all() const;
  void print_listing(std::ostream&) const;                       // help, -h, unknown command
  void print_command(std::ostream&, const CommandSpec&) const;   // -h <command>
  nlohmann::json describe() const;      // --commands: arguments with registry defaults, rules,
                                        // tools and run-level settings
};
const CommandTable& command_table();

struct BoundValue {
  const ArgSpec* spec = nullptr;
  core::Source source;                   // default, positional or named (E1)
  bool given = false;                    // false: left out, or given empty
  std::string text;                      // after expansion, for the echo and run.json
  core::Value value;
};
struct BoundArgs {
  const CommandSpec* command = nullptr;
  int line = 0;
  std::vector<BoundValue> values;        // one per ArgSpec, in table order
  const BoundValue& at(std::string_view name) const;
};
BoundArgs bind(const CommandSpec&, const Line&, const Variables&, core::CheckReport&);
}
```

Planning, checking and running:

```cpp
namespace fusion::batch {
struct Point {
  int index = 1;                         // 1-based, product order
  std::string label;                     // "fdr0.25"; empty without a sweep
  std::string dir;                       // "points/01-fdr0.25"; empty without a sweep
  std::vector<std::pair<std::string, std::string>> values;   // "$FDR" -> "0.25"
};
struct Program {
  SourceFile source;
  std::vector<Line> lines;
  std::vector<Point> points;             // one unlabelled point without sweep lines
};
Program parse(SourceFile, core::CheckReport&);   // 09's report type; diagnostics per row 1.9

struct BatchFacts {                      // the look-ahead (Choice 01.C17)
  std::vector<std::string> sources, targets, fscs;
  std::vector<std::filesystem::path> confirmed_up, confirmed_down;   // 03.C2
};
class CheckContext {
 public:
  void input(const BoundValue& path);            // a run::InputSpec; made_by_line when produced (01.C12)
  void output(const std::filesystem::path&);     // later lines may read it; clashes are 09's
  void require(core::deps::Requirement);         // e.g. dot for pex static=svg (07)
  void feature(std::string_view name, bool on, std::string_view because);   // 0021
  const BatchFacts& facts() const;
  core::CheckReport& report();
};
struct Plan {
  Program program;
  std::vector<BatchFacts> facts;                 // one per point
  std::vector<std::vector<run::StepSpec>> steps; // one list per point: 09's plan (ids, lines,
                                                 // normalised lines, settings, inputs)
  core::CheckReport report;                      // 01's diagnostics; 09 adds the input checks
};
Plan check(Program, std::span<const int> only_points = {});   // tools through core::deps::Registry (11)

using GraphEntry = std::variant<std::shared_ptr<const network::Network>,
                                std::shared_ptr<const corridor::Corridor>>;
class GraphStore {
 public:
  int add(GraphEntry entry);             // next id
  const GraphEntry& get(int id) const;   // 0 = latest (S2.01); throws core::Failure
  int latest() const;                    // #LASTGRAPHNUM (S1.09); 0 when there is none
};
using Remembered = std::variant<std::shared_ptr<const network::Network>,
                                std::shared_ptr<const expression::Observations>,
                                std::shared_ptr<const enrichment::Shortlist>>;
class WrittenFiles {                     // Choice 01.C13
 public:
  std::optional<Remembered> get(const std::filesystem::path&) const;   // same identity and SHA-256
  void put(const std::filesystem::path&, Remembered value);
};
struct Workspace {
  std::shared_ptr<const network::Maps> maps;                     // start, or the shipped maps
  GraphStore graphs;                                             // networks and corridors
  std::shared_ptr<const expression::Observations> observations;  // cudf
  std::shared_ptr<const enrichment::Shortlist> shortlist;        // gsea (08's type)
  std::map<std::string, std::shared_ptr<const solve::Results>> solves;   // by solve name or prefix;
                                                 // each handed to results::publish_solve (row 1.5)
  BatchFacts facts;                              // this point's look-ahead
  WrittenFiles written;
};

class Interpreter {
 public:
  Interpreter(const Plan&, int point, run::Run&);   // run.log(), events() and stop() are 09's
  run::RunStatus run();                  // 09's statuses: ok, failed, stopped
};
struct RunOptions {                      // becomes 09's run::Options; jobs and points stay here
  std::optional<std::filesystem::path> root, run_dir, parent;   // run_dir: adopt; parent: --resume
  std::optional<std::string> label, log;
  std::map<std::string, std::string> tags;                      // --tag, 09's labels
  std::vector<std::pair<std::string, std::string>> settings;    // --set (row 2B.2)
  bool debug = false;
  unsigned jobs = 0;                     // the coordinator's core budget; 0 = auto
  std::vector<std::string> points;       // --point
  std::vector<std::string> command_line;
  std::function<void(int)> on_grace_expired;   // set by the executable: std::_Exit(3) (09)
};
int run_batch(const Plan&, const RunOptions&, core::Log&);   // returns the exit status
int run_query(std::string_view line, std::ostream& json_out, core::Log& stderr_log);   // --query
}
```

`exec`, FuSION 1's moves, and the executable:

```cpp
namespace fusion::batch {
struct ShellResult { int status = 0; bool stopped = false; };
class Shell {
 public:
  static std::optional<Shell> find(const std::filesystem::path& home);   // /bin/sh; busybox on Windows
  std::string describe() const;          // path and version, for run.json
  ShellResult run(std::string_view text, const std::filesystem::path& cwd,
                  core::Log& log,        // output to a file (ProcessSpec::Output::file), then
                                         // every line through the log (row 1.15)
                  const core::StopToken& stop) const;   // 11's core::Process in its own group;
                                         // stdin null; env_set PATH with the bundled tools
                                         // directory appended (11.C20)
};
bool is_window_plot(std::string_view exec_text);   // gnuplot -p without set term (Choice 01.C20)
bool display_available();                // DISPLAY or WAYLAND_DISPLAY on Linux; not SSH on macOS

enum class LegacyName { po_dat, limits_timefile, edges_merged, fb_rch, fb_rch_back, mylog };
struct LegacyMove { LegacyName name; std::filesystem::path dest; };
std::optional<LegacyMove> match_legacy_move(std::string_view exec_text);   // Choice 01.C11
void perform_legacy_move(const LegacyMove&, const Workspace&, core::Context&);   // PO.dat through 07's
                                                    // two_column, limits_timefile.txt through its legacy_limits
}

// app/fusion/main.cpp: the whole executable
namespace fusion::cli {
struct Options {
  std::optional<std::filesystem::path> batch;                 // -b
  std::optional<std::string> help;                            // -h [command]; "" = listing
  bool check = false, json = false, commands = false, query = false;
  bool version = false, debug = false;
  std::optional<std::filesystem::path> root, run_dir, resume, stop;
  std::optional<std::string> label, log;
  std::vector<std::pair<std::string, std::string>> tags;      // --tag key=value
  std::vector<std::pair<std::string, std::string>> settings;  // --set key=value
  std::optional<unsigned> jobs;                               // --jobs N; nullopt = auto
  std::vector<std::string> points;                            // --point
};
std::variant<Options, std::string> parse(std::span<const std::string> utf8_args);  // string: usage error
int run(const Options&);
}
int main(int argc, char** argv);            // Linux and macOS
int wmain(int argc, wchar_t** argv);        // Windows: arguments to UTF-8, console set to UTF-8
```

Algorithms and their cost:
- **Lexing:** one pass over each line, O(length); a token vector sized to the line. The longest corpus line has 705 characters and 81 tokens.
- **Expansion:** trie lookup at each `$`; O(length × longest key) per line at worst. Under a millisecond for the real batch.
- **Binding:** O(tokens + arguments) per line; argument names are looked up in a sorted array of at most about 40 names (`pathz3`).
- **Sweep planning:** O(points × dimensions).
- **Check:** O(points × lines) for the simulation, plus one `stat` and one open per distinct path, cached across points.
- **Memory:** the program is about the size of the file (13.6 KB for the real batch; 1.38 MB for all 116 corpus `*.batch` files together). The workspace holds shared pointers; a network is shared, never copied, by every line that refers to it.

### Files written

| File | Where | Written by |
|---|---|---|
| `points.tsv` | the sweep's run directory | the coordinator, after every change of a point's status (example above) |

Each point writes its own run log, which 09 names (09.C11), and the coordinator discards its children's consoles. This section also supplies fields that 09 writes into `run.json`: the batch file's path, SHA-256 and copy; the command line; every variable in definition order, with its line; the sweep point and its values; and, for each command line, its normalised named form, each argument's value and source (E1), its status and its wall time (row 1.14). The normalised form is enough to rewrite a FuSION 1 batch with names later.

### Events

The interpreter emits section 09's events (09.C10), with dotted names, the key `type`, and the envelope `v`, `seq`, `t` and `type`, to which `core::Events` adds `step`, `line` and `point` for every event inside a step (row 1.3, entry 190). It emits `inputs.checked` when the check ends; `step.started` and `step.finished` for every command line, an `exec` line included; `step.skipped` after a reported result or a stop; `warning` and `error`, with dotted codes and a message; and `run.finished`. A reported result is a `step.finished` with status `reported` and its `report{code, message}`. This section adds four types, which readers that do not know them ignore:

| `type` | When | Fields |
|---|---|---|
| `legacy.written` | a FuSION 1 move is written natively (Choice 01.C11) | `name`, `dest`, `from_step` |
| `sweep.planned` | the coordinator has checked every point | `points`: index, label and values of each |
| `point.started`, `point.finished` | a point's child process starts, or ends | `point`, `dir`; and `status`, `exit`, `wall_s` |

The real batch, shortened. Its check covers 174 input files of 7,250,798 bytes: the seven named inputs and the 167 KGMLs on the candidate list. Times and sequence numbers are illustrative:

```
{"v":1,"seq":2,"t":"2026-11-12T13:00:00.512Z","type":"inputs.checked","inputs":174,"bytes":7250798,"errors":0,"warnings":1}
{"v":1,"seq":3,"t":"2026-11-12T13:00:00.519Z","type":"step.started","step":"cudf","line":145,"point":"","cmd":"cudf"}
{"v":1,"seq":4,"t":"2026-11-12T13:00:00.523Z","type":"step.finished","step":"cudf","line":145,"point":"","status":"ok","wall_s":0.004,"outputs":["cudf/up_reg","cudf/down_reg"]}
{"v":1,"seq":87,"t":"2026-11-12T13:00:15.402Z","type":"legacy.written","step":"exec-18","line":221,"point":"","name":"PO.dat","dest":"/home/…/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10.dat","from_step":"pathz3"}
{"v":1,"seq":171,"t":"2026-11-12T13:10:58.612Z","type":"run.finished","status":"ok","exit":0,"wall_s":658.1,"forced":false}
```

The same batch with `$REACH_PATH_BOUND` set to 3. The shortest EGFR → CXCL8 path in its merged graph has 4 edges, counted over every relation, so the corridor is empty:

```
{"v":1,"seq":61,"t":"2026-11-12T13:20:31.100Z","type":"step.finished","step":"fb_rch","line":191,"point":"","status":"reported","wall_s":0.002,"report":{"code":"corridor.empty","message":"no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3: the shortest path has 4 edges"}}
{"v":1,"seq":62,"t":"2026-11-12T13:20:31.101Z","type":"step.skipped","step":"sz-3","line":192,"point":"","because":"corridor.empty"}
{"v":1,"seq":91,"t":"2026-11-12T13:20:31.140Z","type":"run.finished","status":"ok","exit":0,"wall_s":30.6,"forced":false}
```

Step ids use the canonical command names, `sz` and not `size` (09.C2). Each point of a sweep writes its own `events.jsonl`; the sweep's directory holds the coordinator's.

### Errors

Codes follow 09.C14's dotted form. Conditions found by the generic check or the binder take the generic codes (`batch.*`, `input.*`, `output.clash`, `artefact.*`, `io.write`, `deps.missing`; row 1.17), so every check diagnostic has a code (entry 230). This section's own codes:
- `batch.unknown_command`: an unknown or dropped command;
- `batch.bad_argument`: an unknown, duplicate or missing argument, a positional token after a named one, or a wrong number of positional tokens (entry 230), and an argument no earlier line can supply, such as a corridor for `pathz3`;
- `batch.bad_value`: a token that cannot be read or used: invalid UTF-8, an unterminated quote, a `let` key that is not `$name` or is reserved, `#LASTGRAPHNUM` with no graph yet, or a value not of its type;
- `batch.redefined`: a `let` key, or a sweep variable, defined twice;
- `batch.undefined_variable`: a `$NAME` that matches no key, outside `exec` lines;
- `batch.ignored_tokens`: FuSION 1 tokens, or lines after `exit`, that 2.0 ignores;
- `batch.unmatched_file`: a positional `pex` or `psp` file that matches no solve's prefix (07.C9; entry 230);
- `batch.no_fsc`: the check's note that a positional batch names no FSC (row 2A.9; entry 230);
- `batch.mff_before_cudf`: the check's warning for `mff` before any `cudf` (entry 230);
- `exec.status`: an `exec` line's non-zero status.

| Error | Found | Code | Kind | Status |
|---|---|---|---|---|
| batch file missing, unreadable or empty; no file name (S1.01) | at startup | none: a usage error | usage | 2 |
| unknown option, no `-b`, `-d` (S1.02, S1.03) | at startup | none: a usage error | usage | 2 |
| an install or dependency error, through 09's `error` event (row 2B.16) | at startup | `install.*`, `deps.*` | failure | 1 |
| a run root that cannot be written, or an adopted directory already used (09) | at startup | `run.dir_create`, `run.dir_used` | failure | 1 |
| invalid UTF-8, unterminated quote | check | `batch.bad_value` | error | 1 |
| unknown command (S1.11), or a dropped one such as `upfx` (S2.29); the listing follows | check | `batch.unknown_command` | error | 1 |
| a `let` key that is not `$name`, or is reserved (S1.08, Choice 01.C18) | check | `batch.bad_value` | error | 1 |
| a `let` key defined twice, or a sweep variable also set by `let` (S1.08) | check | `batch.redefined` | error | 1 |
| `#LASTGRAPHNUM` with no graph yet (S1.09) | check | `batch.bad_value` | error | 1 |
| an unknown, duplicate or missing argument; a positional after a named one; a wrong number of positional tokens, such as extra tokens after a `let` value (S1.08, S4.01, S5.02, S7.01); `debug` on the `run` line (entry 229) | check | `batch.bad_argument` | error | 1 |
| a value not of its type (S3.02, S5.02) | check | `batch.bad_value` | error | 1 |
| a line needing a value that no earlier line makes | check | `batch.bad_argument` | error | 1 |
| a missing required input, or a named optional input that is missing or unreadable (0021) | check | `input.missing`, `input.unreadable`, `input.not_file`, `input.list_member` | error | 1 |
| a positional `pex` or `psp` file that matches no solve's prefix (07.C9) | check | `batch.unmatched_file` | error | 1 |
| two outputs that clash (row 2A.7) | check | `output.clash` | error | 1 |
| a missing tool, or no shell when there are `exec` lines (11) | check | `deps.missing` | error | 1 |
| `load` of a run, step or artefact that is missing or unfinished, or that changed | check | `input.missing`, `artefact.changed` | error | 1 |
| `$NAME` that matches no key, outside `exec` (S1.07) | check | `batch.undefined_variable` | warning | none |
| FuSION 1 tokens that are ignored (S2.16), tokens after `exit` (S1.13), lines after `exit` | check | `batch.ignored_tokens` | warning | none |
| a positional batch that names no FSC (row 2A.9) | check | `batch.no_fsc` | note | none |
| `mff` before any `cudf` | check | `batch.mff_before_cudf` | warning | none |
| a window plot skipped without a display (Choice 01.C20) | run | none: a note in the run log | note | none |
| a produced input still missing at its step | run | `input.missing` | failure | 1 |
| a step's own error (S1.28) | run | the module's own code | failure | 1 |
| `exec` with a non-zero status (S1.14) | run | `exec.status` | failure | 1 |
| no path within the bound (S4.18), an empty selection (S7.03), no gene past the thresholds (04.C5) | run | `corridor.empty`, `gsea.empty_selection`, `observations.none` | reported result; later lines skipped | 0 |
| a stop request (A1) | run | none: the run's status is `stopped` | stopped | 3 |
| an unexpected exception | run | `internal` (09.C14) | failure | 1 |

### Settings

This section adds no setting that changes results. The command table records, through the settings registry (core, 09), every argument of every line with its value and source (E1). Registry keys are `<module>.<name>` (row 1.16). This section adds these run-level settings, none of which changes a result:

| Key | Type | Default |
|---|---|---|
| `run.label` | text | the batch file's stem |
| `run.root` | path | the batch file's directory |
| `run.jobs` | `auto` or a positive integer: the core budget for sweep points | `auto` |
| `run.points` | the selected sweep points | all |

It spells section 09's `run.debug` (`--debug`) and `run.log` (`run log=`, `--log`). `run.json` also records the shell's path and version, and the `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT` overrides (11.C20).

### Platform notes

- **Paths:** `std::filesystem::path`, built from UTF-8. On Windows, paths over 260 characters need the long-path manifest (section 11). Both `/` and `\` work in command arguments on Windows, but inside `exec` lines the POSIX shell treats `\` as an escape. So the `$FUSION_` variables hold paths with `/` (the generic form), and hand-written `exec` lines should use `/` too.
- **File identity** (Choice 01.C13): `st_dev` and `st_ino` on Linux and macOS; the volume serial number and file index on Windows. Path text alone would miss symlinks and the case-insensitive file systems of macOS and Windows.
- **Processes** (`exec`, sweep points): section 11's `core::Process` (row 2A.11): `posix_spawn` with its own process group on Linux and macOS; `CreateProcessW` with `CREATE_NEW_PROCESS_GROUP`, in a job object, on Windows. A stop ends the whole group or job. On Windows the shell text is encoded as one argument by the MSVCRT quoting rules.
- **stdin:** a batch run never reads stdin (09.C7); children get the null device (`/dev/null`, `NUL`). Only `--query` reads stdin, for its one line.
- **Signals** (09.C8, 11.C18; entry 185): the `fusion` executable turns Ctrl-C and SIGTERM on Linux and macOS, and Ctrl-C, Ctrl-Break and the closing of its console on Windows, into the same stop request as the `stop` file; the handlers only set a flag. A second Ctrl-C ends the process with status 130. SIGHUP is ignored on Linux and macOS, so a run outlives a closed terminal. Every Z3 solver gets `ctrl_c=false`. Because sweep points run in their own process groups, only the coordinator receives Ctrl-C, and it relays the stop by writing each running point's `stop` file.
- **Displays** (Choice 01.C20): Linux has one when `DISPLAY` or `WAYLAND_DISPLAY` is set; macOS lacks one in an SSH session.
- **Line endings:** CRLF batch files are accepted. Every file this section writes uses `\n` on all platforms, so outputs are byte-identical across them (09.C16).
- **Names:** run directories follow 09.C1, which applies Windows' rules on every platform. Sweep labels are reduced by row 1.26's rule and cut to 32 characters.

### Determinism

- Points are enumerated in a fixed order, and labels are derived from the file alone. A point's results do not depend on `--jobs` or on which point finishes first.
- Graph ids are one per graph a line makes, so the same batch gives the same ids, which the checker predicts.
- FuSION expands only its own variables: `let`, `sweep` and the four `$FUSION_` variables. The environment cannot change a result, except through an `exec` line.
- Numbers are read by one locale-free grammar (04.C2, through fast_float), the same on every platform.
- Diagnostics are sorted by point, line and column.
- `run.json` records the batch's hash, the variables, the bound arguments with their sources, and the shell's path and version.
- `exec` lines are outside FuSION's guarantees: line 211 of the real batch logs `$(date)`, for example.

## Choices

### 01.C1 · How named and positional arguments bind

- **Proposed:**
  - A token is named when it starts, unquoted, with one of the command's argument names (`[a-z][a-z0-9_]*`) and `=`. The name is literal; the value is expanded and may be quoted or empty.
  - Positional tokens first, in the positional form's order; named ones after, in any order. A positional token after a named one, a repeated name, a name already filled by position, or an unknown name is an error, and the last lists the command's argument names.
  - An empty value means "not given": an optional argument keeps its default or turns its feature off; a required one is an error.
  - Values are typed and must be whole tokens, from the types listed under Design. Lists are comma-separated in named form; FuSION 1's positional lists keep their `-1` terminators.
  - A `duration` takes a unit in named form (`4h`, `90s`, `500ms`); positional FuSION 1 slots keep bare milliseconds.
  - With row 1.7, `pathz3` has one set of names, owned by 06: the box `node_min`, `node_max`, `edge_min`, `edge_max`; the durations `timeout`, `incremental_timeout` and `budget`, with no `_ms`; and `coexpression_csv`, `coexpression_threshold` and `frozen_threshold`, since a `_min` suffix would misstate S5.15's and S5.16's strict `>` (entry 187).
- **Alternatives:**
  - Positional and named tokens in any order, with unnamed tokens filling the positional slots from left to right. Lines get shorter, but moving a token silently changes which slot it fills.
  - An empty value is an error, and leaving an argument out is the only way to omit it. Stricter, but the frontend could no longer write a blank field as `name=`, and FuSION 1's empty variables would fail.
  - Bare milliseconds for durations everywhere, as in FuSION 1. One rule, but `timeout=3600` looks like an hour and means 3.6 s.
  - 06's draft names: `node_relax_lb` to `edge_relax_ub`, and `timeout_ms`.
- **Why:**
  - The replay found no non-`exec` token of the form `name=…` and no `let` value containing `=` in the 119 corpus files, so this rule cannot misread a FuSION 1 line.
  - With literal names, a variable holding `=` (a path or a selector) never turns into an argument.
  - Positional-then-named is the rule of Python and R, and errors catch the common slips.
  - Corpus timeouts run from 15,000 to 86,400,000 ms (S5.21). `timeout=4h` is plainer than `14400000`.
- **Decision:** as proposed (prompt log entry 187)

### 01.C2 · Quoting, escapes and relative paths

- **Proposed:** double quotes anywhere in a token, `""` for a literal quote, `$$` for a literal `$` outside `exec` lines, the backslash never special, `$` expanded inside quotes, and a quoted `=` never making a name. `$$` amends S1.07, under which a `$` that matches no key, as in `$$`, passes through unchanged (accepted in entry 187). Relative input paths resolve against the directory of the batch file given to `-b` (a resumed run uses its parent's), and `exec` lines run there too; `$FUSION_BATCH_DIR` names that directory. Output paths follow rows 2B.3 and 4A.11 instead: relative or omitted, they land in the step's directory (entry 169). On Windows both separators work in command arguments.
- **Alternatives:**
  - Shell-style quoting, with backslash escapes and single quotes. Familiar on Linux, but every Windows path then needs doubled backslashes, and an unquoted `C:\temp` loses its backslash.
  - Relative paths, and `exec`'s working directory, in the launch directory, as in FuSION 1 and most command-line tools. A batch file then means something different depending on where `fusion` is started.
  - Every relative path against the batch file's directory, outputs included, as this choice first proposed. Command-line runs would then write outputs outside their run directory, against S1.26 and S8.21.
- **Why:**
  - No non-`exec` token in the corpus contains `"`, and no line contains `$$`, so the rule changes no FuSION 1 line.
  - Windows paths with spaces are common (user names, OneDrive folders). With the backslash literal, they need only quotes: `"C:\Users\Arjun Jeewan\lfc.tsv"`. Section 10's generator writes native paths; `""` and `$$` are the escapes it needs.
  - Section 10 exports a batch with an `overrides/` folder beside it, and the export runs anywhere only if input paths are relative to the batch file.
  - FuSION 1's launchers differ. Four under `runs/` (`run_sweep.sh`, `run_all.sh`, `run_fsc_tests.sh`, `run_direct_runs.sh`) and the GUI's `ProcessBuilder` (`Linux/frontend/src/Gui4Panel.java:153`) start `fusion` in the batch file's directory. Three, `runs/egf_hela/experiment/run_*_sweep.sh`, start it in a per-run subdirectory below the batch file (`cd "$wd" && "$BIN" -b "$batch"`, lines 18-24 of `run_nr4_er4_sweep.sh`). The `runs/real_experiments/run_experiment_*.sh` scripts never change directory (`run_experiment_1.sh:19`).
  - The rule matters only for relative paths, and the corpus has 12: 4 `pathz3` prefixes (`po_w`, `po_wo`) and 8 `exec mv` destinations, all in the two `auxilliary/comparison_results/` batches, whose launcher is not in the repository. The batches the other launchers run use absolute paths, apart from FuSION 1's fixed-name outputs, which Choice 01.C11 handles without a working directory. So the rule changes no result seen.
- **Decision:** as proposed, amending S1.07 (prompt log entries 169 and 187)

### 01.C3 · Sweep syntax

- **Proposed:**
  - `sweep <$var> <value>…` is one dimension, one run per value: `sweep $REACH_PATH_BOUND 6 7 10`. An optional `label=<prefix>` gives labels such as `b6`.
  - `sweep <$var>=<value>… [label=<text>]` is one row, its `label` last as a named argument. All row lines together form one dimension, so several variables vary together.
  - Dimensions multiply, with the first line outermost.
  - Names on `sweep` lines are literal, like `let` keys. Values are expanded at their line, so they may use earlier `let` lines. A sweep variable may not also be set by `let` (S1.08). Sweep lines come before the first step line.
  - Labels come from `label=`, or else from the variable and value (`reach_path_bound-6`), reduced by row 1.26's rule (per code point, each run of disallowed characters becomes one `-`, ends trimmed) and cut to 32 characters. A point runs in `points/<NN>-<label>/`.
- **Alternatives:**
  - List lines only, every combination. Simplest, but the bound-and-FSC set and the candidate set are not products, so they would still need cloned files.
  - A separate table file (`sweep table=points.tsv`, one column per variable, one row per point). Easy to edit in a spreadsheet when there are 20 or more points, but it is a second file, where decision 0018 asked for one.
  - A multi-line `sweep … end` block. This breaks the one-command-per-line rule.
- **Why:**
  - The FDR sweep's four files (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr0{25,30,40,50}`) differ in 2 of their 77 `let` values (`$GSEA_SELECTOR` and `$WORK_DIR`), with identical pipelines: one list line.
  - The three `…_gsea_fdr025` runs at bounds 6, 7 and 10 differ in `$REACH_PATH_BOUND` and both timeouts together (3,600,000/14,400,000 ms at b10, 600,000/3,000,000 at b6 and b7): rows. So does the JUN b10, JUN b6, DUSP1 b10, FOSB b10 set.
  - The nine candidate runs `r0` to `r8` differ in 15 `let` values, and were written by a 364-line Python generator (`runs/egfr_b10_gsea_fdr044_candidates/make_runs.py`): rows, once the FSC and avoid lists are named arguments.
  - The `*_string125` copies of `r0` to `r6`, generated on 2026-11-12, swap the coexpression CSV for `hsa_stringdb_v12.5_coexpression_graph_pairs.csv`: the seven rows times a two-value list.
- **Decision:** as proposed (prompt log entry 187)

### 01.C4 · How sweep points run

- **Proposed:**
  - The `fusion` that reads a sweep becomes a coordinator. It checks every point, creates the sweep's directory, and runs each point as a child `fusion` on the same batch copy, with `--point` and the point's run directory, in point order. A batch without `sweep` lines runs in the process itself.
  - `--jobs auto`, the default, is a core budget of min(⌊logical CPUs / 2⌋, ⌊physical memory / 3 GiB⌋), at least 1, and each case of a point takes one core: a point with an FSC needs two, three with `free`, since 06.C4 solves a run's cases in parallel (row 3A.9, entry 173). Points start in order while their cases fit, and at least one point always runs. `--jobs <n>` sets the budget.
  - The children's consoles go to the null device; each point writes its own run log (09.C11, entry 185).
  - The frontend never uses the coordinator: it submits one run per swept value under its own queue limit, which counts cases the same way (10.C20, 10.C21; entry 187).
- **Alternatives:**
  - Threads in one process. The merged graph could be shared in memory, but a crash in one point ends them all, memory is not returned between points, and stopping one point is harder.
  - Points one after another (`--jobs 1` always), with parallelism by starting several `fusion` processes by hand, as today.
  - A budget that counts points, not cases. It oversubscribes the cores when 06.C4 runs two or three cases per point, and per-query timeouts are wall-clock, so an oversubscribed core can turn a finished check into a timeout.
- **Why:**
  - The nine candidate runs took 245.4 h of wall time added up, but 52.1 h as run in parallel by `run_all.sh`: 4.7 times faster. Their peak memory added up to 12.4 GiB, at most 1.89 GiB for one run (`run_time.txt`).
  - The FDR sweep gained less, 52.3 h against 50.7 h, because one point took 50.7 h.
  - `auto` gives a budget of 18 cores on this machine, which runs the nine candidate points (two cases each) at once, as the user ran them by hand. A laptop with 8 logical CPUs and 16 GiB gets 4 cores: two points with an FSC at a time. The 3 GiB per core leaves a margin of 1.6 over the largest peak measured.
  - Z3 uses one core per check.
  - Child processes keep A1's property that a crash ends one run, and they reuse the stop and resume of single runs unchanged.
- **Decision:** as proposed (prompt log entry 187)

### 01.C5 · What a failing sweep point does to the others

- **Proposed:** a failure stops only its own point, which ends `failed` with exit status 1, as S1.28 says for a run. The other points run to their end, and the sweep exits 1. `--resume` on the sweep later re-runs the failed and stopped points from their last good step. This reads S1.28's "the batch stops at the first failure" as applying per point (row 4B.5, entry 188).
- **Alternatives:** fail fast: the first failure stops every running point, with resumable state, and starts no more. This reads S1.28 as covering the whole file.
- **Why:**
  - Points do not depend on each other. The check finds input errors for every point before any point starts, so a failure while running belongs to one point.
  - It follows the user's reasoning for S4.18 and S7.03: one point's outcome should not halt a sweep.
  - Fail-fast would cost the other points' hours: `r6_mcl1_creb1` alone ran 52.1 h.
- **Decision:** as proposed, as the reading of S1.28 for sweeps (prompt log entry 188)

### 01.C6 · The commands for partial runs

- **Proposed:** as in the partial-runs table under Design:
  - `exit` to stop after a step, with every step saved as it ends (09.C4);
  - `load <file>`, which restores a saved value exactly as saved, and `load run=<dir> step=<step>`, which restores everything that was current after that step of an earlier run, through section 09's `Registry` (row 2A.4, entry 164). `rgx` of a saved network keeps 02's compare-and-rebuild, and `mff` has no `from_saved` argument;
  - `fusion --resume <dir>` for a new run with that run as its parent (09.C5), on the parent's batch, or with `-b` on a corrected one; 09's fingerprints decide which steps are loaded;
  - `pathz3`'s `cases`, box and `points` arguments to solve part of the frontier (06.C9, entry 175).
- **Alternatives:**
  - Resume in place: replay the run's batch by line number inside the same directory. Simpler, but a run directory would change after its run ended, which S8.21 and section 09 rule out (a resumed run is a new run).
  - A `--parent <dir>` option, section 09's word, in place of `--resume`. The same mechanism, but "resume" is the word decision 0019 uses.
  - One load command per kind of value (`load_graph`, `load_corridor` and so on), without restoring the rest. Explicit, but the user must reload every dependency by hand, such as the observations a solve needs.
  - A `--stop-after <command>` option in place of `exit`, which saves editing the file.
- **Why:**
  - The 2026-11-11 re-solve of one point (`r5_mcl1_stat3/resolve_wo_n7_e0`) needed 91 command and `let` lines and rebuilt the merge and two corridors. With `load run=… step=fb_rch` it needs three lines and no rebuild.
  - Users already start from saved steps by hand: the `*_string125` runs drop their `gsea` line and point `$GSEA_KGML_LIST` at the shortlist an earlier run saved, and the re-solve reused its run's `gsea_kgml_files.txt` the same way.
  - Saving every step costs little: the largest step output, a merged graph, is 539,688 bytes in the real run and 13,375,038 bytes for the GUI's 8,237-entry graph (S8.23).
  - Restoring the whole state after a step needs nothing from the user beyond naming the step.
  - S1.28 already says "partial runs restart from the last good step", which `-b <corrected batch> --resume <failed run>` does.
- **Decision:** as proposed (prompt log entry 188)

### 01.C7 · How a batch names its run directory

- **Proposed:** the run directory takes 09.C1's name, `<label>_<YYYYMMDD-HHMMSS>` inside a root, with a run id in `run.json`. The label and the root come from a `run root=… label=…` line before the first step, overridden by `--root` and `--label`, and default to the batch file's stem and directory. `--run-dir` adopts a directory that the frontend made (A1), whose label is the session's name and whose session id goes in `run.json` through `--tag session=<id>` (10.C18; entry 184). The same `run` line carries the batch's other run-level settings, which `--set` overrides (rows 1.16 and 2B.2, entry 190); a step's own argument wins over both for that step (entry 229). `$FUSION_RUN_DIR` holds the result. A sweep's points are its subdirectories, `points/<NN>-<label>/`.
- **Alternatives:**
  - The command line and the defaults only, with no `run` line. One mechanism, but a batch can no longer say where its results go, which 114 of the 116 corpus files do today through `$WORK_DIR`.
  - A `run` line that names the exact directory, with `-2`, `-3` when it holds a run, as FuSION 1 batches name theirs: `x`, `x-2` and `x-3` say nothing about when each ran (09.C1's rejected alternative).
  - A default root under the launch directory, which is FuSION 1's implicit output location. The results' place then depends on where the command was typed.
- **Why:**
  - FuSION 1 batches give both parts today: a working directory (`$WORK_DIR`) named after the experiment, and a batch file whose name repeats it, such as `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch`. They become `root` and `label`.
  - S8.21: a GUI rerun deleted the earlier results with `rm -rf $WRITE_DIR`. With a timestamp in every name, a rerun never meets an earlier run's directory.
  - The default root is the batch file's directory, the directory that relative input paths use (Choice 01.C2). It does not depend on where `fusion` was started, which FuSION 1's launchers chose in three different ways (01.C2).
  - Section 09 owns the name's form and asks this section only where the label and root come from.
- **Decision:** as proposed (prompt log entry 184)

### 01.C8 · The one debug option

- **Proposed:** `--debug` on the command line sets section 09's `debug` setting, off by default. It turns on the debug records in the run log, the corridor traces, the SMT dumps and the modules' slow self-checks. The run log itself is always written, and holds every console line and the output of `exec` lines (09.C11, row 1.15; entry 185). Its per-run name is 09's `log` setting, given by `run log=` or `--log` before any line runs (S1.25). `debug` stays off the `run` line, which takes the other run-level settings; a `run` line that names it gets `batch.bad_argument` (entry 229). Sweep points inherit `--debug`, and `run.json` records it.
- **Alternatives:**
  - `--debug=<path>`, writing debug records to a separate log that the option names. A second log beside 09's one run log (09.C11).
  - A batch line, `debug on`, or `run debug=true`. The batch file then carries it and reruns repeat it. But debugging is a property of how one runs, not of the experiment, and the frontend would have to remove it from the files it writes.
  - An environment variable, `FUSION_DEBUG=1`. It works without changing any command line, but it is invisible in the recorded command line.
  - One switch per output (`--debug=smt,traces`). Finer, but S1.29 asked for one option.
- **Why:**
  - SMT dumps alone fill 206 directories and 6,138,170,603 bytes under `runs/` (measured with `find` and `du -sb` on 2026-11-12; S5.23 counted 171 and 5.55 GB), and 57.5 MB and 65.6 MB for the two solves of the full run (S5.23). They must stay off unless asked for, and 06.C12 reads none back.
  - A command-line option is set before any line runs, and so is a `run log=` line, which the check reads before anything starts: both meet S1.25's "set before `start`".
- **Decision:** as proposed (prompt log entries 185 and 229)

### 01.C9 · Where the command table lives, and how the frontend gets it

- **Proposed:**
  - The table lives in `batch`, one entry per command, with its handler and checker. A handler turns bound arguments into the owning module's typed options and calls that module's public interface, so modules never see tokens or argument names.
  - An argument's default is not copied into the table: its `ArgSpec` names a settings-registry key, `<module>.<name>` (row 1.16), whose `SettingSpec` gives its 09 `core::Type`, default and range (row 1.1), so help, `--commands`, `run.json` and the module always agree.
  - Each entry declares its cross-argument rules, such as `less(down, up)` (S3.07) and `less_or_equal(node_min, node_max)` (S8.11). `fusion` enforces them, and the frontend evaluates the same rules as the user types: decision 0021's one set of rules.
  - Each entry declares the external tools it needs, with versions and features; section 11 owns the check and `deps.missing` (rows 2A.1 and 2B.15, entry 158). Row 2B.15's second field, a mark for shell commands that lab mode would have refused, is dropped, since there is no lab mode (entry 199).
  - `fusion --commands` prints the table as JSON, with the registry's defaults, the rules, the tools and the run-level settings, and the frontend builds its block library from it at start-up (S8.19). This section writes that output's JSON Schema, which section 12 keeps (row 2A.10).
- **Alternatives:**
  - Each module registers its own commands (`corridor::commands()`), and `batch` collects them. Each definition sits next to its code, but every module then depends on the batch language's types.
  - The frontend keeps its own block definitions, as FuSION 1's `block_templates.txt` did.
  - A shared rules file, JSON Schema plus a list of file checks, read by both sides. It cannot express checks on file contents (09.C12).
- **Why:**
  - S1.12's three mismatches came from two hand-kept lists, and S8.19's wrong blocks for `rgx`, `fb_rch`, `pathz3`, `let`, `exec` and `mff` from a third. One table with a JSON export removes all three lists.
  - Handlers in `batch` keep the language out of the step modules (B1, C1), which Phase 3 tests call directly.
  - Adding a reader for another network source or value type (decision 0020) is one table entry and one handler.
- **Decision:** as proposed (prompt log entry 186)

### 01.C10 · The shell that `exec` uses

- **Proposed:** `exec` always means a POSIX shell: `/bin/sh -c <text>` on Linux and macOS, and on Windows the busybox-w32 that FuSION installs with itself, a single executable run as `busybox.exe sh -c <text>` and found through the install directory (S1.20). The shell's path and version go into `run.json`. A batch with `exec` lines on a machine without a usable shell fails the check. `exec` lines get FuSION's bundled tools directory appended to `PATH`, so a system gnuplot still comes first (11.C20, entry 204).
- **Alternatives:**
  - `cmd.exe /d /s /c` on Windows. Always present, but batch files with `exec` stop being portable, and FuSION 1's lines do not run.
  - Windows PowerShell 5.1. It is on every Windows 10 and 11 and has `cat`, `mv` and `echo` aliases. But its `>` writes UTF-16 with a byte-order mark, which FuSION's readers reject, and `mkdir -p`, `touch`, `awk` and `date +%s.%N` are missing or behave differently.
  - Git for Windows' `sh.exe` when it is installed. No bundling, but few biologists will have it, which is the point of G3.
- **Why:**
  - 3,192 of the corpus's 3,917 `exec` lines use a command or construct that only a POSIX shell has: `cat`, `mv`, `touch`, `awk`, `date`, `printf`, `if [ … ]`, `$(…)`, single quotes or `mkdir -p`. Of the other 725, 120 chain commands with `;`, which `cmd.exe` does not take as a separator. That leaves 605 single `echo` lines (15 %), which `cmd.exe` runs differently: it keeps the space before `>` and writes CRLF. The verifier's slightly different patterns gave 3,194 and 723.
  - FuSION 1's lines ran under dash on this machine (`/bin/sh -> dash`). busybox's shell is of the same Almquist family.
  - 2.0's own batches need no `exec` (the table of jobs under Design), so on Windows the shell serves mostly hand-written batches.
  - busybox is GPL-2.0. Shipping it as a separate executable, with its source offer, is section 11's concern, and so is confirming its applets on a Windows build (entry 188).
  - 65 corpus files run gnuplot inside `exec` lines, in 136 lines, 77 of them with `set term png` (entry 204 counted the 59 files and 71 lines under `runs/`). With the bundled tools last on `PATH`, they work on a machine without a system gnuplot, and section 11 checks in Phase 4 that its cairo-only build accepts `set term png` (entries 204 and 205).
- **Decision:** as proposed (prompt log entry 188)

### 01.C11 · FuSION 1's moves of fixed-name outputs

- **Proposed:** before the shell, the interpreter recognises an `exec` line that is exactly `mv <name> <dest>`, where `<name>` is `PO.dat`, `limits_timefile.txt`, `edges_merged_filename`, `fb_rch`, `fb_rch_back` or `mylog.txt`. It writes 2.0's matching output to `<dest>` instead, and prints a note:
  - `PO.dat`: the latest solve's frontier, one `n<TAB>e` line per point (S5.25), through section 07's `two_column`;
  - `limits_timefile.txt`: the latest solve's search log in FuSION 1's rendering, through section 07's `legacy_limits` (entry 230), because the user's `compare_sweep.py` and `compare_candidates.py` parse that format there (rows 3A.29 and 1.21, entry 179, which closes 07 open question 2);
  - `edges_merged_filename`: the latest merge's report (S2.24). After a one-file `mff … y` over the old master (S2.27), which merges nothing and has no report (S2.16), nothing is written and a note says why; all 65 old-master batches have such a line;
  - `fb_rch` and `fb_rch_back`: the latest corridor's traces under `--debug` (S4.19); otherwise nothing is written;
  - `mylog.txt`: the debug records of the run log under `--debug`; otherwise nothing.

  2.0's own copy stays in the run directory, and nothing is written to the launch directory. This amends S1.14, which passes every `exec` line to the shell, for these six moves only (accepted in entry 179).
- **Alternatives:**
  - Write FuSION 1's fixed-name files into `exec`'s working directory and let the shell move them. No pattern matching, but `fb_rch`, `fb_rch_back` and `mylog.txt` would be written on every old batch's run, against S4.19 and S1.25; and the working directory would have to become the run directory, so relative paths in `exec` lines would change meaning.
  - Leave the lines to the shell. Each fails (`mv: cannot stat 'PO.dat'`), and by S1.14 and S1.28 every old batch stops at its first solve, against decisions 0017 and 0023.
  - Skip the lines with a warning. The run goes on, but the corpus's 136 inline gnuplot lines and 29 `plot_po.sh` lines (in 94 files) then read `.dat` files that were never written, and fail.
- **Why:**
  - These moves are 879 of the corpus's 3,917 `exec` lines (22 %), in all 119 files. Every one of them is the single command `mv <name> <dest>`, with no other form, so the pattern is closed and exact.
  - Without it, S1.14, S1.26 and S1.28 together would stop every FuSION 1 batch: the real batch has eight such lines (176, 220, 221, 230, 231, 253, 254 and 271).
  - It needs no shell, so it also works on Windows.
- **Decision:** as proposed, amending S1.14 for these six moves (prompt log entry 179)

### 01.C12 · Checking inputs that the batch itself produces

- **Proposed:** an input path is produced in the batch when an earlier line declares it as an output, or when its text occurs in an earlier `exec` line. A command's checker declares every file it will write, derived names included, such as a positional `pathz3`'s `<prefix>_solutions.txt` and `<prefix>_relaxations.txt`. Such an input is checked again just before its step runs, and copied and hashed then (09.C3); every other input is checked before anything runs. A produced input still missing at its step is a failure, and the message names the line expected to make it.
  - This amends decision 0021 ("all inputs are checked before the first step"), S1.15 (an unreadable named `start` file stops the run before it starts) and S2.16 (every listed path is checked before the run), for inputs that the batch itself makes (accepted in entry 188).
- **Alternatives:**
  - Require every input to exist before the run. Simplest, but every FuSION 1 batch builds `start`'s exception file and `pathz3`'s node files with `exec` lines first.
  - Parse each `exec` line's redirections (`> f`, `>> f`, `touch f`) to know exactly which files it makes. Tighter, but it needs a shell parser, and `cd X; echo … > f` writes relative to X.
- **Why:**
  - In the real batch, all 15 list files of each `pathz3` line are made earlier in the same batch, 2 by `cudf` and 13 by `exec` lines, and so are `start`'s exception file (line 153) and `mff`'s list (`gsea`, line 169). Checked before the run, every FuSION 1 batch would fail. (The files `pex` names at line 269 are not inputs: they bind by prefix, 07.C9.)
  - The textual rule never rejects a valid batch. It can only move a check from before the run to just before the step, which is still before any long solve that depends on it.
- **Decision:** as proposed, amending decision 0021, S1.15 and S2.16 for inputs the batch makes (prompt log entry 188)

### 01.C13 · Reusing files written earlier in the same run

- **Proposed:** when a step writes a file that a later line may read, the interpreter keeps the value with the file's identity and SHA-256. A later input naming the same, unchanged file gets the value from memory. `rgx` of the merged graph that `wgx` just wrote applies 02's re-read normalisation to the in-memory network (S2.26), and `pathz3` reads `cudf`'s lists from memory. A changed file is read from disk, with a note. Positional `pex` and `psp` lines bind by prefix instead (07.C9, entry 179).
- **Alternatives:**
  - Only `wgx` followed by `rgx` is special-cased. Smaller, but `cudf` to `pathz3` and `gsea` to `mff` keep their file round trips.
  - Trust the path without checking a hash. Slightly faster, but `r7` and `r8` append a curated KGML to `gsea`'s shortlist with `exec echo … >>` between `gsea` and `mff`, and the in-memory shortlist would silently miss it.
- **Why:**
  - S2.26's decision asks for the in-memory graph. 114 corpus files run `wgx` and then `rgx`: 111 batch files and the three GUI scripts (S2.26 counted 75 earlier). The real run wrote and parsed 539,688 bytes, and the GUI run 13,375,038 bytes (S8.23).
  - The hash costs one read and no parse.
  - File identity, not path text, handles symlinks, `..` and case-insensitive file systems.
- **Decision:** as proposed (prompt log entries 179 and 189)

### 01.C14 · The command line and exit statuses

- **Proposed:** keep `-b` and `-h` (S1.01, S1.02), and add `--check`, `--json`, `--root`, `--label`, `--log`, `--run-dir`, `--tag`, `--resume`, `--stop`, `--point`, `--jobs`, `--debug`, `--query`, `--commands` and `--version`, whose JSON section 11 describes, plus `--set` from row 2B.2 (entry 190): 16 new flags, the count entry 230 corrects, with entry 189's answer unchanged. The table under Design lists them. `--query` reads one batch line on stdin and writes one JSON answer on stdout (row 3A.8, entry 168). Exit statuses:
  - 0 when the run ran to its end, reported results included;
  - 1 on a failure, as 09.C14 defines it, check errors and install or dependency errors at startup included (row 2B.16); `run.json`'s error code tells the cases apart;
  - 2 on a usage error;
  - 3 when stopped;
  - 130 after a second Ctrl-C (09.C8).
- **Alternatives:**
  - Subcommands (`fusion run`, `check`, `resume`, `stop`, `commands`), with `-b` kept as an alias: two ways to run a batch.
  - Status 2 for every error found before a step runs, check errors included, and 4 for internal errors, so the status alone tells "nothing ran" and "a bug" apart.
  - A JSON request on `--query`'s stdin: a second input format beside the batch language (04.C8's rejected alternative).
  - Queries as command-line arguments (`-e '<line>'`). Windows hands a child process one command line that the C runtime splits again, which breaks paths with spaces and quotes (04.C8).
  - The BSD `sysexits.h` codes (64 for usage, 65 for data, 70 for software errors). Standard on BSD, but unfamiliar to most users, and Windows has no such convention.
- **Why:**
  - A1 needs a directory given by the frontend, a stop request and a "stopped" status. Section 09 asks this section for options that set the label and root, adopt a directory, check only, name a parent, switch on debugging and add labels such as `session=<id>`.
  - Decision 0021 asks the frontend to validate too; `--check --json` gives it exactly the backend's rules (09.C12).
  - S8.14 asks the frontend's preview to use the backend's parser; `--query` runs the exact `cudf` or `suggest_thresholds` line the batch will hold, without a run directory, at about 1 ms of process start-up (04.C8).
  - S8.19 needs the table (`--commands`).
  - The GUI ignored the exit status (S1.28), so the statuses must say exactly what happened.
- **Decision:** as proposed (prompt log entry 189)

### 01.C15 · What a reported result skips

- **Proposed:** after a reported result, every later line of that run or sweep point is skipped, each with a `step.skipped` event. The run ends `ok`, with the report listed, and exits 0. There are three reported results: `corridor.empty` (S4.18), `gsea.empty_selection` (S7.03) and `observations.none` (04.C5); a solve's outcomes are case statuses, not reports (rows 3A.14 and 1.8, entry 167). This follows S7.03 ("that run's later steps are skipped") and 09.C14 for every reported result. For the empty corridor it widens S4.18's wording, which says only that "its solve is skipped" (row 4A.6; accepted in entry 167).
- **Alternatives:**
  - Skip only the steps that need the empty value, directly or through later values (the solve, then the plot and the explanation), and run the rest. This is S4.18's letter. But `exec` lines declare no inputs, so they would run, and in FuSION 1 batches they read files that the skipped solve never wrote: the 165 Pareto-plot lines (gnuplot and `plot_po.sh`) read `.dat` files. They would fail, and S1.14 and S1.28 would turn a reported result into a failure.
  - Different rules for the two results: skip only the solve after an empty corridor, and every later step after an empty selection, as the two spec items are worded.
- **Why:**
  - A bound sweep still carries on, which is the reason S4.18 gives.
  - One rule for every reported result is simpler to explain, and to show in the frontend.
  - In every corpus batch, the lines after `fb_rch` serve the solve and its plots, so nothing useful is lost.
- **Decision:** as proposed, widening S4.18's wording (prompt log entry 167)

### 01.C16 · Steps take the latest value of each kind by default

- **Proposed:** an argument that takes a network, a corridor, observations, a shortlist or results defaults to the latest value of that kind in the run: `0` for graphs (S2.01), the latest `cudf` observations, the latest `gsea` shortlist and the latest `pathz3` results. A name or a path overrides it.
- **Alternatives:**
  - Explicit references only: each producing line names its result (`gsea … as=$SL`) and each consumer names it (`mff list=$SL`). Nothing implicit, but every batch carries the plumbing that FuSION 1 batches did through files.
  - Files only, as in FuSION 1: values pass through the paths written and read. That is the round trip that D1 replaced with values in memory.
- **Why:**
  - It extends S2.01's `0` for the latest graph, which every graph argument in the corpus uses (`fb_rch` 116 lines, `pathz3` 210, `sz` 445, `wgx` 118, the rest through variables), to the other kinds.
  - In the corpus, each consumer uses the value made just before it: the one `cudf`, the shortlist of the `gsea` before `mff`, and the corridor of the `fb_rch` before each `pathz3`.
  - It answers how `fb_rch` gets the observations (05.C9), and gives `mff` the up and down genes (03.C1).
  - The check resolves every default before the run, so a line whose value does not exist yet is an error.
- **Decision:** as proposed (prompt log entry 189)

### 01.C17 · How `mff` learns the source, target and FSC

- **Proposed:** as under Design. The up and down genes come from the latest observations. The source, the target, the FSC and the confirmed up and down lists come from the batch's own later `fb_rch` and `pathz3` lines, collected for each point by the check (`BatchFacts`), as defaults for section 03's `mff` arguments `source`, `target`, `fsc`, `confirmed_up` and `confirmed_down`; an explicit argument wins (row 3A.1 and 03.C2, entry 165). A positional FuSION 1 batch names no FSC, so it keeps its own `start` exception file, and the check's one note says so (03 open question 1, row 2A.9; entry 165).
- **Alternatives:**
  - Run parameters, as 03.C1 proposed: a header line such as `params source=hsa1956 target=hsa3576 fsc=hsa3725`, whose values become the defaults of every argument of the same name in `mff`, `fb_rch` and `pathz3`. Explicit and stated once, but a new statement, with defaults that apply by name across commands.
  - Explicit arguments only: the frontend and users write `mff source=… target=… fsc=…`, sharing variables with the later lines, and the check warns when a later `pathz3` names an FSC that `mff` does not keep apart.
  - Split at the corridor: merge without exceptions, as S2.22's merge reuse does, and let `fb_rch` split the exception ids out. `fb_rch` knows the source and the target, but the FSC appears only on the later `pathz3` line, so `fb_rch` would need it as well.
  - Recognise FuSION 1's pattern of `exec echo` lines that write the FSC into the node files, so that old batches get S2.13's fix too. Only template-shaped batches would benefit, and the check would read shell lines before they run.
- **Why:**
  - S1.17, S2.13 and S8.22 decide that 2.0 adds these ids itself, so merging must know them, and leave the mechanism to Phase 2.
  - In the default pipeline (S8.25) and in an FSC sweep, each id then appears once, on the line that uses it, and each sweep point gets its own exception set from its own `pathz3` line.
  - Leaving the FSC out is the RAS trap of S2.03, so a default that cannot be forgotten is safer than an argument that can.
  - The look-ahead costs nothing: the check binds every line before the run anyway.
- **Decision:** as proposed (prompt log entry 165)

### 01.C18 · Predefined variables

- **Proposed:** four predefined variables, `$FUSION_RUN_DIR`, `$FUSION_BATCH_DIR`, `$FUSION_POINT` and `$FUSION_HOME`, holding paths with `/` on every platform. The `$FUSION_` prefix is reserved: a `let` or `sweep` of such a key is an error.
- **Alternatives:**
  - No predefined variables: batch files name every path themselves, as FuSION 1's do through `$WORK_DIR`. But a batch could then not name its own run directory, which exists only once the run starts, or FuSION's bundled files.
  - Environment variables in expansion (`$HOME`, `$USER`). Familiar, but the environment could then change a result unseen, and `exec` lines already reach the environment through the shell.
- **Why:**
  - No corpus file defines a `$FUSION_` key, so the reserved prefix breaks nothing.
  - `$FUSION_RUN_DIR` lets `exec` lines and FuSION 1-style output paths write into the run directory (S1.26) without absolute paths. `$FUSION_HOME` replaces the absolute path to FuSION's own files that 29 `exec bash …/plot_po.sh` lines carry (S1.20, S6.22). `$FUSION_POINT` lets a sweep point name what it writes.
  - Paths with `/` stay usable in `exec` lines on Windows, where busybox `sh` treats `\` as an escape.
- **Decision:** as proposed (prompt log entry 189)

### 01.C19 · Batch files are UTF-8

- **Proposed:** a batch file is read as UTF-8. A leading byte-order mark is skipped, and an invalid byte sequence is an error that names the line.
- **Alternatives:**
  - Bytes as they are, as in FuSION 1, with the platform's own conversion of paths. On Windows that is the ANSI code page, so a path with characters outside it cannot be named, and one file can name different paths on two machines.
  - UTF-8 or UTF-16, told apart by the byte-order mark. Some Windows editors save UTF-16, but no other FuSION input is UTF-16 (section 04 refuses UTF-16 value files).
- **Why:**
  - Paths must become UTF-16 on Windows, and UTF-8 converts exactly.
  - The corpus is ASCII except two files with a `—` in a comment, which are valid UTF-8.
  - Section 10's generator writes UTF-8 without a byte-order mark, and section 04 reads value files the same way.
- **Decision:** as proposed (prompt log entry 190)

### 01.C20 · Window plots without a display

- **Proposed:** an `exec` line that opens a gnuplot window, `gnuplot -p` without `set term`, runs as in FuSION 1 when a display is available, and is skipped with a note in the run log when none is: no `DISPLAY` or `WAYLAND_DISPLAY` on Linux, or an SSH session on macOS. This is a new rule that amends S1.14 and S6.25 for these lines (row 4B.6; accepted in entry 190).
- **Alternatives:**
  - Run them as written. A headless run waits at the line until stopped.
  - Always skip window plots, even with a display, since `plot_pareto` draws the plot.
- **Why:**
  - The corpus has 59 such lines, in 59 files: the 56 `*.batch` files that the poll counted, and the three GUI scripts. Entry 230 confirmed the count and kept the rule. Run here with no display, gnuplot 6.0 printed a Qt error and waited until a 20 s timeout killed it (status 124), so a headless run, or a frontend run on a server, would wait at such a line until a stop request ended it.
  - 2.0 draws the Pareto plot itself (`plot_pareto`), so these lines only open viewing windows; skipping them loses nothing that the run records.
  - With a display, the user still sees the windows, as in FuSION 1.
- **Decision:** as proposed, a new rule amending S1.14 and S6.25 for window-plot lines (prompt log entry 190)

## Interfaces with other sections

- **02 · Network:** `start`, `rgx`, `wgx`, `sz`, `add` and the query-only `labels [map=<file>]` (row 2B.6) in the table, with their FuSION 1 forms as listed. `start` is optional, and the workspace's `Maps` take the shipped symbol and pathway-name maps for any file it leaves out, or for both without it (`network::shipped_maps`, row 2B.4). Needs: one graph id per network a line makes (S2.01); a normalisation of an in-memory network equal to re-reading it (S2.26, Choice 01.C13); the network file's marker, so that `load` recognises it, and `rgx`'s compare-and-rebuild of a saved network (row 2A.4).
- **03 · Merge:** `mff`'s two positional forms; its list defaults to the latest shortlist (Choice 01.C16); its `source`, `target`, `fsc`, `confirmed_up` and `confirmed_down` default to the batch's facts (Choice 01.C17, row 3A.1). There is no `from_saved` argument (row 2A.4), and the diagnostic for a positional batch with no FSC is this section's check note `batch.no_fsc`, not a `merge.fsc_unknown` (row 2A.9, entry 230). Needs the merge report's writer for `exec mv edges_merged_filename`, and to know when a one-file `mff` made no report (Choice 01.C11). Merge reuse across points and runs lives in 03 and 09's cache.
- **04 · Expression:** `cudf` and `suggest_thresholds` (`sth`) with 04's names and reading arguments; `cudf`'s outputs optional when named (04.C11); Not DE removed in `cudf` before classification (04.C4); `observations.none` as a report (04.C5); the preview through `fusion --query` with the exact batch line (04.C8).
- **05 · Corridor:** `fb_rch` with its long name `corridor` and 05.C8's names (`edges_to_target` with `any`, `activation`, `expression`; `skip_subtypes`; `observations`); its observations default to the latest `cudf` result (Choice 01.C16, 05.C9); `corridor.empty` as a report whose skips follow Choice 01.C15 (row 4A.6), with 05's message; the run-level `corridor.skip_subtypes` on the `run` line and through `--set` (row 2B.2), with a line's own `skip_subtypes` winning for that line, then `--set`, then the `run` line (entry 229); the traces for `--debug` and Choice 01.C11.
- **06 · Solve:** `pathz3`'s FuSION 1 form (28 tokens plus up to 5; mode `1` an error; the relaxed lists ignored with a warning when not empty, S5.05) and row 1.7's names, with `answers_from` and the case `free` (row 2B.9). `fsc` is a set and the cases are `in`, `out`, `free` (06.C10); the cases run in parallel, which `--jobs` counts (06.C4); `points=` asks only listed points (06.C9). 06 owns `solve::Results` and the case statuses (row 1.5). Needs: a solve that honours the stop token and journals its checks for 09.C5's resume; the latest frontier and search log for Choice 01.C11.
- **07 · Results:** `psp`, `pex` and `plot_pareto`, with their tool needs (gnuplot 5.4 with `pngcairo` and the font for `plot_pareto`; `dot` for `static=svg`); `publish_solve`, which the batch layer calls after each solve step (row 1.5); positional `pex` and `psp` files bound by prefix, with an unmatched file an error before the run, `batch.unmatched_file` (07.C9); the one setting `legacy_prefix`, which writes FuSION 1's text files in `pathz3/legacy/` and at an absolute prefix (row 1.21); `two_column` and `legacy_limits` for the `PO.dat` and `limits_timefile.txt` moves of Choice 01.C11 (entry 230); named `pex` cases `in`, `out`, `free`, with `both` only positional (row 1.6).
- **08 · Enrichment:** `gsea` with 08.C11's names (`values`, `candidates`, `selector`, `shortlist`, `keep`; `seed` and `permutations` by name only), `shortlist` required, so this section's examples give it a bare name that lands in `gsea/` (row 2B.3); 08's `Shortlist` in the workspace (row 2B.14); `gsea.empty_selection` as a report.
- **09 · Runs:** the hub for this section, and the owner of what it follows: the vocabulary (`core::Failure`, `Report`, `Outcome<T>`, `Stopped`, `Log`, `Events`, `Context`, `Sha256`, `SettingSpec`; row 1.1); the name `<label>_<YYYYMMDD-HHMMSS>` and run id (09.C1), with the label and root from Choice 01.C7; adoption through `run.lock`; the layout, with `points/<NN>-<label>/` and one directory per step (09.C2); copies of inputs, read once (09.C3); saving every step (09.C4); resume as a new run with a parent, by fingerprint (09.C5); the `stop` file polled every 0.5 s and the 30 s grace hook (09.C7); signal handlers that only request the stop (09.C8); the dotted event set (09.C10, row 1.3); the run log, always written (09.C11); the check-only mode and `core::CheckReport` (09.C12, row 1.9); failures, codes and statuses (09.C14), to which this section adds 2 for usage errors; registry keys `<module>.<name>` (row 1.16); the output rule (row 2B.3). 01 gives 09: the plan as `run::StepSpec`s (each step's id, command, line, normalised line, point, settings and inputs, with produced inputs marked), the run-level options and settings for `run::Options`, the variables for `run.json` (row 1.14), its diagnostics in `core::CheckReport`, and `points.tsv`. 01 needs: the settings registry; `Registry` lookups for `load`; `Run::begin`, `end` and `fail` around each step.
- **10 · Frontend:** the frontend writes batch files by the rules under "The frontend's batch files", runs one run per sweep value (10.C21) and uses the `sweep` syntax only for exports, starts `fusion -b <dir>/run.batch --run-dir <dir>` in the run directory with its console discarded (10.C17), labels runs by session and tags the session id (10.C18), validates with `fusion --check --json`, builds blocks, rules and settings from `fusion --commands`, asks for previews and gene lookups through `fusion --query` (10.C15), resumes with `--resume <old dir> --run-dir <new dir>`, follows `events.jsonl`, and reads the exit statuses above. `--commands` gives each argument's type as 09's `core::Type` name (row 1.1), with its `list` and `takes` marks, so 10's `ArgType` mirror of this section's former types (`ID`, `GRAPH`, `POINTS` and so on) follows them. S8.20's "stop on any unfilled placeholder" maps onto the check's `batch.undefined_variable` warnings, which the frontend treats as errors.
- **11 · Build and platforms:** busybox-w32 bundled on Windows with its GPL-2.0 source offer, and its applets confirmed on a Windows build in Phase 4 (entries 188 and 205); the process layer, `core::Process` and `ProcessSpec` with `Output::file` (row 2A.11, row 1.15); the dependency check, `core::deps::Registry` and `Requirement`, and `deps.missing` (row 2A.1); the Phase 4 check that the cairo-only gnuplot accepts old lines' `set term png` (entries 204 and 205); `core::parse_decimal` for `real` values (11.C5); finding the executable's own path, for the sweep coordinator; `wmain`, a UTF-8 console and long paths on Windows; the home behind `$FUSION_HOME`, the `FUSION_HOME`, `FUSION_GNUPLOT` and `FUSION_DOT` overrides, and the bundled tools directory appended to `exec`'s `PATH` (11.C20); what `fusion --version --json` prints; `ctrl_c=false` for Z3 (11.C18); install errors through 09's `error` event (row 2B.16).
- **12 · Tests:** the hooks below, and the JSON Schemas of `--commands` and of `--check --json`'s output, which this section writes and section 12 keeps (row 2A.10).

## Test hooks

- **Lexer and variables, table-driven:** spaces, tabs and CRLF; quotes (`name="a b"`, `""`, `"x=y"`, unterminated); `$$` inside and outside `exec` lines; Windows paths with and without quotes; `;;` and tab-only lines; longest match (`$PLOT_FILE` and `$PLOT_FILE_WO_GENE`, `$ACTIVE_W` and `$ACTIVE_WO`); single-pass values; unmatched `$` on `exec` and other lines; `let` errors; `#LASTGRAPHNUM` before and after a graph.
- **Corpus replay:** the 2.0 front end must bind every line of the 116 corpus `*.batch` files and of `Linux/script.txt` with no error, with the expected warnings (one per FuSION 1 `mff` line) and exactly the 32 changed expansions in 14 files, which go on decision 0023's list of expected differences. The other two GUI scripts must fail with known errors: `Linux/advtempscript.txt` holds unfilled `%__…__%` placeholders, and `Linux/output_script.txt` has an empty `mff` list (line 149), an empty `wgx` file (line 157) and the thresholds 0.0 and 0.0, which S3.07 rejects. Fixtures: the batch files themselves, without their data; the check runs with input checks off.
- **Binder:** every positional form in the table against named equivalents, which must give equal `BoundArgs`; empty tokens; extra tokens; a missing `-1`; type errors (`1.01x`, `nan`, `0x1p0`, `1,5`, `""`).
- **Golden outputs:** `fusion -h`, `fusion -h fb_rch`, `fusion --commands` and `fusion --check --json` on broken fixtures, each checked against its JSON Schema.
- **Query mode:** `fusion --query` with a `cudf` line, a `suggest_thresholds` line and a `labels` line gives the same values as the batch commands, writes no file and makes no directory; a command that needs a run directory is refused.
- **Sweeps:** point order, rows times lists, labels and their reduction (row 1.26's shared golden list, Windows reserved names, case-only differences), two points writing one output, and the core budget with one, two and three cases per point.
- **`exec`, on each platform:** the status passes through; line 244's double spaces survive; the shell's output reaches the run log; the bundled tools come last on `PATH`; a stop during `exec` ends the whole process tree; on Windows, quotes and backslashes in the shell text arrive intact; a window plot is skipped without a display and run with one.
- **FuSION 1 moves:** each of the six names, with and without `--debug`, the destination files' content, FuSION 1's limits rendering, and the note after a one-file `mff`.
- **Outputs:** a relative, an omitted and an absolute output path, each landing as row 2B.3 says, with both paths recorded in `run.json`.
- **Settings:** a run-level setting from its default, the `run` line, `--set` and a step's own argument, each recorded in `run.json` with its source, in entry 229's rank: the step's own argument, then `--set`, then the `run` line, then the default; `debug` on the `run` line refused with `batch.bad_argument`.
- **Codes:** every check diagnostic in the error table carries its code in `--check --json`; a skipped window plot leaves only a run-log note.
- **Events:** every event the interpreter and the coordinator emit has a dotted name, the key `type` and the envelope with `step`, `line` and `point` inside steps, checked against 09's `events.jsonl` schema; a reported result appears only as `step.finished` with its `report`.
- **End to end:** the real batch's structure on a small synthetic graph (no KGMLs in the repository, S8.31), run in its FuSION 1 form and in a 2.0 named form, must give the same results.
- **Stop and resume:** stop at each kind of line, by the `stop` file and by Ctrl-C, then resume; the results must equal an uninterrupted run's, byte for byte. Failure in one sweep point; an empty corridor in another.
- **Performance:** lexing and expansion of the real batch under 1 ms; the full check of the real batch, with its 167-file list, under 50 ms on warm caches.

## Open questions

1. **Decisions that this section's choices amend.** *Closed:* all four were accepted as amendments: 01.C11 for S1.14 (entry 179), 01.C12 for decision 0021, S1.15 and S2.16 (entry 188), 01.C2's `$$` for S1.07 (entry 187), and 01.C15 widening S4.18's wording (entry 167). Choice 01.C20 adds a fifth, for S1.14 and S6.25 (entry 190).
2. **S1.28 in a sweep.** *Closed:* a failure stops only its own point, as the reading of S1.28 for sweeps (Choice 01.C5, row 4B.5; entry 188).
3. **Debug log and SMT dumps.** *Closed:* 06.C12 reuses answers and witnesses and never reads a dump back, so the dumps stay debug-only (entry 175).
4. **Is `start` still required?** *Closed:* `start` is optional, with the shipped maps by default, amending S1.15 (row 2B.4; entry 163).
5. **The FSC of a positional batch.** *Closed:* old batches keep their own `start` exception file, and the check's note `batch.no_fsc` names the gap and the fix (03 open question 1, row 2A.9; entries 165 and 230).
6. **busybox-w32's applets.** *Closed here:* moved to section 11's Windows build check, run once in Phase 4 (entries 188 and 205).
7. **Argument names across sections.** *Closed:* `pathz3`'s names by row 1.7 (entry 187), `fb_rch`'s by 05.C8 (entry 172), `gsea`'s by 08.C11 (entry 183); examples follow the owners (rows 1.29 and 1.31, entry 158).
8. **Status of check errors.** *Closed:* status 1, with `run.json`'s error code telling the cases apart (Choice 01.C14; entry 189).
9. **Window plots without a display.** *Closed:* skipped with a note when no display is available, as a new rule amending S1.14 and S6.25 (Choice 01.C20; entry 190).
10. **`debug` on the `run` line.** *Closed:* command line only. `debug` is the one run-level setting the `run` line does not take, and a `run` line that names it gets `batch.bad_argument` (Choice 01.C8; entry 229).
11. **A step's own argument against the `run` line and `--set`.** *Closed:* a step's own argument wins for that step, then `--set`, then the `run` line, then the registry default (entry 229).
