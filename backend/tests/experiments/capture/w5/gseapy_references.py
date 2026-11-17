#!/usr/bin/env python3
"""Record gseapy 1.3.1's results for 2.0's exact GSEA test, on synthetic fixtures and the EGF file.

Phase 3 plan (docs/phase3/plan.md), item W5, first part; decision 0037; prompt log entry
282. Section 12 designs the test ("GSEA agreement with gseapy (S7.11)", lines 761 to 772;
capture step 4), and section 08 the harness (its test hooks) and the rules the inputs rest
on: 08.C1 (2.0 reproduces gseapy 1.3.1 exactly), 08.C2 (ties by gene id in byte order),
08.C6 (one thread) and 08.C7 (a draw whose hits all have value 0, where gseapy's result is
an artefact).

Usage:
  FUSION_TEST_DATA=<data root> python3 -I gseapy_references.py [--dry-run]

Inputs, read only:
  - $FUSION_TEST_DATA/expression/egf_hela_t120_lfc.tsv, W1's copy of the EGF file, checked
    against section 12's SHA-256. The EGF run ranks it. Fixture n3054 takes only its tie
    structure, never its values.
  - $FUSION_TEST_DATA/reference/jun-b10-fdr025/inputs/list_of_kegg_xml_files.txt, the 167
    candidates that all 19 gsea batches use (section 08's md5 ac05b281...), mapped by that
    case's path map in cases.json onto the data root's kegg/ folders.
  - gseapy 1.3.1 in FuSION 1's .venv, run as .venv/bin/python with PYTHONDONTWRITEBYTECODE=1,
    no other PYTHON* variable, and MPLCONFIGDIR in a temporary folder, so that nothing is
    written into FuSION 1's tree.

Outputs, each a fresh folder that must not exist yet:
  backend/tests/fixtures/gsea/   synthetic, every id made up, so no KEGG data (S8.31):
    README.md               the fixtures, what each covers, and how gseapy was run
    gseapy-run.json         versions, the call, the seeds, input SHA-256s, the checks, times
    <fixture>/values.tsv    <gene id><TAB><value>, in a shuffled line order
    <fixture>/sets.gmt      <set id><TAB><kind><TAB><gene id>...
    <fixture>/observed.tsv  gseapy's ES, Tag %, Gene % and Lead_genes per tested set
    <fixture>/stats.tsv     gseapy's NES, nominal p, FDR q and FWER p per seed and set
  $FUSION_TEST_DATA/reports/w5/gseapy_egf/   the EGF run, which holds KEGG data:
    values.tsv (a copy of the EGF file), sets.gmt (the 167 candidates' gene sets, built as
    gsea_select_pathways.py:99-116 builds them), inputs.json, observed.tsv, stats.tsv, run.json

A run:
  1. builds the fixtures n50, n500 and n3054 with seeded generators, and checks them: sizes,
     ties, and no draw whose hits are all 0; and builds the EGF run's gene sets from the
     KGMLs; all in a temporary folder;
  2. runs this file under the .venv's Python with --harness, which calls
     gseapy.prerank(rnk=<values ordered by value descending, then id ascending in byte
     order>, gene_sets=<sets.gmt>, permutation_num=1000, min_size=15, max_size=500,
     weight=1.0, seed=S, ascending=None, outdir=None) for seeds 42 and 1 to 20 on every
     fixture and on the EGF file, checks what each call returns, and reruns seed 42 with
     threads=1, which must give identical output;
  3. copies the fixtures to backend/tests/fixtures/gsea/ and the EGF run to the data root.
     A failed run publishes nothing and keeps the temporary folder, naming it.
--dry-run does step 1, reading FuSION 1's own copies of any input that W1 has not copied
yet, prints the README and the harness's command, starts no gseapy, and removes the
temporary folder.
"""
import argparse
import hashlib
import json
import math
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

FUSION1 = "/home/ks-149-u24/Desktop/arjun/FuSION"
SNAPSHOT = "/home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10"
VENV_PYTHON = f"{FUSION1}/.venv/bin/python"
VENV_SITE = Path(f"{FUSION1}/.venv/lib/python3.12/site-packages")
TESTS = Path(__file__).resolve().parents[3]
REPO = TESTS.parents[1]
OUT = TESTS / "fixtures" / "gsea"
ME = "backend/tests/experiments/capture/w5/gseapy_references.py"
# In the data root:
EGF = "expression/egf_hela_t120_lfc.tsv"
EGF_SOURCE = f"{FUSION1}/runs/egf_hela/lfc/egf_hela_t120_lfc.tsv"  # what W1 copies
EGF_SHA256 = "777689a645f48d766fb2f821bb93ae31d0a68d3ec6ab7767970163389f33432d"  # section 12
REFERENCE_RUN = "jun-b10-fdr025"  # its inputs/ hold the candidate list of all 19 gsea batches
CANDIDATES = "list_of_kegg_xml_files.txt"
CANDIDATES_SHA256 = "86fc603f19d96981c688e8356212b45a880c888abb162f2e6abe16ee10156bb8"  # md5 ac05b281...
EGF_OUT = "reports/w5/gseapy_egf"
SECTION_08_SELECTED = {"0.25": 44, "0.4": 79, "0.44": 86, "0.5": 91}  # fdr cut: pathways, at seed 42

