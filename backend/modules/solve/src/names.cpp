// Names, the label rule for unknown answers, the requested cases and pathz3's settings registry
// (section 06, "Settings", 06.C2, 06.C10).
#include "fusion/solve/solve.hpp"

#include <array>

namespace fusion::solve {

std::string_view name(Case c) noexcept {
  switch (c) {
    case Case::in: return "in";
    case Case::out: return "out";
    case Case::free: return "free";
  }
  return "free";
}

std::string_view name(CaseStatus s) noexcept {
  switch (s) {
    case CaseStatus::complete: return "complete";
    case CaseStatus::no_point: return "no_point";
    case CaseStatus::infeasible: return "infeasible";
    case CaseStatus::no_de_gene: return "no_de_gene";
    case CaseStatus::stopped: return "stopped";
    case CaseStatus::budget_exhausted: return "budget_exhausted";
    case CaseStatus::not_requested: return "not_requested";
  }
  return "complete";
}

std::string_view name(Role r) noexcept {
  switch (r) {
    case Role::counted: return "counted";
    case Role::forced: return "forced";
    case Role::exempt_endpoint: return "exempt_endpoint";
    case Role::removed_with_fsc: return "removed_with_fsc";
    case Role::outside_encoding: return "outside_encoding";
    case Role::outside_corridor: return "outside_corridor";
    case Role::not_in_network: return "not_in_network";
  }
  return "counted";
}

std::string_view name(Phase p) noexcept {
  switch (p) {
    case Phase::diagonal: return "diagonal";
    case Phase::smallest_n: return "smallest_n";
    case Phase::smallest_e: return "smallest_e";
    case Phase::point: return "point";
    case Phase::extra: return "extra";
  }
  return "diagonal";
}

std::string_view name(Outcome o) noexcept {
  switch (o) {
    case Outcome::sat: return "sat";
    case Outcome::unsat: return "unsat";
    case Outcome::timeout: return "timeout";
    case Outcome::undefined: return "undefined";
    case Outcome::stopped: return "stopped";
  }
  return "undefined";
}

std::string_view name(Source s) noexcept {
  switch (s) {
    case Source::z3: return "z3";
    case Source::cache: return "cache";
    case Source::model: return "model";
    case Source::transfer: return "transfer";
    case Source::imported: return "imported";
  }
  return "z3";
}

std::optional<Case> parse_case(std::string_view text) noexcept {
  if (text == "in") return Case::in;
  if (text == "out") return Case::out;
  if (text == "free") return Case::free;
  return std::nullopt;
}

// 06.C2 replaces FuSION 1's rule, which also called a check TIMEOUT when its wall time reached 99 % of
// the overall timeout on a one-second clock (Linux/src/GraphManagerNew.cc:29342-29347). The reason
// is read before pop, where Z3 still reports "timeout" (FuSION 1 read it after pop, 29461-29477).
Outcome unknown_outcome(std::string_view reason, bool rlimit_set, bool interrupted) noexcept {
  if (interrupted) return Outcome::stopped;
  if (reason.find("timeout") != std::string_view::npos) return Outcome::timeout;
  if (rlimit_set && (reason.find("resource limit") != std::string_view::npos || reason == "canceled"))
    return Outcome::timeout;
  return Outcome::undefined;
}

// 06.C10: with an FSC the cases are in and out; without one, free (S8.12's "single solve").
// Asked cases come back in the canonical order in, out, free, each once.
std::vector<Case> requested_cases(const Settings& settings) {
  if (settings.cases.empty()) {
    if (settings.fsc.empty()) return {Case::free};
    return {Case::in, Case::out};
  }
  std::vector<Case> out;
  for (Case c : {Case::in, Case::out, Case::free})
    for (Case asked : settings.cases)
      if (asked == c) { out.push_back(c); break; }
  return out;
}

namespace {

using core::Need;
using core::OnResume;
using core::SettingSpec;
using core::Type;

constexpr std::array<std::string_view, 3> case_choices{"in", "out", "free"};
constexpr std::array<std::string_view, 3> retry_choices{"automatic", "unknown", "none"};

constexpr SettingSpec list_file(std::string_view name, std::string_view help) {
  return SettingSpec{.name = name, .module = "solve", .type = Type::input_file, .need = Need::optional,
                     .help = help};
}

constexpr SettingSpec bound(std::string_view name, std::string_view value, std::string_view help) {
  return SettingSpec{.name = name, .module = "solve", .type = Type::integer, .need = Need::defaulted,
                     .default_text = value, .on_resume = OnResume::may_change, .min = 0, .help = help};
}

// Section 06's settings table, in its order (1.7, 1.16).
const std::array<SettingSpec, 31> registry{{
    {.name = "fsc", .module = "solve", .type = Type::text, .need = Need::optional,
     .help = "one or more gene ids forced in or out together (S5.17, 06.C10)"},
    {.name = "cases", .module = "solve", .type = Type::choice, .need = Need::computed,
     .choices = case_choices,
     .help = "in, out, free; in,out with an FSC and free without one (06.C10)"},
    bound("node_min", "0", "lower node-relaxation bound of the box (06.C17)"),
    bound("node_max", "8", "upper node-relaxation bound of the box (06.C17)"),
    bound("edge_min", "0", "lower edge-relaxation bound of the box (06.C17)"),
    bound("edge_max", "8", "upper edge-relaxation bound of the box (06.C17)"),
    {.name = "points", .module = "solve", .type = Type::text, .need = Need::optional,
     .on_resume = OnResume::may_change, .help = "n,e points joined by ';' asked without a search (06.C9)"},
    {.name = "timeout", .module = "solve", .type = Type::duration, .need = Need::defaulted,
     .default_text = "4h", .on_resume = OnResume::may_change, .min = 0,
     .help = "Z3's timeout per query (06.C2)"},
    {.name = "incremental_timeout", .module = "solve", .type = Type::duration, .need = Need::defaulted,
     .default_text = "1h", .on_resume = OnResume::may_change, .min = 0,
     .help = "the incremental phase's limit inside each query (06.C2)"},
    {.name = "budget", .module = "solve", .type = Type::duration, .need = Need::defaulted,
     .default_text = "0", .on_resume = OnResume::may_change, .min = 0,
     .help = "wall time for the whole solve, 0 for none (06.C3)"},
    {.name = "rlimit", .module = "solve", .type = Type::integer, .need = Need::defaulted,
     .default_text = "0", .on_resume = OnResume::may_change, .min = 0,
     .help = "Z3 resource steps per check, 0 for none (2B.8)"},
    {.name = "seed", .module = "solve", .type = Type::integer, .need = Need::defaulted,
     .default_text = "42", .min = 0, .help = "Z3's random seed (S5.22)"},
    {.name = "solutions", .module = "solve", .type = Type::integer, .need = Need::defaulted,
     .default_text = "1", .on_resume = OnResume::may_change, .min = 1,
     .help = "witnesses kept per frontier point (06.C11)"},
    {.name = "threads", .module = "solve", .type = Type::integer, .need = Need::defaulted,
     .default_text = "0", .affects_results = false, .on_resume = OnResume::may_change, .min = 0,
     .help = "cases solved at once, 0 for one per case (06.C4)"},
    {.name = "retry", .module = "solve", .type = Type::choice, .need = Need::defaulted,
     .default_text = "automatic", .on_resume = OnResume::may_change, .choices = retry_choices,
     .help = "which unknown answers a resume asks again (06.C8)"},
    {.name = "answers_from", .module = "solve", .type = Type::text, .need = Need::optional,
     .on_resume = OnResume::may_change, .help = "an earlier solve's step directory (06.C12)"},
    list_file("essential_nodes", "nodes present in every witness (S5.05)"),
    list_file("avoid_nodes", "genes absent, with every complex that contains one (S5.05, entry 229)"),
    list_file("active_nodes", "nodes active in every witness (S5.05)"),
    list_file("inactive_nodes", "nodes inactive in every witness (S5.05)"),
    list_file("nonrelaxed_nodes", "observations held consistent (S5.05)"),
    list_file("confirmed_up", "genes counted as up-regulated (S5.07)"),
    list_file("confirmed_down", "genes counted as down-regulated (S5.07)"),
    list_file("essential_edges", "edges selected in every witness (S5.06)"),
    list_file("avoid_edges", "edges never selected (S5.06)"),
    list_file("nonrelaxed_edges", "edges never relaxed (S5.06)"),
    list_file("coexpression_csv", "coexpression pairs; columns found by header (S5.15)"),
    {.name = "coexpression_threshold", .module = "solve", .type = Type::real, .need = Need::optional,
     .help = "pairs scoring strictly above it keep equal activity (S5.15)"},
    {.name = "frozen_threshold", .module = "solve", .type = Type::real, .need = Need::optional,
     .help = "edges scoring strictly above it are never relaxed (S5.16)"},
    {.name = "constraints", .module = "solve", .type = Type::boolean, .need = Need::optional,
     .help = "0 turns coexpression and frozen edges off; absent, the inputs decide (S5.03)"},
    {.name = "legacy_prefix", .module = "solve", .type = Type::output_name, .need = Need::optional,
     .affects_results = false, .on_resume = OnResume::may_change,
     .help = "writes FuSION 1's text files at this stem (1.21)"},
}};

}  // namespace

std::span<const core::SettingSpec> settings() { return registry; }

}  // namespace fusion::solve
