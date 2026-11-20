// The planner and the check of the whole batch before anything runs (decision 0021, 01.C12, 01.C16, 01.C17).
// FuSION 1 had no check: each line was dispatched as it was reached (Linux/src/main.cc:1148-7945), so a
// typo on line 260 surfaced after hours of solving. 2.0 lexes, expands and binds every line, simulates the
// graph ids and the values each line makes, runs each command's checker and checks every input first.
#include <algorithm>
#include <fstream>
#include <ostream>
#include <sstream>

#include "internal.hpp"

namespace fusion::batch {

// ------------------------------------------------------------------ CheckContext

void CheckContext::error(std::string_view code, std::string message, std::string_view setting,
                            std::string_view path) {
  add_diagnostic(*state_.report, core::Severity::error, args_.line, 0, code,
                 std::string(args_.command->name) + ": " + message, args_.command->name, setting, path);
  state_.report->diagnostics.back().point = state_.point;
}
void CheckContext::warning(std::string_view code, std::string message, std::string_view setting,
                            std::string_view path) {
  add_diagnostic(*state_.report, core::Severity::warning, args_.line, 0, code,
                 std::string(args_.command->name) + ": " + message, args_.command->name, setting, path);
  state_.report->diagnostics.back().point = state_.point;
}
void CheckContext::note(std::string_view code, std::string message, std::string_view setting,
                            std::string_view path) {
  add_diagnostic(*state_.report, core::Severity::note, args_.line, 0, code,
                 std::string(args_.command->name) + ": " + message, args_.command->name, setting, path);
  state_.report->diagnostics.back().point = state_.point;
}

const BatchFacts& CheckContext::facts() const { return state_.facts; }
core::CheckReport& CheckContext::report() { return *state_.report; }

void CheckContext::require(core::deps::Requirement need) { state_.tools.push_back(std::move(need)); }

void CheckContext::feature(std::string_view name, bool on, std::string_view because) {
  core::Feature f;
  f.line = static_cast<std::uint32_t>(args_.line);
  f.command = std::string(args_.command->name);
  f.feature = std::string(name);
  f.on = on;
  f.because = std::string(because);
  state_.report->features.push_back(std::move(f));
}

// 01.C12: an input is produced in the batch when an earlier line declares it as an output, or when its
// text occurs in an earlier exec line; it is then checked just before its step instead of before the run.
void CheckContext::input(const BoundValue& value) {
  if (!value.given || !value.has_value || !value.spec) return;
  const auto* s = find_setting(value.spec->setting);
  std::vector<std::string> paths;
  if (const auto* one = std::get_if<std::string>(&value.value)) paths.push_back(*one);
  else if (const auto* many = std::get_if<std::vector<std::string>>(&value.value)) paths = *many;
  for (const auto& path : paths) {
    run::InputSpec in;
    in.argument = std::string(value.spec->name);
    in.path = core::path_from_utf8(path);
    in.required = s && s->need == core::Need::required;
    in.is_list = s && s->type == core::Type::input_list;
    for (auto it = state_.outputs.rbegin(); it != state_.outputs.rend() && !in.made_by_line; ++it)
      if (it->path == path) in.made_by_line = static_cast<std::uint32_t>(it->line);
    for (auto it = state_.exec_texts.rbegin(); it != state_.exec_texts.rend() && !in.made_by_line; ++it)
      if (it->first.find(path) != std::string::npos ||
          (!value.text.empty() && it->first.find(value.text) != std::string::npos))
        in.made_by_line = static_cast<std::uint32_t>(it->second);
    if (!in.made_by_line) state_.inputs_seen.emplace_back(path, args_.line);
    state_.inputs.push_back(std::move(in));
  }
}

// Outputs: an absolute path is honoured as well as kept in the step directory (rows 2B.3, 4A.11); a
// relative one lands in the step directory. Later lines may read either.
void CheckContext::output(const std::filesystem::path& path) {
  const std::string p = generic(path);
  const int column = 0;
  if (path.is_absolute()) {
    for (const auto& o : state_.outputs)
      if (o.path == p && o.line != args_.line)
        error("output.clash", "writes " + p + ", which line " + std::to_string(o.line) + " also writes", {}, p);
    for (const auto& [in, line] : state_.inputs_seen)
      if (in == p) error("output.clash", "writes " + p + ", which line " + std::to_string(line) + " reads", {}, p);
    for (const auto& [prev, col] : state_.line_outputs)
      if (prev == p) error("output.clash", "writes " + p + " twice", {}, p);
    state_.outputs.push_back({p, args_.line, state_.step_id});
  }
  state_.line_outputs.emplace_back(p, column);
}

std::string step_id(CheckContext::State& state, std::string_view command) {
  const int n = ++state.step_counts[std::string(command)];
  return n == 1 ? std::string(command) : std::string(command) + "-" + std::to_string(n);
}

// ------------------------------------------------------------------ parse

Program parse(SourceFile source, core::CheckReport& report) {
  Program program;
  report.batch = source.path;
  report.batch_sha256 = source.sha256;
  if (source.invalid_utf8_line)   // 01.C19: paths must convert to UTF-16 on Windows
    add_diagnostic(report, core::Severity::error, *source.invalid_utf8_line, 1, "batch.bad_value",
                   "line " + std::to_string(*source.invalid_utf8_line) + " is not valid UTF-8");
  program.lines.reserve(source.lines.size());
  for (std::size_t i = 0; i < source.lines.size(); ++i)
    program.lines.push_back(lex(static_cast<int>(i) + 1, source.lines[i]));
  program.source = std::move(source);
  program.points.push_back(Point{});   // one unlabelled point: sweeps (01.C3) come in a later build
  report.points = 1;
  return program;
}

// ------------------------------------------------------------------ check

namespace {

using Severity = core::Severity;

struct Planner {
  Planner(const Program& p, const CheckOptions& o, Plan& pl) : program(p), options(o), plan(pl) {}
  const Program& program;
  const CheckOptions& options;
  Plan& plan;
  CheckContext::State state;
  Variables vars;
  std::size_t point_index = 0;
  bool seen_step = false, after_exit = false, warned_after_exit = false, seen_run = false;

