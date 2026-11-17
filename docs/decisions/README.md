# Decision records

One file per decision, numbered in the order the decisions were made. Each record names the poll it came from in the [prompt log](../process/PROMPTS.md).

| # | Decision | Status | Date |
|---|---|---|---|
| [0001](0001-repo-location-and-hosting.md) | Local repo at Desktop/arjun/FuSION2, plus a private GitHub repo | Accepted; GitHub step set by 0006 | 2026-10-27 |
| [0002](0002-prompt-log-scope.md) | Log user prompts, polls and subagent prompts | Accepted | 2026-10-27 |
| [0003](0003-phase-plan.md) | Phases: setup, spec, architecture, reference tests, backend, frontend | Accepted | 2026-10-27 |
| [0004](0004-functional-scope.md) | Port only what the current FuSION 1 build runs | Accepted; open questions settled by 0013, 0014 | 2026-10-27 |
| [0005](0005-claude-md-process-rules.md) | CLAUDE.md carries the process rules | Accepted | 2026-10-27 |
| [0006](0006-github-repo-setup.md) | The user creates the GitHub repo and runs every push | Accepted | 2026-10-27 |
| [0007](0007-spec-format.md) | Spec: one Markdown file per stage, numbered items citing FuSION 1 | Accepted | 2026-10-27 |
| [0008](0008-spec-stages.md) | Eight spec stages, reviewed in pipeline order | Accepted | 2026-10-27 |
| [0009](0009-spec-review.md) | Every spec item gets its own poll question | Accepted | 2026-10-27 |
| [0010](0010-spec-reading.md) | One subagent per stage drafts the spec, prompts logged | Accepted | 2026-10-27 |
| [0011](0011-phase1-subagent-prompts.md) | Phase 1 subagent prompts approved as drafted | Accepted | 2026-10-27 |
| [0012](0012-spec-reference-tree.md) | Spec reference: Linux working tree, macOS differences flagged | Accepted | 2026-10-27 |
| [0013](0013-suggest-thresholds-in-scope.md) | suggest_thresholds.py is in scope (stage 3) | Accepted | 2026-10-27 |
| [0014](0014-stage8-gui-scope.md) | Stage 8 covers frontend/ and frontend1/, not GUIForExecutable.java | Accepted | 2026-10-27 |
| [0015](0015-github-initial-commit-merge.md) | Merge GitHub's initial LICENSE commit into main | Accepted | 2026-11-09 |
| [0016](0016-resume-subagents-in-pairs.md) | Resume the Phase 1 subagents two at a time, in stage order | Accepted | 2026-11-09 |
| [0017](0017-batch-file-compatibility.md) | FuSION 1 batch files keep working, except where FuSION 1 was buggy | Accepted (applies: 0018 keeps the batch language) | 2026-11-09 |
| [0018](0018-batch-interface.md) | Interface: the batch language with named arguments and sweeps | Accepted | 2026-11-09 |
| [0019](0019-partial-runs.md) | Partial runs: stop after or start from any step, resume a solve, solve part of the frontier | Accepted | 2026-11-09 |
| [0020](0020-plug-and-play-inputs.md) | Network and per-gene inputs must be plug and play; mechanism in Phase 2 | Accepted | 2026-11-09 |
| [0021](0021-required-and-optional-inputs.md) | Inputs checked up front; missing required or named-but-unreadable inputs stop the run; frontend validates too | Accepted | 2026-11-09 |
| [0022](0022-graph-display.md) | Graph display is a 2.0 feature; its design is worked out with the user | Accepted, design pending | 2026-11-09 |
| [0023](0023-results-may-change-where-improved.md) | Old batches still run, but results may change where 2.0 improves (amends 0017) | Accepted | 2026-11-10 |
| [0024](0024-frozen-fusion1-snapshot.md) | The spec cites a frozen FuSION 1 snapshot (amends 0012) | Accepted | 2026-11-10 |
| [0025](0025-fresh-subagents-stages-5-to-8.md) | Fresh subagents draft stages 5 to 8 (replaces 0016 there) | Accepted | 2026-11-10 |
| [0026](0026-maximum-parallel-subagents.md) | Subagents at maximum parallelism; verification subagents per stage | Accepted | 2026-11-11 |
| [0027](0027-graph-display-in-phase-5.md) | The graph display is designed in Phase 5, with the frontend (amends 0022's timing) | Accepted | 2026-11-12 |
| [0028](0028-phase-2-method.md) | Phase 2: skeleton first, then parallel section drafts with verifiers, every choice polled | Accepted | 2026-11-12 |
| [0029](0029-architecture-section-template.md) | Architecture sections follow one full template; one cross-section check catches conflicts | Accepted | 2026-11-12 |
| [0030](0030-verifier-findings-to-drafters.md) | Verifier findings go back to each section's drafter; Claude checks the revision | Accepted | 2026-11-12 |
| [0031](0031-polling-the-architecture-sections.md) | Architecture polls: substantive items and choices one per question, mechanical alignments grouped | Accepted | 2026-11-13 |
| [0032](0032-resuming-subagents-across-sessions.md) | A new session resumes the old session's subagents by copying their saved transcripts | Accepted | 2026-11-16 |
