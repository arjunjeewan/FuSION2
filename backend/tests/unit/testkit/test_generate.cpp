// The kit's seeded generators (12.C9; section 12, "Test hooks"): golden SHA-256s of generated fixtures
// for fixed seeds, the same on every platform, since they draw only from core::Rng; what tiny_case()
// promises; and ScriptedOracle agreeing with exhaustive_frontier on random tiny cases.
#include <catch2/catch_test_macros.hpp>

#include <set>

#include "fusion/core/core.hpp"
#include "fusion/solve/solve.hpp"
#include "fusion/testkit/generate.hpp"
#include "fusion/testkit/oracles.hpp"
#include "fusion/testkit/scripted_oracle.hpp"

using namespace fusion::testkit;
using fusion::core::Rng;
using fusion::core::Sha256;

namespace {

std::string all_text(const std::vector<SyntheticPathway>& set) {
  std::string out;
  for (const auto& p : set) out += p.id + "\n" + p.kgml;
  return out;
}

std::string tiny_text(const TinyCase& tc) {
  std::string out = tc.corridor.graph.text() + std::to_string(tc.corridor.bound) + "\n";
  const auto list = [&](const std::vector<std::string>& v) {
    for (const auto& s : v) out += s + " ";
    out += "\n";
  };
  list(tc.spec.up);
  list(tc.spec.down);
  list(tc.spec.essential);
  list(tc.spec.avoid);
  list(tc.spec.active);
  list(tc.spec.inactive);
  list(tc.spec.nonrelaxed);
  list(tc.spec.fsc);
  out += std::to_string(static_cast<int>(tc.spec.solve_case)) + (tc.spec.named ? " named\n" : " positional\n");
  return out;
}

struct Quiet final : fusion::solve::SearchObserver {
  void on_started(fusion::solve::Point, fusion::solve::Phase) override {}
  void on_answer(const fusion::solve::Answer&) override {}
  void on_point(fusion::solve::Point) override {}
};

}  // namespace

TEST_CASE("testkit: generated fixtures have golden SHA-256s for fixed seeds", "[unit][testkit][12.C9]") {
  Rng rng = Rng::stream(42, 0);
  const std::string set = all_text(pathways(rng, 3, PathwayShape{}));
  const std::vector<std::string> ids{"t1", "t2", "t3", "t4", "t5", "t6"};
  const std::string values = value_file(rng, ids, 0.3, false);
  const std::string crlf = value_file(rng, ids, 0.3, true);
  std::string tiny;
  for (int k = 0; k < 5; ++k) tiny += tiny_text(tiny_case(rng, 6, 8, 3));
  CHECK(Sha256::of(set).hex() == "90026b7d946d4f509a5b4c17a0b952c3695b16ac5376869276b62a5c0ce5a411");
  CHECK(Sha256::of(values).hex() == "a0a0f66d099cad61f074617f910c220a69a6c042da2a472b28e8d044ee7aedf0");
  CHECK(Sha256::of(crlf).hex() == "44223dbaff171c7f1b2eb3c64e7c4aca81577e74a14926998857aff23a085ac8");
  CHECK(Sha256::of(tiny).hex() == "2b882aaf9832e432dfcbe6c60d4039598672ed585cb486ca4e98bf72aca195b2");
  CHECK(crlf.find("\r\n") != std::string::npos);
  CHECK(values.find('\r') == std::string::npos);
}

