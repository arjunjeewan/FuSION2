// The end-to-end helpers (section 12, "End-to-end tests"; the C5 note's C5.3 and C5.4): the normaliser's
// mutation self-test, compare_tree, run_fusion and read_run on a toy run of the real fusion, and the
// recording StepContext.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>

#include "fusion/core/core.hpp"
#include "fusion/testkit/check.hpp"
#include "fusion/testkit/e2e.hpp"
#include "fusion/testkit/run_facts.hpp"
#include "fusion/testkit/scratch.hpp"
#include "fusion/testkit/step_context.hpp"

#ifndef FUSION_EXE
#error "test_testkit is built with FUSION_EXE, the staged fusion"
#endif

using namespace fusion::testkit;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;
using fusion::core::Json;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// A small run directory as fusion writes one: run.json, two interleaved case streams in events.jsonl and
// a search log, with absolute paths under the run, the case and the data root.
void fake_run(const fs::path& run, const fs::path& case_dir, const fs::path& data, int pid, bool interleave) {
  Json r = {{"format", "fusion.run"}, {"run", "r-" + std::to_string(pid)}, {"name", "toy_" + std::to_string(pid)},
            {"status", "ok"}, {"exit", 0}, {"pid", pid}, {"started", "2026-11-26T10:00:0" + std::to_string(pid % 10) + "Z"},
            {"build_id", "2.0.0+" + std::to_string(pid)}, {"versions", {{"fusion", {{"git_commit", std::to_string(pid)}}}, {"libraries", {{"z3", "4.8.12.0"}}}}},
            {"launch_dir", fusion::core::to_utf8(case_dir)},
            {"inputs", {{{"step", "cudf"}, {"path", fusion::core::to_utf8(data / "expression" / "egf.tsv")}, {"copy", "inputs/egf.tsv"}},
                        {{"step", "gsea"}, {"path", "/opt/fusion/share/data/list.txt"}, {"copy", "inputs/list.txt"}}}},
            {"steps", {{{"id", "pathz3"}, {"status", "ok"}, {"started", "x" + std::to_string(pid)}, {"wall_s", pid * 0.5},
                        {"settings", {{{"key", "solve.fsc"}, {"value", {"hsa5"}}, {"source", "named"}}}},
                        {"outputs", {{{"path", "pathz3/in.dat"}, {"size", 10 + pid},
                                      {"sha256", fusion::core::Sha256::of(std::to_string(pid)).hex()},
                                      {"also", fusion::core::to_utf8(case_dir / "in.dat")}}}}}}}};
  write(run / "run.json", r.dump(2));
  std::vector<Json> events = {
      {{"v", 1}, {"seq", 1}, {"t", "t1"}, {"type", "run.started"}, {"run", "r-" + std::to_string(pid)},
       {"name", "toy_" + std::to_string(pid)}, {"parent", "p-" + std::to_string(pid)}, {"pid", pid},
       {"dir", fusion::core::to_utf8(run)}},
      {{"v", 1}, {"seq", 2}, {"t", "t2"}, {"type", "solve.check"}, {"step", "pathz3"}, {"case", "in"}, {"check", 1}, {"n", 0},
       {"e", 1}, {"outcome", "unsat"}, {"source", "z3"}, {"wall_s", 0.1 * pid}},
      {{"v", 1}, {"seq", 3}, {"t", "t3"}, {"type", "solve.check"}, {"step", "pathz3"}, {"case", "out"}, {"check", 1}, {"n", 0},
       {"e", 1}, {"outcome", "sat"}, {"source", "cache"}, {"wall_s", 0.2}},
      {{"v", 1}, {"seq", 4}, {"t", "t4"}, {"type", "solve.check"}, {"step", "pathz3"}, {"case", "in"}, {"check", 2}, {"n", 1},
       {"e", 1}, {"outcome", "sat"}, {"source", "z3"}, {"wall_s", 0.3}},
      {{"v", 1}, {"seq", 5}, {"t", "t5"}, {"type", "network.read"}, {"step", "rgx"}, {"path", fusion::core::to_utf8(case_dir / "inputs" / "g.xml")},
       {"ms", pid}}};
  if (interleave) std::swap(events[2], events[3]);         // another interleaving of the two cases' streams
  std::string lines;
  for (const Json& e : events) lines += e.dump() + "\n";
  write(run / "events.jsonl", lines);
  write(run / "pathz3" / "in" / "search_log.tsv",
        "check\tphase\tn\te\tsource\toutcome\twall_s\tcpu_s\n1\tdiagonal\t0\t1\tz3\tunsat\t0." + std::to_string(pid) +
            "\t0.01\n");
  write(run / "pathz3" / "in" / "case.json",
        Json{{"case", "in"}, {"status", "complete"}, {"wall_s", 0.1 * pid}, {"cpu_clock_s", 1e-9 * pid}}.dump(1));
}

}  // namespace