  void diag(Severity severity, int line, int column, std::string_view code, std::string message,
            std::string_view command = {}, std::string_view setting = {}, std::string_view path = {}) {
    add_diagnostic(plan.report, severity, line, column, code, std::move(message), command, setting, path);
    plan.report.diagnostics.back().point = state.point;
  }

  std::string expand(const Line& line, const Token& token, std::string_view text) {
    auto e = vars.expand(text, Variables::Mode::argument);
    for (const auto& name : e.unmatched)
      diag(Severity::warning, line.number, token.column, "batch.undefined_variable",
           name + " matches no defined variable");
    return std::move(e.text);
  }

  void define_predefined() {
    // 01.C18: defined before line 1, with '/' on every platform.
    const auto batch_dir = core::to_utf8(program.source.path.parent_path());
    vars.define("$FUSION_RUN_DIR", options.run_dir.empty() ? "<run dir>" : options.run_dir, 0);
    vars.define("$FUSION_BATCH_DIR", batch_dir, 0);
    vars.define("$FUSION_POINT", program.points[point_index].label, 0);
    vars.define("$FUSION_HOME", options.home, 0);
  }

  void let_line(const Line& line) {
    // S1.08: the key is literal; FuSION 1 expanded it (Linux/src/main.cc:1249-1282), so a redefinition
    // reported "1doesn't start with $" and $PLOT_FILE_WO_GENE was never defined.
    if (line.tokens.size() < 2) {
      diag(Severity::error, line.number, line.tokens[0].column, "batch.bad_argument", "let: no $name to define",
           "let");
      return;
    }
    const Token& key = line.tokens[1];
    if (!valid_variable_key(key.text) || key.quoted) {
      diag(Severity::error, line.number, key.column, "batch.bad_value",
           "let: '" + key.text + "' is not a $name ($ then a letter or _, then letters, digits and _)", "let");
      return;
    }
    if (key.text.starts_with(reserved_prefix)) {
      diag(Severity::error, line.number, key.column, "batch.bad_value",
           "let: " + key.text + " is reserved: $FUSION_ names are FuSION's own (01.C18)", "let");
      return;
    }
    if (line.tokens.size() > 3) {
      diag(Severity::error, line.number, line.tokens[3].column, "batch.bad_argument",
           "let: extra tokens after the value of " + key.text + " (S1.08); quote a value with spaces", "let");
      return;
    }
    std::string value = line.tokens.size() == 3 ? expand(line, line.tokens[2], line.tokens[2].text) : std::string();
    if (value == "#LASTGRAPHNUM") {   // S1.09: the latest graph's id, known before the run (one id a line)
      if (state.graphs.empty()) {
        diag(Severity::error, line.number, line.tokens[2].column, "batch.bad_value",
             "let: #LASTGRAPHNUM: no graph has been made yet, so " + key.text + " stays undefined", "let");
        return;
      }
      value = std::to_string(state.graphs.size());
    }
    if (const auto at = vars.defined_at(key.text)) {
      diag(Severity::error, line.number, key.column, "batch.redefined",
           "let: " + key.text + " is already defined at line " + std::to_string(*at), "let");
      return;
    }
    vars.define(key.text, std::move(value), line.number);
  }

