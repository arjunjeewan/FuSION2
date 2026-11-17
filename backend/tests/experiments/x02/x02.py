#!/usr/bin/env python3
"""X02: graph 83 against graph 84. Run the reference batch with fb_rch on the merged graph, and compare.

Phase 3 plan (docs/phase3/plan.md), experiment X02; section 12 of the
architecture, "Phase 3 experiments"; spec stage 2, open question 1, stage 4,
open question 5, and S2.26; section 02, open question 7; decision 0037.

In the reference run, jun-b10-fdr025, mff makes graph 83 and rgx re-reads
wgx's file of it as graph 84 (S2.01, S2.26). The batch never writes 84: its
first `fb_rch 0` runs on the latest graph, which is 84, and
`let $MERGED_GRAPH_NUM #LASTGRAPHNUM`, after rgx, stores 84 for the second
fb_rch (S1.09). Both pathz3 lines name graph 0, the corridor that the fb_rch
before them made, so they follow fb_rch. The script's copy of the batch:
  - moves that let to just after mff's `size 0`, before wgx and rgx, so that
    it stores the id of the graph mff made (83);
  - runs the first fb_rch on $MERGED_GRAPH_NUM instead of 0; the second
    already does;
  - adds `wgx 0 $WRITE_DIR/corridor_w.xml KEGG` after the first fb_rch and
    `wgx 0 $WRITE_DIR/corridor_wo.xml KEGG` after the second, which save each
    corridor as section 12's capture does (wgx only reads the graph:
    Linux/src/GraphManagerNew.cc:7895-7938).
rgx still runs, so every graph keeps its recorded id (S2.01), and the
`size 0` after rgx still prints graph 84. The script runs the copy through
run_fusion1.py --batch (about 11 min: 615 s of solves in the recorded run),
then compares it with the recorded run:
  - the corridors, nodes matched by type and member ids and edges by their
    two nodes, type and subtypes, never by node id or key: the one before the
    with-FSC solve against W3's capture of the recorded run's corridor
    (graph 84's), and the X02 run's two corridors with each other (S4.20);
  - the frontiers, the .dat files of both FSC cases, with the number of
    unknown answers in each limits file;
  - the console's graph ids, corridor counts and solve-input counts, which
    also show that both fb_rch lines ran on the graph mff made.

Usage:
  FUSION_TEST_DATA=<root> python3 -I x02.py [--dry-run | --compare-only RUN_ID]

Inputs: backend/tests/reference/cases.json; the case's batch, found as
run_fusion1.py finds it; in the data root, the recorded run under
reference/jun-b10-fdr025/: f1/run_console.log, both cases' .dat and limits
files in f1/run/, and W3's f1/capture/corridor.xml; and what run_fusion1.py needs.

Outputs, in the data root (they hold KEGG ids, so never in the repository, S8.31):
  scratch/x02-<RUN_ID>/   the run
  reports/x02/<RUN_ID>/   x02.batch, report.json, summary.txt
RUN_ID is the start time, YYYYmmdd-HHMMSS, and the script stops if either
folder exists. --compare-only RUN_ID compares that run again, into
reports/x02/<RUN_ID>-compared-<time>/. --dry-run builds the batch in a
temporary folder, checks it with run_fusion1.py --dry-run, lists the data
root's inputs, and self-tests the comparison on recorded files (read from
FuSION 1's tree, read only, until W1 has filled the data root). It starts no
FuSION 1 run and removes the folder.
"""
import argparse
import difflib
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

CASE_ID = "jun-b10-fdr025"
STAND_IN_CASE = "jun-b10-fdr030"  # the dry run's self-test uses its merged graph as a corridor that differs
ARJUN = "/home/ks-149-u24/Desktop/arjun"
FUSION1 = Path(f"{ARJUN}/FuSION")
SNAPSHOT = Path(f"{ARJUN}/FuSION1-reference-2026-11-10")
TESTS = Path(__file__).resolve().parents[2]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"
FIXTURES = TESTS / "fixtures"
RUN_FUSION1 = TESTS / "experiments" / "capture" / "run_fusion1.py"

