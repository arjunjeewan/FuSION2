#!/usr/bin/env python3
"""X03: does each id sit in exactly one node in FuSION 1's merges?

Phase 3 plan (docs/phase3/plan.md), experiment X03; section 12 of the architecture
(docs/architecture/12-tests.md, "Phase 3 experiments"); spec stage 2, open question 4
(docs/spec/02-graph-input-and-merge.md); section 03 (docs/architecture/03-merge.md).

What it does:
  1. draws, from a fixed seed, 100 random lists of 10 to 60 of the 167 KGMLs, each in two
     orders: (a) the order of the candidate list, which FuSION 1's own lists keep (S7.15),
     and (b) a random permutation;
  2. merges each of the 200 lists with FuSION 1's mff and writes the result with wgx
     (merge_and_write.batch: split threshold 1, raw read, the reference run's exception
     set), through run_fusion1.py for case jun-b10-fdr025, whose path map covers the KGML
     folders; the list and the exception file go in with --extra-inputs;
  3. checks each written XML for an id that sits in more than one gene or compound entry.
     Group entries name complex keys, not ids, and are left out (section 03: "each id is
     in exactly one gene or compound node").

Inputs, in the data root ($FUSION_TEST_DATA):
  reference/jun-b10-fdr025/inputs/list_of_kegg_xml_files.txt
      the 167 KGMLs as FuSION 1 names them; each must map to a file of
      kegg/hsa_pathways_with_exp_score/ or kegg/updated_pathways/, which hold no others
  reference/jun-b10-fdr025/f1/run/exception_file.txt   the reference run's exception set
  fusion1/Linux/fusion and the case's inputs/, which run_fusion1.py uses

Outputs (KEGG data, so all in the data root, S8.31):
  reports/x03/<stamp>/inputs/<run>/   each run's list and exception file
  reports/x03/<stamp>/results.tsv     one row per merge, written as the runs go
  reports/x03/<stamp>/violations.tsv  one row per id found in two or more nodes
  reports/x03/<stamp>/summary.json    settings, input hashes and counts, at the end
  scratch/x03-<stamp>-<run>/          run_fusion1.py's scratch folder, kept: case.batch,
                                      console.log, runner.json, merged_graph.xml
  <run> is l001a to l100b: list number, then order.

Usage:
  FUSION_TEST_DATA=<root> python3 -I run_x03.py [--dry-run]

Both modes first check that the batch's last command is exit: FuSION 1 reads past a batch's
end and loops on stdin (S1.04), so a batch without one never stops. --dry-run checks the
inputs, draws the lists, writes the first run's inputs to a temporary folder, prints
run_fusion1.py's dry run for it, checks that the mapped batch also ends with exit, and
removes the folder. It starts no FuSION 1 run. Before the data root is filled, it reads the
case's list and exception file from FuSION 1's tree, read only, and says so.

The real run refuses an existing report or scratch folder, and stops if run_fusion1.py
fails before FuSION 1 starts. A FuSION 1 run that fails is recorded, and the next one goes
on. Machine time: about 20 min on one core, the 200 runs one after another.
"""
import argparse
import hashlib
import json
import os
import platform
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ARJUN = "/home/ks-149-u24/Desktop/arjun"
FUSION1 = f"{ARJUN}/FuSION"
SNAPSHOT = f"{ARJUN}/FuSION1-reference-2026-11-10"
HERE = Path(__file__).resolve().parent
TESTS = HERE.parents[1]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"
RUN_FUSION1 = TESTS / "experiments" / "capture" / "run_fusion1.py"
BATCH = HERE / "merge_and_write.batch"

CASE = "jun-b10-fdr025"
POPULATION = "list_of_kegg_xml_files.txt"  # in the case's inputs/
KGML_DIRS = ("kegg/hsa_pathways_with_exp_score", "kegg/updated_pathways")
N_KGML = 167
SEED = 42
N_LISTS = 100
MIN_SIZE, MAX_SIZE = 10, 60
LIST_NAME = "x03_kgml_list.txt"  # "list" in the name: run_fusion1.py maps its paths
EXCEPTION_NAME = "x03_exception_file.txt"
XML_NAME = "merged_graph.xml"  # the batch's wgx line writes it in the scratch folder
ORDERS = {
    "a": "the candidate list's order, which FuSION 1's own lists keep (S7.15)",
    "b": "a random permutation of the same KGMLs",
}

