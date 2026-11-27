// fusion-test e2e on toy cases the tests write (the C3 note's engine; the C5 note's case.json; section 12,
// "End-to-end tests"). Most cases run a stand-in for fusion, a POSIX shell script that runs the case's batch
// as shell code, so each check of the engine is met and missed on purpose; one case runs the real fusion on
// the relabelled toy network, records its golden with --update and must pass against it in a second run.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <exception>
#include <fstream>
#include <sstream>

#include "e2e/case.hpp"
#include "e2e/engine.hpp"
#include "fusion/core/core.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"

#ifndef FUSION_EXE
#error "test_fusion_test is built with FUSION_EXE, the staged fusion"
#endif

using namespace fusion::fusiontest;
using namespace fusion::fusiontest::e2e;
using Catch::Matchers::ContainsSubstring;
using fusion::core::Json;
using Outcome = CaseResult::Outcome;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string joined(const std::vector<std::string>& notes) {
  std::string out;
  for (const std::string& n : notes) out += n + "\n";
  return out;
}

// Removes the scratch folders that a report says were kept for failing cases.
void remove_kept(const std::string& report) {
  std::istringstream in(report);
  const std::string mark = "scratch folder kept: ";
  for (std::string line; std::getline(in, line);)
    if (const auto at = line.find(mark); at != std::string::npos) fs::remove_all(line.substr(at + mark.size()));
}

// fusion's stand-in: -b <batch> runs the batch as shell code with $run set to --run-dir's folder; --stop <dir>
// writes <dir>/stop, as fusion --stop does.
const char* const fake_fusion = R"(#!/bin/sh
run=""
batch=""
while [ $# -gt 0 ]; do
  case "$1" in
    -b) batch="$2"; shift 2 ;;
    --run-dir) run="$2"; shift 2 ;;
    --stop) : > "$2/stop"; exit 0 ;;
    *) shift ;;
  esac
done
if [ -z "$batch" ]; then echo "error: no batch file" >&2; exit 2; fi
. "./$batch"
)";

// A run directory as fusion leaves one: run.json with a status, events.jsonl, a search log and a file that
// names the case's folder, with a pid and times that change from run to run, which the normaliser masks.
const char* const ok_run = R"(mkdir -p "$run"
printf '{"status": "ok", "exit": 0, "pid": %s, "launch_dir": "%s"}\n' $$ "$PWD" > "$run/run.json"
printf '{"v":1,"seq":1,"t":"%s","type":"run.started","pid":%s}\n' "$(date +%s)$$" $$ > "$run/events.jsonl"
printf '{"v":1,"seq":2,"t":"x","type":"run.finished","status":"ok"}\n' >> "$run/events.jsonl"
mkdir -p "$run/pathz3/in"
printf 'check\tn\te\toutcome\twall_s\n1\t0\t1\tunsat\t0.%s\n' $$ > "$run/pathz3/in/search_log.tsv"
printf '%s/inputs/own.txt\n' "$PWD" > "$run/paths.txt"
)";

// A toy e2e root, cases/ and inputs/, with the stand-in; the scratch folders of the cases it runs are removed
// with it, failing ones included, unless a REQUIRE ends the test.
struct Toy {
  fusion::testkit::ScratchDir dir{"fusion-test-e2e"};
  fs::path root = dir.path() / "e2e", fake = dir.path() / "fake-fusion";
  mutable std::vector<fs::path> scratches;
  Toy() {
#ifdef _WIN32
    SKIP("the stand-in for fusion is a POSIX shell script");
#endif
    write(fake, fake_fusion);
    fs::permissions(fake, fs::perms::owner_all);
    fs::create_directories(root / "cases");
  }
  Toy(const Toy&) = delete;
  Toy& operator=(const Toy&) = delete;
  ~Toy() {
    if (std::uncaught_exceptions() > 0) return;
    for (const fs::path& p : scratches) {
      std::error_code ec;
      fs::remove_all(p, ec);
    }
  }
  Settings settings() const {
    Settings s;
    s.fusion = fake;
    s.root = root;
    return s;
  }
  fs::path add(const std::string& id, Json json, const std::string& batch) const {
    const fs::path d = root / "cases" / id;
    json["case"] = id;
    if (!json.contains("args")) json["args"] = {"-b", "case.batch"};
    write(d / "case.json", json.dump(2));
    write(d / "case.batch", batch);
    return d;
  }
  CaseResult run(const std::string& id, bool update = false) const {
    CaseResult r = run_case(load_case(root / "cases" / id), settings(), update);
    if (!r.scratch.empty()) scratches.push_back(r.scratch);
    return r;
  }
};

