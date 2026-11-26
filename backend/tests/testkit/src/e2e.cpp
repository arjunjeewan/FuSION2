// Running fusion and comparing run directories (e2e.hpp).
#include "fusion/testkit/e2e.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <tuple>

#include "fusion/core/core.hpp"
#include "fusion/core/process.hpp"
#include "fusion/testkit/check.hpp"
#include "fusion/testkit/run_facts.hpp"
#include "util.hpp"

namespace fusion::testkit {

namespace fs = std::filesystem;
using core::Json;

// ---------------------------------------------------------------- running fusion

RunResult run_fusion(const fs::path& exe, std::span<const std::string> args, const ScratchDir& scratch,
                     std::span<const std::pair<std::string, std::string>> env, std::chrono::milliseconds timeout) {
  core::ProcessSpec spec;
  spec.program = exe;
  spec.args.assign(args.begin(), args.end());
  spec.working_dir = scratch.path();
  spec.env_set.assign(env.begin(), env.end());
  spec.input = core::ProcessSpec::Input::null;
  spec.output = core::ProcessSpec::Output::capture;
  spec.timeout = timeout;
  const core::ProcessResult r = core::run_process(spec);
  RunResult out;
  out.exit_code = r.exit_code;
  out.out = r.stdout_text;
  out.err = r.stderr_text + (r.error.empty() ? "" : (r.stderr_text.empty() ? "" : "\n") + r.error) +
            (r.timed_out ? "\n(timed out after " + std::to_string(timeout.count()) + " ms)" : "");
  for (std::size_t k = 0; k + 1 < args.size(); ++k)
    if (args[k] == "--run-dir") {
      const fs::path p = core::path_from_utf8(args[k + 1]);
      out.run_dir = p.is_absolute() ? p : scratch.path() / p;
      return out;
    }
  std::error_code ec;
  std::vector<fs::path> runs;
  if (fs::is_directory(scratch.path() / "runs", ec))
    for (const auto& entry : fs::directory_iterator(scratch.path() / "runs", ec))
      if (fs::exists(entry.path() / "run.json", ec)) runs.push_back(entry.path());
  std::sort(runs.begin(), runs.end());
  if (!runs.empty()) out.run_dir = runs.back();
  return out;
}

// ---------------------------------------------------------------- the normaliser

Normaliser fusion_normaliser(const fs::path& run_dir, const fs::path& case_dir, const std::optional<DataRoot>& data) {
  Normaliser n;
  // run.json's volatile fields (09-runs.md:292), and FuSION's own version and commit (section 12).
  for (const char* p : {"/run", "/name", "/pid", "/labels", "/started", "/finished", "/resources", "/launch_dir",
                        "/command_line", "/platform", "/install", "/environment", "/batch/path", "/parent",
                        "/steps/*/started", "/steps/*/wall_s", "/tools/*/path", "/stop", "/build_id",
                        "/versions/fusion"})
    n.json_pointers.push_back(std::string("run.json:") + p);
  // events.jsonl: the envelope's t and seq (C5.3), every time field, run.started's pid, dir, fusion, and the
  // run's id, name and parent, which differ in every run (prompt log entry 349); and the model-dependent
  // fields of solve.check events.
  for (const char* p : {"/t", "/seq", "/wall_s", "/cpu_s", "@run.started/pid", "@run.started/dir",
                        "@run.started/fusion", "@run.started/run", "@run.started/name", "@run.started/parent",
                        "@solve.check/source", "@solve.check/model_n", "@solve.check/model_e"})
    n.json_pointers.push_back(std::string("events.jsonl:") + p);
  n.path_tokens.emplace_back(run_dir, "<RUN>");
  n.path_tokens.emplace_back(case_dir, "<CASE>");
  if (data) n.path_tokens.emplace_back(data->root, "<DATA>");
  n.rules.push_back({"run.json", Compare::exact, {}});
  n.rules.push_back({"events.jsonl", Compare::exact, {}});
  return n;
}

namespace {

bool glob_match(std::string_view pattern, std::string_view text) {
  if (pattern.empty()) return text.empty();
  if (pattern.starts_with("**")) {
    const std::string_view rest = pattern.substr(pattern.size() > 2 && pattern[2] == '/' ? 3 : 2);
    for (std::size_t k = 0; k <= text.size(); ++k)
      if ((k == 0 || text[k - 1] == '/' || rest.empty()) && glob_match(rest, text.substr(k))) return true;
    return false;
  }
  if (pattern[0] == '*') {
    for (std::size_t k = 0; k <= text.size(); ++k) {
      if (glob_match(pattern.substr(1), text.substr(k))) return true;
      if (k < text.size() && text[k] == '/') break;
    }
    return false;
  }
  if (text.empty()) return false;
  if (pattern[0] == '?') return text[0] != '/' && glob_match(pattern.substr(1), text.substr(1));
  return pattern[0] == text[0] && glob_match(pattern.substr(1), text.substr(1));
}

std::vector<std::pair<std::string, std::string>> token_strings(const Normaliser& n) {
  std::vector<std::pair<std::string, std::string>> out;
  for (const auto& [path, token] : n.path_tokens) {
    if (path.empty()) continue;
    std::string generic = core::to_utf8(path);
    while (generic.size() > 1 && generic.back() == '/') generic.pop_back();
    out.emplace_back(generic, token);
    std::string native = path.string();
    if (native != generic) out.emplace_back(native, token);
  }
  std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
  return out;
}

std::string replace_tokens(std::string text, const std::vector<std::pair<std::string, std::string>>& tokens) {
  for (const auto& [from, to] : tokens) {
    std::size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos) {
      text.replace(at, from.size(), to);
      at += to.size();
    }
  }
  return text;
}

void tokens_in(Json& j, const std::vector<std::pair<std::string, std::string>>& tokens) {
  if (j.is_string()) {
    j = replace_tokens(j.get<std::string>(), tokens);
  } else if (j.is_array() || j.is_object()) {
    for (auto& child : j) tokens_in(child, tokens);
  }
}

void mask_at(Json& j, const std::vector<std::string>& segments, std::size_t k) {
  if (k == segments.size()) {
    j = "<masked>";
    return;
  }
  const std::string& s = segments[k];
  if (s == "*") {
    if (j.is_array() || j.is_object())
      for (auto& child : j) mask_at(child, segments, k + 1);
  } else if (j.is_object() && j.contains(s)) {
    mask_at(j[s], segments, k + 1);
  } else if (j.is_array() && !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    const std::size_t i = std::stoul(s);
    if (i < j.size()) mask_at(j[i], segments, k + 1);
  }
}

void apply_masks(Json& j, const Normaliser& n, const std::string& file_name) {
  for (const std::string& pointer : n.json_pointers) {
    std::string path = pointer;
    const std::size_t colon = path.find(':');
    if (colon != std::string::npos && colon < path.find('/') && colon < path.find('@')) {
      if (path.substr(0, colon) != file_name) continue;
      path = path.substr(colon + 1);
    }
    if (path.starts_with("@")) {
      const std::size_t slash = path.find('/');
      const std::string type = path.substr(1, slash - 1);
      if (!j.is_object() || !j.contains("type") || j["type"] != type) continue;
      path = slash == std::string::npos ? "" : path.substr(slash);
    }
    std::vector<std::string> segments = detail::split(path, '/');
    if (!segments.empty() && segments.front().empty()) segments.erase(segments.begin());
    mask_at(j, segments, 0);
  }
}

bool absolute_text(const std::string& s) {
  return s.starts_with("/") || (s.size() > 2 && s[1] == ':' && (s[2] == '\\' || s[2] == '/'));
}

// run.json: every absolute "path" and "also" that no token covers is volatile too (09-runs.md:292).
void mask_absolute_paths(Json& j) {
  if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      if ((it.key() == "path" || it.key() == "also") && it.value().is_string() &&
          absolute_text(it.value().get<std::string>()))
        it.value() = "<masked>";
      else
        mask_absolute_paths(it.value());
    }
  } else if (j.is_array()) {
    for (auto& child : j) mask_absolute_paths(child);
  }
}

