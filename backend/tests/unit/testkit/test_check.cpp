// The independent checker under its two semantics, the walk oracle and the merge oracle, on hand-made
// cases (section 12, "Witness checker and re-scoring", "The test kit"; 12.C14).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>

#include "fusion/testkit/check.hpp"
#include "fusion/testkit/oracles.hpp"

using namespace fusion::testkit;
using Catch::Matchers::ContainsSubstring;

namespace {

// A corridor from "src>tgt:subtype" edges over single-id nodes; "{a,b}" names a complex of a and b.
CanonCorridor corridor(std::initializer_list<const char*> edges, int bound = 10) {
  CanonCorridor c;
  std::set<std::string> keys;
  for (const char* text : edges) {
    const std::string e(text);
    const auto gt = e.find('>'), colon = e.find(':');
    CanonEdge edge;
    edge.key = {e.substr(0, gt), e.substr(gt + 1, colon - gt - 1), "PPrel", {e.substr(colon + 1)}};
    keys.insert(edge.key.src);
    keys.insert(edge.key.tgt);
    c.graph.edges.push_back(edge);
  }
  for (const std::string& k : keys) {
    if (is_complex_key(k)) c.graph.nodes.push_back({k, components_of(k), "group"});
    else c.graph.nodes.push_back({k, {k}, "gene"});
  }
  normalise(c.graph);
  c.sources = {"S"};
  c.targets = {"T"};
  c.bound = bound;
  return c;
}

Assignment assign(std::initializer_list<const char*> active, std::initializer_list<const char*> inactive,
                  const CanonCorridor& c, std::initializer_list<const char*> selected) {
  Assignment a;
  for (const char* k : active) a.present.emplace_back(k, true);
  for (const char* k : inactive) a.present.emplace_back(k, false);
  for (const char* text : selected) {
    const std::string e(text);
    const auto gt = e.find('>');
    for (const CanonEdge& x : c.graph.edges)
      if (x.key.src == e.substr(0, gt) && x.key.tgt == e.substr(gt + 1)) a.selected.push_back(x.key);
  }
  return a;
}

bool mentions(const Verdict& v, const std::string& part) {
  return std::any_of(v.violations.begin(), v.violations.end(), [&](const std::string& s) { return s.find(part) != std::string::npos; });
}

}  // namespace

TEST_CASE("testkit: the checker scores a witness by S5.12 and S5.13 and checks S5.11", "[unit][testkit]") {
  const CanonCorridor c = corridor({"S>A:activation", "A>T:activation"});
  CaseSpec spec;
  spec.up = {"A"};
  const Verdict ok = check(c, spec, assign({"S", "A", "T"}, {}, c, {"S>A", "A>T"}), Semantics::fusion2);
  CHECK(ok.valid);
  CHECK(ok.cost == Point{0, 0});
  // A inactive: A is relaxed, and both activation edges between present ends that are not both active.
  const Verdict relaxed = check(c, spec, assign({"S", "T"}, {"A"}, c, {"S>A", "A>T"}), Semantics::fusion2);
  CHECK(relaxed.valid);
  CHECK(relaxed.cost == Point{1, 2});
  // A present without its out-edge (S5.11); an unknown node; the named form's endpoints (06.C16).
  CHECK(mentions(check(c, spec, assign({"S", "A", "T"}, {}, c, {"S>A"}), Semantics::fusion2), "no selected out-edge"));
  CHECK(mentions(check(c, spec, assign({"Q"}, {}, c, {}), Semantics::fusion2), "not in the corridor"));
  CHECK(mentions(check(c, spec, Assignment{}, Semantics::fusion2), "S must be present (the source)"));
  spec.named = false;                                      // a positional line passes its lists as written
  const Verdict empty = check(c, spec, Assignment{}, Semantics::fusion2);
  CHECK(empty.valid);
  CHECK(empty.cost == Point{1, 0});                        // the absent up-regulated A is relaxed
}

TEST_CASE("testkit: every selected path crosses a DE gene before the target (S5.14)", "[unit][testkit]") {
  const CanonCorridor c = corridor({"S>T:activation", "S>A:activation", "A>T:activation"});
  CaseSpec spec;
  spec.up = {"T"};                                         // a DE target does not count
  CHECK(mentions(check(c, spec, assign({"S", "T"}, {}, c, {"S>T"}), Semantics::fusion2), "without crossing a DE gene"));
  spec.up = {"A", "T"};
  CHECK(check(c, spec, assign({"S", "A", "T"}, {}, c, {"S>A", "A>T"}), Semantics::fusion2).valid);
  spec.up = {"S"};                                         // a DE source satisfies the rule by itself
  CHECK(check(c, spec, assign({"S", "T"}, {}, c, {"S>T"}), Semantics::fusion2).valid);
}