GRAPH_COMMANDS = {"mff", "merge_from_file", "rgx", "read_graph_xml", "size", "sz", "wgx", "write_graph_xml",
                  "fb_rch", "pathz3", "get_path_with_z3", "psp", "plot_solution_paths"}
SHAPE = ["mff", "size", "wgx", "rgx", "size", "fb_rch", "size", "pathz3", "fb_rch", "size", "pathz3"]
SHAPE_X02 = ["mff", "size", "wgx", "rgx", "size", "fb_rch", "wgx", "size", "pathz3", "fb_rch", "wgx", "size",
             "pathz3"]
LET = ["let", "$MERGED_GRAPH_NUM", "#LASTGRAPHNUM"]

FACTS = {  # console lines (S2.01, S2.05, S2.28, S4.x, S5.31): name -> pattern, one value per match
    "merged graph (mff)": r"^New graph: (\d+)$",
    "re-read graph (rgx)": r"^New graph id: (\d+)$",
    "fb_rch on graph": r"^fb_rch (\S+) ",
    "corridor (graph, nodes, edges)": r"^New graph (\d+) has (\d+) nodes (\d+) edges",
    "size: nodes, isolated": r"^Number of nodes: (\d+) \(.*\) (\d+) isolated$",
    "size: edges": r"^Number of edges: (\d+)$",
    "Up, Down": r"^Up: (\d+) Down: (\d+)$",
    "coexpression clauses": r"^\[coexpression\] emitted (\d+) clauses",
    "frozen-edge clauses": r"^\[frozen-edge\] emitted (\d+) clauses",
    "PO points found": r"^(\d+) PO points found$",
    "pathz3 wall s": r"^\[pathz3 timing\] Total wall-clock time: ([0-9.]+) s",
}
DIFFERENCES = (("nodes_only_a", "nodes only in the first"), ("nodes_only_b", "nodes only in the second"),
               ("edges_only_a", "edges only in the first"), ("edges_only_b", "edges only in the second"),
               ("components_differ", "complexes whose components differ"),
               ("pathway_sets_differ", "edges whose pathway sets differ"),
               ("scores_differ", "edges whose experiment_score differs"))


def fail(message):
    sys.exit(f"x02: {message}")


def data_root():
    value = os.environ.get("FUSION_TEST_DATA")
    if not value:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(value).resolve()
    for tree in (FUSION1, SNAPSHOT, REPO):
        if root == tree or tree in root.parents:
            fail(f"the data root must lie outside {tree}: {root}")
    return root


def load_case(case_id):
    for case in json.loads(CASES.read_text(encoding="utf-8"))["cases"]:
        if case["id"] == case_id:
            return case
    fail(f"no case {case_id} in {CASES}")


def batch_source(case):
    """The batch run_fusion1.py would run: the frozen copy (W6), else FuSION 1's own, read only."""
    frozen = FIXTURES / case["batch"]
    return frozen if frozen.exists() else FUSION1 / case["origin"]["batch"]


def graph_lines(lines):
    """(index, tokens) of each line that runs a graph command; ;; lines are comments (S1.06)."""
    code = [(i, line.split()) for i, line in enumerate(lines) if line.split()]
    return [(i, t) for i, t in code if not t[0].startswith(";;") and t[0] in GRAPH_COMMANDS]


def uses_of_merged_graph_num(lines):
    return [i for i, line in enumerate(lines) if line.split() and not line.split()[0].startswith(";;")
            and ("$MERGED_GRAPH_NUM" in line or "#LASTGRAPHNUM" in line)]


