// fusion-test ref: the reference runner (section 12, "Reference-test runner"; 12.C5 to 12.C7; the C4 note).
// It runs FuSION 1's reference cases through 2.0, chained (the batch through fusion), pinned (each stage at
// library level on FuSION 1's recorded input) and by verification (single-point checks of a frontier, each
// in its own worker process, C4.2), and judges every difference against expected-differences.json.
// The types are section 12's sketch, with C4.4's additions: Stage::observations, Case::origin and edits,
// Options::cases and modes, and the --check-point worker.
#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/data.hpp"
#include "ref/case.hpp"
#include "ref/expect.hpp"
#include "ref/facts.hpp"

namespace fusion::reftest {

enum class Outcome { match, expected_difference, unexpected_difference, prediction_failed,
                     stale, inconclusive, skipped, not_runnable };
std::string_view name(Outcome) noexcept;
bool failing(Outcome) noexcept;                        // unexpected_difference, prediction_failed, stale, not_runnable

struct Finding {
  std::string case_id, solve_case;                     // solve_case: "in", "out", "free" or empty
  Mode mode = Mode::chained;
  Stage stage = Stage::classify;
  Outcome outcome = Outcome::match;
  std::vector<std::string> entries;
  core::Json detail = core::Json::object();            // "text" is summary.tsv's detail
};

struct Options {
  Tier max_tier = Tier::t1;
  int workers = 1;                                     // cases run at once; verification points at once
  std::optional<testkit::DataRoot> data;               // section 12's sketch has no optional: T1 runs without one
  std::filesystem::path out;                           // the report directory, outside any run directory
  bool record_seen = false;
  std::vector<std::string> cases;                      // C4.4: run these cases only
  std::vector<Mode> modes;                             // C4.4: run these modes only
  std::filesystem::path fusion;                        // the fusion that chained runs start
  std::filesystem::path worker;                        // the fusion-test that verification starts (C4.2)
  std::filesystem::path tests;                         // backend/tests: fixtures/ and reference/
  std::filesystem::path expectations;                  // expected-differences.json, which --record-seen rewrites
  bool keep = false;                                   // keep every case folder, passing or not
};

struct Report {
  std::vector<Finding> findings;
  core::Json meta = core::Json::object();              // runner, versions, tier, cases, edits, times
  int exit_code() const;                               // 1 a failing finding, else 4 all skipped, else 0
};

// load_cases (case.hpp) and load_expectations (expect.hpp) are the sketch's two readers.
// 12.C7: the points that prove a frontier in a box, at most 2k + 1 for k points: each point of F (sat) and
// each inner corner of its staircase (unsat), (n_{i+1} - 1, e_i - 1), with (n_1 - 1, e_hi) and (n_hi, e_k - 1)
// when they lie in the box; (n_hi, e_hi) alone for an empty F.
std::vector<testkit::Point> verification_points(const testkit::Frontier&, testkit::Point box_lo, testkit::Point box_hi);
Report run(std::span<const Case>, std::span<const Entry>, const Options&);
void write(const Report&, const std::filesystem::path& out_dir);   // report.json, summary.tsv

// The subcommand: fusion-test ref [--tier T] [--case ID]... [--mode M]... [--workers N] [--out DIR]
// [--record-seen] [--keep] [--fusion EXE] [--data DIR], or the worker, fusion-test ref --check-point SPEC.
int command(std::span<const std::string> args, std::ostream& out, std::ostream& err);

}  // namespace fusion::reftest
