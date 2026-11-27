// An end-to-end case: backend/tests/e2e/cases/<case>/case.json (section 12, "End-to-end tests"; the C5 note's
// "Layout and format"). Section 12's fields, with the C5 note's optional inputs, env, timeout_s, processes,
// actions and pending.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fusion/testkit/e2e.hpp"

namespace fusion::fusiontest::e2e {

struct Expect {
  std::optional<std::string> status;           // run.json's status: ok, failed or stopped (09)
  int exit = 0;                                // fusion's exit status (01.C14)
  std::optional<int> line;                     // a failure's batch line
  std::optional<std::string> stderr_fragment;  // text that stderr must hold
};

struct Action {                                // when an event of this type appears in events.jsonl,
  std::string on_event;                        // run fusion once more with these arguments, the path
  std::vector<std::string> fusion;             // tokens expanded: here a stop request
};

struct Pending {                               // C5.2: a case that waits for an unported feature
  std::string waits_for;                       // the decision, such as 01.C3
  std::string refused_with;                    // the code fusion refuses it with until then
};

struct Case {
  std::string id;                              // "case", which is its folder's name
  std::filesystem::path dir;                   // e2e/cases/<id>
  std::vector<std::string> args;               // fusion's arguments
  Expect expect;
  std::vector<std::string> tags;               // CTest labels, and C6's traceability
  std::vector<testkit::FileRule> compare;      // one per compared output, after the normaliser's own
  std::vector<std::string> inputs;             // shared sets, e2e/inputs/<name>/, copied beside inputs/
  std::vector<std::pair<std::string, std::string>> env;   // for the child; <RUN>, <CASE> and <DATA> expanded
  std::chrono::milliseconds timeout{std::chrono::seconds(120)};
  int processes = 1;                           // fusion processes started at once in one launch directory
  std::vector<Action> actions;
  std::optional<Pending> pending;
};

// Reads and checks <dir>/case.json; throws core::Failure("fusion_test.case") naming the file and the field.
Case load_case(const std::filesystem::path& dir);

}  // namespace fusion::fusiontest::e2e
