// Running a point (01's "Running a point", S1.28, 01.C15, D1). FuSION 1's loop dispatched each line as it
// came, with no try around dispatch, so a failing line printed a message and the batch went on
// (Linux/src/main.cc:1148-7945, S1.28); 2.0 runs the checked plan, catches a failure once per line,
// stops the batch at the first one, and skips every later line after a reported result.
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include "internal.hpp"

namespace fusion::batch {

// ------------------------------------------------------------------ graphs and remembered files

int GraphStore::add(GraphEntry entry) {
  entries_.push_back(std::move(entry));
  return static_cast<int>(entries_.size());
}

const GraphEntry& GraphStore::get(int id) const {
  if (id == 0) id = latest();   // S2.01: 0 names the latest graph
  if (id <= 0 || static_cast<std::size_t>(id) > entries_.size())
    throw core::Failure("batch.bad_argument", "graph " + std::to_string(id) + " does not exist");
  return entries_[static_cast<std::size_t>(id - 1)];
}

int GraphStore::latest() const { return static_cast<int>(entries_.size()); }

std::optional<FileIdentity> file_identity(const std::filesystem::path& path) {
#ifdef _WIN32
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return std::nullopt;
  return FileIdentity{0, core::stable_hash(core::to_utf8(std::filesystem::weakly_canonical(path, ec)))};
#else
  struct stat st{};
  if (::stat(path.c_str(), &st) != 0) return std::nullopt;
  return FileIdentity{static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)};
#endif
}

// 01.C13: a value is handed back only for the same file (identity, not path text) with the same bytes.
std::optional<Remembered> WrittenFiles::get(const std::filesystem::path& path) const {
  const auto id = file_identity(path);
  if (!id) return std::nullopt;
  for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
    if (it->id != *id) continue;
    if (core::Sha256::of_file(path) != it->sha256) return std::nullopt;   // changed since: read from disk
    return it->value;
  }
  return std::nullopt;
}

void WrittenFiles::put(const std::filesystem::path& path, Remembered value) {
  const auto id = file_identity(path);
  if (!id) return;
  entries_.push_back({*id, core::Sha256::of_file(path), std::move(value)});
}

Handler Handlers::find(std::string_view command) const {
  for (const auto& [name, handler] : commands)
    if (name == command) return handler;
  return nullptr;
}

// ------------------------------------------------------------------ the interpreter

Interpreter::Interpreter(const Plan& plan, int point, run::Run& run, const Handlers& handlers)
    : plan_(plan), point_(point), run_(run), handlers_(handlers) {
  workspace_.batch_dir = plan.program.source.path.parent_path();
  workspace_.facts = plan.facts.at(static_cast<std::size_t>(point));
  workspace_.debug = run.log().debug_on();
}

namespace {

constexpr std::string_view kDashes = "-------------------------------";   // S1.10: 31 dashes

std::string reason_of(const std::exception& e) { return e.what(); }

}  // namespace