def check_shape(lines, shape, args, let_between):
    graph = graph_lines(lines)
    if [t[0] for _, t in graph] != shape:
        fail(f"the batch's graph commands are {[t[0] for _, t in graph]}, not {shape}")
    got = [t[1] for _, t in graph if t[0] not in ("mff", "rgx")]  # mff and rgx make graphs and name none
    if got != args:
        fail(f"the batch's graph arguments are {got}, not {args}")
    at = [i for i, _ in graph]
    lets = [i for i in uses_of_merged_graph_num(lines) if lines[i].split() == LET]
    first, last = at[let_between[0]], at[let_between[1]]
    if len(lets) != 1 or not first < lets[0] < last:
        fail(f"expected one '{' '.join(LET)}' between lines {first + 1} and {last + 1}")
    return at, lets[0]


def graph83_batch(text):
    """The case's batch with both fb_rch lines on the graph mff made and each corridor saved by wgx."""
    lines = text.split("\n")
    at, let = check_shape(lines, SHAPE, ["0"] * 6 + ["$MERGED_GRAPH_NUM", "0", "0"], (3, 5))
    size83, fb1, fb2 = at[1], at[5], at[8]
    if uses_of_merged_graph_num(lines) != [let, fb2] or not lines[fb1].startswith("fb_rch 0 "):
        fail("expected $MERGED_GRAPH_NUM only in its let and the second fb_rch, and the first fb_rch on graph 0")
    out = list(lines)  # edited from the bottom up, so that the indices above stay right
    out[fb2 + 1:fb2 + 1] = [";; X02: save the corridor that the without-FSC solve runs on",
                            "wgx 0 $WRITE_DIR/corridor_wo.xml KEGG"]
    out[fb1:fb1 + 1] = [";; X02: fb_rch on the graph mff made, not on rgx's re-read of it",
                        lines[fb1].replace("fb_rch 0 ", "fb_rch $MERGED_GRAPH_NUM ", 1),
                        ";; X02: save the corridor that the with-FSC solve runs on",
                        "wgx 0 $WRITE_DIR/corridor_w.xml KEGG"]
    out[let:let + 1] = [";; X02: the let of $MERGED_GRAPH_NUM moved up, before wgx and rgx"]
    out[size83 + 1:size83 + 1] = [";; X02: the id of the graph mff made (83), before rgx re-reads it",
                                  lines[let]]
    check_shape(out, SHAPE_X02, ["0", "0", "0", "$MERGED_GRAPH_NUM", "0", "0", "0", "$MERGED_GRAPH_NUM", "0", "0",
                                 "0"], (0, 2))
    return "\n".join(out)


def recorded_side(case, root):
    base = root / "reference" / case["id"]
    solve = case["fusion1"]["solve"]
    return {"dir": base, "console": base / case["fusion1"]["console"], "corridor": base / case["fusion1"]["corridor"],
            "dat": {c: base / e["dat"] for c, e in solve.items()},
            "limits": {c: base / e["limits"] for c, e in solve.items()}}


def fusion1_tree_side(case):
    """The recorded run's files in FuSION 1's own tree, read only: the dry run's self-test before W1."""
    base = FUSION1 / case["origin"]["outputs"]
    solve = case["fusion1"]["solve"]
    at = lambda rel: base / rel.removeprefix("f1/")  # noqa: E731
    return {"dir": base, "console": at(case["fusion1"]["console"]), "corridor": None,
            "dat": {c: at(e["dat"]) for c, e in solve.items()},
            "limits": {c: at(e["limits"]) for c, e in solve.items()}}


def x02_side(case, root, name):
    base = root / "scratch" / name
    solve = case["fusion1"]["solve"]
    return {"dir": base, "console": base / "console.log",
            "corridor_w": base / "run" / "corridor_w.xml", "corridor_wo": base / "run" / "corridor_wo.xml",
            "dat": {c: base / "run" / Path(e["dat"]).name for c, e in solve.items()},
            "limits": {c: base / "run" / Path(e["limits"]).name for c, e in solve.items()}}


