# 0003 · Phase plan

- **Status:** Accepted
- **Date:** 2026-10-27
- **Asked in:** prompt log entry 002, Q3

## Context

The plan shown with the poll, word for word:

> **Proposed process.** Each phase ends with your review, and I'll ask you before every decision within a phase:
>
> 0. **Setup:** an empty repo holding only a prompt log and a decision log. No code.
> 1. **Spec of FuSION 1:** written one pipeline stage at a time, citing FuSION 1 file and line, with known bugs listed. You mark each item keep, fix or drop.
> 2. **Architecture:** repo layout, C++ standard and build, libraries, how Spring Boot drives the C++ backend, and what replaces the Python and gnuplot helpers. Each choice gets its own decision record.
> 3. **Reference tests:** save FuSION 1 outputs from reference runs (the toy examples, EGF-HeLa, EGFR→CXCL8 b7) so 2.0 can be checked against them. For each known FuSION 1 bug, you decide whether 2.0 copies it or fixes it.
> 4. **Backend, one module at a time:** design note, your OK, code with tests, your review of the diff, then a local commit.
> 5. **Spring Boot frontend, one screen at a time:** same loop as the backend.

## Options

1. Approve as proposed (recommended)
2. Vertical slices: phases 0 to 3 unchanged, then each feature built end to end (C++ plus its Spring Boot screen) before the next one starts
3. Architecture first: settle the C++/Spring Boot architecture before writing the spec

## Decision

Option 1, the plan as quoted above.

## Consequences

- The spec is written before any architecture decision, so the architecture choices can cite it.
- Frontend work starts only after the backend is finished and checked against FuSION 1.
