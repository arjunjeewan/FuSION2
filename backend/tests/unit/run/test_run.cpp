// A run from start to finish: run.json (E1), events.jsonl (09.C10), the run log (09.C11), one
// directory per step (09.C2), saved outputs (09.C4), journals (09.C6), statuses (1.31, 09.C14) and
// settings with their sources (E1).
#include <array>
#include <set>

#include "support.hpp"

namespace fusion::run::test {

namespace {

const core::Json* setting_in(const core::Json& list, std::string_view key) {
    for (const core::Json& s : list)
        if (s["key"] == key) return &s;
    return nullptr;
}

core::Setting setting(const core::SettingSpec& spec, std::optional<core::Value> value, core::Source source) {
    core::Setting s;
    s.spec = &spec;
    s.value = std::move(value);
    s.source = source;
    return s;
}

bool no_temp_files(const fs::path& dir) {
    for (const auto& entry : fs::recursive_directory_iterator(dir))
        if (entry.path().extension() == ".tmp") return false;
    return true;
}

}  // namespace

TEST_CASE("run: a run claims its directory and records itself in run.json, events.jsonl, its log and run.lock "
          "(09.C1, 09.C2, E1)",
          "[unit][run]") {
    TempDir tmp;
    auto console = std::make_shared<Capture>();
    Options options = options_in(tmp.path(), "EGFR→CXCL8 / b10");
    options.sinks.push_back(console);
    options.command_line = {"fusion", "-b", "egfr.batch", "--tag", "session=s-01hzx3"};
    options.labels = {{"session", "s-01hzx3"}};
    options.variables = {{"$FSC", "hsa3725"}, {"$LFC", "/data/egf_hela_t120_lfc.tsv"}};
    Run run = Run::start(options);
    const fs::path dir = run.dir();
    const std::string name = dir.filename().string();
    CHECK(dir.parent_path() == tmp.path());
    CHECK(std::regex_match(name, std::regex(R"(EGFR-CXCL8-b10_\d{8}-\d{6})")));
    for (const std::string& file : std::vector<std::string>{"run.json", "events.jsonl", "run.lock", name + ".log"})
        CHECK(fs::is_regular_file(dir / file));
    CHECK_FALSE(fs::exists(dir / "inputs"));     // made only when something is copied
    CHECK_FALSE(fs::exists(dir / "debug"));

    core::Json manifest = read_json(dir / "run.json");
    check_manifest(manifest);
    CHECK(manifest["run"] == run.id());
    CHECK(manifest["name"] == name);
    CHECK(manifest["status"] == "running");
    CHECK(manifest["exit"].is_null());
    CHECK(manifest["pid"] == ::getpid());
    CHECK(manifest["labels"]["session"] == "s-01hzx3");
    CHECK(manifest["build_id"] == std::string(core::build_id()));
    CHECK(manifest["command_line"] == core::Json(options.command_line));
    CHECK(manifest["variables"][0]["name"] == "$FSC");
    CHECK(manifest["variables"][1]["value"] == "/data/egf_hela_t120_lfc.tsv");
    REQUIRE(setting_in(manifest["run_settings"], "run.log"));
    CHECK((*setting_in(manifest["run_settings"], "run.log"))["value"] == name + ".log");   // computed default
    CHECK((*setting_in(manifest["run_settings"], "run.log"))["source"] == "default");
    CHECK((*setting_in(manifest["run_settings"], "run.debug"))["value"] == false);
    CHECK((*setting_in(manifest["run_settings"], "run.cache"))["value"] == true);

    std::vector<core::Json> events = read_lines(dir / "events.jsonl");
    check_events(events);
    REQUIRE(events.size() == 1);
    CHECK(events[0]["type"] == "run.started");
    CHECK(events[0]["run"] == run.id());
    CHECK(events[0]["name"] == name);
    CHECK(events[0]["dir"] == core::to_utf8(dir));
    CHECK(events[0]["pid"] == ::getpid());
    CHECK(events[0]["fusion"] == std::string(core::build_id()));
    CHECK(events[0]["parent"].is_null());
    CHECK_FALSE(events[0].contains("step"));     // a run-level event has no step scope

    CHECK(console->has(core::Level::info, "", "run " + run.id()));
    CHECK(console->has(core::Level::info, "", "Run directory: " + core::to_utf8(dir)));
    std::vector<std::string> lines = log_lines(dir / (name + ".log"));
    REQUIRE(lines.size() >= 2);
    CHECK(std::regex_match(lines[0], std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z info  FuSION \S+ )"
                                                R"(\(build \S+\), run r-\d{8}T\d{6}Z-[0-9a-f]{8})")));

    CHECK(run.finish(RunStatus::ok) == 0);
    manifest = read_json(dir / "run.json");
    check_manifest(manifest);
    CHECK(manifest["status"] == "ok");
    CHECK(manifest["exit"] == 0);
    CHECK(is_timestamp(manifest["finished"]));
    CHECK(manifest["resources"]["wall_s"].get<double>() >= 0.0);
    CHECK(manifest["resources"]["peak_rss_bytes"].get<std::uint64_t>() > 0);
    events = read_lines(dir / "events.jsonl");
    check_events(events);
    CHECK(events.back()["type"] == "run.finished");
    CHECK(events.back()["status"] == "ok");
    CHECK(events.back()["exit"] == 0);
    CHECK(events.back()["forced"] == false);
    lines = log_lines(dir / (name + ".log"));
    CHECK(lines.back().find("info  Finished: ok, exit 0, ") != std::string::npos);
    CHECK(lines.back().find(" bytes") != std::string::npos);
}

