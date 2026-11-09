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

## 004 · Poll · 2026-10-27 18:12

Asked after the phase 0 commit (289b0bc). The eight stages shown with Q2 ("listed above") are quoted in [decision 0008](../decisions/0008-spec-stages.md).

**Q1 (Spec form).** Where should the Phase 1 spec live, and in what form?

- Markdown per stage (Recommended): One file per stage under docs/spec/ (01-batch-language.md and so on). Each requirement is a numbered item citing FuSION 1 file:line, with known bugs flagged.
- One spec file: A single docs/spec/SPEC.md covering every stage.
- Claude Doc per stage: An online doc per stage that you can comment on in the browser. The approved text is then copied into docs/spec/.

**Answer:** Markdown per stage (Recommended) → [decision 0007](../decisions/0007-spec-format.md)

**Q2 (Stages).** Do you approve the eight spec stages listed above, in that order?

- Approve (Recommended): Pipeline order, so each stage builds on the data structures the stage before it defines.
- Solver first: Start with stage 5 (pathz3), the core and riskiest part, then go back to stage 1.

**Answer:** Approve (Recommended) → [decision 0008](../decisions/0008-spec-stages.md)

**Q3 (Review).** How do you want to review each stage's keep, fix and drop calls?

- Poll the non-trivial ones (Recommended): Straight ports are listed as keep. Every proposed fix or drop, and anything ambiguous, gets its own poll question. You then approve the stage file as a whole.
- Poll every item: Every spec item gets its own poll question, straight ports included. The most thorough option, but a stage may run to dozens of questions.
- You edit the file: Each item has a Decision field that you fill in directly. I then check your calls and record them.

**Answer:** Poll every item → [decision 0009](../decisions/0009-spec-review.md)

**Q4 (Reading).** Who reads FuSION 1's code for the spec?

- Me, stage by stage (Recommended): Slower, but the detail stays in this conversation, and the rewrite will need it.
- Subagents in parallel: One subagent per stage, with each prompt logged in full. Faster, but I'd still re-read every part the spec cites.

**Answer:** Subagents in parallel → [decision 0010](../decisions/0010-spec-reading.md)

## 005 · Poll · 2026-10-27 18:19

Asked after the draft subagent prompts were written to a scratch file for review ("the draft file"). The prompts as sent are entries 006 to 014. Only the stage 8 section differs from the draft, because of the answer to Q4.

**Q1 (Prompts).** Do you approve the subagent prompts in the draft file? Approving launches eight subagents in parallel, and each writes only its own docs/spec file.

- Approve as drafted (Recommended): Launch all eight, aiming for about 10 to 30 items per stage. Each prompt is logged as the shared brief once plus that stage's section; joined, they are the exact prompt sent.
- Coarser items: The same prompts, but aiming for 5 to 15 items per stage: fewer poll questions, each covering more ground.

**Answer:** Approve as drafted (Recommended) → [decision 0011](../decisions/0011-phase1-subagent-prompts.md)

**Q2 (Reference).** Which FuSION 1 tree is the reference for the spec?

- Linux, macOS diffs flagged (Recommended): The Linux working tree you build and run. The two places where macOS main.cc differs (the pathz3 argument fix and how the binary finds its own path) become spec items that show both versions.
- macOS main.cc: Use macOS main.cc, which already has the argument fix. All other relevant files are identical either way.

**Answer:** Linux, macOS diffs flagged (Recommended) → [decision 0012](../decisions/0012-spec-reference-tree.md)

**Q3 (Thresholds).** Is suggest_thresholds.py in scope? Nothing calls it, but it's the advisory companion to cudf from roadmap objective 5.

- In scope, stage 3 (Recommended): Stage 3 specs it alongside cudf. It isn't disabled code, just a standalone tool.
- Out of scope: Leave it out, since the binary never runs it. The stage 3 prompt then drops it.

**Answer:** In scope, stage 3 (Recommended) → [decision 0013](../decisions/0013-suggest-thresholds-in-scope.md)

**Q4 (GUI).** Which Swing GUI is the live frontend for stage 8?

- Linux/frontend only (Recommended): Last updated 2026-07-09 and documented in its README. Every frontend1 source file also exists in frontend, and GUIForExecutable.java dates from 2024, so both are left out.
- frontend plus frontend1: Also spec frontend1, in case its shared files behave differently.

**Answer:** frontend plus frontend1 → [decision 0014](../decisions/0014-stage8-gui-scope.md)

## 006 · Subagent prompt · Phase 1 shared brief · 2026-10-27 18:19

Each of the eight Phase 1 subagent prompts (entries 007 to 014) is this text, then a blank line, then that entry's stage section.

```text
You are drafting one stage of a written specification of FuSION 1. The specification is the input to FuSION 2.0, a from-scratch rewrite with a C++ backend and a Spring Boot frontend, in /home/ks-149-u24/Desktop/arjun/FuSION2. Describe what FuSION 1 actually does, precisely enough that someone could reimplement your stage without opening FuSION 1, and flag anything that looks wrong or wasteful. You are not designing FuSION 2.0.

FuSION 1 in one paragraph: it reads KEGG pathway maps (KGML XML) into a signalling graph, merges many pathways into one graph, keeps only the corridor of nodes and edges on bounded paths from a source gene to a target gene, and asks the Z3 SMT solver for Pareto-optimal explanations of a gene-expression experiment. The two quantities traded off are node relaxations (observed up- or down-regulated genes left unexplained) and edge relaxations (graph edges allowed to misbehave), solved once with a chosen node, the FSC, forced into the explanation and once with it excluded. Commands come from batch files (`fusion -b <file>`) or an interactive prompt, and a Java Swing GUI writes those batch files.

### Ground rules

- FuSION 1 lives at /home/ks-149-u24/Desktop/arjun/FuSION. It is read-only for you: change nothing there, and do not build or run it.
- The reference is the Linux working tree (`Linux/`), uncommitted changes included, because the binary in use was built from it. Where `macOS/src/main.cc` differs (two places, named in the stage sections that own them), describe both versions. Every other macOS file that matters is byte-identical to its Linux counterpart.
- Scope is the live build only. The live commands are the ones `main.cc` dispatches on `command` once comments and `#if 0` regions are stripped: add, rgx, mff, wgx, upfx, sz, fb_rch, cudf, pathz3, psp, pex, gsea, let, start, exec, exit and help, plus their long-name aliases. Follow every call chain from your stage's commands into other files. Ignore code inside comments or `#if 0` regions, and code no live command reaches.
- Searching: `grep` in this shell is a wrapper that silently skips gitignored directories such as `runs/` and `auxilliary/`, so use `/usr/bin/grep`. `Linux/src/GraphManagerNew.cc` is about 31k lines and `Linux/src/main.cc` about 8k: search first, then read only the ranges you need.
- Real inputs and outputs from earlier experiments are under `runs/` (for example `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/`). Use them to confirm exact file formats, and quote real lines when you document a format.
- Your stage section lists facts found in earlier sessions. Treat them as leads to check, not as truth. Where the code disagrees, the code wins and you say so.

