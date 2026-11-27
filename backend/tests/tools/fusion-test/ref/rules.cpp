// The comparisons (section 12, "Comparison levels" and "Witness checker and re-scoring"; 12.C5): a rule per
// stage on the kit's canonical facts, the witness rules R1 to R4 through testkit::check, and the judging of
// every difference under the expected differences that apply (12.C6).
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "ref/runner.hpp"

namespace fusion::reftest {

using core::Json;
using testkit::CheckOutcome;
using testkit::Point;

namespace {

Json point_json(Point p) { return Json::array({p.n, p.e}); }
Json points_json(const std::vector<Point>& ps) {
  Json out = Json::array();
  for (Point p : ps) out.push_back(point_json(p));
  return out;
}
std::string point_text(Point p) { return "(" + std::to_string(p.n) + ", " + std::to_string(p.e) + ")"; }
bool below(Point a, Point b) { return a.n <= b.n && a.e <= b.e; }

bool proven_sat(const Answers& answers, Point p) {
  return std::any_of(answers.begin(), answers.end(), [&](const auto& a) { return a.second == CheckOutcome::sat && below(a.first, p); });
}
bool proven_unsat(const Answers& answers, Point p) {
  return std::any_of(answers.begin(), answers.end(), [&](const auto& a) { return a.second == CheckOutcome::unsat && below(p, a.first); });
}
bool undecided(const Answers& answers, Point p) { return !proven_sat(answers, p) && !proven_unsat(answers, p); }

Json set_difference_json(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  const std::set<std::string> sa(a.begin(), a.end()), sb(b.begin(), b.end());
  Json only_a = Json::array(), only_b = Json::array();
  for (const std::string& x : sa)
    if (!sb.contains(x) && only_a.size() < 20) only_a.push_back(x);
  for (const std::string& x : sb)
    if (!sa.contains(x) && only_b.size() < 20) only_b.push_back(x);
  return Json{{"only_fusion1", only_a}, {"only_fusion2", only_b}};
}
bool same_set(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  return std::set<std::string>(a.begin(), a.end()) == std::set<std::string>(b.begin(), b.end());
}

Comparison missing(Stage stage, const Facts& f2, const std::string& solve_case = {}) {
  Comparison c;
  c.stage = stage;
  c.solve_case = solve_case;
  const auto it = f2.missing.find(stage);
  c.not_runnable = it != f2.missing.end() ? it->second : "2.0 has no " + std::string(name(stage)) + " facts";
  return c;
}

// ---- the solve: R1 to R4 on one FuSION 1 solve case and 2.0's
Comparison solve_rules(const SolveFacts& f1, const SolveFacts& f2) {
  Comparison c;
  c.stage = Stage::solve;
  c.solve_case = f1.name;
  Json& d = c.detail;
  d["fusion1"] = points_json(f1.frontier.points);
  d["fusion2"] = points_json(f2.frontier.points);
  d["status"] = f2.status;
  const auto certified = [](const testkit::Frontier& f) {
    std::vector<Point> out;
    for (std::size_t k = 0; k < f.points.size(); ++k)
      if (k < f.certified.size() && f.certified[k]) out.push_back(f.points[k]);
    return out;
  };
  const std::vector<Point> c1 = certified(f1.frontier), c2 = certified(f2.frontier);
  if (c1.size() != f1.frontier.points.size()) d["fusion1_uncertified"] = points_json([&] {
    std::vector<Point> u;
    for (Point p : f1.frontier.points)
      if (std::find(c1.begin(), c1.end(), p) == c1.end()) u.push_back(p);
    return u;
  }());
  std::vector<std::string> inconclusive;

  // R1: every 2.0 witness passes under fusion2 on its own corridor and costs exactly its point (06.C20).
  Json r1 = Json::array();
  int valid = 0;
  for (std::size_t k = 0; k < f2.frontier.points.size(); ++k) {
    const Point p = f2.frontier.points[k];
    if (k >= f2.witnesses.size() || !f2.witnesses[k]) {
      r1.push_back(Json{{"point", point_json(p)}, {"why", "no witness"}});
      continue;
    }
    const testkit::Verdict v = testkit::check(f2.corridor, f2.spec, *f2.witnesses[k], testkit::Semantics::fusion2);
    if (v.valid && v.cost == p) {
      ++valid;
    } else {
      r1.push_back(Json{{"point", point_json(p)},
                        {"why", v.valid ? "costs " + point_text(v.cost) : (v.violations.empty() ? std::string("invalid") : v.violations.front())}});
    }
  }
  d["r1"] = Json{{"valid", valid}, {"witnesses", f2.frontier.points.size()}, {"failed", r1}};
  if (!r1.empty()) c.differing.push_back("solve.r1");

  // R4: the certified points of both frontiers, and every definite answer both sides gave at one point.
  std::vector<Point> moved;
  for (Point p : c1)
    if (std::find(f2.frontier.points.begin(), f2.frontier.points.end(), p) == f2.frontier.points.end()) {
      if (undecided(f2.answers, p)) inconclusive.push_back("FuSION 1's " + point_text(p) + " is undecided in 2.0's answers");
      else moved.push_back(p);
    }
  for (Point p : c2)
    if (std::find(f1.frontier.points.begin(), f1.frontier.points.end(), p) == f1.frontier.points.end()) {
      if (undecided(f1.answers, p)) inconclusive.push_back("2.0's " + point_text(p) + " is undecided in FuSION 1's answers");
      else moved.push_back(p);
    }
  for (Point p : f1.frontier.points)
    if (std::find(c1.begin(), c1.end(), p) == c1.end() &&
        std::find(f2.frontier.points.begin(), f2.frontier.points.end(), p) == f2.frontier.points.end())
      inconclusive.push_back("FuSION 1's " + point_text(p) + " is uncertified");
  for (std::size_t k = 0; k < f2.frontier.points.size(); ++k)
    if ((k >= f2.frontier.certified.size() || !f2.frontier.certified[k]) &&
        std::find(f1.frontier.points.begin(), f1.frontier.points.end(), f2.frontier.points[k]) == f1.frontier.points.end())
      inconclusive.push_back("2.0's " + point_text(f2.frontier.points[k]) + " is uncertified");
  std::sort(moved.begin(), moved.end());
  moved.erase(std::unique(moved.begin(), moved.end()), moved.end());
  if (!moved.empty()) {
    c.differing.push_back("solve.frontier");
    d["moved"] = points_json(moved);
  }
  std::map<Point, CheckOutcome> first_answer;
  for (const auto& [p, o] : f1.answers)
    if (o != CheckOutcome::unknown && o != CheckOutcome::not_checked) first_answer.emplace(p, o);
  Json disagree = Json::array();
  int compared = 0;
  std::set<Point> seen;
  for (const auto& [p, o] : f2.answers) {
    if (o == CheckOutcome::unknown || o == CheckOutcome::not_checked || !seen.insert(p).second) continue;
    const auto it = first_answer.find(p);
    if (it == first_answer.end()) continue;
    ++compared;
    if (it->second != o) disagree.push_back(point_json(p));
  }
  d["answers"] = Json{{"compared", compared}, {"disagree", disagree}};
  if (!disagree.empty()) c.differing.push_back("solve.answers");
  if (f1.dat && f2.dat) {
    d["dat_equal"] = *f1.dat == *f2.dat;
    if (*f1.dat != *f2.dat && moved.empty()) c.differing.push_back("solve.dat");
  }

  // XD-11: a FuSION 1 frontier point without a witness (its re-solve timed out) where 2.0 has one.
  Json gained = Json::array();
  for (std::size_t k = 0; k < f2.frontier.points.size(); ++k) {
    const Point p = f2.frontier.points[k];
    const auto r = std::find_if(f1.records.begin(), f1.records.end(), [&](const testkit::f1::PointRecord& x) { return x.point == p; });
    if (r != f1.records.end() && !r->witness && k < f2.witnesses.size() && f2.witnesses[k]) gained.push_back(point_json(p));
  }
  if (!gained.empty()) {
    c.differing.push_back("solve.witness");
    d["witness_gained"] = gained;
  }

  // R2: each FuSION 1 witness, re-scored under fusion2 on 2.0's corridor, is weakly dominated by 2.0's frontier.
  Json r2_na = Json::array(), unmet = Json::array();
  std::vector<Point> bound;
  int r2_ok = 0;
  for (const testkit::f1::PointRecord& r : f1.records) {
    if (!r.witness) continue;
    const testkit::Assignment a = testkit::from_f1(f2.corridor, *r.witness);
    const testkit::Verdict v = testkit::check(f2.corridor, f2.spec, a, testkit::Semantics::fusion2);
    if (!v.valid) {
      r2_na.push_back(Json{{"point", point_json(r.point)}, {"why", v.violations.empty() ? std::string("invalid") : v.violations.front()}});
      continue;
    }
    bound.push_back(v.cost);
    if (std::any_of(f2.frontier.points.begin(), f2.frontier.points.end(), [&](Point p) { return below(p, v.cost); })) {
      ++r2_ok;
    } else if (undecided(f2.answers, v.cost)) {
      inconclusive.push_back("R2: " + point_text(v.cost) + " is undecided in 2.0's answers");
    } else {
      unmet.push_back(Json{{"point", point_json(r.point)}, {"rescored", point_json(v.cost)}});
    }
  }
  std::vector<Point> pareto;
  for (Point p : bound)
    if (std::none_of(bound.begin(), bound.end(), [&](Point q) { return q != p && below(q, p); })) pareto.push_back(p);
  std::sort(pareto.begin(), pareto.end());
  pareto.erase(std::unique(pareto.begin(), pareto.end()), pareto.end());
  d["r2"] = Json{{"dominated", r2_ok}, {"unmet", unmet}, {"not_applicable", r2_na}, {"bound", points_json(pareto)}};
  if (!unmet.empty()) c.differing.push_back("solve.r2");

  // R3: each 2.0 witness, scored under fusion1 on FuSION 1's corridor, is weakly dominated by a certified point.
  Json r3_na = Json::array(), r3_fail = Json::array();
  int r3_ok = 0;
  for (std::size_t k = 0; k < f2.frontier.points.size() && k < f2.witnesses.size(); ++k) {
    if (!f2.witnesses[k]) continue;
    const testkit::Verdict v = testkit::check(f1.corridor, f1.spec, *f2.witnesses[k], testkit::Semantics::fusion1);
    if (!v.valid) {
      r3_na.push_back(Json{{"point", point_json(f2.frontier.points[k])},
                           {"why", v.violations.empty() ? std::string("invalid") : v.violations.front()}});
      continue;
    }
    if (std::any_of(c1.begin(), c1.end(), [&](Point p) { return below(p, v.cost); })) {
      ++r3_ok;
    } else if (std::any_of(f1.frontier.points.begin(), f1.frontier.points.end(), [&](Point p) { return below(p, v.cost); }) ||
               undecided(f1.answers, v.cost)) {
      inconclusive.push_back("R3: " + point_text(v.cost) + " is undecided in FuSION 1's answers");
    } else {
      r3_fail.push_back(Json{{"point", point_json(f2.frontier.points[k])}, {"fusion1_cost", point_json(v.cost)}});
    }
  }
  d["r3"] = Json{{"dominated", r3_ok}, {"failed", r3_fail}, {"not_applicable", r3_na}};
  if (!r3_fail.empty()) c.differing.push_back("solve.r3");

  if (!inconclusive.empty()) {
    c.inconclusive = true;
    d["inconclusive"] = inconclusive;
  }
  const std::size_t n1 = f1.frontier.points.size(), n2 = f2.frontier.points.size();
  std::string text = moved.empty() && n1 == n2 ? std::to_string(n1) + " point" + (n1 == 1 ? "" : "s") + " equal"
                                               : "FuSION 1 " + std::to_string(n1) + ", 2.0 " + std::to_string(n2) + " points, " +
                                                     std::to_string(moved.size()) + " moved";
  text += "; R1 " + std::to_string(valid) + "/" + std::to_string(n2) + ", R2 " + std::to_string(r2_ok) + "/" +
          std::to_string(r2_ok + unmet.size() + r2_na.size()) + ", R3 " + std::to_string(r3_ok) + "/" +
          std::to_string(r3_ok + r3_fail.size() + r3_na.size());
  d["text"] = text;
  return c;
}

const SolveFacts* solve_named(const Facts& f, const std::string& kase) {
  for (const SolveFacts& s : f.solves)
    if (s.name == kase) return &s;
  return nullptr;
}

}  // namespace

std::vector<Comparison> compare(const Facts& f1, const Facts& f2, std::span<const Stage> stages) {
  std::vector<Comparison> out;
  const auto wanted = [&](Stage s) { return std::find(stages.begin(), stages.end(), s) != stages.end(); };
  if (wanted(Stage::classify) && f1.up) {
    if (!f2.up) {
      out.push_back(missing(Stage::classify, f2));
    } else {
      Comparison c;
      c.stage = Stage::classify;
      const bool same = same_set(*f1.up, *f2.up) && same_set(*f1.down, *f2.down);
      if (!same) c.differing.push_back("classify.lists");
      c.detail = Json{{"fusion1", Json::array({f1.up->size(), f1.down->size()})},
                      {"fusion2", Json::array({f2.up->size(), f2.down->size()})},
                      {"up", set_difference_json(*f1.up, *f2.up)}, {"down", set_difference_json(*f1.down, *f2.down)}};
      c.detail["text"] = std::to_string(f2.up->size()) + " up, " + std::to_string(f2.down->size()) + " down" +
                         (same ? "" : "; FuSION 1 " + std::to_string(f1.up->size()) + " and " + std::to_string(f1.down->size()));
      out.push_back(std::move(c));
    }
  }
  if (wanted(Stage::exceptions) && f1.exceptions) {
    if (!f2.exceptions) {
      out.push_back(missing(Stage::exceptions, f2));
    } else {
      Comparison c;
      c.stage = Stage::exceptions;
      const bool same = same_set(*f1.exceptions, *f2.exceptions);
      if (!same) c.differing.push_back("exceptions.set");
      c.detail = set_difference_json(*f1.exceptions, *f2.exceptions);
      c.detail["text"] = std::to_string(std::set<std::string>(f2.exceptions->begin(), f2.exceptions->end()).size()) + " ids" +
                         (same ? "" : " (FuSION 1: " + std::to_string(std::set<std::string>(f1.exceptions->begin(), f1.exceptions->end()).size()) + ")");
      out.push_back(std::move(c));
    }
  }
  if (wanted(Stage::gsea) && f1.gsea) {
    if (!f2.gsea) {
      out.push_back(missing(Stage::gsea, f2));
    } else {
      // 12, GSEA agreement: every statistic equal as a double; Tag %, Gene % and Lead_genes as text.
      Comparison c;
      c.stage = Stage::gsea;
      std::map<std::string, const testkit::f1::GseaRow*> mine;
      for (const auto& r : f2.gsea->rows) mine.emplace(r.term, &r);
      int differ = 0;
      std::string first;
      for (const auto& r : f1.gsea->rows) {
        const auto it = mine.find(r.term);
        const bool same = it != mine.end() && it->second->es == r.es && it->second->nes == r.nes &&
                          it->second->nom_p == r.nom_p && it->second->fdr_q == r.fdr_q && it->second->fwer_p == r.fwer_p &&
                          it->second->tag_pct == r.tag_pct && it->second->gene_pct == r.gene_pct &&
                          it->second->lead_genes == r.lead_genes;
        if (!same && differ++ == 0) first = r.term;
      }
      if (differ > 0 || f1.gsea->rows.size() != f2.gsea->rows.size()) c.differing.push_back("gsea.table");
      if (!same_set(f1.gsea->shortlist, f2.gsea->shortlist)) c.differing.push_back("gsea.shortlist");
      c.detail = Json{{"rows", f1.gsea->rows.size()}, {"rows_differing", differ}, {"first_differing", first},
                      {"fusion1_selected", f1.gsea->shortlist.size()}, {"fusion2_selected", f2.gsea->shortlist.size()},
                      {"shortlist", set_difference_json(f1.gsea->shortlist, f2.gsea->shortlist)}};
      c.detail["text"] = std::to_string(f2.gsea->shortlist.size()) + " pathways selected (FuSION 1 " +
                         std::to_string(f1.gsea->shortlist.size()) + "); " + std::to_string(differ) + " of " +
                         std::to_string(f1.gsea->rows.size()) + " rows differ";
      out.push_back(std::move(c));
    }
  }
  const auto graphs = [&](Stage stage, const testkit::CanonGraph& a, const testkit::CanonGraph& b, const char* fact) {
    Comparison c;
    c.stage = stage;
    const testkit::GraphDiff g = testkit::diff(b, a);   // 2.0 first: only_a is 2.0's alone
    if (!g.empty()) c.differing.push_back(fact);
    c.detail = Json{{"fusion1", Json::array({a.nodes.size(), a.edges.size()})},
                    {"fusion2", Json::array({b.nodes.size(), b.edges.size()})},
                    {"only_fusion2_nodes", g.only_a_nodes.size()}, {"only_fusion1_nodes", g.only_b_nodes.size()},
                    {"only_fusion2_edges", g.only_a_edges.size()}, {"only_fusion1_edges", g.only_b_edges.size()},
                    {"changed_nodes", g.changed_nodes.size()}, {"changed_edges", g.changed_edges.size()}};
    if (!g.empty()) c.detail["diff"] = g.text(12);
    c.detail["text"] = std::to_string(b.nodes.size()) + " nodes, " + std::to_string(b.edges.size()) + " edges" +
                       (g.empty() ? "" : "; FuSION 1 " + std::to_string(a.nodes.size()) + " and " + std::to_string(a.edges.size()));
    return c;
  };
  if (wanted(Stage::merge) && f1.merged) {
    if (!f2.merged) out.push_back(missing(Stage::merge, f2));
    else out.push_back(graphs(Stage::merge, *f1.merged, *f2.merged, "merge.graph"));
  }
  if (wanted(Stage::corridor) && f1.corridor) {
    if (!f2.corridor) {
      out.push_back(missing(Stage::corridor, f2));
    } else {
      Comparison c = graphs(Stage::corridor, f1.corridor->graph, f2.corridor->graph, "corridor.graph");
      if (f1.corridor_size && f2.corridor_size) {
        c.detail["fusion1_counts"] = Json::array({f1.corridor_size->first, f1.corridor_size->second});
        c.detail["fusion2_counts"] = Json::array({f2.corridor_size->first, f2.corridor_size->second});
        if (*f1.corridor_size != *f2.corridor_size) c.differing.push_back("corridor.counts");
      }
      out.push_back(std::move(c));
    }
  }
  for (const SolveFacts& s1 : f1.solves) {
    const SolveFacts* s2 = solve_named(f2, s1.name);
    if (wanted(Stage::observations) && s1.up_down) {
      if (!s2 || !s2->up_down) {
        out.push_back(missing(Stage::observations, f2, s1.name));
        if (s2 && !s2->error.empty()) out.back().not_runnable = s2->error;
      } else {
        Comparison c;
        c.stage = Stage::observations;
        c.solve_case = s1.name;
        if (*s1.up_down != *s2->up_down) c.differing.push_back("observations.counts");
        c.detail = Json{{"fusion1", Json::array({s1.up_down->first, s1.up_down->second})},
                        {"fusion2", Json::array({s2->up_down->first, s2->up_down->second})}};
        c.detail["text"] = "Up: " + std::to_string(s2->up_down->first) + " Down: " + std::to_string(s2->up_down->second);
        out.push_back(std::move(c));
      }
    }
    if (wanted(Stage::solve_inputs) && (s1.coexpression || s1.frozen || (s2 && (s2->coexpression || s2->frozen)))) {
      Comparison c;
      c.stage = Stage::solve_inputs;
      c.solve_case = s1.name;
      const auto value = [](const std::optional<int>& v) { return v ? Json(*v) : Json(); };
      if (!s2) {
        c = missing(Stage::solve_inputs, f2, s1.name);
      } else {
        if (s1.coexpression != s2->coexpression || s1.frozen != s2->frozen) c.differing.push_back("solve_inputs.counts");
        c.detail = Json{{"fusion1", Json{{"coexpression", value(s1.coexpression)}, {"frozen", value(s1.frozen)}}},
                        {"fusion2", Json{{"coexpression", value(s2->coexpression)}, {"frozen", value(s2->frozen)}}}};
        c.detail["text"] = "coexpression " + value(s2->coexpression).dump() + ", frozen " + value(s2->frozen).dump();
      }
      out.push_back(std::move(c));
    }
    if (wanted(Stage::solve)) {
      if (!s2 || !s2->error.empty()) {
        Comparison c = missing(Stage::solve, f2, s1.name);
        if (s2) c.not_runnable = s2->error;
        out.push_back(std::move(c));
      } else {
        out.push_back(solve_rules(s1, *s2));
      }
    }
  }
  return out;
}

std::vector<Finding> judge(std::span<const Comparison> comparisons, const Case& k, Mode mode, std::span<const Entry> entries,
                           const Signals& signals) {
  std::vector<Finding> out;
  std::set<std::string> upstream;                      // chained: the entries behind the differences so far
  for (const Comparison& c : comparisons) {
    Finding f;
    f.case_id = k.id;
    f.solve_case = c.solve_case;
    f.mode = mode;
    f.stage = c.stage;
    f.detail = c.detail;
    if (c.not_runnable) {
      f.outcome = Outcome::not_runnable;
      f.detail["text"] = *c.not_runnable;
      out.push_back(std::move(f));
      continue;
    }
    std::vector<const Entry*> applicable;
    for (const Entry& e : entries)
      if (std::find(e.stages.begin(), e.stages.end(), c.stage) != e.stages.end() && applies(e, signals, c.solve_case))
        applicable.push_back(&e);
    std::set<std::string> used;
    if (c.differing.empty()) {
      f.outcome = c.inconclusive ? Outcome::inconclusive : Outcome::match;
    } else {
      // Each differing fact is credited to the applicable entries that allow it; an entry that applies always
      // (XD-11, XD-15) only when no entry with a narrower predicate allows it.
      std::vector<std::string> uncovered;
      for (const std::string& fact : c.differing) {
        std::vector<const Entry*> narrow, always;
        for (const Entry* e : applicable)
          if (std::find(e->allows.begin(), e->allows.end(), fact) != e->allows.end())
            (e->applies_when.contains("always") ? always : narrow).push_back(e);
        for (const Entry* e : narrow.empty() ? always : narrow) used.insert(e->id);
        if (narrow.empty() && always.empty()) uncovered.push_back(fact);
      }
      if (uncovered.empty()) {
        f.outcome = Outcome::expected_difference;
      } else {
        // Chained: a difference is accepted only when an upstream difference reaches this stage's input; an
        // entry that predicts this stage unchanged does not reach it (XD-21).
        std::set<std::string> cascade, blocked;
        for (const std::string& id : upstream) {
          const auto e = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) { return x.id == id; });
          const auto stops = e == entries.end() ? std::vector<Stage>{} : unchanged_stages(*e);
          (std::find(stops.begin(), stops.end(), c.stage) == stops.end() ? cascade : blocked).insert(id);
        }
        const bool rules_failed = std::any_of(uncovered.begin(), uncovered.end(), [](const std::string& x) { return x.starts_with("solve.r"); });
        if (mode == Mode::chained && !cascade.empty() && !rules_failed) {
          f.outcome = Outcome::expected_difference;
          used.insert(cascade.begin(), cascade.end());
          f.detail["cascade"] = true;
        } else if (mode == Mode::chained && !blocked.empty() && !rules_failed) {
          f.outcome = Outcome::prediction_failed;
          used.insert(blocked.begin(), blocked.end());
          f.detail["prediction"] = "unchanged: the difference reached " + std::string(name(c.stage));
        } else {
          f.outcome = Outcome::unexpected_difference;
        }
        f.detail["uncovered"] = uncovered;
      }
      f.detail["differing"] = c.differing;
    }