TEST_CASE("run: the run log holds every console line, exec output passed through the log, and debug records only "
          "under the debug option (09.C11, S1.25)",
          "[unit][run]") {
    for (const bool debug : {false, true}) {
        INFO("debug: " << debug);
        TempDir tmp;
        auto console = std::make_shared<Capture>();
        Options options = options_in(tmp.path());
        options.debug = debug;
        options.log_name = "egfr.log";           // S1.25's per-run name, set before the first step
        options.sinks.push_back(console);
        Run run = Run::start(options);
        CHECK(run.log().debug_on() == debug);
        run.log().info("hello from the exec line's shell");   // section 01 passes exec output through the log
        run.log().debug("fb_rch: forward set 354 nodes");
        run.log().warning("values.repeated_id", "egfr.batch:3: cudf: hsa1 appears twice");
        run.finish(RunStatus::ok);
        const std::string name = run.dir().filename().string();
        CHECK_FALSE(fs::exists(run.dir() / (name + ".log")));
        const std::string text = read_file(run.dir() / "egfr.log");
        CHECK(text.find("Z info  hello from the exec line's shell\n") != std::string::npos);
        CHECK(text.find("Z warning egfr.batch:3: cudf: hsa1 appears twice [values.repeated_id]\n") != std::string::npos);
        CHECK((text.find("Z debug fb_rch: forward set 354 nodes\n") != std::string::npos) == debug);
        CHECK(console->has(core::Level::warning, "values.repeated_id", "hsa1 appears twice"));
        const core::Json manifest = read_json(run.dir() / "run.json");
        check_manifest(manifest);
        CHECK((*setting_in(manifest["run_settings"], "run.log"))["value"] == "egfr.log");
        CHECK((*setting_in(manifest["run_settings"], "run.debug"))["value"] == debug);
        const auto events = read_lines(run.dir() / "events.jsonl");
        check_events(events);
        REQUIRE(of_type(events, "warning").size() == 1);   // every warning line is also an event
        CHECK(of_type(events, "warning")[0]["code"] == "values.repeated_id");
        CHECK(of_type(events, "warning")[0]["message"] == "egfr.batch:3: cudf: hsa1 appears twice");
    }
}

