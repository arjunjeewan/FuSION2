# 0026 · Subagents run at maximum parallelism

- **Status:** Accepted. It replaces the two-at-a-time pacing of decisions [0016](0016-resume-subagents-in-pairs.md) and [0025](0025-fresh-subagents-stages-5-to-8.md).
- **Date:** 2026-11-11
- **Asked in:** prompt log entries 045 and 046 (the user's instruction)

## Context

Subagents ran two at a time, to stay under the user's plan limits, which had stopped them several times. The user then wrote: "max out the number of agents. i have max sub now", and then "Try again".

## Decision

Every independent piece of subagent work runs in parallel:

- the stage 7 and 8 drafts start at once, alongside the running stage 5 and 6 drafts;
- each drafted stage gets a verification subagent. It checks every citation against the FuSION 1 snapshot, recomputes every count, and reports discrepancies without editing anything. Claude reviews each report and spot-checks it before correcting the draft and polling the user. Where such a report exists, it stands in for Claude's own line-by-line re-read ([decision 0010](0010-spec-reading.md)).

## Consequences

- Several drafts and checks run at once. Review and polling still follow stage order ([decision 0008](0008-spec-stages.md)).
- Every subagent prompt is still logged in full ([decision 0002](0002-prompt-log-scope.md)).