const Json ok_expect = {{"status", "ok"}, {"exit", 0}};

}  // namespace

TEST_CASE("fusion-test e2e: --update writes the golden, a second run passes, a changed golden fails",
          "[unit][fusion-test]") {
  Toy t;
  const Json log_rule = {{"glob", "pathz3/*/search_log.tsv"}, {"mode", "tsv_masked"}, {"masked_columns", {"wall_s"}}};
  const fs::path d = t.add("toy-ok",
                           {{"expect", ok_expect},
                            {"tags", {"e2e", "S1.26"}},
                            {"compare", {log_rule, {{"glob", "paths.txt"}, {"mode", "exact"}}}}},
                           std::string(ok_run) + "[ -f inputs/own.txt ] || exit 9\n");
  write(d / "inputs" / "own.txt", "the case's own input\n");
  const CaseResult updated = t.run("toy-ok", true);
  INFO(joined(updated.notes));
  REQUIRE(updated.outcome == Outcome::passed);
  CHECK_THAT(joined(updated.notes), ContainsSubstring("added run.json"));
  CHECK_THAT(joined(updated.notes), ContainsSubstring("added pathz3/in/search_log.tsv"));
  CHECK(read(d / "expected" / "paths.txt") == "<CASE>/inputs/own.txt\n");   // the path token, not the scratch
  CHECK_THAT(read(d / "expected" / "run.json"), ContainsSubstring("\"pid\": \"<masked>\""));
  CHECK_FALSE(fs::exists(d / "expected" / "case.batch"));                    // only the files the rules match
  CHECK_FALSE(fs::exists(updated.scratch));                                  // a passing case leaves nothing

  const CaseResult passed = t.run("toy-ok");
  INFO(joined(passed.notes));
  CHECK(passed.outcome == Outcome::passed);
  CHECK(passed.notes.empty());

  std::string golden = read(d / "expected" / "run.json");
  golden.replace(golden.find("\"status\": \"ok\""), 14, "\"status\": \"failed\"");
  write(d / "expected" / "run.json", golden);
  const CaseResult failed = t.run("toy-ok");
  CHECK(failed.outcome == Outcome::failed);
  CHECK_THAT(joined(failed.notes), ContainsSubstring("differs run.json"));
  CHECK(failed.kept);
  CHECK(fs::is_directory(failed.scratch / "run"));                           // kept for inspection

  const CaseResult again = t.run("toy-ok", true);
  CHECK(again.outcome == Outcome::passed);
  CHECK_THAT(joined(again.notes), ContainsSubstring("changed run.json"));
  CHECK_THAT(joined(again.notes), ContainsSubstring("3 golden files unchanged"));
  write(d / "expected" / "pathz3" / "out" / "search_log.tsv", "check\n");     // a golden the run no longer writes
  CHECK_THAT(joined(t.run("toy-ok").notes), ContainsSubstring("missing pathz3/out/search_log.tsv"));
  CHECK_THAT(joined(t.run("toy-ok", true).notes), ContainsSubstring("removed pathz3/out/search_log.tsv"));
  CHECK_FALSE(fs::exists(d / "expected" / "pathz3" / "out" / "search_log.tsv"));
}

