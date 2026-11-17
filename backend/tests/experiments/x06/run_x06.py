#!/usr/bin/env python3
"""X06: what do Z3's :solver2_timeout and :timeout do?

Phase 3 plan (docs/phase3/plan.md), experiment X06; section 12 of the architecture
(docs/architecture/12-tests.md, "Phase 3 experiments"); spec stage 5, open question 1
(docs/spec/05-encoding-and-solve.md); section 06, 06.C2 and its open question 8
(docs/architecture/06-solve.md). NOTES.md beside this script reads Z3 4.8.12's combined
solver; these runs check that reading.

The dump is the full run's (jun-b10-old) without-FSC check at (n 7, e 0):
  reference/jun-b10-old/f1/dumps/egf_hela_t120_lfc_smt-files_20260717_201202/PO_constraints_nr_7_er_0.smt2
  (2,891,394 bytes; SHA-256 pinned below). fill_data_root.sh copies it from
  runs/egfr_cxcl8_jun_b10_nr8_er8/. FuSION 1's smallest-e search asked this point under its
  1 h incremental limit and 4 h cap; it ran 14,396.2 s and came back unknown with "reason:
  unknown" (f1/run/hsa1956_to_hsa3576_wo_hsa3725_egf_hela_t120_lfc.tsv_b10_limits_timefile.txt,
  lines 73-75), the case of the open question. Like every smallest-e dump it has no seed line
  (S5.22), so the runs use Z3's default seed; the seed plays no part in the timeouts.

What it does:
  1. builds two inputs from the dump, both starting with (set-logic QF_BV), the logic of
     FuSION 1's z3::solver(c, "QF_BV") (Linux/src/main.cc:4722):
       push.smt2  the base formula, then the two bounds pushed as FuSION 1's smallest-e step
                  pushes them (GraphManagerNew.cc:29687-29688, 29708-29709), then check-sat,
                  the reason, pop and the reason again;
       flat.smt2  the dump as written, bounds at the top level, then check-sat and the reason;
  2. runs /usr/bin/z3, 4.8.12, the version FuSION 1 links, once per row of RUNS, one after
     another, with -v:15 -st, the soft timeout -t (Z3's "timeout") and
     combined_solver.solver2_timeout. -v:15, not -v:10: the combined solver writes its
     messages at level 15 (combined_solver.cpp:27), so -v:10 would show none of them;
  3. records what each run reports: every stdout and stderr line with its time since the
     start, the answer, both reasons, the combined solver's messages and the statistics.

Inputs: the dump above in the data root ($FUSION_TEST_DATA), and /usr/bin/z3.
Outputs, in the data root: reports/x06/<stamp>/ with push.smt2, flat.smt2, z3-version.txt,
  <run>/stdout.txt and <run>/stderr.txt (each line "seconds<TAB>text"), <run>/run.json,
  summary.tsv and summary.json.

Usage:
  FUSION_TEST_DATA=<root> python3 -I run_x06.py [--dry-run]

--dry-run checks the dump's size and SHA-256 and Z3's package version (with dpkg-query, so Z3
does not start), builds both inputs in a temporary folder, prints their ends and the commands,
and removes the folder. Before the data root is filled it reads FuSION 1's copy of the dump,
read only, and says so. The real run refuses an existing report folder. Machine time: about
3 min of one core (the four checks' timeouts add up to 90 s, plus parsing and bit-blasting).
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ARJUN = "/home/ks-149-u24/Desktop/arjun"
FUSION1 = f"{ARJUN}/FuSION"
SNAPSHOT = f"{ARJUN}/FuSION1-reference-2026-11-10"
HERE = Path(__file__).resolve().parent
TESTS = HERE.parents[1]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"

CASE = "jun-b10-old"
DUMP_DIR = "egf_hela_t120_lfc_smt-files_20260717_201202"  # the without-FSC solve's dumps
DUMP_NAME = "PO_constraints_nr_7_er_0.smt2"
DUMP_SIZE = 2_891_394
DUMP_SHA256 = "f865037afffdaaca5ac8e5ab3129ec43f18f852bef115e209474885708336e49"
BOUNDS = (7, 0)  # (n, e) in the dump's last two assertions
Z3 = "/usr/bin/z3"
Z3_VERSION = "4.8.12"
VERBOSITY = 15
HARD_CAP_S = 300  # safety nets only: Z3's -T: this far past the soft timeout, a kill as far again

RUNS = [  # name, input, combined_solver.solver2_timeout in ms (None: Z3's default, no limit), timeout in ms
    ("a", "push", 10_000, 40_000),  # FuSION 1's 1 h : 4 h, scaled down
    ("b", "push", 40_000, 10_000),  # the overall timeout comes first
    ("c", "push", None, 20_000),    # no incremental limit
    ("d", "flat", 10_000, 20_000),  # no push: the combined solver stays non-incremental
]

NODE_BOUND = re.compile(r"\(assert\n \(bvule __nodestatusrelaxbound \(_ bv(\d+) \d+\)\)\)\n")
EDGE_BOUND = re.compile(r"\(assert\n \(bvule __edgestatusrelaxbound \(_ bv(\d+) \d+\)\)\)\n")
REASON = re.compile(r'^\(:reason-unknown "(.*)"\)$')
COMBINED = re.compile(r'^\(combined-solver "(.*)"\)$')
STATISTIC = re.compile(r":(total-time|time|max-memory|memory)\s+([0-9.]+)")


def fail(message):
    sys.exit(f"run_x06: {message}")


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


def find_dump(root, dry):
    rel = f"reference/{CASE}/f1/dumps/{DUMP_DIR}/{DUMP_NAME}"
    path = root / rel
    if not path.is_file():
        if not dry:
            fail(f"{path} is missing: fill the data root first (W1)")
        case = next((c for c in json.loads(CASES.read_text(encoding="utf-8"))["cases"] if c["id"] == CASE), None)
        if case is None:
            fail(f"no case {CASE} in {CASES}")
        path = Path(FUSION1) / case["origin"]["outputs"] / DUMP_DIR / DUMP_NAME
        print(f"note: {rel} is not in the data root yet; the dry run reads FuSION 1's copy, read only: {path}")
        if not path.is_file():
            fail(f"{path} is missing")
    size = path.stat().st_size
    digest = sha256(path)
    if (size, digest) != (DUMP_SIZE, DUMP_SHA256):
        fail(f"{path} has {size} bytes and SHA-256 {digest}, not {DUMP_SIZE} and {DUMP_SHA256}")
    print(f"dump: {path}, {size} bytes, SHA-256 as pinned")
    return path, rel


def compose(text, source_name):
    """The push and flat inputs, from the dump's text."""
    nodes, edges = list(NODE_BOUND.finditer(text)), list(EDGE_BOUND.finditer(text))
    if len(nodes) != 1 or len(edges) != 1:
        fail(f"expected one node bound and one edge bound, found {len(nodes)} and {len(edges)}")
    node, edge = nodes[0], edges[0]
    if node.end() != edge.start() or text[edge.end():].strip() != "(check-sat)":
        fail("the dump does not end with the node bound, the edge bound and (check-sat)")
    if (int(node.group(1)), int(edge.group(1))) != BOUNDS:
        fail(f"the dump's bounds are n {node.group(1)}, e {edge.group(1)}, not n {BOUNDS[0]}, e {BOUNDS[1]}")
    head = "(set-logic QF_BV)\n"
    push = (f"; X06 (run_x06.py) push input from {source_name}: bounds pushed as FuSION 1's smallest-e step does\n"
            + head + text[:node.start()] + "(push 1)\n" + node.group(0) + "(push 1)\n" + edge.group(0)
            + "(check-sat)\n(get-info :reason-unknown)\n(pop 1)\n(get-info :reason-unknown)\n")
    flat = (f"; X06 (run_x06.py) flat input from {source_name}: the dump as written\n"
            + head + text[:edge.end()] + "(check-sat)\n(get-info :reason-unknown)\n")
    return {"push": push, "flat": flat}


