// The step's driver (06, "The driver"): cases solved in parallel or in turn (06.C4), extra
// solutions (06.C11), Stop (A1) and the budget (06.C3), the events (A2), the console (S5.31) and
// each case's files (S5.30, S6.21).
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <mutex>

#include "fusion/core/core.hpp"
#include "kit.hpp"
#include "step.hpp"

using namespace solvekit;
namespace core = fusion::core;
using solve::Case;
using solve::CaseStatus;

namespace {

std::vector<std::variant<solve::Problem, solve::Skipped>> toy_cases(const ToyCase& toy, const solve::Settings& settings) {
  solve::CaseSpec spec = toy.observations;
  spec.fsc = ids({toy.fsc});
  std::vector<std::variant<solve::Problem, solve::Skipped>> out;
  for (Case c : solve::requested_cases(settings)) out.push_back(solve::make_case(toy.graph, spec, settings, c));
  return out;
}

solve::Settings toy_settings(const ToyCase& toy) {
  solve::Settings settings;
  settings.fsc = {toy.fsc};
  settings.box = toy.box;
  return settings;
}

std::size_t count_lines(const std::string& text) { return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')); }

}  // namespace

TEST_CASE("solve: the step solves both FSC cases and writes each case's files and events", "[unit][solve][06.C10][S5.24][S5.30]") {
  const ToyCase toy = read_toy("toy-hsa5-wni-tsv-b10");
  const solve::Settings settings = toy_settings(toy);
  Step step;
  core::Context ctx = step.context();
  const core::Outcome<solve::Results> outcome = solve::solve_cases(toy_cases(toy, settings), settings, ctx);
  CHECK_FALSE(outcome.report);                               // a solve makes no Report (1.8)
  CHECK_FALSE(outcome.partial);
  const solve::Results& results = outcome.value;
  REQUIRE(results.cases.size() == 2);
  CHECK(results.cases[0].kase == Case::in);
  CHECK(results.cases[1].kase == Case::out);
  CHECK(points_of(results.cases[0].frontier) == toy.with.dat);
  CHECK(points_of(results.cases[1].frontier) == toy.without.dat);
  for (const solve::CaseResult& r : results.cases) {
    INFO(solve::name(r.kase));
    CHECK(r.status == CaseStatus::complete);
    REQUIRE(r.problem);
    for (std::size_t k = 0; k < r.log.size(); ++k) CHECK(r.log[k].check == k + 1);
    for (const solve::FrontierPoint& f : r.frontier.points) {
      REQUIRE(f.witness < r.witnesses.size());
      CHECK(check(*r.problem, r.witnesses[f.witness]).valid);
    }
    CHECK(r.stats.z3 + r.stats.cache + r.stats.model == r.log.size());
    CHECK(r.stats.fingerprint.rfind("sha256:", 0) == 0);
    CHECK(r.stats.nodes == r.problem->nodes.size());
    const std::string dir = std::string(solve::name(r.kase)) + "/";
    const core::Json json = core::Json::parse(step.read(dir + "case.json"));
    CHECK(json["case"] == solve::name(r.kase));
    CHECK(json["status"] == "complete");
    CHECK(json["fsc"] == core::Json::array({"hsa5"}));
    CHECK(json["seed"] == 42);
    CHECK(json["z3"] == "4.8.12.0");
    CHECK(json["box"]["requested"]["n"] == core::Json::array({0, 7}));
    CHECK(json["encoded"]["nodes"] == r.stats.nodes);
    CHECK(json["families"]["coexpression"].is_null());
    CHECK(json["limits"]["timeout_ms"] == 14'400'000);
    CHECK(json["rules"].size() == 4);
    const std::string log = step.read(dir + "search_log.tsv");
    CHECK(log.rfind("check\tphase\tn\te\tsource\toutcome\twall_s\tcpu_s\ttimeout_ms\trlimit\tmodel_n\tmodel_e\treason\n", 0) == 0);
    CHECK(count_lines(log) == r.log.size() + 1);
    const std::string observations = step.read(dir + "observations.tsv");
    CHECK(observations.rfind("gene\tsymbol\tobserved\trole\n", 0) == 0);
    CHECK(count_lines(observations) == r.observations.size() + 1);
    CHECK(observations.find("has16\thas16\tdown\tnot_in_network\n") != std::string::npos);   // the toy's own typo
  }
  CHECK(step.read("out/observations.tsv").find("hsa5\thsa5\tup\tremoved_with_fsc\n") != std::string::npos);
  CHECK(step.read("in/observations.tsv").find("hsa5\thsa5\tup\tforced\n") != std::string::npos);

  // The events (A2): every query, every Z3 check started, each point, each case.
  std::size_t queries = 0, z3 = 0;
  for (const solve::CaseResult& r : results.cases) {
    queries += r.log.size();
    z3 += r.stats.z3;
  }
  CHECK(step.events.of("solve.check").size() == queries);
  CHECK(step.events.of("solve.check.started").size() == z3);
  CHECK(step.events.of("solve.case.started").size() == 2);
  const auto finished = step.events.of("solve.case.finished");
  REQUIRE(finished.size() == 2);
  for (const core::Json& f : finished) CHECK(f["status"] == "complete");
  CHECK(step.events.of("solve.point").size() >= toy.with.dat.size() + toy.without.dat.size());
  CHECK(step.lines->any("[solve in] complete: "));
  CHECK(step.lines->any("[solve out] point n="));
  CHECK(step.lines->any("[solve] done in "));                 // always printed (S5.31)
}

TEST_CASE("solve: cases in parallel give the same answers and witnesses as one after the other", "[unit][solve][06.C4]") {
  const ToyCase toy = read_toy("toy-hsa18-wni-tsv-b10");
  solve::Settings parallel = toy_settings(toy);
  parallel.cases = {Case::in, Case::out, Case::free};
  solve::Settings sequential = parallel;
  sequential.threads = 1;
  Step a, b;
  core::Context ca = a.context(), cb = b.context();
  const auto ra = solve::solve_cases(toy_cases(toy, parallel), parallel, ca).value;
  const auto rb = solve::solve_cases(toy_cases(toy, sequential), sequential, cb).value;
  REQUIRE(ra.cases.size() == 3);
  REQUIRE(rb.cases.size() == 3);
  for (std::size_t c = 0; c < 3; ++c) {
    INFO(solve::name(ra.cases[c].kase));
    REQUIRE(ra.cases[c].log.size() == rb.cases[c].log.size());
    for (std::size_t k = 0; k < ra.cases[c].log.size(); ++k) {
      CHECK(ra.cases[c].log[k].point == rb.cases[c].log[k].point);
      CHECK(ra.cases[c].log[k].outcome == rb.cases[c].log[k].outcome);
      CHECK(ra.cases[c].log[k].model == rb.cases[c].log[k].model);
    }
    REQUIRE(ra.cases[c].witnesses.size() == rb.cases[c].witnesses.size());
    for (std::size_t k = 0; k < ra.cases[c].witnesses.size(); ++k)
      CHECK(ra.cases[c].witnesses[k].selected == rb.cases[c].witnesses[k].selected);
    CHECK(points_of(ra.cases[c].frontier) == points_of(rb.cases[c].frontier));
  }
}

TEST_CASE("solve: a skipped case still writes its files, with an empty search log", "[unit][solve][06.C15][S6.21]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "ELSEWHERE", 'u');
  solve::Settings settings;
  std::vector<std::variant<solve::Problem, solve::Skipped>> cases{solve::make_case(g.g, spec, settings, Case::free)};
  Step step;
  core::Context ctx = step.context();
  const auto results = solve::solve_cases(std::move(cases), settings, ctx).value;
  REQUIRE(results.cases.size() == 1);
  CHECK(results.cases[0].status == CaseStatus::no_de_gene);
  CHECK(results.cases[0].log.empty());
  CHECK(results.cases[0].frontier.points.empty());
  CHECK(core::Json::parse(step.read("free/case.json"))["status"] == "no_de_gene");
  CHECK(count_lines(step.read("free/search_log.tsv")) == 1);
  CHECK(step.read("free/observations.tsv").find("ELSEWHERE\tELSEWHERE\tup\tnot_in_network") != std::string::npos);
  CHECK(step.lines->any("no differentially expressed gene in the corridor"));
}

TEST_CASE("solve: solutions=k keeps up to k witnesses per frontier point", "[unit][solve][06.C11]") {
  Graph g(10);
  g.edge("S", "A");
  g.edge("A", "T");
  g.edge("S", "B");
  g.edge("B", "T");
  g.ends("S", "T");
  solve::CaseSpec spec;
  observe(spec, g.g, "S", 'u');
  solve::Settings settings;
  settings.solutions = 5;
  std::vector<std::variant<solve::Problem, solve::Skipped>> cases{solve::make_case(g.g, spec, settings, Case::free)};
  Step step;
  core::Context ctx = step.context();
  const auto results = solve::solve_cases(std::move(cases), settings, ctx).value;
  const solve::CaseResult& r = results.cases.at(0);
  REQUIRE(r.frontier.points.size() == 1);
  CHECK(r.frontier.points[0].more.size() == 2);              // three witnesses in all, then "no more"
  const auto extra = std::count_if(r.log.begin(), r.log.end(), [](const solve::Answer& a) { return a.phase == Phase::extra; });
  CHECK(extra == 3);
  CHECK(r.log.back().outcome == solve::Outcome::unsat);
  for (std::uint32_t w : r.frontier.points[0].more) CHECK(check(*r.problem, r.witnesses[w]).valid);
  CHECK(r.frontier.points[0].certified);                     // the extras' "no more" proves nothing
}

TEST_CASE("solve: Stop ends every case stopped and returns the partial results", "[unit][solve][A1][09.C14]") {
  const ToyCase toy = read_toy("toy-hsa7-wni-tsv-b10");
  const solve::Settings settings = toy_settings(toy);
  Step step;
  step.stop.request_stop();                                  // before the first check
  core::Context ctx = step.context();
  const auto outcome = solve::solve_cases(toy_cases(toy, settings), settings, ctx);
  CHECK(outcome.partial);
  REQUIRE(outcome.value.cases.size() == 2);
  for (const solve::CaseResult& r : outcome.value.cases) {
    CHECK(r.status == CaseStatus::stopped);
    REQUIRE_FALSE(r.log.empty());
    CHECK(r.log.back().outcome == solve::Outcome::stopped);
    CHECK(core::Json::parse(step.read(std::string(solve::name(r.kase)) + "/case.json"))["status"] == "stopped");
  }
  CHECK(step.lines->any("[solve] stopped by request after "));
}

TEST_CASE("solve: a spent budget ends the unfinished cases as budget_exhausted", "[unit][solve][06.C3]") {
  const ToyCase toy = read_toy("toy-hsa13-wni-tsv-b15");
  solve::Settings settings = toy_settings(toy);
  settings.budget = std::chrono::milliseconds(1);
  Step step;
  core::Context ctx = step.context();
  const auto outcome = solve::solve_cases(toy_cases(toy, settings), settings, ctx);
  CHECK(outcome.partial);                                    // resumable, not a failure
  for (const solve::CaseResult& r : outcome.value.cases) {
    CHECK(r.status == CaseStatus::budget_exhausted);
    for (const solve::FrontierPoint& f : r.frontier.points) CHECK(check(*r.problem, r.witnesses[f.witness]).valid);
  }
  CHECK(step.lines->any("used: "));
}

// The monitor interrupts a case until its thread returns, so an interrupt can land in an add, a push
// or a model read, where Z3 throws "canceled" instead of answering unknown. Many tiny budgets hit
// those places; each must end the cases as budget_exhausted, never as a Z3 failure.
TEST_CASE("solve: an interrupt outside a check ends the case, not the step", "[unit][solve][06.C3]") {
  const ToyCase toy = read_toy("toy-hsa13-wni-tsv-b15");
  for (int round = 0; round < 60; ++round) {
    INFO("round " << round);
    solve::Settings settings = toy_settings(toy);
    settings.budget = std::chrono::milliseconds(1 + round % 3);
    Step step;
    core::Context ctx = step.context();
    const auto outcome = solve::solve_cases(toy_cases(toy, settings), settings, ctx);
    for (const solve::CaseResult& r : outcome.value.cases)
      CHECK((r.status == CaseStatus::budget_exhausted || r.status == CaseStatus::complete));
  }
}

TEST_CASE("solve: the step's settings are checked before any case", "[unit][solve][0021]") {
  const ToyCase toy = read_toy("toy-hsa5-wni-tsv-b10");
  solve::Settings settings = toy_settings(toy);
  Step step;
  core::Context ctx = step.context();
  const auto code = [&](const solve::Settings& s) {
    try {
      solve::solve_cases({}, s, ctx);
    } catch (const core::Failure& f) {
      return f.code();
    }
    return std::string();
  };
  solve::Settings box = settings;
  box.box = {{3, 0}, {2, 8}};
  CHECK(code(box) == "batch.bad_argument");
  solve::Settings none = settings;
  none.solutions = 0;
  CHECK(code(none) == "batch.bad_argument");
  solve::Settings points = settings;
  points.points = {{1, 1}};
  CHECK(code(points) == "solve.unsupported");               // 06.C9 comes later
  CHECK(code(settings).empty());
  CHECK_THROWS_AS(solve::load_saved(step.dir), core::Failure);   // 06.C7 comes later
}
