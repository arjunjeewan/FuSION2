# 00 · Architecture skeleton

The top-level shape of FuSION 2.0, which every Phase 2 section follows ([decision 0028](../decisions/0028-phase-2-method.md)). Each **Choice** below is polled on its own before this file is committed; the text gives the proposal, the alternatives, and the Phase 1 decisions behind it. Sections drafted later may not change a choice without a new poll.

## Inputs from Phase 1

The skeleton has to carry these decisions:

- **One interface.** The batch language stays, with named arguments, positional FuSION 1 lines and sweeps ([0018](../decisions/0018-batch-interface.md)). The frontend's pipeline editor turns every drawn pipeline into a batch file, and the standard run is its default pipeline (S8.18, S8.20).
- **Partial runs.** Stop after any step, start from a saved step, resume a stopped solve, solve part of the frontier ([0019](../decisions/0019-partial-runs.md)).
- **Plug-and-play inputs.** The network and the per-gene values may come from any source or modality ([0020](../decisions/0020-plug-and-play-inputs.md)).
- **Checks up front.** Inputs are validated before a run starts, by the backend and by the frontend ([0021](../decisions/0021-required-and-optional-inputs.md), S8.30).
- **Run outputs.** Every output of a run goes to the run's own directory, which is never deleted or reused (S1.26, S5.30, S8.21). Failures stop the batch with a non-zero exit (S1.28). An empty corridor or an empty GSEA selection is a reported result, not a failure (S4.18, S7.03).
- **Recorded settings.** Every result-changing setting is a run setting recorded with the result (S1.29, S2.11, S2.31, S4.04, S5.03, S5.22, S7.11). Debug outputs sit behind one debug option (S1.29, S5.23).
- **No hidden state.** Nothing survives between solves (S5.29). The corridor is computed once and both FSC cases are solved on it (S4.20, S5.17). The solve keeps its models (S5.26) and writes a structured search log (S5.24).
- **Single rules.** There is one LFC parser (S3.14) and one polarity rule (S6.14).
- **Display-ready results.** The graph display is designed in Phase 5, but the backend's results must be in a form it can use ([0027](../decisions/0027-graph-display-in-phase-5.md)).
- **Frontend runs.** The frontend shows a live log with stages, the real exit status and a Stop button (S8.28). It keeps per-user accounts with salted hashes (S8.02) and sessions with stable ids (S8.04, S8.05).
- **Original brief.** The backend is entirely C++ and the frontend is Spring Boot Java, built as one cohesive tool with clean, readable code and efficient implementations (prompt log entry 001).

## Choice A1 · Process model: how the frontend runs the backend

- **Proposed:** the backend is a command-line program, `fusion`. For each run the frontend writes the run's batch file into a new run directory, starts `fusion` on it as a child process, follows its progress, and reads its results from the run directory when it ends. Stop is a stop request, written into the run directory or sent on the backend's standard input. A watcher thread in `fusion` sees it, interrupts the running Z3 check, saves the solve state for resuming and exits with a "stopped" status, the same on every platform (Choice G3). Killing the process stays the last resort, with no saved state (S5.21, [0019](../decisions/0019-partial-runs.md)).
- **Why:**
  - The batch language is then the only interface, shared by command-line users and the frontend ([0018](../decisions/0018-batch-interface.md), S8.20).
  - A crash in Z3 or the C++ code ends one run, not the web server.
  - Each run is reproducible from its own directory.
- **Alternatives:**
  - The backend as a long-running local service (HTTP or gRPC) that the frontend calls. This allows richer interaction, such as querying a graph live, but gives two interfaces to keep in step.
  - A JNI binding of the C++ library inside the JVM. This has the lowest latency, but one native crash takes the frontend down, and it must be packaged for two platforms.
- **Decision:** the CLI child process, as proposed (prompt log entry 104). *Amended after G3 (prompt log entry 107):* Stop is the platform-neutral stop request described above, not a POSIX signal.

## Choice A2 · Progress and status channel