def command(input_path, solver2_ms, timeout_ms):
    cmd = [Z3, "-smt2", f"-v:{VERBOSITY}", "-st", f"-t:{timeout_ms}", f"-T:{timeout_ms // 1000 + HARD_CAP_S}"]
    if solver2_ms is not None:
        cmd.append(f"combined_solver.solver2_timeout={solver2_ms}")
    return cmd + [str(input_path)]


def run_z3(cmd, deadline_s):
    """Run Z3, timing every line it writes on either stream."""
    lines = {"stdout": [], "stderr": []}
    started = time.monotonic()
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    killer = threading.Timer(deadline_s, proc.kill)
    killer.start()

    def pump(stream, name):
        for raw in stream:
            lines[name].append((time.monotonic() - started, raw.decode("utf-8", "replace").rstrip("\n")))

    reader = threading.Thread(target=pump, args=(proc.stderr, "stderr"))
    reader.start()
    pump(proc.stdout, "stdout")
    reader.join()
    code = proc.wait()
    killer.cancel()
    return code, time.monotonic() - started, lines


def facts(lines):
    out, err = lines["stdout"], lines["stderr"]
    answers = [(t, text.strip()) for t, text in out if text.strip() in ("sat", "unsat", "unknown")]
    reasons = [REASON.match(text.strip()).group(1) for _, text in out if REASON.match(text.strip())]
    combined = [(round(t, 3), COMBINED.match(text).group(1)) for t, text in err if COMBINED.match(text)]
    statistics = dict(STATISTIC.findall("\n".join(text for _, text in out)))
    check_start = combined[0][0] if combined else None
    fallback = next((t for t, m in combined if m.startswith("solver 2 failed")), None)
    answer_at = answers[0][0] if answers else None
    return {
        "answer": answers[0][1] if answers else None,
        "errors": [text for _, text in out + err if text.startswith("(error")],
        "reason_before_pop": reasons[0] if reasons else None,
        "reason_after_pop": reasons[1] if len(reasons) > 1 else None,
        "combined_solver": combined,
        "check_started_s": check_start,
        "fallback_after_s": round(fallback - check_start, 3) if fallback is not None else None,
        "answer_after_s": round(answer_at - check_start, 3) if None not in (answer_at, check_start) else None,
        "statistics": statistics,
    }