def prerequisites(case, root):
    """What a real run needs in the data root, beyond what run_fusion1.py checks itself."""
    recorded = recorded_side(case, root)
    return [root / "fusion1" / "Linux" / "fusion", root / "fusion1" / "Linux" / "supporting_files",
            root / "kegg" / "hsa_pathways_with_exp_score", root / "kegg" / "updated_pathways",
            root / "reference" / case["id"] / "inputs", FUSION1 / ".venv" / "bin", recorded["console"],
            recorded["corridor"]] + list(recorded["dat"].values()) + list(recorded["limits"].values())


def read(path):
    try:
        return path.read_text(errors="replace") if path else None
    except FileNotFoundError:
        return None


def facts(text):
    found = {}
    for name, pattern in FACTS.items():
        matches = re.findall(pattern, text or "", re.M)
        found[name] = [list(m) if isinstance(m, tuple) else m for m in matches]
    return found


def frontier(text):
    """PO.dat's points, one `n<TAB>e` per line (S5.25)."""
    return None if text is None else [[int(v) for v in line.split()] for line in text.splitlines() if line.strip()]


def unknowns(text):
    """Checks whose answer was unknown (S5.19); None if the limits file is missing."""
    return None if text is None else sum(line == "unknown" for line in text.splitlines())


def read_wgx(path):
    """A wgx file's nodes and edges with keys that need no node ids: FuSION 1 writes no node key, and
    the re-read re-keys nodes (S2.25, S2.26). A node is its type and its set of member ids; an edge is
    its two nodes, its type and its set of subtypes."""
    root = ET.parse(path).getroot()
    entries = {}
    for e in root.findall("entry"):
        names = e.get("name", "").split()
        graphics = e.find("graphics")
        entries[e.get("id")] = {"key": f"{e.get('type')}:{' '.join(sorted(names))}", "names": names,
                                "label": graphics.get("name", "") if graphics is not None else "",
                                "components": [c.get("id") for c in e.findall("component")]}
    key = lambda xml_id: entries[xml_id]["key"] if xml_id in entries else f"?{xml_id}"  # noqa: E731
    nodes = [dict(e, components=sorted(key(c) for c in e["components"])) for e in entries.values()]
    edges = []
    for r in root.findall("relation"):
        subtypes = [s.get("name") for s in r.findall("subtype")]
        edges.append({"key": f"{key(r.get('entry1'))} -> {key(r.get('entry2'))} "
                             f"{r.get('type')}[{','.join(sorted(subtypes))}]",
                      "subtypes": subtypes, "pathways": r.get("pathways", "").split(),
                      "score": r.get("experiment_score")})
    return nodes, edges