TEST_CASE("run: each step that saves something gets one directory, named by its id; steps with nothing to save get "
          "none (09.C2, 09.C4)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    const fs::path dir = run.dir();
    const std::vector<StepSpec> plan{step("start", "start", 1), step("cudf", "cudf", 3), step("fb_rch", "fb_rch", 5),
                                     step("fb_rch-2", "fb_rch", 6), step("exec", "exec", 7)};
    run.copy_inputs(plan);
    core::Json manifest = read_json(dir / "run.json");
    REQUIRE(manifest["steps"].size() == 5);      // every planned step is listed from the start
    for (const core::Json& s : manifest["steps"]) CHECK(s["status"] == "pending");

    const auto save = [&](const StepSpec& spec, std::vector<std::pair<std::string, std::string>> files) {
        Run::Step s = run.begin(spec);
        CHECK(s.dir() == dir / spec.id);
        CHECK(s.context().files.step_dir() == dir / spec.id);
        CHECK(s.context().step == spec.id);
        for (const auto& [name, text] : files) {
            core::OutputFile out = s.context().files.create(name);
            out.out() << text;
            out.commit();
        }
        run.end(s, StepStatus::ok);
    };
    save(plan[0], {});
    save(plan[1], {{"up_reg", "hsa1\nhsa2\n"}, {"down_reg", ""}});
    save(plan[2], {{"corridor.json", R"({"first":true})"}});
    save(plan[3], {{"corridor.json", R"({"second":true})"}});
    save(plan[4], {});
    CHECK(run.finish(RunStatus::ok) == 0);

    CHECK_FALSE(fs::exists(dir / "start"));
    CHECK_FALSE(fs::exists(dir / "exec"));
    CHECK(read_file(dir / "cudf/up_reg") == "hsa1\nhsa2\n");
    CHECK(read_file(dir / "cudf/down_reg").empty());
    CHECK(read_file(dir / "fb_rch/corridor.json") == R"({"first":true})");
    CHECK(read_file(dir / "fb_rch-2/corridor.json") == R"({"second":true})");
    CHECK(no_temp_files(dir));

    manifest = read_json(dir / "run.json");
    check_manifest(manifest);
    for (const core::Json& s : manifest["steps"]) {
        CHECK(s["status"] == "ok");
        CHECK(is_timestamp(s["started"]));
        CHECK(s["wall_s"].is_number());
    }
    const core::Json& cudf = manifest["steps"][1];
    REQUIRE(cudf["outputs"].size() == 2);
    CHECK(cudf["outputs"][0]["path"] == "cudf/up_reg");
    CHECK(cudf["outputs"][0]["size"] == 10);
    CHECK(cudf["outputs"][0]["sha256"] == core::Sha256::of("hsa1\nhsa2\n").hex());
    CHECK(cudf["outputs"][1]["path"] == "cudf/down_reg");
    CHECK(cudf["outputs"][1]["sha256"] == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(manifest["steps"][0]["outputs"].empty());

    const auto events = read_lines(dir / "events.jsonl");
    check_events(events);
    const auto finished = of_type(events, "step.finished");
    REQUIRE(finished.size() == 5);
    CHECK(finished[1]["step"] == "cudf");
    CHECK(finished[1]["line"] == 3);
    CHECK(finished[1]["point"] == "");
    CHECK(finished[1]["outputs"] == core::Json::array({"cudf/up_reg", "cudf/down_reg"}));
    CHECK(of_type(events, "step.started")[3]["step"] == "fb_rch-2");
    CHECK(of_type(events, "step.started")[3]["cmd"] == "fb_rch");
}

TEST_CASE("run: an explicit absolute output is written in the step directory and copied to its path, both in run.json "
          "(2B.3)",
          "[unit][run]") {
    TempDir tmp;
    TempDir elsewhere;
    Run run = Run::start(options_in(tmp.path()));
    Run::Step s = run.begin(step("cudf", "cudf", 3));
    core::OutputFile up = s.context().files.create("up.txt");
    up.also_at(elsewhere / "up.txt");            // a FuSION 1 batch reads it there through exec
    up.out() << "hsa1\n";
    up.commit();
    core::OutputFile down = s.context().files.create("down.txt");
    down.out() << "hsa2\n";
    down.commit();
    run.end(s, StepStatus::ok);
    run.finish(RunStatus::ok);
    CHECK(read_file(run.dir() / "cudf/up.txt") == "hsa1\n");
    CHECK(read_file(elsewhere / "up.txt") == "hsa1\n");
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    const core::Json& outputs = manifest["steps"][0]["outputs"];
    REQUIRE(outputs.size() == 2);
    CHECK(outputs[0]["path"] == "cudf/up.txt");
    CHECK(outputs[0]["also"] == core::to_utf8(elsewhere / "up.txt"));
    CHECK_FALSE(outputs[1].contains("also"));
}

TEST_CASE("run: a committed output is never overwritten, names stay inside their step, and an uncommitted one leaves "
          "nothing (09's rules, 1.28)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    Run::Step s = run.begin(step("pathz3", "pathz3", 11));
    core::Files& files = s.context().files;
    {
        core::OutputFile out = files.create("in/case.json");   // modules name files below their step (06)
        out.out() << "{}";
        out.commit();
    }
    CHECK(fs::is_regular_file(run.dir() / "pathz3/in/case.json"));
    CHECK(failure_code([&] { files.create("in/case.json"); }) == "output.clash");
    CHECK(failure_code([&] { files.create("../escape.txt"); }) == "internal");
    CHECK(failure_code([&] { files.create("/tmp/absolute.txt"); }) == "internal");
    CHECK(failure_code([&] { files.create(""); }) == "internal");
    {
        core::OutputFile out = files.create("results.json");
        out.out() << R"({"half":)";              // the step fails before committing
    }
    CHECK_FALSE(fs::exists(run.dir() / "pathz3/results.json"));
    CHECK_FALSE(fs::exists(run.dir() / "pathz3/results.json.tmp"));

    // A write of run.json interrupted between its temp file and the rename leaves the previous one whole.
    const std::string before = read_file(run.dir() / "run.json");
    {
        core::OutputFile torn(run.dir() / "run.json");
        torn.out() << R"({"torn":)";
    }
    CHECK(read_file(run.dir() / "run.json") == before);
    CHECK(core::Json::accept(before));
    CHECK_FALSE(fs::exists(run.dir() / "run.json.tmp"));
    write_file(run.dir() / "run.json.tmp", R"({"torn":)");   // as a kill mid-write leaves it
    run.end(s, StepStatus::ok);
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["steps"][0]["status"] == "ok");
    CHECK_FALSE(fs::exists(run.dir() / "run.json.tmp"));
    run.finish(RunStatus::ok);
}

