# 10 · Frontend architecture

## Scope

This section designs the five frontend modules of Choice C1: `ui` (C1's `web`, renamed by 10.C11), `accounts`, `sessions`, `pipeline` and `runner`. It implements stage 8's decisions (S8.01 to S8.32 and the stage's open questions). It also covers the progress question that S1.10 left to Phase 2, which A2 settles, and the frontend side of S6.19 and S6.26: each run's results shown and linked from its session (S8.29).

The frontend is a JavaFX desktop application, the answer to 10.C1 (prompt log entries 191 and 192). It has a standalone window, like FuSION 1's, with screens written in Java on JavaFX (OpenJFX). Spring Boot (G2) stays as the application framework, for wiring, configuration and the services, with no embedded web server, no HTTP API and no local port.

It leaves to other sections:
- Phase 5 (decision 0027): every screen, navigation (S8.03) and the graph display. This section gives the screens their services and data, and fixes only what the desktop delivery implies for the display.
- Section 01: batch syntax, the command table and its JSON export (01.C9), and the `fusion` command line and exit statuses (01.C14).
- Section 09: the run directory, `run.json` (E1), `events.jsonl` (A2), the stop request (A1), liveness, the run log, saved steps and resume.
- Section 07: the result files and pages that the screens show.
- Section 04: the read-only queries behind the threshold preview (S8.14) and the threshold suggestions (S3.15 to S3.17).
- Section 11: installers, the bundled Java runtime with OpenJFX, signing, versions, and where the backend's own files are installed.
- Sections 02, 03, 05, 06 and 08: the commands and arguments that the blocks expose.

This revision applies every answer of prompt log entries 158 to 210 that touches the section, and the cross-section resolutions they accepted (cited by item number, such as 1.26). Where another section's draft and an accepted answer differ, this section follows the answer. The interface outlines are Java 21 (G2). The C++ side of each contract belongs to the section named with it. FuSION 1 code is cited at the frozen snapshot, with paths as in the FuSION 1 tree. Run data is cited under `FuSION/runs/`.

Evidence used below, measured on this machine on 2026-11-12, or read from published artefacts on 2026-11-16. Counts that grow while the user's runs continue carry their time:
- **Run lengths and memory.** At 23:55, 19 `run_time.txt` files under `runs/` held `/usr/bin/time -v` output.
  - The runs took from 8 min 0 s (`egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/resolve_wo_n7_e0`) to 52 h 7 min 36 s (`…/r6_mcl1_creb1`).
  - Each used 99 to 101 % of one CPU.
  - Peak resident memory ranged from 318,920 KiB (`egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr025`) to 1,982,592 KiB (`…/r5_mcl1_stat3`).
- **Runs at once.** The user's launch scripts start every run of a set at the same moment:
  - 4 FDR-sweep runs (`run_sweep.sh`, 2026-10-14);
  - 9 candidate runs (`run_all.sh`, 2026-11-09);
  - 8 FSC tests (2026-11-12 17:05). All 8 ended at 18:06:28 with exit status 143, which is SIGTERM (`egfr_b10_gsea_fdr044_candidates/status.log`);
  - 10 more at 18:20 and 7 more at 18:26. A one-point re-solve started at 17:05 (`r6_mcl1_creb1/resolve_wo_n2_e8`) was still running until 19:39, so 18 solves ran at once.
- **Launch scripts.** 12 shell scripts under `runs/` start `fusion`. All 12 send its output to a log file. 8 start each run in the background, and the 5 newest also take standard input from `/dev/null`.
- **Log and event volume** (at 23:59, while 13 runs were still writing):
  - 48 `run_console.log` files of 20,279 to 51,996 bytes, median 33,011;
  - 228 limits files with 2,860 solver checks, counted by their `Time taken` lines: a median of 14 per solve and at most 28.
- **Result sizes** (23:54 to 23:59): 193 solutions files, median 54,551 bytes, largest 118,223; 423 pex pages, median 734,210 bytes, 310 MB in all.
- **Graph sizes.** Section 05 found that FuSION 1's corridors hold 39 to 793 nodes and 134 to 3,328 edges across 29 configurations. The GUI run's merged network holds 82,188 relations (S8.23; 8,139 nodes in 02.C5's legacy read).
- **Inputs.**
  - The EGF LFC file has 3,054 lines (41,571 bytes).
  - The largest per-gene table, `runs/egf_hela/lfc/HeLa_log2FC_all_timepoints.csv`, has 22,284 lines (3,076,532 bytes).
  - FuSION 1's symbol map has 6,334 lines (153,153 bytes), and its pathway-name map 2,822 lines (147,681 bytes). 2.0 rebuilds both maps from sources with clear terms (prompt log entry 205), so the shipped files will differ.
  - The 167 KGMLs named by `auxilliary/list_of_kegg_xml_files.txt` total 6,853,555 bytes.
- **Disk.** FuSION 1's `runs/` holds 9.1 GiB. All 113 batch files use `exec`, in 3,752 lines.
- **Process start** (medians of 50, load average 14):
  - `/bin/true` starts in 0.68 ms.
  - FuSION 1's own `fusion`, which links `libz3.so.4`, answers `-h` in 10.64 ms, its static initialisers included. It creates an empty `mylog.txt` in its working directory even for `-h`, so it was run in a scratch directory.
  - Section 04 measured 18 ms for a stand-in of a whole threshold preview.
- **Password hashing** (load average 14):
  - bcrypt through the system's C library (libxcrypt, via Python's `crypt` module), median of 5: 0.062 s at strength 10 and 0.252 s at strength 12.
  - PBKDF2-HMAC-SHA256 at 600,000 iterations, through Python's OpenSSL binding: 0.842 s.
- **OpenJFX 21.0.13** (published artefacts, read 2026-11-16):
  - Maven Central lists 21.0.13 under `org/openjfx/`, dated 2026-10-20. Its jars per platform, in bytes:

    | Module | Linux x86-64 | macOS x86-64 | macOS arm64 | Windows x86-64 |
    |---|---|---|---|---|
    | `javafx-base` | 753,229 | 753,230 | 753,226 | 753,226 |
    | `javafx-graphics` | 4,684,494 | 4,859,596 | 4,824,423 | 5,911,208 |
    | `javafx-controls` | 2,582,586 | 2,582,596 | 2,582,611 | 2,582,586 |
    | the three together | 8,020,309 | 8,195,422 | 8,160,260 | 9,247,020 |
    | `javafx-web` | 45,169,113 | 41,230,625 | 40,158,729 | 37,603,369 |

  - Gluon's jmods bundles, which jlink takes, have every module, web included: 53,622,248 bytes for Linux x86-64, 50,009,754 for macOS x86-64, 48,814,394 for macOS arm64 and 47,109,232 for Windows x86-64.
  - Oracle's JavaFX 21 release notes: "JavaFX 21 is designed to work with both JDK 21 and JDK 17", and 21.0.13 updates WebView's engine to WebKit 625.1 (JDK-8380556).
  - JavaFX 25 is compiled for JDK 23 and needs JDK 23 or later (its build notes). JavaFX 23.0.2, which runs on JDK 21, has had no update since 2025-01-21.
  - Section 11 read OpenJFX 21's `module-info` files: `javafx.web` also requires `javafx.media`. It estimates the runtime image at about 60 to 75 MB without WebView and 165 to 200 MB with it, because jlink does not compress WebKit's native library (102 to 119 MB unpacked).
- **This machine.** A Xeon Gold 6140 with 18 cores of 2 threads each (36 logical CPUs), 124 GiB of memory, and OpenJDK 21.0.13.
- **FuSION 1's stored data.** The GUI's 3 session files hold 5 sessions of 814 to 1,468 bytes each. The account file is 304 bytes.

## Requirements carried in

Delivery:
- **10.C1, answered in prompt log entries 191 and 192.** The user asked: "why does it have to be a web app? the first fusion was a standalone app to interact w the front end", then chose "JavaFX desktop app: Standalone window, Java screens, native dialogs, no local server port." There is no lab-server mode in 2.0.

Accounts and sessions:
- **S8.02:** per-user sign-in and per-user sessions, with salted, slow password hashes in a store outside the repository, validated usernames, and no committed account file.
- **S8.04:** named per-user sessions that can be created, opened, run and deleted. Each is addressed by a stable id, and Cancel really cancels.
- **S8.05:** one record per session, keyed by its id, holding every input a run needs. Its paths are checked when it opens. Both pathway-source fields are kept, and the overrides are stored.
- **S8.06:**
  - Defaults come from the real batches: split threshold 1, edge restriction 0 (no restriction), relaxation lower bounds 0, path bound 10 and the shipped maps.
  - The LFC thresholds have no default: the user sets them or accepts a suggestion ([decision 0013](../decisions/0013-suggest-thresholds-in-scope.md)).
  - The poll's other option, also defaulting the upper bounds to 8 and 8 and the timeouts to the b10 runs' values, was not taken (prompt log entry 091).
- **S8.01:** the frontend finds the backend and its data through configuration, never through its working directory. No compiled classes go into the repository.
- **S8.31:** 2.0 ships the two reference maps, with their source and date, and a KGML list that is resolved against the user's own KGML directory. KGMLs, compiled classes, sessions, accounts and generated scripts stay out of the repository.
  - The list is `data/kegg_pathways.txt`: the 167 candidate pathways of FuSION 1's list as bare pathway ids, listed in `fusion-data.json` (2B.5; prompt log entry 204).
  - Both maps are rebuilt from sources with clear terms, recorded in `data/SOURCES.md`. KEGG pathway names come at run time from each KGML's own title (prompt log entry 205).
- **Stage 8 open question 2:** FuSION 1 sessions are not imported.

Inputs, checks and previews:
- **What a session holds (S8.07 to S8.16):**
  - a results location and an LFC file (S8.07);
  - an explicit pathway-source choice with its file required, and additional edges in both modes (S8.08);
  - a path bound of at least 1, and the edges-to-target restriction (S8.09);
  - solver settings with their units, invalid entries rejected, mapped onto S5.21's per-query timeout and whole-solve budget and S5.26's solutions on request (S8.10);
  - four relaxation bounds from 0 to 100, with lower ≤ upper (S8.11);
  - source and target required and the FSC optional, as bare ids found in the symbol map, entered through the symbol-aware lookup (S8.12);
  - two map choosers that default to the shipped maps, with a cross-database input only once section 02 designs id mapping (S8.13, S2.14);
  - the edge files and the coexpression inputs (S8.16).
- **S8.14:** a live preview of the up and down lists, computed by the backend's one LFC parser (S3.14), with the thresholds taken as typed. S3.07 makes up ≤ down an error.
- **S8.15 and stage 8 open question 3:** per-gene overrides are stored with the session and written as one-id-per-line files in the run directory. Not DE removes the gene from the observed up and down lists; `cudf` applies it before classification (04.C4, which narrows S3.08; prompt log entry 165).
- **S8.30 and [decision 0021](../decisions/0021-required-and-optional-inputs.md):**
  - Every input is checked before a run, by the frontend and again by the backend.
  - A missing required input, or an optional input that is named but unreadable, stops the run.
  - An optional input that is not given turns its feature off. One that is given but empty counts as an empty list.
  - 01.C12 amends decision 0021 for inputs that the batch itself makes: they are checked again just before their step (prompt log entry 188).
- **S8.17:** every value reaches the solve, and an unfilled value stops generation.

Pipeline and batch files:
- **[Decision 0018](../decisions/0018-batch-interface.md):** the batch language, with named arguments, positional FuSION 1 lines and sweeps, is the one interface.
- **S8.18:** the standard run is a default pipeline that the editor opens with. Users edit, add or remove blocks, and every Submit turns the drawn pipeline into a batch file. The editor's faults are fixed.
- **S8.19:** Gen Script writes a runnable batch file of the drawn pipeline. The block library matches the real commands.
- **S8.20:** one template owned with the backend, named arguments, every value checked and quoted where needed (S1.05), and a stop on any unfilled value. With S8.18, the template is the editor's default pipeline.
- **S8.22:** the backend builds the merge-exception set (S1.17, S2.13). The frontend supplies only the inputs. 03.C2 amends S8.22: genes confirmed up or down also join the set (prompt log entry 165).
- **S8.23 and [decision 0019](../decisions/0019-partial-runs.md):** the graph is built once per run and saved with it, and a later run can start from it.
- **S8.24:** additional edges are added only when a file is chosen, in both pathway modes.
- **S8.25:** the default pipeline solves both FSC cases with S5.17's named FSC on one corridor. With no FSC there is a single solve (S8.12). 06.C10 extends S5.17: the FSC is a set of ids, and the cases are `in`, `out` and `free` (prompt log entry 175).
- **S8.26 and S6.22:** the built-in Pareto plot is drawn once, to a file, and shown in the frontend.
- **S8.27:** every live step is a block, `exec` included. The default pipeline includes GSEA shortlisting, the solve, the plot and the explanations.
- **S8.32:** a quick Submit runs the default pipeline at once, without opening the editor. Read in prompt log entry 195: it runs the session's current pipeline, which is the default unless the user edited it.
- **Batch rules from stage 1:** double quotes for spaces (S1.05), a warning for an unknown `$NAME` (S1.07, which 01.C2 amends with `$$` for a literal `$`; prompt log entry 187), and no extra tokens after a `let` value (S1.08).
- **S1.29:** the frontend never shows or sets a debug option.
- **[Decision 0020](../decisions/0020-plug-and-play-inputs.md):** the network and the per-gene values are plug and play. The frontend assumes nothing about KEGG or microarrays beyond what the catalog and the default pipeline declare.
- **Partial runs (decision 0019):** stop after a step, start from a saved step, resume a stopped solve, and solve part of the frontier. **S1.28:** a failed run restarts from its last good step.
- **S7.18:** the GSEA shortlist is reviewed by stopping after `gsea`, or in the frontend.

Runs:
- **A1:** each run is one `fusion` child process, started on a batch file that the frontend writes into a new run directory. Stop is a stop request that works the same on every platform. Killing is the last resort.
  - 06.C19 amends A1: the resume of a killed or crashed run uses its journal (prompt log entry 174).
  - Of A1's two stop channels, 09.C7 keeps the `stop` file only (entry 184), and 09.C8's signal handlers only request that stop, as a reading of A1 (entry 185).
  - A1 covers runs only: read-only calls that make no run directory are outside it (1.10; entry 168).
- **A2 and S1.10:** progress comes from `events.jsonl`. The console echo stays, for people, and the frontend does not parse it.
- **S8.28:** a live log with a stage indicator, the real exit status, a Stop button, and no second run sharing a directory.
- **S8.21 and S1.26:** every run gets a new directory, never deleted or reused, which holds all its outputs and its full parameters.
- **S1.28:** a failure stops the batch and gives a non-zero exit.
- **E1:** `run.json` records the versions, the batch file and command line, every setting with its source, every input's path, size and SHA-256, the times, the exit status and the outputs.
- **S8.29, S6.19, S6.26, and decisions [0022](../decisions/0022-graph-display.md) and [0027](../decisions/0027-graph-display-in-phase-5.md):** each run's results are shown in the frontend and linked from its session. The display is designed in Phase 5, and it must be efficient and seamless.
- **Stage 8 open question 4:** whether the backend dies when the GUI exits stays open for Phase 3. This design removes the pipe that would kill it.

Accepted Phase 2 answers that this design follows (prompt log entries 158 to 210):
- **Names and formats:**
  - examples use the owners' names (1.29):
    - `fb_rch`, long name `corridor`, with `edges_to_target` `any`, `activation` or `expression` (05.C8);
    - `pathz3`'s `node_min`, `node_max`, `edge_min`, `edge_max`, `timeout`, `incremental_timeout` and `budget`, durations with units in the named form, and `coexpression_csv`, `coexpression_threshold` and `frozen_threshold` (01.C1, 1.7);
    - `gsea`'s `values`, `candidates`, `selector`, `shortlist` and `keep` (08.C11), with `kgml_dir` (02.C12, 1.23);
  - the FSC cases are `in`, `out` and `free` in arguments, files and JSON; with no FSC the solve runs `free` (06.C10, 1.6);
  - setting keys are `<module>.<name>`; the `run` line takes run-level settings, and `--commands` exports them (1.16, 2B.2);
  - dotted event names, with the envelope `v`, `seq`, `t` and `type`, plus `step`, `line` and `point` inside a step (1.3).
- **The batch language:**
  - quoting with `""` and `$$`, and relative input paths resolved against the batch file's folder (01.C2);
  - four predefined variables, `$FUSION_RUN_DIR`, `$FUSION_BATCH_DIR`, `$FUSION_POINT` and `$FUSION_HOME`, holding paths with `/` (01.C18);
  - an argument takes the latest value of its kind by default (01.C16);
  - `start` is optional, and the shipped maps are the default (2B.4);
  - exit statuses 0, 1, 2, 3 and 130 (01.C14).
- **Results:**
  - the reported results are exactly `corridor.empty`, `gsea.empty_selection` and `observations.none` (1.8, 04.C5). A case with no point, an infeasible case, no DE gene and an exhausted budget are case statuses, and the batch goes on;
  - after a reported result, every later line of that run is skipped, and the run ends with exit 0 (01.C15).
- **Calls and runs:**
  - `fusion --query` takes one batch line on standard input and answers in JSON. Read-only calls that make no run directory are outside A1 (1.10, entry 168);
  - section 02's `labels map=<file>` serves the gene lookup through `--query` (2B.6);
  - stop through the `stop` file only, which `fusion` checks every 0.5 s (09.C7; entry 184);
  - Ctrl-C and SIGTERM handlers make the same request, and SIGHUP is ignored (09.C8; entry 185);
  - liveness through `run.lock`, and a filesystem without locks stays usable with a warning (09.C9; entry 185);
  - lifecycle events, `solve.check.started` before each check, and rate-limited progress, with no heartbeat (09.C10; entry 185);
  - the resume of a killed or crashed run uses its journal (06.C19, which amends A1; entry 174);
  - cases are solved in parallel, and job limits count one core per case (06.C4; entry 173);
  - `rlimit` (registry key `solve.rlimit`) is a deterministic per-check budget, 0 for none, recorded in `run.json` (2B.8; entry 176).
- **Overrides and inputs:**
  - the Relaxed override and the relaxed-edges file are dropped (entry 165). The override dialog keeps Above Up, Not DE, Below Down, Active, Inactive and Non-relaxed, and the edge files are additional, essential, avoid and non-relaxed;
  - confirmed overrides reach `mff` through section 01's look-ahead and join the exception set (03.C2);
  - labels become directory names per Unicode code point, each run of characters outside `[A-Za-z0-9._-]` becoming one `-`, with the ends trimmed (1.26);
  - gnuplot 5.4 or later with `pngcairo`, found through `FUSION_GNUPLOT`, then the bundled copy, then `PATH` (1.27).

Platform and stack:
- **G2:** Java 21, Spring Boot and Gradle. 11.C21 amends G2's Spring Boot 3 to Spring Boot 4.1 (4.1.2), with the Gradle 9.9.1 wrapper (prompt log entry 200). Spring Boot remains the application framework here, without a web server.
- **G3:** Linux, macOS and Windows, all native, from one source tree. A biologist installs one thing.
- **F1:** no Python.

## Design

### Principles

1. **The frontend keeps no FuSION knowledge of its own.**
   - It learns the commands, their arguments, types, ranges, defaults, rules, stage labels and run-level settings from `fusion --commands`, the JSON export of section 01's command table (01.C9).
   - It learns the standard run, and which inputs each user's sessions should remember, from a pipeline document shipped with the backend (10.C12).
   - FuSION 1's GUI hard-coded all of these, and they drifted: three templates (S8.20), three placeholders never filled (S8.17), defaults that no batch used (S8.06), and a block library that contradicted the commands (S8.19).
2. **The frontend parses no FuSION input format.**
   - Batch checks go through `fusion --check` (09.C12).
   - The threshold preview and suggestions go through `fusion --query` (04.C8).
   - Gene lookups go through the same mode (10.C15).

   This is S3.14's "one parser" rule applied to the frontend. FuSION 1's GUI preview was a fifth LFC parser (S8.14). The frontend reads FuSION's outputs (`run.json`, `events.jsonl`, the run log, section 07's JSON), whose schemas section 12 keeps.
3. **A run owes nothing to the frontend.**
   - `fusion` writes its own run log (09.C11) and its events.
   - Stop is a file (09.C7), and liveness is a lock that the OS releases (09.C9).
   - The frontend can close, crash or restart without touching a running run (10.C16).

Components and data flow:
- **`ui`:** the JavaFX screens (Phase 5), the launcher and single-instance guard, native file dialogs, and the page opener. It calls the services below directly, in the same process.
- **`accounts`, `sessions`, `pipeline` and `runner`:** Spring-managed services. They never touch JavaFX, so Phase 3 tests them without a screen.
- `runner` starts two kinds of `fusion` process:
  - short-lived calls that print JSON: `--version`, `--commands`, `-b … --check` and `--query`;
  - one long-lived `fusion -b <dir>/run.batch --run-dir <dir>` per run.
- Each run writes `run.json`, `events.jsonl`, its run log and its results into its own directory under the session's results location. The runner follows those files and passes updates to the screens (10.C5).
- Nothing listens on a network port.

### Delivery and the running application (10.C1, 10.C3, 10.C4)

- **Package.** One JavaFX application, packaged by section 11's jpackage installers with a bundled Java 21 runtime, OpenJFX 21.0.13 (10.C3), the `fusion` binary, Z3 and gnuplot (G3). The launcher passes `-Dfusion.backend.executable=$APPDIR/backend/bin/fusion` (section 11).
- **Start.** The launcher's `main`:
  1. handles `--reset-password <username>` and exits, if given (Accounts, below);
  2. tries an exclusive lock on `<data dir>/frontend.lock`. If another instance holds it, `main` writes `<data dir>/frontend.focus`, which asks the running instance to bring its window to the front, and exits (10.C4). Two instances never share a data directory;
  3. calls `Application.launch`.
- **The Spring context** starts in `Application.init()`, which JavaFX runs on the launcher thread, not the FX Application Thread. It is a Spring Boot context with `WebApplicationType.NONE`: no Tomcat, no Spring MVC and no web security. Spring Security's `spring-security-crypto` alone supplies the password encoder (10.C9).
- **Backend checks at start,** on a worker thread while the sign-in screen shows:
  - `fusion --version --json` (section 11). It gives the versions, the install directory, the reference maps with their source and date, and the probe of every bundled tool. The frontend refuses a backend whose version differs from its own, since one `VERSION` file serves both (11.C16; prompt log entry 203);
  - `fusion --commands`, for the catalog (01.C9).

  If either fails, Submit is disabled with the reason, while sessions and results stay usable.
- **The window (10.C4).** A normal application:
  - one main window, with a Dock or taskbar entry;
  - Quit in the application menu, which JavaFX provides on macOS, and in the File menu elsewhere;
  - closing the main window quits on every platform.

  Quitting closes the Spring context. Runs continue (10.C16), and the screens say how many.
- **First start.** With no account yet, the sign-in screen opens on sign-up (10.C2).
- **Threads.** The FX Application Thread only draws and handles input.
  - Everything that can block runs on virtual threads: file reads and writes, hashing, `fusion` calls, and process starts and waits.
  - Results come back through `Platform.runLater`, coalesced to at most one hand-over per 250 ms poll tick (10.C5).
- **Memory.** The default maximum heap is 512 MiB, room for the stored data (10.C8) and for the Phase 5 display of a large network.

### What the delivery implies for the graph display (Phase 5)

Phase 5 designs the display (decision 0027). The desktop delivery fixes only this much:
- It is drawn in JavaFX, for example on a `Canvas`, in the application's own window.
- Corridors are small: at most 793 nodes and 3,328 edges (section 05).
- The merged network of the GUI run holds 82,188 relations. Drawing it needs batched drawing and culling of what lies outside the view.

### Security model (10.C2)

Nothing listens on a port, so the reach of the frontend is the reach of the OS login it runs under:
- **Other OS users** of the machine cannot read the frontend's data. Its data directory is created with owner-only permissions: POSIX `0700` for directories and `0600` for files. On Windows, `%LOCALAPPDATA%` already carries per-user permissions.
- **People who share one OS login,** such as a lab workstation with one login:
  - FuSION accounts (S8.02) keep each person's sessions and runs apart in the screens.
  - They do not stop someone with that login from reading the data directory or another person's run directories through the OS. The salted, slow hashes protect the passwords themselves if `accounts.json` is copied (S8.02's threat).
