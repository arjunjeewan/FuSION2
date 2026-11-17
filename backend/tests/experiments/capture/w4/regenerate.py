#!/usr/bin/env python3
"""Regenerate FuSION 1's outputs for the 12 toy cases, the 15 EGF-HeLa b6 cases and the model example.

Phase 3 plan (docs/phase3/plan.md), item W4; section 12 of the architecture, "Test data and the
KEGG licence", "Reference cases" and step 3 of "Capture work in Phase 3"; decision 0037.

Usage:
  FUSION_TEST_DATA=/path/to/data-root python3 -I regenerate.py [--dry-run] [--case ID]...

Inputs, all read only:
  - backend/tests/reference/cases.json: the 28 cases, toy-*, egfhela-* and model-example, each
    with its origin in FuSION 1's tree;
  - the snapshot's Linux/advtempscript.txt, the template the GUI fills (ScriptFile reads
    advtempscript.txt from the GUI's launch folder, Linux/; S8.01, S8.20), its
    Linux/frontend/src/ScriptFile.java, and Linux/script.txt, a batch the GUI wrote from the
    template for the toy network; all three checked against the snapshot's MANIFEST.sha256;
  - FuSION 1's example/ (the three toy networks and three value files, pinned in TOY_SOURCES),
    its KEGG gene-symbol map, and the recorded outputs of the 28 cases;
  - the data root, which W1 fills: the snapshot binary and the inputs the T2 batches name.

Toy cases (T1), which run from the repository alone:
  1. relabels the case's network: each <graphics> name, a list of KEGG aliases, becomes
     G<entry id>, and nothing else changes (checked with the names blanked out);
  2. copies the value file byte for byte, after checking that each line is an id and a number;
  3. fills the template as ScriptFile does (ScriptFile.java:24-79), by plain text replacement.
     The case gives the FSC, the bound, the network, the value file and the root folder
     /fusion-test/case, the toy cases' path map. Every other value is the one Linux/script.txt
     holds (gui_values), with FuSION 1's Linux/empty_file replaced by the case's
     inputs/empty_file; a self-check fills the template with script.txt's own values and must get
     script.txt's lines back. ScriptFile has no value for %__a21__% to %__a23__% (S8.17), so they
     stay, and FuSION 1 solves without the coexpression and frozen-edge constraints, as in every
     Linux GUI run;
  4. writes the inputs to reference/synthetic/<id>/inputs/ and the batch to
     fixtures/toy-batches/<id>.batch, runs the batch through ../run_fusion1.py, and copies its
     write folder to reference/synthetic/<id>/f1/run/ and its console log to f1/run_console.log;
  5. captures the re-read graph and the corridor as section 12 does for the KEGG cases ("Capture
     in Phase 3"): the batch cut before its first pathz3, with
     `wgx 0 $WRITE_DIR/graph_reread.xml KEGG` after rgx and `wgx 0 $WRITE_DIR/corridor.xml KEGG`
     after the fb_rch before that pathz3, then exit. It runs in its own scratch folder, and the
     two files go to f1/capture/;
  6. checks every file it writes into the repository for KEGG names, as whole words: the aliases
     of the original networks and every symbol of FuSION 1's KEGG gene-symbol map. A case with a
     hit is not copied; its files stay in the scratch folder.
  The 2025 run of toy-hsa12-report-csv-b10 stopped with empty up and down lists; the report says
  what the regenerated run does.

EGF-HeLa b6 cases and the model example (T2): runs each case's own batch through
../run_fusion1.py, whose path map points the batch's old paths at the data root, and copies the
write folder to the data root's reference/<id>/f1/run/ and the console log to f1/run_console.log.

Every run gets a fresh scratch folder from run_fusion1.py, scratch/w4-<stamp>-<id>[-capture]/,
and runs alone. Its write folder is the batch's $WRITE_DIR, worked out from the let lines the way
FuSION 1 expands them (S1.05 to S1.08); it must lie inside the scratch folder, because the GUI
template deletes $WRITE_DIR if it exists. Every batch must end with exit, because FuSION 1 reads
past the end of a batch file and then loops on stdin (S1.04): the template and the T2 batches end
with exit, and each capture batch gets one. Runs get GNUTERM=unknown, so the window plots of 27 of
the 28 batches (gnuplot -p without set term) open nothing on the shared display; their png plots
set their own terminal.

Outputs:
  - repository: fixtures/toy-batches/<id>.batch; reference/synthetic/<id>/inputs/ (relabelled
    network, value file, empty_file), f1/run/, f1/run_console.log and f1/capture/;
  - data root: reference/<id>/f1/run/ and f1/run_console.log of the 16 T2 cases; the scratch
    folders; reports/w4/<stamp>/, with report.txt and the capture batches. The report compares
    each regenerated write folder with the recorded one: exit status, up and down list sizes,
    each .dat file's points, and the files that are identical, differ or exist on one side only.

It never deletes or overwrites: it stops before the first run if a case's f1/ folder, a scratch
folder or the report folder exists. A toy input or batch that exists already is kept if it holds
the same bytes, and stops the script if it differs. It exits 1 if a run failed, left no write
folder or capture file, or was held back by the KEGG check.

--dry-run builds every toy file in a temporary folder and checks it, maps every batch with
run_fusion1.py --dry-run, works out its write folder, checks that it ends with exit, lists what a
run would write, and removes the temporary folder. It starts no FuSION 1 run and writes nothing
else. Data-root inputs that W1 has not copied yet are listed as waiting for W1. It exits 1 if
anything would stop a run. --case ID, repeatable, limits the run to those cases.
"""
import argparse
import filecmp
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path

