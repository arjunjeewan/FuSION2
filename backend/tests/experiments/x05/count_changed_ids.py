#!/usr/bin/env python3
"""X05: count the ids that bind to another node in the full run's corridor than in graph 2.

Phase 3 plan (docs/phase3/plan.md), experiments table, X05; section 12 of the
architecture, "Phase 3 experiments" (X05) and the last sentence of "Capture in
Phase 3"; spec stage 4, open question 3, and S4.15; expected difference XD-08.

The question. fb_rch resolves ids against graph 2, the graph rgx re-read, and
pathz3 resolves them against the corridor's own lookup, which fb_rch fills as it
copies nodes (S4.15). Both lookups are insert-only, so an id binds to the first
node that claims it. Replaying rgx's claim order (relation order, components
first) predicted that 57 of the 1399 ids the full run's corridor resolves change
node: 50 because graph 2's first claimer is outside the corridor, 7 because of
copy order. Claiming in entry-file order predicted 58 (50 + 8). XD-08 records
"57 or 58". This script counts them in W3's capture of jun-b10-old, the full run
(docs/spec/04-reachability-corridor.md, line 9).

How the capture answers it.
  - The corridor: duplicate_node registers a node's ids as it creates the node
    (Linux/src/GraphManagerNew.cc:9229-9248), and wgx writes nodes in creation
    order (S2.25), so corridor.xml lists the corridor's nodes in claim order.
  - Graph 2: rgx registers an entry's ids just before it creates the entry's node
    (Linux/src/XMLParser.cc:966-1037, 1084-1115), so graph_reread.xml, also in
    creation order, is in claim order too, but for one case: an entry whose key
    names a node that an entry with other ids created first registers its extra
    ids to that node without adding them to it (S2.08). The script looks for such
    entries in the file rgx read; for the full run, a wgx of graph 1, it expects none.
An id changes node when its corridor node is not the copy of its graph-2 node.
A copy keeps its node's ids (S4.13), so nodes are matched by their written ids;
a node is named here by its first id, its key (S2.07).

The gap. The capture holds both graphs' nodes in claim order, but neither lookup
and not the file rgx read, which the S2.08 check needs. The check reads the
recorded run's copy of that file and its exception set (f1/run/); the capture
batch re-creates both in its scratch folder, but W3 does not keep them.

Usage:
  FUSION_TEST_DATA=<root> python3 -I count_changed_ids.py [--dry-run]

  --dry-run   run the analysis on a built-in example and, once W3 has captured the
              full run, on the capture, both in a temporary folder that is then
              removed; write nothing in the data root

Inputs, in the data root, under reference/jun-b10-old/:
  f1/capture/corridor.xml, f1/capture/graph_reread.xml     W3's capture
  f1/run/merged_master_graph.xml, f1/run/exception_file.txt  the recorded run (W1)
  inputs/hsa_to_gene_symbol_map.txt                         gene symbols (W1)
Outputs, in the data root, in reports/x05/, which must not exist yet:
  summary.json      the counts, the verdict on S4.15 and XD-08, the checks and the gap
  changed_ids.tsv   one row per id that changes node
"""
import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
from pathlib import Path

ARJUN = Path("/home/ks-149-u24/Desktop/arjun")
NOT_A_DATA_ROOT = [Path(__file__).resolve().parents[4], ARJUN / "FuSION", ARJUN / "FuSION1-reference-2026-11-10"]
CASE = "jun-b10-old"
INPUTS = {
    "corridor": f"reference/{CASE}/f1/capture/corridor.xml",
    "reread": f"reference/{CASE}/f1/capture/graph_reread.xml",
    "source": f"reference/{CASE}/f1/run/merged_master_graph.xml",
    "exceptions": f"reference/{CASE}/f1/run/exception_file.txt",
    "symbols": f"reference/{CASE}/inputs/hsa_to_gene_symbol_map.txt",
}
OUT = "reports/x05"
S4_15 = {"ids_resolved": 1399, "predictions": {
    "rgx's claim order (relation order, components first)": {"changed": 57, "outside": 50, "copy_order": 7},
    "entry-file order": {"changed": 58, "outside": 50, "copy_order": 8}}}
EXAMPLE = "hsa1386"  # ATF2: S4.15's example, its own node in graph 2, the CREB3-family node in the corridor
OUTSIDE, COPY_ORDER = "first claimer outside the corridor", "copy order"
COLUMNS = ["id", "symbol", "reason", "graph2_entry", "graph2_node", "graph2_ids", "graph2_label",
           "corridor_entry", "corridor_node", "corridor_ids", "corridor_label"]
