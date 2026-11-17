#!/usr/bin/env python3
"""Run one FuSION 1 batch for a reference case, in a fresh scratch folder.

Phase 3 plan (docs/phase3/plan.md), shared by W2, W3, W4, X02 and X08;
decision 0037. Every Phase 3 run of FuSION 1 goes through this script, so
that each one applies its case's path map the same way and none can touch
FuSION 1's own tree.

Usage:
  python3 -I run_fusion1.py --case ID --name NAME [--batch FILE] [--dry-run]

  --case ID     a case of backend/tests/reference/cases.json
  --name NAME   the scratch folder, $FUSION_TEST_DATA/scratch/NAME; it must not exist
  --batch FILE  run this batch text instead of the case's own (for example a
                capture batch); the case's path map and inputs still apply
  --extra-inputs DIR
                also copy DIR's files into the scratch folder's inputs/, after the
                case's own, mapping their list files the same way; a name the case
                already has is an error
  --dry-run     print the mapped batch and the command, and write nothing

What a run does (section 12, FuSION 1 as the reference; Reference cases):
  1. creates the scratch folder, which is the case's $CASE;
  2. copies the case's inputs/ into it: reference/<id>/inputs/ from the data
     root, or backend/tests/reference/synthetic/<id>/inputs/ for a toy case;
  3. applies the path map, longest "from" first, to the batch and to every
     list file under inputs/; $CASE and $DATA become absolute paths;
  4. refuses to go on if any path into FuSION 1's tree or its snapshot is left;
  5. runs the data root's snapshot binary, fusion1/Linux/fusion -b case.batch,
     in the scratch folder, with FuSION 1's .venv/bin first on PATH (gseapy
     1.3.1, S7.02), no Python bytecode writes and matplotlib's cache in the
     scratch folder;
  6. writes console.log (stdout and stderr) and runner.json (exit status, wall
     time, binary SHA-256) beside the batch's own outputs.

It reads FuSION 1's tree only for the batch text when the frozen corpus copy
(W6) is not there yet, and runs nothing from it but the .venv's Python.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ARJUN = "/home/ks-149-u24/Desktop/arjun"
FUSION1 = f"{ARJUN}/FuSION"
SNAPSHOT = f"{ARJUN}/FuSION1-reference-2026-11-10"
VENV_BIN = f"{FUSION1}/.venv/bin"
TESTS = Path(__file__).resolve().parents[2]
CASES = TESTS / "reference" / "cases.json"
FIXTURES = TESTS / "fixtures"
SYNTHETIC = TESTS / "reference" / "synthetic"
BINARY_SHA256 = "22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50"  # section 12


def fail(message):
    sys.exit(f"run_fusion1: {message}")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def load_case(case_id):
    for case in json.loads(CASES.read_text(encoding="utf-8"))["cases"]:
        if case["id"] == case_id:
            return case
    fail(f"no case {case_id} in {CASES}")


def batch_source(case, override):
    if override:
        return Path(override)
    frozen = FIXTURES / case["batch"]
    if frozen.exists():
        return frozen
    if case["tier"] == "T1":
        fail(f"{case['id']}: {frozen} does not exist yet (W4 writes the toy batches)")
    return Path(FUSION1) / case["origin"]["batch"]  # read only, until W6 has frozen the corpus


def inputs_source(case, data_root):
    if case["tier"] == "T1":
        return SYNTHETIC / case["id"] / "inputs"
    return data_root / "reference" / case["id"] / "inputs"


def apply_map(text, path_map, case_dir, data_root):
    for entry in sorted(path_map, key=lambda e: len(e["from"]), reverse=True):
        target = entry["to"].replace("$CASE", str(case_dir)).replace("$DATA", str(data_root))
        text = text.replace(entry["from"], target)
    return text


def check_exit(text, where):
    """FuSION 1 reads past the end of a batch and loops on stdin (S1.04): the last command must be exit."""
    code = [line.strip(" \t") for line in text.split("\n")]
    code = [line for line in code if line and not line.startswith(";")]
    if not code or code[-1] != "exit":
        fail(f"{where} does not end with an exit line, so FuSION 1 would loop on stdin after it (S1.04)")


def check_clean(text, where):
    for tree in (FUSION1 + "/", FUSION1 + "\n", SNAPSHOT):
        if tree in text + "\n":
            fail(f"{where} still names {tree.strip()} after the path map")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--case", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--batch")
    parser.add_argument("--extra-inputs")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    data_root = os.environ.get("FUSION_TEST_DATA")
    if not data_root:
        fail("set FUSION_TEST_DATA to the data root")
    data_root = Path(data_root).resolve()
    if "/" in args.name or args.name in ("", ".", ".."):
        fail("--name must be a plain folder name")
    case = load_case(args.case)
    case_dir = data_root / "scratch" / args.name
    source = batch_source(case, args.batch)
    batch = apply_map(source.read_text(errors="strict"), case["path_map"], case_dir, data_root)
    check_clean(batch, f"the batch {source}")
    check_exit(batch, f"the batch {source}")
    binary = data_root / "fusion1" / "Linux" / "fusion"
    command = [str(binary), "-b", "case.batch"]

    if args.dry_run:
        print(f"case {case['id']} ({case['tier']}), batch {source}")
        print(f"scratch folder {case_dir} (would be created; exists now: {case_dir.exists()})")
        print(f"inputs from {inputs_source(case, data_root)}")
        print("command:", " ".join(command), "  (cwd: scratch folder)")
        print("----- mapped batch -----")
        print(batch, end="" if batch.endswith("\n") else "\n")
        return

    if not binary.exists():
        fail(f"{binary} is missing: fill the data root first (W1)")
    got = sha256(binary)
    if got != BINARY_SHA256:
        fail(f"{binary} has SHA-256 {got}, not the snapshot binary's")
    if case_dir.exists():
        fail(f"{case_dir} exists; every run gets a fresh scratch folder")
    case_dir.mkdir(parents=True)
    inputs = inputs_source(case, data_root)
    if inputs.is_dir():
        shutil.copytree(inputs, case_dir / "inputs")
    if args.extra_inputs:
        for f in sorted(Path(args.extra_inputs).iterdir()):
            target = case_dir / "inputs" / f.name
            if target.exists():
                fail(f"--extra-inputs: {f.name} clashes with the case's own inputs")
            target.parent.mkdir(exist_ok=True)
            (shutil.copytree if f.is_dir() else shutil.copy2)(f, target)
    if (case_dir / "inputs").is_dir():
        for f in sorted((case_dir / "inputs").rglob("*")):
            if f.is_file() and "list" in f.name:
                mapped = apply_map(f.read_text(errors="strict"), case["path_map"], case_dir, data_root)
                check_clean(mapped, f"the list file {f}")
                f.write_text(mapped)
    (case_dir / "case.batch").write_text(batch)

    env = dict(os.environ)
    env["PATH"] = VENV_BIN + os.pathsep + env.get("PATH", "")
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env["MPLCONFIGDIR"] = str(case_dir / ".matplotlib")
    started = time.time()
    with open(case_dir / "console.log", "wb") as console:
        result = subprocess.run(command, cwd=case_dir, env=env, stdout=console,
                                stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    wall = time.time() - started
    record = {
        "case": case["id"],
        "batch": str(source),
        "command": command,
        "exit": result.returncode,
        "wall_s": round(wall, 3),
        "started": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(started)),
        "binary_sha256": got,
    }
    (case_dir / "runner.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"{case['id']}: exit {result.returncode}, {wall:.1f} s, in {case_dir}")
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