  void run_line(const Line& line) {
    // 01.C7 and row 2B.2: before the first step; bare run names and any run-level key.
    if (seen_step) {
      diag(Severity::error, line.number, line.tokens[0].column, "batch.bad_argument",
           "run: the run line must come before the first step line", "run");
      return;
    }
    if (seen_run) {
      diag(Severity::error, line.number, line.tokens[0].column, "batch.bad_argument", "run: a second run line", "run");
      return;
    }
    seen_run = true;
    plan.run_line.line = line.number;
    for (std::size_t t = 1; t < line.tokens.size(); ++t) {
      const Token& token = line.tokens[t];
      if (!token.key_end) {
        diag(Severity::error, line.number, token.column, "batch.bad_argument",
             "run: '" + token.text + "' is not name=value; it takes root=, label=, log=, cache=, cache_dir=, jobs= "
             "and run-level settings by key", "run");
        continue;
      }
      const std::string name = token.text.substr(0, *token.key_end);
      const std::string value = expand(line, token, std::string_view(token.text).substr(*token.key_end + 1));
      if (name == "debug" || name == "run.debug") {   // 01.C8, entry 229
        diag(Severity::error, line.number, token.column, "batch.bad_argument",
             "run: debug is set only on the command line, with --debug (01.C8)", "run", name);
        continue;
      }
      const std::string key = name.find('.') == std::string::npos ? "run." + name : name;
      const auto keys = run_level_keys();
      const bool allowed = std::find(keys.begin(), keys.end(), key) != keys.end();
      const auto* s = find_setting(key);
      if (!allowed || !s) {
        diag(Severity::error, line.number, token.column, "batch.bad_argument",
             "run: unknown setting '" + name + "'; it takes " + join_views(keys, ", "), "run", name);
        continue;
      }
      if (!value.empty()) {
        std::string why;
        if (s->type != core::Type::text && !parse_value(*s, value, core::Source::named, why)) {
          diag(Severity::error, line.number, token.column, "batch.bad_value", "run: " + name + " '" + value + "' " + why,
               "run", name);
          continue;
        }
      }
      if (key == "run.root") plan.run_line.root = value;
      else if (key == "run.label") plan.run_line.label = value;
      else if (key == "run.log") plan.run_line.log = value;
      plan.run_line.settings.emplace_back(key, value);
    }
  }

  // Values not given on the line come from --set, then the run line, then the default (entry 229).
  std::optional<std::pair<std::string, core::Source>> run_level(std::string_view key) const {
    for (auto it = options.settings.rbegin(); it != options.settings.rend(); ++it)
      if (it->first == key) return std::make_pair(it->second, core::Source::set_option);
    for (auto it = plan.run_line.settings.rbegin(); it != plan.run_line.settings.rend(); ++it)
      if (it->first == key && !it->second.empty()) return std::make_pair(it->second, core::Source::run_line);
    return std::nullopt;
  }

  void apply_run_level(BoundArgs& args) {
    for (auto& v : args.values) {
      if (v.given || !v.spec) continue;
      const auto override_value = run_level(v.spec->setting);
      if (!override_value) continue;
      const auto* s = find_setting(v.spec->setting);
      std::string why;
      if (v.spec->list) {
        std::vector<std::string> items;
        std::string_view text = override_value->first;
        while (!text.empty()) {
          const auto comma = text.find(',');
          items.emplace_back(text.substr(0, comma));
          if (comma == std::string_view::npos) break;
          text.remove_prefix(comma + 1);
        }
        v.value = std::move(items);
      } else if (auto parsed = parse_value(*s, override_value->first, core::Source::named, why)) {
        v.value = std::move(*parsed);
      } else {
        continue;
      }
      v.has_value = true;
      v.text = override_value->first;
      v.source = override_value->second;
    }
  }

