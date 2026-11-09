# 0014 · Which Swing GUI stage 8 covers

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 005, Q4

## Context

FuSION 1 has three Java GUIs under `Linux/`:

- `frontend/`: last commit 2026-07-09, with a README on how to build and run it;
- `frontend1/`: last commit 2026-07-02. Every one of its source files also exists in `frontend/`, which adds `gui4.java`, `Gui4Panel.java` and the block templates;
- `GUIForExecutable.java`: last commit 2024-01-01.

## Options

1. `frontend/` only (recommended)
2. `frontend/` plus `frontend1/`, in case their shared files behave differently

## Decision

Option 2. Stage 8 covers `frontend/` and `frontend1/`, and describes both versions wherever a shared file behaves differently. `GUIForExecutable.java` is out of scope.