### What to write

Write exactly one file, at the path given in your stage section. Do not create or change any other file in either repo, and do not commit.

The file has this shape:

    # NN · <stage title>

    ## Scope
    One paragraph: the commands and FuSION 1 files this stage covers, and where it hands over to the stages before and after it.

    ## Items
    (numbered items, using the template below)

    ## Open questions
    Anything you could not settle from the code, each with what would settle it.

Each item uses this template:

    ### S<stage>.<nn> · <short title>

    - **FuSION 1:** `Linux/src/<file>:<first>-<last>` (`<function>`), plus every other location involved
    - **What it does:** exact behaviour: inputs, outputs, defaults, edge cases and formats, with a real example line where one exists
    - **Problems:** bugs, inconsistencies or avoidable cost, each with evidence (file:line, a concrete input that triggers it, or a count you measured); "None found" if there are none
    - **Proposed call:** keep, fix or drop, with one sentence of reasoning. Keep means port the behaviour as it is; fix means port it with the correction you state; drop means leave it out of FuSION 2.0.
    - **Decision:** pending

Granularity: one item per thing a user or a later stage can observe or depend on. That means a command and its arguments (group the trivial arguments), each input or output file format, each distinct algorithmic step, and each known bug. The user reviews every item as its own question, so aim for about 10 to 30 items per stage. Split an item that holds two independent decisions, and merge items that would always be decided together.

FuSION 2.0 aims for maximum efficiency, so record avoidable cost under Problems: needless asymptotic cost, repeated parsing, repeated solver calls, file round-trips between steps. Flag it with evidence; do not redesign it.

### Your reply to me

At most 300 words: the number of items for each proposed call, the three to five most important problems, the open questions, and anything you noticed that belongs to another stage. Do not paste the spec itself.
```

## 007 · Subagent prompt · Phase 1 stage 1 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 1 · Batch language and run setup

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/01-batch-language.md

You cover how FuSION 1 starts and reads commands: `fusion -b <file>` versus the interactive prompt, how a command line is split into tokens, `let` variables and `$VAR` expansion, `start`, `exec`, `exit` and `help`/`h`, and the non-script files in `Linux/supporting_files/` (config.txt, path.txt, edge_type.txt, edge_subtype.txt, node_type.txt, color_files/): who reads each one, when, and what it controls. Also cover any global state or defaults the driver sets up for later commands.

Not yours: what the pipeline commands themselves do (stages 2 to 7) and the helper scripts (stages 3, 6 and 7).

Leads:
- Dispatch in `Linux/src/main.cc`: `let` near line 1249, `start` near 1305, `exec` near 1318.
- An unset `$VAR` expands to an empty token rather than disappearing (stated in a comment in `macOS/src/main.cc` near line 4535). Document how expansion and tokenizing work in general; stage 5 owns what this does to pathz3's arguments.
- One of the two macOS differences is yours: the binary finds its own path with `readlink("/proc/self/exe")` on Linux and `_NSGetExecutablePath` on macOS (`main.cc` near lines 5730 and 5798), to locate bundled scripts.
```

## 008 · Subagent prompt · Phase 1 stage 2 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 2 · Graph input and merging

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/02-graph-input-and-merge.md

You cover `add` (plaintext edge file to KGML, `main.cc` near line 5841), `rgx`/`read_graph_xml` (KGML parsing in `Linux/src/XMLParser.cc` and its callers), `mff`/`merge_from_file` (merging many pathways into one graph), `wgx`/`write_graph_xml`, `upfx`/`unique_prefix` (near 1331) and `sz`/`size` (near 1581). That includes the in-memory graph model (node and edge fields, ids, types, subtypes), node identity and merging, cross-pathway edge merging, and every id or name map these commands read, such as the hsa-to-symbol and pathway-name maps and hsa_ids_not_to_be_merged.txt.

Not yours: expression data (stage 3) and anything that happens once the merged graph exists (stages 4 to 6).

Leads (details in /home/ks-149-u24/.claude/projects/-home-ks-149-u24-Desktop-arjun-FuSION/memory/kegg-merge-algorithm.md, kegg-pkc-node-merge.md and kegg-indirect-effect-design.md):
- Nodes are keyed by a representative id chosen in priority order: merge_exception_set, then all_id_map, then (threshold 1) collapse to the lexically smallest id. Merged nodes carry that smallest id, for example the merged PI3K node hsa110117499.
- Cross-pathway edge merging deduplicates on the exact ordered subtype vector, and edges marked tool_written skip it.
- Measured on a merged master graph: HRAS ends up in 4 different nodes, with 417 parallel-edge pairs, 28 polarity conflicts and 162 edges with no subtype.
- DISCARD_INDIRECT_EDGES drops indirect edges. An in-source comment says reachability is preserved, but 180 of 598 source-target pairs lose reachability.
- A crash fix, `final_rep_id()`, exists only in the uncommitted working tree.
- Forcing the classical-RAS trio binds RRAS2, the wrong merged node, and leaks HRAS, while the PKC trio merges cleanly into one node.
```

## 009 · Subagent prompt · Phase 1 stage 3 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 3 · Expression data

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/03-expression-data.md

You cover `cudf`/`create_up_down_file` (`main.cc` near line 2629): the log-fold-change input format, the up and down thresholds, how genes are matched to graph nodes, and the up/down file it writes. Also cover `Linux/supporting_files/suggest_thresholds.py`, which reads the same LFC file and suggests thresholds; nothing in FuSION 1 calls it. Describe the up/down file's format fully. Where later stages read it, name the reader and leave its use to stages 4 and 5.

Leads:
- suggest_thresholds.py's docstring says cudf does not skip a header line, which then parses as LFC 0.
- A gene is up when lfc >= up_threshold and down when lfc <= down_threshold, the two applied independently.
```

