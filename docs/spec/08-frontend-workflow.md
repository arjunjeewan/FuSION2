# 08 · Frontend workflow

## Scope

This stage covers FuSION 1's Java Swing GUI in `Linux/frontend/`: its screens and dialogs, what the user enters on each, the session files in `frontend/sessions/`, the login and its account file, how it fills the batch template into a batch file, how it starts the backend and shows progress, the pipeline editor with its block templates and descriptions, the notes in `frontend workflow/`, the READMEs and the data files it ships. It also covers the files the GUI reads and writes in the directory it is launched from, `Linux/`: the template `Linux/advtempscript.txt`, the generated `Linux/output_script.txt` (and the older `Linux/script.txt`), the account file `Linux/fusion_users.dat` and `Linux/Data_Coming_From_Frontend.txt`. `Linux/frontend1/`, an older snapshot, is compared file by file in S8.32; where a shared file behaves differently, the item that owns the behaviour describes both versions. `Linux/GUIForExecutable.java` is out of scope ([decision 0014](../decisions/0014-stage8-gui-scope.md)). The GUI hands over to stage 1 at `./fusion -b output_script.txt` (S1.01) and reads stage 1's command echo for its progress display (S1.10). What each generated command does belongs to stages 1 to 5 (`let`, `exec`, `start`, `cudf`, `mff`, `wgx`, `rgx`, `size`, `fb_rch`, `pathz3`, `add`); this stage records which commands the GUI emits, with which arguments, and where its values come from. The GUI emits no stage 6 or 7 command (S8.27). `macOS/frontend/` is outside this stage's scope, but contrary to the ground rule it is not byte-identical to `Linux/frontend/` (open question 1); where the macOS copy already fixes a problem, the item says so. "The template" means `Linux/advtempscript.txt` (355 lines), the only copy a GUI started as its README says ever reads (S8.01, S8.20). "The real batch" means `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr025/egfr_cxcl8_fsc_jun_b10_nr8_er8_gsea_fdr025.batch`. In quoted file lines, `<TAB>` stands for a tab character.

## Items

### S8.01 · Build, launch and the launch directory

- **FuSION 1:** `Linux/frontend/README.md`; relative paths at `Linux/frontend/src/LoginPage.java:21`, `193`, `431`, `Linux/frontend/src/SessionFrame.java:142`, `156`, `179`, `205`, `223`, `247`, `253`, `Linux/frontend/src/UserInput.java:345`, `Linux/frontend/src/ScriptFile.java:16`, `82`, `Linux/frontend/src/Gui4Panel.java:121`, `153`, `Linux/frontend/src/gui4.java:38-44`, `4004`; `Linux/README.md:56-64`; `Linux/frontend/out/`
- **What it does:**
  - The README builds and starts the GUI from `Linux/`: `javac -d frontend/out -cp "frontend/lib/json-20250517.jar" frontend/src/*.java`, then `java -cp "frontend/out:frontend/resources:frontend/lib/json-20250517.jar" LoginPage`. It asks for JDK 21; the code needs at least Java 16 (`instanceof` patterns, `ReachPathBoundCard.java:148`). The only library is org.json (`lib/json-20250517.jar`), and `resources/` supplies the login icon.
  - Every file the GUI touches is a path relative to the JVM's working directory, so that directory must be `Linux/`:

    | Path | Used by | Purpose |
    |---|---|---|
    | `fusion_users.dat` | LoginPage | accounts (S8.02) |
    | `frontend/sessions/<user>.json` | LoginPage, SessionFrame, UserInput | sessions (S8.05) |
    | `advtempscript.txt` | ScriptFile, read | batch template (S8.20) |
    | `output_script.txt` | ScriptFile, written | generated batch file (S8.20) |
    | `./fusion` | Gui4Panel, run | backend (S8.28) |
    | `Data_Coming_From_Frontend.txt` | Gui4Panel writes, gui4 reads | pipeline editor inputs (S8.18) |
    | `execution.txt`, `execution_history.txt`, `block_templates.txt`, `block_descriptions.txt` | gui4 | pipeline editor (S8.18, S8.19) |

    The default mapping files are relative names too (S8.06, S8.13).
  - The backend inherits the same directory, so every fixed-name file it writes (S1.26) and its SMT dump directories (S8.21) land in `Linux/`.
- **Problems:**
  - Started from any other directory, the GUI comes up with no accounts, cannot find the template or `./fusion`, and keeps sessions somewhere else, with no message until a file is missing.
  - `Linux/README.md:56-64`, the main install guide, tells users to run `java GUIForExecutable`, the out-of-scope 2024 GUI, and never mentions `frontend/`.
  - 67 compiled classes in `frontend/out/` are tracked in git and are stale. `SessionFrame$SessionSelectorPanel.class` holds the label `Previously Saved Sessions:` and `EdgesChooserPanel.class` holds `Go to Sessions`; neither string is in either source tree (checked with `javap`). Running without recompiling starts an older GUI.
- **Proposed call:** fix. 2.0's frontend finds the backend and its data through configuration, not through its working directory, and no compiled classes are kept in the repository.
- **Decision:** fix, as proposed (prompt log entry 089)

### S8.02 · Login and sign-up, and `fusion_users.dat`

- **FuSION 1:** `Linux/frontend/src/LoginPage.java:21-28` (file name, icon, constructor), `52-235` (sign-in card), `237-477` (sign-up card), `517-561` (`authenticateUser`, `hashPassword`, `loadUserData`, `saveUserData`), `563-577` (`UserData`), `579-581` (`main`); `Linux/fusion_users.dat`
- **What it does:**
  - `LoginPage` is the main class. It opens an undecorated 700 × 850 window with two cards, "Sign in to your account" and "Create a new account".
  - At start it reads `fusion_users.dat`, if present, by Java deserialisation: a `HashMap<String, LoginPage$UserData>` whose values hold `username`, `email`, `passwordHash` and a `fullName` that nothing sets. A read error shows "Error loading user data: …" and leaves the map empty.
  - **Sign up** takes username, email, password and confirmation. It rejects an empty email, username or password ("All fields are required", line 411), a mismatch, an empty confirmation included ("Passwords do not match"), and a known username ("User already exists"). Otherwise it creates `frontend/sessions/<username>.json` holding `[]` if that file is missing, adds the user, rewrites the whole map to `fusion_users.dat`, shows "Account created successfully!" and returns to the sign-in card.
  - **Sign in** hashes the password with SHA-256 over `password.getBytes()` (the platform charset), as lower-case hex, and compares it with the stored hash. On success it closes the window, creates the sessions directory and the user's file (`[]`) if missing, and opens the sessions window (S8.04). On failure it shows "Invalid username or password", clears both fields and pops up "Wrong password!" (title "Login Error").
  - The tracked `Linux/fusion_users.dat` (304 bytes) holds one account, `arpit`, with an email address and a password hash.
- **Problems:**
  - The hashes are unsalted SHA-256, and the account file, email address included, is committed to git, so anyone with the repository can attack the hash offline.
  - The login guards nothing: sessions are plain JSON files that anyone with file access can read or edit, and anyone can sign up.
  - Usernames are not checked. One containing `/` or `..` puts its session file outside `frontend/sessions/` (`new File(sessionsDir, username + ".json")`, lines 200 and 438).
  - "Wrong password!" is also shown for an unknown username.
  - Accounts and sessions are separate stores that can disagree: `sessions/arjun.json` exists, but the tracked account file has no `arjun`.
  - Java serialisation ties the file to the class name `LoginPage$UserData`.
- **Proposed call:** fix. Keep per-user sign-in and per-user sessions, but keep salted, slow password hashes in a store outside the repository, validate usernames, and remove the committed account file.
- **Decision:** fix, as proposed (prompt log entry 090)

### S8.03 · Window frame and step navigation

- **FuSION 1:** `Linux/frontend/src/HeaderPanel.java:10-27` (constructors), `35-43` (☰ button), `55-95` (window buttons, dragging), `110-175` (menu, `navigateTo`, `navigate`); the Prev, Next, Save and Sessions buttons at `MainPanel.java:117-147`, `FileSelectionCard.java:91-135`, `XMLSelection.java:244-264`, `ReachPathBoundCard.java:179-254`, `RelaxationBoundsPanel.java:46-59`, `IdEntryPanel.java:340-358`, `ThresholdFilterPanel.java:151-203`, `EdgesChooserPanel.java:77-114`, `Gui4Panel.java:33-65`; styling in `Theme.java`, `RoundedButton.java`, `RoundedPanel.java`; `Linux/frontend1/src/HeaderPanel.java:159`
- **What it does:**
  - All windows are undecorated. Each page has a gradient header with "FuSION", the user name, and minimise, maximise and close buttons; dragging the header moves the window, a double click maximises it, and close calls `System.exit(0)`. `Theme` holds the colours and the Segoe UI fonts; `RoundedButton` and `RoundedPanel` draw the rounded controls.
  - The configuration steps are cards of one `CardLayout`, in this Next order: working directory and log-fold-change file (S8.07), pathway XML (S8.08), reach path bound (S8.09), relaxation bounds (S8.11), node IDs (S8.12), threshold filter (S8.14), edge files (S8.16) and the pipeline editor (S8.18).
  - Each page has "← Prev", "Next →", "Save" and "Sessions" buttons. Next copies the page's values into the in-memory session (`UserInput`) and builds the next page afresh; Save does the same and writes the session file (S8.05), except on the edge-files page, where Save only copies the page's values and never writes the file (`EdgesChooserPanel.java:104`; `frontend1` line 108); Sessions returns to the session list; Prev shows the previous card by a fixed name (`"Main"`, `"fileSelect"`, `"xml"`, `"reachBound"`, `"relaxationPanel"`, `"idEntry"`, `"threshold"`, `"edgeEntry"`). Some values are committed earlier or later than Next: see S8.09, S8.12 and S8.14.
  - The ☰ button lists nine steps, Working Directory, Log Fold Changes, KEGG / XML Files, Reach Path Bound, Relaxation Bounds, Threshold Filter, Edge Files, Node IDs and Pipeline Editor, and opens any of them directly as card `nav_<i>`, replacing the one it opened last time. Its order differs from Next's: Threshold Filter and Edge Files come before Node IDs.
  - **frontend1:** the ninth entry, still labelled "⚙️  Pipeline Editor", opens the edge-files page, because `frontend1` has no pipeline editor (`frontend1/src/HeaderPanel.java:159`).
- **Problems:**
  - Nothing is saved until the user presses Save, or reaches a step that saves (S8.16, S8.18). Closing the window drops unsaved changes without a warning.
  - Prev throws away whatever a page held but had not yet handed to the session: chosen edge files (S8.16), relaxation bounds (S8.11) and threshold overrides (S8.15).
  - The ☰ menu skips Next's checks. A user who never passes the reach-path page reaches Submit with the edge restriction still at −1, and the run stops with `Run Error` "Index -1 out of bounds for length 3" (`ScriptFile.java:40`).
  - Prev shows a card by name; on a page reached through the ☰ menu, that card may never have been created, and Prev does nothing.
  - Next adds a new panel on every visit and never removes the old one, so the card stack grows for as long as the window is open.
