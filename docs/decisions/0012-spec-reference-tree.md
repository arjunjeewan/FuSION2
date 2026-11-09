# 0012 · Reference tree for the spec

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 005, Q2

## Context

FuSION 1 has a `Linux/` and a `macOS/` tree. Of the live sources, only `main.cc` differs (66 changed lines), in two places:

- macOS fixes `pathz3`'s positional-argument parsing. On Linux, a blank argument is skipped without advancing, so every later argument re-reads the blank and keeps its default.
- The binary finds its own path with `readlink("/proc/self/exe")` on Linux and `_NSGetExecutablePath` on macOS.

## Options

1. The Linux working tree the user builds and runs, with the two macOS differences as spec items showing both versions (recommended)
2. macOS `main.cc`, which already has the argument fix

## Decision

Option 1. The reference is the Linux working tree, uncommitted changes included, because the binary in use was built from it.