TEST_CASE("run: a journal record is on disk when append returns; a journal cut mid-line reads back every whole record "
          "(09.C6)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    Run::Step s = run.begin(step("pathz3", "pathz3", 11));
    core::Journal& journal = s.context().files.journal("in/journal.jsonl");
    CHECK(&s.context().files.journal("in/journal.jsonl") == &journal);   // one journal per name
    const fs::path file = run.dir() / "pathz3/in/journal.jsonl";
    journal.append({{"header", 1}, {"case", "in"}, {"seed", 42}});
    CHECK(read_lines(file).size() == 1);         // durable before the next check starts
    journal.append({{"check", 1}, {"n", 0}, {"e", 4}, {"outcome", "unsat"}});
    CHECK(read_lines(file).size() == 2);
    journal.append({{"check", 2}, {"n", 1}, {"e", 3}, {"outcome", "sat"}});
    CHECK(read_lines(file).size() == 3);
    run.end(s, StepStatus::ok);
    CHECK(failure_code([&] { journal.append({{"late", true}}); }) == "internal");
    run.finish(RunStatus::ok);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    REQUIRE(manifest["steps"][0]["outputs"].size() == 1);   // recorded once its step ended
    CHECK(manifest["steps"][0]["outputs"][0]["path"] == "pathz3/in/journal.jsonl");
    CHECK(manifest["steps"][0]["outputs"][0]["sha256"] == core::Sha256::of_file(file).hex());

    const std::string text = read_file(file);
    write_file(tmp / "cut.jsonl", text.substr(0, text.size() - 10));   // a kill mid-record
    const std::vector<core::Json> records = read_journal(tmp / "cut.jsonl");
    REQUIRE(records.size() == 2);
    CHECK(records[0]["header"] == 1);
    CHECK(records[1]["check"] == 1);
    CHECK(read_journal(file).size() == 3);
    write_file(tmp / "odd.jsonl", "{\"a\":1}\r\n{broken\n{\"b\":2}\n{\"c\":");
    const std::vector<core::Json> odd = read_journal(tmp / "odd.jsonl");
    REQUIRE(odd.size() == 2);
    CHECK(odd[0]["a"] == 1);
    CHECK(odd[1]["b"] == 2);
    CHECK(failure_code([&] { read_journal(tmp / "none.jsonl"); }) == "input.unreadable");
}

TEST_CASE("run: a failure: one stderr line with the batch line, an error event in the step's scope, run.json's error, "
          "exit 1 (S1.28, 09.C14)",
          "[unit][run]") {
    TempDir tmp;
    auto console = std::make_shared<Capture>();
    Options options = options_in(tmp.path());
    options.sinks.push_back(console);
    options.batch = tmp / "egfr.batch";
    write_file(options.batch, "mff list=/home/u/kegg/list.txt\nfb_rch source=hsa1956 target=hsa3576\n");
    Run run = Run::start(options);
    const std::vector<StepSpec> plan{step("mff", "mff", 9), step("fb_rch", "fb_rch", 10)};
    run.copy_inputs(plan);
    Run::Step s = run.begin(plan[0]);
    core::OutputFile partial = s.context().files.create("merged_graph.xml");
    partial.out() << "<half";
    run.fail(s, core::Failure("input.unreadable", "cannot read /home/u/kegg/hsa04010.xml: No such file or directory",
                              core::Where{"/home/u/kegg/hsa04010.xml", 0, "list"}));
    run.fail(s, core::Failure("internal", "a second report of the same step is ignored"));
    CHECK(console->has(core::Level::error, "input.unreadable",
                       "egfr.batch:9: mff: cannot read /home/u/kegg/hsa04010.xml: No such file or directory"));
    CHECK(run.finish(RunStatus::failed) == 1);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["status"] == "failed");
    CHECK(manifest["exit"] == 1);
    CHECK(manifest["error"]["code"] == "input.unreadable");
    CHECK(manifest["error"]["step"] == "mff");
    CHECK(manifest["error"]["line"] == 9);
    CHECK(manifest["error"]["file"] == "/home/u/kegg/hsa04010.xml");
    CHECK(manifest["error"]["setting"] == "list");
    CHECK(manifest["steps"][0]["status"] == "failed");
    CHECK(manifest["steps"][0]["error"]["code"] == "input.unreadable");
    CHECK(manifest["steps"][1]["status"] == "skipped");   // the batch stops at its first failure
    CHECK(manifest["batch"]["copy"] == "inputs/egfr.batch");

    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    REQUIRE(of_type(events, "error").size() == 1);
    const core::Json error = of_type(events, "error")[0];
    CHECK(error["code"] == "input.unreadable");
    CHECK(error["step"] == "mff");
    CHECK(error["line"] == 9);
    const auto finished = of_type(events, "step.finished");
    REQUIRE(finished.size() == 1);
    CHECK(finished[0]["status"] == "failed");
    CHECK(finished[0]["error"]["code"] == "input.unreadable");
    CHECK(of_type(events, "step.skipped")[0]["step"] == "fb_rch");
    CHECK(events.back()["type"] == "run.finished");
    CHECK(events.back()["exit"] == 1);
    const std::string log = read_file(run.dir() / (run.dir().filename().string() + ".log"));
    CHECK(log.find(" error egfr.batch:9: mff: cannot read /home/u/kegg/hsa04010.xml: No such file or directory "
                   "[input.unreadable]\n") != std::string::npos);
}