ARJUN = Path("/home/ks-149-u24/Desktop/arjun")
FUSION1 = ARJUN / "FuSION"
SNAPSHOT = ARJUN / "FuSION1-reference-2026-11-10"
HERE = Path(__file__).resolve().parent
TESTS = HERE.parents[2]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"
SYNTHETIC = TESTS / "reference" / "synthetic"
TOY_BATCHES = TESTS / "fixtures" / "toy-batches"
RUN_FUSION1 = HERE.parent / "run_fusion1.py"
MAPPED_MARK = "----- mapped batch -----\n"  # run_fusion1.py --dry-run prints the mapped batch after it
SYMBOL_MAP = FUSION1 / "auxilliary/IITB_ACTREC_old_data/IITB_ACTREC_old_data/hsa_to_gene_symbol_map_6334.txt"
TOY_ROOT = "/fusion-test/case"  # the toy cases' path map in cases.json
EMPTY = "empty_file"
CSV_CASE = "toy-hsa12-report-csv-b10"
# FuSION 1's example/ files on 2026-11-17; example/ is not in the snapshot.
TOY_SOURCES = {
    "example/report_example.xml": "cc1b8c8e8a6c64025d4f6ad1a15c4ee66897a2859000f06a414f8a0991033a2e",
    "example/report_example_with_pathways.xml": "d1991b8ad7d40839efe3ebde147ad4f00c89c6c67c064aec52636be1191276ca",
    "example/with_node_included.xml": "a917bb9af025c0eb7a94d29215e7a1d8c346e929716bc64e779582e7aded9751",
    "example/report_example.tsv": "7b02d6db1ab6de711cadc975c55e3c2d5b88967ec1da7d497ca23340ada3259b",
    "example/report_example.csv": "7b02d6db1ab6de711cadc975c55e3c2d5b88967ec1da7d497ca23340ada3259b",
    "example/with_node_included.tsv": "f3b469613645afa661e34b772577499eec4ff2d70d011045a0b4204228dd980c",
}
PLACEHOLDER = re.compile(r"%__\w+?__%")
UNFILLED = {"%__a21__%", "%__a22__%", "%__a23__%"}  # ScriptFile has no value for them (S8.17)
ENTRY = re.compile(rb'(<entry id="(\d+)"[^>]*>)(.*?)(</entry>)', re.S)
GRAPHICS = re.compile(rb'(<graphics name=")([^"]*)(")')
VALUE_LINE = re.compile(rb"h(?:sa|as)\d+[ \t]-?\d+(?:\.\d+)?\r?")  # has16 is FuSION 1's own typo
WORD = re.compile(r"[A-Za-z0-9_-]+")
PNG = b"\x89PNG\r\n\x1a\n"

