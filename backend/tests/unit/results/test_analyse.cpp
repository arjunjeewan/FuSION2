// results: the checks on every result (shape, polarity, counts, draw cases, FSC), the sign-consistent
// routes (S5.13) and the FSC comparison (07.C7) (section 07, test hooks "Polarity", "Draw cases",
// "Routes", "Counts", "FSC", "Comparison").
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <random>

#include "fixture.hpp"
#include "fusion/results/view.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;
using network::EndState;

namespace {

bool fusion1_relaxed(bool inhibition, EndState s, EndState t) {
  // FuSION 1's rule, Linux/src/GraphManagerNew.cc:28552-28592, written out case by case.
  const bool sp = s != EndState::absent, tp = t != EndState::absent;
  const bool sa = s == EndState::active, ta = t == EndState::active;
  if (inhibition) return (sp && sa && tp && ta) || (sp && !sa && tp && !ta);
  return (sp && sa && tp && !ta) || (sp && !sa && tp && ta) || (sp && !sa && tp && !ta);
}

std::size_t count(const std::vector<Issue>& issues, std::string_view code) {
  return static_cast<std::size_t>(std::count_if(issues.begin(), issues.end(), [&](const Issue& i) { return i.code == code; }));
}

SolveSpec spec(std::string source, std::string target, std::vector<std::string> fsc = {}) {
  SolveSpec s;
  s.source.value = std::move(source);
  s.target.value = std::move(target);
  for (auto& f : fsc) s.fsc.push_back(network::NodeKey{std::move(f)});
  return s;
}

}  // namespace

TEST_CASE("the one polarity rule equals FuSION 1's 3 x 3 x 2 table (S6.14)", "[unit][results]") {
  const EndState states[] = {EndState::absent, EndState::inactive, EndState::active};
  int relaxed = 0;
  for (const auto effect : {network::Effect::activation, network::Effect::inhibition}) {
    for (const auto s : states) {
      for (const auto t : states) {
        CHECK(network::is_relaxed(effect, s, t) == fusion1_relaxed(effect == network::Effect::inhibition, s, t));
        relaxed += network::is_relaxed(effect, s, t) ? 1 : 0;
      }
    }
  }
  CHECK(relaxed == 5);   // 3 activation rows, 2 inhibition rows
  const std::vector<std::string> inh{"inhibition", "dephosphorylation"};
  CHECK(network::effect_of(inh) == network::Effect::inhibition);
  CHECK(network::effect_of(std::vector<std::string>{}) == network::Effect::activation);
}

TEST_CASE("analyse() accepts a consistent witness and reports each kind of internal error", "[unit][results]") {
  fx::Catalogue cat;
  cat.node("a", "A");
  cat.node("b", "B");
  cat.node("c", "C");
  cat.node("x", "X", Observed::up);
  const auto ab = ek("a", "b", {"activation"});
  const auto bc = ek("b", "c", {"inhibition"});
  cat.edge(ab);
  cat.edge(bc);
  // a -> b kept (both active), b -| c kept (active, inactive); x absent and relaxed.
  fx::W good{{"a", "b"}, {"c"}, {ab, bc}, {{"x", false}}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {good}));
  SolveResult r = build(results, spec("a", "c"), cat);
  CHECK(analyse(r).empty());

  SECTION("polarity: a kept edge whose ends break it") {
    fx::W bad{{"a", "b", "c"}, {}, {ab, bc}, {{"x", false}}, {}};   // b -| c with both active is relaxed
    solve::Results res;
    res.cases.push_back(fx::frontier_case(solve::Case::free, {bad}));
    SolveResult rr = build(res, spec("a", "c"), cat);
    const auto issues = analyse(rr);
    REQUIRE(count(issues, "results.polarity") == 1);
    CHECK_THAT(issues[0].message, ContainsSubstring("b|c|PPrel|inhibition is kept in the witness, but its ends (active, active) break it"));
    CHECK(issues[0].kind == Issue::Kind::internal_error);
  }
  SECTION("polarity: a relaxed edge whose ends honour it") {
    fx::W bad{{"a", "b"}, {"c"}, {ab, bc}, {}, {{bc, true}}};
    solve::Results res;
    res.cases.push_back(fx::frontier_case(solve::Case::free, {bad}));
    SolveResult rr = build(res, spec("a", "c"), cat);
    CHECK(count(analyse(rr), "results.polarity") == 1);
  }
  SECTION("counts: a point that differs from its witness's counts (06.C20)") {
    solve::Results res;
    res.cases.push_back(fx::frontier_case(solve::Case::free, {good}));
    res.cases[0].frontier.points[0].point = pt(2, 1);
    SolveResult rr = build(res, spec("a", "c"), cat);
    const auto issues = analyse(rr);
    REQUIRE(count(issues, "results.counts") == 1);
    CHECK_THAT(issues[0].message, ContainsSubstring("n=2 e=1 of unconstrained has a witness with counts n=1 e=0"));
  }
  SECTION("shape: a node both active and inactive") {
    fx::W bad{{"a", "b", "c"}, {"c"}, {ab}, {}, {}};
    solve::Results res;
    res.cases.push_back(fx::frontier_case(solve::Case::free, {bad}));
    SolveResult rr = build(res, spec("a", "c"), cat);
    CHECK(count(analyse(rr), "results.shape") >= 1);
  }
  SECTION("draw cases: an effect both kept and relaxed on one pair is S6.15's twin") {
    const auto ab2 = ek("a", "b", {"phosphorylation"});
    cat.edge(ab2);
    fx::W twin{{"a", "b"}, {"c"}, {ab, ab2, bc}, {}, {{ab2, true}}};
    solve::Results res;
    res.cases.push_back(fx::frontier_case(solve::Case::free, {twin}));
    SolveResult rr = build(res, spec("a", "c"), cat);
    const auto issues = analyse(rr);
    CHECK(count(issues, "results.twin") == 1);
    CHECK(count(issues, "results.polarity") == 1);
  }
}

