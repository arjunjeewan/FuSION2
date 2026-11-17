#!/usr/bin/env python3
"""X08: does the Z3 seed reach the solver? Run fdr:0.3 with seed 42 and with seed 7, and compare.

Phase 3 plan (docs/phase3/plan.md), experiment X08; section 12 of the
architecture, "Phase 3 experiments"; spec stage 5, open question 6 (S5.22);
section 06, open question 7; decision 0037.

The case is jun-b10-fdr030 (738 s of solves). Its batch sets the seed in one
line, `let $Z3_RANDOM_SEED 42`, and passes $Z3_RANDOM_SEED to both pathz3
lines as their fifth optional token (S5.02). pathz3 prints
`z3_random_seed=<n>`, sets the solver parameter random_seed
(Linux/src/main.cc:4732) and writes the seed into its dumps (S5.22). The
script writes two copies of the batch, seed 42 (the batch unchanged) and
seed 7 (that one line changed), and runs them through run_fusion1.py --batch,
one after the other, about 13 min each. It then compares the frontiers (.dat),
the limits files and the witnesses (solutions and relaxations files) of the
two runs with each other and with the recorded run.

Usage:
  FUSION_TEST_DATA=<root> python3 -I x08.py [--dry-run | --compare-only RUN_ID]

Inputs: backend/tests/reference/cases.json; the case's batch, found as
run_fusion1.py finds it; in the data root, the recorded run under
reference/jun-b10-fdr030/ (f1/run_console.log, and both cases' .dat, limits,
solutions and relaxations files in f1/run/), and what run_fusion1.py needs.

Outputs, in the data root (they hold KEGG ids, so never in the repository, S8.31):
  scratch/x08-seed42-<RUN_ID>/, scratch/x08-seed7-<RUN_ID>/   the two runs
  reports/x08/<RUN_ID>/   x08-seed42.batch, x08-seed7.batch, report.json,
                          summary.txt, checks.tsv, and diffs/ for the files that differ
RUN_ID is the start time, YYYYmmdd-HHMMSS, and the script stops if any of
these folders exists. --compare-only RUN_ID compares those two runs again,
into reports/x08/<RUN_ID>-compared-<time>/. --dry-run builds both batches in a
temporary folder, checks them with run_fusion1.py --dry-run, lists the data
root's inputs, and self-tests the comparison on the recorded run (read from
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
from pathlib import Path

CASE_ID = "jun-b10-fdr030"
SEEDS = (42, 7)
SEED_LINE = "let $Z3_RANDOM_SEED {}"
ARJUN = "/home/ks-149-u24/Desktop/arjun"
FUSION1 = Path(f"{ARJUN}/FuSION")
SNAPSHOT = Path(f"{ARJUN}/FuSION1-reference-2026-11-10")
TESTS = Path(__file__).resolve().parents[2]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"
FIXTURES = TESTS / "fixtures"
RUN_FUSION1 = TESTS / "experiments" / "capture" / "run_fusion1.py"
KINDS = ("dat", "limits", "solutions", "relaxations")
PAIRS = (("seed 42", "recorded"), ("seed 7", "seed 42"), ("seed 7", "recorded"))
INCREMENTAL_S = 3600  # the batch's $CONSTR_SOLVER_TIMEOUT1; section 06, open question 8

CHECK = re.compile(r"^Relax bounds(?: \((node|edge) call\))?: (-?\d+) (-?\d+)$")  # (e, n), S5.24
TIME = re.compile(r"^Time taken \(sec\): (.*)$", re.M)
STEER = re.compile(r"^[NE]_[hl]: ")
IDS = re.compile(r"^\d+\t", re.M)  # the node or edge id that starts a witness line (S5.27)


def fail(message):
    sys.exit(f"x08: {message}")


def data_root():
    value = os.environ.get("FUSION_TEST_DATA")
    if not value:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(value).resolve()
    for tree in (FUSION1, SNAPSHOT, REPO):
        if root == tree or tree in root.parents:
            fail(f"the data root must lie outside {tree}: {root}")
    return root


def load_case():
    for case in json.loads(CASES.read_text(encoding="utf-8"))["cases"]:
        if case["id"] == CASE_ID:
            return case
    fail(f"no case {CASE_ID} in {CASES}")


def batch_source(case):
    """The batch run_fusion1.py would run: the frozen copy (W6), else FuSION 1's own, read only."""
    frozen = FIXTURES / case["batch"]
    return frozen if frozen.exists() else FUSION1 / case["origin"]["batch"]


