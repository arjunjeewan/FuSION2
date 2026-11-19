// results: names, display words, labels and build() on scripted solve::Results (section 07, test hooks
// "Build" and "Edge keys"; section 12's results row).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("names round-trip through their parsers", "[unit][results]") {
  for (const auto c : {Case::in, Case::out, Case::free}) CHECK(parse_case(case_name(c)) == c);
  for (int s = 0; s < 7; ++s) {
    const auto status = static_cast<CaseStatus>(s);
    CHECK(parse_status(status_name(status)) == status);
  }
  for (int s = 0; s < 7; ++s) {
    const auto status = static_cast<ObservationStatus>(s);
    CHECK(parse_observation_status(observation_status_name(status)) == status);
  }
  for (int o = 0; o < 5; ++o) {
    const auto outcome = static_cast<solve::Outcome>(o);
    CHECK(parse_outcome(outcome_name(outcome)) == outcome);
  }
  CHECK(case_name(Case::in) == "in");
  CHECK(status_name(CaseStatus::budget_exhausted) == "budget_exhausted");
  CHECK_FALSE(parse_case("incl").has_value());
  CHECK(unknown_label(solve::Outcome::timeout) == "TIMEOUT");
  CHECK(unknown_label(solve::Outcome::undefined) == "UNDEFINED");
  CHECK(unknown_label(solve::Outcome::sat).empty());
}

TEST_CASE("every one of 06's seven statuses has its display words", "[unit][results]") {
  CaseResult c;
  c.status = CaseStatus::complete;
  FrontierPoint p;
  p.certified = true;
  c.points = {p};
  CHECK(status_text(c) == "1 Pareto point, certified");
  c.points.assign(6, p);
  CHECK(status_text(c) == "6 Pareto points, all certified");
  c.points[5].certified = false;
  c.points[4].certified = false;
  CHECK(status_text(c) == "6 Pareto points, 2 not certified");
  c.status = CaseStatus::no_point;
  c.points.clear();
  CHECK(status_text(c) == "no Pareto point within the bounds");
  c.undecided = {{pt(1, 0), "TIMEOUT"}, {pt(0, 1), "TIMEOUT"}, {pt(2, 2), "not asked"}};
  CHECK(status_text(c) == "no point found; 3 cells undecided");
  c.status = CaseStatus::infeasible;
  c.message = "FSC JUN has no in-edge in the corridor";
  CHECK(status_text(c) == "infeasible: FSC JUN has no in-edge in the corridor");
  c.status = CaseStatus::no_de_gene;
  CHECK(status_text(c) == "no differentially expressed gene in the corridor");
  c.status = CaseStatus::stopped;
  CHECK(status_text(c) == "stopped: partial frontier");
  c.status = CaseStatus::budget_exhausted;
  CHECK(status_text(c) == "budget used up: partial frontier");
  c.status = CaseStatus::not_requested;
  CHECK(status_text(c) == "not solved in this run");
  // A points request (06.C9) has no frontier.
  c.status = CaseStatus::complete;
  RequestedPoint sat, unsat, timeout;
  sat.answer = solve::Outcome::sat;
  unsat.answer = solve::Outcome::unsat;
  timeout.answer = solve::Outcome::timeout;
  c.requested_points = {sat, unsat, timeout};
  CHECK(status_text(c) == "3 points asked: 1 sat, 1 unsat, 1 undecided");
}

TEST_CASE("case labels come from the run's case and FSC (1.6, S6.17)", "[unit][results]") {
  const std::vector<std::string> jun{"JUN"}, two{"RAC1", "RAC2+1"};
  CHECK(case_label(Case::in, jun, "solve") == "with FSC (JUN)");
  CHECK(case_label(Case::out, jun, "solve") == "without FSC (JUN)");
  CHECK(case_label(Case::in, two, "solve") == "with FSC (RAC1, RAC2+1)");
  CHECK(case_label(Case::free, {}, "solve") == "unconstrained");
  // A positional pathz3 line's one free case is labelled by the solve's name.
  CHECK(case_label(Case::free, {}, "/runs/x/temp_w") == "solve temp_w");
  CHECK(point_text(pt(7, 1)) == "n=7 e=1");
}

