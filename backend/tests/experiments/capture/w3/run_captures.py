#!/usr/bin/env python3
"""Capture FuSION 1's re-read graph and corridor for the 39 KEGG reference cases.

Phase 3 plan (docs/phase3/plan.md), item W3; section 12 of the architecture,
"Capture in Phase 3" (docs/architecture/12-tests.md, lines 558 to 564).

FuSION 1 never saves its corridor (S4.21). For each case of cases.json whose
tier is T2 or T3, this script builds a capture batch, runs it through
run_fusion1.py --batch, one case at a time, checks what the run wrote, and
copies three of its files into the reference store.

The capture batch is the case's batch cut before its first pathz3 line, with
three lines added:
  wgx 0 $WRITE_DIR/graph_reread.xml KEGG   straight after the rgx line;
  wgx 0 $WRITE_DIR/corridor.xml KEGG       straight after the fb_rch line before the cut;
  exit                                     at the end: the cut drops the batch's own exit,
                                           and FuSION 1 reads past the end of a batch
                                           that has none (S1.04).
wgx takes a graph, a file and the root's pathway name, and graph 0 is the newest
graph (Linux/src/main.cc:2585-2627; S2.01, S2.25). rgx creates one graph (S2.05)
and fb_rch two, the corridor last (S4.01), so 0 names the re-read graph and the
corridor. wgx creates none, so the batch's own #LASTGRAPHNUM (S1.09) and graph
numbers are unchanged. Each console log is checked for all of this.

Usage:
  FUSION_TEST_DATA=<root> python3 -I run_captures.py [--case ID]... [--dry-run]

  --case ID   capture only this case (repeatable); by default all 39
  --dry-run   build and check every capture batch in a temporary folder, pass each
              to run_fusion1.py --dry-run, and check the data root if it exists;
              run nothing and write nothing else

Inputs: backend/tests/reference/cases.json; each case's batch, read where
run_fusion1.py reads it (backend/tests/fixtures/ once W6 has copied the corpus,
FuSION 1's tree until then, read only); the data root that W1 and W2 fill.

Outputs, in the data root ($FUSION_TEST_DATA):
  reference/<id>/f1/capture/  graph_reread.xml and corridor.xml (cases.json's "reread"
                              and "corridor") and console.log, run_fusion1.py's console
                              log. The script stops before any run if one exists.
  reports/w3/<stamp>/         counts.tsv: node and edge counts per case, beside the
                              recorded run's; batches/: the capture batches as given to
                              run_fusion1.py
  scratch/w3-<stamp>-<id>/    run_fusion1.py's scratch folder, as the run left it
A capture that fails a check, or whose counts differ from the recorded run's, is
not copied; the script goes on with the next case and exits 1 at the end.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

CAPTURE_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CAPTURE_DIR))
sys.dont_write_bytecode = True  # the import below must not leave a __pycache__ in the repository
import run_fusion1  # noqa: E402  cases.json, batch sources and path maps, as the runs use them

RUNNER = CAPTURE_DIR / "run_fusion1.py"
NOT_A_DATA_ROOT = [Path(__file__).resolve().parents[5], Path(run_fusion1.FUSION1), Path(run_fusion1.SNAPSHOT)]
ADDED = {"reread": "wgx 0 $WRITE_DIR/graph_reread.xml KEGG",
         "corridor": "wgx 0 $WRITE_DIR/corridor.xml KEGG"}
FILES = {"reread": "graph_reread.xml", "corridor": "corridor.xml"}
SOLVES = ("pathz3", "get_path_with_z3")
AFTER_SOLVES = SOLVES + ("psp", "plot_solution_paths", "pex", "plot_explanation")
DASHES = "-" * 31  # the line under every echoed command (S1.10)
KEGG_CASES = 39

NEW_GRAPH = re.compile(r"^New graph id: (\d+)$")                          # rgx (S2.05)
CORRIDOR = re.compile(r"^New graph (\d+) has (\d+) nodes (\d+) edges$")  # fb_rch (S4.17)
REACHED = re.compile(r"^Reachability Graph: (\d+)$")
WRITTEN = re.compile(r"^(\d+) (nodes|edges) written to xml file$")       # wgx (S2.25)
SIZE = re.compile(r"^Number of (nodes|edges): (\d+)")                    # sz (S2.28)
COLUMNS = ["case", "tier", "status", "exit", "wall_s", "reread_graph", "reread_nodes",
           "reread_edges", "corridor_graph", "corridor_nodes", "corridor_edges",
           "recorded_reread", "recorded_corridor"]


def fail(message):
    sys.exit(f"run_captures: {message}")


def tokens(line):
    """FuSION 1 splits a batch line on spaces only (S1.05)."""
    return [t for t in line.split(" ") if t]


def command(line):
    """The token FuSION 1 dispatches on, or None for a blank or comment line (S1.06)."""
    t = tokens(line)
    return None if not t or t[0].startswith(";;") else t[0]


def build(text):
    """The capture batch for a batch text, and the 1-based lines of rgx, fb_rch and the cut."""
    for name in FILES.values():
        if name in text:
            raise ValueError(f"the batch already names {name}")
    lines = text.split("\n")
    cmds = [command(line) for line in lines]
    cut = next((i for i, c in enumerate(cmds) if c in SOLVES), None)
    if cut is None:
        raise ValueError("no pathz3 line")
    rgx = [i for i in range(cut) if cmds[i] in ("rgx", "read_graph_xml")]
    fb = [i for i in range(cut) if cmds[i] == "fb_rch"]
    if len(rgx) != 1 or len(fb) != 1:
        raise ValueError(f"{len(rgx)} rgx and {len(fb)} fb_rch lines before the first pathz3, not 1 and 1")
    r, f = rgx[0], fb[0]
    if f < r:
        raise ValueError("the fb_rch line comes before the rgx line")
    if "exit" in cmds[:cut]:
        raise ValueError("an exit line comes before the first pathz3")
    kept = (lines[:r + 1] + [ADDED["reread"]] + lines[r + 1:f + 1] + [ADDED["corridor"]]
            + lines[f + 1:cut] + ["exit", ""])
    return "\n".join(kept), (r + 1, f + 1, cut + 1)


def check(capture, text):
    """Re-read a capture batch: each wgx line follows its command, exit ends it, and
    nothing of the batch from its first pathz3 on is left."""
    lines = capture.split("\n")
    body = lines[:-1] if lines[-1] == "" else lines
    code = [(i, command(line)) for i, line in enumerate(body) if command(line)]
    problems = []
    for key, follows in (("reread", ("rgx", "read_graph_xml")), ("corridor", ("fb_rch",))):
        at = [k for k, (i, _) in enumerate(code) if body[i] == ADDED[key]]
        if len(at) != 1 or at[0] == 0 or code[at[0] - 1][1] not in follows:
            problems.append(f"the {FILES[key]} line does not follow a {follows[0]} line exactly once")
    if [c for _, c in code].count("exit") != 1 or body[-1] != "exit":
        problems.append("exit is not the last line and the only exit")
    if any(c in AFTER_SOLVES for _, c in code):
        problems.append("a pathz3, psp or pex line is left")
    rest = [line for line in body[:-1] if line not in ADDED.values()]
    original = text.split("\n")
    if (original[:len(rest)] != rest or len(original) <= len(rest)
            or command(original[len(rest)]) not in SOLVES
            or any(command(line) in SOLVES for line in rest)):
        problems.append("without the added lines it is not the batch cut before its first pathz3")
    return problems


def expand(token, macros):
    """FuSION 1's $ expansion of one token (S1.07): at each $, the first defined key in
    byte order that the rest of the token starts with; scanning resumes after the value."""
    keys = sorted(macros)
    i = 0
    while i < len(token):
        if token[i] == "$":
            key = next((k for k in keys if token.startswith(k, i)), None)
            if key is not None:
                token = token[:i] + macros[key] + token[i + len(key):]
                i += len(macros[key]) - 1
        i += 1
    return token


def written_files(batch):
    """The file each added wgx line names once FuSION 1 has expanded it (S1.07, S1.08),
    with the value $WRITE_DIR has there."""
    macros, files = {}, {}
    for line in batch.split("\n"):
        cmd = command(line)
        if cmd is None:
            continue
        toks = [expand(t, macros) for t in tokens(line)]
        if cmd == "let":
            key = toks[1] if len(toks) > 1 else ""
            if key.startswith("$") and key not in macros:
                macros[key] = toks[2] if len(toks) > 2 else ""  # #LASTGRAPHNUM stays literal here
        for key, added in ADDED.items():
            if line == added:
                files[key] = (toks[2] if len(toks) > 2 else "", macros.get("$WRITE_DIR"))
    return files


def targets(mapped, case_dir):
    """The absolute paths the two wgx lines write in the scratch folder, or the problems."""
    found, problems = {}, []
    files = written_files(mapped)
    for key, name in FILES.items():
        if key not in files:
            problems.append(f"no {name} line in the mapped batch")
            continue
        token, write_dir = files[key]
        path = Path(token)
        if write_dir is None or token != f"{write_dir}/{name}" or "$" in token or not path.is_absolute():
            problems.append(f"$WRITE_DIR/{name} expands to {token!r}")
        elif not path.resolve().is_relative_to(case_dir):
            problems.append(f"{token} is outside the scratch folder {case_dir}")
        else:
            found[key] = path
    return found, problems


def segments(log):
    """A console log as (echoed command, its output lines) pairs (S1.10)."""
    lines = log.split("\n")
    out, i = [], 0
    while i < len(lines):
        if i + 1 < len(lines) and lines[i + 1] == DASHES:
            out.append((lines[i].strip(), []))
            i += 2
            continue
        if out:
            out[-1][1].append(lines[i])
        i += 1
    return out


def grab(lines, regex):
    return next((m.groups() for m in map(regex.match, lines) if m), None)


def size_of(lines):
    found = dict(m.groups() for m in map(SIZE.match, lines) if m)
    return (int(found["nodes"]), int(found["edges"])) if len(found) == 2 else None


def wgx_counts(lines):
    found = dict((kind, int(n)) for n, kind in (m.groups() for m in map(WRITTEN.match, lines) if m))
    return (found["nodes"], found["edges"]) if len(found) == 2 else None


def read_log(log):
    """rgx's graph, the size printed after it, fb_rch's first corridor and the counts each
    added wgx line printed, from a console log; or the problem."""
    segs = segments(log)
    first = [echo.split(" ")[0] for echo, _ in segs]
    if first.count("rgx") + first.count("read_graph_xml") != 1 or "fb_rch" not in first:
        return None, "the console log has no single rgx line and fb_rch line"
    r = first.index("rgx") if "rgx" in first else first.index("read_graph_xml")
    f = first.index("fb_rch")
    size_at = next((k for k in range(r + 1, len(segs)) if first[k] in ("size", "sz")), None)
    facts = {
        "rgx": grab(segs[r][1], NEW_GRAPH),
        "reread_size": size_of(segs[size_at][1]) if size_at is not None else None,
        "corridor": grab(segs[f][1], CORRIDOR),
        "reached": grab(segs[f][1], REACHED),
        "wgx": {},
    }
    for k, (echo, out) in enumerate(segs):
        parts = echo.split(" ")
        if first[k] in ("wgx", "write_graph_xml") and len(parts) > 2:
            for key, name in FILES.items():
                if parts[2].endswith("/" + name):
                    facts["wgx"][key] = (k, wgx_counts(out), [line for line in out if "Error" in line
                                                               or "is not present" in line])
    facts["at"] = {"rgx": r, "fb_rch": f}
    return facts, None


def xml_counts(path):
    lines = path.read_text(errors="replace").split("\n")
    return (sum(line.startswith("\t<entry ") for line in lines),
            sum(line.startswith("\t<relation ") for line in lines))


def check_run(facts, files):
    """A capture's own consistency: graph numbers and the counts of both wgx files."""
    problems = []
    if facts["rgx"] is None or facts["corridor"] is None or facts["reached"] is None:
        return ["rgx or fb_rch did not print its graph"]
    if facts["corridor"][0] != facts["reached"][0]:
        problems.append("fb_rch printed two different corridor graphs")
    wanted = {"reread": (facts["at"]["rgx"] + 1, facts["reread_size"]),
              "corridor": (facts["at"]["fb_rch"] + 1, tuple(map(int, facts["corridor"][1:])))}
    for key, (at, counts) in wanted.items():
        got = facts["wgx"].get(key)
        if got is None:
            problems.append(f"no wgx of {FILES[key]} in the console log")
            continue
        k, written, errors = got
        if k != at:
            problems.append(f"the wgx of {FILES[key]} does not run straight after its command")
        if errors:
            problems.append(f"wgx of {FILES[key]}: {errors[0]}")
        if written is None or written != counts:
            problems.append(f"wgx wrote {written} nodes and edges to {FILES[key]}, not {counts}")
        elif not files[key].is_file() or xml_counts(files[key]) != written:
            problems.append(f"{files[key]} does not hold the {written} nodes and edges wgx reported")
    return problems