TEST_CASE("analyse() checks the FSC node by node in the in and out cases (06.C10)", "[unit][results]") {
  fx::Catalogue cat;
  cat.node("s", "S");
  cat.node("t", "T");
  cat.node("f", "F1", Observed::up);
  cat.node("g", "G2", Observed::down);
  const auto sf = ek("s", "f", {"activation"});
  const auto ft = ek("f", "t", {"activation"});
  const auto sg = ek("s", "g", {"inhibition"});
  const auto gt = ek("g", "t", {"inhibition"});
  for (const auto& e : {sf, ft, sg, gt}) cat.edge(e);
  // In: F1 up and active, G2 down and inactive, both on a path: consistent.
  fx::W in{{"s", "t", "f"}, {"g"}, {sf, ft, sg, gt}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {in}));
  SolveResult r = build(results, spec("s", "t", {"f", "g"}), cat);
  CHECK(analyse(r).empty());

  // G2 absent in the in case: one error, for G2 only.
  fx::W missing{{"s", "t", "f"}, {}, {sf, ft}, {}, {}};
  solve::Results r2;
  r2.cases.push_back(fx::frontier_case(solve::Case::in, {missing}));
  SolveResult rr = build(r2, spec("s", "t", {"f", "g"}), cat);
  auto issues = analyse(rr);
  REQUIRE(count(issues, "results.fsc") == 1);
  CHECK_THAT(issues[0].message, ContainsSubstring("FSC G2 (g) is absent in the in case"));

  // F1 up but inactive in the in case: activity does not match its observation.
  fx::W wrong{{"s", "t"}, {"f"}, {sf, ft}, {}, {{sf, true}, {ft, true}}};
  solve::Results r3;
  r3.cases.push_back(fx::frontier_case(solve::Case::in, {wrong}));
  SolveResult rw = build(r3, spec("s", "t", {"f"}), cat);
  issues = analyse(rw);
  REQUIRE(count(issues, "results.fsc") == 1);
  CHECK_THAT(issues[0].message, ContainsSubstring("is inactive but is observed up"));

  // Out: an FSC node present is an error; absent with removed_with_fsc is not.
  fx::W out_bad{{"s", "t", "f"}, {}, {sf, ft}, {}, {}};
  solve::Results r4;
  r4.cases.push_back(fx::frontier_case(solve::Case::out, {out_bad}));
  SolveResult ro = build(r4, spec("s", "t", {"f"}), cat);
  CHECK(count(analyse(ro), "results.fsc") == 1);
  const auto st = ek("s", "t", {"activation"});
  cat.edge(st);
  fx::W out_good{{"s", "t"}, {}, {st}, {}, {}};
  solve::Results r5;
  r5.cases.push_back(fx::frontier_case(solve::Case::out, {out_good}));
  r5.cases[0].observations = {solve::ObservationRow{"f", "F1", 'u', solve::Role::removed_with_fsc}};
  cat.gene("f", "f");
  SolveResult rg = build(r5, spec("s", "t", {"f"}), cat);
  CHECK(analyse(rg).empty());
  r5.cases[0].observations[0].role = solve::Role::counted;
  SolveResult rc = build(r5, spec("s", "t", {"f"}), cat);
  CHECK(count(analyse(rc), "results.fsc") == 1);
}