# Linux/script.txt's own session: the toy network, FSC hsa12, bound 10, with_node_included.
SCRIPT_EMPTY = "/home/ks-149-u24/Desktop/arjun/FuSION/Linux/empty_file"
SCRIPT_CASE = {"work_dir": "/home/ks-149-u24/Desktop/arjun/FuSION/example", "fsc": "hsa12", "bound": 10,
               "lfc": "/home/ks-149-u24/Desktop/arjun/FuSION/example/with_node_included.tsv",
               "xml": "/home/ks-149-u24/Desktop/arjun/FuSION/example/with_node_included.xml",
               "empty": SCRIPT_EMPTY}

problems = []


def fail(message):
    sys.exit(f"regenerate: {message}")


def stop(message, dry):
    """Stop the run; a dry run notes the problem and carries on checking."""
    if not dry:
        fail(message)
    print(f"  WOULD STOP: {message}")
    problems.append(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def snapshot_file(rel):
    """A snapshot file, checked against the snapshot's MANIFEST.sha256."""
    want = None
    for line in (SNAPSHOT / "MANIFEST.sha256").read_text(encoding="utf-8").splitlines():
        digest, _, path = line.partition("  ")
        if path == f"./{rel}":
            want = digest
    data = (SNAPSHOT / rel).read_bytes()
    if want is None or sha256(data) != want:
        fail(f"the snapshot's {rel} does not match its MANIFEST.sha256 entry")
    return data.decode("utf-8")


def source(rel):
    """A FuSION 1 example/ file, pinned by TOY_SOURCES."""
    data = (FUSION1 / rel).read_bytes()
    if sha256(data) != TOY_SOURCES[rel]:
        fail(f"FuSION 1's {rel} has changed since 2026-11-17 (SHA-256 {sha256(data)})")
    return data


def gui_values(work_dir, fsc, bound, lfc, xml, empty):
    """ScriptFile's replacements (ScriptFile.java:24-72) for a toy session.

    Apart from the case's own values, each one is what Linux/script.txt holds (its lines 20-63);
    check_gui_values proves it. They agree with all that the 2025 toy outputs show: source hsa1
    and target hsa22 (connect_pairs); bounds 0 to 7 (the first line of each limits file); one
    solution counted and explored (an empty "Solution 2" at each point, S5.26); the pre-merged
    mode (list_of_xml_files_new.txt names the network alone); and no ids beyond the up and down
    lists, the source and the target in exception_file.txt.
    """
    values = {
        "%__1__%": work_dir, "%__2__%": "1", "%__3__%": "22", "%__4__%": fsc.removeprefix("hsa"),
        "%__5__%": str(bound),
        "%__6__%": "0", "%__7__%": "7", "%__8__%": "0", "%__9__%": "7",  # edge bounds, then node bounds
        "%__10__%": Path(lfc).name, "%__11__%": Path(empty).name,
        "%__12__%": "0.9", "%__13__%": "-0.9",
        "%__14__%": "$BOTH_ACT_EXPR_EDGES_TO_TARGET",
        "%__lf__%": lfc, "%__ef__%": empty,
        "%__a1__%": "1", "%__a2__%": "30000", "%__a3__%": "150000", "%__a4__%": "1", "%__a5__%": "1",
        "%__a18__%": "", "%__a19__%": xml, "%__c__%": ";;",
    }
    # The five edge files, the four override lists, the three maps and the not-merged ids.
    for n in [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20]:
        values[f"%__a{n}__%"] = empty
    return values


def fill(text, values):
    for key, value in values.items():
        text = text.replace(key, value)
    return text


def check_template(template, script_file_java):
    keys = set(re.findall(r'replacements\.put\("(%__\w+?__%)"', script_file_java))
    found = set(PLACEHOLDER.findall(template))
    if not keys <= found or found - keys != UNFILLED:
        fail(f"the template's placeholders {sorted(found)} do not fit ScriptFile's {sorted(keys)}")
    if set(gui_values(**SCRIPT_CASE)) != keys:
        fail("gui_values does not fill exactly ScriptFile's placeholders")


def check_gui_values(template, script, dry):
    """Filled with script.txt's own values, every placeholder line must be a line of script.txt.

    Exempt: the %__a6__% comment, which the ScriptFile that wrote script.txt left unfilled, and
    %__a21__% to %__a23__%, which joined the template on 2026-05-26, after script.txt.
    """
    values = gui_values(**SCRIPT_CASE)
    lines = set(script.splitlines())
    for line in template.splitlines():
        if PLACEHOLDER.search(line) and not re.search(r"%__a(6|21|22|23)__%", line):
            if fill(line, values) not in lines:
                stop(f"gui_values does not reproduce Linux/script.txt: {fill(line, values)!r}", dry)


def relabel(data, where):
    """Each <graphics> name becomes G<entry id>; every other byte stays."""
    def entry(m):
        body, n = GRAPHICS.subn(lambda g: g.group(1) + b"G" + m.group(2) + g.group(3), m.group(3))
        if n != 1:
            fail(f"{where}: entry {m.group(2).decode()} has {n} <graphics> elements")
        return m.group(1) + body + m.group(4)

    out, entries = ENTRY.subn(entry, data)
    ids = [m.group(2) for m in ENTRY.finditer(data)]
    shape = (entries, len(set(ids)), len(GRAPHICS.findall(data)), data.count(b"<relation "))
    if shape != (22, 22, 22, 25):
        fail(f"{where}: expected 22 entries with one <graphics> each and 25 relations (section 12)")
    if GRAPHICS.sub(rb"\1\3", out) != GRAPHICS.sub(rb"\1\3", data):
        fail(f"{where}: relabelling changed more than the <graphics> names")
    return out


def check_values(data, where):
    for line in data.split(b"\n"):
        if line and not VALUE_LINE.fullmatch(line):
            fail(f"{where}: {line!r} is not an id and a value")
    return data


def first_token(line):
    tokens = [t for t in line.rstrip("\r\n").split(" ") if t]
    return tokens[0] if tokens else ""


def last_command(batch):
    """The batch's last command as FuSION 1 reads it: lines end at \\n, and tokens, which keep any
    \\r or tab, are split at spaces (S1.05, S1.06)."""
    commands = [tokens[0] for tokens in ([t for t in line.split(" ") if t] for line in batch.split("\n"))
                if tokens and not tokens[0].startswith(";;")]
    return commands[-1] if commands else ""


def capture_batch(batch):
    """Section 12's capture batch, or None if the batch has no rgx and fb_rch before a pathz3."""
    lines = batch.splitlines(keepends=True)
    commands = [first_token(line) for line in lines]
    if "pathz3" not in commands:
        return None
    cut = commands.index("pathz3")
    rgx = [i for i in range(cut) if commands[i] == "rgx"]
    fb_rch = [i for i in range(cut) if commands[i] == "fb_rch"]
    if len(rgx) != 1 or not fb_rch or fb_rch[-1] < rgx[0]:
        return None
    out = lines[:cut]
    out.insert(fb_rch[-1] + 1, "wgx 0 $WRITE_DIR/corridor.xml KEGG\n")
    out.insert(rgx[0] + 1, "wgx 0 $WRITE_DIR/graph_reread.xml KEGG\n")
    return "".join(out) + "exit\n"


def expand(token, macros):
    """S1.07: at each $, the first defined key in byte order that starts there is replaced."""
    out, i, keys = [], 0, sorted(macros)
    while i < len(token):
        key = next((k for k in keys if token.startswith(k, i)), None) if token[i] == "$" else None
        if key is None:
            out.append(token[i])
            i += 1
        else:
            out.append(macros[key])
            i += len(key)
    return "".join(out)


def let_values(batch):
    """The batch's let variables as FuSION 1 defines them (S1.05, S1.06, S1.08)."""
    macros = {}
    for line in batch.split("\n"):
        tokens = [t for t in line.split(" ") if t]
        if not tokens or tokens[0].startswith(";;"):
            continue
        if tokens[0] == "exit":
            break
        if tokens[0] == "let":
            key, value = ([expand(t, macros) for t in tokens[1:3]] + ["", ""])[:2]
            if key.startswith("$") and key not in macros:
                macros[key] = value
    return macros


def kegg_words():
    """Whole words that count as KEGG names: every symbol of FuSION 1's KEGG gene-symbol map,
    complexes split into their members, and every alias in the toy networks' <graphics> names.
    Ids (hsa..., cpd...) and numbers are not names."""
    words = set()
    for line in SYMBOL_MAP.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) >= 2:
            words.update(re.split(r"[\[\]+]", parts[1]))
    for rel in TOY_SOURCES:
        if rel.endswith(".xml"):
            for m in GRAPHICS.finditer(source(rel)):
                words.update(WORD.findall(m.group(2).decode()))
    return {w for w in words if w and not re.fullmatch(r"hsa\d+|cpd[A-Z]?\d+|\d+", w)}


