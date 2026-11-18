# gseapy 1.3.1 references

Synthetic GSEA inputs and gseapy 1.3.1's results on them, for section 12's test "GSEA agreement with gseapy (S7.11)". 2.0's `enrichment` module must reproduce them exactly (08.C1): ES, NES, nominal p, FDR q and FWER p equal as doubles, and `Tag %`, `Gene %` and `Lead_genes` equal as text, for every tested set and every seed. Every gene id (`g1`, ...) and set id (`s1`, ...) is made up, and so is every value, so nothing here is KEGG data (S8.31).

Written by `backend/tests/experiments/capture/w5/gseapy_references.py` (Phase 3 plan, item W5), which writes this folder whole and never overwrites it. Do not edit the files by hand.

## How gseapy was run

Section 08's harness (its test hooks), with gseapy 1.3.1 from FuSION 1's `.venv`:

```
gseapy.prerank(rnk=<values.tsv as a Series, by value descending, then by id ascending in byte order>,
               gene_sets=<sets.gmt as a dict>, permutation_num=1000, min_size=15, max_size=500,
               weight=1.0, seed=S, ascending=None, outdir=None)
```

- With `ascending=None`, gseapy keeps the given order (gseapy `gsea.py:493`); the harness checked that every call did.
- Seeds 42 and 1 to 20, 1000 permutations each, on gseapy's default of 4 threads. Seed 42 was then rerun with `threads=1`, and the harness required identical output (2.0 runs on one thread, 08.C6).
- `gseapy-run.json` records the versions, the call, the seeds, each input's SHA-256, the checks and the times.
- The same harness ran gseapy on the EGF file and its 167 candidate pathways, at the same seeds. Those results hold KEGG data, so they are in the data root's `reports/w5/gseapy_egf/`, not here (S8.31).

## Files

Each fixture has a folder:
- `values.tsv`: `<gene id><TAB><value>` per line, LF, no header, the values as two-decimal text, in a shuffled line order, so the ranking is the reader's to build (S7.05, 08.C2).
- `sets.gmt`: one set per line, `<set id><TAB><kind><TAB><gene id>...`, genes in byte order. A gene that `values.tsv` lacks is unranked and counts towards no set's size m.
- `observed.tsv`: `Term`, `ES`, `Tag %`, `Gene %` and `Lead_genes` for every tested set, by set id in byte order. They need no permutation, and the harness checked that all 21 seeds gave the same values.
- `stats.tsv`: `seed`, `Term`, `NES`, `NOM p-val`, `FDR q-val` and `FWER p-val` for every seed and tested set, by seed, then set id.

Numbers are Python's `repr` of gseapy's doubles: the shortest text that reads back to the same double. A set that gseapy did not test has no row.

## Fixtures

