#!/usr/bin/env bash
# fill_data_root.sh: fill FuSION 2.0's test data root from FuSION 1, and write its manifest.
#
# Phase 3 plan (docs/phase3/plan.md), item W1, choices P1 and P2; decision 0037.
# Usage: FUSION_TEST_DATA=/path/to/data-root fill_data_root.sh [--dry-run]
#
# Reads FuSION 1's live tree and its frozen snapshot, read only. It copies, and
# never moves, deletes or overwrites: a file already in the root is kept when its
# SHA-256 matches its source, and stops the script when it differs. Four files must
# match section 12's SHA-256 before anything is copied. With --dry-run the script
# checks every source and prints what it would copy, and writes nothing.
#
# What it copies (section 12, Test data; the case inventory, cases.json):
#   kegg/                 the 163 KGMLs and the 4 updated ones that the cases' lists name
#   legacy/               the old master, and the four old common files the EGF-HeLa batches name
#   expression/, constraints/   the EGF file and the coexpression file
#   fusion1/Linux/        the snapshot binary; the snapshot's supporting_files/, plus the
#                         live color_files/, which the snapshot left out as data
#   reference/<id>/       for each recorded case: inputs/, f1/run/, its console log, and
#                         f1/dumps/ with every SMT dump folder of the run; the model
#                         example's inputs/
# It then writes backend/tests/reference/data-manifest.json: every file's path, size,
# SHA-256 and origin.
set -euo pipefail

ARJUN=/home/ks-149-u24/Desktop/arjun
FUSION1=$ARJUN/FuSION
SNAPSHOT=$ARJUN/FuSION1-reference-2026-11-10
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS=$(cd "$HERE/../.." && pwd)
REPO=$(cd "$TESTS/../.." && pwd)
CASES=$TESTS/reference/cases.json
MANIFEST=$TESTS/reference/data-manifest.json
ROOT=${FUSION_TEST_DATA:?set FUSION_TEST_DATA to the data root}
DRY=0
case ${1:-} in
  --dry-run) DRY=1 ;;
  '') ;;
  *) echo "usage: FUSION_TEST_DATA=<root> $0 [--dry-run]" >&2; exit 2 ;;
esac

fail() { echo "fill_data_root: $*" >&2; exit 1; }

ROOT=$(realpath -m "$ROOT")
case "$ROOT/" in
  "$FUSION1"/*|"$SNAPSHOT"/*|"$REPO"/*) fail "the data root must lie outside FuSION 1, its snapshot and this repository: $ROOT" ;;
esac
command -v jq >/dev/null || fail "jq is needed to read cases.json"

# Section 12's SHA-256s, checked before anything is copied.
declare -A PIN=(
  [fusion1/Linux/fusion]=22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50
  [legacy/merged_kegg_master_graph_with_exp_score.xml]=9cb47389e7a42bdaa0dc329fd0d3b7ee16a049a309998479c8cdcdff4325fa24
  [expression/egf_hela_t120_lfc.tsv]=777689a645f48d766fb2f821bb93ae31d0a68d3ec6ab7767970163389f33432d
  [kegg/hsa_pathways_with_exp_score/hsa04010.xml]=ce0b9f2863d1749510a1e97ab804fe50b0380391c3d1f93cbcba1bd44e53a6d3
)

DEST=()
SRC=()
add() { DEST+=("$1"); SRC+=("$2"); }
add_tree() {  # add_tree <dest prefix> <source dir>: every file under the source dir
  local f
  [[ -d $2 ]] || fail "missing folder: $2"
  while IFS= read -r f; do add "$1/$f" "$2/$f"; done < <(cd "$2" && find . -type f -printf '%P\n' | LC_ALL=C sort)
}

# KEGG data: all 163 KGMLs with experiment scores, and the 4 updated ones the cases' lists name.
n=0
for f in "$FUSION1"/auxilliary/hsa_pathways_with_exp_score/*.xml; do
  add "kegg/hsa_pathways_with_exp_score/${f##*/}" "$f"; n=$((n + 1))
done
(( n == 163 )) || fail "expected 163 KGMLs in auxilliary/hsa_pathways_with_exp_score, found $n"
for id in hsa04216 hsa05224 hsa05225 hsa05226; do
  add "kegg/updated_pathways/$id.xml" "$FUSION1/updated_pathways/xml_files/$id.xml"
done

# The old master, the old common files, the EGF file and the coexpression file.
add legacy/merged_kegg_master_graph_with_exp_score.xml "$FUSION1/runs/real_experiments/merged_kegg_master_graph_with_exp_score.xml"
for f in hsa_ids_not_to_be_merged.txt hsa_path_to_path_name_map_2822.txt hsa_to_gene_symbol_map_6334.txt inter_db_mapping_file; do
  add "legacy/IITB_ACTREC_old_data/$f" "$FUSION1/auxilliary/IITB_ACTREC_old_data/IITB_ACTREC_old_data/$f"
