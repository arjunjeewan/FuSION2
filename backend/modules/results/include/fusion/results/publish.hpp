// fusion/results/publish.hpp: what the batch layer calls after the solve step returns or stops (1.5)
// (section 07, "results.json and the solve's other outputs", "Console output and events"). The first
// declaration is section 07's outline; everything marked "Added" extends it.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/results/analyse.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {
// Called by the batch layer after the solve step returns or stops (1.5): build(), analyse(),
// compare_cases(), then results.json, the .dat files and, with legacy_prefix, FuSION 1's files,
// all through ctx.files (OutputFile); prints the frontier and emits the events below.
// Returns the summary for the step's run.json details (2B.7). Throws core::Failure
// (results.internal) after writing if analyse() found an internal error.
core::Json publish_solve(const PublishInputs&, core::Context& solve_step);

// ---- Added beyond the outline ----

// The same, also handing back the published form, which the run's later pex and plot_pareto steps
// draw from memory (07.C9).
core::Json publish_solve(const PublishInputs&, core::Context& solve_step, SolveResult& published);

// The same for a SolveResult already built (tests; a re-publication of a loaded solve).
core::Json publish_result(SolveResult&, const solve::Results&, core::Context& solve_step);

// The solve step's details for run.json (2B.7): {"results": {"cases": {...}, "comparison": {...}}}.
core::Json solve_summary(const SolveResult&, const std::optional<Comparison>&);

// The frontier lines publish_solve() prints (S5.31): per case its label and display words, its points
// n first, the uncertified ones starred and explained, then the comparison sentence.
std::vector<std::string> frontier_lines(const SolveResult&, const std::optional<Comparison>&);
}  // namespace fusion::results