namespace {

// fdr050 with JUN, modelled on section 07's description: at (n 5, e 1) JUN is present and active but its
// only in-edge, the kept MAPK1 -> JUN, hangs off the relaxed DUSP1 -| MAPK1; at (n 6, e 0) JUN is on
// the route. Node labels follow the run; keys are made up.
struct Fdr050 {
  fx::Catalogue cat;
  std::vector<std::pair<std::string, std::string>> route{
      {"k01", "EGFR"}, {"k02", "PTK2"}, {"k03", "MAPK14"}, {"k04", "CXCL1"}, {"k05", "CXCR6+17"},
      {"k06", "JAK1+3"}, {"k07", "[STAT1+6]"}, {"k08", "MYC"}, {"k09", "CXCL8"}};
  std::vector<network::EdgeKey> route_edges;
  Fdr050() {
    for (const auto& [k, label] : route) cat.node(k, label);
    for (std::size_t i = 1; i < route.size(); ++i) {
      route_edges.push_back(ek(route[i - 1].first, route[i].first, {"activation"}));
      cat.edge(route_edges.back());
    }
    cat.node("k10", "DUSP1");
    cat.node("k11", "MAPK1");
    cat.node("k12", "JUN", Observed::up);
    for (int i = 0; i < 6; ++i) cat.node("u" + std::to_string(i), "UP" + std::to_string(i), Observed::up);
    for (const auto& e : {ek("k01", "k10", {"activation"}), ek("k10", "k11", {"inhibition", "dephosphorylation"}),
                          ek("k11", "k12", {"phosphorylation"}), ek("k12", "k09", {"expression"}),
                          ek("k03", "k12", {"activation"})}) {
      cat.edge(e);
    }
  }
  fx::W off_route() const {   // (n 5, e 1)
    fx::W w;
    for (const auto& [k, l] : route) w.active.push_back(k);
    w.active.insert(w.active.end(), {"k10", "k11", "k12"});
    w.selected = route_edges;
    w.selected.insert(w.selected.end(), {ek("k01", "k10", {"activation"}), ek("k10", "k11", {"inhibition", "dephosphorylation"}),
                                         ek("k11", "k12", {"phosphorylation"}), ek("k12", "k09", {"expression"})});
    w.relaxed_edges = {{ek("k10", "k11", {"inhibition", "dephosphorylation"}), true}};
    for (int i = 0; i < 5; ++i) w.relaxed_nodes.emplace_back("u" + std::to_string(i), false);
    return w;
  }
  fx::W on_route() const {    // (n 6, e 0)
    fx::W w;
    for (const auto& [k, l] : route) w.active.push_back(k);
    w.active.push_back("k12");
    w.selected = route_edges;
    w.selected.insert(w.selected.end(), {ek("k03", "k12", {"activation"}), ek("k12", "k09", {"expression"})});
    for (int i = 0; i < 6; ++i) w.relaxed_nodes.emplace_back("u" + std::to_string(i), false);
    return w;
  }
};

}  // namespace

