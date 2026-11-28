# Running FuSION 2.0

How to build FuSION 2.0 from source and run it, on Linux, Windows and macOS. For each step it gives the commands, what they should print, and what to do if they don't. This file is kept up to date: any change to the build, its dependencies, the `fusion` command line or how the app starts updates it too ([decision 0040](docs/decisions/0040-readme-and-running.md)).

**Last checked:** 28 November 2026, FuSION 0.1.0 at commit `2771118`, on Ubuntu 24.04.5 (x86-64), in a fresh clone of the repository.
- Every command-line step below was run in order.
- That machine already had the apt packages except CMake and Ninja, which were run at the versions apt installs (3.28.3 and 1.11.1).
- The app was started on a virtual display, where its screen test also ran the toy session through it. Nobody clicked through the app's steps by hand.
- The steps for the run page and `fusion --stop` were checked again after those were fixed, later the same day.
- The Windows and macOS routes have not been tried.

## Contents

- [Where each OS stands](#where-each-os-stands)
- [What you need](#what-you-need)
- [Linux](#linux)
- [Windows, through WSL 2](#windows-through-wsl-2)
- [macOS, through a virtual machine](#macos-through-a-virtual-machine)
- [A first run: the toy network](#a-first-run-the-toy-network)
- [The desktop app](#the-desktop-app)
- [Running your own data](#running-your-own-data)
- [What a run writes](#what-a-run-writes)
- [Stopping a run](#stopping-a-run)
- [Not available yet](#not-available-yet)
- [Tests](#tests)
- [Settings and environment variables](#settings-and-environment-variables)
- [Troubleshooting](#troubleshooting)

## Where each OS stands

| OS | How FuSION 2.0 runs there | Tested |
|---|---|---|
| Ubuntu 24.04, x86-64 | natively | yes: every step in this file |
| Other Linux distributions | natively, with the tools listed under [What you need](#what-you-need) | no |
| Windows 11, or Windows 10 build 19044 or later | the Linux build, inside WSL 2 with Ubuntu 24.04 | no |
| macOS | the Linux build, inside an Ubuntu 24.04 virtual machine | no |

There are no native Windows or macOS builds yet ([decision 0038](docs/decisions/0038-end-to-end-version-this-week.md)).
- **macOS:** the backend stops a compile with an `#error` until its run lock, which marks a run as live, is ported.
- **Windows:** the same `#error` stays until three pieces are written: the run lock, the Ctrl-C handling, and the Windows side of the backend's platform layer.

There are no installers yet either, so everyone builds from source.

## What you need

- **A 64-bit x86 computer.** On a Mac with Apple silicon, the virtual machine runs arm64 Linux, which no one has tried yet.
- **Ubuntu 24.04**, or another Linux with these tools:
  - GCC 13 or later (tested with 13.3);
  - CMake 3.28 or later (tested with 3.28.3 and 4.4.5), and Ninja (tested with 1.11.1 and 1.13.2);
  - Z3 4.8.12 with its C++ headers. It must be exactly this version: the build refuses any other until the Z3 version is decided again ([decision 0039](docs/decisions/0039-sprint-toolchain.md));
  - gnuplot (tested with 6.0) and the Liberation Sans font, for the plots;
  - a Java 21 JDK, for the app (tested with OpenJDK 21.0.13).
- **Disk space:**
  - about 40 MB for the source;
  - 46 MB for the release build;
  - 2.1 GB for the dev build, which includes the tests;
  - about 450 MB for Gradle and the app's libraries.
- **Internet on the first build.** CMake downloads the C++ libraries pugixml, nlohmann/json and fast_float, plus Catch2 for builds with tests, and checks each one's SHA-256. Gradle downloads itself and the app's libraries, and checks them against `frontend/gradle/verification-metadata.xml`.
- **For real runs, KGML files from KEGG.** FuSION ships none; see [Running your own data](#running-your-own-data). The toy network below needs none.

## Linux

These steps are for Ubuntu 24.04. Run them in a terminal.

### 1. Install the tools

```sh
sudo apt update
sudo apt install g++ git cmake ninja-build libz3-dev gnuplot fonts-liberation openjdk-21-jdk libgtk-3-0t64
```

| Package | What it is for | Version on Ubuntu 24.04 |
|---|---|---|
| `g++` | the C++ compiler | GCC 13.3 |
| `git` | getting the source | 2.43 |
| `cmake`, `ninja-build` | building the backend | 3.28.3, 1.11.1 |
| `libz3-dev` | the Z3 solver and its headers | 4.8.12 |
| `gnuplot` | the Pareto plot and the threshold histogram | 6.0 |
| `fonts-liberation` | Liberation Sans, the plots' font | 2.1.5 |
| `openjdk-21-jdk` | Java 21, for the app and its Gradle build | 21.0 |
| `libgtk-3-0t64` | GTK 3, which the app's windows (JavaFX) need. A desktop install already has it | 3.24 |

Graphviz (`sudo apt install graphviz`) is optional. FuSION runs its `dot` only when you ask for a static export, with `static=` on `psp` or `pex`.

### 2. Get the source

The GitHub repository is private, so ask its owner for access first. Then, with an SSH key added to your GitHub account:

```sh
git clone git@github.com:arjunjeewan/FuSION2.git
cd FuSION2
```

Without an SSH key, use `git clone https://github.com/arjunjeewan/FuSION2.git`; Git then asks for your user name and a GitHub personal access token.

Every command below runs from this `FuSION2` folder unless it says otherwise.

### 3. Build the backend

```sh
cmake --preset release
cmake --build --preset release
```

The first command sets up the build in `build/release`, downloading its libraries the first time. It should end like this:

```text
-- Z3 4.8.12 from the system: /usr/lib/x86_64-linux-gnu/libz3.so
-- Configuring done (2.7s)
-- Generating done (0.0s)
-- Build files have been written to: /home/you/FuSION2/build/release
```

The second command compiles, using every core (add `-j 4` to use four). It should end with `[111/111] Linking CXX executable stage/bin/fusion`, and print no warnings. On 8 cores it took 1 min 43 s.

`build/release/stage/` is laid out like an installation:
- `bin/fusion`, the program;
- `share/fusion/`, with the reference maps, the KEGG pathway list and the default pipeline.

`fusion` finds its files there by itself, so it works from any folder.

The repository has three build presets:

| Preset | Build | Tests | Folder | Use it for |
|---|---|---|---|---|
| `release` | optimised | no | `build/release` | running FuSION |
| `dev` | debug | yes | `build/dev` | the [tests](#tests) and development |
| `ci-linux` | optimised, warnings as errors | yes | `build/ci-linux` | what continuous integration builds |

### 4. Check the build

```sh
build/release/stage/bin/fusion --version
```

It should print:

```text
FuSION 0.1.0 (build 0.1.0+2771118fabf40da7566a7f73dce50a5e9df6052e)
  home      /home/you/FuSION2/build/release/stage/share/fusion
  dot  not found
  font:Liberation Sans   system
  gnuplot  6.0.0 PATH
  sh   system
  data      ok
```

Each line after the first is something `fusion` uses:
- `home`: its folder of shipped files;
- `dot`: Graphviz, which is optional;
- `font`: the plots' font;
- `gnuplot`: the plotting program;
- `sh`: the shell for `exec` lines;
- `data`: `ok` means the shipped files match their recorded checksums.

Anything other than `dot` reported as not found must be fixed before a run; see [Troubleshooting](#troubleshooting).

### 5. Put `fusion` on your PATH

So that you can type `fusion` from any folder in this terminal:

```sh
export PATH="$PWD/build/release/stage/bin:$PATH"
fusion --version
```

To keep this for new terminals, add the `export` line to `~/.bashrc`, with the full path in place of `$PWD`, for example `export PATH="$HOME/FuSION2/build/release/stage/bin:$PATH"`.

Then go on to [a first run](#a-first-run-the-toy-network), or start [the desktop app](#the-desktop-app).

## Windows, through WSL 2

**Untested.** There is no native Windows build yet. On Windows you run the Linux build inside WSL 2 (the Windows Subsystem for Linux), where the Linux steps work unchanged.

1. **Install WSL 2 with Ubuntu 24.04.** Open PowerShell as administrator and run:

   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

   Name Ubuntu 24.04 explicitly. A plain `wsl --install` now installs a newer Ubuntu, which FuSION 2.0 has not been tried on and whose Z3 may not be 4.8.12. Restart Windows if asked, then open **Ubuntu 24.04** from the Start menu and create your Linux user. Microsoft's guide covers the details: [Install WSL](https://learn.microsoft.com/en-us/windows/wsl/install).
2. **Follow the [Linux](#linux) steps** in the Ubuntu terminal.
   - Clone into your Linux home folder, for example `~/FuSION2`, not under `/mnt/c/`, because building and running from the Windows drive is much slower.
   - Keep your data inside Linux too.
3. **The app's window** needs WSLg, which comes with Windows 11 and with Windows 10 build 19044 or later. With it, Linux apps open as ordinary windows on the Windows desktop. If the app can't open a window, run `wsl --update` in PowerShell as administrator, then `wsl --shutdown`, and try again. See [Run Linux GUI apps with WSL](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps).
4. **Explanation pages and run folders.** The app opens them with Ubuntu's own browser and file manager, which a new WSL install doesn't have, so those buttons may do nothing (untested). Instead, you can open a run's files from Windows Explorer at `\\wsl$\Ubuntu-24.04\home\<your Linux user name>\FuSION\runs\`.

## macOS, through a virtual machine

**Untested.** There is no native macOS build yet. On a Mac you run the Linux build inside an Ubuntu 24.04 virtual machine.

1. **Create the virtual machine.**
   - Install a virtual-machine app, such as UTM (free), Parallels Desktop or VMware Fusion.
   - Create an Ubuntu 24.04 machine with a desktop, so the app has a screen to draw on. Use arm64 on a Mac with Apple silicon (M1 or later), and x86-64 on an Intel Mac.
   - Give it room for the build: the dev build alone takes 2.1 GB.
2. **Follow the [Linux](#linux) steps** inside the virtual machine.

On Apple silicon the virtual machine runs arm64 Linux, where FuSION 2.0 has never been built. The steps should work unchanged, but no one has tried them yet:
- Ubuntu 24.04's packages have the same versions on arm64.
- The app's build picks JavaFX's arm64 Linux libraries by itself.

## A first run: the toy network

The tests include FuSION 1's toy network: 22 genes, 25 edges and a fold-change file. This run finds signalling paths of at most 10 edges from `hsa1` to `hsa22`, and tests whether `hsa13` is needed. It takes about a second. It assumes you've done step 5, so that `fusion` is on your PATH.

1. **Copy the toy's two input files** into a folder of their own. From the `FuSION2` folder:

   ```sh
   mkdir -p ~/fusion-toy
   cp backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/with_node_included.tsv \
      backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/report_example_with_pathways.xml \
      ~/fusion-toy/
   ```

2. **Write the batch file:**

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

   Each line is one step:

   | Line | What it does |
   |---|---|
   | `cudf` | classifies the genes: a value of 0.9 or more is up-regulated, -0.9 or less down-regulated |
   | `rgx` | reads the network, a FuSION 1 network file |
   | `fb_rch` | keeps the corridor: the nodes and edges on paths of at most 10 edges from hsa1 to hsa22 |
   | `pathz3` | solves two cases with Z3: `in`, with hsa13 on a path, and `out`, with hsa13 left out. It allows up to 7 node and 7 edge relaxations, and 150 s per solver check |
   | `plot_pareto` | draws both cases' frontiers in one plot |
   | `pex` | writes an explanation page for each frontier point |
   | `exit` | ends the batch |

   A batch file reads relative paths from its own folder. `fusion -h` lists every command, and `fusion -h <command>`, such as `fusion -h pathz3`, lists that command's arguments and defaults.

3. **Check the batch, then run it:**

   ```sh
   cd ~/fusion-toy
   fusion --check -b toy.batch
   fusion -b toy.batch
   ```

   The check should print `toy.batch: the check found no error`. The run prints each step as it goes, and should end like this:

   ```text
   with FSC (AADAC): 4 Pareto points, all certified
     n=1 e=7 | n=2 e=5 | n=5 e=3 | n=7 e=2
   without FSC (AADAC): 4 Pareto points, all certified
     n=0 e=7 | n=1 e=5 | n=4 e=3 | n=7 e=1
   comparison: forcing AADAC in raises the frontier
   ...
   pex: wrote 8 pages
   exit
   -------------------------------
   Finished: ok, exit 0, 0.848 s wall, 0.847 s CPU, peak RSS 62521344 bytes
   ```

   These are the same frontiers FuSION 1 recorded for this toy.
   - `n` counts node relaxations and `e` edge relaxations. Each point is a best trade-off between the two: no explanation needs fewer of one without needing more of the other.
   - A point is *certified* when the solver proved that no explanation needs fewer of either. A point whose checks timed out stays uncertified.
   - FSC means the gene being tested. The toy reuses real gene ids, so the shipped symbol map shows `hsa13` as AADAC.

4. **Look at the results.** The run made a new folder named after the batch and the time, such as `toy_20261128-130638`. Open its `index.html`, the run page, in your browser, from your file manager or with `xdg-open`. It says how the run ended, shows the Pareto plot, and links each frontier point to its explanation page. [What a run writes](#what-a-run-writes) lists everything in the folder.

## The desktop app

### Starting it

Build the backend first ([Linux](#linux), step 3). Then, from the `FuSION2` folder:

```sh
cd frontend
./gradlew run -Pfusion.backend.executable="$(cd .. && pwd)/build/release/stage/bin/fusion"
```

- **The first start** downloads Gradle 9.9.1 and the app's libraries, about 450 MB, which can take a few minutes. Later starts are quicker.
- **`-Pfusion.backend.executable`** names the `fusion` the app runs. Without it, the app uses the dev build's, `build/dev/stage/bin/fusion`, so you can leave it out if you have built the `dev` preset.
- **The terminal** stays busy while the app is open. Quitting the app gives it back.

### First start: your account

The first screen is **Create a new account**. Give a user name, a display name if you like, and a password of 8 to 72 bytes, twice. Accounts belong to this computer and live in the app's data folder (below), so everyone who uses the computer can have their own. From then on, the app opens with **Sign in**.

### A session for the toy network

A session holds one analysis and its runs. Make one with **File > New session...**, or **New** under the sessions list, and name it, for example "Toy hsa1 to hsa22". Then fill in its **Inputs** tab:

| Field | Value |
|---|---|
| Log fold-change file | `backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/with_node_included.tsv`, in your clone |
| Up threshold | `0.9` |
| Down threshold | `-0.9` |
| Pathway source | `merged_graph` |
| Merged network file | `report_example_with_pathways.xml`, in the same folder |
| Source node | `hsa1` |
| Target node | `hsa22` |
| Node to test for significance | `hsa13` |
| Node relaxations, at most | `7` |
| Edge relaxations, at most | `7` |
| Per-query timeout | `150s` |

Leave everything else as it is; fields marked `*` are required.
- **Check** checks the inputs without running anything.
- **Submit** checks them and starts the run.

The **Runs** tab follows the run. When it has finished, its tabs show:
- **Log**;
- **Frontier**: the same eight points as [the command-line run](#a-first-run-the-toy-network);
- **Pareto plot**;
- **Explanation pages**: double-click one to open it in your browser.

**Open the run page** opens the run's `index.html` in your browser, and **Open folder** opens the run's folder. A run that was killed, or never started, has no run page, and the app says why.

### Where the app keeps things

| What | Default folder | Change it with |
|---|---|---|
| accounts, sessions and settings | `~/.local/share/fusion/frontend`, or `$XDG_DATA_HOME/fusion/frontend` | `FUSION_DATA_DIR` |
| run folders | `~/FuSION/runs`, or the session's **Results folder** | `FUSION_RESULTS_DIR` |

Only one copy of the app runs per data folder. Starting it again prints `FuSION is already running for <folder>; its window was asked to come forward.` and exits.

### A forgotten password

There is no email reset. Quit the app, then from the `frontend` folder:

```sh
./gradlew installDist
build/install/fusion-frontend/bin/fusion-frontend --reset-password <user name>
```

It asks for the new password twice. `./gradlew run` can't do this, because Gradle doesn't pass what you type on to the app.

## Running your own data

### What you need

- **A log fold-change file.** One gene per line, with no header: its KEGG gene id, such as `hsa1956`, then its log2 fold change, separated by spaces or a tab. On the command line, `cudf` can also read a table with a header, and map gene symbols to ids (`fusion -h cudf`).
- **KGML files** for the pathways to merge.
  - Keep them all in one folder, each named by its pathway id, such as `hsa04010.xml`.
  - FuSION ships none. Their use is governed by KEGG's licence. Academic users can download one with the [KEGG REST API](https://www.kegg.jp/kegg/rest/keggapi.html):

    ```sh
    mkdir -p ~/kgml
    curl -o ~/kgml/hsa04010.xml https://rest.kegg.jp/get/hsa04010/kgml
    ```

  - The shipped pathway list, `share/fusion/data/kegg_pathways.txt`, names FuSION 1's 167 pathways by id. The app and `mff` look for each one as `<id>.xml` in your KGML folder. If any is missing, the check before a run stops and names the count and the first missing file, for example `4 files listed in .../kegg_pathways.txt do not exist, the first /home/you/kgml/hsa04216.xml [input.list_member]`.
- **A source gene and a target gene**, by KEGG id, and optionally a gene to test.

### In the app

In a new session's **Inputs** tab:
- Give your fold-change file and the up and down thresholds. The thresholds have no defaults. The **Classification preview** shows how many genes they keep, and suggestions are offered.
- Set **Pathway source** to `kgml_list` and give your **KGML folder**. Keep the shipped **Pathway list**, or give your own.
- **Shortlist pathways with GSEA** is on by default and keeps the pathways at `fdr:0.25`.
- The relaxation bounds and the per-query timeout have no defaults either. The app offers 8, 8 and 4h.

### On the command line

Choose whichever fits:
- **Export the app's batch.** Set the run up in the app, then use **Export the batch...** on its **Pipeline** tab to save the batch it would run, and run that file with `fusion -b`.
- **Write a batch yourself**, like the toy's, with `mff` merging your KGML files in place of `rgx`. For example, EGFR (`hsa1956`) to CXCL8 (`hsa3576`), testing JUN (`hsa3725`):

  ```text
  cudf values=my_lfc.tsv up=1 down=-1
  mff list=$FUSION_HOME/data/kegg_pathways.txt split_threshold=1 kgml_dir=/home/you/kgml
  fb_rch source=hsa1956 target=hsa3576 bound=10 edges_to_target=any
  pathz3 fsc=hsa3725 node_max=8 edge_max=8 timeout=4h
  plot_pareto
  pex
  exit
  ```

  To merge only the pathways GSEA shortlists, as the app does by default, put a `gsea` line before `mff` and drop `mff`'s `list=`. `mff` then merges `gsea`'s shortlist:

  ```text
  gsea values=my_lfc.tsv candidates=$FUSION_HOME/data/kegg_pathways.txt selector=fdr:0.25 shortlist=gsea_kgml_files.txt kgml_dir=/home/you/kgml
  mff split_threshold=1 kgml_dir=/home/you/kgml
  ```

  `$FUSION_HOME` is the folder of shipped files. Both batches passed `fusion --check` against a folder of all 167 KGMLs; a run takes hours, so they were not run through. `fusion -h <command>` lists each command's arguments.
- **Run a FuSION 1 batch file.** Batches that worked in FuSION 1 run in 2.0 and give the same results, except where 2.0 fixes a recorded FuSION 1 bug. Two things to know:
  - FuSION 1's GUI template leaves `%__a21__%`, `%__a22__%` and `%__a23__%` on `pathz3` lines when its coexpression fields are empty, and 2.0 refuses them. Delete `$COEXPRESSION_CSV $COEXP_THRESH $FROZEN_THRESH $NEW_CONSTRAINTS_ENABLED $Z3_RANDOM_SEED` from the end of each `pathz3` line, or fill in those three values. Deleting them also drops the batch's Z3 seed. The template sets that seed to 42, which is 2.0's default too.
  - Their `exec ... gnuplot -p` lines open plot windows when the terminal has a display. Run them with `env -u DISPLAY -u WAYLAND_DISPLAY fusion -b old.batch` to skip those windows; `plot_pareto` draws the plot either way.

### Long runs

A real KEGG run can take hours, since each solver check may run up to its timeout; FuSION 1's largest runs took about two days. To keep a run going after you close the terminal or log out, start it like this:

```sh
setsid nohup fusion -b my.batch > my.out 2>&1 < /dev/null &
```

Follow it with `tail -f my.out`, or through the run folder's log, and stop it with `fusion --stop` ([Stopping a run](#stopping-a-run)).

## What a run writes

Each run gets a new folder, named `<label>_<YYYYMMDD-HHMMSS>`.
- **By default** it goes next to the batch file, labelled with the batch file's name, as in `toy_20261128-130638`.
- **`--root <folder>` and `--label <text>`** choose others: `fusion -b toy.batch --root ~/runs --label first-try` made `~/runs/first-try_20261128-130708`.

FuSION never deletes or reuses a run folder. The toy run's folder holds:

```text
toy_20261128-130638/
  index.html                  the run page: how the run ended, the Pareto plot, and each case's
                              points linked to their explanation pages
  run.json                    the run's record: versions, every input with its size and SHA-256,
                              the settings, each step's files and the run's status
  events.jsonl                progress, one JSON event per line (the app follows this)
  toy_20261128-130638.log     the run log: what the terminal showed, with times
  run.lock                    locked while the run is going
  bindings.tsv                how the source, target and tested gene ids matched the network
  inputs/                     checked copies of every input, the batch file included
  cudf/up_reg, down_reg       the up- and down-regulated genes
  rgx/network.xml             the network read
  fb_rch/corridor.json        the corridor
  fb_rch/observations_match.tsv   where each observed gene falls: in the corridor, outside it
                              or not in the network
  pathz3/results.json         the solve's results: every case's frontier, witnesses and comparison
  pathz3/in.dat, out.dat      each case's frontier, as gnuplot data
  pathz3/in/, out/            each case's details and search log
  plot_pareto/pareto.png      the Pareto plot, and pareto.gp, the gnuplot script that drew it
  pex/explain/in_n1_e7.html   one explanation page per frontier point, with its .json;
                              pex/lib/ holds the pages' drawing library
```

## Stopping a run

- **Ctrl-C** in the run's terminal, or `kill` with the process id, asks the run to stop: `fusion` records the run as stopped and exits with status 3. A second Ctrl-C ends it at once, with status 130.
- **From another terminal**, `fusion --stop <run folder>` asks the same, by writing a `stop` file into the run folder.
- **In the app**, **Stop** asks the same. **Kill** ends the run's process, and every process it started, at once.

`fusion`'s exit statuses:

| Status | Meaning |
|---|---|
| 0 | the batch ran to its end |
| 1 | a failure: the message says what failed |
| 2 | a usage error, such as a batch file that doesn't exist |
| 3 | stopped on request |
| 130 | ended by a second Ctrl-C |

## Not available yet

These stop with a message saying they come in a later version:
- **`sweep` lines in a batch file:** `sweep is not available in this build yet (01.C3, 01.C6) [batch.unknown_command]`.
- **Resuming or restarting a stopped run.** `--resume`, which the app's **Resume** button uses, stops with `restarting or resuming from a parent run (09.C5) comes in a later version of FuSION 2.0 [run.resume_unavailable]`. `load run=...` stops with a similar message.
- **`pathz3`'s `points=` and `answers_from=`,** and loading a saved solve.

These aren't built yet at all: **native Windows and macOS builds,** and installers.

## Tests

### The backend's tests

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev -j 8
```

- **The build:** the `dev` preset is a debug build with the tests: 357 build steps, which took 4 min 7 s on 8 cores.
- **The run:** `ctest --preset dev` runs every test except the slow ones, the benchmarks and T2. That is 598 tests, which took 23 s with `-j 8`, or about 2.5 minutes one at a time. It should end with:

  ```text
  100% tests passed, 0 tests failed out of 598
  ```

- **What they cover:** the 598 include the 12 toy reference cases. Each runs one of FuSION 1's toy batches through `fusion` and compares the results with FuSION 1's recorded ones.
- **Five tests are listed as skipped.** They need the project's test-data folder, named by `FUSION_TEST_DATA`, which isn't in the repository because it holds KEGG data. T2, the KEGG reference tier, needs that folder too.
- **The slow tests:** `ctest --test-dir build/dev -L slow` runs the 3 slow tests, which take about 8 minutes.

### The app's tests

```sh
cd frontend
./gradlew test
```

It runs 111 tests in about a minute and should end with `BUILD SUCCESSFUL`. One of them, the screen test, draws the app's screens into `frontend/build/screens/*.png`. When the dev build exists, it also does a real toy run through the app. It draws off-screen, so no window appears, but it needs a display and is skipped without one.

## Settings and environment variables

| Name | Read by | What it does |
|---|---|---|
| `FUSION_HOME` | `fusion` | the folder of shipped files; by default `../share/fusion`, counted from the folder `fusion` is in |
| `FUSION_DOT` | `fusion` | which Graphviz `dot` to use |
| `DISPLAY`, `WAYLAND_DISPLAY` | `fusion` | when either is set, `exec` lines that open gnuplot windows run; when neither is, they are skipped |
| `--set <key>=<value>` | `fusion` | sets a run setting, overriding the batch's `run` line |
| `FUSION_DATA_DIR`, `FUSION_RESULTS_DIR` | the app | its data folder and its default results folder |
| `-Pfusion.backend.executable=<path>` | `./gradlew run` | the `fusion` the app runs |
| `FUSION_TEST_DATA` | the tests | the test-data folder |

## Troubleshooting

**`cmake --preset release` stops with a message like `The system Z3 is 4.13.0; the build expects 4.8.12 (11.C3, decision 0039)`.**
Your Linux packages a different version of Z3. On Ubuntu 24.04, `sudo apt install libz3-dev` gives 4.8.12. Elsewhere, you can build Z3 4.8.12 from source and point CMake at it with `-DCMAKE_PREFIX_PATH=<its install folder>` (untested).

**It stops with `CMake was unable to find a build program corresponding to "Ninja"`.**
Ninja isn't installed. Run `sudo apt install ninja-build`, then `cmake --preset release` again.

**It stops with `Could not find FUSION_Z3_INCLUDE_DIR using the following files: z3++.h`.**
Z3's headers aren't installed. Run `sudo apt install libz3-dev`, then `cmake --preset release` again.

**`cmake --preset` can't read `CMakePresets.json`, or asks for a newer CMake.**
The build needs CMake 3.28 or later; Ubuntu 22.04, for example, has 3.22. Use Ubuntu 24.04, or install a newer CMake from [cmake.org](https://cmake.org/download/).

**The compiler stops with errors in `<format>` or other C++20 headers.**
The build needs GCC 13 or later. Check with `g++ --version`.

**`fusion --version` reports gnuplot or the font as not found.**
Install `gnuplot` or `fonts-liberation`. Only `dot` may be missing.

**`./gradlew` says `JAVA_HOME is not set and no 'java' command could be found in your PATH`.**
Install `openjdk-21-jdk`.

**The app stops with `java.lang.UnsupportedOperationException: Unable to open DISPLAY`.**
There is no screen to draw on, as in an SSH session or on a server. Start the app from a desktop session; on Windows, see [WSL 2](#windows-through-wsl-2), step 3.

**The app says `Set fusion.backend.executable, or FUSION_HOME, and start FuSION again`.**
It can't find `fusion`. Build the backend, check the path given to `-Pfusion.backend.executable`, and start the app again.

**Starting the app prints `FuSION is already running for <folder>`.**
A copy is already open for that data folder: use its window.

**A FuSION 1 batch fails its check with `coexpression_csv file .../%__a21__% does not exist [input.missing]` and `coexpression_threshold '%__a22__%' is not a number [batch.bad_value]`.**
These are the GUI template's unfilled placeholders; see [On the command line](#on-the-command-line).

**A FuSION 1 batch opens gnuplot windows.**
Run it with `env -u DISPLAY -u WAYLAND_DISPLAY`. Its log then says `exec: no display is available, so this gnuplot window is skipped; plot_pareto draws the plot (01.C20)`.

**`fusion -b` prints `error: batch file my.batch does not exist` and exits with status 2.**
Check the path. A relative batch path is read from the current folder.
