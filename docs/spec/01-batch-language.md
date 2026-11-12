# 01 · Batch language and run setup

## Scope

This stage covers how the `fusion` binary starts and reads its commands. That means the command line (`-b <file>`, `-h`, `-d`), the `sysbio>` prompt, how a batch line is split into tokens, `;;` comments, `let` variables and `$` expansion, the command echo, dispatch and unknown commands, and the script commands `start`, `exec`, `exit` and `help`/`h`. It also covers the four files `start` loads, the objects and global defaults `main` sets up before the first command (`Linux/src/main.cc:1046-1146` and the `GraphManagerNew` constructor at `Linux/src/GraphManagerNew.cc:7988-8181`), how the binary finds its bundled scripts (one of the two places where `macOS/src/main.cc` differs), and the non-script files in `Linux/supporting_files/`. Stage 2 takes over at the first pipeline command. This stage describes how `add`, `rgx`, `mff`, `wgx`, `upfx` and `sz` are dispatched; what they do is stage 2's, and so is how merging uses the maps `start` loads. Stages 3, 6 and 7 own the commands and helper scripts that this stage only dispatches or locates (`cudf`, `psp`, `pex`, `gsea`, `plot_po.sh`). Stage 8 owns the GUI, which writes the batch files and reads the echo described here. In quoted file lines, `<TAB>` stands for a tab character. "The real batch" means `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch` (273 lines), and its console log is `run_console.log` in the same directory. "The 81 batch files" means the 78 `*.batch` files under `runs/` and `macOS/model_example/` plus the three GUI scripts `Linux/output_script.txt`, `Linux/script.txt` and `Linux/advtempscript.txt`.

## Items

### S1.01 · `fusion -b <file>`: loading the batch file

- **FuSION 1:** `Linux/src/main.cc:1046-1082` (`main`), `Linux/src/main.cc:1148-1156` (first statements of the command loop), `Linux/include/macros.h:52` (`inf`)
- **What it does:** If `argv[1]` is exactly `-b`, FuSION opens `argv[2]` as the batch file. A relative path resolves against the launch directory. Every line is read with `getline` into a vector held in memory. Lines are split on `\n` only, nothing is trimmed, a `\r` stays in the line, and a last line without a trailing `\n` is still read. `cmdCount` is set to the number of lines, FuSION prints `Entering batch mode`, and batch mode is switched on. The file is read once, at startup, so editing it during a run has no effect. Arguments after `argv[2]` are ignored. The GUI starts FuSION in exactly this form: `new ProcessBuilder("./fusion", "-b", "output_script.txt")` (`Linux/frontend/src/Gui4Panel.java:153`; the copy at `IdEntryPanel.java:534-535` is never called, S8.28). The parallel sweep launcher runs `"$REPO/Linux/fusion" -b "<batch>" < /dev/null > run_console.log 2>&1` from each run's own directory (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/run_sweep.sh`).
- **Problems:**
  - When the file cannot be opened, FuSION prints `Could not open file` and carries on with an empty vector, batch mode off and `cmdCount = INT_MAX` (`main.cc:1063-1064`). The loop's first statement then reads `commands[0]` from the empty vector (`main.cc:1154`), which is undefined behaviour. A minimal program running the same statements (g++ -O0) dies with a segmentation fault, exit status 139.
  - An empty batch file takes the same path. `cnt_cmd == cmdCount == 0` switches batch mode off immediately (`main.cc:1152-1153`), and then `commands[0]` is read.
  - `fusion -b` with no file name passes `argv[2]`, which is NULL, to `ifstream` (`main.cc:1062`), because `argc` is never checked.
- **Proposed call:** fix. Keep `-b <file>` and reading the whole file first, but exit with a message and a non-zero status when the file is missing, unreadable or empty, instead of continuing into undefined behaviour.
- **Decision:** fix, as proposed (prompt log entries 021 and 022). Data inputs follow [decision 0021](../decisions/0021-required-and-optional-inputs.md).

### S1.02 · Usage text: `-h`, no arguments, other first arguments

- **FuSION 1:** `Linux/src/main.cc:1053-1109` (`main`), `Linux/src/main.cc:889-911` (`display_commands`)
- **What it does:** Only `argv[1]` is examined.
  - `fusion -h` prints `Run as "<argv0> -b <batch_command_file_name>"`, a blank line, `Partial listing of commands that can be used in batch_command_file:` and the command listing (S1.12), then exits with status 0.
  - `fusion` with no arguments, or with a first argument other than `-b`, `-d` or `-h`, prints `Run the tool as "<argv0> -b <batch_command_file_name>"` and the same listing, then calls `exit(-1)` (status 255).
  - All of this goes to stdout.
- **Problems:** None found. (Minor: the error case writes to stdout, not stderr.)
- **Proposed call:** keep. A usage message with status 0 for `-h` and a non-zero status otherwise is correct as it stands.
- **Decision:** keep, except that usage after a bad invocation goes to stderr; `-h` still prints to stdout and exits 0 (prompt log entries 021 and 022)

### S1.03 · Interactive prompt (`sysbio> `) and `-d`

- **FuSION 1:** `Linux/src/main.cc:1083-1089` (`-d`), `1118` and `1183-1187` (the prompt); the `if (!b_mode)` branch in every command block, e.g. `main.cc:1345-1360` (rgx), `5664-5678` (psp), `5702-5715` (pex), `5777-5786` (gsea); `Linux/src/GraphManagerNew.cc:7994-8158` (prompts inside `start`)
- **What it does:** With batch mode off, each loop iteration prints `sysbio> ` and reads one whitespace-delimited word with `cin >> command`. The command then reads its own arguments from `cin`, mostly with prompts of its own, such as `Tool-written: (y/n): ` and `Split node threshold: ` for rgx. In this mode:
  - there is no `$` expansion and no `;;` comment;
  - `let` is an unknown command, because its handler sits inside the batch-only branch (`main.cc:1249`);
  - `exec` runs `system("")` because there are no tokens;
  - `upfx` gets an empty prefix;
  - `start` prompts for its four files and accepts several exception files, ending at `-1` (`GraphManagerNew.cc:8079-8110`).

  Twelve commands carry a second argument parser based on `cin` for this mode: start, rgx, sz, fb_rch, mff, wgx, cudf, pathz3, psp, pex, gsea and add. `-d` prints `Interactive debug mode not supported in this version`, a usage line and the listing, and then does not exit (`main.cc:1083-1089`).
- **Problems:**
  - The prompt cannot be reached without undefined behaviour. Running with no arguments exits with usage (S1.02). `-d` and an unopenable `-b` file both enter the loop with an empty command vector (S1.01). The only remaining route is running off the end of a batch file (S1.04).
  - If the prompt is reached while stdin is at end of file (the sweep launcher redirects `< /dev/null`), `cin >> command` fails and leaves `command` holding the previous command. That command is then dispatched again on every iteration, without arguments, forever. A minimal reproduction printed `command='pex' fail=1` on three successive reads from `/dev/null`.
  - `macOS/model_example/RUNNING.md:206-209` tells users to run `psp`/`pex` "in an interactive session after the solve". The code does not support this.
  - Every command keeps two copies of its argument handling, one for this mode and one for batch mode.
