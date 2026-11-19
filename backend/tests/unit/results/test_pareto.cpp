// results: the Pareto plot step's gnuplot script (section 07, test hook "Pareto script"; S6.22 to S6.24,
// 07.C6). Section 12 compares the script, not the image. FUSION_UPDATE_GOLDEN=1 rewrites the golden files.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>

#include "fixture.hpp"
#include "fusion/results/pareto.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

namespace {

SolveResult solve_with(std::vector<CaseResult> cases, std::vector<std::pair<std::string, std::string>> fsc = {{"hsa3725", "JUN"}}) {
  SolveResult r;
  r.tables.nodes.push_back({network::NodeKey{"hsa1956"}, "EGFR", {}, {}, Observed::none, {}, {}});
  r.tables.nodes.push_back({network::NodeKey{"hsa3576"}, "CXCL8", {}, {}, Observed::none, {}, {}});
  for (const auto& [key, label] : fsc) {
    r.header.fsc.push_back(static_cast<NodeIx>(r.tables.nodes.size()));
    r.tables.nodes.push_back({network::NodeKey{key}, label, {}, {}, Observed::none, {}, {}});
  }
  r.header.source = 0;
  r.header.target = 1;
  r.header.bound = 10;
  r.header.coexpression_threshold = 700;
  r.header.frozen_threshold = 800;
  std::vector<std::string> labels;
  for (const NodeIx f : r.header.fsc) labels.push_back(r.tables.nodes[f].label);
  for (CaseResult& c : cases) {
    c.label = case_label(c.kind, labels, "solve");
    c.status_text = status_text(c);
  }
  r.cases = std::move(cases);
  return r;
}

CaseResult curve(Case kind, std::vector<Point> points, std::vector<Point> uncertified = {},
                 CaseStatus status = CaseStatus::complete) {
  CaseResult c;
  c.kind = kind;
  c.status = status;
  c.requested = pt(8, 8);
  c.searched = pt(8, 8);
  for (const Point p : points) {
    FrontierPoint fp;
    fp.at = p;
    fp.certified = std::find(uncertified.begin(), uncertified.end(), p) == uncertified.end();
    c.points.push_back(fp);
  }
  return c;
}

void golden(const std::string& name, const std::string& script) {
  const auto path = fx::source_dir() / "unit" / "results" / "golden" / (name + ".gp");
  if (const char* update = std::getenv("FUSION_UPDATE_GOLDEN"); update != nullptr && std::string(update) == "1") {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << script;
  }
  INFO(path.string());
  CHECK(script == fx::read(path));
}

std::string script_of(const SolveResult& r) {
  const SolveResult* solves[] = {&r};
  return gnuplot_script(solves, ParetoOptions{});
}

}  // namespace

TEST_CASE("the dodge is sized from the axis range (S6.24)", "[unit][results]") {
  CHECK(dodge_offset(8) == 0.10);   // plot_po.sh's 0.10 at the bound of 8 all its callers used
  CHECK(dodge_offset(15) == Catch::Approx(0.178).margin(0.0005));
}

TEST_CASE("labels are escaped for gnuplot's enhanced text", "[unit][results]") {
  CHECK(gnuplot_text("RAC_1") == "RAC\\\\_1");
  CHECK(gnuplot_text("a\"b") == "a\\\"b");
  CHECK(gnuplot_text("x^2{y}@&~") == "x\\\\^2\\\\{y\\\\}\\\\@\\\\&\\\\~");
  CHECK(gnuplot_text("back\\slash") == "back\\\\\\\\slash");
  CHECK(gnuplot_text("tab\there") == "tab here");
}