def png_text(data):
    """The text chunks of a PNG file, the only words an image holds."""
    text, pos = [], 8
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"tEXt":
            text.append(body)
        elif kind == b"zTXt":
            key, _, rest = body.partition(b"\0")
            text += [key, zlib.decompress(rest[1:])]
        elif kind == b"iTXt":
            key, _, rest = body.partition(b"\0")
            compressed, rest = rest[:1] == b"\1", rest[2:]
            language, _, rest = rest.partition(b"\0")
            translated, _, rest = rest.partition(b"\0")
            text += [key, language, translated, zlib.decompress(rest) if compressed else rest]
        pos += 12 + length
    return b"\n".join(text).decode("utf-8", errors="replace")


def kegg_hits(data, words):
    """The KEGG names in a file's text, at most five of them, then how many more."""
    if data.startswith(PNG):
        text = png_text(data)
    elif b"\0" in data:
        return ["(binary data the check cannot read)"]
    else:
        text = data.decode("utf-8", errors="replace")
    hits = sorted(set(WORD.findall(text)) & words)
    return hits[:5] + ([f"and {len(hits) - 5} more"] if len(hits) > 5 else [])


def toy_files(case, template, words, dry):
    """The repository files of a toy case, {path: bytes}, and its capture batch."""
    o = case["origin"]
    network, values = Path(o["network"]).name, Path(o["values"]).name
    inputs = SYNTHETIC / case["id"] / "inputs"
    files = {
        inputs / network: relabel(source(o["network"]), o["network"]),
        inputs / values: check_values(source(o["values"]), o["values"]),
        inputs / EMPTY: b"",
    }
    at = f"{TOY_ROOT}/inputs/"
    batch = fill(template, gui_values(TOY_ROOT, o["fsc"], o["bound"], at + values, at + network,
                                      at + EMPTY))
    files[TOY_BATCHES / f"{case['id']}.batch"] = batch.encode("utf-8")
    if set(PLACEHOLDER.findall(batch)) != UNFILLED:
        stop(f"{case['id']}: the batch keeps placeholders {sorted(set(PLACEHOLDER.findall(batch)))}", dry)
    lets = let_values(batch)
    outside = sorted(v for v in lets.values()
                     if v.startswith("/") and not (v + "/").startswith(TOY_ROOT + "/"))
    if outside:
        stop(f"{case['id']}: the batch names paths outside {TOY_ROOT}: {outside}", dry)
    recorded = FUSION1 / o["outputs"]
    for key in ("$PO_POINTS_WITH_FSC", "$PO_POINTS_WITHOUT_FSC"):
        dat = recorded / (lets.get(key, "") + ".dat")
        if any(recorded.glob("*.dat")) and not dat.is_file():
            stop(f"{case['id']}: the batch's {key} names {dat.name}, which the 2025 run did not write", dry)
    capture = capture_batch(batch)
    if capture is None:
        stop(f"{case['id']}: the batch has no rgx and fb_rch before its first pathz3", dry)
    for path, data in list(files.items()) + [(Path("capture batch"), (capture or "").encode())]:
        hits = kegg_hits(data, words)
        if hits:
            stop(f"{case['id']}: {path.name} holds KEGG names {hits}", dry)
    return files, capture