def write_lines(path, lines):
    path.write_text("".join(f"{t:.3f}\t{text}\n" for t, text in lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="X06: Z3's solver2_timeout and timeout")
    parser.add_argument("--dry-run", action="store_true", help="check and print; start no Z3")
    args = parser.parse_args()
    dry = args.dry_run

    root = data_root()
    dump, dump_rel = find_dump(root, dry)
    if not os.access(Z3, os.X_OK):
        fail(f"{Z3} is missing or not executable")
    inputs = compose(dump.read_text(encoding="utf-8"), DUMP_NAME)
    stamp = time.strftime("%Y%m%dT%H%M%S")
    out = root / "reports" / "x06" / stamp
    if out.exists():
        fail(f"{out} exists; every run writes to a fresh folder")

    if dry:
        package = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "z3"], capture_output=True, text=True)
        version = package.stdout.strip()
        if package.returncode != 0 or not version.startswith(Z3_VERSION + "-"):
            fail(f"the z3 package is {version or 'not installed'}, not {Z3_VERSION}")
        print(f"z3 package {version} (Z3 itself not started)")
        with tempfile.TemporaryDirectory(prefix="x06-dry-") as tmp:
            for kind, text in inputs.items():
                path = Path(tmp) / f"{kind}.smt2"
                path.write_text(text, encoding="utf-8")
                tail = text.splitlines()[-10 if kind == "push" else -6:]
                print(f"----- {kind}.smt2: {path.stat().st_size} bytes; its last lines -----")
                print("\n".join(tail))
        print("----- the runs, one after another -----")
        for name, kind, solver2_ms, timeout_ms in RUNS:
            print(f"{name}: " + " ".join(command(out / f"{kind}.smt2", solver2_ms, timeout_ms)))
        print(f"would write {out}/ (it does not exist)")
        print("dry run: nothing written outside the temporary folder, which is removed")
        return

    version = subprocess.run([Z3, "-version"], capture_output=True, text=True)
    if Z3_VERSION not in version.stdout:
        fail(f"{Z3} -version says {version.stdout.strip()!r}, not {Z3_VERSION}")
    load = os.getloadavg()
    out.mkdir(parents=True)
    (out / "z3-version.txt").write_text(version.stdout, encoding="utf-8")
    for kind, text in inputs.items():
        (out / f"{kind}.smt2").write_text(text, encoding="utf-8")

    rows = []
    for name, kind, solver2_ms, timeout_ms in RUNS:
        folder = out / name
        folder.mkdir()
        cmd = command(out / f"{kind}.smt2", solver2_ms, timeout_ms)
        print(f"{name}: {' '.join(cmd)}", flush=True)
        code, wall, lines = run_z3(cmd, timeout_ms / 1000 + 2 * HARD_CAP_S)
        write_lines(folder / "stdout.txt", lines["stdout"])
        write_lines(folder / "stderr.txt", lines["stderr"])
        row = {"run": name, "input": f"{kind}.smt2", "solver2_timeout_ms": solver2_ms, "timeout_ms": timeout_ms,
               "command": cmd, "exit": code, "killed": code == -9, "wall_s": round(wall, 3), **facts(lines)}
        (folder / "run.json").write_text(json.dumps(row, indent=2) + "\n", encoding="utf-8")
        rows.append(row)
        print(f"   exit {code}, {wall:.1f} s: {row['answer']}, reason before pop {row['reason_before_pop']!r}; "
              + "; ".join(f"{t:.1f} s {m}" for t, m in row["combined_solver"])
              + (f"; {len(row['errors'])} error lines" if row["errors"] else ""), flush=True)

    columns = ["run", "input", "solver2_timeout_ms", "timeout_ms", "exit", "wall_s", "answer", "reason_before_pop",
               "reason_after_pop", "check_started_s", "fallback_after_s", "answer_after_s", "combined_solver", "errors"]
    with open(out / "summary.tsv", "w", encoding="utf-8") as fh:
        fh.write("\t".join(columns) + "\n")
        for row in rows:
            cells = dict(row, combined_solver="; ".join(f"{t:.3f} {m}" for t, m in row["combined_solver"]),
                         errors=len(row["errors"]))
            fh.write("\t".join("" if cells[c] is None else str(cells[c]) for c in columns) + "\n")
    summary = {
        "schema": 1,
        "experiment": "X06",
        "question": "what do Z3's :solver2_timeout and :timeout do (spec stage 5, open question 1)",
        "script": "backend/tests/experiments/x06/run_x06.py",
        "notes": "backend/tests/experiments/x06/NOTES.md",
        "dump": {"file": dump_rel, "bytes": DUMP_SIZE, "sha256": DUMP_SHA256, "bounds": {"n": BOUNDS[0], "e": BOUNDS[1]}},
        "inputs": {f"{kind}.smt2": sha256(out / f"{kind}.smt2") for kind in inputs},
        "z3": {"path": Z3, "version": version.stdout.strip()},
        "load_average_at_start": [round(x, 2) for x in load],
        "runs": rows,
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