def recorded(case, data_root):
    """The recorded run's re-read and corridor counts, if its console log is in the data root."""
    log = data_root / "reference" / case["id"] / case["fusion1"]["console"]
    if not log.is_file():
        return None
    facts, problem = read_log(log.read_text(errors="replace"))
    if problem or facts["corridor"] is None:
        return None
    return facts["reread_size"], tuple(map(int, facts["corridor"][1:]))


def kegg_cases(selected):
    cases = [c for c in json.loads(run_fusion1.CASES.read_text(encoding="utf-8"))["cases"]
             if c["tier"] in ("T2", "T3")]
    if len(cases) != KEGG_CASES:
        fail(f"cases.json has {len(cases)} KEGG cases (T2 and T3), not {KEGG_CASES}")
    for case in cases:
        for key, name in FILES.items():
            if Path(case["fusion1"][key]) != Path("f1/capture") / name:
                fail(f"{case['id']}: cases.json's {key} is {case['fusion1'][key]}, not f1/capture/{name}")
    if not selected:
        return cases
    by_id = {c["id"]: c for c in cases}
    unknown = [s for s in selected if s not in by_id]
    if unknown:
        fail(f"not KEGG cases of cases.json: {', '.join(unknown)} (W4 captures the toy cases)")
    return [by_id[s] for s in dict.fromkeys(selected)]