  // 01.C16: an argument that takes a value defaults to the latest one of its kind; 0 is the latest graph.
  void resolve_takes(BoundArgs& args, CheckContext& ctx) {
    for (auto& v : args.values) {
      if (v.spec && !v.spec->takes && v.spec->name == "graph" && v.has_value) {   // sz, wgx: any graph
        std::int64_t id = std::get<std::int64_t>(v.value);
        if (id == 0) id = static_cast<std::int64_t>(state.graphs.size());
        if (id == 0)
          ctx.error("batch.bad_argument", "needs a graph, and no earlier line makes one", v.spec->name);
        else if (id > static_cast<std::int64_t>(state.graphs.size()))
          ctx.error("batch.bad_argument", "graph " + std::to_string(id) + " does not exist yet; " +
                                              std::to_string(state.graphs.size()) + " made so far",
                    v.spec->name);
        else
          v.value = id;
        continue;
      }
      if (!v.spec || !v.spec->takes) continue;
      const ValueKind kind = *v.spec->takes;
      if (kind == ValueKind::network || kind == ValueKind::corridor) {
        std::int64_t id = 0;
        if (v.given && v.has_value) id = std::get<std::int64_t>(v.value);
        if (id == 0) {
          if (!v.given) {
            for (std::size_t i = state.graphs.size(); i-- > 0;)
              if (state.graphs[i] == kind) { id = static_cast<std::int64_t>(i) + 1; break; }
          } else {
            id = static_cast<std::int64_t>(state.graphs.size());
          }
          if (id == 0) {
            ctx.error("batch.bad_argument", "needs a " + std::string(kind_name(kind)) + ", and no earlier line makes one",
                      v.spec->name);
            continue;
          }
        }
        if (id > static_cast<std::int64_t>(state.graphs.size())) {
          ctx.error("batch.bad_argument", "graph " + std::to_string(id) + " does not exist yet; " +
                                              std::to_string(state.graphs.size()) + " made so far",
                    v.spec->name);
          continue;
        }
        const ValueKind actual = state.graphs[static_cast<std::size_t>(id - 1)];
        if (actual != kind) {
          ctx.error("batch.bad_argument", "graph " + std::to_string(id) + " is a " + std::string(kind_name(actual)) +
                                              ", not a " + std::string(kind_name(kind)),
                    v.spec->name);
          continue;
        }
        v.value = id;
        v.has_value = true;
        continue;
      }
      if (v.given) continue;   // a name or a path overrides the latest value
      const bool exists = kind == ValueKind::observations ? state.observations
                          : kind == ValueKind::shortlist  ? state.shortlist
                          : kind == ValueKind::results    ? !state.solves.empty()
                                                          : true;
      const auto* s = find_setting(v.spec->setting);
      if (!exists && s && s->need == core::Need::required)
        ctx.error("batch.bad_argument", "needs " + std::string(kind_name(kind)) + ", and no earlier line makes them",
                  v.spec->name);
    }
  }

  void check_rules(const BoundArgs& args, CheckContext& ctx) {
    auto number = [&](std::string_view name) -> std::optional<double> {
      for (const auto& v : args.values) {
        if (!v.spec || v.spec->name != name || !v.has_value) continue;
        if (const auto* d = std::get_if<double>(&v.value)) return *d;
        if (const auto* i = std::get_if<std::int64_t>(&v.value)) return static_cast<double>(*i);
      }
      return std::nullopt;
    };
    auto given = [&](std::string_view name) {
      for (const auto& v : args.values)
        if (v.spec && v.spec->name == name) return v.given;
      return false;
    };
    for (const auto& rule : args.command->rules) {
      switch (rule.kind) {
        case RuleKind::less:
        case RuleKind::less_or_equal: {
          const auto a = number(rule.a), b = number(rule.b);
          if (!a || !b) break;
          const bool ok = rule.kind == RuleKind::less ? *a < *b : *a <= *b;
          if (!ok)
            ctx.error("batch.bad_value",
                      std::string(rule.a) + " (" + core::format_number(*a) + ") must be " +
                          (rule.kind == RuleKind::less ? "below " : "at most ") + std::string(rule.b) + " (" +
                          core::format_number(*b) + ")",
                      rule.a);
          break;
        }
        case RuleKind::requires_:
          if (given(rule.a) && !given(rule.b))
            ctx.error("batch.bad_argument", std::string(rule.a) + " needs " + std::string(rule.b), rule.a);
          break;
        case RuleKind::excludes:
          if (given(rule.a) && given(rule.b))
            ctx.error("batch.bad_argument", std::string(rule.a) + " and " + std::string(rule.b) + " exclude each other",
                      rule.a);
          break;
      }
    }
  }

