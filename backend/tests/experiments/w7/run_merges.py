#!/usr/bin/env python3
"""Run FuSION 1's merge on W7's synthetic merge sets, one run at a time, and record each output.

Phase 3 plan (docs/phase3/plan.md), item W7; section 12 of the architecture, "Merge then split":
"Phase 3 also runs FuSION 1's own merge on the synthetic sets with three exception sets each: none, a
DE-like set, and a bridging set."

Usage:
  FUSION_TEST_DATA=/path/to/data-root python3 -I run_merges.py [--dry-run] [--set merge-NN]...
      [--max-load L]

Inputs: backend/tests/reference/synthetic/merge-*/, as test_merge's hidden test "[.w7-write]" writes
them from the merge suite's own sets (backend/tests/unit/merge/bounded_sets.hpp). Each folder's set.json
lists its three runs, one per exception set (none, de, bridging), each with the batch FuSION 1 runs:
start with the run's exception file, mff over inputs/list.txt at the set's threshold, wgx, and exit.

Each run goes through ../capture/run_fusion1.py --inputs, which copies the folder's inputs/ into a fresh
scratch folder, $FUSION_TEST_DATA/scratch/w7-<stamp>-merge-NN-<run>/, and runs the snapshot binary there
(the batch names its files relative to that folder). Runs go one at a time under nice 19. Before the
first, the script prints uptime and the running fusion and z3 processes; before each, it stops if the
one-minute load average is above --max-load (32, of 36 cores).

Outputs, per run, in the set's folder:
  f1/<run>/merged.xml    the wgx file as FuSION 1 wrote it; absent when FuSION 1 wrote none;
  f1/<run>/summary.json  the batch, exit status, wall time, binary SHA-256, the scratch folder, the load
                         before the run, the console lines that say what FuSION 1 did (reads, merges,
                         counts, warnings, errors), and merged.xml's size and SHA-256.
A run whose f1/<run>/ exists is skipped: results are never overwritten. --dry-run lists the runs and
writes nothing. The exit status is 1 if a run left no merged.xml.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
TESTS = HERE.parents[1]
SYNTHETIC = TESTS / "reference" / "synthetic"
RUN_FUSION1 = HERE.parent / "capture" / "run_fusion1.py"
# Console lines that only echo the batch or frame FuSION 1's own banner (S1.10).
BOILERPLATE = ("Entering batch mode", "No configuration provided.", "Log Manager Object is created",
               "Number of Log entries made totally:0", "LogManager Object is deleted")
# Routine lines every run prints: start's exception file and map sizes (S1.15), the empty map file and the
# naming-map step's edge counts (S2.14), each read's first line and mff's timing (S2.16).
ROUTINE = re.compile(r"\d+|\S+ -1|\S+ has 0 entries|Read map with 0 entries|Starting to read files|"
                     r"Edges (before|after): \d+|Total time taken \(in sec\): .*")


def fail(message):
    sys.exit(f"run_merges: {message}")


def data_root():
    value = os.environ.get("FUSION_TEST_DATA")
    if not value:
        fail("set FUSION_TEST_DATA to the data root")
    return Path(value).resolve()


def load_report():
    uptime = subprocess.run(["uptime"], capture_output=True, text=True).stdout.strip()
    ps = subprocess.run(["ps", "-eo", "comm="], capture_output=True, text=True).stdout.split()
    return f"{uptime}; fusion processes {ps.count('fusion')}, z3 processes {ps.count('z3')}"


def console_facts(text, batch):
    """The console without the banner, the command echoes, their dashed rules and the routine lines."""
    commands = {line.strip() for line in batch}
    keep = []
    for line in text.splitlines():
        line = line.strip()
        if not line or set(line) == {"-"} or line in BOILERPLATE or line in commands or ROUTINE.fullmatch(line):
            continue
        keep.append(line)
    return keep


def plan(selected):
    runs = []
    folders = sorted(p for p in SYNTHETIC.glob("merge-*") if p.is_dir())
    unknown = set(selected) - {p.name for p in folders}
    if unknown:
        fail(f"no such set folders: {sorted(unknown)}")
    for folder in folders:
        if selected and folder.name not in selected:
            continue
        spec = json.loads((folder / "set.json").read_text(encoding="utf-8"))
        for run in spec["runs"]:
            runs.append((folder, spec, run))
    return runs


def run_one(root, stamp, folder, spec, run, max_load, tmp):
    name = f"w7-{stamp}-{folder.name}-{run['exception_set']}"
    out = folder / "f1" / run["exception_set"]
    load = os.getloadavg()
    if load[0] > max_load:
        fail(f"the one-minute load average is {load[0]:.1f}, above {max_load}; stopping before {name}")
    batch = tmp / f"{name}.batch"
    batch.write_text("\n".join(run["batch"]) + "\n", encoding="utf-8")
    env = dict(os.environ, FUSION_TEST_DATA=str(root))
    command = ["nice", "-n", "19", sys.executable, "-I", str(RUN_FUSION1), "--inputs", str(folder / "inputs"),
               "--batch", str(batch), "--name", name]
    subprocess.run(command, env=env)
    scratch = root / "scratch" / name
    if not (scratch / "runner.json").is_file():
        fail(f"{name}: run_fusion1.py stopped before FuSION 1 ran; see its message above")
    record = json.loads((scratch / "runner.json").read_text(encoding="utf-8"))
    console = (scratch / "console.log").read_text(encoding="utf-8", errors="replace")
    out.mkdir(parents=True)
    summary = {
        "set": spec["set"],
        "name": spec["name"],
        "exception_set": run["exception_set"],
        "exceptions": len(run["ids"]),
        "batch": run["batch"],
        "exit": record["exit"],
        "wall_s": record["wall_s"],
        "started": record["started"],
        "binary_sha256": record["binary_sha256"],
        "scratch": f"scratch/{name}",
        "load_before": [round(x, 2) for x in load],
        "console": console_facts(console, run["batch"]),
    }
    merged = scratch / "merged.xml"
    if merged.is_file():
        data = merged.read_bytes()
        with open(out / "merged.xml", "xb") as f:
            f.write(data)
        summary["merged_xml"] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    with open(out / "summary.json", "x", encoding="utf-8") as f:
        f.write(json.dumps(summary, indent=1) + "\n")
    return name, record["exit"], record["wall_s"], merged.is_file()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--set", action="append", default=[], metavar="merge-NN")
    parser.add_argument("--max-load", type=float, default=32.0)
    args = parser.parse_args()
    root = data_root()
    runs = plan(args.set)
    todo = [(f, s, r) for f, s, r in runs if not (f / "f1" / r["exception_set"]).exists()]
    print(f"W7: {len(runs)} runs in {len(runs) // 3} sets; {len(runs) - len(todo)} recorded already, "
          f"{len(todo)} to run")
    print(f"load: {load_report()}")
    if args.dry_run:
        for folder, spec, run in todo:
            print(f"  {folder.name} {run['exception_set']}: {len(run['ids'])} exception ids; "
                  f"{' / '.join(run['batch'])}")
        return
    stamp = time.strftime("%Y%m%d-%H%M%S")
    started = time.time()
    missing = []
    with tempfile.TemporaryDirectory(prefix="w7-") as tmp:
        for folder, spec, run in todo:
            name, status, wall, wrote = run_one(root, stamp, folder, spec, run, args.max_load, Path(tmp))
            if not wrote:
                missing.append(name)
            print(f"  {name}: exit {status}, {wall:.2f} s{'' if wrote else ', no merged.xml'}", flush=True)
    print(f"W7: {len(todo)} runs in {time.time() - started:.1f} s; {len(missing)} without merged.xml"
          + (f": {', '.join(missing)}" if missing else ""))
    print(f"load after: {load_report()}")
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