- **Sign-in:**
  - The same message for an unknown user and a wrong password: "Invalid username or password". FuSION 1 showed "Wrong password!" for both (S8.02).
  - An unknown user is checked against a dummy hash, so the answer takes as long either way. FuSION 1 returns at once for an unknown user (`Linux/frontend/src/LoginPage.java:517-521`).
- **Content of run directories.**
  - Section 07's HTML pages open in the user's browser from disk (10.C6). Browsers treat each `file:` page as its own origin, so a page cannot read other files. Section 07 also escapes `<` in its inline JSON.
- **Batch text.**
  - Session names may not contain control characters, and generated batches carry the session's id, never its name.
  - Values are written with `""` and `$$` (01.C2), and a value with a control character is refused.
- **`exec` blocks** are offered (S8.27). They run shell commands as the user, which the user can already do.

### The `ui` module (10.C4, 10.C5, 10.C6, 10.C7)

- **Screens.** Phase 5 designs them. Each gets its services by constructor injection: `FXMLLoader`'s controller factory is the Spring context's `getBean`.
- **Calls.** A screen calls `AccountService`, `SessionStore`, `PipelineValidator`, `BatchGenerator`, `RunService` and `ResultsIndex` directly, on a virtual thread, and receives their results on the FX thread.
- **Following runs (10.C5).**
  - The runner's poller tails each active run's `events.jsonl` and run log (09.C10, 09.C11), and sends `RunMessage`s to its subscribers.
  - The `ui` module subscribes once, and merges each tick's messages into observable models, such as a run's state, progress, stage, and log lines.
  - The models change in one `Platform.runLater` per tick. The cost is the same with 1 run or 18: at most 4 hand-overs a second.
  - Opening a finished run reads its two files from the start: 30 to 40 KB of events and 20 to 52 KB of log.
- **Files (10.C7).** JavaFX's `FileChooser` and `DirectoryChooser` give native dialogs with real paths, unlike a browser, which never reveals a chosen file's path.
  - A dialog opens in the folder of the input's current value, or else in the last folder used for that input.
  - A path may also be typed or pasted. Both kinds pass the same checks (S8.30).
- **Results (S8.29).**
  - Native views show:
    - a run's state, reports and case statuses;
    - its log;
    - its frontier table, from `results.json` (07.C1);
    - its Pareto plot, `pareto.png`, shown by `ImageView`.
  - Section 07's run page and explanation pages open in the system browser through JavaFX's `HostServices.showDocument` (10.C6), which needs neither AWT nor `javafx-web`.
- **Threading rule** (above): no service call, file read or process wait ever runs on the FX Application Thread.

```java
package fusion.frontend.ui;   // imports omitted; Phase 5 designs the screens

public final class Launcher {                              // jpackage's main class
    public static void main(String[] args);                // --reset-password, single instance, Application.launch
}
public final class FusionApplication extends javafx.application.Application {
    @Override public void init();                          // Spring context, WebApplicationType.NONE
    @Override public void start(javafx.stage.Stage stage); // sign-in, then the main window
    @Override public void stop();                          // context closed; runs continue (10.C16)
}
final class SingleInstance {                               // 10.C4
    static Optional<SingleInstance> claim(Path dataDir);   // empty when another instance holds frontend.lock
    static void askToFocus(Path dataDir);                  // writes frontend.focus for the running instance
}
interface FxDispatch { void post(Runnable update); }       // coalesced into one Platform.runLater per tick
interface PageOpener { void open(Path page); }             // 10.C6: HostServices.showDocument
interface FileDialogs {                                    // 10.C7
    Optional<Path> chooseFile(Window owner, String title, Optional<Path> startIn, List<String> extensions);
    Optional<Path> chooseDirectory(Window owner, String title, Optional<Path> startIn);
}
```

### Accounts (10.C2, 10.C9, 10.C10)

- **Record:** id (UUID), username, display name, role, password hash, creation time, and preferences, with no email address (10.C10).
  - Every account gets the role `user`, and nothing in 2.0 reads it: under 10.C2 every account is equal.
  - The field is kept for a later owner or admin feature, as the user chose (prompt log entry 198).
- **Sign-up (10.C2):** the sign-in screen also offers "Create a new account", as FuSION 1's did. At first start, with no account yet, it opens there.
- **Preferences:**
  - the default results location;
  - remembered inputs: values for the inputs that the default pipeline marks `remember`, which new sessions copy.

  The KGML directory of S8.31 is such an input (`kgml_dir`). The Java code names no FuSION input.
- **Usernames:** trimmed and lower-cased with `Locale.ROOT`, then matched against `[a-z][a-z0-9_-]{1,31}`. Usernames never become paths, because files are named by UUID. S8.02's `../` hole is closed by construction.
- **Passwords:**
  - 8 to 72 bytes in UTF-8; 72 bytes is bcrypt's limit.
  - Hashed with bcrypt at strength 12 through `spring-security-crypto`'s `DelegatingPasswordEncoder` (10.C9).
  - The stored form names its scheme, `{bcrypt}$2a$12$…`, so a later scheme replaces it at the next sign-in.
- **Password reset:** there is no email, so a reset runs from the command line: `fusion-frontend --reset-password <username>`, by someone with access to the OS login. On Windows it runs through the console launcher that section 11 adds (prompt log entry 199).
- **Signed in** until the user signs out or quits.

```java
package fusion.frontend.accounts;   // imports omitted; InputValue is pipeline's

public record AccountId(UUID value) {}
public enum Role { USER }                       // 10.C10: unused in 2.0, every account is USER
public record Preferences(Optional<Path> resultsDir, Map<String, InputValue> rememberedInputs) {}
public record Account(AccountId id, String username, String displayName, Role role,
                      String passwordHash, Instant created, Preferences preferences) {}

public interface AccountService {
    Account create(String username, String displayName, char[] password);   // role USER
    Optional<Account> authenticate(String username, char[] password);   // dummy hash for unknown users
    void changePassword(AccountId id, char[] current, char[] next);
    void resetPassword(String username, char[] next);                    // command line only
    void setPreferences(AccountId id, Preferences preferences);
    Optional<Account> find(AccountId id);
}
// Errors: InvalidUsername, UsernameTaken, InvalidPassword.

public final class UsernamePolicy {
    public static final Pattern VALID = Pattern.compile("[a-z][a-z0-9_-]{1,31}");
    public static String normalise(String raw);   // trim, then lower case with Locale.ROOT
}
```

### Sessions (10.C8)

What a session holds (S8.05):
- **Identity:** an id (a random UUID), the owner, and a name. The name is trimmed, 1 to 100 characters long, has no control characters, and is unique per owner, compared ignoring case after Unicode NFC normalisation.
- **Bookkeeping:** creation and change times, and a version number.
- **`inputs`:** one typed value for each input that the default pipeline declares (S8.07 to S8.16).
  - Both pathway-source files are kept, whichever source is chosen (S8.05).
  - Empty values are written as explicit nulls, never dropped. FuSION 1 lost keys through org.json's `put(key, null)`.
