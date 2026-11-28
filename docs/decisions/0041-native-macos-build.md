# 0041 · A native macOS build from source, with Homebrew's Z3 (amends 0038, 0039 and 0040)

- **Status:** Accepted
- **Date:** 2026-11-28
- **Asked in:** prompt log entry 392

## Context

[Decision 0040](0040-readme-and-running.md) had RUNNING.md send Mac users to an Ubuntu virtual machine, because the backend did not compile on macOS. The user found that too much setup, and asked for a Mac route like FuSION 1's (prompt log entry 391). FuSION 1 was compiled on the Mac itself, with Xcode's command-line tools and Homebrew. Its `macOS/README.md` was verified on macOS 26.5.1 on Apple silicon, with Homebrew's Z3 4.15.4.

A Linux executable cannot run on macOS, but FuSION 2.0's platform code already had macOS branches. Two things stopped a native build:
- **The run lock** (09.C9) uses Linux's open file description locks (`F_OFD_SETLK`). `run/src/posix.cpp` stopped a macOS compile with an `#error` ([decision 0038](0038-end-to-end-version-this-week.md) left the port for later).
- **The Z3 pin:** the build accepted only Z3 4.8.12 ([decision 0039](0039-sprint-toolchain.md)), and Homebrew ships 4.15.

This Linux workstation cannot compile or test macOS code.

## Options

Poll 392 asked two questions:

1. **macOS:**
   - port it, and the user tests the build on a Mac (recommended);
   - port it, and test it on GitHub's macOS runner, which needs a push and billed runner minutes;
   - or keep the virtual machine for now.
2. **Z3 on macOS:** Homebrew's Z3 (recommended), or Z3 4.8.12 everywhere, built from source on the Mac.

## Decision

The recommended option of each:

1. **The port.** Claude ports the two blockers, and RUNNING.md gives native Mac steps like FuSION 1's: Xcode's command-line tools, Homebrew, and the same CMake commands. They stay marked untested until the user builds on a Mac and sends the output, and Claude fixes what it shows.
   - **The run lock.** On macOS it takes classic `fcntl` locks, the kind Java's `FileChannel.tryLock()` takes there, so the app still sees a live run. Classic locks belong to the process: its own lock never conflicts with itself, and closing any descriptor of the file drops it. `Lock` therefore remembers the files this process locks, and `held()` answers for them without opening the file. Linux keeps its open file description locks.
2. **Z3.** A macOS build uses the system's Z3, Homebrew's, as FuSION 1 did on Macs.
   - The build searches Homebrew's prefix for it.
   - `run.json` records the linked version, as on Linux.
   - Frontiers agree with Linux's wherever every solver check finishes. Witnesses and solve times can differ: section 11's comparison of 4.8.12 with 4.15.3 found the same SAT or UNSAT answer for every check both finished, and different models.
   - Linux keeps its 4.8.12 pin until experiment X11.
   - The tests compare Z3's version with the one the build found, instead of a literal 4.8.12.

## Consequences

- RUNNING.md's Mac section becomes native steps, and the Ubuntu virtual machine route goes. Windows keeps WSL 2 until its own port.
- The Mac build is verified only once the user's build report is in. The test suite on macOS is a later step: some tests assume Linux, and FuSION 1's toy batches would open gnuplot windows there, since macOS has a display without `DISPLAY`.
- Section 11's targets (11.C1, 11.C3) still describe the releases' pinned Z3 5.3.3. Until X11 decides the pin, a Mac build links whatever Z3 Homebrew provides.
