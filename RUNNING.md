# Running FuSION 2.0

Installation and usage guide for FuSION 2.0: the `fusion` command-line tool and the desktop application. The guide is maintained with the code; changes to the build, dependencies, command-line interface or application launch are reflected here.

**Verified on** Ubuntu 24.04.5 (x86-64), 28 November 2026, FuSION 0.1.0. The macOS and Windows procedures have not yet been verified.

## Contents

1. [Platform support](#platform-support)
2. [Prerequisites](#prerequisites)
3. [Installation on Linux](#installation-on-linux)
4. [Installation on macOS](#installation-on-macos)
5. [Installation on Windows (WSL 2)](#installation-on-windows-wsl-2)
6. [Quick start](#quick-start)
7. [Desktop application](#desktop-application)
8. [Analysing your own data](#analysing-your-own-data)
9. [Output directory](#output-directory)
10. [Run control](#run-control)
11. [Known limitations](#known-limitations)
12. [Testing](#testing)
13. [Configuration](#configuration)
14. [Troubleshooting](#troubleshooting)

## Platform support

| Platform | Build | Status |
|---|---|---|
| Linux: Ubuntu 24.04, x86-64 | Native | Verified |
| Linux: other distributions | Native, given the [prerequisites](#prerequisites) | Untested |
| macOS 13.3 or later (Apple silicon or Intel) | Native, with Homebrew | Not yet verified |
| Windows 11, or Windows 10 build 19044 or later | Linux build under WSL 2 | Untested |

FuSION is built from source; installers are not yet available.

## Prerequisites

| Component | Version | Purpose |
|---|---|---|
| C++ compiler | GCC 13 or later on Linux; Apple Clang on macOS | Backend |
| CMake and Ninja | CMake 3.28 or later | Build system |
| Z3, with C++ headers | 4.8.12 on Linux; Homebrew's release on macOS | Constraint solver |
| gnuplot | 6.0 | Pareto plots and threshold histograms |
| Liberation Sans | any | Plot font (another font is substituted if missing) |
| JDK | 21 | Desktop application |
| Graphviz (optional) | any | Static SVG exports (`static=svg` on `pex` or `psp`) |

- **Network access** is needed for the first build. CMake fetches pugixml, nlohmann/json, fast_float and Catch2, verified by SHA-256. Gradle fetches itself and the application's dependencies, verified against `frontend/gradle/verification-metadata.xml`.
- **Disk space:** about 50 MB for the source and the release build, 2.1 GB for the debug build with tests, and 450 MB for Gradle and the application's dependencies.
- **KEGG KGML files** are needed to analyse real data; FuSION does not distribute them. The quick-start dataset needs none.

## Installation on Linux

These steps are for Ubuntu 24.04. Run commands from the repository root unless stated otherwise.

### 1. Install system packages

```sh
sudo apt update
sudo apt install g++ git cmake ninja-build libz3-dev gnuplot fonts-liberation openjdk-21-jdk libgtk-3-0t64
```

`libgtk-3-0t64` provides GTK 3 for the JavaFX interface; desktop installations already include it.

### 2. Clone the repository

The repository is private; request access from its owner. Over HTTPS, use `https://github.com/arjunjeewan/FuSION2.git` with a personal access token.

```sh
git clone git@github.com:arjunjeewan/FuSION2.git
cd FuSION2
```

### 3. Configure and build

```sh
cmake --preset release
cmake --build --preset release
```

Configuration reports `Z3 4.8.12 from the system`, and the build ends with `Linking CXX executable stage/bin/fusion`, without warnings. The build uses all cores; pass `-j N` to limit it.

`build/release/stage/` follows the installation layout: `bin/fusion`, and `share/fusion/` with the bundled data. `fusion` locates its data relative to its own path, so it runs from any directory.

### 4. Verify the installation

```sh
build/release/stage/bin/fusion --version
```

```text
FuSION 0.1.0 (build 0.1.0+<commit>)
  home      /home/you/FuSION2/build/release/stage/share/fusion
  dot  not found
  font:Liberation Sans   system
  gnuplot  6.0.0 PATH
  sh   system
  data      ok
```

The report lists the data directory (`home`) and each runtime dependency. `data ok` confirms the integrity of the bundled data. Install any dependency reported as `not found` before running analyses; `dot` is optional.

### 5. Add `fusion` to the PATH

```sh
export PATH="$PWD/build/release/stage/bin:$PATH"
```

To make this permanent, add the same line, with the absolute path, to your shell profile (`~/.bashrc`).

## Installation on macOS

> **Not yet verified on a Mac.** If a step fails, report the command and its full output.

FuSION is built natively on macOS 13.3 or later, on Apple silicon or Intel, with the same build presets as on Linux. On macOS it uses Homebrew's Z3: Pareto frontiers agree with Linux builds when every solver query completes, but individual witnesses and solve times can differ.

1. Install the Xcode Command Line Tools and the [Homebrew](https://brew.sh) packages:

   ```sh
   xcode-select --install
   brew install cmake ninja z3 gnuplot openjdk@21
   brew install --cask font-liberation
   ```

2. Make JDK 21 available to Gradle, and add the line to `~/.zprofile` to keep it:

   ```sh
   export JAVA_HOME="$(brew --prefix openjdk@21)/libexec/openjdk.jdk/Contents/Home"
   ```

3. Clone, build and verify as on Linux ([steps 2 to 5](#2-clone-the-repository)). The build finds Homebrew's Z3 by itself, and configuration reports its version, for example `Z3 4.15.4 on macOS`. Use `~/.zshrc` as the shell profile.

## Installation on Windows (WSL 2)

> **Untested.** There is no native Windows build; FuSION runs as a Linux build under WSL 2.

1. From an elevated PowerShell prompt, install WSL 2 with Ubuntu 24.04:

   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

   Name the distribution explicitly: the default is a newer Ubuntu release, which is untested and may package a different Z3 version. See [Install WSL](https://learn.microsoft.com/en-us/windows/wsl/install).
2. Follow the [Linux installation](#installation-on-linux) in the Ubuntu shell. Keep the repository and data on the Linux file system, for example `~/FuSION2`, not under `/mnt/c`, which is considerably slower.
3. The desktop application needs WSLg, included in Windows 11 and in Windows 10 build 19044 or later. If no window appears, run `wsl --update` and then `wsl --shutdown` from an elevated prompt. See [Run Linux GUI apps with WSL](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).
4. WSL includes no Linux web browser or file manager, so the application may be unable to open pages and folders. Run outputs are accessible from Windows Explorer at `\\wsl$\Ubuntu-24.04\home\<user>\FuSION\runs\`.

## Quick start

The repository includes FuSION 1's toy network (22 genes, 25 edges) and a matching fold-change file. This example searches for signalling paths of up to 10 edges from `hsa1` to `hsa22`, and tests whether `hsa13` is required. It completes in about one second.

1. Create a working directory with the input files:

   ```sh
   mkdir -p ~/fusion-toy
   cp backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/with_node_included.tsv \
      backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/report_example_with_pathways.xml \
      ~/fusion-toy/
   ```

2. Create the batch file:

   ```sh
   cat > ~/fusion-toy/toy.batch <<'EOF'
   ;; FuSION 1's toy network: signalling paths from hsa1 to hsa22, testing whether hsa13 is needed.
   cudf values=with_node_included.tsv up=0.9 down=-0.9
   rgx file=report_example_with_pathways.xml split_threshold=1
   fb_rch source=hsa1 target=hsa22 bound=10 edges_to_target=any
   pathz3 fsc=hsa13 node_min=0 node_max=7 edge_min=0 edge_max=7 timeout=150s
   plot_pareto
   pex
   exit
   EOF
   ```

   The steps, in order:
   - `cudf` classifies genes as up- or down-regulated.
   - `rgx` loads the network.
   - `fb_rch` computes the reachability corridor.
   - `pathz3` computes the Pareto frontier with `hsa13` included (`in`) and excluded (`out`).
   - `plot_pareto` draws the Pareto plot, and `pex` writes the explanation pages.

   Relative paths resolve against the batch file's directory. `fusion -h <command>` lists a command's arguments and defaults.

3. Validate the batch file, then run it:

   ```sh
   cd ~/fusion-toy
   fusion --check -b toy.batch
   fusion -b toy.batch
   ```

   Validation reports `toy.batch: the check found no error`. The run ends with:

   ```text
   with FSC (AADAC): 4 Pareto points, all certified
     n=1 e=7 | n=2 e=5 | n=5 e=3 | n=7 e=2
   without FSC (AADAC): 4 Pareto points, all certified
     n=0 e=7 | n=1 e=5 | n=4 e=3 | n=7 e=1
   comparison: forcing AADAC in raises the frontier
   ...
   Finished: ok, exit 0, ...
   ```

   These frontiers match FuSION 1's reference results.
   - `n` and `e` are the numbers of node and edge relaxations. Each point is Pareto-optimal, and *certified* means its optimality was proven.
   - FSC denotes the node tested for significance, `hsa13`, shown by its gene symbol, AADAC.

4. Open the run summary, `toy_<timestamp>/index.html`, in a web browser (`xdg-open` on Linux, `open` on macOS). It links the Pareto plot and the explanation pages.

## Desktop application

### Launch

Build the backend first, then:

```sh
cd frontend
./gradlew run -Pfusion.backend.executable="$(cd .. && pwd)/build/release/stage/bin/fusion"
```

- The first launch downloads Gradle 9.9.1 and the application's dependencies, about 450 MB.
- `fusion.backend.executable` selects the backend binary. It defaults to the debug build, `build/dev/stage/bin/fusion`.
- The command blocks until the application exits.

### Accounts

On first launch, create an account: a user name, an optional display name, and a password of 8 to 72 bytes. Accounts are local to the machine and stored in the application's data directory. Later launches open the sign-in screen.

### Example session

Create a session with **File > New session...** and enter the quick-start inputs on the **Inputs** tab:

| Field | Value |
|---|---|
| Log fold-change file | `<repository>/backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/with_node_included.tsv` |
| Up threshold / Down threshold | `0.9` / `-0.9` |
| Pathway source | `merged_graph` |
| Merged network file | `report_example_with_pathways.xml`, in the same directory |
| Source node / Target node | `hsa1` / `hsa22` |
| Node to test for significance | `hsa13` |
| Node relaxations / Edge relaxations, at most | `7` / `7` |
| Per-query timeout | `150s` |

Leave the other fields at their defaults; required fields are marked `*`.
- **Check** validates the inputs, and **Submit** validates them and starts the run.
- The **Runs** tab shows progress and, on completion, the log, frontier, Pareto plot and explanation pages.
- **Open the run page** opens `index.html` in the browser, and **Open folder** opens the run directory.

### Data locations

| Data | Default location | Override |
|---|---|---|
| Accounts, sessions, settings (Linux) | `~/.local/share/fusion/frontend`, or `$XDG_DATA_HOME/fusion/frontend` | `FUSION_DATA_DIR` |
| Accounts, sessions, settings (macOS) | `~/Library/Application Support/FuSION/frontend` | `FUSION_DATA_DIR` |
| Run directories | `~/FuSION/runs`, or the session's **Results folder** | `FUSION_RESULTS_DIR` |

One instance runs per data directory; a second launch brings the running instance to the front and exits.

### Password reset

Close the application, then from `frontend/`:

```sh
./gradlew installDist
build/install/fusion-frontend/bin/fusion-frontend --reset-password <username>
```

The command prompts for the new password twice. `./gradlew run` cannot be used for this, because Gradle does not forward standard input.

## Analysing your own data

### Input data

- **Log fold-change file:** two whitespace-separated columns without a header: KEGG gene ID (for example `hsa1956`) and log2 fold change. `cudf` also accepts tables with a header, and gene-symbol mapping; see `fusion -h cudf`.
- **KGML files:** one file per pathway, named by pathway ID (for example `hsa04010.xml`), in one directory. FuSION does not distribute KEGG data, whose use is subject to the KEGG licence. Academic users can retrieve files through the [KEGG REST API](https://www.kegg.jp/kegg/rest/keggapi.html):

  ```sh
  mkdir -p ~/kgml
  curl -o ~/kgml/hsa04010.xml https://rest.kegg.jp/get/hsa04010/kgml
  ```

  The bundled pathway list, `share/fusion/data/kegg_pathways.txt` (167 pathways), is resolved against this directory. Missing files are reported before the run starts.
- **Source and target genes** as KEGG IDs, and optionally a gene to test.

### Using the desktop application

Set **Pathway source** to `kgml_list` and specify the **KGML folder**; use the bundled **Pathway list** or your own. GSEA pathway shortlisting is enabled by default (`fdr:0.25`). The expression thresholds, relaxation bounds and per-query timeout have no defaults; the application suggests values.

### Using the command line

Export a batch file from the application (**Pipeline > Export the batch...**), or write one. For example, EGFR (`hsa1956`) to CXCL8 (`hsa3576`), testing JUN (`hsa3725`):

```text
cudf values=my_lfc.tsv up=1 down=-1
mff list=$FUSION_HOME/data/kegg_pathways.txt split_threshold=1 kgml_dir=/home/you/kgml
fb_rch source=hsa1956 target=hsa3576 bound=10 edges_to_target=any
pathz3 fsc=hsa3725 node_max=8 edge_max=8 timeout=4h
plot_pareto
pex
exit
```

To merge only the pathways GSEA selects, add a `gsea` step and omit `list=` from `mff`:

```text
gsea values=my_lfc.tsv candidates=$FUSION_HOME/data/kegg_pathways.txt selector=fdr:0.25 shortlist=gsea_kgml_files.txt kgml_dir=/home/you/kgml
mff split_threshold=1 kgml_dir=/home/you/kgml
```

`$FUSION_HOME` refers to the bundled data directory. Both examples pass `fusion --check`; full runs take hours and have not been executed for this guide.

### FuSION 1 batch files

FuSION 1 batch files are supported and reproduce FuSION 1's results, except where FuSION 2.0 corrects documented FuSION 1 defects. Two adjustments may be needed:
- **Unfilled template placeholders.** Batch files generated from FuSION 1's GUI template contain `%__a21__%`, `%__a22__%` and `%__a23__%` on `pathz3` lines, which FuSION 2.0 rejects. Remove `$COEXPRESSION_CSV $COEXP_THRESH $FROZEN_THRESH $NEW_CONSTRAINTS_ENABLED $Z3_RANDOM_SEED` from the end of each `pathz3` line, or supply the values. The template's seed, 42, is also FuSION 2.0's default.
- **Interactive plots.** `exec` lines that open gnuplot windows run whenever a display is available. On Linux, suppress them with `env -u DISPLAY -u WAYLAND_DISPLAY fusion -b <batch>`.

### Long-running analyses

Analyses of full KEGG networks can run for hours; FuSION 1's largest took about two days. Run them in the background, detached from the terminal:

```sh
nohup fusion -b my.batch > my.out 2>&1 < /dev/null &
```

On Linux, prefix the command with `setsid` to detach it from the terminal session entirely. Monitor progress with `tail -f my.out` or the run's log file.

## Output directory

Each run creates a new directory, `<label>_<YYYYMMDD-HHMMSS>`. By default it is placed next to the batch file and labelled with the batch file's name; `--root <dir>` and `--label <text>` override both. Run directories are never overwritten or deleted.

| Path | Contents |
|---|---|
| `index.html` | Run summary: status, Pareto plot, and frontier points linked to their explanation pages |
| `run.json` | Run manifest: versions, inputs with sizes and SHA-256 checksums, settings, outputs and status |
| `events.jsonl` | Progress events, one JSON object per line |
| `<run name>.log` | Run log |
| `run.lock` | Lock held while the run is active |
| `bindings.tsv` | Mapping of the source, target and tested gene IDs to network nodes |
| `inputs/` | Verified copies of all inputs, including the batch file |
| `cudf/` | Up- and down-regulated gene lists |
| `rgx/` or `mff/` | Loaded or merged network |
| `fb_rch/` | Corridor (`corridor.json`) and placement of the observed genes (`observations_match.tsv`) |
| `pathz3/` | Solver results (`results.json`), frontier data (`in.dat`, `out.dat`) and per-case details |
| `plot_pareto/` | Pareto plot (`pareto.png`) and its gnuplot script |
| `pex/` | Explanation pages (`explain/*.html`, with JSON views) and their JavaScript library |

## Run control

### Stopping a run

| Method | Effect |
|---|---|
| Ctrl-C, or `kill <pid>` (SIGTERM) | Graceful stop, exit code 3. A second Ctrl-C terminates immediately (exit code 130). |
| `fusion --stop <run directory>` | Graceful stop of an active run. Rejected, with exit code 1, if the directory is not an active run. |
| **Stop** / **Kill** in the application | Graceful stop / immediate termination of the run and its child processes |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Failure; the error message gives the cause |
| 2 | Usage error, for example a missing batch file |
| 3 | Stopped on request |
| 130 | Interrupted by a second Ctrl-C |

## Known limitations

- **Parameter sweeps:** `sweep` lines in batch files are not yet supported.
- **Resuming runs:** resuming or restarting a stopped run (`--resume`, `load run=...`, the application's **Resume** button) is not yet supported.
- **Solver options:** `pathz3`'s `points=` and `answers_from=` are not yet supported.
- **Platforms:** no native Windows build, and no installers. On macOS, `exec` lines that open gnuplot windows always run, except in SSH sessions.

Unsupported features fail with a message stating that they are planned for a later version.

## Testing

### Backend

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev -j 8
```

Every test should pass (`100% tests passed`). The suite includes 12 reference cases that compare FuSION 2.0 with FuSION 1's results on its toy networks.
- **Skipped tests:** a few tests are skipped unless the project's test-data directory (`FUSION_TEST_DATA`) is available. It is not distributed, because it contains KEGG data.
- **Slow tests:** these are excluded by default; run them with `ctest --test-dir build/dev -L slow`, which takes about 8 minutes.
- **macOS:** the backend test suite is not yet supported.

### Desktop application

```sh
cd frontend
./gradlew test
```

The suite takes about a minute. Its screen test renders the application's screens off-screen to `frontend/build/screens/`, and performs a toy run when the debug build exists. It requires a display, and is skipped without one.

## Configuration

| Variable or option | Used by | Purpose |
|---|---|---|
| `FUSION_HOME` | `fusion` | Data directory; defaults to `../share/fusion` relative to the executable |
| `FUSION_DOT` | `fusion` | Path to Graphviz `dot` |
| `DISPLAY`, `WAYLAND_DISPLAY` | `fusion` (Linux) | When neither is set, `exec` lines that open gnuplot windows are skipped |
| `--set <key>=<value>` | `fusion` | Overrides a run setting |
| `FUSION_DATA_DIR`, `FUSION_RESULTS_DIR` | Application | Data directory and default results directory |
| `-Pfusion.backend.executable=<path>` | `./gradlew run` | Backend executable used by the application |
| `FUSION_TEST_DATA` | Tests | Test-data directory |

## Troubleshooting

| Symptom | Resolution |
|---|---|
| `The system Z3 is <version>; the build expects 4.8.12` (Linux) | Install `libz3-dev` on Ubuntu 24.04. On other distributions, build Z3 4.8.12 and pass `-DCMAKE_PREFIX_PATH=<prefix>` (untested). |
| `Could not find FUSION_Z3_INCLUDE_DIR using the following files: z3++.h` | Install `libz3-dev` (Linux) or `z3` (Homebrew), then reconfigure. |
| `CMake was unable to find a build program corresponding to "Ninja"` | Install `ninja-build` (Linux) or `ninja` (Homebrew), then reconfigure. |
| `cmake --preset` cannot read `CMakePresets.json` | CMake 3.28 or later is required; Ubuntu 22.04 ships 3.22. |
| Compilation errors in `<format>` or other C++20 headers | GCC 13 or later is required; on macOS, update the Xcode Command Line Tools. |
| `fusion --version` reports gnuplot or the font as `not found` | Install gnuplot and the Liberation fonts (`fonts-liberation`, or the `font-liberation` cask). |
| `JAVA_HOME is not set and no 'java' command could be found` | Install JDK 21 (`openjdk-21-jdk`, or Homebrew's `openjdk@21` with `JAVA_HOME` set). |
| `java.lang.UnsupportedOperationException: Unable to open DISPLAY` | No graphical session, for example over SSH. Start the application from a desktop session; on Windows, see [WSL 2](#installation-on-windows-wsl-2). |
| `Set fusion.backend.executable, or FUSION_HOME, and start FuSION again` | The backend executable was not found. Build it, and check the `-Pfusion.backend.executable` path. |
| `FuSION is already running for <directory>` | An instance is already open for that data directory. |
| FuSION 1 batch: `%__a21__% does not exist` or `'%__a22__%' is not a number` | Remove the template placeholders; see [FuSION 1 batch files](#fusion-1-batch-files). |
| FuSION 1 batch opens gnuplot windows | On Linux, run it with `env -u DISPLAY -u WAYLAND_DISPLAY`. |
| `fusion --stop`: `is not a run directory` or `is not running` | The directory is not an active run, so there is nothing to stop. |
| `error: batch file <file> does not exist` (exit code 2) | Check the path; relative paths resolve against the current directory. |
