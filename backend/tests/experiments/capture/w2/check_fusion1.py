#!/usr/bin/env python3
"""Check FuSION 1's sources and binary, and rerun the five fastest recorded cases.

Phase 3 plan (docs/phase3/plan.md), item W2; section 12 of the architecture,
"FuSION 1 as the reference"; decision 0037.

Usage:
  FUSION_TEST_DATA=/path/to/data-root python3 -I check_fusion1.py [--dry-run]

Inputs, read only:
  - the snapshot's MANIFEST.sha256, and FuSION 1's live tree;
  - the data root's fusion1/Linux/fusion, and reference/<id>/f1/run/ of the
    five cases (W1 copies both);
  - backend/tests/reference/cases.json.

What it does:
  1. checks that every file in the snapshot's MANIFEST.sha256 has the same
     SHA-256 in the live tree, except README.txt, which exists only in the
     snapshot. Two known differences are allowed (prompt log entry 282), and
     listed with their reason in the output and the report: both copies of
     gsea_select_pathways.py (KNOWN_DIFFERENCES). If any other file differs,
     it stops and names the files;
  2. checks that the data root's fusion1/Linux/fusion has section 12's SHA-256;
  3. reruns the five recorded cases with the smallest sum of fusion1_seconds in
     cases.json, each with its own batch, one at a time, through
     ../run_fusion1.py, in the data root's scratch/w2-<stamp>-<id>/;
  4. compares each rerun's .dat, solutions and relaxations files byte for byte
     with the recorded ones in reference/<id>/f1/run/. It finds the rerun's
     files in the batch's own $WRITE_DIR, which it works out from the batch's
     let lines the way FuSION 1 expands them (spec S1.05 to S1.08);
  5. writes the data root's reports/w2/<stamp>/report.txt: one line per case
     and file, match or differ.
Section 12 expects every file to match, as the fdr025 rerun did (S5.22).

Outputs, all in the data root: the five scratch folders and the report. It
exits 1 if any file differs or any run fails. Every run gets a new <stamp>; the
script stops if the report folder or a scratch folder already exists.

--dry-run does steps 1 and 2 (step 2 once W1 has copied the binary), maps
each case's batch with run_fusion1.py --dry-run and works out its $WRITE_DIR,
and lists the files it would compare. Until W1 has filled the data root, it
looks for the recorded files where W1 copies them from, in FuSION 1's tree. It
reports every problem that would stop the run, exits 1 if there is one, starts
no FuSION 1 run and writes nothing.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ARJUN = Path("/home/ks-149-u24/Desktop/arjun")
FUSION1 = ARJUN / "FuSION"
SNAPSHOT = ARJUN / "FuSION1-reference-2026-11-10"
HERE = Path(__file__).resolve().parent
TESTS = HERE.parents[2]
REPO = TESTS.parents[1]
CASES = TESTS / "reference" / "cases.json"
RUN_FUSION1 = HERE.parent / "run_fusion1.py"
BINARY_SHA256 = "22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50"  # section 12
FIVE = ["jun-b6-fdr025", "jun-b7-fdr025", "jun-b6-fdr040keep", "jun-b6-old", "jun-b7-fdr040keep"]
KINDS = ("dat", "solutions", "relaxations")
RERUN_GLOBS = ("*.dat", "temp_*_solutions.txt", "temp_*_relaxations.txt")
MAPPED_MARK = "----- mapped batch -----\n"  # run_fusion1.py --dry-run prints the batch after it
# Step 1's known differences, allowed by the user in prompt log entry 282.
KNOWN_DIFFERENCES = ("Linux/supporting_files/gsea_select_pathways.py",
                     "macOS/supporting_files/gsea_select_pathways.py")
KNOWN_REASON = ("changed in the live tree on 2026-11-16 at 17:52 (a filter for generated KGMLs whose "
                "entries carry member=\"yes\"); the data root takes the snapshot's copy of the script, "
                "so the reruns never use the changed file")

problems = []


def fail(message):
    sys.exit(f"check_fusion1: {message}")


def stop(message, dry):
    """Stop the run; a dry run notes the problem and carries on checking."""
    if not dry:
        fail(message)
    print(f"  WOULD STOP: {message}")
    problems.append(message)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


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


def let_variables(batch):
    """The batch's let variables as FuSION 1 defines them (S1.05, S1.06, S1.08).

    Lines split on spaces only; ;; starts a comment; key and value are expanded;
    a key that is defined already, or does not start with $, is ignored. A value of
    #LASTGRAPHNUM (S1.09) is kept as written: no graph exists here.
    """
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


def write_dir_of(batch, scratch):
    value = let_variables(batch).get("$WRITE_DIR")
    if not value:
        return None, "the batch defines no $WRITE_DIR"
    path = Path(os.path.normpath(value))
    if not path.is_absolute() or not path.is_relative_to(scratch):
        return None, f"its $WRITE_DIR, {value}, is not inside the scratch folder {scratch}"
    return path, None


def recorded_names(case):
    """The case's .dat, solutions and relaxations files, from cases.json, in f1/run/."""
    names = []
    for entry in case["fusion1"]["solve"].values():
        for kind in KINDS:
            if kind in entry:
                path = Path(entry[kind])
                if path.parent != Path("f1/run"):
                    fail(f"{case['id']}: {path} is not in f1/run/")
                names.append(path.name)
    return names


