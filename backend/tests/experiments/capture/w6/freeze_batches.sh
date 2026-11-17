#!/usr/bin/env bash
# freeze_batches.sh: copy FuSION 1's batch corpus and its three GUI scripts, verbatim, into
# backend/tests/fixtures/fusion1-batches/, with that folder's own manifest.
#
# Phase 3 plan (docs/phase3/plan.md), item W6; section 12 of the architecture, "Evidence used"
# (the batch corpus) and "Repository layout" (fixtures/fusion1-batches/); decision 0037.
# Usage: FUSION_TEST_DATA=/path/to/data-root freeze_batches.sh [--dry-run]
#
# Inputs, read only, from FuSION 1's live tree: every *.batch file under runs/ and
# macOS/model_example/, and Linux/script.txt, Linux/output_script.txt and Linux/advtempscript.txt.
# Output: backend/tests/fixtures/fusion1-batches/, holding each file at its path relative to
# FuSION 1's root, and manifest.json there, with each file's path, size and SHA-256, so that the
# frozen copy does not move with the tree. The batch files hold no KEGG content (section 12, Test
# data), so they belong in the repository. Nothing goes to the data root; FUSION_TEST_DATA is
# required only because every Phase 3 script requires it.
#
# The script stops if the output folder exists. It copies into a temporary folder, checks each
# copy against its source and the whole copy against the manifest, checks that the copy holds
# every batch that cases.json names, and only then renames the folder into place. It prints how
# many files it copied, which hold a CR byte or lack a final LF, and the *.batch files elsewhere
# in FuSION 1's tree, which it does not copy. With --dry-run it does all of this in a temporary
# folder that it removes, and writes nothing else.
set -euo pipefail

FUSION1=/home/ks-149-u24/Desktop/arjun/FuSION
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TESTS=$(cd "$HERE/../../.." && pwd)
CASES=$TESTS/reference/cases.json
DEST=$TESTS/fixtures/fusion1-batches
GUI=(Linux/script.txt Linux/output_script.txt Linux/advtempscript.txt)
: "${FUSION_TEST_DATA:?set FUSION_TEST_DATA to the data root}"
DRY=0
case ${1:-} in
  --dry-run) DRY=1 ;;
  '') ;;
  *) echo "usage: FUSION_TEST_DATA=<root> $0 [--dry-run]" >&2; exit 2 ;;
esac

fail() { echo "freeze_batches: $*" >&2; exit 1; }
PROBLEMS=0
stop() {  # stop the run; a dry run notes the problem and carries on checking
  (( DRY )) || fail "$@"
  echo "  WOULD STOP: $*"
  PROBLEMS=$((PROBLEMS + 1))
}
command -v jq >/dev/null || fail "jq is needed to write the manifest"
if [[ -e $DEST ]]; then stop "$DEST exists; the frozen copy is taken once"; fi

# The corpus: every *.batch file under runs/ and macOS/model_example/, then the GUI scripts.
FILES=()
while IFS= read -r f; do FILES+=("${f#"$FUSION1"/}"); done \
  < <(find "$FUSION1/runs" "$FUSION1/macOS/model_example" -type f -name '*.batch' | LC_ALL=C sort)