- **Proposed call:** drop. No real run uses the prompt (all 81 batch files end with `exit`), it cannot be reached without undefined behaviour, and the FuSION 2.0 frontend replaces it. `-d` goes with it and becomes an unknown option (S1.02).
- **Decision:** drop, as proposed (prompt log entry 021)

### S1.04 · Reaching the end of the batch file without `exit`

- **FuSION 1:** `Linux/src/main.cc:1152-1154`
- **What it does:** At the top of each iteration, batch mode is switched off when `cnt_cmd == cmdCount`. The next statement still reads `commands[cnt_cmd]`, one element past the end of the vector, before the prompt is shown.
- **Problems:** Reading past the end is undefined behaviour. The vector grows by doubling, so for most files that slot lies inside the buffer but holds no constructed string; a 273-line file, for example, has capacity 512. If the process survives, it falls into the prompt (S1.03). With stdin at end of file, it then loops forever re-dispatching the last command. No real run has hit this, because all 81 batch files end with `exit`.
- **Proposed call:** fix. Reaching the end of the file behaves exactly like `exit`.
- **Decision:** fix, as proposed (prompt log entry 021)

### S1.05 · Splitting lines into tokens

- **FuSION 1:** `Linux/src/main.cc:1151-1181`. Commands read `token_cmd[k]` directly, e.g. `main.cc:1348`, `1363-1364`, `5681-5686`. `main.cc:4534-4554` and `5724-5725` test `token_cmd.size()`.
- **What it does:** Each line is split with `strtok(line, " ")`, so the space character is the only delimiter. Runs of spaces collapse, and leading and trailing spaces are dropped. There is no quoting and no escaping, so a token can never contain a space. Tabs and `\r` are ordinary characters and stay inside tokens. The tokens go into a `vector<string> token_cmd(1000000)` that is allocated fresh for every line. `token_cnt` counts the real tokens, and every other slot is an empty string, so a command that reads an argument the line did not supply gets `""`. A real case of a tab staying inside a token is `Linux/output_script.txt:306`, `let $PO_SEARCH_MODE_WITHOUT_FSC 0<TAB>`, which defines the value as `0` followed by a tab.
- **Problems:**
  - Avoidable cost. One million strings are built and freed for every line, blank and comment lines included, because the vector is allocated at `main.cc:1156`, before the blank and comment checks. A micro-benchmark of that one statement, built at -O0 like FuSION (its Makefile passes no `-O` flag), took 12.5 ms and 32 MB per line. That is about 3.4 s for the 273-line real batch. The longest line in all 81 batch files has 81 tokens.
  - `token_cmd.size()` is always 1,000,000, so no command can tell a missing argument from an empty one. pex's `if (token_cmd.size() > 13)` (`main.cc:5724`) and pathz3's `cmd_index < (int)token_cmd.size()` (`main.cc:4534-4554`) are always true, and no command checks `token_cnt`. An example of the consequence: if an `mff` line's list of map files lacks its `-1` terminator, the loop at `main.cc:2386-2399` walks the empty slots. It calls `read_naming_map("")` and appends an empty map about a million times, then reads past the end of the vector.
  - Tabs and `\r` are not delimiters. In a batch file with CRLF line endings every last token would end in `\r`, so `exit\r` would not be recognized and the run would fall off the end of the file (S1.04). None of the 81 batch files contains `\r`; three of them (the GUI scripts) contain the trailing tab quoted above.
  - `strtok` writes into the `std::string` buffer through a `const_cast` of `c_str()` (`main.cc:1154-1155`). That is undefined behaviour in principle, although it works with libstdc++.
- **Proposed call:** fix. Treat any run of spaces and tabs, plus a trailing `\r`, as one delimiter. Size the token list to the line, and give each command the real token count so that a missing argument can be told from an empty one. Keep the rule that there is no quoting.
- **Decision:** fix as proposed, and add quoting: a double-quoted token may contain spaces. `exec` lines, which carry shell quotes, bypass this (S1.14). (prompt log entry 023)

### S1.06 · Comments and blank lines

- **FuSION 1:** `Linux/src/main.cc:1161-1164`, `1165-1180`, `1190-1194`
- **What it does:** In batch mode:
  - a line with no tokens (empty, or spaces only) is skipped;
  - a line whose first token begins with `;;` is a comment and is skipped, so `;;` alone, `;; text` and `;;text` all count; real example, batch line 1: `;; **********************************************************************`;
  - a `;;` anywhere else in a line is an ordinary token and is passed to the command;
  - `#` does not start a comment;
  - comments and blank lines are not echoed.
- **Problems:** A line holding only a tab is not blank (S1.05). Its first token is a tab character, which is dispatched as an unknown command and prints the whole help listing. None of the 81 batch files has a tab-only line, a `#` line or an inline `;;`.
- **Proposed call:** keep. The rule fits every real file, and the tab case is fixed by S1.05.
- **Decision:** keep (prompt log entry 023)

### S1.07 · `$` variable expansion

- **FuSION 1:** `Linux/src/main.cc:1197-1247`
- **What it does:** In batch mode, every token of every non-comment line is expanded before anything else happens. That includes the first token and both operands of `let`. Each token is scanned from left to right. At each `$`, the defined keys are tried in byte order (the order in which the `std::map` iterates), and the first key that is a prefix of the rest of the token is replaced by its value. Scanning resumes after the inserted value, so a value is never scanned again. (Values were already expanded when they were defined, S1.08.) A `$` that matches no key is left unchanged, so shell text such as `$(date)` or awk's `$1` reaches `exec` intact. No delimiter is needed after a key, and the real batch files depend on that. Line 219, `let $PO_POINTS_WITH_FSC $SRC_NODE_to_$TGT_NODE_w_$NODE_TO_TEST_FOR_SIGNIFICANCE_$LOGFOLDCHANGES_b$REACH_PATH_BOUND`, produces `hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10`, as the echo of line 221 shows (`run_console.log:303`: `exec mv PO.dat /home/…/run/hsa1956_to_hsa3576_w_hsa3725_egf_hela_t120_lfc.tsv_b10.dat`).
  - **Empty versus undefined.** A key defined with no value expands to an empty token that keeps its position. `let $X` with nothing after it is how the GUI writes a blank field, e.g. `Linux/output_script.txt:30`, `let $ADDITIONALEDGES `. In `output_script.txt` this gives 16 empty argument tokens: in `start` (argument 1), `mff` (1 and 5), `wgx` (2), both `pathz3` lines (14, 15, 22 and 23) and three `exec` lines. A key that was never defined is not expanded at all, and the token keeps the literal text `$NAME`. The earlier lead, "an unset `$VAR` expands to an empty token", holds only for keys defined with an empty value. What empty tokens do to pathz3's positional arguments is stage 5's question (it is the difference at `macOS/src/main.cc:4535-4582`).
  - **Dispatch ignores expansion.** The command is dispatched on its first token as it was before expansion: `command = token_cmd[0]` runs at `main.cc:1190`, before the expansion loop at `1205`. So `$CMD args` never dispatches, although the echo prints the expanded token.