ENTRY = re.compile(r'\s*<entry id="(\d+)" name="([^"]*)" type="([^"]*)"')
RELATION = re.compile(r'\s*<relation entry1="(\d+)" entry2="(\d+)"')
WRITTEN = re.compile(r"^(\d+) (nodes|edges) written to xml file$", re.M)
TYPE_CONFLICT = "Warning: Node types do not match"  # S2.19: mff drops that pathway

RESULT_COLUMNS = ["run", "list", "order", "kgmls", "exit", "wall_s", "status", "entries", "groups",
                  "relations", "ids", "ids_in_two_or_more_nodes", "type_conflicts", "scratch"]
VIOLATION_COLUMNS = ["run", "list", "order", "id", "nodes", "entries"]


def fail(message):
    sys.exit(f"run_x03: {message}")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def data_root():
    value = os.environ.get("FUSION_TEST_DATA")
    if not value:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(value).resolve()
    for tree in (FUSION1, SNAPSHOT, str(REPO)):
        if str(root) == tree or str(root).startswith(tree + "/"):
            fail(f"the data root must lie outside FuSION 1, its snapshot and this repository: {root}")
    return root


def load_case():
    for case in json.loads(CASES.read_text(encoding="utf-8"))["cases"]:
        if case["id"] == CASE:
            return case
    fail(f"no case {CASE} in {CASES}")


def source(root, rel, fusion1_path, dry):
    """A data-root file; in a dry run before the root is filled, FuSION 1's copy, read only."""
    path = root / rel
    if path.is_file():
        return path, rel
    if not dry:
        fail(f"{path} is missing: fill the data root first (W1)")
    print(f"note: {rel} is not in the data root yet; the dry run reads FuSION 1's copy, read only: {fusion1_path}")
    return fusion1_path, str(fusion1_path)


def kgml_maps(case, root):
    """The case's path-map entries for the two KGML folders: (FuSION 1 folder, data-root folder)."""
    maps = []
    for entry in case["path_map"]:
        for folder in KGML_DIRS:
            if entry["to"] == f"$DATA/{folder}":
                maps.append((entry["from"], root / folder))
    if len(maps) != len(KGML_DIRS):
        fail(f"{CASE}'s path map does not cover {', '.join(KGML_DIRS)}")
    return maps


def read_population(case, root, dry):
    rel = f"reference/{CASE}/inputs/{POPULATION}"
    path, shown = source(root, rel, Path(FUSION1) / case["origin"]["inputs"] / POPULATION, dry)
    text = path.read_text(encoding="utf-8")
    if "\r" in text:
        fail(f"{shown} has CR characters, which break mff's list reader (S2.16)")
    lines = text.splitlines()
    if len(lines) != N_KGML or len(set(lines)) != N_KGML:
        fail(f"{shown}: expected {N_KGML} distinct lines, found {len(lines)} ({len(set(lines))} distinct)")
    maps = kgml_maps(case, root)
    mapped = []
    for line in lines:
        hits = [folder / line[len(f) + 1:] for f, folder in maps if line.startswith(f + "/")]
        if len(hits) != 1:
            fail(f"{shown}: {line} is not in one of the case's two KGML folders")
        mapped.append(hits[0])
    if all(folder.is_dir() for _, folder in maps):
        present = {p for _, folder in maps for p in folder.glob("*.xml")}
        if present != set(mapped):
            fail(f"the data root's KGML folders hold {len(present)} files, not the {N_KGML} the list names")
        print(f"population: {shown}, {N_KGML} KGMLs, each found in the data root's {' and '.join(KGML_DIRS)}")
    elif dry:
        missing = [line for line in lines if not Path(line).is_file()]
        if missing:
            fail(f"{len(missing)} listed KGMLs are missing in FuSION 1's tree, e.g. {missing[0]}")
        print(f"population: {shown}, {N_KGML} KGMLs; the data root's KGML folders do not exist yet, so "
              f"only FuSION 1's originals were found (read only)")
    else:
        fail(f"the data root lacks {' or '.join(KGML_DIRS)}: fill it first (W1)")
    return lines, path, shown