NBATCH=${#FILES[@]}
FILES+=("${GUI[@]}")
odd=$(find "$FUSION1/runs" "$FUSION1/macOS/model_example" -name '*.batch' ! -type f)
if [[ -n $odd ]]; then stop "not regular files, so not copied: $odd"; fi
ELSEWHERE=()
while IFS= read -r f; do ELSEWHERE+=("${f#"$FUSION1"/}"); done \
  < <(find "$FUSION1" \( -path "$FUSION1/runs" -o -path "$FUSION1/macOS/model_example" \) -prune \
        -o -name '*.batch' -print | LC_ALL=C sort)

if (( DRY )); then
  STAGE=$(mktemp -d)
else
  mkdir -p "${DEST%/*}"
  STAGE=$(mktemp -d "${DEST%/*}/.fusion1-batches.XXXXXX")
fi
trap 'rm -rf "$STAGE"' EXIT

ROWS=()
CR=()
NOLF=()
for rel in "${FILES[@]}"; do
  src=$FUSION1/$rel
  if [[ ! -f $src || ! -r $src ]]; then stop "missing or unreadable: $src"; continue; fi
  want=$(sha256sum < "$src" | cut -d' ' -f1)
  mkdir -p "$STAGE/$(dirname "$rel")"
  cp -p "$src" "$STAGE/$rel"
  got=$(sha256sum < "$STAGE/$rel" | cut -d' ' -f1)
  if [[ $got != "$want" ]]; then stop "$src changed while it was copied"; continue; fi
  ROWS+=("$rel"$'\t'"$(stat -c %s "$STAGE/$rel")"$'\t'"$got")
  if [[ $(tr -cd '\r' < "$STAGE/$rel" | wc -c) -ne 0 ]]; then CR+=("$rel"); fi
  if [[ -s $STAGE/$rel && $(tail -c 1 "$STAGE/$rel" | wc -l) -eq 0 ]]; then NOLF+=("$rel"); fi
done

# Every batch that cases.json names must be in the copy (run_fusion1.py reads it from here).
while IFS= read -r b; do
  if [[ ! -f $STAGE/${b#fusion1-batches/} ]]; then stop "cases.json names $b, which the copy lacks"; fi
done < <(jq -r '.cases[].batch | select(startswith("fusion1-batches/"))' "$CASES")

printf '%s\n' "${ROWS[@]}" | LC_ALL=C sort | jq -R -s --arg taken "$(date +%Y-%m-%dT%H:%M:%S%z)" '{
  schema: 1,
  from: "FuSION",
  taken: $taken,
  files: (split("\n") | map(select(length > 0) | split("\t")
          | {path: .[0], size: (.[1] | tonumber), sha256: .[2]}))
}' > "$STAGE/manifest.json"
(cd "$STAGE" && jq -r '.files[] | "\(.sha256)  \(.path)"' manifest.json | sha256sum -c --quiet) \
  || fail "the copy does not match its own manifest"
listed=$(jq '.files | length' "$STAGE/manifest.json")
staged=$(find "$STAGE" -type f ! -path "$STAGE/manifest.json" | wc -l)
(( listed == staged && listed == ${#ROWS[@]} )) || fail "the manifest lists $listed files, the copy holds $staged"

echo "copied $NBATCH *.batch files and ${#GUI[@]} GUI scripts, $listed files in all; each matches manifest.json"
printf '%s\n' "${FILES[@]:0:NBATCH}" \
  | awk -F/ '{ k = ($1 == "runs") ? "runs/" : $1 "/" $2 "/"; n[k]++ } END { for (k in n) printf "  %4d under %s\n", n[k], k }' \
  | LC_ALL=C sort -k3
echo "  the $NBATCH batch files: $(cd "$STAGE" && cat -- "${FILES[@]:0:NBATCH}" | wc -c) bytes, $(cd "$STAGE" && cat -- "${FILES[@]:0:NBATCH}" | wc -l) lines"
echo "files with a CR byte: ${#CR[@]}"
if (( ${#CR[@]} )); then printf '  %s\n' "${CR[@]}"; fi
echo "non-empty files without a final LF: ${#NOLF[@]}"
if (( ${#NOLF[@]} )); then printf '  %s\n' "${NOLF[@]}"; fi
echo "*.batch files elsewhere in FuSION 1's tree, not copied: ${#ELSEWHERE[@]}"
if (( ${#ELSEWHERE[@]} )); then printf '  %s\n' "${ELSEWHERE[@]}"; fi

if (( DRY )); then
  echo "dry run: copied and checked in $STAGE, now removed; nothing else written; $PROBLEMS problem(s) would stop the run"
  exit $(( PROBLEMS > 0 ))
fi
[[ ! -e $DEST ]] || fail "$DEST appeared while the script ran"
chmod --reference="$TESTS" "$STAGE"
mv -T "$STAGE" "$DEST"
echo "wrote $DEST"