- **Problems:**
  - When one key is a prefix of another, the shorter key always wins, because it sorts first. Real case: 12 batch files in `runs/real_experiments/` (e.g. `experiment_6_on.batch:269` and `:275`) define `$PLOT_FILE` and then `$PLOT_FILE_WO_GENE`. The second definition's own key is expanded through `$PLOT_FILE` and is then rejected (S1.08). Every later `$PLOT_FILE_WO_GENE` therefore expands to `<value of $PLOT_FILE>_WO_GENE`. The plot meant to be `PO_…_combined.tsv.png` was written as `PO_hsa7124_to_hsa4792_fsc_hsa1432_final_tnfa_p38i_combined.tsv_b15.png_WO_GENE`, and that file is present in `runs/real_experiments/experiment6.1/`.
  - Undefined references are silent. A misspelt `$NAME` reaches the command as literal text, with no warning.
  - Each `$` costs a linear walk over all the keys (77 in the real batch). This is negligible.
- **Proposed call:** fix. Replace "the first key in sorted order that is a prefix" with "the longest defined key that is a prefix". Keep concatenation without delimiters, single-pass replacement, and literal pass-through of a `$` that matches nothing.
- **Decision:** fix as proposed, plus a warning for a `$NAME` that matches no defined key, except on `exec` lines. Every token, the command name included, is expanded before dispatch. (prompt log entry 023)

### S1.08 · `let <key> <value>`

- **FuSION 1:** `Linux/src/main.cc:1249-1282`
- **What it does:** Batch mode only. After expansion (S1.07), token 2 is the key and token 3 the value. Anything after token 3 is silently ignored, and a missing value becomes the empty string.
  - If the key does not start with `$`, the line is skipped and two lines go to stderr: `"let" to be used with macro key starting with $` and `<key>doesn't start with $; Skipping ...` (there is no space before "doesn't").
  - If the key is already defined, the old value is kept and stdout gets `Macro definition for <key>already present. Ignoring value <value>`.
  - Keys last for the whole run and cannot be unset; there is no scoping.
  - The value is expanded when the `let` runs, so a variable holds a snapshot of what it referred to (line 29: `let $WRITE_DIR $WORK_DIR/$WRITE_SUBDIR`).
  - `let` lines are not echoed. The real batch defines 77 variables.
- **Problems:**
  - The key token is itself expanded. A key that is already defined, or that begins with a defined key, has turned into that key's value before the `$` check runs. So redefinition fails with a misleading message: `let $X 2` after `let $X 1` reports `1doesn't start with $; Skipping ...`. The `already present` branch can only be reached in contrived cases, such as a value that itself begins with `$`. This is how the `$PLOT_FILE_WO_GENE` definition failed (S1.07). The failure is logged as `…tsv_b15.png_WO_GENEdoesn't start with $; Skipping ...` at `runs/real_experiments/experiment6.1/run.log:215`, and the same message appears in the 11 sibling logs.
  - Extra tokens after the value are dropped without a warning.
- **Proposed call:** fix. Take the key literally, never expanding it, and report a redefinition as an error that names the key. Keep first-definition-wins, expansion of the value when the `let` runs, and the empty value for a missing third token. Together with S1.07's longest match, this makes `$PLOT_FILE_WO_GENE` work.
- **Decision:** fix as proposed, and extra tokens after the value are an error too; no real file has any (prompt log entry 025) *Confirmed after stage 8 (prompt log entry 089):* the GUI's generated scripts wrote ids as `hsa1956 - egfr` and relied on the extra tokens being ignored, but none of those scripts is kept, and 2.0's frontend writes bare ids (S8.12), so the rule stands.

### S1.09 · `#LASTGRAPHNUM`

- **FuSION 1:** `Linux/src/main.cc:1263-1273`; `Linux/src/GraphManagerNew.cc:48` (static `graph_id_count = 0`), with the increments at e.g. `GraphManagerNew.cc:8416` and `8462` and the decrements at `9748` and `10112`
- **What it does:** If a `let` value is exactly `#LASTGRAPHNUM`, it is replaced by `graph_id_count`. This is a static counter that starts at 0 and is incremented before each new graph takes its id, so the first graph is 1. It is decremented again when a merge is abandoned. The result is the id of the most recently created graph, captured when the `let` runs. Real use: line 185, `let $MERGED_GRAPH_NUM #LASTGRAPHNUM`, comes after `rgx`, so that phase 2 restarts `fb_rch` from the merged graph: `fb_rch 84 hsa1956 -1 hsa3576 -1 -1 10 0` (`run_console.log:305`). A related convention belongs to stages 2, 4 and 5: graph id `0` given to `sz`, `fb_rch` or `pathz3` means "the latest graph" (`main.cc:1594-1595`, `2119-2120`, `4388-4389`). `#LASTGRAPHNUM` is the only `#` directive, and it is recognized nowhere but as a `let` value.
- **Problems:** Before `start`, when `graph_man == NULL`, stderr gets `Graph Manager not created yet.  Ignoring LASTGRAPHNUM directive.`, but the literal text `#LASTGRAPHNUM` is still stored as the value. It is then substituted wherever the variable is used.
- **Proposed call:** fix. When there is no graph manager, report the error and leave the variable undefined instead of storing the literal text. Keep the snapshot semantics.
- **Decision:** fix, as proposed (prompt log entry 025)

### S1.10 · Command echo on stdout

- **FuSION 1:** `Linux/src/main.cc:1294-1300`; `Linux/src/GraphManagerNew.cc:7975-7983` (the second echo inside `start`)
- **What it does:** After expansion, every batch line that is not a comment, a blank line or a `let` is printed to stdout before it runs. Each token is followed by one space, then comes a newline and a line of 31 dashes. `endl` flushes the echo, so it appears before the command's own output, including the shell output of `exec`. Real example (`run_console.log:6-7`):

  ```
  cudf /home/…/inputs/egf_hela_t120_lfc.tsv 1.01 -1.01 /home/…/run/up_reg /home/…/run/down_reg 
  -------------------------------
  ```

  The stage 8 GUI turns these lines into its progress display. `detectStage` (`Linux/frontend/src/Gui4Panel.java:197-210`; the copy at `IdEntryPanel.java:614-630` is never called, S8.28) trims each console line and tests `startsWith("exec mkdir")`, `"cudf"`, `"start"`, `"mff"`, `"wgx"`, `"rgx"`, `"fb_rch"` and `"pathz3"`, then `contains("gnuplot")` and `equals("exit")`.