TEST_CASE("golden pareto.gp scripts (S6.22, S6.23, 07.C6)", "[unit][results]") {
  // The full JUN run: two curves, two uncertified points, shared points dodged.
  const auto in = curve(Case::in, {pt(2, 6), pt(3, 4), pt(4, 3), pt(5, 2), pt(6, 1), pt(7, 0)});
  const auto out = curve(Case::out, {pt(2, 7), pt(3, 5), pt(4, 4), pt(5, 3), pt(6, 2), pt(7, 1), pt(8, 0)},
                         {pt(7, 1), pt(8, 0)});
  golden("two_curves_uncertified", script_of(solve_with({in, out})));

  // One curve: a solve without an FSC, drawn blue, without the FSC in the subtitle.
  SolveResult single = solve_with({curve(Case::free, {pt(0, 3), pt(1, 1), pt(2, 0)})}, {});
  single.header.coexpression_threshold.reset();
  golden("one_curve", script_of(single));

  // Three curves: a free case beside in and out is grey; a label with _ and " is escaped.
  SolveResult three = solve_with({curve(Case::in, {pt(1, 2)}), curve(Case::out, {pt(2, 2)}), curve(Case::free, {pt(1, 1)})},
                                 {{"hsa5879", "RAC_1\"x"}, {"hsa5880", "RAC2+1"}});
  three.header.frozen_threshold.reset();
  golden("three_curves", script_of(three));

  // Identical curves (the FOSB run): every point dodged, up on in and down on out.
  const std::vector<Point> fosb{pt(2, 6), pt(3, 4), pt(4, 3), pt(5, 2), pt(6, 1), pt(7, 0)};
  golden("identical_curves", script_of(solve_with({curve(Case::in, fosb), curve(Case::out, fosb)})));

  // The reference run: one point with JUN, none without; the empty case gets a key entry (S6.21, S6.23).
  CaseResult none = curve(Case::out, {}, {}, CaseStatus::no_point);
  golden("no_point", script_of(solve_with({curve(Case::in, {pt(1, 0)}), none})));

  // A budget-limited case draws its points so far, labelled partial (06.C3).
  golden("budget_exhausted", script_of(solve_with({curve(Case::in, {pt(2, 6), pt(4, 3)}, {pt(4, 3)}, CaseStatus::budget_exhausted),
                                                   curve(Case::out, {pt(3, 5)})})));

  // A points request (06.C9): its sat points as markers without a line.
  CaseResult asked;
  asked.kind = Case::free;
  asked.status = CaseStatus::complete;
  asked.requested = pt(8, 8);
  for (const auto& [at, answer] : {std::pair{pt(1, 0), solve::Outcome::sat}, std::pair{pt(0, 1), solve::Outcome::unsat},
                                   std::pair{pt(2, 2), solve::Outcome::sat}}) {
    RequestedPoint q;
    q.at = at;
    q.answer = answer;
    asked.requested_points.push_back(q);
  }
  golden("points_request", script_of(solve_with({asked}, {})));
}

TEST_CASE("the script holds what S6.23 asks of the look", "[unit][results]") {
  const auto in = curve(Case::in, {pt(2, 6), pt(7, 0)});
  const auto out = curve(Case::out, {pt(2, 6), pt(8, 0)}, {pt(8, 0)});
  const std::string s = script_of(solve_with({in, out}));
  CHECK_THAT(s, ContainsSubstring("set terminal pngcairo enhanced size 980,690 font \"Liberation Sans,13\" background rgb \"#fcfcfb\""));
  CHECK_THAT(s, ContainsSubstring("{/:Bold EGFR → CXCL8     ·     Pareto frontier}"));
  CHECK_THAT(s, ContainsSubstring("FSC JUN (hsa3725) · bound 10 · coexpression > 700, frozen > 800 · n ≤ 8, e ≤ 8"));
  CHECK_THAT(s, ContainsSubstring("set xrange [-0.5:8.5]"));
  CHECK_THAT(s, ContainsSubstring("lc rgb \"#2a78d6\" lw 2.4 pt 7 ps 1.5"));
  CHECK_THAT(s, ContainsSubstring("lc rgb \"#e34948\" lw 2.4 pt 5 ps 1.4"));
  CHECK_THAT(s, ContainsSubstring("2\t6.1\n"));   // shared by both curves: dodged by dodge_offset(8)
  CHECK_THAT(s, ContainsSubstring("2\t5.9\n"));
  CHECK_THAT(s, ContainsSubstring("title \"open marker: not certified\""));
  CHECK_THAT(s, ContainsSubstring("set output 'pareto.png'"));
  CHECK(s.find("≥") == std::string::npos);
}
