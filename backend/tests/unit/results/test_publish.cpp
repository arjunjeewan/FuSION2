// results: results.json, written and read back (07.C1, S6.02), and publish_solve's outputs, console
// lines, events and run.json summary (section 07, test hooks "JSON", "Counts"; S5.25, S5.31, 2B.7).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"
#include "fusion/results/json.hpp"
#include "fusion/results/publish.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

namespace {

struct Solved {
  fx::Catalogue cat;
  solve::Results results;
  SolveSpec spec;
  Solved() {
    cat.node("s", "EGFR");
    cat.node("t", "CXCL8", Observed::up, 2.0);
    cat.node("f", "JUN", Observed::up, 1.25);
    cat.node("u", "IRS2", Observed::up, 1.39);
    cat.node("d", "DUSP1", Observed::down, -0.5);
    const auto sf = ek("s", "f", {"activation"});
    const auto ft = ek("f", "t", {"expression"});
    const auto st = ek("s", "t", {"activation", "phosphorylation"});
    const auto sd = ek("s", "d", {"inhibition"});
    const auto dt = ek("d", "t", {"inhibition"});
    for (const auto& e : {sf, ft, st, sd, dt}) cat.edge(e, {"path:hsa04010"});
    cat.pathway("path:hsa04010", "MAPK signaling pathway");
    cat.gene("t", "t");
    cat.gene("f", "f");
    cat.gene("u", "u");
    fx::W in{{"s", "f", "t"}, {}, {sf, ft}, {{"u", false}}, {}};
    fx::W out{{"s", "t"}, {"d"}, {st, sd, dt}, {{"u", false}}, {}};
    results.cases.push_back(fx::frontier_case(solve::Case::in, {in}));
    results.cases.push_back(fx::frontier_case(solve::Case::out, {out}, {0}));
    results.cases[1].frontier.points[0].undecided_below = {pt(0, 0)};
    results.cases[1].frontier.undecided = {pt(0, 0)};
    results.cases[1].log.push_back(fx::answer(pt(0, 0), solve::Phase::diagonal, solve::Outcome::timeout, std::nullopt,
                                              solve::Source::z3, 14396.2, "timeout"));
    results.cases[0].observations = {solve::ObservationRow{"f", "JUN", 'u', solve::Role::forced},
                                     solve::ObservationRow{"t", "CXCL8", 'u', solve::Role::exempt_endpoint},
                                     solve::ObservationRow{"u", "IRS2", 'u', solve::Role::counted},
                                     solve::ObservationRow{"z", "ZZZ", 'd', solve::Role::not_in_network}};
    results.cases[1].observations = {solve::ObservationRow{"f", "JUN", 'u', solve::Role::removed_with_fsc}};
    spec.source.value = "s";
    spec.target.value = "t";
    spec.fsc = {network::NodeKey{"f"}};
    spec.bound = 10;
    spec.seed = 42;
    spec.coexpression_threshold = 700;
    spec.rules = {std::string(rule_de_on_every_path), std::string(rule_strict_scores)};
    spec.corridor = {115, 370, 10, "fb_rch/corridor.json"};
  }
};

core::Setting legacy(const std::string& prefix) {
  static const core::SettingSpec spec{.name = "legacy_prefix", .module = "solve", .type = core::Type::output_name,
                                      .need = core::Need::optional};
  core::Setting s;
  s.spec = &spec;
  s.value = core::Value{prefix};
  s.source = core::Source::positional;
  return s;
}

CaseResult points_case(Case kind, std::vector<std::pair<Point, bool>> points) {
  CaseResult c;
  c.kind = kind;
  c.status = CaseStatus::complete;
  for (const auto& [at, certified] : points) {
    FrontierPoint p;
    p.at = at;
    p.certified = certified;
    if (!certified) p.undecided_below = {pt(7, 0)};
    c.points.push_back(p);
  }
  return c;
}

}  // namespace