def compare(case, recorded_dir, write_dir):
    """One (file, result, detail) per recorded file and per extra file of the rerun."""
    names = recorded_names(case)
    extra = sorted({p.name for g in RERUN_GLOBS for p in write_dir.glob(g)} - set(names))
    rows = []
    for name in names + extra:
        old, new = recorded_dir / name, write_dir / name
        if not new.is_file():
            rows.append((name, "differ", "missing from the rerun"))
        elif not old.is_file():
            rows.append((name, "differ", "the record has no such file"))
        else:
            a, b = old.read_bytes(), new.read_bytes()
            if a == b:
                rows.append((name, "match", f"{len(a)} bytes"))
            else:
                rows.append((name, "differ", f"bytes differ (record {len(a)} bytes, rerun {len(b)})"))
    return rows


def check_sources(dry):
    print("step 1: the live tree against the snapshot's MANIFEST.sha256")
    checked, bad, known = 0, [], []
    for line in (SNAPSHOT / "MANIFEST.sha256").read_text(encoding="utf-8").splitlines():
        m = re.fullmatch(r"([0-9a-f]{64}) [ *]\./(.+)", line)
        if not m:
            fail(f"unexpected line in {SNAPSHOT}/MANIFEST.sha256: {line!r}")
        want, rel = m.groups()
        if rel == "README.txt":  # exists only in the snapshot
            continue
        checked += 1
        live = FUSION1 / rel
        if not live.is_file():
            bad.append(f"{rel} (missing)")
        elif sha256(live) != want:
            (known if rel in KNOWN_DIFFERENCES else bad).append(rel)
    print(f"  {checked} files checked, {len(known)} known differences, {len(bad)} other differences")
    for rel in known:
        print(f"  known difference: {rel}: {KNOWN_REASON}")
    if bad:
        stop(f"{len(bad)} live files differ from the snapshot's manifest: " + ", ".join(bad), dry)
    return checked, known


def check_binary(root, dry):
    binary = root / "fusion1" / "Linux" / "fusion"
    print(f"step 2: {binary}")
    if not binary.is_file():
        if dry:
            print("  not checked: it does not exist yet (W1 copies it)")
            return None
        fail(f"{binary} is missing: fill the data root first (W1)")
    got = sha256(binary)
    if got != BINARY_SHA256:
        stop(f"{binary} has SHA-256 {got}, not section 12's {BINARY_SHA256}", dry)
    else:
        print(f"  SHA-256 {got}, as section 12 says")
    return got


def fastest_five(dry):
    cases = json.loads(CASES.read_text(encoding="utf-8"))["cases"]
    recorded = sorted((c for c in cases if c["source"].startswith("recorded")),
                      key=lambda c: sum(c["fusion1_seconds"].values()))[:5]
    print("step 3: the five fastest recorded cases, by the sum of fusion1_seconds")
    for c in recorded:
        print(f"  {c['id']}: {sum(c['fusion1_seconds'].values()):.3f} s")
    if [c["id"] for c in recorded] != FIVE:
        stop(f"cases.json's five fastest are not the plan's {', '.join(FIVE)}", dry)
    return recorded


