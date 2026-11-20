// The command line and its exit statuses (S1.01 to S1.03, 01.C9, 01.C14), with --check --json and
// --commands, run through fusion::cli as the executable runs it.
#include <chrono>
#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "fusion/batch/batch.hpp"
#include "fusion/batch/cli.hpp"
#include "fusion/solve/solve.hpp"

using namespace fusion;

namespace {

struct Result { int exit; std::string out, err; };

Result invoke(std::vector<std::string> args, const std::string& stdin_text = {}) {
  args.insert(args.begin(), "fusion");
  std::istringstream in(stdin_text);
  std::ostringstream out, err;
  const int exit = cli::main_utf8(args, cli::Streams{in, out, err});
  return {exit, out.str(), err.str()};
}

// One folder per process: ctest runs each test case in its own process, at the same time, and each
// process removes its folder at exit.
const std::filesystem::path kScratch =
    std::filesystem::temp_directory_path() /
    ("fusion_batch_cli-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
struct Cleanup { ~Cleanup() { std::error_code ec; std::filesystem::remove_all(kScratch, ec); } } cleanup;

std::filesystem::path write_batch(std::string_view name, std::string_view text) {
  const auto& dir = kScratch;
  std::filesystem::create_directories(dir);
  const auto file = dir / name;
  std::ofstream(file, std::ios::binary) << text;
  return file;
}

}  // namespace

TEST_CASE("batch: options parse, and usage errors exit 2 on stderr (S1.02, S1.03, 01.C14)", "[batch][cli]") {
  const auto parsed = cli::parse(std::vector<std::string>{"fusion", "-b", "x.batch", "--root", "/r", "--label", "egfr",
                                                          "--set", "network.indirect=keep", "--tag", "session=s1",
                                                          "--debug", "--jobs", "4", "--point", "1,fdr0.3"});
  REQUIRE(std::holds_alternative<cli::Options>(parsed));
  const auto& o = std::get<cli::Options>(parsed);
  CHECK(o.batch == std::filesystem::path("x.batch"));
  CHECK(o.root == std::filesystem::path("/r"));
  CHECK(o.label == std::optional<std::string>("egfr"));
  CHECK(o.settings == std::vector<std::pair<std::string, std::string>>{{"network.indirect", "keep"}});
  CHECK(o.tags.size() == 1);
  CHECK(o.debug);
  CHECK(o.jobs == std::optional<unsigned>(4));
  CHECK(o.points == std::vector<std::string>{"1", "fdr0.3"});
  CHECK(o.command_line.size() == 16);

  for (const auto& bad : std::vector<std::vector<std::string>>{
           {}, {"-d"}, {"-b"}, {"--frob"}, {"x.batch"}, {"--check"}, {"-b", "x", "--json"}, {"-b", "x", "--set", "k"},
           {"-b", "x", "--jobs", "0"}, {"-b", "x", "--commands"}, {"--root", "/r"}}) {
    const auto r = invoke(bad);
    INFO(r.err);
    CHECK(r.exit == cli::exit_usage);
    CHECK(r.out.empty());
    CHECK(r.err.find("Run as \"fusion -b <batch_file>\"") != std::string::npos);
  }
  CHECK(invoke({"-d"}).err.find("the interactive prompt was dropped (S1.03)") != std::string::npos);
}

TEST_CASE("batch: -h prints the usage and the listing on stdout, status 0 (S1.02, S1.12)", "[batch][cli]") {
  const auto all = invoke({"-h"});
  CHECK(all.exit == 0);
  CHECK(all.err.empty());
  CHECK(all.out.starts_with("Run as \"fusion -b <batch_file>\"\n"));
  CHECK(all.out.find("fb_rch (corridor)      <graph> <source>... -1 <target>... -1 <exclude>... -1 <bound> <edges_to_target>") !=
        std::string::npos);
  CHECK(all.out.find("graph= source= target= exclude= bound= edges_to_target= skip_subtypes= observations=") !=
        std::string::npos);
  CHECK(all.out.find("mff (merge_from_file)") != std::string::npos);
  CHECK(all.out.find("upfx") == std::string::npos);   // a dropped command is not listed
  const auto one = invoke({"-h", "fb_rch"});
  CHECK(one.exit == 0);
  CHECK(one.out.find("corridor.bound") != std::string::npos);
  CHECK(invoke({"-h", "corridor"}).exit == 0);             // by alias (S1.11)
  CHECK(invoke({"-h", "nonsense"}).exit == cli::exit_usage);
}

TEST_CASE("batch: --commands exports the table, rules and run-level settings (01.C9)", "[batch][cli]") {
  const auto r = invoke({"--commands"});
  REQUIRE(r.exit == 0);
  const auto json = nlohmann::json::parse(r.out);
  CHECK(json["format"] == 1);                             // what the frontend checks (10.C12)
  CHECK(json["fusion"].is_string());
  const auto& commands = json["commands"];
  const auto find = [&](std::string_view name) {
    for (const auto& c : commands)
      if (c["name"] == name) return c;
    return nlohmann::json();
  };
  const auto cudf = find("cudf");
  REQUIRE(cudf.is_object());
  CHECK(cudf["aliases"] == nlohmann::json::array({"create_up_down_file"}));
  CHECK(cudf["rules"] == nlohmann::json::array({{{"kind", "less"}, {"a", "down"}, {"b", "up"}}}));
  CHECK(cudf["needs_run_dir"] == false);   // allowed in --query
  CHECK(cudf["arguments"][0]["name"] == "values");
  CHECK(cudf["arguments"][0]["type"] == "input_file");
  CHECK(cudf["arguments"][0]["need"] == "required");
  const auto z3 = find("pathz3");
  bool timeout_ok = false;
  for (const auto& a : z3["arguments"])
    if (a["name"] == "timeout") timeout_ok = a["type"] == "duration" && a["default"] == "4h";
  CHECK(timeout_ok);
  const auto pareto = find("plot_pareto");
  CHECK(pareto["tools"][0]["id"] == "gnuplot");
  CHECK(pareto["tools"][0]["features"] == nlohmann::json::array({"pngcairo"}));
  CHECK(find("upfx")["dropped"].get<std::string>().find("S2.29") != std::string::npos);
  bool indirect = false;
  for (const auto& s : json["run_settings"]) indirect = indirect || s["key"] == "network.indirect";
  CHECK(indirect);
}

TEST_CASE("batch: --check --json reports every problem with its code (09.C12, row 1.9)", "[batch][cli]") {
  const auto broken = write_batch("broken.batch",
                                  "let $X 1\nlet $X 2\ncudf values=/nowhere/lfc.tsv up=0 down=0\nfrobnicate\n");
  const auto r = invoke({"-b", broken.string(), "--check", "--json"});
  CHECK(r.exit == cli::exit_failed);
  const auto json = nlohmann::json::parse(r.out);
  CHECK(json["ok"] == false);
  std::vector<std::string> codes;
  for (const auto& d : json["diagnostics"]) codes.push_back(d["code"]);
  CHECK(codes == std::vector<std::string>{"batch.redefined", "batch.bad_value", "input.missing", "batch.unknown_command"});
  CHECK(json["diagnostics"][2]["command"] == "cudf");
  CHECK(json["diagnostics"][2]["setting"] == "values");
  CHECK(json["diagnostics"][2]["path"] == "/nowhere/lfc.tsv");

  const auto console = invoke({"-b", broken.string(), "--check"});
  CHECK(console.exit == cli::exit_failed);
  CHECK(console.err.find("error: broken.batch:2: let: $X is already defined at line 1 [batch.redefined]") !=
        std::string::npos);

  // Every diagnostic names its argument and, where one applies, its path (cross-section item 1.9).
  const auto typed = write_batch("typed.batch", "cudf values=/nowhere/v.tsv up=1.01x down=-1 up_out=/w/a down_out=/w/a\n");
  const auto t = nlohmann::json::parse(invoke({"-b", typed.string(), "--check", "--json"}).out);
  bool bad_value = false, clash = false;
  for (const auto& d : t["diagnostics"]) {
    if (d["code"] == "batch.bad_value") bad_value = d["setting"] == "up" && d["command"] == "cudf";
    if (d["code"] == "output.clash") clash = d["setting"] == "down_out" && d["path"] == "/w/a";
  }
  CHECK(bad_value);
  CHECK(clash);

  const auto good = write_batch("good.batch", "let $X 1\nexec echo $X\nhelp\n");
  const auto ok = invoke({"-b", good.string(), "--check", "--json"});
  CHECK(ok.exit == 0);
  CHECK(nlohmann::json::parse(ok.out)["ok"] == true);
}

TEST_CASE("batch: a missing or empty batch file is a usage error, status 2 (S1.01)", "[batch][cli]") {
  CHECK(invoke({"-b", "/nowhere/x.batch"}).exit == cli::exit_usage);
  CHECK(invoke({"-b", "/nowhere/x.batch", "--check"}).exit == cli::exit_usage);
  const auto empty = write_batch("empty.batch", ";; nothing\n\n");
  const auto r = invoke({"-b", empty.string()});
  CHECK(r.exit == cli::exit_usage);
  CHECK(r.err.find("holds no command") != std::string::npos);
}

// --version needs core::build_id(), which hashes the executable of a dirty build: it is tested with the
// runs, in test_interpreter.cpp, so that one process pays for the hash.
TEST_CASE("batch: --stop and --query (01.C14)", "[batch][cli]") {
  const auto dir = std::filesystem::temp_directory_path() / "fusion_batch_cli_stop";
  std::filesystem::create_directories(dir);
  CHECK(invoke({"--stop", dir.string()}).exit == 0);
  CHECK(std::filesystem::exists(dir / "stop"));
  std::filesystem::remove_all(dir);
  CHECK(invoke({"--stop", "/nowhere/run"}).exit == cli::exit_failed);
  // --query refuses a command that needs a run directory (entry 168).
  const auto query = invoke({"--query"}, "pathz3 fsc=hsa1\n");
  CHECK(query.exit == 1);
  CHECK(nlohmann::json::parse(query.out)["error"]["code"] == "batch.bad_argument");
}

TEST_CASE("batch: --query answers labels, cudf and suggest_thresholds lines (04.C8, row 2B.6)", "[batch][cli]") {
  const auto map = write_batch("symbols.txt", "hsa3725\tJUN\nhsa1956\tEGFR\nhsa10\tNAT2\n");
  const auto labels = invoke({"--query"}, "labels map=" + map.string() + "\n");
  INFO(labels.out << labels.err);
  REQUIRE(labels.exit == 0);
  const auto answer = nlohmann::json::parse(labels.out);   // network::labels_answer's JSON
  CHECK(answer["map"]["path"] == map.string());
  CHECK(answer["map"]["sha256"] == core::Sha256::of_file(map).hex());
  CHECK(answer["labels"] == nlohmann::json::array({{"hsa10", "NAT2"}, {"hsa1956", "EGFR"}, {"hsa3725", "JUN"}}));

  const auto values = write_batch("values.tsv", "hsa1 2.5\nhsa2 -2\nhsa3 0.1\n");
  const auto cudf = invoke({"--query"}, "cudf values=" + values.string() + " up=1 down=-1\n");
  INFO(cudf.out << cudf.err);
  REQUIRE(cudf.exit == 0);
  const auto c = nlohmann::json::parse(cudf.out);
  CHECK(c["ok"] == true);
  CHECK(c["n_up"] == 1);
  CHECK(c["n_down"] == 1);
  CHECK(std::filesystem::exists(kScratch / "values.tsv"));
  CHECK_FALSE(std::filesystem::exists(std::filesystem::current_path() / "up_reg"));   // a query writes nothing
  const auto bad = invoke({"--query"}, "cudf values=" + values.string() + " up=0 down=0\n");
  CHECK(bad.exit == 1);
  CHECK(nlohmann::json::parse(bad.out)["ok"] == false);
  const auto sth = invoke({"--query"}, "suggest_thresholds values=" + values.string() + "\n");
  REQUIRE(sth.exit == 0);
  CHECK(nlohmann::json::parse(sth.out)["query"] == "suggest_thresholds");
}

TEST_CASE("batch: the linked Z3 is checked before the batch is read (11's check at startup)", "[batch][cli]") {
  CHECK_NOTHROW(solve::verify_z3());   // this build: the system Z3 4.8.12 that solve was compiled against
  auto parsed = cli::parse(std::vector<std::string>{"fusion", "-b", "/nowhere/never-read.batch"});
  REQUIRE(std::holds_alternative<cli::Options>(parsed));
  auto o = std::get<cli::Options>(parsed);
  o.startup = [] {
    throw core::InstallError("install.z3_mismatch", "fusion: linked Z3 is 4.8.11.0 but this build needs 4.8.12.0");
  };
  std::istringstream in;
  std::ostringstream out, err;
  CHECK(cli::run(o, cli::Streams{in, out, err}) == cli::exit_failed);   // not 2: the batch was never read
  CHECK(err.str().find("[install.z3_mismatch]") != std::string::npos);
  o.batch.reset();
  o.help = "";                                                        // -h needs no Z3
  CHECK(cli::run(o, cli::Streams{in, out, err}) == cli::exit_ok);
}
