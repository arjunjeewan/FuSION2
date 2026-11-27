# 09 · Runs

## Scope

This section designs the `run` module and the run-level parts of `core` (Choice C1): the settings registry, the log and event interfaces, the error and failure conventions every module follows, ids and stable orderings, and seeded random numbers. Section 11's four components (build information, the install directory, the platform layer and the dependency registry) also live in `core`, by 11.C19, which amends C1's description of `core` (prompt log entry 204); this section uses them and does not design them. It answers these Phase 1 questions:

- the run directory: its name and layout, never deleted or reused (S1.26, S5.30, S8.21);
- the `run.json` manifest (E1) and `events.jsonl` (A2);
- the platform-neutral stop request (A1);
- saved steps: how each step's output is saved, found and loaded, how a run restarts from its last good step and how a stopped solve resumes ([decision 0019](../decisions/0019-partial-runs.md), D1);
- the per-run debug log (S1.25);
- the shared store for reusable results, outside every run directory (S2.22).

It also covers the settings registry (S1.29, E1), the error conventions and the one definition of a reported result (S1.28, S4.18, S7.03), where outputs land (S1.26, S8.21), the checks before a run and input hashing ([decision 0021](../decisions/0021-required-and-optional-inputs.md)), two runs at once (S8.28) and disk use (S5.23).

Left to other sections:

- **01:** the batch syntax and `fusion` options that give a run its label and root, set run-level settings, adopt a directory, check only, name a parent run, load a saved step and switch on the debug option; which inputs a batch makes for itself (01.C12); sweep semantics and point labels; the command table and its cross-argument rules (01.C9).
- **02 to 08:** each module's saved format, the file names inside its step directory, its fingerprint fields, its own events and its own error codes. Section 06 defines the solve's journal and case statuses, and with section 01 how a batch solves part of the frontier (decision 0019). Section 03 lays out, keys and checks its merges in the shared store. Section 08 specifies GSEA's own generator, which sits in `core` (08.C1).
- **10:** how the frontend creates, follows, stops and shows runs.
- **11:** the libraries, toolchains and packages this design assumes, the external-tool check (`core::deps`), the process layer and the `run.json` blocks it supplies.
- **12:** the harnesses behind the test hooks below, and the one copy of the JSON Schemas.

Evidence used below:

- "The reference run", "the full run" and "fdr050" are as in stage 5. "The candidate set" is `runs/egfr_b10_gsea_fdr044_candidates/`.
- Counts over `runs/` were taken with `find`, `du`, `stat` and `sha256sum` on 2026-11-12 at 18:38, and the solver-check counts again at 19:32. Runs still going add to both: new run sets were being added that day, so some counts exceed the spec's (199 SMT dump directories, against 171 in S5.23).
- FuSION 1 was not built or run. Three small probes ran in a scratch directory: Java's `FileChannel.tryLock()` against locks taken with Python's `fcntl` and `flock`; a g++ 13.3 timing of `std::mt19937_64`; and `cp`, `stat` and `sha256sum` timings on real run inputs.

## Requirements carried in

Run directory and outputs:

- **S1.26, S5.30:** every fixed-name output goes under one output directory per run, named by solve case for the solve, and nothing is written to the launch directory. Each owning stage keeps its file names. Partial runs find saved steps there. A relative or omitted output path lands in the step's directory, and an explicit absolute one is honoured as well, with the file also kept in the step directory; section 07's site export is the one other write outside (cross-section items 2B.3 and 4A.11, the reading of S1.26 and S8.21 accepted in prompt log entry 169).
- **S8.21:** every run gets a new directory of its own; none is deleted; every output goes in it, SMT dumps included; the run's full parameters are recorded there. A resumed or restarted run is a new run, so it gets a new directory too.
- **S8.15:** the frontend writes each kind of per-gene override to a one-id-per-line file in the run directory.
- **S8.28:** each run has its own directory, and a second run never shares one; the frontend shows the real exit status and offers Stop.
- **S3.10:** outputs are never truncated before the inputs are checked, and output paths may not clash with each other or with an input.
- **S4.19, S5.23:** `fb_rch`'s traces and the SMT dumps are written only under the debug option, into the run's directory; dumps are named by solve case and point.
- **S6.11:** helper dependencies such as gnuplot are checked before the first step.

Manifest, events and logs:

- **E1:** `run.json` records FuSION's version and its libraries' versions, the batch file and command line, every setting with its value and source (default, positional or named; a run-level setting may also come from `fusion --set` or the `run` line, entry 229), each input file's path, size and SHA-256, the start and end times, the exit status and the list of outputs.
- **A2:** `events.jsonl` in the run directory holds step started and finished, each solver check with its point and outcome, warnings and the final status. The human-readable console log stays on stdout.
- **S1.10:** the command echo stays; the frontend tracks progress from events, not from the echo.
- **S1.25:** one debug log per run, with a per-run name that can be set before `start`.
- **S2.29:** `upfx` is dropped; S1.25 and S1.26 cover its purpose.
- **S1.27:** numbers on the console are printed with enough digits to be exact.
- **B1:** the library never writes to the console; it reports through a logger and an event interface.
- **S5.24, S5.31:** the solve writes a structured per-check search log and prints its milestones with wall time.

Failures and partial runs:

- **S1.01, S1.02:** a missing, unreadable or empty batch file stops `fusion` with a message and a non-zero status; usage after a bad invocation goes to stderr.
- **S1.28:** errors are caught per command and reported with the batch line number; the batch stops at the first failure and exits non-zero; a failing `exec` is a failure (S1.14). Partial runs restart from the last good step. In a sweep this holds per point (01.C5, entry 188).
- **S4.18, S7.03:** an empty corridor and an empty GSEA selection are reported results, not failures. After any reported result, every later line of that run or point is skipped, which widens S4.18's wording (01.C15, accepted in entry 167).
- **S6.21:** a solve with no Pareto point still writes its results, with an explicit "no PO point" result.
- **Decision 0019:** stop after any step, start from a saved step, resume a stopped solve, solve part of the frontier. Every step's output must be saveable and loadable.
- **D1:** steps pass typed values in memory; each step's output can be saved to the run directory in a documented format and loaded back.
- **S2.26:** the written merged graph doubles as a saved step.
- **S4.21:** the corridor records its parameters and can be saved and loaded.
- **S5.20, S5.21:** the solve caches TIMEOUT and UNDEFINED answers and keeps that cache across a resume, except that a TIMEOUT is asked again under a longer timeout (06.C8, amending S5.20, entry 174); an optional whole-solve budget stops cleanly and stays resumable.
- **S5.29:** no state survives a solve.
- **S7.18:** GSEA's choice is reviewed by stopping after `gsea` and starting again from its saved shortlist.
- **A1:** the frontend writes the batch file into a new run directory and starts `fusion` on it as a child process. Stop is a request written into the run directory or sent on stdin; a watcher thread interrupts the running Z3 check and exits with a "stopped" status, the same on every platform. Prompt log entry 107 chose this request over "signals, plus a Windows path". Phase 2 amended A1 so that a killed or crashed run's journal is used on resume (06.C19, entry 174), and read it as allowing signal handlers that only make the same request (09.C8, entry 185) and read-only calls that make no run directory (entry 168).

Inputs and reuse:

- **Decision 0021:** all inputs are checked before the first step. A missing required input stops the run; an optional input not given turns its feature off; one given but empty is an empty list; one named but missing or unreadable stops the run before it starts. The frontend validates too, with one set of rules (S8.30). An input that the batch makes for itself is checked just before its step instead (01.C12, amending decision 0021, S1.15 and S2.16, entry 188).
- **Decision 0017:** FuSION 1 batch files keep running, including those whose `exec` lines make the files that later steps read, such as `pathz3`'s node files.
- **S2.22:** each pathway list is merged once without the run's exceptions, and runs on the same pathways share that merge.
- **S7.02:** library versions are pinned and recorded with each result.

Settings and determinism:

- **S1.29:** every flag that changes results becomes a run-time setting recorded in the run's output; debug flags sit behind one debug option that the frontend does not show. Each flag's kind is confirmed by the stage that owns it.
- **S5.22, S7.11:** the solver's seed (default 42), GSEA's seed (default 42) and its permutation count (default 1000) are recorded.
- **S4.14, S7.05:** outputs name nodes and edges by stable keys, and ties are broken by gene id, so no output depends on internal ids or container order.
- **F1:** GSEA reproduces gseapy 1.3.1 bit for bit, with gseapy's own generator ported into `core` (08.C1, amending F1, entry 159).
- **Decision 0023:** results may change only where 2.0 improves. Nothing in this section may change a result: copies, caches, restarts and resumes must give what an uninterrupted run gives.
- **G1, G3:** C++20 on GCC 13 or later and Clang 17 or later, so `std::format` may be used (11.C22, amending G1, entry 200); Linux, macOS 13.3 or later (11.C1) and Windows, all native, from one source tree; no POSIX signals in the Stop mechanism.

## Design

### Components

| Component | Module | Job |
|---|---|---|
| `Log`, `Events` | core | the only way a module reports (B1) |
| `Failure`, `UsageError`, `InstallError`, `Report`, `Outcome`, `Stopped` | core | how a step ends other than with plain success |
| `StopSource`, `StopToken`, `StopCallback` | core | carry a stop request into every step |
| `SettingSpec`, `Settings` | core | the settings registry |
| `Interner`, `GeneId`, `Sym`, `stable_compare`, `stable_hash` | core | ids and stable orderings, as section 02 outlined them (cross-section item 2A.5) |
| `Sha256`, `Rng`, text helpers | core | digests, random numbers outside GSEA, determinism |
| `Diagnostic`, `CheckReport` | core | the one type behind `fusion --check --json` (cross-section item 1.9) |
| `Context`, `Files`, `OutputFile`, `Journal`, `Cache` | core | what a step receives; implemented by `run` |
| `Run` | run | one process's run: directory, lock, manifest, events, log, stop watcher, steps |
| `Manifest` | run | `run.json` |
| `EventLog`, `RunLog` | run | `events.jsonl` and the run log |
| `StopWatcher` | run | sees the `stop` file and the executable's signal flag |
| `Inputs` | run | copies, hashes and checks the inputs |
| `Registry` | run | the saved steps of a run directory: find, verify, match |
| `Store` | run | the shared store |
| `Lock` | run | claims a run directory, shows whether it is live, and locks any file |

Section 11 puts beside these in `core`: `BuildInfo`, `InstallDir`, the platform layer (`Process`, CPU time, long paths) and the dependency registry (`core::deps`); section 08 puts GSEA's generator there (08.C1), and sections 07 and 11 the shared gnuplot runner (cross-section item 2B.13).

Dependencies point one way. Every module depends on `core`; `run` depends on `core`; only `batch` and the `fusion` executable use `run`. Modules never see `run` types: they get a `core::Context`, so they can be tested with in-memory sinks (B1, section 12).

### The run directory

**Name (09.C1).** `<label>_<YYYYMMDD-HHMMSS>` in local time, for example `egfr-cxcl8-jun-b10_20261112-184400`.

- The label comes from section 01's `run root=… label=…` line or `--label`, by default the batch file's stem, and the root from the same line or `--root`, by default the batch file's directory (01.C7). The frontend's label is the session's name, with the swept value appended for a sweep (10.C18).
- The label is reduced code point by code point (cross-section item 1.26): each run of code points outside `[A-Za-z0-9._-]` becomes one `-`, and leading and trailing `-` are trimmed. `EGFR→CXCL8 / b10` becomes `EGFR-CXCL8-b10`, and `EGFR to CXCL8, FSC JUN` becomes `EGFR-to-CXCL8-FSC-JUN`. The label is then cut to 44 characters, so a name with its timestamp and any suffix stays within 64. Section 10's Java copy of the rule shares one golden list with the C++ one. With no label the name is `run_<timestamp>`.
- The directory is claimed with one `std::filesystem::create_directory`, which is atomic on all three platforms. If the name exists, `-2`, `-3` and so on are tried, so two runs started in the same second under one label get different directories.
- Windows' rules are applied on every platform, so a name made on Linux is valid on Windows: no `<>:"/\|?*`, no trailing dot or space, no reserved name such as `CON` or `NUL`.
- The run id, `r-<UTC yyyymmddThhmmssZ>-<8 hex digits>`, is separate from the name. It never changes, even if the directory moves, and the frontend keys on it (S8.29); the session's id reaches `run.json` through `--tag session=<id>` (10.C18). The run id's random part comes from `std::random_device` and never affects a result.

