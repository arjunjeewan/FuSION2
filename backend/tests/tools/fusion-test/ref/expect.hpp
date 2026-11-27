// The expected differences (section 12, "Expected differences"; 12.C6; the C4 note's expect.cpp): the
// recorded list of decision 0023, each entry with its predicate, the facts it allows, an optional checked
// prediction and the cases it is seen in.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fusion/core/core.hpp"
#include "ref/facts.hpp"

namespace fusion::reftest {

struct Entry {                                         // one expected difference
  std::string id, why;
  std::vector<std::string> spec, allows, seen_in;      // seen_in: "<case>" or "<case>:<solve case>"
  std::vector<Stage> stages;
  core::Json applies_when;                             // {"<predicate>": <argument>}, from a closed list
  std::optional<core::Json> predicts;                  // {"rule": ...}, or a list of them
};

// Throws core::Failure("ref.expectations") for an unreadable file, an unknown predicate, fact, stage or
// prediction rule, or a malformed entry: the harness cannot run, exit code 2.
std::vector<Entry> load_expectations(const std::filesystem::path& expected_differences_json);

// The facts an entry may allow, "<stage>.<fact>" (section 12's comparison table, row by row).
const std::vector<std::string>& known_facts();

// Whether the entry applies to the case, at one of its solve cases ("in", "out", "free") or, with an empty
// solve case, at a case-level stage, where a per-solve predicate holds if it holds for any solve case.
bool applies(const Entry&, const Signals&, const std::string& solve_case);

// The entry's predictions as a list, each {"rule": ...}.
std::vector<core::Json> predictions(const Entry&);
// The stages an entry's unchanged prediction names: its difference must not reach them (XD-21).
std::vector<Stage> unchanged_stages(const Entry&);

// The file's text with each entry's seen_in replaced, written in the file's own layout so a review sees
// only the lists that changed (--record-seen).
std::string with_seen(const std::filesystem::path& expected_differences_json, const std::vector<Entry>& entries);
// The layout itself: one key per line, values on one line.
std::string layout(const core::Json& document);

}  // namespace fusion::reftest