TEST_CASE("results.json reads back to the same result, and writes the same bytes twice (07.C1)", "[unit][results]") {
  Solved s;
  SolveResult r = build(s.results, s.spec, s.cat);
  const auto issues = analyse(r);
  REQUIRE(issues.empty());
  const auto cmp = compare_cases(r);
  const std::string json = to_json(r, issues, cmp);
  CHECK(to_json(r, issues, cmp) == json);
  CHECK(json.rfind("{\n \"format\": \"fusion.results\",\n \"version\": 1,\n \"solve\": {", 0) == 0);   // one-space indent
  const ResultsFile back = read_results(json);
  CHECK(back.result == r);
  CHECK(back.comparison == cmp);
  CHECK(back.issues.empty());
  CHECK(from_json(json) == r);
  CHECK(to_json(from_json(json), issues, cmp) == json);
  // Witnesses name nodes and edges by key, never by index (E1, S4.14); edges also give the key's fields.
  const auto j = core::Json::parse(json);
  CHECK(j["cases"][0]["points"][0]["witnesses"][0]["selected"][0] == "f|t|PPrel|expression");
  CHECK(j["edges"][0]["key"] == "d|t|PPrel|inhibition");
  CHECK(j["edges"][0]["subtypes"][0] == "inhibition");
  CHECK(j["solve"]["constraints"].contains("coexpression"));
  CHECK_FALSE(j["solve"]["constraints"].contains("frozen"));   // only the families used (S5.03)
  CHECK(j["cases"][1]["points"][0]["certified"] == false);
  CHECK(j["comparison"]["verdict"] == "identical");
  CHECK(json.find("/tmp") == std::string::npos);
}

TEST_CASE("the one reader refuses a newer version, another format and broken JSON", "[unit][results]") {
  Solved s;
  SolveResult r = build(s.results, s.spec, s.cat);
  const auto issues = analyse(r);
  std::string json = to_json(r, issues, compare_cases(r));
  const auto code_of = [](const std::string& text) {
    try {
      (void)from_json(text);
    } catch (const core::Failure& f) {
      return f.code();
    }
    return std::string("none");
  };
  std::string newer = json;
  newer.replace(newer.find("\"version\": 1"), 12, "\"version\": 2");
  CHECK(code_of(newer) == "artefact.newer_format");
  try {
    (void)from_json(newer);
  } catch (const core::Failure& f) {
    CHECK_THAT(std::string(f.what()), ContainsSubstring("format fusion.results version 2 is newer than this FuSION reads (1)"));
  }
  std::string other = json;
  other.replace(other.find("fusion.results"), 14, "fusion.network");
  CHECK(code_of(other) == "artefact.invalid");
  CHECK(code_of(json.substr(0, json.size() / 2)) == "artefact.invalid");
  CHECK(code_of("[]") == "artefact.invalid");
  std::string unknown_word = json;
  unknown_word.replace(unknown_word.find("\"status\": \"complete\""), 20, "\"status\": \"finished\"");
  CHECK(code_of(unknown_word) == "artefact.invalid");
  CHECK(code_of(json) == "none");
}

TEST_CASE("publish writes results.json, the .dat files and the legacy files, and prints the frontier",
          "[unit][results]") {
  Solved s;
  SolveResult r = build(s.results, s.spec, s.cat);
  fx::Step step("pathz3", {legacy("temp_w")});
  auto ctx = step.context();
  const core::Json summary = publish_result(r, s.results, ctx);
  CHECK(std::filesystem::exists(step.dir() / "results.json"));
  CHECK(step.file("in.dat") == "1\t0\n");
  CHECK(step.file("out.dat") == "1\t0\n");
  CHECK(std::filesystem::exists(step.dir() / "legacy" / "temp_w_in_solutions.txt"));
  CHECK(std::filesystem::exists(step.dir() / "legacy" / "temp_w_out_relaxations.txt"));
  CHECK(from_json(step.file("results.json")) == r);
  const std::vector<std::string> expected{
      "with FSC (JUN): 1 Pareto point, certified",
      "  n=1 e=0",
      "without FSC (JUN): 1 Pareto point, not certified",
      "  n=1 e=0*",
      "  * not certified: blocked by the undecided cell n=0 e=0 (TIMEOUT)",
      "comparison: forcing JUN in changes nothing: both frontiers are identical (not certified)"};
  REQUIRE(step.lines().size() == expected.size() + 1);
  for (std::size_t i = 0; i < expected.size(); ++i) CHECK(step.lines()[i] == expected[i]);
  CHECK_THAT(step.lines().back(), ContainsSubstring("wrote pathz3/results.json ("));
  // Events: one results.frontier per case, then results.comparison (A2).
  REQUIRE(step.events().size() == 3);
  CHECK(step.events()[0].first == "results.frontier");
  CHECK(step.events()[1].second["points"][0]["certified"] == false);
  CHECK(step.events()[1].second["undecided"] == 1);
  CHECK(step.events()[2].first == "results.comparison");
  // The summary for run.json (2B.7).
  CHECK(summary["results"]["cases"]["in"]["status"] == "complete");
  CHECK(summary["results"]["cases"]["in"]["points"] == 1);
  CHECK(summary["results"]["cases"]["out"]["certified"] == 0);
  CHECK(summary["results"]["cases"]["in"]["least_total"] == 1);
  CHECK(summary["results"]["comparison"]["verdict"] == "identical");
}

