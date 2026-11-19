// results: the steps that run a tool or bind positional lines: plot_pareto through core's gnuplot runner
// (2B.13), pex's static export through dot (07.C5), positional slots (07.C2) and the endpoint check
// (07.C12).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>

#include "fixture.hpp"
#include "fusion/core/process.hpp"
#include "fusion/results/explain.hpp"
#include "fusion/results/pareto.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

namespace {

SolveResult positional(const std::string& name) {
  fx::Catalogue cat;
  cat.node("hsa1", "EGFR");
  cat.node("hsa22", "CXCL8", Observed::up);
  cat.node("hsa9", "MID", Observed::up);
  const auto a = ek("hsa1", "hsa9", {"activation"});
  const auto b = ek("hsa9", "hsa22", {"activation"});
  cat.edge(a);
  cat.edge(b);
  fx::W w{{"hsa1", "hsa9", "hsa22"}, {}, {a, b}, {}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::free, {w}));
  SolveSpec spec;
  spec.name = name;
  spec.source.value = "hsa1";
  spec.target.value = "hsa22";
  SolveResult r = build(results, spec, cat);
  REQUIRE(analyse(r).empty());
  return r;
}

}  // namespace

TEST_CASE("a positional pex line names its pages by slot, incl as in and excl as out (07.C2)", "[unit][results]") {
  const SolveResult w = positional("/runs/x/temp_w"), wo = positional("/runs/x/temp_wo");
  CHECK(w.cases[0].label == "solve temp_w");
  const SolveResult* solves[] = {&w, &wo};
  fx::Step step("pex");
  auto ctx = step.context();
  ExplainOptions options;
  options.prefix = "pex";
  explain(solves, options, SlotNames{{Case::in, Case::out}}, ctx);
  CHECK(std::filesystem::exists(step.dir() / "explain" / "pex_in_n0_e0.html"));
  CHECK(std::filesystem::exists(step.dir() / "explain" / "pex_out_n0_e0.html"));
  // Without slots, two solves sharing the free case put their names in the file names.
  fx::Step plain("pex-2");
  auto pctx = plain.context();
  explain(solves, ExplainOptions{}, pctx);
  CHECK(std::filesystem::exists(plain.dir() / "explain" / "temp_w_free_n0_e0.html"));
  CHECK(std::filesystem::exists(plain.dir() / "explain" / "temp_wo_free_n0_e0.html"));
}

TEST_CASE("a positional pex line's src and tgt must be the solve's (07.C12)", "[unit][results]") {
  const SolveResult r = positional("/runs/x/temp_w");
  CHECK_NOTHROW(check_endpoints(r, "hsa1", "hsa22", "pex (line 269)"));
  try {
    check_endpoints(r, "hsa1950", "hsa22", "pex (line 269)");
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "results.endpoint_mismatch");
    CHECK(std::string(f.what()) == "pex (line 269): src hsa1950 is not the source of solve temp_w (hsa1)");
  }
  CHECK_THROWS_WITH(check_endpoints(r, "hsa1", "hsa2", "pex"), ContainsSubstring("tgt hsa2 is not the target"));
}

TEST_CASE("plot_pareto keeps its script and draws through core's gnuplot runner", "[unit][results]") {
  const SolveResult r = positional("solve");
  const SolveResult* solves[] = {&r};
  fx::Step step("plot_pareto");
  auto ctx = step.context();
  const bool have_gnuplot = std::getenv("FUSION_GNUPLOT") != nullptr || core::find_on_path("gnuplot").has_value();
  if (!have_gnuplot) {
    CHECK_THROWS_WITH(plot_pareto(solves, ParetoOptions{}, ctx), ContainsSubstring("plot_pareto: gnuplot"));
    return;
  }
  plot_pareto(solves, ParetoOptions{}, ctx);
  CHECK(step.file("pareto.gp") == gnuplot_script(solves, ParetoOptions{}));
  CHECK(std::filesystem::file_size(step.dir() / "pareto.png") > 1000);
  CHECK(step.file("pareto.png").rfind("\x89PNG", 0) == 0);
  CHECK_FALSE(std::filesystem::exists(step.dir() / "pareto.png.tmp"));
  REQUIRE_FALSE(step.lines().empty());
  CHECK(step.lines().back() == "plot_pareto: plot_pareto/pareto.png (unconstrained 1 point)");
}

#ifndef _WIN32
TEST_CASE("static=svg fails with results.dot_failed when dot cannot run (07.C5)", "[unit][results]") {
  const SolveResult r = positional("solve");
  const SolveResult* solves[] = {&r};
  fx::Step step("pex");
  auto ctx = step.context();
  ExplainOptions options;
  options.static_svg = true;
  const char* old = std::getenv("FUSION_DOT");
  const std::string saved = old != nullptr ? old : "";
  ::setenv("FUSION_DOT", (step.root() / "no-such-dot").c_str(), 1);
  try {
    explain(solves, options, ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "results.dot_failed");
  }
  if (old != nullptr) ::setenv("FUSION_DOT", saved.c_str(), 1);
  else ::unsetenv("FUSION_DOT");
  // The DOT source is kept beside the missing SVG, and no half-written SVG is left (09, 2A.6).
  CHECK(std::filesystem::exists(step.dir() / "explain" / "free_n0_e0.dot"));
  CHECK_FALSE(std::filesystem::exists(step.dir() / "explain" / "free_n0_e0.svg"));
  CHECK_FALSE(std::filesystem::exists(step.dir() / "explain" / "free_n0_e0.svg.tmp"));
}
#endif