TEST_CASE("testkit: fusion1 counts self-loops and edges into the source; fusion2 trims them (S5.13, 05.C2)", "[unit][testkit]") {
  // X lies within the bound only on walks back through S, as AREG does in the full run (XD-17).
  const CanonCorridor c = corridor({"S>A:activation", "A>T:activation", "A>A:inhibition", "S>X:activation", "X>S:activation"});
  CaseSpec spec;
  spec.up = {"A", "X"};
  const Assignment w = assign({"S", "A", "T"}, {}, c, {"S>A", "A>T"});
  const Verdict f1 = check(c, spec, w, Semantics::fusion1), f2 = check(c, spec, w, Semantics::fusion2);
  CHECK(f1.valid);
  CHECK(f2.valid);
  CHECK(f1.cost == Point{1, 1});                           // X absent and relaxed; A's inhibitory self-loop
  CHECK(f2.cost == Point{0, 0});                           // X outside the trimmed corridor; no self-loop
  // Under fusion2 a node kept only through a trimmed edge cannot be present.
  CHECK(mentions(check(c, spec, assign({"S", "A", "T", "X"}, {}, c, {"S>A", "A>T", "S>X", "X>S"}), Semantics::fusion2),
                 "outside the encoding"));
}

TEST_CASE("testkit: an avoided gene forbids its complexes under fusion2 only (entries 170 and 229)", "[unit][testkit]") {
  const CanonCorridor c = corridor({"S>{F,Z}:activation", "{F,Z}>T:activation", "S>F:activation", "F>T:activation"});
  CaseSpec spec;
  spec.up = {"S"};
  spec.avoid = {"F"};
  const Assignment through_complex = assign({"S", "{F,Z}", "T"}, {}, c, {"S>{F,Z}", "{F,Z}>T"});
  const Verdict f2 = check(c, spec, through_complex, Semantics::fusion2);
  CHECK_FALSE(f2.valid);
  CHECK(mentions(f2, "a complex holding the avoided F"));
  CHECK(check(c, spec, through_complex, Semantics::fusion1).valid);
  // The out case's FSC alike; the in case holds the FSC present and active, or inactive when down.
  spec.avoid.clear();
  spec.fsc = {"F"};
  spec.solve_case = CaseSpec::Case::out;
  CHECK(mentions(check(c, spec, through_complex, Semantics::fusion2), "a complex holding the avoided F"));
  spec.solve_case = CaseSpec::Case::in;
  CHECK(mentions(check(c, spec, through_complex, Semantics::fusion2), "F must be present"));
  spec.down = {"F"};
  const Verdict down = check(c, spec, assign({"S", "T"}, {"F"}, c, {"S>F", "F>T"}), Semantics::fusion2);
  CHECK(down.valid);
  CHECK(down.cost.n == 0);                                 // the in case's FSC is not counted
  CHECK(mentions(check(c, spec, assign({"S", "F", "T"}, {}, c, {"S>F", "F>T"}), Semantics::fusion2), "F must be inactive"));
}

