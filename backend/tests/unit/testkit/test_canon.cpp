// The kit's canonical forms (section 12, "Test hooks"): equal graphs built in different orders give
// equal text, and every single-field mutation is detected by diff.
#include <catch2/catch_test_macros.hpp>

#include <functional>

#include "fusion/testkit/canon.hpp"

using namespace fusion::testkit;

namespace {

CanonGraph sample() {
  CanonGraph g;
  g.nodes.push_back({node_key({"t2", "t1"}), {"t2", "t1"}, "gene"});
  g.nodes.push_back({"t3", {"t3"}, "gene"});
  g.nodes.push_back({"cpdx1", {"cpdx1"}, "compound"});
  g.nodes.push_back({complex_key({"t3", "t1+t2"}), {"t1", "t2", "t3"}, "group"});
  g.edges.push_back({{"t1+t2", "t3", "PPrel", {"phosphorylation", "activation"}}, false, {"path:b", "path:a"}, 700});
  g.edges.push_back({{"t3", "t1+t2", "PPrel", {"association"}}, true, {"path:a"}, 0});
  g.edges.push_back({{"{t1+t2,t3}", "cpdx1", "PCrel", {}}, false, {"path:a"}, 12.5});
  normalise(g);
  return g;
}

}  // namespace

TEST_CASE("testkit: canonical keys name nodes by members and complexes by components", "[unit][testkit]") {
  CHECK(node_key({"hsa5", "hsa10", "hsa5"}) == "hsa10+hsa5");          // byte order, no repeats
  CHECK(complex_key({"t3", "t1+t2", "t3"}) == "{t1+t2,t3}");
  CHECK(is_complex_key("{t1+t2,t3}"));
  CHECK_FALSE(is_complex_key("t1+t2"));
  CHECK(components_of("{t1+t2,{t3,t4},t5}") == std::vector<std::string>{"t1+t2", "{t3,t4}", "t5"});
  // The module key (S2.07, S2.09): the smallest member, or c_s_ + the component keys + _c_e.
  CHECK(module_key_of("hsa5+hsa10") == "hsa10");
  CHECK(module_key_of("{hsa6772+hsa6773,hsa6774}") == "c_s_hsa6772_hsa6774_c_e");
  CHECK(module_key_of("{{t1,t2},t3}") == "c_s_c_s_t1_t2_c_e_t3_c_e");
  CHECK(module_key(CanonNode{"c_s_a_b_c_e", {"a", "b"}, "group"}) == "c_s_a_b_c_e");   // canon(CaseGraph)'s complexes
}

TEST_CASE("testkit: equal graphs built in different orders give equal text", "[unit][testkit]") {
  const CanonGraph a = sample();
  CanonGraph b;
  b.edges = {a.edges[2], a.edges[0], a.edges[1]};
  b.nodes = {a.nodes[3], a.nodes[1], a.nodes[0], a.nodes[2]};
  std::reverse(b.edges[1].pathways.begin(), b.edges[1].pathways.end());
  normalise(b);
  CHECK(a.text() == b.text());
  CHECK(a.sha256() == b.sha256());
  CHECK(diff(a, b).empty());
  CHECK(a.text().find("edge t1+t2|t3|PPrel|activation,phosphorylation\ttag=0\tpathways=path:a,path:b\tscore=700.0\n") !=
        std::string::npos);
  CHECK(a.node("t3") != nullptr);
  CHECK(a.node("t9") == nullptr);
}

TEST_CASE("testkit: diff detects every single-field mutation", "[unit][testkit]") {
  const CanonGraph a = sample();
  const std::vector<std::pair<std::string, std::function<void(CanonGraph&)>>> mutations = {
      {"node type", [](CanonGraph& g) { g.nodes[1].type = "compound"; }},
      {"complex members", [](CanonGraph& g) { g.nodes.back().members.pop_back(); }},
      {"node removed", [](CanonGraph& g) { g.nodes.erase(g.nodes.begin()); }},
      {"node added", [](CanonGraph& g) { g.nodes.push_back({"t9", {"t9"}, "gene"}); }},
      {"edge source", [](CanonGraph& g) { g.edges[0].key.src = "t3"; }},
      {"edge target", [](CanonGraph& g) { g.edges[0].key.tgt = "cpdx1"; }},
      {"edge type", [](CanonGraph& g) { g.edges[0].key.type = "GErel"; }},
      {"edge subtypes", [](CanonGraph& g) { g.edges[0].key.subtypes.push_back("inhibition"); }},
      {"edge tag", [](CanonGraph& g) { g.edges[0].dummy_tag = !g.edges[0].dummy_tag; }},
      {"edge pathways", [](CanonGraph& g) { g.edges[0].pathways.push_back("path:z"); }},
      {"edge score", [](CanonGraph& g) { g.edges[0].score += 0.5; }},
      {"edge removed", [](CanonGraph& g) { g.edges.pop_back(); }},
      {"edge repeated", [](CanonGraph& g) { g.edges.push_back(g.edges.front()); }},
  };
  for (const auto& [name, mutate] : mutations) {
    INFO(name);
    CanonGraph b = a;
    mutate(b);
    normalise(b);
    const GraphDiff d = diff(a, b);
    CHECK_FALSE(d.empty());
    CHECK_FALSE(d.text().empty());
    CHECK(a.text() != b.text());
  }
  // A key that repeats (parallel edges whose subtypes differ only in order) matches pairwise.
  CanonGraph twice = a, twice_too = a;
  twice.edges.push_back(a.edges[0]);
  twice_too.edges.push_back(a.edges[0]);
  twice_too.edges.back().score = 1;
  normalise(twice);
  normalise(twice_too);
  const GraphDiff d = diff(twice, twice_too);
  REQUIRE(d.changed_edges.size() == 1);
  CHECK(d.changed_edges[0].first.score == 700);
  CHECK(d.changed_edges[0].second.score == 1);
}