- **Problems:** The first echo of every run shares its line with 450 leading spaces printed at startup (S1.24); the GUI's `trim()` hides them. The echo shows the expanded first token, even though dispatch used the unexpanded one (S1.07).
- **Proposed call:** keep. The GUI parses this exact format, so it acts as an interface.
- **Decision:** keep; how the Spring Boot frontend tracks progress is decided in Phase 2 (prompt log entry 025)

### S1.11 · Command dispatch and unknown commands

- **FuSION 1:** `Linux/src/main.cc:1305-5842` (the if/else chain), `main.cc:7939-7944` (the fallback)
- **What it does:** The unexpanded first token is compared, exactly and case-sensitively, against a chain of names in this order: `start`, `exec`, `unique_prefix`|`upfx`, `read_graph_xml`|`rgx`, `size`|`sz`, `fb_rch`, `merge_from_file`|`mff`, `write_graph_xml`|`wgx`, `create_up_down_file`|`cudf`, `get_path_with_z3`|`pathz3`, `plot_solution_paths`|`psp`, `plot_explanation`|`pex`, `gsea_select_pathways`|`gsea`, `help`|`h`, `exit`, `add`. `let` is handled before the chain, in batch mode only. Any other name prints `Did you enter one of the following valid commands?` and the full listing (S1.12), and the run carries on with the next line. Command names inside `#if 0` regions are out of scope.
- **Problems:** An unknown command, including a typo, neither stops the run nor changes the exit status (S1.28). The listing advertises `fwd_bkwd_rch`, which the chain does not accept (S1.12).
- **Proposed call:** keep. Exact matching on the first token, with the aliases above. The alias mismatch is S1.12 and the failure policy is S1.28.
- **Decision:** keep (prompt log entry 025)

### S1.12 · `help` / `h` and the command listing

- **FuSION 1:** `Linux/src/main.cc:5832-5835`, `main.cc:889-911` (`display_commands`)
- **What it does:** `help` or `h` prints a blank line, `Commands available in this (debloated) build: ` and another blank line. It then prints one line per command, with argument placeholders separated by tabs (e.g. `start <TAB><inter_db_map> <TAB><exception_merge_file> <TAB><hsa_to_symbol_map> <TAB><hsa_path_to_name_map>`), and ends with `exit`. The same listing follows the usage text (S1.02) and every unknown command (S1.11).
- **Problems:**
  - `fwd_bkwd_rch (or fb_rch)` (`main.cc:904`) advertises a name the dispatcher rejects, since only `fb_rch` is accepted (`main.cc:2109`).
  - `unique_prefix`/`upfx` and the long name `create_up_down_file` are accepted (`main.cc:1331`, `2629`) but not listed.
  - The `read_graph_xml` line calls its second argument `<merge_flag>`, but the code reads a tool-written `y`/`n` flag (`main.cc:1363`, `1367-1370`). What that flag does is stage 2's.
- **Proposed call:** fix. Build the listing and the dispatch from one table, so every advertised name works, every accepted name is listed, and the argument names match the code.
- **Decision:** fix, as proposed; the same table also supplies the argument names that named arguments need ([decision 0018](../decisions/0018-batch-interface.md)) (prompt log entry 026)

### S1.13 · `exit`

- **FuSION 1:** `Linux/src/main.cc:5837-5840`, `main.cc:7947`; `Linux/DSManagers/LogManager/LogManager.cc:16-23` (the destructor of the static logger)
- **What it does:** `exit` ends the loop, ignoring any arguments. FuSION closes the debug log (S1.25) and returns 0 from `main`. On the way out, the static logger's destructor prints `Number of Log entries made totally:0`, a blank line, ` LogManager Object is deleted ` and blank lines; this is the real tail of `run_console.log`. The graph manager is never deleted, which is harmless at exit.
- **Problems:** The exit status is 0 whatever failed earlier (S1.28). The two logger lines are noise from a component nothing uses (S1.24).
- **Proposed call:** keep. End the run with status 0 when nothing failed; the status after a failure is S1.28.
- **Decision:** keep (prompt log entry 026)

### S1.14 · `exec <shell command>`

- **FuSION 1:** `Linux/src/main.cc:1318-1330`
- **What it does:** `exec` joins the expanded tokens from token 2 on with single spaces, adds a trailing space, and passes the string to `system()`, which runs `/bin/sh -c`. The return value is discarded. Each `exec` gets a fresh shell, so a `cd` lasts only for its own line. The real batch chains commands with `;` instead (line 151: `exec cd $WRITE_DIR; echo $SRC_NODE > $SRC_FILE; echo $TGT_NODE > $TGT_FILE`). A `$` that matches no variable reaches the shell (S1.07). Because tokens are split on spaces and rejoined with one space, runs of spaces inside shell quotes collapse. Batch line 244, `exec echo "[egfr-cxcl8-jun] pathz3 wo  FSC  : start $(date)" >> $WRITE_DIR/timing.log`, wrote `[egfr-cxcl8-jun] pathz3 wo FSC : start Mon Sep 28 06:51:19 PM IST 2026` to `run/timing.log`, with the doubled spaces gone. In the real batch, 34 of the 273 lines are `exec`: `mkdir`, `echo` and `cat` into input files, `mv` of outputs, `date`, `awk`, and the `plot_po.sh` plot.
- **Problems:**
  - A failing shell command, such as a `cat` of a missing file or a failed `mkdir`, is ignored and the run continues.
  - Whitespace inside quoted shell strings is changed without warning.
  - The batch language relies on `exec` for file plumbing that the pipeline could do itself. The batch builds `connect_pairs`, `src_file`, `tgt_file` and the exception file this way (S1.17), and uses 8 `mv` lines to move fixed-name outputs out of the launch directory (S1.26).
- **Proposed call:** fix. Keep the shell pass-through, but report a non-zero shell status on stderr, with the batch line number, instead of discarding it.
- **Decision:** fix as proposed, and pass the rest of the line to the shell exactly as written after `$` expansion, so spacing inside shell quotes survives (prompt log entry 026)

### S1.15 · `start <inter_db_map> <exception_file> <symbol_map> <pathway_name_map>`

- **FuSION 1:** `Linux/src/main.cc:1305-1317`; `Linux/src/GraphManagerNew.cc:7967-7986` (`tokenize_cmd`), `GraphManagerNew.cc:7988-8181` (the constructor)
- **What it does:** `start` creates the graph manager that every graph command needs. The handler passes `[arg1, arg2 + " -1", arg3, arg4]` to the `GraphManagerNew` constructor, which:
  1. reads arg1 into `all_id_map` (S1.16);
  2. splits `arg2 + " -1"` on spaces, prints the tokens between two lines of 28 dashes, and reads each token up to `-1` as an exception file (S1.17). arg2 is one token, so batch mode takes exactly one exception file; an arg2 of `-1`, or an empty arg2, means none;
  3. reads arg3 into the display-name map and prints the map's size alone on a line (S1.18);
  4. reads arg4 into the pathway-name map and prints its size alone on a line (S1.19);
  5. sets `unique_prefix = ""`, `smt_dump_dir = ""` (SMT dumps go to the launch directory unless pathz3 changes this) and `z3_random_seed = -1` (`GraphManagerNew.cc:8177-8179`).

  A file that cannot be opened produces only a message and an empty map. The message goes to stderr for files 1, 3 and 4, but to stdout for the exception file (`GraphManagerNew.cc:8104`). Real output, `run_console.log:16-22`: the echo, then `----------------------------`, `/home/…/run/exception_file.txt -1 `, `----------------------------`, `6334` and `2349`.