def seed_batches(text):
    """The batch for each seed. The copies differ only in the line that sets $Z3_RANDOM_SEED."""
    lines = text.split("\n")
    sets = [i for i, line in enumerate(lines) if line.strip() == SEED_LINE.format(42)]
    code = [line.split() for line in lines if line.split() and not line.split()[0].startswith(";;")]
    uses = [tokens for tokens in code if any("$Z3_RANDOM_SEED" in t for t in tokens)]
    pathz3 = [tokens for tokens in uses if tokens[0] in ("pathz3", "get_path_with_z3")]
    if len(sets) != 1:
        fail(f"expected one line '{SEED_LINE.format(42)}' in the batch, found {len(sets)}")
    if len(uses) != 3 or len(pathz3) != 2 or any(len(t) != 34 or t[-1] != "$Z3_RANDOM_SEED" for t in pathz3):
        fail("expected $Z3_RANDOM_SEED as the 34th and last token of both pathz3 lines, and nowhere else")
    batches = {}
    for seed in SEEDS:
        copy = list(lines)
        copy[sets[0]] = lines[sets[0]].replace(SEED_LINE.format(42), SEED_LINE.format(seed))
        batches[seed] = "\n".join(copy)
    assert batches[42] == text
    return batches


def solve_files(case, at):
    return {c: {k: at(e[k]) for k in KINDS if k in e} for c, e in case["fusion1"]["solve"].items()}


def recorded_side(case, root):
    base = root / "reference" / case["id"]
    return {"label": "recorded", "dir": base, "console": base / case["fusion1"]["console"],
            "files": solve_files(case, lambda rel: base / rel)}


def fusion1_tree_side(case):
    """The recorded run's files in FuSION 1's own tree, read only: the dry run's self-test before W1."""
    base = FUSION1 / case["origin"]["outputs"]
    at = lambda rel: base / rel.removeprefix("f1/")  # noqa: E731
    return {"label": "recorded", "dir": base, "console": at(case["fusion1"]["console"]),
            "files": solve_files(case, at)}


def scratch_side(case, root, seed, name):
    base = root / "scratch" / name
    return {"label": f"seed {seed}", "dir": base, "console": base / "console.log",
            "files": solve_files(case, lambda rel: base / "run" / Path(rel).name)}


def prerequisites(case, root):
    """What a real run needs in the data root, beyond what run_fusion1.py checks itself."""
    recorded = recorded_side(case, root)
    need = [root / "fusion1" / "Linux" / "fusion", root / "fusion1" / "Linux" / "supporting_files",
            root / "kegg" / "hsa_pathways_with_exp_score", root / "kegg" / "updated_pathways",
            root / "reference" / case["id"] / "inputs", FUSION1 / ".venv" / "bin", recorded["console"]]
    return need + [p for files in recorded["files"].values() for p in files.values()]


def read(path):
    try:
        return path.read_text(errors="replace") if path else None
    except FileNotFoundError:
        return None


def frontier(text):
    """PO.dat's points, one `n<TAB>e` per line (S5.25)."""
    return None if text is None else [[int(v) for v in line.split()] for line in text.splitlines() if line.strip()]


def checks(text):
    """One record per solver check in a limits file (S5.24): step, point, answer, CPU time, steering line."""
    found, current = [], None
    for line in text.splitlines():
        m = CHECK.match(line)
        if m:
            current = {"step": m.group(1) or "diagonal", "n": int(m.group(3)), "e": int(m.group(2)),
                       "answer": None, "cpu_s": None, "steer": ""}
            found.append(current)
        elif current is None:
            continue
        elif current["cpu_s"] is None and line.startswith("Time taken (sec): "):
            current["cpu_s"] = float(line.split(": ", 1)[1])
        elif current["answer"] is None and line in ("sat", "unsat", "unknown"):
            current["answer"] = line
        elif current["answer"] is not None:
            if STEER.match(line):
                current["steer"] = line
            if not line.startswith(("TIMEOUT", "UNDEFINED")):
                current = None
    return found


def conflicts(a, b):
    """Points, as (n, e), where one run answered sat and the other unsat."""
    def definite(found):
        got = {}
        for c in found:
            if c["answer"] in ("sat", "unsat"):
                got.setdefault((c["n"], c["e"]), set()).add(c["answer"])
        return got
    da, db = definite(a), definite(b)
    return sorted([n, e] for (n, e) in da.keys() & db.keys() if da[(n, e)] != db[(n, e)])