TEST_CASE("run: a reported result: the step is reported, every later line skipped, and the run ends ok with exit 0 "
          "(S4.18, 01.C15)",
          "[unit][run]") {
    TempDir tmp;
    auto console = std::make_shared<Capture>();
    Options options = options_in(tmp.path());
    options.sinks.push_back(console);
    options.batch = tmp / "egfr.batch";
    write_file(options.batch, "fb_rch\npathz3\nexec echo\n");
    Run run = Run::start(options);
    Run::Step s = run.begin(step("fb_rch", "fb_rch", 191));
    run.end(s, StepStatus::ok,
            core::Report{"corridor.empty", "no path from EGFR (hsa1956) to CXCL8 (hsa3576) within bound 3"});
    run.skip(step("pathz3", "pathz3", 192), "corridor.empty at line 191");
    run.skip(step("exec", "exec", 193), "corridor.empty at line 191");
    CHECK(run.finish(RunStatus::ok) == 0);
    CHECK(console->has(core::Level::info, "", "egfr.batch:191: result: no path from EGFR (hsa1956)"));

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["status"] == "ok");
    CHECK(manifest["steps"][0]["status"] == "reported");
    CHECK(manifest["steps"][0]["report"]["code"] == "corridor.empty");
    REQUIRE(manifest["reports"].size() == 1);
    CHECK(manifest["reports"][0]["code"] == "corridor.empty");
    CHECK(manifest["reports"][0]["step"] == "fb_rch");
    CHECK(manifest["reports"][0]["line"] == 191);
    CHECK(manifest["steps"][1]["status"] == "skipped");
    CHECK(manifest["steps"][2]["because"] == "corridor.empty at line 191");
    CHECK_FALSE(fs::exists(run.dir() / "pathz3"));

    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    const auto finished = of_type(events, "step.finished");
    REQUIRE(finished.size() == 1);
    CHECK(finished[0]["status"] == "reported");
    CHECK(finished[0]["report"]["code"] == "corridor.empty");
    const auto skipped = of_type(events, "step.skipped");
    REQUIRE(skipped.size() == 2);
    CHECK(skipped[1]["step"] == "exec");
    CHECK(skipped[1]["line"] == 193);
}

TEST_CASE("run: every setting is recorded with the source that applied, and two lines of one command keep their own "
          "values (E1, precedence)",
          "[unit][run]") {
    static constexpr std::array<core::SettingSpec, 4> corridor{{
        {.name = "bound", .module = "corridor", .type = core::Type::integer, .need = core::Need::defaulted,
         .default_text = "10"},
        {.name = "edges_to_target", .module = "corridor", .type = core::Type::choice, .need = core::Need::defaulted,
         .default_text = "any"},
        {.name = "exclude", .module = "corridor", .type = core::Type::text, .need = core::Need::optional},
        {.name = "skip_subtypes", .module = "corridor", .type = core::Type::text, .need = core::Need::defaulted},
    }};
    const core::Settings first({setting(corridor[0], std::int64_t{3}, core::Source::named),
                                setting(corridor[1], std::string("activation"), core::Source::run_line),
                                setting(corridor[2], std::nullopt, core::Source::default_value),
                                setting(corridor[3], std::string("binding/association"), core::Source::set_option)});
    core::Settings second({setting(corridor[0], std::int64_t{8}, core::Source::positional),
                           setting(corridor[1], std::string("any"), core::Source::default_value),
                           setting(corridor[2], std::nullopt, core::Source::default_value),
                           setting(corridor[3], std::string(), core::Source::default_value)});
    StepSpec a = step("fb_rch", "fb_rch", 10);
    a.settings = &first;
    StepSpec b = step("fb_rch-2", "fb_rch", 11);
    b.settings = &second;

    TempDir tmp;
    Options options = options_in(tmp.path());
    const auto run_spec = [](std::string_view name) {
        for (const core::SettingSpec& spec : settings())
            if (spec.name == name) return &spec;
        return static_cast<const core::SettingSpec*>(nullptr);
    };
    options.settings = {setting(*run_spec("cache"), false, core::Source::run_line),
                        setting(*run_spec("debug"), false, core::Source::default_value),
                        setting(*run_spec("log"), std::nullopt, core::Source::default_value)};
    Run run = Run::start(options);
    {
        Run::Step s = run.begin(a);
        run.end(s, StepStatus::ok);
    }
    {
        Run::Step s = run.begin(b);
        second.set_effective("bound", std::int64_t{3}, "3 changed genes lie in the corridor (S4.16)");
        run.end(s, StepStatus::ok);
    }
    run.finish(RunStatus::ok);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    const core::Json& one = manifest["steps"][0]["settings"];
    CHECK((*setting_in(one, "corridor.bound"))["value"] == 3);
    CHECK((*setting_in(one, "corridor.bound"))["source"] == "named");
    CHECK((*setting_in(one, "corridor.edges_to_target"))["source"] == "run_line");
    CHECK((*setting_in(one, "corridor.skip_subtypes"))["source"] == "set");
    CHECK((*setting_in(one, "corridor.exclude"))["value"].is_null());   // left out: its feature is off (0021)
    CHECK((*setting_in(one, "corridor.exclude"))["source"] == "default");
    const core::Json& two = manifest["steps"][1]["settings"];
    CHECK((*setting_in(two, "corridor.bound"))["value"] == 8);
    CHECK((*setting_in(two, "corridor.bound"))["source"] == "positional");
    CHECK((*setting_in(two, "corridor.bound"))["effective"] == 3);
    CHECK((*setting_in(two, "corridor.bound"))["reason"] == "3 changed genes lie in the corridor (S4.16)");
    CHECK((*setting_in(manifest["run_settings"], "run.cache"))["value"] == false);
    CHECK((*setting_in(manifest["run_settings"], "run.cache"))["source"] == "run_line");
    CHECK((*setting_in(manifest["run_settings"], "run.log"))["value"] == manifest["name"].get<std::string>() + ".log");
}

