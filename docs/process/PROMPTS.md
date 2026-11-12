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

## 032 · Poll · 2026-11-09 15:41

Asked after stage 1 was fully decided. Claude proposed two local commits by concern, (1) "Set up Phase 1 and correct decision 0004" (decisions 0007 to 0016, the 0004 fix, their index rows, log entries 004 to 017) and (2) "Review spec stage 1: batch language" (the stage 1 spec, decisions 0017 to 0022, their index rows, log entries 018 to 031), with the unreviewed stage 2 draft left out.

**Q1 (Commit).** May I commit the reviewed work locally, as described above? The unreviewed stage 2 draft stays out either way.

- Two commits (Recommended): 'Set up Phase 1 and correct decision 0004', then 'Review spec stage 1: batch language', each ending with a Co-Authored-By: Claude line. The shared log and index are split at the stage 1 boundary.
- One commit: Everything reviewed in a single commit, 'Phase 1 setup and spec stage 1 review'.
- Show me the diff first: I print the full diff of the tracked files and list the new files, then ask again.

**Answer:** Two commits (Recommended). This entry itself is left for the next commit.

## 033 · Poll · 2026-11-10 04:18

The first stage 2 item poll (`docs/spec/02-graph-input-and-merge.md`). Before polling, Claude checked 98 of the draft's code citations against FuSION 1 and recomputed its headline counts, then corrected three slips: the RAS-trap relation is the 81st in file order, not the 80th (S2.03); a citation should read `GraphManagerNew.cc:8736`, not `8735` (S2.15); and S2.10 now cites `XMLParser.cc:1476-1478`, where an edge is marked undirected.

**Q1 (S2.01).** S2.01 · Graph store and ids. Every graph gets an integer id from one counter, and 0 means the latest graph. Node and edge ids are session-wide, so the same gene has a different node id in every graph. Nothing is ever freed: the 163-pathway build keeps 162 intermediate copies. Genesis records are written but never read. The draft proposes keeping integer graph ids and 0 for the latest, freeing merge intermediates and dropping genesis records.

- Fix as proposed (Recommended): Integer graph ids and 0 for the latest stay. Merge intermediates are freed, and genesis records go.
- Keep FuSION 1 behaviour: Keep every intermediate graph and the genesis records.

**Answer:** Fix as proposed (Recommended), written into S2.01

**Q2 (S2.02).** S2.02 · Node record. A node holds a session-wide id, its key (rep_id), its member ids, its KGML type, a complex's components, display names, in- and out-edge lists, and three fields no stage-2 code fills (description, compartment, pathways). Each graph keeps insert-only maps from key to node and from id to key. The draft found no problems beyond accessors that copy whole lists.

- Keep (Recommended): Port the record as described. Accessors that don't copy are an implementation detail. The unfilled compartment field may serve a future location check.
- Keep only filled fields: Leave out description, compartment and pathways, which nothing in stage 2 fills.

**Answer:** Keep (Recommended), written into S2.02

**Q3 (S2.03).** S2.03 · Looking up an id. An id resolves to the first node that registered it. In the old master, HRAS resolves to the node labelled RRAS2, which is the RAS trap. In raw merges every id sits in exactly one node, but that node can be a whole family (HRAS → a 6-gene node, EGF → a 52-gene node), and nothing says so. The draft proposes keeping the lookup but reporting the node's members whenever an id belongs to several nodes or to a node with other members.

- Fix as proposed (Recommended): Keep the lookup, and report the members whenever an id lands in a node with other genes or in several nodes.
- Fix, refuse ambiguous ids: Also stop the run when a source, target, FSC or constraint id belongs to several nodes, so the user must choose.
- Keep FuSION 1 behaviour: First claim wins, silently.

**Answer:** Fix as proposed (Recommended), written into S2.03

**Q4 (S2.04).** S2.04 · Edge record. An edge holds its id, its endpoints, its KGML type, an ordered subtype list (possibly including dummy_u_to_d), its pathways and an experiment score. Two edges are the same when source, target, type and the exact ordered subtype list match. The constructor leaves seven fields uninitialised; stage 2 never reads them, and open question 7 asks whether stages 4 and 5 do.

- Keep (Recommended): Port the fields and the exact ordered-list identity, with every member initialised. Whether the never-read fields stay is settled with stages 4 and 5.
- Keep, unordered subtypes: The same, but two edges match when their subtype sets are equal, whatever the order.

**Answer:** Keep (Recommended), written into S2.04

## 034 · Poll · 2026-11-10 04:22

The second stage 2 item poll.

**Q1 (S2.05).** S2.05 · rgx <file> <y|n> <thres>. Reads one graph. y means the file was written by wgx (S2.12); the threshold is S2.07's. A missing, unreadable or malformed file, or a missing threshold, throws an uncaught exception and aborts the whole run. This reader is KGML-only, which decision 0020 flags: other pathway sources plug in through the Phase 2 mechanism.

- Fix as proposed (Recommended): Keep the arguments and console lines. A bad file or argument is a command error, not a crash, and under decision 0021 the file is checked before the run starts.
- Keep FuSION 1 behaviour: Uncaught exceptions abort the run.

**Answer:** Fix as proposed (Recommended), written into S2.05

**Q2 (S2.06).** S2.06 · What is read from a KGML file. Entries (id, name with colons removed, type, group components) and relations (entries, type, subtypes, experiment_score) are read; nodes are made only for entries a relation reaches. Coordinates, links and reactions are ignored. Dangling references, repeated entry ids, cyclic groups or empty groups crash or go silently wrong, though no current file has any.

- Fix as proposed (Recommended): Accept the same subset of KGML, and reject a file with dangling references, repeated ids or cyclic groups, with a message. Under decision 0020 this becomes one input reader among others.
- Keep FuSION 1 behaviour: Crash or go wrong silently on such files.

**Answer:** Fix as proposed (Recommended), written into S2.06

**Q3 (S2.07).** S2.07 · Entry → node keys and the split threshold. Ids in the exception set get their own nodes; the rest of a box collapse into one node keyed by its lexically smallest id when the box has more than thres ids. Every batch uses thres = 1. The key has no biological meaning, and node identity depends on this run's differentially expressed genes. The draft proposes keeping the rule, with 1 as the default, and leaving family merging to roadmap objective 2.

- Keep (Recommended): Port the exception-first, collapse-above-threshold rule, with thres defaulting to 1. How families should merge is revisited under roadmap objective 2.
- Keep, drop the threshold: Always collapse, as thres = 1 does. Old batches still pass 1 and run (decision 0017); any other value is rejected.

**Answer:** Keep (Recommended), written into S2.07

**Q4 (S2.08).** S2.08 · Bug: genes vanish. When a later multi-gene box collapses to a key that already names a node in the same pathway, its other genes are registered but never added to that node, so they disappear from the merge. Eleven ids vanish from the 42-pathway merge, including RUNX1, TSHB, SERPINF2 and seven glutamate-receptor subunits; Claude reproduced the list. The draft proposes adding every id of a box to the node the box resolves to.

- Fix as proposed (Recommended): Every id in a box joins the node the box resolves to, so no gene is lost.
- Keep FuSION 1 behaviour: Those genes keep vanishing.

**Answer:** Fix as proposed (Recommended), written into S2.08

## 035 · Poll · 2026-11-10 04:26

The third stage 2 item poll.

**Q1 (S2.09).** S2.09 · Complexes (KGML groups). A group becomes one complex node per combination of its components' nodes, keyed c_s_…_c_e and labelled [A+B]. After a raw read each complex gets two PPrel edges per component (component → complex and back), both tagged component and dummy_u_to_d, and both copy the pathway list of the graph's first edge, which crashes when a file has groups but no edges. Tool-written reads add no component edges. The draft proposes keeping complexes and their two-way component edges, but giving component edges the file's own pathway id.

- Fix as proposed (Recommended): Keep complexes and their two-way component edges, labelled with the file's own pathway. What dummy_u_to_d means when both directions carry it is settled with stage 5 (open question 5).
- Keep FuSION 1 behaviour: Copy the first edge's pathways, crash included.

**Answer:** Fix as proposed (Recommended), written into S2.09

**Q2 (S2.10).** S2.10 · From a relation to edges. Subtypes are normalised (indirect effect → indirect, binding/association → association, …). One edge is made per pair of the two entries' nodes. A duplicate with the same endpoints, type and subtype list is skipped, and its experiment score is lost. Association, dissociation and mapping edges get a reverse edge, with exactly one direction tagged dummy_u_to_d by key order. A relation whose two entries share a key makes a self-loop (18 in the 42-pathway merge).

- Keep (Recommended): Port the rules as described.
- Keep, max score on duplicates: My addition: a skipped duplicate's experiment score still counts, with the larger kept, as the cross-pathway merge does (S2.20). It can change which edges pass the frozen-edge threshold in stage 5.
- Keep, no self-loops: The same, but a relation whose two ends land on one node makes no edge. Stage 5 says what self-loops do in the solve.

**Answer:** Keep, max score on duplicates, written into S2.10

**Q3 (S2.11).** S2.11 · Indirect-effect relations are discarded, fixed at compile time. The source comment says reachability is preserved, and it isn't: in the old master 181 of 598 entry pairs lose every path, and 48 of 186 in the 42-pathway merge. 176 of the 204 discarded relations also carried another subtype, whose sign is lost, and 22 nodes are left isolated. Under S1.29 this becomes a run-time setting recorded in the output.

- Fix, default discard (Recommended): A run option, recorded in the output and applied before nodes are made. By default it discards, as FuSION 1 does, so old batches give the same results (decision 0017).
- Fix, default keep as 1+: The same option, but by default indirect edges are kept and counted as one hop of unknown length, as in your roadmap objective 2 design.

**Answer (free text):** both fixes are good, should be configuration with default being discarding indirect effects. old batches need not give the same results provided we are improving so much → written into S2.11, and [decision 0023](../decisions/0023-results-may-change-where-improved.md), which amends decision 0017

**Q4 (S2.12).** S2.12 · Tool-written read (y). The y flag tells rgx and mff that wgx wrote the file. Then there's no duplicate test, no reverse edges or dummy tags, no component edges, and pathways come from the file's pathways attribute. The flag is trusted, not checked, so the old master, which wgx didn't write, gets no component edges and one-way associations in 65 batches. The draft proposes a reader that recognises FuSION's own merged-graph format instead of trusting a flag.

- Fix as proposed (Recommended): FuSION's merged-graph format is recognised from the file itself. Old batches may still pass y. How the old master is read is S2.27's question.
- Keep FuSION 1 behaviour: Trust the flag.

**Answer:** Fix as proposed (Recommended), written into S2.12

## 036 · Poll · 2026-11-10 04:41

Asked after the stage 3 and stage 4 subagents finished, and after the stage 3 subagent reported that FuSION 1 had changed on 2026-11-09. Claude confirmed the change in chat: the UNDEFINED/TIMEOUT labelling fix, with one line added to `main.cc` (4733) and 11 lines to `GraphManagerNew.cc` (from 8180), plus nine new batch files.

**Q1 (Reference).** FuSION 1 changed under the spec. On 2026-11-09 at 19:16 another session fixed the UNDEFINED/TIMEOUT labelling, adding 1 line to main.cc (at 4733) and 11 to GraphManagerNew.cc (around 8180), so later citations are off by those amounts. Behaviour described in stages 1 to 4 is unaffected, apart from stage 5's timeout lead. How should the spec's reference be held steady?

- Pin a snapshot (Recommended): Copy FuSION 1's current source files (about 12 MB) into the read-only folder /home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10. Every stage cites it. I shift stage 1 and stage 2 citations to it, stage 1's in a correction commit, and the remaining subagents use it. Run data under runs/ stays live.
- Follow the live tree: Keep citing FuSION 1's working tree, and re-check the affected citations each time it changes.

**Answer:** Pin a snapshot (Recommended) → [decision 0024](../decisions/0024-frozen-fusion1-snapshot.md)

**Q2 (Resume text).** Stages 5 and 6 resume next. Their logged resume message says nothing in FuSION 1 has changed, which is no longer true. Approve this replacement? It's shown for the snapshot choice; with the live tree, its third sentence instead says to cite the current files.

- Approve (Recommended): Resume stages 5 and 6 with this text, logged in full, and later stages 7 and 8 the same way. Preview shown with this option:

  ```text
  Your earlier run stopped before you finished, because
  of a usage limit or the end of the session. Since you
  started, FuSION 1 has changed: on 2026-11-09 another
  session fixed the UNDEFINED/TIMEOUT labelling of Z3
  results. It added z3_overall_timeout_ms to
  Linux/include/GraphManagerNew.h, 11 lines after the
  constructor in Linux/src/GraphManagerNew.cc (later
  lines moved by 11 or more), one line at
  Linux/src/main.cc:4733 (later lines moved by 1), and a
  new timeout check from GraphManagerNew.cc:29478 on.
  Nine batch files were also added under
  runs/egfr_b10_gsea_fdr044_candidates/. From now on,
  read and cite FuSION 1 code from the frozen copy at
  /home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10
  (same relative paths), not the live tree. Re-check
  every line number you noted earlier, then carry on and
  finish the task exactly as your original prompt
  describes: write only your own stage file, then reply
  in at most 300 words.
  ```

- Restart them fresh: Start new subagents for stages 5 to 8 with the original prompts plus a note about the change, discarding their saved progress.

**Answer:** Restart them fresh → [decision 0025](../decisions/0025-fresh-subagents-stages-5-to-8.md)

**Q3 (S2.13).** S2.13 · Merge-exception set. An id in the set always gets its own node and never joins a box. The batch builds the set from the up and down genes, the source, the target and hsa_ids_not_to_be_merged.txt, and S1.17 decided 2.0 adds the first four itself. The FSC is not added, so an FSC that isn't differentially expressed lands inside its family node: the RAS trap. The draft proposes always adding the FSC being tested.

- Fix as proposed (Recommended): The FSC joins the set automatically, along with S1.17's up and down genes, source and target. Merging must therefore know them, and Phase 2 designs how.
- Keep FuSION 1 behaviour: The FSC is not added.

**Answer:** Fix as proposed (Recommended), written into S2.13

**Q4 (S2.14).** S2.14 · The inter-database map in merging. One file feeds two mechanisms: start's all_id_map, which would key a gene by its mapped id, and mff's naming-map step, which would add protein nodes and mapping edges. Neither has ever run, because all 17 copies of the file are empty, and the code has four faults, including a duplicate node for any id inside a family. The draft proposes dropping both. S1.16 kept the file's reader, so dropping both leaves that reader unused.

- Drop both, redesign later (Recommended): Remove both uses. Mapping ids across sources is designed afresh in Phase 2, for decision 0020's plug-and-play inputs, and may reuse S1.16's two-column file.
- Keep the keying, fixed: Keep all_id_map's keying path with its inconsistency fixed, and drop only the naming-map step.
- Keep FuSION 1 behaviour: Both, as written.

**Answer:** Drop both, redesign later (Recommended), written into S2.14

## 037 · Subagent prompt · Note for the fresh stage 5 to 8 subagents · 2026-11-10 07:36

Each fresh stage 5 to 8 subagent ([decision 0025](../decisions/0025-fresh-subagents-stages-5-to-8.md)) gets entry 006, a blank line, its stage section (entries 011 to 014), a blank line, and this note. Sent first to new subagents for stages 5 and 6 at 07:36; the stage 7 and 8 sends are logged as their own entries.

```text
Note added on 2026-11-10, after the prompt above was first written. On 2026-11-09 another session changed FuSION 1 to fix the UNDEFINED/TIMEOUT labelling of Z3 results. It added `z3_overall_timeout_ms` to `Linux/include/GraphManagerNew.h`; an initialisation and then a setter and getter to `Linux/src/GraphManagerNew.cc`, so old line numbers from 8180 to 8215 in that file moved down by 1 and those from 8216 on by 11; a helper `z3_unknown_is_timeout` at `GraphManagerNew.cc:29342`, with new checks from line 29478 on, which moved the lines after it further; and one line at `Linux/src/main.cc:4733` (`macOS/src/main.cc:4761`), so later lines in main.cc moved down by 1. Line numbers in your stage section's leads refer to the tree before that change. If a lead says Z3 timeouts are logged as UNDEFINED, it describes runs made before the fix: describe the code as it is now, and say how earlier runs were labelled. Nine batch files were also added under `runs/egfr_b10_gsea_fdr044_candidates/`.

The reference is now a frozen copy of FuSION 1's source files at /home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10, with the same relative paths; its README.txt says what it holds. Read and cite FuSION 1 code from there, not from the live tree, which may keep changing. This replaces the ground rule that names the Linux working tree as the reference. Run data under /home/ks-149-u24/Desktop/arjun/FuSION/runs/ and the KGML data directories are still read from the live tree.
```

## 038 · Poll · 2026-11-10 07:39

The fifth stage 2 item poll, asked after the snapshot was made, the stage 1 and 2 citations were moved to it, and the stage 1 correction was committed (ce347a4).

**Q1 (S2.15).** S2.15 · How stage 2 uses the symbol and pathway-name maps: for labels only. A node's display name is its key's symbol, or the key itself, and the edge report names nodes by all their members' symbols. Two lookups use operator[], which inserts an empty name for every unknown id; that once made a second read in one session segfault, and a fallback now hides it. 291 of the 2175 gene ids in the 42-pathway merge have no symbol. The draft proposes lookups that never insert, keeping the fallback to the id.

- Fix as proposed (Recommended): Lookups never insert, and an id with no symbol is still labelled with the id itself.
- Keep FuSION 1 behaviour: operator[] lookups, as now.

**Answer:** Fix as proposed (Recommended), written into S2.15

**Q2 (S2.16).** S2.16 · mff <list> xml <thres> <y|n> [map files…] -1. mff reads every listed file as rgx would, then folds them into one graph; with one file it just returns that graph. The help line misstates the arguments, and a blank line, bad path or CRLF in the list aborts the run. A one-file mff writes no edge report, so the batch's next mv fails. The draft proposes keeping mff <list> <thres> <y|n>, dropping the format token and the map-file list (S2.14 dropped their only use), and checking every listed path before reading any.

- Fix, old lines still run (Recommended): The new form is mff <list> <thres> <y|n>. FuSION 1's xml token and map-file list are still accepted, with a warning that they're ignored, so old batches run. Every listed path is checked up front (decision 0021).
- Fix as drafted: The format token and map-file list are no longer accepted, so old mff lines need editing.
- Keep FuSION 1 behaviour: The current arguments and failure modes.

**Answer:** Fix, old lines still run (Recommended), written into S2.16

**Q3 (S2.17).** S2.17 · Merging nodes across pathways. mff folds the pathways in list order, building a new graph at each step, and unions every pair of boxes that share a non-exception id, transitively, across all pathways. This makes huge family nodes. In the 42-pathway merge: 52 growth-factor ligands (EGF, FGFs, VEGFs, PDGFs, INS, IGF, HGF), 44 chemokines, 19 G-protein subunits joined through one GNA12+GNG12 box, and 6-gene RAS and PKC nodes. The 87-pathway merge has a 161-gene GPCR node. The result can depend on list order. Was this transitive union intended? Your call matters here (open question 3).

- Fix: explicit union (Recommended): The draft's proposal: the same union, computed in one order-independent pass, with every merged node's members listed in the output. Whether families should be unioned at all is revisited under roadmap objective 2.
- No union across pathways: Boxes from different pathways merge only when they hold exactly the same genes. That avoids the 52- to 161-gene nodes, but a gene can then sit in several nodes, which S2.03's reports flag. Results change, which decision 0023 allows.
- Keep FuSION 1 behaviour: The order-dependent fold, as now.

**Answer:** Fix: explicit union (Recommended), written into S2.17

**Q4 (S2.18).** S2.18 · Merge crash on chained keys, fixed only in the working tree. When pathways box overlapping families under different keys, the key map chains (A → B → C). The committed code followed one step in the edge phase and could crash; the working tree's final_rep_id, now in the snapshot, follows the whole chain. Three other places still follow one step, which can't crash but can key a complex by a stale member key.

- Keep the fix (Recommended): Edge endpoints resolve through the whole chain. This only matters if S2.17 keeps chain-following, since an explicit union has no chains.
- Keep, and fix the rest: Also make the three remaining one-step lookups follow the whole chain.

**Answer:** Keep the fix (Recommended), written into S2.18

## 039 · Poll · 2026-11-10 07:52

The sixth stage 2 item poll.

**Q1 (S2.19).** S2.19 · A node-type conflict drops a whole pathway. If a node in the next pathway shares its key with an existing node of a different KGML type, mff prints a warning, abandons that whole merge step and carries on, so the pathway silently vanishes from the result. This has never fired in a recorded run. The draft proposes treating it as an error naming the node and both pathways; under S1.28 the run then stops.

- Fix as proposed (Recommended): An error naming the node and both pathways. The run stops at it, as S1.28 decided.
- Fix, skip only that node: Keep merging the pathway, leave out only the conflicting node, and warn.
- Keep FuSION 1 behaviour: Drop the whole pathway with a warning.

**Answer:** Fix as proposed (Recommended), written into S2.19

