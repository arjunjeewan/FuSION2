# 0024 · The spec cites a frozen snapshot of FuSION 1

- **Status:** Accepted. It amends [decision 0012](0012-spec-reference-tree.md).
- **Date:** 2026-11-10
- **Asked in:** prompt log entry 036, Q1

## Context

[Decision 0012](0012-spec-reference-tree.md) made FuSION 1's Linux working tree the reference. On 2026-11-09 at 19:16 another session changed that tree, fixing the UNDEFINED/TIMEOUT labelling of Z3 results:

- `Linux/src/main.cc` gained one line at 4733 (in `macOS/src/main.cc`, at 4761);
- `GraphManagerNew.cc` gained an initialisation at 8180, a setter and a getter from 8216, and a helper at 29342, in both trees;
- `Linux/include/GraphManagerNew.h` gained a member at 501-505.

Later line numbers therefore moved. The behaviour described in stages 1 to 4 did not change. Stage 5's lead about UNDEFINED labels did.

## Options

1. Pin a snapshot of FuSION 1's current source files that every stage cites (recommended)
2. Follow the live tree, and re-check the affected citations each time it changes

## Decision

Option 1. The reference is `/home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10`. It holds FuSION 1's source files as copied on 2026-11-10: 352 files, 12 MB, with a `README.txt` and a `MANIFEST.sha256`, all read-only. Its paths match the FuSION 1 tree. Run data under `FuSION/runs/` and the KGML data directories are still read from the live tree.

## Consequences

- Stage 1 and stage 2 citations were moved to the snapshot by rule (`main.cc` +1 from line 4733; `GraphManagerNew.cc` +1 from 8180 and +11 from 8216; `macOS/src/main.cc` +1 from 4761) and checked against it.
- A later FuSION 1 change reaches the spec only if the user asks for a new snapshot.
