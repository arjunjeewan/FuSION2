// results: the one renderer's view model, the point selectors and the path filter (section 07, test
// hooks "Draw cases", "Path filter", "Levels", "View files"; S6.03 to S6.06, S6.13, S6.15, S6.17).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"
#include "fusion/results/view.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("point selectors and path modes parse strictly (S6.01, S6.03)", "[unit][results]") {
  CHECK(parse_point_selector("all").kind == PointSelector::Kind::all);
  CHECK(parse_point_selector("least").kind == PointSelector::Kind::least);
  const auto one = parse_point_selector("7,1");
  CHECK(one.kind == PointSelector::Kind::one);
  CHECK(one.at == pt(7, 1));
  for (const char* bad : {"7,1x", "7, 1", "-1,2", "7", ",", "x,1", "", "ALL"}) {
    CHECK_THROWS_WITH(parse_point_selector(bad), ContainsSubstring("is not all, least or <n>,<e>"));
  }
  try {
    parse_point_selector("7,1x");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "batch.bad_value");
  }
  CHECK(parse_path_filter("subgraph").mode == PathMode::subgraph);
  CHECK(parse_path_filter("shortest").mode == PathMode::shortest);
  CHECK(parse_path_filter("alt").window == 2);
  CHECK(parse_path_filter("alt:0").window == 0);
  CHECK(parse_path_filter("alt:5").window == 5);
  CHECK(path_filter_text(parse_path_filter("alt:5")) == "alt:5");
  CHECK_THROWS_WITH(parse_path_filter("alt:-1"), ContainsSubstring("the window must be a whole number of at least 0"));
  CHECK_THROWS_WITH(parse_path_filter("alt:x"), ContainsSubstring("the window must be a whole number of at least 0"));
  CHECK_THROWS_WITH(parse_path_filter("longest"), ContainsSubstring("is not subgraph, all, shortest, alt or alt:W"));
}

TEST_CASE("select_points takes all, the least n + e, or one point", "[unit][results]") {
  CaseResult c;
  for (const auto& p : {pt(2, 6), pt(3, 4), pt(4, 3), pt(7, 0)}) {
    FrontierPoint fp;
    fp.at = p;
    c.points.push_back(fp);
  }
  CHECK(select_points(c, parse_point_selector("all")) == std::vector<std::size_t>{0, 1, 2, 3});
  CHECK(select_points(c, parse_point_selector("least")) == std::vector<std::size_t>{1});   // 3+4 = 4+3 = 7+0; least n
  CHECK(select_points(c, parse_point_selector("3,4")) == std::vector<std::size_t>{1});
  CHECK(select_points(c, parse_point_selector("7,1")).empty());
}