- **Proposed call:** drop. Undecorated Swing frames and a card stack do not carry over to a web frontend; Phase 5 designs navigation, and the problems above are what it must not repeat.
- **Decision:** drop, as proposed: Phase 5 designs navigation and must not repeat the problems above (prompt log entry 090)

### S8.04 · Sessions window

- **FuSION 1:** `Linux/frontend/src/SessionFrame.java:21-41` (window), `57-217` (`SessionSelectorPanel` and its buttons), `219-236` (`loadSessions`), `238-268` (`addNewSession`); `Linux/frontend1/src/SessionFrame.java:151-169` (frontend1's Submit), `281-391` (its run window)
- **What it does:**
  - After sign-in, an undecorated 1200 × 750 window, "Your Sessions", lists the user's sessions from `frontend/sessions/<user>.json` in file order, each as `<sessionName> (<timestamp>)`, for example `arjun (2026-07-09T19:06:15.808647958)`.
  - **Configure →** opens the selected session at the working-directory page (S8.03).
  - **New Session** asks "Enter new session name:", rejects a name already in the list ("Session name already exists!"), appends `{"sessionName": <trimmed name>, "timestamp": <LocalDateTime.now()>}` to the file, and opens the new session at the working-directory page.
  - **Delete** asks "Are you sure you want to delete this session?" and rewrites the file without the selected entry.
  - **Log Out** reopens the login window and closes this one.
  - **▶ Submit** differs between the two GUIs:
    - `frontend`: opens the pipeline editor for the selected session (S8.18), whose own Submit starts the run;
    - `frontend1`: fills the template from the stored session (S8.20) and starts the backend at once (S8.28); an exception while filling shows "Error preparing script: …".
  - Configure, Submit and Delete with nothing selected show "Kindly Select a Session".
- **Problems:**
  - Sessions are addressed by their position in the list. A session that is open keeps the index it was opened with (`UserInput.java:352`), so after an earlier entry is deleted, its Save overwrites the neighbouring session.
  - New Session followed by Cancel, or by a blank name, still opens a session: the one selected before, or index −1. With −1 the read fails (`JSONArray[-1] not found.`, caught at `UserInput.java:136-138`), so the string fields stay null, the number fields 0 and `pipelineData` `""` (`UserInput.java:34`), and building the next page throws a `NullPointerException` at `new File(null)` (`MainPanel.java:72`), so nothing opens and nothing is shown.
  - The duplicate check uses the untrimmed name and the trimmed one is stored, so `" a"` passes when `a` exists. The index of the new session is then taken as `sessionMap.size() - 1` (line 267), which is wrong once two names collide.
  - The timestamp is the creation time and never changes, so the list cannot say when a session was last changed or run.
- **Proposed call:** fix. Keep named, per-user sessions that can be created, opened, run and deleted, but address each by a stable id instead of its list position, and make Cancel cancel.
- **Decision:** fix, as proposed (prompt log entry 090)

### S8.05 · Session file `frontend/sessions/<user>.json`

- **FuSION 1:** `Linux/frontend/src/UserInput.java:44-140` (read), `344-407` (`saveData`); `Linux/frontend/sessions/arjun.json`, `arjun.j.json`, `arpit.json`
- **What it does:**
  - One file per user: a JSON array of session objects, rewritten whole on every save with org.json's `toString(2)` (two-space indent, keys in hash order).
  - A new object holds `sessionName` and `timestamp` (S8.04). The first save adds 34 keys, read back with `optString` and the defaults below; numbers are written as JSON numbers and read through `Integer.parseInt` or `Double.parseDouble` of their text, except `CoexpThresh` and `FrozenThresh`, which are read with `optInt` (`UserInput.java:81-82`).

    | Key | Set on | Default when absent | Placeholder (S8.20) |
    |---|---|---|---|
    | `workingDirectory` | S8.07 | the user's home directory | `%__1__%` |
    | `logFold` | S8.07 | `""` | `%__10__%` (base name), `%__lf__%` |
    | `TxtFile`, `XmlFile`, `KEGG` | S8.08 | `""`, `""`, `""` | `%__a18__%`, `%__a19__%`, `%__c__%` |
    | `nodeSplitThreshold` | S8.08 | `15000` | `%__a1__%` |
    | `SignallingPath`, `EdgeRestriction` | S8.09 | `50`, `-1` | `%__5__%`, `%__14__%` |
    | `incSolv`, `overSolv`, `solCount`, `solExplore` | S8.10 | `60000`, `150000`, `1`, `1` | `%__a2__%` to `%__a5__%` |
    | `nodeLBound`, `nodeUBound`, `edgeLBound`, `edgeUBound` | S8.11 | `20`, `80`, `20`, `70` | `%__8__%`, `%__9__%`, `%__6__%`, `%__7__%` |
    | `SourceHsaID`, `TargetHsaID`, `CandidateID`, `HSANotMerged` | S8.12 | `""` each | `%__2__%`, `%__3__%`, `%__4__%`, `%__a20__%` |
    | `HsaToGene`, `HsaToPath`, `InterDB` | S8.13 | `hsa_to_gene_symbol_map_6334.txt`, `" hsa_path_to_path_name_map_2822.txt"`, `""` | `%__a16__%`, `%__a17__%`, `%__a15__%` |
    | `dThreshold`, `uThreshold` | S8.14 | `0`, `0` | `%__13__%`, `%__12__%` |
    | `AdditionalEdge`, `EssentialEdge`, `AvoidEdge`, `RelaxedEdge`, `NonrelaxedEdge` | S8.16 | `""` each | `%__ef__%` (also `%__11__%`, `%__a6__%`), `%__a7__%`, `%__a8__%`, `%__a13__%`, `%__a14__%` |
    | `CoexpressionCsv`, `CoexpThresh`, `FrozenThresh` | S8.16 | `""`, `0`, `0` | none (S8.17) |
    | `pipelineData` | S8.18 | `""` | none |

  - Real lines, `arjun.json:37-58`: `"TargetHsaID": "hsa3576 - cxcl8"`, `"SourceHsaID": "hsa1956 - egfr"`, `"CoexpThresh": 700`, `"KEGG": ";;"`, `"EdgeRestriction": 0`, `"CoexpressionCsv": "/home/ks-149-u24/Desktop/arjun/FuSION/coexpression_csv/hsa_stringdb_coexpression_links_over400.csv"`.
  - `sessions/arpit.json` is tracked in git; the other two files are not.
- **Problems:**
  - Paths are absolute and tied to one machine, and nothing checks them when a session opens. 27 of the 28 absolute paths in the three files no longer exist; the one survivor is `/home/ks-149-u24/Desktop/arjun/FuSION/example`. The lead holds: `arjun.json`'s `CoexpressionCsv` names the old `coexpression_csv/` folder, and the file now lives in `additional_constraints/coexpression_csv/`. Its maps point into `IITB_ACTREC_old_data/` (now under `auxilliary/`), and its LFC and master graph into `EGF_HeLa/` (now `runs/egf_hela/`). `arjun.j.json` uses the repository's even older location, `/home/ks-149-u24/Desktop/FuSION/`, and `arpit.json` another machine's `/home/linux`.
  - org.json's `put(key, null)` deletes the key. Choosing one pathway source sets the other field to null (S8.08), so after a save the session has no `TxtFile`, or no `XmlFile`, at all: both sessions in `arjun.json` lack `TxtFile` (behaviour confirmed with the shipped jar).
  - The threshold overrides (S8.15) are not stored and are lost when the session is reopened.
  - Nothing about runs is stored: not the output directory, the time or the outcome (S8.21, S8.29).
- **Proposed call:** fix. Keep one stored record per session with every input a run needs, but key it by a stable id, check its paths when it is opened, keep both pathway-source fields, and store the overrides.
- **Decision:** fix, as proposed (prompt log entry 090)

### S8.06 · Defaults of a new session

- **FuSION 1:** `Linux/frontend/src/UserInput.java:53-95`; `Linux/output_script.txt:20-63` (a script from an untouched session)
- **What it does:** Until a page sets them, a session's fields hold the defaults in S8.05's table. `Linux/output_script.txt`, generated from the near-default `arpit.json`, shows them reaching the batch file: `let $SRC_NODE hsa`, `let $REACH_PATH_BOUND 50`, `let $EDGE_RELAX_LB 20`, `let $NODE_RELAX_UB 80`, `let $UP_REG_THRESH 0.0`, `let $DOWN_REG_THRESH 0.0`, `let $NODE_SPLIT_THRESHOLD 15000`, `let $CONSTR_SOLVER_TIMEOUT1 60000`, and `let $HSA_PATH_TO_PATH_NAME_MAP_FILE  hsa_path_to_path_name_map_2822.txt` with two spaces.
- **Problems:** The defaults contradict what every real batch uses:
  - thresholds 0 and 0 put every gene with a value ≥ 0 in the up list and every gene ≤ 0 in the down list, and a 0 (or an unreadable value, S3.04) in both; the real batches use ±0.67, ±1.01 or ±1.45;
  - a split threshold of 15000 means no multi-gene box is ever collapsed, because a box collapses only when it holds more ids than the threshold (S2.07); every batch uses 1, and the tooltip only says "Threshold to split nodes while merging pathways";
  - relaxation lower bounds of 20 and upper bounds of 80 and 70, where 84 of the 85 batches that set them start both lower bounds at 0, and the upper bounds lie between 0 and 20 (8 is the commonest);
  - a path bound of 50, where the batches use 6, 7, 10, 12 or 15;
  - timeouts of 60000 and 150000 ms, where 51 batches use 600000 and 3000000 and the b10 runs 3600000 and 14400000;
  - an edge restriction of −1, which is none of the three choices and makes the script filler throw if the reach-path page is skipped (S8.03);
  - mapping-file names relative to `Linux/`, where they do not exist (S8.13); the pathway map's name starts with a space (`UserInput.java:91`), which `arpit.json:7` stores and `output_script.txt:60` writes.
- **Proposed call:** fix. Default to what the real batches use (split threshold 1, edge restriction 0, relaxation bounds from 0, path bound 10, the shipped maps), and have the user set, or accept a suggestion for (decision 0013), the LFC thresholds instead of defaulting to 0.
- **Decision:** fix, as proposed (prompt log entry 091)

### S8.07 · Working-directory and log-fold-change pages

- **FuSION 1:** `Linux/frontend/src/MainPanel.java:47-112` (page), `172-185` (`changeDirectory`); `Linux/frontend/src/FileSelectionCard.java:28-99`; template lines `20`, `29`, `36`
- **What it does:**
  - "Welcome to FuSION" shows the session's working directory and a "Change Directory" button, which opens a directory chooser at the home directory. The choice becomes `$WORK_DIR`, under which every run makes its output directory (S8.21).
  - "Log Fold Changes File" ("Select the TSV file containing expression fold-change data") shows the stored path, or "No file selected", and a "Choose File" button whose chooser opens in the working directory with no file-type filter. Next stays disabled until a file is set. The absolute path becomes `$LOG_FOLD_CHANGE_FILE`; its base name becomes `$LOGFOLDCHANGES`, which names the output directory and the Pareto files (S8.21).
- **Problems:** Neither page checks anything: the directory may not exist or be writable, and the file may not be an LFC file (`arpit.json:31` stores `/home/linux/generated_script.txt` as one). After a choice the page shows only "Selected: <name>", not where the file is.
- **Proposed call:** keep. A results location and an LFC file are needed inputs; their checks belong to S8.30.
- **Decision:** keep (prompt log entry 091)

### S8.08 · Pathway XML page: KGML list or pre-merged XML, and the split threshold

- **FuSION 1:** `Linux/frontend/src/XMLSelection.java:45-139` (radio buttons, choosers), `201-235` (split threshold), `250-262` (Next, Save); template lines `61-62`, `162`, `170`, `177`, `180`; `Linux/frontend/src/ScriptFile.java:68-72`; `Linux/script.txt:149-167`
- **What it does:**
  - Two radio buttons choose the pathway source:
    - "List of KEGG XML Files to Merge" enables a chooser for a text file listing KGML paths (stored as `TxtFile`), sets `XmlFile` to null and `KEGG` to `""`;
    - "XML File of Merged KEGG Pathways" enables a chooser for one pre-merged XML (stored as `XmlFile`), sets `TxtFile` to null and `KEGG` to `";;"`.
  - On opening, the button whose file is set is selected (the XML one if both are). Selecting it this way does not run its action, so `KEGG` keeps its stored value.
  - "Node Split Threshold" is a text field read with `Integer.parseInt` on Next and Save. It becomes `$NODE_SPLIT_THRESHOLD` for both `mff` lines and `rgx` (S2.07).
  - The mode reaches the batch file through `%__c__%`, which ScriptFile replaces by `KEGG`'s value at the start of five template lines: `let $LIST_OF_MERGED_KEGG_XML_FILE`, the first `mff`, the first `wgx`, `exec add` and the `exec echo` that adds the additional-edges XML to the merge list. With `;;` each line becomes a comment (S1.06), for example `Linux/script.txt:149`: `;;mff $LIST_OF_MERGED_KEGG_XML_FILE xml $NODE_SPLIT_THRESHOLD n $INTER_DB_MAP_FILE -1`. The rest of graph building is S8.23.
- **Problems:**
  - The mode is stored as a comment prefix for template lines, so the session file says `"KEGG": ";;"`.
  - The pre-merged mode also comments out the additional-edges step, so an Additional Edges File chosen in S8.16 is silently ignored.
  - The list mode relies on `"" + null`: its template line becomes `let $MERGED_KEGG_PATHWAYS_XML_FILE null`, and the first `wgx` writes the merged graph to a file called `null` in `Linux/`. After a save and a reopen the field is `""` instead (S8.05); the variable is then empty, and `wgx` and the merge list get an empty file name. The list mode also always fails at `exec add` (S8.24).
  - Next requires no file in either mode.
  - A non-numeric split threshold throws inside the button handler, so Next and Save silently do nothing.
  - Both sessions in `arjun.json` and the generated `script.txt` use the pre-merged mode. `Linux/output_script.txt`, made from `arpit.json`, where `KEGG` is `""`, is a list-mode script: its `mff`, `wgx`, `exec add` and `exec echo` lines (149, 157, 164 and 167) are not commented out, and lines 61-62 have empty values.
- **Proposed call:** fix. Keep the choice between a KGML list and a pre-merged graph, but store it as an explicit choice, keep the additional-edges step in both modes, and require the chosen file.
- **Decision:** fix, as proposed (prompt log entry 091)

### S8.09 · Reach path bound and edges-to-target restriction

- **FuSION 1:** `Linux/frontend/src/ReachPathBoundCard.java:17-67` (bound), `69-101` (restriction), `210-254` (Next, Save); template lines `11-13`, `24`, `34`, `229`, `282`; `Linux/frontend/src/ScriptFile.java:13`, `29`, `40`; `macOS/frontend/src/ReachPathBoundCard.java:58-79`, `224`, `252`
- **What it does:**
  - "Maximum Signalling Path Length" is a slider from 0 to 100 with a text field. Moving the slider stores the value at once; a typed value is stored only when Enter is pressed and it lies in 0 to 100. It becomes `$REACH_PATH_BOUND`, the bound of both `fb_rch` lines and of both `pathz3` lines.
  - "Final Edges in pathways restricted to:" offers "Activation edges only", "Expression edges only" and "No restriction", stored as 1, 2 and 0. ScriptFile writes the matching variable, `$ONLY_ACT_EDGES_TO_TARGET`, `$ONLY_EXPR_EDGES_TO_TARGET` or `$BOTH_ACT_EXPR_EDGES_TO_TARGET`, which the template defines as 1, 2 and 0 (lines 11-13) and passes as `fb_rch`'s last argument (S4.05).
  - Next and Save refuse to go on without a restriction ("Select restriction.").
  - The page also opens the solver dialog (S8.10).
- **Problems:**
  - A typed bound is lost, without a message, unless Enter was pressed before Next or Save. `macOS/frontend/src/ReachPathBoundCard.java:58-79` already commits it on focus loss and before Next and Save.
  - 0 is accepted and gives an empty corridor (S4.18).
  - The restriction labels promise more than fb_rch does (S4.05).
- **Proposed call:** fix. Keep both inputs, but commit a typed bound without Enter, as the macOS copy does, and reject a bound below 1.
- **Decision:** fix, as proposed (prompt log entry 091)

### S8.10 · Solver configuration dialog

- **FuSION 1:** `Linux/frontend/src/ReachPathBoundCard.java:107-170`, `260-279` (`configPanel`); template lines `45-48`; `Linux/frontend/src/ScriptFile.java:46-49`
- **What it does:** "Configure", next to "Backend solver configuration:", opens a modal "Solver Configuration" dialog with four integer fields: Increment Solver Timeout, Overall Solver Timeout, Solutions to Count and Solutions to Explore. OK stores all four in the session; Cancel discards them. They become `$CONSTR_SOLVER_TIMEOUT1`, `$CONSTR_SOLVER_TIMEOUT2`, `$NUM_SOLNS_TO_COUNT` and `$NUM_SOLNS_TO_EXPLORE`, the last four positional arguments of each `pathz3` line (stage 5). The batches read the timeouts as milliseconds (the b10 runs' 3600000 is one hour); the dialog shows no unit.
- **Problems:** A field that does not parse shows "Invalid Inputs", but OK then stores 0 for that field and closes the dialog anyway (lines 153-163), so a 0 timeout reaches pathz3 without a further message.
- **Proposed call:** fix. Keep the four settings, show their units, and reject an invalid entry instead of storing 0.
- **Decision:** fix, as proposed, with the settings mapped onto S5.21's per-query timeout and whole-solve budget and S5.26's solutions on request (prompt log entry 092)

### S8.11 · Relaxation bounds page

- **FuSION 1:** `Linux/frontend/src/RelaxationBoundsPanel.java:8-77`, `81-191` (cards and steppers), `201-214` (`makeSlider`); `Linux/frontend1/src/RangeSlider.java:64-97` (`Linux/frontend/src/RangeSlider.java` differs only in line endings); `RangeSliderUI.java`; template lines `25-28`; `Linux/frontend/src/ScriptFile.java:30-34`
- **What it does:** Two cards, "Node Relaxation Bounds" and "Edge Relaxation Bounds", each have lower and upper steppers (−, field, +) and a two-thumb slider over 0 to 100. Typed values are clamped to 0 to 100 and committed on Enter or focus loss; the slider keeps lower ≤ upper, pulling a stepper's value back to the other thumb. Next and Save store the four values, which become `$NODE_RELAX_LB`, `$NODE_RELAX_UB`, `$EDGE_RELAX_LB` and `$EDGE_RELAX_UB` in both `pathz3` lines.
- **Problems:**
  - A stored lower bound above 50 puts the slider's lower thumb at 50: `RangeSlider.setValue` cannot move it past the model's initial upper value, 50, because `makeSlider` sets the lower value before the upper one (`RelaxationBoundsPanel.java:205-206`, `RangeSlider.java:72`). The stepper field still shows the stored value (`RelaxationBoundsPanel.java:131`). The stored value survives until any change on that card, which then rewrites it to 50. One batch uses a lower bound above 0: `runs/egfr_b10_gsea_fdr044_candidates/r5_mcl1_stat3/resolve_wo_n7_e0/resolve_wo_n7_e0.batch` sets `$NODE_RELAX_LB 7`, the exception in S8.06's "84 of the 85".
  - The values stay in the page until Next or Save (S8.03).
- **Proposed call:** keep. Four bounded integers with lower ≤ upper is the right input; the slider fault is specific to the Swing control.
- **Decision:** keep (prompt log entry 092)

### S8.12 · Node IDs page

- **FuSION 1:** `Linux/frontend/src/IdEntryPanel.java:36-134` (rows), `154-214` (Enter and suggestion handlers), `216-233` (not-merged file, symbol map), `406-503` (`performExactMatch`, `loadMappingFile`, `SuggestionUpdater`); template lines `21-23`, `63`; `Linux/frontend/src/ScriptFile.java:26-28`, `70`; reader `Linux/src/GraphManagerNew.cc:18688-18707`
- **What it does:**
  - "HSA Node IDs" has three text fields, Source HSA ID, Target HSA ID and Candidate Implicated Node HSA ID (the FSC), each with a drop-down of suggestions.
  - Suggestions come from the symbol map (`HsaToGene`, S8.13), read into a map from lower-cased symbol to ids. Every keystroke rescans the map and lists `<id> - <symbol>` entries: exact symbol or id matches first, then substring matches.
  - Picking a suggestion, or pressing Enter on an exact symbol or id, stores `<id> - <lower-case symbol>`, for example `"SourceHsaID": "hsa1956 - egfr"` (`arjun.json:42`).
  - ScriptFile strips a leading `hsa` and the template puts it back, so `let $SRC_NODE hsa%__2__%` becomes `let $SRC_NODE hsa1956 - egfr`. `let` keeps only the first value token (S1.08), so `$SRC_NODE` is `hsa1956`; the same holds for `$TGT_NODE` and `$NODE_TO_TEST_FOR_SIGNIFICANCE`.
  - "HSA IDs not to be merged" chooses a file of ids (stored as `HSANotMerged`), which the script appends to the merge-exception file (S8.22).
- **Problems:**
  - Enter with no exact match shows "No exact match found." and still stores the typed text: `performExactMatch` returns `true` on both paths (lines 416 and 421). With no symbol map loaded (the default, S8.13), typing `EGFR` stores `EGFR`, and the script names the node `hsaEGFR`.
  - Text typed without Enter or a pick is not stored. When the page is reopened, the fields start empty and the not-merged label reads "No File Selected", whatever the session holds.
  - An empty field writes `let $SRC_NODE hsa` (`Linux/output_script.txt:21-23`). An unknown source or target only gets a warning from fb_rch (S4.02). An unknown or empty FSC is skipped without a message by pathz3's node-file reader, which ignores ids with no node; the with-FSC and without-FSC solves are then the same problem, and the run is no significance test at all.
  - The stored text relies on `let` ignoring extra tokens, which stage 1 decided 2.0 rejects (S1.08).
  - Only one source, one target and one FSC can be entered, although fb_rch takes several (S4.10).
- **Proposed call:** fix. Keep the symbol-aware lookup, but store the bare id, accept only ids found in the map, show the stored values, and require all three ids.
- **Decision:** fix, with the FSC optional: keep the symbol-aware lookup, store bare ids, accept only ids found in the map and show the stored values; the source and target are required, and a run without an FSC is a single solve, as in r7 and r8 (prompt log entry 092)

### S8.13 · Mapping files dialog and the `start` line

- **FuSION 1:** `Linux/frontend/src/IdEntryPanel.java:26`, `137-149`, `235-338` (dialog), `345-356` (Next, Save); `Linux/frontend/src/UserInput.java:90-93`, `129-130`; template lines `58-60`, `153`, `162`, `188`; `Linux/frontend/src/ScriptFile.java:62-67`
- **What it does:** "Change Mapping Files", on the node-IDs page, opens a dialog with three choosers: "HSA to Gene Symbol Map" (`HsaToGene`), "HSA path to Path Name Map" (`HsaToPath`) and "Cross Database Map" (`InterDB`). A new symbol map is loaded for the suggestions at once. The paths become `$HSA_TO_GENE_SYMBOL_MAP_FILE`, `$HSA_PATH_TO_PATH_NAME_MAP_FILE` and `$INTER_DB_MAP_FILE` in `start $INTER_DB_MAP_FILE $EXCEPTION_TO_NODE_MERGE_FILE $HSA_TO_GENE_SYMBOL_MAP_FILE $HSA_PATH_TO_PATH_NAME_MAP_FILE` (template line 153; S1.15 to S1.19); the cross-database map also goes to both `mff` lines.
- **Problems:**
  - The defaults, `hsa_to_gene_symbol_map_6334.txt` and ` hsa_path_to_path_name_map_2822.txt`, are relative to `Linux/`, where neither exists; the shipped copies sit in `Linux/frontend/` (S8.31). A new session therefore has no suggestions, and `start` runs with empty display-name and pathway-name maps, with only an error line on stderr to show for it (S1.15). `Linux/output_script.txt:58-60` is this case.
  - The dialog edits the session's own array (line 26 takes the array `getMappingFile()` returns), so a pick is kept even if the dialog is closed without OK. It has no Cancel and no way to clear a file.
  - An empty cross-database map gives `start` and both `mff` lines an empty token (S1.07), which FuSION 1 tolerates.
- **Proposed call:** fix. Ship the two reference maps with 2.0 and use them by default from a checked location, keeping the three choosers for other maps.
- **Decision:** fix with two choosers: ship the two reference maps and use them by default from a checked location, with choosers for other symbol and pathway-name maps; a cross-database input waits for Phase 2's id mapping (S2.14) (prompt log entry 092)

### S8.14 · Threshold filter page

- **FuSION 1:** `Linux/frontend/src/ThresholdFilterPanel.java:31-49` (symbol map), `56-145` (layout), `167-201` (Prev, Next, Save), `235-245` (Filter, Search), `434-520` (`processFiles`, `readFileScores`); template lines `31-32`, `137`; `Linux/frontend/src/ScriptFile.java:38-39`
- **What it does:**
  - The top row has "Down Threshold" and "Up Threshold" fields, filled from the session, a "Filter" button, and a search field with "Search" and "Override" (S8.15).
  - Filter checks that both fields are numbers and that down ≤ up ("Lower bound is greater than upper bound"), stores both in the session, and previews the LFC file in seven columns: "Up Regulatory Threshold:", "Not Differentially Expressed:", "Down Regulatory Thresholds:", and four columns that only overrides fill, "Active Node File:", "Inactive Node File:", "Relaxed Node File:" and "Non-Relaxed Node File:". Each entry reads `<id> - <symbol> (<value>)`; with the EGF file and ±1.01, JUN appears in the first column as `hsa3725 - JUN (2.41)`.
  - The preview has a parser of its own: each line is split on whitespace, only lines with exactly two fields count, the value is read with `Double.parseDouble`, and a gene goes to Up when its value ≥ up, otherwise to Down when ≤ down, otherwise to Not DE.
  - The thresholds become `$UP_REG_THRESH` and `$DOWN_REG_THRESH` for the script's `cudf` (template line 137), which classifies the genes again for the run (stage 3).
- **Problems:**
  - Only Filter stores the thresholds; Next and Save ignore the two fields, so typed thresholds are lost unless Filter is pressed. `macOS/model_example/RUNNING.md` warns users about exactly this.
  - The preview is a fifth LFC parser with rules of its own (S3.14 lists four):
    - a line with three or more fields, or with leading whitespace, is skipped silently, where cudf reads the first two tokens;
    - one non-numeric value on a two-field line, such as a header `gene<TAB>lfc` or `hsa1<TAB>NA`, aborts the whole preview with "Error reading tSV: …";
    - a repeated id keeps its last value, where cudf classifies every line;
    - with down = up, a gene at that value is shown as Up only, where cudf puts it in both lists (S3.07).
  - The preview is not what the run uses: cudf reclassifies from the file, and the override columns reach the run only as broken text (S8.15).
  - The 6334-line symbol map is read again on every visit, after S8.12 read it; the cost is negligible.
- **Proposed call:** fix. Keep a live preview of the up and down lists, but take the thresholds as typed and compute the preview with the backend's single LFC parser (S3.14's decision), so it shows exactly what cudf will produce.
- **Decision:** fix, as proposed (prompt log entry 093)

### S8.15 · Bug: node overrides reach the script as gene text

- **FuSION 1:** `Linux/frontend/src/ThresholdFilterPanel.java:169-201` (override lists), `247-404` (Override dialog), `522-578` (`searchAndHighlight`), `580-589`; `Linux/frontend/src/UserInput.java:315-330`; `Linux/frontend/src/ScriptFile.java:18-21`, `54-58`; template lines `52-55`; `Linux/src/GraphManagerNew.cc:18688-18707`
- **What it does:**
  - Search highlights every preview line containing the text, ignoring case. Override is enabled unless more than one distinct line matches.
  - Override opens "Choose Transfer Destination" with three groups: "Above Up Regulatory Threshold", "Not Differentially Expressed" or "Below Down Regulatory Threshold"; "Active Node File" or "Inactive Node File"; "Relaxed Node" or "Non Relaxed Node". OK copies the line into the chosen columns; Clear removes the override and filters again.
  - Next and Save turn the overrides into one list per column. ScriptFile writes four of them, joined with commas, and writes the path of a fresh empty temporary file (`/tmp/empty_file<random>.txt`) for a list that is empty:

    | Column | Variable | pathz3 input |
    |---|---|---|
    | Above Up | `$CONFIRMED_UP_REG_FILE` (`%__a10__%`) | confirmed up-regulated nodes |
    | Below Down | `$CONFIRMED_DOWN_REG_FILE` (`%__a11__%`) | confirmed down-regulated nodes |
    | Inactive | `$INACTIVE_NODES_FILE` (`%__a9__%`) | inactive nodes |
    | Relaxed | `$RELAXED_NODES_FILE` (`%__a12__%`) | relaxed nodes |
    | Not DE, Active, Non-relaxed | not written | — |

    With no overrides all four get the temporary file, as in `Linux/output_script.txt:52-55`: `let $INACTIVE_NODES_FILE /tmp/empty_file13231154830371008063.txt`.
- **Problems:**
  - The lists hold display text, not file paths. Overriding JUN into Inactive writes `let $INACTIVE_NODES_FILE hsa3725 - jun (2.41)`; `let` keeps `hsa3725` (S1.08), and pathz3 tries to open a file named `hsa3725` in `Linux/`, prints `File hsa3725 could not be opened.` on stderr and uses an empty set. No override ever reaches the solve.
  - The override keys are lower-cased display lines (line 544). Removing the original entry from its column (line 336) then fails for any upper-case symbol, so the gene shows in two columns, and a new Filter does not recognise the key as an id (line 469).
  - Search alone records an override: it stores one match, an arbitrary element of a `HashSet` (line 564), with the columns of every matching line (line 568), even when several lines match and Override stays disabled. Searching for EGFR's id while EGFR sits in the Up column puts it in the next script's confirmed-up list. A search that finds nothing overwrites the previous match's entry with an empty one.
  - Three of the seven columns (Not DE, Active, Non-relaxed) are never written.
  - Overrides are not saved with the session (S8.05).
  - The temporary file is deleted when the JVM exits; a run still going then loses it (S8.28).
- **Proposed call:** fix. Keep per-gene overrides, but write each kind to a one-id-per-line file in the run directory, pass every kind pathz3 accepts, and store the overrides with the session.
- **Decision:** fix, as proposed, with the three kinds the GUI never wrote wired too: Active and Non-relaxed join pathz3's active and non-relaxed node files, and Not DE removes the gene from the observed up and down lists (open question 3; prompt log entry 093)

### S8.16 · Edge files page and the coexpression inputs

- **FuSION 1:** `Linux/frontend/src/EdgesChooserPanel.java:13-126`, `128-169` (`createFileRow`), `171-231` (`createIntRow`); `Linux/frontend1/src/EdgesChooserPanel.java:81-122` (frontend1's Submit); template lines `37`, `50-51`, `56-57`, `70-72`; `Linux/frontend/src/ScriptFile.java:37`, `43`, `51-53`, `59-60`; reader `Linux/src/GraphManagerNew.cc:18739-18748`
- **What it does:**
  - "Edge Files" ("Optional files to constrain the pathway search") has five file choosers, "Additional Edges File", "Essential Edge File", "Avoid Edge File", "Relaxed Edges File" and "Non Relaxed Edges File", a "Coexpression CSV File" chooser limited to `*.csv`, and two integer steppers, "Coexpression Threshold (COEXP_THRESH)" and "Frozen Threshold (FROZEN_THRESH)", changed by ±1 or by typing (committed on Enter or focus loss).
  - Each row shows the chosen file's name or "No file selected".
  - The five edge files become `$ADDITIONAL_EDGES_FILE`, `$ESSENTIAL_EDGES_FILE`, `$AVOID_EDGES_FILE`, `$RELAXED_EDGES_FILE` and `$NONRELAXED_EDGES_FILE`. An unchosen file is written as an empty value, which pathz3 treats as an unopenable file and an empty set. The additional-edges file feeds S8.24; the coexpression inputs are meant for `$COEXPRESSION_CSV`, `$COEXP_THRESH` and `$FROZEN_THRESH` (S8.17).
  - The last button differs between the two GUIs:
    - `frontend`: "Next →" stores the page, saves the session and opens the pipeline editor (S8.18);
    - `frontend1`: "▶  Submit" stores the page, saves the session, fills the template (S8.20) and starts the run (S8.28).
- **Problems:**
  - Save on this page only copies its values into the in-memory session and never writes the session file (`EdgesChooserPanel.java:104`; `frontend1` line 108); only Next, or `frontend1`'s Submit, writes it.
  - A chosen file cannot be removed again.
  - The edge-file formats are neither shown nor checked, and the thresholds accept negative numbers.
- **Proposed call:** keep. The five optional edge files and the coexpression inputs are all live pathz3 inputs; S8.17 fixes how they reach pathz3, and S8.30 checks them.
- **Decision:** keep; S8.17 fixes how the inputs reach pathz3, S8.30 checks them, and Phase 5's navigation (S8.03) covers Save and clearing a choice (prompt log entry 093)

### S8.17 · Bug: the coexpression and frozen-edge settings never reach `pathz3` (Linux)

- **FuSION 1:** `Linux/frontend/src/ScriptFile.java:24-72` (no entry for `%__a21__%` to `%__a23__%`); template lines `70-76`, `258`, `321`; `Linux/src/main.cc:4527-4563`; `macOS/frontend/src/ScriptFile.java:72-80`; `macOS/src/main.cc:4535-4582`
- **What it does:** The template defines `let $COEXPRESSION_CSV %__a21__%`, `let $COEXP_THRESH %__a22__%`, `let $FROZEN_THRESH %__a23__%`, `let $NEW_CONSTRAINTS_ENABLED 1` and `let $Z3_RANDOM_SEED 42`, and ends both `pathz3` lines with these five variables. ScriptFile has no replacement for the first three, so the placeholders reach the batch file as they are. Linux pathz3 then reads its optional arguments as follows:
  1. `%__a21__%` is taken as the coexpression CSV path;
  2. `stof("%__a22__%")` fails, so the coexpression threshold keeps its default, −1, and the parser does not move on;
  3. the frozen threshold, the enable flag and the seed are each tried on that same `%__a22__%` and fail, so `new_constraints_enabled` stays false.

  pathz3 prints `[new-constraints] enabled=0 z3_random_seed=42` and emits neither coexpression nor frozen-edge constraints, whatever the user entered.
- **Problems:**
  - Every Linux GUI run is solved without the coexpression and frozen-edge constraints, although the page asks for them and the session stores them (`arjun.json:46`, `58`, `67`: threshold 700, a CSV path, frozen 800).
  - The failure is silent: no message names the unreadable tokens.
  - Filling the placeholders would not be enough on Linux. With no CSV chosen, `$COEXPRESSION_CSV` is an empty token, and the Linux parser stops at an empty slot without moving on (`main.cc:4533-4535`), so the thresholds and the enable flag are never read and the frozen-edge constraints are lost along with the coexpression ones.
  - The macOS tree fixes both halves: `macOS/frontend/src/ScriptFile.java:72-80` fills the three placeholders, and `macOS/src/main.cc:4535-4582` consumes every optional slot, blank or not. Neither change is in the Linux tree.
- **Proposed call:** fix. Fill all three values, give every optional pathz3 argument a fixed slot that is consumed whether blank or not (as the macOS code does), and stop on any unfilled placeholder (S8.20).
- **Decision:** fix, as proposed; the backend half is S5.02's decision (prompt log entry 094)

### S8.18 · Pipeline editor page (`Gui4Panel`, `gui4`)

- **FuSION 1:** `Linux/frontend/src/Gui4Panel.java:13-132`; `Linux/frontend/src/gui4.java:15-246` (state, built-in descriptions, entry-to-variable map, constructors), `247-554` (pipeline save and load), `733-910` (layout and buttons), `911-1421` (input-data panel), `1422-1555` (history), `2164-2189` (built-in blocks), `2406-2455` (Execute), `3062-3161` (ordering and cycle check), `3998-4085` (execution plan); `Linux/Data_Coming_From_Frontend.txt`; `Linux/frontend/frontend workflow/README.md`, `execution.txt`, `execution_history.txt`, `Data_Coming_From_Frontend.txt`
- **What it does:**
  - The last `frontend` page embeds a block editor (`gui4`, 4601 lines, the "ACTREC Frontend" of `frontend workflow/README.md`), taken from a hidden `JFrame` built anew on each visit, with "💾  Save Session" and "▶  Submit" below it.
  - Before building the editor, `Gui4Panel` writes `Data_Coming_From_Frontend.txt` to the launch directory: one `name,type,value` line per session value that is set, file paths cut to their base names. Real file, lines 1-5: `log_fold_changes,file,egf_hela_t120_lfc.tsv`, `kegg_xml,file,merged_kegg_master_graph_with_exp_score.xml`, `hsa_to_gene_map,file,hsa_to_gene_symbol_map_6334.txt`, `hsa_path_map,file,hsa_path_to_path_name_map_2822.txt`, `signalling_path_length,integer,6`. The editor's "Input Data" side panel shows these as cards that can be connected to block inputs.
  - The editor has ten built-in blocks, `let`, `cudf`, `exec`, `start`, `mff` (6 inputs), `wgx` (3), `rgx` (3), `fb_rch` (8), `pathz3` (28) and `result`, plus user-defined ones ("+Tmpl"). Blocks can be added, dragged, renamed, searched, zoomed and connected output to input, with type checks over float, integer, string, file, graph, Status and char.
  - **Execute** orders the blocks and writes `execution.txt`, a plan in a notation of its own, not batch syntax, and adds it to `execution_history.txt` (the last 50 runs, each ended by `===END_ENTRY===`). Real plan, `frontend workflow/execution.txt:6-11`: `let $cudf_2`, `    input1 default`, `    input2 0`, `    input3 0`, `    output1 $cudf_2.output1`, `    output2 $cudf_2.output2`.
  - **Save** and **Load** write and read a `.pipeline` text file. The same text is kept in the session as `pipelineData` and restored when the page opens. Format: `#PIPELINE_V1`, `ZOOM=`, `FUNC_COUNTER=`, one `INST=<template>,<count>` per template, `BLOCK_COUNT=`, for each block `BLOCK_BEGIN=<i>` with `B_NAME`, `B_ORIG`, `B_TMPL`, `B_INCOUNT`, `B_OUTCOUNT`, `B_INTYPE`, `B_OUTTYPE`, `B_INVAL`, `B_OUTVAR`, `B_X`, `B_Y`, `B_W`, `B_H` and `B_HIST` lines and `BLOCK_END`, then `CONN=<from>,<out>,<to>,<in>,<type>,<file>` and `FENTRY=<name>,<type>,<value>,<row>,<block>,<input>,<file>` lines, then `#END`; values are URL-encoded. The only stored pipeline, `arjun.json:49`, is empty (`BLOCK_COUNT=0`).
  - **Submit** saves the session, pipeline included, and runs the template exactly as S8.20 describes.
- **Problems:**
  - Nothing in the editor affects a run: Submit ignores the drawn pipeline, Execute writes a plan nothing reads, and "Gen Script" writes a file nothing runs (S8.19).
  - The cycle check cannot fire. When the queue empties, the ordering adds any unvisited block (lines 3094-3102), so every block always gets ordered and the "Circular dependency detected" branch (3104-3121) never runs; a cycle just gets an arbitrary order.
  - `Data_Coming_From_Frontend.txt` keeps only base names, so its entries cannot locate any file, and it labels the KGML list `microarray_data` (`Gui4Panel.java:96`). `frontend workflow/Data_Coming_From_Frontend.txt` documents a different, 42-line format (`working_directory,string,/home/user/FuSION/example`) that no code writes.
  - Each visit rebuilds the editor window and rereads its files from disk.
- **Proposed call:** drop. The editor never influences a run; the flexible step order and partial runs it points towards are designed in Phase 2 (decisions [0018](../decisions/0018-batch-interface.md) and [0019](../decisions/0019-partial-runs.md)).
- **Decision:** keep and make it work. The standard run (S8.20's template) becomes a default pipeline the editor opens with; users edit, add or remove blocks, and every Submit turns the drawn pipeline into a batch file in the batch language (decision 0018). The problems above are fixed as part of making it work (prompt log entries 094 and 095)

### S8.19 · Pipeline editor: "Gen Script" and the block library files

- **FuSION 1:** `Linux/frontend/src/gui4.java:4086-4511` (`generateScriptFromPipeline`, `guiCmd`, `generateAdvtempScript`), `4513-4569` (input and output names), `4571-4589` (`getVariableForPosition`, never called), `144-178` (`ENTRY_VAR`), `556-682` (library load and save); `Linux/frontend/src/block_templates.txt`, `block_descriptions.txt`; `Linux/frontend/frontend workflow/block_templates.txt`, `block_descriptions.txt`, `generated_script.txt`, `README.md`
- **What it does:**
  - "📄 Gen Script" asks for a file name (default `generated_script.txt`) and writes a template with unfilled placeholders, with the same header, variables and `exec` lines as the template. A `cudf`, `start`, second `mff`, second `wgx`, `rgx`, `fb_rch` or `pathz3` line is written only for a block of that type on the canvas, with the block's typed values or the variables of connected inputs. The first `mff` and `wgx` are always written; other blocks follow at the end; then `exit`. It writes the three coexpression `let` lines but not `$NEW_CONSTRAINTS_ENABLED` or `$Z3_RANDOM_SEED`.
  - The block library is read from `block_templates.txt` (records of `TEMPLATE:`, `INPUTS:`, `OUTPUTS:`, `INTYPES:`, `OUTTYPES:`) and `block_descriptions.txt` (records of `BLOCK:`, `FULLNAME:`, `DESC:`, `CATEGORY:`) in the launch directory and completed with the built-ins. New templates and edited descriptions are written back there.
- **Problems:**
  - Nothing reads the generated file; runs use the template (S8.20).
  - Its output would not run as written. Placeholders are unfilled. An input connected to an "Input Data" card becomes `$` plus a name from `ENTRY_VAR`, and several of those the template never defines (`$KEGG_XML_FILE`, `$MICROARRAY_DATA_FILE`); an unconnected output becomes `$<block>_out<n>`, also undefined (S1.07 leaves both as literal text). An empty input value leaves a gap that tokenising closes up (S1.05), shifting every later argument; `frontend workflow/generated_script.txt:8`, `cudf     `, is such a line from an older version of the generator.
  - The shipped library files contradict the built-ins and the commands. `block_templates.txt` gives `rgx`, `fb_rch`, `pathz3`, `let` and `exec` two integer inputs and one file output, and `mff` one float input; run with `frontend/src/` as working directory, as the `java gui4` of `frontend workflow/README.md` would be, they replace the built-ins. The descriptions are wrong for FuSION: `cudf` "Processes package dependency files and generates upgrade plans", `mff` "Computes maximum flow in a network graph", `fb_rch` "Final Bound Reach". The `frontend workflow/` copies differ again (`OUTTYPES:status`, `DESC:DESCRIPTION:` prefixes).
  - `frontend workflow/README.md` describes cloning a separate repository (`tanishasenapati13/ACTREC-frontend`), running `java gui4` and JDK 8.
- **Proposed call:** drop, with S8.18. Its output is never run, and its block library contradicts the real commands.
- **Decision:** keep and correct: Gen Script writes a runnable batch file of the drawn pipeline, and the block library's inputs, outputs, types and descriptions match the real commands (prompt log entries 094 and 095)

### S8.20 · Filling the batch template (`ScriptFile`)

- **FuSION 1:** `Linux/frontend/src/ScriptFile.java:8-84`; `Linux/advtempscript.txt`; `Linux/output_script.txt`, `Linux/script.txt`; `Linux/frontend/advtempscript.txt`, `Linux/frontend1/advtempscript.txt`
- **What it does:** On Submit (S8.18; in `frontend1`, S8.04 and S8.16), ScriptFile:
  1. reads `advtempscript.txt` from the launch directory, which is `Linux/advtempscript.txt`;
  2. creates an empty temporary file `/tmp/empty_file<random>.txt`, deleted when the JVM exits;
  3. replaces every occurrence of each placeholder by plain text substitution:

     | Placeholder | Value | Template line |
     |---|---|---|
     | `%__1__%` | working directory | 20, `$WORK_DIR` |
     | `%__2__%`, `%__3__%`, `%__4__%` | source, target, FSC, leading `hsa` removed | 21-23, after a literal `hsa` |
     | `%__5__%` | path bound | 24 |
     | `%__6__%`, `%__7__%`, `%__8__%`, `%__9__%` | edge lower, edge upper, node lower, node upper bound | 25-28 |
     | `%__10__%` | LFC file's base name | 29, `$LOGFOLDCHANGES` |
     | `%__11__%` | additional-edges file's base name | 30, `$ADDITIONALEDGES`, used nowhere |
     | `%__12__%`, `%__13__%` | up and down threshold, as Java prints a `double` (`0.99`) | 31-32 |
     | `%__14__%` | `$BOTH_ACT_EXPR_EDGES_TO_TARGET`, `$ONLY_ACT_EDGES_TO_TARGET` or `$ONLY_EXPR_EDGES_TO_TARGET` | 34 |
     | `%__lf__%`, `%__ef__%` | LFC file, additional-edges file | 36-37 |
     | `%__a1__%` to `%__a5__%` | split threshold, timeout 1, timeout 2, solutions to count, solutions to explore | 44-48 |
     | `%__a6__%` | additional-edges file | 49, inside a comment |
     | `%__a7__%`, `%__a8__%` | essential, avoid edges file | 50-51 |
     | `%__a9__%` to `%__a12__%` | override lists or the temporary file (S8.15) | 52-55 |
     | `%__a13__%`, `%__a14__%` | relaxed, non-relaxed edges file | 56-57 |
     | `%__a15__%`, `%__a16__%`, `%__a17__%` | cross-database, symbol, pathway-name map | 58-60 |
     | `%__a18__%`, `%__a19__%` | KGML list, pre-merged XML, as `"" + value`, so `null` when unset | 61-62 |
     | `%__a20__%` | not-merged ids file | 63 |
     | `%__c__%` | `""` or `;;` (S8.08) | 61, 162, 170, 177, 180 |
     | `%__a21__%` to `%__a23__%` | nothing (S8.17) | 70-72 |

  4. writes the result to `output_script.txt` in the launch directory and prints `Template filled and saved to output_script.txt`.

  The template's own sections are S8.21 to S8.26. It also defines eight unused min-cut constants (`$COEFF1` to `$EXP4`, lines 96-103). Real outputs: `Linux/output_script.txt` (342 lines, from `sessions/arpit.json`, made with a template that had no coexpression lines) and `Linux/script.txt` (342 lines, pre-merged mode). Both are tracked in git.
- **Problems:**
  - Three templates exist and two are never read when the GUI runs from `Linux/`: `frontend/advtempscript.txt` (345 lines; it has the three coexpression `let` lines but `pathz3` lines without the five optional arguments) and `frontend1/advtempscript.txt` (342 lines; no coexpression lines).
  - Nothing ties the GUI's fields to the template's placeholders: three placeholders have no value (S8.17), and leftovers are not detected.
  - Values are inserted unquoted. A value with a space, such as a path or a stored ID like `hsa1956 - egfr`, becomes several tokens, of which `let` keeps the first (S1.08, S8.12, S8.30); an empty value gives `let $X ` (S1.07).
  - The edge restriction is used as an array index, so its default, −1, throws (S8.03, S8.06).
  - `output_script.txt` is overwritten by every run, and two generated scripts are committed; stage 1 counts them, with the template, among "the 81 batch files", although they are outputs, not inputs.
- **Proposed call:** fix. Keep generating a batch file from the session (the batch language stays, decision 0018), but from one template owned with the backend, using named arguments, with every value checked, quoted where needed (S1.05's decision), and a stop on any unfilled placeholder.
- **Decision:** fix, as proposed (prompt log entry 094) *With S8.18 (prompt log entry 095):* the template is the editor's default pipeline, and every Submit turns the drawn pipeline into the batch file.

### S8.21 · Generated script: run directory

- **FuSION 1:** template lines `110-130`, `113-114`, `204`, `267-269`, `290-291`, `329-331`, `343`, `352`; `Linux/src/main.cc:866-887` (`make_smt_dump_dir_for`)
- **What it does:**
  - `$WRITE_SUBDIR` is `$SRC_NODE_to_$TGT_NODE_fsc_$NODE_TO_TEST_FOR_SIGNIFICANCE_$LOGFOLDCHANGES_b$REACH_PATH_BOUND`, for example `hsa1956_to_hsa3576_fsc_hsa3725_egf_hela_t120_lfc.tsv_b10`, and `$WRITE_DIR` is `$WORK_DIR/$WRITE_SUBDIR`. These expand correctly under S1.07's prefix rule.
  - `exec if [ -d $WRITE_DIR ] ; then /bin/rm -rf $WRITE_DIR; fi` deletes any existing directory of that name, and `exec mkdir $WRITE_DIR` creates it.
  - The run's files go there: the input lists (S8.22), `list_of_xml_files_new.txt`, `merged_master_graph.xml`, `merged_edges_file`, the node files of S8.25, pathz3's `temp_w*` and `temp_wo*` files, `fb_rch`, `fb_rch_back`, the `<…>_w_<…>` and `<…>_wo_<…>` pairs of `_limits_timefile.txt` and `.dat` files, the plot `<WRITE_SUBDIR>.png` and `mylog.txt`. The fixed-name ones are moved there from the launch directory with `exec mv` (S1.26).
- **Problems:**
  - Data loss. A rerun with the same source, target, FSC, LFC file name and bound deletes the earlier results without asking, even when the thresholds, relaxation bounds, edge files or coexpression settings differ, since none of them is in the name.
  - Each `pathz3` creates its SMT dump directory, `<lfc>_smt-files_<YYYYMMDD_HHMMSS>`, in the launch directory (`main.cc:866-887`), and the script never moves it, so GUI runs leave two directories each in `Linux/`.
  - Fixed-name files are moved out of `Linux/` by name. A run that failed before writing one moves an earlier run's file (for example `PO.dat`) into its own directory, and two GUI runs at once take each other's files (S8.28).
  - A working directory with a space is cut at the space by `let` (S1.08), so the run deletes and writes under a different directory.
- **Proposed call:** fix. Give every run a new directory of its own, never delete one, put every output of the run in it, SMT dumps included, and record the run's full parameters there.
- **Decision:** fix, as proposed (prompt log entry 096)

### S8.22 · Generated script: input preparation

- **FuSION 1:** template lines `116-130`, `137`, `143-147`
- **What it does:**
  - `cudf $LOG_FOLD_CHANGE_FILE $UP_REG_THRESH $DOWN_REG_THRESH $UP_REG_FILE $DOWN_REG_FILE` writes `up_reg` and `down_reg` (stage 3);
  - `exec cd $WRITE_DIR; echo $SRC_NODE > $SRC_FILE; echo $TGT_NODE > $TGT_FILE` writes `src_file` and `tgt_file`;
  - `exec cd $WRITE_DIR;` does nothing, since each `exec` gets its own shell (S1.14);
  - `exec echo $SRC_NODE $TGT_NODE > $CONNECT_PAIRS_FILE` writes `connect_pairs`;
  - `exec cat $UP_REG_FILE $DOWN_REG_FILE $SRC_FILE $TGT_FILE $HSA_IDS_NOT_TO_BE_MERGED_FILE > $EXCEPTION_TO_NODE_MERGE_FILE` builds the merge-exception file for `start` (S1.17);
  - `exec touch $WRITE_DIR/empty_file` makes the with-FSC solve's empty avoid-nodes file.

  The hand-written batches use the same lines, without the no-op (the real batch's lines 145-154).
- **Problems:** This is the file plumbing through `exec` that S1.14 and S1.17 describe, with a round trip through disk for cudf's lists.
- **Proposed call:** fix. Follow S1.17's decision: the backend builds the exception set from cudf's lists, the source, the target and the not-merged file, so the frontend supplies only those inputs.
- **Decision:** fix, as proposed: the backend builds the exception set from cudf's lists, the source, the target, the FSC (S2.13) and the not-merged file (prompt log entry 096)

### S8.23 · Generated script: building the graph

- **FuSION 1:** template lines `162-221`; S8.08 for the mode
- **What it does:** In order, with the list-mode-only lines marked:
  1. list mode: `mff $LIST_OF_MERGED_KEGG_XML_FILE xml $NODE_SPLIT_THRESHOLD n $INTER_DB_MAP_FILE -1` merges the KGMLs, and `wgx 0 $MERGED_KEGG_PATHWAYS_XML_FILE KEGG` writes the master;
  2. list mode: `exec add $ADDITIONAL_EDGES_FILE $ADDITIONAL_EDGES_XML_FILE` (S8.24);
  3. `$NEW_LIST_OF_MERGED_XML_FILE` is rewritten to name `$MERGED_KEGG_PATHWAYS_XML_FILE` (the master, or the user's pre-merged XML), plus, in list mode, `additional_edges.xml`;
  4. `mff $NEW_LIST_OF_MERGED_XML_FILE xml $NODE_SPLIT_THRESHOLD y $INTER_DB_MAP_FILE -1`, then `size 0`;
  5. `wgx 0 $MERGED_PATHWAYS_XML_FILE KEGG` writes `merged_master_graph.xml`;
  6. `exec mv edges_merged_filename $WRITE_DIR/merged_edges_file`;
  7. `rgx $MERGED_PATHWAYS_XML_FILE y $NODE_SPLIT_THRESHOLD`, `size 0`, `let $MERGED_GRAPH_NUM #LASTGRAPHNUM` (S1.09).
- **Problems:**
  - In pre-merged mode the user's XML is parsed twice and written once more per run: step 4 is a one-file `mff … y`, which is a plain read (S2.27), step 5 writes it, and step 7 reads it back. GUI sessions use split threshold 15000 (S8.06). The surviving GUI run (open question 2) read the 1,945,801-byte `merged_kegg_master_graph_with_exp_score.xml` that both `arjun.json` sessions name (5307 entries, 7517 relations) and wrote a 13,375,038-byte `merged_master_graph.xml` (8237 entries, 82,188 relations), so step 5 writes about 13 MB per run and step 7 parses it back. List mode adds a second merge, write and read (S2.26).
  - In pre-merged mode the one-file `mff` writes no `edges_merged_filename`, so step 6 fails with `mv: cannot stat`, or moves a file an earlier run left in `Linux/` (S2.16).
  - Every run rebuilds the graph from scratch, even when an earlier run of the same session already built the same graph.
- **Proposed call:** fix. Build the graph once in memory (S2.26's decision), save it with the run, and let a later run of the session start from it (decision 0019).
- **Decision:** fix, as proposed (prompt log entry 096)

### S8.24 · Bug: `exec add` cannot run

- **FuSION 1:** template lines `177-180`; `Linux/src/main.cc:1318-1330` (`exec`), `5842-5934` (`add`); `Linux/add`; `macOS/advtempscript.txt:171-177`
- **What it does:** In list mode the script runs `exec add $ADDITIONAL_EDGES_FILE $ADDITIONAL_EDGES_XML_FILE`, which hands `add <in> <out>` to `/bin/sh`. `add` is a FuSION command (S2.30), and `Linux/add` is a standalone build of it, but the shell looks `add` up on `PATH`. From `Linux/` on this machine, `sh -c 'command -v add'` finds nothing, so the line fails with `add: not found` and writes no XML. The next lines still add `$WRITE_DIR/additional_edges.xml` to the merge list.
- **Problems:**
  - Every list-mode run reaches the second `mff` with a missing file in its list, and the uncaught parse exception aborts the run (S2.05, S2.16), whether or not the user chose an additional-edges file.
  - With no file chosen, even a working `add` would not help: according to the comment at `macOS/advtempscript.txt:172-176`, an empty edge list gives an XML with no entries, which fails an assertion in the reader. That template comments both lines out (and names the command `addx`, which no build has).
  - S2.30 reads this line as calling the standalone tool; it does so only when the launch directory is on `PATH`.
- **Proposed call:** fix. Call the built-in command (`add <in> <out>`), only when an additional-edges file was chosen, and in both pathway modes (S8.08).
- **Decision:** fix, as proposed (prompt log entry 096)

### S8.25 · Generated script: the two solves

- **FuSION 1:** template lines `229-258`, `267-269`, `282-321`, `329-331`
- **What it does:** As in the hand-written batches:
  1. `fb_rch 0 $SRC_NODE -1 $TGT_NODE -1 -1 $REACH_PATH_BOUND $EDGES_TO_TARGET`, then `size 0` (stage 4);
  2. with-FSC node files: the active, non-relaxed and essential node files each hold the source, the target and the FSC; the avoid file is `empty_file`; the output prefix is `$WRITE_DIR/temp_w`;
  3. `pathz3 0 $PO_SEARCH_MODE_WITH_FSC $CONNECT_PAIRS_FILE …`, 33 arguments: graph `0`, search mode `0`, 26 positional arguments and the five optional ones of S8.17;
  4. `exec mv limits_timefile.txt` and `exec mv PO.dat` to `$WRITE_DIR/<src>_to_<tgt>_w_<fsc>_<lfc>_b<bound>_limits_timefile.txt` and `….dat`;
  5. `fb_rch $MERGED_GRAPH_NUM …` with the same arguments, `size 0`, and `exec mv` of `fb_rch` and `fb_rch_back`;
  6. without-FSC node files: the active, non-relaxed and essential node files hold the source and the target; the avoid file `avoid_<fsc>_file` holds the FSC; the prefix is `$WRITE_DIR/temp_wo`;
  7. the second `pathz3` and its two `exec mv` lines (`_wo_`). Line 319 ends in a tab, so the search mode is `0<TAB>` (S1.05), which `stoi` reads as 0.
- **Problems:** The corridor is computed twice (S4.20), and the second solve waits for the first although neither reads the other's output; those costs belong to stages 4 and 5. The search mode is fixed at 0, so the GUI cannot check points against a saved curve (mode 1).
- **Proposed call:** keep. These are the commands the hand-written batches use; their costs are decided in stages 4 and 5, and their optional arguments in S8.17.
- **Decision:** keep: the default pipeline (S8.18) still runs both FSC cases, written with S5.17's named FSC on one corridor (S4.20); mode 1 is gone (S5.28) (prompt log entry 097)

### S8.26 · Generated script: Pareto plot and wrap-up

- **FuSION 1:** template lines `337-354`; the real batch's line 262
- **What it does:**
  - `exec cd $WRITE_DIR; gnuplot -p -e "set xlabel 'Node relaxn'; set ylabel 'Edge relaxn'; set xrange [-0.5:$NODE_RELAX_UB+0.5]; set yrange [-0.5:$EDGE_RELAX_UB+0.5]; plot '$PO_POINTS_WITH_FSC.dat' with lp linecolor 'blue', '$PO_POINTS_WITHOUT_FSC.dat' with lp linecolor 'red'; exit"` opens a gnuplot window that stays open, with the with-FSC curve in blue and the without-FSC curve in red;
  - `let $PLOT_FILE $WRITE_DIR/<WRITE_SUBDIR>.png`, `exec echo "Saving plot in $PLOT_FILE ..."`, and the same plot again with `set term png; set output '$PLOT_FILE'`;
  - `exec mv mylog.txt $WRITE_DIR/mylog.txt` and `exit`.
- **Problems:**
  - This is not the canonical plot. Current batches call `supporting_files/plot_po.sh` (stage 6; the real batch's line 262 passes the gene names, the bound and the coexpression settings), which styles the curves and keeps coincident points of the two curves visible. Here, equal curves hide one another.
  - `-p` opens a window on whatever machine runs the backend, and fails without a display. It is the only result the GUI shows (S8.29).
  - The plot is drawn twice.
- **Proposed call:** fix. Draw the plot once, to a file, with the canonical plot script, and show it in the frontend (S8.29).
- **Decision:** fix, as proposed: S6.22's built-in plot step, drawn once to a file and shown in the frontend (S8.29) (prompt log entry 097)

### S8.27 · Commands the GUI cannot emit

- **FuSION 1:** `Linux/advtempscript.txt` (whole file); `Linux/frontend/src/gui4.java:4105-4511`; the real batch's lines 169, 262 and 269
- **What it does:** The lead holds. Neither the template nor the editor's generator has a `gsea`, `psp`, `pex` or `plot_po.sh` line, so a GUI user cannot shortlist pathways, plot solution paths or explanation subgraphs, or get the canonical Pareto plot. Of the 88 batch files under `runs/` and `macOS/`, 19 run `gsea`, 25 run `pex` and 22 call `plot_po.sh` (two more mention it only in a comment); none runs `psp`. The real batch has `gsea $LOG_FOLD_CHANGE_FILE $LIST_OF_MERGED_KEGG_XML_FILE $GSEA_SELECTOR $GSEA_KGML_LIST` (line 169), `plot_po.sh` (262) and `pex … $SRC_NODE $TGT_NODE both all $WRITE_DIR/pex` (269). Also fixed in the template: `$NEW_CONSTRAINTS_ENABLED 1` and `$Z3_RANDOM_SEED 42`, pathz3 search mode 0, no fb_rch exclusions, one source-target pair and no `upfx`.
- **Problems:** A GUI run does strictly less than current batches do, and nothing in the GUI says so.
- **Proposed call:** fix. The 2.0 frontend must be able to request pathway shortlisting, the explanation and solution-path plots and the canonical Pareto plot, since the recent batches use them.
- **Decision:** fix, as proposed: every live step is available from the frontend as a pipeline-editor block (S8.18), and the default pipeline includes the steps current batches use (prompt log entry 097)

### S8.28 · Running the backend and the run window

- **FuSION 1:** `Linux/frontend/src/Gui4Panel.java:141-246` (`runTool`, `detectStage`, `createModernOutputWindow`); `Linux/frontend1/src/SessionFrame.java:281-391` and `Linux/frontend1/src/EdgesChooserPanel.java:128-238` (`runFusion`, `detectStage`, `dotIcon`); `Linux/frontend/src/IdEntryPanel.java:506-692`, identical in `frontend1` (never called)
- **What it does:**
  - Starts `./fusion -b output_script.txt` with `ProcessBuilder` in the launch directory, with stderr merged into stdout.
  - Opens a 1100 × 550 window with a dark log area, a stage label at the top and a status line at the bottom. A background thread reads the output line by line and appends each line with a `[HH:mm:ss] ` prefix.
  - `detectStage` trims each line and maps the command echo (S1.10) to a stage: `exec mkdir` "Initializing workspace", `cudf` "Processing fold-change data", `start` "Loading mappings & metadata", `mff` "Merging pathways", `wgx` "Writing merged graph", `rgx` "Reading graph XML", `fb_rch` "Reachability pruning", `pathz3` "Constraint solving (Z3)", a line containing `gnuplot` "Generating plots", `exit` "Finalizing". The label, the window title (`FuSION — <stage>`) and the status line (`<stage>...`) follow it.
  - When the output ends and the process has exited, the window shows "Execution Completed" and "✔ Done" and asks "Do you want to run another experiment?"; Yes opens a new sessions window for the same user without signing in.
  - If the process cannot start (no `./fusion`), the label reads "Execution Failed" and a dialog shows the exception's message.
  - The two GUIs carry near-identical copies of this code: `frontend` in `Gui4Panel`, `frontend1` in `SessionFrame` and `EdgesChooserPanel`. `frontend1` titles its window "FuSION — Running" (`frontend1/src/SessionFrame.java:298`) instead of "FuSION — Initializing" (`Gui4Panel.java:213`), and does not re-set the run icons. A fourth copy, `runTool` and `detectStage` in `IdEntryPanel` (lines 506-692 in both GUIs), is never called, yet it is the copy stage 1 cites (S1.01, S1.10).
- **Problems:**
  - The exit status is ignored (`process.waitFor()`'s result is unused, `Gui4Panel.java:174`). A run that aborts, for instance on S8.24's parse exception, still ends with "Execution Completed" and "✔ Done".
  - There is no Stop. Closing the run window leaves the backend and the reading thread running. The header's close button ends the JVM with `System.exit(0)`, which deletes S8.15's temporary file and leaves the backend writing into a pipe nobody reads; it should then die of SIGPIPE at its next write (not tested).
  - Nothing prevents a second Submit while a run is going; both backends then share `Linux/` and its fixed-name files (S1.26, S8.21).
  - Progress is guessed from echo prefixes; during a solve that can take hours, only raw lines move.
  - "Run another experiment?" opens a new sessions window and leaves the old one open, so windows pile up.
- **Proposed call:** fix. Keep a live log with a stage indicator, but report the exit status, offer Stop, give each run its own directory, and refuse or queue a second run that would share one.
- **Decision:** fix, as proposed (prompt log entry 097)

### S8.29 · Showing results

- **FuSION 1:** nothing in the GUI; template lines `337-345`
- **What it does:** The GUI shows only the run log (S8.28). It does not open the run directory, the plot, pathz3's solution files or anything else; the only picture is the gnuplot window the script itself opens (S8.26). Users find `$WORK_DIR/<WRITE_SUBDIR>/` by hand.
- **Problems:** No session links to its runs' results, so the user cannot tell which directory came from which settings, and a rerun silently replaces it (S8.21). The two Pareto curves, the FSC comparison and the explanations that stages 5 and 6 compute are never shown in the GUI.
- **Proposed call:** fix. The 2.0 frontend shows each run's results, at least the two Pareto curves, the solutions and the explanation views, linked from its session; [decision 0022](../decisions/0022-graph-display.md) designs the graph display.
- **Decision:** fix, as proposed (prompt log entry 098)

### S8.30 · No checks before a run

- **FuSION 1:** every page above; `Linux/frontend/src/ScriptFile.java:24-82`
- **What it does:** Submit fills the template with whatever the session holds. Nothing checks that files exist and are readable, that ids exist, that numbers are in range, or that paths contain no space.
- **Problems:**
  - Every file chooser accepts a path with a space, which `let` then cuts at the space (S1.08): `let $LOG_FOLD_CHANGE_FILE /home/u/My Data/x.tsv` sets `/home/u/My`.
  - Bad inputs show up only as backend messages in the log, or not at all: a missing map gives an empty map (S8.13), an unknown FSC gives two identical solves (S8.12), an empty edge-file name gives an empty set (S8.16), an unfilled placeholder silently disables constraints (S8.17).
  - Stale stored paths, 27 of the 28 in the shipped sessions (S8.05), go into the script unchecked.
- **Proposed call:** fix. Check every input before a run starts, as [decision 0021](../decisions/0021-required-and-optional-inputs.md) requires of both the backend and the frontend.
- **Decision:** fix, as proposed (prompt log entry 098)

### S8.31 · Files shipped with the GUI

- **FuSION 1:** `Linux/frontend/`: `hsa_to_gene_symbol_map_6334.txt`, `hsa_path_to_path_name_map_2822.txt`, `hsa_ids_not_to_be_merged.txt`, `list_of_kegg_xml_files.txt`, `lib/json-20250517.jar`, `resources/`, `dummy.txt`, `src/README.md`, `out/`, `sessions/`; in `Linux/`: `advtempscript.txt`, `output_script.txt`, `script.txt`, `fusion_users.dat`
- **What it does:**
  - Four data files, identical in `frontend/` and `frontend1/` and byte-identical to the copies in `auxilliary/IITB_ACTREC_old_data/IITB_ACTREC_old_data/`:
    - `hsa_to_gene_symbol_map_6334.txt`, 6334 lines such as `hsa7791<TAB>ZYX`, the same bytes as the real runs' `inputs/hsa_to_gene_symbol_map.txt`;
    - `hsa_path_to_path_name_map_2822.txt`, 2822 lines such as `path:hsa01100<TAB>Metabolic_pathways`, the same bytes as the real runs' `inputs/hsa_path_to_path_name_map.txt`;
    - `hsa_ids_not_to_be_merged.txt`: `hsa1432`, `hsa5594` and `hsa5599`, the same as 17 of the 26 copies under `runs/` and `macOS/`; the nine candidate runs of 2026-11-09 add five more ids;
    - `list_of_kegg_xml_files.txt`, 162 absolute paths such as `/home/sukanya/sukanya-cse-git-desktop/network_tool/data/KEGG_Feb_2018/hsa03320.xml`.
  - `lib/json-20250517.jar` (org.json); `resources/icons8-male-user-50.png`, the login icon; `resources/icons8-cross-48.png` and `icons8-next-40.png`, unused; `dummy.txt` (`dummy`); an empty `src/README.md`.
  - Tracked artefacts: 67 classes in `frontend/out/` (S8.01), `frontend/sessions/arpit.json` (S8.05), and in `Linux/` the template, two generated scripts (S8.20) and the account file (S8.02).
- **Problems:**
  - Nothing reads the four data files where they are: the defaults name the maps relative to `Linux/` (S8.13), and the KGML list and the not-merged file are only ever picked by hand.
  - None of the 162 KGML paths exists on this machine; the list the batches use now, `auxilliary/list_of_kegg_xml_files.txt` (167 paths), is a different file.
  - The repository carries compiled, generated and personal files: stale classes, one user's session, one user's account with an email address and a password hash, and two generated scripts.
- **Proposed call:** fix. Ship only what 2.0 reads (the reference maps, with their source and date, and a KGML list that resolves) and keep compiled classes, sessions, accounts and generated scripts out of the repository.
- **Decision:** fix, without shipping KGMLs: ship the reference maps with their source and date, and a KGML list resolved against the user's own KGML directory; keep compiled classes, sessions, accounts and generated scripts out of the repository (prompt log entry 098)

### S8.32 · `frontend1`, the older snapshot

- **FuSION 1:** `Linux/frontend1/` (all files; added in commit 6064c18, 2026-07-02)
- **What it does:** `frontend1/src/` holds 18 Java files, all also in `frontend/src/`, which adds `gui4.java`, `Gui4Panel.java`, the two block files and an empty `README.md`. Compared file by file:
  - identical: `FileSelectionCard`, `IdEntryPanel`, `MainPanel`, `ReachPathBoundCard`, `RelaxationBoundsPanel`, `RoundedButton`, `RoundedPanel`, `Theme`, `ThresholdFilterPanel`, `XMLSelection`, the four data files, the jar and the three icons;
  - line endings only (CRLF in `frontend/`, LF in `frontend1/`): `LoginPage`, `RangeSlider`, `RangeSliderUI`, `ScriptFile`, `UserInput`;
  - different behaviour: `SessionFrame` (Submit runs at once, S8.04), `EdgesChooserPanel` (Submit instead of Next, S8.16) and `HeaderPanel` (the ninth menu entry opens the edge-files page, S8.03), the first two carrying the run window of S8.28;
  - `advtempscript.txt` lacks the three coexpression `let` lines; like `frontend/advtempscript.txt`, it is never read when the GUI runs from `Linux/` (S8.20).

  `frontend1/` has no `out/`, an unused `sessions/.gitkeep` (its code writes to `frontend/sessions/`), and a `.gitignore` for `*.class`, `out/`, `sessions/*.json` and editor files. Run from `Linux/`, it shares `fusion_users.dat`, `frontend/sessions/` and the template with `frontend`.
- **Problems:** Two copies of 18 source files have to be kept in step. The only behavioural difference is that `frontend1` skips the pipeline editor, which does not affect runs anyway (S8.18).
- **Proposed call:** drop. It is an older snapshot of `frontend/`; nothing in it needs carrying over beyond what S8.03 to S8.30 describe.
- **Decision:** drop the copy, but keep its quick Submit: 2.0's frontend has a Submit that runs the default pipeline at once, without opening the editor (prompt log entry 098)

## Open questions

1. **Is `macOS/frontend/` part of this stage?** It is not byte-identical to `Linux/frontend/`: `src/ScriptFile.java` fills the three coexpression placeholders (lines 72-80), `src/ReachPathBoundCard.java` commits a typed bound (58-79), and `README.md` differs. `macOS/advtempscript.txt` also differs from the Linux template: it comments out the additional-edges lines (calling the command `addx`) and adds `exec echo "*DISPLAY* …"` progress lines that no GUI in scope reads. The macOS changes came in commits 3d673af and 224e817 (2026-07-10), after the Linux GUI's last change (a875747, 2026-07-09). *Would settle it:* the user, amending decision 0014 if the macOS GUI should be described as a third version.
   *Resolved:* decision 0014's scope stands. The items cite `macOS/frontend/` where it already fixes a Linux problem, as stage 5 does for the macOS backend (prompt log entry 089).
2. **Has the GUI produced any recorded run, and must 2.0 import FuSION 1 sessions?** No batch file under `runs/` or `macOS/` comes from the template, the only stored pipeline is empty, and 27 of 28 stored paths are stale. `Linux/Data_Coming_From_Frontend.txt` (2026-07-09 19:29) and `Linux/diff_expr_nodes.txt` (19:44, written by pathz3) suggest one run from `Linux/` that evening, and `runs/egf_hela_hsa1956_cxcl8_b6/` appears to be its output directory: its files start at 19:29:26, 3 s after `Data_Coming_From_Frontend.txt`, it uses the template's file names, its `list_of_xml_files_new.txt` names the `arjun` session's pre-merged XML, and it has no `merged_edges_file`, as S8.23 predicts. *Would settle it:* the user, saying who uses the GUI and whether existing sessions must carry over.
   *Resolved:* the GUI produced one recorded run (above), and 2.0 does not import FuSION 1 sessions (prompt log entry 089).
3. **What should an override to "Not Differentially Expressed", "Active" or "Non Relaxed Node" do?** The GUI collects them but never writes them (S8.15). pathz3 takes active-node and non-relaxed-node files, which the template already builds from the source, target and FSC, and has no "not differentially expressed" input. *Would settle it:* the user or the GUI's authors, with stage 5's account of those inputs.
   *Resolved:* all three are wired. Active and Non-relaxed join pathz3's active and non-relaxed node files, and Not DE removes the gene from the observed up and down lists (S8.15; prompt log entry 093).
4. **Does the backend die when the GUI exits during a run (S8.28)?** It should die of SIGPIPE at its next write to stdout, but this was not tested, and a long Z3 call writes nothing for hours. *Would settle it:* a Phase 3 test that closes the GUI during a run and watches the process.
   *Still open, for phase 3.*
