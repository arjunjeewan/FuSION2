// fusion/testkit/run_facts.hpp: a 2.0 run directory as kit types, from its results.json, run.json and
// copied inputs, for the reference runner's rules and the end-to-end results mode.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/check.hpp"

namespace fusion::testkit {

struct SolveFacts {
  std::string step;                      // "pathz3", "pathz3-2"
  // The solve's corridor: results.json's nodes with their members, completed to the whole corridor from
  // the fb_rch corridor.json that results.json names and the run's network file, which hold the nodes and
  // edges no witness uses (results.json lists only those its witnesses name).
  CanonCorridor corridor;
  struct Case {
    std::string name, status;            // "in", "out", "free"; section 06's case status
    Frontier frontier;
    std::vector<Assignment> witnesses;   // one per frontier point: its first witness
    CaseSpec spec;                       // the case as the run gave it: observations, lists, FSC, families
  };
  std::vector<Case> cases;
};
struct RunFacts { std::string status; int exit = 0; std::vector<SolveFacts> solves; };

RunFacts read_run(const std::filesystem::path& run_dir);   // throws core::Failure

}  // namespace fusion::testkit