TEST_CASE("sign-consistent routes reproduce fdr050 with JUN (S5.13)", "[unit][results]") {
  Fdr050 f;
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {f.off_route(), f.on_route()}));
  SolveResult r = build(results, spec("k01", "k09", {"k12"}), f.cat);
  REQUIRE(analyse(r).empty());
  const CaseResult& c = r.cases[0];
  REQUIRE(c.points.size() == 2);
  const auto labels = [&](const std::vector<NodeIx>& nodes) {
    std::vector<std::string> out;
    for (const NodeIx v : nodes) out.push_back(r.tables.nodes[v].label);
    return out;
  };
  const Route& off = c.points[0].at == pt(5, 1) ? c.points[0].witnesses[0].route : c.points[1].witnesses[0].route;
  const Route& on = c.points[0].at == pt(6, 0) ? c.points[0].witnesses[0].route : c.points[1].witnesses[0].route;
  CHECK(off.exists);
  CHECK(off.fsc_on_route.empty());   // JUN present and active, on no sign-consistent route
  CHECK(labels(off.nodes) == std::vector<std::string>{"EGFR", "PTK2", "MAPK14", "CXCL1", "CXCR6+17", "JAK1+3",
                                                      "[STAT1+6]", "MYC", "CXCL8"});
  REQUIRE(on.fsc_on_route.size() == 1);
  CHECK(r.tables.nodes[on.fsc_on_route[0]].label == "JUN");
  CHECK(labels(on.nodes) == std::vector<std::string>{"EGFR", "PTK2", "MAPK14", "JUN", "CXCL8"});
  const auto view = build_view(r, c, c.points[0].at == pt(5, 1) ? c.points[0] : c.points[1]);
  CHECK_THAT(view.witnesses[0].route_note, ContainsSubstring("JUN is on no sign-consistent route"));
}

TEST_CASE("a consistent witness never gives S6.15's twin (random end states)", "[unit][results]") {
  // Under the rule, a kept activation needs both ends active, and then no activation on that pair can
  // be relaxed; a kept inhibition needs the ends to differ, and then no inhibition is relaxed.
  std::mt19937_64 random(20261117);
  const EndState states[] = {EndState::inactive, EndState::active};
  const std::vector<std::vector<std::string>> subtypes{{"activation"}, {"phosphorylation"}, {"inhibition"},
                                                       {"repression"}, {"expression"}, {"inhibition", "ubiquitination"}};
  for (int round = 0; round < 200; ++round) {
    fx::Catalogue cat;
    fx::W w;
    const int n = 2 + static_cast<int>(random() % 5);
    std::vector<EndState> state(static_cast<std::size_t>(n));
    for (int v = 0; v < n; ++v) {
      const std::string key = "v" + std::to_string(v);
      cat.node(key, key);
      state[static_cast<std::size_t>(v)] = v == 0 ? EndState::active : states[random() % 2];
      (state[static_cast<std::size_t>(v)] == EndState::active ? w.active : w.inactive).push_back(key);
    }
    for (int e = 0; e < 3 * n; ++e) {
      const auto u = static_cast<int>(random() % static_cast<unsigned>(n));
      const auto v = static_cast<int>(random() % static_cast<unsigned>(n));
      if (u == v) continue;
      const auto key = ek("v" + std::to_string(u), "v" + std::to_string(v), subtypes[random() % subtypes.size()]);
      if (std::find(w.selected.begin(), w.selected.end(), key) != w.selected.end()) continue;
      cat.edge(key);
      const bool selected = random() % 3 != 0;
      const bool relaxed = network::is_relaxed(network::effect_of(key.subtypes), state[static_cast<std::size_t>(u)],
                                               state[static_cast<std::size_t>(v)]);
      if (selected) w.selected.push_back(key);
      if (relaxed && (selected || random() % 2 == 0)) w.relaxed_edges.emplace_back(key, selected);
      else if (!selected) continue;
    }
    solve::Results results;
    results.cases.push_back(fx::frontier_case(solve::Case::free, {w}));
    SolveResult r = build(results, spec("v0", "v1"), cat);
    const auto issues = analyse(r);
    CHECK(count(issues, "results.twin") == 0);
    CHECK(count(issues, "results.polarity") == 0);
    for (const ViewEdge& e : build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]).edges) {
      if (e.draw == Draw::flipped) CHECK(e.replaced != e.effect);
    }
  }
}