TEST_CASE("testkit: the normaliser masks only the volatile fields and writes the path tokens (C5.3, C5.4)", "[unit][testkit]") {
  ScratchDir dir("normaliser");
  const fs::path a_case = dir.path() / "a", b_case = dir.path() / "b", data = dir.path() / "data";
  fake_run(a_case / "run", a_case, data, 1, false);
  fake_run(b_case / "run", b_case, data, 2, true);          // other volatile values, the other interleaving
  Normaliser na = fusion_normaliser(a_case / "run", a_case, DataRoot{data});
  Normaliser nb = fusion_normaliser(b_case / "run", b_case, DataRoot{data});
  const FileRule log{"pathz3/*/search_log.tsv", Compare::tsv_masked, {"wall_s", "cpu_s", "source"}};
  const FileRule case_file{"pathz3/*/case.json", Compare::exact, {}};
  for (Normaliser* n : {&na, &nb}) {
    n->rules.push_back(log);
    n->rules.push_back(case_file);
  }
  // Each normalised file of run a is the golden of run b: equal after the masks and tokens.
  ScratchDir golden_dir("golden");
  for (const auto& [rel, rule] : matched_files(a_case / "run", na))
    write(golden_dir.path() / rel, normalise_file(a_case / "run" / rel, rule, na));
  CHECK(matched_files(a_case / "run", na).size() == 4);
  const TreeDiff same = compare_tree(b_case / "run", golden_dir.path(), nb);
  INFO((same.differing.empty() ? std::string() : same.differing.front()));
  CHECK(same.empty());
  const std::string run_json = read(golden_dir.path() / "run.json");
  CHECK_THAT(run_json, ContainsSubstring("\"launch_dir\": \"<masked>\""));   // 09's volatile fields
  CHECK_THAT(run_json, ContainsSubstring("\"also\": \"<CASE>/in.dat\""));     // a path token, not a mask (C5.4)
  CHECK_THAT(run_json, ContainsSubstring("\"<DATA>/expression/egf.tsv\""));
  CHECK_THAT(run_json, ContainsSubstring("\"path\": \"<masked>\""));     // an absolute path outside the three roots
  CHECK_THAT(read(golden_dir.path() / "events.jsonl"), ContainsSubstring("\"path\":\"<CASE>/inputs/g.xml\""));
  // A changed unmasked field is a difference, in each mode.
  const auto mutated = [&](const std::string& rel, const std::string& from, const std::string& to) {
    std::string text = read(b_case / "run" / rel);
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    write(b_case / "run" / rel, text);
    const TreeDiff d = compare_tree(b_case / "run", golden_dir.path(), nb);
    fake_run(b_case / "run", b_case, data, 2, true);
    return d;
  };
  CHECK(mutated("run.json", "\"status\": \"ok\"", "\"status\": \"failed\"").differing.size() == 1);
  CHECK(mutated("run.json", "\"4.8.12.0\"", "\"4.16.0.0\"").differing.size() == 1);   // the Z3 version stays
  CHECK(mutated("events.jsonl", "\"outcome\":\"unsat\"", "\"outcome\":\"sat\"").differing.size() == 1);
  CHECK(mutated("pathz3/in/search_log.tsv", "\tunsat\t", "\tsat\t").differing.size() == 1);
  CHECK(mutated("pathz3/in/case.json", "\"complete\"", "\"stopped\"").differing.size() == 1);
  // A changed masked field is not.
  CHECK(mutated("run.json", "\"pid\": 2", "\"pid\": 99").empty());
  CHECK(mutated("events.jsonl", "\"source\":\"cache\"", "\"source\":\"model\"").empty());
  CHECK(mutated("events.jsonl", "\"parent\":\"p-2\"", "\"parent\":\"p-99\"").empty());   // a resumed run's parent
  CHECK(mutated("pathz3/in/search_log.tsv", "\t0.01\n", "\t9.99\n").empty());
  CHECK(mutated("run.json", "\"size\": 12", "\"size\": 99").empty());          // an output's size and SHA-256
  // Within one case's stream the order is compared (C5.3); a file a rule matches with no golden is unexpected.
  std::vector<std::string> event_lines;
  {
    std::istringstream in(read(b_case / "run" / "events.jsonl"));
    for (std::string l; std::getline(in, l);) event_lines.push_back(l);
  }
  std::vector<std::size_t> in_case;
  for (std::size_t k = 0; k < event_lines.size(); ++k)
    if (event_lines[k].find("\"case\":\"in\"") != std::string::npos) in_case.push_back(k);
  REQUIRE(in_case.size() == 2);
  std::swap(event_lines[in_case[0]], event_lines[in_case[1]]);
  std::string swapped;
  for (const auto& l : event_lines) swapped += l + "\n";
  write(b_case / "run" / "events.jsonl", swapped);
  CHECK(compare_tree(b_case / "run", golden_dir.path(), nb).differing.size() == 1);
  write(b_case / "run" / "pathz3" / "out" / "search_log.tsv", "check\n");
  const TreeDiff extra = compare_tree(b_case / "run", golden_dir.path(), nb);
  CHECK(extra.unexpected == std::vector<std::string>{"pathz3/out/search_log.tsv"});
  fs::remove(golden_dir.path() / "run.json");
  CHECK(compare_tree(b_case / "run", golden_dir.path(), nb).unexpected.size() == 2);
  CHECK(compare_tree(golden_dir.path(), b_case / "run", nb).missing.size() == 2);
}

