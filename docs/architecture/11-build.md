# 11 · Build, dependencies and platforms

## Scope

This section has no module of its own (Choice C1). It covers how the whole repository is laid out, built, checked in continuous integration and shipped:

- the repository layout;
- the CMake build of `libfusion` and `fusion`, and the frontend's Gradle build;
- the C++ dependencies, how they are pinned and fetched, and which Z3 version is pinned;
- compilers, flags and the usable C++20 subset on each platform;
- one installer per platform that holds the backend, Z3, gnuplot, the plot font and a Java runtime (G3, F1), plus a backend-only archive for servers;
- how the installed backend finds its own files (S1.20) and checks its external dependencies before a run (S6.11);
- the versions recorded in `run.json` (E1, S7.02), and reproducible builds;
- continuous integration once the user pushes the repository (decision 0006);
- the licences of everything shipped.

It puts four small components inside `core` that every module and the `fusion` executable use: build information, the install directory, the platform layer (executable path, child processes without a shell, CPU time) and the dependency registry. C19 places them there, amending C1's description of `core` (accepted in prompt log entry 204). Beside them sit the small gnuplot runner and the plot palette that section 04's histogram and section 07's Pareto plot share (cross-section item 2B.13, entry 158).

It leaves to other sections:
- the `run.json` and `events.jsonl` schemas, the error codes' conventions and the stop request (09);
- the `fusion` command line, its exit statuses and the shell that `exec` uses (01; settled by 01.C10 and 01.C14);
- the Pareto plot step `plot_pareto`, what a missing font does, the Graphviz export on request and the explanation pages (07);
- the JavaFX application's screens, accounts and sessions (10);
- test content, tiers, fixtures and the reference-test runner (12);
- GSEA, including its generator, an exact port of gseapy's that `core` holds beside section 09's `core::Rng` (08.C1 and cross-section item 1.19, entry 159).