TEST_CASE("testkit: pathways() writes KGML-format text with made-up ids, overlapping within a set", "[unit][testkit]") {
  Rng rng = Rng::stream(7, 0);
  PathwayShape shape;
  shape.entries = 6;
  shape.relations = 8;
  shape.groups = 2;
  shape.shared_id = 1.0;                                   // every id from the shared pool
  const auto set = pathways(rng, 2, shape);
  REQUIRE(set.size() == 2);
  CHECK(set[0].id == "path:t00001");
  CHECK(set[1].id == "path:t00002");
  for (const auto& p : set) {
    CHECK(p.kgml.rfind("<?xml", 0) == 0);
    CHECK(p.kgml.find("<pathway name=\"" + p.id + "\"") != std::string::npos);
    CHECK(p.kgml.find("type=\"group\"") != std::string::npos);
    CHECK(p.kgml.find("hsa:") == std::string::npos);        // no KEGG ids (S8.31)
  }
  // Both pathways draw from one pool of 6 ids, so their exception-free merge has overlapping boxes.
  const std::vector<std::string> texts{set[0].kgml, set[1].kgml};
  const CanonGraph g = exception_merge_texts(texts, {}, MergeOracleSettings{});
  std::set<std::string> ids;
  for (const auto& n : g.nodes)
    if (n.type != "group")
      for (const auto& m : n.members) CHECK(ids.insert(m).second);   // each id in exactly one node
  CHECK(ids.size() <= 6 + 2);                                       // the pool and the compounds
}

TEST_CASE("testkit: tiny_case() gives a corridor of the size asked, its own walk corridor", "[unit][testkit]") {
  Rng rng = Rng::stream(2026, 9);
  int complexes = 0, fsc = 0, positional = 0;
  for (int k = 0; k < 300; ++k) {
    const int bound = 2 + static_cast<int>(rng.below(3));
    const TinyCase tc = tiny_case(rng, 6, 8, bound);
    INFO(tc.corridor.graph.text());
    CHECK(tc.corridor.graph.nodes.size() == 6);
    CHECK(tc.corridor.graph.edges.size() == 8);
    CHECK(tc.corridor.bound == bound);
    const std::vector<std::string> s{"S"}, t{"T"};
    CHECK(walk_corridor(tc.corridor.graph, s, t, bound).graph == tc.corridor.graph);
    for (const auto& e : tc.corridor.graph.edges) {
      CHECK(e.key.src != e.key.tgt);
      CHECK(e.key.tgt != "S");
      CHECK(e.key.src != "T");
    }
    for (const auto& n : tc.corridor.graph.nodes) complexes += n.type == "group" ? 1 : 0;
    fsc += tc.spec.fsc.empty() ? 0 : 1;
    positional += tc.spec.named ? 0 : 1;
  }
  CHECK(complexes > 30);
  CHECK(fsc > 60);
  CHECK(positional > 30);
}

TEST_CASE("testkit: ScriptedOracle and exhaustive_frontier agree on random tiny cases", "[unit][testkit]") {
  // Section 12's test hook: the Pareto search driven by a staircase scripted from the exhaustive
  // frontier finds that frontier again, each point certified.
  Rng rng = Rng::stream(2026, 10);
  int nonempty = 0;
  for (int k = 0; k < 60; ++k) {
    const TinyCase tc = tiny_case(rng, 6, 8, 2 + static_cast<int>(rng.below(3)));
    const Frontier f = exhaustive_frontier(tc.corridor, tc.spec, Semantics::fusion2, {0, 0}, {8, 8});
    std::vector<fusion::solve::Point> staircase;
    for (Point p : f.points) staircase.push_back({p.n, p.e});
    ScriptedOracle oracle(staircase);
    const fusion::solve::Box box{{0, 0}, {8, 8}};
    fusion::solve::AnswerCache cache(box);
    Quiet quiet;
    const auto found = fusion::solve::pareto_search(box, cache, oracle, quiet);
    const fusion::solve::Frontier assembled = fusion::solve::assemble(box, cache.answers(), oracle.witnesses, found);
    std::vector<Point> points;
    for (const auto& fp : assembled.points) {
      points.push_back({fp.point.n, fp.point.e});
      CHECK(fp.certified);
    }
    CHECK(points == f.points);
    for (const auto& a : cache.answers()) CHECK(oracle.calls(a.point) <= 1);   // S5.20
    nonempty += f.points.empty() ? 0 : 1;
  }
  CHECK(nonempty > 20);
}