- **Problems:**
  - No command checks that `start` has run. Eight commands use `graph_man` without a NULL check, at their first use: upfx (`main.cc:1338`), rgx (`1378`), sz (`1595`), fb_rch (`2120`), mff (`2389`), wgx (`2594`), pathz3 (`4389`) and psp (`5688`). Before `start` they crash. cudf, gsea, pex, add, exec, let, help and exit work without it, and the real batch runs `cudf` (line 145) before `start` (line 160).
  - Calling `start` twice leaks the first manager and starts with fresh maps, but the static graph counter is not reset (S1.09).
  - The ` -1` sentinel is a leftover from the interactive prompt for several files; batch mode cannot pass two exception files.
  - The two sizes are printed without labels.
- **Proposed call:** fix. Keep the four-file load and its output, but fail with a clear message when a command that needs the graph manager runs before `start`.
- **Decision:** fix as proposed; a named `start` file that cannot be read stops the run before it starts ([decision 0021](../decisions/0021-required-and-optional-inputs.md)) (prompt log entry 026)

### S1.16 · Inter-database id map (`start` argument 1)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8006-8022` (the reader), `8702-8733` (`get_rep_id_for_id`, `get_all_ids_for_rep_id`); `GraphManagerNew.cc:8307-8341` (`read_naming_map`, mff's second reading of the same file)
- **What it does:** For each line, `>> id >> repid` reads the first two whitespace-separated tokens and sets `all_id_map[id] = repid`. A later line with the same id wins, extra tokens are ignored, and a blank line adds the entry `""` → `""`. The map's size is not printed. Stage 2 uses the map when merging nodes.
- **Problems:**
  - No real example exists. All 17 copies of `inter_db_mapping_file` in the repo are 0 bytes, including the one in every EGFR run's `inputs/`, so the intended contents cannot be checked against data.
  - The batch passes the same file to `mff` (line 171: `mff $GSEA_KGML_LIST xml $NODE_SPLIT_THRESHOLD n $INTER_DB_MAP_FILE -1`). mff parses it again with `read_naming_map`, as a symmetric synonym map that can hold several values per id (it inserts both `nmap[a] ∋ b` and `nmap[b] ∋ a`). So one file is parsed twice, with two different meanings. Stage 2 owns mff's side.
  - `get_all_ids_for_rep_id` scans the whole map on every call (`GraphManagerNew.cc:8721-8733`). Its callers belong to stage 2.
- **Proposed call:** keep. Port the two-column reader as it is; stage 2 decides how merging uses the map and whether mff's second reading stays.
- **Decision:** keep (prompt log entry 027)

### S1.17 · Merge-exception file (`start` argument 2)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8072-8110`; used by `Linux/src/XMLParser.cc:925`, `958` and `1000` (stage 2)
- **What it does:** For each line, the first whitespace-separated token is added to `merge_exception_set`, and the rest of the line is ignored. In the real run the batch builds this file itself, at line 153: `exec cat $UP_REG_FILE $DOWN_REG_FILE $SRC_FILE $TGT_FILE $HSA_IDS_NOT_TO_BE_MERGED_FILE > $EXCEPTION_TO_NODE_MERGE_FILE`. The result, `run/exception_file.txt`, has 50 lines of the form `hsa1026`. `inputs/hsa_ids_not_to_be_merged.txt` holds `hsa1432`, `hsa5594` and `hsa5599`.
- **Problems:** cudf's up and down files are written to disk and come back in through a shell `cat` and this reader. That is a file round trip the pipeline could do in memory. It works because each line of the up and down files is a bare gene id (`run/up_reg` line 1: `hsa1026`), a format stage 3 owns.
- **Proposed call:** keep. Port the first-token-per-line set semantics.
- **Decision:** keep, and 2.0 adds cudf's up and down genes and the source and target to the exception set itself, as all 81 batch files do by hand. Old batches that still build the file keep working ([decision 0017](../decisions/0017-batch-file-compatibility.md)). Merging must therefore know the source and target; the stage 2 review and Phase 2 settle how. (prompt log entry 027)

### S1.18 · Gene-symbol (display-name) map (`start` argument 3)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8112-8144`; lookups at `GraphManagerNew.cc:8735-8753` and `8768-8786`, and at `Linux/src/XMLParser.cc:1004`, `1052` and `1258`
- **What it does:** For each line, `>> hsa_id >> name` sets `kegg_hsa_id_to_display_name_map[hsa_id] = name`, later lines winning, and the map's size is printed. The real `inputs/hsa_to_gene_symbol_map.txt` has 6334 lines such as `hsa7791<TAB>ZYX`, with 6334 distinct ids, and is printed as `6334`. Lookups through `get_display_names_from_rep_id` fall back to the id itself when the name is missing or empty (`GraphManagerNew.cc:8777-8781`).
- **Problems:** Only the first whitespace-delimited word of a name is kept; no name in the real file contains whitespace. Other lookups use `operator[]`, which inserts `""` for an unknown id. The comment at `GraphManagerNew.cc:8771-8776` connects this to a segfault when one session reads two graphs; stage 2 owns that.
- **Proposed call:** keep. Port the two-column reader, the first-word name and the fallback to the id.
- **Decision:** read names by the same rule as S1.19, so both maps share one reader; the fallback to the id stays (prompt log entry 027)

### S1.19 · Pathway-name map (`start` argument 4)

- **FuSION 1:** `Linux/src/GraphManagerNew.cc:8146-8175`; lookup in `get_pathway_names_from_path_id`, `GraphManagerNew.cc:8788-8799`
- **What it does:** For each line, `>> path_id >> path_name` sets `pathway_name_map[path_id] = path_name`, later lines winning, and the map's size is printed. Line 1 of the real `inputs/hsa_path_to_path_name_map.txt` is `path:hsa01100<TAB>Metabolic_pathways`; names use `_` in place of spaces. An id with no entry falls back to the id itself. The names go into stage 2's merged-edge listings and other outputs (e.g. `GraphManagerNew.cc:10820-10840`).
- **Problems:**
  - A name is cut at its first whitespace. 23 of the 2822 lines contain a second tab inside the name. For example, line 490, `pathway_420597<TAB>Nectin/Necl<TAB>trans_heterodimerization`, is stored as `Nectin/Necl`. All 23 have Reactome-style `pathway_…` ids; none of the 479 `path:hsa…` rows is affected.
  - The 2822 lines give only 2349 keys. 121 keys repeat, all with identical values. The last two lines, one holding only a tab and one empty, both add the key `""`.
  - The batch passes the same file to `pex` (line 269), whose Python script parses it again (stage 6).