| Fixture | Genes N | Distinct values | Zeros | Repeated values (gseapy's count) | Sets | Tested |
|---|---|---|---|---|---|---|
| n50 | 50 | 18 | 5, 3 of them written `-0.00` | 32 (64.00%) | 11 | 9 |
| n500 | 500 | 155 | 9, 4 of them written `-0.00` | 345 (69.00%) | 23 | 22 |
| n3054 | 3054 | 190 | 145, 24 of them written `-0.00` | 2864 (93.78%) | 32 | 30 |

- **n50:** values in steps of 0.1, written with two decimals, so the ties are heaviest.
- **n500:** two-decimal values, mostly near 0, with a long upper tail like the EGF file's.
- **n3054:** the EGF file's tie structure, as section 12 asks: 3054 genes and 190 distinct values, with a block of 145 at 0.00, 24 of them written `-0.00`, one of 71 at -0.06, and the EGF file's other 188 block sizes, in a seeded order, at made-up two-decimal values. Only that structure comes from the EGF file, none of its other values.

What they cover:
- **Exact equality** (08.C1): every fixture at 21 seeds, and on one thread at seed 42.
- **Ties by id in byte order** (S7.05, 08.C2): ids run from `g1` with no padding, so inside a block of equal values byte order (`g10` < `g100` < `g11`) differs from numeric order. `0.00` and `-0.00` are one value.
- **The size filter** (S7.09): gseapy tests 15 to 500 ranked genes, and fewer than N. It gives an untested set no row, where 2.0 gives one without scores.
  - n50: tested m from 15 to 49; untested s1 (m = 14), s8 (m = 50).
  - n500: tested m from 15 to 218; untested s1 (m = 14).
  - n3054: tested m from 15 to 500; untested s1 (m = 14), s31 (m = 501).
- **Unranked genes** (S7.08): these tested sets hold genes that `values.tsv` lacks: n50's s11; n500's s10 and s23; n3054's s10 and s32.
- **Both signs:** the blocks and the biased sets sit at the ends of the ranking, to give large positive and negative NES with small p values; the random sets give the rest.
- **The pooled FDR** (S7.12): 9 tested sets in n50, 22 tested sets in n500 and 30 tested sets in n3054.
- **Equal |NES|** (08.C9, XD-28): the twins (n500's s9 and s10; n3054's s9 and s10) have the same ranked genes. The harness checked that each pair got identical results; 2.0 lists s10 before s9, by byte order.
- **Hits of value 0** (S7.10): n3054's s29 holds 14 genes of value 0 and 6 others, so its walk takes steps of 0.
- **The tie warning's count** (08.C10): the repeated values above are gseapy's count, which the harness checked.

## Sets

Kinds:
- `random`: genes drawn uniformly from the ranking.
- `all`: every ranked gene, so m = N, which gseapy does not test.
- `top-block`: the first m genes of the ranking.
- `bottom-block`: the last m genes of the ranking.
- `top-biased`: half of the genes from the top tenth of the ranking, half from anywhere.
- `bottom-biased`: half of the genes from the bottom tenth of the ranking, half from anywhere.
- `twin`: s9 and s10 have the same ranked genes, and s10 also has 3 unranked ones.
- `zero-heavy`: 14 genes of value 0 and 6 others.

### n50

| Set | Kind | Ranked genes m | Unranked genes | Tested |
|---|---|---|---|---|
| s1 | random | 14 | 0 | no |
| s2 | random | 15 | 0 | yes |
| s3 | random | 20 | 0 | yes |
| s4 | random | 25 | 0 | yes |
| s5 | random | 30 | 0 | yes |
| s6 | random | 40 | 0 | yes |
| s7 | random | 49 | 0 | yes |
| s8 | all | 50 | 0 | no |
| s9 | top-block | 15 | 0 | yes |
| s10 | bottom-block | 15 | 0 | yes |
| s11 | random | 18 | 5 | yes |

### n500

| Set | Kind | Ranked genes m | Unranked genes | Tested |
|---|---|---|---|---|
| s1 | random | 14 | 0 | no |
| s2 | random | 15 | 0 | yes |
| s3 | random | 59 | 0 | yes |
| s4 | random | 177 | 0 | yes |
| s5 | random | 37 | 0 | yes |
| s6 | random | 213 | 0 | yes |
| s7 | random | 51 | 0 | yes |
| s8 | random | 85 | 0 | yes |
| s9 | twin | 40 | 0 | yes |
| s10 | twin | 40 | 3 | yes |
| s11 | random | 126 | 0 | yes |
| s12 | random | 51 | 0 | yes |
| s13 | random | 218 | 0 | yes |
| s14 | random | 73 | 0 | yes |
| s15 | random | 76 | 0 | yes |
| s16 | random | 20 | 0 | yes |
| s17 | random | 59 | 0 | yes |
| s18 | random | 144 | 0 | yes |
| s19 | top-biased | 30 | 0 | yes |
| s20 | top-biased | 80 | 0 | yes |
| s21 | bottom-biased | 25 | 0 | yes |
| s22 | bottom-biased | 60 | 0 | yes |
| s23 | random | 35 | 10 | yes |

### n3054

| Set | Kind | Ranked genes m | Unranked genes | Tested |
|---|---|---|---|---|
| s1 | random | 14 | 0 | no |
| s2 | random | 15 | 0 | yes |
| s3 | random | 326 | 0 | yes |
| s4 | random | 194 | 0 | yes |
| s5 | random | 69 | 0 | yes |
| s6 | random | 102 | 0 | yes |
| s7 | random | 59 | 0 | yes |
| s8 | random | 58 | 0 | yes |
| s9 | twin | 60 | 0 | yes |
| s10 | twin | 60 | 3 | yes |
| s11 | random | 30 | 0 | yes |
| s12 | random | 67 | 0 | yes |
| s13 | random | 25 | 0 | yes |
| s14 | random | 27 | 0 | yes |
| s15 | random | 406 | 0 | yes |
| s16 | random | 22 | 0 | yes |
| s17 | random | 74 | 0 | yes |
| s18 | random | 176 | 0 | yes |
| s19 | random | 143 | 0 | yes |
| s20 | random | 286 | 0 | yes |
| s21 | random | 179 | 0 | yes |
| s22 | random | 18 | 0 | yes |
| s23 | top-biased | 50 | 0 | yes |
| s24 | top-biased | 150 | 0 | yes |
| s25 | bottom-biased | 40 | 0 | yes |
| s26 | bottom-biased | 120 | 0 | yes |
| s27 | top-block | 15 | 0 | yes |
| s28 | bottom-block | 15 | 0 | yes |
| s29 | zero-heavy | 20 | 0 | yes |
| s30 | random | 500 | 0 | yes |
| s31 | random | 501 | 0 | no |
| s32 | random | 30 | 20 | yes |

## Kept out

- **A draw whose hits all have value 0** (08.C7): gseapy divides by zero there, so its result is an artefact, and 2.0 follows its own rule instead (XD-27). No tested set's ranked genes all have value 0. For the permutations:
  - n50 has 5 zeros, fewer than the 15 ranked genes of its smallest tested set, so no draw can hit only zeros;
  - n500 has 9 zeros, fewer than the 15 ranked genes of its smallest tested set, so no draw can hit only zeros;
  - n3054 has 145 zeros; the chance that any draw of any tested set, seed and permutation hits only zeros is at most 4e-16 (a union bound, each draw taken as uniform).

  Section 12's fixture with 80 % zeros, which checks 2.0's own rule, is not here.
- **The real inputs:** the EGF file and the 167 candidate KGMLs are KEGG data, and so are gseapy's results on them, which are in the data root's `reports/w5/gseapy_egf/` (S8.31).
