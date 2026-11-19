// fusion/results/json.hpp: the writer and the one reader of results.json (section 07, "results.json
// and the solve's other outputs"; 07.C1, S6.02, E1, 2A.2). The declarations are section 07's outline;
// everything marked "Added" extends it.
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/results/analyse.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {
inline constexpr std::string_view results_format = "fusion.results";
inline constexpr int results_version = 1;
std::string to_json(const SolveResult&, std::span<const Issue>, const std::optional<Comparison>&);
SolveResult from_json(std::string_view);        // throws core::Failure: artefact.newer_format, or a parse error

// ---- Added beyond the outline ----

// The whole file, as from_json() reads it: the result, its issues and its comparison.
struct ResultsFile {
  SolveResult result;
  std::vector<Issue> issues;
  std::optional<Comparison> comparison;
};
ResultsFile read_results(std::string_view json);   // the same checks and failures as from_json()
}  // namespace fusion::results