def witness_points(text):
    """A solutions or relaxations file cut at each `Current PO point` line (S5.27), ids masked."""
    points, key = {}, None
    for line in IDS.sub("#\t", text).splitlines(keepends=True):
        if line.startswith("Current PO point: "):
            key = line.strip()[len("Current PO point: "):]
            points[key] = ""
        if key is not None:
            points[key] += line
    return points


def write_diff(path, a, b, a_name, b_name):
    path.parent.mkdir(exist_ok=True)
    path.write_text("".join(difflib.unified_diff(a.splitlines(keepends=True), b.splitlines(keepends=True),
                                                 a_name, b_name)))


def compare_case(a, b, case, diffs, tag):
    fa, fb = a["files"][case], b["files"][case]
    result = {}
    pa, pb = frontier(read(fa.get("dat"))), frontier(read(fb.get("dat")))
    result["frontier"] = {a["label"]: pa, b["label"]: pb, "same": pa is not None and pa == pb}
    la, lb = read(fa.get("limits")), read(fb.get("limits"))
    if la is None or lb is None:
        result["limits"] = {"missing": True}
    else:
        ca, cb = checks(la), checks(lb)
        sa = [(c["step"], c["n"], c["e"], c["answer"], c["steer"]) for c in ca]
        sb = [(c["step"], c["n"], c["e"], c["answer"], c["steer"]) for c in cb]
        first = next((i + 1 for i, (x, y) in enumerate(zip(sa, sb)) if x != y),
                     None if len(sa) == len(sb) else min(len(sa), len(sb)) + 1)
        ma, mb = TIME.sub(r"Time taken (sec): *", la), TIME.sub(r"Time taken (sec): *", lb)
        result["limits"] = {"same_but_times": ma == mb, "checks": [len(ca), len(cb)],
                            "first_different_check": first, "conflicting_answers": conflicts(ca, cb),
                            "cpu_s": [round(sum(c["cpu_s"] or 0.0 for c in found), 3) for found in (ca, cb)]}
        if ma != mb:
            write_diff(diffs / f"{tag}.{case}.limits.diff", ma, mb, a["label"], b["label"])
    for kind in ("solutions", "relaxations"):
        wa, wb = read(fa.get(kind)), read(fb.get(kind))
        if wa is None or wb is None:
            result[kind] = {"result": "absent from both" if wa is None and wb is None else "missing from one"}
        elif wa == wb:
            result[kind] = {"result": "identical"}
        else:
            pa, pb = witness_points(wa), witness_points(wb)
            differ = sorted(k for k in pa.keys() | pb.keys() if pa.get(k) != pb.get(k))
            result[kind] = {"result": "differ" if differ else "same apart from node and edge ids", "points": differ}
            write_diff(diffs / f"{tag}.{case}.{kind}.diff", wa, wb, a["label"], b["label"])
    return result


def same(cases):
    return all(r["frontier"]["same"] and r["limits"].get("same_but_times")
               and all(r[k]["result"] in ("identical", "same apart from node and edge ids", "absent from both")
                       for k in ("solutions", "relaxations"))
               for r in cases.values())


def reading(report):
    sides, pairs = report["sides"], report["pairs"]
    for label, seed in (("seed 42", "42"), ("seed 7", "7")):
        if sides[label]["console_seeds"] != [seed, seed]:
            return f"not valid: the {label} run did not print z3_random_seed={seed} for both solves"
    clash = [(pair, case) for pair, cases in pairs.items() for case, r in cases.items()
             if r["limits"].get("conflicting_answers")]
    if clash:
        return f"CHECK: sat against unsat at one point, in {clash}; a seed must not change a definite answer"
    if not same(pairs["seed 42 vs recorded"]):
        return ("inconclusive: the seed 42 rerun does not reproduce the recorded run, so a difference between "
                "the seeds need not come from the seed")
    if same(pairs["seed 7 vs seed 42"]):
        return ("no effect seen in the models: seed 7 gave the same checks, models' counts, frontiers and witnesses "
                "as seed 42, which reproduces the recorded run; an unsat check has no model, so compare the seeds' "
                "check times (checks.tsv) with the spread between the seed 42 rerun and the recorded run")
    return ("the seed reaches the solver: the seed 42 rerun reproduces the recorded run, and seed 7 changes "
            "what the pairs above show")