GSEAPY = "1.3.1"
SEEDS = [42] + list(range(1, 21))                  # section 12
PERMUTATIONS = 1000
MIN_SIZE, MAX_SIZE = 15, 500                       # S7.09
ZERO_DRAW_LIMIT = 1e-9                             # 08.C7: an all-zero draw must be this unlikely
FIXTURES = {"n50": 50, "n500": 500, "n3054": 3054}  # name: generator seed
OBSERVED = ["ES", "Tag %", "Gene %", "Lead_genes"]  # gseapy's columns that need no permutation
STATS = ["NES", "NOM p-val", "FDR q-val", "FWER p-val"]
KINDS = {
    "random": "genes drawn uniformly from the ranking",
    "all": "every ranked gene, so m = N, which gseapy does not test",
    "top-block": "the first m genes of the ranking",
    "bottom-block": "the last m genes of the ranking",
    "top-biased": "half of the genes from the top tenth of the ranking, half from anywhere",
    "bottom-biased": "half of the genes from the bottom tenth of the ranking, half from anywhere",
    "twin": "s9 and s10 have the same ranked genes, and s10 also has 3 unranked ones",
    "zero-heavy": "14 genes of value 0 and 6 others",
}


def fail(message):
    sys.exit(f"gseapy_references: {message}")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def write_tsv(path, rows):
    write_text(path, "".join("\t".join(row) + "\n" for row in rows))


def read_tsv(path):
    return [line.split("\t") for line in Path(path).read_text(encoding="utf-8").splitlines()]


def ranked(values):
    """The order gseapy is given: value descending, then id ascending in byte order (08.C2)."""
    return sorted(values, key=lambda gv: (-float(gv[1]), gv[0].encode()))


def is_tested(m, n):
    """gseapy's size filter (gseapy base.py:492, S7.09): 15 to 500 ranked genes, and fewer than N."""
    return MIN_SIZE <= m <= MAX_SIZE and m < n


def ranked_sizes(values, sets):
    ids = {g for g, _ in values}
    return {sid: sum(g in ids for g in genes) for sid, _, genes in sets}


def tie_structure(values):
    """The tie structure of [(id, value text)]: what n3054 takes from the EGF file."""
    counts = Counter(float(t) for _, t in values)  # 0.00 and -0.00 are one value
    return {"genes": len(values), "distinct": len(counts), "zeros": counts[0.0],
            "written -0.00": sum(t == "-0.00" for _, t in values), "at -0.06": counts[-0.06],
            "blocks": sorted(counts.values(), reverse=True)}


# ---- step 1: the fixtures ------------------------------------------------------------

def make_values(name, rng, structure):
    """[(gene id, value text)]: two-decimal text with heavy ties, as in the EGF file."""
    def made_up():  # mostly near 0, with a long upper tail like the EGF file's
        return rng.gauss(1.2, 0.9) if rng.random() < 0.04 else rng.gauss(0.04, 0.33)

    if name == "n50":  # steps of 0.1, written with two decimals
        texts = [f"{round(rng.gauss(0.05, 0.4), 1):.2f}" for _ in range(50)]
    elif name == "n500":
        texts = [f"{made_up():.2f}" for _ in range(500)]
    else:  # the EGF file's tie structure (section 12) at made-up values
        others = list(structure["blocks"])
        others.remove(structure["zeros"])
        others.remove(structure["at -0.06"])
        rng.shuffle(others)
        texts = (["-0.00"] * structure["written -0.00"]
                 + ["0.00"] * (structure["zeros"] - structure["written -0.00"])
                 + ["-0.06"] * structure["at -0.06"])
        taken = {0.0, -0.06}
        for size in others:
            value = round(made_up(), 2)
            while value in taken:
                value = round(made_up(), 2)
            taken.add(value)
            texts += [f"{value:.2f}"] * size
        rng.shuffle(texts)
    return [(f"g{i}", t) for i, t in enumerate(texts, 1)]