- **`overrides`:** for each gene, at most one value in each group (S8.15, entry 165):
  - observation: `confirmed_up` (Above Up), `not_de` (Not DE) or `confirmed_down` (Below Down);
  - activity: `active` or `inactive`;
  - relaxation: `nonrelaxed`.

  Not DE goes to `cudf` (04.C4). The other kinds go to the solve's lists of the same names (06). Section 01's look-ahead also hands the confirmed lists to `mff`, so the confirmed genes join the exception set (03.C2).
- **`pipeline`:** absent until the user edits the pipeline; then the edited document (S8.18).
- Runs are not listed in the session. Each run record names its session, so there is a single source.

The store (10.C8):
- **Where:** each session is a folder, `<data dir>/sessions/<id>/`.
  - `session.json` holds the record.
  - `overrides/<kind>.txt` holds the current override lists, rewritten from the record on every change. Section 04's preview reads the Not DE list from this file (04.C8).
- **Writing:**
  1. Serialise the record and write it to a temporary file in the same directory.
  2. Force it to disk.
  3. Rename it over the old file with `Files.move(…, ATOMIC_MOVE, REPLACE_EXISTING)`. On Windows, the rename is retried up to 5 times, waiting 50 ms and doubling each time, when a virus scanner holds the file.

  A crash leaves either the old file or the new one, never half of one.
- **Conflicts:** each change states the version it was based on, and a mismatch is a `VersionConflict`. Two screens editing one session, such as the inputs and the pipeline editor, cannot silently overwrite each other. FuSION 1's save addressed sessions by list position and could overwrite the neighbouring session (`Linux/frontend/src/UserInput.java:352`).
- **Index:** at start, the store reads every `session.json` once and keeps (id, owner, name, change time) in memory. 1,000 sessions of 10 KB each make 10 MB, read in well under a second.
- **Format changes:** each file carries `"format": 1`. A later format is converted on load by code. Inputs that a new default pipeline no longer declares are kept and flagged as unused.

Operations:
- **Create:** a blank name, a name with a control character, or one already taken after trimming, is refused, and nothing is created. Cancel and blank names therefore never open a session (S8.04).
- **Open, change and rename:** a change touches one input, one override, the pipeline or the name.
- **Delete:** refused while a run of the session is queued or running, which the store learns through `SessionUse`. Run directories are never deleted (S8.21), and the screen says where they stay.

`SessionUse` is an interface that `sessions` declares and `runner` implements, so `sessions` never depends on `runner` (10.C11).

Checks when a session opens (S8.05):
- **Quick, before the session shows:**
  - every path-valued input is checked for existence, readability and kind (file or directory). An empty file is a valid input, an empty list (0021), and is never reported as a problem;
  - every direct input file is hashed and compared with the hash in the `run.json` of the session's last run (E1, through `SessionUse`), so a screen can say "changed since the last run".

  Measured cost of hashing: 0.31 ms for the EGF LFC file and 17.4 ms for the 3.08 MB table.
- **Deep, afterwards:** the session's pipeline is generated and checked with `fusion --check` (Pipeline, below). Its problems arrive when it finishes.

```java
package fusion.frontend.sessions;   // imports omitted; InputValue, GeneOverride and PipelineDocument are pipeline's

public record SessionId(UUID value) {}
public record Session(SessionId id, AccountId owner, String name, Instant created, Instant updated,
                      long version, Map<String, InputValue> inputs, List<GeneOverride> overrides,
                      Optional<PipelineDocument> pipeline) {}
public record SessionSummary(SessionId id, String name, Instant created, Instant updated) {}

public interface SessionUse {                         // implemented by runner (10.C11)
    boolean busy(SessionId session);                  // a run is queued, starting, running or stopping
    Map<Path, String> lastRunInputHashes(SessionId session);   // from the last run's run.json
}

public interface SessionStore {
    List<SessionSummary> list(AccountId owner);       // newest change first, then name
    Session create(AccountId owner, String name);
    Optional<Session> get(AccountId owner, SessionId id);
    Session update(AccountId owner, SessionId id, long basedOnVersion, UnaryOperator<Session> change);
    void delete(AccountId owner, SessionId id);       // refused while SessionUse.busy
}
// Errors: InvalidSessionName, SessionNameTaken, VersionConflict, SessionBusy.

public record PathProblem(String input, Path path, Kind kind) {
    public enum Kind { MISSING, UNREADABLE, NOT_A_FILE, NOT_A_DIRECTORY, CHANGED_SINCE_LAST_RUN }
}
public interface SessionChecks {
    List<PathProblem> quick(Session session);         // stat and hash only
}
```

### Pipeline

#### The command catalog (01.C9)

At start, the runner calls `fusion --commands` and caches the result under `fusion`'s version.

What the catalog lists for each command:
- name and aliases, such as `fb_rch` and its long name `corridor` (05.C8);
- stage label, summary and description;
- the kinds of value it needs and the kind it produces, from section 01's `ValueKind`: maps, network, corridor, observations, shortlist and results (D1);
- whether it is debug-only (S1.29);
- its arguments, as section 01's type table gives them (prompt log entry 230), each with:
  - a name and its registry key, `<module>.<name>`;
  - a type from 09's `core::Type`: `boolean`, `integer`, `real`, `duration`, `text`, `choice`, `input_file`, `input_list` or `output_name`;
  - section 01's two marks: `list` for several values, comma-separated in the named form, and `takes` for the kind of in-memory value the argument names, the latest of that kind by default (01.C16);
  - its need (`required`, `optional`, `defaulted` or `computed`), default, minimum and maximum, choices, help text and debug flag;
- its cross-argument rules, of section 01's four kinds `less`, `less_or_equal`, `requires` and `excludes`, such as `less(down, up)` (S3.07), `less_or_equal(node_min, node_max)` (S8.11) and `requires(coexpression_threshold, coexpression_csv)`.

The catalog also lists the run-level settings, keyed `<module>.<name>`, with their registry defaults, that a `run` line takes (1.16, 2B.2, 09.C12).

The catalog carries no mark for shell commands. 2B.15's mark existed so that lab mode could refuse `exec` blocks, and it is dropped (prompt log entry 199); `exec` blocks are offered like any other (S8.27).

This is decision 0021's one set of rules:
- section 01's command table declares the rules;
- `fusion` enforces them when it runs or checks a batch;
- the frontend evaluates the same declared rules while the user types;
- whatever needs the contents of a file is checked by `fusion --check` alone (09.C12).

What the editor shows:
- Blocks are catalog commands.
- Language statements (`let`, `run`, comments, `sweep`) are written by the generator and are never drawn.
- Debug-only arguments are hidden.

Example, in the owners' names:

```json
{
  "format": 1,
  "fusion": "2.0.0",
  "commands": [
    {
      "name": "fb_rch",
      "aliases": ["corridor"],
      "stage": "Reachability corridor",
      "summary": "Keep the nodes and edges on paths of at most b edges from the sources to the targets",
      "needs": ["network"],
      "produces": "corridor",
      "args": [
        {"name": "graph", "setting": "corridor.graph", "type": "integer", "takes": "network",
         "need": "defaulted", "default": "0"},
        {"name": "source", "setting": "corridor.source", "type": "text", "list": true, "need": "required"},
        {"name": "target", "setting": "corridor.target", "type": "text", "list": true, "need": "required"},
        {"name": "bound", "setting": "corridor.bound", "type": "integer", "need": "required", "min": 0},
        {"name": "edges_to_target", "setting": "corridor.edges_to_target", "type": "choice", "need": "defaulted",
         "default": "any", "choices": ["any", "activation", "expression"]}
      ],
      "rules": [],
      "debug": false
    }
  ]
}
```

Section 01 writes the export's JSON Schema (01.C9); the field names here mirror its `ArgSpec` and 09's `SettingSpec`. The bound's minimum is the registry's 0, and the default pipeline's own input asks for at least 1 (S8.09).

#### The pipeline document