def write_dir(batch, scratch):
    value = let_values(batch).get("$WRITE_DIR", "")
    path = Path(os.path.normpath(value)) if value else None
    if path and path.is_absolute() and path.is_relative_to(scratch) and path != scratch:
        return path
    return None


def run_fusion1(root, case_id, name, batch=None, dry=False):
    command = [sys.executable, "-I", str(RUN_FUSION1), "--case", case_id, "--name", name]
    if batch:
        command += ["--batch", str(batch)]
    if dry:
        command.append("--dry-run")
    env = dict(os.environ, FUSION_TEST_DATA=str(root), GNUTERM="unknown")
    return subprocess.run(command, env=env, capture_output=dry, text=True)


def put(path, data):
    """Write a new file; one that exists already is kept if it holds the same bytes."""
    if path.exists():
        if path.read_bytes() != data:
            fail(f"{path} exists and differs from what W4 writes; it is never overwritten")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "xb") as f:
        f.write(data)


def copy_new(src, dest):
    if dest.exists():
        fail(f"{dest} exists; results are never overwritten")
    dest.parent.mkdir(parents=True, exist_ok=True)
    if src.is_dir():
        shutil.copytree(src, dest)
    else:
        with open(src, "rb") as f, open(dest, "xb") as g:
            shutil.copyfileobj(f, g)


