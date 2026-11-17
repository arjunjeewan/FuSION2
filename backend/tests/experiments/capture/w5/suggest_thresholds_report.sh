#!/usr/bin/env bash
# suggest_thresholds_report.sh: save FuSION 1's suggest_thresholds.py report on the EGF file.
#
# Phase 3 plan (docs/phase3/plan.md), item W5, second part; section 12, capture step 4 and
# the expression row of the library-level tests (line 381), whose [kegg] test compares 2.0's
# threshold suggestions with this report; spec stage 3, S3.15 to S3.17; decision 0037.
# Usage: FUSION_TEST_DATA=/path/to/data-root suggest_thresholds_report.sh [--dry-run]
#
# Inputs, read only:
#   $FUSION_TEST_DATA/fusion1/Linux/supporting_files/suggest_thresholds.py
#       W1's copy of the snapshot's script, run from the data root as the plan runs FuSION 1's
#       files; it must match the snapshot's MANIFEST.sha256
#   $FUSION_TEST_DATA/expression/egf_hela_t120_lfc.tsv
#       W1's copy of the EGF file, which must have section 12's SHA-256
#   FuSION 1's .venv/bin/python, with PYTHONDONTWRITEBYTECODE=1 and no other PYTHON* variable.
#       The script needs only Python's standard library, and without --plot no gnuplot.
# Outputs, in $FUSION_TEST_DATA/reports/w5/suggest_thresholds/, which must not exist yet:
#   plain.txt, plain.stderr        the report with no options
#   current.txt, current.stderr    the report with --current 1.01 -1.01, the reference run's cut:
#                                  S3.15 recomputed the report with it, and section 04's
#                                  expected table has its row (04-expression.md:548)
#   run.json                       the commands, exit statuses, input SHA-256s and Python version
# With --dry-run it checks the inputs, using the snapshot's script and FuSION 1's EGF file in
# place of W1's copies until W1 has run, prints the commands, and writes nothing.
set -euo pipefail

ARJUN=/home/ks-149-u24/Desktop/arjun
FUSION1=$ARJUN/FuSION
SNAPSHOT=$ARJUN/FuSION1-reference-2026-11-10
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)
VENV_PY=$FUSION1/.venv/bin/python
EGF_SHA256=777689a645f48d766fb2f821bb93ae31d0a68d3ec6ab7767970163389f33432d  # section 12
ROOT=${FUSION_TEST_DATA:?set FUSION_TEST_DATA to the data root}
DRY=0
case ${1:-} in
  --dry-run) DRY=1 ;;
  '') ;;
  *) echo "usage: FUSION_TEST_DATA=<root> $0 [--dry-run]" >&2; exit 2 ;;
esac

fail() { echo "suggest_thresholds_report: $*" >&2; exit 1; }

ROOT=$(realpath -m "$ROOT")
case "$ROOT/" in
  "$FUSION1"/*|"$SNAPSHOT"/*|"$REPO"/*) fail "the data root must lie outside FuSION 1, its snapshot and this repository: $ROOT" ;;
esac
command -v jq >/dev/null || fail "jq is needed to write run.json"
SCRIPT=$ROOT/fusion1/Linux/supporting_files/suggest_thresholds.py
EGF=$ROOT/expression/egf_hela_t120_lfc.tsv
OUT=$ROOT/reports/w5/suggest_thresholds
SCRIPT_SHA256=$(awk '$2 == "./Linux/supporting_files/suggest_thresholds.py" { print $1 }' "$SNAPSHOT/MANIFEST.sha256")
[[ -n $SCRIPT_SHA256 ]] || fail "the snapshot's MANIFEST.sha256 does not list Linux/supporting_files/suggest_thresholds.py"

# check FILE SHA256 SOURCE: FILE must have SHA256. Before W1 has run, a dry run checks SOURCE,
# the file W1 copies, instead.
check() {
  local file=$1 got
  if [[ ! -f $file ]]; then
    (( DRY )) || fail "$file is missing: fill the data root first (W1)"
    echo "not there yet (W1 copies it): $file; checking its source $3"
    file=$3
  fi
  got=$(sha256sum "$file" | cut -d' ' -f1)
  [[ $got == "$2" ]] || fail "$file has SHA-256 $got, not $2"
  echo "ok: $file"
}
check "$SCRIPT" "$SCRIPT_SHA256" "$SNAPSHOT/Linux/supporting_files/suggest_thresholds.py"
check "$EGF" "$EGF_SHA256" "$FUSION1/runs/egf_hela/lfc/egf_hela_t120_lfc.tsv"
[[ -x $VENV_PY ]] || fail "$VENV_PY is missing"
echo "ok: $VENV_PY"

NAMES=(plain current)
declare -A OPTIONS=([plain]="" [current]="--current 1.01 -1.01")

if (( DRY )); then
  if [[ -e $OUT ]]; then echo "note: $OUT exists, so a real run would stop"; else echo "would create $OUT"; fi
  for name in "${NAMES[@]}"; do
    read -ra opts <<< "${OPTIONS[$name]}"
    echo "command: cd $OUT && PYTHONDONTWRITEBYTECODE=1 $VENV_PY $SCRIPT $EGF${opts[*]:+ ${opts[*]}} > $name.txt 2> $name.stderr"
  done
  echo "dry run: nothing written"
  exit 0
fi

[[ ! -e $OUT ]] || fail "$OUT exists; each run writes a fresh folder"
mkdir -p "${OUT%/*}"
mkdir "$OUT"
while IFS= read -r var; do unset "$var"; done < <(compgen -e | grep '^PYTHON' || true)
export PYTHONDONTWRITEBYTECODE=1
python_version=$("$VENV_PY" -c 'import sys; print(sys.version.split()[0])')
started=$(date +%Y-%m-%dT%H:%M:%S%z)
runs=()
status=0
for name in "${NAMES[@]}"; do
  read -ra opts <<< "${OPTIONS[$name]}"
  rc=0
  (cd "$OUT" && "$VENV_PY" "$SCRIPT" "$EGF" "${opts[@]}" > "$name.txt" 2> "$name.stderr") || rc=$?
  if (( rc != 0 )); then status=1; fi
  if [[ -s $OUT/$name.stderr ]]; then echo "warning: $name wrote to stderr, see $OUT/$name.stderr" >&2; fi
  echo "$name: exit $rc, $(wc -l < "$OUT/$name.txt") lines in $OUT/$name.txt"
  runs+=("$(jq -n --arg report "$name.txt" --arg stderr "$name.stderr" --argjson exit "$rc" \
    --arg options "${OPTIONS[$name]}" \
    '{report: $report, stderr: $stderr, options: ($options | split(" ") | map(select(. != ""))), exit: $exit}')")
done

printf '%s\n' "${runs[@]}" | jq -s \
  --arg started "$started" --arg python "$python_version" --arg script "${SCRIPT#"$ROOT"/}" \
  --arg script_sha256 "$SCRIPT_SHA256" --arg values "${EGF#"$ROOT"/}" --arg values_sha256 "$EGF_SHA256" '{
  made_by: "backend/tests/experiments/capture/w5/suggest_thresholds_report.sh",
  plan_item: "W5",
  started: $started,
  python: $python,
  command: "cd <this folder> && PYTHONDONTWRITEBYTECODE=1 .venv/bin/python <script> <values> <options>",
  script: {path: $script, sha256: $script_sha256, snapshot: "Linux/supporting_files/suggest_thresholds.py"},
  values: {path: $values, sha256: $values_sha256},
  runs: .
}' > "$OUT/run.json"
echo "wrote $OUT"
exit "$status"