TEST_CASE("testkit: lists, edge lists, coexpressed pairs and frozen edges (S5.05, S5.06, S5.15, S5.16)", "[unit][testkit]") {
  CanonCorridor c = corridor({"S>A:activation", "A>T:activation", "S>B:inhibition", "B>T:activation"});
  for (CanonEdge& e : c.graph.edges)
    if (e.key.src == "S" && e.key.tgt == "B") e.score = 900;
  CaseSpec spec;
  spec.up = {"S", "A", "B"};
  const Assignment via_a = assign({"S", "A", "T"}, {}, c, {"S>A", "A>T"});
  const Assignment via_b = assign({"S", "B", "T"}, {}, c, {"S>B", "B>T"});
  spec.essential = {"B"};
  CHECK(mentions(check(c, spec, via_a, Semantics::fusion2), "B must be present (essential)"));
  spec.essential.clear();
  spec.avoid_edges = {{"S", "A", {"activation"}}};
  CHECK(mentions(check(c, spec, via_a, Semantics::fusion2), "the avoided edge S|A|PPrel|activation is selected"));
  spec.avoid_edges.clear();
  spec.frozen_threshold = 800;                             // S>B inhibits between two active ends: relaxed
  CHECK(mentions(check(c, spec, via_b, Semantics::fusion2), "the frozen edge S|B|PPrel|inhibition is relaxed"));
  spec.frozen_threshold.reset();
  spec.nonrelaxed = {"A"};
  CHECK(mentions(check(c, spec, assign({"S", "T"}, {"A"}, c, {"S>A", "A>T"}), Semantics::fusion2), "A must not be relaxed"));
  spec.nonrelaxed.clear();
  spec.coexpressed = {{"A", "B"}};
  const Assignment both = assign({"S", "A", "T"}, {"B"}, c, {"S>A", "A>T", "S>B", "B>T"});
  CHECK(mentions(check(c, spec, both, Semantics::fusion2), "coexpressed A and B differ"));
}

TEST_CASE("testkit: from_f1 names FuSION 1's nodes by their keys, and an unknown one stays unknown", "[unit][testkit]") {
  CanonCorridor c;
  c.graph.nodes = {{"S", {"S"}, "gene"}, {"T", {"T"}, "gene"}, {"hsa1+hsa2", {"hsa1", "hsa2"}, "gene"}};
  c.graph.edges = {{{"S", "hsa1+hsa2", "PPrel", {"activation"}}, false, {}, 0},
                   {{"hsa1+hsa2", "T", "GErel", {"expression"}}, false, {}, 0}};
  normalise(c.graph);
  c.sources = {"S"};
  c.targets = {"T"};
  c.bound = 10;
  f1::Witness w;
  w.present_nodes = {{"S", true, true}, {"hsa1", true, true}, {"T", true, true}};
  w.selected_edges = {{"S", "hsa1", {"activation"}}, {"hsa1", "T", {"expression"}}};
  const Assignment a = from_f1(c, w);
  CHECK(std::find(a.present.begin(), a.present.end(), std::pair<std::string, bool>{"hsa1+hsa2", true}) != a.present.end());
  CHECK(a.selected.size() == 2);
  CaseSpec spec;
  spec.up = {"hsa2"};                                      // a family member binds to its node (S2.03)
  const Verdict v = check(c, spec, a, Semantics::fusion1);
  INFO((v.violations.empty() ? std::string() : v.violations.front()));
  CHECK(v.valid);
  CHECK(v.cost == Point{0, 0});
  w.present_nodes.push_back({"hsa9", true, false});
  CHECK(mentions(check(c, spec, from_f1(c, w), Semantics::fusion1), "f1:hsa9"));
}

TEST_CASE("testkit: walk_corridor keeps the edges of walks within the bound, trimmed as 05.C2 reads S4.11", "[unit][testkit]") {
  const CanonCorridor whole = corridor({"S>A:activation", "A>T:activation", "A>B:activation", "B>T:expression",
                                        "B>A:activation", "T>X:activation", "Y>S:activation", "S>S:activation",
                                        "S>C:association"});
  const std::vector<std::string> s{"S"}, t{"T"};
  const auto edges = [](const CanonCorridor& c) {
    std::vector<std::string> out;
    for (const auto& e : c.graph.edges) out.push_back(e.key.src + ">" + e.key.tgt);
    return out;
  };
  CHECK(edges(walk_corridor(whole.graph, s, t, 2)) == std::vector<std::string>{"A>T", "S>A"});
  CHECK(edges(walk_corridor(whole.graph, s, t, 3)) == std::vector<std::string>{"A>B", "A>T", "B>T", "S>A"});
  CHECK(edges(walk_corridor(whole.graph, s, t, 4)) == std::vector<std::string>{"A>B", "A>T", "B>A", "B>T", "S>A"});
  WalkRules expression;
  expression.to_target = WalkRules::ToTarget::expression;   // S4.05: only B>T carries expression
  CHECK(edges(walk_corridor(whole.graph, s, t, 4, expression)) == std::vector<std::string>{"A>B", "B>T", "S>A"});
  WalkRules no_b;
  no_b.exclude = {"B"};
  CHECK(edges(walk_corridor(whole.graph, s, t, 4, no_b)) == std::vector<std::string>{"A>T", "S>A"});
  const CanonCorridor cut = walk_corridor(whole.graph, s, t, 3);
  CHECK(cut.sources == s);
  CHECK(cut.targets == t);
  CHECK(cut.graph.nodes.size() == 4);                      // S, A, B, T: no node off the walks
  // A complex that contains an excluded gene is excluded too (S4.06, entry 170).
  const CanonCorridor cx = corridor({"S>{F,Z}:activation", "{F,Z}>T:activation", "S>F:activation", "F>T:activation"});
  WalkRules no_f;
  no_f.exclude = {"F"};
  CHECK(walk_corridor(cx.graph, s, t, 4, no_f).graph.edges.empty());
}