namespace {

CaseResult points_case(Case kind, CaseStatus status, std::vector<std::pair<Point, bool>> points,
                       std::vector<UndecidedCell> undecided = {}) {
  CaseResult c;
  c.kind = kind;
  c.status = status;
  c.requested = pt(8, 8);
  for (const auto& [at, certified] : points) {
    FrontierPoint p;
    p.at = at;
    p.certified = certified;
    c.points.push_back(p);
  }
  c.undecided = std::move(undecided);
  return c;
}

SolveResult jun(CaseResult in, CaseResult out) {
  SolveResult r;
  r.tables.nodes.resize(3);
  r.tables.nodes[0].key.value = "hsa1956";
  r.tables.nodes[0].label = "EGFR";
  r.tables.nodes[1].key.value = "hsa3576";
  r.tables.nodes[1].label = "CXCL8";
  r.tables.nodes[2].key.value = "hsa3725";
  r.tables.nodes[2].label = "JUN";
  r.header.source = 0;
  r.header.target = 1;
  r.header.fsc = {2};
  r.cases = {std::move(in), std::move(out)};
  return r;
}

}  // namespace

TEST_CASE("the FSC comparison gives the runs' verdicts (07.C7)", "[unit][results]") {
  // The full JUN run: forcing JUN in saves one node at e = 0, but (n 7, e 1) and (n 8, e 0) without it
  // are blocked by the undecided (n 7, e 0).
  const auto full = jun(points_case(Case::in, CaseStatus::complete,
                                    {{pt(2, 6), true}, {pt(3, 4), true}, {pt(4, 3), true}, {pt(5, 2), true}, {pt(6, 1), true}, {pt(7, 0), true}}),
                        points_case(Case::out, CaseStatus::complete,
                                    {{pt(2, 7), true}, {pt(3, 5), true}, {pt(4, 4), true}, {pt(5, 3), true}, {pt(6, 2), true},
                                     {pt(7, 1), false}, {pt(8, 0), false}},
                                    {{pt(7, 0), "TIMEOUT"}}));
  const auto cmp = compare_cases(full);
  REQUIRE(cmp);
  CHECK(cmp->verdict == Verdict::with_cheaper);
  CHECK(cmp->delta_n_at_e0 == 1);
  CHECK_FALSE(cmp->certified);
  CHECK(comparison_sentence(full, *cmp) == "forcing JUN in saves 1 node at e=0 (not certified)");

  // The reference run: one point with JUN, none without, settled by unsat answers.
  const auto reference = jun(points_case(Case::in, CaseStatus::complete, {{pt(1, 0), true}}),
                             points_case(Case::out, CaseStatus::no_point, {}));
  const auto ref = compare_cases(reference);
  REQUIRE(ref);
  CHECK(ref->verdict == Verdict::essential);
  CHECK_FALSE(ref->delta_n_at_e0.has_value());
  CHECK(ref->certified);
  CHECK(comparison_sentence(reference, *ref) == "JUN is essential: there is no point without it");

  // The FOSB run: identical curves.
  const std::vector<std::pair<Point, bool>> fosb{{pt(7, 0), true}, {pt(6, 1), true}, {pt(5, 2), true},
                                                 {pt(4, 3), true}, {pt(3, 4), true}, {pt(2, 6), true}};
  const auto same = compare_cases(jun(points_case(Case::in, CaseStatus::complete, fosb),
                                      points_case(Case::out, CaseStatus::complete, fosb)));
  REQUIRE(same);
  CHECK(same->verdict == Verdict::identical);
  CHECK(same->delta_n_at_e0 == 0);
  CHECK(same->certified);

  CHECK(compare_cases(jun(points_case(Case::in, CaseStatus::no_point, {}),
                          points_case(Case::out, CaseStatus::complete, {{pt(1, 1), true}})))->verdict == Verdict::with_impossible);
  CHECK(compare_cases(jun(points_case(Case::in, CaseStatus::no_point, {}),
                          points_case(Case::out, CaseStatus::no_point, {})))->verdict == Verdict::no_points);
  CHECK(compare_cases(jun(points_case(Case::in, CaseStatus::complete, {{pt(1, 3), true}, {pt(4, 0), true}}),
                          points_case(Case::out, CaseStatus::complete, {{pt(2, 1), true}})))->verdict == Verdict::mixed);
  CHECK(compare_cases(jun(points_case(Case::in, CaseStatus::complete, {{pt(3, 0), true}}),
                          points_case(Case::out, CaseStatus::complete, {{pt(2, 0), true}})))->verdict == Verdict::with_costlier);
  // Only when in and out were both solved.
  auto free_only = jun(points_case(Case::free, CaseStatus::complete, {{pt(1, 0), true}}),
                       points_case(Case::out, CaseStatus::not_requested, {}));
  CHECK_FALSE(compare_cases(free_only).has_value());
}