def f1_dir(case, root):
    return (SYNTHETIC if case["tier"] == "T1" else root / "reference") / case["id"] / "f1"


def w4_cases(selected):
    cases = [c for c in json.loads(CASES.read_text(encoding="utf-8"))["cases"]
             if c["tier"] == "T1" or c["id"].startswith("egfhela-") or c["id"] == "model-example"]
    groups = [sum(c["tier"] == "T1" for c in cases), sum(c["id"].startswith("egfhela-") for c in cases),
              sum(c["id"] == "model-example" for c in cases)]
    if groups != [12, 15, 1]:
        fail(f"cases.json holds {groups} toy, EGF-HeLa and model cases, not 12, 15 and 1")
    for c in cases:
        toy_form = (c["batch"] == f"toy-batches/{c['id']}.batch"
                    and c["path_map"] == [{"from": TOY_ROOT, "to": "$CASE"}])
        if (c["tier"] == "T1") != toy_form or c["tier"] not in ("T1", "T2"):
            fail(f"{c['id']}: unexpected tier, batch or path map in cases.json")
    unknown = set(selected) - {c["id"] for c in cases}
    if unknown:
        fail(f"not W4 cases: {sorted(unknown)}")
    return [c for c in cases if not selected or c["id"] in selected]


def data_root():
    value = os.environ.get("FUSION_TEST_DATA")
    if not value:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(value).resolve()
    for tree in (FUSION1, SNAPSHOT, REPO):
        if root.is_relative_to(tree):
            fail(f"the data root must lie outside {tree}: {root}")
    return root


def plan_runs(cases, toy, stamp):
    """(case, run name, capture batch text or None) for every run, in order."""
    runs = []
    for case in cases:
        runs.append((case, f"w4-{stamp}-{case['id']}", None))
        if case["tier"] == "T1":
            runs.append((case, f"w4-{stamp}-{case['id']}-capture", toy[case["id"]][1]))
    return runs


def map_runs(runs, toy, root, dry, tmp):
    """Map every batch as run_fusion1.py will, and check where it writes."""
    waiting = set()
    for case, name, capture in runs:
        scratch = root / "scratch" / name
        if scratch.exists():
            stop(f"{scratch} exists; every run gets a fresh scratch folder", dry)
        batch = None
        if case["tier"] == "T1":
            batch = tmp / f"{name}.batch"
            text = capture if capture else toy[case["id"]][0][TOY_BATCHES / f"{case['id']}.batch"].decode()
            batch.write_text(text, encoding="utf-8")
        out = run_fusion1(root, case["id"], name, batch, dry=True)
        if out.returncode != 0 or MAPPED_MARK not in out.stdout:
            stop(f"{name}: run_fusion1.py --dry-run failed: {out.stderr.strip()}", dry)
            continue
        mapped = out.stdout.split(MAPPED_MARK, 1)[1]
        want = scratch / Path(case["origin"]["outputs"]).name
        if write_dir(mapped, scratch) != want:
            stop(f"{name}: $WRITE_DIR is {let_values(mapped).get('$WRITE_DIR')!r}, not {want}", dry)
        commands = [first_token(line) for line in mapped.splitlines()]
        if (capture and "pathz3" in commands) or (not capture and "pathz3" not in commands):
            stop(f"{name}: unexpected batch: {'a capture with' if capture else 'a run without'} pathz3", dry)
        if last_command(mapped) != "exit":
            stop(f"{name}: the batch does not end with exit, so FuSION 1 would loop on stdin (S1.04)", dry)
        for value in let_values(mapped).values():
            path = Path(os.path.normpath(value))
            if value.startswith(f"{root}/") and not path.is_relative_to(scratch) and not path.exists():
                waiting.add(path)
        if case["origin"].get("inputs") and not (root / "reference" / case["id"] / "inputs").is_dir():
            waiting.add(root / "reference" / case["id"] / "inputs")
        print(f"  {name}: writes {want.relative_to(root)}")
    if not (root / "fusion1" / "Linux" / "fusion").is_file():
        waiting.add(root / "fusion1" / "Linux" / "fusion")
    if waiting:
        print(f"  {len(waiting)} data-root inputs wait for W1, for example:")
        for path in sorted(waiting)[:5]:
            print(f"    {path}")
        if not dry:
            fail("the data root lacks inputs the batches name; run W1's fill_data_root.sh first")


