// fusion/results/analyse.hpp: the checks run on every SolveResult before anything is written, the
// sign-consistent routes (S5.13) and the FSC comparison (07.C7) (section 07, "Checks and analysis").
// The declarations up to compare_cases() are section 07's outline; everything marked "Added" extends it.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fusion/results/model.hpp"

namespace fusion::results {

struct Issue {
  enum class Kind : std::uint8_t { internal_error, warning } kind = Kind::internal_error;
  std::string code, message;
  bool operator==(const Issue&) const = default;
};
std::vector<Issue> analyse(SolveResult&);       // checks, then routes

enum class Verdict : std::uint8_t {
  identical, with_cheaper, with_costlier, mixed, essential, with_impossible, no_points
};
struct Comparison {
  Verdict verdict = Verdict::no_points;
  std::optional<std::int32_t> delta_n_at_e0;
  bool certified = false;
  bool operator==(const Comparison&) const = default;
};
std::optional<Comparison> compare_cases(const SolveResult&);   // only when in and out were both solved (07.C7)

// ---- Added beyond the outline ----

std::string_view verdict_name(Verdict) noexcept;                 // "identical", "with_cheaper", ...
std::optional<Verdict> parse_verdict(std::string_view) noexcept;
std::string_view issue_kind_name(Issue::Kind) noexcept;          // "internal_error", "warning"

// The comparison in words, for the console, the run page and the site card: "forcing JUN in saves
// 1 node at e=0 (not certified)".
std::string comparison_sentence(const SolveResult&, const Comparison&);

// The sign-consistent routes alone (S5.13), for one witness: what analyse() stores in Witness::route.
Route sign_consistent_route(const SolveResult&, Case, const Witness&);

}  // namespace fusion::results