TEST_CASE("fusion-test e2e: the exit status, run.json's status, the batch line and the stderr fragment",
          "[unit][fusion-test]") {
  Toy t;
  // A check error: no run directory, and the line on stderr.
  const std::string check_error =
      "echo \"error: case.batch:3: no command frobnicate [batch.unknown_command]\" >&2\nexit 1\n";
  t.add("check-error", {{"expect", {{"exit", 1}, {"line", 3}, {"stderr", "no command frobnicate"}}}}, check_error);
  const CaseResult ok = t.run("check-error");
  INFO(joined(ok.notes));
  CHECK(ok.outcome == Outcome::passed);
  t.add("wrong-line", {{"expect", {{"exit", 1}, {"line", 4}, {"stderr", "no such file"}}}}, check_error);
  const CaseResult wrong = t.run("wrong-line");
  CHECK(wrong.outcome == Outcome::failed);
  CHECK_THAT(joined(wrong.notes), ContainsSubstring("stderr names no failure at case.batch:4:"));
  CHECK_THAT(joined(wrong.notes), ContainsSubstring("stderr does not hold \"no such file\""));
  CHECK_THAT(joined(wrong.notes), ContainsSubstring("| error: case.batch:3: no command frobnicate"));
  t.add("wrong-exit", {{"expect", {{"status", "ok"}, {"exit", 0}}}}, check_error);
  const std::string exit_notes = joined(t.run("wrong-exit").notes);
  CHECK_THAT(exit_notes, ContainsSubstring("exit 1, expected 0"));
  CHECK_THAT(exit_notes, ContainsSubstring("no run.json, so no status; expected ok"));
  // A step failure: run.json names the line.
  const std::string step_failure = R"(mkdir -p "$run"
printf '{"status": "failed", "exit": 1, "error": {"code": "input.missing", "line": 7}}\n' > "$run/run.json"
: > "$run/events.jsonl"
echo "error: case.batch:7: mff: no such file [input.missing]" >&2
exit 1
)";
  t.add("step-failure", {{"expect", {{"status", "failed"}, {"exit", 1}, {"line", 7}, {"stderr", "[input.missing]"}}}},
        step_failure);
  REQUIRE(t.run("step-failure", true).outcome == Outcome::passed);
  CHECK(t.run("step-failure").outcome == Outcome::passed);
  t.add("other-line", {{"expect", {{"status", "stopped"}, {"exit", 1}, {"line", 8}}}}, step_failure);
  const CaseResult other = t.run("other-line", true);
  CHECK(other.outcome == Outcome::failed);
  CHECK_THAT(joined(other.notes), ContainsSubstring("run.json's status is failed, expected stopped"));
  CHECK_THAT(joined(other.notes), ContainsSubstring("run.json's error is at batch line 7, expected 8"));
  CHECK_THAT(joined(other.notes), ContainsSubstring("expected/ was not written"));
  CHECK_FALSE(fs::exists(t.root / "cases" / "other-line" / "expected"));
  // The engine places the run unless the case does: --run-dir run is appended, but not to a case with --root.
  t.add("placed", {{"expect", {{"exit", 0}}}}, "[ \"$run\" = run ] || exit 7\n");
  CHECK(t.run("placed").outcome == Outcome::passed);
  t.add("own-root", {{"args", {"-b", "case.batch", "--root", "runs"}}, {"expect", {{"exit", 0}}}},
        "[ -z \"$run\" ] || exit 7\n");
  CHECK(t.run("own-root").outcome == Outcome::passed);
}

TEST_CASE("fusion-test e2e: a pending case is skipped while refused with its code, and fails once it runs (C5.2)",
          "[unit][fusion-test]") {
  Toy t;
  const Json pending = {{"waits_for", "01.C3"}, {"refused_with", "batch.unknown_command"}};
  const std::string refused =
      "echo \"error: case.batch:2: unknown command sweep [batch.unknown_command]\" >&2\nexit 1\n";
  t.add("still-refused", {{"expect", ok_expect}, {"pending", pending}}, refused);
  const CaseResult skipped = t.run("still-refused");
  CHECK(skipped.outcome == Outcome::skipped);
  CHECK_THAT(joined(skipped.notes), ContainsSubstring("waits for 01.C3"));
  t.add("now-runs", {{"expect", ok_expect}, {"pending", pending}}, ok_run);
  const CaseResult stale = t.run("now-runs");
  CHECK(stale.outcome == Outcome::failed);
  CHECK_THAT(joined(stale.notes), ContainsSubstring("stale"));
  CHECK_THAT(joined(stale.notes), ContainsSubstring("now runs it (exit 0)"));
  t.add("other-code", {{"expect", ok_expect}, {"pending", pending}},
        "echo \"error: x [solve.unsupported]\" >&2\nexit 1\n");
  CHECK_THAT(joined(t.run("other-code").notes), ContainsSubstring("exits 1 without [batch.unknown_command]"));
  CHECK(t.run("still-refused", true).outcome == Outcome::harness);            // no golden for a pending case
}