def check_recorded(case, root, dry):
    """Every recorded file must be in the data root before anything runs."""
    names = recorded_names(case)
    recorded_dir = root / "reference" / case["id"] / "f1" / "run"
    if recorded_dir.is_dir() or not dry:
        missing = [n for n in names if not (recorded_dir / n).is_file()]
        where = recorded_dir
    else:  # dry run before W1: check the files W1 will copy
        origin = FUSION1 / case["origin"]["outputs"] / "run"
        missing = [n for n in names if not (origin / n).is_file()]
        where = f"{origin} (the data root has no {recorded_dir} yet)"
    print(f"    recorded files: {len(names) - len(missing)} of {len(names)} found in {where}")
    if missing:
        stop(f"{case['id']}: missing recorded files: {', '.join(missing)}", dry)
    return recorded_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    dry = parser.parse_args().dry_run

    root = os.environ.get("FUSION_TEST_DATA")
    if not root:
        fail("set FUSION_TEST_DATA to the data root")
    root = Path(root).resolve()
    for tree in (FUSION1, SNAPSHOT, REPO):
        if root.is_relative_to(tree):
            fail(f"the data root must lie outside {tree}: {root}")
    stamp = time.strftime("%Y%m%dT%H%M%S")
    report_dir = root / "reports" / "w2" / stamp
    print(f"W2{' dry run' if dry else ''}, {stamp}; data root {root}")

    checked, known = check_sources(dry)
    binary_sha = check_binary(root, dry)
    five = fastest_five(dry)

    plan = []
    for case in five:
        name = f"w2-{stamp}-{case['id']}"
        scratch = root / "scratch" / name
        print(f"  {case['id']}: scratch folder {scratch}")
        if scratch.exists():
            stop(f"{scratch} exists", dry)
        plan.append((case, name, scratch, check_recorded(case, root, dry)))
        if dry:
            out = subprocess.run([sys.executable, "-I", str(RUN_FUSION1), "--case", case["id"],
                                  "--name", name, "--dry-run"], capture_output=True, text=True)
            if out.returncode != 0 or MAPPED_MARK not in out.stdout:
                stop(f"run_fusion1.py --dry-run failed for {case['id']}: {out.stderr.strip()}", dry)
                continue
            write_dir, why = write_dir_of(out.stdout.split(MAPPED_MARK, 1)[1], scratch)
            if why:
                stop(f"{case['id']}: {why}", dry)
                continue
            print(f"    $WRITE_DIR {write_dir}")
            print(f"    would compare: {', '.join(recorded_names(case))}, and any other "
                  f"{', '.join(RERUN_GLOBS)} the rerun writes there")
    if report_dir.exists():
        stop(f"{report_dir} exists", dry)

    if dry:
        print(f"step 5: would write {report_dir / 'report.txt'}")
        print(f"dry run: nothing started, nothing written; {len(problems)} problem(s) would stop the run")
        sys.exit(1 if problems else 0)

    report_dir.mkdir(parents=True)
    failed = 0
    with open(report_dir / "report.txt", "x", encoding="utf-8") as report:
        def put(line):
            report.write(line + "\n")
            report.flush()
            print(line)

        put(f"# W2, {stamp}: FuSION 1's reruns against its recorded outputs (plan item W2)")
        put(f"# step 1: {checked - len(known)} of the {checked} files of {SNAPSHOT}/MANIFEST.sha256 "
            "match the live tree")
        for rel in known:
            put(f"# step 1, known difference (prompt log entry 282): {rel}: {KNOWN_REASON}")
        put(f"# step 2: {root}/fusion1/Linux/fusion has SHA-256 {binary_sha}")
        put("# case\tfile\tresult\tdetail")
        for case, name, scratch, recorded_dir in plan:
            print(f"step 3: running {case['id']} ...", flush=True)
            result = subprocess.run([sys.executable, "-I", str(RUN_FUSION1),
                                     "--case", case["id"], "--name", name])
            runner = scratch / "runner.json"
            if not runner.is_file():
                fail(f"{case['id']}: run_fusion1.py started no run (exit {result.returncode})")
            record = json.loads(runner.read_text(encoding="utf-8"))
            write_dir, why = write_dir_of((scratch / "case.batch").read_text(encoding="utf-8"), scratch)
            if why:
                fail(f"{case['id']}: {why}")
            put(f"# {case['id']}: exit {record['exit']}, {record['wall_s']} s, $WRITE_DIR {write_dir}")
            failed += record["exit"] != 0
            for file, verdict, detail in compare(case, recorded_dir, write_dir):
                put(f"{case['id']}\t{file}\t{verdict}\t{detail}")
                failed += verdict != "match"
        put(f"# {'all files match' if not failed else f'{failed} failure(s): see the lines above'}")
    print(f"wrote {report_dir / 'report.txt'}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
