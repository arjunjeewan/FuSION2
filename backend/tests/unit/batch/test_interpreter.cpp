// Running a point (S1.04, S1.10, S1.13, S1.14, S1.28, 01.C11, 01.C12, 01.C15, 01.C20), through 09's real
// Run and the batch's own handlers table, with recording handlers in place of the step modules.
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "fusion/batch/batch.hpp"
#include "fusion/batch/cli.hpp"
#include "fusion/core/build_info.hpp"

using namespace fusion;

namespace {

std::vector<std::string> g_calls;
std::vector<batch::LegacyMove> g_moves;

core::Outcome<void> record(const batch::BoundArgs& args, batch::Workspace&, core::Context&) {
  g_calls.emplace_back(args.command->name);
  return {};
}
core::Outcome<void> record_graph(const batch::BoundArgs& args, batch::Workspace& ws, core::Context&) {
  g_calls.emplace_back(args.command->name);
  ws.graphs.add(std::shared_ptr<const network::Network>());
  return {};
}
core::Outcome<void> empty_corridor(const batch::BoundArgs& args, batch::Workspace& ws, core::Context&) {
  g_calls.emplace_back(args.command->name);
  ws.graphs.add(std::shared_ptr<const corridor::Corridor>());
  core::Outcome<void> out;
  out.report = core::Report{"corridor.empty", "no path from a to b within bound 3: the shortest path has 4 edges"};
  return out;
}
core::Outcome<void> fail(const batch::BoundArgs& args, batch::Workspace&, core::Context&) {
  g_calls.emplace_back(args.command->name);
  throw core::Failure("network.test", "the graph is broken");
}
core::Outcome<void> stop(const batch::BoundArgs& args, batch::Workspace&, core::Context&) {
  g_calls.emplace_back(args.command->name);
  throw core::Stopped();
}
void move(const batch::LegacyMove& m, const batch::Workspace&, core::Context&) { g_moves.push_back(m); }

constexpr std::pair<std::string_view, batch::Handler> kFakes[] = {
    {"rgx", record_graph}, {"cudf", record}, {"fb_rch", empty_corridor}, {"sz", record},
    {"wgx", fail},         {"gsea", stop},   {"help", record},
};
const batch::Handlers kHandlers{kFakes, move, nullptr};
// The stand-ins with the real end of a run, which writes the run page (prompt log entry 387).
const batch::Handlers kWithPage{kFakes, move, batch::step_handlers().finish};

class Recorder final : public core::LogSink {
 public:
  void write(core::Level level, std::string_view code, std::string_view text) override {
    std::lock_guard lock(mutex_);
    const char* tag = level == core::Level::error ? "error: " : level == core::Level::warning ? "warning: " : "";
    lines.push_back(tag + std::string(text) + (code.empty() ? "" : " [" + std::string(code) + "]"));
  }
  bool has(std::string_view piece) const {
    return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(piece) != std::string::npos; });
  }
  std::vector<std::string> lines;
 private:
  std::mutex mutex_;
};

struct Ran {
  int exit = -1;
  std::filesystem::path dir;             // the run directory
  std::shared_ptr<Recorder> console;
  std::vector<nlohmann::json> events;
  nlohmann::json manifest;
  std::vector<std::string> types(std::string_view step = {}) const {
    std::vector<std::string> out;
    for (const auto& e : events)
      if (step.empty() || e.value("step", "") == step) out.push_back(e["type"]);
    return out;
  }
};

