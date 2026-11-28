# FuSION 2.0

FuSION finds signalling paths from a source gene to a target gene through a network merged from KEGG pathways, and checks them against a gene-expression experiment. It classifies the genes of a log fold-change file as up- or down-regulated, and can shortlist the pathways with GSEA. It then merges the pathways' KGML files into one network and keeps the corridor: the paths of bounded length from the source to the target. The Z3 solver searches the corridor for paths that agree with the observations, and FuSION reports the Pareto frontier between node relaxations and edge relaxations, the two ways an explanation can depart from the data. Given a gene to test, FuSION compares the frontier with that gene forced into the paths and with it kept out. Each run ends with a Pareto plot and an explanation page for each frontier point.

FuSION 2.0 rewrites [FuSION 1](https://github.com/supratik1/FuSION) in two parts:
- **The backend** is a C++20 library and the `fusion` command-line program, which runs batch files.
- **The desktop app** is a JavaFX application on Spring Boot. It builds a batch from the default pipeline and runs it with `fusion`.

FuSION 1's batch files keep working: a batch that ran correctly in FuSION 1 gives the same results in 2.0, except where 2.0 fixes a recorded FuSION 1 bug ([decision 0017](docs/decisions/0017-batch-file-compatibility.md), [decision 0023](docs/decisions/0023-results-may-change-where-improved.md)). Batches made from FuSION 1's GUI template may need one edit first, which [RUNNING.md](RUNNING.md#on-the-command-line) describes.

## Status

Version 0.1.0, the first end-to-end version ([decision 0038](docs/decisions/0038-end-to-end-version-this-week.md)), runs on Linux. The work is in Phase 3, the reference tests, which check 2.0 against FuSION 1's recorded results ([plan](docs/phase3/plan.md)).

What works:
- `fusion` runs a batch end to end: reading KGML and FuSION network files, the `mff` merge, `cudf`, GSEA (a bit-exact port of gseapy's), `fb_rch`, `pathz3` on Z3 with the `in`, `out` and free cases, the Pareto plot, the explanation pages, and a run page that sums up each run.
- In the app you create an account and sessions, enter a run's inputs and check them, then run the default pipeline. You can follow the run's progress and see its frontier, Pareto plot and explanation pages.
- On the 12 synthetic reference cases, every frontier matches FuSION 1's except one, which a recorded fix explains.

Not available yet:
- **Native Windows and macOS builds.** On those systems, run the Linux build in WSL 2 or a virtual machine. [RUNNING.md](RUNNING.md) gives both routes; neither has been tested yet.
- **Installers.** Everyone builds from source for now.
- **Some batch features:** `sweep` lines, resuming a stopped run (`--resume`, and the app's Resume button), and `pathz3`'s `points=` and `answers_from=`. Each of these stops with a message saying it comes in a later version.

## Quick start (Ubuntu 24.04)

```sh
sudo apt install g++ git cmake ninja-build libz3-dev gnuplot fonts-liberation openjdk-21-jdk libgtk-3-0t64
git clone git@github.com:arjunjeewan/FuSION2.git
cd FuSION2
cmake --preset release
cmake --build --preset release
build/release/stage/bin/fusion --version
```

`fusion --version` should end with `data      ok`. To start the desktop app:

```sh
cd frontend
./gradlew run -Pfusion.backend.executable="$(cd .. && pwd)/build/release/stage/bin/fusion"
```

[RUNNING.md](RUNNING.md) explains each step, what it should print and what to do if it fails. It also has a first run on a toy network, running your own data, the tests and troubleshooting.

## Repository layout

```text
backend/
  modules/<module>/   the ten libraries: core, network, merge, expression, corridor, solve,
                      results, enrichment, run, batch
  cli/main.cpp        the fusion program
  tests/              unit tests, the test kit, fusion-test and the reference cases
frontend/             the desktop app: one Gradle project
share/fusion/         installed with fusion: the reference maps, the KEGG pathway list and
                      the default pipeline
cmake/                the build's modules and every pinned C++ dependency
docs/                 the specification, the architecture, decisions and the prompt log
CMakePresets.json     the build presets: dev, release, ci-linux
VERSION               one version for the backend and the app
```

## Documentation

- [RUNNING.md](RUNNING.md): building and running FuSION 2.0 on each OS.
- [docs/spec/](docs/spec/): FuSION 1's behaviour, stage by stage, with the fixes 2.0 makes.
- [docs/architecture/](docs/architecture/): FuSION 2.0's design, one section per module, with the build in [section 11](docs/architecture/11-build.md) and the tests in [section 12](docs/architecture/12-tests.md).
- [docs/decisions/](docs/decisions/README.md): every project decision, with the poll it came from.
- [docs/phase3/](docs/phase3/plan.md): the reference tests' plan and notes.
- [docs/process/PROMPTS.md](docs/process/PROMPTS.md): the project's prompt log.

## Licence

FuSION 2.0 is released under the MIT licence ([LICENSE](LICENSE)), as FuSION 1 is. FuSION ships no KEGG data: you need your own KGML files for real runs ([RUNNING.md](RUNNING.md#running-your-own-data)).