def count_lines(path):
    if not path.is_file():
        return "none"
    return str(sum(1 for line in path.read_text(errors="replace").splitlines() if line.strip()))


def dat_points(path):
    if not path.is_file():
        return "missing"
    rows = [line.split() for line in path.read_text().splitlines() if line.strip()]
    return " ".join(f"({row[0]},{row[1]})" for row in rows) or "no point"


def compare(recorded, regenerated):
    """The names of files identical in both folders, those that differ, and those on one side only."""
    rec = {p.name: p for p in recorded.iterdir()} if recorded.is_dir() else {}
    new = {p.name: p for p in regenerated.iterdir()} if regenerated.is_dir() else {}
    both = sorted(rec.keys() & new.keys())
    same = [n for n in both if rec[n].is_file() and new[n].is_file()
            and filecmp.cmp(rec[n], new[n], shallow=False)]
    differ = [n for n in both if n not in same]
    return same, differ, sorted(rec.keys() - new.keys()), sorted(new.keys() - rec.keys())


def run_one(root, case_id, name, batch=None):
    """Run a batch through run_fusion1.py; return its runner.json, write folder and scratch folder."""
    print(f"{name}: running", flush=True)
    run_fusion1(root, case_id, name, batch)
    scratch = root / "scratch" / name
    if not (scratch / "runner.json").is_file():
        fail(f"{name}: run_fusion1.py stopped before FuSION 1 ran; see its message above")
    record = json.loads((scratch / "runner.json").read_text(encoding="utf-8"))
    return record, write_dir((scratch / "case.batch").read_text(encoding="utf-8"), scratch), scratch


def collect(case, name, root, words, report_dir):
    """Run a case, and a toy case's capture, and copy what they wrote; return the report's facts."""
    toy, f1 = case["tier"] == "T1", f1_dir(case, root)
    record, wdir, scratch = run_one(root, case["id"], name)
    res = {"name": name, "exit": record["exit"], "wall": record["wall_s"], "folder": None,
           "notes": [], "capture": {}}
    if record["exit"] != 0:
        res["notes"].append(f"FuSION 1 exited {record['exit']}")
    written = sorted(p for p in wdir.rglob("*") if p.is_file()) if wdir and wdir.is_dir() else []
    hits = {str(p.relative_to(scratch)): h for p in [scratch / "console.log"] + written
            if toy and (h := kegg_hits(p.read_bytes(), words))}
    if hits:
        res["notes"].append(f"not copied, KEGG names: {hits}")
        res["folder"] = wdir
    else:
        if wdir and wdir.is_dir():
            copy_new(wdir, f1 / "run")
            res["folder"] = f1 / "run"
        else:
            res["notes"].append("no write folder")
        copy_new(scratch / "console.log", f1 / "run_console.log")
    if not toy:
        return res

    record, wdir, _ = run_one(root, case["id"], name + "-capture",
                              report_dir / "capture-batches" / f"{case['id']}.batch")
    if record["exit"] != 0:
        res["notes"].append(f"FuSION 1 exited {record['exit']} in the capture")
    for file in ("graph_reread.xml", "corridor.xml"):
        src = wdir / file if wdir else None
        if not src or not src.is_file():
            res["notes"].append(f"the capture wrote no {file}")
        elif hits := kegg_hits(src.read_bytes(), words):
            res["notes"].append(f"{file} not copied, KEGG names: {hits}")
        else:
            copy_new(src, f1 / "capture" / file)
            text = src.read_text(errors="replace")
            res["capture"][file] = f"{text.count('<entry ')} entries, {text.count('<relation ')} relations"
    return res