## 010 · Subagent prompt · Phase 1 stage 4 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 4 · Reachability corridor

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/04-reachability-corridor.md

You cover `fb_rch` (forward and backward reachability): its arguments, the bound and how path length is counted, what is kept and what is dropped, any node renumbering, and every file it writes or piece of state it hands to `pathz3`.

Not yours: the solver encoding (stage 5).

Leads:
- Node ids (nids) are renumbered between phases, so an id from one phase may not mean the same node in another. Find out where and why.
- Measured corridor sizes at bound 10, EGFR (hsa1956) to CXCL8 (hsa3576): 115 nodes and 370 edges from a 42-pathway graph of 1134 nodes and 2529 edges, and 592 nodes and 2138 edges from the full master graph of 3100 nodes and 7400 edges. Use these to check your reading of the algorithm where the run directories hold the inputs.
```

## 011 · Subagent prompt · Phase 1 stage 5 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 5 · Encoding and Pareto solve

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/05-encoding-and-solve.md

You cover `pathz3`/`get_path_with_z3`: every argument and its default; the SMT encoding (node and edge variables, path constraints under the bound, consistency with the up/down observations, node and edge relaxations, the FSC forced in or out, and the coexpression and frozen-edge constraints with their CSV inputs under `additional_constraints/`); how the Pareto frontier is enumerated; solver timeouts, the random seed and how results are classified; SMT dumps (`PO_constraints_nr_<n>_er_<e>.smt2`); and every output file (`temp_w_solutions.txt`, `temp_wo_solutions.txt`, `temp_*_relaxations.txt`, limits files and `.dat` files).

Not yours: the corridor (stage 4) and plotting results (stage 6).

Leads (details in gsea-fdr-sweep-b10.md, pex-command-and-graphviz.md and egfr-cxcl8-bound-fsc-sweep.md in the memory folder named in stage 2):
- The second macOS difference is yours. Linux `main.cc` near line 4534 (macOS near 4535) skips a blank positional argument without advancing, so every later argument re-reads the blank and keeps its default; a non-numeric value does the same through the try/catch. macOS fixes this. Describe both versions.
- Z3 calls that end at the 4-hour timeout are logged as UNDEFINED, not TIMEOUT: the code checks `reason.find("timeout")` (`GraphManagerNew.cc` near line 29451), but Z3's reason_unknown() returns "unknown". In one run, 36 of 50.7 hours went into nine such calls.
- Timeouts come from `:solver2_timeout` and a per-check `:timeout` (batch values 3600000 and 14400000 ms).
- The solver's inhibitory edge set is only {inhibition, repression}; dephosphorylation and ubiquitination count as activating.
- Limits files print points as (e, n), while `.dat` files print (n, e).
```

## 012 · Subagent prompt · Phase 1 stage 6 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 6 · Results and plots

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/06-results-and-plots.md

You cover `psp`/`plot_solution_paths` (source-to-target solution paths for each Pareto point, drawn with Graphviz `dot`, plus a pathway-source CSV), `pex`/`plot_explanation` (runs `Linux/supporting_files/plot_explanation.py`, which builds interactive PyVis HTML subgraphs for both FSC cases and overlays relaxed nodes and edges), and `Linux/supporting_files/plot_po.sh` (gnuplot Pareto curves, called from batch files). Include how the binary finds the scripts and what each one needs on PATH or in Python.

Not yours: how the solution files are produced (stage 5).

Leads (details in psp-solution-path-plotting.md, pex-command-and-graphviz.md and gnuplot-for-po-curves.md in the memory folder named in stage 2):
- plot_explanation.py's docstring says up = red and down = blue, but the code draws green and orange.
- A relaxed edge with a same-effect parallel twin is drawn as a dashed curved twin.
- The renderer and the solver disagree on edge polarity, because the solver's inhibitory set is only {inhibition, repression}.
- plot_po.sh offsets coincident points so that overlapping frontiers both show.
```

## 013 · Subagent prompt · Phase 1 stage 7 section · 2026-10-27 18:19

Full prompt: entry 006, a blank line, then:

```text
### Stage 7 · Pathway shortlisting

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/07-gsea-shortlisting.md

