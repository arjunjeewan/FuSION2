// What the reference runner's modes share (internal to fusion-test ref): a case set up in its folder, the
// comparisons per stage and their judging (rules.cpp), and each mode's entry points (chained.cpp,
// pinned.cpp, verify.cpp).
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "fusion/corridor/corridor.hpp"
#include "fusion/solve/solve.hpp"
#include "fusion/testkit/e2e.hpp"
#include "ref/ref.hpp"

namespace fusion::reftest {

// One case in one mode's folder: the folder prepared (or, for a verification worker, opened), its batch
// read, and its FuSION 1 solves.
struct CaseSetup {
  const Case* kase = nullptr;
  Roots roots;
  std::filesystem::path artefacts;                     // the case's FuSION 1 artefacts (cases.json's fusion1 paths)
  std::filesystem::path f1_run;                        // FuSION 1's run folder: the batch's write folder as it ran
  Prepared prepared;
  BatchView batch;
  std::vector<Arm> arms;
};
CaseSetup set_up(const Case&, const Roots&, const std::filesystem::path& case_dir);    // prepares the folder
CaseSetup open_setup(const Case&, const Roots&, const std::filesystem::path& case_dir);   // a folder prepared before

// ---- comparisons (rules.cpp)
struct Comparison {                                    // one stage, or one solve case of it, in one mode
  Stage stage = Stage::classify;
  std::string solve_case;
  std::vector<std::string> differing;                  // facts that differ; solve.r1, r2, r3 when a rule fails
  bool inconclusive = false;                           // an unknown answer on either side (R2 to R4)
  std::optional<std::string> not_runnable;             // why the stage could not be compared
  core::Json detail = core::Json::object();
};
// FuSION 1's facts against 2.0's, at the given stages: canonical forms per stage, rules R1 to R4 for solves.
std::vector<Comparison> compare(const Facts& fusion1, const Facts& fusion2, std::span<const Stage> stages);
// The outcome of each comparison under the expected differences that apply, their predictions, and in
// chained mode the cascade of the differences upstream (section 12, "Modes").
std::vector<Finding> judge(std::span<const Comparison>, const Case&, Mode, std::span<const Entry>, const Signals&);

// ---- pinned (pinned.cpp): each stage at library level on FuSION 1's recorded input
struct PinnedBase;                                     // the maps, FuSION 1's observations and exceptions, the corridor
std::shared_ptr<PinnedBase> pinned_base(const CaseSetup&, core::Context&);
struct PinnedSolve {                                   // one arm's solve, as the pinned solve and verification build it
  solve::Settings settings;
  std::variant<solve::Problem, solve::Skipped> problem;
  testkit::CaseSpec spec;                              // the same case in ids, for the kit's checker
  testkit::CanonCorridor corridor;                     // the corridor it is solved on
  std::shared_ptr<const corridor::Corridor> source;    // and as the module holds it, for witnesses' keys
};
PinnedSolve pinned_solve(const PinnedBase&, const CaseSetup&, const Arm&, core::Context&);
// Every stage but the solve unless with_solve; the stages that fail are recorded in Facts::missing.
Facts pinned_facts(const CaseSetup&, bool with_solve, const std::filesystem::path& work);

// ---- chained (chained.cpp): the batch through fusion
struct ChainedRun {
  testkit::RunResult result;
  std::filesystem::path run_dir;                       // where its files are
  std::filesystem::path ran_at;                        // the run directory it ran in, which its files name
  double seconds = 0;
};
ChainedRun run_chained(const CaseSetup&, const std::filesystem::path& fusion);
// C4.3: two chained runs of one case, file by file through the kit's normaliser (12, Determinism, "Repeat").
std::vector<std::string> repeat_differences(const ChainedRun& first, const std::filesystem::path& first_case,
                                            const ChainedRun& second, const std::filesystem::path& second_case,
                                            const std::optional<testkit::DataRoot>&);

// ---- verify (verify.cpp): 12.C7 with C4.2's worker
// One comparison per solve case of the case's FuSION 1 frontier: proven, or judged by R1 to R4 after a full
// pinned solve where a point disagrees (section 12, "Frontier verification").
std::vector<Comparison> run_verify(const CaseSetup&, const Facts& fusion1, const Options&, const std::filesystem::path& cases_json);
int check_point(const std::filesystem::path& spec, std::ostream& out, std::ostream& err);   // the worker

}  // namespace fusion::reftest