**Adoption.** A1 has the frontend create the directory and write the batch file into it, and S8.15 has it write the override files there too (section 10: `run.batch` and `overrides/<kind>.txt`). `fusion` then adopts that directory instead of making one (section 01's `--run-dir`). It claims the directory by creating `run.lock` exclusively (09.C9); a directory that already holds a `run.lock` has been used and is refused. The batch and the override files are recorded as inputs where they lie, not copied (09.C3).

**Layout (09.C2; cross-section items 1.4, 1.22 and 3A.15, accepted in entries 158 and 177).** One directory per step that saves something, named by the command's canonical name in section 01's table, with `-2`, `-3` for repeats. The reference experiment, run in 2.0 from the command line:

```
egfr-cxcl8-jun-b10_20261112-184400/
  run.json                                  manifest (E1)
  events.jsonl                              events (A2)
  egfr-cxcl8-jun-b10_20261112-184400.log    run log (S1.25)
  run.lock                                  claimed at start, locked while running
  stop                                      only once a stop is requested
  bindings.tsv                              how every listed id bound: section 02's table, with 06's line and note;
                                            written once when the run ends, whatever its status
  index.html                                the run page, written once when the run ends (section 07)
  inputs/                                   checked copies of every input (09.C3)
    egfr_cxcl8_jun_b10.batch
    egf_hela_t120_lfc.tsv
    hsa_to_gene_symbol_map.txt
    hsa_path_to_path_name_map.txt
    hsa_ids_not_to_be_merged.txt
    hsa_stringdb_coexpression_links_over400.csv
    list_of_kegg_xml_files.txt
    list_of_kegg_xml_files/hsa03320.xml     … 167 files
  cudf/up_reg  cudf/down_reg                observations (section 04)
  gsea/gsea_kgml_files.txt                  shortlist (section 08)
  gsea/gsea_kgml_files.gsea.tsv             its table (S7.16)
  mff/merged_graph.xml  mff/exceptions.tsv  merged network and exception set (sections 02, 03)
  fb_rch/corridor.json                      corridor (section 05)
  fb_rch/observations_match.tsv             where every observed gene falls (05.C9)
  pathz3/results.json                       the solve's results, every case (07.C1)
  pathz3/in.dat  pathz3/out.dat             each case's frontier, for gnuplot (section 07)
  pathz3/in/  pathz3/out/                   case.json, journal.jsonl and per-case files (section 06)
  plot_pareto/pareto.png  pareto.gp         Pareto plot (section 07)
  pex/explain/in_n1_e0.json  in_n1_e0.html  explanation pages (07.C2)
  pex/lib/                                  their drawing library
  debug/                                    only under the debug option
    fb_rch/forward.tsv  backward.tsv        traces (S4.19)
    pathz3/in/dumps/n0_e4.smt2 …            dumps by case and point (S5.23, section 06)
```

- A run the frontend adopted also holds, at the top level, its `run.batch` and `overrides/` (S8.15), and its `inputs/` then holds no copy of the batch.
- FuSION 1's text files go in `pathz3/legacy/`, only when the run setting `legacy_prefix` asks for them (E1, cross-section item 1.21).
- A sweep's directory holds section 01's `points.tsv` and the coordinator's own `run.json`, `events.jsonl` and log. Each point is a complete run directory of its own in `points/<NN>-<label>/` (section 01), with everything above.
- S5.30's "named by solve case" is met by the case labels inside `results.json` and the per-case names (cross-section item 4A.12, entry 177).

Rules:

- FuSION never deletes a run directory, and never overwrites a committed output in one. `run.json` is the one file it rewrites, and only while the run lasts. The run-level files, section 02's `bindings.tsv` and section 07's run page `index.html`, are written once, through `Run::create_run_file`, when the run ends, whatever its status (entry 230); a forced exit or a kill leaves them unwritten. The only files it removes are its own `*.tmp` files after a failed write.
- Every backend write goes through `OutputFile`. A step gets its files from `Files`, only in its step directory and in `debug/<step>/`; the run-level files come from `Run::create_run_file`. An `OutputFile` is written as `<name>.tmp` and renamed when committed, so a stop, a crash or a full disk never leaves a half-written file under its real name, and a failed step never truncates an earlier output (S3.10). On Windows the rename is retried ten times, 50 ms apart (cross-section item 1.28).
- `inputs`, `debug`, `points` and `overrides` are reserved names; section 01's table must not use them for commands.
- `run.lock` holds one line for people to read, the process id and the run id, such as `41872 r-20261112T131400Z-7f3a9c2e`. The lock itself sits on one byte past that line (09.C9).
- Commands with nothing to save (`start`, `sz`, `exec`, `help`) get no directory. They are still steps in `run.json` and `events.jsonl`.
- Only a batch run makes a run directory. The check-only mode (09.C12) and section 04's query mode (04.C8) write nothing.
- Path lengths: run names are at most 64 characters and point labels 32 (section 01), and modules keep their file names under 40, so a path inside a run directory stays under 100 characters when the input files' own names are short. On Windows without long-path support, where a whole path is limited to 260, the check warns when the run directory's own path exceeds 160 characters.

### Where outputs land

The rule for every output argument (cross-section items 2B.3 and 4A.11, the reading of S1.26 and S8.21 accepted in entry 169), such as `cudf`'s `up_out` and `down_out`, `gsea`'s shortlist, `wgx`'s file, `add`'s output, a report or a positional `pathz3` prefix:

- A relative or omitted output path lands in the step's directory: `cudf … up_out=up.txt` writes `cudf/up.txt`, and `cudf` in the named form without outputs writes `cudf/up_reg` and `cudf/down_reg` (04.C11).
- An explicit absolute path is honoured as well. The file is written in the step directory and then copied to that path, both through `OutputFile`, and `run.json` records both (`path` and `also`). FuSION 1 batches read such files through `exec`: the reference batch `cat`s `cudf`'s `$UP_REG_FILE` and `$DOWN_REG_FILE` from `$WRITE_DIR` (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch:153`).
- Section 07's site export is the one other write outside the run directory, as an explicit exception. It takes the site's lock through `Lock::acquire` and writes through `OutputFile`.
- Section 01's rule for relative paths (01.C2) covers inputs only.
- The check refuses an output that clashes with another output or an input, comparing file identity, not path text, as section 04 designs (`output.clash`; cross-section item 2A.7).

### run.json

The reference experiment's manifest, shortened. The `versions`, `tools`, `platform`, `install` and `environment` blocks are section 11's (cross-section item 1.14):

```json
{
  "format": "fusion.run",
  "format_version": 1,
  "run": "r-20261112T131400Z-7f3a9c2e",
  "name": "egfr-cxcl8-jun-b10_20261112-184400",
  "status": "ok",
  "exit": 0,
  "parent": null,
  "point": null,
  "pid": 41872,
  "labels": {"session": "s-01hzx3"},
  "started": "2026-11-12T13:14:00.412Z",
  "finished": "2026-11-12T13:24:58.512Z",
  "resources": {"wall_s": 658.099, "cpu_s": 655.214, "peak_rss_bytes": 326574080},
  "build_id": "2.0.0+3f2c9a1e0d4b7c6a5e8f9d0c1b2a3f4e5d6c7b8a",
  "versions": {
    "fusion": {"version": "2.0.0", "git_commit": "3f2c9a1e0d4b7c6a5e8f9d0c1b2a3f4e5d6c7b8a",
               "git_dirty": false, "build_type": "Release", "compiler": "GNU 14.2.1", "target": "linux-x86_64"},
    "libraries": {"z3": "5.3.3.0", "pugixml": "1.16", "nlohmann_json": "3.12.0", "fast_float": "8.3.3"}
  },
  "tools": {
    "gnuplot": {"version": "6.0.5", "origin": "bundled", "features": ["pngcairo"],
                "needed_by": ["plot_pareto (line 12)"],
                "path": "/opt/fusion/lib/app/backend/share/fusion/tools/gnuplot/bin/gnuplot"}
  },
  "platform": {"os": "Ubuntu 24.04.5 LTS", "kernel": "7.0.0-38-generic", "arch": "x86_64",
               "cpu": "Intel(R) Xeon(R) Gold 6140 CPU @ 2.30GHz", "logical_cpus": 36},
  "install": {"prefix": "/opt/fusion/lib/app/backend", "home": "/opt/fusion/lib/app/backend/share/fusion",
              "source": "executable"},
  "environment": {"FUSION_HOME": null, "FUSION_GNUPLOT": null, "FUSION_DOT": null, "FUSION_CACHE_DIR": null},
  "command_line": ["fusion", "-b", "egfr_cxcl8_jun_b10.batch"],
  "launch_dir": "/home/u/FuSION",
  "batch": {"path": "/home/u/FuSION/egfr_cxcl8_jun_b10.batch", "size": 1873, "sha256": "0b6f2d…",
            "copy": "inputs/egfr_cxcl8_jun_b10.batch"},
  "variables": [
    {"name": "$LFC", "value": "/home/u/data/egf_hela_t120_lfc.tsv", "line": 2},
    {"name": "$FSC", "value": "hsa3725", "line": 3}
  ],
  "run_settings": [
    {"key": "run.cache", "value": true, "source": "default"},
    {"key": "run.debug", "value": false, "source": "default"},
    {"key": "run.log", "value": "egfr-cxcl8-jun-b10_20261112-184400.log", "source": "default"}
  ],
  "inputs": [
    {"step": "cudf", "argument": "values", "path": "/home/u/data/egf_hela_t120_lfc.tsv", "size": 41571,
     "sha256": "777689a6…", "copy": "inputs/egf_hela_t120_lfc.tsv"},
    {"step": "gsea", "argument": "candidates", "path": "/home/u/kegg/list_of_kegg_xml_files.txt", "size": 14982,
     "sha256": "86fc603f…", "copy": "inputs/list_of_kegg_xml_files.txt"},
    {"step": "gsea", "argument": "candidates", "member_of": "inputs/list_of_kegg_xml_files.txt",
     "path": "/home/u/kegg/hsa04010.xml", "size": 69314, "sha256": "…",
     "copy": "inputs/list_of_kegg_xml_files/hsa04010.xml"}
  ],
  "steps": [
    {"id": "gsea", "cmd": "gsea", "line": 8,
     "normalised": "gsea values=/home/u/data/egf_hela_t120_lfc.tsv candidates=/home/u/kegg/list_of_kegg_xml_files.txt selector=fdr:0.25",
     "status": "ok", "started": "2026-11-12T13:14:00.561Z", "wall_s": 1.329, "fingerprint": "3be1a0…",
     "settings": [
       {"key": "gsea.permutations", "value": 1000, "source": "default"},
       {"key": "gsea.seed", "value": 42, "source": "default"},
       {"key": "gsea.selector", "value": "fdr:0.25", "source": "named"}
     ],
     "outputs": [
       {"kind": "shortlist", "format": "fusion.shortlist", "format_version": 1,
        "path": "gsea/gsea_kgml_files.txt", "size": 3768, "sha256": "0226b6d7…"},
       {"kind": "table", "format": "fusion.gsea_table", "format_version": 1,
        "path": "gsea/gsea_kgml_files.gsea.tsv", "size": 71846, "sha256": "…"}
     ]},
    {"id": "mff", "cmd": "mff", "line": 9, "status": "ok",
     "details": {"merge_key": "0e91f9a9…", "base_merge": "store"},
     "outputs": [{"kind": "network", "format": "fusion.network", "format_version": 1,
                  "path": "mff/merged_graph.xml", "…": "…"}]},
    {"id": "pathz3", "cmd": "pathz3", "line": 11, "status": "ok",
     "settings": [
       {"key": "solve.fsc", "value": ["hsa3725"], "source": "named"},
       {"key": "solve.node_max", "value": 8, "source": "named",
        "effective": 3, "reason": "3 changed genes lie in the corridor (S4.16)"},
       {"key": "solve.timeout", "value": 14400000, "source": "named"}
     ],
     "details": {"summary": "…section 07's points per case, least relaxations and FSC verdict…"},
     "outputs": [{"kind": "solve", "part": "in", "…": "…"}, {"kind": "solve", "part": "out", "…": "…"},
                 {"kind": "results", "path": "pathz3/results.json", "…": "…"}]}
  ],
  "reports": [],
  "error": null,
  "stop": null
}
```

- **When it is written:** at the start (status `running`), when a step ends (at most once a second; the last change is always written within a second), on a stop or failure, and at exit. Each write goes through `OutputFile`, so a reader always sees a whole file.
- **`status`:** `running`, `ok`, `failed` or `stopped`. `running` while `run.lock` is free means the process died (09.C9); readers report it as "died".
- **`pid` and `started`:** in the first version of the file, so a frontend that restarts can find the process again (section 10).
- **`build_id`:** section 11's version and commit, which every fingerprint uses (09.C5, 11.C16).
- **`point`:** for a sweep point, its index, label and values (section 01); `null` outside sweeps. A sweep coordinator's `run.json` lists instead its `points`, each with its index, label, values, directory and status, as section 01's `points.tsv` does.
- **`variables`:** section 01's variables, in definition order, with their lines.
- **`steps`:** every planned step is listed from the start, as `pending`, so the frontend can show progress as step k of n. A step's `status` is then `running`, `ok`, `reported`, `skipped`, `failed`, `stopped`, or `loaded` (taken from a parent run, 09.C5). A step whose module reports a partial result, such as a solve with a case ended `stopped` or `budget_exhausted` (06.C3), also has `"partial": true`.
- **`normalised`:** section 01's normalised form of the step's line.
- **`settings`:** every setting of the step, given or defaulted, by its registry key, with the source that applied: `positional`, `named`, `set`, `run_line` or `default` (the precedence rule under the settings registry). When a step changes a value, as the node-bound clamp does (S4.16, S5.07), it records `effective` and `reason`. A duration's value is whole milliseconds.
- **`details`:** fields the owning module adds to its step: section 03's merge key and where the base merge came from, section 07's results summary (cross-section item 2B.7).
- **`outputs`:** each committed file with its kind, format, format version, size and SHA-256, and `also` for the copy at an absolute path. The kinds later steps load are `network`, `observations`, `shortlist`, `corridor` and `solve`; the rest are final results.
- **`fingerprint`:** identifies a step's work (09.C5).
- **`reports`, `error`, `stop`:** the reported results; the failure (code, message, step, batch line, file and line in it); the stop request (source, time, note, whether the exit was forced).
- **Volatile fields,** which section 12's normaliser masks (cross-section item 1.14): `run`, `name`, `pid`, `labels`, `started`, `finished`, `resources`, `launch_dir`, `command_line`, `platform`, `install`, `environment`, every `path` and `also` that is absolute, `batch.path`, `parent`, `steps/*/started`, `steps/*/wall_s`, `tools/*/path`, and `stop`.
- Readers ignore keys they do not know. `format_version` changes only when an existing key changes meaning. This section writes `run.json`'s JSON Schema, and section 12 keeps the one copy in `backend/tests/schemas/` (cross-section item 2A.10).

### events.jsonl (09.C10)

Each line is one JSON object, UTF-8, ended by `\n`, written with one `fwrite` and flushed. The envelope is `v` (1), `seq` (1, 2, 3, …), `t` (UTC with milliseconds) and `type`; `core::Events` also adds `step`, `line` and `point` to every event emitted inside a step. Types are dotted, `<area>.<event>`, like the error codes (cross-section item 1.3, entry 190). A reader takes only complete lines and ignores types and keys it does not know.

| `type` | When | Fields beyond the envelope |
|---|---|---|
| `run.started` | the directory is claimed | `run`, `name`, `dir`, `pid`, `fusion` (the build id), `parent` |
| `inputs.checked` | the checks end | `inputs`, `bytes`, `errors`, `warnings` |
| `step.started` | a step begins | `cmd` |
| `step.progress` | inside long steps, at most once a second per step | `done`, `total`, `unit` |
| `step.finished` | a step ends | `status`, `wall_s`, `outputs`, and `report` (`code`, `message`) or `error` (`code`, `message`) |
| `step.skipped` | after a reported result or a stop | `because` |
| `warning`, `error` | each warning and error line | `code` (dotted), `message` |
| `stop.requested` | the watcher sees a request | `source` (`file` or `signal`) |
| `run.finished` | the last line of any run that does not die | `status`, `exit`, `wall_s`, `forced` |
| `solve.check.started` | before each solver check (09.C10) | `case`, `check`, `n`, `e`, `phase` |
| each module's own | named by its section in this style: section 06's `solve.check`, section 01's `sweep.planned` | |

Reported results travel only in `step.finished`'s `report`, and warnings only in `warning`. Size: at 19:32 the 222 FuSION 1 solves with a limits file had made 2,723 checks, at most 28 in one solve. Two cases therefore give at most about 112 check events, and a whole run's file is 150 to 200 lines, 30 to 40 KB. This section writes `events.jsonl`'s JSON Schema, which section 12 keeps (2A.10).

The reference experiment, shortened:

```
{"v":1,"seq":1,"t":"2026-11-12T13:14:00.413Z","type":"run.started","run":"r-20261112T131400Z-7f3a9c2e","name":"egfr-cxcl8-jun-b10_20261112-184400","dir":"/home/u/fusion-runs/egfr-cxcl8-jun-b10_20261112-184400","pid":41872,"fusion":"2.0.0+3f2c9a1e…","parent":null}
{"v":1,"seq":2,"t":"2026-11-12T13:14:00.531Z","type":"inputs.checked","inputs":175,"bytes":7252671,"errors":0,"warnings":0}
{"v":1,"seq":7,"t":"2026-11-12T13:14:00.561Z","type":"step.started","step":"gsea","line":8,"point":"","cmd":"gsea"}
{"v":1,"seq":8,"t":"2026-11-12T13:14:01.890Z","type":"step.finished","step":"gsea","line":8,"point":"","status":"ok","wall_s":1.329,"outputs":["gsea/gsea_kgml_files.txt","gsea/gsea_kgml_files.gsea.tsv"]}
{"v":1,"seq":14,"t":"2026-11-12T13:14:05.002Z","type":"solve.check.started","step":"pathz3","line":11,"point":"","case":"in","check":1,"n":0,"e":4,"phase":"diagonal"}
{"v":1,"seq":15,"t":"2026-11-12T13:14:14.702Z","type":"solve.check","step":"pathz3","line":11,"point":"","case":"in","check":1,"n":0,"e":4,"phase":"diagonal","source":"z3","outcome":"unsat","wall_s":9.7,"cpu_s":9.7}
{"v":1,"seq":61,"t":"2026-11-12T13:24:58.512Z","type":"run.finished","status":"ok","exit":0,"wall_s":658.099,"forced":false}
```

### The run log (09.C11)

`<run name>.log` holds every console line, each with its UTC time and level, and under the debug option also the debug records, which never reach the console. In this example the `debug` line appears only because the option is on:

```
2026-11-12T13:14:00.412Z info  FuSION 2.0.0 (build 2.0.0+3f2c9a1e…), run r-20261112T131400Z-7f3a9c2e
2026-11-12T13:14:00.413Z info  Run directory: /home/u/fusion-runs/egfr-cxcl8-jun-b10_20261112-184400
2026-11-12T13:14:00.531Z info  Checked 175 inputs (7252671 bytes) and copied them to inputs/
2026-11-12T13:14:01.890Z info  gsea: selected 42 of 166 pathways (fdr:0.25)
2026-11-12T13:14:02.904Z debug fb_rch: forward set 354 nodes; forward graph 376 nodes, 895 edges
2026-11-12T13:14:02.911Z info  fb_rch: corridor 115 nodes, 370 edges
2026-11-12T13:24:58.512Z info  Finished: ok, exit 0, 658.099 s wall, 655.214 s CPU, peak RSS 326574080 bytes
```

- Its name is per run and can be changed with the `run.log` setting before the first step (section 01's `run log=` or `--log`), which meets S1.25's "set before `start`": the batch is read whole before anything runs (S1.01).
- `fusion` captures each `exec` child's standard output and error through section 11's process layer and passes every line through `core::Log`, so the run log holds them too (cross-section item 1.15). The frontend (10.C17) and section 01's sweep coordinator start `fusion` with its console on the null device, so the run log is the one record of the console.
- Console conventions for every module: information goes to stdout; warnings and errors go to stderr, starting `warning:` or `error:` and giving the batch line where known. Numbers are printed exactly (S1.27), through `core::format_number`.

### Stop (09.C7, 09.C8)

1. A request arrives: the file `stop` appears in the run directory, or the executable's handler for Ctrl-C or SIGTERM sets a flag (09.C8). Only the file's existence matters. Its content is kept in `run.json` as text, as the stop's note; section 10 writes it under a temporary name and renames it.
2. The watcher thread, which looks every 0.5 s, calls `StopSource::request_stop()` once and emits `stop.requested`. *Phase 3 (prompt log entry 356):* it first emits the event and writes `run.json`'s stop record, and only then calls `request_stop()`. A step that ends at once can otherwise let the interpreter write the final `run.json` before the record, which is then lost.
3. Registered callbacks run. The solve's calls `z3::context::interrupt()`, Z3's soft interrupt (`/usr/include/z3++.h:228-230` in the 4.8.12 headers on this machine). Every Z3 solver runs with `ctrl_c=false`, so a Ctrl-C during a check reaches only `fusion`'s handler (11.C18).
4. Each step tests its `StopToken` at safe points (between checks, pathways, permutation batches, pages) and throws `Stopped`. The interrupted check is not an answer: it is neither cached nor journaled, and a resume asks it again (section 06).
5. The interpreter marks the step `stopped` and the later steps `skipped`, writes `run.json` with status `stopped`, emits `run.finished` and exits with status 3. Every finished check is already on disk (09.C6), so the stop has nothing extra to save.
6. If the main thread has not ended 30 s after the request, for example inside a Z3 tactic that ignores the interrupt, the watcher writes the same final `run.json` and event with `forced: true` and calls the grace hook it was given. The `fusion` executable's hook ends the process with `std::_Exit(3)`. The library itself never ends the process (B1), so a library test passes a hook that only records the call. Section 10 offers Kill only 60 s after the request (10.C19).

A kill (`kill -9`, Task Manager, Java's `destroyForcibly()`), a crash or a power cut skips steps 2 to 6. `run.json` still says `running`, the OS releases the lock, and readers report the run as died. The journal still holds every finished check, and a resume uses it like a stopped run's, so a kill loses only the check in progress (06.C19, amending A1, entry 174).

A batch run never reads stdin, so `< /dev/null`, a closed pipe and a terminal all behave the same. Section 04's query mode reads its one batch line from stdin (04.C8), but it makes no run directory and takes no stop request.

### Saved steps (09.C4)

Every step that produces a value saves it in its step directory. The owning module defines the format; `run` decides where it goes, records it and finds it again.

| Kind | Producer | Format owner | Size, measured on FuSION 1's nearest equivalent |
|---|---|---|---|
| `network` | `mff`, `rgx`, `add` | 02 (S2.25) | merged graphs: median 1,444,862 B, max 13,375,038 B (129 files) |
| `observations` | `cudf` | 04 | `up_reg` plus `down_reg`: 358 B in the reference run |
| `shortlist` | `gsea` | 08 | 3,768 B list and 71,846 B table in the reference run |
| `corridor` | `fb_rch` | 05 | not saved by FuSION 1; about 0.3 MB estimated for the full run's 592 nodes and 2,138 edges |
| `solve`, per case | `pathz3` | 06: `case.json` and `journal.jsonl` | solutions plus relaxations: 95,064 and 99,308 B for the full run's two cases |

The solve's `results.json` is a final output, which `pex` and `plot_pareto` may also read from a parent run (cross-section item 2A.2). A saved step is loaded in one of two ways:

- **From a named source**, decision 0019's "start from a saved step": section 01's `load run=<dir> step=<step>`, or `load <file>`. For a run directory, `Registry` finds the latest artefact of each kind up to that step in its `run.json` and verifies its SHA-256. Either way the file is then copied and checked like any input (09.C3), and the module's loader checks its format version: it reads the versions it knows and fails on a newer one.
- **From a parent run** (09.C5), automatically, by fingerprint.

A step's fingerprint is the SHA-256 of a canonical JSON object (keys sorted, no spaces) holding the command, the build id, every result-changing setting whose resume policy is `must_match`, and the SHA-256 of every input the step reads, by role, the artefacts it consumes included. For the corridor step:

```
{"build":"2.0.0+3f2c9a1e…","command":"fb_rch","inputs":{"network":"9f1c5e…"},"settings":{"corridor.bound":10,"corridor.edges_to_target":"any","corridor.exclude":[],"corridor.source":["hsa1956"],"corridor.target":["hsa3576"]}}
```

- Each owning module gives its fields a canonical form. For example, section 03 sorts the KGML hashes, because the union is order-independent (S2.17).
- The build id is section 11's version, from the repository's one `VERSION` file, and the commit (11.C16). A build with uncommitted changes adds the SHA-256 of its own executable, computed once at start, so two such builds never share a fingerprint. Hashing an executable of a few tens of MB takes about a tenth of a second, and only such builds pay it.
- **Reuse across versions** (cross-section item 3A.3, stated once here; entry 166): a resume's fingerprints use the build id, so a run stopped under one build and resumed under another reuses nothing, and a resumed solve always runs the Z3 that the build pins (06.C18). The shared store keys each entry on its owner's rules version instead (03.C8), so a release that changes no merge rule keeps its entries.

### Restart and resume (09.C5, 09.C6)

A run may name a parent: a stopped, failed or dead run (section 01's `--resume`). The new run gets its own directory, and its `run.json` records the parent's id and path.

- Before the first step, the parent's `run.json` is read and its lock tested. A live parent is refused (`parent.live`).
- Steps run in order. A step is loaded from the parent, instead of run, when all three hold: its fingerprint matches a step the parent completed (`ok` and not `partial`, or `reported`); every output of that step is present with its hash intact; and its `may_change` settings are equal too. Its outputs are then copied into the new step directory, and it is marked `loaded`.
- A step whose fingerprint matches but whose `may_change` settings differ, or that the parent left incomplete (`stopped`, `running`, or `partial`), runs again with the parent's journal records available through `Files::resumed()`. Section 06 rebuilds its cache from them, so finished checks are not asked again (S5.20, 06.C7), and a finished solve can be widened or asked for more solutions without redoing its checks.
- Every other step runs. `exec` lines always run again, since a shell line has no fingerprint (section 01).
- Before the first step, the console and `events.jsonl` say which steps will be loaded and which will run, and why.

Uses:

- **Resume a stopped or killed solve** (0019): `fusion --resume <dir>`, on the parent's own batch copy. Every step before the solve is loaded, and the solve continues from its journal.
- **Restart from the last good step** (S1.28): the corrected batch with `--resume` naming the failed run. The steps before the change are loaded.
- **Sweeps:** each point is a run of its own, so finished points are loaded, the point in progress resumes, and later points run.
- **Stop after a step** (0019): the batch ends there (section 01's `exit`); the step's output is already saved (09.C4), ready for a later run to load.

A journal is append-only, one JSON object per line, in its step directory, each record on disk before the next unit of work starts (09.C6). Section 06 defines the solve's `<case>/journal.jsonl` and `<case>/case.json` (06.C7); its journal, shortened:

```
{"header": 1, "case": "in", "fingerprint": "sha256:9c1e…", "encoder": 1, "seed": 42, "box": [[0, 0], [1, 8]]}
{"check": 1, "n": 0, "e": 4, "phase": "diagonal", "outcome": "unsat", "wall_s": 9.7, "cpu_s": 9.7, "timeout_ms": 14400000, "rlimit": 0}
```

A reader ignores a torn last line.

### Inputs and the checks before a run

A batch run goes through these phases (cross-section item 1.13, entry 184):

1. Read the batch file whole and build the plan: steps, points and settings (section 01). A syntax error ends here, with no run directory.
2. Claim the run directory and `run.lock`; write `run.json`; open the log and `events.jsonl`; start the stop watcher.
3. Copy every input into `inputs/` and hash it in the same read (09.C3). Files already in the run directory, the frontend's batch and override files, are hashed where they lie. An input that cannot be read is noted for the check, not failed at once. `run.json` maps each original path to its copy, and steps open inputs through `Files::input()`, which returns the copy.
4. Check everything, on the copies, collecting every problem before reporting:
   - each step's required inputs exist and are readable regular files; optional inputs follow decision 0021 (not given: the feature is off; empty: an empty list; named but missing or unreadable: an error);
   - the inputs are parsed as their modules require, network inputs fully (02.C15), and each parse is kept for the step that uses it, so every input is read once and the hash in `run.json` is of the bytes parsed;
   - the members of list inputs, such as KGML lists, exist; the owning module parses the list;
   - an input that the batch itself makes, as an earlier line's output or in an earlier `exec` line, is noted instead (01.C12) and handled just before its step;
   - each setting has its type and lies in its range, and section 01's cross-argument rules hold (01.C9);
   - saved-step sources and the parent run exist, their hashes match, and a parent is not live;
   - the external tools the commands declare in section 01's table are present: the check calls section 11's `deps::Registry::check()`, a missing tool is 11's `deps.missing`, and the versions found go into `run.json`'s `tools` (cross-section item 2A.1);
   - outputs clash with nothing (`output.clash`);
   - the run root's volume has at least 1 GB free, or a warning is given; a run root where `run.lock` cannot be locked gives a warning too (09.C9).

   On any error, `run.json` gets status `failed` and the whole list, the console prints each error with its batch line, and `fusion` exits with status 1.
5. Run the steps. Before a step that reads an input the batch made, that input is checked, copied and hashed the same way; one that is an earlier step's output in this run directory is read in place. This keeps FuSION 1 batches whose `exec` lines write `pathz3`'s node files working (decision 0017).

The check-only mode (09.C12) runs phases 1 and 4 on the originals, with no run directory. It copies no input and records nothing, and it hashes only what a check compares: the saved steps and the parent run a batch loads. It prints section 01's diagnostics, with this section's `command`, `setting` and `path` fields added, and section 01's features (cross-section item 1.9):

```json
{"ok": false, "batch": "/home/u/exp/egfr.batch", "sha256": "5f0c…", "points": 1,
 "diagnostics": [
  {"severity": "error", "line": 9, "column": 5, "point": null, "code": "input.missing",
   "command": "mff", "setting": "list", "path": "/home/u/kegg/hsa04010.xml",
   "message": "listed in /home/u/kegg/list.txt, but no such file"},
  {"severity": "warning", "line": 4, "column": 18, "point": null, "code": "batch.undefined_variable",
   "command": null, "setting": null, "path": null,
   "message": "$PLOT_FILE_WO_GENE matches no defined variable"}],
 "features": [
  {"line": 11, "command": "pathz3", "feature": "coexpression", "on": true,
   "because": "a CSV and a threshold are given"}]}
```

### The shared store (09.C13)

```
~/.cache/fusion/
  merge/1/0e91f9a911c19a7f98fd415ffd56bc95ae6fd52128c70bc586557d35978c74d5.xml   section 03's base merge
  merge/1/.tmp-0e91f9a9…-41872-1.xml                                            while being written
```

The example is section 03's entry for the reference run's 42 pathways (03.C3, 03.C4).

- Merges are reused in three tiers (cross-section item 3A.2, entry 166): in memory within one process, as the batch interpreter holds them for a sweep; in this store, across runs; and in each run's own `mff/merged_graph.xml`, which embeds its sources. The store is read as a cache, not as an output: every run keeps its own copies, so deleting the store loses nothing, and S1.26 and S8.21 stand.
- This section gives the store's root, how an entry is written, and the rules every user follows. Each owning section lays out its own subdirectory, names and keys its entries, and checks them before use. Section 03 keeps one file per base merge, `merge/1/<key>.xml`, keyed by its rules version, its build settings and the files' contents, and checks each file's recorded key and sources.
- An entry is written under a temporary name in its final directory and renamed into place (`Cache::publish`). It never changes afterwards. If the name already exists, another run won the race with the same content, and the staged copy is dropped. No locks are needed (S8.28).
- An entry that fails its owner's check is bypassed with a warning and rebuilt.
- Builds with uncommitted changes neither read nor write the store, so a rule changed in a working copy without a new rules version cannot leave a wrong entry behind.
- Nothing is evicted automatically. Deleting the store, or one kind's directory, is always safe.
- A store that cannot be created or written gives a warning, and the run goes on without it.
- The store never changes a result (decision 0023); with `run.cache=false` every step computes its own output. Section 03 uses this setting, not a switch of its own (cross-section item 1.16).
- Modules reach the store only through `core::Cache` in their `Context`, which is null when `run.cache=false`, when the build has uncommitted changes, or when the store is unusable.
- Users: section 03 for merges (S2.22), and section 06 if it adds a cross-run solve cache (roadmap objective 4).

### The settings registry

- Each module declares its settings as a `constexpr` table of `SettingSpec`: name, owning module, type, whether it must be given, may be left out, has a fixed default or a default computed at run time, whether it changes results, whether it is a debug setting, its resume policy, its range or choices, and a help line.
- A setting's key is `<module>.<name>`, such as `solve.seed`, `gsea.seed`, `network.indirect` or `run.cache`; on a command line the bare name is the argument (cross-section item 1.16, entry 190).
- Section 01's command table (S1.12) points at these specs for each command's positional and named arguments, so dispatch, help, named arguments, defaults and the registry share one source; `fusion --commands` exports the table, with the registry's defaults and each command's cross-argument rules, for the frontend (01.C9).
- Run-level settings live in the same registry: this section's (below), section 01's label and root, and other modules' such as `network.indirect`. Section 01's `run` line takes any of them by key (`run network.indirect=keep`), except `run.debug`, and `fusion --set <key>=<value>` overrides the line (cross-section item 2B.2).
- **Precedence** (entry 229): a setting resolves to the first of a step's own argument, positional or named, for that step; then `fusion --set`; then the `run` line; then the registry default. The narrowest statement wins, so two `fb_rch` lines that set `skip_subtypes` differently keep both.
- An optional input that is not given turns its feature off (decision 0021); its setting is recorded with no value. A computed default, such as `run.log` or `run.cache_dir`, is recorded with the value it took and the source `default`. A duration takes a unit in the named form (`timeout=4h`, 01.C1) and is stored as whole milliseconds.
- For each step, the interpreter resolves a `Settings`: each value, the source that applied (`positional` or `named` for the step's own argument, `set` for `fusion --set`, `run_line` for the `run` line, or `default`; E1's three plus the two run-level ones), and, where the step changes a value, the effective value and the reason.
- `run.json` records every setting of every step, whether it changes results or not. Only result-changing settings enter fingerprints and appear in the frontend's forms; debug settings are hidden there (S1.29).

The run-level `run.*` settings. None changes results. This section adds the first four; section 01 adds the last four and owns their spelling (its Settings, entry 244).

| Key | Type | Default | Purpose |
|---|---|---|---|
| `run.debug` | bool | `false` | S1.29's one debug option, set only on the command line, by section 01's `--debug`; the `run` line does not take it (01.C8, entry 229). It turns on debug records in the run log, the `debug/` outputs (S4.19, S5.23), and the slow self-checks that the owning sections put behind it. S1.29 leaves each flag's kind to its stage; poll 031's option counted assertions, FuSION 1's `ASSERT_FLAG`, among the debug flags, and 01.C8 puts them here. |
| `run.log` | file name, with no directory part | computed: `<run name>.log` | S1.25's per-run log name, settable before the first step |
| `run.cache` | bool | `true` | use the shared store (09.C13) |
| `run.cache_dir` | path | computed per platform (09.C13); `FUSION_CACHE_DIR` overrides it | where the store lives |
| `run.label` | text | the batch file's stem | section 01's: the run directory's label (09.C1, 01.C7) |
| `run.root` | path | the batch file's directory | section 01's: where the run directory is made (09.C1, 01.C7) |
| `run.jobs` | `auto` or a positive integer: the core budget for sweep points | `auto` | section 01's: how many sweep points run at once (01.C4) |
| `run.points` | the selected sweep points | all | section 01's: which sweep points run (`--point`, 01.C14) |

### Errors, failures and reported results (09.C14)

A **reported result** is a `core::Report` that a step returns instead of a value. There are three (cross-section items 1.8 and 3A.14, entry 167):

- `corridor.empty`: no path within the bound (S4.18);
- `gsea.empty_selection`: no pathway passed the selector (S7.03);
- `observations.none`: no gene past the thresholds; `cudf` writes its two empty lists and succeeds (04.C5).

After a reported result, every later line of that run or point is skipped, `exec` lines included, each with a `step.skipped` event, and the run ends `ok` with exit 0 (01.C15, widening S4.18's wording, entry 167). Outcomes inside a solve are case statuses in its results, not reports: a case with no point (S6.21), an infeasible case, no DE gene in the corridor (06.C15) and an exhausted whole-solve budget (06.C3). The batch, the Pareto plot and the pages still run after them.

| Kind | Raised by | Effect | Exit |
|---|---|---|---|
| Failure | `throw core::Failure(code, message, where)` | caught once per command by the interpreter; a stderr line with the batch line; an `error` event; `run.json`'s `error`; the batch stops (S1.28), or in a sweep the point (01.C5) | 1 |
| Usage error | `throw core::UsageError(...)`, before any run directory exists | stderr | 2 |
| Install error | `throw core::InstallError(...)` at startup, from section 11's checks | stderr and an `error` event with an `install.*` code (cross-section item 2B.16) | 1 |
| Reported result | returning `core::Outcome<T>` with a `Report` | step `reported`, with `report` in `step.finished`; every later line of the point `skipped` | 0 |
| Stop request | `StopToken` throws `core::Stopped` | step `stopped`; run `stopped` | 3 |
| Warning | `Log::warning(code, text)` | stderr, the run log and a `warning` event; never stops anything | none |

Conventions for every module:

- Codes are stable, dotted and lower case, `<area>.<problem>`. A condition found by the generic check or the binder takes this section's or section 01's generic code (`input.*`, `io.write`, `artefact.*`, `output.clash`, `batch.*`) or section 11's `deps.missing`; modules keep codes only for their own faults, such as `values.encoding` (cross-section item 1.17). The frontend can key help texts on them.
- A message names the file, the line in it and the setting where known, and always the batch line: `error: egfr_cxcl8_jun_b10.batch:9: mff: cannot read /home/u/kegg/hsa04010.xml: No such file or directory [input.unreadable]`.
- Library exceptions become failures where they are caught: `z3::exception` as `solve.z3`, `std::filesystem::filesystem_error` as `io.*`, `std::bad_alloc` as `internal.out_of_memory`, anything else as `internal`, always with the step and batch line.
- A module never prints, and never calls `exit` or `abort`. The only forced end of a process is the `fusion` executable's grace hook after a stop (Stop, step 6).

Exit statuses (01.C14, entry 189):

| Status | Meaning |
|---|---|
| 0 | the batch ran to its end, reported results included |
| 1 | a failure: a batch syntax error, a failed check, an unusable run directory, an install error, a failing step or an internal error; in a sweep, a failed point |
| 2 | a usage error on the command line, before any run directory exists (S1.01, S1.02) |
| 3 | stopped on request (09.C7) |
| 130 | a second Ctrl-C ended the process at once (09.C8) |

Errors this section raises:

| Code | When | Kind |
|---|---|---|
| `run.dir_create` | the run root cannot be created or written | failure; nothing is written |
| `run.dir_used` | an adopted directory already holds `run.lock` | failure; nothing is written |
| `input.missing`, `input.unreadable`, `input.not_file` | decision 0021's checks, before the run or, for an input the batch makes, before its step | failure |
| `input.list_member` | a list input names a missing file | failure at the check |
| `output.clash` | two outputs, or an output and an input, are one file | failure at the check |
| `artefact.changed` | a saved step's SHA-256 no longer matches its `run.json` | failure |
| `artefact.newer_format` | the artefact was written by a newer FuSION | failure |
| `parent.live` | the parent run still holds its lock | failure at the check |
| `io.write` | a write, rename or copy fails (full disk, permissions) | failure |
| `run.low_disk` | less than 1 GB free on the run root's volume | warning |
| `run.long_path` | on Windows, the run directory's path exceeds 160 characters | warning |
| `run.lock_unsupported` | the run root's filesystem cannot lock `run.lock`, so liveness cannot be shown (09.C9) | warning at the start |
| `store.unavailable` | the store cannot be created or written | warning |
| `stop.forced` | the main thread did not end within 30 s of a stop | warning, recorded in `run.json` |

### Determinism

Rules every module follows; section 12 tests them:

- **Random numbers** come from `core`'s two generators, each seeded from a recorded setting: section 08's exact port of gseapy's stream for GSEA (08.C1), and `core::Rng` for everything else (09.C15). Z3 has its own seed (S5.22).
- **Order:** every list in an output follows a documented total order on stable keys (S4.14), compared with `core::stable_compare` (unsigned bytes). Unordered containers serve lookups only, and are never iterated to produce output or to make a choice: their order differs between libstdc++, libc++ and MSVC's library. `core::stable_hash` is a fixed function, never `std::hash`. Sorts use a total order or `std::stable_sort`. Parallel work is merged in index order, never in completion order, and the thread count never changes a result.
- **Text:** files are written in binary mode, UTF-8, with `\n` line ends, so Windows writes the same bytes as Linux. Readers accept `\r\n`. No locale is set or used. Numbers in tables and on the console are written by `core::format_number`: the shortest digits that read back exactly, in Python's `repr` layout (`0.0`, `1e-05`, `0.0001`, `13632.95`), which keeps 2.0's tables reading like FuSION 1's; JSON numbers are nlohmann/json's (cross-section item 1.20, entry 180). Strings are compared byte by byte.
- **Time** enters only metadata (`run.json`, events, the log), never a result.
- **Floating point:** builds turn off contraction into fused multiply-add, and canonical outputs are byte-identical on Linux, macOS and Windows (09.C16). Result paths therefore avoid the platform's `exp`, `log` and `pow`, or use one correctly rounded implementation.
- **Versions** recorded in `run.json`: section 11's `versions` block (FuSION's version, commit and toolchain; Z3, pugixml, nlohmann/json and fast_float), the tools a run used, the OS and the CPU (S7.02).

### Two runs at once

- Each run has its own directory by construction (09.C1, 09.C9), and the launch directory is never written (S1.26).
- The shared store is safe for concurrent writers (09.C13). Of the nine candidate runs started together on 2026-11-09 at 19:50:56, seven (r0 to r6) merged one 87-pathway list and two (r7, r8) one 88-pathway list with the same curated KGML. Keyed by content, that is two entries; each is built by whichever run gets there first, and the others drop identical copies.
- Section 01's sweep points run as separate processes, each in its own run directory, so they are runs at once in this sense too.
- The one output outside a run directory is section 07's site export (S6.19), which several runs may update. It takes the site's lock through `Lock::acquire` (cross-section item 2B.17) and writes through `OutputFile`.
- Resources: the 14 FuSION 1 runs with a `run_time.txt` peaked at 318,920 KiB (fdr025) to 1,982,592 KiB (r5) of resident memory, and nine ran together on this 36-thread machine. Each Z3 check uses one thread. How many runs the frontend lets run at once is section 10's (10.C20).

### Disk use

| Files | FuSION 1, measured | 2.0 |
|---|---|---|
| SMT dumps | 6.97 MB (reference run) to 123.1 MB (full run) per run; 199 directories with 5.77 GB under `runs/` | only under the debug option (S5.23) |
| Explanation pages | 0.71 to 0.75 MB each, each with its own vis.js; 9.6 MB for the full run's 13 | section 07: 40 to 64 KB per page, one library copy (07.C2, S6.18) |
| Inputs | 49 `inputs/` directories: median 0.45 MB, max 15.2 MB; KGMLs referenced in place | the same plus the KGMLs: 6.85 MB for the 167 candidates (09.C3) |
| Merged network | median 1.44 MB, max 13.4 MB | similar (section 02) |
| Solve results | 95 to 99 KB per case in the full run | `results.json`: 137,225 B for the full JUN run (07.C1), plus journals under 60 KB per case (06) |
| Console log | median 22.9 KB, max 52.0 KB (105 logs) | the run log, about the same |
| `run.json`, `events.jsonl` | none | about 60 KB and 40 KB |

A FuSION 1 run measured here took 9.2 MB (the reference run) to 137.0 MB (the full run), mostly dumps. A 2.0 run without the debug option should take about 10 to 40 MB, mostly input copies and the network; with the debug option, add the dumps.

### Interface (C++20 outline)

```cpp
// ---------------------------------------------------------------- core
namespace fusion::core {

using Json = nlohmann::ordered_json;

struct Where {                                   // where a failure was found
    std::string file;                            // input file, if any
    std::uint32_t line = 0;                      // line in that file; 0 = none
    std::string setting;                         // setting at fault, if any
};

class Failure : public std::runtime_error {      // stops the batch (S1.28); exit 1
public:
    Failure(std::string code, std::string message, Where where = {});
    const std::string& code() const noexcept;    // "input.missing", "corridor.no_source", ...
    const Where& where() const noexcept;
};
class UsageError : public Failure { using Failure::Failure; };     // bad command line; exit 2
class InstallError : public Failure { using Failure::Failure; };   // section 11's startup checks; exit 1

class Stopped : public std::exception {};        // thrown at a safe point after a stop request

struct Report {                                  // a reported result (S4.18, S7.03, 04.C5)
    std::string code;                            // "corridor.empty", "gsea.empty_selection", "observations.none"
    std::string message;                         // "no path from EGFR to CXCL8 within bound 3"
};

template <class T>
struct Outcome {
    T value;
    std::optional<Report> report;                // set: every later line of the point is skipped
    bool partial = false;                        // a resumable partial result (06.C3)
};

// Core keeps its own stop token, with std's interface, so no target needs an experimental flag.
class StopToken {
public:
    bool stop_requested() const noexcept;
    void throw_if_stopped() const;               // throws Stopped
};
class StopSource {
public:
    StopToken token() const noexcept;
    bool request_stop() noexcept;                // the first call runs the callbacks
};
class StopCallback {                             // the solve registers z3::context::interrupt()
public:
    StopCallback(const StopToken&, std::function<void()> on_stop);
    ~StopCallback();
};

enum class Level : std::uint8_t { debug, info, warning, error };

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(Level, std::string_view code, std::string_view text) = 0;
};

class Log {                                      // thread-safe; each call is one whole line
public:
    explicit Log(bool debug_on);
    void add_sink(std::shared_ptr<LogSink>);
    bool debug_on() const noexcept;
    void debug(std::string_view text);           // run log only, and only under the debug option
    void info(std::string_view text);            // stdout and the run log
    void warning(std::string_view code, std::string_view text);  // stderr, run log, "warning" event
    void error(std::string_view code, std::string_view text);    // stderr, run log, "error" event
};

class Events {
public:
    virtual ~Events() = default;
    virtual void emit(std::string_view type, Json fields) = 0;   // adds v, seq and t; inside a step
                                                                 // also step, line and point
    virtual void progress(std::uint64_t done, std::uint64_t total,
                          std::string_view unit) = 0;            // "step.progress", at most 1 per second
};

enum class Type : std::uint8_t {
    boolean, integer, real, duration, text, choice, input_file, input_list, output_name };
enum class Need : std::uint8_t {
    required,                                    // must be given
    optional,                                    // may be left out: its feature is then off (0021)
    defaulted,                                   // default_text when left out
    computed };                                  // default worked out at run time (run.log, run.cache_dir)
enum class Source : std::uint8_t {               // precedence: step argument, --set, run line, default
    default_value, positional, named,            // E1's three: "default", "positional", "named"
    set_option, run_line };                      // "set" (fusion --set), "run_line" (entry 229)
enum class OnResume : std::uint8_t { must_match, may_change };

struct SettingSpec {                             // constexpr tables, one per module
    std::string_view name;                       // "bound"; the bare name on a command line
    std::string_view module;                     // "corridor"; the key is "corridor.bound"
    Type type;
    Need need;
    std::string_view default_text = {};          // canonical text, for Need::defaulted
    bool affects_results = true;                 // only these enter fingerprints
    bool debug = false;                          // hidden in the frontend (S1.29)
    OnResume on_resume = OnResume::must_match;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    std::span<const std::string_view> choices = {};
    std::string_view help = {};
};
std::string key(const SettingSpec&);             // "<module>.<name>"

using Value = std::variant<bool, std::int64_t, double, std::string, std::vector<std::string>>;
                                                 // a duration is an int64 of milliseconds

struct Setting {
    const SettingSpec* spec;
    std::optional<Value> value;                  // empty: an optional setting left out
    Source source;
    std::optional<Value> effective;              // when the step changed it (S4.16)
    std::string reason;
};

class Settings {                                 // one step's settings, sorted by key
public:
    explicit Settings(std::vector<Setting>);
    const Setting& at(std::string_view name) const;                 // bare name within the step
    template <class T> const T& get(std::string_view name) const;   // throws if left out
    template <class T> std::optional<T> find(std::string_view name) const;
    void set_effective(std::string_view name, Value, std::string reason);
    std::span<const Setting> all() const noexcept;
};
Json to_json(const Settings&);

// Ids and stable orderings (cross-section item 2A.5, section 02's outline)
using GeneId = /* strong 32-bit handle */;       // an interned entity id or node key
using Sym    = /* strong 32-bit handle */;       // other interned strings: types, subtypes, pathways
class Interner {                                 // append-only; handles follow first appearance
public:
    GeneId id(std::string_view);
    Sym sym(std::string_view);
    std::optional<GeneId> find_id(std::string_view) const noexcept;  // never inserts (S2.15)
    std::string_view str(GeneId) const noexcept;
    std::string_view str(Sym) const noexcept;
};
std::strong_ordering stable_compare(std::string_view, std::string_view) noexcept;   // unsigned bytes
std::uint64_t stable_hash(std::string_view) noexcept;                               // never std::hash

struct Sha256 {                                  // a 32-byte digest (cross-section item 1.1)
    std::array<std::byte, 32> bytes;
    std::string hex() const;
    static Sha256 of(std::string_view data);
    friend auto operator<=>(const Sha256&, const Sha256&) = default;
};
class Sha256Builder {                            // streaming, for copies and outputs
public:
    void update(std::span<const std::byte>);
    Sha256 finish();
};

class Rng {                                      // 09.C15: everything outside GSEA
public:
    static Rng stream(std::uint64_t seed, std::uint64_t stream_id);  // SplitMix64 seeds the engine
    std::uint64_t next();                        // std::mt19937_64, bit-exact by the standard
    std::uint64_t below(std::uint64_t n);        // uniform in [0, n): Lemire's method
    double unit();                               // uniform in [0, 1): 53 bits
    template <std::random_access_iterator It>
    void shuffle(It first, It last);             // Fisher-Yates on below()
private:
    std::mt19937_64 engine_;
};
// GSEA's generator, Xoshiro256pp with shuffle_like_rand09, is section 08's (08.C1).

std::string format_number(double);              // shortest exact digits, Python repr layout (1.20)
std::string utc_timestamp(std::chrono::system_clock::time_point);    // "2026-11-12T13:14:00.412Z"
std::string to_utf8(const std::filesystem::path&);                   // generic form, '/' separators
std::filesystem::path path_from_utf8(std::string_view);
double cpu_seconds();                            // process CPU time
std::uint64_t peak_rss_bytes();
// path_from_utf8, cpu_seconds, peak_rss_bytes and build_id are implemented by section 11's
// platform layer and build information, which also live in core (11.C19).

std::string_view build_id();                     // VERSION and commit; a build with uncommitted
                                                 // changes adds its executable's SHA-256 (11.C16)
bool clean_build() noexcept;                     // false: uncommitted changes
std::string fingerprint(const Json& fields);     // SHA-256 hex of canonical JSON, build id added

enum class Severity : std::uint8_t { error, warning, note };
struct Diagnostic {                              // the one type behind --check --json (1.9)
    Severity severity;
    std::uint32_t line = 0, column = 0;
    std::optional<std::uint32_t> point;
    std::string code;                            // dotted, e.g. "input.missing"
    std::string message;
    std::optional<std::string> command, setting, path;
};
struct Feature { std::uint32_t line; std::string command, feature; bool on; std::string because; };
struct CheckReport {
    std::filesystem::path batch;
    Sha256 batch_sha256;
    std::uint32_t points = 1;
    std::vector<Diagnostic> diagnostics;         // sorted by point, line, column
    std::vector<Feature> features;               // decision 0021: what is on and why
    bool ok() const noexcept;                    // no error
    Json to_json() const;
};

class OutputFile {                               // writes "<name>.tmp"; commit() renames it
public:
    std::ostream& out();                         // binary mode
    void also_at(const std::filesystem::path& absolute);   // copy committed there too (2B.3)
    void commit();                               // flush, rename (retried on Windows), SHA-256,
                                                 // record in run.json
    ~OutputFile();                               // not committed: the temp file is removed
};

class Journal {
public:
    virtual ~Journal() = default;
    virtual void append(const Json& record) = 0; // on disk before it returns (09.C6)
};

class Cache {                                    // the shared store as a module sees it (09.C13)
public:
    virtual ~Cache() = default;
    virtual std::filesystem::path dir(std::string_view kind) = 0;          // <root>/<kind>/
    virtual bool publish(const std::filesystem::path& staged,
                         const std::filesystem::path& final_path) = 0;    // false: another run won
};

class Files {                                    // implemented by run
public:
    virtual ~Files() = default;
    virtual std::filesystem::path input(const std::filesystem::path& as_named) = 0;  // the checked copy
    virtual OutputFile create(std::string_view name) = 0;          // in the step directory
    virtual OutputFile create_debug(std::string_view name) = 0;    // in debug/<step>/
    virtual Journal& journal(std::string_view name) = 0;
    virtual std::span<const Json> resumed(std::string_view name) const = 0;  // the parent's records
};

struct Context {                                 // what every step receives
    Log& log;
    Events& events;
    StopToken stop;
    const Settings& settings;
    Files& files;
    Cache* cache;                                // null: cache=false, uncommitted build, or unusable
    std::string_view step;                       // "fb_rch", "fb_rch-2"
};

} // namespace fusion::core

// ----------------------------------------------------------------- run
namespace fusion::run {

enum class RunStatus : std::uint8_t { running, ok, failed, stopped };   // readers infer "died"
enum class StepStatus : std::uint8_t { pending, running, ok, reported, skipped, failed, stopped, loaded };

struct InputSpec {                               // from section 01's plan
    std::string argument;                        // "values"
    std::filesystem::path path;                  // as named, after $ expansion
    bool required;                               // decision 0021
    bool is_list = false;                        // its lines name more inputs (KGML lists)
    std::optional<std::uint32_t> made_by_line;   // the batch makes it (01.C12): handled before its step
};

struct StepSpec {
    std::string id;                              // "fb_rch", "fb_rch-2"
    std::string command;                         // canonical name from section 01's table
    std::uint32_t line;                          // batch line
    std::string normalised;                      // section 01's normalised line
    std::string point;                           // the run's sweep point label; empty outside sweeps
    const core::Settings* settings;
    std::vector<InputSpec> inputs;
};

struct Options {                                 // from the command line and the batch (section 01)
    std::filesystem::path root;
    std::string label;
    std::optional<std::filesystem::path> adopt;  // a directory the frontend made (A1)
    std::optional<std::filesystem::path> parent; // restart or resume from this run (09.C5)
    std::vector<std::string> command_line;
    std::map<std::string, std::string> labels;   // section 01's --tag, such as session=<id>
    std::vector<std::pair<std::string, std::string>> variables;   // section 01's, for run.json
    bool debug = false;                          // --debug only; the run line does not take it (01.C8)
    std::string log_name;                        // empty: "<run name>.log"
    bool cache = true;
    std::optional<std::filesystem::path> cache_dir;
    std::function<void(int exit_status)> on_grace_expired;  // set by the fusion executable (B1)
};

class Run {
public:
    static Run start(const Options&);            // claim directory and lock; open run.json,
                                                 // events and log; start the stop watcher
    static core::CheckReport check_only(std::span<const StepSpec>);   // 09.C12, on the originals

    const std::filesystem::path& dir() const noexcept;
    const std::string& id() const noexcept;
    core::Log& log() noexcept;
    core::Events& events() noexcept;
    core::StopToken stop() const noexcept;

    void copy_inputs(std::span<const StepSpec>);             // 09.C3: copy and hash first (1.13)
    core::CheckReport check(std::span<const StepSpec>);      // decision 0021, on the copies

    class Step;                                  // a running step: its directory and Context
    Step begin(const StepSpec&);                 // checks and copies inputs the batch made
    bool load_from_parent(Step&);                // 09.C5: true if the parent's output was used
    void end(Step&, StepStatus, std::optional<core::Report> = {}, bool partial = false);
    void fail(Step&, const core::Failure&);
    core::OutputFile create_run_file(std::string_view name);  // top level, at the run's end, whatever its
                                                 // status: bindings.tsv (02), index.html (07); once each

    int finish(RunStatus);                       // final run.json and event; the exit status
};

struct Artefact {                                // one saved output, as run.json lists it
    std::string kind;                            // "network", "corridor", "solve", ...
    std::string part;                            // "in", "out", "free"; empty for one-part steps
    std::string format;
    std::uint32_t format_version;
    std::string path;                            // relative to its run directory
    std::uint64_t size;
    core::Sha256 sha256;
    std::string step;
    std::string fingerprint;
};

// A step's fingerprint: core::fingerprint over the command, its must_match result-changing
// settings by key, and the SHA-256 of each input by role (09.C5).
std::string step_fingerprint(std::string_view command, const core::Settings&,
                             const std::map<std::string, core::Sha256>& inputs);

class Registry {                                 // the saved steps of one run directory
public:
    static Registry open(const std::filesystem::path& run_dir);
    RunStatus status() const;
    bool live() const;                           // its run.lock is held
    std::vector<Artefact> find(std::string_view kind, std::string_view up_to_step = {}) const;
    std::vector<Artefact> match(std::string_view step, std::string_view fingerprint) const;
    std::filesystem::path verified(const Artefact&) const;   // throws Failure("artefact.changed")
};

class Store final : public core::Cache {         // 09.C13
public:
    static std::optional<Store> open(const Options&);        // nullopt: off, unclean build or unusable
    std::filesystem::path dir(std::string_view kind) override;
    bool publish(const std::filesystem::path& staged,
                 const std::filesystem::path& final_path) override;
};

class Lock {                                     // 09.C9; OS lock on one byte at offset 2^31
public:
    static Lock claim(const std::filesystem::path& run_dir);  // exclusive create of run.lock, then lock
    static bool held(const std::filesystem::path& run_dir);
    static Lock acquire(const std::filesystem::path& file,
                        std::chrono::milliseconds wait);      // any file, e.g. section 07's site (2B.17)
    static bool supported(const std::filesystem::path& dir);  // false on a lock-less filesystem
};

} // namespace fusion::run
```

### Algorithms and cost

| Operation | Time | Memory | Measured or bounded |
|---|---|---|---|
| Claim a run directory | O(k) `mkdir` calls; k = 1 plus same-second collisions | O(1) | none needed |
| Copy and hash inputs | O(B) for B bytes, one read | one buffer | 15.2 MB, the largest FuSION 1 `inputs/`: 0.10 s with `cp` and `sha256sum` as two passes; the 167 KGMLs (6,853,555 B): 0.03 s to hash |
| Hash outputs as they are committed | O(B), in the same pass as the write | one buffer | a network up to 13.4 MB; under the debug option, dumps such as the full run's 123.1 MB, about half a second at the rate above |
| Check inputs | O(files) `stat` calls, plus the modules' parses | O(files) | the reference run's 174 input files: 17 ms with one `stat` command; every parse is kept for its step, so no input is parsed twice |
| Fingerprint a step | O(s + i) for s settings and i inputs | O(s + i) | microseconds |
| Match a parent's steps | O(1) per step through a map | O(parent's steps) | none needed |
| Write `run.json` | O(M) per write, at most one write a second | O(M) | M is about 60 KB with 175 inputs |
| Emit an event | O(size), one flush | O(1) | 150 to 200 events per run |
| Watch for a stop | one `stat` per 0.5 s | O(1) | 7,200 per hour |
| Append a journal record | O(size) plus one `fsync` | O(1) | at most 28 checks per solve, so the fsyncs add at most a few tenths of a second to a solve |
| Look up the store | O(1) path, plus the owner's check | O(1) | section 03 estimates tens of milliseconds for a hit (03.C3) |
| `Rng::below` | O(1) expected | O(1) | 1000 shuffles of 3000 items, 3 million draws: 53 ms |

### Platform notes

- **Claiming a directory:** `create_directory` is atomic everywhere. macOS volumes are usually case-insensitive, so two labels that differ only in case collide and the second gets a suffix.
- **Exclusive create and lock:** POSIX `open(O_CREAT | O_EXCL)` and `fcntl(F_SETLK)`; Windows `CreateFileW(CREATE_NEW)` and `LockFileEx`. `flock` is not used: Java's `FileChannel.tryLock()` does not see it (probe above). An `fcntl` lock is dropped when the process closes any descriptor of the file, so `run.lock` is opened once. `LockFileEx` locks are mandatory, so a locked byte cannot be read by another process; the lock therefore covers one byte at offset 2^31, past the readable line, which both APIs allow beyond the end of a file. A reader's `tryLock()` covers the whole file and so meets it. `Lock::acquire` uses the same scheme for any file.
- **Atomic replace:** POSIX `rename`. On Windows, `std::filesystem::rename` (`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`) fails while another process holds the target open without delete sharing. `OutputFile` retries ten times, 50 ms apart; readers must open files with delete sharing, which Java NIO does by default and `java.io.FileInputStream` does not. Phase 3 confirms both on Windows.
- **Durability:** `fsync` on Linux and macOS (macOS's `fsync` leaves the drive's cache, which is accepted); `FlushFileBuffers` or `_commit` on Windows.
- **Signals:** POSIX `sigaction` for SIGINT and SIGTERM, and SIGHUP ignored; Windows `SetConsoleCtrlHandler` for Ctrl-C, Ctrl-Break and closing the console, after which Windows kills the process within about 5 s. Only in the `fusion` executable (B1; 09.C8).
- **Store location:** `$XDG_CACHE_HOME/fusion` or `~/.cache/fusion` on Linux; `~/Library/Caches/FuSION` on macOS; `%LOCALAPPDATA%\FuSION\Cache` on Windows.
- **CPU time and peak memory:** `std::clock()` measures wall time on Windows, so `cpu_seconds()` uses `getrusage` or `GetProcessTimes`. `ru_maxrss` is in KiB on Linux and in bytes on macOS; Windows reports `PeakWorkingSetSize`. Section 11's platform layer implements these.
- **Paths:** on Windows, a `std::filesystem::path` built from a `std::string` uses the ANSI code page, so core converts from UTF-8 explicitly; `run.json` stores the generic form, with `/`. Paths over 260 characters need long-path support on Windows (section 11's `long_path()`).
- **Library floor:** GCC 13 and Clang 17 (11.C22) give `<format>`, which messages may use. `format_number` uses `std::to_chars`, whose floating-point overloads need macOS 13.3, the oldest macOS supported (11.C1). Core keeps its own stop token, with std's interface, so no target needs an experimental library flag.
- **Line ends:** files are opened in binary mode everywhere, so Windows writes `\n`, not `\r\n`.

## Choices

### 09.C1 · Run directory names

- **Proposed:** `<label>_<YYYYMMDD-HHMMSS>` in local time, claimed with one atomic `mkdir`, with `-2`, `-3` on a collision. The label comes from the batch (01.C7) or the frontend (10.C18), reduced by cross-section item 1.26's rule (accepted in entry 158): each run of code points outside `[A-Za-z0-9._-]` becomes one `-`, ends trimmed, cut to 44 characters. A separate run id, `r-<UTC time>-<8 hex digits>`, goes in `run.json`, and the frontend keys on it.
- **Alternatives:**
  - the timestamp first (`20261112-184400_<label>`), which sorts runs by time instead of grouping them by experiment;
  - nested `<label>/<timestamp>/`, one directory per experiment;
  - the label alone, as FuSION 1 batches name their directories, with `-2`, `-3` only when it is taken.
- **Why:**
  - FuSION 1 run directories are named by experiment (`egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025`, `r5_mcl1_stat3`), and the GUI's rerun under the same name deleted the earlier results (S8.21). Label first keeps an experiment's runs together in a listing, and the timestamp makes every name new without asking anyone.
  - `mkdir` either creates the directory or reports that it exists, atomically, on all three platforms, so two runs started in the same second never share one (S8.28). The candidate set started nine runs in the same second (2026-11-09 19:50:56).
  - The label alone gives `x`, `x-2`, `x-3`, which says nothing about when each ran. Nesting adds a level to every path, against Windows' 260-character limit.
  - Local time reads naturally; `run.json` and events keep UTC. Where clocks go back an hour, a name can repeat once, and the suffix handles it.
  - Windows forbids `<>:"/\|?*`, trailing dots and names such as `CON`, so names are reduced the same way on every platform, by one rule that the C++ and Java copies test against one golden list.
- **Decision:** as proposed (prompt log entry 184)

### 09.C2 · One directory per step

- **Proposed:** each step that saves something gets a directory named by its command, with `-2`, `-3` for repeats. The placement follows cross-section item 1.4 (accepted in entry 177): `pathz3/results.json` with `in.dat` and `out.dat` beside it and section 06's case directories (`pathz3/in/`, `pathz3/out/`) for journals and per-case files; `plot_pareto/pareto.png` and `pareto.gp`; `pex/explain/…` with the library in `pex/lib/`; FuSION 1 text in `pathz3/legacy/`; debug files only in `debug/<step>/`. The top level holds `run.json`, `events.jsonl`, the run log, `run.lock`, `stop` once requested, `bindings.tsv` (cross-section item 1.22), the run page `index.html`, written once when the run ends, the frontend's batch file and `overrides/` (A1, S8.15), `inputs/` and `debug/`. A sweep's points are run directories of their own in `points/<NN>-<label>/` (section 01).
- **Alternatives:**
  - one results file per case, `pathz3/in/results.json` and `out/results.json`, as S5.30 can be read literally;
  - section 07's draft `results/` subtree, with a run page rewritten after each `pex` and plot;
  - one flat directory, with each file prefixed by its step and case, as in FuSION 1's `run/`;
  - step directories numbered in batch order (`04-fb_rch/`).
- **Why:**
  - Fixed names collide in a flat directory. FuSION 1 batches that call `fb_rch` twice lose the first call's traces (S4.19), and `PO.dat` and `limits_timefile.txt` are overwritten unless moved (S5.30). All 116 `*.batch` files in FuSION 1's tree carry `exec mv` lines, 855 in all, whose only job is to move such files. Per-step directories make a collision impossible while each module keeps its file names (S1.26).
  - The full run's flat `run/` holds 48 files. Per-step directories keep a step's outputs together, so a step can be loaded or redone as a unit (09.C5).
  - One `results.json` per solve is 137,225 bytes for the full JUN run; one file per case would be 33 % larger, and the FSC comparison would need a file of its own (07.C1). S5.30's "named by solve case" is met by the case labels inside it and the per-case names (cross-section item 4A.12).
  - A run page rewritten after each `pex` and plot would break the rule that no committed output is rewritten; written once at the end, it lists every result.
  - Numbers shift when a line is added to a batch, which would break references to the steps of earlier runs; command names do not shift.
- **Decision:** as proposed (prompt log entry 177)

### 09.C3 · Inputs are copied into the run directory, and read once

- **Proposed:** every input file outside the run directory is copied into `inputs/` and hashed in the same read: the batch file, every named file and every member of a list such as the KGML list. Files the frontend put in the run directory (A1, S8.15) are hashed where they lie. The check then parses the copies, and each step uses the check's parse, so every input is read once and the recorded hash is of the bytes parsed (cross-section item 1.13). An input the batch makes for itself is copied, hashed and checked just before the step that reads it (01.C12, which amends decision 0021, S1.15 and S2.16 for such inputs; entry 188). Steps read only the copies. `run.json` records each input's original path, size, SHA-256 and copy. The check-only mode reads the originals.
- **Alternatives:**
  - record the path, size and SHA-256 only (E1's minimum), and re-check size and time whenever a step opens the file, failing if it changed;
  - keep one copy of each distinct file in the shared store and reference it by hash, or hard-link it into each run.
- **Why:**
  - FuSION 1 users already copy inputs into every run: the candidate set's generator says "Each run directory is self-contained (inputs/ copied …)" (`runs/egfr_b10_gsea_fdr044_candidates/make_runs.py:4`). Under `runs/`, 49 `inputs/` directories hold 408 files, 281.3 MB, of only 35 distinct contents (17.8 MB).
  - A run is then reproducible from its own directory (A1's reason for the process model) after an input is moved or edited, and immune to an edit during a run of up to 52 h.
  - Checking the copy, not the original, means a file edited between the check and the copy cannot leave parsed values and a recorded hash that disagree.
  - The cost per run: FuSION 1's `inputs/` directories are 0.45 MB median and 15.2 MB at most, plus 6.85 MB for the 167 candidate KGMLs, which FuSION 1 runs referenced in place. Copying and hashing 15.2 MB took 0.10 s.
  - Against it: the duplication across runs (16-fold in FuSION 1's `runs/`), and a run directory then holds KEGG files, so sharing a whole run directory shares KEGG data, the licence concern behind S8.31.
  - Recording only costs no disk, but an old run cannot be redone once its inputs move, and a change during a run must be caught at every open. A shared copy removes the duplication, but a run then depends on a directory outside it, and a hard-linked file edited in place would change every run that links it.
- **Decision:** as proposed (prompt log entry 184)

### 09.C4 · Every step saves its output

- **Proposed:** every step that produces a value (network, observations, shortlist, corridor, solve state) writes it to its step directory as it ends, and `run.json` records it with its SHA-256. There is no switch.
- **Alternatives:**
  - save only what the batch asks to save, with a `save` argument or command, and keep the rest in memory;
  - save only the corridor and the solve state, the two that cost most to recompute.
- **Why:**
  - The sizes are small next to the run times. Merged networks: median 1.44 MB, at most 13.4 MB (129 files). A corridor: about 0.3 MB. Solve results: 95 to 99 KB per case. The GSEA shortlist and table: 75 KB. A run's saved values come to about 2 to 15 MB, written in milliseconds, against runs of 11 min to 52 h.
  - Saving everything is what lets a failed run restart from its last good step (S1.28) and a stopped run resume (decision 0019), without the user having planned for it.
  - FuSION 1 users already kept the merged graph in every run: 129 copies under `runs/`, of 19 distinct contents.
  - Saving on request saves a few MB per run, but a run that fails or is stopped can then restart only from what someone thought to save.
- **Decision:** as proposed (prompt log entry 184)

### 09.C5 · Restart and resume reuse a parent run's steps by fingerprint

- **Proposed:** a run may name a parent run (section 01's `--resume`), and gets its own new directory. Each step whose fingerprint (command, build id, result-changing settings, input hashes) matches a step the parent completed, with its outputs intact and its may-change settings equal, is loaded from the parent's saved output instead of being run. A matching step that the parent left incomplete, or whose may-change settings differ, runs again with the parent's journal (09.C6). Everything else runs. "Resume" is the same batch with the stopped run as parent; "restart from the last good step" is the corrected batch with the failed run as parent. The build id is section 11's version and commit, plus the executable's SHA-256 for a build with uncommitted changes (11.C16). The rule for reuse across versions is stated once, in "Saved steps": resume uses the build id, the store its owners' rules versions (cross-section item 3A.3, entry 166).
- **Alternatives:**
  - explicit positions: the user says which step to start from, every earlier step is loaded without checking its settings or inputs, and every later step runs;
  - result versions per module for resume too, raised by hand when a module's results change, so a resume across an upgrade keeps finished steps.
- **Why:**
  - A fingerprint matches only when the work would be identical, so a corrected later line, a changed setting or an edited input (by its SHA-256) never reuses a stale result, and correctness does not rest on the user's bookkeeping. Explicit positions trust the user to know which earlier steps are still valid.
  - It is cheap: a fingerprint is one SHA-256 over a few hundred bytes, and loading is one verified copy, at most 13.4 MB for the largest merged network measured.
  - What it saves: fdr050 took 50.7 h and r6 52.1 h. FuSION 1's re-solves of single points (candidate set `README.md`, "Re-solves") rebuilt the graph and corridor in new directories, checked by hand that they were byte-identical, and pinned the search window by hand.
  - Rerunning a matching but incomplete or widened solve from its journal is section 06's need: a wider box, more solutions or a longer timeout must not load the old result as it is.
  - With the build id, a resume never mixes builds or Z3 versions (06.C18); the price is that a run stopped before an upgrade is redone. Result versions would keep the work, but trust every change to raise them.
  - Continuing inside the stopped run's own directory is not an option: S8.21 rules out reusing a directory, and two processes' outputs would mix in one `run.json`.
- **Decision:** as proposed (prompt log entry 166)

### 09.C6 · Resumable steps journal every finished unit of work

- **Proposed:** a step that can be resumed appends one record per finished unit of work to a journal in its step directory: for the solve, `<case>/journal.jsonl`, with each Z3 answer and its witness when SAT, beside `<case>/case.json` (06.C7, accepted with this choice in entry 174). Each record is on disk (`fflush`, then `fsync` or `FlushFileBuffers`) before the next unit starts. A resume reads the journal, ignores a torn last line and replays the answers through the solve's cache. A stop then has nothing left to save, and a kill, crash or power cut loses only the unit in progress: a killed or crashed run's journal is used on resume like a stopped run's. That amends Choice A1, which said a kill leaves no saved state (06.C19; accepted in prompt log entry 174).
- **Alternatives:**
  - write the resume state only when a stop request or the solve's budget ends the solve, as A1 first described;
  - write a full snapshot every few minutes.
- **Why:**
  - Checks take from under a millisecond to the 4 h cap: at 19:32, over the 2,696 timed checks in FuSION 1's limits files, the median was 1.40 s, a tenth took over 800 s, and the longest 14,400 s; r5's witness check once ran 34 h (candidate set `README.md`). A solve has at most 28 checks (2,723 checks in 222 solves), so an `fsync` per check, a few milliseconds each, adds at most a few tenths of a second to a solve.
  - On 2026-11-12, eight candidate-set runs (r9 to r16) were ended by SIGTERM at 18:06:28, 61 min after they started (`status.log`: `exit=143`; `run_fsc_tests.out`: `Terminated` eight times). r9, r10 and r12 had finished 7, 5 and 7 solver checks (8, 5 and 8 answers, counting cache hits), 4,194 s of CPU in all; the other five were inside their first check. Nothing was resumable. FuSION 1 also aborts with status 134 on an uncaught exception (S1.28). State written only on a clean stop survives none of these.
  - A periodic snapshot loses up to one interval, and still needs a clean-stop path.
  - Section 06 decides what the records hold; this choice fixes only that they are durable one by one.
- **Decision:** as proposed (prompt log entry 174)

### 09.C7 · Stop through a `stop` file

- **Proposed:** a stop is requested by creating `stop` in the run directory, the file of A1's two channels (entry 184). A watcher thread checks for it every 0.5 s, and a batch run never reads stdin. A stopped run ends with status `stopped` and exit status 3. If the main thread has not ended 30 s after the request, the watcher writes `run.json` and calls the grace hook, through which the `fusion` executable ends the process.
- **Alternatives:**
  - a `stop` line on standard input, read by a watcher thread;
  - both channels.
- **Why:**
  - The file works however a run was started. Twelve launcher scripts under `runs/` start FuSION 1, and none keeps a pipe to its stdin: five start it in the background with stdin from `/dev/null` (`run_sweep.sh`, and `run_all.sh`, `run_fsc_tests.sh`, `run_direct_runs.sh` and `run_string125_reruns.sh` in the candidate set), three in the background without redirecting stdin (`egf_hela/experiment/run_*_sweep.sh`), and four in the foreground (`real_experiments/run_experiment_*.sh`). A frontend restarted mid-run has lost any pipe as well, and section 10 uses none (10.C16, 10.C17). Anyone can create the file: the frontend, `touch` on Linux and macOS, `type nul >` on Windows, or section 01's `fusion --stop`.
  - It works the same on every platform (G3), with no platform code.
  - Polling costs one `stat` every 0.5 s; a stop then takes at most 0.5 s plus Z3's response to its interrupt.
  - A stdin reader is a thread blocked in a read at exit, and must treat the end of `/dev/null` as "no request". It adds nothing the file cannot do.
  - The 30 s cap bounds a Z3 tactic that ignores its interrupt. Nothing is lost by it, because the journal (09.C6) already holds every finished check. The executable, not the library, ends the process, as B1 requires.
  - Exit status 3 tells a stop from success (0) and failure (1) without reading `run.json`.
- **Decision:** as proposed (prompt log entry 184)

### 09.C8 · Ctrl-C and termination signals request the same stop; SIGHUP is ignored

- **Proposed:** the `fusion` executable, not the library, turns Ctrl-C and SIGTERM on Linux and macOS, and Ctrl-C, Ctrl-Break and the closing of its console on Windows, into the same stop request as the `stop` file. The handlers only set a flag that the watcher reads. A second Ctrl-C ends the process at once with status 130. SIGHUP is ignored on Linux and macOS, so a run outlives a closed terminal or a frontend that exits, and console writes that then fail are dropped, since the run log keeps every line (cross-section item 2B.19). Every Z3 solver runs with `ctrl_c=false`, so a Ctrl-C during a check reaches only `fusion`'s handler (11.C18). This is a reading of Choice A1, not an amendment (cross-section items 3A.5 and 4A.3; entry 185): prompt log entry 107 chose the platform-neutral stop request over "signals, plus a Windows path", and the stop file stays the one Stop mechanism.
- **Alternatives:** keep the operating system's defaults, the strict reading of entry 107: Ctrl-C and SIGTERM end the process at once. The journal survives (09.C6), but the run shows as died, not stopped, and section 01's coordinator cannot relay a stop.
- **Why:**
  - Entry 107 settled how Stop works for the frontend: a file, the same on every platform. This choice keeps that and adds nothing the frontend relies on.
  - Command-line users press Ctrl-C, and `kill`, `timeout` and Java's `Process.destroy()` send SIGTERM on Linux and macOS. The eight candidate-set runs above were ended with SIGTERM.
  - A clean stop writes `run.json` with status `stopped`, so the run shows as stopped rather than died, and the run log says where it stopped.
  - Section 01's sweep coordinator relies on it: only the coordinator receives Ctrl-C, and it writes each running point's `stop` file.
  - The frontend's runs must outlive a terminal from which it was started (section 10's platform notes), which only an ignored SIGHUP gives.
  - The cost is about 40 lines of platform code in one file of the executable: `sigaction` on POSIX, `SetConsoleCtrlHandler` on Windows.
- **Decision:** as proposed (prompt log entry 185)

### 09.C9 · Liveness through an OS file lock

- **Proposed:** when `fusion` claims its directory, it creates `run.lock` exclusively, writes one line with its process id and run id, and holds an OS lock until it exits: `fcntl` on POSIX, `LockFileEx` on Windows, on one byte at offset 2^31, past that line. The OS drops the lock when the process ends for any reason. A reader that can take the lock knows the process is gone, so a `run.json` that says `running` means the run died. The frontend tests with `FileChannel.tryLock()` and reattaches to running runs after a restart (10.C16), and a resume refuses a parent whose lock is held. A run root whose filesystem cannot lock, such as NFS without lock support, stays usable, with a warning at the start of the run that liveness cannot be shown (entry 185). The same lock can lock any file, for section 07's site (cross-section item 2B.17).
- **Alternatives:**
  - a lock, with a run root that cannot be locked refused as an error;
  - a heartbeat: the watcher touches a file every 10 s, and a run is dead once the file is a minute old;
  - the process id and start time in `run.json`, checked against the process table.
- **Why:**
  - A lock cannot go stale: the kernel releases it on exit, kill or crash.
  - Probed on this Linux machine: Java's `FileChannel.tryLock()` sees a lock taken with `fcntl` but not one taken with `flock`, so POSIX must use `fcntl`. On Windows, both sides use `LockFileEx`, whose locks are mandatory: a lock over the line would stop anyone reading it, so the lock sits past the end of the file, where both APIs allow it and a whole-file `tryLock()` still meets it.
  - A heartbeat needs a write every few seconds through checks of hours, and misjudges a laptop that sleeps or a slow network disk. Process ids are reused, and reading another process's start time differs on each platform.
  - The same exclusive create stops two `fusion` processes adopting one directory.
  - On a lock-less filesystem, refusing the run root would block users whose home directories are on NFS; with the warning they run, and liveness falls back to `run.json` alone.
  - The cost is about 30 lines per platform.
- **Decision:** as proposed (prompt log entry 185)

### 09.C10 · The event set

- **Proposed:** the lifecycle events in the table above, with dotted names (cross-section item 1.3, entry 190); a `solve.check.started` event before each solver check as well as section 06's `solve.check` after it; `step.progress` events inside long steps, at most one a second per step; no heartbeat.
- **Alternatives:**
  - A2's minimum: one event per finished check, and none when a check starts;
  - add a heartbeat event every few seconds.
- **Why:**
  - Single checks run for up to 4 h, and once 34 h. With a start event, the frontend can show which point is being checked and for how long. Without it, it shows only "solving" for hours, as FuSION 1's GUI did (S8.28).
  - The file stays small: at most 28 checks per solve, so about 112 check events for two cases, and 150 to 200 lines (30 to 40 KB) per run.
  - A heartbeat every 10 s would add 18,720 lines to a 52 h run and say nothing the lock (09.C9) does not.
- **Decision:** as proposed (prompt log entry 185)

### 09.C11 · One run log, `exec` output included

- **Proposed:** `<run name>.log` in the run directory is always written. It holds every console line, warnings and errors included, each with its UTC time and level; under the debug option (01.C8's `--debug`) it also holds the debug records, which never reach the console. `fusion` captures each `exec` child's output and passes every line through its log, so the run log holds that too (cross-section item 1.15). The frontend starts `fusion` with its console on the null device (10.C17), and section 01's sweep coordinator discards its children's consoles the same way. The `run.log` setting renames the log before the first step (S1.25).
- **Alternatives:**
  - a debug log that exists only under the debug option, leaving the caller to capture the console;
  - two files: `console.log` always, and a debug log under the debug option.
- **Why:**
  - All twelve FuSION 1 launchers capture the console by shell redirection into a log file (105 such logs: median 22.9 KB, max 52.0 KB). A frontend reading the child's pipe loses it when it restarts, and a Windows user who starts `fusion` from a shortcut has no redirection. In the run directory, the log survives both and can be followed.
  - Without the capture, an `exec` line's output would reach a console that the frontend and the coordinator discard, and be lost.
  - S1.25 asks for one debug log per run with a per-run name. One file holding the console transcript plus the debug records meets that, and is never empty.
  - Two files would split one story across two places, and with the debug option off the debug file would be empty.
- **Decision:** as proposed (prompt log entry 185)

### 09.C12 · A check-only mode gives the frontend the backend's rules

- **Proposed:** `fusion --check --json` (01.C14) reads the batch and runs every check a real run makes, on the original files, without a run directory, without copying or recording inputs, and prints the problems in cross-section item 1.9's format: section 01's diagnostics with this section's `command`, `setting` and `path` fields, and section 01's features, from one C++ type in `core`. It hashes only what a check compares: the saved steps and parent run a batch loads. The frontend calls it before Submit and to validate a form; `fusion --commands` gives it the command table, with the registry's defaults and each command's cross-argument rules, to evaluate as the user types (01.C9, accepted with this choice in entry 186).
- **Alternatives:**
  - the frontend re-implements the rules in Java;
  - a rules file, such as JSON Schema plus a list of file checks, read by both sides.
- **Why:**
  - Decision 0021 asks both sides to validate and leaves to Phase 2 how they share one set of rules. One implementation cannot drift from itself; a Java copy would have to follow every module's inputs.
  - It is quick: testing the reference run's 174 input files took 17 ms with one `stat` command. Copying and hashing every input, the slow part, are left out (0.03 s per 7 MB); a saved step that a batch loads is hashed, at most 13.4 MB for the largest network.
  - A rules file can state types and ranges, but not checks such as "every member of this KGML list exists" or "this saved step was written in a format this FuSION reads".
- **Decision:** as proposed (prompt log entry 186)

### 09.C13 · The shared store lives in each user's cache directory

- **Proposed:** one store per user for results that runs can share: `$XDG_CACHE_HOME/fusion` (by default `~/.cache/fusion`) on Linux, `~/Library/Caches/FuSION` on macOS, `%LOCALAPPDATA%\FuSION\Cache` on Windows. `run.cache_dir` or `FUSION_CACHE_DIR` moves it, and `run.cache=false` bypasses it, as do builds with uncommitted changes. It is the middle of section 03's three tiers: memory within a process, this store, and each run's own copy (03.C3, accepted with this choice in entry 166). The store is read as a cache, not an output, so S1.26 and S8.21 stand (entry 166). Each owning section lays out, keys and checks its own entries (section 03: `merge/1/<key>.xml`, keyed by rules version, 03.C4 and 03.C8). Entries are written once under a temporary name and renamed into place, the first writer winning, and are never evicted automatically.
- **Alternatives:**
  - no shared store: reuse only within a process and by starting from a saved network, so a new batch on the same pathways rebuilds;
  - a store inside the runs root (`<root>/.fusion-store`), shared by everyone who uses that root, such as a lab server's users.
- **Why:**
  - S2.22 requires that runs on the same pathways share one merge, without one run naming another. Under `runs/`, 129 merged graphs have only 19 distinct contents, and FuSION 1 spent 3 to 126 s merging in every run that merged (S2.22's table).
  - In 2.0 a hit saves less time than in FuSION 1: section 03 estimates tens of milliseconds per run (03.C3), since its merge is a single fast pass. The case for the store is S2.22's requirement, not speed.
  - An entry is the size of a merged network, 1.44 MB median and 13.4 MB at most, so a hundred distinct merges take about 150 MB.
  - The cache directory is each platform's place for data that can be rebuilt, which is what an entry is: runs keep their own copies, so deleting the store loses nothing. Inside a runs root, the store would sit among results and need group permissions on a shared server.
  - Concurrent runs may build the same entry: the nine candidate runs started together would have built the two distinct merges they needed several times each. Renaming makes that safe without locks, at the cost of duplicated work that in 2.0 is small.
  - Keeping builds with uncommitted changes out protects the store from a rule changed in a working copy before its rules version is raised.
- **Decision:** as proposed (prompt log entry 166)

### 09.C14 · Failures are exceptions; reported results are return values

- **Proposed:** a failure is a `core::Failure` exception with a stable code, a message and where it happened; `core::UsageError` and `core::InstallError` derive from it (cross-section item 1.1). The interpreter catches it once per command, reports it with the batch line and stops the batch (S1.28). A reported result, one of the three this section defines (entry 167), is part of a step's return value (`core::Outcome<T>`, with `Report{code, message}`), never an exception. A stop is `core::Stopped`.
- **Alternatives:**
  - `expected<T, Failure>` return values throughout, from a library such as `tl::expected`, since `std::expected` is C++23;
  - error codes.
- **Why:**
  - FuSION 1's live commands parse numbers with `stoi`, `stof` or `stoul` 24 times, with only pathz3's 4 optional arguments inside a `try`, so a blank value aborts the run with status 134; and `GraphManagerNew.cc` has 27 live `exit(1)` calls (S1.28). One catch per command turns each such case into a reported failure with its batch line.
  - Z3's C++ API, `std::filesystem` and the standard containers already throw, so an `expected` style still needs a catch around each of them, plus a wrapper at every call.
  - Exceptions cost nothing on paths that do not throw, and a failure ends the batch, so its own cost does not matter.
  - Keeping reported results out of exceptions makes them visible in each step's signature: `fb_rch`'s code shows that an empty corridor is an outcome, not an error.
- **Decision:** as proposed (prompt log entry 186)

### 09.C15 · Random numbers outside GSEA: `std::mt19937_64` with FuSION's own distributions

- **Proposed:** for everything but GSEA, `core::Rng` wraps `std::mt19937_64`, whose output the C++ standard fixes bit for bit, and implements its own bounded integers (Lemire's method), uniform doubles (53 bits) and Fisher–Yates shuffle. Independent streams are seeded from (seed, stream id) through SplitMix64, so split work draws the same numbers whatever the thread count. Section 12's property tests use it, with a fixed default seed and `FUSION_TEST_SEED` for the weekly runs (12.C9, accepted with this choice in entry 186). GSEA keeps its own exact port of gseapy's generator in `core` (08.C1, amending F1, entry 159).
- **Alternatives:**
  - the standard distributions, such as `std::uniform_int_distribution` and `std::shuffle`;
  - another engine written into core, such as PCG64 or xoshiro256**.
- **Why:**
  - The standard fixes the engines' outputs but not the distributions' algorithms, so libstdc++, libc++ and MSVC's library draw different numbers from the same engine and seed, and FuSION's results must not depend on the platform (G3).
  - Checked here: the 10,000th output of a default-constructed `std::mt19937_64` is 9981545732273789042, the value the standard requires.
  - Speed does not decide it: 1000 shuffles of 3000 items, 3 million draws, took 53 ms (g++ 13.3, `-O2`).
  - GSEA's numbers must match gseapy to the last bit, which only gseapy's own stream gives (08.C1); every other use needs only a fixed, platform-independent stream.
- **Decision:** as proposed (prompt log entry 186)

### 09.C16 · Floating-point contraction is off, and canonical outputs are byte-identical across platforms

- **Proposed:** no build fuses `a*b+c` into one fused multiply-add: `-ffp-contract=off` for GCC and Clang, and MSVC without `/fp:contract`, with section 11's other flags (11.C7: `-O2` with link-time optimisation, `/fp:precise` on MSVC, no `-ffast-math`, `-Ofast` or `-march=native`, the baseline x86-64 and arm64 instruction sets, the same flags for Z3 where it is built from source). One promise follows (cross-section item 1.18, accepted with this choice in entry 186): canonical outputs are byte-identical on Linux, macOS and Windows, which section 12 tests with one golden set (12.C11). Every result path therefore avoids the platform's `exp`, `log` and `pow`, or uses one correctly rounded implementation.
- **Alternatives:**
  - contraction off, with cross-platform results agreeing only within a tolerance, which changes section 11's identity test and 12.C11;
  - the compilers' defaults: GCC contracts freely in its default GNU mode and Clang within an expression, each wherever the target has an FMA instruction.
- **Why:**
  - Apple silicon always has FMA, while x86-64 builds use it only when the target is raised, so with the defaults the same source rounds differently on a Mac and on Linux in every sum of products, such as GSEA's weighted running sums and normalised scores (S7.10, S7.11).
  - Maths libraries differ in the last bit between platforms, so a pathway exactly at the FDR cut-off could flip unless result paths avoid them. With both measures, one golden set serves all three platforms.
  - The user has already removed one platform dependence of GSEA's result, the tie order (S7.05).
  - The cost is the speed of fused instructions in FuSION's floating-point work, which is mostly GSEA's 1.3 s (S7.19). It is not measured here and is expected to be small.
- **Decision:** as proposed (prompt log entry 186)

## Interfaces with other sections

**01 · Batch language and interpreter**

- 01 gives this section the plan: each step's id, command, batch line, normalised line, settings and inputs (`StepSpec`), with the inputs the batch makes for itself marked (01.C12), its variables, and the run-level options in `Options`: the `run` line, which takes any run-level setting by key except `run.debug`, or `--root`, `--label`, `--log` and `--set`; `--run-dir` to adopt; `--resume` for a parent; `--debug`; `--tag` for labels; `--check --json`; `--point`.
- 01 takes 09.C1's name, with the label from the batch and, by default, the batch file's stem and directory (01.C7). It runs `load run=<dir> step=<step>` through `Registry`, and `fusion --stop` writes the `stop` file.
- 01's sweeps: each point is a complete run in `points/<NN>-<label>/` inside the sweep's directory, beside `points.tsv`. The coordinator relays a stop by writing each running point's `stop` file, and discards its children's consoles (09.C11). `run.json`'s `point`, and the coordinator's `points`, hold 01's indexes, labels and values.
- 01 catches `core::Failure` once per command and calls `Run::fail`; after a `Report`, it skips every later line of the point (01.C15); it marks the steps after a stop `skipped`.
- 01 runs `exec` through section 11's process layer and passes the child's output through `core::Log` (cross-section item 1.15).
- 01's command table holds the `SettingSpec`s, declares each command's tool needs for section 11's check (cross-section items 2A.1 and 2B.15) and its cross-argument rules (01.C9), and must not name a command `inputs`, `debug`, `points` or `overrides`. `fusion --commands` exports it with the registry's defaults.
- Exit statuses: 01.C14 and this section give the same table (0, 1, 2 for usage, 3 and 130).
- Events: 01 emits this section's lifecycle events and its own (`sweep.planned`, `point.started`, `point.finished` and its record of a FuSION 1 move) in this envelope, with dotted names (cross-section item 1.3).
- FuSION 1 compatibility: 01.C11 rewrites FuSION 1's six `exec mv` moves of fixed-name outputs; 01 runs `exec` lines again on a resume; 01.C2's relative-path rule covers inputs only.

**02 · Network model and 03 · Merge**

- 02 defines the `network` artefact (S2.25), with a format name and version, and its loader. This section adopts 02's outline of `core`'s ids and orderings (cross-section item 2A.5), and 02 uses `core::Sha256` and this section's vocabulary (1.1).
- 02's `BindingTable` writes the run-level `bindings.tsv` at the run directory's top level, with 02's columns plus section 06's `line` and `note` (cross-section item 1.22), once when the run ends, whatever its status, through `Run::create_run_file` (entry 230).
- 03 owns its store entries: one file per base merge, `merge/1/<key>.xml`, keyed by a rules version, its build settings, the id map's digest and the files' contents, but not FuSION's version; written under `.tmp-<key>-<pid>-<n>.xml` and renamed; checked by its recorded key and sources (03.C3, 03.C4, 03.C8). This section gives the root (09.C13), `Cache::publish`, the bypass for unclean builds, and `run.cache`, which replaces 03's own switch (1.16).
- 03's run-directory files (`merged_graph.xml`, `exceptions.tsv` and, on request, `merged_edges.tsv`) go in the `mff/` step directory. 03's `run.json` fields (the merge key, where the base merge came from, the exception set's digest and size) go in the step's `details`.
- 02 or 03 parses KGML list files for the check, which records each member as an input; network inputs are parsed in full during the check, and each parse is kept for its step (02.C15).

**04 · Expression**

- 04 defines the `observations` artefact and returns `observations.none` as a `Report` (04.C5). `cudf`'s omitted outputs land in its step directory (04.C11). Its query mode reads one batch line from stdin and writes no run directory and no events (04.C8); read-only calls like it are outside A1 (entry 168).
- The check finds output clashes by file identity, as 04 designs (2A.7).

**05 · Corridor**

- 05 defines the `corridor` artefact with its parameters (S4.21), its arguments named as 05.C8 has them (`edges_to_target` with `any`, `activation` or `expression`), writes `observations_match.tsv` (05.C9), writes its traces through `create_debug` (S4.19), and returns `corridor.empty` as a `Report` (S4.18).

**06 · Solve**

- 06 registers a `core::StopCallback` that calls `z3::context::interrupt()`, and every solver runs with `ctrl_c=false` (11.C18). An interrupted check is not an answer: it is not cached, not journaled and not labelled UNDEFINED.
- 06 appends each answer to `<case>/journal.jsonl` before the next check (09.C6, 06.C7), writes `case.json` through `OutputFile`, and rebuilds its cache from `Files::resumed()`, also after a kill (06.C19).
- 06 gives each of its settings a resume policy, the box, `points`, `solutions`, `retry`, the timeouts, the budget and `threads` among the may-change ones. 09.C5 then reruns a matching solve from its journal when these differ, and reruns one whose result is `partial`.
- 06's outcomes inside a solve (`no_point`, `infeasible`, `no_de_gene`, `budget_exhausted`) are case statuses, not reports (cross-section item 1.8); a case ended `stopped` or `budget_exhausted` makes the step `partial`.
- 06 writes its dumps (`<case>/dumps/n<n>_e<e>.smt2`, `base.smt2`, `session.smt2`) through `create_debug`, so they land in `debug/pathz3/<case>/dumps/` (cross-section item 1.4).
- Events: 06 emits `solve.check.started` and its own `solve.*` events in this envelope, renames its per-check counter to `check`, and sends warnings only through `warning` with dotted codes (cross-section item 1.3).
- Cases are named `in`, `out` and `free` (06.C10), as here. A resumed solve runs the Z3 the build pins (06.C18).
- A cross-run solve cache, if 06 adds one, goes through `Context::cache`, keyed with the Z3 version, the seed and the timeout.

**07 · Results and outputs**

- 07 writes `pathz3/results.json` with each case's `.dat` beside it (07.C1), FuSION 1's text in `pathz3/legacy/` when `legacy_prefix` asks for it (cross-section item 1.21), `plot_pareto/pareto.png` and `pareto.gp`, and `pex/explain/…` with the library in `pex/lib/` (07.C2, 07.C3). The outputs appear in `run.json`.
- 07 gives `run_page_html`, from which this section writes the top-level `index.html` once, when the run ends, through `Run::create_run_file` (cross-section item 1.4).
- 07 puts its results summary (points per case, least relaxations, the FSC verdict) in the solve step's `details` (cross-section item 2B.7).
- 07 declares gnuplot's requirement through section 01's table, and section 11's check probes it (2A.1); the gnuplot runner sits in `core` (2B.13).
- The site export is the one write outside a run directory (2B.3). It takes the site's lock through `Lock::acquire` (2B.17) and writes through `OutputFile`.

**08 · Enrichment**

- 08's generator, the exact port of gseapy's stream (`Xoshiro256pp` with `shuffle_like_rand09`), sits in `core` beside `Rng` (08.C1, amending F1); its seed and permutation count are the settings `gsea.seed` and `gsea.permutations`.
- 08 defines the `shortlist` artefact and its table `<stem>.gsea.tsv` (S7.16), which S7.18's review loads, and returns `gsea.empty_selection` as a `Report` (S7.03).

**10 · Frontend architecture**

- The frontend is a JavaFX desktop application (10.C1). It creates each run directory with 09.C1's name and its label (10.C18), writes `run.batch` and the `overrides/<kind>.txt` files of S8.15 into it, and starts `fusion -b <dir>/run.batch --run-dir <dir> --tag session=<id> --tag frontend_run=<run id>` with its console on the null device (10.C17). After that it writes only `stop`, as a note written under a temporary name and renamed.
- It follows `events.jsonl` by complete lines and the run log for the live view, and reads `run.json` with Java NIO, whose delete sharing lets the backend replace the file on Windows.
- It tests liveness with a whole-file `FileChannel.tryLock()` on `run.lock`, releases the lock at once, and reattaches to running runs after a restart (10.C16).
- It treats exit statuses 0, 1, 2, 3 and 130 as above, and offers Kill (`destroyForcibly()` on the process tree) only 60 s after a stop request (10.C19); the journal still holds every finished check after a kill.
- It calls the check-only mode before Submit (09.C12), builds its blocks from `fusion --commands`, and resumes a run with `fusion --resume <old dir> --run-dir <new dir>`.
- It refuses a backend whose version differs from its own (11.C16).
- Sizing for its queue (10.C20): FuSION 1 runs peaked at 0.33 to 2.03 GB of resident memory each (318,920 to 1,982,592 KiB), and each Z3 check uses one thread.
- The label rule's C++ and Java copies share one golden list (cross-section item 1.26).

**11 · Build, dependencies and platforms**

- 11 supplies `run.json`'s `versions`, `tools`, `platform`, `install` and `environment` blocks; `environment` also records `FUSION_CACHE_DIR` (cross-section item 1.14).
- 11's platform layer implements the `core` functions declared here (`path_from_utf8`, `cpu_seconds`, `peak_rss_bytes`), its build information gives `build_id()`, and its process layer runs `exec` with the output captured (11.C19).
- 11 owns the external-tool check and `deps.missing`; this section's check calls `deps::Registry::check()` (2A.1). Install errors at startup are `core::InstallError`, reported through the `error` event with `install.*` codes, and exit 1 (2B.16).
- SHA-256: a small vendored implementation is enough. A run hashes its inputs as it copies them, about 22 MB at most, and its outputs as it commits them: a network up to 13.4 MB and, under the debug option, dumps such as the full run's 123.1 MB.
- The build id is the `VERSION` file's version and the commit, plus the executable's SHA-256 for a build with uncommitted changes (11.C16).
- With GCC 13 and Clang 17 as minimums (11.C22), `std::format` may be used; core keeps its own stop token. macOS 13.3 or later (11.C1) gives floating-point `std::to_chars`.
- Builds turn contraction off with 11.C7's flags (09.C16). gseapy's BSD and `rand`'s MIT notices ship with 08's generator in `core` (08.C1).
- The Windows build is long-path aware.

**12 · Test architecture**

- This section writes the JSON Schemas of `run.json` and `events.jsonl`; section 12 keeps the one copy in `backend/tests/schemas/` (2A.10), and its normaliser masks the volatile fields listed under `run.json`.
- The test hooks below; golden values for `Rng`, fingerprints, `format_number` and the label rule; one golden set for all three platforms (12.C11).

## Test hooks

Library level, with no `fusion` process:

- **Directory:** 16 threads claim one label in the same second and get 16 directories; the label rule's golden list, shared with section 10's Java copy, includes `EGFR→CXCL8 / b10` → `EGFR-CXCL8-b10` and `EGFR to CXCL8, FSC JUN` → `EGFR-to-CXCL8-FSC-JUN`; `CON` is altered; adoption of a directory with `run.lock` is refused, and one holding `run.batch` and `overrides/` is adopted with both recorded in place.
- **Manifest:** a write interrupted between the temp file and the rename leaves the previous `run.json` whole; every `run.json` written validates against its schema.
- **Events:** every line parses and validates against its schema; `seq` rises by one across threads; events emitted inside a step carry `step`, `line` and `point`; `step.progress` is coalesced to one a second.
- **Stop:** callbacks run once, including one registered after the request; a `stop` file gives `stop.requested` within 1 s; a step that ignores its token leads, after the grace period (shortened in tests), to `forced: true` in `run.json` and one call of the grace hook, which the test records instead of ending the process.
- **Journal:** a file cut mid-line reads back every whole record.
- **Fingerprint:** golden values for fixed inputs, the same on Linux, macOS and Windows; any change to a result-changing setting or to an input byte changes it; a change to a `may_change` setting does not, but it makes a finished step rerun from its journal instead of loading, and so does a `partial` step; two unclean builds give different build ids.
- **Registry and store:** a changed artefact byte gives `artefact.changed`; two processes publishing one entry leave one file and both succeed; an unclean build gets no `Cache`.
- **Inputs:** decision 0021's four cases (missing required, absent optional, empty optional, missing named optional); the copies' SHA-256 equals `sha256sum`'s; a file changed after its copy does not change what the check parsed; an input that the batch makes is checked and copied just before its step.
- **Outputs:** a relative output lands in the step directory; an absolute one is written there and copied to its path, with both in `run.json`; a clash by file identity is `output.clash`.
- **Lock:** a child process holding the lock makes `held()` true, and after the child is killed it is false; Java's `tryLock()` agrees (section 12's frontend tests); on Windows, another process reads `run.lock`'s line while the lock is held; on a filesystem without locks, the run starts with `run.lock_unsupported`.
- **Precedence:** one setting given by a step's argument, by `--set`, on the `run` line and by default resolves in that order, and `run.json` names the source that applied (`named`, `set`, `run_line`, `default`); two `fb_rch` lines with different values keep both.
- **Rng and text:** golden first values of `Rng::stream(42, 0)`, of `below(3054)` and of a shuffle of 0 to 9; golden outputs of `format_number` (`0.0`, `1e-05`, `0.0001`, `13632.95`) and `utc_timestamp`; all equal on the three platforms.

End to end, through `fusion`:

- Stop by file during a solve on a small corridor with a slowed check: exit 3, `run.json` stopped. Resume: the frontier equals an uninterrupted run's, and the checks journaled before the stop plus those asked after it equal the uninterrupted run's checks. Witnesses may differ, as Z3 can return another model for the same point after a resume; each must still be valid (section 06).
- The same after `kill -9` on POSIX or `TerminateProcess` on Windows: `run.json` says running, the lock is free, and the resume gives the same frontier.
- A forced stop, with a step that ignores its token: exit 3 after the grace period, with `forced: true`.
- A batch whose line 9 fails exits 1 with the error at line 9. Restarted with the line corrected, its earlier steps are `loaded` and their outputs are byte-identical.
- An empty corridor: `step.finished` carries `report.code` `corridor.empty`, every later line, `exec` included, is `skipped`, and the exit status is 0.
- A FuSION 1 batch whose `exec` lines write `pathz3`'s node files runs to its end, and the `exec` lines' output is in the run log.
- Two `fusion` processes on one batch at once make two directories with identical artefact hashes.
- The same batch run twice gives every artefact the same SHA-256 (`run.json`, events and the log excepted, after masking the volatile fields). Across platforms, in CI: canonical outputs are byte-identical (09.C16, 12.C11).
- For a set of broken batches, the check-only mode reports exactly the problems a real run's check records.
- The run page `index.html` is written once, at the end, and never rewritten; `bindings.tsv` is written at the end of a failed or stopped run too.
- On Windows, replacing `run.json` succeeds while a Java NIO reader holds it open.
- A full disk (a small tmpfs or a quota) gives `io.write` with exit 1.
- Without the debug option there is no `debug/`; with it, dumps are named by case and point.

## Open questions

All ten are closed.

1. **A1 and kills.** Closed: a killed or crashed run's journal is used on resume, amending A1 (06.C19, prompt log entry 174).
2. **The whole-solve budget.** Closed: an exhausted budget is a case status inside the results, the batch goes on, and the step is `partial` and resumable (06.C3, entry 167).
3. **FuSION 1's `exec mv` lines.** Closed: the interpreter recognises the six moves and writes 2.0's matching outputs to their destinations (01.C11, amending S1.14 for them, entry 179).
4. **Floating point across platforms.** Closed: contraction off, and canonical outputs byte-identical on all three platforms (09.C16, entry 186).
5. **Resuming across versions.** Closed: resume fingerprints use the build id, and the store keys on rules versions (cross-section item 3A.3, entry 166).
6. **Network filesystems.** Closed: a run root that cannot be locked stays usable, with a warning (09.C9, entry 185).
7. **An empty frontier.** Closed: only `corridor.empty`, `gsea.empty_selection` and `observations.none` are reports; an empty frontier is a case status, and the plot still runs (cross-section items 1.8 and 3A.14, entry 167).
8. **Outputs outside the run directory.** Closed: section 07's site export is the one accepted exception (cross-section items 2B.3 and 4A.11, entry 169).
9. **SIGHUP.** Closed: `fusion` ignores SIGHUP on Linux and macOS (09.C8, entry 185).
10. **09.C8 and Choice A1.** Closed: handlers that only make the stop request are a reading of A1, not an amendment (cross-section items 3A.5 and 4A.3, entry 185).
