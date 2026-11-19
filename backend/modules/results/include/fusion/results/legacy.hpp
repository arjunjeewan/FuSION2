// fusion/results/legacy.hpp: the two-column file and FuSION 1's text formats (section 07, "FuSION 1 text
// formats"; E1, S5.24, S5.25, S5.27, cross-section item 1.21, 01.C11). The declarations are section 07's
// outline.
#pragma once

#include <string>
#include <string_view>

#include "fusion/core/core.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {
std::string two_column(const CaseResult&);                     // the .dat text (S5.25)
std::string legacy_solutions(const SolveResult&, Case);        // S5.27's format
std::string legacy_relaxations(const SolveResult&, Case);
std::string legacy_limits(const solve::CaseResult&);           // FuSION 1's rendering of 06's answers
void write_legacy(const SolveResult&, const solve::Results&, std::string_view legacy_prefix, core::Context&);
}  // namespace fusion::results