TEST_CASE("testkit: exception_merge gives section 03's split of the AP-1 and JNK families", "[unit][testkit][12.C14]") {
  // AP-1 {FOS, JUN}, JNK {MAPK8, MAPK9, MAPK10}, hsa05161's {JUN, MAPK8, MAPK9, MAPK10} and hsa05224's
  // {FOS, JUN, SP1}: with JUN and MAPK8 as exceptions, {FOS, SP1} and {MAPK9, MAPK10} (section 03).
  const auto kgml = [](const std::string& name, const std::string& body) {
    return "<?xml version=\"1.0\"?>\n<pathway name=\"" + name + "\">\n" + body + "</pathway>\n";
  };
  const std::vector<std::string> texts = {
      kgml("path:hsa04010", "<entry id=\"1\" name=\"hsa:2353 hsa:3725\" type=\"gene\"/>"
                            "<entry id=\"2\" name=\"hsa:5599 hsa:5601 hsa:5602\" type=\"gene\"/>"
                            "<entry id=\"3\" name=\"hsa:1\" type=\"gene\"/>"
                            "<relation entry1=\"2\" entry2=\"1\" type=\"PPrel\"><subtype name=\"phosphorylation\"/></relation>"
                            "<relation entry1=\"1\" entry2=\"3\" type=\"GErel\"><subtype name=\"expression\"/></relation>"),
      kgml("path:hsa05161", "<entry id=\"1\" name=\"hsa:3725 hsa:5599 hsa:5601 hsa:5602\" type=\"gene\"/>"
                            "<entry id=\"2\" name=\"hsa:7\" type=\"gene\"/>"
                            "<relation entry1=\"1\" entry2=\"2\" type=\"PPrel\"><subtype name=\"activation\"/></relation>"),
      kgml("path:hsa05224", "<entry id=\"1\" name=\"hsa:2353 hsa:3725 hsa:6667\" type=\"gene\"/>"
                            "<entry id=\"2\" name=\"hsa:8\" type=\"gene\"/>"
                            "<relation entry1=\"1\" entry2=\"2\" type=\"GErel\"><subtype name=\"expression\"/></relation>")};
  const auto keys = [](const CanonGraph& g) {
    std::vector<std::string> out;
    for (const auto& n : g.nodes) out.push_back(n.key);
    return out;
  };
  CHECK(keys(exception_merge_texts(texts, {}, MergeOracleSettings{})) ==
        std::vector<std::string>{"hsa1", "hsa2353+hsa3725+hsa5599+hsa5601+hsa5602+hsa6667", "hsa7", "hsa8"});
  const CanonGraph split = exception_merge_texts(texts, {"hsa3725", "hsa5599"}, MergeOracleSettings{});
  CHECK(keys(split) == std::vector<std::string>{"hsa1", "hsa2353+hsa6667", "hsa3725", "hsa5599", "hsa5601+hsa5602", "hsa7", "hsa8"});
  // hsa05161's relation fans out over its box's three nodes (S2.10); FOS and SP1 lose the bridge.
  const auto has_edge = [&](const std::string& s, const std::string& t) {
    return std::any_of(split.edges.begin(), split.edges.end(), [&](const CanonEdge& e) { return e.key.src == s && e.key.tgt == t; });
  };
  CHECK(has_edge("hsa3725", "hsa7"));
  CHECK(has_edge("hsa5599", "hsa7"));
  CHECK(has_edge("hsa5601+hsa5602", "hsa7"));
  CHECK_FALSE(has_edge("hsa2353+hsa6667", "hsa7"));
  // Threshold 15000: nothing collapses, every id is a node of its own.
  MergeOracleSettings none;
  none.threshold = 15000;
  CHECK(exception_merge_texts(texts, {}, none).nodes.size() == 9);
}