def describe(case, res):
    status = f"exit {res['exit']}, {res['wall']:.1f} s" + "".join(f"; {n}" for n in res["notes"])
    lines = [f"{case['id']} ({case['tier']}): {status}; scratch/{res['name']}"]
    recorded, new = FUSION1 / case["origin"]["outputs"], res["folder"]
    if new is None:
        return lines
    ud = {d: "/".join(count_lines(d / n) for n in ("up_reg", "down_reg")) for d in (recorded, new)}
    lines.append(f"  up/down: recorded {ud[recorded]}, regenerated {ud[new]}")
    for dat in sorted({p.name for p in recorded.glob("*.dat")} | {p.name for p in new.glob("*.dat")}):
        a, b = dat_points(recorded / dat), dat_points(new / dat)
        lines.append(f"  {dat}: recorded {a}; regenerated {'the same' if a == b else b}")
    same, differ, only_rec, only_new = compare(recorded, new)
    lines.append(f"  files: {len(same)} identical; differ: {', '.join(differ) or '-'}; "
                 f"only recorded: {', '.join(only_rec) or '-'}; "
                 f"only regenerated: {', '.join(only_new) or '-'}")
    for file, counts in res["capture"].items():
        lines.append(f"  capture {file}: {counts}")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--case", action="append", default=[], metavar="ID")
    args = parser.parse_args()
    dry = args.dry_run
    root = data_root()
    cases = w4_cases(args.case)
    template = snapshot_file("Linux/advtempscript.txt")
    check_template(template, snapshot_file("Linux/frontend/src/ScriptFile.java"))
    check_gui_values(template, snapshot_file("Linux/script.txt"), dry)
    words = kegg_words()
    stamp = time.strftime("%Y%m%d-%H%M%S")
    report_dir = root / "reports" / "w4" / stamp
    print(f"W4{' dry run' if dry else ''}: {len(cases)} cases, data root {root}")
    if report_dir.exists():
        stop(f"{report_dir} exists", dry)

    toy = {}
    for case in cases:
        if case["tier"] == "T1":
            toy[case["id"]] = toy_files(case, template, words, dry)
            kept = []
            for path, data in toy[case["id"]][0].items():
                if path.exists() and path.read_bytes() != data:
                    stop(f"{path} exists and differs from what W4 writes", dry)
                kept += [path.name] if path.exists() else []
            print(f"  {case['id']}: {len(toy[case['id']][0]) - len(kept)} toy files to write"
                  + (f", {len(kept)} already there and the same ({', '.join(kept)})" if kept else ""))
        if f1_dir(case, root).exists():
            stop(f"{f1_dir(case, root)} exists; results are never overwritten", dry)
        if not (FUSION1 / case["origin"]["outputs"]).is_dir():
            stop(f"{case['id']}: the recorded outputs {case['origin']['outputs']} are missing", dry)
    runs = plan_runs(cases, toy, stamp)
    with tempfile.TemporaryDirectory(prefix="w4-") as tmp:
        map_runs(runs, toy, root, dry, Path(tmp))
    if dry:
        print(f"dry run: {len(runs)} runs; {len(problems)} problem(s) would stop the run; nothing written")
        sys.exit(1 if problems else 0)

    (report_dir / "capture-batches").mkdir(parents=True)
    for case_id, (files, capture) in toy.items():
        for path, data in files.items():
            put(path, data)
        put(report_dir / "capture-batches" / f"{case_id}.batch", capture.encode("utf-8"))
    results = {c["id"]: collect(c, f"w4-{stamp}-{c['id']}", root, words, report_dir) for c in cases}

    bad = [i for i, r in results.items() if r["notes"]]
    lines = [f"W4 {stamp}: {len(cases)} cases, {len(cases) - len(bad)} without a problem"
             + (f"; problems in {', '.join(bad)}" if bad else ""),
             "Recorded: FuSION 1's outputs in its tree (the toy runs date from 2025); regenerated: this run,",
             "by the snapshot binary.", ""]
    for case in cases:
        lines += describe(case, results[case["id"]])
    if CSV_CASE in results:
        r = results[CSV_CASE]
        folder = r["folder"]
        dats = ", ".join(sorted(p.name for p in folder.glob("*.dat"))) if folder else ""
        ud = "/".join(count_lines(folder / n) for n in ("up_reg", "down_reg")) if folder else "none"
        lines += ["", f"{CSV_CASE}: the 2025 run stopped with empty up and down lists and no solve. "
                      f"Regenerated: exit {r['exit']}, up/down {ud}, .dat files: {dats or 'none'}."]
    text = "\n".join(lines) + "\n"
    with open(report_dir / "report.txt", "x", encoding="utf-8") as f:
        f.write(text)
    print(text + f"wrote {report_dir / 'report.txt'}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