**Q2 (S2.20).** S2.20 · Merging edges across pathways. An edge matches an existing one when the endpoints, type and ordered subtype list are equal (or equal plus dummy_u_to_d). On a match its pathways are added and the larger score is kept. The 42-pathway merge has 140 node pairs with 2 or 3 parallel edges; RAF → MEK, for example, carries phosphorylation; activation + phosphorylation; and activation as three edges. It also has 4 pairs with opposite signs and 18 self-loops. All of these reach the solver, where each parallel edge is one more relaxable edge (stage 5's concern).

- Keep (Recommended): Port the exact-list matching, with pathways unioned and the larger score kept.
- Decide with stage 5: Hold this item until stage 5 shows how parallel and conflicting edges affect the solve.

**Answer:** Keep (Recommended), written into S2.20

**Q3 (S2.21).** S2.21 · Bug: one association can survive twice. The duplicate test accepts an existing edge equal to the incoming one plus dummy_u_to_d, but not the reverse. Key order decides which direction gets the tag in each pathway, so one association can end up as two parallel edges in the same direction, one tagged and one not. The 42-pathway merge has 6 such pairs, a count Claude reproduced. The draft proposes comparing subtype lists with dummy_u_to_d removed, and computing the tag once, on the merged graph.

- Fix as proposed (Recommended): Compare without the tag and tag once on the merged graph, so each association survives exactly once in each direction.
- Keep FuSION 1 behaviour: Duplicates like these stay.

**Answer:** Fix as proposed (Recommended), written into S2.21

**Q4 (S2.22).** S2.22 · Cost of merging. Merging k pathways takes k − 1 steps, each one copying the whole graph so far, with linear scans inside: 3 s for 42 pathways, 53 s for 87 and about 124 s for 163. The merge is repeated for every run, because the exception set changes the result. The draft proposes building the same merged graph in one pass over all pathways, with hashed indexes from id to node and from edge key to edge.

- Fix as proposed (Recommended): One pass with hashed indexes, giving the same merged graph.
- Fix, and reuse merges: Also merge each pathway list once without the run's exceptions and split those ids out afterwards, so runs on the same pathways share one merge. The split must be shown to give the same graph.

**Answer:** Fix, and reuse merges, written into S2.22

## 040 · Poll · 2026-11-10 07:56

The seventh stage 2 item poll.

**Q1 (S2.23).** S2.23 · Dead work inside the live paths. Four computations whose results nothing uses: occurrence maps built for every graph, naming two output files that are never written; an always-empty MAPK_merged_ids search; display names written and then overwritten in duplicate_node; and an edge-id vector copied after every edge just to store its size. The last is quadratic: one read of the old master makes about 27 million integer copies.

- Drop (Recommended): None of the four in 2.0.
- Keep: Port them as they are.

**Answer:** Drop (Recommended), written into S2.23

**Q2 (S2.24).** S2.24 · The merged-edge report, edges_merged_filename. After merging two or more graphs, mff writes a tab-separated provenance report to that fixed name in the launch directory: each merged edge, then each per-pathway edge it came from, with pathway names. The real run's report has 3585 lines for 2529 edges. Nothing reads it except the GUI's mv, and a one-file mff writes none. The draft proposes keeping it as an optional report, written to a path given to the command whenever it's asked for.

- Fix as proposed (Recommended): An optional provenance report, written to the path the command is given.
- Always write it: Write it on every merge, into the run's output directory (S1.26), as FuSION 1 does but without the fixed launch-directory name.
- Drop: No merged-edge report in 2.0.

**Answer:** Fix as proposed (Recommended), written into S2.24

**Q3 (S2.25).** S2.25 · wgx <graph|0> <file> <tag> and the merged-graph XML. wgx writes nodes as entries (name = member ids, graphics = display names) and edges as relations with pathways and experiment_score. Nothing is XML-escaped (latent). Labels mix in other genes' aliases, entry names keep list order so keys can change on re-read, isolated nodes are written and then dropped on re-read, and the file claims to be KGML 0.7.1 when it isn't. The draft proposes keeping the format, escaping attribute values, and writing display names from the symbol map.

- Fix, and mark the format (Recommended): As drafted, plus a marker naming the file as FuSION's merged-graph format rather than KGML, which S2.12's reader needs in order to recognise it. The marker is my addition.
- Fix as drafted: Escape attribute values and take labels from the symbol map; the DOCTYPE stays as it is.
- Keep FuSION 1 behaviour: The file as written today.

**Answer:** Fix, and mark the format (Recommended), written into S2.25

**Q4 (S2.26).** S2.26 · The mff → wgx → rgx round trip. Every batch that merges (75 of them) writes the merged graph and reads it straight back, and later stages run on the re-read graph. The re-read is an undocumented normalisation: it drops isolated nodes, re-keys nodes, sorts pathway lists, rebuilds labels and re-applies the exception set. The real run writes and re-parses 540 KB this way. The draft proposes keeping the merged graph as an artefact but running later stages on the in-memory graph after the same normalisation. Whether results match is a Phase 3 check (open question 1).

- Fix as proposed (Recommended): Normalise in memory and keep writing the file as an artefact, which also serves as a saved step for partial runs (decision 0019).
- Keep FuSION 1 behaviour: Write the file and parse it back.

**Answer:** Fix as proposed (Recommended), written into S2.26

## 041 · User prompt · 2026-11-10 10:58

> continue

## 042 · Poll · 2026-11-10 11:00

Asked after the fresh stage 5 and 6 subagents both stopped on the plan's 5-hour limit (reset at 9:10), and after Claude checked which reference runs read the old master: 42 EGF-HeLa experiment batches, the `real_experiments` batches and the published b10 runs.

**Q1 (S2.27).** S2.27 · The pre-merged master that 65 batches read. 65 of the 78 batch files that call mff never merge KGMLs; they include the 42 EGF-HeLa experiment batches and the published b10 runs. They read runs/real_experiments/merged_kegg_master_graph_with_exp_score.xml, made by an external script, through a one-file mff … y. Read that way, its 598 complexes have no component edges, associations stay one-way, and 1796 ids sit in several entries, which causes the RAS trap. The draft proposes dropping it, so that 2.0 builds merged graphs only from raw KGMLs (open question 2).

- Drop, as drafted (Recommended): 2.0 builds merged graphs only from raw KGMLs. The 65 batches give different results, which decision 0023 allows, and Phase 3 compares against the raw-merge GSEA runs. The published runs stay reproducible with FuSION 1.
- Compatibility reader: 2.0 can also read this one file the way FuSION 1 does, kept apart from the raw pipeline and flagged as legacy, so the published runs can be re-run in 2.0 and compared like for like.

**Answer:** Compatibility reader, written into S2.27

**Q2 (S2.28).** S2.28 · sz <graph|0>. Prints the node count by type (genes, complexes, compounds, others), the number of isolated nodes, and the edge count, and under DEBUG_FLAG also writes the counts to the debug log. A missing or non-numeric argument aborts the run; under S1.28 that becomes a reported error.

- Keep (Recommended): The same two lines of counts.
- Keep, and count families: Also print how many nodes hold more than one gene, and the largest, since S2.03 and S2.17 show family nodes matter.

**Answer:** Keep, and count families, written into S2.28

**Q3 (S2.29).** S2.29 · upfx <prefix>. Renames the debug log to <prefix>_mylog.txt and prefixes two pathz3 outputs (limits_timefile.txt and PO.dat). No batch file or GUI uses it, it covers only three of the fixed-name outputs, and it crashes if used before start. S1.25's per-run log name and S1.26's per-run output directory now cover its purpose. The draft proposes dropping it.

- Drop (Recommended): No old batch uses it, so none breaks.
- Keep: Port it, with the crash fixed.

**Answer:** Drop (Recommended), written into S2.29

**Q4 (Stages 5-6).** The fresh stage 5 and 6 subagents both stopped almost at once on your plan's 5-hour limit, which reset at 9:10. They had barely started, and the reference snapshot hasn't changed. Resume them now with this message, logged in full?

- Resume both now (Recommended): Send the text shown to the stage 5 and stage 6 subagents. Stages 7 and 8 follow once both finish. (Its preview is the text of entry 043, wrapped over several lines.)
- Restart them fresh: New subagents with the same prompts (entry 006, entry 011 or 012, and entry 037).

**Answer:** Resume both now (Recommended)

## 043 · Subagent prompt · Resume message to the fresh stage 5 and 6 subagents · 2026-11-10 11:01

Sent word for word to the stage 5 and stage 6 subagents of [decision 0025](../decisions/0025-fresh-subagents-stages-5-to-8.md), as approved in entry 042, Q4.

```text
Your run stopped before you finished, because of your plan's usage limit. The reference snapshot has not changed, and your stage file has not been written yet. Carry on from where you stopped and finish the task exactly as your prompt describes: write only your own stage file, then reply in at most 300 words.
```

## 044 · Poll · 2026-11-10 12:38

The last stage 2 item poll, with the stage 2 commit.

**Q1 (S2.30).** S2.30 · add <edges_file> <out_xml>, a plaintext edge list to KGML-style XML. It reads lines of the form src tgt type subtype, numbers ids by first appearance, and writes entries plus relations with pathways="KEGG" so that mff … y can merge them; the GUI's script uses a standalone copy through exec. It warns on every type other than PPRel, but KEGG spells it PPrel, so correct lines trigger the warning and a PPRel line is written as a type nothing else recognises. Each edge takes one subtype only. The draft proposes keeping the command and its format, checking types against KEGG's spelling and rejecting malformed lines with a message.

- Fix as proposed (Recommended): Same command and output format, KEGG's spellings, and malformed lines rejected. A plain edge list like this is also a candidate exchange format for decision 0020's plug-and-play inputs, in Phase 2.
- Fix, several subtypes: The same, plus comma-separated subtypes on one line (activation,phosphorylation), each written as its own subtype.
- Keep FuSION 1 behaviour: The PPRel check and one subtype per edge, as now.

**Answer:** Fix as proposed (Recommended), written into S2.30. All 30 stage 2 items are now decided.

**Q2 (Commit).** Once S2.30 is decided, stage 2 is complete. May I commit it locally as one commit, 'Review spec stage 2: graph input and merging'? It would hold the stage 2 spec; decisions 0023 to 0025 with their index rows and the status notes on 0012, 0016 and 0017; the new docs/spec/README.md; and prompt log entries 032 to 043. The stage 3 and 4 drafts stay out until they're reviewed.

- Commit (Recommended): One local commit ending with a Co-Authored-By: Claude line. This poll's own log entry goes into the next commit.
- Show me the diff first: I list every change, then ask again.

**Answer:** Commit (Recommended). This entry is left for the next commit.

## 045 · User prompt · 2026-11-11 00:52

Sent while Claude was verifying the stage 3 draft.

> max out the number of agents. i have max sub now

## 046 · User prompt · 2026-11-11 00:52

Sent right after entry 045. Claude found the stage 5 and 6 subagents still running (started 12 hours earlier) and read this as repeating entry 045 → [decision 0026](../decisions/0026-maximum-parallel-subagents.md).

> Try again

## 047 · Subagent prompt · Fresh stage 7 and 8 drafts · 2026-11-11 00:55

Launched together under [decision 0026](../decisions/0026-maximum-parallel-subagents.md), alongside the running stage 5 and 6 drafts. As [decision 0025](../decisions/0025-fresh-subagents-stages-5-to-8.md) sets out, each prompt is entry 006, a blank line, the stage section (entry 013 for stage 7, entry 014 for stage 8), a blank line, and the note in entry 037, all word for word.

## 048 · Subagent prompt · Verification of a drafted stage · 2026-11-11 00:55

The verification prompt of [decision 0026](../decisions/0026-maximum-parallel-subagents.md). It is sent word for word, with `<STAGE FILE>` replaced by the stage's spec file; each send is logged by entry number and stage. First sent for stage 4 (`docs/spec/04-reachability-corridor.md`).

```text
You are checking a draft specification against the code and data it describes. The draft is /home/ks-149-u24/Desktop/arjun/FuSION2/<STAGE FILE>, one stage of a specification of FuSION 1 written by another agent. FuSION 1's source code is frozen at /home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10, with the same relative paths as the FuSION 1 repo; its README.txt says what it holds. Run data are under /home/ks-149-u24/Desktop/arjun/FuSION/runs/. In this shell, grep is a wrapper that silently skips gitignored directories such as runs/, so use /usr/bin/grep.

Some of the draft's line numbers may refer to FuSION 1's live tree as it was before 2026-11-09 19:16. At that time Linux/src/main.cc gained one line at 4733 (macOS/src/main.cc at 4761), and Linux/src/GraphManagerNew.cc gained one line at 8180, ten more from 8216, and a helper at 29342. So an old number may be off by 1, 11 or more against the snapshot.

Do this:
1. For every citation of FuSION 1 code in the draft, whether path:line or a bare line range that inherits its file from the surrounding text, open the snapshot at that location and confirm it holds what the draft says. Where it doesn't, find the right lines.
2. For every count, size or measured value the draft reports, recompute it from the files it names. Write any helper scripts under /tmp/claude-1000/-home-ks-149-u24-Desktop-arjun-FuSION/542fdce7-1f24-4eb5-9dd5-2d360919cc30/scratchpad/. Where a value could only come from running FuSION, or from a rebuild that the draft describes but that does not exist, say so instead.
3. Check each item's account of behaviour (inputs, outputs, defaults, edge cases, formats) against the code.

Do not edit the draft or any other file in either repo, and do not build or run FuSION. Reply in at most 500 words: every discrepancy you found (item id, what the draft says, what is true, with file:line), the claims you could not check and why, and a one-line verdict.
```

## 049 · Poll · 2026-11-11 01:58

The first stage 3 item poll (`docs/spec/03-expression-data.md`). Before polling, Claude checked 41 of the draft's code citations against the snapshot and recomputed its data claims from the LFC files; all matched.

**Q1 (S3.01).** S3.01 · cudf <lfc_file> <up_threshold> <down_threshold> <up_out> <down_out>, with the long name create_up_down_file. It never touches the graph, so it runs before start in every batch. The argument count is never checked: extra tokens are ignored, a missing threshold aborts the whole run, and a missing output path fails only after the other output has been truncated. The draft proposes keeping the five positional arguments and both names, and reporting a wrong argument count before any file is touched.

- Fix as proposed (Recommended): Same five arguments (also nameable, decision 0018) and both names, with a wrong count reported as an error before anything is opened.
- Keep FuSION 1 behaviour: No argument-count check.

**Answer:** Fix as proposed (Recommended), written into S3.01

**Q2 (S3.02).** S3.02 · Threshold parsing. Each threshold goes through std::stof, which reads the longest numeric prefix and ignores the rest: 1.01x reads as 1.01, and a typo like 1.0l as 1.0. It accepts nan and inf, which silently empty a list or fill it with every gene. A token with no numeric prefix, or the empty string of a missing argument, throws and aborts the whole run with status 134. The draft proposes accepting a threshold only when the whole token is a finite number, and otherwise failing with an error that names the argument.

- Fix as proposed (Recommended): The whole token must be a finite number; anything else is a named error, and the run stops (S1.28).
- Keep FuSION 1 behaviour: Prefix parsing; nan and inf accepted.

**Answer:** Fix as proposed (Recommended), written into S3.02

**Q3 (S3.03).** S3.03 · The LFC input file. Plain text, one gene per line: the first whitespace-separated token is the id, the second the value, and anything after is ignored. LF and CRLF both work. Values are used as given, with no log transform or scale check, and there's no header or comment convention. The id is copied verbatim and must match the network's ids exactly, which ties the file to KEGG (decision 0020): hsa:3725, JUN or the bare Entrez id 3725 pass through cudf and then match nothing. The draft proposes keeping the format, and leaving id mapping to Phase 2.

- Keep (Recommended): Port the two-column format. Mapping ids from other sources onto network ids is decision 0020's Phase 2 question.
- Keep, strip colons: The same, and also remove colons from ids as the KGML reader does, so hsa:3725 matches now. Symbols and Entrez ids still wait for Phase 2.

**Answer:** Keep (Recommended), written into S3.03

**Q4 (S3.04).** S3.04 · Bug: a value that isn't a number counts as 0. When the second token has no numeric prefix, the read stores 0 without a message, so a header line, NA, nan, inf or - all read as an LFC of 0. With the real thresholds a 0 lands in no list, so nothing shows; with a threshold that admits 0, a header line becomes a gene, and with both thresholds at 0 (the GUI script) every NA gene lands in both lists. The draft proposes skipping, and reporting with its line number, any line whose value isn't a finite number.

- Fix as proposed (Recommended): Such lines are skipped and reported by line number. Well-formed files give the same lists as now.
- Keep FuSION 1 behaviour: Unreadable values count as 0.

**Answer:** Fix as proposed (Recommended), written into S3.04

## 050 · Subagent prompt · Verification sent for stage 6 · 2026-11-11 01:59

Entry 048's text, word for word, with `<STAGE FILE>` replaced by `docs/spec/06-results-and-plots.md`, sent once the stage 6 draft was written ([decision 0026](../decisions/0026-maximum-parallel-subagents.md)).

## 051 · Poll · 2026-11-11 02:01

The second stage 3 item poll.

**Q1 (S3.05).** S3.05 · Bug: a line without a value reuses the previous line's gene or value. The id and value variables live outside the read loop, so a blank line re-classifies the previous gene (and can write it twice), and a line with an id but no value takes the previous line's value. A blank first line reads an uninitialised value. No real LFC file has such a line. The draft proposes skipping and reporting any line that lacks an id or a value, and never carrying a value over.

- Fix, blank lines silent (Recommended): A line with an id but no value is skipped and reported by line number. Blank lines are skipped without a message, and no value is ever carried over. The silent blank lines are my refinement.
- Fix as drafted: Every skipped line is reported, blank ones included.
- Keep FuSION 1 behaviour: Values carry over from the previous line.

**Answer:** Fix, blank lines silent (Recommended), written into S3.05

**Q2 (S3.06).** S3.06 · Classifying a gene. A gene is up when value >= up_threshold, and down when value <= down_threshold; the two tests are independent and both inclusive, and only the id is written. FuSION 1 compares 32-bit floats. With doubles on both sides, every real file (at most two decimals) gives the same lists, but a value within float rounding of a threshold could differ: 1.0099999999 is up at 1.01 in FuSION 1, but not with doubles.

- Keep, in doubles (Recommended): The same inclusive, independent rule, with values and thresholds both as doubles. Results on real files are identical, and decision 0023 allows the rounding-edge difference.
- Keep, in floats: The same rule with 32-bit floats, matching FuSION 1 bit for bit.

**Answer:** Keep, in doubles (Recommended), written into S3.06

**Q3 (S3.07).** S3.07 · No check that the up threshold is above the down threshold. When up_threshold <= down_threshold, every value between them goes into both lists without a warning. The GUI-written script sets both to 0.0, which on the EGF file puts 1630 genes up, 1569 down and 145 in both; pathz3 then quietly treats those 145 as up. The draft proposes rejecting up_threshold <= down_threshold with an error before anything is written.

- Fix as proposed (Recommended): Reject up_threshold <= down_threshold before writing anything. A FuSION 1 GUI script with both thresholds at 0.0 then stops with this error.
- Warn only: Report the overlap as a warning and carry on, as before.
- Keep FuSION 1 behaviour: No check.

**Answer:** Fix as proposed (Recommended), written into S3.07

**Q4 (S3.08).** S3.08 · Repeated gene ids. cudf classifies each line on its own, so a repeated id can be written to a list several times or, if its values disagree, to both lists. The TNFa file lists hsa5806 three times and hsa3280 twice, and its up list holds hsa5806 three times. gsea and pex keep the last value instead. The draft proposes reporting repeats, writing each gene at most once per list, and stopping with an error when an id's lines put it in different classes. Whatever is chosen also binds gsea and pex (S3.14).

- Fix as proposed (Recommended): Repeats are reported, each gene is written at most once per list, and lines that put one gene in different classes are an error.
- Fix, last value wins: Repeats are reported, and each gene's last value is used, as gsea and pex already do. No error.
- Keep FuSION 1 behaviour: Line by line, repeats and all.

**Answer:** Fix as proposed (Recommended), written into S3.08

## 052 · Subagent prompt · Verification sent for stage 5 · 2026-11-11 02:02

Entry 048's text, word for word, with `<STAGE FILE>` replaced by `docs/spec/05-encoding-and-solve.md`, sent once the stage 5 draft was written ([decision 0026](../decisions/0026-maximum-parallel-subagents.md)).

## 053 · Poll · 2026-11-11 17:46

The third stage 3 item poll (asked at 02:02, answered by 17:46).

**Q1 (S3.09).** S3.09 · The up and down list format. Two plain-text files, one gene id per line exactly as read, with no header or values. Entries follow input order (S3.08 now removes repeats), and an empty list is an empty file. pathz3's other node lists use the same format and reader. Under decision 0019 the lists are a saveable step that a later run can start from.

- Keep (Recommended): Port the format, written on every cudf as now.
- Keep, files optional: The lists stay in memory and are written only when asked, since the exception set no longer needs them on disk (S1.17). Saving still works for partial runs.

**Answer:** Keep (Recommended), written into S3.09

**Q2 (S3.10).** S3.10 · Outputs are truncated before the input is checked. All three files are opened at once, so both outputs are emptied before any check. A wrong LFC path therefore empties the previous run's lists, and the batch carries on with status 0; pathz3 then computes an undefined bit width from ceil(log2(0)). Clashing paths destroy data silently: an up path equal to the LFC path truncates the input before it is read. The draft proposes checking the LFC file and both output paths before truncating anything, stopping the run on failure (decision 0021), and refusing output paths that coincide with each other or with the input.

- Fix as proposed (Recommended): Check every path first, stop on failure, and refuse clashing paths.
- Keep FuSION 1 behaviour: Open all three at once, as now.

**Answer:** Fix as proposed (Recommended), written into S3.10

**Q3 (S3.11).** S3.11 · Console output. On success cudf prints nothing beyond the command echo, so the effect of the thresholds stays invisible until pathz3 prints Up: N Down: M after graph matching: 3 and 0 in the real run, against the 43 and 2 that cudf wrote. The draft proposes printing how many genes went to each list and, together with S3.04, S3.05 and S3.08, how many lines were skipped or repeated.

- Fix as proposed (Recommended): Print the counts per list and the skipped and repeated lines.
- Keep FuSION 1 behaviour: Silent on success.

**Answer:** Fix as proposed (Recommended), written into S3.11

**Q4 (S3.12).** S3.12 · How a list entry becomes a graph node. cudf writes every gene that passes a threshold, and pathz3 later looks each one up exactly in the graph being solved, the corridor. Absent ids are skipped silently. In the real run cudf wrote 43 up and 2 down genes; 30 and 1 are in the merged graph, and only 3 and 0 reach the solve. Nothing lists the other 42. The draft proposes keeping the exact lookup and reporting how many entries of each list match no node of the graph being solved, and which ones.

- Fix, report two levels (Recommended): Keep the exact lookup, and list unmatched genes in two groups: not in the merged network at all, and in the network but outside the corridor. The split is my refinement; stage 4's S4.16 reports the corridor side too.
- Fix as drafted: One list of genes that match no node of the graph being solved.
- Keep FuSION 1 behaviour: Unmatched genes vanish silently.

**Answer:** Fix, report two levels (Recommended), written into S3.12

## 054 · Poll · 2026-11-11 19:39

The fourth stage 3 item poll.

**Q1 (S3.13).** S3.13 · The other readers of the lists. pathz3 first reads both lists into a map that nothing uses afterwards (two parses and one lookup per entry, discarded), then reads them again for real (S3.12). Every batch also builds the merge-exception file from the lists, which S1.17 has already moved into memory. The draft proposes keeping the list format that both readers rely on, and leaving the unused read to stage 5.

- Keep (Recommended): Port the list format. Stage 5's review decides the unused read.
- Keep, drop the read now: The same, and record here that pathz3's unused first read goes, for stage 5 to confirm.

**Answer:** Keep (Recommended), written into S3.13

**Q2 (S3.14).** S3.14 · Readers of the LFC file disagree. The real run parses its LFC file three times: in cudf and in the Python behind gsea and pex. The three treat headers, blank lines, NA, nan, inf, 1,5, 1_000, 1.5abc and repeated ids differently, so one gene can get three different values in one run. No real run is affected yet. pathz3 receives the path too, but uses only its base name, to name the SMT dump directory. The draft proposes one set of parsing rules (S3.03 to S3.05 and S3.08) for every consumer.

- Fix as proposed (Recommended): One LFC parser, used by every command that reads the file.
- Keep FuSION 1 behaviour: Each reader keeps its own parser.

**Answer:** Fix as proposed (Recommended), written into S3.14

**Q3 (S3.15).** S3.15 · suggest_thresholds.py: invocation and report. python3 suggest_thresholds.py LFC_FILE [--current UP DOWN] [--plot OUT.png] prints the distribution (min, max, mean, SD, median, MAD), a table of candidate thresholds, and three explanatory lines. Nothing in FuSION calls it, and the report has no machine-readable form. The draft proposes keeping the advisory report as it is; Phase 2 decides whether it becomes a FuSION command or stays a separate tool.

- Keep (Recommended): Port the report. Its form, command or separate tool, is a Phase 2 decision, alongside the all-C++ backend.
- Keep, as a FuSION command: Decide now that it becomes a backend command, with a machine-readable result the frontend can show.

**Answer:** Keep (Recommended), written into S3.15

**Q4 (S3.16).** S3.16 · suggest_thresholds.py reads the LFC file differently from cudf. It skips headers and NA, blank and one-field lines, which cudf reads as 0 or as stale values; it reads 1_000 as 1000, where cudf reads 1; and it accepts nan and inf, where a single NaN breaks the sort and turns the mean and SD into NaN. Its docstring claims 'Counting mirrors cudf exactly'. The draft proposes reading the file with the same rules as cudf (S3.14).

- Fix as proposed (Recommended): The same parser as cudf and every other reader, so its counts are the ones cudf writes.
- Keep FuSION 1 behaviour: Its own parser.

**Answer:** Fix as proposed (Recommended), written into S3.16

## 055 · Poll · 2026-11-11 19:45

The last stage 3 item poll, with a factual question from the draft's open questions.

**Q1 (S3.17).** S3.17 · suggest_thresholds.py's candidate cuts are quantiles (1%, 2.5%, 5%), median ± k·MAD and mean ± k·SD (k = 2, 3). Your batches cut at ±3·SD about 0 instead: the real batch derives ±1.01 from SD 0.336, a row the script never prints. Its mean-centred SD k=3 row, +1.046 / −0.972, gives 39 up genes rather than 43, and it drops FOSB (1.04), the FSC of one of your b10 runs. Which centre should an SD-based threshold use (open question 1)?

- Show both centres (Recommended): Keep the seven criteria and add ±k·SD about 0 (k = 2, 3), the rule your batches use, so the table includes the cut in use. This addition is mine.
- Zero-centred only: Replace the mean-centred SD rows with ±k·SD about 0.
- Keep as drafted: The seven criteria as they are.

**Answer:** Show both centres (Recommended), written into S3.17

**Q2 (S3.18).** S3.18 · suggest_thresholds.py counts genes in the file, not genes the solve sees. #up and #down count rows of the LFC file, because the script has no graph. For the real run's ±1.01 it shows 43 and 2, while 30 and 1 are in the merged graph and only 3 and 0 reach pathz3. The draft proposes keeping it this way: counting what the solve sees needs the corridor, which exists only after fb_rch, and S3.11 and S3.12 now report those counts where they're known.

- Keep (Recommended): Count rows of the file; the matched counts come from S3.11 and S3.12.
- Fix, optional graph: Optionally take a merged graph and also count only the genes in it. The corridor still comes later.

**Answer:** Keep (Recommended), written into S3.18

**Q3 (S3.19).** S3.19 · suggest_thresholds.py --plot. With gnuplot on PATH it draws a log-scale histogram of 80 bins as a 980×620 PNG, with candidate cuts as dotted lines and the cuts in use as solid blue and red. It exits 0 even when the plot wasn't written, and a file whose values are all equal makes gnuplot fail and leaves a 0-byte PNG. The draft proposes keeping the plot and exiting non-zero when a requested plot isn't written.

- Fix as proposed (Recommended): Keep the plot, and report a failure when it isn't written. Where 2.0 draws plots, gnuplot or the frontend, is a Phase 2 question.
- Keep FuSION 1 behaviour: Exit 0 regardless.

**Answer:** Fix as proposed (Recommended), written into S3.19

**Q4 (TNFa scale).** A fact only you can settle (stage 3, open question 2): is runs/real_experiments/final_tnfa_p38i_combined.tsv on a log scale? Its values reach +94.57 (CXCL8), its 19 batches cut at ±1.45, and nothing in the repo records how it was made; it comes from auxilliary/IITB_ACTREC_old_data/. The answer decides whether 2.0 should check the scale of per-gene values (decision 0020).

- Raw fold changes: Not log-transformed, so 2.0 should check the scale of per-gene values.
- Log fold changes: Already log-scaled, so +94.57 is a genuine outlier.
- Not sure: Record it as unknown. Phase 2 decides whether to add a scale check anyway.

**Answer:** Log fold changes, written into stage 3's open questions. All 19 stage 3 items are now decided.

## 056 · Poll · 2026-11-11 19:47

**Q1 (Commit).** Stage 3 is complete. May I commit it locally as one commit, 'Review spec stage 3: expression data'? It would hold the stage 3 spec, decision 0026 with its index row, and prompt log entries 044 to 055. The stage 4 to 8 drafts stay out until they're reviewed, and this poll's own log entry goes into the next commit.

- Commit (Recommended): One local commit ending with a Co-Authored-By: Claude line.
- Show me the diff first: I list every change, then ask again.

**Answer:** Commit (Recommended). This entry is left for the next commit.

## 057 · Poll · 2026-11-11 20:02

The first stage 4 item poll (`docs/spec/04-reachability-corridor.md`). Before polling, the stage 4 verification subagent (entry 048's prompt) checked the draft, and Claude spot-checked its report and applied it. The draft cited 83 `GraphManagerNew.cc` lines in the numbering from before 2026-11-09 19:16; these were moved to the snapshot, and the script confirmed the moved ranges start on the functions they name. Six claims were corrected: the header lines in S4.13, the member-list reader in S4.12, the observations seen by the two solves in S4.06 and S4.16, b = 0 in S4.18, the pathway origin of the signed bindings in S4.04, and the claim-order count in S4.15 (57, not 58). Four minor slips were also corrected: in S4.07, S4.05, S4.20 and the scope. The stage 6 draft had already been corrected the same way from its own verification report.

**Q1 (S4.01).** S4.01 · fb_rch <graph-id> <sources…> -1 <targets…> -1 <excluded…> -1 <bound> <edges_to_tgt>. Each list is read until a token that is exactly -1, into a set, and 0 means the latest graph. All 170 batch lines have one source, one target and no exclusions. A missing -1 walks the million-slot token vector, taking 10, 0 and empty strings as ids; a missing number aborts the run, and extra tokens are ignored. The draft proposes keeping the positional form for old batches, adding names (decision 0018), and rejecting a list without its terminator, a missing argument or extra tokens.

- Fix as proposed (Recommended): Positional form still accepted, names added, and malformed lines rejected with a message.
- Keep FuSION 1 behaviour: No checks.

**Answer:** Fix as proposed (Recommended), written into S4.01

**Q2 (S4.02).** S4.02 · Resolving the listed ids. Each source, target and excluded id is looked up in the input graph; the first claim wins, and a member id resolves to its whole family node (S2.03). An unknown id only gets a warning, and if no source or target resolves, the call later fails with a message blaming the edge-type flag. A node listed twice is seeded twice. The draft proposes making an unresolvable source or target a command error (decision 0021), resolving each id once, and reporting which node each id bound to.

- Fix as proposed (Recommended): An unknown source or target is an error, each id is resolved once, and the binding is reported.
- Keep FuSION 1 behaviour: Warnings only.

**Answer:** Fix as proposed (Recommended), written into S4.02

**Q3 (S4.03).** S4.03 · The bound and path length. Length is the number of edges, so the bound b admits paths of up to b edges, inclusive. Edges have no weights, and a complex is reached by an edge into the complex itself. Indirect relations aren't edges at all (S2.11, whose '1+' option would make each count as one hop). b < 0 is an error. b = 0 is accepted but can never keep an edge between a distinct source and target (S4.18).

- Keep (Recommended): Edge-count length, b ≥ 0, inclusive.
- Keep, but reject b = 0: The same, but refuse b = 0 up front, since it can't connect a distinct source and target.

**Answer:** Keep (Recommended), written into S4.03

**Q4 (S4.04).** S4.04 · Which edges can be traversed. Every build fixes this at compile time: an edge is never followed if any of its subtypes is component, association, dissociation or mapping. So a relation KEGG marks as both binding and signed (e.g. activation + association) is dropped with the binding: 21 edges in the reference graph and 40 in the full graph. Only 8 and 4 of those are chemokine ligand → receptor edges (hsa04062); the rest come from other pathways. Following them naively would also follow their reverse copies (S2.10). The draft proposes making the non-traversable set a run setting, recorded in the output, defaulting to today's four. Whether signed bindings should be traversable is your biological call (open question 4).

- Fix as proposed (Recommended): A run setting with today's four subtypes as the default, recorded in the output (S1.29).
- Fix, traverse signed bindings: The same setting, but a binding that also carries activation, inhibition or phosphorylation is followed in the direction KGML records, not its reverse copy. Corridors change, which decision 0023 allows.
- Keep FuSION 1 behaviour: A compile-time rule, as now.

**Answer:** Fix as proposed (Recommended), written into S4.04

## 058 · Poll · 2026-11-11 20:10

The second stage 4 item poll.

**Q1 (S4.05).** S4.05 · The edges_to_tgt flag acts only on edges into a target. 1 drops expression or repression edges, 2 drops activation, inhibition or phosphorylation edges, and anything else, 0 included, filters nothing. The batch names ($ONLY_ACT_EDGES_TO_TARGET = 1, $ONLY_EXPR_EDGES_TO_TARGET = 2) and the GUI labels promise more than this: flag 2 also keeps subtype-less, dephosphorylation and other edges, and a typo like 3 silently means no filter. 75 batches pass 0, 11 pass 2 and none passes 1. The draft proposes accepting only 0, 1, 2 or their names, and making each rule match its name, which gives the same corridors on every graph measured.

- Fix as proposed (Recommended): Only 0, 1 and 2, or their names. 'Expression only' keeps just expression/repression edges into a target, and 'activation only' just activation/inhibition/phosphorylation.
- Keep FuSION 1 behaviour: The drop rules as they are, and any other value means no filter.

**Answer:** Fix as proposed (Recommended), written into S4.05

**Q2 (S4.06).** S4.06 · Excluded nodes are never seeded or entered, but one can still slip into the corridor as an isolated member of a copied complex (S4.12). No batch or GUI script excludes anything. The batches avoid the FSC in pathz3 instead, so both solves run on the same corridor, though not on the same observations: avoiding an up-regulated FSC such as JUN also drops it from the up set (Up: 3, then Up: 2). The draft proposes keeping the exclusion list and never letting an excluded node into the corridor as a complex member.

- Fix as proposed (Recommended): Keep exclusions, and keep excluded nodes out of the corridor entirely.
- Keep FuSION 1 behaviour: Excluded nodes can re-enter as complex members.

**Answer:** Fix as proposed (Recommended), written into S4.06

**Q3 (S4.07).** S4.07 · Forward search. Breadth-first search from the sources over traversable edges, giving ds(v), the exact shortest distance from the nearest source, for every node within the bound: 354 nodes in the reference run, 1186 in the full run. The avoidable costs are implementation details, such as about 0.92 million queue comparisons in the full run and copied vectors. The draft proposes porting the search and its distances.

- Keep (Recommended): Port the search and its distances, without the avoidable costs.
- Keep, export distances: The same, and also write each corridor node's distances to the nearest source and target, for inspection and for the graph display (decision 0022).

**Answer:** Keep, export distances, written into S4.07

**Q4 (S4.08).** S4.08 · The forward graph: an intermediate copy. Every edge with both ends in the forward set is copied into a new registered graph, complex members included, and the backward search runs over that copy. Nothing uses it afterwards, it's never freed, and in the full run it's 2.1 times the corridor's nodes. The draft shows the copy isn't needed: running the backward search on the forward set inside the input graph gives the same corridor, which the verifier confirmed on four cases. The draft proposes dropping it.

- Drop (Recommended): Search backward within the input graph. The corridor is unchanged, and its graph id becomes one lower; no batch hard-codes graph ids.
- Keep: Build the intermediate forward graph, as now.

**Answer:** Drop (Recommended), written into S4.08

## 059 · Subagent prompt · Verification sent for stage 7 · 2026-11-11 20:11

Entry 048's text, word for word, with `<STAGE FILE>` replaced by `docs/spec/07-gsea-shortlisting.md`, sent once the stage 7 draft was written ([decision 0026](../decisions/0026-maximum-parallel-subagents.md)).

## 060 · Poll · 2026-11-11 20:16

The third stage 4 item poll.

**Q1 (S4.09).** S4.09 · Backward search and the keep rule. A breadth-first search back from the targets over the forward set gives dt(v), the distance to the nearest target. A node is kept if ds(v) + dt(v) ≤ b, and an edge u → v if both ends are kept and ds(u) + 1 + dt(v) ≤ b. Those are exactly the nodes and edges on some walk of at most b edges from a source to a target; the reference corridor has 115 nodes and 370 edges. The draft found no problem in the rule itself; walks versus simple paths is S4.11.

- Keep (Recommended): Port the keep rule.
- Hold for stage 5: Decide after stage 5's review shows how the solve uses the corridor.

**Answer:** Keep (Recommended), written into S4.09

**Q2 (S4.10).** S4.10 · Several sources or targets. The corridor is the union over every (source, target) pair, cross pairs included, and a node that is both a source and a target keeps its cycles of up to b edges. pathz3 solves only the pairs in its connect-pairs file, so the corridor can hold paths for pairs no solve asks about. No batch uses more than one source or one target.

- Keep (Recommended): The union, which is never too small for any pair.
- Fix: solved pairs only: Build the corridor only for the pairs pathz3 will solve.

**Answer:** Fix: solved pairs only, written into S4.10

**Q3 (S4.11).** S4.11 · Walks, not simple paths. The keep rule admits walks, so the corridor keeps edges that no bounded simple path uses: 31 of 370 in the reference run and 54 of 2138 in the full run. They are self-loops, edges into the source, edges out of the target, and edges whose only bounded walks repeat a node. Stage 5's draft, not yet reviewed, finds that such off-path edges do change results, because pathz3 counts an edge as relaxed whenever both its ends are present (S5.13). The draft proposes keeping the walk rule, since exact simple-path pruning is exponential, and trimming the cheap cases if stage 5 confirms they matter.

- Decide with stage 5 (Recommended): Hold until S5.13 is settled. If off-path edges keep counting as relaxed, trim self-loops and edges into a source or out of a target; if not, keep the walk rule as it is.
- Keep the walk rule: Port it unchanged, now.
- Trim the cheap cases now: Drop self-loops, edges into a source and edges out of a target from the corridor, whatever stage 5 decides.

**Answer:** Decide with stage 5 (Recommended), written into S4.11 as deferred

**Q4 (S4.12).** S4.12 · Complex members come along with their complex. Copying a group node copies all its members, whether or not they lie on a bounded walk, so members with no corridor edges appear: 3 of the 115 nodes in the reference corridor (DOCK2, ELMO1/ELMO2, TIRAP) and 41 of 592 in the full one. They are the isolated nodes sz reports. They enter the id lookup and the solver, and an excluded node can re-enter this way. Stage 5's draft says isolated expressed members count only in Up: and the relaxation-bound clamp, not in the objective, which stage 5's review will confirm. The draft proposes keeping each complex's member list, but adding a member as a corridor node only if it's kept in its own right.

- Fix as proposed (Recommended): Members stay listed on their complex, but only members kept in their own right become corridor nodes.
- Keep FuSION 1 behaviour: Every member comes along, as now.

**Answer:** Fix as proposed (Recommended), written into S4.12

## 061 · Subagent prompt · Verification sent for stage 8 · 2026-11-11 20:17

Entry 048's text, word for word, with `<STAGE FILE>` replaced by `docs/spec/08-frontend-workflow.md`, sent once the stage 8 draft was written ([decision 0026](../decisions/0026-maximum-parallel-subagents.md)).

## 062 · Poll · 2026-11-11 20:21

The fourth stage 4 item poll.

**Q1 (S4.13).** S4.13 · Copying attributes and merging duplicate edges. Copied nodes keep their type, key, member ids (registered first-claim-wins), labels and description. A copied edge merges with an existing one when source, target, type and ordered subtype list match, or match plus dummy_u_to_d; pathways are unioned and the larger score kept. In the full run 4126 selected edges become 3535, because the old master's 768 duplicate relations first merge here. After stage 2's fixes raw reads have no duplicates, but S2.27's compatibility reader keeps them. The draft proposes porting the copy and the merge rule as described.

- Keep (Recommended): Port the attribute copy and the merge rule.
- Keep, and report merges: The same, and print how many duplicate edges the corridor merged, since sz counts before and after fb_rch aren't comparable.

**Answer:** Keep (Recommended), written into S4.13

**Q2 (S4.14).** S4.14 · Node and edge ids change with every copy. Ids come from session-wide counters, so every copy renumbers everything: EGFR is node 3273 in the full run's master graph, 7596 in the phase-1 corridor and 10044 in the phase-2 corridor. Outputs mix these ids, so comparing the two solves has to go through gene keys. The draft proposes naming nodes and edges in outputs by stable keys (the rep id for a node; source, target and subtypes for an edge), with any internal id that does appear meaning the same thing in every step of a run.

- Fix as proposed (Recommended): Stable keys in every output, and internal ids, where shown, stable across a run.
- Keep FuSION 1 behaviour: Fresh ids per copy, as now.

**Answer:** Fix as proposed (Recommended), written into S4.14

**Q3 (S4.15).** S4.15 · The corridor rebuilds the id lookup. pathz3 resolves its pairs and every node file (up, down, essential, avoid, …) against the corridor's own lookup, rebuilt first-claim-wins from the copied nodes, not against the input graph's. In the full run that moves 57 of 1399 ids to a different node. For example, ATF2's own node is outside the corridor, so hsa1386 resolves to the 10-gene CREB3-family node, and naming ATF2 as the FSC would force the whole family. The draft proposes resolving every id once per run, against one lookup, and carrying that binding into the corridor.

- Fix as proposed (Recommended): One lookup per run, and the corridor inherits its bindings.
- Keep FuSION 1 behaviour: The corridor rebuilds its own lookup.

**Answer:** Fix as proposed (Recommended), written into S4.15

**Q4 (S4.16).** S4.16 · The corridor decides which expression changes pathz3 sees. Up- or down-regulated genes off the corridor are dropped, and pathz3 lowers its node-relaxation bound to the number left: of the reference run's 45 changed genes, 3 remain, so the requested bound of 8 becomes 3. Nothing lists the genes left out. S3.12 already decided to report them, as its 'outside the corridor' group. The draft proposes keeping the rule and writing those genes out together with the corridor's parameters.

- Fix as proposed (Recommended): Keep the rule, and write out the out-of-corridor genes with the corridor's parameters, as S3.12's second group.
- Keep FuSION 1 behaviour: They vanish silently.

**Answer:** Fix as proposed (Recommended), written into S4.16

## 063 · Poll · 2026-11-11 20:27

The fifth stage 4 item poll.

**Q1 (S4.17).** S4.17 · Console output. A successful call prints Bound: 10, the 'N out of M edges to target excluded' line, fwd nodes 354, eids 2529, edges 895, the forward graph's node and edge counts, the corridor's, and Reachability Graph: 86. Three different node counts appear, with no label saying what each counts, and M counts edges into a target examined from any node within the bound, twice for a source listed twice. The draft proposes printing the same counts, each labelled with what it counts. The forward-graph line goes with S4.08.

- Fix as proposed (Recommended): The same counts, each labelled.
- Keep FuSION 1 behaviour: Unlabelled counts, as now.

**Answer:** Fix as proposed (Recommended), written into S4.17

**Q2 (S4.18).** S4.18 · Failures, and what graph 0 names afterwards. A failed call prints 'Error: reachability didn't work' and the batch carries on. With no such graph, or b < 0, nothing is created. If the flag dropped every edge into the target, which also happens when no source or target resolved, the message blames the edge types and graph 0 is an empty graph. If no target is reachable it prints 'Exiting.' without exiting, and graph 0 is empty. The next pathz3 0 then runs on the input graph or on an empty one. The draft proposes that a failure leaves no graph and stops the run (S1.28), with a message naming the real cause, and that an empty corridor counts as a failure.

- Fix, empty = result (Recommended): Real failures stop the run, with a message naming the cause. An empty corridor ('no path within b') is reported as a result, not a failure, and its solve is skipped, so one small bound doesn't halt a bound sweep (decision 0018). The empty-corridor part is my refinement.
- Fix as drafted: An empty corridor also counts as a failure and stops the run.
- Keep FuSION 1 behaviour: Carry on after failures, as now.

**Answer:** Fix, empty = result (Recommended), written into S4.18

**Q3 (S4.19).** S4.19 · Trace files fb_rch and fb_rch_back. Two fixed-name debug traces are written to the launch directory on every call, one entry per node each search dequeued, mixing rep ids with input-graph node ids. A two-call batch loses the first call's files, a failed call can leave a stale fb_rch_back, and nothing reads either. The draft proposes writing them only under the debug option (S1.29), into the run's output directory (S1.26), naming nodes by rep id.

- Fix as proposed (Recommended): Debug-only traces in the run's output directory, naming nodes by rep id.
- Drop them: No trace files at all; S4.07's exported distances cover inspection.
- Keep FuSION 1 behaviour: Always written, in the launch directory.

**Answer:** Fix as proposed (Recommended), written into S4.19

**Q4 (S4.20).** S4.20 · The corridor is computed twice per batch. 84 batch files and the GUI call fb_rch again before phase 2, because pathz3's undirected copy moves graph 0, and the fresh corridor also gives the second solve a graph pathz3 hasn't written to. In every log both corridors match. That costs a second forward graph and corridor (1264 + 592 nodes in the full run) and shifts every id between phases. Stage 5's draft says pathz3 can't safely run twice on one corridor, because some helpers keep function-static state. The draft proposes computing the corridor once and letting both solves use it; decision 0019 also needs it saveable.

- Fix as proposed (Recommended): One corridor per run, shared by both solves and saveable. 2.0's solve must then leave the corridor unchanged, which FuSION 1's doesn't (stage 5).
- Keep FuSION 1 behaviour: Recompute the corridor before each solve.

**Answer:** Fix as proposed (Recommended), written into S4.20

## 064 · Poll · 2026-11-11 20:31

The last stage 4 item poll, with the stage 4 commit.

**Q1 (S4.21).** S4.21 · What fb_rch hands to pathz3: only the corridor graph object (graph 0, or its printed id), with its nodes, edges, attributes, id lookup and creation order. The bound, sources, targets, flag, exclusions, traversable-edge rule and distances are all discarded. pathz3 takes its own bound and reads its endpoints from the connect-pairs file; the batches keep the two in step by hand, and nothing checks that they match. No batch writes the corridor to disk. The draft proposes that the corridor records its parameters, can be saved and loaded, and that pathz3 checks its own bound and pairs against them.

- Fix as proposed (Recommended): The corridor carries its parameters, is saveable and loadable (decision 0019), and the solve checks itself against them.
- Keep FuSION 1 behaviour: In memory only, unchecked.

**Answer:** Fix as proposed (Recommended), written into S4.21

**Q2 (S4.22).** S4.22 · Compile-time flags in this stage. ONLY_INTERAC_EDGES_FOR_MINCUT changes results and becomes S4.04's run setting. DUP_CHECK_FLAG_SET and DUMMY_EDGE_FLAG don't change fb_rch's output: fb_rch uses the default duplicate check, and no traversable edge carries dummy_u_to_d. The always-on asserts can't fire on a consistent graph. The draft proposes dropping those two flags for this stage, with stage 5 confirming them for its own copies.

- Fix as proposed (Recommended): Following S1.29: the result-changing flag becomes S4.04's setting, and the two no-effect flags go for this stage.
- Keep FuSION 1 behaviour: Keep all three as compile-time flags.

**Answer:** Fix as proposed (Recommended), written into S4.22. All stage 4 items are decided, except S4.11, which is deferred to S5.13.

**Q3 (Commit).** Stage 4 is then complete, apart from S4.11, which is deferred to stage 5's S5.13 and says so in its decision field. May I commit it locally as 'Review spec stage 4: reachability corridor'? It would hold the stage 4 spec and prompt log entries 056 to 063. The stage 5 to 8 drafts stay out until they're reviewed, and this poll's own entry goes into the next commit.

- Commit (Recommended): One local commit ending with a Co-Authored-By: Claude line.
- Show me the diff first: I list every change, then ask again.

**Answer:** Commit (Recommended). This entry is left for the next commit.

## 065 · Poll · 2026-11-11 20:45

The first stage 5 item poll (`docs/spec/05-encoding-and-solve.md`). Before polling, Claude applied the stage 5 verification report after spot-checking it: the dump directories under `runs/` (171 with 5.55 GB at draft time, not 102 with 3.0 GB), the census bound range, the moved `r0_cxcl8_jun` limits file, the pre-fix timeout tests, the dump file sizes, the comparison batches' limits-file names, three line ranges, the never-filled static sets, the edge-relaxation rule, the dump-directory sharing condition and mode 1's parse.

**Q1 (S5.01).** S5.01 · pathz3 takes 28 positional tokens, then up to five optional ones: graph, mode, connect-pairs file, four relaxation bounds, path bound, output prefix, 14 list files (8 of them empty in every real batch), the fold-change file, count/explore, and two timeouts. A missing number aborts the run, an unknown graph calls exit(1), and the help line documents none of the 28. The draft proposes keeping the positional form for old batches, adding names (decision 0018), validating every argument before the solve (decision 0021), and making an unknown graph a command error.

- Fix as proposed (Recommended): Positional and named forms, everything validated up front, and an unknown graph is a command error.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S5.01

**Q2 (S5.02).** S5.02 · The optional arguments: coexpression CSV, coexpression threshold, frozen threshold, enable flag and seed. On Linux an empty token, such as an empty $VAR, stops the parse, so every later slot keeps its default: with an empty CSV, '700 800 1 42' leaves the frozen constraints off. On macOS each slot consumes its token. No logged line is affected, but stage 8's draft says the GUI never fills these slots on Linux, which its verifier is checking. The draft proposes taking the macOS behaviour and rejecting a non-numeric value with a message.

- Fix as proposed (Recommended): Each position consumes its token, and non-numeric values are errors.
- Keep Linux behaviour: A blank stops the parse.

**Answer:** Fix as proposed (Recommended), written into S5.02

**Q3 (S5.03).** S5.03 · The enable flag defaults to off. One flag gates both the coexpression and the frozen-edge constraints, and it defaults to 0. Six old batch lines pass a CSV and both thresholds but no flag. They ran with the constraints before the flag existed, but rerun today they run without them, with only a stderr note. 126 lines pass 1 and 32 pass 0. The draft proposes, under decision 0021: a given flag still decides, so 0 turns both off; with no flag, a given CSV and threshold turn coexpression on and a given frozen threshold turns frozen edges on; and the run records which families it used.

- Fix as proposed (Recommended): A flag decides if given; otherwise the inputs given decide. The families used are recorded.
- Fix, flag required: Passing a CSV or threshold without the flag is an error, so nothing is turned on implicitly.
- Keep FuSION 1 behaviour: Off unless the flag says 1.

**Answer:** Fix as proposed (Recommended), written into S5.03

**Q4 (S5.04).** S5.04 · The connect-pairs file and the active source. One '<source> <target>' pair per line, resolved in the corridor, with unresolved pairs skipped silently; only the first pair's source is forced active. With no resolvable pair the code calls front() on an empty list, which is undefined behaviour. Every batch writes exactly one pair, repeating fb_rch's source and target by hand. The draft proposes taking the source and target from the corridor (S4.21) or from named arguments, requiring them to resolve, and supporting one pair until multi-pair semantics are designed.

- Fix as proposed (Recommended): Source and target from the corridor or named arguments, required to resolve, one pair for now.
- Keep FuSION 1 behaviour: The pairs file, as now.

**Answer:** Fix as proposed (Recommended), written into S5.04

## 066 · Poll · 2026-11-11 20:50

The second stage 5 item poll.

**Q1 (S5.05).** S5.05 · Node list files. Up, down, essential, avoid, active, inactive, confirmed up and down, relaxed, and non-relaxed are all read the same way and resolved in the corridor, with unresolved ids dropped silently. Essential means present, avoid means absent, active and inactive fix activity, and non-relaxed forbids relaxation. The relaxed-nodes list is read and ignored. The console gives counts, not names, and contradictory lists simply make the solve UNSAT. The draft proposes keeping the list semantics, dropping the ignored relaxed-nodes and relaxed-edges inputs, reporting which node each id bound to (S2.03), and rejecting contradictory lists before solving.

- Fix as proposed (Recommended): Same semantics, the two ignored inputs dropped, bindings reported and contradictions rejected up front.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S5.05

**Q2 (S5.06).** S5.06 · Edge list files: essential, avoid, relaxed and non-relaxed, one '<source> <target> <subtypes>' per line. A line selects an edge only if its subtype field matches the stored order exactly, so activation_phosphorylation can match where phosphorylation_activation doesn't. Unmatched lines pass silently: one real avoid file has 10 lines and matched 3 edges. A line with only two ids reuses the previous line's subtypes. The draft proposes requiring the subtype field, comparing it as a set, and reporting every line that matches no edge.

- Fix as proposed (Recommended): The subtype field is required and compared as a set, and unmatched lines are reported.
- Keep FuSION 1 behaviour: Exact-order match, silent misses.

**Answer:** Fix as proposed (Recommended), written into S5.06

**Q3 (S5.07).** S5.07 · The observations and the node-bound clamp. up_use = up minus avoid, adjusted by the confirmed lists, and down_use likewise; only the up set loses avoided nodes. The node-relaxation bounds are clamped to |up_use| + |down_use|, which counts the source, the target and nodes outside the encoding, none of which can be relaxed. In the reference run only IRS2 can be relaxed, yet the window is n ∈ [0, 3]. The lists are also read twice, once into a map nothing uses. The draft proposes keeping the set arithmetic, dropping the unused read, and clamping the node bound to the number of observations that can actually be relaxed, printing that number.

- Fix as proposed (Recommended): Same sets, one read, and a clamp that counts only relaxable observations, printed.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S5.07

**Q4 (S5.08).** S5.08 · Work whose result nothing uses. Before encoding, pathz3 registers an undirected copy of the corridor, fills an n×n adjacency matrix and computes its O(n³) transitive closure (about 24 s per call on the full run's corridor, 48 s per batch). It also allocates four n×n matrices nothing prints, opens a mincuts file it never writes, builds disjunctions it then discards, and asserts two totals every assignment satisfies. The draft proposes dropping all of it; none of it changes any output.

- Drop (Recommended): None of it in 2.0.
- Keep: Port it as it is.

**Answer:** Drop (Recommended), written into S5.08

## 067 · Poll · 2026-11-11 20:55

The third stage 5 item poll.

**Q1 (S5.09).** S5.09 · Solver, variables and bit widths. Each call makes one Z3 QF_BV solver. Every corridor node gets present, active and relaxed Booleans plus a distance bit-vector; every edge gets selected, relaxed and path-flag Booleans; and there are five single bound bit-vectors, all sized to their ranges. Bound values are built with bv_val, which wraps modulo 2^width, and the edge bound is never clamped, so on a small corridor an edge bound of 4 can silently become 0. With no observation in the corridor, the node-relaxation width is ceil(log2(0)), which is undefined. The draft proposes keeping the variable families and the bit-vector encoding, clamping or rejecting an edge bound above |E|, and stopping with a message when no observation reaches the corridor.

- Fix as proposed (Recommended): Same encoding, an edge bound above |E| clamped or rejected, and a clear stop when no observation reaches the corridor.
- Keep FuSION 1 behaviour: Wrapping bounds and an undefined width.

**Answer:** Fix as proposed (Recommended), written into S5.09

**Q2 (S5.10).** S5.10 · Which nodes and edges enter the encoding. Constraints are posted only over the edges reachable from the source along paths that don't pass through the target, plus their endpoints and any essential nodes: 108 of 115 nodes and 364 of 370 edges in the reference run. It's computed by a pair worklist with function-static memory, named for mincuts it never computes. A reached node with no out-edges would throw out_of_range, which a corridor can't trigger. Expressed nodes left out count in Up: but never in the objective. The draft proposes computing the sets with a plain forward search from the source that stops at the target, inside the corridor, and listing the observations that fall outside.

- Fix as proposed (Recommended): The same sets from a plain forward search, with excluded observations listed.
- Keep FuSION 1 behaviour: The pair worklist.

**Answer:** Fix as proposed (Recommended), written into S5.10

**Q3 (S5.11).** S5.11 · Path constraints under the bound. A selected edge implies both ends present and a strictly increasing distance; every target's distance is at most the path bound; a present source has a selected out-edge, a present target an in-edge, and every other present node both. Together these make the selected edges a DAG of source-to-target paths of at most b edges, and a self-loop can never be selected. An essential node that can't lie on a path, for example an FSC bound to an isolated complex member, trips a compiled-in assert and aborts the run. The draft proposes keeping the constraints, and reporting that case as infeasible with a message before solving.

- Fix as proposed (Recommended): Same constraints, with a clear infeasibility message instead of an abort.
- Keep FuSION 1 behaviour: The assert aborts the run.

**Answer:** Fix as proposed (Recommended), written into S5.11

**Q4 (S5.12).** S5.12 · Node relaxations. An up-regulated node is explained only if it's present and active; a down-regulated node if it's absent, or present and inactive; a node in both lists counts as up. Sources and targets are exempt, so the target's own expression never enters the objective. As a modelling choice that nothing states, an up gene on no path always costs 1 and a down gene on no path never does. The counted nodes go to a fixed-name diff_expr_nodes.txt in the launch directory, overwritten by each call. The draft proposes keeping the rule, and writing the counted observations and the excluded ones (source, target, outside the encoding) to the run's output.

- Fix as proposed (Recommended): Same rule; the counted and excluded observations are written to the run's output, stating the modelling choices.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S5.12

## 068 · Poll · 2026-11-11 21:00

The fourth stage 5 item poll. The S5.13 answer also settles stage 4's deferred S4.11.

**Q1 (S5.13).** S5.13 · Edge relaxations and signs. An inhibition or repression edge is relaxed when both ends are present and in the same state; every other edge (activation, expression, phosphorylation, dephosphorylation, ubiquitination, binding, dummy_u_to_d, or none) is relaxed when both ends are present and not both active. Relaxation doesn't depend on whether the edge is selected, so edges no solution path uses still change the result: an inhibitory self-loop on a present node always costs 1, and walk-only edges count too. Connectivity ignores signs, so a forced node can hang off a relaxed inhibition edge. Your answer also settles S4.11: if off-path edges keep counting, the corridor drops self-loops and edges into a source or out of a target.

- Fix as proposed (Recommended): Keep the sign rules and keep counting every edge between present nodes (FuSION's notion of consistency), but leave self-loops out of the count and report sign-consistent routes separately. S4.11 then trims the cheap off-path edges.
- Count only selected edges: An edge counts as relaxed only if a solution path uses it, so unused edges never change the result. Frontiers change (decision 0023), and S4.11 keeps the walk rule as it is.
- Keep FuSION 1 behaviour: As now, self-loops included.

**Answer:** Fix as proposed (Recommended), written into S5.13 and S4.11

**Q2 (S5.14).** S5.14 · Every path passes a differentially expressed node. Path flags on edges force every selected source-to-target path through an up- or down-regulated node before the target; a relaxed DE node still counts, and a direct edge from a non-DE source to the target can never be selected. No help line or output mentions this rule, yet the frontier depends on it. The draft proposes keeping the rule as it is, and documenting it in the help and the output.

- Keep, documented (Recommended): Port the rule, and state it in the help and the run's output.
- Make it a setting: On by default, but a run can turn it off, and the output records which.

**Answer:** Keep, documented (Recommended), written into S5.14

**Q3 (S5.15).** S5.15 · Coexpression constraints. The CSV's first line is skipped, columns are positional (ids in columns 3 and 4, score after the fourth comma), and rows scoring strictly above the threshold give pairs of corridor nodes. For each pair, if both are present their activity must match. The reference run loads 1 pair and emits 1 clause. Malformed and unresolved rows are skipped without a count, and every pathz3 call parses the file again. The draft proposes keeping the constraint and the strict threshold, finding columns by header name, reporting skipped and unresolved rows, and parsing the file once per run.

- Fix as proposed (Recommended): Same constraint and strict threshold, columns by header name, skipped rows reported, one parse per run.
- Keep FuSION 1 behaviour: Positional columns, silent skips, reparsed per call.

**Answer:** Fix as proposed (Recommended), written into S5.15

**Q4 (S5.16).** S5.16 · Frozen edges. Every considered edge whose experiment_score is strictly above the threshold can't be relaxed (97 clauses in the reference run), whether or not it's selected. The batches say 'frozen edges score >=800', but the test is >, so the reference run's one edge at exactly 800 isn't frozen. KGMLs without the attribute score 0 and can never be frozen, without notice. The draft proposes keeping the rule and the strict >, which matches the coexpression test and leaves results unchanged, correcting the batches' wording, stating the test in the help and output, and reporting how many considered edges have no score.

- Fix as proposed (Recommended): Strict >, as now, stated clearly, with unscored edges reported.
- Fix, use ≥: Change the test to ≥ to match the batches' wording. The reference run's edge at exactly 800 becomes frozen, so results can change (decision 0023).
- Keep FuSION 1 behaviour: As now, unreported.

**Answer:** Fix as proposed (Recommended), written into S5.16

## 069 · Poll · 2026-11-11 21:05

The fifth stage 5 item poll, with stage 5's open question 5.

**Q1 (S5.17).** S5.17 · The FSC forced in or out. pathz3 has no FSC argument: the batches build the two cases from the lists. 'In' puts the FSC in the essential, active and non-relaxed lists; 'out' puts it in the avoid list, which also drops an up-regulated FSC from the observations. The two solves run on two separately built corridors, and the batch keeps 15 arguments in step across four command lines. The draft proposes making the FSC a named argument of the solve and running both cases on one corridor. 'In' means present and active, and 'out' means absent and removed from the observations.

- Fix as proposed (Recommended): A named FSC, both cases solved on one corridor, with the in/out meanings above.
- Keep FuSION 1 behaviour: Built by hand from the lists in the batch.

**Answer:** Fix as proposed (Recommended), written into S5.17

**Q2 (Down FSC).** What should 'FSC forced in' mean for a down-regulated FSC (stage 5, open question 5)? Today 'in' means present and active, which contradicts a down-regulated gene's observation. Its relaxation becomes true while the non-relaxed list forbids it, so every point of the with-FSC solve is UNSAT. That's traced from the code, not run.

- Present, consistent (Recommended): 'In' means present on a path, with activity matching its observation: inactive when down-regulated, active when up-regulated.
- Present and active, paid: Present and active either way, with its node relaxation paid in the objective for a down-regulated FSC.
- Refuse it: A down-regulated FSC is rejected with a message, and only up-regulated or unobserved FSCs can be forced in.

**Answer:** Present, consistent (Recommended), written into S5.17 and stage 5's open question 5

**Q3 (S5.18).** S5.18 · How the Pareto frontier is enumerated. Assuming monotonicity (if (n, e) is SAT, so is every point with larger n and e), the search runs a diagonal search, then binary searches for the smallest n and the smallest e, records one or two points, and recurses into the two remaining windows. A function-static matrix caches SAT and UNSAT per point. It's correct when every check gets an exact answer; its weak points are unknown answers (S5.19), repeated timeouts (S5.20) and discarded models (S5.26). The draft proposes keeping the algorithm.

- Keep (Recommended): Port the search, with its weak points fixed in S5.19, S5.20 and S5.26.
- Hold for Phase 2: Decide the search algorithm in Phase 2, together with dump reuse (roadmap objective 4).

**Answer:** Keep (Recommended), written into S5.18

**Q4 (S5.19).** S5.19 · Unknown results. A check can return unknown. Since the 2026-11-09 fix it's labelled TIMEOUT when the reason says so or the check used 99% of the overall timeout, and UNDEFINED otherwise; before the fix every timeout was logged as UNDEFINED. Either way the search treats it as not SAT, so a frontier point found next to an unknown answer isn't certified Pareto-optimal, and nothing in the outputs says so. The full run's (n 7, e 1) and (n 8, e 0) may be dominated by the undecided (n 7, e 0). The draft proposes keeping the TIMEOUT/UNDEFINED distinction and marking every frontier point whose optimality depends on an unknown answer, in the frontier and solutions outputs.

- Fix as proposed (Recommended): Keep the labels, and mark uncertified points in the outputs.
- Keep FuSION 1 behaviour: Unmarked.

**Answer:** Fix as proposed (Recommended), written into S5.19

## 070 · Poll · 2026-11-11 21:20

The sixth stage 5 item poll.

**Q1 (S5.20).** S5.20 · Timed-out points are solved again. The search caches only SAT and UNSAT, so when it reaches a TIMEOUT or UNDEFINED point again, from another step or window, it solves the same query from scratch. In fdr050, 5 of the 9 four-hour unknown checks repeated a query that had already timed out: about 20 of the run's 50.7 hours. It still happens after the 2026-11-09 fix. The draft proposes caching TIMEOUT and UNDEFINED like SAT and UNSAT within a solve, and keeping that cache across a resumed solve (decision 0019).

- Fix as proposed (Recommended): Cache unknown answers too, and keep the cache when a solve is resumed.
- Keep FuSION 1 behaviour: Re-solve timed-out points.

**Answer:** Fix as proposed (Recommended), written into S5.20

**Q2 (S5.21).** S5.21 · Solver timeouts. timeout1 becomes Z3's :solver2_timeout and timeout2 its :timeout, in milliseconds. Both apply to each check separately, and nothing limits a whole solve: the full run's without-FSC solve took 15.9 h under a 4 h cap, and fdr050's 34.5 h. The limits file logs CPU time, while the cap and the TIMEOUT test use wall time, and what timeout1 actually does is undocumented (open question 1). The draft proposes keeping a per-query timeout, adding an optional budget for the whole solve that stops cleanly and saves state for resuming (decision 0019), and logging wall time.

- Fix as proposed (Recommended): A per-query timeout, an optional whole-solve budget with clean resumable stops, and wall time logged.
- Keep FuSION 1 behaviour: Per-check timeouts only.

**Answer:** Fix as proposed (Recommended), written into S5.21

**Q3 (S5.22).** S5.22 · Random seed. It defaults to 42, is set on the solver, and is written at the top of each dump. Reruns reproduce on the same machine and Z3 build: the fdr025 rerun matched the reference run byte for byte. The smallest-e step writes its dumps without the seed line, and a source comment claims standalone Z3 on a dump reproduces the in-process run byte for byte, which is untested and unlikely without the timeouts and solver history. Outside the dumps, the seed appears in only one console line. The draft proposes keeping the seed and its default, recording it in the run's outputs, and writing it into every dump.

- Fix as proposed (Recommended): Seed 42 by default, recorded in the outputs and in every dump.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S5.22

**Q4 (S5.23).** S5.23 · SMT dumps. Before every uncached check, the whole formula is written to <fold-change name>_smt-files_<timestamp>/PO_constraints_nr_<n>_er_<e>.smt2 in the launch directory, and FuSION never reads them. When this stage was drafted, runs/ held 171 dump directories with 5.55 GB; the full run alone wrote 123 MB. A re-solved point overwrites its earlier dump, and variable names are session ids, meaningless without the run's solutions file. The draft proposes writing dumps only under the debug option (S1.29), into the run's output directory, named by solve case and point. Reusing dumps (roadmap objective 4) is a Phase 2 question.

- Fix as proposed (Recommended): Debug-only dumps in the run's output directory, named by case and point.
- Fix, on by default: Same location and naming, but written unless turned off, ready for roadmap objective 4's reuse.
- Keep FuSION 1 behaviour: Always written, in the launch directory.

**Answer:** Fix as proposed (Recommended), written into S5.23

## 071 · Poll · 2026-11-11 21:35

The seventh stage 5 item poll.

**Q1 (S5.24).** S5.24 · The limits file (limits_timefile.txt). Each pathz3 call truncates it in the launch directory and writes a free-text trace of the search: windows, every check with its CPU time and sat/unsat/unknown, cache hits, binary-search steps and results. Points are written (e, n) here but (n, e) in PO.dat, and the window line mixes both orders. The trace is the only record of unknown answers (S5.19) and solver times, and mode 1 empties it while writing nothing (S5.28). The draft proposes a structured search log in the run's output directory instead: one record per check with point, step, outcome, wall and CPU time, and reason, all in one point order. One record per check means it can still be followed while a solve runs.

- Fix as proposed (Recommended): A structured per-check search log in the run's output directory, one point order.
- Keep FuSION 1 behaviour: The free-text trace in the launch directory.

**Answer:** Fix as proposed (Recommended), written into S5.24

**Q2 (S5.25).** S5.25 · PO.dat and the dominance filter. After the search the points are sorted by n descending, then e ascending, and a filter skips any point whose e equals the last kept point's e. Of two such points it keeps the larger n, which is backwards. It also never fires, because the search's windows give every point a distinct e. The rest are written as <n>\t<e> to PO.dat, which plot_po.sh and pex read; there is no header, no case label and no mark for uncertified points (S5.19). The draft proposes dropping the filter, keeping the two-column file for stage 6, and carrying the certification flag in the structured outputs.

- Fix as proposed (Recommended): Drop the filter, keep the two-column file, certification flag in the structured outputs.
- Keep a corrected filter: As proposed, but keep a filter that drops the larger n, as a guard in case a later search change yields dominated points.
- Keep FuSION 1 behaviour: The backwards filter stays.

**Answer:** Fix as proposed (Recommended), written into S5.25

**Q3 (S5.26).** S5.26 · Solutions at each frontier point. The search finds a model at every frontier point and discards it, so afterwards each point is solved again, with a blocking clause after each solution. In fdr050 that re-solving cost about 2226 s (with FSC) and 12,871 s (without). The blocking clause covers every Boolean, including don't-care bits such as the activity of absent nodes, so a second solution may differ only in such a bit. With count = explore = 1 (166 of 170 batch lines) each point costs two more checks and prints an empty 'Solution 2' header. With no frontier point, no solution files are written at all. The draft proposes keeping the search's model at each point, enumerating further solutions only on request while blocking only on present nodes, their activity and selected edges, and always writing the solution files, empty when there is no point.

- Fix as proposed (Recommended): Reuse the search's model, extra solutions on request with a tight blocking clause, files always written.
- Keep FuSION 1 behaviour: Re-solve every point and block on every variable.

**Answer:** Fix as proposed (Recommended), written into S5.26

**Q4 (S5.27).** S5.27 · Solutions and relaxations files. For each frontier point, temp_w/temp_wo _solutions.txt lists every present node (nid, rep id, symbol, up/down/not DE, present, active/inactive) and every selected edge (eid, subtypes, both ends with their state, then a bracketed list of pathways); _relaxations.txt lists every relaxed node and edge, present or not. Example node line: 39118\thsa3725\tJUN\tup_reg\tpresent\tactive. The ids are corridor ids, which differ between the two solves (S4.14); an absent node's active/inactive is an arbitrary model value; and the format is positional text with a bracketed tail. The draft proposes keeping both files for stage 6 until it settles its inputs, naming nodes and edges by stable keys (S4.14), and leaving out the activity of absent nodes.

- Fix as proposed (Recommended): Same two files for now, stable keys, no activity for absent nodes.
- Keep FuSION 1 behaviour: Corridor ids and every column as now.

**Answer:** Fix as proposed (Recommended), written into S5.27

## 072 · Poll · 2026-11-11 21:50

The last stage 5 item poll.

**Q1 (S5.28).** S5.28 · Mode 1: testing given points. With mode 1, pathz3 reads '<n> <e>' lines from a file, checks each point, stops at the first SAT one ('Sat point obtained on previous curve') or reports 'New curve completely separated from previous points', and writes no solutions. No batch uses it: all 170 lines are mode 0. It truncates the previous solve's PO.dat and limits file before it starts, and a blank or short line silently reuses the previous point. The draft proposes dropping it: decision 0019's 'solve part of the frontier' is designed in Phase 2 and can cover this use.

- Drop (Recommended): Remove mode 1; Phase 2's partial-frontier design covers checking given points.
- Fix and keep: Keep mode 1 as its own option, without truncating earlier outputs and with strict line parsing.
- Keep FuSION 1 behaviour: As now.

**Answer:** Drop (Recommended), written into S5.28

**Q2 (S5.29).** S5.29 · Function-static state. Seven encoding functions keep static sets of what they've already encoded and skip those items on later calls; six more keep sets they never fill, and one keeps an unused static vector. The batches stay correct only because their second fb_rch builds a new corridor with new ids. A second pathz3 on the same graph would encode no edges, skip the essential nodes and most rules, and most likely report the single point (n 0, e 0) with no error. Traced from the code; no batch does it. This answers stage 4's open question 2 (no). The draft proposes that no state survives a solve: each solve builds its encoding from its own inputs.

- Fix as proposed (Recommended): No state survives a solve; each solve encodes from its own inputs.
- Keep FuSION 1 behaviour: Static state stays, with a second solve per corridor unsupported.

**Answer:** Fix as proposed (Recommended), written into S5.29

**Q3 (S5.30).** S5.30 · Where the outputs go. limits_timefile.txt and PO.dat go to the launch directory with the upfx prefix joined on; the solutions, relaxations and mincuts files go wherever the solve's prefix points (the batches use $WRITE_DIR/temp_w and temp_wo); diff_expr_nodes.txt and the dump directories go to the launch directory without any prefix. Every batch moves files with exec mv after each solve. Two runs in one directory overwrite each other's PO.dat, limits file and diff_expr_nodes.txt. The draft proposes that every output of a solve goes to the run's output directory, named by solve case, with no fixed names written to the launch directory (as decided for S1.26).

- Fix as proposed (Recommended): Everything in the run's output directory, named by solve case.
- Keep FuSION 1 behaviour: Fixed names in the launch directory, moved by the batch.

**Answer:** Fix as proposed (Recommended), written into S5.30

**Q4 (S5.31).** S5.31 · Console output. A successful pathz3 prints setup lines (seed, coexpression pairs loaded, dump directory, list counts, Up/Down counts, the bound clamp), encoding milestones, each frontier point as it is found as (e, n), the frontier summary ('(0 ,1) is not dominated'), 'Solution 1'/'Solution 2', and a '[pathz3 timing]' wall-clock line. Points are (e, n) with uneven spacing, the timing line is missing when the frontier is empty and in mode 1, some lines describe unused work ('Not printing matrices in the current run'), and stdout versus stderr follows no rule. The draft proposes printing the same milestones with points in one labelled order, always printing the timing line, and removing the lines about unused work.

- Fix as proposed (Recommended): Same milestones, labelled points in one order, timing line always, no lines about unused work.
- Keep FuSION 1 behaviour: Console output as now.

**Answer:** Fix as proposed (Recommended), written into S5.31. All 31 stage 5 items are now decided.

## 073 · Correction · 2026-11-11 21:32

Entries 062 to 072 give estimated times instead of the times they were logged. The times they were logged, taken from the session record, are:

| Entry | Logged |
|---|---|
| 062 | 2026-11-11 20:18 |
| 063 | 2026-11-11 20:19 |
| 064 | 2026-11-11 20:20 |
| 065 | 2026-11-11 20:25 |
| 066 | 2026-11-11 20:26 |
| 067 | 2026-11-11 20:27 |
| 068 | 2026-11-11 20:40 |
| 069 | 2026-11-11 20:49 |
| 070 | 2026-11-11 21:04 |
| 071 | 2026-11-11 21:25 |
| 072 | 2026-11-11 21:30 |

## 074 · Poll · 2026-11-11 21:38

The stage 5 commit poll.

**Q1 (Commit).** Stage 5 (encoding and Pareto solve) is fully decided: 31 items, 27 fix / 2 keep / 2 drop. May I commit it locally as one commit? It would contain the new docs/spec/05-encoding-and-solve.md; two cross-stage notes (stage 4: S4.11 settled by S5.13 and its open question 1 resolved; stage 2: open question 5 resolved, as the solver never reads dummy_u_to_d); and prompt log entries 064 to 073. Entry 073 is a correction listing the real logging times of entries 062 to 072, whose stated times were estimates up to 28 minutes off. Message: 'Review spec stage 5: encoding and Pareto solve', ending with the Co-Authored-By line. The stage 6, 7 and 8 drafts stay out.

- Commit as shown (Recommended): One local commit with those four files; nothing is pushed.
- Change something first: Tell me what to change before committing.

**Answer:** Commit as shown (Recommended), committed as 5448bf4

## 075 · Poll · 2026-11-11 22:51

The first stage 6 item poll. Before it, the stage 6 verifier's corrections were applied to the draft:

- S6.26's example pair;
- the `not_present` counts, 1935 node and 667 edge lines in the relaxation files dated 2026-11-10 or earlier (S6.02, S6.15, open question 4);
- S6.05's three b10 runs and how its edges are counted;
- a real one-column collapse case in S6.13;
- `alt:-1` in S6.01;
- the unwritable prefix in S6.09;
- the ipython dependency in S6.11;
- the legend's dash patterns in S6.16;
- the manifest's escapes in S6.19;
- CDKN1A and FOXO6 do appear on the page cited in S6.06;
- open question 3, answered yes (r5_mcl1_stat3).

Claude also corrected S6.14's citation of `edge_has_subtype`, which ends at line 1874, not 1873.

**Q1 (S6.01).** S6.01 · psp command line: 'psp <solution_file> <src> <tgt> <path_mode> <po_selector> <out_prefix>', path_mode shortest | alt[:W] | all | subgraph. No batch file or GUI has ever called psp. A missing token arrives as an empty string (a missing prefix writes '_n7_e0.dot' into the launch directory), extra tokens are ignored, 'alt:-1' is accepted and draws only the unconnected source and target, and errors leave the exit status unchanged. The draft proposes keeping the six arguments (named, decision 0018) and rejecting an unknown mode, a bad window, missing or extra arguments, and identical output names for the two cases. Whether psp survives as its own command is S6.26.

- Fix as proposed (Recommended): Named arguments, strict validation; psp's survival as a command decided in S6.26.
- Drop psp now: Unused, so leave it out of 2.0; S6.26 can still carry its path filter into the one renderer.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S6.01

**Q2 (S6.02).** S6.02 · Reading the solution and relaxation dumps. psp (C++) and pex (Python) each parse temp_*_solutions.txt and temp_*_relaxations.txt: blocks start at 'Current PO point: N nodes, M edges relaxed', only lines under 'Solution 1' are kept, and tab-split lines with 6 fields are nodes and 11+ are edges. Both key everything by rep id, so the nid shift between solves doesn't affect them. Problems: later witnesses are ignored silently; a PO point with no witness (its witness solve ended unknown) is skipped silently by psp and drawn as an empty page by pex, with no mention of the timeout. 12 such points sit in four old runs, and it still happens after the 2026-11-09 fix (r5_mcl1_stat3, without FSC, (7,0)). The draft proposes one documented reader for the renderers, rep-id keys, every recorded witness read, and a missing witness marked as such. Stage 5 already decided the solve keeps its model (S5.26) and uses stable keys (S5.27).

- Fix as proposed (Recommended): One documented reader, all witnesses, missing witnesses marked.
- Keep FuSION 1 behaviour: Two parsers, first witness only, witness-less points skipped or drawn empty.

**Answer:** Fix as proposed (Recommended), written into S6.02

**Q3 (S6.03).** S6.03 · Choosing PO points. Both renderers take 'all' (every point, ascending (n, e)), 'least' (smallest n + e, ties to smaller n) or '<n>,<e>' (that point; an unknown point prints the available ones and draws nothing). All 24 pex calls use 'all'. Problems: points without a witness are in pex's set but not psp's (S6.02), so 'least' can pick an empty page in pex; psp's sscanf accepts trailing text ('7,1x' gives 7,1), pex rejects it. The draft proposes keeping the three selectors: the witness-less case is fixed by S6.02 and malformed arguments are already rejected under S1.05.

- Keep (Recommended): all, least and <n>,<e> as they are, with S6.02 and S1.05 covering the problems.
- Fix: add selectors: Also allow a list or range of points; tell me which forms in a note.

**Answer:** Keep (Recommended). Not yet written into S6.03: the question's premise that S1.05 rejects malformed arguments was imprecise, so a follow-up question confirms it (entry 076).

**Q4 (S6.04).** S6.04 · psp's path modes. Over edges with both ends active, psp runs a forward search from the source and a backward one from the target, then keeps an edge whose shortest walk through it is: equal to the shortest path length L (shortest); at most L + W (alt, W = 2 by default); any finite length (all); or every candidate edge with no test (subgraph). It costs O(V + E) per point. On the full JUN run with FSC at (2,6) it gives 6 nodes/6 edges (shortest), 6/6 (alt), 76/103 (all) and 79/106 (subgraph). alt and all work on walks, not simple paths: an edge passes if a walk through it fits, even when no simple path does, the same issue as S4.11. No such edge shows up in that run. Which edges are candidates is wrong in two ways, handled in S6.05 and S6.06. The draft proposes keeping the filter and the four modes, documented as walk-based, since enumerating simple paths would be exponential.

- Keep (Recommended): Distance filter and four modes, documented as walk-based.
- Fix: simple paths for alt: Enumerate simple paths up to L + W for alt, accepting exponential worst cases on dense corridors.

**Answer:** Keep (Recommended), written into S6.04

## 076 · Poll · 2026-11-11 22:55

The second stage 6 item poll, with a follow-up to entry 075's S6.03 question and stage 6's open question 5. After it, S6.03's proposed call was corrected to drop the imprecise S1.05 premise.

**Q1 (S6.03).** S6.03 follow-up. In my last question I said malformed selectors are 'already rejected under S1.05'. That was imprecise: S1.05 only lets a command tell a missing argument from an empty one. Today pex rejects a selector with trailing text such as '7,1x', while psp reads it as 7,1. Which form should 'keep' port?

- Keep, strict (Recommended): all, least and <n>,<e>; trailing text such as '7,1x' is an error, as in pex.
- Keep, lenient: Trailing text is ignored, as in psp.

**Answer:** Keep, strict (Recommended), written into S6.03

**Q2 (S6.05).** S6.05 · psp draws relaxed inhibition edges as ordinary path edges. Its candidate edges are those with both ends active, whatever the sign, and it never reads the relaxations file. Under the solver's rule an inhibition or repression edge with both ends active is relaxed and counts against the edge budget. Example, full JUN run without FSC, point (2,7): one shortest path is EGFR → SRC → CDC42 → MAPK14 → TP53 → DUSP1 ⊣ MAPK1 → ETS1 → CXCL8, and DUSP1 ⊣ MAPK1 (inhibition_dephosphorylation) is one of that point's 7 relaxed edges, drawn as a plain hop. Each solution file of the three b10 full runs has 3 to 5 such edges. The draft says a path view must use the solver's polarity rule and either follow only honoured edges or mark relaxed ones. The solver's own paths ignore signs (S5.11), and S5.13 already decided that sign-consistent routes are reported separately.

- Fix: mark relaxed edges (Recommended): Paths follow the witness's edges as the solver chose them, with relaxed ones drawn as relaxed; sign-consistent routes come from S5.13's separate report.
- Fix: honoured edges only: The path view follows only edges whose effect the solution honours.
- Keep FuSION 1 behaviour: Relaxed inhibitions drawn as plain path edges.

**Answer:** Fix: mark relaxed edges (Recommended), written into S6.05

**Q3 (S6.06).** S6.06 · Both renderers hide honoured inhibitions and the nodes they switch off. They build the explanation from edges whose ends are both active; pex then adds the relaxed overlay. Under the solver's rule an inhibition is honoured exactly when its ends differ (active ⊣ inactive or inactive ⊣ active), and the solutions file lists such edges as present, but neither renderer draws one, nor a present inactive node unless it ends a relaxed edge. Example, full JUN run with FSC, (7,0): CDKN1A ⊣ CDK2 ⊣ FOXO6 is a present double negative, with CDK2 and ID1 present and inactive; CDKN1A and FOXO6 are on the page, CDK2, ID1 and both inhibition edges are not. Across 569 renders, 1192 such present edges and 302 present nodes go undrawn, so a down-regulated gene explained through an inhibition never shows its explanation. The draft proposes drawing every present node and edge of the witness, with each node's activity and whether each edge is honoured or relaxed. Choosing that also answers stage 6's open question 5: the explanation is every present element of the witness.

- Fix as proposed (Recommended): Draw the whole witness: every present node with its activity, every present edge as honoured or relaxed.
- Fix, active core by default: Same, but the page opens on the both-active core and the inactive part is one click away.
- Keep FuSION 1 behaviour: Both-active edges plus the relaxed overlay.

**Answer:** Fix as proposed (Recommended), written into S6.06 and stage 6's open question 5

**Q4 (S6.07).** S6.07 · psp's DOT and SVG output. For each point psp writes <prefix>_n<n>_e<e>.dot (source green, target red, the rest grey, one edge per distinct source/target/subtype, so expression + activation gives two parallel edges) and runs 'dot -Tsvg' through system(). File names reach the shell unquoted (a space breaks it, a metacharacter runs), labels aren't escaped (a quote in a symbol breaks the file), a missing dot is reported only after the .dot is written, each point costs one dot process, and the drawing has no expression colours, polarity or relaxations. FuSION 1's Linux README wrongly says dot must be at /usr/bin/dot and that pex uses it. The draft proposes keeping a static export, escaping names and labels, checking for dot before the run starts (decision 0021), and documenting dot correctly.

- Fix as proposed (Recommended): Static export kept, escaped, dot checked up front, documented correctly.
- Fix, no Graphviz: Write the static export (e.g. SVG) directly from 2.0, so dot is no longer a dependency.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S6.07

## 077 · Poll · 2026-11-11 22:57

The third stage 6 item poll.

**Q1 (S6.08).** S6.08 · psp's edge-sources CSV. Per point psp writes <prefix>_n<n>_e<e>_edge_sources.csv with header edge_index,source_symbol,source_hsa,target_symbol,target_hsa,subtype,pathway_ids,pathway_names: one row per kept edge line (not deduplicated), sorted by source symbol, target symbol and subtype, fields quoted, pathway ids joined by ';', names from start's pathway-name map. No real CSV exists, since psp was never run. Problems: rows don't match the drawing, because two edge ids with the same source, target and subtype give two rows but one drawn edge (104 rows for 103 edges on the full JUN run, with FSC, (2,6), mode all); a quote inside a field isn't doubled; psp needs start only for this lookup, while pex reads the map file itself. The draft proposes one row per drawn edge listing all its edge ids, standard CSV quoting, and names from the run's one pathway-name map.

- Fix as proposed (Recommended): One row per drawn edge with all its edge ids, standard quoting, one name map.
- Keep FuSION 1 behaviour: One row per edge line, as now.

**Answer:** Fix as proposed (Recommended), written into S6.08

**Q2 (S6.09).** S6.09 · psp's messages and failures. Each drawn point prints 'plot_solution_paths: PO (n,e) mode=<mode> -> <svg> (<k> nodes, <m> edges); sources -> <csv>'; errors (unreadable file, no points parsed, bad selector or mode, missing path, Graphviz status) go to stderr and the batch carries on. Problems: <m> counts CSV rows, not drawn edges (S6.08); output streams aren't checked, so with an unwritable prefix the DOT file is never written, dot fails, and psp prints 'graphviz returned 512' followed by its success line; no failure changes the exit status. The draft proposes that a failure fails the command, as S1.28 decided, and that the printed counts match what is drawn.

- Fix as proposed (Recommended): Failures fail the command; counts match the drawing.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S6.09

**Q3 (S6.10).** S6.10 · pex command line: 11 positional tokens (four solution/relaxation files, LFC file, pathway map, src, tgt, both|incl|excl, all|least|<n>,<e>, out prefix) plus optional site dir, run key and PO png. The dispatcher builds a python3 command string and runs it through system(); a file argument of 0 means none. 24 batch files call pex: 22 with 'both all', and the single-solve r7_epha2_curated and r8_phlda1_curated with '0 0' and 'incl all'; none passes the site tokens. Problems: the optional-token size tests are always true (it works only because empty strings are skipped); src, tgt, cases and po aren't quoted and no value is escaped, so an empty source gives an argparse error reported as 'renderer returned 512', and a quote or $ in a path is read by the shell; run title, takeaway and site title can only be set by running the script directly; extra tokens are ignored. The draft proposes named arguments (decision 0018), each value passed as its own argument with no shell, missing required arguments rejected, and every site option settable from a batch.

- Fix as proposed (Recommended): Named arguments, no shell, required ones checked, all site options available.
- Keep FuSION 1 behaviour: Positional tokens through a shell command string.

**Answer:** Fix as proposed (Recommended), written into S6.10

**Q4 (S6.11).** S6.11 · Finding the helpers and what they need. pex's script is <binary's directory>/supporting_files/plot_explanation.py, found on Linux via /proc/self/exe and on macOS via _NSGetExecutablePath (unresolved, possibly relative), falling back to '.'; S1.20 already decided to fix that lookup. It runs on the first python3 on PATH and needs pyvis plus jinja2, networkx, jsonpickle and ipython; the repo's .venv has them, /usr/bin/python3 here doesn't, and the repo has no requirements file. psp needs dot on PATH (only in a conda env on this machine). plot_po.sh is named by an absolute path in each batch and needs bash, awk, mktemp and gnuplot with pngcairo. Nothing is checked before the run: a missing pyvis shows up only at the end. The draft proposes S1.20's single install-directory lookup, every helper's dependencies listed in one place, and all of them checked before the first step runs (decision 0021).

- Fix as proposed (Recommended): One lookup, one dependency list, checked before the run starts.
- Keep FuSION 1 behaviour: Three conventions, dependencies found when their step runs.

**Answer:** Fix as proposed (Recommended), written into S6.11

## 078 · Poll · 2026-11-11 22:58

The fourth stage 6 item poll.

**Q1 (S6.12).** S6.12 · pex's other inputs. Every file argument is optional: '0', an empty string or a path that isn't a regular file all count as absent, silently. A missing solutions file skips that case with a message; a missing relaxations file silently drops the overlay; a missing LFC file makes the hover show 'lfc: n/a'; a missing pathway map shows ids instead of names. pex parses the LFC file (first two fields, float(), bad lines skipped, a repeated gene keeps its last value) and the pathway map (split at the first tab, else ' => ', else the first space) itself, with rules that differ from the C++ readers (S3.14, S1.19). A mistyped path therefore silently removes part of the output, against decision 0021. The draft proposes that a named file that can't be read is an error (decision 0021), '0' means not given, and pex reuses the run's one LFC reader (S3.14) and one pathway-name map (S1.19).

- Fix as proposed (Recommended): Unreadable named files are errors; one LFC reader and one name map for the whole run.
- Keep FuSION 1 behaviour: Silent fallbacks and pex's own parsers.

**Answer:** Fix as proposed (Recommended), written into S6.12

**Q2 (S6.13).** S6.13 · pex: which nodes are drawn, the layout and the relaxed-node table. pex draws the active nodes, the relaxed nodes and the ends of every drawn pair; any other node is 'parked' in a collapsible table headed 'Relaxed-away nodes (no surviving edge)' (symbol, hsa, log-FC, expression). The layout is hierarchical left to right by hop distance from the source, physics off, with same-level edges curved. The replay parked 1969 nodes in 569 renders. Problems: the table calls every parked node relaxed, but 84 never were (e.g. BCL2, active, whose only edges are honoured inhibitions); and when the source has no drawn edge every node gets level 1 and the drawing collapses into one column (a real case: EGFR, whose only edge is the honoured inhibition EGFR ⊣ CTNND1, parked as 'relaxed-away'). With S6.06's decision those honoured inhibitions are now drawn, so these cases go away and the table would hold relaxed nodes that are not in the witness. The draft proposes keeping the layout and the table, with the table saying why each node is in it.

- Fix as proposed (Recommended): Same layout and table, each row saying why the node is there.
- Keep FuSION 1 behaviour: Table labels every parked node relaxed.

**Answer:** Fix as proposed (Recommended), written into S6.13

**Q3 (S6.14).** S6.14 · pex edge polarity. pex calls an edge inhibitory when any '_'-separated part of its dumped subtype is inhibition or repression, and activating otherwise, which matches the solver's rule since 2026-07-21 (so dephosphorylation and ubiquitination alone count as activation). Within a pair it draws at most one edge per effect: a grey arrow for activation, a red T-bar for inhibition, with subtypes and pathway names in the hover. The only problem is that the rule exists twice, in C++ and in Python, linked by a comment. The draft proposes keeping it, with the polarity rule defined in one place that both the solver and the renderer use.

- Keep (Recommended): Same polarity and one-edge-per-effect drawing, one shared rule.
- Fix: one edge per subtype: Draw each subtype as its own edge instead of merging by effect.

**Answer:** Keep (Recommended), written into S6.14

**Q4 (S6.15).** S6.15 · pex: how relaxed edges are drawn. Each node pair is drawn one of four ways: (1) nothing relaxed: solid edge per effect; (2) only relaxed records: dashed edge, 'effect REMOVED by relaxation'; (3) kept and relaxed effects differ: purple dotted edge, 'net effect FLIPPED'; (4) the same effect both kept and relaxed: solid edge plus a dashed curved twin. The replay drew 45198 solid, 1917 dashed, 8 flipped and 0 twins: case 4 can no longer happen under the solver's rule. A relaxed edge that isn't in the witness (not_present, 667 lines) is drawn exactly like one that is, and relaxed status is matched on (source, target, subtype) rather than edge id. Stage 6's open question 4, whether not_present edges count as relaxed, is settled by S5.13: they do. The draft proposes keeping cases 1 to 3, dropping case 4, matching relaxed edges by edge id, and showing whether each relaxed edge is part of the witness.

- Fix as proposed (Recommended): Cases 1 to 3, no twin, match by edge id, witness membership shown.
- Keep FuSION 1 behaviour: All four cases, matched by (source, target, subtype).

**Answer:** Fix as proposed (Recommended), written into S6.15 and stage 6's open question 4

## 079 · Poll · 2026-11-11 23:00

The fifth stage 6 item poll.

**Q1 (S6.16).** S6.16 · pex node colours. Fill follows the observed expression class: up #8CCB4E (green), down #F2A24C (orange), other #DEDEDE (grey). The source has a dark-green border and the target a dark-blue one, both 4 px wide, and relaxed nodes get a dotted border; the hover shows symbol, rep id, lfc, class and flags. Problems: the docstring and RUNNING.md say 'red/blue for up/down', which is wrong, since the code draws green/orange; the legend's dash patterns (6,4 and 2,3) and 3 px rings differ from what is drawn ([8,6], [2,6], 4 px); the fill shows the observed class, not the solved activity, which only the hover shows (more visible now that S6.06 draws inactive nodes); and psp uses another scheme (S6.26). The draft proposes porting the drawn colours and treating the docs as the error.

- Keep, legend fixed (Recommended): Port fill and borders, make the legend match the drawing, correct the docs; how solved activity is shown visually goes to decision 0022's design.
- Fix: fill by activity: Fill shows the solved activity, with the observed class shown another way (e.g. ring or badge).
- Keep FuSION 1 behaviour: Colours and legend exactly as now.

**Answer:** Keep, legend fixed (Recommended), written into S6.16

**Q2 (S6.17).** S6.17 · pex's HTML page. One page per case and point, <prefix>_<incl|excl>_n<n>_e<e>.html, built by pyvis and then patched with regular expressions (removing pyvis's header and Bootstrap, adding responsive CSS, the title, subtitle, legend, table and a fit-on-load script). The title is 'Explanation subgraph (with FSC|without FSC) : <src> → <tgt> — PO n nodes, e edges relaxed', and the subtitle is always 'Showing one representative solution (of several) found at this PO point.' Problems: every incl page says 'with FSC', including the single-solve runs r7_epha2_curated and r8_phlda1_curated, which have no FSC; the title names source and target by rep id, not symbol; '(of several)' is hard-coded even when the witness is unique; and the regex patching ties the page to pyvis 0.3.2's exact markup. The draft proposes a title built from the run's real case and from symbols, 'of several' only when known, and the page generated directly instead of patching a library template.

- Fix as proposed (Recommended): Correct case and symbols in the title, honest subtitle, page generated directly.
- Keep FuSION 1 behaviour: pyvis page patched by regular expressions, fixed wording.

**Answer:** Fix as proposed (Recommended), written into S6.17

**Q3 (S6.18).** S6.18 · Every pex page carries its own copy of vis.js. Pages inline vis-network's JavaScript and CSS so each works offline as one file. On 2026-11-11 runs/ held 238 pex pages, 174.5 MB, each over 700 KB; in the reference run's 707 KB page about 97% is the library and about 7 KB is the graph. The public site holds 37 such pages, 27 MB. The draft proposes shipping the library once per output directory or site, or serving it from the 2.0 frontend, keeping pages usable offline.

- Fix as proposed (Recommended): One copy of the library per output directory or site (or served by the frontend); pages stay usable offline.
- Keep FuSION 1 behaviour: Self-contained pages, each with its own library copy.

**Answer:** Fix as proposed (Recommended), written into S6.18

**Q4 (S6.19).** S6.19 · pex site mode. With --site-dir D, pages go to D/<run-key>/ (prefix forced to pex, --out-prefix silently ignored); pex writes a run index page (title, the Pareto PNG, each case's PO points linked to their pages) and manifest.json, then rebuilds D/index.html as a landing page with one card per run (thumbnail, title, takeaway). No batch uses it: the three-run public site was built by running the script directly with --run-title and --takeaway, which a batch can't set. The manifest file is never explicitly closed. The draft proposes keeping the static report (run page, manifest, landing page) with every option settable from a batch; whether the 2.0 frontend replaces it is for decision 0022's design.

- Fix as proposed (Recommended): Keep the static report, every option settable from a batch; frontend role decided under 0022.
- Drop site mode: Leave publishing to the 2.0 frontend.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S6.19

## 080 · Poll · 2026-11-11 23:01

The sixth stage 6 item poll.

**Q1 (S6.20).** S6.20 · pex's messages and exit status. Each page prints 'plot_explanation: <case> PO (n,e) -> <file> (<k> nodes, <m> edges)', then a total line. If nothing is drawn, the script prints 'nothing rendered' and exits 1, and the dispatcher prints 'plot_explanation: renderer returned 256 (is python3 with pyvis installed? script: …)'. Problems: the dispatcher prints the raw wait status (256 for exit 1, 512 for an argparse error) with the same pyvis hint whatever the cause; a run where one case is skipped still exits 0; the batch carries on regardless. The draft proposes reporting the actual cause, and that a failure fails the command, as S1.28 decided.

- Fix as proposed (Recommended): Real cause reported; failures fail the command.
- Keep FuSION 1 behaviour: Raw status and one generic hint.

**Answer:** Fix as proposed (Recommended), written into S6.20

**Q2 (S6.21).** S6.21 · A solve without PO points leaves the renderers a missing or stale file. When pathz3 finds no PO point it truncates the .dat file but neither creates nor truncates the solutions and relaxations files. In the reference run, plot_po.sh then labels the without-FSC curve 'no solution within bounds' and pex skips that case. If an earlier run left a temp_wo_solutions.txt in the same directory, pex and psp draw that old file as current while the Pareto plot says 'no solution', with no warning; and 'file missing' looks the same as 'no solution'. The draft proposes that stage 5 always writes each case's results, including an explicit 'no PO point' result, and the renderers report that result as such. S5.26 already decided the solution files are always written.

- Fix as proposed (Recommended): Explicit 'no PO point' result, reported by the renderers as such.
- Keep FuSION 1 behaviour: Missing or stale files as now.

**Answer:** Fix as proposed (Recommended), written into S6.21

**Q3 (S6.22).** S6.22 · plot_po.sh's arguments and how batches call it. It takes 12 positional arguments under set -eu (WITH.dat, WITHOUT.dat, OUT.png, SRC, TGT, FSC, FSC_ID, BOUND, COEXP, FROZEN, NODE_UB, EDGE_UB). 22 batch files call it through 'exec cd $WRITE_DIR; bash /home/ks-149-u24/…/plot_po.sh …', all with bounds 8 and 8, with source, target and FSC labels typed by hand. Problems: an absolute path into one user's checkout; hand-typed labels can drift from the ids; a variable with an empty value vanishes and shifts every later argument; only two curves can be drawn, so single-solve runs get no plot; 'mktemp --suffix' is GNU-only (open question 1); the exit status is ignored. The draft proposes a built-in Pareto plot step that takes the run's own results, ids and symbols and draws one or two curves. Where and with what the plot is drawn is already deferred to Phase 2; your standing preference for PO curves is gnuplot, never matplotlib.

- Fix as proposed (Recommended): Built-in plot step from the run's own data, one or two curves; tool chosen in Phase 2.
- Fix, keep plot_po.sh: Keep the script, called by the binary with the run's own data and labels instead of from batches.
- Keep FuSION 1 behaviour: Batches call the script by absolute path.

**Answer:** Fix as proposed (Recommended), written into S6.22

**Q4 (S6.23).** S6.23 · The Pareto plot's look (plot_po.sh). A pngcairo image, 980×690, Liberation Sans 13 on #fcfcfb, titled '<SRC> → <TGT> · Pareto frontier', with a subtitle giving FSC, bound, coexpression and frozen thresholds and the two relaxation limits. Axes are 'Node relaxations' and 'Edge relaxations' over [−0.5, UB + 0.5] with integer tics, a light grid and left/bottom borders. The with-FSC curve is blue #2a78d6 with circles, the without-FSC curve red #e34948 with squares, each marker on a background-coloured halo; an empty curve gets a key entry 'no solution within bounds'. Problems: the subtitle prints the coexpression and frozen thresholds even when the run didn't use them, and a missing Liberation Sans is silently replaced. The draft proposes porting the look as the 2.0 Pareto plot.

- Keep, subtitle fixed (Recommended): Port the look; the subtitle lists only constraints the run used (S5.03 records them) and the font is checked with the other dependencies (S6.11).
- Keep FuSION 1 behaviour: Port the look exactly, subtitle included.

**Answer:** Keep, subtitle fixed (Recommended), written into S6.23

## 081 · Poll · 2026-11-11 23:01

The last stage 6 item poll.

**Q1 (S6.24).** S6.24 · Points present on both Pareto curves are moved apart. plot_po.sh's two awk passes shift every point found in both .dat files by +0.10 in y on the with-FSC curve and −0.10 on the without-FSC curve, so overlapping frontiers both stay visible; on the FOSB run, whose curves are identical, the PNG shows two parallel lines with touching markers. The offset is in data units, so on screen it shrinks as the edge bound grows: it is tuned for 8, the value all 22 calls pass, and with a larger bound the markers overlap again. The draft proposes keeping the offset but sizing it from the axis range, so the markers touch at any bound.

- Fix as proposed (Recommended): Offset scaled to the axis range.
- Keep FuSION 1 behaviour: Fixed ±0.10 data-unit offset.

**Answer:** Fix as proposed (Recommended), written into S6.24

**Q2 (S6.25).** S6.25 · Inline gnuplot lines in batch files and GUI templates. 62 of the 89 batch files carry 130 inline 'exec … gnuplot -p -e "…"' lines: 56 open an on-screen window, 60 write $PLOT_FILE as a 900×650 PNG, 12 write $PLOT_FILE_WO_GENE (broken by S1.07), 2 write pareto.png. Both GUIs write only a window line and a PNG line, never psp, pex or plot_po.sh. pathz3's own gnuplot call has been commented out since 2019. Three plot styles coexist, the window lines need a display (pop-ups under the GUI, failure when headless), and nothing checks their status. The draft proposes dropping them: the 2.0 GUI emits the built-in plot step (S6.22), and old batch files keep working because exec passes these lines to the shell (S1.14).

- Drop (Recommended): 2.0 generates only the built-in plot step; old inline lines still run through exec.
- Keep FuSION 1 behaviour: The 2.0 GUI keeps writing inline gnuplot lines.

**Answer:** Drop (Recommended), written into S6.25

**Q3 (S6.26).** S6.26 · Two renderers that disagree about one solution. psp draws a path filter over the both-active edges, one edge per subtype, source green and target red, no relaxations, as a static SVG plus CSV via Graphviz; no batch calls it. pex draws the whole both-active subgraph plus the relaxed overlay, one edge per effect, nodes filled by expression class, as interactive HTML via Python and pyvis; 24 batches call it. In the reference run at (1,0) psp would draw 10 edges and pex 8 for the same witness. Each carries its own copy of the dump reader and the polarity rule. The draft proposes one explanation renderer in 2.0, offering psp's path filter (S6.04) and edge-source table (S6.08) as options of pex's view, with a static export (S6.07); how it is built is for decision 0022's graph-display design.

- Fix as proposed (Recommended): One renderer; psp's path filter and edge table become options of the pex view.
- Keep both renderers: psp and pex stay separate commands, sharing one reader and one polarity rule.

**Answer:** Fix as proposed (Recommended), written into S6.26. All 26 stage 6 items are now decided.

## 082 · Poll · 2026-11-11 23:02

The stage 6 commit poll.

**Q1 (Commit).** Stage 6 (results and plots) is fully decided: 26 items, 20 fix / 5 keep / 1 drop. May I commit it locally as one commit? It would contain the new docs/spec/06-results-and-plots.md (decisions, the verifier's corrections, my S6.14 citation fix, S6.03's corrected premise, and its open questions closed or deferred) and prompt log entries 074 to 081. Message: 'Review spec stage 6: results and plots', ending with the Co-Authored-By line. The stage 7 and 8 drafts stay out.

- Commit as shown (Recommended): One local commit with those two files; nothing is pushed.
- Change something first: Tell me what to change before committing.

**Answer:** Commit as shown (Recommended), committed as ba62454

## 083 · Poll · 2026-11-11 23:05

The first stage 7 item poll. Before it, the stage 7 verifier's corrections were applied to the draft:

- S7.08: at least five of the 42 fdr:0.25 pathways pass mostly on genes outside their relations, not two (Viral carcinogenesis, Transcriptional misregulation in cancer and Malaria added);
- S7.05: the stable-sort variant changes 158 ES values, the AVX2 and SSE variants 159;
- S7.01 and S7.15: 17 of the 19 batches run `gsea` on the line before `mff`; in r7 and r8 an `exec echo` line sits between them;
- S7.07: `updated_pathways/xml_files` holds 273 KGMLs, 163 of them shared with the other directory;
- wording: `pip3` in the macOS README (S7.02); the stale-list case prints the script's and C++'s errors, only `mff` is silent (S7.03); gseapy's NaN warning added to the stderr list (S7.17).

Claude checked the 273 count, the README line and the five pathways' presence in the fdr:0.25 shortlist before applying them.

**Q1 (S7.01).** S7.01 · gsea command line: 'gsea <lfc_file> <kgml_list> <selector> <out_list> [<keep_ids>]', long name gsea_select_pathways. C++ checks none of the values and hands them to the Python script; the step works before start, and every batch runs it right before mff (17 of 19 on the next line). The argument count is never checked: with the selector missing the script exits 2; with the shortlist path missing, the whole analysis runs, prints its selection and then dies with FileNotFoundError; extra tokens are ignored. The draft proposes keeping the four arguments, the optional keep list and both names (positional, and by name under decision 0018), and reporting a wrong argument count before the analysis runs.

- Fix as proposed (Recommended): Same arguments and names, count checked before the analysis.
- Keep FuSION 1 behaviour: Unchecked arguments.

**Answer:** Fix as proposed (Recommended), written into S7.01

**Q2 (S7.02).** S7.02 · Running the GSEA script. C++ builds a 'python3 "…/gsea_select_pathways.py" …' string and runs it with system(), so the arguments pass through sh in double quotes ($, backquote, " or \ in a path get interpreted). python3 is whichever is first on PATH: the sweep launchers put the repo's .venv first (Python 3.12.3, gseapy 1.3.1, pandas 3.0.1, numpy 2.4.3), and that .venv's activate script points at a path that no longer exists. The READMEs say 'pip install pyvis gseapy' with no versions, and there is no requirements file. Results depend on the versions: the tie order comes from numpy and the CPU (S7.05), and under pandas 3 an inf value crashes gseapy (S7.04). The call uses 1000 permutations, sizes 15 to 500, seed 42, weight 1 and 4 threads. The draft proposes that whatever runs GSEA in 2.0 pins its library versions, records them with each result, and gets its arguments without a shell; the helper's language is a Phase 2 choice (S1.20).

- Fix as proposed (Recommended): Pinned and recorded versions, no shell; implementation language chosen in Phase 2.
- Keep FuSION 1 behaviour: Unpinned python3 from PATH through a shell string.

**Answer:** Fix as proposed (Recommended), written into S7.02

**Q3 (S7.03).** S7.03 · GSEA failure and what mff merges afterwards. The script exits 1 when no pathway passes (writing an empty shortlist) and 2 on a bad selector; C++ then prints 'no shortlist produced, script returned <wait status> (is python3 with gseapy installed? …)' for every cause and carries on, and the next line runs mff on the list path. So a mistyped selector in a re-run merges the previous run's stale shortlist and the run ends with status 0; with no file or an empty one, mff merges nothing and rgx then reads any merged graph an earlier run left behind. No real run hit this. The draft proposes that a script failure or an empty selection fails the gsea step, which stops the batch before mff (S1.28), with the exit code and the script's own reason. For an empty corridor, S4.18 decided the opposite: a reported result, not a failure, so a bound sweep isn't halted. The same question arises for an FDR sweep, where a strict cut-off can select nothing.

- Fix, empty = reported result (Recommended): A script failure stops the batch; an empty selection ends that run as a reported result ('no pathway passed'), like S4.18's empty corridor, so a sweep carries on.
- Fix as drafted: A script failure or an empty selection both stop the batch before mff.
- Keep FuSION 1 behaviour: Carry on into mff with whatever list is there.

**Answer:** Fix, empty = reported result (Recommended), written into S7.03

**Q4 (S7.04).** S7.04 · The LFC file as gsea reads it. The script splits each line on whitespace, takes the id and float() of the second token, silently skips lines that don't parse (NA, '1,5', a header), accepts nan, inf, exponents and '1_000', and lets a repeated id keep its last value. gseapy then drops NaN with a warning, crashes on inf under pandas 3 (TypeError), and fails on a one-line file; ids not in 'hsa1956' form match no gene set, and if none match the step ends in a LookupError traceback. So gsea, cudf and pex read the same file by different rules, and S3.14's table wrongly says gsea keeps nan and inf. S3.14 already decided on one LFC parser for every command. The draft proposes reading the file with that one parser, so gsea ranks exactly the values cudf classifies, with the reader reporting a non-finite value instead of letting it crash the GSEA library.

- Fix as proposed (Recommended): S3.14's single parser; non-finite values reported by the reader.
- Keep FuSION 1 behaviour: gsea's own parser and gseapy's handling.

**Answer:** Fix as proposed (Recommended), written into S7.04

## 084 · Poll · 2026-11-11 23:09

The second stage 7 item poll.

**Q1 (S7.05).** S7.05 · Tied values are ordered by numpy's unstable sort, so the shortlist depends on the CPU. The EGF file's 3054 genes have only 190 distinct values (two decimals), so 2994 genes are tied, with blocks of 145 at 0.00 and 71 at −0.06, and gseapy warns about it in every run. numpy picks its sort code by CPU features, so the same inputs and library versions give different tie orders and results: fdr:0.25 selects 42 pathways with AVX-512 (the real runs) but 44 with AVX2 only, SSE, or a stable sort; at fdr:0.4 the counts range from 71 to 80, and NES moves by up to 1.37. All 19 real tables match only because they ran on the same Xeon; Apple silicon was not tested. The draft proposes ordering tied values by gene id, ascending, so every machine gives the same ranking, scores and shortlist. That changes the real runs' shortlists (fdr:0.25 gives 44 instead of 42), which decision 0023 allows.

- Fix as proposed (Recommended): Ties broken by gene id, ascending: the same result everywhere, independent of file order.
- Fix: ties in file order: A stable sort keeps tied genes in the LFC file's order (here the same as gene id order, but it would differ for files ordered otherwise).
- Keep FuSION 1 behaviour: Whatever order numpy's sort leaves.

**Answer:** Fix as proposed (Recommended), written into S7.05

**Q2 (S7.06).** S7.06 · The candidate list file (one KGML path per line). gsea trims each line, drops a CRLF's \r, skips blank lines, and skips paths that don't exist with one warning naming the first; there is no comment syntax. mff reads the same format but keeps the \r and aborts on a blank line or a bad path (S2.16). So gsea quietly cleans a list mff would reject, and a candidate whose file is missing just drops out of the shortlist with a warning. All 19 batches pass the same 167-path list. S2.16 already decided that mff checks every listed path before the run starts (decision 0021). The draft proposes reading candidate lists with that same reader and checks, so a missing candidate stops the run instead of silently leaving the shortlist.

- Fix as proposed (Recommended): One list reader shared with mff; a missing candidate stops the run before it starts.
- Keep FuSION 1 behaviour: gsea's lenient reader, missing files skipped with a warning.

**Answer:** Fix as proposed (Recommended), written into S7.06

**Q3 (S7.07).** S7.07 · Candidates that share a pathway id collapse into one. A candidate's id is its KGML root name without 'path:' (or the file path if that's missing), and candidates are kept in a dict keyed by that id, so a second file with the same id silently replaces the first: a list naming the same five files twice gives five tested pathways and a five-line shortlist, where mff would have read ten files. The two KGML directories share 163 pathways with the same names, and two of those pairs have different gene sets (hsa04080, hsa04141), so a list drawing on both would silently lose one version. No real list does this. The draft proposes stopping with an error that names both files when two candidates share a pathway id.

- Fix as proposed (Recommended): Duplicate pathway ids are an error naming both files.
- Fix: warn and keep the first: Warn, naming both files, and test only the first.
- Keep FuSION 1 behaviour: The later file silently wins.

**Answer:** Fix as proposed (Recommended), written into S7.07

**Q4 (S7.08).** S7.08 · Gene sets come from every gene entry of each KGML. A candidate's set is every name of every gene entry in its KGML (colon removed, repeats dropped); relations play no part, so a gene with no edge in the pathway still counts. This makes every tested set exactly one file mff can merge. But GSEA scores membership while mff merges relations: across the 167 candidates, 3301 of 21,205 memberships (16%) are genes on no relation of their pathway, and at least five of the 42 fdr:0.25 pathways pass mostly on such genes (e.g. Viral carcinogenesis: 6 of its 205 genes are on its 3 relations; Systemic lupus erythematosus: 21 of 143 on 7). Ids are KEGG's on both sides (decision 0020). The draft proposes keeping the method; how other pathway sources supply gene sets is for Phase 2.

- Keep, show relation share (Recommended): Same gene sets, plus a table column giving each pathway's share of genes that sit on its relations, so cases like Viral carcinogenesis are visible.
- Keep as drafted: Same gene sets, no extra column.
- Fix: relation genes only: Score only genes on at least one relation of their pathway (a change of method; shortlists change).

**Answer:** Keep, show relation share (Recommended), written into S7.08

## 085 · Poll · 2026-11-11 23:10

The third stage 7 item poll.

**Q1 (S7.09).** S7.09 · Size filter. gseapy tests a set only when 15 to 500 of its genes are in the ranking (constants in the script; 15 is GSEA's default). On the real inputs 166 of 167 candidates are tested; Sulfur relay system has only 6 ranked genes. An untested pathway gets no scores, is never selected, and has a table row only if the keep list chose it, so otherwise only the count line shows it existed. If no candidate is in range (e.g. ids in the wrong form), gseapy raises LookupError and the step ends in an uncaught traceback. The draft proposes keeping 15 to 500, giving every candidate a table row (an untested one with its ranked-gene count and no scores), and reporting 'no candidate has 15 to 500 ranked genes' as a gsea failure instead of a traceback; that is an input problem, unlike S7.03's empty selection.

- Fix as proposed (Recommended): 15 to 500 kept, a row for every candidate, no-candidate case a clean failure.
- Keep FuSION 1 behaviour: Untested candidates leave no row; traceback when none fits.

**Answer:** Fix as proposed (Recommended), written into S7.09

**Q2 (S7.10).** S7.10 · Enrichment score, leading edge, Tag % and Gene %. This is the classic weighted GSEA running sum (weight 1): hits add |r| over the set's total |r|, misses subtract 1/(N − m); ES is the running sum's extreme, the leading edge is the set's genes up to that rank (from the bottom for negative ES), Tag % is leading-edge size over set size, Gene % is the rank fraction. Recomputing from gseapy's ranking reproduces all 166 real ES values to within 9×10⁻¹⁶ and every leading edge, Tag % and Gene % exactly (e.g. Bladder cancer: ES 0.747, Tag % 10/39, Gene % 6.12%). No problems were found in the formula; the tie order that feeds it is S7.05. The draft proposes keeping it.

- Keep (Recommended): Same formula and columns.
- Fix: unweighted (weight 0): Classic Kolmogorov–Smirnov form, ignoring the LFC magnitudes (a change of method; results change).

**Answer:** Keep (Recommended), written into S7.10

**Q3 (S7.11).** S7.11 · Significance. gseapy shuffles gene labels 1000 times with seed 42 to give each set 1000 null scores, from which it derives NES, nominal p, the FDR q-value (pooled over all tested sets, S7.12) and FWER p. Thread count and candidate order don't change the numbers. The fixed seed hides Monte Carlo noise: with seeds 1 to 20, fdr:0.25 selects 41 to 46 pathways (38 always, 8 sometimes; ErbB signaling passes with 4 of 20 seeds), and 10,000 permutations (11.6 s instead of 1.3 s) select 43. Matching FuSION 1's numbers exactly needs gseapy's own random stream, so a reimplementation would agree statistically but not byte for byte, and pathways near a cut-off could change. The draft proposes porting the procedure with seed 42 and 1000 permutations, leaving the noise for you to weigh.

- Keep, as run settings (Recommended): Defaults seed 42 and 1000 permutations, both settable and recorded in the output, like S1.29's other result-changing settings.
- Keep as drafted: Seed 42 and 1000 permutations, fixed.
- Fix: 10,000 by default: Less Monte Carlo noise at about 10 s more per call.

**Answer:** Keep, as run settings (Recommended), written into S7.11

**Q4 (S7.12).** S7.12 · A pathway's FDR depends on the other candidates. All candidates go into one GSEA call, and the q-value pools every tested set's null scores and NES (gseapy documents this). ES, NES and nominal p depend only on a pathway's own genes, but its FDR moves with what else was offered: dropping the four updated_pathways KGMLs changes 154 of the remaining 162 FDRs (by at most 0.0071), and GnRH signaling and Type II diabetes then pass fdr:0.4 although neither is related to the four; GSEA on just the 42 fdr:0.25 pathways gives all 42 an FDR ≤ 0.068. So a cut-off means different things for different candidate lists. The draft proposes keeping this, since it is GSEA's definition of the q-value and the table lists every tested candidate.

- Keep (Recommended): GSEA's pooled q-value; read each result against its own candidate list.
- Fix: fixed background: Always compute FDR over one fixed reference set of pathways, then restrict to the candidates, so cut-offs are comparable across lists (a change of method).

**Answer:** Keep (Recommended), written into S7.12

## 086 · Poll · 2026-11-11 23:12

The fourth stage 7 item poll.

**Q1 (S7.13).** S7.13 · The selector: fdr:Q, pval:P, nes:X, top:N, comma-joined. Terms are applied in turn to the tested pathways sorted by |NES| (fdr keeps FDR ≤ Q, pval keeps nominal p ≤ P, nes keeps |NES| ≥ X, top keeps the first N); an unknown key or unparsable value exits 2. All 19 batches use a single fdr: term (0.25, 0.3, 0.4, 0.44, 0.5). Problems: values aren't range-checked (top:-5 keeps 161 of 166, top:0 and fdr:nan keep none, nes:-1 and fdr:inf keep all, fdr:0_5 reads as 5.0 and keeps all); term order matters once top: bites (top:10,fdr:0.06 keeps 7, fdr:0.06,top:10 keeps 8); and FDR isn't monotone in |NES|, so fdr: and nes: cut-offs aren't interchangeable. The draft proposes keeping the four keys and the in-turn rule, accepting only finite, in-range values (fdr and pval 0 to 1, nes ≥ 0, top a positive integer), with an error naming any bad term.

- Fix, order-independent (Recommended): As proposed, but the thresholds always apply before top:N, so term order never matters. No real batch combines terms.
- Fix as drafted: Range-checked values, terms applied in the order written.
- Keep FuSION 1 behaviour: Unchecked values, in-turn rule.

**Answer:** Fix, order-independent (Recommended), written into S7.13

**Q2 (S7.14).** S7.14 · The keep list. An optional comma-separated list of gene ids: every candidate whose gene set (all its KGML gene ids, ranked or not) contains one is chosen whatever its GSEA result, even if it was too small or large to test. Ids must match exactly in 'hsa1956' form; an id in no candidate gets a warning. The console adds a 'plus <n> kept' line and the table a kept_for column. Three batches use it with source, target and FSC: in the keep run, fdr:0.4 picks 71 and the keep list adds 16, for 87. It exists because GSEA scores transcription, so the pathway the stimulus signals through can score low (ErbB signaling, FDR 0.259, misses fdr:0.25). The only problem: an id in the wrong form (hsa:1956, EGFR) just warns and the intended pathways are silently missing; the same ids drive fb_rch and pathz3, which would fail on them too. The draft proposes keeping it.

- Keep (Recommended): Same keep rule and outputs; id forms handled by Phase 2's id mapping (S2.14).
- Fix: unmatched id is an error: A keep id in no candidate stops the run (a valid id in no KGML, like PHLDA1, would then need removing).

**Answer:** Keep (Recommended), written into S7.14

**Q3 (S7.15).** S7.15 · The shortlist file. Written only after GSEA succeeds: one KGML path per chosen pathway, exactly as the candidate list gave it, in candidate-list order, no header, empty when nothing is chosen. Every batch passes it to mff. Users also edit and reuse it directly: r7 and r8 append a curated non-KEGG KGML to it, and a batch written today reruns from r5's saved list instead of running gsea again. No problems were found in the format; the hand-over to mff is a file round trip, which decision 0019 keeps anyway as a saveable step output. The draft proposes keeping it.

- Keep (Recommended): Same plain list of paths, usable and editable between steps.
- Fix: add a header: Record the selector, LFC file and candidate list in comment lines at the top (mff would need to skip them).

**Answer:** Keep (Recommended), written into S7.15

**Q4 (S7.16).** S7.16 · The results table <stem>.gsea.tsv. Written next to the shortlist (run/gsea_kgml_files.txt gives run/gsea_kgml_files.gsea.tsv), tab-separated with a header: Term, pathway_name, genes, ES, NES, NOM p-val, FDR q-val, FWER p-val, Tag %, Gene %, Lead_genes, selected, kept_for, kgml. Rows are every tested pathway by |NES|, then untested chosen ones with empty scores. Problems: nothing reads it back, although the docstring says it is for picking a different threshold without re-running; candidates neither tested nor kept have no row (fixed by S7.09); 'genes' counts parsed ids, including NaN genes gseapy dropped (fixed by S7.04). Your S7.08 answer adds a relation-share column. The draft proposes keeping the table.

- Keep (Recommended): Same table, with S7.04's, S7.08's and S7.09's changes.
- Fix: re-select from a saved table: Also let a run apply a new selector to a saved table without re-running GSEA (about 3 s saved per call).

**Answer:** Keep (Recommended), written into S7.16

## 087 · Poll · 2026-11-11 23:14

The last stage 7 item poll.

**Q1 (S7.17).** S7.17 · gsea's console report. stderr carries gseapy's tie warning (with a timestamp), its NaN warning when the file has NaN values, and the script's warnings (missing KGMLs, unknown keep ids, empty selection). stdout gives the LFC file, '3054 genes ranked by log-fold change, 1000 permutations', '166 of 167 candidate pathways tested', the NES range, 'selected 42 of 166 tested pathways (fdr:0.25)', a table of the chosen pathways (number, id, name, genes, ES, NES, nominal p, FDR) with a kept-for column when there is a keep list, and the shortlist and table paths. The only problem: the gene count includes NaN genes that gseapy then drops (3055 reported for 3054 ranked), which S7.04's fix removes. The draft proposes keeping the report.

- Keep (Recommended): Same report; the count is right once S7.04's single parser is in place.
- Fix: shorter report: Print only the summary lines and point to the table, without the per-pathway rows.

**Answer:** Keep (Recommended), written into S7.17

**Q2 (S7.18).** S7.18 · The review pause, which you added on 2026-09-28. After the GSEA script returns, whether or not it succeeded, FuSION checks whether stdin is a terminal; if so it prints 'Review the pathways selected above. Press Enter to merge them, Ctrl-C to abort: ' and waits. Without a terminal (nohup, &, < /dev/null) there's no pause, and both sweep launchers redirect stdin, so no real run has ever paused. Problems: it also pauses after a failure, offering to merge nothing; a batch started in a terminal without redirection blocks there; Ctrl-C while the script runs kills only Python, and the batch carries on. The draft proposes dropping it: decision 0019 lets a run stop after gsea, so you can read the table and restart from the saved shortlist; S1.03 already dropped interactive input. The 2.0 frontend could also show the shortlist for approval before the merge (stage 8).

- Drop (Recommended): Review by stopping after gsea (decision 0019) or in the frontend; no terminal prompt in the batch.
- Keep, opt-in: Pause only when the batch asks for it, still only on a terminal, and never after a failure.
- Keep FuSION 1 behaviour: Pause whenever stdin is a terminal.

**Answer:** Drop (Recommended), written into S7.18

**Q3 (S7.19).** S7.19 · Cost of the gsea step. One call takes 3.3 s wall and 13.4 s CPU with a 318 MB peak: 1.7 s to start Python and import gseapy and pandas, 0.24 s to parse 167 KGMLs, 1.3 s for GSEA on 4 threads. The shortlist pays for itself: the CHANGELOG measured a b7 EGFR→CXCL8 batch at 7 s with the fdr:0.25 shortlist against 3 min 14 s with all 163 KGMLs. Every run recomputes the same analysis (19 batches, identical score columns, about a minute in all), and the candidate KGMLs are parsed once in Python and the chosen ones again by mff in C++. The draft proposes keeping the step as it is: it's cheap, and starting from a saved shortlist (decision 0019) removes the repeats.

- Keep (Recommended): Cheap enough; saved shortlists (decision 0019) avoid repeats.
- Fix: parse KGMLs once: Share one parse of the candidate KGMLs between gsea and mff (depends on Phase 2's choice of GSEA language).

**Answer:** Keep (Recommended), written into S7.19. All 19 stage 7 items are now decided.

## 088 · Poll · 2026-11-11 23:25

The stage 7 commit poll.

**Q1 (Commit).** Stage 7 (GSEA shortlisting) is fully decided: 19 items, 9 fix / 9 keep / 1 drop. May I commit it locally as one commit? It would contain the new docs/spec/07-gsea-shortlisting.md (decisions, the verifier's corrections, open questions marked) and prompt log entries 082 to 087. Message: 'Review spec stage 7: GSEA shortlisting', ending with the Co-Authored-By line. The stage 8 draft stays out.

- Commit as shown (Recommended): One local commit with those two files; nothing is pushed.
- Change something first: Tell me what to change before committing.

**Answer:** Commit as shown (Recommended), committed as 4192226

## 089 · Poll · 2026-11-12 17:15

The first stage 8 poll: three framing questions and S8.01. Before it, the stage 8 verifier's corrections were applied to the draft, after Claude checked each against the snapshot and the run data:

- S8.08: `Linux/output_script.txt` is a list-mode script (lines 149, 157, 164 and 167 active, lines 61-62 empty);
- S8.11: one batch (`r5_mcl1_stat3/resolve_wo_n7_e0`) sets a node relaxation lower bound of 7, and the stepper field still shows a stored value above 50;
- S8.27: 22 batches call `plot_po.sh`, not 24;
- S8.23: GUI sessions use split threshold 15000, and the GUI run wrote a 13,375,038-byte merged graph, not about 2 MB;
- open question 2: `runs/egf_hela_hsa1956_cxcl8_b6/` appears to be the GUI run of 2026-07-09;
- S8.03 and S8.16: Save on the edge-files page never writes the session file;
- minor: S8.05 (`optInt`), S8.04 (field values and the `new File(null)` NPE), S8.01 (`gui4.java:4004`), S8.28 (the two run windows differ slightly), S8.15 (an arbitrary `HashSet` element), S8.32 (an empty `README.md`), S8.02 (which fields the empty check covers).

**Q1 (macOS GUI).** Stage 8 scope, open question 1: macOS/frontend is not a copy of Linux/frontend, as the drafting brief assumed. Its ScriptFile.java fills the three coexpression placeholders and its ReachPathBoundCard.java commits a typed bound (both fix Linux bugs, S8.17 and S8.09); its README differs; and macOS/advtempscript.txt comments out the additional-edges lines and adds '*DISPLAY*' progress echoes no in-scope GUI reads. These came in your macOS commits of 2026-07-10, after the Linux GUI's last change. Decision 0014 covers Linux/frontend and frontend1 only. How should stage 8 treat the macOS GUI?

- Note its fixes only (Recommended): Keep decision 0014's scope; the items already cite macOS/frontend where it fixes a Linux problem, as stage 5 does for the macOS backend.
- Cover it fully: Amend decision 0014 so stage 8 describes macOS/frontend and its template as a third version (another draft-and-verify pass).

**Answer:** Note its fixes only (Recommended), written into stage 8's open question 1

**Q2 (Sessions).** Stage 8, open question 2: must 2.0 import FuSION 1's GUI sessions? There are three session files (arjun.json, arjun.j.json, arpit.json) under Linux/frontend/sessions/; 27 of their 28 absolute paths no longer exist; the only stored pipeline is empty. The GUI's one recorded run appears to be runs/egf_hela_hsa1956_cxcl8_b6/ (2026-07-09). No batch file under runs/ comes from the GUI's template, and old batch files keep running in 2.0 anyway (decisions 0017, 0023).

- No import (Recommended): 2.0 starts with fresh sessions; the three old files stay in FuSION 1.
- One-time importer: 2.0 can read a FuSION 1 session file once, flagging the stale paths for the user to fix.

**Answer:** No import (Recommended), written into stage 8's open question 2

**Q3 (S1.08).** A clash with stage 1's S1.08. You decided that extra tokens after a let value are an error, on the basis that no real file has any. But the GUI stores ids as display text ('hsa1956 - egfr') and its template writes 'let $SRC_NODE hsa1956 - egfr', relying on let keeping only the first token; its 2026-07-09 run did exactly that. No such script survives, because output_script.txt is overwritten each run and the two tracked GUI scripts have bare ids, and S8.12 proposes that 2.0's frontend store and write bare ids. Should S1.08 stand?

- Keep S1.08 (Recommended): An old GUI-style line now stops with an error naming the key; none is saved, and 2.0's frontend writes bare ids.
- Amend S1.08: warn: Extra tokens give a warning and the first token is used, so old GUI-generated scripts would still run.

**Answer:** Keep S1.08 (Recommended). S1.08 stands; a note goes into stage 1 with the cross-stage corrections.

**Q4 (S8.01).** S8.01 · Build, launch and the launch directory. The README builds the GUI with javac and starts it with java from Linux/ (JDK 21 asked; the code needs Java 16+; org.json is the only library). Every file the GUI touches (accounts, sessions, the template, the generated script, ./fusion, the editor's files) is a path relative to the JVM's working directory, which must be Linux/, and the backend inherits that directory, so its fixed-name outputs and SMT dumps land in Linux/. Started anywhere else, the GUI has no accounts and can't find the template or ./fusion, with no message. Linux/README.md's install guide tells users to run the out-of-scope GUIForExecutable instead, and 67 stale compiled classes are tracked in git. The draft proposes that 2.0's frontend finds the backend and its data through configuration, not its working directory, and that no compiled classes are kept in the repository.

- Fix as proposed (Recommended): Configured locations, no reliance on the working directory, no compiled classes in git.
- Keep FuSION 1 behaviour: Must be launched from Linux/.

**Answer:** Fix as proposed (Recommended), written into S8.01

## 090 · Poll · 2026-11-12 17:17

The second stage 8 poll.

**Q1 (S8.02).** S8.02 · Login, sign-up and fusion_users.dat. LoginPage is the GUI's entry point: sign-up takes username, email and password, creates the user's session file and rewrites the whole account map (Java-serialised) to fusion_users.dat; sign-in compares an unsalted SHA-256 hash. Problems: the hashes are unsalted, and FuSION 1's git tracks the account file with arpit's email and password hash, open to offline attack; the login guards nothing, since sessions are plain JSON anyone with file access can read or edit, and anyone can sign up; a username containing / or .. puts its session file outside the sessions folder; 'Wrong password!' also shows for an unknown user; accounts and sessions can disagree (arjun has sessions but no account). The draft proposes keeping per-user sign-in and per-user sessions, with salted, slow password hashes in a store outside the repository, validated usernames, and no committed account file.

- Fix as proposed (Recommended): Per-user accounts done properly: salted slow hashes, validated names, nothing committed.
- Drop the login: Treat 2.0's frontend as a single-user local app with no accounts; sessions belong to whoever runs it.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S8.02

**Q2 (S8.03).** S8.03 · Window frame and step navigation. Every window is an undecorated Swing frame with its own header and buttons. The configuration steps are cards in one CardLayout, in a Next order (working directory and LFC, pathway XML, path bound, relaxation bounds, node ids, thresholds, edge files, pipeline editor), each page with Prev, Next, Save and Sessions, plus a ☰ menu listing the steps in a different order. Problems: nothing is saved until Save (and Save on the edge-files page doesn't write the file); closing loses changes silently; Prev discards unsaved page values; the ☰ menu skips Next's checks, so a skipped reach-path page leaves the edge restriction at −1 and the run dies with 'Index -1 out of bounds for length 3'; Prev can do nothing on a page reached by the menu; the card stack grows on every visit. The draft proposes dropping it: Swing frames and a card stack don't carry over to a web frontend; Phase 5 designs navigation, and these problems are what it must not repeat.

- Drop (Recommended): Phase 5 designs the web navigation, avoiding the listed problems.
- Keep the step order: Drop the Swing frame, but keep the same step-by-step wizard order as the 2.0 frontend's starting point.

**Answer:** Drop (Recommended), written into S8.03

**Q3 (S8.04).** S8.04 · Sessions window. After sign-in, 'Your Sessions' lists the user's sessions as '<name> (<creation timestamp>)', with Configure, New Session, Delete, Log Out and Submit (frontend opens the pipeline editor, frontend1 runs at once). Problems: sessions are addressed by list position, so after deleting an earlier entry an open session's Save overwrites its neighbour; New Session followed by Cancel or a blank name still opens a session, and with index −1 the read fails and the next page throws a NullPointerException with nothing shown; the duplicate-name check compares the untrimmed name but stores the trimmed one; the timestamp is the creation time and never changes. The draft proposes keeping named, per-user sessions that can be created, opened, run and deleted, addressed by a stable id, with Cancel actually cancelling.

- Fix as proposed (Recommended): Same session operations, stable ids, a working Cancel.
- Keep FuSION 1 behaviour: Sessions addressed by list position.

**Answer:** Fix as proposed (Recommended), written into S8.04

**Q4 (S8.05).** S8.05 · The session file frontend/sessions/<user>.json. One JSON array per user, rewritten whole on each save, with 34 keys per session (working directory, LFC file, pathway source, split threshold, path bound and edge restriction, solver settings, relaxation bounds, node ids, mapping files, thresholds, edge files, coexpression inputs, pipeline). Problems: paths are absolute and unchecked, and 27 of the 28 stored paths no longer exist (e.g. CoexpressionCsv names the old coexpression_csv/ folder); org.json's put(key, null) deletes the key, so choosing one pathway source removes the other field entirely; threshold overrides (S8.15) aren't stored; nothing about runs is stored (output directory, time, outcome). The draft proposes one stored record per session with every input a run needs, keyed by a stable id, its paths checked when it opens, both pathway-source fields kept, and the overrides stored.

- Fix as proposed (Recommended): Complete, id-keyed session records with paths checked on opening and overrides kept.
- Keep FuSION 1 behaviour: As now.

**Answer:** Fix as proposed (Recommended), written into S8.05

## 091 · Poll · 2026-11-12 17:19

The third stage 8 poll.

**Q1 (S8.06).** S8.06 · Defaults of a new session. Until a page sets them, a session holds defaults that contradict every real batch: up/down thresholds 0 and 0 (every gene ≥ 0 is up and ≤ 0 down, a 0 in both; the batches use ±0.67, ±1.01 or ±1.45); split threshold 15000, so no multi-gene box is ever collapsed (every batch uses 1); relaxation bounds 20–80 and 20–70 (84 of 85 batches start both lower bounds at 0, with upper bounds 0 to 20, 8 the commonest); path bound 50 (the batches use 6 to 15); timeouts 60000/150000 ms (the batches use 600000/3000000 or 3600000/14400000); edge restriction −1, which is not a valid choice and crashes the script filler if the page is skipped; and mapping files relative to Linux/, where they don't exist (one with a leading space in its name). The draft proposes defaulting to what the real batches use (split threshold 1, edge restriction 0, relaxation bounds from 0, path bound 10, the shipped maps), and having the user set the LFC thresholds or accept a suggested value (suggest_thresholds, decision 0013) instead of defaulting to 0.

- Fix as proposed (Recommended): Defaults from the real batches; LFC thresholds set by the user or suggested, never 0 by default.
- Fix, all from the b10 runs: Also default the upper bounds to 8/8 and the timeouts to 3600000/14400000, as in your b10 runs.
- Keep FuSION 1 behaviour: Current defaults.

**Answer:** Fix as proposed (Recommended), written into S8.06

**Q2 (S8.07).** S8.07 · Working-directory and log-fold-change pages. 'Welcome to FuSION' shows the session's working directory with a 'Change Directory' chooser; it becomes $WORK_DIR, under which each run makes its output directory. 'Log Fold Changes File' has a 'Choose File' chooser (no file-type filter); Next stays disabled until a file is set. The file becomes $LOG_FOLD_CHANGE_FILE, and its base name names the output directory and Pareto files. Neither page checks anything: the directory may not exist or be writable, and the file may not be an LFC file (arpit.json stores /home/linux/generated_script.txt as one); after a choice the page shows only the file name, not where it is. The draft proposes keeping both inputs, since a results location and an LFC file are needed for any run, with their checks under S8.30.

- Keep (Recommended): Both inputs stay; checks come from S8.30.
- Fix: show full paths: Keep both, and also show each chosen file's full path rather than its name alone.

**Answer:** Keep (Recommended), written into S8.07

**Q3 (S8.08).** S8.08 · Pathway XML page. Two radio buttons choose the pathway source: a text file listing KGMLs to merge, or one pre-merged XML; a 'Node Split Threshold' field sets $NODE_SPLIT_THRESHOLD. The mode is stored as a comment prefix (KEGG = '' or ';;') that ScriptFile pastes onto five template lines. Problems: the pre-merged mode also comments out the additional-edges step, so a chosen additional-edges file is silently ignored; the list mode relies on '' + null, so the merged graph is written to a file named 'null' (or an empty name after a save and reopen), and it always fails at exec add (S8.24); Next requires no file in either mode; a non-numeric split threshold makes Next and Save silently do nothing. Your arjun sessions use the pre-merged mode; Linux/output_script.txt is a list-mode script. The draft proposes keeping the choice between a KGML list and a pre-merged graph, stored as an explicit choice, with the additional-edges step in both modes and the chosen file required.

- Fix as proposed (Recommended): Explicit pathway-source choice, additional edges in both modes, file required.
- Keep FuSION 1 behaviour: Comment-prefix modes as now.

**Answer:** Fix as proposed (Recommended), written into S8.08

**Q4 (S8.09).** S8.09 · Reach path bound and edges-to-target restriction. 'Maximum Signalling Path Length' is a 0 to 100 slider with a text field; the slider stores at once, but a typed value only on Enter. It becomes the bound for both fb_rch and both pathz3 lines. 'Final Edges in pathways restricted to:' offers activation only, expression only or no restriction (1, 2, 0), passed as fb_rch's last argument; Next and Save require a choice. Problems: a typed bound is silently lost unless Enter was pressed (macOS/frontend already commits it on focus loss and before Next/Save); 0 is accepted and gives an empty corridor (S4.18); the labels promise more than fb_rch did, which S4.05's fix now makes each rule match its name. The draft proposes keeping both inputs, committing a typed bound without Enter as the macOS copy does, and rejecting a bound below 1.

- Fix as proposed (Recommended): Both inputs kept; typed bound committed; bound at least 1.
- Keep FuSION 1 behaviour: Typed bound needs Enter; 0 accepted.

**Answer:** Fix as proposed (Recommended), written into S8.09

## 092 · Poll · 2026-11-12 17:21

The fourth stage 8 poll.

**Q1 (S8.10).** S8.10 · Solver configuration dialog. 'Configure' opens a modal dialog with four integer fields: Increment Solver Timeout, Overall Solver Timeout, Solutions to Count and Solutions to Explore, which become the last four positional arguments of each pathz3 line. The timeouts are milliseconds, but the dialog shows no unit. A field that doesn't parse shows 'Invalid Inputs', yet OK then stores 0 for it and closes anyway, so a 0 timeout reaches pathz3 silently. The draft proposes keeping the settings, showing their units, and rejecting an invalid entry instead of storing 0. In 2.0 the settings follow stage 5's decisions: a per-query timeout plus an optional whole-solve budget (S5.21), and extra solutions only on request (S5.26).

- Fix as proposed (Recommended): Solver settings with units, invalid entries rejected, mapped onto S5.21's and S5.26's settings.
- Keep FuSION 1 behaviour: Four unlabelled integers; invalid becomes 0.

**Answer:** Fix as proposed (Recommended), written into S8.10

**Q2 (S8.11).** S8.11 · Relaxation bounds page. Two cards, node and edge relaxation bounds, each with lower and upper steppers and a two-thumb slider over 0 to 100; typed values are clamped and committed on Enter or focus loss, and the slider keeps lower ≤ upper. They become the four relaxation bounds of both pathz3 lines. Problems: a stored lower bound above 50 puts the slider's lower thumb at 50 (the stepper still shows the stored value), and any change on that card then rewrites it to 50, a Swing RangeSlider quirk; one batch does use a lower bound of 7 (r5_mcl1_stat3's resolve_wo_n7_e0); values stay on the page until Next or Save. The draft proposes keeping the input as four bounded integers with lower ≤ upper; the slider fault belongs to the Swing control.

- Keep (Recommended): Four bounded integers, lower ≤ upper; the Swing slider fault doesn't carry over.
- Fix: no upper cap of 100: Same input, but bounds limited by the graph's size rather than a fixed 0 to 100.

**Answer:** Keep (Recommended), written into S8.11

**Q3 (S8.12).** S8.12 · Node IDs page. Source, target and FSC fields with suggestions from the symbol map; picking one stores '<id> - <lower-case symbol>' (e.g. 'hsa1956 - egfr'), and the script relies on let keeping the first token (S1.08 stands, so 2.0 must write bare ids). Problems: Enter with no exact match still stores the typed text ('EGFR' becomes the node 'hsaEGFR'); text typed without Enter or a pick is lost; a reopened page shows empty fields; an empty field writes 'let $SRC_NODE hsa'; an unknown or empty FSC is silently skipped by pathz3, so the with- and without-FSC solves are the same problem and the run is no significance test; only one source, target and FSC can be entered. The draft proposes keeping the symbol-aware lookup, storing bare ids, accepting only ids in the map, showing the stored values, and requiring all three ids. Your r7 and r8 runs are single solves with no FSC, which requiring an FSC would rule out in the GUI.

- Fix, FSC optional (Recommended): As proposed, but source and target are required and the FSC optional: no FSC means a single solve, as in r7 and r8.
- Fix as drafted: As proposed, with all three ids required.
- Keep FuSION 1 behaviour: Display text stored, unchecked.

**Answer:** Fix, FSC optional (Recommended), written into S8.12

**Q4 (S8.13).** S8.13 · Mapping files dialog and the start line. 'Change Mapping Files' offers three choosers: the HSA-to-gene-symbol map, the pathway-name map and the cross-database map, which feed 'start' (and the cross-database map also both mff lines). Problems: the defaults are names relative to Linux/, where neither file exists (the shipped copies are in Linux/frontend/), so a new session gets no suggestions and start runs with empty name maps, with only a stderr line to show for it; the dialog edits the session directly, with no Cancel and no way to clear a file; an empty cross-database map gives start and mff an empty token. The draft proposes shipping the two reference maps with 2.0, using them by default from a checked location, and keeping the three choosers for other maps. S2.14 already dropped both uses of the cross-database map, pending Phase 2's id-mapping design.

- Fix, two choosers (Recommended): Ship and default the two reference maps, keep choosers for the symbol and pathway-name maps; a cross-database input waits for Phase 2's id mapping (S2.14).
- Fix as drafted: Ship the two maps, keep all three choosers.
- Keep FuSION 1 behaviour: Relative default names, dialog as now.

**Answer:** Fix, two choosers (Recommended), written into S8.13

## 093 · Poll · 2026-11-12 17:22

The fifth stage 8 poll, with stage 8's open question 3.

**Q1 (S8.14).** S8.14 · Threshold filter page. Down and Up threshold fields, a Filter button that checks them (numbers, down ≤ up), stores them and previews the LFC file in columns (Up, Not DE, Down, plus four override-only columns), each entry like 'hsa3725 - JUN (2.41)'. Problems: only Filter stores the thresholds, so typed values are lost on Next or Save unless Filter was pressed (RUNNING.md warns about this); the preview is a fifth LFC parser with its own rules (lines with three or more fields skipped, one non-numeric value aborts the whole preview, a repeated id keeps its last value, down = up shows a gene as Up only); and the preview isn't what the run uses, since cudf reclassifies from the file. S3.07 already makes up ≤ down an error. The draft proposes keeping a live preview of the up and down lists, taking the thresholds as typed, and computing the preview with the backend's single LFC parser (S3.14), so it shows exactly what cudf will produce.

- Fix as proposed (Recommended): Live preview from the backend's own parser; thresholds kept as typed.
- Keep FuSION 1 behaviour: Filter-only storage and the GUI's own parser.

**Answer:** Fix as proposed (Recommended), written into S8.14

**Q2 (S8.15).** S8.15 · Node overrides reach the script as gene text. On the threshold page, Search highlights lines and Override moves a gene into chosen columns (Up / Not DE / Down; Active / Inactive; Relaxed / Non-relaxed). ScriptFile writes four of the lists (confirmed up, confirmed down, inactive, relaxed) as comma-joined display text, or an empty temp file path. So overriding JUN to Inactive writes 'let $INACTIVE_NODES_FILE hsa3725 - jun (2.41)'; pathz3 tries to open a file named 'hsa3725', fails and uses an empty set: no override has ever reached a solve. Also: Search alone records an override (an arbitrary match), override keys are lower-cased so removal fails for upper-case symbols, three columns are never written, overrides aren't saved, and the temp file vanishes when the GUI exits. The draft proposes keeping per-gene overrides, written as one-id-per-line files in the run directory, passing every kind pathz3 accepts, and storing them with the session.

- Fix as proposed (Recommended): Real override files in the run directory, every accepted kind passed, stored with the session.
- Drop overrides: No per-gene overrides in the 2.0 frontend; users edit the node files themselves.

**Answer:** Fix as proposed (Recommended), written into S8.15

**Q3 (Overrides).** Stage 8, open question 3: what should the three override kinds the GUI collects but never writes do? 'Active' and 'Non Relaxed Node' match pathz3's active-node and non-relaxed-node files, which the template already fills with the source, target and FSC. 'Not Differentially Expressed' has no pathz3 input: it would mean dropping the gene from the up and down lists the solve treats as observations.

- Wire all three (Recommended): Active and Non-relaxed join pathz3's active and non-relaxed node files; Not DE removes the gene from the observed up and down lists.
- Wire two, drop Not DE: Active and Non-relaxed only; the Not DE override goes.
- Drop all three: Only the four kinds the GUI already writes.

**Answer:** Wire all three (Recommended), written into S8.15 and stage 8's open question 3

**Q4 (S8.16).** S8.16 · Edge files page and the coexpression inputs. Five optional file choosers (additional, essential, avoid, relaxed and non-relaxed edges), a coexpression CSV chooser and two integer steppers (COEXP_THRESH, FROZEN_THRESH). An unchosen file is written as an empty value, which pathz3 treats as an unopenable file and an empty set. Problems: Save on this page never writes the session file (only Next does); a chosen file can't be removed again; the edge-file formats are neither shown nor checked; the thresholds accept negative numbers. The draft proposes keeping the page's inputs, since all are live pathz3 inputs; S8.17 fixes how they reach pathz3, S8.30 checks them, and Phase 5's navigation (S8.03) covers Save and clearing a choice.

- Keep (Recommended): Same inputs; delivery, checks and page behaviour handled by S8.17, S8.30 and Phase 5.
- Keep FuSION 1 behaviour: Page as now, including its Save and removal faults.

**Answer:** Keep (Recommended), written into S8.16

## 094 · Poll · 2026-11-12 17:24

The sixth stage 8 poll.

**Q1 (S8.17).** S8.17 · The coexpression and frozen-edge settings never reach pathz3 on Linux. The template ends both pathz3 lines with $COEXPRESSION_CSV $COEXP_THRESH $FROZEN_THRESH $NEW_CONSTRAINTS_ENABLED $Z3_RANDOM_SEED, but ScriptFile has no replacement for the first three placeholders, so they reach the batch as literal '%__a21__%' etc. Linux pathz3 takes '%__a21__%' as the CSV path, fails to parse '%__a22__%' and stops reading, so it logs 'enabled=0' and emits neither constraint family: every Linux GUI run was solved without them, silently, although the page asks for them and arjun.json stores 700, a CSV and 800. Filling the placeholders alone wouldn't suffice, since an empty CSV token also stops the Linux parser. macOS fixes both halves. S5.02 already decided the backend half (each optional slot consumes its token, as on macOS; a non-numeric value is an error). The draft proposes filling all three values and stopping on any unfilled placeholder (S8.20).

- Fix as proposed (Recommended): Values filled, S5.02's slot parsing, unfilled placeholders stop the run.
- Keep FuSION 1 behaviour: Constraints silently off in GUI runs.

**Answer:** Fix as proposed (Recommended), written into S8.17

**Q2 (S8.18).** S8.18 · The pipeline editor page (gui4, 4601 lines). The last page embeds a block editor: ten built-in blocks (let, cudf, exec, start, mff, wgx, rgx, fb_rch, pathz3, result) plus user templates, connectable with type checks, fed by 'Input Data' cards from a Data_Coming_From_Frontend.txt the page writes (base names only). Execute writes a plan (execution.txt) in its own notation, Save/Load write a .pipeline file also kept in the session (your only stored pipeline is empty), and Submit runs the template, ignoring the drawn pipeline. Nothing in the editor affects a run, its cycle check can never fire, and it rebuilds and rereads its files on every visit. The draft proposes dropping it; the flexible step order and partial runs it points towards are designed in Phase 2 (decisions 0018 and 0019).

- Drop (Recommended): Drop this editor; whether 2.0's frontend offers a working visual pipeline builder is a Phase 5 question, built on decisions 0018 and 0019.
- Keep and make it work: Port the editor and make Submit run the drawn pipeline.

**Answer:** Keep and make it work. Not yet written into S8.18: a follow-up confirms how it combines with S8.20 (entry 095).

**Q3 (S8.19).** S8.19 · The pipeline editor's 'Gen Script' and block library files. 'Gen Script' writes a template-like file with unfilled placeholders, nothing reads it, and its output wouldn't run (undefined variables such as $KEGG_XML_FILE, gaps that shift arguments). The shipped block_templates.txt and block_descriptions.txt contradict the built-ins and the real commands (rgx, fb_rch, pathz3, let and exec get two integer inputs and one file output; cudf 'processes package dependency files and generates upgrade plans', mff 'computes maximum flow'), and the editor's README describes cloning a separate repository and running it under JDK 8. The draft proposes dropping it with S8.18.

- Drop (Recommended): Drop with the editor; its output is never run and its library is wrong.
- Keep FuSION 1 behaviour: Keep Gen Script and the library files.

**Answer:** Keep FuSION 1 behaviour. Not yet written into S8.19: a follow-up confirms whether the files are kept as they are or made correct (entry 095).

**Q4 (S8.20).** S8.20 · Filling the batch template (ScriptFile). On Submit, ScriptFile reads Linux/advtempscript.txt, makes an empty temp file, replaces about forty %__..__% placeholders by plain text substitution, and writes Linux/output_script.txt. Problems: three templates exist and two are never read; nothing ties the GUI's fields to the placeholders, so three are never filled (S8.17) and leftovers go undetected; values go in unquoted, so a path with a space or a stored id like 'hsa1956 - egfr' becomes several tokens; the edge restriction is used as an array index, so its default −1 throws; output_script.txt is overwritten by every run; two generated scripts are committed. The draft proposes keeping the approach of generating a batch file from the session (the batch language stays, decision 0018), from one template kept with the backend, using named arguments, with every value checked, quoted where needed (S1.05's quoting), and a stop on any unfilled placeholder.

- Fix as proposed (Recommended): One template with the backend, named arguments, checked and quoted values, no unfilled placeholders.
- Fix: no template: The frontend calls the backend's steps directly instead of generating a batch file (the batch stays for command-line users).
- Keep FuSION 1 behaviour: Text substitution into advtempscript.txt.

**Answer:** Fix as proposed (Recommended), written into S8.20

## 095 · Poll · 2026-11-12 17:26

Follow-ups to entry 094's S8.18 and S8.19 answers. After it, a note on how the two combine was added to S8.20's decision.

**Q1 (S8.18).** S8.18 follow-up. You chose to keep the pipeline editor and make Submit run the drawn pipeline. S8.20 keeps generating a batch file from one template. How should the two fit together in 2.0?

- Template = default pipeline (Recommended): The standard run is a pre-drawn pipeline the editor opens with; users edit, add or remove blocks; every Submit turns the drawn pipeline into a batch file. One generation path.
- Pipeline when drawn: Submit runs the drawn pipeline if there is one, otherwise the standard template filled from the session's pages (two generation paths).

**Answer:** Template = default pipeline (Recommended), written into S8.18 and S8.20

**Q2 (S8.19).** S8.19 follow-up. With the editor kept and made to work (S8.18), you chose to keep 'Gen Script' and the block library files. Today Gen Script's output doesn't run and the library files contradict the real commands (e.g. cudf 'processes package dependency files', mff 'computes maximum flow'). Did you mean to keep them as they are, or to keep them and make them correct?

- Keep and correct (Recommended): Gen Script writes a runnable batch file of the drawn pipeline; the block library's inputs, outputs, types and descriptions match the real commands.
- Keep exactly as now: Gen Script and the two library files as FuSION 1 has them.

**Answer:** Keep and correct (Recommended), written into S8.19

## 096 · Poll · 2026-11-12 17:27

The seventh stage 8 poll.

**Q1 (S8.21).** S8.21 · The generated script's run directory. The output directory is $WORK_DIR/<src>_to_<tgt>_fsc_<fsc>_<lfc file name>_b<bound>, and the script first runs 'exec if [ -d $WRITE_DIR ] ; then /bin/rm -rf $WRITE_DIR; fi', then recreates it; the run's fixed-name files are moved there from the launch directory with exec mv. Problems: a rerun with the same source, target, FSC, LFC file name and bound deletes the earlier results without asking, even when thresholds, relaxation bounds, edge files or coexpression settings differ; each pathz3 leaves its SMT dump directory in Linux/, never moved; moving fixed-name files by name can grab an earlier run's file (e.g. PO.dat), and two simultaneous runs take each other's; a working directory with a space is cut at the space. The draft proposes a new directory for every run, never deleting one, with every output of the run in it (SMT dumps included) and the run's full parameters recorded there.

- Fix as proposed (Recommended): A fresh, never-deleted directory per run, holding all its outputs and its full parameters.
- Keep FuSION 1 behaviour: Same-name reruns delete earlier results.

**Answer:** Fix as proposed (Recommended), written into S8.21

**Q2 (S8.22).** S8.22 · The generated script's input preparation. As in the hand-written batches: cudf writes up_reg and down_reg; exec lines echo the source and target into files, write connect_pairs, cat cudf's lists, the source, the target and the not-merged ids file into the merge-exception file for start, and touch an empty avoid file; one 'exec cd $WRITE_DIR;' line does nothing, since each exec gets its own shell. This is file plumbing through exec with a disk round trip for cudf's lists. The draft proposes following S1.17's decision: the backend builds the exception set itself from cudf's lists, the source, the target and the not-merged file (S2.13 adds the FSC), so the frontend supplies only those inputs.

- Fix as proposed (Recommended): The backend assembles the exception set; the frontend passes only the inputs.
- Keep FuSION 1 behaviour: exec plumbing in the generated script.

**Answer:** Fix as proposed (Recommended), written into S8.22

**Q3 (S8.23).** S8.23 · The generated script's graph building. List mode merges the KGMLs and writes a master graph, runs exec add (S8.24), then both modes do a second mff with 'y' over the master (plus the additional-edges XML in list mode), write merged_master_graph.xml, move edges_merged_filename, and read the graph back with rgx. Problems: in pre-merged mode the user's XML is parsed twice and written once more per run, and with the GUI's split threshold of 15000 the GUI run wrote a 13 MB merged graph from a 1.9 MB input; the one-file mff writes no edges_merged_filename, so the mv fails or moves an earlier run's file; every run rebuilds the graph from scratch, even when the session already built the same one. The draft proposes building the graph once in memory (S2.26's decision), saving it with the run, and letting a later run of the session start from it (decision 0019).

- Fix as proposed (Recommended): One in-memory build, saved with the run, reusable by later runs.
- Keep FuSION 1 behaviour: Rebuild and round-trip the graph every run.

**Answer:** Fix as proposed (Recommended), written into S8.23

**Q4 (S8.24).** S8.24 · 'exec add' cannot run. In list mode the script runs 'exec add $ADDITIONAL_EDGES_FILE $ADDITIONAL_EDGES_XML_FILE', which hands 'add' to /bin/sh; add is a FuSION command (S2.30) and Linux/add is a standalone build of it, but the shell looks it up on PATH and finds nothing, so it fails with 'add: not found' and writes no XML. The next lines still add that missing XML to the merge list, so the second mff aborts on it: every list-mode GUI run fails here, whether or not an additional-edges file was chosen. Even a working add would produce an empty XML that fails a reader assertion when no file was chosen. S2.30's text says this line runs the standalone tool, which holds only when the launch directory is on PATH (a stage 2 correction to come). The draft proposes calling the built-in add command, only when an additional-edges file was chosen, in both pathway modes (S8.08).

- Fix as proposed (Recommended): Built-in add, only with a chosen file, in both modes.
- Keep FuSION 1 behaviour: exec add through the shell.

**Answer:** Fix as proposed (Recommended), written into S8.24

## 097 · Poll · 2026-11-12 17:29

The eighth stage 8 poll.

**Q1 (S8.25).** S8.25 · The generated script's two solves. As in the hand-written batches: fb_rch, then the with-FSC node files (active, non-relaxed and essential hold source, target and FSC; avoid is empty) and the first pathz3 (33 arguments), then fb_rch again with the same arguments, the without-FSC files (the avoid file holds the FSC) and the second pathz3, each followed by exec mv of its limits and PO files. Problems: the corridor is computed twice (S4.20) and the second solve waits for the first although neither reads the other's output; the search mode is fixed at 0. Since then, stage 4 decided to compute the corridor once (S4.20), stage 5 made the FSC a named argument with both cases solved on one corridor (S5.17), and mode 1 was dropped (S5.28). The draft proposes keeping these commands, with their costs handled in stages 4 and 5 and their optional arguments in S8.17.

- Keep (Recommended): The default pipeline still runs both FSC cases, written with S5.17's named FSC on one corridor (S4.20).
- Keep FuSION 1 behaviour: Two fb_rch calls and two hand-built pathz3 lines, as the template has them.

**Answer:** Keep (Recommended), written into S8.25

**Q2 (S8.26).** S8.26 · The generated script's Pareto plot and wrap-up. The template opens a persistent gnuplot window ('gnuplot -p', with-FSC blue, without-FSC red), then draws the same plot again to a PNG named after the run, moves mylog.txt into the run directory and exits. Problems: this isn't the canonical plot (current batches call plot_po.sh, which styles the curves and keeps coincident points visible; here equal curves hide one another); the -p window opens on whatever machine runs the backend and fails without a display, and it is the only result the GUI shows; the plot is drawn twice. The draft proposes drawing the plot once, to a file, with the canonical plot, and showing it in the frontend (S8.29). S6.22 already made the canonical plot a built-in step.

- Fix as proposed (Recommended): S6.22's built-in plot, drawn once to a file and shown in the frontend.
- Keep FuSION 1 behaviour: gnuplot window plus a second PNG plot.

**Answer:** Fix as proposed (Recommended), written into S8.26

**Q3 (S8.27).** S8.27 · Commands the GUI cannot emit. Neither the template nor the editor's generator has a gsea, psp, pex or plot_po.sh line, so a GUI user can't shortlist pathways, plot solution paths or explanation subgraphs, or get the canonical Pareto plot, although 19 of the 88 batch files run gsea, 25 run pex and 22 call plot_po.sh. The template also fixes the constraint-family switch to 1, the seed to 42, the search mode to 0, no fb_rch exclusions, one source-target pair and no upfx. A GUI run does strictly less than current batches, and nothing says so. The draft proposes that the 2.0 frontend can request pathway shortlisting, the explanation and solution-path views and the canonical Pareto plot. With S8.18, these would be blocks of the pipeline editor, with the default pipeline including the ones current batches use.

- Fix as proposed (Recommended): Every live step available from the frontend, as editor blocks; the default pipeline matches current batches (gsea, solve, plot, explanations).
- Fix, steps only on request: All steps available as blocks, but the default pipeline stays the minimal solve.

**Answer:** Fix as proposed (Recommended), written into S8.27

**Q4 (S8.28).** S8.28 · Running the backend and the run window. Submit starts './fusion -b output_script.txt' in the launch directory and opens a log window: each output line with a timestamp, and a stage label guessed from the echoed command (cudf 'Processing fold-change data', mff 'Merging pathways', pathz3 'Constraint solving (Z3)', and so on); at the end it shows 'Execution Completed' and offers another experiment. Problems: the exit status is ignored, so aborted runs show 'Execution Completed'; there is no Stop, and closing the window leaves the backend running (closing the GUI deletes the run's temp file); nothing prevents a second Submit sharing Linux/ with the first; progress is guessed from echoes, so an hours-long solve shows nothing; windows pile up. Stage 1 cites a dead copy of this code (to be corrected). The draft proposes keeping a live log with a stage indicator, reporting the exit status, offering Stop, giving each run its own directory (S8.21), and refusing or queueing a second run that would share one.

- Fix as proposed (Recommended): Live log and stage indicator, real exit status, Stop, one directory per run.
- Keep FuSION 1 behaviour: Run window as now.

**Answer:** Fix as proposed (Recommended), written into S8.28

## 098 · Poll · 2026-11-12 17:31

The last stage 8 poll.

**Q1 (S8.29).** S8.29 · Showing results. The GUI shows only the run log. It doesn't open the run directory, the plot, pathz3's solution files or anything else; the only picture is the gnuplot window the script itself pops up, and users find $WORK_DIR/<run>/ by hand. No session links to its runs' results, so the user can't tell which directory came from which settings, and a rerun silently replaces it (S8.21). The two Pareto curves, the FSC comparison and the explanations computed in stages 5 and 6 are never shown. The draft proposes that the 2.0 frontend shows each run's results (at least the two Pareto curves, the solutions and the explanation views), linked from its session, with decision 0022 designing the graph display.

- Fix as proposed (Recommended): Results shown in the frontend and linked from the session; graph display designed under decision 0022.
- Fix, links only: The frontend lists each run's output files and opens them, without its own result views.

**Answer:** Fix as proposed (Recommended), written into S8.29

**Q2 (S8.30).** S8.30 · No checks before a run. Submit fills the template with whatever the session holds: nothing checks that files exist and are readable, that ids exist, that numbers are in range, or that paths contain no space (a path with a space is cut by let, e.g. '/home/u/My Data/x.tsv' becomes '/home/u/My'). Bad inputs surface only as backend log lines or not at all: a missing map gives an empty map, an unknown FSC gives two identical solves, an empty edge-file name an empty set, an unfilled placeholder silently disables constraints; and 27 of 28 stored paths are stale. The draft proposes checking every input before a run starts, as decision 0021 requires of both the backend and the frontend.

- Fix as proposed (Recommended): Every input checked before the run, in the frontend and again in the backend.
- Keep FuSION 1 behaviour: No checks.

**Answer:** Fix as proposed (Recommended), written into S8.30

**Q3 (S8.31).** S8.31 · Files shipped with the GUI. Linux/frontend/ carries the symbol map (6334 lines) and pathway-name map (2822), the not-merged ids file (hsa1432, hsa5594, hsa5599), a KGML list of 162 paths on another user's machine (none exists here), the org.json jar, three icons (two unused), dummy.txt and an empty README; git also tracks 67 stale classes, arpit's session, and in Linux/ the template, two generated scripts and the account file with an email and password hash. Nothing reads the data files where they are. The draft proposes shipping only what 2.0 reads (the reference maps with their source and date, and a KGML list that resolves) and keeping compiled classes, sessions, accounts and generated scripts out of the repository. KEGG data (KGMLs and KEGG-derived maps) comes with licence terms, which matters for what a repository ships.

- Fix, KGMLs not shipped (Recommended): Ship the reference maps with source and date, and a KGML list resolved against the user's own KGML directory; no compiled, generated or personal files.
- Fix as drafted: Ship the maps and a KGML list that resolves, as the draft says.
- Keep FuSION 1 behaviour: Ship the same files.

**Answer:** Fix, KGMLs not shipped (Recommended), written into S8.31

**Q4 (S8.32).** S8.32 · frontend1, the older snapshot (2026-07-02). Its 18 Java files all exist in frontend/src, most identical or differing only in line endings. The behavioural differences are that its sessions window and edge-files page Submit and run at once, skipping the pipeline editor, and its ☰ menu's ninth entry opens the edge-files page. Keeping two copies in step is the cost. With S8.18 the editor now matters, so frontend1's one real difference, a Submit that runs straight away, amounts to a 'run the default pipeline as is' shortcut. The draft proposes dropping frontend1.

- Drop (Recommended): Drop the copy; a 'run the default pipeline as is' button is for Phase 5's design.
- Keep its quick Submit: Drop the copy, but require a run-at-once Submit that skips the editor in 2.0's frontend.

**Answer:** Keep its quick Submit, written into S8.32. All 32 stage 8 items are now decided.
