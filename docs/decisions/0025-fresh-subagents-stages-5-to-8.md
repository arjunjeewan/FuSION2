# 0025 · Stages 5 to 8 are drafted by fresh subagents

- **Status:** Accepted. It replaces [decision 0016](0016-resume-subagents-in-pairs.md) for stages 5 to 8.
- **Date:** 2026-11-10
- **Asked in:** prompt log entry 036, Q2

## Context

The stage 5 to 8 subagents had stopped before writing anything ([decision 0016](0016-resume-subagents-in-pairs.md)). Their logged resume message (entry 017) says nothing in FuSION 1 has changed, which stopped being true on 2026-11-09 ([decision 0024](0024-frozen-fusion1-snapshot.md)). Claude proposed a replacement resume message.

## Options

1. Resume them with the replacement message (recommended)
2. Start new subagents with the original prompts plus a note about the change, discarding their saved progress

## Decision

Option 2. Each new subagent's prompt is the shared brief (entry 006), a blank line, its stage section (entries 011 to 014), a blank line, and the note in entry 037. They still run two at a time, stages 5 and 6 first, as in decision 0016.

## Consequences

- The old subagents' partial work is not used.