- **Proposed call:** fix. Take the name as the rest of the line after the first tab, so names containing whitespace survive. Keep last-wins and the fallback to the id.
- **Decision:** fix as proposed. A further tab inside the name becomes an underscore, the file's convention for spaces, so tab-separated outputs don't break, and blank lines are skipped. (prompt log entry 027)

### S1.20 · Finding the bundled helper scripts (Linux and macOS differ)

- **FuSION 1:** Linux `Linux/src/main.cc:5728-5740` (pex) and `5796-5808` (gsea); macOS `macOS/src/main.cc:31`, `5756-5767` and `5823-5834`; `Linux/include/macros.h:30`
- **What it does:** pex and gsea each work out `exe_dir`, the directory of the running binary, and then run `python3 "<exe_dir>/supporting_files/<script>.py" …` through `system()`.
  - **Linux:** `readlink("/proc/self/exe", buf, 4095)`, then NUL-terminate. `exe_dir` is everything before the last `/`. `/proc/self/exe` gives the real path with symlinks resolved.
  - **macOS:** `_NSGetExecutablePath(buf, &size) == 0` (with `#include <mach-o/dyld.h>`), then the same split. This returns the path the binary was started by, which may be a symlink or a relative path (for `./fusion`, `exe_dir` becomes `.`). Symlinks are not resolved. So if `fusion` is a symlink in another directory, macOS looks for `supporting_files/` next to the link, while Linux looks next to the real binary.
  - **Both:** if the call fails, `exe_dir` falls back to `.`, the launch directory.
  - `python3` is whichever one comes first on `PATH`. The sweep launcher puts the repo's `.venv/bin` and a conda graphviz environment at the front of `PATH` (`run_sweep.sh`).
- **Problems:**
  - The same block (12 lines on Linux, 11 on macOS) is copied into both commands.
  - Three conventions for finding helpers coexist:
    - relative to the binary, for pex and gsea;
    - relative to the launch directory, `SUPPORTING_FILES_DIR "./supporting_files"` (`macros.h:30`), used only by dead code (S1.22);
    - an absolute path written into the batch file, for `plot_po.sh` (line 262: `exec cd $WRITE_DIR; bash /home/ks-149-u24/Desktop/arjun/FuSION/Linux/supporting_files/plot_po.sh …`).
- **Proposed call:** fix. Work out the install directory once, at startup, resolving symlinks the same way on both OSes, and use it for every bundled helper.
- **Decision:** fix, as proposed; whether any Python helpers remain is a Phase 2 question (prompt log entry 028)

### S1.21 · `supporting_files/config.txt`

- **FuSION 1:** `Linux/supporting_files/config.txt`; `Linux/src/main.cc:1120-1122`; `Linux/DSManagers/ConfigurationOptions/ConfigurationOptions.cc:31-41` and `182-221`
- **What it does:** Nothing in the live build. The file holds five `KEY=VALUE` lines in the format `t_ConfigurationOptions` reads, where lines starting with `;` or `#` are comments: `MODULENAME=test3`, `CYCLES=15`, `RUNFROM=READCDFG`, `CDFGFILE=test3.cdfg` and `SIGNALSFILE=SignalsToObserve`. These are settings for the hardware-verification tool the code came from. `main` builds `t_ConfigurationOptions config_options("")` with an empty file name, and the line that would have built the config path is commented out (`main.cc:1120-1121`), so the file is never opened. The constructor just prints `No configuration provided.`.
- **Problems:** A tracked file that looks like configuration but controls nothing.
- **Proposed call:** drop. Nothing reads it.
- **Decision:** drop (prompt log entry 028)

### S1.22 · Node-type, edge-type, edge-subtype and pathway encoding tables

- **FuSION 1:** `Linux/supporting_files/node_type.txt`, `edge_type.txt`, `edge_subtype.txt` and `path.txt`; `Linux/src/GraphManager.cc:58-122` (the old `GraphManager`); `Linux/Makefile:80` and `83`; `Linux/include/macros.h:20-21`
- **What it does:** Nothing in the live build. Each file maps a name to an integer code, with the delimiter ` => `:
  - `node_type.txt` has 6 lines, from `undefined => 0` to `other => 5`;
  - `edge_type.txt` has 6 (`PPrel => 1`, `PCrel => 2`, `GErel => 3`, `ECrel => 4`, `maplink => 5`);
  - `edge_subtype.txt` has 18, from `phosphorylation => 1` to `missing interaction => 17`;
  - `path.txt` has 41, from `path:hsa04010 => 1` to `path:HumanPPI => 40`.

  Only the constructor of the old `GraphManager` class would read the first three, and no live code constructs a `GraphManager`. Its paths would be wrong anyway. `SUPPORTING_FILES_DIR + "node_type.txt"` gives `./supporting_filesnode_type.txt`, with no `/` (`GraphManager.cc:80-81`). For pathways it reads `all_paths.txt`, which exists nowhere in the repo, rather than `path.txt` (`GraphManager.cc:117`). The live code instead hard-codes the subtype strings it tests, e.g. `EDGE_IS_EXPR_REPR` (`macros.h:76`).
- **Problems:** The Makefile lists all four files as prerequisites of `GraphManager.o` and `GraphManagerNew.o`, so editing any of them forces a rebuild that changes nothing.
- **Proposed call:** drop. No live code reads them; the subtype vocabulary that matters is the one hard-coded in stages 2 and 5.
- **Decision:** drop (prompt log entry 028)

### S1.23 · `supporting_files/color_files/`

- **FuSION 1:** `Linux/supporting_files/color_files/` (22 files); the former readers at `Linux/src/main.cc:1666-1717` and nearby, inside the `#if 0` region `main.cc:1646-2107`
- **What it does:** Nothing in the live build. The directory holds three kinds of file:
  - colour palettes, one `R G B` triple per line (`5_colors`, `10_colors`, up to `85_colors`);
  - colour maps of the form `<id><TAB>R G B` (`cMap_sample` line 1: `hsa1300<TAB>255 0 0`);
  - plain id lists (`cMap_4620.txt` line 1: `IFNB1`).

  The only readers were the graph-display commands (`display_graph`, `display_graph_richer`), which are now disabled. No live code, helper script or GUI file names these files.
- **Problems:** None found, beyond being dead data.
- **Proposed call:** drop. Nothing reads them.
- **Decision:** drop the colour files. A graph display feature is crucial for 2.0 and its design is still to be worked out ([decision 0022](../decisions/0022-graph-display.md)). (prompt log entry 028)

### S1.24 · Startup objects that no live command uses