TEST_CASE("testkit: run_fusion and read_run on a toy run: every witness passes the checker at its point",
          "[unit][testkit][e2e]") {
  // The relabelled toy network of booking 1 (no KEGG content), solved in both FSC cases by the real fusion.
  const fs::path toy = source_dir() / "reference" / "synthetic" / "toy-hsa11-wni-tsv-b10";
  ScratchDir scratch("toy-run");
  fs::create_directories(scratch.path() / "inputs");
  for (const auto& e : fs::directory_iterator(toy / "inputs")) fs::copy_file(e.path(), scratch.path() / "inputs" / e.path().filename());
  write(scratch.path() / "toy.batch",
        "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
        "cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
        "rgx file=inputs/with_node_included.xml tool_written=y\n"
        "fb_rch source=hsa1 target=hsa22 bound=10\n"
        "pathz3 fsc=hsa11 node_max=7 edge_max=7 timeout=150s incremental_timeout=30s\n");
  const fs::path exe = fusion::core::path_from_utf8(FUSION_EXE);
  const std::vector<std::string> args{"-b", "toy.batch", "--run-dir", "run"};
  const std::vector<std::pair<std::string, std::string>> env{
      {"FUSION_HOME", fusion::core::to_utf8(exe.parent_path().parent_path() / "share" / "fusion")}};
  const RunResult ran = run_fusion(exe, args, scratch, env);
  INFO(ran.out << ran.err);
  REQUIRE(ran.exit_code == 0);
  CHECK(ran.run_dir == scratch.path() / "run");
  const RunFacts facts = read_run(ran.run_dir);
  CHECK(facts.status == "ok");
  CHECK(facts.exit == 0);
  REQUIRE(facts.solves.size() == 1);
  const SolveFacts& solve = facts.solves[0];
  CHECK(solve.step == "pathz3");
  CHECK(solve.corridor.graph.nodes.size() == 20);           // the whole corridor, not only the witnesses' nodes
  CHECK(solve.corridor.graph.edges.size() == 22);
  CHECK(solve.corridor.sources == std::vector<std::string>{"hsa1"});
  CHECK(solve.corridor.bound == 10);
  REQUIRE(solve.cases.size() == 2);
  CHECK(solve.cases[0].name == "in");
  CHECK(solve.cases[0].frontier.points == std::vector<Point>{{2, 7}, {3, 6}, {4, 5}, {6, 3}});   // FuSION 1's .dat
  CHECK(solve.cases[0].status == "complete");
  for (const SolveFacts::Case& c : solve.cases) {
    CHECK(c.spec.fsc == std::vector<std::string>{"hsa11"});
    CHECK(c.spec.named);
    REQUIRE(c.witnesses.size() == c.frontier.points.size());
    for (std::size_t k = 0; k < c.witnesses.size(); ++k) {
      const Verdict v = check(solve.corridor, c.spec, c.witnesses[k], Semantics::fusion2);
      INFO(c.name << " " << k << ": " << (v.violations.empty() ? std::string() : v.violations.front()));
      CHECK(v.valid);
      CHECK(v.cost == c.frontier.points[k]);
    }
  }
  // results mode: the run against itself, then against a golden with another frontier.
  Normaliser n = fusion_normaliser(ran.run_dir, scratch.path(), std::nullopt);
  n.rules.push_back({"pathz3/results.json", Compare::results, {}});
  CHECK(compare_tree(ran.run_dir, ran.run_dir, n).empty());
  ScratchDir golden("toy-golden");
  Json results = Json::parse(read(ran.run_dir / "pathz3" / "results.json"));
  results["cases"][0]["points"][0]["n"] = 1;
  write(golden.path() / "pathz3" / "results.json", results.dump());
  const Normaliser only_results{{}, {}, {{"pathz3/results.json", Compare::results, {}}}};
  const TreeDiff d = compare_tree(ran.run_dir, golden.path(), only_results);
  REQUIRE(d.differing.size() == 1);
  CHECK_THAT(d.differing[0], ContainsSubstring("frontier"));
}