namespace {

// Source S, target T, FSC F; A up and absent when relaxed, D down.
struct World {
  fx::Catalogue cat;
  SolveSpec spec;
  World() {
    cat.node("hS", "SRC");
    cat.node("hT", "TGT", Observed::up, 2.5);
    cat.node("hF", "FSC1", Observed::up, 1.0);
    cat.node("hA", "AUP", Observed::up, 1.5);
    cat.node("hD", "DDOWN", Observed::down, -1.25);
    cat.node("hB", "BMID");
    cat.edge(ek("hS", "hF", {"activation"}), {"path:p1"});
    cat.edge(ek("hF", "hT", {"expression"}), {"path:p1", "path:p2"});
    cat.edge(ek("hS", "hB", {"activation"}), {"path:p2"});
    cat.edge(ek("hB", "hT", {"activation"}), {"path:p2"});
    cat.edge(ek("hB", "hD", {"inhibition"}), {"path:p2"});
    cat.pathway("path:p1", "Pathway one");
    cat.gene("hT", "hT");
    cat.gene("hF", "hF");
    cat.gene("hA", "hA");
    cat.gene("hD", "hD");
    spec.source.value = "hS";
    spec.target.value = "hT";
    spec.fsc = {network::NodeKey{"hF"}};
    spec.bound = 10;
    spec.seed = 42;
    spec.requested = pt(8, 8);
  }
};

solve::ObservationRow row(std::string gene, char observed, solve::Role role) {
  return solve::ObservationRow{std::move(gene), {}, observed, role};
}

}  // namespace

TEST_CASE("build() turns solve::Results into tables, keys and cases", "[unit][results]") {
  World w;
  fx::W in1{{"hS", "hF", "hT"}, {}, {ek("hS", "hF", {"activation"}), ek("hF", "hT", {"expression"})}, {{"hA", false}}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {in1}));
  results.cases[0].observations = {row("hT", 'u', solve::Role::exempt_endpoint), row("hF", 'u', solve::Role::forced),
                                   row("hA", 'u', solve::Role::counted), row("hZ", 'u', solve::Role::not_in_network),
                                   row("hY", 'd', solve::Role::outside_corridor)};
  solve::CaseResult out{};
  out.kase = solve::Case::out;
  out.status = solve::CaseStatus::no_point;
  out.frontier.undecided = {pt(1, 0), pt(0, 2)};
  out.log = {fx::answer(pt(1, 0), solve::Phase::diagonal, solve::Outcome::timeout, std::nullopt, solve::Source::z3, 9.5, "timeout"),
             fx::answer(pt(0, 1), solve::Phase::smallest_e, solve::Outcome::unsat),
             fx::answer(pt(0, 1), solve::Phase::smallest_e, solve::Outcome::unsat, std::nullopt, solve::Source::cache)};
  out.observations = {row("hF", 'u', solve::Role::removed_with_fsc)};
  results.cases.push_back(out);
  // The case as 06 encoded it gives the clamped box (S5.07).
  auto problem = std::make_shared<solve::Problem>();
  problem->requested.hi = pt(8, 8);
  problem->box.hi = pt(1, 8);
  results.cases[0].problem = problem;

  const SolveResult r = build(results, w.spec, w.cat);
  // Tables: every node and edge a witness, an observation or the header refers to, sorted by key.
  std::vector<std::string> keys;
  for (const auto& n : r.tables.nodes) keys.push_back(n.key.value);
  CHECK(keys == std::vector<std::string>{"hA", "hF", "hS", "hT"});
  REQUIRE(r.tables.edges.size() == 2);
  CHECK(r.tables.edges[0].key.text() == "hF|hT|PPrel|expression");
  CHECK(r.tables.edges[1].key.text() == "hS|hF|PPrel|activation");
  CHECK(r.tables.nodes[r.tables.edges[1].source].key.value == "hS");
  CHECK(r.tables.pathway_names.at("path:p1") == "Pathway one");
  CHECK(r.tables.pathway_names.at("path:p2") == "path:p2");   // a name lookup falls back to the id
  CHECK(r.tables.nodes[r.header.source].label == "SRC");
  CHECK(r.tables.nodes[r.header.target].key.value == "hT");
  REQUIRE(r.header.fsc.size() == 1);
  CHECK(r.tables.nodes[r.header.fsc[0]].label == "FSC1");

  REQUIRE(r.cases.size() == 2);
  const CaseResult& in = r.cases[0];
  CHECK(in.kind == Case::in);
  CHECK(in.label == "with FSC (FSC1)");
  CHECK(in.status_text == "1 Pareto point, certified");
  CHECK(in.requested == pt(8, 8));
  CHECK(in.searched == pt(1, 8));
  REQUIRE(in.points.size() == 1);
  CHECK(in.points[0].at == pt(1, 0));
  CHECK(in.points[0].enumeration == Enumeration::not_requested);
  const Witness& wit = in.points[0].witnesses.at(0);
  CHECK(wit.counts() == pt(1, 0));
  CHECK(wit.relaxed_absent.size() == 1);
  CHECK(r.tables.nodes[wit.relaxed_absent[0]].key.value == "hA");
  // Observations sorted by gene, with their node only inside the corridor (05.C9).
  REQUIRE(in.observations.size() == 5);
  CHECK(in.observations[0].gene == "hA");
  CHECK(in.observations[0].status == ObservationStatus::counted);
  CHECK(in.observations[0].node.has_value());
  const auto outside = std::find_if(in.observations.begin(), in.observations.end(),
                                    [](const Observation& o) { return o.gene == "hY"; });
  CHECK(outside->status == ObservationStatus::outside_corridor);
  CHECK(outside->observed == Observed::down);
  CHECK_FALSE(outside->node.has_value());

  const CaseResult& o = r.cases[1];
  CHECK(o.label == "without FSC (FSC1)");
  CHECK(o.status_text == "no point found; 2 cells undecided");
  REQUIRE(o.undecided.size() == 2);
  CHECK(o.undecided[0].at == pt(0, 2));
  CHECK(o.undecided[0].label == "not asked");
  CHECK(o.undecided[1].label == "TIMEOUT");
  // The search's Z3 checks, as FuSION 1's limits file counted them; cache answers are not checks.
  CHECK(o.checks == CheckCounts{0, 1, 1});
}

