// fusion-test e2e: runs the end-to-end cases (section 12, "End-to-end tests"; the C3 note's "The e2e engine";
// the C5 note's case format). For each case: a fresh ScratchDir holding the case's files (all but case.json
// and expected/) and its shared input sets; fusion run there through testkit::run_fusion, with --run-dir run
// appended unless the case's arguments place the run (--run-dir, --root), stdin from the null device and the
// case's timeout; then, in order:
//   1. the exit status, run.json's status, and for a failure the batch line and the stderr fragment;
//   2. no two files of the run directory differ only in case (12, Platforms);
//   3. testkit::compare_tree(run, expected/, normaliser): testkit::fusion_normaliser plus the case's rules. A
//      case whose expected/ is empty or absent compares no file (C5.1; prompt log entry 362).
// With processes N above 1, N fusion processes start at once with no --run-dir, each naming its own run
// directory in the launch directory; each process and each of the N run directories is checked, and each
// run is compared with the one expected/ (entry 362). A failing case keeps its scratch folder and prints its
// path; a passing one is removed. --update writes the normalised files the rules match into expected/ instead
// of comparing, once 1 and 2 hold, from the first run directory (the others are compared with it), and says
// what changed. A pending case (C5.2) must still be refused with its code: it is then skipped, and fails as
// stale once fusion runs it.
#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "e2e/case.hpp"
#include "fusion/testkit/data.hpp"

namespace fusion::fusiontest::e2e {

struct Settings {
  std::filesystem::path fusion;                 // the fusion each case runs
  std::optional<testkit::DataRoot> data;        // for the <DATA> token, when FUSION_TEST_DATA or --data is set
  std::filesystem::path root;                   // backend/tests/e2e: cases/ and the shared inputs/
  bool keep = false;                            // --keep: every case's scratch folder stays
};

struct CaseResult {
  enum class Outcome { passed, failed, skipped, harness };   // harness: the case could not be run (exit 2)
  Outcome outcome = Outcome::passed;
  std::vector<std::string> notes;               // why it failed or was skipped; what --update changed
  std::filesystem::path scratch;                // the case's launch directory, <CASE>
  bool kept = false;
};

CaseResult run_case(const Case&, const Settings&, bool update);

// The selected cases (every case under <root>/cases/ when ids and update are both empty), each reported on
// out. Exit code: 2 if a case could not be run, else 1 if one failed, else 4 if every case was skipped, else 0.
int run(const Settings&, std::span<const std::string> ids, std::span<const std::string> update, std::ostream& out,
        std::ostream& err);
// --list: one line per case, "<id>\t<state>\t<tag>,<tag>...", state being golden, no golden or pending.
int list(const Settings&, std::span<const std::string> ids, std::ostream& out, std::ostream& err);

// The paths under a folder that differ only in ASCII case, each pair as "a | b", sorted.
std::vector<std::string> case_clashes(const std::filesystem::path& dir);

}  // namespace fusion::fusiontest::e2e