  std::unique_ptr<core::Settings> settings_of(const BoundArgs& args) {
    std::vector<core::Setting> all;
    for (const auto& v : args.values) {
      core::Setting s;
      s.spec = find_setting(v.spec->setting);
      if (v.has_value) s.value = v.value;
      s.source = v.source;
      all.push_back(std::move(s));
    }
    for (const auto key : args.command->run_settings) {
      core::Setting s;
      s.spec = find_setting(key);
      if (!s.spec) continue;
      std::string why;
      if (const auto r = run_level(key)) {
        if (auto parsed = parse_value(*s.spec, r->first, core::Source::named, why)) {
          s.value = std::move(*parsed);
          s.source = r->second;
        }
      } else if (s.spec->need == core::Need::defaulted) {
        if (auto parsed = parse_value(*s.spec, s.spec->default_text, core::Source::named, why)) s.value = *parsed;
      }
      all.push_back(std::move(s));
    }
    return std::make_unique<core::Settings>(std::move(all));
  }

  // Pass 1: let, run and the binding of every line, with the graph ids each line makes, so that the
  // facts of later lines exist when the checkers run (01.C17).
  struct Bound { const Line* line; PlannedLine planned; };
  std::vector<Bound> bound;

  void pass1() {
    define_predefined();
    for (const Line& line : program.lines) {
      Bound b{&line, {}};
      b.planned.line = line.number;
      if (line.kind != LineKind::command) { bound.push_back(std::move(b)); continue; }
      const Token& first = line.tokens.front();
      const std::string name = expand(line, first, first.text);
      if (after_exit) {
        if (!warned_after_exit)
          diag(Severity::warning, line.number, first.column, "batch.ignored_tokens",
               "the lines after exit are ignored (S1.13)");
        warned_after_exit = true;
        bound.push_back(std::move(b));
        continue;
      }
      if (name == "exec") {   // S1.14: the rest of the line exactly as written, after $ expansion
        b.planned.kind = PlannedLine::Kind::step;
        b.planned.exec = true;
        b.planned.exec_text = vars.expand(line.rest, Variables::Mode::exec).text;
        b.planned.echo = "exec " + b.planned.exec_text + " ";
        state.exec_texts.emplace_back(b.planned.exec_text, line.number);
        seen_step = true;
        bound.push_back(std::move(b));
        continue;
      }
      if (line.unterminated_quote) {
        diag(Severity::error, line.number, *line.unterminated_quote, "batch.bad_value",
             "unterminated quote, opened at column " + std::to_string(*line.unterminated_quote));
        bound.push_back(std::move(b));
        continue;
      }
      if (name == "let") { let_line(line); bound.push_back(std::move(b)); continue; }
      if (name == "run") { run_line(line); bound.push_back(std::move(b)); continue; }
      if (name == "exit") {
        if (line.tokens.size() > 1)
          diag(Severity::warning, line.number, line.tokens[1].column, "batch.ignored_tokens",
               "exit: extra tokens are ignored (S1.13)", "exit");
        b.planned.kind = PlannedLine::Kind::exit;
        b.planned.echo = "exit ";
        after_exit = true;
        bound.push_back(std::move(b));
        continue;
      }
      const CommandSpec* spec = command_table().find(name);
      if (!spec) {
        std::vector<std::string_view> names;
        for (const auto& c : command_table().all())
          if (c.dropped.empty()) names.push_back(c.name);
        diag(Severity::error, line.number, first.column, "batch.unknown_command",
             "unknown command '" + name + "'; the commands are " + join_views(names, ", "));
        bound.push_back(std::move(b));
        continue;
      }
      if (!spec->dropped.empty()) {
        diag(Severity::error, line.number, first.column, "batch.unknown_command", std::string(spec->dropped),
             spec->name);
        bound.push_back(std::move(b));
        continue;
      }
      if (spec->name == "sweep" || spec->name == "load") {
        diag(Severity::error, line.number, first.column, "batch.unknown_command",
             std::string(spec->name) + " is not available in this build yet (01.C3, 01.C6)", spec->name);
        bound.push_back(std::move(b));
        continue;
      }
      seen_step = true;
      const auto before = plan.report.diagnostics.size();
      b.planned.args = bind(*spec, line, vars, plan.report, program.source.path.parent_path());
      for (auto i = before; i < plan.report.diagnostics.size(); ++i) plan.report.diagnostics[i].point = state.point;
      b.planned.kind = PlannedLine::Kind::step;
      // S1.10: the expanded tokens, each followed by one space.
      b.planned.echo = name + " ";
      for (std::size_t t = 1; t < line.tokens.size(); ++t) {
        const Token& token = line.tokens[t];
        b.planned.echo += vars.expand(token.text, Variables::Mode::argument).text + " ";
      }
      if (spec->effects.creates_graph) state.graphs.push_back(*spec->effects.produces);
      collect_facts(b.planned.args);
      bound.push_back(std::move(b));
    }
    state.graphs.clear();
  }