TEST_CASE("run: events: seq rises by one across threads, and every event inside a step carries step, line and point "
          "(1.3, 09.C10)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    StepSpec spec = step("pathz3", "pathz3", 11);
    spec.point = "fdr0.25";
    Run::Step s = run.begin(spec);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&, t] {
            for (int i = 0; i < 50; ++i)
                s.context().events.emit("solve.check.started", {{"case", "in"}, {"check", t * 50 + i}, {"n", 0}});
        });
    for (std::thread& thread : threads) thread.join();
    run.events().emit("sweep.note", {{"note", "inside the step"}});
    run.log().warning("solve.edge_clamped", "pathz3: the edge bound is clamped to 7");
    run.end(s, StepStatus::ok);
    run.events().emit("results.page", {{"page", "index.html"}});
    run.finish(RunStatus::ok);

    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);                        // seq 1, 2, 3 ... in file order
    const auto checks = of_type(events, "solve.check.started");
    CHECK(checks.size() == 400);
    std::set<int> seen;
    for (const core::Json& e : checks) {
        CHECK(e["step"] == "pathz3");
        CHECK(e["line"] == 11);
        CHECK(e["point"] == "fdr0.25");
        CHECK(e["case"] == "in");
        seen.insert(e["check"].get<int>());
    }
    CHECK(seen.size() == 400);
    CHECK(of_type(events, "sweep.note")[0]["step"] == "pathz3");
    CHECK(of_type(events, "warning")[0]["point"] == "fdr0.25");
    CHECK_FALSE(of_type(events, "results.page")[0].contains("step"));
}

TEST_CASE("run: step.progress is coalesced to at most one a second per step (09.C10)", "[unit][run]") {
    TempDir tmp;
    Options options = options_in(tmp.path());
    options.timing.progress_every = 10000ms;
    Run run = Run::start(options);
    {
        Run::Step s = run.begin(step("gsea", "gsea", 8));
        for (std::uint64_t i = 0; i < 1000; ++i) s.context().events.progress(i, 1000, "permutations");
        run.events().progress(999, 1000, "permutations");   // the run's events reach the step's coalescer
        run.end(s, StepStatus::ok);
    }
    {
        Run::Step s = run.begin(step("gsea-2", "gsea", 9));
        s.context().events.progress(1, 10, "permutations");
        run.end(s, StepStatus::ok);
    }
    run.finish(RunStatus::ok);
    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    const auto progress = of_type(events, "step.progress");
    REQUIRE(progress.size() == 2);
    CHECK(progress[0]["step"] == "gsea");
    CHECK(progress[0]["done"] == 0);
    CHECK(progress[0]["total"] == 1000);
    CHECK(progress[0]["unit"] == "permutations");
    CHECK(progress[1]["step"] == "gsea-2");
}

