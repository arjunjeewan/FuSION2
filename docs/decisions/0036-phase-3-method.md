# 0036 · How Phase 3 is run

- **Status:** Accepted
- **Date:** 2026-11-17
- **Asked in:** prompt log entry 261

## Context

The user signed off Phase 2 on 2026-11-17 (prompt log entry 260). Phase 3 is the reference tests ([decision 0003](0003-phase-plan.md)), and section 12 of the architecture fixes what it holds (`docs/architecture/12-tests.md`):

- capture work, about 1 h of machine time: the data root, the snapshot binary, capture batches for 39 KEGG cases, regenerated toy and EGF-HeLa runs, and the gseapy references;
- the first code: the C++ test kit and reference runner (12.C8), the expected-differences list and the end-to-end fixtures;
- experiments X02, X03, X05 to X08 and X11, with measurements M01 to M04 (12.C15): about 30 h on the workstation, booked with the user. X11 needs Z3 4.16.0 and 5.3.3 installed and must finish before Phase 4 (11.C2).

## Options

1. Plan first, then in order: a written plan, its choices polled; the captures and cheap runs first, the long runs on dates the user books, and code in Phase 4's loop (recommended)
2. Start the captures at once, and plan the code and the long experiments afterwards
3. Book X11 and X07 first, since X11 gates the Z3 pin, and plan and build the rest while they run

## Decision

Option 1:

1. Claude writes `docs/phase3/plan.md`, giving each item its inputs, machine time, outputs and what the user reviews, and polls the plan's choices.
2. The captures and cheap runs go first. The long runs run on dates the user books.
3. Code follows Phase 4's loop from decision 0003: a design note, the user's OK, code with tests, the user's review of the diff, then a commit.

## Consequences

- No Phase 3 run or code starts before the plan's choices are decided.
- Every run that loads the workstation is booked with the user and never started beside the user's own runs (section 12, open question 6).
