# FuSION 2.0 prompt log

Append-only record of the FuSION 2.0 thread, kept as set out in [decision 0002](../decisions/0002-prompt-log-scope.md). It holds three kinds of entry:

- **User prompt:** every prompt the user sends in this thread, word for word.
- **Poll:** every question Claude puts to the user, with the options offered (word for word) and the answer chosen.
- **Subagent prompt:** the full text of any prompt Claude gives a subagent.

Entries are numbered in order and never edited afterwards. A correction is a new entry that names the entry it corrects. Times are IST and record when the entry was logged.

---

## 001 · User prompt · 2026-10-27 17:57

> starting a separate thread here, the task is to create a separate repo for FuSION 2.0, which draws from the current fusion for all functionality, but is entirely redone by claude, with clean code that's readable, implentations made with maximum efficiency, into a single cohesive tool, entire backend in cpp and frontend in spring boot java. track all prompts for this, and poll at every step, make sure every decision is run by me and thoroughly reviewed.

## 002 · Poll · 2026-10-27 17:57

Asked after a read-only survey of FuSION 1. The survey and the proposed phase plan shown with the poll are reproduced in decisions [0003](../decisions/0003-phase-plan.md) and [0004](../decisions/0004-functional-scope.md).

**Q1 (Repo).** Where should the FuSION 2.0 repo live? It starts as a fresh local git repo and nothing is pushed anywhere.

- Desktop/arjun/FuSION2 (Recommended): At /home/ks-149-u24/Desktop/arjun/FuSION2, next to the current FuSION folder. A fresh git init, local only.
- misc/FuSION2: Under /home/ks-149-u24/Desktop/arjun/misc/, next to FuSION-explanations.
- Local plus private GitHub: The same local folder as the first option, plus a new private repo under arjunjeewan. I'd create it and push only after you confirm separately.

**Answer:** Local plus private GitHub → [decision 0001](../decisions/0001-repo-location-and-hosting.md)

**Q2 (Prompt log).** What should the prompt log record?

- Prompts and polls (Recommended): Your prompts word for word with timestamps, plus each question I ask you and your answer, in one append-only docs/process/PROMPTS.md. Each decision also gets its own record under docs/decisions/.
- Also subagent prompts: Everything in the first option, plus the full text of any prompt I give a subagent, so every generated file traces back to an instruction.
- Your prompts only: Your prompts word for word with timestamps. No separate decision records.

**Answer:** Also subagent prompts → [decision 0002](../decisions/0002-prompt-log-scope.md)

**Q3 (Phases).** Do you approve the phase order: 0 setup, 1 spec, 2 architecture, 3 reference tests, 4 backend, 5 frontend?

- Approve as proposed (Recommended): The spec comes before the architecture, so the design follows what FuSION 1 actually does. The backend is finished and checked against FuSION 1 before frontend work starts.
- Vertical slices: Phases 0 to 3 stay the same. After that, each feature is built end to end (C++ plus its Spring Boot screen) before the next one starts.
- Architecture first: Settle the C++/Spring Boot architecture before writing the spec.

**Answer:** Approve as proposed (Recommended) → [decision 0003](../decisions/0003-phase-plan.md)

**Q4 (Scope).** Which FuSION 1 functionality is in scope?

- Live set, legacy listed (Recommended): Port everything the current build runs. The spec gives each disabled command one line, so you can bring back any of them.
- Live set and all legacy: Also port the roughly 100 disabled commands. That makes the backend much larger.
- Live set only: Leave the disabled code out of the spec entirely.

**Answer:** Live set only → [decision 0004](../decisions/0004-functional-scope.md)

## 003 · Poll · 2026-10-27 18:06

Asked after the phase 0 files were written. "The files linked above" in Q2 were this log, docs/decisions/README.md and decision records 0001 to 0004.

**Q1 (CLAUDE.md).** Should FuSION2 get a CLAUDE.md with the process rules? My memory is stored per project folder, so a session started inside FuSION2 wouldn't otherwise know to poll you or keep the logs.

- Add CLAUDE.md (Recommended): Goes into the Phase 0 commit with its own decision record, 0005. The preview shows the full text. Preview shown with this option:

  ```text
  # FuSION 2.0: rules for Claude

  FuSION 2.0 is a full rewrite of FuSION 1 (read-only
  reference at /home/ks-149-u24/Desktop/arjun/FuSION).
  Backend in C++, frontend in Spring Boot (Java).

  ## Process
  - Poll the user before every decision, recommended
    option first. Never decide silently or fold several
    decisions into one step.
  - Log every user prompt word for word, every poll with
    its options and answer, and the full text of every
    subagent prompt in docs/process/PROMPTS.md. Append
    only. (decision 0002)
  - Give every decision a record in docs/decisions/ and
    a row in docs/decisions/README.md.
  - Work through the phases in order. A phase ends only
    after the user reviews it. (decision 0003)
  - Show the diff and get approval before every commit.
    Push only when the user says so. (decision 0001)

  ## Scope
  - Port only what the current FuSION 1 build runs. Code
    in FuSION 1's #if 0 regions is out of scope.
    (decision 0004)
  ```

- Memory only: Keep the rules only in my memory for the FuSION 1 folder. Sessions started inside FuSION2 won't see them.

**Answer:** Add CLAUDE.md (Recommended) → [decision 0005](../decisions/0005-claude-md-process-rules.md)

**Q2 (Commit).** Once the CLAUDE.md question is settled, may I make the first local commit on main? It holds the files linked above plus this poll's log entry.

- Commit (Recommended): One local commit with the message 'Start FuSION 2.0: prompt log and decision records', authored as arjunjeewan and ending with a Co-Authored-By: Claude line.
- Hold the commit: Leave the files uncommitted for now.

**Answer:** Commit (Recommended). This is an approval, not a design decision, so it has no decision record.

**Q3 (GitHub).** How should the private GitHub repo get created and pushed? This machine can't reach GitHub as you yet: there's no gh, no git credential helper, and github.com isn't in known_hosts.

- You create, you push (Recommended): You create an empty private repo arjunjeewan/FuSION2 on github.com, with no README or license. I add it as origin, and you run git push -u origin main yourself, so no credentials pass through me.
- Set up SSH, I push: You add ~/.ssh/id_ed25519.pub to your GitHub account and create the empty repo. I add github.com's published host key to known_hosts, set an SSH origin and push.
- Install gh, I create: I install the GitHub CLI into ~/.local/bin (no sudo). You run gh auth login once, then I create the private repo and push.
- Later: Stay local for now, and I'll ask again at the end of Phase 1.

**Answer:** You create, you push (Recommended) → [decision 0006](../decisions/0006-github-repo-setup.md)
