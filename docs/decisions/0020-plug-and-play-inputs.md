# 0020 · Plug-and-play inputs

- **Status:** Accepted
- **Date:** 2026-11-09
- **Asked in:** prompt log entries 019 and 020 (Q3)

## Context

The user, in entry 019: "the backend in fusion just takes 2 types of inputs and what those 2 inputs encode, be it whichever pathway source, or lfc from any other data modality other than microarray should be simply plug and play."

The two inputs are the network (today KEGG KGML) and per-gene values (today log-fold changes from a microarray experiment).

## Options

1. Record it as a requirement now; the stage 2 and 3 reviews flag anything tied to KEGG or to microarrays, and Phase 2 chooses the mechanism (recommended)
2. Decide the mechanism now: built-in readers for each source, or one plain exchange format with converters

## Decision

Option 1.

## Consequences

- When stage 2 and stage 3 items are polled, anything specific to one pathway source or one data modality is flagged in the question.
