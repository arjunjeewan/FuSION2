# 0030 · Verifier findings go back to the section's drafter

- **Status:** Accepted
- **Date:** 2026-11-12
- **Asked in:** prompt log entry 137

## Context

In Phase 1, Claude applied each verifier's corrections to the stage files itself ([decision 0026](0026-maximum-parallel-subagents.md)). The first Phase 2 verification report, for section 04, found more than factual slips:

- two of the section's own choices conflict;
- choices are made silently in the design and must be listed and polled;
- the design overlaps skeleton Choices A1 and F1;
- file fields have inconsistent shapes.

The other eleven sections will bring similar reports.

## Options

1. The section's drafter revises it, and Claude checks the revision (recommended)
2. Claude applies the findings, as in Phase 1
3. A fresh agent revises each section from the draft and the report

## Decision

Option 1. Each verification report is sent back to the section's own drafter, which still has its context. The message is a fixed revision prompt (prompt log entry 138) with the report appended. The drafter revises only its own section. Claude spot-checks each revision before the cross-section check of [decision 0029](0029-architecture-section-template.md).

## Consequences

- Each revision send is logged with the report it carries.
- A finding that a drafter does not accept comes back in its reply, and Claude raises it with the user if it matters.
- A design that needs a skeleton Choice or a spec Decision changed stays a proposal under Choices, naming the decision it would amend, until the user is polled.