run::RunStatus Interpreter::run() {
  const auto& lines = plan_.lines.at(static_cast<std::size_t>(point_));
  const auto& steps = plan_.steps.at(static_cast<std::size_t>(point_));
  core::Log& log = run_.log();
  std::optional<std::string> skipping;   // after a reported result or a stop (01.C15)
  bool stopped = false, failed = false;
  for (const PlannedLine& planned : lines) {
    if (planned.kind == PlannedLine::Kind::skip) continue;
    if (planned.kind == PlannedLine::Kind::exit) {   // S1.13: exit ends the run
      if (!skipping) {
        log.info(planned.echo);
        log.info(kDashes);
      }
      break;
    }
    const run::StepSpec& spec = steps.at(planned.step);
    if (!skipping && run_.stop().stop_requested()) {
      stopped = true;
      skipping = "stopped";
    }
    if (skipping) {
      run_.skip(spec, *skipping);
      continue;
    }
    // S1.10: the expanded tokens and 31 dashes, flushed before the command runs.
    log.info(planned.echo);
    log.info(kDashes);
    std::optional<run::Run::Step> step;
    try {
      step.emplace(run_.begin(spec));                 // inputs the batch made are checked now (01.C12)
    } catch (const core::Failure&) {
      failed = true;                                  // begin() has failed the step and printed it
      break;
    } catch (const std::exception& e) {
      log.error("internal", e.what());
      failed = true;
      break;
    }
    core::Context& ctx = step->context();
    workspace_.details = core::Json::object();
    try {
      core::Outcome<void> outcome;
      if (planned.exec) {
        outcome = run_exec(planned, ctx);
      } else {
        const Handler handler = handlers_.find(spec.command);
        if (!handler) throw core::Failure("internal", spec.command + " has no handler in this build");
        outcome = handler(planned.args, workspace_, ctx);
      }
      for (const auto& [key, value] : workspace_.details.items()) step->detail(key, value);   // 09's details
      if (outcome.report) {                           // S4.18, S7.03, 04.C5: a reported result
        reports_.push_back(*outcome.report);
        run_.end(*step, run::StepStatus::reported, outcome.report, outcome.partial);
        skipping = outcome.report->code;
      } else {
        run_.end(*step, run::StepStatus::ok, std::nullopt, outcome.partial);
      }
    } catch (const core::Stopped&) {                  // A1: the step stopped at a safe point
      run_.end(*step, run::StepStatus::stopped);
      stopped = true;
      skipping = "stopped";
    } catch (const core::Failure& f) {                // S1.28: the batch stops at the first failure
      run_.fail(*step, f);
      failed = true;
      break;
    } catch (const std::bad_alloc&) {
      run_.fail(*step, core::Failure("internal.out_of_memory", "out of memory"));
      failed = true;
      break;
    } catch (const std::filesystem::filesystem_error& e) {
      run_.fail(*step, core::Failure("io.write", reason_of(e)));
      failed = true;
      break;
    } catch (const std::exception& e) {
      run_.fail(*step, core::Failure("internal", reason_of(e)));
      failed = true;
      break;
    }
  }
  if (handlers_.finish) {   // the run-level files, whatever the status (entry 230)
    try {
      handlers_.finish(workspace_, run_);
    } catch (const std::exception& e) {
      log.warning("io.write", std::string("the run's last files could not be written: ") + e.what());
    }
  }
  if (failed) return run::RunStatus::failed;
  return stopped ? run::RunStatus::stopped : run::RunStatus::ok;
}

// exec (S1.14): FuSION 1's six moves are written natively (01.C11), window plots skipped without a
// display (01.C20), and every other line runs in the POSIX shell (01.C10) in the batch's directory (01.C2).
core::Outcome<void> Interpreter::run_exec(const PlannedLine& planned, core::Context& ctx) {
  if (const auto move = match_legacy_move(planned.exec_text)) {
    if (!handlers_.legacy_move)
      throw core::Failure("internal", "this build cannot write FuSION 1's " +
                                          std::string(legacy_file_name(move->name)));
    LegacyMove resolved = *move;
    if (resolved.dest.is_relative()) resolved.dest = workspace_.batch_dir / resolved.dest;
    handlers_.legacy_move(resolved, workspace_, ctx);
    return {};
  }
  if (is_window_plot(planned.exec_text) && !display_available()) {
    ctx.log.info("exec: no display is available, so this gnuplot window is skipped; plot_pareto draws the plot "
                 "(01.C20)");
    return {};
  }
  const auto shell = Shell::find(workspace_.home);
  if (!shell) throw core::Failure("deps.missing", "exec: no POSIX shell (/bin/sh) is available (01.C10)");
  const ShellResult result = shell->run(planned.exec_text, workspace_.batch_dir, ctx.log, ctx.stop);
  if (result.stopped) throw core::Stopped();
  if (result.status != 0) {
    std::string text = planned.exec_text;
    if (text.size() > 160) text = text.substr(0, 157) + "...";
    throw core::Failure("exec.status", "shell exited with status " + std::to_string(result.status) + ": " + text);
  }
  return {};
}

// ------------------------------------------------------------------ the run