- **Proposed:** the backend writes structured events, one JSON object per line, to `events.jsonl` in the run directory: step started and finished, each solver check with its point and outcome, warnings, the final status. The human-readable console log stays on stdout. The frontend follows `events.jsonl`, so its stage indicator and progress no longer guess from the command echo (S1.10's question, left to Phase 2).
- **Alternative:** parse the console echo, as FuSION 1's GUI does (S1.10, S8.28).
- **Decision:** `events.jsonl`, as proposed (prompt log entry 104)

## Choice B1 · Backend structure

- **Proposed:** a C++ library, `libfusion`, holds all behaviour, and a thin `fusion` executable reads batch files and calls it. The library reports through a logger and event interface and never writes to the console itself. Phase 3's tests call the library directly and also run `fusion` end to end.
- **Alternative:** one executable with no library boundary, as in FuSION 1.
- **Decision:** library plus thin CLI, as proposed (prompt log entry 104)

## Choice C1 · Module list

- **Proposed:** ten backend modules, each implementing its Phase 1 stage, plus five frontend modules:

  | Module | Responsibility | Spec |
  |---|---|---|
  | `core` | ids, errors, logging and events, the settings registry (every result-changing setting with its default, recorded in the manifest), seeded random numbers and stable orderings | cross-cutting |
  | `batch` | the batch language: tokens and quoting, comments, `let` and `$`, named and positional arguments from one command table (S1.12), sweeps, `exec`, failure policy, partial-run control | stage 1 |
  | `network` | the graph model; readers for KGML, FuSION's merged-graph format, the legacy master (S2.27) and plain edge lists (`add`); the writer; the plug-and-play network input; id mapping | stage 2 |
  | `merge` | order-independent union, exception split, merge reuse, merge report | stage 2 |
  | `expression` | the one per-gene value reader, classification (`cudf`), threshold suggestions, the plug-and-play value input | stage 3 |
  | `corridor` | `fb_rch`: traversable subtypes, forward and backward search, the keep rule, distances | stage 4 |
  | `solve` | the Z3 encoding, the Pareto search with its cache, timeouts and budget, resume state, seed, debug dumps, witnesses | stage 5 |
  | `results` | frontier, witnesses, relaxations, sign-consistent routes, structured outputs, the Pareto plot step, the explanation export | stage 6 |
  | `enrichment` | GSEA prerank, selection, keep list, results table | stage 7 |
  | `run` | run directory, manifest, saving and loading step outputs, resume, events | cross-cutting |
  | frontend `web`, `accounts`, `sessions`, `pipeline`, `runner` | pages; accounts; session store; pipeline model, default pipeline and batch generation; process control, events and results index | stage 8 |

  The frontend's screens are Phase 5's ([0027](../decisions/0027-graph-display-in-phase-5.md)).
- **Alternative:** fewer, larger modules, for example `merge` folded into `network`, and `results` into `solve`.
- **Decision:** these modules, as proposed (prompt log entry 104)

## Choice D1 · Data flow between steps

- **Proposed:**
  - Each step is a function from typed values to typed values: network to merged network; values to observations; network, observations and endpoints to corridor; corridor to frontier and witnesses; frontier to outputs.
  - The batch interpreter keeps these values in memory. Graphs keep integer ids, with `0` for the latest (S2.01).
  - Each step's output can be saved to the run directory in a documented format and loaded back, which is how a run stops after or starts from any step ([0019](../decisions/0019-partial-runs.md), S2.26).
  - No step keeps state between calls (S5.29).
- **Alternative:** every step reads and writes files, as FuSION 1's `mff` → `wgx` → `rgx` round trip does (S2.26).
- **Decision:** in memory plus saved steps, as proposed (prompt log entry 105)

## Choice E1 · Output formats and the run manifest

- **Proposed:**
  - Every run directory has a manifest, `run.json`. It records:
    - FuSION's version and the versions of its libraries (Z3 and the others);
    - the batch file and command line;
    - every setting, with its value and whether it came from a default, a positional argument or a name;
    - each input file's path, size and SHA-256;
    - start and end times, the exit status, and the list of outputs.
  - Structured results are JSON: the frontier with each point's certification flag (S5.19), the witnesses keyed by stable ids (S4.14, S5.27) and the corridor summary.
  - Tables are TSV with a header: the search log (S5.24), the GSEA table (S7.16) and the edge sources (S6.08).
  - FuSION 1's text formats (`temp_w_solutions.txt` and the rest) are written only when a run setting asks for them, for old scripts.
- **Alternative:** plain text files only, as in FuSION 1, with a documented layout for each.
- **Decision:** `run.json` plus JSON results and TSV tables, as proposed (prompt log entry 105)

## Choice F1 · Languages for the helpers

- **Proposed:** the whole backend is C++, as the original brief asked:
  - GSEA prerank is reimplemented in C++, seeded, with ties ordered by gene id (S7.05). Its numbers agree with gseapy statistically but not byte for byte (S7.11), which [decision 0023](../decisions/0023-results-may-change-where-improved.md) allows.
  - Threshold suggestion becomes a C++ command (S3.15).
  - The explanation export is written by C++: JSON data plus a static page that uses one shared copy of the drawing library (S6.18, S6.26).
  - gnuplot stays as an external tool for the Pareto plot image, your standing preference for PO curves (S6.22 to S6.24). It is checked before the run like every other dependency (S6.11).
  - No Python remains in the backend.
- **Alternatives:**
  - A C++ core with pinned Python helpers (gseapy for GSEA, pyvis for explanation pages). Results stay closer to FuSION 1's, but there are two runtimes to install and pin (S7.02).
  - C++ draws the Pareto plot itself (SVG and PNG), with no gnuplot.
- **Decision:** all C++, with gnuplot kept for the Pareto image. Measured before the answer: a real Pareto plot through `plot_po.sh` takes 0.11 s (gnuplot alone 0.06 s for PNG, 0.02 s for SVG), against well under a millisecond for an SVG written by C++; one plot per run makes the difference negligible (prompt log entries 105 and 106)

## Choice G1 · Backend language level, build and dependencies

- **Proposed:**
  - C++20, built with CMake, with GCC 12 or later or Clang 15 or later (Apple Clang on macOS). FuSION 1 builds as C++0x with no optimisation.
  - Z3 through its C++ API, at a pinned version recorded in the manifest. This machine has Z3 4.8.12, which FuSION 1 links.
  - pugixml for XML, replacing FuSION 1's libxml++ 2.6 and its glibmm chain.
  - nlohmann/json for JSON.
  - Catch2 for tests.
  - The small libraries are fetched at pinned versions by the build; Z3 comes from the system or a pinned download.
- **Alternatives:** keep libxml++ (FuSION 1's parser); GoogleTest instead of Catch2; C++17.
- **Decision:** C++20, CMake and pugixml, as proposed, with nlohmann/json and Catch2 (prompt log entry 105)

## Choice G2 · Frontend stack

- **Proposed:** Java 21 LTS (installed here) with Spring Boot 3, built with Gradle.
- **Alternative:** Maven instead of Gradle.
- **Decision:** Java 21, Spring Boot 3 and Gradle, as proposed (prompt log entry 106)

## Choice G3 · Platforms

- **Proposed:** Linux (x86-64) and macOS (Apple silicon and x86-64) are both supported first-class, with one source tree for both. FuSION 1 keeps a separately drifting `macOS/` copy. Windows is out of scope.
- **Alternative:** Linux only, with macOS best-effort.
- **Decision:** Linux, macOS and Windows, all three native, from one source tree. The user: "rn windows works w the linux version through WSL, but ideally the end biologist user would not want to set that up." Consequences: the Stop mechanism must not rely on POSIX signals (Choice A1); section 01 settles which shell `exec` uses on Windows, whose FuSION 1 batch files carry POSIX shell lines; section 11 packages each platform so that a biologist installs one thing, gnuplot and Z3 included (prompt log entry 106)

## Choice H1 · Sections drafted after the skeleton

- **Proposed:** twelve sections, drafted in parallel, each with the Phase 1 questions assigned to it:

  | Section | Phase 1 questions it answers |
  |---|---|
  | 01 Batch language and interpreter | named-argument and sweep syntax ([0018](../decisions/0018-batch-interface.md)); the commands for partial runs ([0019](../decisions/0019-partial-runs.md)); how a batch names its run directory (S1.26); the debug option (S1.29) |
  | 02 Network model, inputs and id mapping | plug-and-play network inputs ([0020](../decisions/0020-plug-and-play-inputs.md)); id mapping across sources (S2.14); the merged-graph format (S2.25); the legacy reader (S2.27); the ECrel and indirect-effect settings (S2.31, S2.11) |
  | 03 Merge | the union, the exception split and merge reuse (S2.17, S2.22) |
  | 04 Expression | plug-and-play value inputs ([0020](../decisions/0020-plug-and-play-inputs.md)); the one parser (S3.14); the form of threshold suggestion and its plot (S3.15, S3.19) |
  | 05 Corridor | traversable subtypes (S4.04); trimming (S4.11); distances (S4.07) |
  | 06 Solve | the Z3 layer; search and cache (S5.18 to S5.20); timeouts and budget (S5.21); resume and partial frontiers ([0019](../decisions/0019-partial-runs.md)); dumps and their reuse (S5.23, roadmap objective 4) |
  | 07 Results and outputs | the structured results; the Pareto plot step (S6.22); the explanation export (S6.26); FuSION 1's text formats |
  | 08 Enrichment | GSEA in C++ (S7.02, S7.05, S7.11) |
  | 09 Runs | run directory, manifest, saved steps, resume, events |
  | 10 Frontend architecture | accounts (S8.02); sessions (S8.04, S8.05); the pipeline model and default pipeline (S8.18 to S8.20); the runner (S8.28); validation (S8.30). Screens are left to Phase 5. |
  | 11 Build, dependencies and platforms | the concrete versions behind G1 to G3 |
  | 12 Test architecture | the hooks Phase 3 needs: library-level tests, deterministic outputs, and a reference-test runner that compares 2.0 with FuSION 1 runs against a recorded list of expected differences ([0023](../decisions/0023-results-may-change-where-improved.md)) |
- **Alternative:** fewer, larger sections, for example 03 folded into 02 and 09 into 01.
- **Decision:** these twelve sections, as proposed (prompt log entry 106)
