# 0029 · What an architecture section contains, and how conflicts between sections are caught

- **Status:** Accepted
- **Date:** 2026-11-12
- **Asked in:** prompt log entry 109

## Context

[Decision 0028](0028-phase-2-method.md) has subagents draft the twelve architecture sections of Choice H1 in parallel, against the skeleton (`docs/architecture/00-skeleton.md`). Drafts written in parallel may define shared things differently: graph and corridor types, setting names, file formats, event names.

## Options

Template:

1. Full template: scope; requirements carried in; design with C++ interface outlines; choices; interfaces with other sections; test hooks; open questions (recommended)
2. Lighter template: design and choices only, in prose

Conflicts:

1. One cross-section check after the drafts and their verifiers (recommended)
2. Each section's verifier also checks its interfaces against the other drafts

## Decision

The full template, and one cross-section check.

Every section has these parts:

- **Scope:** the modules (Choice C1) and spec items it implements, and what it leaves to other sections.
- **Requirements carried in:** every decided spec item and decision it must honour, cited.
- **Design:**
  - components and data structures;
  - interfaces as C++ declarations in outline, without bodies;
  - algorithms with their complexity;
  - file formats with examples;
  - the errors, events and settings it adds.
- **Choices:** one entry per undecided choice, each with Proposed, Alternatives, Why and `Decision: pending`.
- **Interfaces with other sections.**
- **Test hooks.**
- **Open questions.**

After the drafts and their verifiers, one more subagent reads all twelve sections together and lists every conflict between them. Claude polls each resolution before anything is committed.

## Consequences

- Each choice in a section is polled on its own, as spec items were.
- The cross-section check runs once, before the first section commit.