// 01.C7: the label and root come from the command line, then the run line, then the batch file.
run::Options run_options(const Plan& plan, const RunOptions& options) {
  run::Options out;
  const auto& batch = plan.program.source.path;
  const auto batch_dir = batch.parent_path();
  if (options.root) out.root = std::filesystem::absolute(*options.root);
  else if (plan.run_line.root && !plan.run_line.root->empty()) out.root = resolve_input(batch_dir, *plan.run_line.root);
  else out.root = batch_dir;
  if (options.label) out.label = *options.label;
  else if (plan.run_line.label && !plan.run_line.label->empty()) out.label = *plan.run_line.label;
  else out.label = core::to_utf8(batch.stem());
  if (options.log) out.log_name = *options.log;
  else if (plan.run_line.log) out.log_name = *plan.run_line.log;
  out.adopt = options.run_dir;
  out.parent = options.parent;
  out.command_line = options.command_line;
  out.labels = options.tags;
  if (!plan.variables.empty())
    for (const auto& [key, value, line] : plan.variables.front()) out.variables.emplace_back(key, value);
  out.debug = options.debug;
  out.batch = batch;
  out.settings = plan.run_settings;
  for (const auto& s : out.settings) {
    if (!s.spec || !s.value) continue;
    if (s.spec->module == "run" && s.spec->name == "cache") out.cache = std::get<bool>(*s.value);
    if (s.spec->module == "run" && s.spec->name == "cache_dir")
      out.cache_dir = core::path_from_utf8(std::get<std::string>(*s.value));
  }
  out.sinks = options.sinks;
  out.on_grace_expired = options.on_grace_expired;
  return out;
}

int run_batch(const Plan& checked, const RunOptions& options, core::Log& log, const Handlers& handlers) {
  if (!checked.report.ok()) return 1;   // the caller printed the check's errors; nothing is created
  std::optional<run::Run> run;
  try {
    run.emplace(run::Run::start(run_options(checked, options)));
  } catch (const core::Failure& f) {
    log.error(f.code(), f.what());
    return 1;
  }
  // $FUSION_RUN_DIR exists only now (01.C18): a batch that names it is planned again with the real path.
  const Plan* plan = &checked;
  std::optional<Plan> replanned;
  const bool names_run_dir = std::any_of(checked.program.source.lines.begin(), checked.program.source.lines.end(),
                                         [](const std::string& l) { return l.find("$FUSION_RUN_DIR") != std::string::npos; });
  if (names_run_dir) {
    CheckOptions again;
    again.inputs = again.tools = false;
    again.run_dir = core::to_utf8(run->dir());
    again.home = options.home;
    again.settings = options.settings;
    replanned.emplace(check(checked.program, again));
    plan = &*replanned;
  }
  if (handlers.start) {   // section 11's blocks in run.json
    try {
      handlers.start(*run, *plan);
    } catch (const std::exception& e) {
      run->log().warning("run.versions", std::string("run.json's versions could not be recorded: ") + e.what());
    }
  }
  // The plan's warnings and notes, now in the run log too (decision 0021: printed, and the run goes on).
  const std::string name = core::to_utf8(checked.program.source.path.filename());
  for (const auto& d : plan->report.diagnostics) {
    const std::string text = name + ":" + std::to_string(d.line) + ": " + d.message;
    if (d.severity == core::Severity::warning) run->log().warning(d.code, text);
    else if (d.severity == core::Severity::note) run->log().info("note: " + text + " [" + d.code + "]");
  }
  for (const auto& f : plan->report.features)
    run->log().info(name + ":" + std::to_string(f.line) + ": " + f.command + ": " + f.feature + " is " +
                    (f.on ? "on" : "off") + ": " + f.because);
  try {
    const auto report = run->check(plan->steps.front());   // 09.C3 and decision 0021, on the copies
    if (!report.ok()) return run->finish(run::RunStatus::failed);
  } catch (const core::Failure& f) {
    run->log().error(f.code(), f.what());
    return run->finish(run::RunStatus::failed);
  }
  Interpreter interpreter(*plan, 0, *run, handlers);
  interpreter.set_home(options.home);
  const auto status = interpreter.run();
  return run->finish(status);
}

}  // namespace fusion::batch