TEST_CASE("build() keeps the published witnesses, extra solutions and a points request", "[unit][results]") {
  World w;
  w.spec.solutions = 3;
  fx::W first{{"hS", "hF", "hT"}, {}, {ek("hS", "hF", {"activation"}), ek("hF", "hT", {"expression"})}, {}, {}};
  fx::W second{{"hS", "hB", "hT", "hF"}, {}, {ek("hS", "hB", {"activation"}), ek("hB", "hT", {"activation"})}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {first}));
  auto& c = results.cases[0];
  c.witnesses.push_back(second.make());
  c.frontier.points[0].more = {1};
  c.log.push_back(fx::answer(pt(0, 0), solve::Phase::extra, solve::Outcome::sat, 1));
  c.log.push_back(fx::answer(pt(0, 0), solve::Phase::extra, solve::Outcome::unsat));
  SolveResult r = build(results, w.spec, w.cat);
  REQUIRE(r.cases[0].points[0].witnesses.size() == 2);
  CHECK(r.cases[0].points[0].enumeration == Enumeration::complete);
  CHECK(r.cases[0].label == "unconstrained");

  // 06.C9: points=1,0;0,1 asks without a search; the frontier stays empty.
  solve::Results asked;
  solve::CaseResult q{};
  q.kase = solve::Case::free;
  q.status = solve::CaseStatus::complete;
  q.witnesses.push_back(first.make());
  q.log = {fx::answer(pt(0, 0), solve::Phase::point, solve::Outcome::sat, 0),
           fx::answer(pt(0, 1), solve::Phase::point, solve::Outcome::timeout, std::nullopt, solve::Source::z3, 1, "timeout")};
  asked.cases.push_back(q);
  w.spec.solutions = 1;
  const SolveResult rq = build(asked, w.spec, w.cat);
  const CaseResult& rc = rq.cases[0];
  CHECK(rc.points.empty());
  REQUIRE(rc.requested_points.size() == 2);
  CHECK(rc.requested_points[0].answer == solve::Outcome::sat);
  CHECK(rc.requested_points[0].witnesses.size() == 1);
  CHECK(rc.requested_points[1].label == "TIMEOUT");
  CHECK(rc.requested_points[1].witnesses.empty());
  CHECK(rc.status_text == "2 points asked: 1 sat, 0 unsat, 1 undecided");
}

TEST_CASE("edge keys with the type keep two compound edges apart (02.C11)", "[unit][results]") {
  // The full JUN run's PLCG1 -> PRKCA compound edges: one PPrel, one ECrel.
  fx::Catalogue cat;
  cat.node("hsa1956", "EGFR");
  cat.node("hsa3576", "CXCL8");
  cat.node("hsa5335", "PLCG1");
  cat.node("hsa5578", "PRKCA");
  const auto pprel = ek("hsa5335", "hsa5578", {"compound"});
  const auto ecrel = ek("hsa5335", "hsa5578", {"compound"}, "ECrel");
  cat.edge(pprel);
  cat.edge(ecrel);
  fx::W w{{"hsa1956", "hsa3576", "hsa5335", "hsa5578"}, {}, {pprel, ecrel}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {w}));
  SolveSpec spec;
  spec.source.value = "hsa1956";
  spec.target.value = "hsa3576";
  const SolveResult r = build(results, spec, cat);
  REQUIRE(r.tables.edges.size() == 2);
  CHECK(r.tables.edges[0].key.text() == "hsa5335|hsa5578|ECrel|compound");
  CHECK(r.tables.edges[1].key.text() == "hsa5335|hsa5578|PPrel|compound");
  CHECK(r.cases[0].points[0].witnesses[0].selected.size() == 2);
}