def prepare(case, data_root, stamp, batch_dir):
    """Build, check and write one case's capture batch; return what its run needs and the problems."""
    name = f"w3-{stamp}-{case['id']}"
    case_dir = data_root / "scratch" / name
    source = run_fusion1.batch_source(case, None)
    text = source.read_text(errors="strict")
    item = {"case": case, "name": name, "case_dir": case_dir, "source": source}
    try:
        capture, item["lines"] = build(text)
    except ValueError as e:
        return item, [str(e)]
    problems = check(capture, text)
    item["batch"] = batch_dir / f"{case['id']}.batch"
    item["batch"].write_text(capture)
    mapped = run_fusion1.apply_map(capture, case["path_map"], case_dir, data_root)
    item["targets"], more = targets(mapped, case_dir)
    return item, problems + more


def rehearse(item):
    """run_fusion1.py --dry-run on the capture batch: the exact real command, which runs nothing."""
    result = subprocess.run([sys.executable, "-I", str(RUNNER), "--case", item["case"]["id"],
                             "--name", item["name"], "--batch", str(item["batch"]), "--dry-run"],
                            capture_output=True, text=True)
    return [] if result.returncode == 0 else [f"run_fusion1.py --dry-run: {result.stderr.strip()}"]


def destination(case, data_root):
    return data_root / "reference" / case["id"] / "f1" / "capture"