TEST_CASE("fusion-test e2e: shared inputs, env with path tokens, and a stop action on an event (the C5 note)",
          "[unit][fusion-test]") {
  Toy t;
  write(t.root / "inputs" / "toy" / "values.tsv", "G1\t2.5\n");
  write(t.root / "inputs" / "toy" / "tool.sh", "#!/bin/sh\nexit 0\n");   // a fake tool keeps its executable bit
  fs::permissions(t.root / "inputs" / "toy" / "tool.sh", fs::perms::owner_all);
  t.add("shared",
        {{"expect", ok_expect},
         {"inputs", {"toy"}},
         {"env", {{"TOY_VALUES", "<CASE>/toy/values.tsv"}}},
         {"compare", {{{"glob", "values.tsv"}, {"mode", "exact"}}}}},
        std::string(ok_run) + "cp \"$TOY_VALUES\" \"$run/values.tsv\"\n./toy/tool.sh || exit 8\n");
  const CaseResult shared = t.run("shared", true);
  INFO(joined(shared.notes));
  REQUIRE(shared.outcome == Outcome::passed);
  CHECK(read(t.root / "cases" / "shared" / "expected" / "values.tsv") == "G1\t2.5\n");
  t.add("no-set", {{"expect", ok_expect}, {"inputs", {"nothing"}}}, ok_run);
  CHECK(t.run("no-set").outcome == Outcome::harness);
  t.add("needs-data", {{"expect", ok_expect}, {"env", {{"KGML", "<DATA>/kegg"}}}}, ok_run);
  const CaseResult no_data = t.run("needs-data");                           // the toy settings name no data root
  CHECK(no_data.outcome == Outcome::skipped);
  CHECK_THAT(joined(no_data.notes), ContainsSubstring("<DATA> needs the data root"));

  // The stand-in writes solve.check.started, then waits for the stop file that the action's fusion --stop
  // writes, and ends stopped with exit 3 (A1).
  const std::string waits_for_stop = R"(mkdir -p "$run"
printf '{"status": "running"}\n' > "$run/run.json"
printf '{"v":1,"seq":1,"t":"a","type":"run.started"}\n' > "$run/events.jsonl"
printf '{"v":1,"seq":2,"t":"b","type":"solve.check.started"}\n' >> "$run/events.jsonl"
i=0
while [ ! -e "$run/stop" ] && [ $i -lt 600 ]; do sleep 0.05; i=$((i + 1)); done
if [ -e "$run/stop" ]; then printf '{"status": "stopped", "exit": 3}\n' > "$run/run.json"; exit 3; fi
printf '{"status": "ok", "exit": 0}\n' > "$run/run.json"
exit 0
)";
  const Json stop = {{"on_event", "solve.check.started"}, {"fusion", {"--stop", "<RUN>"}}};
  t.add("stopped", {{"expect", {{"status", "stopped"}, {"exit", 3}}}, {"actions", {stop}}, {"timeout_s", 60}},
        waits_for_stop);
  const CaseResult stopped = t.run("stopped", true);
  INFO(joined(stopped.notes));
  CHECK(stopped.outcome == Outcome::passed);
  CHECK(t.run("stopped").outcome == Outcome::passed);
  const Json never = {{"on_event", "solve.never"}, {"fusion", {"--stop", "<RUN>"}}};
  t.add("never", {{"expect", ok_expect}, {"actions", {never}}}, std::string(ok_run) + "exit 0\n");
  CHECK_THAT(joined(t.run("never").notes), ContainsSubstring("the action on solve.never never ran"));
}