TEST_CASE("run: the run page and bindings.tsv are written once, at the top level, at the run's end (09.C2, 1.4)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    {
        core::OutputFile page = run.create_run_file("index.html");
        page.out() << "<html></html>\n";
        page.commit();
        core::OutputFile bindings = run.create_run_file("bindings.tsv");
        bindings.out() << "role\tgiven\tstatus\n";
        bindings.commit();
    }
    CHECK(failure_code([&] { run.create_run_file("index.html"); }) == "output.clash");
    CHECK(failure_code([&] { run.create_run_file("run.json"); }) == "internal");
    CHECK(failure_code([&] { run.create_run_file("pages/index.html"); }) == "internal");
    CHECK(run.finish(RunStatus::failed) == 1);   // written whatever the run's status
    CHECK(failure_code([&] { run.create_run_file("late.html"); }) == "internal");
    CHECK(read_file(run.dir() / "index.html") == "<html></html>\n");
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    REQUIRE(manifest["outputs"].size() == 2);
    CHECK(manifest["outputs"][0]["path"] == "index.html");
    CHECK(manifest["outputs"][1]["path"] == "bindings.tsv");
    CHECK(manifest["outputs"][1]["sha256"] == core::Sha256::of("role\tgiven\tstatus\n").hex());
}

TEST_CASE("run: debug outputs exist only under the debug option, in debug/<step>/ (S1.29, S4.19, S5.23)", "[unit][run]") {
    for (const bool debug : {false, true}) {
        INFO("debug: " << debug);
        TempDir tmp;
        Options options = options_in(tmp.path());
        options.debug = debug;
        Run run = Run::start(options);
        Run::Step s = run.begin(step("fb_rch", "fb_rch", 5));
        CHECK(s.context().log.debug_on() == debug);
        if (!debug) {
            CHECK(failure_code([&] { s.context().files.create_debug("forward.tsv"); }) == "run.debug_off");
        } else {
            core::OutputFile trace = s.context().files.create_debug("forward.tsv");
            trace.out() << "node\tqueued\n";
            trace.commit();
            core::OutputFile dump = s.context().files.create_debug("in/dumps/n0_e4.smt2");
            dump.commit();
        }
        run.end(s, StepStatus::ok);
        run.finish(RunStatus::ok);
        CHECK(fs::exists(run.dir() / "debug") == debug);
        CHECK_FALSE(fs::exists(run.dir() / "fb_rch"));
        if (debug) {
            CHECK(read_file(run.dir() / "debug/fb_rch/forward.tsv") == "node\tqueued\n");
            CHECK(fs::exists(run.dir() / "debug/fb_rch/in/dumps/n0_e4.smt2"));
            const core::Json manifest = read_json(run.dir() / "run.json");
            CHECK(manifest["steps"][0]["outputs"][0]["path"] == "debug/fb_rch/forward.tsv");
        }
    }
}

TEST_CASE("run: two runs at once in one launch directory do not interfere (S1.26, S8.28)", "[unit][run]") {
    TempDir tmp;
    std::array<fs::path, 2> dirs;
    const auto body = [&](int which) {
        Run run = Run::start(options_in(tmp.path(), "together"));
        dirs[static_cast<std::size_t>(which)] = run.dir();
        for (int k = 0; k < 3; ++k) {
            Run::Step s = run.begin(step(k == 0 ? std::string("cudf") : std::format("cudf-{}", k + 1), "cudf",
                                         static_cast<std::uint32_t>(3 + k)));
            core::OutputFile up = s.context().files.create("up_reg");
            up.out() << std::format("run {} step {}\n", which, k);
            up.commit();
            run.end(s, StepStatus::ok);
        }
        run.finish(RunStatus::ok);
    };
    std::thread first(body, 0);
    std::thread second(body, 1);
    first.join();
    second.join();
    CHECK(dirs[0] != dirs[1]);
    std::set<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(tmp.path())) entries.insert(entry.path());
    CHECK(entries == std::set<fs::path>{dirs[0], dirs[1]});   // nothing else lands beside them
    for (int which = 0; which < 2; ++which) {
        const fs::path& dir = dirs[static_cast<std::size_t>(which)];
        CHECK(read_file(dir / "cudf/up_reg") == std::format("run {} step 0\n", which));
        CHECK(read_file(dir / "cudf-3/up_reg") == std::format("run {} step 2\n", which));
        const core::Json manifest = read_json(dir / "run.json");
        check_manifest(manifest);
        CHECK(manifest["steps"].size() == 3);
        check_events(read_lines(dir / "events.jsonl"));
    }
}

