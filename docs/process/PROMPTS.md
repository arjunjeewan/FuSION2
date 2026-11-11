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