TEST_CASE("fusion-test e2e: two processes in one launch directory, each run compared with the one golden (S1.26)",
          "[unit][fusion-test]") {
  Toy t;
  // As fusion does with no --run-dir: each process names its own run directory in the launch directory.
  const std::string own_run = std::string("[ -z \"$run\" ] || exit 7\nrun=$(mktemp -d \"$PWD/toy_XXXXXX\")\n") + ok_run;
  t.add("two", {{"expect", ok_expect}, {"processes", 2}}, own_run);
  const CaseResult recorded = t.run("two", true);
  INFO(joined(recorded.notes));
  REQUIRE(recorded.outcome == Outcome::passed);                    // the second run equals the golden of the first
  CHECK_THAT(joined(recorded.notes), ContainsSubstring("added run.json"));
  const CaseResult compared = t.run("two");
  INFO(joined(compared.notes));
  CHECK(compared.outcome == Outcome::passed);
  // Two processes in one run directory are not two runs.
  t.add("one-dir", {{"expect", ok_expect}, {"processes", 2}},
        std::string("run=\"$PWD/shared\"\n") + ok_run);
  CHECK_THAT(joined(t.run("one-dir").notes), ContainsSubstring("2 fusion processes made 1 run directories"));
  t.add("given-dir", {{"args", {"-b", "case.batch", "--run-dir", "run"}}, {"expect", ok_expect}, {"processes", 2}},
        ok_run);
  CHECK(t.run("given-dir").outcome == Outcome::harness);
}

TEST_CASE("fusion-test e2e: a case with no golden checks its expectations and compares no file (C5.1)",
          "[unit][fusion-test]") {
  Toy t;
  t.add("no-golden", {{"expect", ok_expect}}, ok_run);
  const CaseResult r = t.run("no-golden");
  INFO(joined(r.notes));
  CHECK(r.outcome == Outcome::passed);
  CHECK_THAT(joined(r.notes), ContainsSubstring("no golden"));
  t.add("no-golden-wrong", {{"expect", {{"status", "failed"}, {"exit", 0}}}}, ok_run);
  CHECK_THAT(joined(t.run("no-golden-wrong").notes), ContainsSubstring("run.json's status is ok, expected failed"));
}

TEST_CASE("fusion-test e2e: exit codes over several cases, and --list", "[unit][fusion-test]") {
  Toy t;
  std::ostringstream out, err;
  CHECK(run(t.settings(), {}, {}, out, err) == exit_skipped);                 // no case at all
  t.add("a-pass", {{"expect", {{"exit", 0}}}}, "exit 0\n");
  t.add("b-pending",
        {{"expect", ok_expect},
         {"tags", {"e2e", "S8.21"}},
         {"pending", {{"waits_for", "06.C9"}, {"refused_with", "solve.unsupported"}}}},
        "echo \"error: points= [solve.unsupported]\" >&2\nexit 1\n");
  const std::vector<std::string> pass{"a-pass"}, pending{"b-pending"}, both{"a-pass", "b-pending"}, none{"no-such"};
  CHECK(run(t.settings(), pass, {}, out, err) == exit_ok);
  CHECK(run(t.settings(), pending, {}, out, err) == exit_skipped);           // everything skipped: CTest's skip
  CHECK(run(t.settings(), both, {}, out, err) == exit_ok);
  CHECK(run(t.settings(), none, {}, out, err) == exit_harness);
  t.add("c-fail", {{"expect", {{"exit", 1}}}}, "exit 0\n");
  std::ostringstream all;
  CHECK(run(t.settings(), {}, {}, all, err) == exit_failed);
  CHECK_THAT(all.str(), ContainsSubstring("e2e c-fail: FAILED"));
  CHECK_THAT(all.str(), ContainsSubstring("3 cases, 1 passed, 1 failed, 1 skipped, 0 not run"));
  write(t.root / "cases" / "d-broken" / "case.json",
        R"({"case": "d-broken", "args": [], "expect": {"exit": 0}, "expcet": 1})");
  CHECK(run(t.settings(), {}, {}, out, err) == exit_harness);
  Settings missing = t.settings();
  missing.fusion = t.dir.path() / "no-fusion";
  CHECK(run(missing, pass, {}, out, err) == exit_harness);
  fs::remove_all(t.root / "cases" / "d-broken");
  remove_kept(all.str() + out.str());

  std::ostringstream listed;
  CHECK(list(t.settings(), {}, listed, err) == exit_ok);
  CHECK(listed.str() ==
        "case\tstate\ttags\na-pass\tno golden\t\nb-pending\tpending 06.C9\te2e,S8.21\nc-fail\tno golden\t\n");
}