def number(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return text


def compare_graphs(a, b):
    (na, ea), (nb, eb) = a, b
    ca, cb = Counter(n["key"] for n in na), Counter(n["key"] for n in nb)
    da, db = Counter(e["key"] for e in ea), Counter(e["key"] for e in eb)
    first_na, first_nb = {n["key"]: n for n in reversed(na)}, {n["key"]: n for n in reversed(nb)}
    first_ea, first_eb = {e["key"]: e for e in reversed(ea)}, {e["key"]: e for e in reversed(eb)}
    nodes, edges = sorted(first_na.keys() & first_nb.keys()), sorted(first_ea.keys() & first_eb.keys())
    pair = lambda k, field: (first_ea[k][field], first_eb[k][field])  # noqa: E731
    result = {
        "nodes": [len(na), len(nb)], "edges": [len(ea), len(eb)],
        "nodes_only_a": sorted((ca - cb).elements()), "nodes_only_b": sorted((cb - ca).elements()),
        "edges_only_a": sorted((da - db).elements()), "edges_only_b": sorted((db - da).elements()),
        "components_differ": [k for k in nodes if first_na[k]["components"] != first_nb[k]["components"]],
        "pathway_sets_differ": [k for k in edges if sorted(pair(k, "pathways")[0]) != sorted(pair(k, "pathways")[1])],
        "scores_differ": [k for k in edges if number(pair(k, "score")[0]) != number(pair(k, "score")[1])],
        "repeated_node_keys": [sum(v > 1 for v in ca.values()), sum(v > 1 for v in cb.values())],
        # What the re-read changes without changing the graph (S2.26):
        "node_order_same": [n["key"] for n in na] == [n["key"] for n in nb],
        "edge_order_same": [e["key"] for e in ea] == [e["key"] for e in eb],
        "labels_differ": sum(first_na[k]["label"] != first_nb[k]["label"] for k in nodes),
        "member_order_differs": sum(first_na[k]["names"] != first_nb[k]["names"] for k in nodes),
        "pathway_order_differs": sum(pair(k, "pathways")[0] != pair(k, "pathways")[1]
                                     and sorted(pair(k, "pathways")[0]) == sorted(pair(k, "pathways")[1]) for k in edges),
        "subtype_order_differs": sum(pair(k, "subtypes")[0] != pair(k, "subtypes")[1] for k in edges),
    }
    result["same_content"] = not any(result[k] for k, _ in DIFFERENCES)
    return result


def corridor_pair(a, b):
    missing = [str(p) for p in (a, b) if p is None or not p.exists()]
    if missing:
        return {"missing": missing}
    try:
        return dict(compare_graphs(read_wgx(a), read_wgx(b)), files=[str(a), str(b)])
    except ET.ParseError as error:
        return {"error": str(error), "files": [str(a), str(b)]}


def show(values):
    return " ".join("/".join(v) if isinstance(v, list) else v for v in values) or "-"


def describe_corridor(c):
    if "missing" in c:
        return [f"  not compared, missing: {', '.join(c['missing'])}"]
    if "error" in c:
        return [f"  not compared, a file does not parse: {c['error']}"]
    lines = [f"  nodes {c['nodes'][0]} and {c['nodes'][1]}, edges {c['edges'][0]} and {c['edges'][1]}; "
             f"same nodes, edges, complexes, pathway sets and scores: {'yes' if c['same_content'] else 'NO'}"]
    for key, what in DIFFERENCES:  # report.json has every item in full
        if c[key]:
            shown = [k if len(k) <= 100 else k[:97] + "..." for k in c[key][:8]]
            lines.append(f"  {what} ({len(c[key])}): {'; '.join(shown)}{' …' if len(c[key]) > len(shown) else ''}")
    if any(c["repeated_node_keys"]):
        lines.append(f"  node keys held by more than one node: {c['repeated_node_keys']}")
    lines.append(f"  order and text: node order {'same' if c['node_order_same'] else 'differs'}, edge order "
                 f"{'same' if c['edge_order_same'] else 'differs'}; labels differ on {c['labels_differ']} nodes, "
                 f"member order alone on {c['member_order_differs']}; pathway order alone differs on "
                 f"{c['pathway_order_differs']} edges, subtype order alone on {c['subtype_order_differs']}")
    return lines


def reading(report):
    if not report["fb_rch_on_merged_graph"]:
        return "not a valid X02 run: the console does not show both fb_rch lines on the graph mff made"
    corridor = report["corridors"]["w against recorded"]
    if "same_content" not in corridor:
        parts = ["the corridors could not be compared"]
    elif corridor["same_content"]:
        parts = ["graph 83 and graph 84 give corridors with the same nodes, edges, complexes, pathway sets and scores"]
    else:
        parts = ["the corridors of graph 83 and graph 84 DIFFER"]
    same = all(f["same"] for f in report["frontiers"].values())
    parts.append("the frontiers are the same" if same else "the frontiers DIFFER")
    if any(v for counts in report["unknown_answers"].values() for v in counts.values()):
        parts.append("but some checks were unknown, so the frontiers are not all certified")
    return "; ".join(parts) + "."


def summary(report):
    fx, fr = report["console"]["x02"], report["console"]["recorded"]
    run = report["run"]
    lines = [f"X02: graph 83 against graph 84. Case {report['case']}, run {report['run_id']}.",
             f"  X02 run: {report['dirs']['x02']}" + (f", exit {run['exit']}, {run['wall_s']} s" if run else ""),
             f"  recorded run: {report['dirs']['recorded']}",
             "console, X02 | recorded:"]
    lines += [f"  {name}: {show(fx[name])} | {show(fr[name])}" for name in FACTS]
    lines.append(f"  both fb_rch lines ran on the graph mff made: {'yes' if report['fb_rch_on_merged_graph'] else 'NO'}")
    lines.append("corridor before the with-FSC solve: X02's, from graph 83 (first), against the recorded run's, "
                 "from graph 84 (second; W3's capture):")
    lines += describe_corridor(report["corridors"]["w against recorded"])
    lines.append("X02's corridor before the without-FSC solve (first) against its corridor before the with-FSC "
                 "solve (second):")
    lines += describe_corridor(report["corridors"]["wo against w"])
    lines.append("frontiers (.dat), points (n, e):")
    for c, f in report["frontiers"].items():
        lines.append(f"  {c}: X02 {f['x02']}, recorded {f['recorded']}: {'same' if f['same'] else 'DIFFER'}")
    lines.append("unknown answers in the limits files: " + "; ".join(
        f"{c}: X02 {u['x02']}, recorded {u['recorded']}" for c, u in report["unknown_answers"].items()))
    lines.append(f"reading: {report['reading']}")
    return "\n".join(lines) + "\n"


def compare(case_id, x02, recorded, out, run_id):
    """Write report.json and summary.txt into the folder out, and return the summary."""
    runner = json.loads(read(x02["dir"] / "runner.json") or "{}")
    fx, fr = facts(read(x02["console"])), facts(read(recorded["console"]))
    merged = fx["merged graph (mff)"]
    report = {
        "experiment": "X02", "case": case_id, "run_id": run_id,
        "dirs": {"x02": str(x02["dir"]), "recorded": str(recorded["dir"])},
        "run": {k: runner[k] for k in ("exit", "wall_s", "started") if k in runner},
        "console": {"x02": fx, "recorded": fr},
        "fb_rch_on_merged_graph": len(merged) == 1 and fx["fb_rch on graph"] == merged * 2
                                  and fx["re-read graph (rgx)"] != merged,
        "corridors": {"w against recorded": corridor_pair(x02["corridor_w"], recorded["corridor"]),
                      "wo against w": corridor_pair(x02["corridor_wo"], x02["corridor_w"])},
        "frontiers": {}, "unknown_answers": {},
    }
    for c in x02["dat"]:
        fa, fb = frontier(read(x02["dat"][c])), frontier(read(recorded["dat"][c]))
        report["frontiers"][c] = {"x02": fa, "recorded": fb, "same": fa is not None and fa == fb}
        report["unknown_answers"][c] = {"x02": unknowns(read(x02["limits"][c])),
                                        "recorded": unknowns(read(recorded["limits"][c]))}
    report["reading"] = reading(report)
    text = summary(report)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (out / "summary.txt").write_text(text)
    return text


def run_fusion1(name, batch, dry_run=False):
    command = [sys.executable, "-I", str(RUN_FUSION1), "--case", CASE_ID, "--name", name, "--batch", str(batch)]
    if dry_run:
        return subprocess.run(command + ["--dry-run"], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    print(f"x02: {' '.join(command[2:])}", flush=True)
    return subprocess.run(command, stdin=subprocess.DEVNULL)


def dry_run(case, root, source, text, batch):
    print(f"x02 dry run: case {CASE_ID}, batch {source}")
    edits = [d for d in difflib.unified_diff(text.splitlines(), batch.splitlines(), lineterm="", n=0)
             if not d.startswith(("---", "+++"))]
    print("  the copy's edits (line numbers of the case's batch, then the copy's):")
    print("".join(f"    {d}\n" for d in edits), end="")
    with tempfile.TemporaryDirectory(prefix="x02-dry-run-") as tmp:
        tmp = Path(tmp)
        (tmp / "x02.batch").write_text(batch)
        done = run_fusion1("x02-dry-run", tmp / "x02.batch", dry_run=True)
        if done.returncode != 0:
            fail(f"run_fusion1.py --dry-run failed:\n{done.stdout}{done.stderr}")
        mapped = [line for line in done.stdout.splitlines()
                  if line.split()[:1] in (["fb_rch"], ["wgx"], ["rgx"], ["mff"]) or "MERGED_GRAPH_NUM" in line]
        print("  run_fusion1.py --dry-run ok; the mapped batch's graph lines:")
        print("".join(f"    {line[:150]}\n" for line in mapped), end="")
        print("  data-root inputs a real run needs:")
        for p in prerequisites(case, root):
            print(f"    {'ok     ' if p.exists() else 'MISSING'} {p}{'  (W3)' if p.name == 'corridor.xml' else ''}")
        # Self-test on recorded files: wgx's file of a merged graph stands in for each corridor, the
        # reference run's for the recorded corridor and fdr:0.3's for X02's, so that the two differ.
        filled = (root / "reference" / CASE_ID / "f1").is_dir()
        recorded = recorded_side(case, root) if filled else fusion1_tree_side(case)
        other = load_case(STAND_IN_CASE)
        other_dir = root / "reference" / STAND_IN_CASE / "f1" if filled else FUSION1 / other["origin"]["outputs"]
        recorded["corridor"] = recorded["dir"] / ("f1/run" if filled else "run") / "merged_master_graph.xml"
        x02 = dict(recorded, corridor_w=other_dir / "run" / "merged_master_graph.xml",
                   corridor_wo=other_dir / "run" / "merged_master_graph.xml")
        (tmp / "report").mkdir()
        summary_text = compare(CASE_ID, x02, recorded, tmp / "report", "self-test")
        written = sorted(p.name for p in (tmp / "report").iterdir())
        print(f"  self-test of the comparison on {recorded['dir']}: its own files on both sides, except that "
              f"the corridors are stand-ins, fdr:0.3's merged graph for X02's and the reference run's for the "
              f"recorded one. It wrote {', '.join(written)}. Its summary:")
        print("".join(f"    | {line}\n" for line in summary_text.splitlines()), end="")
    print("x02 dry run: done; nothing was written outside the temporary folder, which is removed")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--compare-only", metavar="RUN_ID")
    args = parser.parse_args()
    root = data_root()
    case = load_case(CASE_ID)
    source = batch_source(case)
    text = source.read_text(errors="strict")
    batch = graph83_batch(text)
    if args.dry_run:
        dry_run(case, root, source, text, batch)
        return
    now = time.strftime("%Y%m%d-%H%M%S")
    run_id = args.compare_only or now
    if not re.fullmatch(r"\d{8}-\d{6}", run_id):
        fail("RUN_ID has the form YYYYmmdd-HHMMSS")
    name = f"x02-{run_id}"
    out = root / "reports" / "x02" / (f"{run_id}-compared-{now}" if args.compare_only else run_id)
    if out.exists():
        fail(f"{out} exists; every run writes to a fresh folder")
    if args.compare_only:
        if not (root / "scratch" / name).is_dir():
            fail(f"no run {name} in {root / 'scratch'}")
    else:
        missing = [str(p) for p in prerequisites(case, root) if not p.exists()]
        if (root / "scratch" / name).exists():
            fail(f"{root / 'scratch' / name} exists; every run gets a fresh scratch folder")
        if missing:
            fail("missing from the data root: " + ", ".join(missing))
    out.mkdir(parents=True)
    if not args.compare_only:
        (out / "x02.batch").write_text(batch)
        code = run_fusion1(name, out / "x02.batch").returncode
        if code != 0:
            print(f"x02: the run exited {code}; comparing what it wrote", file=sys.stderr)
    print(compare(CASE_ID, x02_side(case, root, name), recorded_side(case, root), out, run_id), end="")
    print(f"x02: wrote {out}")


if __name__ == "__main__":
    main()