    // The predictions of the entries that apply here, whether or not the stage differs.
    Json failed_predictions = Json::array();
    for (const Entry* e : applicable)
      for (const Json& p : predictions(*e)) {
        const std::string rule = p.value("rule", "");
        std::optional<std::string> why;
        if (rule == "rescore_bound" && c.stage == Stage::solve) {
          f.detail["predicted_bound"] = c.detail.value("r2", Json::object()).value("bound", Json::array());
          if (!c.detail.value("r2", Json::object()).value("unmet", Json::array()).empty()) why = "2.0's frontier misses the re-scored bound";
        } else if (rule == "counts" && c.stage == Stage::corridor && p["corridor"].contains(k.id)) {
          const Json want = p["corridor"][k.id];
          const Json got = c.detail.value("fusion2_counts", c.detail.value("fusion2", Json()));
          f.detail["predicted_counts"] = want;
          if (got != want) why = "corridor " + got.dump() + ", predicted " + want.dump();
        } else if (rule == "selection_size" && c.stage == Stage::gsea && p["cases"].contains(k.id)) {
          const Json want = p["cases"][k.id];
          f.detail["predicted_selection"] = want;
          if (c.detail.value("fusion2_selected", Json()) != want)
            why = std::to_string(c.detail.value("fusion2_selected", 0)) + " pathways selected, predicted " + want.dump();
        }
        if (why) {
          failed_predictions.push_back(Json{{"entry", e->id}, {"rule", rule}, {"why", *why}});
          used.insert(e->id);
        }
      }
    if (!failed_predictions.empty()) {
      f.detail["failed_predictions"] = failed_predictions;
      if (f.outcome != Outcome::unexpected_difference) f.outcome = Outcome::prediction_failed;
    }
    f.entries.assign(used.begin(), used.end());
    // Only a difference in what a later stage reads reaches it: the lists, the exception set, the
    // shortlist, the merged graph and the corridor; a count or a table that differs changes no input.
    const auto feeds_later = [](const std::string& fact) {
      return fact == "classify.lists" || fact == "exceptions.set" || fact == "gsea.shortlist" || fact == "merge.graph" ||
             fact == "corridor.graph";
    };
    if (mode == Mode::chained && (f.outcome == Outcome::expected_difference || f.outcome == Outcome::prediction_failed) &&
        std::any_of(c.differing.begin(), c.differing.end(), feeds_later))
      upstream.insert(used.begin(), used.end());
    out.push_back(std::move(f));
  }
  return out;
}

std::vector<Point> verification_points(const testkit::Frontier& frontier, Point lo, Point hi) {
  std::vector<Point> f = frontier.points;
  std::sort(f.begin(), f.end());
  const auto inside = [&](Point p) { return p.n >= lo.n && p.e >= lo.e && p.n <= hi.n && p.e <= hi.e; };
  if (f.empty()) return {hi};
  std::vector<Point> out(f.begin(), f.end());
  for (std::size_t i = 0; i + 1 < f.size(); ++i) {
    const Point corner{f[i + 1].n - 1, f[i].e - 1};
    if (inside(corner)) out.push_back(corner);
  }
  if (const Point p{f.front().n - 1, hi.e}; inside(p)) out.push_back(p);
  if (const Point p{hi.n, f.back().e - 1}; inside(p)) out.push_back(p);
  return out;
}

}  // namespace fusion::reftest
