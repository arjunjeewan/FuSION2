// Stop through the stop file, the grace period and the forced end (09.C7), and when run.json is
// written (E1).
#include "support.hpp"

namespace fusion::run::test {

TEST_CASE("run: a stop file gives stop.requested within 1 s; the step sees its token and its callback runs once (09.C7)",
          "[unit][run]") {
    TempDir tmp;
    Options options = options_in(tmp.path());
    options.timing.stop_poll = 500ms;            // section 09's own interval
    Run run = Run::start(options);
    Run::Step s = run.begin(step("pathz3", "pathz3", 11));
    std::atomic<int> interrupts{0};
    const core::StopCallback interrupt(s.context().stop, [&] { ++interrupts; });   // the solve's z3 interrupt
    // As section 10 writes it: under a temporary name, then renamed. Only the file's existence matters.
    write_file(run.dir() / "stop.note", "asked by the frontend\n");
    fs::rename(run.dir() / "stop.note", run.dir() / "stop");
    const auto asked = std::chrono::steady_clock::now();
    REQUIRE(wait_for([&] { return s.context().stop.stop_requested(); }));
    CHECK(std::chrono::steady_clock::now() - asked < 1000ms);
    CHECK(interrupts == 1);
    CHECK_THROWS_AS(s.context().stop.throw_if_stopped(), core::Stopped);
    CHECK(run.stop().stop_requested());
    run.end(s, StepStatus::stopped);
    run.skip(step("plot_pareto", "plot_pareto", 12), "stopped at line 11");   // the interpreter skips the rest
    CHECK(run.finish(RunStatus::stopped) == 3);
    CHECK(interrupts == 1);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["status"] == "stopped");
    CHECK(manifest["exit"] == 3);
    CHECK(manifest["stop"]["source"] == "file");
    CHECK(manifest["stop"]["note"] == "asked by the frontend");
    CHECK(manifest["stop"]["forced"] == false);
    CHECK(manifest["steps"][0]["status"] == "stopped");
    CHECK(manifest["steps"][1]["status"] == "skipped");
    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    REQUIRE(of_type(events, "stop.requested").size() == 1);
    CHECK(of_type(events, "stop.requested")[0]["source"] == "file");
    CHECK(of_type(events, "step.skipped")[0]["because"] == "stopped at line 11");
    CHECK(events.back()["type"] == "run.finished");
    CHECK(events.back()["status"] == "stopped");
    CHECK(events.back()["exit"] == 3);
    CHECK(events.back()["forced"] == false);
}

TEST_CASE("run: a step that ignores its token is ended after the grace period, with forced: true and one hook call "
          "(09.C7)",
          "[unit][run]") {
    TempDir tmp;
    Options options = options_in(tmp.path());
    options.timing.grace = 200ms;                // 30 s in a real run
    std::atomic<int> calls{0};
    std::atomic<int> status{-1};
    options.on_grace_expired = [&](int code) {   // records the call instead of ending the process
        status = code;
        ++calls;
    };
    Run run = Run::start(options);
    Run::Step s = run.begin(step("pathz3", "pathz3", 11));
    run.copy_inputs(std::vector<StepSpec>{step("pathz3", "pathz3", 11), step("pex", "pex", 12)});
    write_file(run.dir() / "stop", "");
    REQUIRE(wait_for([&] { return calls.load() == 1; }));
    CHECK(status == 3);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["status"] == "stopped");
    CHECK(manifest["exit"] == 3);
    CHECK(manifest["stop"]["forced"] == true);
    CHECK(manifest["steps"][0]["status"] == "stopped");   // the step still running takes the run's status
    CHECK(manifest["steps"][1]["status"] == "skipped");
    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    CHECK(events.back()["type"] == "run.finished");
    CHECK(events.back()["forced"] == true);
    REQUIRE_FALSE(of_type(events, "warning").empty());
    CHECK(of_type(events, "warning").back()["code"] == "stop.forced");

    // The main thread comes back late: nothing is written twice, and run.finished stays the last line.
    run.end(s, StepStatus::stopped);
    CHECK(run.finish(RunStatus::stopped) == 3);
    CHECK(calls == 1);
    CHECK(read_lines(run.dir() / "events.jsonl").size() == events.size());
    CHECK(read_json(run.dir() / "run.json") == manifest);
}

TEST_CASE("run: run.json is written at most once a second between steps, and the last change within a second (E1)",
          "[unit][run]") {
    TempDir tmp;
    Options options = options_in(tmp.path());
    options.timing.manifest_every = 1000ms;
    options.timing.stop_poll = 500ms;
    Run run = Run::start(options);
    REQUIRE(read_json(run.dir() / "run.json")["steps"].empty());   // the first version, at the start
    Run::Step s = run.begin(step("cudf", "cudf", 3));
    const auto changed = std::chrono::steady_clock::now();
    CHECK(read_json(run.dir() / "run.json")["steps"].empty());     // within the second: not yet
    REQUIRE(wait_for([&] { return read_json(run.dir() / "run.json")["steps"].size() == 1; }, 2000ms));
    CHECK(std::chrono::steady_clock::now() - changed < 1200ms);
    CHECK(read_json(run.dir() / "run.json")["steps"][0]["status"] == "running");
    run.end(s, StepStatus::ok);
    run.finish(RunStatus::ok);                   // the end is written at once
    CHECK(read_json(run.dir() / "run.json")["steps"][0]["status"] == "ok");
}

}  // namespace fusion::run::test