def describe(r):
    f, lim = r["frontier"], r["limits"]
    points = [v for k, v in f.items() if k != "same"]
    if None in points:
        parts = ["frontier (.dat) missing"]
    else:
        parts = [f"frontier {'same ' + str(points[0]) if f['same'] else 'DIFFERS: ' + ' against '.join(map(str, points))}"]
    if lim.get("missing"):
        parts.append("limits file missing")
    elif lim["same_but_times"]:
        parts.append(f"limits same apart from times (CPU {lim['cpu_s'][0]} s and {lim['cpu_s'][1]} s)")
    else:
        parts.append(f"limits DIFFER ({lim['checks'][0]} and {lim['checks'][1]} checks, CPU {lim['cpu_s'][0]} s and "
                     f"{lim['cpu_s'][1]} s, first difference at check {lim['first_different_check']}; "
                     f"sat against unsat at (n, e) {lim['conflicting_answers'] or 'none'})")
    for kind in ("solutions", "relaxations"):
        w = r[kind]
        parts.append(f"{kind} {w['result']}" + (f" at {'; '.join(w['points'])}" if w.get("points") else ""))
    return "; ".join(parts)


def summary(report):
    lines = [f"X08: does the Z3 seed reach the solver? Case {report['case']}, run {report['run_id']}."]
    for label, info in report["sides"].items():
        run = f", exit {info['exit']}, {info['wall_s']} s" if "exit" in info else ""
        lines.append(f"  {label}: {info['dir']}{run}; console z3_random_seed= "
                     f"{' '.join(info['console_seeds']) or 'none'}; longest check {info['longest_check_s']} s")
    for pair, cases in report["pairs"].items():
        lines.append(pair)
        lines += [f"  {case}: {describe(r)}" for case, r in cases.items()]
    longest = [i["longest_check_s"] for i in report["sides"].values() if i["longest_check_s"] is not None]
    if longest and max(longest) < INCREMENTAL_S:
        lines.append(f"Every check took under {INCREMENTAL_S} s of CPU time, so none left the combined solver's "
                     "incremental phase (timeout1; section 06, open question 8): X08 tests that path only.")
    lines.append(f"reading: {report['reading']}")
    return "\n".join(lines) + "\n"


def compare(case, sides, out, run_id):
    """Write report.json, summary.txt, checks.tsv and diffs/ into the folder out, and return the summary."""
    by_label = {s["label"]: s for s in sides}
    report = {"experiment": "X08", "case": case["id"], "run_id": run_id, "sides": {}, "pairs": {}}
    rows = ["case\tside\tcheck\tstep\tn\te\tanswer\tcpu_s\tsteering"]
    for s in sides:
        info = {"dir": str(s["dir"]),
                "console_seeds": re.findall(r"z3_random_seed=(-?\d+)", read(s["console"]) or "")}
        runner = json.loads(read(s["dir"] / "runner.json") or "{}")
        info.update({k: runner[k] for k in ("exit", "wall_s", "started") if k in runner})
        times = []
        for c, files in s["files"].items():
            for i, ch in enumerate(checks(read(files.get("limits")) or ""), 1):
                rows.append(f"{c}\t{s['label']}\t{i}\t{ch['step']}\t{ch['n']}\t{ch['e']}\t{ch['answer']}\t"
                            f"{ch['cpu_s']}\t{ch['steer']}")
                times.append(ch["cpu_s"] or 0.0)
        info["longest_check_s"] = max(times) if times else None
        report["sides"][s["label"]] = info
    for a, b in PAIRS:
        tag = f"{a}-vs-{b}".replace(" ", "")
        report["pairs"][f"{a} vs {b}"] = {c: compare_case(by_label[a], by_label[b], c, out / "diffs", tag)
                                          for c in case["fusion1"]["solve"]}
    report["reading"] = reading(report)
    text = summary(report)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (out / "summary.txt").write_text(text)
    (out / "checks.tsv").write_text("\n".join(rows) + "\n")
    return text