- **Contents:** a pipeline is a JSON document of declared inputs and blocks.
- **A block** has an id, a command, its argument bindings, an optional condition, and a canvas position. The position is layout only; generation ignores it.
- **A binding** is one of:
  - a literal;
  - an input of the pipeline;
  - the value that another block produces (its command's `produces` kind);
  - a saved step of an earlier run (decision 0019);
  - `first_of` a list of these, which takes the first one that is bound.
- **A condition** enables a block only when inputs hold given values, or are set at all; every condition in its list must hold. A disabled block emits nothing, and its outputs are unbound.
- **An input** declares its name, its type from the catalog's types with the `list` mark, its required flag, default, unit, range, choices, label, group and help text. It also says whether it may be swept, and whether each user's sessions remember it (`remember`). A `hint` adds what 09's types leave out: `gene_id` for the symbol-aware lookup and the symbol-map check (S8.12), and `directory` for a folder dialog and check (10.C7).
- **Connections:**
  - The edges of the drawn graph are the output bindings. They carry the catalog's value kinds: the producing block's kind must equal the `takes` mark of the argument it feeds, so a corridor cannot feed a network argument.
  - An input with the `list` mark binds only to an argument with that mark. A single value binds to one as a list of one item.
  - A connection compiles to section 01's rule that an argument takes the latest value of its kind (01.C16). The validator refuses a connection to a value that is no longer the latest of its kind at that point in the order, and says why.

#### The default pipeline (10.C12)

- **Where it lives:** in the repository at `share/fusion/pipelines/standard.json`, which installs into `$FUSION_HOME` (11.C14; prompt log entry 202). It is owned and tested with the commands it calls. The frontend reads it at start. The runner checks it against the catalog and refuses Submit, with the reason, if it does not fit.
- **Its blocks** are the real batch's steps in their 2.0 form (S8.25 to S8.27):
  1. the maps (`start`), only when the session names a map other than the shipped ones (2B.4);
  2. classify (`cudf`), with the Not DE list (04.C4);
  3. shortlist (`gsea`), when the source is a KGML list and shortlisting is on;
  4. the network: merged from the shortlist or the whole list (`mff`), or the pre-merged graph (S8.08, 03.C7), with additional edges and the not-merged ids when given (S8.22, S8.24);
  5. the corridor (`fb_rch`, S4.20);
  6. the solve (`pathz3`), with the FSC named as one or more ids, an input with the `list` mark, its cases `in` and `out` on one corridor, or `free` when there is no FSC (S5.17, S8.12, 06.C10);
  7. the Pareto plot (`plot_pareto`, S6.22);
  8. the explanations (`pex`, S6.26).
- **No `exec` line.** FuSION 1's 3,752 `exec` lines in 113 batch files did file plumbing that 2.0's commands now do themselves (S1.17, S1.26, S6.22, S8.22).
- **Defaults:**
  - S8.06's: split threshold 1, no edge restriction, relaxation lower bounds 0, path bound 10 and the shipped maps;
  - the shipped KGML list as the pathway list, resolved against `kgml_dir`, an input that each user's sessions remember (S8.31, 02.C12);
  - GSEA on, with `fdr:0.25`;
  - no default for the LFC thresholds;
  - no default for the relaxation upper bounds and the per-query timeout, with section 06's values offered as a one-click suggestion (10.C13);
  - every argument the pipeline leaves unbound keeps the backend's default (10.C14).
- **Paths under the installation** are values of their own type, stored as `{"install": "<relative path>"}`. The generator writes them through `$FUSION_HOME` (01.C18), so a batch stays valid on another installation.
- **Edits:** a session that the user has not edited uses the current default pipeline. Editing copies the pipeline into the session, recording its origin (`standard@2.0.0`).
- **Command-line copy:** section 11's build writes the default pipeline out as `share/fusion/pipelines/standard.batch`, through the same generator.

Excerpt:

```json
{
  "format": 1,
  "origin": "standard@2.0.0",
  "inputs": [
    {"name": "up", "type": "real", "required": true, "label": "Up threshold", "group": "Expression"},
    {"name": "down", "type": "real", "required": true, "label": "Down threshold", "group": "Expression"},
    {"name": "network_source", "type": "choice", "choices": ["kgml_list", "merged_graph"], "default": "kgml_list",
     "label": "Pathway source", "group": "Network"},
    {"name": "kgml_list", "type": "input_list", "default": {"install": "data/kegg_pathways.txt"},
     "label": "Pathway list", "group": "Network"},
    {"name": "kgml_dir", "type": "input_file", "hint": "directory", "remember": true,
     "label": "KGML folder", "group": "Network"},
    {"name": "use_gsea", "type": "boolean", "default": true, "label": "Shortlist pathways with GSEA", "group": "Network"},
    {"name": "gsea_selector", "type": "text", "default": "fdr:0.25", "label": "GSEA selector", "group": "Network"},
    {"name": "bound", "type": "integer", "default": 10, "min": 1, "unit": "edges",
     "label": "Maximum signalling path length", "group": "Corridor", "sweepable": true},
    {"name": "fsc", "type": "text", "list": true, "hint": "gene_id",
     "label": "Node to test for significance", "group": "Solve"},
    {"name": "node_max", "type": "integer", "required": true, "min": 0, "max": 100,
     "label": "Node relaxations, at most", "group": "Relaxation"}
  ],
  "blocks": [
    {"id": "gsea", "command": "gsea",
     "when": [{"input": "network_source", "equals": "kgml_list"}, {"input": "use_gsea", "equals": true}],
     "args": {"values": {"input": "lfc"}, "candidates": {"input": "kgml_list"}, "selector": {"input": "gsea_selector"},
              "shortlist": {"literal": "gsea_kgml_files.txt"}, "kgml_dir": {"input": "kgml_dir"}},
     "x": 120, "y": 80},
    {"id": "network", "command": "mff",
     "when": [{"input": "network_source", "equals": "kgml_list"}],
     "args": {"list": {"first_of": [{"output": "gsea"}, {"input": "kgml_list"}]},
              "split_threshold": {"input": "split_threshold"}, "kgml_dir": {"input": "kgml_dir"}},
     "x": 320, "y": 80},
    {"id": "corridor", "command": "fb_rch",
     "args": {"source": {"input": "source"}, "target": {"input": "target"}, "bound": {"input": "bound"},
              "edges_to_target": {"input": "edges_to_target"}},
     "x": 520, "y": 80}
  ]
}
```

`data/kegg_pathways.txt` holds the 167 candidate pathways of FuSION 1's list as bare pathway ids, and `fusion-data.json` lists it (2B.5; prompt log entry 204).

#### Validation (S8.30)

Each step runs only if the steps before it passed, except that paths missing at step 4 wait for step 5's answer:
1. **Inputs**, against their declarations and the catalog's rules: types, ranges, choices, required values and cross-argument rules. This is instant.
2. **The pipeline:**
   - unknown commands or arguments;
   - required arguments left unbound after conditions and `first_of` are applied (S8.20's stop on an unfilled value, S8.17);
   - kind mismatches, connections to a value that is not the latest of its kind, and more than one binding for one argument;
   - cycles. Kahn's algorithm finds whether a cycle exists. Tarjan's algorithm then gives the strongly connected components, and the message names the blocks of each component that holds a cycle. Both are O(B + C) for B blocks and C connections. FuSION 1's check could never fire, because its ordering added any unvisited block whenever its queue emptied (`Linux/frontend/src/gui4.java:3094-3102`).
3. **Gene ids:** every value of an input with the `gene_id` hint (the source, target and FSC of the default pipeline) and every override's gene must be in the symbol map in use (S8.12), looked up in the cached map (10.C15).
4. **Paths:** every path the batch will name exists and is readable. An empty file is accepted as an empty list (0021). A file that the batch writes before reading it is checked only just before its step (01.C12, which amends decision 0021 for such inputs; prompt log entry 188), so a missing path counts as an error only when `fusion --check` reports it too.
5. **`fusion -b <batch> --check --json`** (01.C14, 09.C12, 1.9) on the generated batch:
   - It runs in a scratch directory that holds the batch and its side files, never in a run directory, so a refused Submit leaves no run directory behind.
   - Its `batch.undefined_variable` warnings count as errors, for S8.20's "stop on any unfilled placeholder".

How problems are reported:
- Every problem names its block, argument or input. The backend's diagnostics, keyed by batch line and argument, are mapped back through the source map.
- Submit is refused while any error remains. Warnings are shown and do not block. *Sprint (prompt log entry 325):* they are shown when Submit succeeds too, and the run's record keeps them, as RunView's sixth component, warnings.

#### Generating a batch (10.C14)

`generate(pipeline, inputs, overrides, catalog, options)` is a pure function: no clock, no random numbers, no I/O. It works in seven steps:
1. **Resolve:** evaluate the conditions and bindings, and drop disabled blocks.
2. **Trim:** with `stop_after`, keep only that block and the blocks it depends on, so the batch ends after it. This is decision 0019's "stop after a step", which section 01 writes as an `exit` after that step (01.C6).
3. **Order:** topological, with ties broken by the blocks' order in the document, so one pipeline always gives one order. FuSION 1 kept its block graph in `HashSet`s of blocks that use identity hash codes (`gui4.java:3063-3092`; `FunctionBlock`, line 3176, defines no `hashCode`), so the order of independent blocks could differ between two runs of the GUI.
4. **Write**, in this order:
   - header comments: the frontend's version, the session's id, and the pipeline's origin and SHA-256, taken over its canonical JSON (sorted keys, no spaces). The session's name is never written;
   - for an export of a swept session, one `sweep` line per swept input, in section 01's syntax (01.C3);
   - a `run` line with the run-level settings the pipeline binds, if any (1.16, 2B.2). The default pipeline binds none. The runner never passes `--set`, so the batch carries every setting the frontend chose;
   - one `let` for every other input that a block uses, in declaration order, named `$` plus the input's name in upper case (`lfc` becomes `$LFC`). A swept input gets no `let`, since its `sweep` line defines it and S1.08 makes a redefinition an error;
   - one line per block: the catalog name, then `name=value` for each bound argument, in catalog order. An argument bound to an input is written as that input's variable; a literal is written in place. Unbound optional arguments are left out, so the backend applies its default and records it as a default (E1);
   - `exit`.
5. **Values,** following section 01's rules for the batch files the frontend writes (01.C2):
   - integers in decimal;
   - reals as the shortest decimal that reads back exactly: Java 21's `Double.toString`, then `BigDecimal`'s `stripTrailingZeros().toPlainString()`, giving `1.01`, `-1.01` and `0.0000001`. Never through a locale, since a German or French locale would write `1,01`;
   - durations with a unit, as the named form requires (`4h`; 01.C1);
   - gene ids bare (S8.12, S1.08);
   - a list value as its items joined by commas, as section 01's `list` mark reads them, with an item that holds a comma refused;
   - paths absolute and native, since backslashes are ordinary characters in 01.C2;
   - an installation path is written as `$FUSION_HOME/<relative path>`;
   - a value with a space, a tab or a `"`, or an empty value, is double-quoted;
   - inside any other value, `"` is written `""` and `$` is written `$$`, so a user's path never expands a variable;
   - a value with a control character is refused, since a batch value cannot hold a line break (01).
6. **Side files:** overrides become one-id-per-line files, `overrides/<kind>.txt`, only for kinds that have genes. The batch names them as `$FUSION_BATCH_DIR/overrides/<kind>.txt`, so the same text works in a run directory and in a Gen Script export (S8.15).
7. **Encoding:** `\n` line endings, UTF-8 without a byte-order mark (01.C19).

Output: the text, its SHA-256, the side files, and a source map from line numbers to blocks and from (line, argument) to inputs. Cost: O(B log B + A) for B blocks and A arguments. The standard run's batch below has 7 command lines, 25 named arguments and 21 `let` lines.

One generator serves every use:
- **Gen Script (S8.19):** a `.batch` file, or a zip of the batch and its `overrides/` folder when the session has overrides. It runs with `fusion -b` anywhere. A swept session exports one batch with `sweep` lines.
- **Sweeps (10.C21):** for runs, the generator is called once per swept value, with that value fixed. Each batch is the plain batch that the exported sweep file runs for that value.
- **Quick Submit (S8.32):** generates from the session's current pipeline, which is the default pipeline unless the user edited it (entry 195), without opening the editor.
- **The editor's Submit:** generates from the session's pipeline, from the editor.

Partial runs (decision 0019) need nothing beyond the catalog and the bindings:
- stop after a step: `stop_after`;
- start from a saved step: a `saved` binding, written as section 01's `load run=<dir> step=<step>` and offered from the earlier run's `run.json` (01.C6);
- resume a stopped solve, or restart a failed run from its last good step (S1.28): the runner's Resume (below);
- part of the frontier, or one FSC case: arguments of the solve block (`cases`, `points`, and the relaxation window; 06.C9).

GSEA review (S7.18): run with `stop_after` set to the `gsea` block, read the table, then start a second run that loads the saved shortlist.

```java
package fusion.frontend.pipeline;   // imports omitted; depends on no other frontend module

public enum ArgType { BOOLEAN, INTEGER, REAL, DURATION, TEXT, CHOICE, INPUT_FILE, INPUT_LIST,
                      OUTPUT_NAME }                     // 09's core::Type, as section 01's type table uses it
public enum Need { REQUIRED, OPTIONAL, DEFAULTED, COMPUTED }                  // 09's core::Need
public enum ValueKind { MAPS, NETWORK, CORRIDOR, OBSERVATIONS, SHORTLIST, RESULTS }   // 01's ValueKind
public record ArgSpec(String name, String setting, ArgType type, boolean list, Optional<ValueKind> takes,
                      Need need, Optional<String> defaultValue, OptionalDouble min, OptionalDouble max,
                      List<String> choices, boolean debug, String help) {}
                      // setting: the registry key "<module>.<name>"; list and takes: 01's two marks
public enum RuleKind { LESS, LESS_OR_EQUAL, REQUIRES, EXCLUDES }             // 01's RuleKind
public record Rule(RuleKind kind, String a, String b) {}                     // e.g. less(down, up), S3.07
public record CommandSpec(String name, List<String> aliases, String stage, String summary, String help,
                          List<ArgSpec> args, List<Rule> rules, List<ValueKind> needs,
                          Optional<ValueKind> produces, boolean debug) {}    // needs, produces: 01's Effects
public record SettingSpec(String key, ArgType type, Need need, Optional<String> defaultValue,
                          String help) {}                                    // a run-level setting, <module>.<name>
public record Catalog(int format, String fusionVersion, List<CommandSpec> commands,
                      List<SettingSpec> runSettings) {
    public Optional<CommandSpec> command(String nameOrAlias);
}
public interface CatalogProvider { Catalog catalog(); }       // implemented by runner

public sealed interface InputValue {
    record Text(String value) implements InputValue {}
    record Int(long value) implements InputValue {}
    record Real(double value) implements InputValue {}
    record PathValue(Path value) implements InputValue {}
    record InstallPath(String relative) implements InputValue {}     // written as $FUSION_HOME/<relative>
    record GeneId(String value) implements InputValue {}
    record Choice(String value) implements InputValue {}
    record Flag(boolean value) implements InputValue {}
    record Items(List<InputValue> items) implements InputValue {}    // an input with the list mark
    record Sweep(List<InputValue> values) implements InputValue {}   // sweepable inputs only
    record Unset() implements InputValue {}                          // stored as null
}
public enum Observation { CONFIRMED_UP, NOT_DE, CONFIRMED_DOWN }
public enum Activity { ACTIVE, INACTIVE }
public enum Relaxation { NONRELAXED }                                // S5.05; entry 165
public record GeneOverride(String geneId, Optional<Observation> observation,
                           Optional<Activity> activity, Optional<Relaxation> relaxation) {}

public sealed interface Binding {
    record Literal(InputValue value) implements Binding {}
    record Input(String name) implements Binding {}
    record Output(String block) implements Binding {}                // the value that block produces
    record Saved(Path runDir, String step) implements Binding {}
    record FirstOf(List<Binding> options) implements Binding {}
}
public record Condition(String input, Optional<InputValue> equals) {}   // empty: the input is set
public record Block(String id, String command, Map<String, Binding> args, List<Condition> when,
                    double x, double y) {}
public enum InputHint { GENE_ID, DIRECTORY }       // what 09's types leave out (S8.12, 10.C7)
public record InputDecl(String name, ArgType type, boolean list, Optional<InputHint> hint, boolean required,
                        Optional<InputValue> defaultValue, Optional<String> unit, OptionalDouble min,
                        OptionalDouble max, List<String> choices, boolean sweepable, boolean remember,
                        String label, String group, String help) {}
public record PipelineDocument(int format, String origin, List<InputDecl> inputs, List<Block> blocks) {}

public enum Severity { ERROR, WARNING }
public record PipelineProblem(Severity severity, Optional<String> block, Optional<String> argument,
                              Optional<String> input, String code, String message) {}
public interface PipelineValidator {
    List<PipelineProblem> validate(PipelineDocument pipeline, Map<String, InputValue> inputs, Catalog catalog);
}

public enum Target { RUN, EXPORT }          // EXPORT writes sweep lines; RUN fixes one swept value
public record Provenance(String frontendVersion, String sessionId) {}   // never the session's name
public record GenerateOptions(Target target, Optional<String> stopAfter, OptionalInt sweepValue,
                              Provenance provenance) {}
public record SideFile(String relativePath, List<String> lines) {}
public record SourceMap(Map<Integer, String> blockByLine, Map<String, String> inputByLineAndArgument) {}
public record GeneratedBatch(String text, String sha256, List<SideFile> sideFiles, SourceMap sourceMap) {}
public interface BatchGenerator {
    GeneratedBatch generate(PipelineDocument pipeline, Map<String, InputValue> inputs,
                            List<GeneOverride> overrides, Catalog catalog, GenerateOptions options);
}
public interface DefaultPipeline { PipelineDocument standard(); }
```

### Runner

#### Calling the backend for checks, previews and lookups (09.C12, 04.C8, 10.C15)

How the calls run:
- `BackendClient` starts `fusion` with an argument list, never through a shell. It reads JSON from standard output and kills the process after a timeout (default 30 s).
- At most 4 calls run at once; more wait their turn.
- These calls write nothing and make no run directory, so they are outside A1 (1.10, entry 168).

The calls:
- **`fusion --version --json`** (section 11): at start (Delivery, above).
- **`fusion --commands`** (01.C9): the catalog.
- **`fusion -b <batch> --check --json`** (01.C14, 09.C12, 1.9): the whole-batch check, with every input checked as decision 0021 requires.
- **`fusion --query`** (04.C8, 1.10): one batch line on standard input, one JSON answer on standard output. The frontend uses three queries:
  - the classification preview (S8.14): the `cudf` line it will submit, with the session's Not DE file. The answer gives every gene's class exactly as `cudf` would write it. On the EGF file at ±1.01, with FOSB set to Not DE, that is 42 up, 2 down and 3,010 none (section 04's example). The frontend adds the symbols from its map;
  - threshold suggestions (S3.15 to S3.17, decision 0013): the `suggest_thresholds` line, answered with `thresholds.json`'s content, the histogram's bins included (04.C7);
  - the symbol map (S8.12): section 02's `labels map=<file>` line, whose answer lists the map's id-label pairs, read by the backend's map reader (2B.6, 10.C15).

Caching and cost:
- Results are cached in `<data dir>/query-cache/`, keyed by the SHA-256 of the input files, the request and `fusion`'s version. Hashing the EGF LFC file takes 0.31 ms.
- A call costs a process start plus its work. Here `/bin/true` starts in 0.68 ms, and FuSION 1's own `fusion` answers `-h` in 10.64 ms, an upper bound for a start with Z3 linked. Section 04 measured 18 ms for a stand-in of the whole preview.
- The preview is requested 250 ms after the last keystroke, and a newer request cancels an older one.

Gene lookup (S8.12):
- The map's pairs are held in memory, once per map file.
- A query scans every entry (FuSION 1's map has 6,334) and ranks exact id or symbol matches first, then prefixes, then substrings. Within each class it orders by symbol, then id.
- It returns the top 20: O(N log 20).

#### Run records and states

Each run has one record, `<data dir>/runs/<id>.json`. It holds:
- the frontend's run id, the session, the owner, and which pipeline it ran (`default` or `session`);
- the batch text, kept until the run starts, and its SHA-256;
- the cores it needs: one per FSC case of its solve (06.C4), and one for a run without a solve;
- the run directory, the sweep group and swept value, and the run it resumes, if any;
- `fusion`'s own run id (section 09's `r-…`), taken from `run.json` once the run starts;
- the submission, start and end times;
- the process id and process start time;
- the state, and the OS exit status when the frontend saw the exit.

The frontend's own id exists from Submit, before `fusion` has made one, so queued runs can be keyed. At start, the runner reads every record. Records in a final state go into an in-memory index by session; the others are reattached (below).

States:
- `queued` → `starting` → `running` → `finished` or `failed`;
- `running` → `stopping` → `stopped`, after Stop;
- `queued` → `cancelled`, on Stop before it started (no directory was made);
- `starting` → `not_started`, when the process could not start (a missing or non-executable `fusion`);
- `stopping` → `killed`, on Kill;
- `running` → `crashed`, when the run ended without a final record and the runner saw its OS status;
- any active state → `lost`, which is section 09's "died": after a restart, the lock is free and `run.json` still says `running`.

#### Starting a run (A1)

1. Submit validates (above), generates the batch, and queues the run. A swept session gets one batch and one run per value (10.C21).
2. When the queue has the run's cores free (10.C20), the runner creates its directory under the session's results location.
   - **The name** follows 09.C1: `<label>_<YYYYMMDD-HHMMSS>`. The label is the session's name (10.C18), reduced by rule 1.26: per Unicode code point, each run of characters outside `[A-Za-z0-9._-]` becomes one `-`, the ends are trimmed, and the label is cut to 44 characters. "EGFR to CXCL8, FSC JUN" becomes `EGFR-to-CXCL8-FSC-JUN`.
   - **For a sweep value,** the input and value are appended before the reduction (`EGFR-to-CXCL8-FSC-JUN-bound-6`). Any cut comes out of the session's part, so the value survives.
   - The directory is created with one atomic `Files.createDirectory`, and `-2`, `-3` are tried on a collision. An existing directory is never used (S8.21). A golden test shared with the C++ code keeps the Java copy of the rule in step (1.26).
3. It writes `run.batch` and the `overrides/` files into the run directory. Section 09 reserves `overrides/` for them (1.31, 1.32).
4. It starts `fusion -b <dir>/run.batch --run-dir <dir> --tag session=<session id> --tag frontend_run=<run id>` (01.C14), with:
   - the working directory set to the run directory;
   - standard input, output and error all on the null device (10.C17). `fusion` writes every console line to its run log, `exec` output included (09.C11).

   No pipe connects the run to the frontend.
5. It records the process id and `ProcessHandle.info().startInstant()`, and moves the run to `running`. `fusion` adopts the directory and claims it with `run.lock` (09.C9).

FuSION 1 started `./fusion -b output_script.txt` in the GUI's own directory and read its output through a pipe (`Linux/frontend/src/Gui4Panel.java:153-172`).

#### Following a run (A2, S8.28)

- **Polling.** One scheduler thread polls every active run every 250 ms. When `events.jsonl` or the run log has grown, it:
  - reads the new bytes from the last offset;
  - keeps an incomplete last line for the next poll;
  - parses each complete event line, tolerating a trailing `\r`;
  - updates the run's view and sends `RunMessage`s to its subscribers (10.C5).
- **Reading.** All reads use Java NIO, which opens files with delete sharing on Windows, so `fusion` can still replace `run.json` by rename while the frontend reads it (09, 11).
- **Why poll.** Java's `WatchService` is a polling implementation on macOS in the JDKs known here, which notices changes only after seconds (to confirm on the pinned JDK in Phase 3). A 250 ms check of each file's size behaves the same on all three platforms. It costs one size check per file per run per tick: 144 checks a second for 18 running runs.
- **Volume.** Section 09 estimates 150 to 200 event lines (30 to 40 KB) per run, and run logs are about the size of FuSION 1's console logs (20 to 52 KB). A run's view keeps its last 1,000 events in memory; older ones are read from the file on request.
- **Events read** (section 09's envelope `v`, `seq`, `t` and `type`, with `step`, `line` and `point` inside a step; 1.3):
  - `run.started` and `inputs.checked`;
  - `step.started` and `step.finished`; a reported result is `step.finished`'s `report`, with its `code` and `message`, and the files a step wrote are its `outputs`, since no file has an event of its own (07, 09);
  - `step.skipped`, `step.progress` (rate-limited), `warning` and `stop.requested`. There is no heartbeat event: liveness comes from `run.lock` (09.C9, 09.C10);
  - `solve.check.started` (09.C10), and section 06's `solve.case.started`, `solve.check`, `solve.point` and `solve.case.finished`;
  - section 07's result events, `results.frontier` and `results.comparison`;
  - `run.finished`.
- **Stage indicator.** The latest `step.started` event gives the command and its batch line. The catalog's stage label names it, and the source map points to its block. FuSION 1 guessed stages from the command echo (`Gui4Panel.java:197-210`).
- **Solve progress:**
  - `solve.check.started` shows which point is being checked, and since when, through checks of up to 4 h;
  - `solve.check` and `solve.point` give the checks done, the points found per case and the unknown answers;
  - `solve.case.finished` gives each case's status;
  - elapsed time is measured against the budget.
- **End.** The status comes from `run.finished` and `run.json`. When the frontend is the parent, the process's exit gives the OS status too. The mapping is under Errors.

#### Stop, Kill and Resume (A1, decision 0019, S1.28)

- **Stop**
  - On a queued run, Stop cancels it.
  - On a running run, the runner creates `stop` in the run directory (09.C7) and moves the run to `stopping`. Only the file's existence matters; its text goes into `run.json` as the stop's note (1.31). It is written under a temporary name and renamed.
  - `fusion`'s watcher sees the file within 0.5 s. If its main thread has not ended 30 s later, `fusion` ends itself with status 3 (09.C7).
- **Kill (10.C19)** is offered by the screens once `fusion.runs.stop-grace` (default 60 s) passes without an exit.
  - It ends the process and every descendant: `ProcessHandle.descendants()`, then `destroyForcibly`. On Windows, ending a parent does not end its children, so a running gnuplot would otherwise survive.
  - The journal keeps every finished check (09.C6), so only the check in progress is lost.
- **Resume**
  - On a `stopped`, `killed`, `lost` or `crashed` run, it starts a new run that continues the same batch: `fusion --resume <old dir> --run-dir <new dir>` (01.C6, 09.C5). The resume of a killed or crashed run uses its journal (06.C19, which amends A1; prompt log entry 174).
  - On a `failed` run, it restarts from the last good step (S1.28): the session's current batch, which may hold the user's fix, runs with the failed run as parent: `fusion -b <new dir>/run.batch --resume <old dir> --run-dir <new dir>` (01.C6).
  - Either way the run gets a new directory, its record names the old one, and the old directory is never reused.
- **Why a clean Stop matters.** The user's 8 FSC tests were ended by SIGTERM (exit 143) at 18:06:28 on 2026-11-12. FuSION 1 has no other way to stop a run, and the killed runs kept no state. In 2.0, SIGTERM requests the same clean stop (09.C8).

#### Reattaching after a restart (10.C16)

For each record in `starting`, `running` or `stopping`:
1. If `run.json` holds a final status, the run takes the matching final state.
2. Otherwise, if the run directory's `run.lock` is held, the process is alive: `FileChannel.tryLock()` fails (09.C9). The runner then reattaches:
   - it follows the run's files from offset 0, since the files are the log;
   - Stop works as before. Kill uses the recorded process id, which cannot have been reused while the lock is held;
   - the exit status comes from `run.json`, because the frontend is no longer the parent.

   A `tryLock()` that succeeds is released at once.
3. Otherwise the run is `lost`, and Resume is offered.

On a filesystem without lock support, the run still starts and its run log carries section 09's warning (09.C9). The runner then falls back to the process id and start instant in its record. Queued runs stay queued.

#### The queue (10.C20)

- One first-in, first-out queue for all users, counted in cores.
- A run needs one core per FSC case of its solve (06.C4): 2 for `in` and `out`, 1 for `free`, and 1 for a run without a solve. It starts when that many cores are free. A run that needs more cores than the whole limit starts alone.
- The limit is `max(1, min(⌊logical CPUs / 2⌋, ⌊physical memory / 3 GiB⌋))` cores, section 01's `--jobs auto` without its point count (01.C4).
- That gives 18 cores on this machine, 9 runs with an FSC at once, and 4 on an 8-thread laptop that reports 15.5 GiB of memory.

#### The results index (S8.29)

- **What a run's results are:** the outputs that its `run.json` lists (E1, 09), which are section 07's files:
  - `results.json`: the frontier with certification flags, the witnesses, the case statuses and the FSC comparison;
  - the Pareto plot;
  - the run page, and each point's view JSON with its page shell, named by case (`in_n1_e0.html`; 1.6);
  - the GSEA table and the edge sources.
- **Session lists** read the results summary that section 07 puts in `run.json`: points per case, least relaxations and the FSC verdict (2B.7). They do not parse witnesses.
- **Stopped solves:** section 07 publishes a stopped solve with `"status": "partial"` on the unfinished case, so a stopped run still shows what it found.
- **Showing them:** native views for the state, reports, case statuses, frontier table and plot, and the system browser for section 07's pages (10.C6).
- **A run without a run page** (Phase 3, prompt log entry 387): Open the run page says why there is none.
  - While the run is active: that the page is written when the run ends.
  - After a kill, a crash or a lost run: that it ended before it could write the page.
  - For a run that never started: that it has none.
  - For an ended run without one: that the backend that ran it did not write one.
- **Comparing runs:** two runs' settings can be compared from their `run.json`, which answers S8.29's "which directory came from which settings".

```java
package fusion.frontend.runner;   // imports omitted

public record RunId(UUID value) {}
public enum RunState { QUEUED, STARTING, RUNNING, STOPPING, FINISHED, FAILED, STOPPED, KILLED,
                       CRASHED, LOST, CANCELLED, NOT_STARTED }
public enum PipelineChoice { DEFAULT, SESSION }                 // the default pipeline, or the session's edit
public record SubmitRequest(Optional<String> stopAfter) {}      // the session's current pipeline (entry 195)
public record RunRecord(RunId id, SessionId session, AccountId owner, PipelineChoice pipeline,
                        String batchSha256, int cores, Optional<Path> runDir, Optional<UUID> sweepGroup,
                        OptionalInt sweepValue, Optional<RunId> resumedFrom, Optional<String> fusionRunId,
                        Instant submitted, Optional<Instant> started, Optional<Instant> ended,
                        OptionalLong pid, Optional<Instant> processStart, RunState state,
                        OptionalInt osExitStatus) {}
public record Progress(Optional<String> block, Optional<String> stage, int checks,
                       Map<String, Integer> pointsByCase, int unknowns,
                       Optional<String> checking, Duration elapsed) {}
public record RunView(RunRecord record, Progress progress, List<JsonNode> reports,
                      Map<String, String> caseStatuses, Optional<JsonNode> failure,
                      List<PipelineProblem> warnings) {}   // sprint, prompt log entry 325: Submit's
                                                           // --check warnings, kept with the run

public sealed interface RunMessage {
    record Event(RunId run, long seq, JsonNode body) implements RunMessage {}
    record Status(RunId run, RunState state, Instant at) implements RunMessage {}
    record Log(RunId run, long from, String text) implements RunMessage {}
}

public interface RunService extends SessionUse {               // SessionUse is sessions'
    List<RunId> submit(AccountId who, SessionId session, SubmitRequest request);   // one id per swept value
    void stop(AccountId who, RunId run);
    void stopSweep(AccountId who, UUID sweepGroup);
    void kill(AccountId who, RunId run);
    RunId resume(AccountId who, RunId run);
    Optional<RunView> view(AccountId who, RunId run);
    List<RunRecord> runsOf(AccountId who, SessionId session);
    AutoCloseable subscribe(AccountId who, Consumer<List<RunMessage>> perTick);   // 10.C5
}
// Errors: ValidationFailed (with problems), BackendUnavailable, NotOwner, NotResumable, SessionBusy.

public record BackendInfo(String fusionVersion, Map<String, String> libraries, Path home,
                          Map<String, Path> referenceData, List<String> failedProbes) {}
public record CheckDiagnostic(Severity severity, int line, OptionalInt point, String code,
                              String message) {}
public record CheckReport(boolean ok, List<CheckDiagnostic> diagnostics) {}
public interface BackendClient extends CatalogProvider {
    BackendInfo info();                                          // --version --json
    CheckReport check(Path batchFile);                           // -b <file> --check --json
    JsonNode query(String batchLine, Duration timeout);          // --query: one batch line on stdin
}

public interface ResultsIndex {
    Optional<JsonNode> manifest(AccountId who, RunId run);      // run.json, with section 07's summary
    Optional<Path> file(AccountId who, RunId run, String relativePath);   // inside the run directory only
}
```

### Files this section writes

In a run directory:
- **`run.batch`**, the generated batch. Command and argument names are the owners' (1.29):

  ```
  ;; FuSION 2.0 batch written by the FuSION frontend 2.0.0
  ;; session 0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90, pipeline standard@2.0.0 sha256 4f0c9b1e7a2d5c3f8e6b1a9d0c7e4f2a5b8d1c6e9f3a0b7d4c1e8f5a2b9c6d3e
  let $LFC /home/ks-149-u24/data/egf_hela_t120_lfc.tsv
  let $UP 1.01
  let $DOWN -1.01
  let $KGML_LIST $FUSION_HOME/data/kegg_pathways.txt
  let $KGML_DIR /home/ks-149-u24/kegg/hsa_pathways_with_exp_score
  let $GSEA_SELECTOR fdr:0.25
  let $SPLIT_THRESHOLD 1
  let $NOT_MERGED /home/ks-149-u24/data/hsa_ids_not_to_be_merged.txt
  let $SOURCE hsa1956
  let $TARGET hsa3576
  let $BOUND 10
  let $EDGES_TO_TARGET any
  let $FSC hsa3725
  let $NODE_MIN 0
  let $NODE_MAX 8
  let $EDGE_MIN 0
  let $EDGE_MAX 8
  let $TIMEOUT 4h
  let $COEXPRESSION_CSV /home/ks-149-u24/data/hsa_stringdb_coexpression_links_over400.csv
  let $COEXPRESSION_THRESHOLD 700
  let $FROZEN_THRESHOLD 800
  cudf values=$LFC up=$UP down=$DOWN not_de=$FUSION_BATCH_DIR/overrides/not_de.txt
  gsea values=$LFC candidates=$KGML_LIST selector=$GSEA_SELECTOR shortlist=gsea_kgml_files.txt kgml_dir=$KGML_DIR
  mff split_threshold=$SPLIT_THRESHOLD kgml_dir=$KGML_DIR not_merged=$NOT_MERGED
  fb_rch source=$SOURCE target=$TARGET bound=$BOUND edges_to_target=$EDGES_TO_TARGET
  pathz3 fsc=$FSC node_min=$NODE_MIN node_max=$NODE_MAX edge_min=$EDGE_MIN edge_max=$EDGE_MAX timeout=$TIMEOUT coexpression_csv=$COEXPRESSION_CSV coexpression_threshold=$COEXPRESSION_THRESHOLD frozen_threshold=$FROZEN_THRESHOLD
  plot_pareto
  pex
  exit
  ```

  - There is no `start` line, because the session names no map other than the shipped ones (2B.4).
  - `mff` merges the shortlist that `gsea` just chose, by the latest-value rule (01.C16).
  - `pathz3` solves the `in` and `out` cases, its default with an FSC (06.C10). Without an FSC, `fsc` is not written and the solve runs `free`.
  - `pex` leaves `cases` and `points` to their defaults, every solved case and every point (07), so the same line serves a session without an FSC.
  - The upper bounds and the timeout are the values this user entered (10.C13).
- **`overrides/<kind>.txt`**, one per override kind that has genes, with one id per line. Kinds: `confirmed_up`, `confirmed_down`, `not_de`, `active`, `inactive`, `nonrelaxed`. Here `overrides/not_de.txt` holds FOSB, which sits at 1.04, just above the 1.01 cut:

  ```
  hsa2354
  ```
- **`stop`**, only when Stop is pressed (09.C7). Its text becomes the stop's note in `run.json`:

  ```
  requested by arjun from the frontend at 2026-11-12T13:22:10Z
  ```

`fusion` writes the run log, `run.json`, `events.jsonl` and everything else in the directory (09).

In the data directory:

| Path | Holds |
|---|---|
| `frontend.lock` | the single-instance lock |
| `frontend.focus` | written by a second launch, so the running instance brings its window forward; deleted when seen |
| `accounts.json` | every account, written by atomic replace (owner-only permissions) |
| `sessions/<id>/session.json` | one session |
| `sessions/<id>/overrides/<kind>.txt` | the session's current override lists, for the preview |
| `runs/<id>.json` | one run record |
| `query-cache/` | backend query results keyed by SHA-256; safe to delete |
| `scratch/<random>/` | the batch and side files of one check; deleted after it |
| `logs/frontend.log` | the frontend's own log, rotated at 10 MB, 5 files kept |

`accounts.json`:

```json
{
  "format": 1,
  "accounts": [
    {
      "id": "6f1d3c2a-9b8e-4d7f-a1c2-3e4f5a6b7c8d",
      "username": "arjun",
      "display_name": "Arjun",
      "role": "user",
      "password_hash": "{bcrypt}$2a$12$R9h/cIPz0gi.URNNX3kh2OPST9/PgBkqquzi.Ss7KIUgO2t0jWMUW",
      "created": "2026-11-12T12:44:05Z",
      "preferences": {
        "results_dir": "/home/ks-149-u24/FuSION/runs",
        "remembered_inputs": {"kgml_dir": "/home/ks-149-u24/kegg/hsa_pathways_with_exp_score"}
      }
    }
  ]
}
```

`sessions/0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90/session.json` (inputs shortened):

```json
{
  "format": 1,
  "id": "0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90",
  "owner": "6f1d3c2a-9b8e-4d7f-a1c2-3e4f5a6b7c8d",
  "name": "EGFR to CXCL8, FSC JUN",
  "created": "2026-11-12T12:40:11Z",
  "updated": "2026-11-12T12:43:58Z",
  "version": 7,
  "inputs": {
    "results_dir": "/home/ks-149-u24/FuSION/runs",
    "lfc": "/home/ks-149-u24/data/egf_hela_t120_lfc.tsv",
    "network_source": "kgml_list",
    "kgml_list": {"install": "data/kegg_pathways.txt"},
    "merged_graph": null,
    "symbols": null,
    "source": "hsa1956",
    "target": "hsa3576",
    "fsc": ["hsa3725"],
    "bound": 10,
    "node_max": 8,
    "edge_max": 8,
    "timeout": "4h",
    "up": 1.01,
    "down": -1.01
  },
  "overrides": [{"gene_id": "hsa2354", "observation": "not_de", "activity": null, "relaxation": null}],
  "pipeline": null
}
```

`runs/3d9c5a7e-1f2b-4c6d-8e9f-0a1b2c3d4e5f.json`:

```json
{
  "format": 1,
  "id": "3d9c5a7e-1f2b-4c6d-8e9f-0a1b2c3d4e5f",
  "session": "0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90",
  "owner": "6f1d3c2a-9b8e-4d7f-a1c2-3e4f5a6b7c8d",
  "pipeline": "default",
  "batch_sha256": "9c1e5a0f3b7d2e6c8a4f1b9d0e3c7a5f2d8b6e1c4a9f0d3b7e2c5a8f1d6b9e4c",
  "cores": 2,
  "run_dir": "/home/ks-149-u24/FuSION/runs/EGFR-to-CXCL8-FSC-JUN_20261112-181406",
  "sweep_group": null,
  "sweep_value": null,
  "resumed_from": null,
  "fusion_run_id": "r-20261112T124406Z-7f3a9c2e",
  "submitted": "2026-11-12T12:44:05Z",
  "started": "2026-11-12T12:44:06Z",
  "ended": null,
  "pid": 412233,
  "process_start": "2026-11-12T12:44:06.118Z",
  "state": "running",
  "os_exit_status": null
}
```

### Settings

- **No result-changing setting.** This section adds none. Every value that changes a result is an argument or a run-level setting of the backend, written into the batch and recorded by `fusion` in `run.json` (E1). The frontend keeps no default of its own: the catalog and the default pipeline supply every default.
- **The frontend's own settings** change how it runs, not what a run computes, and are not recorded in `run.json`. One caveat: `fusion.runs.max-cores` can change results indirectly, because per-query timeouts are wall-clock (S5.21; 10.C20). Section 06's `rlimit` is the deterministic budget when that matters (2B.8).
- They are Spring properties, which can also be set as environment variables.

| Name | Type | Default |
|---|---|---|
| `fusion.backend.executable` | path | set by the launcher: `$APPDIR/backend/bin/fusion`, `fusion.exe` on Windows (section 11) |
| `fusion.data-dir` | path | `$XDG_DATA_HOME/fusion/frontend`, else `~/.local/share/fusion/frontend` on Linux; `~/Library/Application Support/FuSION/frontend` on macOS; `%LOCALAPPDATA%\FuSION\frontend` on Windows |
| `fusion.results-dir` | path | `~/FuSION/runs` (`%USERPROFILE%\FuSION\runs` on Windows) |
| `fusion.runs.max-cores` | integer, at least 1 | `max(1, min(⌊logical CPUs / 2⌋, ⌊physical memory / 3 GiB⌋))` |
| `fusion.runs.poll-interval` | duration | 250 ms |
| `fusion.runs.stop-grace` | duration | 60 s |
| `fusion.backend.call-timeout` | duration | 30 s |
| `fusion.backend.max-calls` | integer | 4 |
| `fusion.accounts.bcrypt-strength` | integer, 10 to 14 | 12 |

The default results location lies outside the folders that cloud services sync by default: Documents with OneDrive on Windows, and Desktop and Documents with iCloud on macOS. FuSION 1's `runs/` already holds 9.1 GiB.

### Events

- **Writes none.** The frontend writes nothing to `events.jsonl`; `fusion` is its only writer (A2).
- **Reads all,** as listed under Following a run, in section 09's envelope (1.3).
- **Passes them to the screens** as `RunMessage`s, delivered once per poll tick (10.C5).

### Errors

Before a run, nothing starts and nothing is written to a results location. Submit is refused, with every problem tied to a block, argument or input, for:
- input and pipeline problems;
- errors from `fusion --check`, and its `batch.undefined_variable` warnings;
- unknown gene ids;
- a backend that cannot be reached, has another version (11.C16), or does not match the default pipeline.

Run outcomes, shown on the run and on its session. The exit statuses are section 01's (01.C14):

| What the runner sees | State | Notes |
|---|---|---|
| exit 0 | `finished` | Any reported result is listed: `corridor.empty`, `gsea.empty_selection` or `observations.none` (1.8). After one, the run's later lines were skipped (01.C15). The case statuses (no point, infeasible, no DE gene, budget exhausted) are shown from the results; they are not reported results, and the plot and pages still ran. |
| exit 1 | `failed` | S1.28: the batch stopped at its first failure, which can be a check error at start, a failing step or an internal error. The code, line and message come from `run.json` and point to the block. Resume restarts from the last good step. |
| exit 2 | `failed` | A usage error: the frontend's command line or batch file was unusable. That is a frontend bug, and it is logged as one. |
| exit 3 | `stopped` | Resumable. A forced exit (09.C7) is marked `forced`. |
| exit 130 | `stopped` | A second Ctrl-C from a terminal (09.C8). The frontend never sends one. |
| Kill | `killed` | Resumable from its journal (06.C19). |
| POSIX status 128 + n, no final record | `crashed` | Java reports death by signal n as 128 + n: 134 is an abort, 137 SIGKILL (also Linux's out-of-memory killer), 139 a segmentation fault. Resumable from its journal (06.C19). |
| Windows status code, no final record | `crashed` | For example `0xC0000005`, an access violation, which Java reports as −1073741819. |
| lock free after a restart, `run.json` says `running` | `lost` | Section 09's "died". Resume is offered. |
| process could not start | `not_started` | The OS message, for example no file at `fusion.backend.executable`. |

- Every run outcome stays with its run. Under 10.C21, each swept value is its own run, so one value's failure does not stop the others.
- Frontend failures, such as a session file that cannot be written or a full disk: the screen shows the cause, and the store keeps its previous state (atomic replace).

### Platform notes (G3)

- **JavaFX:**
  - On Linux, JavaFX draws through GTK 3, version 3.8 or later (JavaFX 21's release notes, as section 11 cites them), which desktop systems have. Section 11's `.deb` and `.rpm` declare it.
  - The FX Application Thread must never block (Threads, above). Spring's context starts in `Application.init()`, off that thread.
  - On macOS, JavaFX provides the application menu, with Quit. LaunchServices activates a running application instead of starting a second, so `frontend.focus` serves Windows and Linux.
  - `HostServices.showDocument` opens a page in the system browser on all three platforms without AWT. Phase 3 confirms it with `file:` URIs on each.
- **Starting processes:**
  - `ProcessBuilder` with an argument list, never a shell.
  - On Windows, Java's own argument quoting is known to mishandle an argument that ends in a backslash inside quotes. The runner passes only paths that end in a file or directory name, never a trailing separator. Phase 3 confirms the quoting on Windows.
  - A JavaFX application has no console, so OpenJDK starts children with `CREATE_NO_WINDOW`, and `fusion.exe` opens no console window. Phase 3 confirms this.
- **Surviving the frontend:**
  - Linux and macOS: `fusion` ignores SIGHUP (09.C8), so a run outlives a closed terminal from which the frontend was started.
  - Windows: a child outlives its parent unless it sits in a job object that kills it. The runner creates none; whether jpackage's launcher places the JVM in such a job is to be confirmed in Phase 3.
  - Stop is a file, never a signal (A1, 09.C7). Kill ends descendants explicitly.
- **Reading files that `fusion` is writing:**
  - On Windows, the backend opens `events.jsonl` and the run log with read sharing (section 09).
  - The frontend reads with Java NIO, whose delete sharing lets the backend replace `run.json` and section 07's pages by rename. `java.io.FileInputStream` lacks delete sharing and is not used (09, 11).
- **Locks:**
  - `frontend.lock` uses `FileChannel.tryLock`, which is advisory on POSIX and mandatory on Windows.
  - `run.lock` is tested the same way and released at once. Section 09 probed that Java sees `fcntl` locks, which is why `fusion` uses `fcntl` on POSIX (09.C9). How the lock behaves on Windows is section 09's to confirm.
- **Paths:**
  - Sessions and batches hold absolute, native paths (01.C2). The predefined variables hold paths with `/` (01.C18).
  - Windows paths are case-insensitive, and the containment check of `ResultsIndex.file` compares real paths.
  - Windows limits paths to 260 characters (`MAX_PATH`) unless long paths are enabled. 09.C1 cuts labels to 44 characters, so a run directory's name stays within 64, and the Windows build is long-path aware (09, 11).
  - Non-ASCII paths, such as `C:\Users\José\…`, occur. Batches are UTF-8 (01.C19), and `fusion` converts paths to UTF-16 on Windows (01).
- **Data directory:** the frontend's data directory is a `frontend` folder inside FuSION's per-user folder. On Windows, `%LOCALAPPDATA%\FuSION\cache` would be the same folder as section 09's shared store, `%LOCALAPPDATA%\FuSION\Cache`, because Windows names ignore case.
- **gnuplot:** `fusion`'s own plots find it through `FUSION_GNUPLOT`, then the bundled copy (gnuplot 6.0.5, 11.C9), then `PATH` (1.27). `exec` lines get the bundled tools directory at the end of `PATH`, so a system gnuplot comes first there (11.C20; prompt log entry 204). An application started from Finder on macOS gets a minimal `PATH` (`/usr/bin:/bin:/usr/sbin:/sbin`), so the bundled copy is what a biologist's runs use.
- **Line endings:** generated files use `\n`; the event and log readers also accept `\r\n`.
- **Numbers:** `Locale.ROOT` everywhere in the generator, whatever the system's language.

### Determinism

- **Batches:** the generator is pure. The same pipeline, inputs and catalog give byte-identical batches. Ties follow the document's block order, arguments follow catalog order, and `let` lines follow declaration order.
- **Ids and times** come from injected sources (`Supplier<UUID>` and `java.time.Clock`), which tests fix.
- **Lists shown on screens have stable orders:**
  - sessions by last change, then name;
  - runs by submission time, then id;
  - gene suggestions as described under Runner.
- **Provenance:** each batch's header carries the pipeline's SHA-256 and the frontend's version, and `run.json` records the batch and the `--tag` labels (E1). `fusion` records its own versions.

## Choices

### 10.C1 · Delivery: a JavaFX desktop application

- **Proposed:** a standalone JavaFX desktop application with one window, like FuSION 1's. Its screens are written in Java on OpenJFX, and Spring Boot is the application framework, with no web server. Files are chosen with native dialogs, and nothing listens on a port. Section 07's pages open in the system browser (10.C6), Phase 5 draws its display in JavaFX, and there is no lab-server mode.
- **Alternatives:**
  - A local web application in the system browser, served by Spring Boot on a loopback port (this section's earlier draft).
  - A JavaFX window over web pages: a WebView showing a local server's HTML screens.
  - A Swing desktop application, as FuSION 1's GUI was.
  - An Electron or Tauri shell around web pages.
- **Why:**
  - The user asked: "why does it have to be a web app? the first fusion was a standalone app to interact w the front end", and then chose this option (prompt log entries 191 and 192).
  - Native dialogs give real paths, where a browser never reveals a chosen file's path. A KGML list names 167 files by path, so an upload would point at files the application never received.
  - No port means no local-port security: the setup token, the CSRF, Origin and Host checks, the capability links and the server-side file browser of the earlier draft all go.
  - The Phase 5 display is drawn in JavaFX, for example on a `Canvas`. Corridors are small (at most 793 nodes and 3,328 edges), while the 82,188-relation merged network needs batched drawing and culling. JavaFX's WebView lacks WebGL (JDK-8091035), so a display built from web pages inside the window would also be limited to 2D drawing.
  - A window over web pages keeps a local port and gives screens in HTML and JavaScript. Swing ships in the JDK but has an older look and Java2D drawing. Electron adds Chromium and Node.js to every installer, and Tauri adds Rust to the build.
  - Costs: OpenJFX joins the bundle (10.C3), and there is no lab-server mode in 2.0.
- **Decision:** a JavaFX desktop application (prompt log entries 191 and 192)

### 10.C2 · Accounts on a shared machine: open sign-up

- **Proposed:** the sign-in screen also offers "Create a new account", as FuSION 1's did. At first start it opens there. Every account is equal, with no owner role: the record's role field (10.C10) is `user` for every account.
- **Alternatives:**
  - Owner-created accounts: the first account becomes the owner, who creates the others.
  - No accounts, one implicit user. S8.02's poll turned this down ("Drop the login", prompt log entry 090), so it would amend S8.02.
- **Why:**
  - With no port and no lab mode, only people at the machine's own OS login can reach the application. Accounts (S8.02) keep apart the sessions of people who share that login.
  - An owner could not really control who has an account, since anyone at the login can edit the data directory. The gatekeeping would cost friction without giving protection.
  - S8.02 lists "anyone can sign up" among the problems of a login that "guards nothing", beside session files anyone can read. Its decision fixes the hashes, the store, the usernames and the committed file, and no sign-up rule fixes file access. This proposal leaves open sign-up as it was, on purpose.
- **Decision:** as proposed (prompt log entry 197)

### 10.C3 · OpenJFX version: 21.0.13, the JavaFX 21 LTS line

- **Proposed:** OpenJFX 21.0.13 from Maven Central (`org.openjfx`), and Gluon's jmods for jlink. Its licence is GPL-2.0 with the Classpath Exception. The application uses `javafx.base`, `javafx.graphics` and `javafx.controls`; `javafx.web` and the `javafx.media` it requires are not bundled, since 10.C6 opens section 07's pages in the system browser (prompt log entry 198). Updates follow the 21.0.x line, and changing to another line is a new decision. Section 11 pins the same version (11.C24).
- **Alternatives:**
  - OpenJFX 25.0.5, the newer LTS line. It needs JDK 23 or later, so G2's Java 21 would become Java 25, which amends G2.
  - OpenJFX 23.0.2, which runs on JDK 21 but has had no update since 2025-01-21.
- **Why:**
  - 21.0.13 was published on 2026-10-20, and its release notes say "JavaFX 21 is designed to work with both JDK 21 and JDK 17", which fits G2.
  - The 21.0.x line still gets updates: 21.0.11 and 21.0.13 both updated WebView's WebKit, the latter to 625.1 (JDK-8380556), the same as 25.0.5.
  - OpenJFX 24 and later need JDK 22 or later, so Java 21 runs the lines up to 23. Of those, only 21 still gets updates: 22 stopped at 22.0.2, and 23 at 23.0.2 (prompt log entry 197).
  - Sizes per platform, from Maven Central's jars: the three modules take 8,020,309 bytes on Linux x86-64, 8,195,422 on macOS x86-64, 8,160,260 on macOS arm64 and 9,247,020 on Windows x86-64. The WebView that 10.C6 turned down would have added 37,603,369 to 45,169,113 bytes of `javafx-web`, plus `javafx-media`.
  - Gluon's jmods bundles, all modules, are 47,109,232 to 53,622,248 bytes; jlink keeps only the modules used.
- **Decision:** as proposed (prompt log entry 197)

### 10.C4 · Desktop integration: a normal application, one instance per data directory

- **Proposed:**
  - A normal application: one main window, a Dock or taskbar entry, Quit in the application menu (macOS) or the File menu, and closing the main window quits. Runs continue after a quit (10.C16).
  - One instance per data directory through `frontend.lock`. A second launch writes `frontend.focus`, so the running instance brings its window forward, and exits.
- **Alternatives:**
  - A second launch shows "FuSION is already running" and exits, without bringing the window forward.
  - A tray or menu-bar icon that keeps the application alive after its window closes. JavaFX has no tray API, so this needs AWT's `SystemTray`, which many Linux desktops lack.
- **Why:**
  - It is the standalone window of FuSION 1 that the user asked for (10.C1).
  - On macOS, LaunchServices already activates a running bundle. On Windows and Linux a second launch starts a second JVM, and only the lock can tell it apart. A file hand-off needs no port, and costs the running instance one size check per 250 ms poll.
  - Quitting while runs continue is safe, because runs owe nothing to the frontend (10.C16).
- **Decision:** as proposed (prompt log entry 197)

### 10.C5 · Screens and services: in-process calls, run updates once per tick

- **Proposed:**
  - The screens call the Spring-managed services directly, in the same process, on virtual threads, and receive results on the FX Application Thread.
  - The runner's poller tails `events.jsonl` and the run log of every active run (09.C10, 09.C11), and hands the screens one batch of `RunMessage`s per 250 ms tick, through one `Platform.runLater`.
- **Alternatives:**
  - Keep an HTTP API with Server-Sent Events inside the application, so a web frontend could be added later. That brings back a port and its security.
  - Spring's `ApplicationEventPublisher` as the bus between the runner and the screens, with one event per message.
- **Why:**
  - One process needs no serialisation, no port and no API versioning.
  - JavaFX allows changes to the scene only on its own thread, and blocking it freezes the window. Coalescing per tick bounds the hand-overs at 4 a second, whether 1 run is followed or 18.
  - The volume is small: 150 to 200 events per run by section 09's estimate, run logs of 20 to 52 KB, and at most 28 checks per solve.
  - An event per message would wake the FX thread for every line of every run.
- **Decision:** as proposed (prompt log entry 197)

### 10.C6 · Showing run results: native views, and section 07's pages in the system browser

- **Proposed:**
  - The run's state, reports, case statuses, log, frontier table (from `results.json`) and Pareto plot (`pareto.png`) are native views in the window.
  - Section 07's run page and explanation pages open in the system browser, through `HostServices.showDocument`. No WebView is used, and `javafx-web` is not bundled.
- **Alternatives:** an embedded WebView that shows the pages inside the window, with navigation confined to the run directory, pop-ups refused and no Java object exposed to scripts.
- **Why:**
  - Section 07's pages fetch nothing and work from `file:` in any browser (07.C2), so they need no embedding.
  - Phase 5's display is drawn in JavaFX in the window (10.C1), so the static pages serve export and sharing.
  - `javafx-web` is 37,603,369 to 45,169,113 bytes per platform, more than four times the other three modules together, and it brings `javafx-media`. Section 11 estimates the runtime image at about 60 to 75 MB without WebView and 165 to 200 MB with it.
  - WebView's WebKit is updated only with JavaFX: 21.0.11 and 21.0.13 each updated it. The system browser is patched by its own updates.
  - From documentation, the WebView alternative would show 07's pages: its engine in 21.0.13 is WebKit 625.1 with its JavaScriptCore, and JavaFX's WebView guide lists Canvas and SVG, DOM, web workers and web fonts among its HTML5 features. vis-network draws on 2D canvas (07.C3), and WebGL is missing (JDK-8091035). JavaFX documents no ECMAScript level, so Phase 3 would load 07's pages to confirm.
  - The cost: a page opens outside the window.
- **Decision:** as proposed (prompt log entry 198)

### 10.C7 · Choosing input files: native dialogs, and typed paths checked alike

- **Proposed:** inputs are chosen with JavaFX's `FileChooser` and `DirectoryChooser`. A dialog opens in the folder of the input's current value, or else the last folder used for that input. A path may also be typed or pasted. Both pass the same checks (S8.30).
- **Alternatives:** dialogs only, with path fields that cannot be edited.
- **Why:**
  - Native dialogs give real paths, with the platform's recent places and network locations (10.C1).
  - A typed path serves a path copied from a terminal, from FuSION 1's batch files (all 123 under `runs/` name absolute paths, counted on 2026-11-16), or from a colleague. Checking it like a chosen one costs nothing extra.
  - Dialogs only would remove typing errors, but a misspelt path is caught by the checks anyway.
- **Decision:** as proposed (prompt log entry 198)

### 10.C8 · Store: JSON files in a per-user data directory

- **Proposed:** one `accounts.json` for the accounts, one folder per session and one file per run record. Each file is written by atomic replace, and sessions carry a version number for conflicts. They live in a per-user data directory, outside the installation and outside any repository.
- **Alternatives:**
  - An embedded H2 database: pure Java, with transactions and unique constraints.
  - An embedded SQLite database through the xerial driver.
- **Why:**
  - The data is small. FuSION 1's sessions are 814 to 1,468 bytes each and its account file 304 bytes. 1,000 sessions of 10 KB each make 10 MB, read at start in well under a second.
  - Files need no driver, connection pool or schema migration. The user can read them and back them up, and they hold no native code (G3).
  - One file per id removes S8.05's faults by construction: no list positions, no whole-array rewrite, no dropped keys.
  - H2 has changed its file format between major versions (1.4 to 2.x needed an export and a re-import). SQLite's driver extracts a native library at run time, which a signed and notarised macOS application must be allowed to load.
  - Files have their own costs: uniqueness and conflicts are checked in code, under one lock per store, and `frontend.lock` enforces a single instance per data directory.
- **Decision:** as proposed (prompt log entry 195)

### 10.C9 · Password hashing: bcrypt, strength 12

- **Proposed:** bcrypt at strength 12, through the `DelegatingPasswordEncoder` of `spring-security-crypto`, the only part of Spring Security the desktop application uses. It stores the scheme with each hash (`{bcrypt}…`), and replaces a hash at its next sign-in if the scheme changes.
- **Alternatives:**
  - Argon2id, which is memory-hard, through Spring Security's `Argon2PasswordEncoder` and the BouncyCastle provider.
  - PBKDF2-HMAC-SHA256 with 600,000 iterations, using the JDK alone.
- **Why:**
  - The threat that S8.02 names is offline cracking of a copied account file. Any salted, slow hash answers it. FuSION 1 used a single unsalted SHA-256 (`Linux/frontend/src/LoginPage.java:523-538`).
  - bcrypt needs no extra library. Strength 12 took 0.252 s per hash here in C (0.062 s at Spring's default of 10). Spring's pure-Java implementation is slower, and Phase 3 measures it.
  - Argon2id is the stronger choice against GPU attacks, but it brings BouncyCastle, a jar of about 8 MB and a second crypto provider.
  - PBKDF2 at OWASP's 600,000 iterations took 0.842 s per hash here, and it is not memory-hard.
- **Decision:** as proposed (prompt log entry 195)

### 10.C10 · Account fields: no email, and a role field kept for later

- **Proposed:** an account holds a username, a display name, a role, a password hash and preferences, with no email address. The role is unused by 2.0's features: every account gets `user`, since under 10.C2 every account is equal. The field is kept for a later owner or admin feature.
- **Alternatives:**
  - No role and no email: a username, a display name, a password hash and preferences, following open sign-up (10.C2). This was the recommended option in prompt log entry 198.
  - Keep FuSION 1's email field, which prompt log entry 196 turned down.
- **Why:**
  - No email was accepted in prompt log entry 196. In the re-poll after 10.C2 (prompt log entry 198), the user chose "Keep a role field: Unused now, kept for a later owner or admin feature", not the recommended record without a role.
  - FuSION 1's sign-up requires an email address (`Linux/frontend/src/LoginPage.java:406-428`), and nothing reads it: `getEmail` (line 573) has no caller.
  - The email address was the personal data that `fusion_users.dat` put into git (S8.02).
  - Nothing in 2.0 sends mail, and password resets go through the command line.
  - The role costs one field per account, and `accounts.json` carries it from format 1 on.
- **Decision:** keep a role field, unused while every account is equal (10.C2), and no email (prompt log entries 196 and 198)

### 10.C11 · Module boundaries: five packages checked by a test, with `web` renamed `ui`

- **Proposed:**
  - One Gradle project and one Spring Boot application, with C1's five modules as top-level packages: `fusion.frontend.ui`, `.accounts`, `.sessions`, `.pipeline` and `.runner`.
  - C1's `web` module, "pages", becomes `ui`, the JavaFX screens and the launcher. This changes the wording of skeleton C1 (`web`) and of A1 ("not the web server"), not their structure; wording notes on both were accepted in prompt log entry 198.
  - A test, Spring Modulith's `ApplicationModules.of(…).verify()`, fails the build if one module reaches into another's internals or the dependencies form a cycle.
  - Allowed dependencies: `ui` on all the others; `runner` on `pipeline`, `sessions` and `accounts`; `sessions` on `pipeline` and `accounts`; `accounts` on `pipeline`; `pipeline` on none. JavaFX types appear only in `ui`.
  - Where a lower module needs a higher one's answer, it declares an interface that the higher module implements: `pipeline`'s `CatalogProvider` and `sessions`' `SessionUse`, both implemented by `runner`.
- **Alternatives:**
  - Keep the package name `web` for the screens, as C1 has it.
  - Five Gradle subprojects, one jar each, with the dependencies enforced by the compiler.
- **Why:**
  - With 10.C1 there is no web layer, and a package called `web` holding JavaFX screens would mislead.
  - Keeping JavaFX out of the four service modules lets Phase 3 test them without a display.
  - The modules are small and share one Spring context. Subprojects would add five build files for a single application, and the test catches the same violations.
- **Decision:** as proposed (prompt log entry 198)

### 10.C12 · Default pipeline: a pipeline document shipped with the backend

- **Proposed:** the standard run is a JSON pipeline document kept with the backend at `share/fusion/pipelines/standard.json` (11.C14's layout; prompt log entry 202), tested against the command table, and installed with `fusion`. The frontend opens it as the editor's default pipeline, and a quick Submit of an unedited session runs it. Its inputs carry S8.06's defaults and declare which inputs each user's sessions remember. GSEA shortlisting is on, with `fdr:0.25`, whenever the source is a KGML list.
- **Alternatives:**
  - A batch file with typed parameters, shipped with the backend and opened in the editor through `fusion`'s own parser.
  - The standard run built into the frontend's Java code, as FuSION 1's `ScriptFile` and template were.
- **Why:**
  - FuSION 1 kept three templates and read only one (S8.20), left three placeholders unfilled (S8.17), and defaulted to values that no batch used (S8.06), because the template lived apart from the commands.
  - A document keeps what a batch file cannot: each input's label, unit, range and group for the screens, the remembered inputs, the block layout, and the conditions that switch GSEA, the maps and the network source (S8.08, 2B.4).
  - The batch-file alternative would need typed parameter declarations in the batch language. Section 11 instead writes `standard.batch` from the document at build time.
  - `fdr:0.25` is the reference run's selector and GSEA's conventional cut-off. 19 of the 88 batch files of the spec's census run `gsea` (S8.27).
- **Decision:** as proposed (prompt log entry 195)

### 10.C13 · Relaxation upper bounds and the per-query timeout: no defaults in the frontend

- **Proposed:** the default pipeline declares the node and edge relaxation upper bounds and the per-query timeout as required inputs with no default, like the LFC thresholds. The screens offer section 06's values (8, 8 and 4 h) as a suggestion that the user accepts with one click. The incremental timeout stays an advanced argument with section 06's default. Section 06's defaults stay for command-line batches (3A.10).
- **Alternatives:** pre-fill the three inputs with section 06's defaults, or leave them unbound so that those defaults apply.
- **Why:**
  - S8.06's poll offered "also default the upper bounds to 8/8 and the timeouts to 3600000/14400000, as in your b10 runs", and the user took the option without them (prompt log entry 091).
  - Section 06's backend defaults are exactly those values (06.C2, 06.C17). Unbound inputs would apply them silently, and pre-filled ones would make them the frontend's defaults after all.
  - The census shows why a default is risky: the timeout pairs in use were 600,000/3,000,000 ms in 102 of 170 lines and the b10 values in 34 (S5.21), and the upper bounds ranged from 1 to 20 (S5.01).
  - The cost is three more fields to fill before the first run, each with a one-click suggestion.
- **Decision:** as proposed (prompt log entry 173)

### 10.C14 · Generated batches: only the arguments the pipeline binds

- **Proposed:** a generated batch writes each argument that the pipeline binds to a session input or a literal, and leaves the others out. `fusion` then applies its defaults and records them as defaults in `run.json`.
- **Alternatives:** write every argument, filling the unbound ones with the catalog's current defaults, so that the batch alone fixes every value.
- **Why:**
  - E1 records each setting's value and whether it came from a default. With every argument written, every value would read as chosen by the user.
  - FuSION 1's solve lines take 28 to 34 positional tokens (S5.01). Named arguments for every one of them would make each solve line long when the user set only a few.
  - The explicit form would keep a `run.batch`'s meaning fixed under a later FuSION whose defaults changed. With the proposal, `run.json` still holds every value used, so nothing is lost for reproducing a run.
- **Decision:** as proposed (prompt log entry 196)

### 10.C15 · Gene lookups: the symbol map read through `fusion`

- **Proposed:** the frontend gets a symbol map's id-label pairs from section 02's `labels map=<file>` line through `fusion --query` (2B.6), the same mode as the threshold preview (04.C8). It caches the pairs by the file's SHA-256 and searches them in memory. With 09.C12's check and 04.C8's queries, the frontend then reads no FuSION input format itself.
- **Alternatives:** Java code in the frontend reads the two-column map file itself.
- **Why:**
  - S1.18 and S1.19 give the map one reader with exact rules: tabs inside names become underscores, blank lines are skipped, and an id with no name falls back to the id. A Java reader would be a second copy of those rules, the drift that S3.14 removed for the LFC file and S8.14 found in the GUI's fifth parser.
  - The cost is one query per map file content (FuSION 1's map has 6,334 pairs in 153,153 bytes), plus a process start of 0.68 to 10.64 ms as measured here. Searching the cached pairs then costs microseconds per keystroke.
  - A Java reader would save that start, but Submit needs the backend anyway.
- **Decision:** as proposed (prompt log entry 168)

### 10.C16 · Run lifetime: runs outlive the frontend

- **Proposed:**
  - Once started, a run depends on nothing in the frontend. `fusion` writes its run log (09.C11) and `events.jsonl`, Stop is the `stop` file (09.C7), and no pipe joins the two (10.C17).
  - After a restart, the frontend reattaches to running runs through 09.C9's `run.lock`. It marks the others finished, stopped or lost from `run.json`.
- **Alternatives:** runs bound to the frontend: output through a pipe, every run asked to stop when the frontend quits, and stopped runs resumed by the user later.
- **Why:**
  - Runs take up to 52 h 7 min (`r6_mcl1_creb1`) and 50 h 42 min (`fdr050`). Quitting the application, an update or a crash should not cost them, not even as a resumable stop.
  - With a pipe, a frontend that exits leaves `fusion` writing into a pipe that nobody reads (stage 8 open question 4). With files there is nothing to break.
  - The user already keeps runs apart from any terminal. All 12 launch scripts under `runs/` send the output to a log file, 8 put each run in the background, and the 5 newest also take standard input from `/dev/null`.
  - A frontend crash needs reattachment anyway, so the bound alternative would need it too.
  - The costs: care on each platform (see the platform notes), and runs that finish while no frontend watches, which `run.json` covers.
- **Decision:** as proposed (prompt log entry 185)

### 10.C17 · A run's console output: the null device

- **Proposed:** the runner starts `fusion` with standard input, output and error all on the null device. The run log (09.C11), written by `fusion` into the run directory, is the record of the console.
- **Alternatives:**
  - Send standard output and error to a file in the run directory.
  - Send them to a file in the frontend's data directory.
- **Why:**
  - Section 09's run log holds every console line, `exec` output included, each with its time and level. A captured copy would be a second record of the same lines.
  - S8.21 puts every output of a run in its own directory, which rules out the data directory.
  - What a capture would add is output from before the log opens, such as a shared library that fails to load. The start-up `fusion --version` call runs the same binary with the same libraries, so such a failure shows there, and the run's exit status still reaches the frontend.
- **Decision:** as proposed (prompt log entry 185)

### 10.C18 · Run labels: the session's name

- **Proposed:** a run directory's label is the session's name, reduced by rule 1.26, with the swept value appended for a sweep. The session's id is recorded in `run.json` with `--tag session=<id>` (01.C14).
- **Alternatives:** the session's id as the label.
- **Why:**
  - A directory listing then reads `EGFR-to-CXCL8-FSC-JUN_20261112-181406` instead of `0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90_20261112-181406`, and a UUID would take 36 of the label's 44 characters.
  - FuSION 1 users name their run directories by experiment, the evidence behind 09.C1.
  - The id still reaches `run.json` through `--tag`, so a run is linked to its session even without the frontend's records.
- **Decision:** as proposed (prompt log entry 184)

### 10.C19 · Kill: the process tree, 60 s after the stop request

- **Proposed:** once 60 s have passed since a stop request without an exit, the screens offer Kill. It ends the process and every descendant with `destroyForcibly`.
- **Alternatives:** `Process.destroy()` 30 s after the request.
- **Why:**
  - On Linux and macOS, `Process.destroy()` sends SIGTERM, which section 09 turns into another stop request (09.C8), so it cannot end a run that a stop request has not ended.
  - `fusion` already ends itself 30 s after a stop request (09.C7). A run still alive at 60 s is stuck beyond its own watcher, and only a forced kill ends it.
  - On Windows, the children of a killed process survive it, so a running gnuplot or `exec` shell must be ended separately.
  - The journal (09.C6) keeps every finished check, and a resume uses it (06.C19, which amends A1; prompt log entry 174), so a kill loses only the check in progress.
- **Decision:** as proposed (prompt log entry 196)

### 10.C20 · Runs at once: a queue counted in cores

- **Proposed:** one first-in, first-out queue for all of an instance's runs, counted in cores. A run needs one core per FSC case of its solve (06.C4). At most `min(⌊logical CPUs / 2⌋, ⌊physical memory / 3 GiB⌋)` cores are in use, and at least 1: 18 on this machine, and 4 on an 8-thread laptop that reports 15.5 GiB. The user can change the limit. The formula is section 01's `--jobs auto` without its point count (01.C4).
- **Alternatives:**
  - No limit: every Submit starts at once, as the user's launch scripts do.
  - One run at a time.
- **Why:**
  - Every measured run used one CPU (99 to 101 %) and at most 1,982,592 KiB of memory (`r5_mcl1_stat3`). With 06.C4, each case of a run takes a core and a Z3 heap of its own. Half the logical CPUs (one per physical core on machines with two threads per core) and 3 GiB per case, 1.6 times the largest peak, bound the load.
  - Per-query timeouts are wall-clock (S5.21). Two cases that share a core both run slower, so a check that finishes in time alone can time out, and the frontier changes. A limit at the core count keeps runs from changing each other's results.
  - The user ran 4, 9 and 18 solves at once on this machine. One run at a time would have turned the 9 candidate runs' 52.1 h into 245.4 h.
- **Decision:** as proposed (prompt log entry 210)

### 10.C21 · Sweeps: one run per value

- **Proposed:** when a session sweeps an input (decision 0018), Submit makes one run per value. Each is the plain batch that the exported sweep file would run for that value, with its own process, directory, state, Stop and Resume, all queued under 10.C20 and grouped in the session. Gen Script exports the whole sweep as one batch file with section 01's `sweep` lines.
- **Alternatives:** submit the sweep as one batch and one run, and let section 01's coordinator run its points as child processes, at most `--jobs` at a time (01.C4).
- **Why:**
  - Values run in parallel either way. The 4-value FDR sweep took 50.7 h in parallel against 52.3 h one after another; the 9 candidate runs, the same pattern, took 52.1 h against 245.4 h.
  - There is one scheduler. With the coordinator, each sweep would hold cores that the frontend's limit cannot see.
  - Every value is a run that the frontend already knows how to follow, stop, resume and show, in a normal run directory.
  - A Phase 3 test keeps the two forms in step: the per-value batches must equal what the exported sweep file runs for each value.
- **Decision:** as proposed (prompt log entry 187)

## Interfaces with other sections

- **01 Batch language.** This section uses, as accepted:
  - `fusion --commands` (01.C9), with its type table (09's `core::Type` with the `list` and `takes` marks; prompt log entry 230), the cross-argument rules and the run-level settings (2B.2);
  - `fusion -b <file> --check --json` (01.C14, 1.9);
  - `--run-dir`, `--tag`, `--resume` (with `-b` for a failed run), `--query` with one batch line on standard input (1.10), and the exit statuses 0, 1, 2, 3 and 130 (01.C14);
  - quoting with `""` and `$$`, absolute native paths, relative input paths against the batch file's folder (01.C2), `$FUSION_BATCH_DIR` and `$FUSION_HOME` (01.C18);
  - the latest-value rule (01.C16), `load run= step=` (01.C6), the `sweep` syntax for exports (01.C3), the `run` line for run-level settings (2B.2), and the look-ahead that hands confirmed lists to `mff` (03.C2);
  - `batch.undefined_variable`, which the frontend treats as an error (1.31).

  It gives batches in section 01's rules for the frontend's files: named arguments, no `exec`, and one run per swept value (10.C21, 01.C4).
- **02 Network.** The `labels map=<file>` query (2B.6); `kgml_dir` as an argument of `gsea` and `mff` (02.C12), remembered per user; the shipped list of bare pathway ids, resolved against it.
- **03 Merge.** The not-merged file on `mff`; confirmed overrides joining the exception set (03.C2); the pre-merged mode naming a saved network (03.C7).
- **04 Expression.** The preview and suggestion queries (04.C8), whose bins the screens draw (04.C7); the session's Not DE file, kept by the frontend for the preview and written as `overrides/not_de.txt` for a run (04.C4); `observations.none` as a reported result (04.C5).
- **05 Corridor.** `fb_rch` and `edges_to_target` (05.C8). The frontend refuses a bound below 1 (S8.09); the backend still accepts 0 and reports an empty corridor.
- **06 Solve.**
  - The argument names (1.7), the cases `in`, `out` and `free` (06.C10), and parallel cases, which the queue counts (06.C4).
  - The backend defaults stay for the command line, while the frontend requires the values (10.C13, 3A.10).
  - `rlimit` (2B.8, registry key `solve.rlimit`), shown as an advanced argument through the catalog. Resuming from the journal after a kill (06.C19, which amends A1).
  - The events `solve.case.started`, `solve.check`, `solve.point` and `solve.case.finished`, and the case statuses.
- **07 Results.**
  - The files and pages (07.C1, 07.C2), named by case (1.6); the results summary in `run.json` (2B.7); `"status": "partial"` for a stopped solve.
  - With 10.C6, the pages open in the system browser from disk, which 07.C2 already supports.
  - The Phase 5 display reads 07's view JSON in JavaFX.
- **08 Enrichment.** `gsea`'s names (08.C11), with its required `shortlist` given as a file name that lands in the step directory (2B.3), and S7.18's review through the `gsea.selected` event and the saved shortlist.
- **09 Runs.**
  - 09.C1's run directory names with rule 1.26, re-implemented in Java and kept in step by a shared golden test.
  - Adoption through `--run-dir`, with `overrides/` reserved for the frontend (1.31, 1.32).
  - `run.json`, `events.jsonl` in the dotted envelope (09.C10, 1.3), and the run log (09.C11).
  - The `stop` file with a text note (09.C7, 1.31), the signal handlers with SIGHUP ignored (09.C8), and `run.lock` with its fallback for filesystems without locks (09.C9).
  - The journal (09.C6) and the check-only mode (09.C12).
- **11 Build.** Section 11 packages the desktop application (11.C12; prompt log entry 202):
  - **OpenJFX 21.0.13** (10.C3), GPL-2.0 with the Classpath Exception, as a licence row. The runtime takes `javafx.base`, `javafx.graphics` and `javafx.controls` from Gluon's jmods (47,109,232 to 53,622,248 bytes per platform), and neither `javafx.web` nor `javafx.media` (10.C6). Section 11 pins the same version (11.C24) and resolves which JDK modules JavaFX itself needs.
  - **A normal application bundle,** with its own window and a Dock or taskbar entry. No port, no `open.html`, no `LSUIElement` and no headless runtime: the earlier draft's local-server packaging goes.
  - **The launcher** runs `ui.Launcher`, passes `-Dfusion.backend.executable` (kept), and handles `--reset-password`. On Windows, section 11 adds a console launcher for `fusion-frontend --reset-password <username>`, for example a second jpackage launcher with a console (prompt log entry 199).
  - **On Linux,** the `.deb` and `.rpm` declare GTK 3.8 or later for JavaFX. There is no systemd unit, since there is no lab mode.
  - **Kept:** `--version --json` with its probes and reference maps, and one `VERSION` for frontend and backend (11.C16); `share/fusion/pipelines/standard.json` in the repository layout (11.C14) and the `standard.batch` built from it; gnuplot's lookup order (1.27), and `exec`'s `PATH` with the bundled tools last (11.C20).
  - **Ships:** `data/kegg_pathways.txt`, 167 bare pathway ids listed in `fusion-data.json` (2B.5; prompt log entry 204), and the two rebuilt maps with `data/SOURCES.md` (prompt log entry 205).
  - **Spring Boot 4.1** (11.C21, which amends G2; prompt log entry 200): this section uses Spring's core, Spring Boot's configuration with `WebApplicationType.NONE`, `spring-security-crypto` and Spring Modulith's verification test, and no feature of Spring Boot 3 alone.
- **12 Tests.** Section 12's native `fake-fusion`, in `backend/tests/tools/fake-fusion/`, replaces `fusion` in the runner tests (12.C2; prompt log entry 206). The frontend's Gradle tests validate against the one copy of the JSON Schemas, read from `../backend/tests/schemas/` (2A.10), and section 12's cross-language test passes every pipeline the generator can make to `fusion --check`. Phase 5 tests the screens.

## Test hooks

Library level (JUnit 5, no JavaFX):
- `UsernamePolicy`: accepted and refused names, such as `../x`, `a/b`, a 33-character name, and `Arjun` normalised to `arjun`.
- `AccountService`: hashes start with `{bcrypt}$2a$12$`; every account is created with the role `USER` (10.C10); an unknown user and a wrong password take the same code path and give the same message.
- `SessionStore`:
  - create, rename and conflict, where a stale version gives `VersionConflict`;
  - an input set to null keeps its key, with a null value;
  - a crash between writing the temporary file and the rename leaves the old record readable;
  - a blank name, a duplicate name, or a name with a line break creates nothing;
  - delete is refused while `SessionUse.busy`.
- `PipelineValidator`:
  - cycles, with each strongly connected component named;
  - required arguments left unbound after conditions and `first_of`;
  - kind mismatches and connections to a value that is not the latest of its kind;
  - declared rules such as `down < up` and `node_min ≤ node_max`;
  - unknown commands.
- `BatchGenerator`:
  - golden files: the default pipeline with fixture sessions gives byte-identical batches. The fixtures cover a KGML list with and without GSEA, a pre-merged graph, another symbol map (so `start` appears), no FSC (`free`), overrides, additional edges, `stop_after`, a swept bound and its export;
  - the same pipeline with its blocks shuffled in memory gives the same bytes;
  - every input appears in exactly one `let` line, and no session name appears in the batch;
  - a quoting table: spaces, tabs, `"` written as `""`, `$` written as `$$`, backslashes, non-ASCII and empty values, with control characters refused, and installation paths written through `$FUSION_HOME`;
  - list values joined by commas, and a list item that holds a comma refused;
  - real formatting (`1.01`, `-1.01`, `1e-7` written as `0.0000001`) under `de_DE` and `fr_FR` default locales.
- `EventTail`: a line split across two reads, `\r\n`, a file that appears late, and a file that shrinks (an error).
- Exit mapping: the statuses 0, 1, 2, 3 and 130, POSIX signals, and Windows status codes.
- `RunQueue`: the core count per run, first-in first-out order, a run larger than the limit, and cancelling a queued run.
- Run directory names: the Java copy of 09.C1 and rule 1.26 against golden names shared with the C++ tests, `EGFR-to-CXCL8-FSC-JUN` included.
- `FxDispatch` with a fake FX executor: 1,000 messages in one tick reach the screens in one hand-over.

Contract tests against a real `fusion` build (CI, on all three platforms):
- `fusion --commands` matches the catalog schema, and the default pipeline validates against it.
- `fusion --check` accepts every golden batch with fixture inputs (section 12's synthetic network).
- The preview of a fixture LFC file equals the up and down lists that a real `cudf` run writes from the same file (S8.14: "exactly what cudf will produce").
- `labels map=<file>` through `--query` returns one pair per id line of a fixture map, with S1.18's rules: tabs inside names become underscores, blank lines are skipped, and an id with no name falls back to the id.
- The per-value batches of a swept fixture session equal what its exported sweep file runs for each value (10.C21).

Process tests with section 12's `fake-fusion` in place of `fusion`, on all three platforms:
- following a run, with the run log and events;
- Stop within the grace period, and 09.C7's forced exit;
- Kill of a run that has a child process, which must end the descendants too;
- the exit mapping;
- reattachment through `run.lock` after the runner restarts while `fake-fusion` runs, and the fallback on a filesystem without locks;
- the single-instance guard: a second launch writes `frontend.focus` and exits, and the first sees it within one tick.

End to end, on each platform (G3), with the installed package:
- start the application, create the first account, create a session from fixture inputs, quick Submit, follow the run to `finished`, and open its results, the run page included, which opens in the system browser;
- Stop a long fixture run, then Resume it; kill one, then Resume it from its journal; make a fixture run fail, fix its input, and Resume it from its last good step;
- run a two-value sweep and see both runs start at once under the limit;
- quit the application during a run, start it again, and see the run reattached and finished. This answers stage 8's open question 4 for 2.0.

## Open questions

1. **Quick Submit on an edited session (S8.32).** *Closed:* the quick Submit runs the session's current pipeline, the default unless edited (prompt log entry 195).
2. **Stop and liveness rest on section 09's proposals.** *Closed:* 09.C7 and 09.C9 were accepted (entries 184 and 185).
3. **Short-lived `fusion` calls and A1.** *Closed:* read-only calls that make no run directory are outside A1 (entry 168).
4. **Results depend on machine load.** *Closed:* section 06 adds `rlimit`, and `run.json` records the OS, the CPU model and the thread count (entry 176).
5. **Relaxed overrides and the relaxed-edges file.** *Closed:* both are dropped from the frontend (entry 165).
6. **Defaults for the relaxation upper bounds and the timeouts.** *Closed:* 10.C13 as proposed, with section 06's defaults kept for the command line (entry 173).
7. **`exec` in lab mode.** *Withdrawn:* there is no lab-server mode in 2.0 (entry 192), and S8.27 stands as decided.
8. **The symbol-map query has no owner.** *Closed:* section 02's `labels map=<file>` through `--query` (2B.6, entries 158 and 168).
9. **Graph queries for the display (Phase 5).** *Deferred to Phase 5:* the display's design settles it with section 02 (prompt log entry 199).
10. **Lab-server mode.** *Withdrawn:* no lab mode in 2.0 (entry 192).
11. **Skeleton wording after 10.C1.** *Closed:* 10.C11's rename to `ui` was accepted, with wording notes on skeleton C1 and A1 (prompt log entry 198).