namespace {

// s -> a -> t and s -> b -> t, plus a -> b at the same level, b -| c, a pair a -> t with two effects.
struct Diamond {
  fx::Catalogue cat;
  SolveSpec spec;
  Diamond() {
    for (const char* k : {"s", "a", "b", "c", "t", "x"}) cat.node(k, std::string("N") + k);
    cat.node("x", "XUP", Observed::up);
    spec.source.value = "s";
    spec.target.value = "t";
  }
  const network::EdgeKey& add(const network::EdgeKey& key) {
    cat.edge(key);
    keys.push_back(key);
    return keys.back();
  }
  std::vector<network::EdgeKey> keys;
  SolveResult result(const fx::W& w) {
    solve::Results results;
    results.cases.push_back(fx::frontier_case(solve::Case::free, {w}));
    SolveResult r = build(results, spec, cat);
    REQUIRE(analyse(r).empty());
    return r;
  }
};

const ViewEdge* find(const SolveResult& r, const WitnessView& v, std::string_view s, std::string_view t,
                     network::Effect effect) {
  for (const ViewEdge& e : v.edges) {
    if (r.tables.nodes[e.source].key.value == s && r.tables.nodes[e.target].key.value == t && e.effect == effect) return &e;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("edges group by node pair into honoured, removed and flipped (S6.15)", "[unit][results]") {
  Diamond d;
  const auto sa = d.add(ek("s", "a", {"activation"}));
  const auto at = d.add(ek("a", "t", {"activation"}));
  const auto at2 = d.add(ek("a", "t", {"phosphorylation"}));
  const auto sb = d.add(ek("s", "b", {"activation"}));
  const auto bt = d.add(ek("b", "t", {"activation"}));
  const auto bc = d.add(ek("b", "c", {"inhibition"}));
  const auto ba = d.add(ek("b", "a", {"inhibition"}));
  const auto bx = d.add(ek("b", "c", {"activation"}));
  // s, a, b, t active; c inactive. b -| c honoured, b -> c (activation, active to inactive) relaxed:
  // a flipped pair. b -| a (both active) relaxed and not selected: removed, outside the witness.
  fx::W w{{"s", "a", "b", "t"}, {"c"}, {sa, at, at2, sb, bt, bc, bx}, {{"x", false}}, {{bx, true}, {ba, false}}};
  const SolveResult r = d.result(w);
  const WitnessView v = build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]);
  CHECK(v.counts == pt(1, 2));

  const ViewEdge* honoured = find(r, v, "a", "t", network::Effect::activation);
  REQUIRE(honoured != nullptr);
  CHECK(honoured->draw == Draw::honoured);
  CHECK(honoured->members.size() == 2);   // one drawn edge per effect, both records (S6.14, S6.08)
  CHECK(honoured->in_witness);

  const ViewEdge* flipped = find(r, v, "b", "c", network::Effect::inhibition);
  REQUIRE(flipped != nullptr);
  CHECK(flipped->draw == Draw::flipped);
  CHECK(flipped->replaced == network::Effect::activation);
  CHECK(flipped->members.size() == 2);
  CHECK(find(r, v, "b", "c", network::Effect::activation) == nullptr);

  const ViewEdge* removed = find(r, v, "b", "a", network::Effect::inhibition);
  REQUIRE(removed != nullptr);
  CHECK(removed->draw == Draw::removed);
  CHECK_FALSE(removed->in_witness);   // relaxed but not selected (S6.15)
  CHECK_FALSE(removed->slack.has_value());

  // Every present node is drawn, the inactive c included (S6.06); x, relaxed and absent, is parked.
  CHECK(v.nodes.size() == 5);
  REQUIRE(v.parked.size() == 1);
  CHECK(r.tables.nodes[v.parked[0].node].label == "XUP");
  CHECK(v.parked[0].reason == "absent from this explanation; up-regulated, so it counts as one node relaxation");
  // Edges sorted by (source key, target key, effect).
  for (std::size_t i = 1; i < v.edges.size(); ++i) {
    const auto& p = v.edges[i - 1];
    const auto& q = v.edges[i];
    CHECK(std::tie(p.source, p.target, p.effect) <= std::tie(q.source, q.target, q.effect));
  }
}

TEST_CASE("levels follow hop distance over every drawn pair (S6.13)", "[unit][results]") {
  Diamond d;
  const auto sa = d.add(ek("s", "a", {"activation"}));
  const auto at = d.add(ek("a", "t", {"activation"}));
  const auto ab = d.add(ek("a", "b", {"inhibition"}));   // relaxed, unselected: still a drawn pair
  const auto bc = d.add(ek("b", "c", {"activation"}));
  // b reached only through the relaxed a -| b; c only through b. x absent.
  fx::W w{{"s", "a", "t", "b"}, {"c"}, {sa, at}, {}, {{ab, false}, {bc, false}}};
  const SolveResult r = d.result(w);
  const WitnessView v = build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]);
  std::map<std::string, std::int32_t> level;
  for (const ViewNode& n : v.nodes) level[r.tables.nodes[n.node].key.value] = n.level;
  CHECK(level["s"] == 0);
  CHECK(level["a"] == 1);
  CHECK(level["t"] == 2);
  CHECK(level["b"] == 2);   // over the relaxed pair, as pex measured over all its pairs
  CHECK(level["c"] == 3);

  // A present node no pair reaches goes one level past the deepest.
  Diamond e;
  const auto sa2 = e.add(ek("s", "a", {"activation"}));
  const auto at2 = e.add(ek("a", "t", {"activation"}));
  fx::W lone{{"s", "a", "t", "c"}, {}, {sa2, at2}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {lone}));
  const SolveResult r2 = build(results, e.spec, e.cat);
  const WitnessView v2 = build_witness_view(r2, r2.cases[0], r2.cases[0].points[0].witnesses[0]);
  for (const ViewNode& n : v2.nodes) {
    if (r2.tables.nodes[n.node].key.value == "c") CHECK(n.level == 3);
  }
}