// A home with the two reference maps, for runs that use the shipped ones (2B.4), and the fusion-data.json
// of this build's version that the command line's startup check asks for (S1.20). FUSION_HOME is set
// before main, so core's install_dir(), worked out once per process, sees it.
const std::filesystem::path kHome = [] {
  const auto home = std::filesystem::temp_directory_path() /
                    ("fusion_batch_home-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(home / "data");
  std::ofstream(home / "data" / "hsa_to_gene_symbol_map.txt", std::ios::binary) << "hsa1\tONE\nhsa22\tTWENTYTWO\n";
  std::ofstream(home / "data" / "hsa_path_to_path_name_map.txt", std::ios::binary) << "path:hsa1\tPathway one\n";
  std::ofstream(home / "fusion-data.json", std::ios::binary)
      << nlohmann::json{{"fusion_version", std::string(core::build_info().version)}, {"files", nlohmann::json::object()}}
             .dump();
  ::setenv("FUSION_HOME", home.c_str(), 1);
  return home;
}();
struct HomeCleanup { ~HomeCleanup() { std::error_code ec; std::filesystem::remove_all(kHome, ec); } } home_cleanup;

struct Scratch {
  std::filesystem::path dir;
  explicit Scratch(std::string_view name) : dir(std::filesystem::temp_directory_path() / ("fusion_batch_" + std::string(name))) {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~Scratch() { std::filesystem::remove_all(dir); }
};

Ran run_text(const Scratch& scratch, std::string_view text, const batch::Handlers& handlers = kHandlers) {
  g_calls.clear();
  g_moves.clear();
  const auto file = scratch.dir / "t.batch";
  { std::ofstream(file, std::ios::binary) << text; }
  for (const char* input : {"g.xml", "x", "v", "c"})   // the inputs the recording handlers never read
    std::ofstream(scratch.dir / input, std::ios::binary) << "";
  core::CheckReport report;
  batch::CheckOptions options;
  options.inputs = false;
  options.tools = false;
  const auto plan = batch::check(batch::parse(batch::read_source(file), report), options);
  Ran ran;
  ran.console = std::make_shared<Recorder>();
  for (const auto& d : plan.report.diagnostics) ran.console->lines.push_back("check: " + d.message + " [" + d.code + "]");
  batch::RunOptions run;
  run.root = scratch.dir / "runs";
  run.sinks.push_back(ran.console);
  core::Log log(false);
  log.add_sink(ran.console);
  ran.exit = batch::run_batch(plan, run, log, handlers);
  if (std::filesystem::exists(scratch.dir / "runs"))
    for (const auto& e : std::filesystem::directory_iterator(scratch.dir / "runs")) ran.dir = e.path();
  if (!ran.dir.empty()) {
    std::ifstream events(ran.dir / "events.jsonl");
    for (std::string line; std::getline(events, line);) ran.events.push_back(nlohmann::json::parse(line));
    std::ifstream manifest(ran.dir / "run.json");
    ran.manifest = nlohmann::json::parse(manifest);
  }
  return ran;
}

}  // namespace

namespace {


const std::filesystem::path kCase =
    std::filesystem::path(FUSION_TEST_SOURCE_DIR) / "reference" / "synthetic" / "toy-hsa11-wni-tsv-b10";

std::string bytes(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Case {
  // One folder per case: ctest runs the test cases that use it in parallel processes.
  std::filesystem::path dir =
      std::filesystem::temp_directory_path() /
      ("fusion_batch_e2e-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Case() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "inputs");
    for (const auto& e : std::filesystem::directory_iterator(kCase / "inputs"))
      std::filesystem::copy_file(e.path(), dir / "inputs" / e.path().filename());
  }
  ~Case() { std::filesystem::remove_all(dir); }
  int run(std::string_view text, std::string& out, std::string& err) const {
    { std::ofstream(dir / "toy.batch", std::ios::binary) << text; }
    std::istringstream in;
    std::ostringstream o, e;
    const std::vector<std::string> args{"fusion", "-b", (dir / "toy.batch").string(), "--root", (dir / "runs").string()};
    const int exit = cli::main_utf8(args, cli::Streams{in, o, e});
    out = o.str();
    err = e.str();
    return exit;
  }
  nlohmann::json manifest() const {
    for (const auto& e : std::filesystem::directory_iterator(dir / "runs")) {
      std::ifstream in(e.path() / "run.json");
      return nlohmann::json::parse(in);
    }
    return {};
  }
};

}  // namespace










TEST_CASE("batch: run options from the command line, the run line and the batch file (01.C7)", "[batch][interpreter]") {
  core::CheckReport report;
  batch::CheckOptions options;
  options.inputs = options.tools = false;
  const auto plan = batch::check(
      batch::parse(batch::source_from_text("/data/exp/egfr.batch", "run root=out label=jun log=x.log\nsz 0\n"), report),
      options);
  batch::RunOptions command_line;
  auto o = batch::run_options(plan, command_line);
  CHECK(o.root == "/data/exp/out");   // a relative root is the batch file's folder's
  CHECK(o.label == "jun");
  CHECK(o.log_name == "x.log");
  CHECK(o.batch == "/data/exp/egfr.batch");
  command_line.label = "cli";
  command_line.root = "/elsewhere";
  o = batch::run_options(plan, command_line);
  CHECK(o.label == "cli");
  CHECK(o.root == "/elsewhere");
  const auto bare = batch::check(batch::parse(batch::source_from_text("/data/exp/egfr.batch", "sz 0\n"), report), options);
  o = batch::run_options(bare, batch::RunOptions{});
  CHECK(o.root == "/data/exp");
  CHECK(o.label == "egfr");
}

// Every case that starts 09's Run shares one test case, so one process pays core::build_id()'s hash of
// the (debug) test executable once; ctest runs each test case in a process of its own.
TEST_CASE("batch: running batches through 09's Run", "[batch][interpreter]") {
  SECTION("a reported result skips every later line, and the run exits 0 (01.C15)") {
    Scratch s("reported");
    const auto ran = run_text(s, "rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 3 0\nsz 0\nexec echo never\ncudf values=x up=1 down=-1\n");
    CHECK(ran.exit == 0);
    CHECK(g_calls == std::vector<std::string>{"rgx", "fb_rch"});
    CHECK(ran.types("fb_rch") == std::vector<std::string>{"step.started", "step.finished"});
    CHECK(ran.types("sz") == std::vector<std::string>{"step.skipped"});
    CHECK(ran.types("exec") == std::vector<std::string>{"step.skipped"});   // exec lines too (09.C14)
    const auto finished = std::find_if(ran.events.begin(), ran.events.end(), [](const nlohmann::json& e) {
      return e["type"] == "step.finished" && e["step"] == "fb_rch";
    });
    REQUIRE(finished != ran.events.end());
    CHECK((*finished)["status"] == "reported");
    CHECK((*finished)["report"]["code"] == "corridor.empty");
    CHECK(ran.manifest["status"] == "ok");
    CHECK(ran.manifest["reports"].size() == 1);
    CHECK(ran.console->has("t.batch:2: result: no path from a to b within bound 3"));
  }
  SECTION("the first failure stops the batch with status 1 (S1.28)") {
    Scratch s("failure");
    const auto ran = run_text(s, "rgx g.xml y 1\nwgx 0 out.xml KEGG\ncudf values=x up=1 down=-1\n");
    CHECK(ran.exit == 1);
    CHECK(g_calls == std::vector<std::string>{"rgx", "wgx"});
    CHECK(ran.console->has("error: t.batch:2: wgx: the graph is broken [network.test]"));
    CHECK(ran.manifest["status"] == "failed");
    CHECK(ran.manifest["error"]["code"] == "network.test");
    CHECK(ran.manifest["error"]["line"] == 2);
  }
  SECTION("a stop ends the step, skips the rest and exits 3 (A1)") {
    Scratch s("stopped");
    const auto ran = run_text(s, "gsea values=v candidates=c selector=fdr:0.25 shortlist=s.txt\nhelp\n");
    CHECK(ran.exit == 3);
    REQUIRE_FALSE(ran.types("gsea").empty());
    CHECK(ran.types("gsea").back() == "step.finished");
    CHECK(ran.types("help") == std::vector<std::string>{"step.skipped"});
    CHECK(ran.manifest["status"] == "stopped");
  }
  SECTION("the echo, exit and the end of the file (S1.04, S1.10, S1.13)") {
    Scratch s("echo");
    const auto ran = run_text(s, "let $G 0\nrgx g.xml y 1\nsz $G\nexit\ncudf values=x up=1 down=-1\n");
    CHECK(ran.exit == 0);
    CHECK(g_calls == std::vector<std::string>{"rgx", "sz"});   // nothing after exit runs
    const auto& lines = ran.console->lines;
    const auto at = std::find(lines.begin(), lines.end(), "sz 0 ");
    REQUIRE(at != lines.end());
    REQUIRE(at + 1 != lines.end());
    CHECK(*(at + 1) == "-------------------------------");
    CHECK(std::find(lines.begin(), lines.end(), "exit ") != lines.end());
    CHECK(std::none_of(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("let"); }));
    // Without exit, the end of the file ends the run (S1.04).
    const auto no_exit = run_text(s, "rgx g.xml y 1\nsz 0\n");
    CHECK(no_exit.exit == 0);
    CHECK(g_calls == std::vector<std::string>{"rgx", "sz"});
  }
  SECTION("exec runs in the shell, keeps its spacing and fails on a non-zero status (S1.14)") {
    Scratch s("exec");
    const auto ok = run_text(s, "exec echo \"a  b\" > spaced.txt\nexec echo hello from the shell\n");
    CHECK(ok.exit == 0);
    std::ifstream spaced(s.dir / "spaced.txt");   // exec runs in the batch file's folder (01.C2)
    std::string text;
    std::getline(spaced, text);
    CHECK(text == "a  b");                        // FuSION 1 collapsed the two spaces
    CHECK(ok.console->has("hello from the shell"));   // the shell's output reaches the log (row 1.15)
    std::ifstream log(ok.dir / (ok.dir.filename().string() + ".log"));
    std::string all((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
    CHECK(all.find("hello from the shell") != std::string::npos);

    const auto bad = run_text(s, "exec exit 3\nexec echo never\n");
    CHECK(bad.exit == 1);
    CHECK(bad.console->has("t.batch:1: exec: shell exited with status 3: exit 3 [exec.status]"));
    CHECK_FALSE(bad.console->has("never"));
  }
  SECTION("FuSION 1's fixed-name moves go to their handler, not the shell (01.C11)") {
    Scratch s("moves");
    const auto ran = run_text(s, "exec mv PO.dat /w/x.dat\nexec mv limits_timefile.txt rel/limits.txt\n"
                                 "exec mv mylog.txt /w/mylog.txt\nexec mv other.txt /w/o.txt\n");
    // The last line is an ordinary mv, which the shell runs and which fails: no such file.
    CHECK(ran.exit == 1);
    REQUIRE(g_moves.size() == 3);
    CHECK(g_moves[0].name == batch::LegacyName::po_dat);
    CHECK(g_moves[0].dest == "/w/x.dat");
    CHECK(g_moves[1].dest == s.dir / "rel/limits.txt");   // relative to the batch file's folder
    CHECK(g_moves[2].name == batch::LegacyName::mylog);
  }
  SECTION("a window plot is skipped without a display (01.C20)") {
    Scratch s("window");
    const char* display = std::getenv("DISPLAY");
    const std::string saved = display ? display : "";
    ::unsetenv("DISPLAY");
    ::unsetenv("WAYLAND_DISPLAY");
    const auto ran = run_text(s, "exec gnuplot -p -e \"plot sin(x)\"\n");
    if (!saved.empty()) ::setenv("DISPLAY", saved.c_str(), 1);
    CHECK(ran.exit == 0);
    CHECK(ran.console->has("no display is available, so this gnuplot window is skipped"));
  }
  SECTION("an input the batch makes is checked just before its step (01.C12)") {
    Scratch s("produced");
    const auto made = run_text(s, "exec touch made.tsv\ncudf values=made.tsv up=1 down=-1\n");
    CHECK(made.exit == 0);
    CHECK(g_calls == std::vector<std::string>{"cudf"});
    const auto missing = run_text(s, "exec echo made.tsv\ncudf values=never.tsv up=1 down=-1\n");
    CHECK(missing.exit == 1);   // not made by line 1 (its text is not there): refused before the run
    const auto late = run_text(s, "exec echo late.tsv\ncudf values=late.tsv up=1 down=-1\n");
    CHECK(late.exit == 1);      // named by line 1, which did not make it: the step fails
    CHECK(late.console->has("[input.missing]"));
    CHECK(late.console->has("line 1"));
  }
  SECTION("every run ends with its run page, which says how the run ended (07.C2, 09; prompt log entry 387)") {
    const auto page_of = [](const Ran& ran) {
      std::ifstream in(ran.dir / "index.html", std::ios::binary);
      return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };
    Scratch failing("page_failed");
    const auto failed = run_text(failing, "rgx g.xml y 1\nwgx 0 out.xml KEGG\n", kWithPage);
    CHECK(failed.exit == 1);
    const std::string page = page_of(failed);
    CHECK(page.find("<span class=\"bad\">failed</span>") != std::string::npos);
    CHECK(page.find("<p class=\"end\">line 2, wgx: the graph is broken</p>") != std::string::npos);
    CHECK(page.find("No solve finished before the run ended.") != std::string::npos);
    CHECK(page.find(failed.manifest["name"].get<std::string>() + " · ") != std::string::npos);
    CHECK(failed.manifest["outputs"].back()["path"] == "index.html");

    Scratch stopping("page_stopped");
    const auto stopped = run_text(stopping, "gsea values=v candidates=c selector=fdr:0.25 shortlist=s.txt\n", kWithPage);
    CHECK(stopped.exit == 3);
    CHECK(page_of(stopped).find("<span class=\"bad\">stopped on request</span>") != std::string::npos);

    Scratch reporting("page_reported");
    const auto reported = run_text(reporting, "rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 3 0\n", kWithPage);
    CHECK(reported.exit == 0);
    const std::string finished = page_of(reported);
    CHECK(finished.find(" · finished</p>") != std::string::npos);
    CHECK(finished.find("This run has no solve, so there is no frontier to show.") != std::string::npos);

    // The check of the inputs' copies fails before the first step: the page still says why.
    Scratch unchecked("page_unchecked");
    const auto refused = run_text(unchecked, "cudf values=never.tsv up=1 down=-1\n", kWithPage);
    CHECK(refused.exit == 1);
    REQUIRE_FALSE(refused.dir.empty());
    const std::string before = page_of(refused);
    CHECK(before.find("<span class=\"bad\">failed</span>") != std::string::npos);
    CHECK(before.find("<p class=\"end\">the check of the inputs failed: line 1, ") != std::string::npos);
  }
  SECTION("a check error ends fusion before any run directory exists (09.C14)") {
    Scratch s("check_error");
    const auto ran = run_text(s, "bogus line\n");
    CHECK(ran.exit == 1);
    CHECK_FALSE(std::filesystem::exists(s.dir / "runs"));
  }
  SECTION("the toy case's named pipeline gives FuSION 1's frontier (S8.25, 01.C11)") {
    if (!std::filesystem::exists(kCase)) return;
    Case c;
    std::string out, err;
    const int exit = c.run(
        "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
        "cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
        "rgx file=inputs/with_node_included.xml tool_written=y\n"
        "fb_rch source=hsa1 target=hsa22 bound=10\n"
        "pathz3 fsc=hsa11 node_max=7 edge_max=7 timeout=150s incremental_timeout=30s\n"
        "exec mv PO.dat $FUSION_BATCH_DIR/w.dat\n",
        out, err);
    INFO(out << err);
    REQUIRE(exit == 0);
    // The with-FSC case is the solve's first: FuSION 1's ..._w_hsa11_....dat, byte for byte.
    CHECK(bytes(c.dir / "w.dat") ==
          bytes(kCase / "f1" / "run" / "hsa1_to_hsa22_w_hsa11_with_node_included.tsv_b10.dat"));
    CHECK(bytes(c.dir / "runs" / c.manifest()["name"].get<std::string>() / "cudf" / "up_reg") ==
          bytes(kCase / "f1" / "run" / "up_reg"));
    const auto manifest = c.manifest();
    CHECK(manifest["status"] == "ok");
    CHECK(manifest["versions"]["libraries"].contains("z3"));   // section 11's blocks in run.json
    CHECK(manifest["platform"].contains("logical_cpus"));
    CHECK(manifest["install"].contains("home"));
    CHECK(manifest["environment"].contains("FUSION_DOT"));
    CHECK(manifest["tools"].is_object());
    std::vector<std::string> ids;
    for (const auto& s : manifest["steps"]) ids.push_back(s["id"]);
    CHECK(ids == std::vector<std::string>{"start", "cudf", "rgx", "fb_rch", "pathz3", "exec"});
    CHECK(out.find("pathz3 fsc=hsa11 node_max=7 edge_max=7 timeout=150s incremental_timeout=30s \n") != std::string::npos);
  }
  SECTION("the run page links every frontier point's page and the Pareto plot (07.C2; prompt log entry 387)") {
    if (!std::filesystem::exists(kCase)) return;
    Case c;
    std::string out, err;
    const int exit = c.run(
        "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
        "cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
        "rgx file=inputs/with_node_included.xml tool_written=y\n"
        "fb_rch source=hsa1 target=hsa22 bound=10\n"
        "pathz3 fsc=hsa11 node_max=7 edge_max=7 timeout=150s incremental_timeout=30s\n"
        "plot_pareto\n"
        "pex\n",
        out, err);
    INFO(out << err);
    REQUIRE(exit == 0);
    const auto run = c.dir / "runs" / c.manifest()["name"].get<std::string>();
    const std::string page = bytes(run / "index.html");
    CHECK(page.find(" · finished</p>") != std::string::npos);
    CHECK(page.find("<script") == std::string::npos);   // static, no script (07)
    CHECK(page.find("<img src=\"plot_pareto/pareto.png\"") != std::string::npos);
    CHECK(std::filesystem::exists(run / "plot_pareto" / "pareto.png"));
    std::ifstream in(run / "pathz3" / "results.json");
    const auto results = nlohmann::json::parse(in);
    std::size_t points = 0;
    for (const auto& kase : results["cases"]) points += kase["points"].size();
    REQUIRE(points > 0);
    std::size_t links = 0;
    const std::string link = "href=\"pex/explain/";
    for (auto at = page.find(link); at != std::string::npos; at = page.find(link, at + 1)) {
      const auto start = at + 6, end = page.find('"', start);
      CHECK(std::filesystem::exists(run / page.substr(start, end - start)));
      ++links;
    }
    CHECK(links == points);   // each point links its own page, and each page exists
    CHECK(page.find("href=\"pathz3/results.json\"") != std::string::npos);
  }
  SECTION("an empty corridor is a reported result; the solve is skipped and the run exits 0 (S4.18)") {
    if (!std::filesystem::exists(kCase)) return;
    Case c;
    std::string out, err;
    const int exit = c.run(
        "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
        "cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
        "rgx file=inputs/with_node_included.xml tool_written=y\n"
        "fb_rch source=hsa1 target=hsa22 bound=3\n"
        "pathz3 fsc=hsa11\n"
        "exec echo never\n",
        out, err);
    INFO(out << err);
    CHECK(exit == 0);
    const auto manifest = c.manifest();
    CHECK(manifest["status"] == "ok");
    REQUIRE(manifest["reports"].size() == 1);
    CHECK(manifest["reports"][0]["code"] == "corridor.empty");
    CHECK(manifest["steps"][4]["status"] == "skipped");
    CHECK(manifest["steps"][5]["status"] == "skipped");
    CHECK(out.find("never\n") == std::string::npos);
  }
  SECTION("the toy case's FuSION 1 batch, as written, gives FuSION 1's frontiers (0017, 01.C11, 01.C13)") {
    if (!std::filesystem::exists(kCase)) return;
    const auto toy = std::filesystem::path(FUSION_TEST_SOURCE_DIR) / "fixtures" / "toy-batches" / "toy-hsa11-wni-tsv-b10.batch";
    if (!std::filesystem::exists(toy)) return;
    Case c;
    // The reference runner's path map, and the GUI template's unfilled placeholders, which S5.02 refuses
    // as numbers; the gnuplot lines need a gnuplot this test does not assume.
    std::string text;
    std::istringstream lines(bytes(toy));
    for (std::string line; std::getline(lines, line);) {
      if (line.find("gnuplot") != std::string::npos) continue;
      for (std::size_t at; (at = line.find("/fusion-test/case")) != std::string::npos;)
        line.replace(at, 17, c.dir.string());
      for (const char* placeholder : {"%__a21__%", "%__a22__%", "%__a23__%"})
        for (std::size_t at; (at = line.find(placeholder)) != std::string::npos;) line.erase(at, 9);
      text += line + "\n";
    }
    std::string out, err;
    const int exit = c.run(text, out, err);
    INFO(out << err);
    REQUIRE(exit == 0);
    const auto run_dir = c.dir / "hsa1_to_hsa22_fsc_hsa11_with_node_included.tsv_b10";
    for (const char* dat : {"hsa1_to_hsa22_w_hsa11_with_node_included.tsv_b10.dat",
                            "hsa1_to_hsa22_wo_hsa11_with_node_included.tsv_b10.dat"})
      CHECK(bytes(run_dir / dat) == bytes(kCase / "f1" / "run" / dat));   // written natively (01.C11)
    CHECK(bytes(run_dir / "up_reg") == bytes(kCase / "f1" / "run" / "up_reg"));
    CHECK(out.find("rgx: graph 2: 22 nodes, 25 edges, from memory: this run wrote the file (S2.26)") != std::string::npos);
    CHECK(out.find("Corridor, graph 4: same as graph 3") != std::string::npos);   // 05.C4
    CHECK(out.find("exec: mv edges_merged_filename: the latest mff read FuSION 1's merged master") != std::string::npos);
    CHECK(err.find("[batch.ignored_tokens]") != std::string::npos);   // mff's xml token and map list
    // Results hold no absolute path (11-build.md:575): each positional solve is named by its prefix's last
    // component, and run.json keeps the whole prefix as the solve.legacy_prefix setting (prompt log entry 372).
    const auto manifest = c.manifest();
    const auto run = c.dir / "runs" / manifest["name"].get<std::string>();
    std::vector<std::string> names;
    for (const auto& step : manifest["steps"]) {
      if (step["cmd"] != "pathz3") continue;
      std::ifstream in(run / step["id"].get<std::string>() / "results.json");
      const auto results = nlohmann::json::parse(in);
      names.push_back(results["solve"]["name"].get<std::string>());
      // The second fb_rch reuses the first's corridor and writes no file (05.C4), so both solves name the
      // file the first saved (Phase 3, prompt log entry 376).
      CHECK(results["corridor"]["artefact"] == "fb_rch/corridor.json");
      CHECK(std::filesystem::exists(run / results["corridor"]["artefact"].get<std::string>()));
      std::string prefix;
      for (const auto& s : step["settings"])
        if (s["key"] == "solve.legacy_prefix") prefix = s["value"].get<std::string>();
      CHECK(std::filesystem::path(prefix).is_absolute());
      CHECK(prefix.ends_with("/" + names.back()));
    }
    CHECK(names == std::vector<std::string>{"temp_w", "temp_wo"});
  }
  SECTION("the shipped maps a run uses are recorded by path and SHA-256 (02, row 2B.4)") {
    if (!std::filesystem::exists(kCase)) return;
    Case c;
    std::string out, err;
    REQUIRE(c.run("cudf values=inputs/with_node_included.tsv up=0.9 down=-0.9\n"
                  "rgx file=inputs/with_node_included.xml tool_written=y\n"
                  "sz\n",
                  out, err) == 0);
    const auto manifest = c.manifest();
    const auto& maps = manifest["steps"][1]["details"]["maps"];   // rgx is the first step that needs labels
    const auto symbols = kHome / "data" / "hsa_to_gene_symbol_map.txt";
    CHECK(maps["symbols"]["path"] == symbols.string());
    CHECK(maps["symbols"]["sha256"] == core::Sha256::of_file(symbols).hex());
    CHECK(maps["symbols"]["shipped"] == true);
    CHECK(maps["pathway_names"]["shipped"] == true);
    CHECK(maps["id_map"].is_null());
    bool listed = false;   // and read through the run's checked copy, so run.json lists it as an input (09.C3)
    for (const auto& input : manifest["inputs"]) listed = listed || input["path"] == symbols.string();
    CHECK(listed);
    // A start that names a map uses its own, and the shipped one only for the map it leaves out.
    std::string out2, err2;
    REQUIRE(c.run("start symbols=inputs/empty_file\nrgx file=inputs/with_node_included.xml tool_written=y\n", out2, err2) == 0);
    std::filesystem::path latest;
    for (const auto& e : std::filesystem::directory_iterator(c.dir / "runs"))
      if (latest.empty() || e.path().filename() > latest.filename()) latest = e.path();
    std::ifstream in(latest / "run.json");
    const auto second = nlohmann::json::parse(in);
    const auto& start_maps = second["steps"][0]["details"]["maps"];
    CHECK(start_maps["symbols"]["shipped"] == false);
    CHECK(start_maps["pathway_names"]["shipped"] == true);
  }
  SECTION("gsea maps its keep ids and records its base, seed and permutations (08, S7.14, 2B.10)") {
    Case c;
    // Two synthetic pathways of 20 genes each (no KEGG content): KGML-shaped, with made-up ids.
    std::string list;
    for (int p = 0; p < 2; ++p) {
      const std::string id = "hsa9000" + std::to_string(p);
      std::string kgml = "<?xml version=\"1.0\"?>\n<pathway name=\"path:" + id + "\" org=\"hsa\" number=\"" +
                         id.substr(3) + "\" title=\"Synthetic " + std::to_string(p) + "\">\n";
      for (int g = 1; g <= 20; ++g)
        kgml += "  <entry id=\"" + std::to_string(g) + "\" name=\"hsa:" + std::to_string(p * 20 + g) +
                "\" type=\"gene\"/>\n";
      kgml += "</pathway>\n";
      std::ofstream(c.dir / "inputs" / (id + ".xml"), std::ios::binary) << kgml;
      list += id + ".xml\n";
    }
    std::ofstream(c.dir / "inputs" / "list.txt", std::ios::binary) << list;
    std::string values;
    for (int g = 1; g <= 40; ++g) values += "hsa" + std::to_string(g) + "\t" + std::to_string(g % 7 - 3) + ".5\n";
    std::ofstream(c.dir / "inputs" / "values.tsv", std::ios::binary) << values;
    std::string out, err;
    const int exit = c.run("gsea values=inputs/values.tsv candidates=inputs/list.txt selector=nes:100 "
                           "shortlist=s.txt keep=hsa:25 seed=7 permutations=10\n",
                           out, err);
    INFO(out << err);
    REQUIRE(exit == 0);
    const auto manifest = c.manifest();
    const std::string run = c.dir.string() + "/runs/" + manifest["name"].get<std::string>();
    std::ifstream shortlist(run + "/gsea/s.txt");
    std::string chosen((std::istreambuf_iterator<char>(shortlist)), std::istreambuf_iterator<char>());
    CHECK(chosen == "hsa90001.xml\n");   // nes:100 chooses nothing; hsa:25 is hsa25, in the second pathway
    const auto& details = manifest["steps"][0]["details"];
    CHECK(details["base"] == (c.dir / "inputs").string());
    CHECK(details["seed"] == 7);
    CHECK(details["permutations"] == 10);
  }
  SECTION("fusion --version prints the build (01.C14)") {
    std::istringstream in;
    std::ostringstream out, err;
    CHECK(cli::main_utf8(std::vector<std::string>{"fusion", "--version"}, cli::Streams{in, out, err}) == 0);
    CHECK(out.str().starts_with("FuSION "));
    std::ostringstream json;
    CHECK(cli::main_utf8(std::vector<std::string>{"fusion", "--version", "--json"}, cli::Streams{in, json, err}) == 0);
    const auto v = nlohmann::json::parse(json.str());   // section 11's blocks, as run.json records them
    CHECK(v.contains("build_id"));
    CHECK(v["versions"]["fusion"]["version"] == v["fusion"]);
    for (const char* field : {"git_commit", "git_dirty", "build_type", "compiler", "target"})
      CHECK(v["versions"]["fusion"].contains(field));
    CHECK(v["versions"]["libraries"]["z3"].get<std::string>().starts_with(std::string(FUSION_TEST_Z3_VERSION) + "."));
    CHECK(v["versions"]["libraries"].contains("pugixml"));
    CHECK(v["install"].contains("home"));
    CHECK(v["install"].contains("prefix"));
    CHECK(v["platform"]["logical_cpus"].get<int>() > 0);
    CHECK(v["environment"].contains("FUSION_HOME"));
    CHECK(v["tools"].contains("sh"));
    CHECK(v["tools"]["sh"]["ok"] == true);
    CHECK(v["data"]["reference_maps"].contains("symbols"));
    CHECK(v["data"]["reference_maps"].contains("pathway_names"));
  }
  SECTION("a failing step names its batch line and exits 1 (S1.28)") {
    if (!std::filesystem::exists(kCase)) return;
    Case c;
    std::string out, err;
    const int exit = c.run(
        "start symbols=inputs/empty_file pathway_names=inputs/empty_file\n"
        "rgx file=inputs/with_node_included.xml tool_written=y\n"
        "fb_rch source=hsa1 target=hsa999 bound=10\n"
        "sz\n",
        out, err);
    CHECK(exit == 1);
    CHECK(err.find("toy.batch:3: fb_rch: ") != std::string::npos);
    CHECK(c.manifest()["status"] == "failed");
    CHECK(c.manifest()["steps"][3]["status"] != "ok");   // the batch stopped at the failure
  }
}