TEST_CASE("fusion-test e2e: case.json is read field by field, and an unknown field is refused", "[unit][fusion-test]") {
  Toy t;
  const auto refused = [&](const std::string& text) {
    write(t.root / "cases" / "x" / "case.json", text);
    try {
      load_case(t.root / "cases" / "x");
    } catch (const fusion::core::Failure& f) {
      return std::string(f.what());
    }
    return std::string("accepted");
  };
  const std::string head = R"({"case": "x", "args": [], "expect": {"exit": 0}})";
  const auto with = [&](const std::string& more) { return head.substr(0, head.size() - 1) + ", " + more + "}"; };
  CHECK(refused(head) == "accepted");
  CHECK_THAT(refused(with(R"("expcet": {})")), ContainsSubstring("unknown field expcet"));
  CHECK_THAT(refused(R"({"case": "y", "args": [], "expect": {"exit": 0}})"), ContainsSubstring("its folder is x"));
  CHECK_THAT(refused(R"({"case": "x", "args": [], "expect": {"status": "ok"}})"),
             ContainsSubstring("expect has no exit"));
  CHECK_THAT(refused(R"({"case": "x", "args": [], "expect": {"exit": 0, "status": "done"}})"),
             ContainsSubstring("not ok, failed or stopped"));
  CHECK_THAT(refused(R"({"case": "x", "args": [], "expect": {"exit": 0, "code": 1}})"),
             ContainsSubstring("unknown field expect.code"));
  CHECK_THAT(refused(with(R"("compare": [{"glob": "a", "mode": "bytes"}])")), ContainsSubstring("compare mode bytes"));
  CHECK_THAT(refused(with(R"("compare": [{"glob": "a", "mode": "exact", "masked_columns": ["t"]}])")),
             ContainsSubstring("masked_columns goes with mode tsv_masked"));
  CHECK_THAT(refused(R"({"case": "x", "args": "-b case.batch", "expect": {"exit": 0}})"),
             ContainsSubstring("args must be an array"));
  CHECK_THAT(refused(with(R"("inputs": ["../toy"])")), ContainsSubstring("shared set"));
  CHECK_THAT(refused(with(R"("pending": {"waits_for": "01.C3"})")),
             ContainsSubstring("pending needs waits_for and refused_with"));
  CHECK_THAT(refused(with(R"("timeout_s": 0)")), ContainsSubstring("timeout_s must be a number above 0"));
  CHECK_THAT(refused("{\"case\": \"x\""), ContainsSubstring("case.json"));
  write(t.root / "cases" / "x" / "case.json", R"({
    "case": "x", "args": ["-b", "case.batch"],
    "expect": {"status": "failed", "exit": 1, "line": 14, "stderr": "no such file"},
    "tags": ["e2e", "S1.28"],
    "compare": [{"glob": "pathz3/*/search_log.tsv", "mode": "tsv_masked", "masked_columns": ["wall_s"]},
                {"glob": "pathz3/results.json", "mode": "results"}],
    "inputs": ["toy"], "env": {"FUSION_GNUPLOT": "<CASE>/fake-tools/gnuplot"}, "timeout_s": 2.5, "processes": 1,
    "actions": [{"on_event": "solve.check.started", "fusion": ["--stop", "<RUN>"]}],
    "pending": {"waits_for": "01.C3", "refused_with": "batch.unknown_command"}})");
  const Case full = load_case(t.root / "cases" / "x");
  CHECK(full.args == std::vector<std::string>{"-b", "case.batch"});
  CHECK(full.expect.status == "failed");
  CHECK(full.expect.exit == 1);
  CHECK(full.expect.line == 14);
  CHECK(full.expect.stderr_fragment == "no such file");
  CHECK(full.tags == std::vector<std::string>{"e2e", "S1.28"});
  REQUIRE(full.compare.size() == 2);
  CHECK(full.compare[0].mode == fusion::testkit::Compare::tsv_masked);
  CHECK(full.compare[0].masked_columns == std::vector<std::string>{"wall_s"});
  CHECK(full.compare[1].mode == fusion::testkit::Compare::results);
  CHECK(full.inputs == std::vector<std::string>{"toy"});
  CHECK(full.env == std::vector<std::pair<std::string, std::string>>{{"FUSION_GNUPLOT", "<CASE>/fake-tools/gnuplot"}});
  CHECK(full.timeout == std::chrono::milliseconds(2500));
  REQUIRE(full.actions.size() == 1);
  CHECK(full.actions[0].on_event == "solve.check.started");
  CHECK(full.actions[0].fusion == std::vector<std::string>{"--stop", "<RUN>"});
  REQUIRE(full.pending);
  CHECK(full.pending->waits_for == "01.C3");
  CHECK(full.pending->refused_with == "batch.unknown_command");
}

