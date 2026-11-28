# FuSION 2.0

FuSION identifies signalling paths between a source gene and a target gene that are consistent with gene-expression data. It merges KEGG pathways into one network, restricts the network to paths of bounded length between the two genes, and uses the Z3 SMT solver to compute the Pareto frontier of node and edge relaxations: the smallest departures from the observed expression that a path explanation requires. FuSION 2.0 is a rewrite of [FuSION 1](https://github.com/supratik1/FuSION), with a C++20 backend and a JavaFX desktop application.

## Features

- Batch-driven analysis pipeline through the `fusion` command-line tool, compatible with FuSION 1 batch files
- Differential-expression classification, and GSEA pathway shortlisting (a bit-exact port of gseapy)
- KGML pathway merging and computation of the reachability corridor
- Pareto-optimal path explanations computed with Z3, with significance testing of a chosen gene
- Pareto plots, interactive explanation pages and a summary page for each run
- Desktop application (JavaFX, Spring Boot) for configuring, running and inspecting analyses

## Status

Version 0.1.0, in active development. Reference testing against FuSION 1 is in progress: on the 12 synthetic reference cases, every Pareto frontier matches FuSION 1's except one, which a documented correction explains.

- **Platforms:** Linux, verified on Ubuntu 24.04. macOS through a native build with Homebrew, not yet verified. Windows through WSL 2, untested.
- **Distribution:** build from source; installers are not yet available.
- **Limitations:** parameter sweeps, resuming stopped runs and some solver options are not yet supported; see [Known limitations](RUNNING.md#known-limitations).

## Quick start (Ubuntu 24.04)

```sh
sudo apt install g++ git cmake ninja-build libz3-dev gnuplot fonts-liberation openjdk-21-jdk libgtk-3-0t64
git clone git@github.com:arjunjeewan/FuSION2.git
cd FuSION2
cmake --preset release
cmake --build --preset release
build/release/stage/bin/fusion --version    # the report ends with "data ok"
```

Launch the desktop application:

```sh
cd frontend
./gradlew run -Pfusion.backend.executable="$(cd .. && pwd)/build/release/stage/bin/fusion"
```

For macOS, Windows, a worked example and troubleshooting, see [RUNNING.md](RUNNING.md).

## Documentation

- [RUNNING.md](RUNNING.md): installation and usage guide
- [docs/spec/](docs/spec/): specification of FuSION 1's behaviour, with the corrections made in FuSION 2.0
- [docs/architecture/](docs/architecture/): architecture of FuSION 2.0, including its [build](docs/architecture/11-build.md) and [testing](docs/architecture/12-tests.md)
- [docs/decisions/](docs/decisions/README.md): design decision records
- [docs/process/PROMPTS.md](docs/process/PROMPTS.md): development log

## Licence

MIT ([LICENSE](LICENSE)), as for FuSION 1. KEGG data is not distributed: KGML files must be obtained separately, subject to KEGG's licence terms.
