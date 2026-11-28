// The fusion executable's paths (01.C14): help, --commands, --version, --stop, --check and a batch run.
// It lives with the handlers because a run calls the step modules.
#include <fstream>
#include <iostream>
#include <sstream>

#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"
#include "fusion/solve/solve.hpp"

#include "fusion/batch/cli.hpp"
#include "fusion/core/build_info.hpp"
#include "fusion/core/install.hpp"
#include "state.hpp"

namespace fusion::cli {

namespace {

// 09's console conventions: information on stdout; warnings and errors on stderr, with their code.
class ConsoleSink final : public core::LogSink {
 public:
  ConsoleSink(std::ostream& out, std::ostream& err) : out_(out), err_(err) {}
  void write(core::Level level, std::string_view code, std::string_view text) override {
    switch (level) {
      case core::Level::debug: return;   // the run log only (09.C11)
      case core::Level::info: out_ << text << '\n'; out_.flush(); return;   // S1.10: flushed
      case core::Level::warning: err_ << "warning: " << text << " [" << code << "]\n"; err_.flush(); return;
      case core::Level::error: err_ << "error: " << text << " [" << code << "]\n"; err_.flush(); return;
    }
  }
 private:
  std::ostream& out_;
  std::ostream& err_;
};

std::string home_text() {
  try {
    return core::to_utf8(core::install_dir().home);
  } catch (const std::exception&) {
    return {};
  }
}

// Section 11's --version: the versions as run.json records them, the home, the data files with the
// reference maps' source and date, and the probe of every tool the installation provides. It exits 1
// when the home fails the startup check, or a file or tool that fusion-data.json lists is missing or
// damaged; the check's failure is reported, as install.error, instead of stopping the report.
int version(const Options& o, Streams s) {
  const auto& info = core::build_info();
  std::optional<core::InstallError> home_problem;
  try {
    core::check_home();
  } catch (const core::InstallError& e) {
    home_problem = e;
  }
  const batch::DataCheck data = batch::data_block();
  const auto probes = batch::installation_probes();
  if (o.json) {
    core::Json json{{"fusion", std::string(info.version)}, {"build_id", std::string(core::build_id())},
                    {"versions", batch::versions_block()}, {"tools", batch::probes_block(probes)},
                    {"platform", batch::platform_block()}, {"install", batch::install_block()},
                    {"environment", batch::environment_block()}, {"data", data.json}};
    if (home_problem)
      json["install"]["error"] = core::Json{{"code", home_problem->code()}, {"message", home_problem->what()}};
    s.out << json.dump(2) << "\n";
  } else {
    s.out << "FuSION " << info.version << " (build " << core::build_id() << ")\n";
    const auto install = batch::install_block();
    if (install["home"].is_string()) s.out << "  home      " << install["home"].get<std::string>() << "\n";
    for (const auto& p : probes)
      s.out << "  " << p.id << "  " << (p.ok ? p.version + " " + p.origin : std::string("not found")) << "\n";
    s.out << "  data      " << (data.json["manifest"].is_null() ? "no fusion-data.json" : data.damaged ? "damaged" : "ok")
          << "\n";
  }
  if (home_problem) s.err << "error: " << home_problem->what() << " [" << home_problem->code() << "]\n";
  return data.damaged || home_problem ? exit_failed : exit_ok;
}

// How an ended run ended, from its run.json, for --stop's refusal.
std::string ended_how(const std::filesystem::path& dir) {
  std::string status;
  try {
    std::ifstream in(dir / "run.json");
    const core::Json manifest = core::Json::parse(in);
    if (manifest.contains("status") && manifest["status"].is_string()) status = manifest["status"].get<std::string>();
  } catch (const std::exception&) {
  }
  if (status == "running") return "it died without finishing, and nothing holds its lock";
  if (!status.empty()) return "run.json says it ended with status " + status;
  return "nothing holds its lock";
}

// 09.C7: the stop file; only its existence matters, written under a temporary name and renamed.
// 01.C14's table writes it only into a running run's directory, which 09.C9's lock tells: a folder
// that is not a run, or a run that has ended, gets no file (Phase 3, prompt log entry 387).
int stop(const Options& o, Streams s) {
  const auto dir = *o.stop;
  if (!std::filesystem::is_directory(dir)) {
    s.err << "error: " << core::to_utf8(dir) << " is not a run directory\n";
    return exit_failed;
  }
  if (!std::filesystem::exists(dir / "run.lock")) {
    s.err << "error: " << core::to_utf8(dir) << " is not a run directory: it holds no run.lock\n";
    return exit_failed;
  }
  if (!run::Lock::held(dir)) {
    if (run::Lock::supported(dir)) {
      s.err << "error: the run in " << core::to_utf8(dir) << " is not running: " << ended_how(dir)
            << " [run.not_running]\n";
      return exit_failed;
    }
    s.err << "note: the file system of " << core::to_utf8(dir)
          << " cannot lock, so whether its run is live cannot be checked; the stop file is written anyway (09.C9)\n";
  }
  const auto tmp = dir / "stop.tmp";
  {
    std::ofstream f(tmp, std::ios::binary);
    f << "requested by fusion --stop\n";
  }
  std::error_code ec;
  std::filesystem::rename(tmp, dir / "stop", ec);
  if (ec) {
    s.err << "error: cannot write " << core::to_utf8(dir / "stop") << ": " << ec.message() << "\n";
    return exit_failed;
  }
  return exit_ok;
}

int check_or_run(const Options& o, Streams s) {
  batch::SourceFile source;
  try {
    source = batch::read_source(*o.batch);
  } catch (const core::UsageError& e) {   // S1.01: missing, unreadable or empty: usage, status 2
    s.err << "error: " << e.what() << "\n";
    return exit_usage;
  }
  core::CheckReport parse_report;
  batch::CheckOptions options;
  options.home = home_text();
  options.settings = o.settings;
  options.run_dir = o.run_dir ? core::to_utf8(std::filesystem::absolute(*o.run_dir)) : std::string();
  const std::string name = core::to_utf8(source.path.filename());
  batch::Plan plan = batch::check(batch::parse(std::move(source), parse_report), options);
  if (o.check) {   // 09.C12: every check a real run makes; no directory, no copy
    if (o.json) {
      core::Json json = plan.report.to_json();
      s.out << json.dump(2) << "\n";
    } else {
      batch::print_diagnostics(s.err, plan.report, name);
      for (const auto& f : plan.report.features)
        s.out << name << ":" << f.line << ": " << f.command << ": " << f.feature << " is " << (f.on ? "on" : "off")
              << ": " << f.because << "\n";
      if (plan.report.ok()) s.out << name << ": the check found no error\n";
    }
    return plan.report.ok() ? exit_ok : exit_failed;
  }
  if (!plan.report.ok()) {   // 09.C14: a check error ends fusion with status 1 before a step runs
    batch::print_diagnostics(s.err, plan.report, name);
    return exit_failed;
  }
  batch::RunOptions run;
  run.root = o.root;
  run.run_dir = o.run_dir;
  run.parent = o.resume;
  run.label = o.label;
  run.log = o.log;
  for (const auto& [k, v] : o.tags) run.tags[k] = v;
  run.settings = o.settings;
  run.debug = o.debug;
  run.points = o.points;
  run.jobs = o.jobs.value_or(0);
  run.command_line = o.command_line;
  run.home = options.home;
  run.sinks.push_back(std::make_shared<ConsoleSink>(s.out, s.err));
  run.on_grace_expired = [](int status) { std::_Exit(status); };   // 09.C7's grace hook (B1)
  core::Log startup(o.debug);
  startup.add_sink(run.sinks.front());
  return batch::run_batch(plan, run, startup);
}

}  // namespace

int run(const Options& o, Streams s) {
  try {
    if (o.help) {   // S1.02: -h prints on stdout and exits 0
      if (o.help->empty()) {
        print_usage(s.out, true);
      } else if (const auto* spec = batch::command_table().find(*o.help)) {
        batch::command_table().print_command(s.out, *spec);
      } else {
        s.err << "error: unknown command '" << *o.help << "'\n\n";
        print_usage(s.err, true);
        return exit_usage;
      }
      return exit_ok;
    }
    if (o.commands) {   // 01.C9: the frontend's block library
      s.out << batch::command_table().describe().dump(2) << "\n";
      return exit_ok;
    }
    // Section 11's checks at startup, before the batch is read, each an install error with exit status 1:
    // for the commands that read a batch, the home holds this version's fusion-data.json (S1.20, steps 1
    // to 3; install.home_missing, install.version_mismatch), which --version reports instead; and the
    // linked Z3 is the one solve was compiled against ("Check at startup"; install.z3_mismatch).
    if (o.batch || o.query) core::check_home();
    if (o.version || o.batch) {
      if (o.startup) o.startup();
      else solve::verify_z3();
    }
    if (o.version) return version(o, s);
    if (o.stop) return stop(o, s);
    if (o.query) {
      std::string line;
      std::getline(s.in, line);
      core::Log log(false);
      log.add_sink(std::make_shared<ConsoleSink>(s.err, s.err));
      return batch::run_query(line, s.out, log);
    }
    if (o.resume && !o.batch) {
      s.err << "error: --resume comes in a later build of FuSION 2.0 (01.C6) [run.resume_unavailable]\n";
      return exit_failed;
    }
    return check_or_run(o, s);
  } catch (const core::UsageError& e) {
    s.err << "error: " << e.what() << "\n";
    return exit_usage;
  } catch (const core::Failure& e) {
    s.err << "error: " << e.what() << " [" << e.code() << "]\n";
    return exit_failed;
  } catch (const std::exception& e) {
    s.err << "error: " << e.what() << " [internal]\n";
    return exit_failed;
  }
}

int run(const Options& o) { return run(o, Streams{std::cin, std::cout, std::cerr}); }

int main_utf8(std::span<const std::string> args, Streams s) {
  auto parsed = parse(args);
  if (auto* message = std::get_if<std::string>(&parsed)) {   // S1.02: usage on stderr, status 2
    s.err << "error: " << *message << "\n\n";
    print_usage(s.err, false);
    return exit_usage;
  }
  return run(std::get<Options>(parsed), s);
}

}  // namespace fusion::cli