  void collect_facts(const BoundArgs& args) {
    auto strings = [&](std::string_view name) -> std::vector<std::string> {
      for (const auto& v : args.values)
        if (v.spec && v.spec->name == name && v.has_value)
          if (const auto* list = std::get_if<std::vector<std::string>>(&v.value)) return *list;
      return {};
    };
    auto file = [&](std::string_view name) -> std::optional<std::filesystem::path> {
      for (const auto& v : args.values)
        if (v.spec && v.spec->name == name && v.given && v.has_value)
          if (const auto* s = std::get_if<std::string>(&v.value)) return core::path_from_utf8(*s);
      return std::nullopt;
    };
    auto add = [](std::vector<std::string>& to, const std::vector<std::string>& items) {
      for (const auto& i : items)
        if (std::find(to.begin(), to.end(), i) == to.end()) to.push_back(i);
    };
    if (args.command->name == "fb_rch") {
      add(state.facts.sources, strings("source"));
      add(state.facts.targets, strings("target"));
    } else if (args.command->name == "pathz3") {
      add(state.facts.fscs, strings("fsc"));
      if (auto f = file("confirmed_up")) state.facts.confirmed_up.push_back(*f);
      if (auto f = file("confirmed_down")) state.facts.confirmed_down.push_back(*f);
    }
  }

  // Pass 2: the checkers, the simulation of values, inputs and outputs, in batch order.
  void pass2() {
    auto& steps = plan.steps[point_index];
    auto& settings = plan.settings[point_index];
    auto& lines = plan.lines[point_index];
    for (auto& b : bound) {
      PlannedLine planned = std::move(b.planned);
      if (planned.kind != PlannedLine::Kind::step) { lines.push_back(std::move(planned)); continue; }
      state.inputs.clear();
      state.tools.clear();
      state.line_outputs.clear();
      run::StepSpec step;
      step.line = static_cast<std::uint32_t>(planned.line);
      step.point = program.points[point_index].label;
      if (planned.exec) {
        step.command = "exec";
        step.id = step_id(state, "exec");
        step.normalised = "exec " + planned.exec_text;
        state.tools.push_back(core::deps::Requirement{.id = "sh"});
        settings.push_back(std::make_unique<core::Settings>());
      } else {
        BoundArgs& args = planned.args;
        if (!args.command) { lines.push_back(std::move(planned)); continue; }
        const CommandSpec& spec = *args.command;
        state.step_id = step_id(state, spec.name);
        CheckContext ctx(state, args);
        apply_run_level(args);
        resolve_takes(args, ctx);
        check_rules(args, ctx);
        for (const auto kind : spec.effects.needs) {
          const bool have = kind == ValueKind::network    ? std::count(state.graphs.begin(), state.graphs.end(), kind) > 0
                            : kind == ValueKind::corridor ? std::count(state.graphs.begin(), state.graphs.end(), kind) > 0
                            : kind == ValueKind::results  ? !state.solves.empty()
                                                          : true;
          if (!have)
            ctx.error("batch.bad_argument",
                      "needs a " + std::string(kind_name(kind)) + ", and no earlier line makes one");
        }
        for (const auto& t : spec.tools) state.tools.push_back(t);
        if (spec.check) spec.check(args, ctx);
        if (spec.effects.creates_graph) {
          state.graphs.push_back(*spec.effects.produces);
          if (spec.name == "fb_rch") {
            CorridorFacts c;
            c.graph = static_cast<int>(state.graphs.size());
            c.line = planned.line;
            if (args.at("bound").has_value) c.bound = std::get<std::int64_t>(args.at("bound").value);
            if (args.at("source").has_value) c.source = std::get<std::vector<std::string>>(args.at("source").value);
            if (args.at("target").has_value) c.target = std::get<std::vector<std::string>>(args.at("target").value);
            state.corridors[c.graph] = std::move(c);
          }
        }
        if (spec.effects.produces == ValueKind::observations) state.observations = state.any_cudf = true;
        if (spec.effects.produces == ValueKind::shortlist) state.shortlist = true;
        step.command = std::string(spec.name);
        step.id = state.step_id;
        step.normalised = args.normalised();
        settings.push_back(settings_of(args));
      }
      step.settings = settings.back().get();
      step.inputs = state.inputs;
      for (const auto& [path, column] : state.line_outputs) step.outputs.push_back(core::path_from_utf8(path));
      for (const auto& t : state.tools) require_tool(t, planned.line, step.command);
      planned.step = steps.size();
      steps.push_back(std::move(step));
      lines.push_back(std::move(planned));
    }
  }