- **FuSION 1:** `Linux/src/main.cc:1122-1136`; `Linux/DSManagers/ConfigurationOptions/ConfigurationOptions.cc:31-41`; `Linux/DSManagers/ExpressionManager/ExpressionManager.cc:506-662` (log files at `510-513` and `651-654`, rules file at `548-559`); `Linux/DSManagers/ExpressionManager/CWD.h` and its generating rule `create_hfile` in `Linux/DSManagers/ExpressionManager/Makefile:19-20`; `Linux/DSManagers/LogManager/LogManager.cc:4-23` and `33-42`; `Linux/DSManagers/DAGManager/rules_parser.l` (whose generated `rules_parser.yy.c:1076-1080` keeps flex's default `ECHO` rule)
- **What it does:** Before the first command, `main` builds:
  - `t_ConfigurationOptions("")`, which prints `No configuration provided.`;
  - a `t_ExpressionManager`:
    - its logger singleton prints `Log Manager Object is created`;
    - it tries to open `LogFiles/ExpressionManager.log` and `LogFiles/Expression.log` under the launch directory, and fails silently when `LogFiles/` does not exist;
    - it builds a DAG manager;
    - it parses `<build directory>/DSManagers/ExpressionManager/new_rules.txt`. This path is baked in at build time: the Makefile writes the output of `pwd` into `CWD.h` (here `/home/ks-149-u24/Desktop/arjun/FuSION/Linux/DSManagers/ExpressionManager`). If the file is missing, FuSION prints `ERROR : <path> Does not exist` and calls `exit(-1)`;
    - the rules lexer has no rule for the space character, so flex's default action echoes every space in the rules file to stdout. That is 450 spaces, exactly the number of spaces in `new_rules.txt`, with no newline;
  - a `Z3Solver`, a `z3::context`, a `z3::solver`, a `z3::expr_vector`, and three maps (`map_potential_conflict_nodes`, `map_nid_to_unrolled_graph_nids`, `map_eid_to_unrolled_graph_eids`).

  No live command uses any of these. `em` appears only where it is created (`main.cc:1123`), nothing passes it on, and `GraphManagerNew` holds no copy of it. pathz3 makes its own context and solver (`main.cc:4721-4722`). At exit the logger reports `Number of Log entries made totally:0`.
- **Problems:**
  - FuSION refuses to start if the source tree is moved or the binary is copied to another machine, and the cause is a component that does nothing.
  - Every console log begins with 450 spaces. All 77 run logs under `runs/` that echo a command show the same count.
  - A Z3 context and a DAG manager are built and thrown away on every run.
- **Proposed call:** drop. No live command depends on any of them.
- **Decision:** drop (prompt log entry 029)

### S1.25 · Debug log `mylog.txt`

- **FuSION 1:** `Linux/src/main.cc:42-51`, `1331-1340` (upfx), `1611-1613` and `1635-1641` (sz, under `DEBUG_FLAG`), `7947`; 75 `debug_log <<` statements outside comments and `#if 0` regions in `Linux/src/GraphManagerNew.cc` (e.g. the BFS and mincut traces at `2516-2678` and `11986-12452`)
- **What it does:** A global `ofstream debug_log("mylog.txt")` is opened in the launch directory during static initialization, before `main` runs, truncating any existing file. `sz` and the debug traces write to it when the build defines `DEBUG_FLAG` (S1.29). `upfx <prefix>` closes it and opens `<prefix>_mylog.txt`; upfx's other effect is stage 2's. `exit` closes it. Real content (`run/mylog.txt`, 472 bytes) begins `Number of nodes: 1156 (972 genes, 96 complexes, 88 compounds, 0 others)`, then `Number of edges: 2529`. At the end, batch files move it with `exec mv mylog.txt $WRITE_DIR/mylog.txt` (line 271).
- **Problems:**
  - Because the name is fixed and the file lives in the launch directory, two runs started there overwrite each other's log. The sweep launcher starts each job in its own directory for this reason (see the `run_sweep.sh` header). Stray empty logs sit in the repo: `FuSION/mylog.txt` and `Linux/mylog.txt`, both 0 bytes.
  - The comment at `main.cc:42-49` tells users to run `unique_prefix` first. But `upfx` before `start` uses the NULL `graph_man` (`main.cc:1338`) and crashes. Run after `start`, it splits the log into two files, as the comment admits. No real batch uses `upfx`.
- **Proposed call:** fix. Keep one debug log per run, but give it a per-run name instead of hard-wiring `mylog.txt` in the launch directory, and allow renaming it before `start` has run.
- **Decision:** fix, as proposed: one debug log per run with a per-run name, which can be set before `start` (prompt log entry 029)

### S1.26 · The launch directory as the implicit output location

- **FuSION 1:** No single location in the code. FuSION never changes its working directory, so every relative path resolves against the launch directory. The fixed-name outputs written there are: `mylog.txt` (S1.25); `PO.dat`, `limits_timefile.txt` and the `<lfc>_smt-files_<timestamp>/` dump directories (pathz3, stage 5); `fb_rch` and `fb_rch_back` (fb_rch, stage 4); and `edges_merged_filename` (mff/wgx, stage 2).
- **What it does:** Batch files keep runs apart by building absolute paths from `$WORK_DIR`. Line 26 is `let $WORK_DIR /home/ks-149-u24/Desktop/arjun/FuSION/runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025`, under the comment `$WORK_DIR MUST be an ABSOLUTE path` (line 23). Each fixed-name output is then moved with `exec mv` after it appears; that accounts for 8 of the 34 `exec` lines in the real batch. The GUI writes `output_script.txt` and runs `./fusion` in its own working directory.
- **Problems:**
  - Two runs started in one directory overwrite each other's `PO.dat`, `limits_timefile.txt` and logs. The `run_sweep.sh` header says so: "pathz3 writes PO.dat, limits_timefile.txt and the SMT dumps into the CWD".
  - Every fixed-name output is written to disk and then moved by a shell command.
  - A relative `$WORK_DIR` breaks without any error, because some `exec` lines `cd` first. `macOS/model_example/RUNNING.md:40-42` says the run then "exits 0 while quietly producing no plot".
- **Proposed call:** fix. Write every fixed-name output under one output directory per run instead of the launch directory. Each owning stage keeps its file names.
- **Decision:** fix, as proposed. The run's output directory is also where partial runs find saved steps ([decision 0019](../decisions/0019-partial-runs.md)); Phase 2 designs how a batch names it. (prompt log entry 029)

### S1.27 · Console number format

- **FuSION 1:** `Linux/src/main.cc:1142-1143`; `Linux/include/macros.h:84` (`PRECISION 2`)
- **What it does:** Before the first command, `std::cout` is set to fixed notation with 2 decimals, for the rest of the run. Every `float` or `double` that any command prints to stdout therefore shows exactly two decimals; for example, `rgx` prints `Time taken (in sec): 0.00` (`run_console.log`). Files written through their own `ofstream` keep the C++ default of 6 significant digits, unless the command writing them sets its own format. `FloatToString` (`main.cc:1039-1044`) uses the same precision, but no live code calls it.
- **Problems:** Values below 0.005 print as `0.00`, and the console and the output files show numbers at different precision. Both are minor.
- **Proposed call:** keep. Port the two-decimal fixed format for numbers on the console.
- **Decision:** fix: numbers on the console are printed with enough digits to be exact, not with two fixed decimals (prompt log entry 029)

### S1.28 · Failure handling and exit status

- **FuSION 1:** `Linux/src/main.cc:1148-7945` (there is no `try` around dispatch); `main.cc:4392-4395`, plus 27 `exit(1)` calls outside comments and `#if 0` regions in `Linux/src/GraphManagerNew.cc` (e.g. `4094`, `4116`, `4249`); number parsing that can throw, throughout the live command blocks (e.g. `main.cc:1364`, `2360`)
- **What it does:** Most failures print a message, inconsistently on stdout or stderr, and the batch carries on with the next line. Examples are unknown commands (S1.11), unreadable `start` files (S1.15) and failing `exec` lines (S1.14). Some errors call `exit(1)` at once, such as pathz3's `No such graph` (`main.cc:4392-4395`). The live command blocks parse numbers with `stoi`, `stof` or `stoul` 24 times, and only the 4 optional pathz3 arguments are inside a `try`/`catch` (`main.cc:4534-4554`). An empty or non-numeric value for a numeric argument, which an empty-valued variable produces (S1.07), throws `std::invalid_argument`. Nothing catches it, so `std::terminate` aborts the run with status 134. Otherwise the status is 0: all four sweep runs ended with `exit=0` (`sweep_status.log`). The GUI ignores the status and always reports "Execution Completed" (`Linux/frontend/src/IdEntryPanel.java:575-583`).
- **Problems:**
  - The exit status cannot tell a run that partly failed from a clean one. `RUNNING.md:40-42` documents one such silent exit-0 failure.
  - One blank numeric field aborts the whole run, and the message does not give the batch line.
- **Proposed call:** fix. Catch errors per command, report each failure with its batch line number, and exit non-zero if any command failed. Whether the batch should stop at the first failure is an open question.
- **Decision:** fix as proposed, and the batch stops at the first failure, exiting non-zero; a failing `exec` counts as a failure (S1.14). Partial runs restart from the last good step ([decision 0019](../decisions/0019-partial-runs.md)). (prompt log entry 030)

### S1.29 · Compile-time feature flags

- **FuSION 1:** `Linux/Makefile:10` (`CFLAGS`), applied to `main.cc`, `GraphManager.cc`, `GraphManagerNew.cc`, `XMLParser.cc` and `Z3Solver.cc` (`Makefile:71-90`)
- **What it does:** Every build defines ten flags, and they fix behaviour that no batch command can change: `QE`, `DUP_CHECK_FLAG_SET`, `DUMMY_EDGE_FLAG`, `DISCARD_INDIRECT_EDGES`, `COMPUTE_MINCUT_EDGES_FLAG`, `ONLY_INTERAC_EDGES_FOR_MINCUT`, `OPT_GAP_HEUR_FLAG`, `ASSERT_FLAG`, `DEBUG_FLAG` and `USE_FLOAT_EDGE_WEIGHTS_FOR_MINCUT`. The table counts the `#if`/`#ifdef` lines that test each flag outside comments and `#if 0` regions.

  | Flag | main.cc | GraphManagerNew.cc | XMLParser.cc |
  |---|---|---|---|
  | `DEBUG_FLAG` (gates the `mylog.txt` writes, S1.25) | 2 | 37 | 0 |
  | `ONLY_INTERAC_EDGES_FOR_MINCUT` | 0 | 30 | 0 |
  | `USE_FLOAT_EDGE_WEIGHTS_FOR_MINCUT` | 0 | 15 | 0 |
  | `OPT_GAP_HEUR_FLAG` | 0 | 12 | 0 |
  | `DUMMY_EDGE_FLAG` | 0 | 7 | 4 |
  | `DUP_CHECK_FLAG_SET` | 0 | 6 | 0 |
  | `ASSERT_FLAG` | 0 | 1 | 2 |
  | `DISCARD_INDIRECT_EDGES` | 0 | 0 | 1 |
  | `QE` | 0 | 0 | 0 |
  | `COMPUTE_MINCUT_EDGES_FLAG` (its one `#ifdef`, `main.cc:3662`, is inside `#if 0`) | 0 | 0 | 0 |

  What each flag does belongs to the stage whose code it gates: stage 2, 4 or 5.
- **Problems:**
  - `QE` and `COMPUTE_MINCUT_EDGES_FLAG` have no effect in the live build.
  - No `-O` flag is given anywhere, so FuSION's own code (graph building, merging, reachability, constraint generation) runs unoptimized. Only the Z3 library is optimized.
  - Nothing in a run's output records which flags were set.
- **Proposed call:** fix. Each owning stage documents its flag's fixed value as ordinary behaviour, and the two flags with no effect are dropped.
- **Decision:** drop `QE` and `COMPUTE_MINCUT_EDGES_FLAG`. (`QE` is also tested in the `DSManagers` code, but that code is built without it, because `DSManagers/Makefile.include:28-30` adds `-DQE` only when make itself is run with `QE` set, and S1.24 drops that code.) A flag that changes results becomes a run-time setting that a batch or the frontend can change, and its value is recorded in each run's output. Debug flags sit behind one debug option that the frontend doesn't show. Each flag's kind is confirmed in the stage that owns it (2, 4 or 5). (prompt log entries 030 and 031)

## Open questions

1. **What should a line of the inter-database id map look like (S1.16)?** All 17 copies in the repo are empty. `start` reads the file as a one-way `id → repid` map, while `mff` reads it as a symmetric list of synonyms. *Would settle it:* a non-empty example from FuSION's authors, or stage 2's reading of how merging uses `all_id_map` and `read_naming_map`.
   *Resolved for stage 1:* the two-column reader is kept (S1.16); stage 2's review decides how merging uses it.
2. **Should the batch stop at the first failing command, or carry on and exit non-zero (S1.28)?** FuSION 1 carries on. *Would settle it:* the user's choice, together with stage 8 on whether the frontend should act on the exit status.
   *Resolved:* the batch stops at the first failure and exits non-zero (S1.28).
3. **What does the real binary do for `fusion -d`, for `fusion -b` with a missing file, and at the end of a batch file with no `exit` (S1.01, S1.03, S1.04)?** All three are undefined behaviour. I reproduced only the first two, in a minimal program that segfaulted, and no real run hits the third. *Would settle it:* running the three cases against `Linux/fusion` with stdin from `/dev/null` during phase 3 reference testing.
   *Still open, for phase 3.* None of the three decisions depends on the answer.
4. **Does anyone use the `sysbio>` prompt outside the repo's batch files (S1.03)?** `RUNNING.md:206-209` suggests interactive sessions. *Would settle it:* the user.
   *Resolved:* the prompt is dropped (S1.03).
