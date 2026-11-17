# 0031 · How the cross-section items and the section choices are polled

- **Status:** Accepted
- **Date:** 2026-11-13
- **Asked in:** prompt log entry 157

## Context

The cross-section check of [decision 0029](0029-architecture-section-template.md) (prompt log entry 156) found 179 items in the twelve architecture sections:

- 32 things the sections define differently;
- 11 things two sections both design;
- 19 gaps no section fills;
- 13 decisions two sections read differently;
- 30 sets of choices that answer one question;
- 33 ordering dependencies;
- 41 proposed amendments of decided items, 8 of which the sections had not flagged.

With the sections' own 187 choices, that is roughly 230 decisions, each with a proposed resolution or recommendation.

## Options

1. Polls, with the mechanical alignments grouped (recommended)
2. One review page, with polls only for what the user marks
3. Every item its own question

## Decision

Option 1:

- **Grouped questions:** about 30 purely mechanical alignments come in grouped questions that list each item. These are names that follow their owner's spelling, ownership assignments, gaps with an obvious owner, stale cross-references and examples.
- **One question each:** every substantive conflict and every section choice gets its own question, four to a poll, in dependency order, starting with 08.C1.
- **Asked together:** choices that answer one question are asked together, and a conflict tied to a choice is settled in that choice's question.
- **After the polls:**
  1. Each section's drafter applies the approved changes ([decision 0030](0030-verifier-findings-to-drafters.md)).
  2. Claude fills in the Decision lines and checks the result.
  3. The sections are committed.

## Consequences

- Every question and answer is logged in the prompt log as usual.
- A ledger in Claude's scratchpad maps each item to its answer until the sections are updated.
