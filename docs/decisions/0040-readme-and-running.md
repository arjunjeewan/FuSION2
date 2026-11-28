# 0040 · A README and a RUNNING.md at the root, for lab members who build from source (amends 11.C14)

- **Status:** Accepted
- **Date:** 2026-11-28
- **Asked in:** prompt log entry 384

## Context

On 2026-11-28 the user asked for detailed run instructions "in a running md file for all OSs", and a README (prompt log entry 383). The repository had no `README.md`, although section 11's layout puts one at the root (11.C14).

The backend builds only on Linux. `backend/modules/run/src/posix.cpp` stops a Windows or macOS build with an `#error`, because [decision 0038](0038-end-to-end-version-this-week.md) left those ports for later iterations. There is no Windows machine or Mac here to test on. Nothing is packaged yet: 11.C12's installers also come later, so every reader builds FuSION 2.0 from source.

## Options

Poll 384 asked four questions:

1. **Run file:** `RUNNING.md` at the root (recommended), or `docs/running.md`.
2. **Other OSs:**
   - Linux in full, Windows through WSL 2 and macOS through an Ubuntu virtual machine, both marked untested (recommended);
   - Linux only;
   - or port the backend to macOS and Windows first.
3. **Readers:** lab members who build from source (recommended), biologists using the app, or FuSION 2.0's developers.
4. **README:** an overview, the status and a quick start (recommended), or a short page of links.

## Decision

The recommended option of each:

1. **Run file:** `RUNNING.md` sits at the root, beside `README.md`, which links to it. Claude keeps it current whenever building or running changes, as it does the release notes. This adds `RUNNING.md` to 11.C14's root list.
2. **Other OSs:** `RUNNING.md` covers Linux in full, with every step checked on this machine.
   - Windows runs the Linux build in WSL 2 with Ubuntu 24.04, where the Linux steps work unchanged.
   - macOS runs it in an Ubuntu 24.04 virtual machine.
   - Both routes are marked untested, and the native builds are marked not yet available.
3. **Readers:** both files are for lab members who are comfortable in a terminal and build and run FuSION 2.0 themselves. Every command is spelled out, with what it should print and what to do if it fails. A short section covers the tests.
4. **README:** it says:
   - what FuSION 2.0 is and what it keeps from FuSION 1;
   - where the work stands;
   - a short quick start and the repository's layout;
   - links to `RUNNING.md` and `docs/`;
   - the MIT licence.

## Consequences

- When the macOS and Windows ports arrive (decision 0038) and have been tested, their native steps replace `RUNNING.md`'s virtual-machine routes.
- Any change to the build, the dependencies, the command line or how the app starts updates `RUNNING.md` along with it.