TEST_CASE("run: section 11's blocks, a step's details and an output's kind go in their places in run.json (E1, 2B.7, "
          "09.C4)",
          "[unit][run]") {
    TempDir tmp;
    Run run = Run::start(options_in(tmp.path()));
    run.set("versions", {{"fusion", {{"version", "0.1.0"}}}, {"libraries", {{"z3", "4.8.12"}}}});
    CHECK(failure_code([&] { run.set("status", "ok"); }) == "internal");
    Run::Step s = run.begin(step("mff", "mff", 9));
    core::OutputFile graph = s.context().files.create("merged_graph.xml");
    graph.out() << "<graph/>\n";
    graph.commit();
    s.artefact("merged_graph.xml", "network", "fusion.network", 1);
    s.detail("merge_key", "0e91f9a911c19a7f");
    s.detail("base_merge", "store");
    CHECK(failure_code([&] { s.artefact("missing.xml", "network", "fusion.network", 1); }) == "internal");
    run.end(s, StepStatus::ok);
    run.finish(RunStatus::ok);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["versions"]["libraries"]["z3"] == "4.8.12");
    std::vector<std::string> keys;
    for (const auto& [key, value] : manifest.items()) keys.push_back(key);
    const auto at = [&](std::string_view key) { return std::find(keys.begin(), keys.end(), key) - keys.begin(); };
    CHECK(at("versions") == at("build_id") + 1);   // in its place, not appended
    const core::Json& output = manifest["steps"][0]["outputs"][0];
    CHECK(output.begin().key() == "kind");
    CHECK(output["kind"] == "network");
    CHECK(output["format"] == "fusion.network");
    CHECK(output["format_version"] == 1);
    CHECK(output["path"] == "mff/merged_graph.xml");
    CHECK(manifest["steps"][0]["details"]["merge_key"] == "0e91f9a911c19a7f");
    CHECK(manifest["steps"][0]["details"]["base_merge"] == "store");
}

TEST_CASE("run: misuse is refused before anything is written: a bad log name, a parent run (later), a reserved step "
          "name, a step begun twice",
          "[unit][run]") {
    TempDir tmp;
    for (const std::string name : {"../escape.log", "run.json", "mff", "a/b.log"}) {
        Options options = options_in(tmp.path());
        options.log_name = name;
        CHECK(failure_code([&] { Run::start(options); }) == "run.log_name");
    }
    Options resume = options_in(tmp.path());
    resume.parent = tmp / "earlier";
    CHECK(failure_code([&] { Run::start(resume); }) == "run.resume_unavailable");
    CHECK(fs::is_empty(tmp.path()));

    Run run = Run::start(options_in(tmp.path()));
    CHECK(failure_code([&] { run.begin(step("inputs", "inputs", 1)); }) == "internal");
    CHECK(failure_code([&] { run.begin(step("a/b", "a", 1)); }) == "internal");
    {
        Run::Step s = run.begin(step("cudf", "cudf", 3));
        CHECK_FALSE(run.load_from_parent(s));    // 09.C5 comes later
        run.end(s, StepStatus::ok);
        run.end(s, StepStatus::ok);              // a second end changes nothing
    }
    CHECK(failure_code([&] { run.begin(step("cudf", "cudf", 3)); }) == "internal");
    CHECK(run.finish(RunStatus::ok) == 0);
    CHECK(run.finish(RunStatus::failed) == 0);   // the first finish stands
    CHECK(failure_code([&] { run.begin(step("late", "late", 9)); }) == "internal");
    const core::Json manifest = read_json(run.dir() / "run.json");
    CHECK(manifest["status"] == "ok");
    CHECK(manifest["steps"].size() == 1);
}

TEST_CASE("run: a step's fingerprint changes with its result-changing settings and inputs, not with may-change ones "
          "(09.C5)",
          "[unit][run]") {
    static constexpr std::array<core::SettingSpec, 3> specs{{
        {.name = "bound", .module = "corridor", .type = core::Type::integer, .need = core::Need::defaulted},
        {.name = "timeout", .module = "solve", .type = core::Type::duration, .need = core::Need::defaulted,
         .on_resume = core::OnResume::may_change},
        {.name = "threads", .module = "solve", .type = core::Type::integer, .need = core::Need::defaulted,
         .affects_results = false},
    }};
    const auto make = [&](std::int64_t bound, std::int64_t timeout, std::int64_t threads) {
        return core::Settings({setting(specs[0], bound, core::Source::named),
                               setting(specs[1], timeout, core::Source::named),
                               setting(specs[2], threads, core::Source::named)});
    };
    const std::map<std::string, core::Sha256> inputs{{"network", core::Sha256::of("graph")}};
    const std::string base = step_fingerprint("fb_rch", make(10, 1000, 4), inputs);
    CHECK(base.size() == 64);
    CHECK(step_fingerprint("fb_rch", make(10, 1000, 4), inputs) == base);
    CHECK(step_fingerprint("pathz3", make(10, 1000, 4), inputs) != base);
    CHECK(step_fingerprint("fb_rch", make(11, 1000, 4), inputs) != base);
    CHECK(step_fingerprint("fb_rch", make(10, 1000, 4), {{"network", core::Sha256::of("graph!")}}) != base);
    CHECK(step_fingerprint("fb_rch", make(10, 9999, 4), inputs) == base);   // may_change
    CHECK(step_fingerprint("fb_rch", make(10, 1000, 1), inputs) == base);   // changes no result
    // The canonical fields of 09's example, which core::fingerprint hashes with the build id.
    core::Json fields = core::Json::object();
    fields["command"] = "fb_rch";
    fields["inputs"] = {{"network", core::Sha256::of("graph").hex()}};
    fields["settings"] = {{"corridor.bound", 10}};
    CHECK(base == core::fingerprint(fields));
}

}  // namespace fusion::run::test