def run_fusion1(name, batch, dry_run=False):
    command = [sys.executable, "-I", str(RUN_FUSION1), "--case", CASE_ID, "--name", name, "--batch", str(batch)]
    if dry_run:
        return subprocess.run(command + ["--dry-run"], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    print(f"x08: {' '.join(command[2:])}", flush=True)
    return subprocess.run(command, stdin=subprocess.DEVNULL)


def dry_run(case, root, source, batches):
    print(f"x08 dry run: case {CASE_ID}, batch {source}")
    with tempfile.TemporaryDirectory(prefix="x08-dry-run-") as tmp:
        tmp = Path(tmp)
        for seed in SEEDS:
            batch = tmp / f"x08-seed{seed}.batch"
            batch.write_text(batches[seed])
            done = run_fusion1(f"x08-seed{seed}-dry-run", batch, dry_run=True)
            if done.returncode != 0:
                fail(f"run_fusion1.py --dry-run failed for seed {seed}:\n{done.stdout}{done.stderr}")
            lets = [line.strip() for line in done.stdout.splitlines() if line.strip().startswith("let $Z3_RANDOM_SEED")]
            if lets != [SEED_LINE.format(seed)]:
                fail(f"the mapped seed {seed} batch sets {lets}")
            print(f"  seed {seed}: run_fusion1.py --dry-run ok; the mapped batch has '{lets[0]}'")
        changed = [d for d in difflib.unified_diff(batches[42].splitlines(), batches[7].splitlines(), lineterm="", n=0)
                   if d[:1] in "+-" and d[:3] not in ("+++", "---")]
        print(f"  the seed 7 batch differs from the case's batch only in: {changed}")
        print("  data-root inputs a real run needs:")
        for p in prerequisites(case, root):
            print(f"    {'ok     ' if p.exists() else 'MISSING'} {p}")
        recorded = recorded_side(case, root) if (root / "reference" / case["id"] / "f1").is_dir() \
            else fusion1_tree_side(case)
        flipped = tmp / "limits_flipped.txt"
        flipped.write_text((read(recorded["files"]["in"]["limits"]) or "").replace("\nunsat\n", "\nsat\n", 1))
        swapped = {"in": dict(recorded["files"]["out"], limits=flipped), "out": recorded["files"]["in"]}
        stand_ins = [recorded, dict(recorded, label="seed 42"), dict(recorded, label="seed 7", files=swapped)]
        (tmp / "report").mkdir()
        text = compare(case, stand_ins, tmp / "report", "self-test")
        written = sorted(str(p.relative_to(tmp / "report")) for p in (tmp / "report").rglob("*") if p.is_file())
        print(f"  self-test of the comparison on the recorded run in {recorded['dir']}: 'seed 42' is the "
              "recorded run itself; 'seed 7' is the recorded run with its in and out cases swapped and its "
              f"in case's first unsat answer turned to sat. It wrote {', '.join(written)}. Its summary:")
        print("".join(f"    | {line}\n" for line in text.splitlines()), end="")
    print("x08 dry run: done; nothing was written outside the temporary folder, which is removed")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--compare-only", metavar="RUN_ID")
    args = parser.parse_args()
    root = data_root()
    case = load_case()
    source = batch_source(case)
    batches = seed_batches(source.read_text(errors="strict"))
    if args.dry_run:
        dry_run(case, root, source, batches)
        return
    now = time.strftime("%Y%m%d-%H%M%S")
    run_id = args.compare_only or now
    if not re.fullmatch(r"\d{8}-\d{6}", run_id):
        fail("RUN_ID has the form YYYYmmdd-HHMMSS")
    names = {seed: f"x08-seed{seed}-{run_id}" for seed in SEEDS}
    out = root / "reports" / "x08" / (f"{run_id}-compared-{now}" if args.compare_only else run_id)
    if out.exists():
        fail(f"{out} exists; every run writes to a fresh folder")
    if args.compare_only:
        if not any((root / "scratch" / n).is_dir() for n in names.values()):
            fail(f"no run {' or '.join(names.values())} in {root / 'scratch'}")
    else:
        taken = [root / "scratch" / n for n in names.values() if (root / "scratch" / n).exists()]
        missing = [str(p) for p in prerequisites(case, root) if not p.exists()]
        if taken:
            fail(f"{taken[0]} exists; every run gets a fresh scratch folder")
        if missing:
            fail("missing from the data root: " + ", ".join(missing))
    out.mkdir(parents=True)
    if not args.compare_only:
        for seed in SEEDS:
            (out / f"x08-seed{seed}.batch").write_text(batches[seed])
        for seed in SEEDS:
            code = run_fusion1(names[seed], out / f"x08-seed{seed}.batch").returncode
            if code != 0:
                print(f"x08: the seed {seed} run exited {code}; stopping before the next one", file=sys.stderr)
                break
    sides = [recorded_side(case, root)] + [scratch_side(case, root, seed, names[seed]) for seed in SEEDS]
    print(compare(case, sides, out, run_id), end="")
    print(f"x08: wrote {out}")


if __name__ == "__main__":
    main()