TEST_CASE("an internal error fails the step only after results.json is written (S1.28)", "[unit][results]") {
  Solved s;
  // JUN inactive in the in case although up-regulated: an FSC violation (06.C10).
  s.results.cases[0].witnesses[0].active = {network::NodeKey{"s"}, network::NodeKey{"t"}};
  s.results.cases[0].witnesses[0].inactive = {network::NodeKey{"f"}};
  s.results.cases[0].witnesses[0].relaxed_edges = {{ek("s", "f", {"activation"}), true}, {ek("f", "t", {"expression"}), true}};
  s.results.cases[0].frontier.points[0].point = pt(1, 2);
  SolveResult r = build(s.results, s.spec, s.cat);
  fx::Step step("pathz3");
  auto ctx = step.context();
  try {
    publish_result(r, s.results, ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "results.internal");
    CHECK_THAT(std::string(f.what()), ContainsSubstring("FSC JUN (f) is inactive but is observed up"));
  }
  const ResultsFile kept = read_results(step.file("results.json"));
  REQUIRE(kept.issues.size() == 1);
  CHECK(kept.issues[0].code == "results.fsc");
}

TEST_CASE("the console names points n first, as section 07's full JUN run shows", "[unit][results]") {
  SolveResult r;
  r.tables.nodes.resize(3);
  r.tables.nodes[0].label = "EGFR";
  r.tables.nodes[1].label = "CXCL8";
  r.tables.nodes[2].label = "JUN";
  r.header.source = 0;
  r.header.target = 1;
  r.header.fsc = {2};
  CaseResult in = points_case(Case::in, {{pt(2, 6), true}, {pt(3, 4), true}, {pt(4, 3), true}, {pt(5, 2), true},
                                         {pt(6, 1), true}, {pt(7, 0), true}});
  CaseResult out = points_case(Case::out, {{pt(2, 7), true}, {pt(3, 5), true}, {pt(4, 4), true}, {pt(5, 3), true},
                                           {pt(6, 2), true}, {pt(7, 1), false}, {pt(8, 0), false}});
  out.undecided = {{pt(7, 0), "TIMEOUT"}};
  in.label = "with FSC (JUN)";
  out.label = "without FSC (JUN)";
  in.status_text = status_text(in);
  out.status_text = status_text(out);
  r.cases = {in, out};
  CHECK(frontier_lines(r, compare_cases(r)) ==
        std::vector<std::string>{"with FSC (JUN): 6 Pareto points, all certified",
                                 "  n=2 e=6 | n=3 e=4 | n=4 e=3 | n=5 e=2 | n=6 e=1 | n=7 e=0",
                                 "without FSC (JUN): 7 Pareto points, 2 not certified",
                                 "  n=2 e=7 | n=3 e=5 | n=4 e=4 | n=5 e=3 | n=6 e=2 | n=7 e=1* | n=8 e=0*",
                                 "  * not certified: blocked by the undecided cell n=7 e=0 (TIMEOUT)",
                                 "comparison: forcing JUN in saves 1 node at e=0 (not certified)"});
  // The reference run without JUN, with FuSION 1's check count.
  CaseResult none;
  none.kind = Case::out;
  none.status = CaseStatus::no_point;
  none.label = "without FSC (JUN)";
  none.checks = {0, 4, 0};
  none.status_text = status_text(none);
  r.cases = {none};
  CHECK(frontier_lines(r, std::nullopt) ==
        std::vector<std::string>{"without FSC (JUN): no Pareto point within the bounds (4 checks, all unsat)"});
  none.status = CaseStatus::not_requested;
  none.status_text = status_text(none);
  r.cases = {none};
  CHECK(frontier_lines(r, std::nullopt) == std::vector<std::string>{"without FSC (JUN): not solved in this run"});
}
