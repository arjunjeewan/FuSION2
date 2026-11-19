// results/src/publish.cpp: what the batch layer calls after the solve step returns or stops (1.5):
// results.json, the .dat files and, with legacy_prefix, FuSION 1's files, the frontier on the console,
// the events, and the summary for run.json (section 07, "results.json and the solve's other outputs",
// "Console output and events"; 07.C1, 07.C7, S5.25, S5.31, 2B.7).
#include "fusion/results/publish.hpp"

#include <algorithm>
#include <climits>
#include <set>
#include <utility>
#include <variant>

#include "fusion/results/json.hpp"
#include "fusion/results/legacy.hpp"
#include "text.hpp"

namespace fusion::results {

namespace {

using Json = core::Json;

std::optional<std::string> legacy_prefix(const core::Settings& settings) {
  // 06's pathz3 row owns the setting (solve.legacy_prefix, 1.21); this section writes the files.
  for (const core::Setting& s : settings.all()) {
    if (s.spec == nullptr || s.spec->name != "legacy_prefix" || !s.value) continue;
    if (const auto* text = std::get_if<std::string>(&*s.value); text != nullptr && !text->empty()) return *text;
  }
  return std::nullopt;
}

Json comparison_json(const Comparison& cmp) {
  Json j = Json::object();
  j["verdict"] = verdict_name(cmp.verdict);
  j["delta_n_at_e0"] = cmp.delta_n_at_e0 ? Json(*cmp.delta_n_at_e0) : Json(nullptr);
  j["certified"] = cmp.certified;
  return j;
}

std::string blocking_text(const CaseResult& c) {
  // The undecided cells that block the uncertified points (06.C6), with their labels (S5.19).
  std::set<std::pair<int, int>> cells;
  for (const FrontierPoint& p : c.points) {
    if (p.certified) continue;
    for (const Point b : p.undecided_below) cells.emplace(b.n, b.e);
  }
  if (cells.empty()) return "  * not certified";
  std::vector<std::string> parts;
  for (const auto& [n, e] : cells) {
    Point at;
    at.n = n;
    at.e = e;
    std::string text = point_text(at);
    for (const UndecidedCell& u : c.undecided) {
      if (u.at == at) text += " (" + u.label + ")";
    }
    parts.push_back(std::move(text));
  }
  return std::string("  * not certified: blocked by the undecided ") + (parts.size() == 1 ? "cell " : "cells ") +
         detail::join(parts, ", ");
}

}  // namespace

core::Json solve_summary(const SolveResult& r, const std::optional<Comparison>& cmp) {
  // 2B.7: the solve step's details in run.json, so the frontend lists sessions without results.json.
  Json cases = Json::object();
  for (const CaseResult& c : r.cases) {
    Json cj = Json::object();
    cj["status"] = status_name(c.status);
    cj["points"] = c.points.size();
    cj["certified"] = std::count_if(c.points.begin(), c.points.end(), [](const FrontierPoint& p) { return p.certified; });
    int least = INT_MAX;                        // FuSION 1's "min relaxations" (plot_explanation.py:608-609)
    for (const FrontierPoint& p : c.points) least = std::min(least, p.at.n + p.at.e);
    cj["least_total"] = least == INT_MAX ? Json(nullptr) : Json(least);
    cases[std::string(case_name(c.kind))] = std::move(cj);
  }
  Json results = Json::object();
  results["cases"] = std::move(cases);
  results["comparison"] = cmp ? comparison_json(*cmp) : Json(nullptr);
  Json out = Json::object();
  out["results"] = std::move(results);
  return out;
}

std::vector<std::string> frontier_lines(const SolveResult& r, const std::optional<Comparison>& cmp) {
  // S5.31: FuSION 1's milestones, with points n first and labelled ("(0 ,1) is not dominated" printed
  // (e, n), main.cc:4920-4953).
  std::vector<std::string> lines;
  for (const CaseResult& c : r.cases) {
    std::string head = c.label + ": " + c.status_text;
    const std::uint32_t checks = c.checks.sat + c.checks.unsat + c.checks.unknown;
    if (c.status == CaseStatus::no_point && checks > 0) {
      head += " (" + std::to_string(checks) + (checks == 1 ? " check" : " checks");
      head += c.checks.unsat == checks ? std::string(checks == 1 ? ", unsat)" : ", all unsat)")
                                       : ": " + std::to_string(c.checks.sat) + " sat, " +
                                             std::to_string(c.checks.unsat) + " unsat, " +
                                             std::to_string(c.checks.unknown) + " unknown)";
    }
    lines.push_back(std::move(head));
    if (!c.points.empty()) {
      std::vector<std::string> points;
      bool uncertified = false;
      for (const FrontierPoint& p : c.points) {
        points.push_back(point_text(p.at) + (p.certified ? "" : "*"));
        uncertified = uncertified || !p.certified;
      }
      lines.push_back("  " + detail::join(points, " | "));
      if (uncertified) lines.push_back(blocking_text(c));
    }
    if (!c.requested_points.empty()) {
      std::vector<std::string> points;
      for (const RequestedPoint& q : c.requested_points) {
        points.push_back(point_text(q.at) + " " + (q.label.empty() ? std::string(outcome_name(q.answer)) : q.label));
      }
      lines.push_back("  " + detail::join(points, " | "));
    }
  }
  if (cmp) lines.push_back("comparison: " + comparison_sentence(r, *cmp));
  return lines;
}

core::Json publish_result(SolveResult& r, const solve::Results& results, core::Context& ctx) {
  if (!ctx.step.empty()) r.header.step = std::string(ctx.step);
  std::vector<Issue> issues = analyse(r);
  const std::optional<Comparison> cmp = compare_cases(r);

  // results.json first: an internal error is raised only after it is written, so a long solve is
  // kept for inspection (section 07, checks).
  const std::string json = to_json(r, issues, cmp);
  {
    core::OutputFile file = ctx.files.create("results.json");
    file.out() << json;
    file.commit();
  }
  // The two-column file for every requested case, empty when the case has no point (S5.25, S5.26).
  for (const CaseResult& c : r.cases) {
    if (c.status == CaseStatus::not_requested) continue;
    core::OutputFile file = ctx.files.create(std::string(case_name(c.kind)) + ".dat");
    const std::string text = two_column(c);
    if (!text.empty()) file.out() << text;
    file.commit();
  }
  if (const auto prefix = legacy_prefix(ctx.settings)) write_legacy(r, results, *prefix, ctx);

  for (const std::string& line : frontier_lines(r, cmp)) ctx.log.info(line);
  ctx.log.info("wrote " + r.header.step + "/results.json (" + std::to_string(json.size()) + " bytes)");

  // Events (A2, 1.3): the envelope and step come from core::Events.
  for (const CaseResult& c : r.cases) {
    Json fields = Json::object();
    fields["case"] = case_name(c.kind);
    fields["status"] = status_name(c.status);
    Json points = Json::array();
    for (const FrontierPoint& p : c.points) {
      Json pj = Json::object();
      pj["n"] = p.at.n;
      pj["e"] = p.at.e;
      pj["certified"] = p.certified;
      points.push_back(std::move(pj));
    }
    fields["points"] = std::move(points);
    fields["undecided"] = c.undecided.size();
    ctx.events.emit("results.frontier", std::move(fields));
  }
  if (cmp) ctx.events.emit("results.comparison", comparison_json(*cmp));

  for (const Issue& i : issues) {
    if (i.kind == Issue::Kind::warning) ctx.log.warning(i.code, i.message);
  }
  const auto errors = static_cast<std::size_t>(std::count_if(
      issues.begin(), issues.end(), [](const Issue& i) { return i.kind == Issue::Kind::internal_error; }));
  if (errors > 0) {
    const auto first = std::find_if(issues.begin(), issues.end(),
                                    [](const Issue& i) { return i.kind == Issue::Kind::internal_error; });
    std::string message = first->message;
    if (errors > 1) message += " (and " + std::to_string(errors - 1) + " more in " + r.header.step + "/results.json)";
    throw core::Failure("results.internal", message);
  }
  return solve_summary(r, cmp);
}

core::Json publish_solve(const PublishInputs& in, core::Context& ctx, SolveResult& published) {
  published = build(in);
  return publish_result(published, in.results, ctx);
}

core::Json publish_solve(const PublishInputs& in, core::Context& ctx) {
  SolveResult published;
  return publish_solve(in, ctx, published);
}

}  // namespace fusion::results