def set_specs(name, rng):
    """[(kind, ranked genes m, unranked genes)]; set i is s<i>."""
    def sizes(k, hi):  # k sizes, log-uniform from 16 to hi
        return [round(math.exp(rng.uniform(math.log(16), math.log(hi)))) for _ in range(k)]

    if name == "n50":
        return ([("random", m, 0) for m in (14, 15, 20, 25, 30, 40, 49)]
                + [("all", 50, 0), ("top-block", 15, 0), ("bottom-block", 15, 0), ("random", 18, 5)])
    big = name == "n3054"
    hi, twin = (480, 60) if big else (250, 40)
    specs = [("random", 14, 0), ("random", 15, 0)] + [("random", m, 0) for m in sizes(6, hi)]
    specs += [("twin", twin, 0), ("twin", twin, 3)]  # s9 and s10
    specs += [("random", m, 0) for m in sizes(12 if big else 8, hi)]
    if not big:
        return specs + [("top-biased", 30, 0), ("top-biased", 80, 0), ("bottom-biased", 25, 0),
                        ("bottom-biased", 60, 0), ("random", 35, 10)]
    return specs + [("top-biased", 50, 0), ("top-biased", 150, 0), ("bottom-biased", 40, 0),
                    ("bottom-biased", 120, 0), ("top-block", 15, 0), ("bottom-block", 15, 0),
                    ("zero-heavy", 20, 0), ("random", 500, 0), ("random", 501, 0), ("random", 30, 20)]