Names and evidence used below:
- "This machine" is the workstation that made FuSION 1's runs: Ubuntu 24.04.5, 36 logical CPUs, GCC 13.3.0, OpenJDK 21.0.13, gnuplot 6.0.0 (`gnuplot-qt`), Z3 4.8.12 (Ubuntu package `4.8.12-3.1build1`), Liberation fonts 2.1.5. CMake, Ninja, Clang and Gradle are not installed.
- "The Z3 comparison" ran two Z3 builds already on this machine on real SMT dumps: the system `z3` 4.8.12 and the official 4.15.3 release binary in `Desktop/arjun/misc/z3/z3-4.15.3-x64-glibc-2.39/`. The dumps come from the reference run (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`, 15 dumps, all run) and fdr050 (`runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr050/`, 32 dumps, 14 run).
  - Each dump was prefixed with `(set-option :random-seed 42)` and `(set-logic QF_BV)`, the logic of FuSION 1's solver (`Linux/src/main.cc:4721-4722`), and followed by `(get-model)`.
  - Z3 then solves a lone `check-sat` with its non-incremental QF_BV tactic. A second set of runs, on 13 of the reference run's 15 dumps, added `(push 1)` first, which makes Z3 use its incremental solver instead. FuSION 1's checks use both: each runs inside `push` and `pop`, on the incremental solver up to the first timeout and then on the tactic up to the cap (section 06).
  - Each run was solved from scratch, with a hard cap of 600 s, four to six at a time at `nice 10`, while other FuSION 1 jobs ran on the same machine (`ps` showed 11 `fusion -b` processes when the runs began).
  - Two SAT models count as equal when they set the same `__node_*` and `__edge_*` variables true.
  - In-process times are not comparable: FuSION 1's solver keeps what earlier checks learnt. Only two runs of the same dump in the same mode are compared.
  - Z3 5.x is not on this machine and was not run.
- Upstream versions, dates and SHA-256 digests were read from each project's release page or API on 2026-11-12.
- Nothing was built or installed, and FuSION 1 was not run.

## Requirements carried in

Skeleton:
- **G1:** C++20 built with CMake; Z3 through its C++ API at a pinned version recorded in the manifest; pugixml, replacing libxml++ 2.6 and its glibmm chain; nlohmann/json; Catch2. The small libraries are fetched at pinned versions by the build. Z3 comes from the system or a pinned download (read by C3, entry 200). The compiler minimums are GCC 13 and Clang 17, Apple Clang on macOS, since C22 amended G1's GCC 12 and Clang 15 (entry 200).
- **G2:** Java 21 LTS, Spring Boot and Gradle. C21 amended G2's Spring Boot 3 to Spring Boot 4.1 (entry 200).
- **G3:** Linux, macOS and Windows, all native, from one source tree. A biologist installs one thing, with gnuplot and Z3 included.
- **F1:** the backend is all C++, with no Python. gnuplot stays an external tool for the Pareto image and is checked before the run like every other dependency. 08.C1 amended F1: the C++ GSEA reproduces gseapy 1.3.1 bit for bit (entry 159).
- **B1:** `libfusion` holds all behaviour and never writes to the console. The thin `fusion` executable reads batch files. Phase 3 calls the library directly and also runs `fusion` end to end.
- **C1:** ten backend modules and five frontend modules. C19 amended the description of `core` (entry 204).
- **E1:** `run.json` records FuSION's version and the versions of its libraries, Z3 included, and every input's path, size and SHA-256.
- **A1, A2:** one `fusion` child process per run; Stop is a platform-neutral request; progress goes to `events.jsonl`. Signal handlers that only make the same request are a reading of A1, not an amendment (09.C8 and C18, entry 185).

Spec items:
- **S1.20:** the install directory is worked out once, at startup, resolving symlinks the same way on every OS, and used for every bundled helper.
- **S6.11:** every helper's dependencies are listed in one place and checked before the first step runs.
- **S7.02:** whatever runs GSEA pins its library versions, records them with each result and gets its arguments without a shell. F1 made GSEA C++, so the pins are libfusion's own.
- **S6.10:** a helper gets each value as its own argument, without a shell.
- **S6.23:** the Pareto plot keeps `plot_po.sh`'s look (pngcairo, Liberation Sans), and the font is checked with the other dependencies.
- **S6.18:** the explanation page's drawing library ships once per output directory or site.
- **S1.24:** FuSION 1 baked its build path into the binary (`CWD.h`) and refused to start when moved. The component is dropped; 2.0 must run from wherever it is installed.
- **S1.29:** no compile-time flag changes results. A flag that did becomes a run setting recorded with the run, and debug output sits behind one option. FuSION 1 builds without optimisation.
- **S1.27:** console numbers are printed with enough digits to be exact.
- **S7.05:** ties are ordered by gene id, so every machine gives the same ranking, scores and shortlist.
- **S8.01:** the frontend finds the backend and its data through configuration, not through its working directory. No compiled classes are kept in the repository.
- **S8.31:** no KGMLs are shipped. The reference maps ship with their source and date. The KGML list resolves against the user's own KGML directory. Compiled classes, sessions, accounts and generated scripts stay out of the repository.
- **S8.13, S8.06:** the two reference maps are shipped and used by default from a checked location, with choosers for other symbol and pathway-name maps; a new session's defaults include the shipped maps.
- **S1.21, S1.22, S1.23:** `config.txt`, the encoding tables and the colour files are dropped, so nothing ships them.

Decisions:
- **0001, 0006:** one repository at `Desktop/arjun/FuSION2`, plus the private GitHub repository `arjunjeewan/FuSION2` with an HTTPS `origin`. The user runs every push, and no GitHub credentials are stored on this machine.
- **0005:** `CLAUDE.md` stays at the repository root.
- **0015:** FuSION 2.0 is MIT-licensed. FuSION 1's notice travels with any file copied from it.
- **0021:** everything a run needs is checked before its first step, and the frontend validates the same cases.
- **0023:** results may change where 2.0 improves on FuSION 1; Phase 3 records each expected difference.
- **0004:** only FuSION 1's live functionality is ported.

Settled in the architecture polls (prompt log entries 158 to 210):
- **10.C1 (entry 192):** the frontend is a JavaFX desktop application, a standalone window with Java screens and native file dialogs, and no local server port. Spring Boot stays as the application framework, with no web server. 2.0 has no lab-server mode.
- **10.C4 (entry 197):** a normal application with one main window, a Dock or taskbar entry and Quit; one instance per data directory through `frontend.lock`, a second launch handing focus over through `frontend.focus`.
- **10.C6 (entry 198):** section 07's pages open in the system browser; there is no WebView and no `javafx.web`.
- **10.C12 (entry 195):** the default pipeline is a JSON document kept and tested with the backend and installed with it; this section writes `standard.batch` from it.
- **01.C10 (entry 188):** `exec` always means a POSIX shell: `/bin/sh` on Linux and macOS, the bundled busybox-w32 on Windows.
- **01.C14 (entry 189):** the command line keeps `-b` and `-h` and adds 16 flags, `--version` and `--set` among them (the count corrected in entry 230); exit statuses are 0 (ran to its end), 1 (failure), 2 (usage error), 3 (stopped) and 130 (a second Ctrl-C).
- **01.C18 (entry 189):** four predefined variables, `$FUSION_RUN_DIR`, `$FUSION_BATCH_DIR`, `$FUSION_POINT` and `$FUSION_HOME`, hold paths with `/` on every platform.
- **09.C14 (entry 186):** a failure is a `core::Failure` with a stable code; a reported result is part of `core::Outcome<T>`; a stop is `core::Stopped`. Section 09's names are the one `core` vocabulary (cross-section item 1.1, entry 158).
- **1.3 (entry 190):** events have dotted names under the key `type`, in the envelope `v`, `seq`, `t`, `type`; warnings go through `warning`, failures through 09's `error` event.
- **1.17, 2A.1 and 2B.16 (entry 158):** generic conditions take generic codes; this section owns the external-tool check and `deps.missing`, and the check's font warning `deps.font_missing` (entry 230); install and dependency errors go through 09's `error` event and exit with status 1.
- **02 open question 6 (entry 164):** nothing in 2.0 fetches KGMLs; the user guide says how to get them, and the check before a run names every listed pathway missing from the user's folder.

## Design

### Platforms and toolchains

The targets (C1), compilers (C8), Z3 sources (C3), gnuplot sources (C9) and packages (C12), all accepted (entries 200 to 202):

| Target | Oldest OS | Release compiler | Z3 5.3.3 from | gnuplot 6.0.5 from | Shipped as |
|---|---|---|---|---|---|
| Linux x86-64 | glibc 2.28: RHEL, Rocky and Alma 8, Debian 10, Ubuntu 20.04 | GCC 14 (`gcc-toolset-14`) in the `manylinux_2_28` image | source archive, built in that image | source, built in that image, libraries collected | `.deb`, `.rpm`, backend `.tar.gz` |
| macOS arm64 | 13.3 | Apple Clang, Xcode 16.4 or later | `z3-5.3.3-arm64-osx-13.3.zip` | source, built against Homebrew's cairo and pango, dylibs collected | `.dmg`, backend `.tar.gz` |
| macOS x86-64 | 13.3 | the same, on GitHub's `macos-15-intel` runner | `z3-5.3.3-x64-osx-13.3.zip` | the same | `.dmg`, backend `.tar.gz` |
| Windows x86-64 | 10 22H2, 11 | MSVC 19.44 (Visual Studio 2022 17.14) | `z3-5.3.3-x64-win.zip` | official `gp605-win64-mingw.7z` | per-user `.msi`, backend `.zip` |

On Windows the bundle also holds busybox-w32, the POSIX shell of `exec` lines (01.C10). Graphviz is not bundled anywhere: section 07 runs it only on request, for the static export (07.C5, accepted in entry 178).

Windows on Arm runs the x86-64 build under emulation. Linux arm64 and native Windows arm64 are not built at first, although Z3 (`arm64-glibc-2.38`, `arm64-win`) and gnuplot (`gp605-woa.exe`) provide them. Developers also build on this machine with GCC 13.3, and with any Clang 17 or later (C22). Phase 4 needs CMake and Ninja installed here; Ubuntu 24.04's own packages (CMake 3.28.3, Ninja 1.11.1) are new enough, and Gradle arrives through its wrapper.

### Pinned versions

Every artefact either build downloads is pinned by version and SHA-256 in one place per build (C4): `cmake/FusionVersions.cmake` for C++ and for the bundled tools and fonts, and `frontend/gradle/libs.versions.toml` with `frontend/gradle/verification-metadata.xml` for Java. Dates are release dates.

| Component | Pin | Released | Licence | Used by | Arrives as |
|---|---|---|---|---|---|
| CMake | 3.28 or later; CI uses 4.4.5 | 2026-10-27 | BSD-3-Clause | build | tool |
| Ninja | 1.13.2 | 2025-11-20 | Apache-2.0 | build | tool |
| Z3 | 5.3.3 (C2) | 2026-10-29 | MIT | `solve` | release archive or source archive (C3) |
| pugixml | 1.16 | 2026-06-16 | MIT | `network` | FetchContent |
| nlohmann/json | 3.12.0 | 2025-04-11 | MIT | `run`, `results`, `core` | FetchContent |
| fast_float | 8.3.3 (C5) | 2026-10-31 | MIT, Apache-2.0 or BSL-1.0 | `core::parse_decimal`, for `expression`, `batch` and `network` | FetchContent |
| Catch2 | 3.16.1 | 2026-10-08 | BSL-1.0 | tests only | FetchContent |
| gnuplot | 6.0.5 (C9) | 2026-08-04 | gnuplot licence | Pareto plot (07), threshold histogram (04.C7) | bundled |
| Liberation Sans | 2.1.5, Regular and Bold (C10) | 2021 | OFL-1.1 | gnuplot plots | fetched at packaging, bundled |
| vis-network | 10.1.5 (07.C3; 9.1.2 if Phase 3 sees a visible change, 07 open question 7) | not checked | Apache-2.0 or MIT | explanation pages | compiled into libfusion (C11) |
| busybox-w32 | newest tag `FRP-6354-g5f09fb446` (01.C10) | not checked | GPL-2.0 | `exec` on Windows | bundled on Windows |
| Java runtime | Eclipse Temurin 21.0.13+9 | October 2026, OpenJDK's 21.0.13 update (this machine's Ubuntu build is dated 2026-10-24) | GPL-2.0 with Classpath Exception | frontend | jlink image from the pinned JDK (C13) |
| OpenJFX | 21.0.13 (C24) | 2026-10-20 | GPL-2.0 with Classpath Exception | frontend | jmods from Gluon for the image, jars from Maven Central for compiling (C25) |
| Spring Boot | 4.1.2 (C21) | 2026-10-22 | Apache-2.0 | frontend, without a web server | Gradle |
| Gradle | 9.9.1 (C13) | 2026-11-10 | Apache-2.0 | frontend build | wrapper |

Notes:
- Spring Boot 4.1 replaces G2's Spring Boot 3 (C21 amends G2; entry 200): 3.5's free support ended on 2026-06-30, and 4.1's runs to 2027-07-31. Spring Boot 4.1.2 runs on Java 17 to 27 and supports Gradle 8.14 or later and 9.x, so the wrapper is 9.9.1.
- gnuplot 6.0.6 (2026-10-26) exists, but only as source; 6.0.5 is the newest release with official Windows binaries.
- The Z3 archives carry SHA-256 digests on the release page. For 5.3.3: source `6cc8c5d6…11aa`, `x64-win` `4e48624a…d798`, `arm64-osx-13.3` `6957e6c7…3c6b`, `x64-osx-13.3` `ace69787…f8e7`.

### Repository layout

The layout (C14, accepted with 10.C12's place in entry 202):

```text
FuSION2/
  CLAUDE.md  LICENSE  README.md  RUNNING.md  VERSION
  .gitignore  .gitattributes  .editorconfig  .clang-format
  CMakeLists.txt  CMakePresets.json
  cmake/
    FusionVersions.cmake       every C++ pin: version, URL, SHA-256
    FusionDependencies.cmake   FetchContent and the Z3 provider
    FusionFlags.cmake          compiler flags (C7)
    FusionAssets.cmake         assets compiled into libfusion (C11)
    FusionInstall.cmake        install rules and the staging tree
    build_info.cpp.in          version, commit and toolchain (BuildInfo)
    FetchZ3.cmake              fills the dependency prefix (C3)
  backend/
    modules/<module>/include/fusion/<module>/*.hpp   public headers
    modules/<module>/src/*.cpp                       private sources
    modules/<module>/assets/                         files compiled in, e.g. results' vis-network (C11)
    cli/main.cpp                                     the fusion executable
    tests/                                           section 12
  frontend/                    Gradle project: settings.gradle.kts, build.gradle.kts,
                               gradlew, gradlew.bat, gradle/, src/main/java, src/test/java
  share/fusion/                installed data, copied into FUSION_HOME
    data/                      the two reference maps, kegg_pathways.txt and SOURCES.md (S8.31)
    pipelines/standard.json    the default pipeline (section 10)
    fonts/fonts.conf           the font files themselves are fetched at packaging (C10)
  packaging/                   jpackage resources per platform, gnuplot recipes,
                               the notices generator
  third_party/                 licence texts of shipped components; no third-party source
  .github/workflows/           CI (C15)
  docs/                        spec, decisions, architecture, process (as today)
```

Rules:
- Nothing generated is tracked: build trees (`build/`, `out/`), `.gradle/`, `*.class`, jars other than the Gradle wrapper's, run directories, accounts, sessions and generated batch files (S8.01, S8.31). FuSION 1 tracks 67 class files and 3 jars (`git ls-files`).
- `gradle/wrapper/gradle-wrapper.jar` is the one tracked binary. CI checks it against Gradle's published checksums. Third-party binaries, the fonts included, are fetched by pinned SHA-256, never committed.
- No KGML is stored anywhere in the repository (S8.31). Tests that need KGMLs read them from the data root that `FUSION_TEST_DATA` names, outside the repository (12.C3).
- `.gitattributes`: `* text=auto eol=lf`; `*.bat` and `*.cmd` get `eol=crlf`; test fixtures that must keep CRLF are marked `-text`, because real inputs can have CRLF lines: the LFC file `runs/real_experiments/final_tnfa_p38i_combined.tsv` does.

### CMake structure

One CMake project builds the backend. Its targets (C6):

| Target | Kind | Links | Notes |
|---|---|---|---|
| `fusion_flags` | INTERFACE | none | C++20, warnings, floating-point and reproducibility flags (C7) |
| `fusion_core` | STATIC | PRIVATE `nlohmann_json`, `fast_float` | build info, install directory, platform layer, dependency registry (C19); the gnuplot runner and plot palette (2B.13); section 09's vocabulary, settings registry, events, errors and `Rng`; 08.C1's GSEA generator (1.19); section 04's `parse_decimal`, the one decimal parser (C5, 3A.19) |
| `fusion_network` | STATIC | PUBLIC `core`; PRIVATE `pugixml` | the only module that sees pugixml |
| `fusion_merge` | STATIC | PUBLIC `network`, `expression` | |
| `fusion_expression` | STATIC | PUBLIC `core`, `network` | |
| `fusion_corridor` | STATIC | PUBLIC `network`, `expression` | |
| `fusion_solve` | STATIC | PUBLIC `corridor`; PRIVATE `z3::libz3` | the only module that sees `z3++.h`; also reports and checks the linked Z3's version |
| `fusion_results` | STATIC | PUBLIC `solve`; PRIVATE `nlohmann_json` | |
| `fusion_enrichment` | STATIC | PUBLIC `network`, `expression` | |
| `fusion_run` | STATIC | PUBLIC `core`; PRIVATE `nlohmann_json` | |
| `fusion_batch` | STATIC | PUBLIC every step module, `run` | dispatches commands; holds every command handler, `cudf`'s included; calls `publish_solve` after the solve step |
| `fusion::libfusion` | INTERFACE | the ten module libraries | what `fusion` and the tests link |
| `fusion` | EXECUTABLE | `fusion::libfusion` | `backend/cli/main.cpp` only (B1) |

- These edges are the ones accepted with C6 and cross-section item 1.2 (entry 201): `expression` links `network` and `merge` links `expression`. Section 04's matcher takes a `network::GraphView`, which section 05 passes as `corridor.view()`; `results` depends on `solve`; and `batch` calls `results::publish_solve` after the solve step. No module sees `run` types (09).
- Each module has its own include directory, so a module can include only the headers of modules it links. An include that breaks the layering fails to compile.
- Z3 stays inside `solve`. On this machine, parsing `#include <z3++.h>` alone takes 0.84 s and 125 MB per translation unit with 4.8.12's header (3,962 lines), and 0.96 s and 134 MB with 4.15.3's (4,601 lines): the median of five runs of `g++ -std=c++20 -fsyntax-only` at a load average of 14. Kept out of the other modules' sources, that cost is paid only by `solve`'s few files.
- Because only `solve` links Z3, the Z3 version check and Z3's entry in `run.json` come from `solve`, and the `fusion` executable calls them at startup. `core` knows every library's version only as text written at configure time from `cmake/FusionVersions.cmake`, so it needs no library's header to report versions. Its only third-party headers are nlohmann/json's and fast_float's, both private.
- `nlohmann/json.hpp` is included only in `.cpp` files that read or write JSON, and every module that does links `nlohmann_json` privately (the table shows those this section knows of; each module's section adds its own). Public headers expose no JSON type; a module whose header had to would link it publicly.
- Options: `FUSION_BUILD_TESTS` (default ON), `FUSION_Z3_PROVIDER` (`PINNED` by default, or `SYSTEM`; C3), `FUSION_WARNINGS_AS_ERRORS` (ON in CI), `FUSION_SANITIZERS` (`address;undefined` or `thread`, Linux and macOS only), `FUSION_LTO` (ON in release presets). None of them changes results (S1.29).
- `CMakePresets.json` holds `dev` (Debug, tests on, sanitizers on Linux and macOS), `release` (Release, LTO, tests off) and one `ci-*` preset per platform. The generator is Ninja everywhere.
- The build writes a staging tree, `<build>/stage/{bin,lib,share/fusion}`, laid out exactly like an installation. A `fusion` run from the build tree finds its files the same way an installed one does.
- `cmake --install` copies the staging tree to the prefix. Run paths: `$ORIGIN/../lib` on Linux and `@loader_path/../lib` on macOS. On Windows, `libz3.dll` and the Visual C++ runtime DLLs it and `fusion.exe` need sit next to `fusion.exe` in `bin\`, the first place Windows looks (C23).

### Dependencies: pinning and fetching

- **pugixml, nlohmann/json, fast_float, Catch2:** `FetchContent_Declare` with `URL` and `URL_HASH SHA256=…`, versions from `cmake/FusionVersions.cmake` (C4). Catch2 is declared only when tests are built.
- `FETCHCONTENT_TRY_FIND_PACKAGE_MODE` is `NEVER`, so a system copy of a library is never picked up without notice.
- Downloads land in `FETCHCONTENT_BASE_DIR`, which CI caches. With `FETCHCONTENT_FULLY_DISCONNECTED=ON` a build runs offline from that cache.
- **Z3:** a dependency prefix outside the build tree is filled once per Z3 version and toolchain by `cmake -P cmake/FetchZ3.cmake` (C3):
  - macOS and Windows: download the official archive, check its SHA-256, unpack.
  - Linux: download the source archive, check its SHA-256, build it with the release toolchain and flags (C7), install.
  - The prefix is `~/.cache/fusion-deps/z3-5.3.3-<platform>-<compiler>` on Linux and macOS and `%LOCALAPPDATA%\fusion-deps\z3-5.3.3-<platform>-<compiler>` on Windows.
- **Finding Z3.** The build uses its own `cmake/FindZ3.cmake`, not Z3's CMake package, because not every source has one. Reading each 5.3.3 archive's file list over HTTP, a range request for its central directory with nothing unpacked, shows `lib/cmake/z3/Z3Config.cmake` in the Linux and macOS archives but not in the Windows one (`bin/libz3.dll`, `bin/libz3.lib` and headers only). Ubuntu's `libz3-dev` ships only `z3.pc` (`dpkg -L`). The module:
  - finds `z3++.h` and the library in the prefix, or anywhere CMake searches under `FUSION_Z3_PROVIDER=SYSTEM`;
  - reads `Z3_MAJOR_VERSION`, `Z3_MINOR_VERSION`, `Z3_BUILD_NUMBER` and `Z3_REVISION_NUMBER` from `z3_version.h`;
  - defines the imported target `z3::libz3`, the name Z3 5.3 uses for its own target.
- **`FUSION_Z3_PROVIDER=SYSTEM`** accepts an installed Z3 only if those numbers equal the pin. Otherwise configuration stops and names both versions. On this machine, the system Z3 4.8.12 is refused under a 5.3.3 pin: `found Z3 4.8.12.0 in /usr/include, but this build pins 5.3.3.0`.

### Z3

- **Version:** 5.3.3 (C2, accepted in entry 173), linked as a shared library on every platform (C17): `libz3.so` on Linux, `libz3.dylib` on macOS, `libz3.dll` on Windows. The pin is gated: before Phase 4, section 12's experiment X11 runs the real dumps through 4.8.12, 4.16.0 and 5.3.3 in both modes, and the user re-decides if 5.3.3 is slower than 4.8.12 at the median.
- **Check at startup:** `solve::verify_z3()`, which the `fusion` executable calls before it reads the batch, compares `Z3_get_version()` with the version `solve` was compiled against (`Z3_MAJOR_VERSION`, `Z3_MINOR_VERSION`, `Z3_BUILD_NUMBER`, `Z3_REVISION_NUMBER`). A mismatch stops `fusion`.
  - This catches another `libz3` that `LD_LIBRARY_PATH` or `DYLD_LIBRARY_PATH` puts first.
  - The check uses the numbers, never the version string, whose format changed between releases: `z3_version.h` defines `Z3_FULL_VERSION` as `"4.8.12.0"` in 4.8.12 and `"Z3 4.15.3.0"` in 4.15.3. `run.json` records the version as one string, as section 09 shows it.
- **Ctrl-C inside a check (C18, accepted with 09.C8 in entry 185 as a reading of A1).** Z3 has a `ctrl_c` parameter that lets a running check catch SIGINT itself. Both versions accept it as a solver parameter, and setting it to false on the solver lets SIGINT reach the program's own handler (tested on 4.8.12 and 4.15.3 with a small probe program during this section's review). 4.15.3 also lists it as a global parameter, default true (`z3 -p`); 4.8.12 does not. `solve` sets the solver parameter to false on every solver, which works on both versions, so Ctrl-C reaches only the executable's handler, which turns it into the stop request (09.C8: the handlers only set the flag the watcher reads; a second Ctrl-C ends the process with status 130; SIGHUP is ignored on Linux and macOS). This answers section 06's question; Phase 4 confirms it on 5.3.3 (test hooks).
- **Glibc floors**, measured with `objdump -T` (highest `GLIBC_` and `GLIBCXX_` symbol versions):

  | Build | Bytes | Needs |
  |---|---|---|
  | Ubuntu's `libz3.so.4` (4.8.12) | 16,106,752 | `GLIBC_2.38`, `GLIBCXX_3.4.32` |
  | Official 4.15.3 `x64-glibc-2.39` `libz3.so` | 34,503,464 | `GLIBC_2.38`, `GLIBCXX_3.4.32` |
  | PyPI wheel 4.15.1 (`manylinux2014`) `libz3.so` | 22,793,296 | `GLIBC_2.14`, `GLIBCXX_3.4.19` |
  | FuSION 1's `Linux/fusion` | 19,525,272 | `GLIBC_2.38`, `GLIBCXX_3.4.32` |

  So neither FuSION 1 nor an official Linux Z3 archive starts on RHEL 8 or 9, Debian 12 or Ubuntu 22.04. For 5.x the archive names say the same (`x64-glibc-2.39`, `arm64-glibc-2.38`).
- **What a different Z3 changes**, from the Z3 comparison (4.15.3 against 4.8.12):

  | Mode and dumps | Run under both | Same answer where both finished | Finished only with 4.8.12 | Finished with neither | Time, 4.15.3 ÷ 4.8.12, median (range) | Total where both finished | SAT models that differ |
  |---|---|---|---|---|---|---|---|
  | tactic, reference run | 15 | 14 of 14 | 1 | 0 | 1.48 (0.34 to 2.85) | 1,178 s against 1,612 s | 4 of 4 |
  | tactic, fdr050 | 14 | none finished under both | 10 | 4 | none | none | none |
  | incremental, reference run | 13 | 11 of 11 | 0 | 2 | 1.24 (0.03 to 5.15) | 445 s against 497 s | 4 of 4 |

  - Every check that finished under both versions gave the same SAT or UNSAT answer.
  - Every SAT model differed: the two versions chose different witnesses for every satisfiable point.
  - Times swing both ways, dump by dump. In incremental mode the reference run's (n 1, e 0) took 48.9 s with 4.8.12 and 1.4 s with 4.15.3, while (n 0, e 1) took 18.1 s and 93.1 s. Overall 4.15.3 was the slower.
  - On fdr050's dumps, 4.15.3 finished none within 600 s, while 4.8.12 finished 10 of 14, 4 of them SAT, in 79 to 585 s. 4.15.3 also used more memory there: 186 to 249 MB against 113 to 200 MB.
  - The runs were stopped at a fixed time: the tactic runs had reached 14 of fdr050's 32 dumps, and the incremental runs 13 of the reference run's 15. The rest were not run.

- **What this means for 2.0:**
  - Witness identity with FuSION 1's runs is lost whichever Z3 is linked. 2.0's encoding differs from FuSION 1's (for example, self-loops leave the relaxation count, S5.13), and new code creates Z3 terms in its own order. Phase 3 compares frontiers, which do not depend on the Z3 version wherever every check finished ([decision 0023](../decisions/0023-results-may-change-where-improved.md)).
  - Z3 5.3.0 adds "cross-platform solver-determinism checks" and fixes "that make solver output reproducible across platforms (random-seed parsing, AST-construction order, …)"; 5.1.0 had already sequenced term creation in rewriters "for cross-compiler determinism" (Z3 `RELEASE_NOTES.md`). Under G3, that makes it likely that one batch gives the same witness on Linux, macOS and Windows. The promise stays narrower: section 12's one golden set covers frontiers and every query's SMT-LIB text, and excludes witnesses and times (12.C11, entry 186). FuSION 1 already ran two Z3 versions: 4.8.12 on Linux and Homebrew's 4.15.4 on macOS (`macOS/README.md:43`).

### Compilers, flags and the C++20 subset

Compilers (C8):
- **Linux:** GCC 14 from `gcc-toolset-14` in the `manylinux_2_28` image for releases. CI also builds with Clang 18 on Ubuntu 24.04 on every push, and once a week with GCC 13, the oldest accepted GCC (C8 and C22, entry 201).
- **macOS:** Apple Clang from Xcode 16.4 or later, deployment target 13.3 (`CMAKE_OSX_DEPLOYMENT_TARGET`). Xcode 16.3 wrongly demanded 13.4 for floating-point `to_chars`, a bug fixed in 16.4 (Apple developer forums, thread 779096).
- **Windows:** MSVC 19.44 (Visual Studio 2022 17.14, toolset v143) or Visual Studio 2026, with `/permissive- /Zc:__cplusplus /utf-8 /EHsc /W4` and the DLL runtime (`/MD`), which the official `libz3.dll` also uses.
- CMake stops with a clear message below the minimums: GCC 13 and Clang 17 (C22 amends G1's GCC 12 and Clang 15; entry 200), and MSVC 19.40 (Visual Studio 2022 17.10), which G1 does not name and C8 sets (entry 201).

Flags (C7):
- Release builds use `-O2` (MSVC `/O2`) and link-time optimisation. FuSION 1 builds with `-g` and no `-O` at all (`Linux/Makefile:10`, `68`).
- Floating point: `-ffp-contract=off` on GCC and Clang, `/fp:precise` without `/fp:contract` on MSVC. Never `-ffast-math`, `-Ofast` or `-march=native`. The x86-64 baseline instruction set, so one binary runs on every x86-64 CPU.
- The promise is byte-identical canonical outputs on all three platforms (C7 with 09.C16 and 12.C11, entry 186). Maths libraries differ in the last bit, so no result path calls the platform's `exp`, `log` or `pow`; one that needs them uses a single correctly rounded implementation compiled into libfusion, the same on every platform. None is known to need them yet: GSEA's weight is 1 (S7.10), and the other result paths are counts, sums and divisions.
- Z3 built from source (Linux) gets the same floating-point flags.
- Reproducibility (see below): `-ffile-prefix-map=<source>=.`, `-Wdate-time` so `__DATE__` and `__TIME__` cannot creep in, MSVC `/Brepro`.
- Warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` and `/W4`, as errors in CI only.

The C++20 subset that builds with the minimums of C22 and C8 and with macOS 13.3:

| Feature | Rule | Why |
|---|---|---|
| concepts, `std::span`, `<bit>`, designated initialisers, `<=>` | use | available on every target |
| `<ranges>` algorithms and basic views | use | available on every target; C++23 views are not |
| `std::to_chars(double)` | use, for exact numbers (S1.27) | libstdc++ 11+, MSVC; Apple's libc++ from macOS 13.3, which sets the deployment target |
| `std::from_chars(double)` | avoid; fast_float instead (C5) | libc++ has it only from LLVM 20, behind an availability check that a 13.3 target fails |
| `std::format` | use (C22 amends G1; entry 200) | libstdc++ has it from GCC 13, libc++ from LLVM 17 and MSVC from 19.29; its floating-point output needs macOS 13.3, the deployment target |
| `std::jthread`, `std::stop_token` | avoid; `std::thread` with an atomic flag | experimental in libc++ |
| `std::execution` policies | avoid; explicit threads | libstdc++ needs TBB; libc++'s are experimental |
| modules | avoid | not supported by every compiler and CMake generator here |
| `std::shuffle`, `std::uniform_int_distribution` and the other distributions | avoid in any result path | their output is implementation-defined, so libstdc++, libc++ and MSVC's STL give different numbers from the same seeded engine; `core` provides its own: 09's `core::Rng` (`std::mt19937_64` with its own distributions, 09.C15) and 08.C1's port of gseapy's generator for GSEA |
| iteration over `std::unordered_map` and `std::unordered_set` | never into an output | order differs between the three standard libraries; sort first (09's stable orderings) |
| `std::filesystem::directory_iterator` | sort before use | order differs by OS and file system |

### The portability layer in `core`

Interface outline. All strings are UTF-8. Section 09's names are the one `core` vocabulary (cross-section item 1.1, entry 158): `core::Failure` with its derived `UsageError` and `InstallError`, `Outcome<T>`, `Log`, `Events`, `Context`, `Sha256` and `stable_compare`. Section 09 also declares four `core` functions this layer implements per platform: `path_from_utf8`, `cpu_seconds`, `peak_rss_bytes` and `build_id`. They are not repeated here.

```cpp
// backend/modules/core/include/fusion/core/build_info.hpp
namespace fusion::core {

struct BuildInfo {
    std::string_view version;            // contents of VERSION, e.g. "2.0.0-dev"
    std::string_view git_commit;         // 40 hex digits, or "unknown" for a source archive
    bool git_dirty;                      // 09's build_id() adds the executable's SHA-256 when true
    std::string_view build_type;         // "Release", "Debug"
    std::string_view compiler;           // "GNU 14.2.1", "AppleClang 17.0.0", "MSVC 19.44.35207"
    std::string_view target;             // "linux-x86_64", "macos-arm64", "windows-x86_64"
    std::int64_t source_date_epoch;      // commit time; no wall-clock build time is embedded
    std::span<const std::pair<std::string_view, std::string_view>> libraries;
                                         // pinned versions as text from cmake/FusionVersions.cmake:
                                         // {"pugixml", "1.16"}, {"nlohmann_json", "3.12.0"}, {"fast_float", "8.3.3"}
};

const BuildInfo& build_info() noexcept;  // generated by cmake/build_info.cpp.in; links no library
}

// backend/modules/solve/include/fusion/solve/z3_version.hpp   (solve alone links Z3)
namespace fusion::solve {
std::string z3_version();                // runtime version, "5.3.3.0", from Z3_get_version()
void verify_z3();                        // throws core::InstallError (code install.z3_mismatch) if the versions differ
}

// backend/modules/core/include/fusion/core/install.hpp
namespace fusion::core {

struct InstallDir {
    enum class Source { env_override, executable };
    std::filesystem::path prefix;      // the directory above bin/
    std::filesystem::path bin_dir;     // prefix/bin
    std::filesystem::path home;        // prefix/share/fusion: every bundled file; $FUSION_HOME (01)
    std::filesystem::path tools_dir;   // home/tools
    Source source;
};

std::filesystem::path executable_path();                  // absolute, symlinks resolved
const InstallDir& install_dir();                          // computed once, thread-safe
std::filesystem::path home_file(std::string_view rel);    // throws InstallError (install.file_missing) if missing
// InstallError is section 09's, derived from core::Failure; this section raises it with install.* codes.
}

// backend/modules/core/include/fusion/core/gnuplot.hpp   (2B.13: shared by 07's plot_pareto and 04's histogram)
namespace fusion::core {

struct PlotPalette {                  // plot_po.sh's look (S6.23)
    std::string_view with_case = "#2a78d6", without_case = "#e34948";
    std::string_view background = "#fcfcfb", grid = "#e1e0d9";
    std::string_view font = "Liberation Sans";
};
const PlotPalette& plot_palette() noexcept;

struct GnuplotJob {
    std::string script;                    // the whole script, data inline (gnuplot 5 data blocks)
    std::filesystem::path output;          // the PNG it must write
    std::filesystem::path working_dir;
};

struct GnuplotResult {
    int exit_code;
    bool output_written;                   // the file exists and is non-empty
    std::string stderr_text;               // warnings included, such as an unknown terminal
};

// Finds gnuplot in C20's order, sets FONTCONFIG_FILE and PANGOCAIRO_BACKEND for the bundle (C10),
// runs it through run_process with the script on stdin, and reports; the caller raises a failed
// plot as its own failure (04's expression.gnuplot_failed, 07's results.gnuplot_failed).
GnuplotResult run_gnuplot(const GnuplotJob&);
}

// backend/modules/core/include/fusion/core/platform.hpp
namespace fusion::core {

struct ProcessSpec {
    std::filesystem::path program;                            // absolute; never looked up by a shell
    std::vector<std::string> args;                            // each passed as one argument
    std::filesystem::path working_dir;
    std::vector<std::pair<std::string, std::string>> env_set; // added or replaced variables
    enum class Input { null, text } input = Input::null;
    std::string stdin_text;                                   // e.g. a gnuplot script
    enum class Output { capture, file } output = Output::capture;
    std::filesystem::path output_file;                        // stdout and stderr together, for Output::file
    bool own_group = false;           // own process group (POSIX) or job object (Windows)
    std::chrono::milliseconds timeout{0};                     // 0 = none
};

struct ProcessResult {
    int exit_code;                    // -1 if it could not start or was killed
    bool timed_out;
    std::string stdout_text, stderr_text;   // empty for Output::file
    std::chrono::milliseconds wall;
};

class Process {                       // move-only handle; the sweep coordinator and exec use it (01)
public:
    static Process spawn(const ProcessSpec&);
    std::optional<int> poll();        // exit code once finished
    ProcessResult wait();
    void stop();                      // ends the whole group or job
};

ProcessResult run_process(const ProcessSpec&);          // spawn, then wait
std::optional<std::filesystem::path> find_on_path(std::string_view program);
std::chrono::nanoseconds thread_cpu_time();             // per-check CPU time (S5.24, section 06)
std::filesystem::path long_path(const std::filesystem::path&);  // adds \\?\ on Windows when needed
}

// backend/modules/core/include/fusion/core/deps.hpp
namespace fusion::core::deps {

enum class Kind { program, font };

struct Requirement {                    // one per command line that needs a tool
    std::string id;                     // "gnuplot", "dot", "sh", "font:Liberation Sans"
    Kind kind;
    std::string min_version;            // "5.4" for gnuplot (07)
    std::vector<std::string> features;  // for gnuplot, terminals, e.g. {"pngcairo"}
    std::string requested_by;           // command and batch line, e.g. "plot_pareto (line 262)"
};

struct Probe {                          // one per id; requirements with the same id are merged
    std::string id;
    bool ok;
    std::filesystem::path path;
    std::string version;                  // "6.0.5"
    std::string origin;                   // "bundled", "FUSION_GNUPLOT", "FUSION_DOT", "PATH", "system"
    std::vector<std::string> features;    // requested features that were found
    std::vector<std::string> missing;     // requested features or files not found
    std::vector<std::string> needed_by;   // every requested_by merged into this probe
    std::string message;                  // what to do about it
};

class Registry {
public:
    void require(Requirement r);                   // commands add theirs from the command table (01)
    std::vector<Probe> check() const;              // probes each id once; cached for the process
};
}
```

What each platform does underneath:

| Concern | Linux | macOS | Windows |
|---|---|---|---|
| Executable path | `readlink("/proc/self/exe")` | `_NSGetExecutablePath`, then `realpath` | `GetModuleFileNameW` with a growing buffer, then `std::filesystem::canonical` |
| Symlinks | resolved by the kernel | resolved by `realpath`; FuSION 1 left them unresolved (S1.20) | resolved by `canonical` |
| Child process | `vfork`, then only `setpgid`, `chdir`, `dup2` and `execve` in the child: glibc's `posix_spawn` gained a working-directory action (`posix_spawn_file_actions_addchdir_np`) only in 2.29, above C1's 2.28 floor; pipes or a file; its own process group on request | `posix_spawn` with `posix_spawn_file_actions_addchdir_np` (macOS 10.15 and later) | `CreateProcessW` with each argument quoted by the C runtime's rules, `CREATE_NO_WINDOW`, and a job object, so that `stop()` or the end of `fusion` also ends the child's children |
| Text encoding | UTF-8 | UTF-8 | manifest `activeCodePage` UTF-8 (Windows 10 1903 and later), `/utf-8`, console code page set to UTF-8 by `fusion` |
| Long paths | none | none | `long_path()` adds `\\?\` to absolute paths of 248 characters or more; the manifest also sets `longPathAware` |
| Line endings | outputs are written in binary mode with `\n`; readers accept `\n` and `\r\n` | the same | the same; a text-mode stream would write `\r\n` and break byte comparisons across platforms |
| Environment | case-sensitive | case-sensitive | names are case-insensitive (`Path` and `PATH` are one variable); `run_process` merges them that way |
| File sharing | no mandatory locks | no mandatory locks | a file open in another process cannot be renamed over unless it was opened with `FILE_SHARE_DELETE`, and antivirus scanners hold new files briefly; replacing a file retries, by section 09's rule |
| CPU time | `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` | the same | `GetThreadTimes` |

Measured path lengths: the longest file path under FuSION 1's `runs/`, relative to `runs/`, is 163 characters (`real_experiments/hsa7124_to_hsa4792_fsc_…/…_limits_timefile.txt`; 2 of 8,671 files exceed 160). Under a Windows profile path such as `C:\Users\<name>\OneDrive - <institution>\Documents\FuSION\runs\` (about 75 characters), that reaches about 240 of the 260 that Windows allows without long-path support.

Names that differ only in case are one file on macOS (APFS by default) and Windows, so names that FuSION creates must be unique ignoring case (09).

Cost: the executable path and install layout are worked out once, in a few system calls. `run_process` costs one process start; gnuplot starts in 0.02 to 0.06 s (F1's measurement).

### Finding the backend's own files (S1.20)

At startup, before the batch is read:

1. If the environment variable `FUSION_HOME` is set (C20), it is the home directory, the one that holds every bundled file. This is for tests and for unusual installs, and it is recorded in `run.json`.
2. Otherwise the prefix is the parent of the directory that holds the resolved executable, and the home is `<prefix>/share/fusion`. A `fusion` symlink, such as `/usr/bin/fusion` from the Linux package, therefore finds the real installation on every OS, which FuSION 1's macOS build did not (S1.20). Finding the tools this way also makes a Finder-started app independent of its minimal `PATH` (section 10's note).
3. The home must contain `fusion-data.json`, whose version must equal `build_info().version`. Otherwise `fusion` stops with an install error (`install.home_missing` or `install.version_mismatch`) that lists the paths it tried.
4. Every bundled file is reached through `install_dir()`. There is no lookup relative to the launch directory, and no absolute path is written into a batch file.

The home is what section 01's batch variable `$FUSION_HOME` names, written with `/` on every platform (01.C18, entry 189), so `$FUSION_HOME/data/hsa_to_gene_symbol_map.txt` is the bundled symbol map, and a batch that names it runs on any machine. By contrast, all 29 FuSION 1 batch files that call `plot_po.sh` today (one line each; S6.22 counted 22) name it by an absolute path into one user's checkout.

The home is the checked location that S8.13 asks for. The shipped maps are the default when a batch names none (cross-section item 2B.4), a new session's maps default to them (S8.06; section 10), and `fusion --version --json` checks each data file against its SHA-256 in `fusion-data.json`, so the frontend can refuse a damaged install before a run (10). The choosers for other maps stay section 10's.

The data directory holds four files, settled in entries 204 and 205:
- `hsa_to_gene_symbol_map.txt`, rebuilt from NCBI Gene's human gene table: each line pairs a gene's KEGG-style id (`hsa` and its NCBI Gene id, such as `hsa7791`) with its official symbol (`ZYX`). NCBI Gene's data are free to redistribute.
- `hsa_path_to_path_name_map.txt`, rebuilt from Reactome's pathway names, under CC0, with its attribution. *Sprint (prompt log entry 315):* Reactome's data, and files derived from them, are CC0 (its licence, section 1(c)), not CC BY 4.0 as this section first said; CC BY 4.0 covers only Reactome's illustrations, icons, art and branding, none of which ship. The attribution is still given, though not required. It carries no KEGG pathway names: those are read at run time from the title on each KGML's root element ("MAPK signaling pathway") in the user's own KGMLs, a rule section 02 applies to every label (entry 205). FuSION 1's copy, of unknown source, held 479 KEGG rows beside its Reactome-style rows.
- `kegg_pathways.txt`, the 167 candidate pathways of FuSION 1's list (`auxilliary/list_of_kegg_xml_files.txt`) as bare pathway ids, one per line (`hsa03320`, `hsa03460`, `hsa04010`, …; 1,503 bytes), which section 02 resolves against each user's `kgml_dir` (02.C12; cross-section items 2B.5, 4A.10 and 4B.41, entry 204). Ids carry no KEGG content, and S8.31's "KGML list resolved against the user's own KGML directory" is this file.
- `SOURCES.md`, which records for each file its source, the date it was taken, its licence and the attribution owed. Nothing in 2.0 fetches KGMLs: the user guide says how to get them, and the check before a run names every listed pathway missing from the user's folder (02 open question 6, entry 164).

The layout is the same on every platform, and the build's staging tree has it too:

```text
<prefix>/
  bin/fusion                 on Windows: fusion.exe, libz3.dll and the Visual C++ runtime DLLs (C23)
  lib/libz3.so               with its versioned name; libz3.dylib on macOS; nothing on Windows
  share/fusion/              the home: $FUSION_HOME
    fusion-data.json
    data/hsa_to_gene_symbol_map.txt, data/hsa_path_to_path_name_map.txt,
    data/kegg_pathways.txt, data/SOURCES.md
    pipelines/standard.json, pipelines/standard.batch          (10.C12)
    fonts/LiberationSans-Regular.ttf, fonts/LiberationSans-Bold.ttf, fonts/fonts.conf, fonts/OFL.txt
    tools/gnuplot/bin/gnuplot  with its libraries in tools/gnuplot/lib (bin\ on Windows)
    tools/busybox/busybox.exe  Windows only (01.C10)
    licenses/THIRD_PARTY_NOTICES.txt and one text per component
```

`fusion-data.json`, written by the build:

```json
{
  "fusion_version": "2.0.0",
  "git_commit": "3f2c9a1e0d4b7c6a5e8f9d0c1b2a3f4e5d6c7b8a",
  "files": {
    "data/hsa_to_gene_symbol_map.txt": {"bytes": 211406, "sha256": "4b1d…", "source": "NCBI Gene, see data/SOURCES.md"},
    "data/hsa_path_to_path_name_map.txt": {"bytes": 118295, "sha256": "90ce…", "source": "Reactome, CC0, see data/SOURCES.md"},
    "data/kegg_pathways.txt": {"bytes": 1503, "sha256": "d27a…", "source": "FuSION 1's candidate list, ids only"},
    "pipelines/standard.json": {"bytes": 6214, "sha256": "a07c…"},
    "fonts/LiberationSans-Regular.ttf": {"bytes": 410820, "sha256": "e3a5…"},
    "fonts/LiberationSans-Bold.ttf": {"bytes": 414568, "sha256": "7f02…"},
    "fonts/fonts.conf": {"bytes": 377, "sha256": "c18b…"}
  },
  "tools": {"gnuplot": "tools/gnuplot/bin/gnuplot"}
}
```

The byte counts of the two rebuilt maps above are only illustrations; the real files are made in Phase 4. `fusion --version --json` verifies every listed file against its SHA-256: about 1.2 MB, a few milliseconds. A normal run hashes only the files it uses, which it records anyway as inputs (E1).

### The dependency check before a run (S6.11)

- **One list.** This section owns the check, its error `deps.missing` (cross-section item 2A.1, entry 158) and its one warning, `deps.font_missing` (entry 230). Each command declares its requirement, a tool with its minimum version and features, in a field of section 01's command table; section 09's check before the run calls `deps::Registry::check()`. After F1, the external programs are:

  | Tool | Needed by | Bundled | Minimum |
  |---|---|---|---|
  | gnuplot with `pngcairo` | `plot_pareto` (07) and the threshold histogram when `plot` is given (04.C7), both through `core::run_gnuplot` | yes (C9) | 5.4 for both plots (cross-section item 1.27, entry 158); the bundle has 6.0.5 |
  | Liberation Sans | the same plots | yes (C10) | the family; a missing font is only the warning `deps.font_missing` (07.C8) |
  | a POSIX `sh` | `exec` lines (01.C10) | Windows only, busybox-w32; `/bin/sh` elsewhere | none; its path and version go into `run.json` |
  | Graphviz `dot` | `static=svg` only, on request (07.C5) | no; the user installs it | 07's |

- **When.** Before the first step, section 01's check collects the requirements of every command in the batch, sweeps included, and probes each once (decision 0021). A batch without a plot step never starts gnuplot. `fusion -b <file> --check` runs only this check (01).
- **Where a tool is looked for (C20, and cross-section item 1.27):** `FUSION_GNUPLOT` or `FUSION_DOT` if set; else the bundled copy under `tools/`; else the program on `PATH`, which only Graphviz and development builds without a bundle use. The origin is recorded.
- **`exec` lines and the bundled tools (C20, entry 204).** An `exec` child gets the bundled tools directory appended to its `PATH`, so a system gnuplot, as FuSION 1 used, still comes first, and a machine without one still runs FuSION 1's inline gnuplot lines. 59 corpus batch files run gnuplot inside `exec` lines, 71 of those lines with `set term png`, and S6.25 keeps them working; Phase 4 checks that the cairo-only bundled build accepts `set term png`.
- **A missing terminal is only a warning.** Measured on this machine with gnuplot 6.0.0, each case run on its own, with no pipe after it:
  - `set terminal nosuchterm` prints `warning: unknown or ambiguous terminal type`, keeps the previous terminal and exits 0;
  - real errors exit 1: `GPVAL_VERSION . GPVAL_PATCHLEVEL` (the `.` operator on a number), an output file in a missing directory, and a missing data file (`No data in plot`).

  So the probe does not try `set terminal`. It runs `gnuplot -e "print GPVAL_VERSION, GPVAL_PATCHLEVEL; print GPVAL_TERMINALS"` and reads stderr: here, `6.0 0` and a list of 56 terminals, including `pngcairo` and `svg`. The plot step (07) checks both the exit status and that the output file was written, because a gnuplot without the asked terminal would warn, draw nothing and exit 0.
- **The font** (C10). With the bundle, the probe checks the two font files against `fusion-data.json`. With a system gnuplot, it asks `fc-match -f '%{family[0]}' 'Liberation Sans:bold'` where fontconfig exists. On this machine that answers `Liberation Sans`, from `fonts-liberation` 2.1.5, whose files cover the plot's `→`, `·`, `≤` and `≥`. A font still missing gives the warning `deps.font_missing` before the first step, and gnuplot substitutes another (07.C8, accepted with C10 in entry 179; the code from entry 230); only a missing gnuplot or `pngcairo` fails the check.
- **`fusion --version [--json]`** (01.C14, whose JSON this section describes) prints the versions as `run.json` records them, the home directory, the data files with their source and date, and the probe of every tool the installation provides. It exits 1 if a bundled file or tool is missing or damaged. The installer's smoke test and the frontend's start-up check call it. Example:

  ```text
  FuSION 2.0.0 (3f2c9a1) at /opt/fusion/lib/app/backend
    z3        5.3.3.0          linked, equals the pinned 5.3.3                ok
    gnuplot   6.0.5            share/fusion/tools/gnuplot (bundled)          ok  pngcairo svg
    font      Liberation Sans  share/fusion/fonts (bundled, 2 files)         ok
    dot       not found        optional, for static=svg (07.C5)              -
    sh        /bin/sh          for exec lines                                ok
    data      7 files          share/fusion (sha256 verified)                ok
  ```

- **Cost.** One gnuplot start per run that plots, 0.02 to 0.06 s, plus hashing at most 1.2 MB.

### Versions in `run.json` (E1, S7.02)

Section 09 owns the schema, which takes this section's five blocks beside section 01's variables and each step's normalised line, and lists its volatile fields for section 12's comparisons (cross-section item 1.14, entry 158). The blocks are filled at startup and by the dependency check:

```json
{
  "versions": {
    "fusion": {"version": "2.0.0", "git_commit": "3f2c9a1e0d4b7c6a5e8f9d0c1b2a3f4e5d6c7b8a",
               "git_dirty": false, "build_type": "Release",
               "compiler": "GNU 14.2.1", "target": "linux-x86_64"},
    "libraries": {"z3": "5.3.3.0", "pugixml": "1.16", "nlohmann_json": "3.12.0", "fast_float": "8.3.3"}
  },
  "tools": {
    "gnuplot": {"version": "6.0.5", "origin": "bundled", "features": ["pngcairo"],
                "needed_by": ["plot_pareto (line 262)"],
                "path": "/opt/fusion/lib/app/backend/share/fusion/tools/gnuplot/bin/gnuplot"},
    "font": {"family": "Liberation Sans", "version": "2.1.5", "origin": "bundled",
             "sha256": {"LiberationSans-Regular.ttf": "e3a5…", "LiberationSans-Bold.ttf": "7f02…"}}
  },
  "platform": {"os": "Ubuntu 24.04.5 LTS", "kernel": "7.0.0-38-generic", "arch": "x86_64",
               "cpu": "Intel(R) Xeon(R) Gold 6140 CPU @ 2.30GHz", "logical_cpus": 36},
  "install": {"prefix": "/opt/fusion/lib/app/backend",
              "home": "/opt/fusion/lib/app/backend/share/fusion", "source": "executable"},
  "environment": {"FUSION_HOME": null, "FUSION_GNUPLOT": null, "FUSION_DOT": null,
                  "FUSION_CACHE_DIR": null}
}
```

Only the tools a run used appear under `tools`, each with the fields of a `Probe`: `version`, `origin`, `features`, `needed_by` and `path`. A run without `exec` lines or `static=svg` lists neither the shell nor `dot`. `environment` also records section 09's `FUSION_CACHE_DIR`, which moves the shared store (1.14).

- Results do not depend on the CPU: FuSION's own code does not dispatch on CPU features, and the flags forbid contraction (C7). The CPU is recorded for timings, and the identity test (C15) checks the claim.
- Z3 appears as one string, as in section 09's example. A finished run never needs two: a runtime version that differs from the compiled one has already stopped `fusion`.
- The identity test relies on result files that hold no absolute path and no time, as section 07 designs its outputs; only `run.json` holds paths.

### Reproducible builds

- **Same source, same toolchain, same binary.** Builds embed no wall-clock time: the build time is the commit time (`SOURCE_DATE_EPOCH`). Absolute source paths are mapped to `.`. Static archives are written in deterministic mode. MSVC uses `/Brepro`.
- **Checked on Linux.** CI builds the release twice, in two directories, and compares the SHA-256 of `fusion` (C15). macOS and Windows builds use the same kind of flags, but are not compared: Linux minutes are the cheapest, and signed binaries differ by design anyway.
- **Gradle.** Archives are built with `preserveFileTimestamps = false` and `reproducibleFileOrder = true`, and every artefact is checked against `verification-metadata.xml` (C4). A Gradle toolchain can ask only for a language version and a vendor, not an update release, so the shipped runtime is pinned in the packaging step instead (C13).
- **Results across platforms.** The promise is byte-identical canonical outputs on Linux, macOS and Windows for every deterministic stage, for the SMT-LIB text of every query and for frontiers, with witnesses and times excluded (C7 with 09.C16 and 12.C11, entry 186). It rests on the floating-point flags and the rule on `exp`, `log` and `pow` (C7), on `core`'s own generators instead of the standard library's distributions (09.C15, 08.C1), and on `\n` line endings. CI's identity test checks it on a synthetic batch (C15, test hooks). Z3 5.3's cross-platform work makes identical witnesses likely too, but they are not promised.

### The frontend's Gradle build

- `frontend/` is one Gradle project with a Kotlin DSL build script, a version catalog and the Gradle 9.9.1 wrapper (C13). It builds a JavaFX desktop application (10.C1). Spring Boot's Gradle plugin 4.1.2 (C21) imports Spring Boot's dependency versions; the application uses Spring Boot's core, with no web starter and no embedded server.
- JavaFX for compiling and testing comes from Maven Central: `org.openjfx:javafx-base`, `javafx-graphics` and `javafx-controls` 21.0.13 (C24), each with the build platform's classifier (`linux`, `mac`, `mac-aarch64`, `win`) and checked by dependency verification (C4, C25). `javafx-web` and `javafx-media` are not used: section 07's pages open in the system browser (10.C6, entry 198).
- The Java toolchain asks only for language version 21, with no vendor, so this machine's Ubuntu OpenJDK 21.0.13 qualifies. A Gradle toolchain cannot name an update release, and a vendor of Adoptium would refuse this machine's JDK. Without any JDK 21, Gradle downloads the current Temurin 21 (C13).
- The runtime that ships is pinned separately. The packaging step downloads the Temurin 21.0.13+9 JDK for its platform by URL and SHA-256 (from `cmake/FusionVersions.cmake`) and runs `jlink` and `jpackage` from it, so every installer of a release carries the same runtime whatever JDK compiled the jar.
- Tasks:
  - `jar`: the application's own classes, as a plain jar built reproducibly; with its dependency jars it goes on the class path (C25), so Spring Boot's nested-jar `bootJar` is not used;
  - `test`: JUnit 5 (section 12);
  - `jdepsModules`: runs `jdeps --print-module-deps --ignore-missing-deps --multi-release 21` over the application's jars and fails if a module is missing from the checked-in list `packaging/jlink-modules.txt`;
  - `runtimeImage`: `jlink --module-path <Temurin jmods>:<OpenJFX jmods> --add-modules <list> --strip-debug --no-header-files --no-man-pages --compress zip-6`, with the pinned Temurin JDK and the pinned OpenJFX jmods for the platform (C25);
  - `standardBatch`: writes `pipelines/standard.batch` from `pipelines/standard.json` with the frontend's own batch generator, so command-line users get the standard run (10.C12, entry 195); `package.yml` runs it before either deliverable is assembled, so the backend-only archive carries `standard.batch` too, and only a CMake-only build's staging tree lacks it;
  - `jpackage`: one installer per platform (C12), taking the backend's install tree as input, with the main launcher and, on Windows, the console launcher `fusion-frontend` for `--reset-password <username>` (entry 199).
- **The runtime image's modules.** The roots are OpenJFX's own needs and Spring Boot's:
  - OpenJFX 21's `module-info` files: `javafx.base` requires `java.desktop`; `javafx.graphics` requires `java.desktop`, `java.xml` and `jdk.unsupported`; `javafx.controls` requires only the other two JavaFX modules.
  - Spring Boot without a web server: `java.logging`, `java.naming` and `java.management`, and `java.sql`, which Spring Boot's own jar refers to (its `org.springframework.boot.jdbc` package) even with section 10's JSON-file store (10.C8). `jdepsModules` gives the real list.
- **Their size**, resolved from this machine's jmods with `jmod describe`: 12 JDK modules, 44,643,339 bytes of jmods (`java.base`, `java.datatransfer`, `java.desktop`, `java.logging`, `java.management`, `java.naming`, `java.prefs`, `java.security.sasl`, `java.sql`, `java.transaction.xa`, `java.xml` and `jdk.unsupported`), against 85,023,528 bytes for all 69 modules.
- **Estimated runtime image: about 60 to 75 MB.** The JDK part is 48 to 58 MB. The JavaFX part is at most the unpacked 21.0.13 jars below, read from their file lists on Maven Central; jlink compresses the classes but not the native libraries. jlink itself was not run.

  | OpenJFX 21.0.13 module | Linux x86-64 | macOS x86-64 | macOS arm64 | Windows x86-64 |
  |---|---|---|---|---|
  | `javafx.base` | 1,521,188 | about the same | about the same | about the same |
  | `javafx.controls` | 5,319,503 | about the same | about the same | about the same |
  | `javafx.graphics`, with its native libraries | 8,970,463 | 9,308,604 | 9,444,700 | 11,621,482 |

  `javafx.base` and `javafx.controls` hold no native code, and their packed jars differ by at most 25 bytes between platforms. Packed, the jars of the three modules are 8,020,309 bytes on Linux, 8,195,422 on macOS x86-64, 8,160,260 on macOS arm64 and 9,247,020 on Windows. Gluon's jmods archives for all of OpenJFX 21.0.13 are 53,622,248 bytes (Linux x86-64), 50,009,754 (macOS x86-64), 48,814,394 (macOS arm64) and 47,109,232 (Windows x86-64); jlink takes only the three modules from them. `javafx.web`, which 10.C6 leaves out, would have added 102,371,564 to 118,548,212 bytes unpacked, almost all of it `jfxwebkit`.

### Packaging per platform

Deliverables per release (C12):

| Platform | Desktop installer (frontend and backend) | Backend-only archive |
|---|---|---|
| Linux x86-64 | `fusion_2.0.0_amd64.deb`, `fusion-2.0.0-1.x86_64.rpm`, installed under `/opt/fusion` | `fusion-2.0.0-linux-x86_64.tar.gz` |
| macOS arm64, x86-64 | `FuSION-2.0.0-arm64.dmg`, `FuSION-2.0.0-x86_64.dmg` with `FuSION.app` | `fusion-2.0.0-macos-arm64.tar.gz`, `…-x86_64.tar.gz` |
| Windows x86-64 | `FuSION-2.0.0.msi`, a per-user install that needs no administrator rights | `fusion-2.0.0-windows-x86_64.zip` |

- **What the installer holds:** a runtime image jlink'd from the pinned Temurin 21 JDK and OpenJFX 21.0.13's `javafx.base`, `javafx.graphics` and `javafx.controls`, with the JDK modules listed under the Gradle build and no `javafx.web` (10.C6); the frontend's jars and their Spring Boot dependencies, on the class path (C25); and the backend tree of the layout above (`fusion`, `libz3`, on Windows the Visual C++ runtime DLLs (C23), and the home with gnuplot, the fonts, the data and, on Windows, busybox-w32).
- **Where the backend sits:** jpackage copies its `--input` directory into the application, so the backend lands in `app/backend/`: `/opt/fusion/lib/app/backend` on Linux, `FuSION.app/Contents/app/backend` on macOS, and `app\backend` under the per-user install directory on Windows.
- **How the frontend finds it (S8.01):** the launcher passes `--java-options '-Dfusion.backend.executable=$APPDIR/backend/bin/fusion'` (`fusion.exe` on Windows), the setting section 10 reads. jpackage replaces `$APPDIR` at launch, so nothing depends on the working directory.
- **A normal desktop application** (10.C4, entry 197). jpackage's main launcher starts the JavaFX application in its own window:
  - on macOS, a standard `.app` with a Dock icon and Quit in the application menu;
  - on Windows, a Start-menu entry and a taskbar button (`--win-menu`, `--win-shortcut`);
  - on Linux, a desktop entry (`--linux-shortcut`) in the system's application menu.

  Quit in the File menu, closing the main window, runs that continue after a quit, and one instance per data directory through `frontend.lock`, a second launch handing focus over through `frontend.focus`, are section 10's.
- **A console launcher on Windows** (entry 199). Password resets go through the command line (10.C10), and the windowed launcher has no console on Windows, so the `.msi` adds a second jpackage launcher, `fusion-frontend`, with a console (`--add-launcher` with `win-console=true`), which runs `fusion-frontend --reset-password <username>`. On Linux and macOS the same command runs from a terminal through the application's own launcher.
- **Nothing for a web server.** The packaging has no port, no `open.html` page for the OS opener, and no `LSUIElement` background-agent setting on macOS, which section 10's earlier local-web-server draft proposed. The runtime keeps `java.desktop`, since `javafx.base` and `javafx.graphics` require it. With no lab-server mode (10.C1), the Linux packages install no systemd unit.
- **Linux needs a desktop.** JavaFX 21 draws through GTK 3, version 3.8 or later (JavaFX 21 release notes). RHEL and Rocky 8 ship GTK 3.22, so C1's oldest distributions qualify. The `.deb` and `.rpm` declare the distribution's GTK 3 package as a dependency. Oracle certifies JDK 21 on Ubuntu 24.04 under X.org and Xwayland and on Oracle Linux 8 and Ubuntu 22.04 under X.org only. The backend-only archive needs no desktop.
- **Command-line use:**
  - the Linux packages add `/usr/bin/fusion` as a symlink to the backend, which step 2 above resolves;
  - on macOS and Windows the installer offers to put `fusion` on the user's `PATH`;
  - servers and clusters use the backend-only archive, unpacked anywhere: no root, no Java, no frontend. FuSION 1's recorded runs were started from shell scripts such as `run_sweep.sh` and `run_all.sh`, except one from the GUI (stage 8, open question 2).
- **First launch.** The frontend runs `fusion --version --json` and shows any failure (10).
- **Tools.** jpackage from JDK 21 builds `.msi` files with WiX 3.14; JDK 24 and later also accept WiX 4 and 5. Building `.deb` and `.rpm` needs `dpkg-deb` and `rpmbuild` on the CI runner.
- **Signing.** Development builds are unsigned, and signing is decided before the first public release, when the user creates the Apple and Windows accounts and adds their secrets to GitHub (open question 3, entry 203). Until then an unsigned `.dmg` is stopped by Gatekeeper at first launch, and an unsigned `.msi` meets a SmartScreen warning. The workflow has a signing step that stays off until signing identities exist. When it is on, it signs every native library in the bundle, OpenJFX's (`libglass`, `libprism_*`) as well as `fusion`, `libz3` and gnuplot's.
- **Estimated installed size: about 120 to 210 MB,** depending on the platform and on section 10's libraries. Most of it is the runtime image (above), `libz3`, gnuplot and the frontend's jars, whose size is not known before section 10 fixes its libraries. The parts that were measured:

  | Part | Size | How measured |
  |---|---|---|
  | `libz3` 5.3.3 | 41.8 MB on Linux, 33.7 MB on macOS (x86-64), 17.5 MB on Windows | uncompressed sizes in the official archives' directories; the Linux library will be built from source (C3) |
  | Visual C++ runtime (Windows) | at most 1.5 MB: the 5.3.3 Windows archive carries nine runtime DLLs, 1,523,592 bytes | the archive's directory; only the DLLs `fusion.exe` and `libz3.dll` import are shipped |
  | OpenJFX 21.0.13 | 15.8 MB on Linux to 18.5 MB on Windows, unpacked, for `javafx.base`, `javafx.graphics` and `javafx.controls` | the jars' file lists on Maven Central (table under the Gradle build); the Windows `javafx.graphics` jar brings its own C runtime DLLs, `ucrtbase.dll` (982,720 bytes) among them |
  | gnuplot on Linux | about 15.1 MB: binary 2.2 MB (`gnuplot-qt`), cairo and pango with everything they load 12.9 MB | `ldd` on `libcairo.so.2` and `libpangocairo-1.0.so.0`, their 37 libraries summed without glibc (29 without X11 and xcb, 11.2 MB); the full Ubuntu closure with Qt, wxWidgets and ICU is 110 libraries, 105.5 MB |
  | gnuplot on Windows | `gp605-win64-mingw.7z` is 43.9 MB packed, before trimming to the files `gnuplot.exe` loads | release page |
  | fonts | 825,388 bytes (Regular 410,820, Bold 414,568) | `fonts-liberation` 2.1.5 files |
  | reference maps and the KGML list | FuSION 1's maps were 300,834 bytes (153,153 and 147,681); the rebuilt ones are made in Phase 4; `kegg_pathways.txt` is 1,503 bytes | FuSION 1's `Linux/frontend/` copies; the 167 ids |
  | `fusion` | not known until it exists; FuSION 1's 19.5 MB binary has 3.6 MB of code, and the rest is debug information | `size` |

### Continuous integration

GitHub Actions on `arjunjeewan/FuSION2`, once the user pushes (C15, accepted in entry 203). The suites are section 12's five tiers (12.C1, entry 206): CI runs the unit tier T0 and the end-to-end tier T1, which need only repository data, within budgets of 1 and 5 minutes per platform; the reference tiers T2 and T3 need the KEGG data root and run on this workstation, and the benchmarks B on demand.

| Workflow | Runs on | Jobs |
|---|---|---|
| `ci.yml` | every push and pull request | Linux release build in the `manylinux_2_28` image with T0 and T1; Linux Clang 18 Debug build with AddressSanitizer and UndefinedBehaviorSanitizer; frontend `gradle build`, with JavaFX's screen tests under a virtual X display (Xvfb), since the runner has no screen |
| `platforms.yml` | pull requests into `main`, weekly, and on demand | macOS arm64, macOS x86-64 and Windows x86-64 builds with T0 and T1; the cross-platform identity test; the oldest-GCC build, GCC 13 (C8 after C22); the twice-built reproducibility check; and, in the weekly run only, section 12's Linux ThreadSanitizer build |
| `package.yml` | version tags and on demand | the installers and archives of C12, a smoke test of each (`fusion --version --json`, one synthetic batch), artefacts uploaded to the run |

- **No KEGG data and no secrets.** CI runs only synthetic fixtures (12.C3), because no KGML may be stored in the repository (S8.31). The reference tiers on real runs stay on this machine, as does section 12's once-off golden search at the end of Phase 4. Workflows need no credentials; signing secrets would be the first ones, before the first public release (open question 3). Decision 0006 is unchanged: the user pushes.
- **Caches:** the Z3 dependency prefix per version and toolchain, FetchContent downloads, ccache (Linux and macOS) or sccache (Windows), and Gradle's caches. GitHub gives each repository 10 GB of cache.
- **Pinned actions.** Third-party actions are pinned by commit SHA, as Z3's own workflows now are (5.3.0 release notes).
- **Cost.** The repository is private, so minutes are billed: $0.006 a minute on Linux, $0.010 on Windows and $0.062 on macOS, with $12 a month included on GitHub Free and $18 on Pro (GitHub's billing page, 2026-11-12). Nothing is built yet, so job times are assumptions. At 15 minutes a job:
  - a `ci.yml` run (two Linux jobs) costs $0.18;
  - a `platforms.yml` run (two macOS jobs, one Windows job, two Linux jobs) costs $2.19, of which $1.86 is macOS, and the weekly ThreadSanitizer job adds $0.09;
  - $12 covers, for example, 4 weekly `platforms.yml` runs ($9.12 with ThreadSanitizer) and 16 `ci.yml` runs ($2.88) a month.

  Hence Linux on every push, and the other platforms weekly and before merges.

### Licences of everything shipped

| Component | Licence | What shipping it requires |
|---|---|---|
| FuSION 2.0 | MIT ([decision 0015](../decisions/0015-github-initial-commit-merge.md)) | the notice; FuSION 1's notice with any file copied from it |
| Z3 | MIT | the notice |
| pugixml, nlohmann/json, fast_float | MIT (fast_float under its MIT option) | the notices |
| Catch2 | BSL-1.0 | nothing; tests only, never shipped |
| gnuplot 6.0.5 | gnuplot licence: copying and distribution allowed with the notice; modified versions only as patches, with a distinct version | the notice; no source changes, so no patches |
| cairo, pango, glib (with gio), fribidi, libthai, libdatrie, graphite2, libmount, libblkid (among gnuplot's 37 libraries) | LGPL-2.1 or later, or LGPL-2.0 or later (cairo also MPL-1.1) | shipped as separate shared libraries a user can replace; licence texts; the exact sources offered with each release |
| harfbuzz, fontconfig, freetype, pixman, libpng, zlib, expat, pcre2, libffi, brotli, bzip2, libselinux, libbsd, libmd, the X11 and xcb libraries | MIT-style, FTL, BSD-style, zlib, libpng and bzip2 licences; libselinux is public domain | the notices; Phase 4 lists each library of the built bundle with its licence |
| Visual C++ runtime DLLs (Windows, C23) | Microsoft's licence terms for the Visual C++ redistributable files | shipped unmodified, next to the program that uses them, as those terms allow |
| Liberation Sans 2.1.5 | SIL OFL 1.1 | the licence text; the font is not sold on its own |
| Temurin 21 runtime | GPL-2.0 with Classpath Exception | the `legal/` notices that jlink copies; the offer of the matching OpenJDK source |
| OpenJFX 21.0.13: `javafx.base`, `javafx.graphics`, `javafx.controls` | GPL-2.0 with Classpath Exception | as for the Java runtime: the `legal/` notices that jlink copies from the jmods, and the offer of the matching OpenJFX source; `javafx.web` and `javafx.media` are not shipped (10.C6) |
| Spring Boot 4.1 and its dependencies | mostly Apache-2.0; Logback is EPL-1.0 or LGPL-2.1 | a generated report of every jar (Gradle license-report plugin) |
| vis-network 10.1.5 (07.C3) | Apache-2.0 or MIT | its notice in `LICENSES.txt`, written once per results directory with the library (S6.18) |
| busybox-w32 (Windows, 01.C10) | GPL-2.0 | shipped as a separate program, never linked; its licence text and the exact source offered with each release |
| GSEA's core ported from gseapy 1.3.1, and the generator ported from Rust's `rand` crate (08.C1, entry 159) | BSD-3-Clause; MIT | both notices ship with libfusion's |
| Graphviz (07.C5) | EPL-1.0 | nothing: not shipped; the user installs it for `static=svg` |
| `data/hsa_to_gene_symbol_map.txt` | NCBI Gene's data, free to redistribute | its source and date in `data/SOURCES.md` (S8.31) |
| `data/hsa_path_to_path_name_map.txt` | Reactome's pathway names, CC0 (corrected from CC BY 4.0, entry 315) | the attribution, source and date in `data/SOURCES.md` and in `THIRD_PARTY_NOTICES.txt`; no KEGG pathway names ship (entry 205) |
| `data/kegg_pathways.txt` | 167 bare pathway ids, no KEGG content | its source in `data/SOURCES.md` |

`packaging/` generates `THIRD_PARTY_NOTICES.txt` from `third_party/` and the Gradle report. The package step fails if a file in the bundle belongs to no listed component.

### Settings, events, errors and files

- **Settings.** This section adds no result-changing setting, so it adds no key to section 09's registry (whose keys are `<module>.<name>`, cross-section item 1.16). The build options do not change results (S1.29). C20 adds three environment variables, recorded in `run.json` under `environment` (above): `FUSION_HOME` (path; default: `<prefix>/share/fusion`, worked out from the executable), `FUSION_GNUPLOT` (path; default: the bundled gnuplot) and `FUSION_DOT` (path; default: `dot` on `PATH`). `exec` children get the bundled tools directory appended to their `PATH` (C20).
- **Events.** Dotted names under the key `type`, in section 09's envelope `v`, `seq`, `t`, `type` (cross-section item 1.3, entry 190). The check runs before the first step, so these events carry no `step`, `line` or `point`:

  ```json
  {"v": 1, "seq": 3, "t": "2026-11-20T09:14:02.118Z", "type": "deps.checked", "id": "gnuplot", "ok": true,
   "version": "6.0.5", "origin": "bundled", "features": ["pngcairo"], "missing": [],
   "needed_by": ["plot_pareto (line 262)"], "ms": 41,
   "path": "/opt/fusion/lib/app/backend/share/fusion/tools/gnuplot/bin/gnuplot"}
  {"v": 1, "seq": 4, "t": "2026-11-20T09:14:02.131Z", "type": "deps.checked", "id": "font:Liberation Sans",
   "ok": true, "origin": "bundled"}
  ```

  A failure goes through section 09's `error` event with its `install.*` or `deps.missing` code (cross-section item 2B.16, entry 158), and a missing font through 09's `warning` event with the code `deps.font_missing` (entry 230).
- **Errors.** Every error here is a `core::Failure` that stops the batch with exit status 1 (S1.28, 09.C14, 01.C14; startup install errors too, by 2B.16). None is a reported result (S4.18, S7.03).

  | Code | When | Message, for example |
  |---|---|---|
  | `install.home_missing` | startup, before the batch is read | `fusion: fusion-data.json not found in /opt/fusion/lib/app/backend/share/fusion (from /opt/fusion/lib/app/backend/bin/fusion); set FUSION_HOME or reinstall` |
  | `install.version_mismatch` | startup | `fusion: the home belongs to FuSION 2.0.0-dev+g1a2b3c4, this binary is 2.0.0` |
  | `install.file_missing`, `install.file_damaged` | `fusion --version` and any use of a bundled file | `fusion: data/kegg_pathways.txt does not match its SHA-256 in fusion-data.json` |
  | `install.z3_mismatch` | startup | `fusion: linked Z3 is 4.8.12.0 but this build needs 5.3.3.0 (check LD_LIBRARY_PATH)` |
  | `deps.missing` | before the first step | `line 262 (plot_pareto) needs gnuplot 5.4 or later with the pngcairo terminal; none found (FUSION_GNUPLOT unset; /opt/fusion/lib/app/backend/share/fusion/tools/gnuplot/bin/gnuplot missing; not on PATH)` |

  A tool that fails during a step is reported by that step under its own code, through `run_process` or `run_gnuplot`, with the exit code and the last 20 lines of stderr: 04's `expression.gnuplot_failed` and 07's `results.gnuplot_failed` (entry 230). A missing font is no error but the warning `deps.font_missing`, which the dependency check raises before the first step (07.C8, entry 230).

- **Files.** This section writes no file into a run directory. It supplies the `run.json` blocks above.

## Choices

### 11.C1 · Target platforms and oldest supported versions

- **Proposed:**
  - Linux x86-64 with glibc 2.28 or later, built in the `manylinux_2_28` image (AlmaLinux 8). That covers RHEL, Rocky and Alma 8 to 10, Debian 10 and later, and Ubuntu 20.04 and later.
  - macOS 13.3 or later, as two builds, arm64 and x86-64. The x86-64 build continues while GitHub offers Intel runners: `macos-15-intel`, the last such image, is announced as supported until about August 2027.
  - Windows 10 22H2 and Windows 11, x86-64. Windows on Arm runs it under emulation.
  - No Linux arm64 or native Windows arm64 builds at first.
- **Alternatives:**
  - Raise the Linux floor to glibc 2.39 (Ubuntu 24.04, Debian 13, RHEL 10), so that the official Z3 archives serve every platform (C3).
  - Add native Linux arm64 and Windows arm64 builds now. Z3 ships `arm64-glibc-2.38` and `arm64-win` archives and gnuplot ships `gp605-woa.exe`.
  - macOS arm64 only.
- **Why:**
  - Labs and clusters commonly run RHEL-family 8 or 9 (glibc 2.28, 2.34), Debian 12 (2.36) and Ubuntu 22.04 (2.35). The official Z3 Linux archives, FuSION 1's binary and Ubuntu 24.04's `libz3` all need `GLIBC_2.38` and `GLIBCXX_3.4.32` (measured above), so none of them starts there.
  - macOS 13.3 is the oldest macOS whose system libc++ has `std::to_chars` for `double`, which S1.27's exact numbers need. Z3's macOS archives are built for the same version (`arm64-osx-13.3`, `x64-osx-13.3`).
  - Every extra target is another CI job and another test pass. macOS minutes cost 10.3 times Linux minutes.
- **Decision:** as proposed (prompt log entry 200)

### 11.C2 · The Z3 version

- **Proposed:** pin Z3 5.3.3 (2026-10-29), record it in `run.json`, and change it only by a new decision. Because a newer Z3 was measured slower here, the pin is gated: before Phase 4 starts, section 12's experiment X11 runs the real dumps through 4.8.12, 4.16.0 and 5.3.3, in both modes (about 5 h on 16 workers), and the user re-decides if 5.3.3 is slower than 4.8.12 at the median. Z3 is configured as FuSION 1 configured it, one combined QF_BV solver per case (06.C1), and each case runs in its own thread and context (06.C4).
- **Alternatives:**
  - 4.8.12, which FuSION 1 links on Linux: the fastest version measured here, but with no Apple silicon build.
  - 4.16.0 (2026-02-19), the last 4.x. FuSION 1's macOS build was tested with 4.15.4.
  - 5.4.1 (2026-11-11), the newest release.
- **Why:**
  - Speed favours the old version so far. In the Z3 comparison, 4.15.3 took 1.48 times as long as 4.8.12 at the median on the tactic path and 1.24 times on the incremental path. On fdr050's harder dumps, 4.8.12 finished 10 of 14 within 600 s and 4.15.3 none. Solves of 15.9 h and 50.7 h make that matter. 5.x was not measured, because it is not on this machine.
  - Results favour no version: every check that finished under both gave the same answer, so frontiers agree wherever no check is left unknown, and every SAT model differed, so witnesses change with any version change. 2.0's witnesses differ from FuSION 1's even on 4.8.12, because its encoding and term order differ.
  - Platforms favour 5.x. Only the 5.x line claims identical output across platforms: 5.3.0 fixes "random-seed parsing" and "AST-construction order" for that purpose, after 5.1.0's work "for cross-compiler determinism". With three platforms (G3), that makes it likely that a biologist on Windows sees the same explanation as the Linux run in a paper, although 12.C11's promise leaves witnesses out. 5.3.0 also fixes thread-safety defects in parallel solving reached through QF_BV, which matters now that each case runs in its own thread (06.C4).
  - 4.8.12 has no official Apple silicon build: Darwin arm64 support began in 4.8.16 and native builds in 4.9.0, so 4.8.12 would have to be built from source, possibly with fixes, and Homebrew no longer carries it. It also lacks the warm starts from nearby models that section 06 mentions.
  - 5.4.1 came out one day after 5.4.0, and 5.4.0 changes `Z3_mk_power`'s semantics, which FuSION does not use. 5.3.3 has stood for two weeks as the last 5.3 release.
- **Decision:** as proposed (prompt log entry 173)

### 11.C3 · Where each platform's Z3 comes from

- **Proposed:**
  - macOS and Windows: the official release archives, checked against the SHA-256 digests on the release page.
  - Linux: the official source archive (`z3-5.3.3-source.tar.gz`, SHA-256 `6cc8c5d6…11aa`), built once per toolchain in the `manylinux_2_28` image and cached.
  - `FUSION_Z3_PROVIDER=SYSTEM` accepts an installed Z3 only if its version equals the pin. This is how this section reads G1's "from the system or a pinned download".
- **Alternatives:**
  - The official binary archives on Linux too, with the glibc 2.39 floor of C1's first alternative.
  - Build Z3 from source on all three platforms, with one set of flags everywhere.
  - On Linux, the `libz3.so` from Z3's official `manylinux_2_27_x86_64` Python wheel (38.6 MB packed for 5.4.1), which needs only glibc 2.27.
- **Why:**
  - The glibc floors measured above rule out the official Linux archive for a 2.28 floor, and only Linux has that problem.
  - A source build costs CI time once per Z3 version and toolchain, because the result is cached. It is cheapest on Linux ($0.006 a minute) and dearest on macOS ($0.062).
  - The official macOS and Windows builds are the ones Z3's own cross-platform determinism checks run on.
  - A wheel is an odd place to take a C++ library from, and it ties the Linux build to Z3's Python packaging.
- **Decision:** as proposed (prompt log entry 200)

### 11.C4 · Pinning and verifying fetched dependencies

- **Proposed:** pin every artefact by version and SHA-256 in one file per build:
  - C++: CMake `FetchContent` with `URL` and `URL_HASH SHA256` in `cmake/FusionVersions.cmake`, and `FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER`;
  - Java: the version catalog plus Gradle's dependency verification (`verification-metadata.xml`, SHA-256 for every jar and plugin), and the wrapper's `distributionSha256Sum`.
- **Alternatives:**
  - vcpkg in manifest mode with a pinned baseline. It could also build gnuplot's cairo and pango stack the same way on every platform (C9), at the cost of another tool and long first builds.
  - Conan 2 with lockfiles.
  - Vendor the C++ libraries' sources into `third_party/`. This would amend G1, which has the small libraries "fetched at pinned versions by the build".
  - Gradle without dependency verification: versions pinned, contents not checked.
- **Why:**
  - FuSION 1 pinned nothing: the READMEs say `pip install pyvis gseapy` with no versions, and the repository has no requirements file. S7.02 and S7.04 found the cost: pandas 3 removed an argument that gseapy 1.3.1 still passes, so an `inf` in the LFC file now crashes the GSEA step.
  - The four C++ libraries are small, have no dependencies and build with CMake, so FetchContent needs no extra tool.
  - Vendoring would hide version changes inside large diffs.
  - Dependency verification makes a changed or substituted jar fail the build. Its cost is regenerating the file whenever a version changes.
- **Decision:** as proposed (prompt log entry 201)

### 11.C5 · fast_float as the one decimal parser

- **Proposed:** add fast_float 8.3.3 (header-only; MIT, Apache-2.0 or BSL-1.0) as the parser behind every decimal number the backend reads, under one grammar: a token counts only when the whole of it matches `[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?`. One function applies it, section 04's `core::parse_decimal`, so fast_float links privately to `core` alone. It serves values and thresholds (section 04's one reader, S3.14), every real argument of the batch language (01) and edge scores (02). A value that does not match is skipped and reported (S3.04); a threshold that does not match is an error (S3.02). Numbers are written by section 09's `core::format_number`, with `std::to_chars`. With 04.C2 this refines S3.02's "finite number", so `0x1p0` is refused, and settles S3.03's prefix reading, under which `1,5` read as 1 (cross-section item 3A.19, accepted in entry 167).
- **Alternatives:**
  - `std::from_chars` where the standard library has it, with `strtod_l` in the C locale on macOS.
  - `strtod` in the C locale everywhere.
- **Why:**
  - No standard function parses a `double` on all three targets: `std::from_chars(double)` is in libstdc++ and MSVC, but in libc++ only from LLVM 20, behind an availability check that a macOS 13.3 target fails.
  - `strtod` follows the process locale. A locale with a decimal comma would misread `-0.06`.
  - fast_float is correctly rounded and locale-free, so every platform reads every value to the same `double`. libstdc++ itself uses fast_float inside its `from_chars`.
  - The cost is one header-only dependency. Speed is not the reason: the real LFC file has 3,054 lines.
  - No real input changes: all 6,372 value tokens of FuSION 1's five two-column files, and all 22,283 values of the probe table's HeLa.EGF.120 column, match the grammar (section 04's count).
- **Decision:** as proposed (prompt log entry 167)

### 11.C6 · CMake targets: one static library per module

- **Proposed:** one static library per backend module, each with its own include directory, joined in an INTERFACE target `fusion::libfusion`; `fusion` is `main.cpp` alone. Z3 is linked privately to `solve` only, pugixml privately to `network` only and fast_float privately to `core` only. Module dependencies are link edges, so an include that breaks the layering fails to compile. The edges are those of the CMake table, with cross-section item 1.2's two additions (entry 201): `expression` links `network`, and `merge` links `expression`. Section 04's matcher takes a `network::GraphView`, `cudf`'s handler lives in `batch` with every other handler, the gnuplot runner lives in `core`, `results` depends on `solve`, and `batch` calls `publish_solve` after the solve step, which removes the three cycles the sections' needs formed.
- **Alternatives:**
  - One static `libfusion` target holding every module's sources, with layering kept by review.
  - A shared `libfusion`: a DLL on Windows needs export annotations on every public symbol, and only `fusion` and the tests use it.
- **Why:**
  - B1 asks for a library boundary, and C1 gives ten modules. Per-module targets make the boundaries visible to the compiler at no runtime cost.
  - Parsing `z3++.h` costs 0.84 to 0.96 s and 125 to 134 MB per translation unit (measured above); confined to `solve`, it is paid only there, and a Z3 change recompiles only `solve`.
  - Tests can link the one module they test.
- **Decision:** as proposed (prompt log entry 201)

### 11.C7 · Floating-point and optimisation flags

- **Proposed:** `-O2` with link-time optimisation for releases; `-ffp-contract=off` on GCC and Clang and `/fp:precise` on MSVC; no `-ffast-math`, `-Ofast` or `-march=native`; the baseline x86-64 and arm64 instruction sets. The same flags apply to Z3 where it is built from source. The promise is byte-identical canonical outputs on all three platforms (09.C16, 12.C11), so no result path calls the platform's `exp`, `log` or `pow`; one that needs them uses a single correctly rounded implementation compiled into libfusion.
- **Alternatives:**
  - Compiler defaults, with `-march=x86-64-v3` (AVX2 and FMA) for speed.
  - Per-machine builds with `-march=native`.
  - Contraction off, but a promise of agreement within a tolerance instead of byte identity, which would change the identity test (C15) and 12.C11.
- **Why:**
  - GCC's default in its GNU mode is `-ffp-contract=fast` and Clang's is `on`. On arm64, where fused multiply-add is in the base instruction set, `a*b+c` then becomes one fused operation with one rounding instead of two. An Apple silicon build would then compute GSEA's scores differently in the last bits from an x86-64 build.
  - GSEA's NES and FDR are compared against cut-offs such as `fdr:0.25` (S7.13), so a last-bit difference can move a pathway across a cut-off. In FuSION 1 the machine already changed a shortlist: the same inputs selected 42 pathways on AVX-512 and 44 on other CPUs, through numpy's CPU-specific sort (S7.05). S7.05 removed that cause; this removes the floating-point one. Sections 04 and 08 require contraction off for the same reason.
  - FuSION's hot paths are graph searches and Z3's integer and Boolean reasoning, where contraction buys almost nothing. FuSION 1 runs unoptimised, so `-O2` alone is a large gain.
- **Decision:** as proposed (prompt log entry 186)

### 11.C8 · Compilers per platform

- **Proposed:** GCC 14 (`gcc-toolset-14` in `manylinux_2_28`) for Linux releases, with CI also building with Clang 18 every time and weekly with GCC 13, the oldest accepted after C22; Apple Clang from Xcode 16.4 or later on macOS; MSVC 19.44 (Visual Studio 2022 17.14) or Visual Studio 2026 on Windows, with MSVC 19.40 (Visual Studio 2022 17.10) as the oldest accepted. G1 names no MSVC minimum, because Windows joined only with G3.
- **Alternatives:**
  - clang-cl on Windows, so that every platform uses a Clang front end.
  - MinGW-w64 GCC on Windows, so that Linux and Windows share GCC. Z3's official Windows build is MSVC-built, so this would also mean building Z3 from source on Windows.
- **Why:**
  - `manylinux_2_28` gives a current GCC on a glibc 2.28 base, and its toolset links the newer parts of libstdc++ statically, so the binary keeps the 2.28 floor.
  - A Clang build on Linux catches what Apple Clang would reject before a macOS job is paid for.
  - MSVC matches the ABI of Z3's official Windows archive and of the Windows debugging tools.
  - 19.40 is a year older than the release compiler and already has every C++20 feature in the subset above, `std::format` included.
- **Decision:** as proposed (prompt log entry 201)

### 11.C9 · gnuplot on each platform

- **Proposed:** bundle gnuplot 6.0.5 on every platform:
  - **Windows:** the official `gp605-win64-mingw.7z` (43.9 MB), checked by its published SHA-256 and trimmed to `gnuplot.exe` and the libraries it loads;
  - **macOS:** built in CI from the 6.0.5 source archive with only the cairo terminals (no Qt, wxWidgets or X11), against Homebrew's cairo and pango, with the libraries copied into the bundle and relinked to `@rpath`;
  - **Linux:** built the same way in the `manylinux_2_28` image, with its libraries collected and run paths set to `$ORIGIN/../lib`.
- **Alternatives:**
  - The Linux `.deb` and `.rpm` depend on the distribution's gnuplot (`gnuplot-nox`, `gnuplot`) instead of bundling it. The package manager then installs it with FuSION, but versions differ: Ubuntu 24.04 has 6.0.0 and Ubuntu 22.04 has 5.4.2.
  - Build gnuplot and its whole cairo and pango stack from source on all three platforms, for example through vcpkg (C4). One set of library versions everywhere, much more CI work.
  - If section 07 draws only SVG, a gnuplot without cairo needs no extra libraries and could be linked statically everywhere.
- **Why:**
  - G3 asks that a biologist install one thing, with gnuplot included. No official macOS binary exists, and gnuplot 6.0.6 has no Windows binary yet, so 6.0.5 is the newest version available on all three platforms.
  - On Linux, cairo and pango with everything they load come to 37 libraries and 12.9 MB without glibc, 8 of them for X11, plus a 2.2 MB binary. The Ubuntu package pulls in 110 libraries, 105.5 MB, because of its Qt and wxWidgets terminals (measured with `ldd` on this machine).
  - conda-forge's gnuplot is not an option: its recipe skips Windows and depends on Qt.
- **Decision:** as proposed (prompt log entry 201)

### 11.C10 · The plot font

- **Proposed:** ship Liberation Sans 2.1.5, Regular and Bold (825,388 bytes, OFL 1.1), fetched at packaging by pinned SHA-256, with a `fonts.conf` that lists only the bundled fonts. Every gnuplot plot runs with `FONTCONFIG_FILE` pointing at that file and `PANGOCAIRO_BACKEND=fc`, set by `core::run_gnuplot`, so the same font files draw the plot on every platform. If the font is still not available, with a system gnuplot, the dependency check gives the warning `deps.font_missing` before the first step and gnuplot substitutes another (07.C8, accepted with this choice in entry 179; the code from entry 230).
- **Alternatives:**
  - Use whatever the system resolves for "Liberation Sans" and warn when it substitutes another. On Windows and macOS that is usually Arial, which has the same metrics.
  - Install the font into the operating system at install time: per user on Windows, `~/Library/Fonts` on macOS.
- **Why:**
  - S6.23 keeps the look and asks for the font to be checked. fontconfig substitutes silently, which is S6.23's own problem. Liberation Sans is not installed by default on macOS or Windows.
  - Pinning the font files also makes the image depend on FuSION's version rather than on the machine's fonts.
  - Pango's fontconfig back end is selected with `PANGOCAIRO_BACKEND` on Windows and macOS; whether the official Windows gnuplot build includes it is a Phase 4 check (open question 5), with a per-user font install as the fallback.
  - A missing font changes only the look, so a warning is enough: a 50-hour solve should not fail over a font (07.C8).
- **Decision:** as proposed (prompt log entry 179)

### 11.C11 · Internal assets compiled into libfusion

- **Proposed:** compile into `libfusion`, as byte arrays generated by CMake (`cmake/FusionAssets.cmake`) with their SHA-256 recorded, the files the code cannot work without: section 07's `vis-network.min.js` (10.1.5, pinned here; 9.1.2 instead if Phase 3's check of the full JUN run's 13 points shows a visible change, 07 open question 7), `fusion-explain.js`, `fusion-explain.css` and `LICENSES.txt`, which S6.18 writes once per results directory. Files a user may read or replace stay as files in the home: the reference maps, the KGML list, the default pipeline, the fonts and the licences.
- **Alternatives:**
  - Everything as files in the home, so assets can be patched without a rebuild.
  - Everything compiled in, the maps included.
- **Why:**
  - S1.24 shows the failure this avoids: FuSION 1 refused to start when a file at a baked-in path was missing. An asset inside the binary cannot be missing or mismatched. Section 07 already plans its page files this way.
  - The cost is the asset size in the binary: vis-network 10.1.5 is 653,713 bytes (07.C3).
  - The maps are inputs that S8.31 ships with their source and date, so they stay visible files.
- **Decision:** as proposed (prompt log entry 177)

### 11.C12 · Installers for a JavaFX desktop application: jpackage, plus a backend-only archive

*Revised for 10.C1's JavaFX desktop application (entry 192), with 10.C6's system browser (entry 198) and the console launcher of entry 199.*

- **Proposed:**
  - jpackage builds one installer per platform: a per-user `.msi`, one `.dmg` per macOS architecture, and a `.deb` and an `.rpm`.
  - Each holds a jlink'd runtime with the JDK modules needed and OpenJFX's `javafx.base`, `javafx.graphics` and `javafx.controls`, with no `javafx.web` (10.C6), the frontend's jars on the class path, and the backend tree.
  - The main launcher starts a normal windowed application, with a Dock icon, taskbar button or desktop entry and the platform's Quit (10.C4). On Windows, a second launcher with a console, `fusion-frontend`, runs `fusion-frontend --reset-password <username>`.
  - Development builds are unsigned; signing and notarisation are decided before the first public release (open question 3).
  - Each release also has a backend-only archive per platform for command-line and server use.
- **Alternatives:**
  - A commercial packager such as Conveyor or install4j, which builds every platform from one machine and adds automatic updates.
  - Separate downloads: the backend archive, the frontend's jars and a Java with JavaFX that the user installs.
- **Why:**
  - G3 asks for one install per platform with everything inside. jpackage is part of the JDK (no extra licence), makes native installers, and supports per-user Windows installs that need no administrator rights.
  - jpackage descends from JavaFX's own packager and takes JavaFX modules in the runtime image as they come (C25). Its launcher is an ordinary application, so there is no browser to open, no port and no background agent.
  - Password resets go through the command line (10.C10), and on Windows only a console launcher gives that command a console.
  - The backend archive serves the way FuSION 1 is actually used, from shell scripts on a server, with no Java and no root.
  - Estimated installed size: about 120 to 210 MB (see Packaging). Leaving `javafx.web` out saves 102.4 to 118.5 MB unpacked.
- **Decision:** as proposed (prompt log entry 202)

### 11.C13 · Gradle build script and Java toolchain

- **Proposed:** Kotlin DSL build scripts, a version catalog (`gradle/libs.versions.toml`) and the Gradle 9.9.1 wrapper, which Spring Boot 4.1 supports (C21). The Java toolchain asks only for language version 21, any vendor, and downloads Temurin 21 when no JDK 21 is installed. The shipped runtime is pinned apart from it: the packaging step downloads the Temurin 21.0.13+9 JDK by URL and SHA-256 and runs `jlink` and `jpackage` from it.
- **Alternatives:**
  - Groovy DSL build scripts.
  - A toolchain that asks for vendor Adoptium as well, so that compiling also uses Temurin. On this machine it would refuse the installed Ubuntu OpenJDK and download Temurin.
  - Require a locally installed JDK 21 and never download one, and build the runtime image from whatever JDK is installed.
- **Why:**
  - The Kotlin DSL is checked as it is edited and is Gradle's default for new builds.
  - Spring Boot 4.1.2 supports Gradle 8.14 or later and 9.x, so the wrapper is the current 9.9.1 (2026-11-10). The draft's 8.14.7 was for Spring Boot 3.5, which supports no Gradle 9.
  - A Gradle toolchain can name a language version, a vendor and an implementation, but not an update release such as 21.0.13+9. The exact runtime that users get therefore has to be pinned where the image is made, which also keeps the installers of one release identical whatever JDK compiled the jar.
  - This machine has OpenJDK 21.0.13, so builds here download nothing.
- **Decision:** as proposed (prompt log entry 202)

### 11.C14 · Repository layout

- **Proposed:** the layout shown in Design: `backend/` (modules with their assets, CLI, tests), `frontend/` (one Gradle project), `share/fusion/` (the data, default pipeline and font configuration that go into the home), `packaging/`, `third_party/` (licence texts only), `cmake/`, `.github/workflows/` and the existing `docs/`, with one version in `VERSION`.
- **Alternatives:**
  - The C++ sources at the top level (`src/`, `include/`, `tests/`) and the frontend beside them.
  - Two repositories, backend and frontend. Decision 0001 created one repository, so this would need a new decision.
- **Why:**
  - Two build systems live side by side without nesting. Packaging spans both, so it has its own directory.
  - Per-module directories mirror the CMake targets (C6) and the modules of C1.
  - Nothing generated is tracked, which fixes S8.01 and S8.31. `CLAUDE.md` stays at the root (decision 0005).
- **Decision:** as proposed (prompt log entry 202)
- **Amended:** [decision 0040](../decisions/0040-readme-and-running.md) adds `RUNNING.md`, the run instructions, to the root beside `README.md` (prompt log entry 384).

### 11.C15 · Continuous integration

- **Proposed:** GitHub Actions with the three workflows above:
  - Linux on every push;
  - macOS, Windows, the cross-platform identity test, the oldest-GCC build (GCC 13, after C22) and the reproducibility check on pull requests into `main`, weekly and on demand, with section 12's ThreadSanitizer build weekly;
  - packaging on version tags.

  Actions are pinned by commit SHA, the Z3 prefix and compiler caches are cached, and no secret exists until signing is decided.
- **Alternatives:**
  - The full matrix on every push.
  - A self-hosted runner on this workstation.
  - No CI until the repository is public, when standard runners are free.
- **Why:**
  - The repository is private, so minutes are billed. A macOS minute costs 10.3 times a Linux minute, and a `platforms.yml` run would cost about 12 times a `ci.yml` run (see Continuous integration).
  - A self-hosted runner needs a GitHub registration token on this machine, against decision 0006's "no credentials here". It would also compete with the solves that run here for days (fdr050 took 50.7 h).
  - Without CI, the Windows and macOS builds would be tested only by hand.
- **Decision:** as proposed (prompt log entry 203)

### 11.C16 · One version number for the repository

- **Proposed:** a top-level `VERSION` file (for example `2.0.0-dev`) read by CMake and Gradle. The commit and dirty flag are stamped at configure time, releases are tagged `v2.y.z`, and the frontend refuses a backend whose version differs from its own. Section 09's build id, which goes into every resume fingerprint (09.C5), is this version plus the commit, plus the executable's SHA-256 for a build with uncommitted changes.
- **Alternatives:**
  - Separate versions for the backend and the frontend, with a compatibility table.
  - Versions taken from git tags only (`git describe`).
- **Why:**
  - The frontend and backend share the batch language, `events.jsonl` and `run.json` (A1, A2, E1), so they change together and ship in one installer (C12).
  - A version taken from git alone is lost in a source archive without `.git`. A file is not.
- **Decision:** as proposed (prompt log entry 203)

### 11.C17 · Linking Z3 as a shared library

- **Proposed:** link Z3 as a shared library on every platform, ship it beside `fusion` (`lib/`, or `bin\` on Windows), find it through the run path, and check its version at startup (`solve::verify_z3()`).
- **Alternatives:**
  - Link Z3 statically into `fusion`. That is possible from a source build on Linux and macOS, and the official Linux archives carry a static library (4.15.3's `libz3.a` is 83,675,050 bytes). The official Windows archive has only `libz3.dll` and its import library, so Windows would need its own source build.
- **Why:**
  - The official macOS and Windows archives that C3 uses ship shared libraries, so one way of linking serves all three platforms.
  - A static `fusion` would be one file fewer and could never meet a mismatched library, but it would need Z3 source builds on macOS and Windows too, at $0.062 and $0.010 a CI minute.
  - The startup check already turns a mismatched library into a clear error before any work.
- **Decision:** as proposed (prompt log entry 203)

### 11.C18 · Z3's own Ctrl-C handling: off

- **Proposed:** `solve` sets the solver parameter `ctrl_c` to false on every solver it creates, so a Ctrl-C during a check reaches only the `fusion` executable's handler, which turns it into the stop request. With 09.C8 this is a reading of A1, not an amendment (entry 185): the handlers for Ctrl-C and SIGTERM, and on Windows for Ctrl-C, Ctrl-Break and the console's closing, only set the flag the watcher reads; a second Ctrl-C ends the process with status 130; SIGHUP is ignored on Linux and macOS.
- **Alternatives:**
  - Keep Z3's default, true. A Ctrl-C then ends the running check at once with `unknown`, as Z3's own cancellation.
  - Set the global parameter instead. Only newer versions list it: 4.15.3 does, 4.8.12 does not.
- **Why:**
  - A1 decided that a stop saves state for resuming, and sections 01 and 09 route Ctrl-C into that stop. If Z3 caught SIGINT itself, the check would end as an ordinary unknown answer, which the search records and caches (S5.19, S5.20), and no stop would happen.
  - The solver parameter works on both versions measured: with it set to false, SIGINT reached the program's own handler on 4.8.12 and on 4.15.3 (a probe program run during this section's review).
- **Decision:** as proposed (prompt log entry 185)

### 11.C19 · Where the four platform components live (amends C1's description of `core`)

- **Proposed:** put build information, the install directory, the platform layer and the dependency registry in `core`, beside section 09's run-level parts. C1 lists `core`'s duties as "ids, errors, logging and events, the settings registry, … seeded random numbers and stable orderings", so this adds four duties to C1's text: it amends skeleton C1's description of `core` (accepted in prompt log entry 204).
- **Alternatives:**
  - A new eleventh backend module, `platform`, which amends C1's module list.
  - Build information and the install directory in `run`; the process layer and the dependency registry in `batch`.
- **Why:**
  - Every module and the executable need them: `batch` runs `exec` and the checks, `results` and `expression` start gnuplot, `solve` needs per-check CPU time, and `run` records the versions. `core` is the only module that every other links (C6).
  - Section 09 already puts path conversion, process CPU time, peak memory and `build_id()` in `core` (09's outline), so `core` holds platform code either way.
  - A separate module would add a target and a layer for four small headers.
- **Decision:** as proposed, amending skeleton C1's description of core (prompt log entry 204)

### 11.C20 · Environment overrides for the home and the tools

- **Proposed:**
  - three environment variables, each recorded in `run.json` under `environment`: `FUSION_HOME`, the home directory; `FUSION_GNUPLOT` and `FUSION_DOT`, the paths of those programs;
  - `exec` lines get FuSION's bundled tools directory appended to their `PATH`, so a system gnuplot, as FuSION 1 used, still comes first; Phase 4 checks that the cairo-only bundled build accepts `set term png`.
- **Alternatives:**
  - Overrides, with `PATH` untouched: `exec` lines use only the user's own tools, as in FuSION 1.
  - Overrides, with the bundled tools first: every `exec` gnuplot line uses the bundled build, like `plot_pareto`.
  - Command-line options of `fusion`, `--home` and `--gnuplot`, in section 01's table instead of environment variables, with the bundled tools last.
- **Why:**
  - Tests need a fake home and fake tools without changing batch files (test hooks).
  - A lab may keep one site-wide gnuplot, and Graphviz is never bundled (07.C5).
  - Environment variables reach `fusion` however it is started, by the frontend, a script or a sweep's child processes, without touching the batch language. Recording them keeps a run reproducible. FuSION 1's launchers already steered the tools through `PATH` (`run_sweep.sh`), with no record.
  - 59 corpus batch files run gnuplot inside `exec` lines, 71 of those lines with `set term png`, and S6.25 keeps them working. On a machine without a system gnuplot they would fail unless `exec` can reach the bundled one; appending it keeps FuSION 1's behaviour wherever a system gnuplot exists.
- **Decision:** as proposed (prompt log entry 204)

### 11.C21 · Spring Boot 4.1 instead of Spring Boot 3 (amends G2)

- **Proposed:** amend G2 from Spring Boot 3 to Spring Boot 4.1 (4.1.2 today), with the Gradle 9.9.1 wrapper, before Phase 5 starts. This amends skeleton G2 (accepted in prompt log entry 200), and the design above follows it. With the desktop application (10.C1), Spring Boot is the application framework, with no web server.
- **Alternatives:**
  - Keep G2: Spring Boot 3.5.16, the last free 3.x release, with Gradle 8.14.7. Fixes after 2026-06-30 then come only with commercial support.
  - Spring Boot 4.0, whose free support ends on 2026-12-31.
- **Why:**
  - Free support (endoflife.date, 2026-11-12): 3.5 ended on 2026-06-30, with commercial support until 2032-06-30; 4.0 ends on 2026-12-31; 4.1, released on 2026-06-30, runs until 2027-07-31.
  - Spring Boot 4.1.2 runs on Java 17 to 27, so G2's Java 21 stands, and supports Gradle 8.14 or later and 9.x.
  - Phase 5 builds the frontend months from now, so a Spring Boot 3 frontend would ship without free security fixes. The price is the move to Spring Framework 7 and Jakarta EE 11, before any frontend code exists.
- **Decision:** as proposed, amending skeleton G2 (prompt log entry 200)

### 11.C22 · Compiler minimums GCC 13 and Clang 17 (amends G1)

- **Proposed:** raise G1's minimums from GCC 12 and Clang 15 to GCC 13 and Clang 17, so the code may use `std::format`. This amends skeleton G1 (accepted in prompt log entry 200), and the design above follows it: the weekly oldest-GCC build is GCC 13 (C8).
- **Alternatives:**
  - Keep G1's minimums and avoid `std::format`, as the design does now.
  - Keep G1's minimums and add the {fmt} library (MIT) for formatting.
- **Why:**
  - `std::format` arrives in libstdc++ with GCC 13 and in libc++, without the experimental flag, with LLVM 17.
  - Every release compiler of C8 already qualifies (GCC 14, Apple Clang from Xcode 16.4, MSVC 19.44), and so does this machine's GCC 13.3. GCC 12 remains only on older distributions, such as Debian 12 and Ubuntu 22.04, and releases are built in `manylinux_2_28` anyway.
  - The backend formats messages everywhere: errors, console milestones (S5.31), file headers.
- **Decision:** as proposed, amending skeleton G1 (prompt log entry 200)

### 11.C23 · The Visual C++ runtime on Windows: copied beside `fusion.exe`

- **Proposed:** copy the Visual C++ runtime DLLs that `fusion.exe` and `libz3.dll` import, from the Visual Studio used for the build, next to `fusion.exe` in `bin\`.
- **Alternatives:**
  - Run Microsoft's Visual C++ Redistributable installer from the `.msi`. Windows Update would then service the runtime, but the installer needs administrator rights, against C12's per-user install.
  - Link the C runtime statically (`/MT`). The official `libz3.dll` uses the DLL runtime, so this would also need a Z3 source build on Windows with `/MT`.
- **Why:**
  - The official 5.3.3 Windows archive itself carries nine runtime DLLs beside `libz3.dll` (1,523,592 bytes: `msvcp140*.dll`, `vcruntime140*.dll`, `vcomp140.dll`), so `libz3.dll` needs them, and a clean Windows machine may have none.
  - Microsoft's terms for the redistributable files allow copies beside the program that uses them. The cost is that each FuSION release must carry runtime fixes itself.
- **Decision:** as proposed (prompt log entry 204)

### 11.C24 · The OpenJFX version: 21.0.13

*New after 10.C1 (prompt log entry 192); polled together with section 10's 10.C3, which names the same version (entry 197).*

- **Proposed:** pin OpenJFX 21.0.13 (2026-10-20), the current update of the OpenJFX 21 long-term line, for both the jmods in the runtime image and the jars used to compile and test, and follow its quarterly updates as the Java 21 runtime is updated.
- **Alternatives:**
  - OpenJFX 23.0.2, the newest feature release that still runs on Java 21 (JavaFX 23 requires JDK 21 or later). It has had no update since 23.0.2, as the 22 line has had none since 22.0.2.
  - A current feature release, OpenJFX 27.0.1 or 26.0.2. JavaFX 26 requires JDK 24 and JavaFX 27 JDK 25, so either would amend G2's Java 21, for example to Java 25.
- **Why:**
  - OpenJFX raised its minimum JDK from release 23 on: JavaFX 22 still requires only JDK 17 (its CSR, JDK-8325993), while 23 requires JDK 21 (JDK-8321603), 24 requires 22, 25 requires 23, 26 requires 24, and 27 requires 25 (the OpenJFX release notes, and the changes JDK-8340003, JDK-8359387, JDK-8365402 and JDK-8376601). So 21.x, 22.x and 23.x run on the Java 21 that G2 chose, and of those only the 21 line is still updated.
  - The 21 line still receives public updates: Maven Central lists 21.0.1 to 21.0.13, the last published on 2026-10-20, alongside 21.0.13 of the JDK, while 22 stopped at 22.0.2 and 23 at 23.0.2.
  - Gluon publishes 21.0.13's jmods for all four targets (47,109,232 to 53,622,248 bytes per archive), and Maven Central its jars for Linux, macOS (x86-64 and arm64) and Windows.
  - JavaFX 21's release notes call it designed for JDK 21 and JDK 17, and on Linux it needs GTK 3.8 or later, which C1's oldest distributions have.
- **Decision:** as proposed (prompt log entries 197 and 205)

### 11.C25 · How JavaFX and the frontend's jars enter the bundle

*New after 10.C1 (prompt log entry 192).*

- **Proposed:**
  - jlink puts OpenJFX's three modules (`javafx.base`, `javafx.graphics`, `javafx.controls`) into the runtime image as named modules, from Gluon's jmods archive for each platform, pinned by version and SHA-256 in `cmake/FusionVersions.cmake`.
  - The frontend's own plain jar and its Spring Boot and other dependency jars go into jpackage's input directory, so they run on the class path.
  - The build compiles and tests against the same OpenJFX version's jars from Maven Central, declared directly with the platform classifier and covered by dependency verification (C4).
- **Alternatives:**
  - A Java runtime that already contains JavaFX, such as BellSoft's Liberica JDK Full or Azul's Zulu with JavaFX, in place of Temurin plus separate OpenJFX jmods. One download, but the runtime's vendor and its JavaFX version then come together.
  - The OpenJFX jars on the class path inside the application, with no jlink step for JavaFX. JavaFX then warns at start-up that its classes were loaded from the unnamed module, an unsupported configuration since JavaFX 16.
  - Spring Boot's executable jar (`bootJar`, nested jars and its own launcher) as the main jar.
  - The OpenJFX Gradle plugin for the compile-time jars, instead of declaring them directly.
- **Why:**
  - Named JavaFX modules in the runtime image are the configuration OpenJFX supports and the one jpackage is designed for. jlink then also drops what is not used: with no WebView (10.C6), `javafx.web`'s 102.4 to 118.5 MB never enter the image.
  - Pinning Gluon's jmods by SHA-256 keeps every installer of a release on the same JavaFX, as the pinned Temurin JDK does for Java (C13).
  - Plain jars on the class path need no special launcher and build reproducibly (C13). Spring Boot runs from them as from any class path.
  - Declaring the three OpenJFX jars directly keeps one plugin fewer in the build, at the price of naming the platform classifier in the build script.
- **Decision:** as proposed (prompt log entry 202)

## Interfaces with other sections

- **01 Batch language:**
  - Gives: `CommandSpec` carries each command's tool requirement (tool, minimum version, features), and the check before the first step (and `--check`) probes them through `core::deps` (2A.1, 2B.15; 2B.15's shell-command mark is dropped, entry 199). `--version [--json]` prints what this section describes (01.C14).
  - Gets: `$FUSION_HOME` is `install_dir().home`, laid out as above and written with `/` (01.C18); it is the `home` that 01's `Shell::find` takes.
  - Gets: busybox-w32 is bundled on Windows under `tools/busybox/` (01.C10), with its source offer; its path and version go into `run.json`; `exec` children get the bundled tools directory appended to `PATH` (C20). The process layer offers spawning with an argument vector, a working directory, output to a file, a null stdin, waiting and stopping a whole group or job; `wmain`, the UTF-8 console and long paths are handled as in the platform table.
  - Startup install errors exit with status 1 (2B.16), within 01.C14's statuses 0, 1, 2, 3 and 130. Real arguments are read with `core::parse_decimal`, 04.C2's grammar through fast_float (C5).
- **02 Network:** pugixml is private to `network`. No KGML is ever stored in the repository or the bundle. `data/kegg_pathways.txt` is resolved by 02.C12 against the user's `kgml_dir`, the check names every listed pathway missing there (02 open question 6), and KEGG pathway names come from each KGML's root title, since the shipped name map carries none (entry 205). Edge scores are read with `core::parse_decimal` (C5).
- **03 Merge:** `merge` links `expression` (1.2).
- **04 Expression:** `expression` links `network` (1.2), and its matcher takes a `network::GraphView`. Its `core::parse_decimal` applies 04.C2's grammar through fast_float, the one parser that 01's real arguments and 02's scores also use (C5, 3A.19). The histogram (04.C7) runs through `core::run_gnuplot`, with the plot palette and font set-up; a requested plot that is not written fails the step with 04's `expression.gnuplot_failed` (S3.19, entry 230), since the runner only reports. Readers accept CRLF, because a real LFC input under `runs/` has it.
- **05 Corridor:** passes `corridor.view()` to section 04's matcher (1.2).
- **06 Solve:**
  - Z3 5.3.3, gated on X11, is linked privately to `solve` as a shared library (C2, C17); `solve` exposes `z3_version()` and `verify_z3()` and sets `ctrl_c` to false on every solver (C18), which answers 06's question. Each case runs in its own thread and context (06.C4), which 5.3.0's QF_BV thread-safety fixes serve.
  - Section 06 measured everything on 4.8.12 and lists what must be re-checked on the pinned version (its combined-solver fallback and the reason read before `pop`); warm starts from nearby models need a newer Z3 than 4.8.12.
  - No `std::jthread`; the stop watcher is a `std::thread` with an atomic flag. Per-check CPU time comes from `core::thread_cpu_time()`.
- **07 Results:**
  - `plot_pareto` needs gnuplot 5.4 or later with `pngcairo` (1.27); the bundle has 6.0.5 (C9). It runs through `core::run_gnuplot` (2B.13), which checks both the exit status and that the output file was written, since an unknown terminal is only a warning to gnuplot (measured).
  - The font is set up by the runner (C10); a missing font is the dependency check's warning `deps.font_missing` (07.C8, entry 230).
  - vis-network 10.1.5 and the page files are compiled in (C11). Graphviz runs on request and is not bundled (07.C5); `FUSION_DOT` overrides `PATH` (C20).
  - The identity test relies on result files that hold no absolute path and no time, as 07 designs them. Section 07's pages open in the system browser (10.C6), so no `javafx.web` ships.
- **08 Enrichment:** GSEA's generator is 08.C1's exact port of gseapy's, held in `core` beside 09's `Rng` (1.19); gseapy's BSD-3-Clause and `rand`'s MIT notices ship. Floating-point contraction is off and no result path uses the platform's `exp`, `log` or `pow` (C7).
- **09 Runs:** `run.json`'s schema takes this section's five blocks, with Z3 as one string and `FUSION_CACHE_DIR` in `environment` (1.14); `events.jsonl` gets `deps.checked`; install and dependency failures use 09's `error` event with `install.*` and `deps.missing` codes and exit 1 (2B.16), and a missing font uses 09's `warning` event with `deps.font_missing` (entry 230). This section's platform layer implements the `core` functions 09 declares (`path_from_utf8`, `cpu_seconds`, `peak_rss_bytes`, and `build_id` from `BuildInfo`, as 09.C5 and C16 define it). Every backend write goes through 09's `OutputFile` with its Windows retry rule (1.28).
- **10 Frontend:**
  - The main launcher sets `fusion.backend.executable` through `$APPDIR` (Packaging); on Windows a console launcher, `fusion-frontend`, runs `--reset-password` (C12). The frontend runs `fusion --version --json` at start and refuses a backend of another version (C16).
  - The home holds `pipelines/standard.json` (10.C12), and the build writes `pipelines/standard.batch` from it with the frontend's generator. A new session's maps default to `$FUSION_HOME/data/…` (S8.06, S8.13).
  - The frontend is a JavaFX desktop application (10.C1) with OpenJFX 21.0.13 (10.C3 and C24), packaged as a normal windowed application (10.C4 and C12), with OpenJFX's modules jlinked into the runtime (C25) and no `javafx.web` (10.C6). The runtime keeps `java.desktop`, which JavaFX requires.
  - Java 21 and Spring Boot 4.1.2 (C21), without a web server.
- **12 Tests:** CI runs section 12's tiers T0 and T1 on C15's schedule, with the weekly ThreadSanitizer build; T2 and T3 need the KEGG data root and run on this workstation (12.C1, 12.C3); the identity test checks 12.C11's promise. The test tree is `backend/tests/` and `frontend/src/test/`. JavaFX screen tests need a display: on Linux runners `ci.yml` gives them a virtual X display (Xvfb); macOS and Windows runners have a desktop session.

## Test hooks

- **Versions:** `fusion --version --json` prints `build_info()` and `solve::z3_version()`. A test checks that the runtime Z3 equals the pin and that `run.json`'s `versions` block matches.
- **Install lookup:**
  - Tests set `FUSION_HOME` to a fake home and check each failure and its code: missing `fusion-data.json` (`install.home_missing`), version mismatch (`install.version_mismatch`), a missing or damaged data file (`install.file_missing`, `install.file_damaged`).
  - A symlink test, `fusion` reached through a link in another directory, runs on Linux and macOS, and on Windows where symlinks are allowed.
- **Dependency check:**
  - `FUSION_GNUPLOT` points at a small test program, built in the test tree so that it works on all three platforms. One variant lists no `pngcairo` among its terminals: the check must fail before the first step with `deps.missing`. Another prints gnuplot's unknown-terminal warning, writes no file and exits 0: `run_gnuplot` must report the plot as not written. A third reports version 5.2.
  - A fake home without `tools/` makes a batch with a plot step fail in `--check` and leaves a batch without one untouched.
  - An `exec` line running `gnuplot` finds a system gnuplot first and the bundled one only when no system gnuplot exists (C20).
- **`fusion --version --json`** is the smoke test of every installer and archive in `package.yml`.
- **The desktop application:** after installing each package, `package.yml` starts the application, waits for its main window (under Xvfb on Linux) and quits it through the platform's Quit; on Windows it also runs `fusion-frontend --reset-password` on a test account. It lists the runtime image's modules (`java --list-modules` from the image) and compares them with `packaging/jlink-modules.txt` and OpenJFX's three modules, which catches a missing `java.desktop` or an unwanted `javafx.web`.
- **Process layer:** a child started with a working directory reports that directory, on all three platforms. On Linux the binary must need no glibc newer than 2.28 (`objdump -T`, as measured above), which also catches an accidental `posix_spawn_file_actions_addchdir_np`.
- **Windows runtime:** the Windows smoke test runs `fusion --version` on a runner image without Visual Studio's runtime on `PATH`, which catches a missing app-local DLL (C23).
- **Ctrl-C:** a test sends SIGINT (`GenerateConsoleCtrlEvent` on Windows) during a long check and expects 09's stop path, not a bare Z3 `unknown`, which checks that `ctrl_c` is off (C18); a second SIGINT must end the process with status 130 (09.C8).
- **Cross-platform identity:** one synthetic batch (merge, corridor, both solves, GSEA) runs on Linux, macOS and Windows in `platforms.yml`. The canonical outputs of every deterministic stage, the SMT-LIB text of every query in a debug run and the frontiers must have the same SHA-256 on all three platforms (12.C11). Witnesses, times, `run.json`, logs and images are excluded.
- **Reproducible build:** two Linux release builds in different directories must give the same SHA-256 for `fusion`.
- **Layering:** a deliberately forbidden include in a test-only target must fail to compile (C6), and `core` must build without Z3 on the include path.
- **Licences:** the package step fails if a shipped file belongs to no component in `THIRD_PARTY_NOTICES.txt`, or if `data/SOURCES.md` lacks an entry for a file in `data/`.
- **Z3 re-pin benchmark:** the Z3 comparison of this section, kept as a script under `packaging/` or section 12's benchmarks, re-runs the real dumps with two Z3 builds in both modes and reports agreement, model differences and times. X11 (C2) is its first use, and a later re-pin decision cites its output.

## Open questions

1. **Spring Boot 3 is out of free support (C21).** *Closed:* C21 accepted, amending G2 to Spring Boot 4.1 with Gradle 9.9.1 (prompt log entry 200).
2. **Compiler minimums (C22).** *Closed:* C22 accepted, amending G1 to GCC 13 and Clang 17 (entry 200).
3. **Code signing.** Without an Apple Developer ID ($99 a year) and notarisation, the `.dmg` is blocked at first launch until the user allows it under System Settings, Privacy & Security. Without a Windows code-signing certificate or a signing service, the `.msi` meets a SmartScreen warning. *Deferred:* development builds stay unsigned, and signing is decided before the first public release, when the user creates the accounts and adds the secrets to GitHub (entry 203).
4. **May the reference maps be shipped?** *Closed:* both maps are rebuilt from sources with clear terms, recorded in `data/SOURCES.md` (NCBI Gene's symbols; Reactome's names, with attribution), and no KEGG pathway names ship: they are read at run time from each KGML's root title (entry 205). *Sprint (prompt log entry 315):* Reactome's names are CC0, so the attribution is given though not required.
5. **Does the official Windows gnuplot honour `PANGOCAIRO_BACKEND=fc` and `FONTCONFIG_FILE`?** *Recorded as a Phase 4 check* (entry 205): one plot on a Windows machine; the fallback is to install the font per user.
6. **How does Z3 5.3.3 perform on FuSION's dumps?** *Recorded as a Phase 3 check* (entries 173 and 205): section 12's experiment X11, the gate on C2's pin.
7. **busybox-w32's applets (section 01's open question 6).** *Recorded as a Phase 4 check* (entries 188 and 205): `date +%N` and a `bash` applet for the 29 `exec bash …/plot_po.sh` lines, run once on Windows with the pinned busybox-w32.
8. **`core`'s duties in C1 (C19).** *Closed:* C19 accepted, amending skeleton C1's description of `core` (entry 204).
9. **Do OpenJFX 21.0.13's Linux libraries meet C1's glibc 2.28 floor?** *Recorded as a Phase 4 check* (entry 205): `objdump -T` on the libraries of the jmods archive, as this section did for Z3. If they need a newer glibc, the desktop packages for Linux get a higher floor than the backend archive.
10. **Does the cairo-only bundled gnuplot accept `set term png`?** *Recorded as a Phase 4 check* (entries 204 and 205): FuSION 1's inline `exec` gnuplot lines use `set term png`, gnuplot's libgd terminal, and the bundle builds only the cairo terminals (C9). If the bundled build refuses `png`, those lines need a system gnuplot, or the bundle adds libgd.
