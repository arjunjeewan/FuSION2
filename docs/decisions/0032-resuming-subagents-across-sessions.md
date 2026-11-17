# 0032 · A new session resumes the old session's subagents by copying their saved transcripts

- **Status:** Accepted
- **Date:** 2026-11-16
- **Asked in:** prompt log entry 227

## Context

After a usage-limit stop, the rule has been to resume the same subagent with a message, never to start a new one ([decision 0016](0016-resume-subagents-in-pairs.md), [decision 0030](0030-verifier-findings-to-drafters.md)). On 2026-11-16 six Phase 2 drafters stopped at the limit, and the work continued in a new Claude session. The new session's first resume message failed with "No transcript found for agent ID" (prompt log entry 226): a session can resume only subagents whose saved transcripts sit in its own folder, and these sat in the old session's folder.

## Options

1. Copy each stopped drafter's saved transcript and meta file into the new session's folder, then resume the same agents (recommended)
2. Reopen the old session and resume the agents there
3. Start fresh agents, each given its brief and told that its section is partly revised
4. Claude finishes the revisions itself

## Decision

Option 1. Claude copies `agent-<id>.jsonl` and `agent-<id>.meta.json` unchanged, checked byte for byte, from the old session's `subagents/` folder into the new session's, and then resumes each agent with its logged message. The originals stay where they were.

## Consequences

- The six drafters resumed with their full context (prompt log entry 228). The drafters of sections 05 to 10 were reached the same way for their follow-up messages (entry 243).
- An agent must not also be resumed from the old session, which would give two diverging continuations of one agent.
- A later new session copies the transcripts again, from the newest session's folder, and logs that it did.
- The method relies on how Claude Code stores subagents. If it stops working, Claude asks the user again, with options 2 to 4.