TEST_CASE("fusion-test e2e: names that differ only in case are found (12, Platforms)", "[unit][fusion-test]") {
  fusion::testkit::ScratchDir dir("case-clashes");
  write(dir.path() / "pathz3" / "In.dat", "1\n");
  write(dir.path() / "pathz3" / "in.dat", "2\n");
  write(dir.path() / "pathz3" / "out.dat", "3\n");
  if (read(dir.path() / "pathz3" / "In.dat") != "1\n")
    SKIP("this file system ignores case, so it cannot hold the pair");
  CHECK(case_clashes(dir.path()) == std::vector<std::string>{"pathz3/In.dat | pathz3/in.dat"});
  CHECK(case_clashes(dir.path() / "nothing").empty());
}

TEST_CASE("fusion-test e2e: the real fusion on the relabelled toy network passes against the golden it recorded",
          "[unit][fusion-test][e2e]") {
  // FuSION 1's toy network relabelled (no KEGG content), solved in both FSC cases, whose check events
  // interleave: the second run must equal the first after the kit's normaliser (C5.3, C5.4).
  Toy t;
  const fs::path toy = fusion::testkit::source_dir() / "reference" / "synthetic" / "toy-hsa11-wni-tsv-b10" / "inputs";
  const fs::path exe = fusion::core::path_from_utf8(FUSION_EXE);
  const Json rules = {{{"glob", "pathz3/results.json"}, {"mode", "results"}},
                      {{"glob", "pathz3/*/search_log.tsv"}, {"mode", "tsv_masked"},
                       {"masked_columns", {"wall_s", "cpu_s", "source", "model_n", "model_e"}}},
                      {{"glob", "fb_rch/observations_match.tsv"}, {"mode", "exact"}}};
  const std::string home = fusion::core::to_utf8(exe.parent_path().parent_path() / "share" / "fusion");
  const fs::path d = t.add("toy-real",   // the staged home, whatever FUSION_HOME the tests inherit
                           {{"expect", ok_expect}, {"compare", rules}, {"env", {{"FUSION_HOME", home}}}},
                           "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
                           "cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
                           "rgx file=inputs/with_node_included.xml tool_written=y\n"
                           "fb_rch source=hsa1 target=hsa22 bound=10\n"
                           "pathz3 fsc=hsa11 node_max=7 edge_max=7 timeout=150s incremental_timeout=30s\n");
  fs::create_directories(d / "inputs");
  for (const auto& e : fs::directory_iterator(toy)) fs::copy_file(e.path(), d / "inputs" / e.path().filename());
  Settings s = t.settings();
  s.fusion = exe;
  const CaseResult recorded = run_case(load_case(d), s, true);
  INFO(joined(recorded.notes));
  REQUIRE(recorded.outcome == Outcome::passed);
  CHECK(fs::exists(d / "expected" / "pathz3" / "results.json"));
  CHECK(fs::exists(d / "expected" / "pathz3" / "in" / "search_log.tsv"));
  CHECK_THAT(read(d / "expected" / "events.jsonl"), ContainsSubstring("\"seq\":\"<masked>\""));
  const CaseResult second = run_case(load_case(d), s, false);
  INFO(joined(second.notes));
  CHECK(second.outcome == Outcome::passed);
}
