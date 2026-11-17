# 0039 · The sprint's toolchain: the system Z3 4.8.12 until X11, and user-local CMake, Ninja and Gradle

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 280

## Context

[Decision 0038](0038-end-to-end-version-this-week.md) builds a first end-to-end version this week. This machine has GCC 13.3, OpenJDK 21.0.13, gnuplot 6.0 and Z3 4.8.12 with its headers, but no CMake, Ninja or Gradle. Section 11 pins Z3 5.3.3, gated on experiment X11 (11.C2), which now runs after the first version; CMake 3.28 or later (CI uses 4.4.5); Ninja 1.13.2; and Gradle 9.9.1.

## Options

1. **Z3:** link the system Z3 4.8.12 for now (recommended), or build 5.3.3 from source now.
2. **Tools:** user-local official releases of CMake 4.4.5, Ninja 1.13.2 and Gradle 9.9.1, each checked against its published SHA-256 (recommended), or the user installs CMake 3.28.3 and Ninja 1.11 with apt.

## Decision

The recommended option of each:

1. The sprint's build links the system Z3 4.8.12, the version FuSION 1 links, so its results compare directly with FuSION 1's references. Until X11 has run, `cmake/FusionVersions.cmake` names 4.8.12, and 11.C3's system provider accepts it.
2. Claude downloads CMake 4.4.5, Ninja 1.13.2 and Gradle 9.9.1 from their official release pages, checks each against its published SHA-256, and unpacks them under `~/.local/opt/`, with nothing installed system-wide and no `sudo`. Gradle is used only to generate the project's pinned Gradle wrapper.

## Consequences

- After X11, the user confirms or re-decides the 5.3.3 pin (11.C2), and the build switches to it.
- Commands that build put `~/.local/opt/`'s tools first on `PATH`; nothing in the user's shell profile changes.