GAP = ("The capture holds both graphs' nodes in claim order, but neither id lookup, and not the "
       "file rgx read. Graph 2's lookup follows its creation order only if no entry of that file "
       "registered ids to a node that lacks them (S2.08); checking this needs the file and the "
       "exception set, which this analysis takes from the recorded run (f1/run/), not the capture.")

ENTRY = re.compile(r'^\t<entry id="(\d+)" name="([^"]*)" type="([^"]*)">$')
GRAPHICS = re.compile(r'^\t\t<graphics name="(.*)"/>$')
COMPONENT = re.compile(r'^\t\t<component id="(\d+)"/>$')


def fail(message):
    sys.exit(f"count_changed_ids: {message}")


def read_wgx(path):
    """A wgx-written file's entries in file order, which is creation order (S2.25), and its
    relation count. The writer escapes nothing, so its lines are matched, not parsed as XML."""
    entries, relations = [], 0
    for line in path.read_text(encoding="utf-8", errors="replace").split("\n"):
        m = ENTRY.match(line)
        if m:
            if int(m.group(1)) != len(entries) + 1:
                fail(f"{path}: entry {m.group(1)} is out of sequence")
            entries.append({"name": m.group(2), "ids": m.group(2).split(" "), "type": m.group(3),
                            "label": "", "components": []})
        elif line.startswith("\t<relation "):
            relations += 1
        elif entries and GRAPHICS.match(line):
            entries[-1]["label"] = GRAPHICS.match(line).group(1).split(", ")[0]
        elif entries and COMPONENT.match(line):
            entries[-1]["components"].append(int(COMPONENT.match(line).group(1)))
    if not entries:
        fail(f"{path}: no entries")
    return entries, relations


def first_claims(entries):
    """id -> index of the first entry that lists it: an insert-only lookup filled in entry order."""
    claims = {}
    for k, entry in enumerate(entries):
        for x in entry["ids"]:
            claims.setdefault(x, k)
    return claims


def split_string(name):
    """rgx's ids of an entry: colons dropped, split on single spaces (main.cc:954-973)."""
    name = name.replace(":", "")
    ids, pos = set(), 0
    while pos < len(name):
        end = name.find(" ", pos)
        if end == -1:
            ids.add(name[pos:])
            break
        ids.add(name[pos:end])
        pos = end + 1
    return ids


def keyed_sets(source, exceptions):
    """For the file rgx read: each key with the id sets of the entries that key to it. With
    threshold 1, which the full run passes like every batch (S2.05), a non-group entry keys
    to its smallest id outside the exception set (S2.07); groups key on their components."""
    sets = {}
    for entry in source:
        if entry["components"]:
            continue
        kept = sorted(x for x in split_string(entry["name"]) if x not in exceptions)
        if kept:
            sets.setdefault(kept[0], set()).add(frozenset(kept))
    return sets


def missing_inputs(root):
    return [rel for rel in INPUTS.values() if not (root / rel).is_file()]