  // Tools (rows 2A.1, 2B.15): section 11's registry probes each tool once, before the first step.
  core::deps::Registry tools;
  std::map<std::string, std::pair<int, std::string>> first_need;   // id -> the first line that needs it
  void require_tool(core::deps::Requirement need, int line, std::string_view command) {
    if (!options.tools) return;
    need.requested_by = std::string(command) + " (line " + std::to_string(line) + ")";
    first_need.emplace(need.id, std::make_pair(line, std::string(command)));
    tools.require(std::move(need));
  }
  void check_tools() {
    if (!options.tools || first_need.empty()) return;
    plan.tools = tools.check();
    for (const auto& probe : plan.tools) {
      if (probe.ok) continue;
      const auto& [line, command] = first_need[probe.id];
      const bool font = probe.id.starts_with("font:");
      diag(font ? Severity::warning : Severity::error, line, 0, font ? "deps.font_missing" : "deps.missing",
           probe.message, command);
    }
  }

  // Decision 0021: every input that the batch does not make exists, is a regular file and is readable
  // before anything starts; each distinct path is checked once.
  void check_inputs() {
    if (!options.inputs) return;
    for (const auto& step : plan.steps[point_index]) {
      for (const auto& in : step.inputs) {
        if (in.made_by_line) continue;
        const std::string p = generic(in.path);
        if (!state.checked_paths.insert(p + "\n" + step.id).second) continue;
        std::error_code ec;
        const auto status = std::filesystem::status(in.path, ec);
        if (ec || !std::filesystem::exists(status)) {
          diag(Severity::error, static_cast<int>(step.line), 0, "input.missing",
               step.command + ": " + in.argument + " file " + p + " does not exist", step.command, in.argument, p);
          continue;
        }
        if (!std::filesystem::is_regular_file(status)) {
          diag(Severity::error, static_cast<int>(step.line), 0, "input.not_file",
               step.command + ": " + in.argument + " " + p + " is not a file", step.command, in.argument, p);
          continue;
        }
        std::ifstream probe(in.path, std::ios::binary);
        if (!probe) {
          diag(Severity::error, static_cast<int>(step.line), 0, "input.unreadable",
               step.command + ": cannot read " + in.argument + " file " + p, step.command, in.argument, p);
          continue;
        }
        if (in.is_list) check_list(step, in, probe);
      }
    }
  }