TEST_CASE("testkit: the recording StepContext keeps log lines, events, files and journals", "[unit][testkit]") {
  ScratchDir dir("step-context");
  StepContext step(dir.path() / "pathz3", "pathz3");
  fusion::core::Context& ctx = step.ctx();
  CHECK(ctx.step == "pathz3");
  ctx.log.info("[solve] done");
  ctx.log.warning("solve.bound_clamped", "node_max 8 -> 3");
  ctx.events.emit("solve.point", Json{{"n", 1}});
  ctx.events.emit("solve.check", Json{{"n", 0}});
  ctx.files.journal("journal.jsonl").append(Json{{"check", 1}});
  auto out = ctx.files.create("in/case.json");
  out.out() << "{}\n";
  out.commit();
  CHECK(step.logged("[solve] done"));
  CHECK(step.logged("solve.bound_clamped"));
  CHECK_FALSE(step.logged("nothing"));
  REQUIRE(step.lines().size() == 2);
  CHECK(step.lines()[1].level == fusion::core::Level::warning);
  CHECK(step.events().size() == 2);
  REQUIRE(step.events("solve.point").size() == 1);
  CHECK(step.events("solve.point")[0]["n"] == 1);
  REQUIRE(step.created().size() == 1);
  CHECK(step.created()[0] == dir.path() / "pathz3" / "in" / "case.json");
  CHECK(read(step.created()[0]) == "{}\n");
  CHECK(step.journal("journal.jsonl").size() == 1);
  CHECK(ctx.files.step_dir() == dir.path() / "pathz3");
  CHECK_FALSE(ctx.stop.stop_requested());
  step.stop().request_stop();
  CHECK(ctx.stop.stop_requested());
}