def capture(item, data_root):
    """Run one capture, check it, and copy its files; return the counts row."""
    case, case_dir = item["case"], item["case_dir"]
    row = dict.fromkeys(COLUMNS, "-")
    row.update(case=case["id"], tier=case["tier"])
    result = subprocess.run([sys.executable, "-I", str(RUNNER), "--case", case["id"],
                             "--name", item["name"], "--batch", str(item["batch"])])
    runner = case_dir / "runner.json"
    if runner.is_file():
        record = json.loads(runner.read_text())
        row.update(exit=record["exit"], wall_s=record["wall_s"])
    console = case_dir / "console.log"
    if result.returncode != 0 or not console.is_file():
        row["status"] = f"failed: run_fusion1.py exited {result.returncode}"
        return row
    files, problems = targets((case_dir / "case.batch").read_text(), case_dir.resolve())
    facts, problem = read_log(console.read_text(errors="replace"))
    if problem:
        problems.append(problem)
    elif not problems:
        problems += check_run(facts, files)
    if facts and facts["rgx"] and facts["corridor"]:
        row.update(reread_graph=facts["rgx"][0], corridor_graph=facts["corridor"][0],
                   corridor_nodes=facts["corridor"][1], corridor_edges=facts["corridor"][2])
        if facts["reread_size"]:
            row.update(reread_nodes=facts["reread_size"][0], reread_edges=facts["reread_size"][1])
    before = recorded(case, data_root)
    if before:
        row.update(recorded_reread="%s/%s" % before[0] if before[0] else "-",
                   recorded_corridor="%s/%s" % before[1])
        if not problems and ((before[0] and before[0] != facts["reread_size"])
                             or before[1] != tuple(map(int, facts["corridor"][1:]))):
            problems.append("the counts differ from the recorded run's")
    if problems:
        row["status"] = "failed: " + "; ".join(problems)
        return row
    dest = destination(case, data_root)
    dest.mkdir(parents=True)
    for key, path in files.items():
        shutil.copy2(path, data_root / "reference" / case["id"] / case["fusion1"][key])
    shutil.copy2(console, dest / "console.log")
    row["status"] = "ok"
    return row