std::string normalise_json(const std::string& text, const std::string& name, const Normaliser& n,
                           const std::vector<std::pair<std::string, std::string>>& tokens) {
  Json j = Json::parse(text);
  apply_masks(j, n, name);
  tokens_in(j, tokens);
  if (name == "run.json") mask_absolute_paths(j);
  return j.dump(2) + "\n";
}

std::string normalise_events(const std::string& text, const std::string& name, const Normaliser& n,
                             const std::vector<std::pair<std::string, std::string>>& tokens) {
  using Key = std::tuple<std::string, std::string, std::string>;
  std::map<Key, std::vector<std::string>> streams;
  for (const std::string& line : detail::lines_of(text)) {
    if (line.empty()) continue;
    Json j = Json::parse(line);
    const auto field = [&](const char* k) {
      return j.is_object() && j.contains(k) ? (j[k].is_string() ? j[k].get<std::string>() : j[k].dump()) : std::string();
    };
    const Key key{field("step"), field("point"), field("case")};
    apply_masks(j, n, name);
    tokens_in(j, tokens);
    streams[key].push_back(j.dump());
  }
  std::string out;
  for (const auto& [key, lines] : streams)
    for (const std::string& l : lines) out += l + "\n";
  return out;
}

std::string normalise_tsv(const std::string& text, const FileRule& rule,
                          const std::vector<std::pair<std::string, std::string>>& tokens) {
  const std::vector<std::string> lines = detail::lines_of(text);
  if (lines.empty()) return {};
  const std::vector<std::string> header = detail::split(lines[0], '\t');
  std::vector<bool> masked(header.size(), false);
  for (std::size_t c = 0; c < header.size(); ++c)
    masked[c] = std::find(rule.masked_columns.begin(), rule.masked_columns.end(), header[c]) != rule.masked_columns.end();
  std::string out = lines[0] + "\n";
  for (std::size_t k = 1; k < lines.size(); ++k) {
    std::vector<std::string> f = detail::split(lines[k], '\t');
    for (std::size_t c = 0; c < f.size() && c < masked.size(); ++c)
      if (masked[c]) f[c].clear();
    out += detail::join(f, "\t") + "\n";
  }
  return replace_tokens(out, tokens);
}