  // A list input's members (S2.16, S7.06): relative lines resolve against the list's folder, or kgml_dir;
  // a bare pathway id names <id>.xml there (02.C12).
  void check_list(const run::StepSpec& step, const run::InputSpec& in, std::ifstream& file) {
    std::filesystem::path base = in.path.parent_path();
    if (step.settings) {
      for (const auto& s : step.settings->all())
        if (s.spec && s.spec->name == "kgml_dir" && s.value)
          base = resolve_input(program.source.path.parent_path(), std::get<std::string>(*s.value));
    }
    std::string text;
    int missing = 0;
    std::string first_missing;
    while (std::getline(file, text)) {
      if (!text.empty() && text.back() == '\r') text.pop_back();
      const auto b = text.find_first_not_of(" \t");
      if (b == std::string::npos || text[b] == '#') continue;
      const auto e = text.find_last_not_of(" \t");
      const std::string entry = text.substr(b, e - b + 1);
      std::filesystem::path member = resolve_input(base, entry);
      if (entry.find('/') == std::string::npos && entry.find('.') == std::string::npos)
        member = resolve_input(base, entry + ".xml");
      std::error_code ec;
      if (!std::filesystem::is_regular_file(member, ec)) {
        if (missing++ == 0) first_missing = generic(member);
      }
    }
    if (missing > 0)
      diag(Severity::error, static_cast<int>(step.line), 0, "input.list_member",
           step.command + ": " + std::to_string(missing) + " file" + (missing == 1 ? "" : "s") + " listed in " +
               generic(in.path) + " do" + (missing == 1 ? "es" : "") + " not exist, the first " + first_missing,
           step.command, in.argument, first_missing);
  }
};

}  // namespace

Plan check(Program program, std::span<const int> only_points) {
  CheckOptions options;
  options.only_points.assign(only_points.begin(), only_points.end());
  return check(std::move(program), options);
}

Plan check(Program program, const CheckOptions& options) {
  Plan plan;
  plan.report.batch = program.source.path;
  plan.report.batch_sha256 = program.source.sha256;
  plan.report.points = static_cast<std::uint32_t>(program.points.size());
  // The program's own diagnostics (invalid UTF-8) come first.
  if (program.source.invalid_utf8_line)
    add_diagnostic(plan.report, core::Severity::error, *program.source.invalid_utf8_line, 1, "batch.bad_value",
                   "line " + std::to_string(*program.source.invalid_utf8_line) + " is not valid UTF-8");
  // fusion --set takes the run line's keys (row 2B.2); debug is --debug's alone (01.C8).
  for (const auto& [key, value] : options.settings) {
    const auto keys = run_level_keys();
    const auto* s = find_setting(key);
    if (key == "run.debug" || key == "debug")
      add_diagnostic(plan.report, core::Severity::error, 0, 0, "batch.bad_argument",
                     "--set: debug is set with --debug (01.C8)", {}, "run.debug");
    else if (!s || std::find(keys.begin(), keys.end(), key) == keys.end())
      add_diagnostic(plan.report, core::Severity::error, 0, 0, "batch.bad_argument",
                     "--set: unknown setting '" + key + "'; it takes " + join_views(keys, ", "), {}, key);
    else if (std::string why; !value.empty() && s->type != core::Type::text &&
                              !parse_value(*s, value, core::Source::named, why))
      add_diagnostic(plan.report, core::Severity::error, 0, 0, "batch.bad_value",
                     "--set: " + key + " '" + value + "' " + why, {}, key);
  }
  plan.program = std::move(program);
  const std::size_t points = plan.program.points.size();
  plan.facts.resize(points);
  plan.steps.resize(points);
  plan.lines.resize(points);
  plan.settings.resize(points);
  plan.variables.resize(points);
  for (std::size_t p = 0; p < points; ++p) {
    Planner planner(plan.program, options, plan);
    planner.point_index = p;
    planner.state.report = &plan.report;
    planner.state.batch_dir = plan.program.source.path.parent_path();
    if (points > 1) planner.state.point = static_cast<std::uint32_t>(p + 1);
    planner.pass1();
    planner.pass2();
    planner.check_inputs();
    planner.check_tools();
    plan.facts[p] = planner.state.facts;
    // Every variable in definition order, the four predefined ones first with line 0 (01.C18, row 1.14).
    plan.variables[p] = planner.vars.in_order();
  }
  // Run-level settings with their sources (E1).
  for (const auto key : run_level_keys()) {
    const auto* s = find_setting(key);
    if (!s) continue;
    core::Setting setting;
    setting.spec = s;
    std::string value;
    core::Source source = core::Source::default_value;
    for (const auto& [k, v] : plan.run_line.settings)
      if (k == key && !v.empty()) { value = v; source = core::Source::run_line; }
    for (const auto& [k, v] : options.settings)
      if (k == key) { value = v; source = core::Source::set_option; }
    if (source == core::Source::default_value && s->need == core::Need::defaulted) value = std::string(s->default_text);
    std::string why;
    if (!value.empty())
      if (auto parsed = parse_value(*s, value, core::Source::named, why)) setting.value = std::move(*parsed);
    setting.source = source;
    plan.run_settings.push_back(std::move(setting));
  }
  // Diagnostics are sorted by point, line and column (01's Determinism).
  std::stable_sort(plan.report.diagnostics.begin(), plan.report.diagnostics.end(),
                   [](const core::Diagnostic& a, const core::Diagnostic& b) {
                     const auto pa = a.point.value_or(0), pb = b.point.value_or(0);
                     if (pa != pb) return pa < pb;
                     if (a.line != b.line) return a.line < b.line;
                     return a.column < b.column;
                   });
  return plan;
}

void print_diagnostics(std::ostream& err, const core::CheckReport& report, std::string_view batch_name) {
  for (const auto& d : report.diagnostics) {
    const char* severity = d.severity == core::Severity::error ? "error" : d.severity == core::Severity::warning ? "warning" : "note";
    err << severity << ": " << batch_name;
    if (d.line > 0) err << ":" << d.line;
    if (d.point) err << " (point " << *d.point << ")";
    err << ": " << d.message << " [" << d.code << "]\n";
  }
}

}  // namespace fusion::batch