def dry_run(cases, data_root, stamp):
    bad = 0
    with tempfile.TemporaryDirectory(prefix="w3-capture-batches-") as tmp:
        for case in cases:
            item, problems = prepare(case, data_root, stamp, Path(tmp))
            if "batch" in item:
                problems += rehearse(item)
            if problems:
                bad += 1
                print(f"MISFIT {case['id']}: {'; '.join(problems)}")
                continue
            r, f, cut = item["lines"]
            where = item["targets"]["reread"].parent.relative_to(item["case_dir"])
            print(f"ok     {case['id']}: rgx line {r}, fb_rch line {f}, cut before line {cut} (first pathz3), "
                  f"nothing after it kept; writes to $CASE/{where}/")
    print(f"{len(cases) - bad} of {len(cases)} capture batches fit; built and checked in {tmp}, now removed")
    if data_root.is_dir():
        if not (data_root / "fusion1" / "Linux" / "fusion").is_file():
            bad += 1
            print("the data root has no fusion1/Linux/fusion (W1, W2)")
        for case in cases:
            if destination(case, data_root).exists():
                bad += 1
                print(f"{case['id']}: {destination(case, data_root)} exists; the real run would stop")
            if case.get("origin", {}).get("inputs") and not (data_root / "reference" / case["id"] / "inputs").is_dir():
                bad += 1
                print(f"{case['id']}: the data root has no reference/{case['id']}/inputs/ (W1)")
    else:
        print(f"the data root {data_root} does not exist yet (W1 fills it); its checks were skipped")
    print("dry run: nothing run, nothing written outside the temporary folder")
    return 1 if bad else 0


def real_run(cases, data_root, stamp):
    if not data_root.is_dir():
        fail(f"{data_root} does not exist: fill the data root first (W1)")
    report = data_root / "reports" / "w3" / stamp
    items, problems = [], []
    with tempfile.TemporaryDirectory(prefix="w3-capture-batches-") as tmp:
        for case in cases:
            item, found = prepare(case, data_root, stamp, Path(tmp))
            problems += [f"{case['id']}: {p}" for p in found]
            if destination(case, data_root).exists():
                problems.append(f"{case['id']}: {destination(case, data_root)} exists")
            if item["case_dir"].exists():
                problems.append(f"{case['id']}: {item['case_dir']} exists")
            items.append(item)
        if problems:
            fail("nothing was run:\n  " + "\n  ".join(problems))
        (report / "batches").mkdir(parents=True)  # stops if this run's report folder exists
        for item in items:
            item["batch"] = Path(shutil.copy2(item["batch"], report / "batches"))
    table = report / "counts.tsv"
    table.write_text("\t".join(COLUMNS) + "\n")
    failed = 0
    for n, item in enumerate(items, 1):
        print(f"[{n}/{len(items)}] {item['case']['id']}", flush=True)
        row = capture(item, data_root)
        failed += row["status"] != "ok"
        with open(table, "a") as out:
            out.write("\t".join(str(row[c]) for c in COLUMNS) + "\n")
        print(f"  {row['status']}; corridor {row['corridor_nodes']} nodes, {row['corridor_edges']} edges"
              f" (recorded {row['recorded_corridor']})", flush=True)
    print(f"{len(items) - failed} of {len(items)} captured; counts in {table}")
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    root = os.environ.get("FUSION_TEST_DATA")
    if not root:
        fail("set FUSION_TEST_DATA to the data root")
    data_root = Path(root).resolve()
    if any(data_root.is_relative_to(tree) for tree in NOT_A_DATA_ROOT):
        fail(f"the data root must lie outside this repository, FuSION 1 and its snapshot: {data_root}")
    cases = kegg_cases(args.case)
    stamp = time.strftime("%Y%m%dT%H%M%S")
    sys.exit((dry_run if args.dry_run else real_run)(cases, data_root, stamp))


if __name__ == "__main__":
    main()