done
add expression/egf_hela_t120_lfc.tsv "$FUSION1/runs/egf_hela/lfc/egf_hela_t120_lfc.tsv"
add constraints/hsa_stringdb_coexpression_links_over400.csv "$FUSION1/additional_constraints/coexpression_csv/hsa_stringdb_coexpression_links_over400.csv"

# FuSION 1 itself: the snapshot binary, the snapshot's supporting files, and the live color maps.
add fusion1/Linux/fusion "$FUSION1/Linux/fusion"
add_tree fusion1/Linux/supporting_files "$SNAPSHOT/Linux/supporting_files"
while IFS= read -r f; do  # live color maps the snapshot left out; the snapshot's own copies win
  [[ -e $SNAPSHOT/Linux/supporting_files/color_files/$f ]] && continue
  add "fusion1/Linux/supporting_files/color_files/$f" "$FUSION1/Linux/supporting_files/color_files/$f"
done < <(cd "$FUSION1/Linux/supporting_files/color_files" && find . -type f -printf '%P\n' | LC_ALL=C sort)

# The recorded cases: inputs, outputs, console log and SMT dumps.
while IFS=$'\t' read -r id outputs inputs console; do
  add_tree "reference/$id/inputs" "$FUSION1/$inputs"
  add_tree "reference/$id/f1/run" "$FUSION1/$outputs/run"
  add "reference/$id/$console" "$FUSION1/$outputs/${console##*/}"
  dumps=0
  for d in "$FUSION1/$outputs"/*smt-files*/; do
    d=${d%/}
    add_tree "reference/$id/f1/dumps/${d##*/}" "$d"
    dumps=$((dumps + 1))
  done
  (( dumps > 0 )) || fail "$id: no SMT dump folder in $outputs"
done < <(jq -r '.cases[] | select(.source | startswith("recorded")) | [.id, .origin.outputs, .origin.inputs, .fusion1.console] | @tsv' "$CASES")

# The model example's inputs; its outputs are regenerated (W4).
add_tree reference/model-example/inputs "$FUSION1/$(jq -r '.cases[] | select(.id == "model-example") | .origin.inputs' "$CASES")"

# Every source must exist; the four pinned ones must match section 12.
total=0
for i in "${!DEST[@]}"; do
  [[ -f ${SRC[i]} && -r ${SRC[i]} ]] || fail "missing or unreadable: ${SRC[i]}"
  total=$((total + $(stat -c %s "${SRC[i]}")))
done
for i in "${!DEST[@]}"; do
  want=${PIN[${DEST[i]}]:-}
  [[ -n $want ]] || continue
  got=$(sha256sum "${SRC[i]}" | cut -d' ' -f1)
  [[ $got == "$want" ]] || fail "${SRC[i]} has SHA-256 $got, not section 12's $want"
  echo "pinned ok: ${DEST[i]}"
done
dups=$(printf '%s\n' "${DEST[@]}" | LC_ALL=C sort | uniq -d)
[[ -z $dups ]] || fail "two sources for one destination: $dups"

echo "${#DEST[@]} files, $((total / 1000000)) MB, into $ROOT"
printf '%s\n' "${DEST[@]}" | awk -F/ '{ k = $1; if ($1 == "reference") k = $1 "/<id>/" $3; n[k]++ } END { for (k in n) printf "  %6d  %s\n", n[k], k }' | LC_ALL=C sort -k2
if (( DRY )); then
  echo "dry run: nothing written"
  exit 0
fi

copied=0
kept=0
for i in "${!DEST[@]}"; do
  dest=$ROOT/${DEST[i]}
  if [[ -e $dest ]]; then
    [[ $(sha256sum "$dest" | cut -d' ' -f1) == $(sha256sum "${SRC[i]}" | cut -d' ' -f1) ]] \
      || fail "$dest already exists and differs from ${SRC[i]}"
    kept=$((kept + 1))
  else
    mkdir -p "${dest%/*}"
    cp -p "${SRC[i]}" "$dest"
    copied=$((copied + 1))
  fi
done
echo "copied $copied files; $kept were already there and matched"

for i in "${!DEST[@]}"; do
  f=$ROOT/${DEST[i]}
  printf '%s\t%s\t%s\t%s\n' "${DEST[i]}" "$(stat -c %s "$f")" "$(sha256sum "$f" | cut -d' ' -f1)" "${SRC[i]#"$ARJUN"/}"
done | LC_ALL=C sort | jq -R -s '{
  schema: 1,
  root_env: "FUSION_TEST_DATA",
  files: (split("\n") | map(select(length > 0) | split("\t")
          | {path: .[0], size: (.[1] | tonumber), sha256: .[2], from: .[3]}))
}' > "$MANIFEST"
echo "wrote $MANIFEST"