std::string first_difference(const std::string& a, const std::string& b) {
  const auto la = detail::lines_of(a), lb = detail::lines_of(b);
  for (std::size_t k = 0; k < std::max(la.size(), lb.size()); ++k) {
    const std::string x = k < la.size() ? la[k] : "<end>", y = k < lb.size() ? lb[k] : "<end>";
    if (x != y) return "line " + std::to_string(k + 1) + ": actual " + x.substr(0, 160) + " | golden " + y.substr(0, 160);
  }
  return "the bytes differ";
}

// results mode: the frontier and its flags exactly; each actual witness passes check() at its point.
std::optional<std::string> compare_results(const fs::path& actual_root, const std::string& rel, const fs::path& expected) {
  const Json a = Json::parse(detail::slurp(actual_root / core::path_from_utf8(rel)));
  const Json e = Json::parse(detail::slurp(expected));
  const auto frontier = [](const Json& r) {
    std::vector<std::string> out;
    for (const auto& c : r.at("cases")) {
      std::string line = c.at("case").get<std::string>() + " " + c.at("status").get<std::string>() + ":";
      for (const auto& p : c.at("points"))
        line += " (" + std::to_string(p.at("n").get<int>()) + "," + std::to_string(p.at("e").get<int>()) +
                (p.at("certified").get<bool>() ? ",certified)" : ",uncertified)");
      out.push_back(line);
    }
    return out;
  };
  const auto fa = frontier(a), fe = frontier(e);
  if (fa != fe) return "frontier: actual " + detail::join(fa, "; ") + " | golden " + detail::join(fe, "; ");
  const std::string step = rel.substr(0, rel.find('/'));
  const RunFacts facts = read_run(actual_root);
  for (const SolveFacts& s : facts.solves) {
    if (s.step != step) continue;
    for (const SolveFacts::Case& c : s.cases)
      for (std::size_t k = 0; k < c.frontier.points.size() && k < c.witnesses.size(); ++k) {
        const Verdict v = check(s.corridor, c.spec, c.witnesses[k], Semantics::fusion2);
        const Point p = c.frontier.points[k];
        if (!v.valid || v.cost != p)
          return c.name + " (" + std::to_string(p.n) + "," + std::to_string(p.e) + "): the witness " +
                 (v.valid ? "costs (" + std::to_string(v.cost.n) + "," + std::to_string(v.cost.e) + ")"
                          : "fails: " + (v.violations.empty() ? std::string() : v.violations.front()));
      }
    return std::nullopt;
  }
  return "no solve " + step + " in the run";
}

}  // namespace

std::string normalise_file(const fs::path& file, const FileRule& rule, const Normaliser& n) {
  const std::string text = detail::slurp(file);
  const auto tokens = token_strings(n);
  const std::string name = core::to_utf8(file.filename());
  if (rule.mode == Compare::tsv_masked) return normalise_tsv(text, rule, tokens);
  if (name.ends_with(".jsonl")) return normalise_events(text, name, n, tokens);
  if (name.ends_with(".json")) return normalise_json(text, name, n, tokens);
  return replace_tokens(text, tokens);
}

std::vector<std::pair<std::string, FileRule>> matched_files(const fs::path& root, const Normaliser& n) {
  std::vector<std::pair<std::string, FileRule>> out;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return out;
  for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec || !it->is_regular_file(ec)) continue;
    const std::string rel = core::to_utf8(fs::relative(it->path(), root, ec));
    for (const FileRule& rule : n.rules)
      if (glob_match(rule.glob, rel)) {
        out.emplace_back(rel, rule);
        break;
      }
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return out;
}

bool TreeDiff::empty() const { return missing.empty() && unexpected.empty() && differing.empty(); }

TreeDiff compare_tree(const fs::path& actual, const fs::path& expected, const Normaliser& n) {
  TreeDiff d;
  std::map<std::string, FileRule> a, e;
  for (auto& [rel, rule] : matched_files(actual, n)) a.emplace(rel, rule);
  for (auto& [rel, rule] : matched_files(expected, n)) e.emplace(rel, rule);
  for (const auto& [rel, rule] : e)
    if (!a.contains(rel)) d.missing.push_back(rel);
  for (const auto& [rel, rule] : a) {
    if (!e.contains(rel)) {
      d.unexpected.push_back(rel);
      continue;
    }
    const fs::path golden = expected / core::path_from_utf8(rel);
    try {
      if (rule.mode == Compare::results) {
        if (auto why = compare_results(actual, rel, golden)) d.differing.push_back(rel + "\t" + *why);
        continue;
      }
      const std::string x = normalise_file(actual / core::path_from_utf8(rel), rule, n);
      const std::string y = normalise_file(golden, rule, n);
      if (x != y) d.differing.push_back(rel + "\t" + first_difference(x, y));
    } catch (const std::exception& ex) {
      d.differing.push_back(rel + "\t" + ex.what());
    }
  }
  return d;
}

}  // namespace fusion::testkit