def read_exceptions(case, root, dry):
    rel_in_case = case["fusion1"]["exceptions"]  # f1/run/exception_file.txt
    original = Path(FUSION1) / case["origin"]["outputs"] / Path(rel_in_case).relative_to("f1")
    path, shown = source(root, f"reference/{CASE}/{rel_in_case}", original, dry)
    ids = {line.split()[0] for line in path.read_text(encoding="utf-8").splitlines() if line.split()}
    if not ids:
        fail(f"{shown} names no id")
    print(f"exception set: {shown}, {len(ids)} distinct ids")
    return path, shown, len(ids)


def draw(size):
    """N_LISTS lists, each as (order a, order b): indices into the population."""
    rng = random.Random(SEED)
    lists = []
    for _ in range(N_LISTS):
        k = rng.randint(MIN_SIZE, MAX_SIZE)
        in_list_order = sorted(rng.sample(range(size), k))
        lists.append((in_list_order, rng.sample(in_list_order, k)))
    return lists


def last_command(text):
    """The last command of a FuSION 1 batch, as its tokens (S1.05, S1.06), or None if CR is present.

    Tokens are split on spaces only; a line with no tokens or whose first token starts
    with ";;" is skipped. FuSION 1 reads past a batch's end and then loops on stdin (S1.04),
    and run_fusion1.py gives it /dev/null, so a batch must end with exit or it never stops.
    """
    if "\r" in text:
        return None  # exit\r would not be the exit command (S1.05)
    commands = [tokens for tokens in ([t for t in line.split(" ") if t] for line in text.split("\n"))
                if tokens and not tokens[0].startswith(";;")]
    return commands[-1] if commands else None


def check_ends_with_exit(text, where):
    tokens = last_command(text)
    if not tokens or tokens[0] != "exit":
        fail(f"{where} does not end with an exit line, so FuSION 1 would never stop (S1.04)")


def write_inputs(folder, population, order, exceptions):
    folder.mkdir(parents=True)
    (folder / LIST_NAME).write_text("".join(population[i] + "\n" for i in order), encoding="utf-8")
    shutil.copyfile(exceptions, folder / EXCEPTION_NAME)


def fusion1_command(name, inputs, dry):
    command = [sys.executable, "-I", str(RUN_FUSION1), "--case", CASE, "--name", name,
               "--batch", str(BATCH), "--extra-inputs", str(inputs)]
    return command + ["--dry-run"] if dry else command


def check_xml(path):
    """Entries, relations, and every id held by two or more gene or compound entries."""
    entries = {}
    entry_lines = relations = 0
    touched = set()
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = ENTRY.match(line)
            if m:
                entry_lines += 1
                entries[m.group(1)] = (m.group(3), m.group(2).split())
                continue
            m = RELATION.match(line)
            if m:
                relations += 1
                touched.update(m.groups())
    holders = {}
    for eid, (etype, ids) in entries.items():
        if etype != "group":
            for i in set(ids):
                holders.setdefault(i, []).append(eid)
    multi = {i: [(e, entries[e][0], len(entries[e][1]), e not in touched) for e in sorted(es, key=int)]
             for i, es in sorted(holders.items()) if len(es) > 1}
    return {"entries": entry_lines, "distinct_entries": len(entries),
            "groups": sum(t == "group" for t, _ in entries.values()), "relations": relations,
            "ids": len(holders), "multi": multi}


def tsv_line(values):
    return "\t".join(str(v) for v in values) + "\n"