namespace fusion::batch {

namespace {

// A query makes no run directory and writes nothing (04.C8): its files are the originals, and an output
// is refused; its events go nowhere.
class QueryFiles final : public core::Files {
 public:
  std::filesystem::path input(const std::filesystem::path& as_named) override { return as_named; }
  core::OutputFile create(std::string_view name) override { refuse(name); }
  core::OutputFile create_debug(std::string_view name) override { refuse(name); }
  core::Journal& journal(std::string_view name) override { refuse(name); }
  std::span<const core::Json> resumed(std::string_view) const override { return {}; }
  std::filesystem::path step_dir() const override { return std::filesystem::current_path(); }
 private:
  [[noreturn]] static void refuse(std::string_view name) {
    throw core::Failure("query.no_files", "a query writes no file, so " + std::string(name) + " is not written (04.C8)");
  }
};
class NoEvents final : public core::Events {
 public:
  void emit(std::string_view, core::Json) override {}
  void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
};

core::Json failure(std::string_view query, std::string_view code, std::string_view message) {
  return core::Json{{"ok", false}, {"query", std::string(query)},
                    {"error", {{"code", std::string(code)}, {"message", std::string(message)}}}};
}

expression::ValueInput query_value_input(const BoundArgs& args) {
  expression::ValueInput in;
  in.path = *path_arg(args, "values");
  if (given(args, "id_column")) in.id_column = text_arg(args, "id_column");
  if (given(args, "value_column")) in.value_column = text_arg(args, "value_column");
  if (given(args, "id_map")) in.id_map = text_arg(args, "id_map");
  if (text_or(args, "repeats", "classify_each") == "max_abs") in.repeats = expression::RepeatRule::max_abs;
  return in;
}

}  // namespace

// --query (04.C8, row 3A.8): the exact cudf, suggest_thresholds or labels line the batch will hold, in;
// the owner's answer, out; no run directory, no file, no event, and log lines on stderr (entry 168).
int run_query(std::string_view text, std::ostream& json_out, core::Log& log) {
  const Line line = lex(1, text);
  if (line.kind != LineKind::command) {
    json_out << failure("", "batch.bad_argument", "the query holds no command").dump() << "\n";
    return 1;
  }
  const std::string name = line.tokens.front().text;
  const CommandSpec* spec = command_table().find(name);
  if (!spec || spec->effects.needs_run_dir) {
    const std::string message = name + " cannot run through --query: it needs a run directory (entry 168)";
    log.error("batch.bad_argument", message);
    json_out << failure(name, "batch.bad_argument", message).dump() << "\n";
    return 1;
  }
  core::CheckReport report;
  const Variables vars;
  const BoundArgs args = bind(*spec, line, vars, report, std::filesystem::current_path());
  for (const auto& d : report.diagnostics)
    if (d.severity == core::Severity::error) {
      json_out << failure(spec->name, d.code, d.message).dump() << "\n";
      return 1;
    }
  const core::Settings settings;
  QueryFiles files;
  NoEvents events;
  core::Context ctx{log, events, core::StopToken{}, settings, files, nullptr, "query"};
  try {
    core::Json answer;
    if (spec->name == "labels") {   // 2B.6: the shipped symbol map unless map= names another
      const auto named = path_arg(args, "map");
      const auto map = named ? *named : core::home_file("data/hsa_to_gene_symbol_map.txt");
      const network::LabelMap symbols = network::LabelMap::read(map, ctx);
      network::SourceRef ref;
      ref.path = map;
      ref.size = std::filesystem::file_size(map);
      ref.sha256 = core::Sha256::of_file(map);
      answer = network::labels_answer(symbols, ref);
    } else {
      core::Interner interner;
      const network::Maps maps;
      const expression::ValueInput in = query_value_input(args);
      const expression::GeneValues values = expression::read_values(in.path, in, maps, interner, ctx);
      if (spec->name == "cudf") {
        std::vector<core::GeneId> not_de;
        const auto not_de_file = path_arg(args, "not_de");
        if (not_de_file) not_de = expression::read_id_list(*not_de_file, maps.ids, interner);
        expression::ClassifyRequest request;
        request.thresholds = expression::Thresholds{*real_arg(args, "up"), *real_arg(args, "down")};
        if (request.thresholds.down >= request.thresholds.up)   // S3.07, as the check says it
          throw core::Failure("batch.bad_value", "cudf: down must be below up (S3.07)");
        request.not_de = not_de;
        request.interner = &interner;
        request.not_de_file = not_de_file;
        answer = expression::cudf_answer(values, expression::classify(values, request, ctx).value, interner);
      } else {
        std::optional<expression::Thresholds> current;
        if (const auto up = real_arg(args, "current_up"), down = real_arg(args, "current_down"); up && down)
          current = expression::Thresholds{*up, *down};
        answer = expression::suggestion_answer(expression::suggest_thresholds(values, current), values);
      }
    }
    json_out << answer.dump() << "\n";
    return 0;
  } catch (const core::Failure& f) {
    json_out << failure(spec->name, f.code(), f.what()).dump() << "\n";
    return 1;
  } catch (const std::exception& e) {
    json_out << failure(spec->name, "internal", e.what()).dump() << "\n";
    return 1;
  }
}

}  // namespace fusion::batch