def describe(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def analyse(root):
    paths = {key: root / rel for key, rel in INPUTS.items()}
    reread, reread_edges = read_wgx(paths["reread"])
    corridor, corridor_edges = read_wgx(paths["corridor"])
    source, _ = read_wgx(paths["source"])
    exceptions = {(line.split() or [""])[0] for line in paths["exceptions"].read_text().splitlines()}
    symbols = dict(line.split("\t", 1) for line in paths["symbols"].read_text(errors="replace").splitlines()
                   if "\t" in line)

    by_ids = {}
    for k, entry in enumerate(reread):
        by_ids.setdefault((entry["name"], entry["type"]), []).append(k)
    if any(len(ks) > 1 for ks in by_ids.values()):
        fail("two nodes of graph 2 have the same ids, so copies cannot be matched")
    copy_of = []
    for c, entry in enumerate(corridor):
        if (entry["name"], entry["type"]) not in by_ids:
            fail(f"corridor entry {c + 1} ({entry['name']}) is no node of graph 2")
        copy_of.append(by_ids[(entry["name"], entry["type"])][0])
    if len(set(copy_of)) != len(copy_of):
        fail("two corridor nodes copy one node of graph 2")
    position = {k: c for c, k in enumerate(copy_of)}

    in_graph2, in_corridor = first_claims(reread), first_claims(corridor)
    rows = []
    for x, c in in_corridor.items():
        g = in_graph2[x]  # the copy of corridor node c lists x, so graph 2 resolves it
        if copy_of[c] == g:
            continue
        reason = COPY_ORDER if g in position else OUTSIDE
        rows.append({"id": x, "symbol": symbols.get(x, ""), "reason": reason,
                     "graph2_entry": g + 1, "graph2_node": reread[g]["ids"][0],
                     "graph2_ids": len(reread[g]["ids"]), "graph2_label": reread[g]["label"],
                     "corridor_entry": c + 1, "corridor_node": corridor[c]["ids"][0],
                     "corridor_ids": len(corridor[c]["ids"]), "corridor_label": corridor[c]["label"]})
    rows.sort(key=lambda r: (r["reason"] != OUTSIDE, r["id"]))

    # S2.08: may graph 2's lookup differ from its creation order? Only through a key that
    # entries with different ids share, and only for the ids they do not all list.
    sets = keyed_sets(source, exceptions)
    shared = [s for s in sets.values() if len(s) > 1]
    at_risk = set()
    for s in shared:
        at_risk |= frozenset().union(*s) - frozenset.intersection(*s)
    unsettled = sorted(at_risk & set(in_corridor))
    source_ids = set().union(*(split_string(e["name"]) for e in source if not e["components"]))
    unexplained = 0
    for entry in reread:
        ids = entry["ids"]
        if entry["components"]:
            continue
        if len(ids) == 1 and ids[0] in exceptions:
            unexplained += ids[0] not in source_ids
        else:
            unexplained += ids != sorted(ids) or frozenset(ids) not in sets.get(ids[0], ())

    outside = sum(r["reason"] == OUTSIDE for r in rows)
    counts = {"changed": len(rows), "outside": outside, "copy_order": len(rows) - outside}
    matches = [name for name, p in S4_15["predictions"].items() if p == counts]
    told = f"{counts['changed']} ids change node ({counts['outside']} + {counts['copy_order']})"
    if unexplained:
        verdict = (f"not settled: the recorded run's merged_master_graph.xml does not explain "
                   f"{unexplained} nodes of graph 2, so the S2.08 check does not hold; {told}")
    elif unsettled:
        verdict = f"not settled for {len(unsettled)} ids that S2.08 may bind elsewhere in graph 2; {told}"
    elif matches:
        verdict = f"{told}, as S4.15's replay with {matches[0]} predicts"
    else:
        verdict = f"{told}, which matches neither of S4.15's replays (57 = 50 + 7, 58 = 50 + 8)"
    example = None
    if EXAMPLE in in_corridor:
        g, c = in_graph2[EXAMPLE], in_corridor[EXAMPLE]
        example = {"changes_node": copy_of[c] != g,
                   "graph2_node": reread[g]["ids"][0], "graph2_ids": len(reread[g]["ids"]),
                   "corridor_node": corridor[c]["ids"][0], "corridor_ids": len(corridor[c]["ids"])}
    summary = {
        "item": "X05",
        "question": ("stage 4, open question 3 (S4.15): how many of the ids that the full run's corridor "
                     "resolves bind to another node there than in graph 2, which fb_rch read"),
        "case": CASE,
        "inputs": {key: dict(path=rel, **describe(paths[key])) for key, rel in INPUTS.items()},
        "graph2": {"nodes": len(reread), "edges": reread_edges},
        "corridor": {"nodes": len(corridor), "edges": corridor_edges, "ids_resolved": len(in_corridor)},
        "changed": counts["changed"],
        "first_claimer_outside_corridor": counts["outside"],
        "copy_order": counts["copy_order"],
        "s4_15": S4_15,
        "matches": matches[0] if matches else None,
        "verdict": verdict,
        "xd_08_prediction_holds": not unexplained and not unsettled and counts["changed"] in (57, 58),
        "example_" + EXAMPLE: example,
        "checks": {"graph2_nodes_not_explained_by_the_file_rgx_read": unexplained,
                   "keys_that_entries_with_other_ids_share": len(shared),
                   "corridor_ids_unsettled_by_s2_08": unsettled},
        "gap": GAP,
    }
    return summary, rows


def write(out, summary, rows):
    out.mkdir(parents=True)  # stops if it exists: each run writes to a fresh folder
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    with open(out / "changed_ids.tsv", "w") as f:
        f.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            f.write("\t".join(str(r[c]) for c in COLUMNS) + "\n")


def report(summary, where):
    c = summary["corridor"]
    print(f"{summary['changed']} of the {c['ids_resolved']} ids the corridor resolves change node "
          f"({summary['first_claimer_outside_corridor']} first claimer outside the corridor, "
          f"{summary['copy_order']} copy order); corridor {c['nodes']} nodes, {c['edges']} edges")
    print(f"verdict: {summary['verdict']}")
    print(f"XD-08's prediction (57 or 58) holds: {summary['xd_08_prediction_holds']}")
    print(f"written to {where}")


def write_example(root):
    """A made-up full run in wgx's format (GraphManagerNew.cc:2892-2953): t5's first claimer,
    the entry 't2 t5', is outside the corridor, and t6's node 't4 t6' is copied after 't6'."""
    def wgx(entries, relations):
        lines = ['<?xml version="1.0"?>',
                 '<!DOCTYPE pathway SYSTEM "http://www.kegg.jp/kegg/xml/KGML_v0.7.1_.dtd">',
                 '<pathway name="KEGG">']
        for k, (name, kind, label, parts) in enumerate(entries, 1):
            lines += [f'\t<entry id="{k}" name="{name}" type="{kind}">', f'\t\t<graphics name="{label}"/>']
            lines += [f'\t\t<component id="{p}"/>' for p in parts] + ["\t</entry>"]
        for a, b in relations:
            lines += [f'\t<relation entry1="{a}" entry2="{b}" type="PPrel" pathways="path:x" '
                      f'experiment_score="0">', '\t\t<subtype name="activation"/>', "\t</relation>"]
        return "\n".join(lines + ["</pathway>", ""])
    graph2 = [("t1", "gene", "T1", []), ("t2 t5", "gene", "T2", []), ("t5", "gene", "T5", []),
              ("t4 t6", "gene", "T4", []), ("t6", "gene", "T6", []),
              ("c_s_t1_t4_c_e", "group", "[T1+T4]", [1, 4])]
    corridor = [graph2[0], graph2[4], graph2[2], graph2[3], ("c_s_t1_t4_c_e", "group", "[T1+T4]", [1, 4])]
    files = {"reread": wgx(graph2, [(1, 2), (2, 3), (4, 5), (1, 4)]),
             "source": wgx(graph2, [(1, 2), (2, 3), (4, 5), (1, 4)]),
             "corridor": wgx(corridor, [(1, 2), (2, 3), (3, 4)]),
             "exceptions": "t9\n", "symbols": "t5\tFIVE\nt6\tSIX\n"}
    for key, text in files.items():
        (root / INPUTS[key]).parent.mkdir(parents=True, exist_ok=True)
        (root / INPUTS[key]).write_text(text)


def dry_run(root):
    with tempfile.TemporaryDirectory(prefix="x05-") as tmp:
        tmp = Path(tmp)
        write_example(tmp / "example")
        summary, rows = analyse(tmp / "example")
        write(tmp / "example-out", summary, rows)
        got = (summary["corridor"]["ids_resolved"], [(r["id"], r["reason"]) for r in rows],
               summary["checks"]["keys_that_entries_with_other_ids_share"])
        want = (5, [("t5", OUTSIDE), ("t6", COPY_ORDER)], 0)
        if got != want:
            fail(f"the built-in example gave {got}, not {want}")
        print("built-in example: ok (t5 first claimer outside the corridor, t6 copy order)")
        missing = missing_inputs(root)
        if missing:
            print(f"not in the data root {root} yet (W1, W3):\n  " + "\n  ".join(missing))
        else:
            summary, rows = analyse(root)
            write(tmp / "x05", summary, rows)
            report(summary, f"{tmp / 'x05'} (temporary)")
    if (root / OUT).exists():
        print(f"{root / OUT} exists: the real run would stop")
    print("dry run: nothing written in the data root; the temporary folder is removed")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    root = os.environ.get("FUSION_TEST_DATA")
    if not root:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(root).resolve()
    if any(root.is_relative_to(tree) for tree in NOT_A_DATA_ROOT):
        fail(f"the data root must lie outside this repository, FuSION 1 and its snapshot: {root}")
    if args.dry_run:
        sys.exit(dry_run(root))
    if (root / OUT).exists():
        fail(f"{root / OUT} exists; each run writes to a fresh folder")
    missing = missing_inputs(root)
    if missing:
        fail("missing from the data root (W1, W3):\n  " + "\n  ".join(missing))
    summary, rows = analyse(root)
    write(root / OUT, summary, rows)
    report(summary, root / OUT)


if __name__ == "__main__":
    main()