def main():
    parser = argparse.ArgumentParser(description="X03: one node per id in FuSION 1's merges")
    parser.add_argument("--dry-run", action="store_true", help="check and print; start no FuSION 1 run")
    args = parser.parse_args()
    dry = args.dry_run

    root = data_root()
    case = load_case()
    population, population_path, population_shown = read_population(case, root, dry)
    exceptions, exceptions_shown, n_exception_ids = read_exceptions(case, root, dry)
    for path in (BATCH, RUN_FUSION1):
        if not path.is_file():
            fail(f"{path} is missing")
    check_ends_with_exit(BATCH.read_text(encoding="utf-8"), f"the batch {BATCH}")
    print(f"batch: {BATCH.relative_to(REPO)}, last command exit (S1.04)")
    lists = draw(len(population))
    stamp = time.strftime("%Y%m%dT%H%M%S")
    out = root / "reports" / "x03" / stamp
    runs = [(f"l{i:03d}{order}", i, order, indices)
            for i, pair in enumerate(lists, 1) for order, indices in zip("ab", pair)]
    names = {run: f"x03-{stamp}-{run}" for run, _, _, _ in runs}
    taken = [n for n in names.values() if (root / "scratch" / n).exists()]
    if out.exists() or taken:
        fail(f"{out if out.exists() else root / 'scratch' / taken[0]} exists; every run writes to fresh folders")
    sizes = [len(a) for a, _ in lists]
    print(f"lists: {N_LISTS} from seed {SEED}, {min(sizes)} to {max(sizes)} KGMLs (mean {sum(sizes) / len(sizes):.1f}),"
          f" two orders each: {len(runs)} FuSION 1 runs reading {2 * sum(sizes)} KGMLs in all")

    if dry:
        binary = root / "fusion1" / "Linux" / "fusion"
        print(f"FuSION 1 binary {binary}: {'present' if binary.is_file() else 'missing (W1, W2 fill it)'}")
        first, _, _, order = runs[0]
        print(f"{first} would merge: " + ", ".join(Path(population[i]).stem for i in order))
        print(f"{runs[1][0]} would merge: " + ", ".join(Path(population[i]).stem for i in runs[1][3]))
        with tempfile.TemporaryDirectory(prefix="x03-dry-") as tmp:
            inputs = Path(tmp) / "inputs" / first
            write_inputs(inputs, population, order, exceptions)
            print(f"----- run_fusion1.py --dry-run for {first} (inputs in {inputs}) -----")
            shown = subprocess.run(fusion1_command(names[first], inputs, True), capture_output=True, text=True)
            print(shown.stdout + shown.stderr, end="")
            if shown.returncode != 0:
                fail(f"run_fusion1.py's dry run failed with exit {shown.returncode}")
            marker = "----- mapped batch -----\n"
            if marker not in shown.stdout:
                fail("run_fusion1.py's dry run printed no mapped batch")
            check_ends_with_exit(shown.stdout.split(marker, 1)[1], "the mapped batch")
            print("the mapped batch, which run_fusion1.py writes as case.batch, also ends with exit")
        print(f"would write {out}/ and {root / 'scratch'}/x03-{stamp}-<run>/ (none of them exists)")
        print("dry run: nothing written outside the temporary folder, which is removed")
        return

    binary = root / "fusion1" / "Linux" / "fusion"
    if not binary.is_file():
        fail(f"{binary} is missing: fill the data root first (W1, W2)")
    if not (root / "reference" / CASE / "inputs").is_dir():
        fail(f"{root / 'reference' / CASE / 'inputs'} is missing: fill the data root first (W1)")
    started = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    load = os.getloadavg()
    out.mkdir(parents=True)
    for run, _, _, order in runs:
        write_inputs(out / "inputs" / run, population, order, exceptions)
    results = out / "results.tsv"
    violations = out / "violations.tsv"
    results.write_text(tsv_line(RESULT_COLUMNS), encoding="utf-8")
    violations.write_text(tsv_line(VIOLATION_COLUMNS), encoding="utf-8")

    counts = {"ok": 0, "violation": 0, "no_xml": 0, "count_mismatch": 0}
    nonzero = 0
    bad_ids = set()
    binary_sha = None
    for n, (run, number, order_name, order) in enumerate(runs, 1):
        name = names[run]
        scratch = root / "scratch" / name
        launched = subprocess.run(fusion1_command(name, out / "inputs" / run, False),
                                  capture_output=True, text=True, stdin=subprocess.DEVNULL)
        runner = scratch / "runner.json"
        if not runner.is_file():
            fail(f"{run}: run_fusion1.py stopped before FuSION 1 ran (exit {launched.returncode}): "
                 f"{(launched.stderr or launched.stdout).strip()}")
        record = json.loads(runner.read_text(encoding="utf-8"))
        binary_sha = binary_sha or record["binary_sha256"]
        nonzero += record["exit"] != 0
        console = (scratch / "console.log").read_text(encoding="utf-8", errors="replace")
        written = {kind: int(count) for count, kind in WRITTEN.findall(console)}
        conflicts = console.count(TYPE_CONFLICT)
        xml = scratch / XML_NAME
        facts = check_xml(xml) if xml.is_file() else None
        if facts is None:
            status = "no_xml"
        elif (written.get("nodes"), written.get("edges")) != (facts["entries"], facts["relations"]) \
                or facts["entries"] != facts["distinct_entries"]:
            status = "count_mismatch"
        elif facts["multi"]:
            status = "violation"
        else:
            status = "ok"
        counts[status] += 1
        f = facts or {}
        multi = f.get("multi", {})
        with open(results, "a", encoding="utf-8") as fh:
            fh.write(tsv_line([run, number, order_name, len(order), record["exit"], record["wall_s"], status,
                               f.get("entries", ""), f.get("groups", ""), f.get("relations", ""), f.get("ids", ""),
                               len(multi) if facts else "", conflicts, f"scratch/{name}"]))
        if multi:
            bad_ids.update(multi)
            with open(violations, "a", encoding="utf-8") as fh:
                for i, held in multi.items():
                    detail = ";".join(f"{e}:{t}:{size}:{'isolated' if alone else 'connected'}"
                                      for e, t, size, alone in held)
                    fh.write(tsv_line([run, number, order_name, i, len(held), detail]))
        print(f"[{n:3d}/{len(runs)}] {run}: {len(order)} KGMLs, exit {record['exit']}, {record['wall_s']} s, "
              f"{status}" + (f", {len(multi)} ids in two or more nodes" if multi else ""), flush=True)

    checked = counts["ok"] + counts["violation"]
    summary = {
        "schema": 1,
        "experiment": "X03",
        "question": "does each id sit in exactly one gene or compound node in FuSION 1's merges "
                    "(spec stage 2, open question 4)",
        "script": "backend/tests/experiments/x03/run_x03.py",
        "batch": {"path": "backend/tests/experiments/x03/merge_and_write.batch", "sha256": sha256(BATCH)},
        "case": CASE,
        "population": {"file": population_shown, "sha256": sha256(population_path), "kgmls": N_KGML},
        "exceptions": {"file": exceptions_shown, "sha256": sha256(exceptions), "ids": n_exception_ids},
        "seed": SEED,
        "lists": N_LISTS,
        "kgmls_per_list": [MIN_SIZE, MAX_SIZE],
        "orders": ORDERS,
        "python": platform.python_version(),
        "started": started,
        "finished": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "load_average_at_start": [round(x, 2) for x in load],
        "fusion1_binary_sha256": binary_sha,
        "runs": len(runs),
        "status_counts": counts,
        "nonzero_exits": nonzero,
        "merges_checked": checked,
        "merges_with_an_id_in_two_or_more_nodes": counts["violation"],
        "distinct_ids_in_two_or_more_nodes": len(bad_ids),
        "scratch_folders": f"scratch/x03-{stamp}-<run>",
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if counts["violation"]:
        print(f"X03: in {counts['violation']} of {checked} merges checked, some id sits in two or more nodes "
              f"({len(bad_ids)} distinct ids); see {violations}")
    else:
        print(f"X03: every id sits in exactly one node in all {checked} merges checked")
    if checked != len(runs):
        print(f"X03: {len(runs) - checked} runs gave no checkable XML; see {results}")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