TEST_CASE("the path filter keeps groups by slack (S6.04)", "[unit][results]") {
  Diamond d;
  // Short route s -> a -> t (L = 2), long route s -> b -> c -> t (slack 1), and the relaxed b -| a.
  const auto sa = d.add(ek("s", "a", {"activation"}));
  const auto at = d.add(ek("a", "t", {"activation"}));
  const auto sb = d.add(ek("s", "b", {"activation"}));
  const auto bc = d.add(ek("b", "c", {"activation"}));
  const auto ct = d.add(ek("c", "t", {"activation"}));
  const auto ba = d.add(ek("b", "a", {"inhibition"}));
  fx::W w{{"s", "a", "b", "c", "t"}, {}, {sa, at, sb, bc, ct}, {}, {{ba, false}}};
  const SolveResult r = d.result(w);
  const WitnessView v = build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]);
  REQUIRE(v.shortest == 2);
  const auto keys = [&](const std::vector<bool>& kept) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < kept.size(); ++i) {
      if (kept[i]) out.push_back(r.tables.nodes[v.edges[i].source].key.value + r.tables.nodes[v.edges[i].target].key.value);
    }
    return out;
  };
  CHECK(keys(kept_by(r, v, parse_path_filter("subgraph"))) == std::vector<std::string>{"at", "ba", "bc", "ct", "sa", "sb"});
  // all keeps exactly the groups in the witness: S5.11 puts every selected edge on a path.
  CHECK(keys(kept_by(r, v, parse_path_filter("all"))) == std::vector<std::string>{"at", "bc", "ct", "sa", "sb"});
  CHECK(keys(kept_by(r, v, parse_path_filter("shortest"))) == std::vector<std::string>{"at", "sa"});
  CHECK(keys(kept_by(r, v, parse_path_filter("alt:0"))) == std::vector<std::string>{"at", "sa"});
  CHECK(keys(kept_by(r, v, parse_path_filter("alt:1"))) == std::vector<std::string>{"at", "bc", "ct", "sa", "sb"});
  const auto shortest = parse_path_filter("shortest");
  const auto drawn = drawn_nodes(r, v, shortest, kept_by(r, v, shortest));
  CHECK(drawn.size() == 3);

  // psp's own endpoints: the searches are repeated from them; an absent endpoint keeps nothing.
  PathFilter from_b = parse_path_filter("shortest");
  from_b.from = network::NodeKey{"b"};
  CHECK(keys(kept_by(r, v, from_b)) == std::vector<std::string>{"bc", "ct"});
  from_b.from = network::NodeKey{"x"};
  CHECK(keys(kept_by(r, v, from_b)).empty());
  CHECK(drawn_nodes(r, v, from_b, kept_by(r, v, from_b)).empty());
}

TEST_CASE("a point's view file holds every recorded witness, and an unsat request says why (S6.02)",
          "[unit][results]") {
  Diamond d;
  d.spec.solutions = 3;
  const auto sa = d.add(ek("s", "a", {"activation"}));
  const auto at = d.add(ek("a", "t", {"activation"}));
  const auto sb = d.add(ek("s", "b", {"activation"}));
  const auto bt = d.add(ek("b", "t", {"activation"}));
  const auto sc = d.add(ek("s", "c", {"activation"}));
  const auto ct = d.add(ek("c", "t", {"activation"}));
  fx::W one{{"s", "a", "t"}, {}, {sa, at}, {}, {}};
  fx::W two{{"s", "b", "t"}, {}, {sb, bt}, {}, {}};
  fx::W three{{"s", "c", "t"}, {}, {sc, ct}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {one}));
  auto& c = results.cases[0];
  c.witnesses.push_back(two.make());
  c.witnesses.push_back(three.make());
  c.frontier.points[0].more = {1, 2};
  c.log.push_back(fx::answer(pt(0, 0), solve::Phase::extra, solve::Outcome::sat, 1));
  c.log.push_back(fx::answer(pt(0, 0), solve::Phase::extra, solve::Outcome::sat, 2));
  c.log.push_back(fx::answer(pt(0, 0), solve::Phase::extra, solve::Outcome::unsat));
  SolveResult r = build(results, d.spec, d.cat);
  REQUIRE(analyse(r).empty());
  const PointView view = build_view(r, r.cases[0], r.cases[0].points[0]);
  CHECK(view.witnesses.size() == 3);
  CHECK(view.title == "Ns → Nt · unconstrained · n 0, e 0");
  CHECK(view.subtitle == "Explanation 1 of 3; there are exactly 3.");
  CHECK(view.certification_note == "Certified Pareto-optimal.");
  CHECK(view.missing_reason.empty());

  RequestedPoint unsat;
  unsat.at = pt(0, 0);
  unsat.answer = solve::Outcome::unsat;
  const PointView none = build_view(r, r.cases[0], unsat);
  CHECK(none.witnesses.empty());
  CHECK_THAT(none.missing_reason, ContainsSubstring("unsat at this point"));
  RequestedPoint timeout;
  timeout.answer = solve::Outcome::timeout;
  timeout.label = "TIMEOUT";
  CHECK(build_view(r, r.cases[0], timeout).missing_reason == "undecided: TIMEOUT");

  // The certification note names the blocking cells (06.C6).
  CaseResult blocked = r.cases[0];
  blocked.points[0].certified = false;
  blocked.points[0].undecided_below = {pt(0, 0)};
  blocked.undecided = {{pt(0, 0), "TIMEOUT"}};
  CHECK(build_view(r, blocked, blocked.points[0]).certification_note ==
        "Not certified: blocked by the undecided cell n=0 e=0 (TIMEOUT).");
  // S6.17: "of several" only when known; with no extra solutions asked, the page says so.
  blocked.points[0].enumeration = Enumeration::not_requested;
  CHECK(build_view(r, blocked, blocked.points[0]).subtitle ==
        "One explanation found at this point; the solve did not look for others.");
}