def make_sets(name, rng, values):
    """[(set id, kind, genes in byte order)]; ids past g<N> are not in the ranking."""
    order = [g for g, _ in ranked(values)]
    value = dict(values)
    zeros = [g for g in order if float(value[g]) == 0]
    others = [g for g in order if float(value[g]) != 0]
    tenth = len(order) // 10
    unranked = (f"g{i}" for i in range(len(order) + 1, 100 * len(order)))
    sets, twin = [], None
    for i, (kind, m, u) in enumerate(set_specs(name, rng), 1):
        if kind == "twin" and twin:  # the second twin takes the first one's ranked genes
            genes, twin = list(twin), None
        elif kind in ("random", "twin"):
            genes = rng.sample(order, m)
            twin = genes if kind == "twin" else None
        elif kind == "all":
            genes = list(order)
        elif kind == "top-block":
            genes = order[:m]
        elif kind == "bottom-block":
            genes = order[-m:]
        elif kind == "zero-heavy":
            genes = rng.sample(zeros, 14) + rng.sample(others, m - 14)
        else:  # top-biased or bottom-biased
            genes = rng.sample(order[:tenth] if kind == "top-biased" else order[-tenth:], m // 2)
            chosen = set(genes)
            genes += rng.sample([g for g in order if g not in chosen], m - m // 2)
        sets.append((f"s{i}", kind, sorted(genes + [next(unranked) for _ in range(u)], key=str.encode)))
    return sets


def check(name, values, sets, structure):
    """The README's facts about one fixture; stops on a fixture section 12's test cannot use."""
    n = len(values)
    value = {g: float(t) for g, t in values}
    if len(value) != n or len({s for s, _, _ in sets}) != len(sets):
        fail(f"{name}: a repeated id")
    ids = list(value) + [s for s, _, _ in sets] + [g for _, _, genes in sets for g in genes]
    if not all(re.fullmatch(r"[gs][1-9][0-9]*", x) for x in ids):
        fail(f"{name}: an id is not g<n> or s<n>")  # so no "__" and no upper case (08, Test hooks)
    ties = tie_structure(values)
    if name == "n3054" and ties != structure:
        fail("n3054 does not have the EGF file's tie structure")
    sizes = ranked_sizes(values, sets)
    rows, chance = [], 0.0
    for sid, kind, genes in sets:
        m = sizes[sid]
        rows.append((sid, kind, m, len(genes) - m, is_tested(m, n)))
        if not is_tested(m, n):
            continue
        if all(value[g] == 0 for g in genes if g in value):
            fail(f"{name}: every ranked gene of {sid} has value 0 (08.C7)")
        if m <= ties["zeros"]:  # a uniform draw of m of the n positions that lands only on zeros
            chance += len(SEEDS) * PERMUTATIONS * math.comb(ties["zeros"], m) / math.comb(n, m)
    if chance > ZERO_DRAW_LIMIT:
        fail(f"{name}: an all-zero draw has a chance of {chance:.1e} over the seeds (08.C7)")
    return {"genes": n, "distinct": ties["distinct"], "zeros": ties["zeros"],
            "minus_zero": ties["written -0.00"], "sets": rows, "chance": chance}


def build(stage, structure):
    """Step 1: write every fixture's inputs and the README into stage."""
    facts = {}
    for name, seed in FIXTURES.items():
        rng = random.Random(seed)
        values = make_values(name, rng, structure)
        sets = make_sets(name, rng, values)
        rng.shuffle(values)
        facts[name] = check(name, values, sets, structure)
        (stage / name).mkdir()
        write_tsv(stage / name / "values.tsv", values)
        write_tsv(stage / name / "sets.gmt", [[s, kind] + genes for s, kind, genes in sets])
    write_text(stage / "README.md", readme(facts))


def readme(facts):
    def listing(parts):
        return parts[0] if len(parts) == 1 else ", ".join(parts[:-1]) + " and " + parts[-1]

    def named(test):  # the sets that pass test, as "n50's s11; n500's s10 and s23"
        found = [(name, [r[0] for r in f["sets"] if test(r)]) for name, f in facts.items()]
        return "; ".join(f"{name}'s {listing(ids)}" for name, ids in found if ids)

    lines = [
        "# gseapy 1.3.1 references",
        "",
        "Synthetic GSEA inputs and gseapy 1.3.1's results on them, for section 12's test \"GSEA "
        "agreement with gseapy (S7.11)\". 2.0's `enrichment` module must reproduce them exactly "
        "(08.C1): ES, NES, nominal p, FDR q and FWER p equal as doubles, and `Tag %`, `Gene %` and "
        "`Lead_genes` equal as text, for every tested set and every seed. Every gene id (`g1`, ...) "
        "and set id (`s1`, ...) is made up, and so is every value, so nothing here is KEGG data (S8.31).",
        "",
        f"Written by `{ME}` (Phase 3 plan, item W5), which writes this folder whole and never "
        "overwrites it. Do not edit the files by hand.",
        "",
        "## How gseapy was run",
        "",
        "Section 08's harness (its test hooks), with gseapy 1.3.1 from FuSION 1's `.venv`:",
        "",
        "```",
        "gseapy.prerank(rnk=<values.tsv as a Series, by value descending, then by id ascending in byte order>,",
        "               gene_sets=<sets.gmt as a dict>, permutation_num=1000, min_size=15, max_size=500,",
        "               weight=1.0, seed=S, ascending=None, outdir=None)",
        "```",
        "",
        "- With `ascending=None`, gseapy keeps the given order (gseapy `gsea.py:493`); the harness "
        "checked that every call did.",
        "- Seeds 42 and 1 to 20, 1000 permutations each, on gseapy's default of 4 threads. Seed 42 "
        "was then rerun with `threads=1`, and the harness required identical output (2.0 runs on "
        "one thread, 08.C6).",
        "- `gseapy-run.json` records the versions, the call, the seeds, each input's SHA-256, the "
        "checks and the times.",
        "- The same harness ran gseapy on the EGF file and its 167 candidate pathways, at the same "
        "seeds. Those results hold KEGG data, so they are in the data root's "
        f"`{EGF_OUT}/`, not here (S8.31).",
        "",
        "## Files",
        "",
        "Each fixture has a folder:",
        "- `values.tsv`: `<gene id><TAB><value>` per line, LF, no header, the values as two-decimal "
        "text, in a shuffled line order, so the ranking is the reader's to build (S7.05, 08.C2).",
        "- `sets.gmt`: one set per line, `<set id><TAB><kind><TAB><gene id>...`, genes in byte order. "
        "A gene that `values.tsv` lacks is unranked and counts towards no set's size m.",
        "- `observed.tsv`: `Term`, `ES`, `Tag %`, `Gene %` and `Lead_genes` for every tested set, by "
        "set id in byte order. They need no permutation, and the harness checked that all 21 seeds "
        "gave the same values.",
        "- `stats.tsv`: `seed`, `Term`, `NES`, `NOM p-val`, `FDR q-val` and `FWER p-val` for every "
        "seed and tested set, by seed, then set id.",
        "",
        "Numbers are Python's `repr` of gseapy's doubles: the shortest text that reads back to the "
        "same double. A set that gseapy did not test has no row.",
        "",
        "## Fixtures",
        "",
        "| Fixture | Genes N | Distinct values | Zeros | Repeated values (gseapy's count) | Sets | Tested |",
        "|---|---|---|---|---|---|---|",
    ]
    for name, f in facts.items():
        repeated = f["genes"] - f["distinct"]
        lines.append(f"| {name} | {f['genes']} | {f['distinct']} | {f['zeros']}, {f['minus_zero']} of them "
                     f"written `-0.00` | {repeated} ({repeated / f['genes']:.2%}) | {len(f['sets'])} | "
                     f"{sum(r[4] for r in f['sets'])} |")
    lines += [
        "",
        "- **n50:** values in steps of 0.1, written with two decimals, so the ties are heaviest.",
        "- **n500:** two-decimal values, mostly near 0, with a long upper tail like the EGF file's.",
        "- **n3054:** the EGF file's tie structure, as section 12 asks: 3054 genes and 190 distinct "
        "values, with a block of 145 at 0.00, 24 of them written `-0.00`, one of 71 at -0.06, and "
        "the EGF file's other 188 block sizes, in a seeded order, at made-up two-decimal values. "
        "Only that structure comes from the EGF file, none of its other values.",
        "",
        "What they cover:",
        "- **Exact equality** (08.C1): every fixture at 21 seeds, and on one thread at seed 42.",
        "- **Ties by id in byte order** (S7.05, 08.C2): ids run from `g1` with no padding, so inside "
        "a block of equal values byte order (`g10` < `g100` < `g11`) differs from numeric order. "
        "`0.00` and `-0.00` are one value.",
        "- **The size filter** (S7.09): gseapy tests 15 to 500 ranked genes, and fewer than N. It "
        "gives an untested set no row, where 2.0 gives one without scores.",
    ]
    for name, f in facts.items():
        ms = [r[2] for r in f["sets"] if r[4]]
        untested = ", ".join(f"{r[0]} (m = {r[2]})" for r in f["sets"] if not r[4])
        lines.append(f"  - {name}: tested m from {min(ms)} to {max(ms)}; untested {untested}.")
    lines += [
        "- **Unranked genes** (S7.08): these tested sets hold genes that `values.tsv` lacks: "
        + named(lambda r: r[3] and r[4]) + ".",
        "- **Both signs:** the blocks and the biased sets sit at the ends of the ranking, to give "
        "large positive and negative NES with small p values; the random sets give the rest.",
        "- **The pooled FDR** (S7.12): " + listing([
            f"{sum(r[4] for r in f['sets'])} tested sets in {name}" for name, f in facts.items()]) + ".",
        "- **Equal |NES|** (08.C9, XD-28): the twins (" + named(lambda r: r[1] == "twin") + ") have "
        "the same ranked genes. The harness checked that each pair got identical results; 2.0 "
        "lists s10 before s9, by byte order.",
        "- **Hits of value 0** (S7.10): " + named(lambda r: r[1] == "zero-heavy") + " holds "
        + KINDS["zero-heavy"] + ", so its walk takes steps of 0.",
        "- **The tie warning's count** (08.C10): the repeated values above are gseapy's count, which "
        "the harness checked.",
        "",
        "## Sets",
        "",
        "Kinds:",
    ]
    lines += [f"- `{k}`: {v}." for k, v in KINDS.items()]
    for name, f in facts.items():
        lines += ["", f"### {name}", "", "| Set | Kind | Ranked genes m | Unranked genes | Tested |",
                  "|---|---|---|---|---|"]
        lines += [f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {'yes' if r[4] else 'no'} |" for r in f["sets"]]
    lines += [
        "",
        "## Kept out",
        "",
        "- **A draw whose hits all have value 0** (08.C7): gseapy divides by zero there, so its "
        "result is an artefact, and 2.0 follows its own rule instead (XD-27). No tested set's ranked "
        "genes all have value 0. For the permutations:",
    ]
    for name, f in facts.items():
        smallest = min(r[2] for r in f["sets"] if r[4])
        if f["zeros"] < smallest:
            lines.append(f"  - {name} has {f['zeros']} zeros, fewer than the {smallest} ranked genes of "
                         "its smallest tested set, so no draw can hit only zeros;")
        else:
            lines.append(f"  - {name} has {f['zeros']} zeros; the chance that any draw of any tested set, "
                         f"seed and permutation hits only zeros is at most {f['chance']:.0e} (a union "
                         "bound, each draw taken as uniform).")
    lines += [
        "",
        "  Section 12's fixture with 80 % zeros, which checks 2.0's own rule, is not here.",
        "- **The real inputs:** the EGF file and the 167 candidate KGMLs are KEGG data, and so are "
        f"gseapy's results on them, which are in the data root's `{EGF_OUT}/` (S8.31).",
    ]
    return "\n".join(lines) + "\n"


def pinned(path, source, digest, dry_run):
    """path, checked against digest. Before W1 has copied it, a dry run checks its source."""
    if not path.is_file():
        if not dry_run:
            fail(f"{path} is missing: fill the data root first (W1)")
        print(f"not there yet (W1 copies it): {path}; the dry run reads its source {source}")
        path = Path(source)
    if sha256(path) != digest:
        fail(f"{path} does not have the SHA-256 {digest}")
    print(f"ok: {path}")
    return path


def build_egf(stage, root, egf, dry_run):
    """Step 1 for the EGF run: the EGF file and its 167 candidates' gene sets (KEGG data)."""
    case = next(c for c in json.loads((TESTS / "reference" / "cases.json").read_text(encoding="utf-8"))["cases"]
                if c["id"] == REFERENCE_RUN)
    listed = pinned(root / "reference" / REFERENCE_RUN / "inputs" / CANDIDATES,
                    f"{FUSION1}/{case['origin']['inputs']}/{CANDIDATES}", CANDIDATES_SHA256, dry_run)
    maps = sorted(case["path_map"], key=lambda e: len(e["from"]), reverse=True)
    kegg = f"{root}/kegg/"
    sets = []
    for line in listed.read_text(encoding="utf-8").splitlines():
        kgml = line.strip()
        if not kgml:
            continue
        mapped = next((e["to"].replace("$DATA", str(root)) + kgml[len(e["from"]):] for e in maps
                       if kgml.startswith(e["from"] + "/")), kgml)
        if not mapped.startswith(kegg):
            fail(f"{kgml} does not map into the data root's kegg/ folders")
        read = Path(mapped)
        if not read.is_file():
            if not dry_run:
                fail(f"{read} is missing: fill the data root first (W1)")
            read = Path(kgml)  # before W1, a dry run reads the file W1 copies
        # As FuSION 1 builds a candidate's gene set (gsea_select_pathways.py:110-115, S7.08).
        tree = ET.parse(read).getroot()
        pid = tree.get("name", kgml).replace("path:", "")
        genes = sorted({g.replace(":", "") for e in tree.iter("entry") if e.get("type") == "gene"
                        for g in e.get("name", "").split()})
        sets.append((pid, mapped[len(str(root)) + 1:], genes))
    if len({s[0] for s in sets}) != len(sets):
        fail("two candidates share a pathway id (S7.07)")
    values = [(g, t) for g, t in read_tsv(egf)]
    sizes = ranked_sizes(values, sets)
    tested = [m for m in sizes.values() if is_tested(m, len(values))]
    print(f"egf: {len(sets)} candidates with {min(len(s[2]) for s in sets)} to {max(len(s[2]) for s in sets)} "
          f"genes; {len(tested)} tested, with {min(tested)} to {max(tested)} ranked genes")
    if (len(sets), len(tested)) != (167, 166):
        fail("expected section 08's 167 candidates, 166 of them tested")
    stage.mkdir()
    shutil.copyfile(egf, stage / "values.tsv")
    write_tsv(stage / "sets.gmt", [[pid, path] + genes for pid, path, genes in sets])
    write_text(stage / "inputs.json", json.dumps({
        "values": {"path": EGF, "sha256": EGF_SHA256},
        "candidates": {"path": f"reference/{REFERENCE_RUN}/inputs/{CANDIDATES}", "sha256": CANDIDATES_SHA256,
                       "count": len(sets), "gene_sets": "as gsea_select_pathways.py:99-116 builds them; "
                       "sets.gmt gives each set's KGML in the data root"},
    }, indent=2) + "\n")


# ---- step 2: the harness, run under FuSION 1's .venv -------------------------------

def run_gseapy(folder, gseapy, pandas):
    """Seeds 42 and 1 to 20 on one input folder, then seed 42 on one thread. Writes
    observed.tsv and stats.tsv; returns the folder's record, the values that differ on one
    thread, and the table at seed 42."""
    started = time.time()
    values = [(g, t) for g, t in read_tsv(folder / "values.tsv")]
    sets = [(row[0], row[1], row[2:]) for row in read_tsv(folder / "sets.gmt")]
    order = ranked(values)
    ids = [g for g, _ in order]
    n = len(ids)
    rnk = pandas.Series([float(t) for _, t in order], index=pandas.Index(ids, name="gene"), name="value")
    gene_sets = {sid: genes for sid, _, genes in sets}
    want = sorted((s for s, m in ranked_sizes(values, sets).items() if is_tested(m, n)), key=str.encode)

    def run(seed, **threads):  # one checked call: {term: OBSERVED and STATS as text}
        pre = gseapy.prerank(rnk=rnk.copy(), gene_sets=gene_sets, permutation_num=PERMUTATIONS,
                             min_size=MIN_SIZE, max_size=MAX_SIZE, weight=1.0, seed=seed,
                             ascending=None, outdir=None, **threads)
        where = f"{folder.name}, seed {seed}" + (", one thread" if threads else "")
        if pre.ranking.index.tolist() != ids:
            fail(f"{where}: gseapy did not keep the given order")
        rows = {r["Term"]: r for r in pre.res2d.to_dict("records")}
        if sorted(rows, key=str.encode) != want:
            fail(f"{where}: gseapy tested {sorted(rows, key=str.encode)}, not {want}")
        table = {}
        for t in want:
            r = rows[t]
            if not all(math.isfinite(float(r[c])) for c in ["ES"] + STATS):
                fail(f"{where}, {t}: gseapy gave a value that is not finite: {r}")
            table[t] = ([repr(float(r["ES"]))] + [str(r[c]) for c in OBSERVED[1:]]
                        + [repr(float(r[c])) for c in STATS])
        return pre, table

    k = len(OBSERVED)
    tables, repeated = {}, None
    for seed in SEEDS:
        pre, tables[seed] = run(seed)
        if repeated is None:
            repeated = int(pre.ranking.duplicated().sum())
        elif any(tables[seed][t][:k] != tables[SEEDS[0]][t][:k] for t in want):
            fail(f"{folder.name}, seed {seed}: ES or a leading edge differs from seed {SEEDS[0]}'s")
    _, one = run(SEEDS[0], threads=1)
    differences = [f"{folder.name}, {t}, {c}: {a} on gseapy's default threads, {b} on one thread"
                   for t in want for c, a, b in zip(OBSERVED + STATS, tables[SEEDS[0]][t], one[t]) if a != b]
    if repeated != n - len({float(t) for _, t in values}):
        fail(f"{folder.name}: gseapy counts {repeated} repeated values, not N minus the distinct values")
    twins, id_set = {}, set(ids)
    for sid, _, genes in sets:
        if sid in want:
            twins.setdefault(frozenset(g for g in genes if g in id_set), []).append(sid)
    for first, *others in twins.values():
        for other in others:
            if any(tables[s][other] != tables[s][first] for s in SEEDS):
                fail(f"{folder.name}: {first} and {other} have the same ranked genes but different results")
    write_tsv(folder / "observed.tsv", [["Term"] + OBSERVED] + [[t] + tables[SEEDS[0]][t][:k] for t in want])
    write_tsv(folder / "stats.tsv", [["seed", "Term"] + STATS]
              + [[str(s), t] + tables[s][t][k:] for s in sorted(SEEDS) for t in want])
    seconds = round(time.time() - started, 3)
    print(f"{folder.name}: {len(want)} of {len(sets)} sets tested at {len(SEEDS)} seeds and once on one "
          f"thread, {seconds} s; {'identical' if not differences else 'DIFFERENT'} on one thread", flush=True)
    record = {
        "sha256": {"values.tsv": sha256(folder / "values.tsv"), "sets.gmt": sha256(folder / "sets.gmt")},
        "genes": n, "sets": len(sets), "tested": len(want),
        "repeated_values": repeated, "repeated_share": f"{repeated / n:.2%}",
        "one_thread": {"seed": SEEDS[0], "threads": 1, "identical": not differences},
        "seconds": seconds,
    }
    return record, differences, tables[SEEDS[0]]


def harness(top):
    import inspect

    import gseapy
    import numpy
    import pandas

    if gseapy.__version__ != GSEAPY:
        fail(f"gseapy is {gseapy.__version__}, not {GSEAPY}")
    base = {
        "made_by": ME,
        "plan_item": "W5",
        "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "versions": {"python": sys.version.split()[0], "gseapy": gseapy.__version__,
                     "numpy": numpy.__version__, "pandas": pandas.__version__},
        "gseapy_core": {p.name: sha256(p) for p in sorted(Path(gseapy.__file__).parent.glob("gse.*.so"))},
        "call": {"rnk": "values.tsv by value descending, then by id ascending in byte order",
                 "gene_sets": "sets.gmt as a dict", "permutation_num": PERMUTATIONS,
                 "min_size": MIN_SIZE, "max_size": MAX_SIZE, "weight": 1.0, "ascending": None,
                 "outdir": None,
                 "threads": inspect.signature(gseapy.prerank).parameters["threads"].default},
        "seeds": SEEDS,
        "one_thread_check": f"seed {SEEDS[0]} rerun with threads=1 must give identical output",
    }
    fixtures, differences = {}, []
    for name in FIXTURES:
        fixtures[name], found, _ = run_gseapy(top / "gsea" / name, gseapy, pandas)
        differences += found
    egf, found, table = run_gseapy(top / "egf", gseapy, pandas)
    differences += found
    fdr = len(OBSERVED) + STATS.index("FDR q-val")
    egf["selected_at_seed_42"] = {f"fdr:{q}": sum(float(row[fdr]) <= float(q) for row in table.values())
                                  for q in SECTION_08_SELECTED}
    egf["section_08_selected"] = {f"fdr:{q}": count for q, count in SECTION_08_SELECTED.items()}
    same = list(egf["selected_at_seed_42"].values()) == list(SECTION_08_SELECTED.values())
    print(f"egf at seed 42 selects {egf['selected_at_seed_42']}: "
          + ("as section 08 measured" if same else f"NOT section 08's {egf['section_08_selected']}"), flush=True)
    write_text(top / "gsea" / "gseapy-run.json", json.dumps({**base, "fixtures": fixtures}, indent=2) + "\n")
    write_text(top / "egf" / "run.json", json.dumps({**base, "egf": egf}, indent=2) + "\n")
    if differences:
        print("\n".join(differences), file=sys.stderr)
        fail(f"{len(differences)} value(s) differ between gseapy's default threads and one thread, listed above")


# ---- the driver --------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true",
                        help="build and check the inputs in a temporary folder; start no gseapy")
    parser.add_argument("--harness", metavar="DIR", help=argparse.SUPPRESS)  # step 2, started by step 1
    args = parser.parse_args()
    if not os.environ.get("FUSION_TEST_DATA"):
        fail("set FUSION_TEST_DATA to the data root")
    if args.harness:
        harness(Path(args.harness))
        return
    root = Path(os.environ["FUSION_TEST_DATA"]).resolve()
    if any(str(root) == t or str(root).startswith(t + "/") for t in (FUSION1, SNAPSHOT, str(REPO))):
        fail(f"the data root must lie outside FuSION 1, its snapshot and this repository: {root}")

    for out in (OUT, root / EGF_OUT):
        if out.exists():
            if not args.dry_run:
                fail(f"{out} exists; a run never overwrites it")
            print(f"note: {out} exists, so a real run would stop")
    if not os.access(VENV_PYTHON, os.X_OK):
        fail(f"{VENV_PYTHON} is missing")
    found = sorted(p.name for p in VENV_SITE.glob("gseapy-*.dist-info"))
    if found != [f"gseapy-{GSEAPY}.dist-info"]:
        fail(f"expected gseapy {GSEAPY} alone in {VENV_SITE}, found {found}")
    print(f"ok: gseapy {GSEAPY} in {VENV_SITE}")
    egf = pinned(root / EGF, EGF_SOURCE, EGF_SHA256, args.dry_run)
    structure = tie_structure([(g, t) for g, t in read_tsv(egf)])
    if [structure[k] for k in ("genes", "distinct", "zeros", "at -0.06")] != [3054, 190, 145, 71]:
        fail(f"the EGF file's ties are not section 12's: {structure}")

    tmp = Path(tempfile.mkdtemp(prefix="w5-gseapy-"))
    try:
        (tmp / "gsea").mkdir()
        build(tmp / "gsea", structure)
        build_egf(tmp / "egf", root, egf, args.dry_run)
    except BaseException:
        shutil.rmtree(tmp)
        raise
    env = {k: v for k, v in os.environ.items() if not k.startswith("PYTHON")}
    env.update(PYTHONDONTWRITEBYTECODE="1", MPLCONFIGDIR=str(tmp / "matplotlib"))
    command = [VENV_PYTHON, str(Path(__file__).resolve()), "--harness", str(tmp)]
    shown = f"PYTHONDONTWRITEBYTECODE=1 MPLCONFIGDIR={env['MPLCONFIGDIR']} {' '.join(command)}"
    if args.dry_run:
        print("----- README.md -----")
        print((tmp / "gsea" / "README.md").read_text(encoding="utf-8"), end="")
        print("-----")
        print(f"harness (cwd {tmp}, no other PYTHON* variable):\n  {shown}")
        print(f"then: copy {tmp / 'gsea'} to {OUT}, and {tmp / 'egf'} to {root / EGF_OUT}")
        shutil.rmtree(tmp)
        print("dry run: the temporary folder is removed, and nothing else was written")
        return

    print(f"running: {shown}", flush=True)
    result = subprocess.run(command, cwd=tmp, env=env, stdin=subprocess.DEVNULL)
    if result.returncode != 0:
        fail(f"the harness exited with status {result.returncode}; nothing was published, "
             f"and the temporary folder {tmp} is kept")
    shutil.copytree(tmp / "gsea", OUT)
    (root / EGF_OUT).parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(tmp / "egf", root / EGF_OUT)
    shutil.rmtree(tmp)
    print(f"wrote {OUT} and {root / EGF_OUT}")


if __name__ == "__main__":
    main()
