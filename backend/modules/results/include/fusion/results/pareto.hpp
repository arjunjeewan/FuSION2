// fusion/results/pareto.hpp: the built-in Pareto plot step, plot_pareto, drawn by gnuplot through core's
// runner (section 07, "Pareto plot step"; S6.22 to S6.24, 07.C6, F1). The declarations are section 07's
// outline; everything marked "Added" extends it.
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "fusion/core/core.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {
struct ParetoOptions { std::string font = "Liberation Sans"; };
double dodge_offset(std::int32_t edge_upper) noexcept;     // 0.10 * (edge_upper + 1) / 9
std::string gnuplot_script(std::span<const SolveResult* const>, const ParetoOptions&);   // pure, deterministic
void plot_pareto(std::span<const SolveResult* const>, const ParetoOptions&, core::Context&);

// ---- Added beyond the outline ----

// gnuplot's enhanced-text form of a label inside a double-quoted string: _ ^ { } @ & ~ \ and " escaped,
// control characters made spaces.
std::string gnuplot_text(std::string_view label);
}  // namespace fusion::results