You cover `gsea`/`gsea_select_pathways`, called as `gsea <lfc> <kgml_list> <fdr:Q|pval:P|nes:X|top:N> <out_list>`. It runs `Linux/supporting_files/gsea_select_pathways.py` (gseapy prerank, in the repo's `.venv`) and writes the pathway list that `mff` reads.

Leads (details in gsea-pathway-shortlist.md and gsea-fdr-sweep-b10.md in the memory folder named in stage 2):
- Gene sets are built from the KGML files themselves, because MSigDB's kegg_legacy collection covers only 90 of 163 pathways.
- Seed 42 and 1000 permutations make reruns byte-identical.
- 94% of LFC values are tied, which affects the ranking.
- FDR depends on which candidate pathways are offered.
- The C++ side pauses for input only when attached to a terminal (isatty).
```

## 014 · Subagent prompt · Phase 1 stage 8 section · 2026-10-27 18:19

Changed from the reviewed draft because of entry 005, Q4: the draft's sentence putting `frontend1/` out of scope was replaced by the sentences covering it.

Full prompt: entry 006, a blank line, then:

```text
### Stage 8 · Frontend workflow

Your file: /home/ks-149-u24/Desktop/arjun/FuSION2/docs/spec/08-frontend-workflow.md

You cover the Swing GUI in `Linux/frontend/` (source in `src/`, notes in `frontend workflow/`, and its README): every screen and panel, what the user enters, the batch script it generates (block templates and descriptions), sessions (`sessions/*.json`), the login page and its user file, how it launches the backend and shows results, and the data files it ships. Also cover `Linux/frontend1/`, an older snapshot: every one of its source files also exists in `frontend/`, which adds `gui4.java`, `Gui4Panel.java` and the block templates. Compare each file the two share, and wherever a shared file behaves differently, describe both versions in the item. `Linux/GUIForExecutable.java` dates from 2024 and is out of scope. For this stage, an item is one screen or panel behaviour, or one generated command.

Leads:
- The GUI does not yet emit psp, pex, plot_po.sh or gsea.
- `sessions/arjun.json` points CoexpressionCsv at an old root path that no longer exists.
```

## 015 · User prompt · 2026-11-09 13:41

> continue

## 016 · Poll · 2026-11-09 13:44

Asked after finding that none of the eight Phase 1 subagents had written its stage file, and that the GitHub repo held GitHub's own first commit (9914fe2, an MIT LICENSE), fetched by a `git pull` but not merged.

**Q1 (GitHub).** Your GitHub repo has GitHub's own first commit (9914fe2, adding an MIT LICENSE in your name). Our local main can't be pushed on top of it as things stand. How should I reconcile the two?

- Merge it in (Recommended): One local merge commit, 'Merge GitHub's initial commit (LICENSE)', with a Co-Authored-By: Claude line. It adds only LICENSE. Every hash the logs cite, such as 289b0bc, stays valid. Then you run git push -u origin main.
- Rebase onto it: Replay our commit on top of GitHub's, for a straight history. Our commit gets a new hash, so the log's mention of 289b0bc goes stale and needs a correction entry.
- Drop GitHub's commit: Leave main as it is, and you force-push over the remote. The LICENSE file is lost.

**Answer:** Merge it in (Recommended) → [decision 0015](../decisions/0015-github-initial-commit-merge.md)

**Q2 (Resume).** How should I resume the eight spec subagents? Their saved progress carries over. Each one gets the same short resume message, logged in full.

- Two at a time (Recommended): Resume stages 1 and 2 now, then the next pair as each pair finishes. Stage 1's review can start while later stages draft, and it's gentler on your usage limit than eight at once.
- All eight at once: Fastest if your limit holds, but this is what hit the session limit last time.
- One at a time: Slowest and lightest on your usage limit.

**Answer:** Two at a time (Recommended) → [decision 0016](../decisions/0016-resume-subagents-in-pairs.md)

## 017 · Subagent prompt · Phase 1 resume message · 2026-11-09 13:44

Sent word for word to each Phase 1 subagent when it is resumed ([decision 0016](../decisions/0016-resume-subagents-in-pairs.md)). First sent to the stage 1 and stage 2 subagents; each later send is logged as its own entry.

```text
Your earlier run stopped before you finished, because of a usage limit or the end of the session. Nothing in FuSION 1 has changed since you started, and your stage file has not been written yet. Carry on from where you stopped and finish the task exactly as your original prompt describes: write only your own stage file, then reply in at most 300 words.
```

## 018 · Poll · 2026-11-09 14:38

Asked after Claude verified the stage 1 draft, `docs/spec/01-batch-language.md`, against FuSION 1 and corrected two counts in S1.19. These framing questions came before the stage 1 item questions.

**Q1 (Batch lang).** Should FuSION 2.0 keep FuSION 1's batch language (fusion -b <file>, with let, $ variables, exec and one command per line) as an interface? Most stage 1 items describe that language, so this decides how to read them.

- Keep it (Recommended): 2.0 runs batch files the way FuSION 1 does, with the stage 1 fixes. Your 81 existing batch files stay usable, and Phase 3 can run the same batch through both versions and compare. How the Spring Boot frontend drives the backend remains a Phase 2 decision.
- Replace it in Phase 2: Phase 2 designs a new job format, such as a structured job file. Stage 1's language items are then recorded as 'drop, replaced in Phase 2', and existing batch files need converting.

**Answer (free text):** lay down the alternatives to the current batch language, are there any more efficient or better alternatives in any way?

No decision; the alternatives are laid out with the next poll.

**Q2 (Compat).** If the batch language stays, must FuSION 1's existing batch files run unchanged in 2.0?

- Yes, except FuSION 1 bugs (Recommended): A batch file that worked in FuSION 1 gives the same result in 2.0. Fixes may change behaviour only where FuSION 1 went wrong, as with $PLOT_FILE_WO_GENE.
- No, a clean break: 2.0's language can change its syntax freely (for example ${NAME} variables), and old batch files are converted once.

**Answer:** Yes, except FuSION 1 bugs (Recommended) → [decision 0017](../decisions/0017-batch-file-compatibility.md)

## 019 · Poll · 2026-11-09 14:45

Asked after Claude laid out four alternatives in chat: (1) the fixed batch language, (2) the batch language with named arguments, (3) a declarative job file with a converter (recommended), and (4) embedded Lua scripting. Python bindings and separate subcommands driven by a workflow manager were also discussed and not offered. Claude also reported that no interface choice changes run time noticeably, that 76 of the 81 batch files run one pipeline, that about 3,000 `exec` lines carry the shell glue, and that a `pathz3` line takes 28 to 33 positional arguments.

**Q1 (Interface).** Which interface should FuSION 2.0 have in place of FuSION 1's batch language? (Options 1 to 4 above.)

- Job file + converter (Recommended): One JSON or TOML file per experiment or sweep, checked against a schema before anything runs; the backend does the file plumbing. A converter carries FuSION 1 batch files over. JSON versus TOML and the schema are Phase 2 decisions.
- Fixed batch language: FuSION 1's language with the stage 1 fixes. All 81 files run as they are (decision 0017).
- Named-argument batch: The same line format plus name=value arguments. Old positional files still run.
- Embedded Lua scripting: FuSION functions called from Lua scripts, with loops and conditionals. Existing files are rewritten.

**Answer (free text):** the pipeline order should be very flexible, some users may not need entire solve or want to complete a partial solve, the current batch framework allows for that. readability from 2nd option is definitely an improvement. the backend in fusion just takes 2 types of inputs and what those 2 inputs encode, be it whichever pathway source, or lfc from any other data modality other than microarray should be simply plug and play.

No decision yet; Claude's reading of this answer was put back to the user in the next poll.

## 020 · Poll · 2026-11-09 14:52

Asked right after entry 019, to confirm Claude's reading of its free-text answer.

**Q1 (Interface).** So the interface is option 2: the batch language stays one command per line, so any subset of steps can run in any order, and arguments can be named (pathz3 graph=$G bound=10 …), while FuSION 1's positional lines still run (decision 0017). Is that right?

- Yes, option 2 (Recommended): Named-argument batch language, with positional lines still accepted. Stage 1's items stay in play, plus a new item for named arguments.
- Option 2, plus sweeps: The same, plus a simple parameter list so one file can run a bound or FSC sweep instead of cloned files.

**Answer:** Option 2, plus sweeps → [decision 0018](../decisions/0018-batch-interface.md)

**Q2 (Partial run, several allowed).** Which kinds of partial run must 2.0 support? Pick all that apply. The stage 5 spec will report what FuSION 1's pathz3 supports today.

- Stop after any step: Run only part of the pipeline, such as merge and corridor without a solve, keeping each step's output on disk.
- Start from a saved step: Begin a run from saved results, such as a merged graph or a corridor, without redoing the earlier steps.
- Resume a stopped solve: Continue an interrupted pathz3 from the Pareto points it already found, instead of starting again.
- Solve part of the frontier: Solve only chosen Pareto points, or only the with-FSC or without-FSC case.

**Answer:** Solve part of the frontier, Resume a stopped solve, Start from a saved step, Stop after any step (all four) → [decision 0019](../decisions/0019-partial-runs.md)

**Q3 (Inputs).** Should I record the two plug-and-play inputs (a network from any pathway source, and per-gene values from any data modality) as a requirement that the stage 2 and stage 3 reviews check against, with the mechanism decided in Phase 2?

- Requirement now (Recommended): Stage 2 and 3 items flag anything tied to KEGG or to microarrays. Phase 2 chooses how new sources and modalities plug in.
- Decide the mechanism now: Choose now between built-in readers for each source and one plain exchange format with converters.

**Answer:** Requirement now (Recommended) → [decision 0020](../decisions/0020-plug-and-play-inputs.md)

## 021 · Poll · 2026-11-09 14:58

The first stage 1 item poll (S1.01 to S1.04 of `docs/spec/01-batch-language.md`).

**Q1 (S1.01).** S1.01 · Loading the batch file. FuSION reads the whole -b file before running it. A missing, unreadable or empty file, or no file name at all, runs into undefined behaviour (a reproduction segfaulted). The draft proposes keeping the load but stopping with a message and a non-zero exit status in those cases.

- Fix as proposed (Recommended): Read the whole file first. A missing, unreadable or empty file, or a missing file name, stops the run with a message and a non-zero status.
- Fix, empty file is a no-op: The same, except an empty batch file exits 0 without doing anything.
- Keep FuSION 1 behaviour: Port it exactly, undefined behaviour included.

**Answer (free text):** not all files are mandatory for solve, so missing/unreadable/empty file should stop the solve only if the solve is impossible. the mode of solve should be predetermined based on inputs. for example any solve should have both lfc and pathway files defined, but if coexp csv is not provided, the solve can still take place, similarly for all the additional edge, list of edges, exceptions etc.

S1.01 stays pending. Claude separated the batch file itself (S1.01) from the rule for data inputs and asked again in entry 022.

**Q2 (S1.02).** S1.02 · Usage text. fusion -h prints usage and the command list, then exits 0. No arguments, or an unknown first argument, prints the same and exits 255. Everything goes to stdout.

- Keep (Recommended): The same messages and exit statuses.
- Keep, errors on stderr: The same, but usage printed after a bad invocation goes to stderr.

**Answer (free text):** explain both alternatives

S1.02 stays pending. Claude explained stdout and stderr in chat and asked again in entry 022.

**Q3 (S1.03).** S1.03 · Interactive prompt (sysbio>) and -d. The prompt reads one command at a time from stdin, with its own argument prompts and no $ variables, let or comments. It can't be reached without undefined behaviour, and no real run uses it. -d only prints a notice. The draft proposes dropping both.

- Drop (Recommended): No interactive prompt in 2.0, and -d becomes an unknown option. The Spring Boot frontend covers interactive use.
- Keep, rebuilt: A working prompt that reads lines and runs each exactly like a batch line, with the same $, let and comments, sharing the batch parser.

**Answer:** Drop (Recommended), written into S1.03

**Q4 (S1.04).** S1.04 · End of file without exit. A batch with no exit at the end reads past the end of the command list, which is undefined behaviour. All 81 real files end with exit. The draft proposes treating the end of the file as exit.

- Fix as proposed (Recommended): The end of the file ends the run exactly as exit would.
- Require exit: A batch that ends without exit is reported as an error, with a non-zero status.

**Answer:** Fix as proposed (Recommended), written into S1.04

## 022 · Poll · 2026-11-09 15:04

Asked after Claude explained in chat that S1.01 covers only the batch file, that the free-text answer in entry 021 sets a separate rule for data inputs, and how stdout and stderr differ for S1.02.

**Q1 (Inputs rule).** Inputs rule, from your S1.01 answer. Required inputs for a requested step (any solve needs an LFC file and a network) are checked before anything runs, and a missing one stops the run. An optional input that isn't given just turns its feature off, and one that is given but empty counts as an empty list. What happens when an optional file is named but missing or unreadable?

- Stop before running (Recommended): A named optional file that can't be read stops the run before it starts, so a typo in a coexpression CSV path can't cost a 50-hour run without coexpression constraints. Leaving the file out still just turns the feature off.
- Warn and carry on: Your rule as stated: the run goes ahead without that feature and logs a warning, because the solve is still possible.

**Answer (free text):** stop before running yes. but ideally there should be robust frontend validation for these cases as well. → [decision 0021](../decisions/0021-required-and-optional-inputs.md)

**Q2 (S1.01).** S1.01, the batch file itself (not the data inputs). Without the -b file nothing can run, so by your rule it stops. What about an empty batch file?

- Stop, as proposed (Recommended): A missing, unreadable or empty batch file, or a missing file name, stops with a message and a non-zero status.
- Empty file is a no-op: The same, except an empty batch file exits 0 without doing anything.

**Answer:** Stop, as proposed (Recommended), written into S1.01

**Q3 (S1.02).** S1.02, now that both are explained: where should usage go after a bad invocation?

- Errors on stderr (Recommended): Usage after a bad invocation goes to stderr. fusion -h still prints to stdout and exits 0. This follows the Unix convention and lets the frontend tell errors from output.
- Keep, all on stdout: As in FuSION 1, everything goes to stdout.

**Answer:** Errors on stderr (Recommended), written into S1.02

## 023 · Poll · 2026-11-09 15:06

The second stage 1 item poll.

**Q1 (S1.05).** S1.05 · Splitting lines into tokens. Only the space character separates tokens, so tabs and a trailing \r stay inside them. Every line allocates a list of a million tokens (about 3.4 s per real batch), and commands can't tell a missing argument from an empty one. There's no quoting. The draft proposes that runs of spaces or tabs, and a trailing \r, all separate tokens, that the list fits the line, and that commands get the real token count. Still no quoting.

- Fix as proposed (Recommended): Spaces, tabs and a trailing \r separate tokens, the token list fits the line, and a missing argument can be told from an empty one. No quoting, as now.
- Fix, and add quoting: The same, plus double-quoted tokens that may contain spaces, such as a path with a space in it. exec lines, which carry shell quotes, would bypass this (see S1.14).
- Keep FuSION 1 behaviour: Spaces only, with a million-token list per line.

**Answer:** Fix, and add quoting, written into S1.05

**Q2 (S1.06).** S1.06 · Comments and blank lines. A line whose first token starts with ;; is a comment, and blank lines are skipped; neither is echoed. A ;; later in a line is an ordinary token, and # is not special.

- Keep (Recommended): The same rules.
- Keep, plus # comments: Also treat a line whose first token starts with # as a comment.

**Answer:** Keep (Recommended), written into S1.06

**Q3 (S1.07).** S1.07 · $ variable expansion. At each $, FuSION inserts the first defined name, in sorted order, that matches, so a shorter name beats a longer one. In 12 real batches, $PLOT_FILE_WO_GENE therefore became the value of $PLOT_FILE followed by _WO_GENE, and the plots were misnamed. A $ that matches no name passes through unchanged, so shell $(date) and awk's $1 still work. The draft proposes matching the longest defined name and keeping everything else.

- Fix as proposed (Recommended): The longest matching name wins. Names still need no delimiter, each value is inserted once and not rescanned, and an unmatched $ passes through.
- Fix, and warn on unknown names: The same, plus a warning for a $NAME that matches nothing, except on exec lines, where $ usually belongs to the shell. This addition is mine.
- Keep FuSION 1 behaviour: The first name in sorted order, as now.

**Answer:** Fix, and warn on unknown names, written into S1.07

**Q4 (S1.07 cmd).** S1.07, second part · The command name. FuSION picks the command from the first token before expanding it, so a line that starts with a variable never runs, although the echo shows the expanded name. No real file starts a line with a variable. The draft proposes nothing here; this question is my addition.

- Expand, then dispatch (Recommended): Every token, the command name included, is expanded before dispatch, so the echo always shows what runs.
- No variable as command: A $ in the command position is an error.
- Keep FuSION 1 behaviour: Dispatch on the unexpanded name.

**Answer:** Expand, then dispatch (Recommended), written into S1.07

## 024 · Subagent prompt · Phase 1 resume message to stages 3 and 4 · 2026-11-09 15:06

The stage 1 and stage 2 subagents had both finished, so the stage 3 and stage 4 subagents were resumed ([decision 0016](../decisions/0016-resume-subagents-in-pairs.md)) with the text of entry 017, word for word.

## 025 · Poll · 2026-11-09 15:10

The third stage 1 item poll.

**Q1 (S1.08).** S1.08 · let <key> <value>. The key is expanded before FuSION checks it, so redefining $X fails with a misleading 'doesn't start with $' message; that's how the $PLOT_FILE_WO_GENE definition was lost. The first definition wins, the value is expanded when let runs, a missing value is empty, and extra tokens are silently dropped. The draft proposes taking the key literally and reporting a redefinition as an error that names the key.

- Fix as proposed (Recommended): The key is taken literally. A redefinition is reported as an error naming the key, and the first value stays. The value is expanded when let runs, and a missing value is empty. Extra tokens after the value are an error too; that part is my addition.
- Fix, later definitions win: The key is taken literally, but a redefinition replaces the value instead of being an error.
- Keep FuSION 1 behaviour: The key is expanded first, as now.

**Answer:** Fix as proposed (Recommended), written into S1.08

**Q2 (S1.09).** S1.09 · #LASTGRAPHNUM. A let value of exactly #LASTGRAPHNUM becomes the id of the most recently created graph, fixed when the let runs. The real batch uses it to run fb_rch on the merged graph. Before start, FuSION reports an error but still stores the literal text #LASTGRAPHNUM. The draft proposes reporting the error and leaving the variable undefined.

- Fix as proposed (Recommended): Report the error and leave the variable undefined. Otherwise #LASTGRAPHNUM still captures the latest graph's id at the moment the let runs.
- Keep FuSION 1 behaviour: Store the literal text, as now.

**Answer:** Fix as proposed (Recommended), written into S1.09

**Q3 (S1.10).** S1.10 · Command echo. Before running, each line is printed to stdout in expanded form, followed by a line of 31 dashes, and flushed so it appears before the command's own output. FuSION 1's GUI builds its progress display by parsing these lines, so the format works as an interface.

- Keep (Recommended): The same format. How the Spring Boot frontend tracks progress is decided in Phase 2.
- Drop: No echo. The frontend gets progress from a channel that Phase 2 designs.

**Answer:** Keep (Recommended), written into S1.10

**Q4 (S1.11).** S1.11 · Dispatch and unknown commands. Command names match exactly and case-sensitively, with short aliases such as rgx, mff and sz. An unknown name prints 'Did you enter one of the following valid commands?' and the full command list, and the run carries on. What a failure does to the run is S1.28.

- Keep (Recommended): Exact matching, with the same names and aliases.
- Keep, suggest nearest name: The same, but an unknown name prints the closest valid command instead of the whole list.

**Answer:** Keep (Recommended), written into S1.11

## 026 · Poll · 2026-11-09 15:14

The fourth stage 1 item poll.

**Q1 (S1.12).** S1.12 · help and the command list. The list advertises fwd_bkwd_rch, which doesn't run. It leaves out upfx and create_up_down_file, which do, and it calls rgx's y/n tool-written flag a merge flag. The draft proposes building the list and the dispatcher from one table, so they always agree.

- Fix as proposed (Recommended): One command table drives dispatch, help and the argument names. Named arguments (decision 0018) need that table anyway.
- Keep FuSION 1 behaviour: The hand-written list, mismatches included.

**Answer:** Fix as proposed (Recommended), written into S1.12

**Q2 (S1.13).** S1.13 · exit. Ends the run and ignores any arguments. FuSION 1 then returns 0 whatever failed earlier; the status after a failure is S1.28. The two logger lines printed at exit come from an unused component (S1.24).

- Keep (Recommended): exit ends the run, and its status reflects failures, as S1.28 decides.
- Keep, reject arguments: The same, but exit with arguments is an error.

**Answer:** Keep (Recommended), written into S1.13

**Q3 (S1.14).** S1.14 · exec <shell command>. FuSION joins the rest of the line with single spaces, runs it with /bin/sh, and throws the status away. So a failing mkdir or cat is ignored, and doubled spaces inside shell quotes collapse, as in the 'pathz3 wo  FSC' timing labels. The draft proposes keeping the shell pass-through and reporting a non-zero status with the batch line number.

- Fix both (Recommended): Report a non-zero status on stderr with the batch line number, and pass the rest of the line to the shell exactly as written after $ expansion, which fits the quoting you chose in S1.05. The second part is my addition.
- Fix the status only: The draft's proposal: report a non-zero status, but the line is still rejoined with single spaces.
- Keep FuSION 1 behaviour: The status is discarded and spaces collapse.

**Answer:** Fix both (Recommended), written into S1.14

**Q4 (S1.15).** S1.15 · start <inter_db_map> <exception_file> <symbol_map> <pathway_name_map>. Creates the graph manager and loads four files; a file that won't open gives only a message and an empty map. Eight commands crash if run before start, while cudf, gsea, pex and add work without it. The draft proposes keeping the four-file load and its output, and failing with a clear message when a command that needs start runs before it.

- Fix as proposed (Recommended): Keep the four-file load and its output. A command that needs start fails clearly if it runs first. Under decision 0021, a named start file that can't be read stops the run before it starts.
- Keep FuSION 1 behaviour: Those eight commands crash before start, and an unreadable file gives an empty map.

**Answer:** Fix as proposed (Recommended), written into S1.15

## 027 · Poll · 2026-11-09 15:18

The fifth stage 1 item poll.

**Q1 (S1.16).** S1.16 · Inter-database id map (start, file 1). Read as two columns, id → representative id, with later lines winning. All 17 copies in the repo are empty, so the intended content can't be checked, and mff reads the same file again with a different meaning, as a symmetric synonym list. How merging uses it is stage 2's.

- Keep (Recommended): Port the two-column reader. Stage 2's review decides how merging uses the map and whether mff's second reading stays. A cross-database id map is also a natural place for other pathway sources to plug in (decision 0020).
- Decide with stage 2: Leave this item pending until stage 2's merge items are settled.

**Answer:** Keep (Recommended), written into S1.16

**Q2 (S1.17).** S1.17 · Merge-exception file (start, file 2). The first token of each line joins the set of ids that are never merged. All 81 batch files build this file the same way: a shell cat of cudf's up and down lists, the source and target files, and hsa_ids_not_to_be_merged.txt.

- Keep (Recommended): Port the first-token-per-line reader. Batches keep building the file as they do now.
- Keep, add genes in memory: Also have 2.0 add cudf's up and down genes and the source and target to the set itself, as every real batch does by hand, so the cat line becomes unnecessary. Old batches still work (decision 0017), but merging then has to know the source and target.

**Answer:** Keep, add genes in memory, written into S1.17

**Q3 (S1.18).** S1.18 · Gene-symbol map (start, file 3). Two columns, id → display name, keeping only the first word of the name. A missing name falls back to the id. No real name contains whitespace.

- Same rule as S1.19 (Recommended): Read the name the way S1.19 decides for pathway names, so both maps share one reader. On the real file the result is identical. The draft proposed keeping it as it is.
- Keep: Port it as it is, keeping the first word of the name.

**Answer:** Same rule as S1.19 (Recommended), written into S1.18

**Q4 (S1.19).** S1.19 · Pathway-name map (start, file 4). Two columns, id → name, but the name is cut at its first whitespace, so 23 Reactome-style names lose their tail: 'Nectin/Necl<TAB>trans_heterodimerization' is stored as 'Nectin/Necl'. A missing name falls back to the id. The draft proposes taking the name as the rest of the line after the first tab.

- Fix as proposed (Recommended): The name is the rest of the line after the first tab. Later lines win, and a missing name falls back to the id. My additions: a further tab becomes an underscore, the file's convention for spaces, so tab-separated outputs don't break, and blank lines are skipped (today they add an empty key).
- Keep FuSION 1 behaviour: Names are cut at the first whitespace.

**Answer:** Fix as proposed (Recommended), written into S1.19

## 028 · Poll · 2026-11-09 15:22

The sixth stage 1 item poll.

**Q1 (S1.20).** S1.20 · Finding bundled helper scripts. pex and gsea each locate the binary's directory and run python3 <dir>/supporting_files/<script>.py. Linux resolves symlinks (/proc/self/exe) and macOS doesn't (_NSGetExecutablePath), and plot_po.sh is called through an absolute path written into the batch files. The draft proposes finding the install directory once at startup, resolving symlinks the same way on both systems, and using it for every bundled helper.

- Fix as proposed (Recommended): One install directory, found once and in the same way on Linux and macOS, serves every bundled file. Whether any Python helpers remain is a Phase 2 question, since the backend is meant to be all C++.
- Keep FuSION 1 behaviour: Each command finds its helper itself, differently on Linux and macOS.

**Answer:** Fix as proposed (Recommended), written into S1.20

**Q2 (S1.21).** S1.21 · supporting_files/config.txt. Nothing reads it. It holds settings (MODULENAME=test3, CYCLES=15, …) for the hardware-verification tool FuSION's code came from.

- Drop (Recommended): Leave it out of 2.0.
- Keep: Carry the file over unchanged.

**Answer:** Drop (Recommended), written into S1.21

**Q3 (S1.22).** S1.22 · node_type.txt, edge_type.txt, edge_subtype.txt and path.txt. Each maps a name to a number (phosphorylation => 1, …). No live code reads them; only the unused old GraphManager would, and through wrong paths. The subtype names that matter are hard-coded, and stages 2 and 5 specify them.

- Drop (Recommended): Leave them out. The vocabulary that matters is specified in stages 2 and 5.
- Keep as reference data: Carry them over as a record of KEGG's vocabulary.

**Answer:** Drop (Recommended), written into S1.22

**Q4 (S1.23).** S1.23 · supporting_files/color_files/: 22 colour palettes, colour maps and id lists. Only the disabled graph-display commands read them.

- Drop (Recommended): Leave them out of 2.0.
- Keep: Carry them over for a future graph display.

**Answer (free text):** the graph display feature is actually crucial once implemented efficiently and seamlessly. need to ideate on the right way to develop this. the color files themselves are replaceable so no need to retain them. → S1.23 drop, and [decision 0022](../decisions/0022-graph-display.md)

## 029 · Poll · 2026-11-09 15:27

The seventh stage 1 item poll.

**Q1 (S1.24).** S1.24 · Startup objects nothing uses. Before the first command, FuSION builds an expression manager, a DAG manager, a configuration object and a Z3 context. The expression manager parses a rules file from a path fixed at build time, so FuSION won't start if the source tree moves, and its lexer prints the 450 spaces at the top of every log.

- Drop (Recommended): None of them in 2.0.
- Keep: Port them as they are.

**Answer:** Drop (Recommended), written into S1.24

**Q2 (S1.25).** S1.25 · Debug log mylog.txt. A fixed-name log opened in the launch directory before main runs. sz and the debug traces write to it, batch files move it into the run directory with exec mv, and two runs started in one directory overwrite each other's log. upfx renames it, but crashes if used before start. The draft proposes one debug log per run with a per-run name, renamable before start.

- mylog.txt in run dir (Recommended): My suggestion: if each run gets its own output directory (S1.26), the log keeps its name inside it and nothing needs renaming. This depends on S1.26.
- Per-run name (the draft's): One debug log per run with a per-run name, which can be set before start.
- Keep FuSION 1 behaviour: mylog.txt in the launch directory.

**Answer:** Per-run name (the draft's), written into S1.25

**Q3 (S1.26).** S1.26 · Outputs land in the launch directory. FuSION never changes directory, so fixed-name outputs (mylog.txt, PO.dat, limits_timefile.txt, the SMT dumps, the fb_rch files, edges_merged_filename) are written wherever it was started, and batch files move them afterwards with exec mv (8 lines in the real batch). Two runs started in one directory overwrite each other. The draft proposes one output directory per run for every fixed-name output, with each stage keeping its file names.

- Fix as proposed (Recommended): One output directory per run holds every fixed-name output. It is also where partial runs (decision 0019) find saved steps. Phase 2 designs how a batch names that directory.
- Keep FuSION 1 behaviour: The launch directory, with exec mv afterwards.

**Answer:** Fix as proposed (Recommended), written into S1.26

**Q4 (S1.27).** S1.27 · Console number format. Every number printed to the console shows exactly two decimals, so a value below 0.005 shows as 0.00. Output files keep C++'s default of six significant digits unless a command sets its own.

- Keep (Recommended): Two decimals on the console, as now.
- Fix, full precision: Print numbers on the console with enough digits to be exact.

**Answer:** Fix, full precision, written into S1.27

## 030 · Poll · 2026-11-09 15:33

The eighth stage 1 item poll.

**Q1 (S1.28).** S1.28 · Failures and exit status. Most failures print a message, on stdout or stderr inconsistently, and the run carries on. A few call exit(1). A blank numeric argument aborts the run with status 134 and no line number. Otherwise the status is always 0, and the GUI ignores it. The draft proposes catching errors per command, reporting each with its batch line number, and exiting non-zero if anything failed.

- Fix as proposed (Recommended): Each command's errors are caught and reported on stderr with the batch line number, and the exit status is non-zero if anything failed.
- Keep FuSION 1 behaviour: Mixed messages, crashes on bad numbers, and status 0.

**Answer:** Fix as proposed (Recommended), written into S1.28

**Q2 (S1.28 stop).** S1.28, second part · After a command fails, does the batch stop or carry on? FuSION 1 carries on, and the draft leaves this open. A failing exec counts as a failure (S1.14).

- Stop at first failure (Recommended): Later steps usually need earlier ones, so carrying on can waste hours on bad input. The run exits non-zero at once, and partial runs (decision 0019) let you restart from the last good step.
- Carry on, fail at the end: Run every line, then exit non-zero if any failed.

**Answer:** Stop at first failure (Recommended), written into S1.28

**Q3 (S1.29).** S1.29 · Compile-time flags. Ten flags are fixed in the Makefile (DISCARD_INDIRECT_EDGES, DEBUG_FLAG and others); two do nothing, and no run output records them. The build also has no -O, so FuSION's own code runs unoptimised (the build is a Phase 2 matter). The draft proposes that each owning stage documents its flag's fixed value as ordinary behaviour, and that the two flags with no effect are dropped.

- Fix as proposed (Recommended): Each flag's behaviour is specified, as fixed behaviour, in the stage that owns it, and the two that do nothing go.
- Make them run-time options: Each flag becomes a setting a batch can change, recorded in the run's output.

**Answer (free text):** some flags are relevant to the front end user, some flags are meant for debug. both should be treated differently. the 2 flags that do nothing can go (tell me which ones they are)

S1.29 stays pending. Claude named the two flags in chat, after checking every compiled file and the `DSManagers` build: `COMPUTE_MINCUT_EDGES_FLAG`, whose only test is inside `#if 0`, and `QE`, which no file compiled with it tests, while the `DSManagers` code that does test it is built without it (`DSManagers/Makefile.include:28-30`). Claude also gave a provisional split of the other eight and asked about their treatment in entry 031.

**Q4 (Graph view).** When should we work out how to build the graph display (decision 0022)?

- After stages 6 and 8 (Recommended): Those stages specify today's psp and pex plots and the GUI, so the work starts from what exists, and it then feeds Phase 2.
- Now: Before the stage 2 review starts.

**Answer:** After stages 6 and 8 (Recommended) → [decision 0022](../decisions/0022-graph-display.md)

## 031 · Poll · 2026-11-09 15:37

Asked after Claude named the two flags that do nothing (entry 030, Q3) and gave a provisional split of the other eight: debug (`DEBUG_FLAG`, `ASSERT_FLAG`), and likely to change results (`DISCARD_INDIRECT_EDGES`, `DUMMY_EDGE_FLAG`, `DUP_CHECK_FLAG_SET`, `OPT_GAP_HEUR_FLAG`, and the two mincut flags, whose reachability stages 4 and 5 must show).

**Q1 (S1.29).** S1.29 · QE and COMPUTE_MINCUT_EDGES_FLAG go. How should the two kinds of remaining flag be treated? Each flag's kind, user-facing or debug, is confirmed in the stage that owns it (2, 4 or 5).

- Run-time + debug switch (Recommended): A flag that changes results becomes a setting a batch or the frontend can change, and its value is recorded in each run's output. Debug flags (debug-log traces, assertions) sit behind one debug option that the frontend doesn't show.
- Run-time + build-time debug: Flags that change results become run-time settings, as above, but debug flags stay compile-time, for developers only.

**Answer:** Run-time + debug switch (Recommended), written into S1.29. All 29 stage 1 items are now decided.
