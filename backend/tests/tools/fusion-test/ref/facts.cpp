// The facts per stage (facts.hpp): FuSION 1's from its recorded files, 2.0's from a chained run, and the
// predicates' signals, all as the kit's types and read through the kit's readers.
#include "ref/facts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include <pugixml.hpp>

#include "files.hpp"
#include "fusion/network/network.hpp"
#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/run_facts.hpp"
#include "fusion/testkit/step_context.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;
using testkit::CanonGraph;
using testkit::CheckOutcome;
using testkit::Point;

// ---------------------------------------------------------------- names

std::string_view name(Stage s) noexcept {
  switch (s) {
    case Stage::classify: return "classify";
    case Stage::exceptions: return "exceptions";
    case Stage::gsea: return "gsea";
    case Stage::merge: return "merge";
    case Stage::corridor: return "corridor";
    case Stage::observations: return "observations";
    case Stage::solve_inputs: return "solve_inputs";
    case Stage::solve: return "solve";
  }
  return "solve";
}
std::optional<Stage> parse_stage(std::string_view s) noexcept {
  for (Stage st : kStages)
    if (name(st) == s) return st;
  return std::nullopt;
}
std::string_view name(Mode m) noexcept {
  switch (m) {
    case Mode::chained: return "chained";
    case Mode::pinned: return "pinned";
    case Mode::verify: return "verify";
  }
  return "chained";
}
std::optional<Mode> parse_mode(std::string_view s) noexcept {
  if (s == "chained") return Mode::chained;
  if (s == "pinned") return Mode::pinned;
  if (s == "verify") return Mode::verify;
  return std::nullopt;
}

namespace {

std::vector<std::string> split(std::string_view s, char sep) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const std::size_t at = s.find(sep, start);
    out.emplace_back(s.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
    if (at == std::string_view::npos) break;
    start = at + 1;
  }
  return out;
}
std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out = split(text, '\n');
  for (std::string& l : out)
    if (!l.empty() && l.back() == '\r') l.pop_back();
  if (!out.empty() && out.back().empty()) out.pop_back();
  return out;
}
std::vector<std::string> words(std::string_view s) {
  std::istringstream in{std::string(s)};
  std::vector<std::string> out;
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}
std::string stem(const std::string& path) { return core::to_utf8(core::path_from_utf8(path).stem()); }
std::string strip_colons(std::string s) {
  s.erase(std::remove(s.begin(), s.end(), ':'), s.end());
  return s;
}
// A command's canonical name from a run.json step id: "fb_rch-2" is fb_rch.
std::string command_of(const std::string& step) {
  const std::size_t dash = step.rfind('-');
  if (dash != std::string::npos && dash + 1 < step.size() &&
      std::all_of(step.begin() + static_cast<std::ptrdiff_t>(dash) + 1, step.end(), [](char c) { return c >= '0' && c <= '9'; }))
    return step.substr(0, dash);
  return step;
}
Json read_json(const fs::path& p) {
  try {
    return Json::parse(fusiontest::slurp(p));
  } catch (const Json::exception& e) {
    throw core::Failure("ref.facts", core::to_utf8(p) + ": " + e.what());
  }
}

CheckOutcome outcome_of(const std::string& s) {
  if (s == "sat") return CheckOutcome::sat;
  if (s == "unsat") return CheckOutcome::unsat;
  return CheckOutcome::unknown;
}

}  // namespace

// 06.C6: a frontier point is certified when each lower neighbour inside the box is proven UNSAT by an
// answer, directly or by monotonicity (S5.18).
testkit::Frontier certify(std::vector<Point> points, const Answers& answers, Point lo, Point hi) {
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  const auto inside = [&](Point p) { return p.n >= lo.n && p.e >= lo.e && p.n <= hi.n && p.e <= hi.e; };
  const auto unsat = [&](Point p) {
    return std::any_of(answers.begin(), answers.end(), [&](const auto& a) {
      return a.second == CheckOutcome::unsat && p.n <= a.first.n && p.e <= a.first.e;
    });
  };
  testkit::Frontier f;
  for (Point p : points) {
    bool ok = true;
    for (Point below : {Point{p.n - 1, p.e}, Point{p.n, p.e - 1}})
      if (inside(below) && !unsat(below)) ok = false;
    f.points.push_back(p);
    f.certified.push_back(ok);
  }
  return f;
}

namespace {

// The corridor node an id names: its canonical key, its module key, or a member of a non-complex node.
std::optional<std::string> node_named(const CanonGraph& g, const std::string& id) {
  for (const auto& n : g.nodes)
    if (n.key == id || testkit::module_key(n) == id) return n.key;
  for (const auto& n : g.nodes)
    if (!testkit::is_complex_key(n.key) && std::binary_search(n.members.begin(), n.members.end(), id)) return n.key;
  return std::nullopt;
}

// FuSION 1's counting of observations (main.cc:4667-4679, as S5.07 records it), on 2.0's or FuSION 1's genes:
// the up genes less the avoided and confirmed-down ones, plus the confirmed-up ones in the corridor; a gene
// in both is up. An out case's FSC leaves both (S5.17).
}  // namespace

std::pair<int, int> observed_counts(const std::vector<std::pair<std::string, char>>& in_corridor, const testkit::CaseSpec& s,
                                    const std::set<std::string>& corridor_ids) {
  std::set<std::string> up, down;
  for (const auto& [id, r] : in_corridor) (r == 'u' ? up : down).insert(id);
  for (const std::string& id : s.avoid) up.erase(id);
  if (s.solve_case == testkit::CaseSpec::Case::out)
    for (const std::string& id : s.fsc) up.erase(id);   // avoided, as FuSION 1's without-FSC lists have it
  for (const std::string& id : s.confirmed_down) up.erase(id);
  for (const std::string& id : s.confirmed_up)
    if (corridor_ids.contains(id)) up.insert(id);
  for (const std::string& id : s.confirmed_up) down.erase(id);
  for (const std::string& id : s.confirmed_down)
    if (corridor_ids.contains(id)) down.insert(id);
  for (const std::string& id : up) down.erase(id);
  return {static_cast<int>(up.size()), static_cast<int>(down.size())};
}

std::set<std::string> member_ids(const CanonGraph& g) {
  std::set<std::string> out;
  for (const auto& n : g.nodes) out.insert(n.members.begin(), n.members.end());
  return out;
}

std::vector<std::string> file_lines(const std::string& text) { return lines_of(text); }
std::vector<std::string> split_on(std::string_view s, char sep) { return split(s, sep); }

// ---------------------------------------------------------------- the batch

const BatchLine* BatchView::first(std::string_view command) const {
  for (const BatchLine& l : lines)
    if (l.command == command) return &l;
  return nullptr;
}
std::vector<const BatchLine*> BatchView::all(std::string_view command) const {
  std::vector<const BatchLine*> out;
  for (const BatchLine& l : lines)
    if (l.command == command) out.push_back(&l);
  return out;
}

namespace {
const batch::BoundValue* bound(const BatchLine& l, std::string_view arg) {
  if (!l.args) return nullptr;
  for (const batch::BoundValue& v : l.args->values)
    if (v.spec && v.spec->name == arg) return &v;
  return nullptr;
}
}  // namespace

bool given(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  return v && v->given && v->has_value;
}
std::optional<std::string> text(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* s = std::get_if<std::string>(&v->value)) return *s;
  return batch::value_text(v->value);
}
std::optional<fs::path> path(const BatchLine& l, std::string_view arg) {
  if (!given(l, arg)) return std::nullopt;
  const auto t = text(l, arg);
  if (!t || t->empty()) return std::nullopt;
  return core::path_from_utf8(*t);
}
std::optional<std::int64_t> integer(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* i = std::get_if<std::int64_t>(&v->value)) return *i;
  return std::nullopt;
}
std::optional<double> real(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* d = std::get_if<double>(&v->value)) return *d;
  if (const auto* i = std::get_if<std::int64_t>(&v->value)) return static_cast<double>(*i);
  return std::nullopt;
}
std::optional<bool> flag(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* b = std::get_if<bool>(&v->value)) return *b;
  return std::nullopt;
}
std::vector<std::string> list(const BatchLine& l, std::string_view arg) {
  const auto* v = bound(l, arg);
  if (!v || !v->has_value) return {};
  if (const auto* x = std::get_if<std::vector<std::string>>(&v->value)) return *x;
  if (const auto* s = std::get_if<std::string>(&v->value)) return {*s};
  return {};
}

BatchView read_batch(const fs::path& batch_file) {
  core::CheckReport parsed;
  batch::Program program = batch::parse(batch::source_from_text(batch_file, fusiontest::slurp(batch_file)), parsed);
  batch::CheckOptions options;
  options.inputs = options.tools = false;                 // no file checks (the batch corpus test's way)
  auto plan = std::make_shared<batch::Plan>(batch::check(std::move(program), options));
  for (const core::CheckReport* r : {&parsed, &plan->report})
    for (const core::Diagnostic& d : r->diagnostics)
      if (d.severity == core::Severity::error)
        throw core::Failure("ref.batch", core::to_utf8(batch_file.filename()) + ":" + std::to_string(d.line) + ": " +
                                             d.message + " [" + d.code + "]");
  BatchView view;
  if (plan->lines.empty()) throw core::Failure("ref.batch", "the batch has no point to run");
  for (const batch::PlannedLine& l : plan->lines.front())
    if (l.kind == batch::PlannedLine::Kind::step && !l.exec && l.args.command)
      view.lines.push_back({std::string(l.args.command->name), l.line, &l.args});
  view.plan = std::move(plan);
  if (const BatchLine* cudf = view.first("cudf"))
    if (const auto up = path(*cudf, "up_out"); up && up->is_absolute()) view.write_dir = up->parent_path();
  return view;
}

fs::path recorded(const fs::path& named, const BatchView& b, const fs::path& f1_run) {
  if (b.write_dir.empty()) return named;
  const fs::path rel = named.lexically_relative(b.write_dir);
  if (rel.empty() || *rel.begin() == "..") return named;
  return f1_run / rel;
}

std::vector<std::string> ids_in(const std::optional<fs::path>& file) {
  if (!file) return {};
  std::error_code ec;
  if (!fs::is_regular_file(*file, ec)) throw core::Failure("ref.facts", "no list file " + core::to_utf8(*file));
  return testkit::f1::read_ids(*file);
}

std::vector<Arm> arms(const Case& c, const BatchView& b, const fs::path& f1_run) {
  const std::vector<const BatchLine*> lines = b.all("pathz3");
  const auto prefix = [](const BatchLine* l) {
    const auto t = text(*l, "legacy_prefix");
    return t ? core::to_utf8(core::path_from_utf8(*t).filename()) : std::string();
  };
  std::vector<Arm> out;
  for (const auto& [kase, artefacts] : c.fusion1.solve) {
    Arm a;
    a.name = kase;
    if (kase == "free" && lines.size() == 1) {
      a.line = lines.front();
    } else {
      const std::string want = kase == "out" ? "temp_wo" : "temp_w";
      for (const BatchLine* l : lines)
        if (prefix(l) == want) a.line = l;
    }
    out.push_back(std::move(a));
  }
  const auto ids = [&](const std::string& kase, const char* arg) {
    std::set<std::string> s;
    for (const Arm& a : out)
      if (a.name == kase && a.line)
        if (const auto p = path(*a.line, arg))
          for (const std::string& id : ids_in(recorded(*p, b, f1_run))) s.insert(id);
    return s;
  };
  const bool with = std::any_of(out.begin(), out.end(), [](const Arm& a) { return a.name == "in" && a.line; });
  const bool without = std::any_of(out.begin(), out.end(), [](const Arm& a) { return a.name == "out" && a.line; });
  for (Arm& a : out) {
    std::set<std::string> fsc;
    if (a.name == "in" && without) {
      const auto mine = ids("in", "essential_nodes"), theirs = ids("out", "essential_nodes");
      std::set_difference(mine.begin(), mine.end(), theirs.begin(), theirs.end(), std::inserter(fsc, fsc.end()));
    } else if (a.name == "out" && with) {
      const auto mine = ids("out", "avoid_nodes"), theirs = ids("in", "avoid_nodes");
      std::set_difference(mine.begin(), mine.end(), theirs.begin(), theirs.end(), std::inserter(fsc, fsc.end()));
    }
    a.fsc.assign(fsc.begin(), fsc.end());
  }
  return out;
}

// ---------------------------------------------------------------- FuSION 1's facts

namespace {

// FuSION 1's case as its pathz3 line gave it, in ids: the line's list files as the run left them, the up
// and down lists, and the families FuSION 1's console shows it emitted (S5.03, S5.15, S5.16).
testkit::CaseSpec fusion1_spec(const BatchLine& line, const BatchView& b, const fs::path& f1_run, const Facts& f,
                               const testkit::f1::ConsoleFacts::Solve* console) {
  testkit::CaseSpec s;
  if (f.up) s.up = *f.up;
  if (f.down) s.down = *f.down;
  const auto ids = [&](const char* arg) {
    const auto p = path(line, arg);
    return p ? ids_in(recorded(*p, b, f1_run)) : std::vector<std::string>{};
  };
  s.essential = ids("essential_nodes");
  s.avoid = ids("avoid_nodes");
  s.active = ids("active_nodes");
  s.inactive = ids("inactive_nodes");
  s.nonrelaxed = ids("nonrelaxed_nodes");
  s.confirmed_up = ids("confirmed_up");
  s.confirmed_down = ids("confirmed_down");
  const auto edges = [&](const char* arg) {
    std::vector<testkit::EdgeSpec> out;
    const auto p = path(line, arg);
    if (!p) return out;
    for (const std::string& l : lines_of(fusiontest::slurp(recorded(*p, b, f1_run)))) {
      const auto w = words(l);
      if (w.size() < 2) continue;
      testkit::EdgeSpec e{w[0], w[1], {}};
      if (w.size() >= 3 && w[2] != "-")
        for (const std::string& part : split(w[2], w[2].find(',') != std::string::npos ? ',' : '_'))
          if (!part.empty()) e.subtypes.push_back(part);
      out.push_back(std::move(e));
    }
    return out;
  };
  s.essential_edges = edges("essential_edges");
  s.avoid_edges = edges("avoid_edges");
  s.nonrelaxed_edges = edges("nonrelaxed_edges");
  if (console && console->coexpression_clauses)
    if (const auto csv = path(line, "coexpression_csv"))
      s.coexpressed = testkit::f1::read_coexpression(recorded(*csv, b, f1_run), real(line, "coexpression_threshold").value_or(0));
  if (console && console->frozen_clauses) s.frozen_threshold = real(line, "frozen_threshold");
  s.solve_case = testkit::CaseSpec::Case::free;          // FuSION 1 has no FSC argument (S5.17)
  s.named = false;                                       // a positional line (06.C16)
  return s;
}

}  // namespace

Facts fusion1_facts(const Case& c, const BatchView& b, const fs::path& dir) {
  Facts f;
  const fs::path f1_run = (dir / c.fusion1.up).parent_path();
  f.up = testkit::f1::read_ids(dir / c.fusion1.up);
  f.down = testkit::f1::read_ids(dir / c.fusion1.down);
  f.exceptions = testkit::f1::read_ids(dir / c.fusion1.exceptions);
  if (c.fusion1.gsea_table && c.fusion1.shortlist) {
    GseaFacts g;
    g.rows = testkit::f1::read_gsea_table(dir / *c.fusion1.gsea_table);
    for (const std::string& p : testkit::f1::read_ids(dir / *c.fusion1.shortlist)) g.shortlist.push_back(stem(p));
    f.gsea = std::move(g);
  }
  f.merged = testkit::f1::read_written_graph(dir / c.fusion1.reread);
  const testkit::f1::ConsoleFacts console = testkit::f1::read_console(dir / c.fusion1.console);
  if (!console.corridor_sizes.empty()) f.corridor_size = console.corridor_sizes.front();
  testkit::CanonCorridor corridor;
  corridor.graph = testkit::f1::read_written_graph(dir / c.fusion1.corridor);
  if (const BatchLine* fb = b.first("fb_rch")) {
    for (const std::string& s : list(*fb, "source")) corridor.sources.push_back(node_named(corridor.graph, s).value_or(s));
    for (const std::string& t : list(*fb, "target")) corridor.targets.push_back(node_named(corridor.graph, t).value_or(t));
    corridor.bound = static_cast<int>(integer(*fb, "bound").value_or(0));
  }
  f.corridor = corridor;

  const std::vector<const BatchLine*> pathz3 = b.all("pathz3");
  for (const Arm& arm : arms(c, b, f1_run)) {
    const SolveArtefacts& a = c.fusion1.solve.at(arm.name);
    SolveFacts s;
    s.name = arm.name;
    s.status = "recorded";
    s.dat = fusiontest::slurp(dir / a.dat);
    const testkit::f1::Limits limits = testkit::f1::read_limits(dir / a.limits);
    s.box_lo = limits.box_lo;
    s.box_hi = limits.box_hi;
    for (const testkit::f1::Check& k : limits.checks) s.answers.emplace_back(k.point, k.answer);
    s.frontier = certify(testkit::f1::read_dat(dir / a.dat), s.answers, s.box_lo, s.box_hi);
    if (a.solutions) s.records = testkit::f1::read_solutions(dir / *a.solutions, a.relaxations ? dir / *a.relaxations : fs::path());
    s.corridor = corridor;
    if (arm.line) {
      s.step = core::to_utf8(core::path_from_utf8(text(*arm.line, "legacy_prefix").value_or("")).filename());
      const auto k = static_cast<std::size_t>(std::find(pathz3.begin(), pathz3.end(), arm.line) - pathz3.begin());
      const testkit::f1::ConsoleFacts::Solve* block = k < console.solves.size() ? &console.solves[k] : nullptr;
      s.spec = fusion1_spec(*arm.line, b, f1_run, f, block);
      if (block) {
        s.up_down = block->up_down;
        s.coexpression = block->coexpression_clauses;
        s.frozen = block->frozen_clauses;
      }
    }
    if (const auto it = c.fusion1_seconds.find(arm.name); it != c.fusion1_seconds.end()) s.seconds = it->second;
    f.solves.push_back(std::move(s));
  }
  return f;
}

// ---------------------------------------------------------------- 2.0's facts from a chained run

Facts chained_facts(const Case& c, const BatchView& b, const fs::path& case_dir, const fs::path& run_dir) {
  Facts f;
  const Json run = read_json(run_dir / "run.json");
  f.exit = run.value("exit", 0);
  std::map<std::string, std::string> first_step;         // command -> its first step id
  std::map<std::string, std::string> prefix_of;          // pathz3 step -> its legacy prefix's last component
  for (const Json& step : run.value("steps", Json::array())) {
    const std::string id = step.value("id", "");
    first_step.emplace(command_of(id), id);
    for (const Json& s : step.value("settings", Json::array()))
      if (s.value("key", "") == "solve.legacy_prefix" && s.contains("value") && s["value"].is_string())
        prefix_of[id] = core::to_utf8(core::path_from_utf8(s["value"].get<std::string>()).filename());
  }
  const auto step_dir = [&](const std::string& command) -> std::optional<fs::path> {
    const auto it = first_step.find(command);
    if (it == first_step.end()) return std::nullopt;
    return run_dir / core::path_from_utf8(it->second);
  };
  std::error_code ec;

  if (const auto d = step_dir("cudf"); d && b.first("cudf")) {
    const BatchLine& cudf = *b.first("cudf");
    const auto file = [&](const char* arg, const char* fallback) {
      const auto p = path(cudf, arg);
      return *d / (p ? p->filename() : fs::path(fallback));
    };
    if (fs::exists(file("up_out", "up_reg"), ec)) {
      f.up = testkit::f1::read_ids(file("up_out", "up_reg"));
      f.down = testkit::f1::read_ids(file("down_out", "down_reg"));
    }
  }
  if (const auto d = step_dir("mff")) {
    if (fs::exists(*d / "exceptions.tsv", ec)) {
      std::vector<std::string> ids;
      const auto rows = lines_of(fusiontest::slurp(*d / "exceptions.tsv"));
      for (std::size_t k = 1; k < rows.size(); ++k) ids.push_back(split(rows[k], '\t').front());
      f.exceptions = std::move(ids);
    }
    if (fs::exists(*d / "merged_graph.xml", ec)) {
      testkit::StepContext sc(case_dir / "facts", "facts");
      f.merged = testkit::adapt::canon(*network::read_fusion_network(*d / "merged_graph.xml", sc.ctx()));
    }
  }
  if (const auto d = step_dir("gsea"); d && b.first("gsea")) {
    const auto shortlist = path(*b.first("gsea"), "shortlist");
    const std::string name = shortlist ? core::to_utf8(shortlist->filename()) : std::string("shortlist.txt");
    const fs::path table = *d / (core::to_utf8(core::path_from_utf8(name).stem()) + ".gsea.tsv");
    if (fs::exists(table, ec)) {
      GseaFacts g;
      g.rows = testkit::f1::read_gsea_table(table);
      for (const std::string& p : testkit::f1::read_ids(*d / name)) g.shortlist.push_back(stem(p));
      g.zero_weight_draws = 0;
      for (const std::string& l : lines_of(fusiontest::slurp(run_dir / "events.jsonl")))
        if (l.find("\"gsea.zero_weight_draws\"") != std::string::npos) g.zero_weight_draws = 1;
      f.gsea = std::move(g);
    }
  }

  const testkit::RunFacts facts = testkit::read_run(run_dir);
  std::size_t solves_left = 0;
  for (const auto& s : facts.solves) solves_left += s.cases.size();
  for (const testkit::SolveFacts& solve : facts.solves) {
    if (!f.corridor) f.corridor = solve.corridor;
    const Json results = read_json(run_dir / core::path_from_utf8(solve.step) / "results.json");
    const std::string artefact = results.value("corridor", Json::object()).value("artefact", "");
    const fs::path fb_dir = artefact.empty() ? fs::path() : (run_dir / core::path_from_utf8(artefact)).parent_path();
    if (!f.corridor_size && !artefact.empty() && fs::exists(run_dir / core::path_from_utf8(artefact), ec)) {
      const Json cj = read_json(run_dir / core::path_from_utf8(artefact));
      const Json counts = cj.value("counts", Json::object());
      f.corridor_size = std::pair{counts.value("nodes", 0), counts.value("edges", 0)};
    }
    std::vector<std::pair<std::string, char>> in_corridor;
    if (!fb_dir.empty() && fs::exists(fb_dir / "observations_match.tsv", ec)) {
      const auto rows = lines_of(fusiontest::slurp(fb_dir / "observations_match.tsv"));
      for (std::size_t k = 1; k < rows.size(); ++k) {
        const auto cells = split(rows[k], '\t');
        if (cells.size() >= 5 && cells[4] == "in_corridor") in_corridor.emplace_back(cells[0], cells[2] == "down" ? 'd' : 'u');
      }
    }
    const std::set<std::string> corridor_ids = member_ids(solve.corridor.graph);
    for (const testkit::SolveFacts::Case& kase : solve.cases) {
      SolveFacts s;
      // FuSION 1's case name: a positional line's one free case is the case its prefix names (C4 note).
      const std::string prefix = prefix_of.count(solve.step) ? prefix_of[solve.step] : std::string();
      s.name = c.fusion1.solve.size() == 1 && solves_left == 1 ? c.fusion1.solve.begin()->first
               : prefix == "temp_wo"                         ? "out"
               : prefix == "temp_w"                          ? "in"
                                                             : kase.name;
      s.step = solve.step;
      s.status = kase.status;
      s.frontier = kase.frontier;
      for (const testkit::Assignment& a : kase.witnesses) s.witnesses.emplace_back(a);
      s.spec = kase.spec;
      s.corridor = solve.corridor;
      const fs::path case_files = run_dir / core::path_from_utf8(solve.step) / kase.name;
      if (fs::exists(case_files / "search_log.tsv", ec)) {
        const auto rows = lines_of(fusiontest::slurp(case_files / "search_log.tsv"));
        const auto header = rows.empty() ? std::vector<std::string>{} : split(rows[0], '\t');
        const auto column = [&](const char* h) {
          return static_cast<std::size_t>(std::find(header.begin(), header.end(), h) - header.begin());
        };
        const std::size_t cn = column("n"), ce = column("e"), co = column("outcome");
        for (std::size_t k = 1; k < rows.size(); ++k) {
          const auto cells = split(rows[k], '\t');
          if (cells.size() <= std::max({cn, ce, co})) continue;
          s.answers.emplace_back(Point{std::atoi(cells[cn].c_str()), std::atoi(cells[ce].c_str())}, outcome_of(cells[co]));
        }
      }
      if (fs::exists(case_files / "case.json", ec)) {
        const Json cj = read_json(case_files / "case.json");
        const Json used = cj.value("box", Json::object()).value("used", Json::object());
        if (used.contains("n") && used.contains("e")) {
          s.box_lo = {used["n"][0].get<int>(), used["e"][0].get<int>()};
          s.box_hi = {used["n"][1].get<int>(), used["e"][1].get<int>()};
        }
        const Json fam = cj.value("families", Json::object());
        if (fam.contains("coexpression") && fam["coexpression"].is_object()) s.coexpression = fam["coexpression"].value("pairs", 0);
        if (fam.contains("frozen") && fam["frozen"].is_object()) s.frozen = fam["frozen"].value("edges", 0);
        s.seconds = cj.value("wall_s", 0.0);
      }
      if (const auto it = c.fusion1.solve.find(s.name); it != c.fusion1.solve.end() && !b.write_dir.empty()) {
        const fs::path dat = b.write_dir / it->second.dat.filename();   // written natively (01.C11)
        if (fs::exists(dat, ec)) s.dat = fusiontest::slurp(dat);
      }
      s.up_down = observed_counts(in_corridor, s.spec, corridor_ids);
      f.solves.push_back(std::move(s));
    }
  }
  if (!f.corridor) f.missing[Stage::corridor] = "the run wrote no solve, so its corridor is not known";
  return f;
}

// ---------------------------------------------------------------- signals for the predicates

namespace {

struct ValueScan { bool near = false, prefix = false, repeats = false, cr_or_bom = false; };

// A two-column value file as FuSION 1 read it, against 2.0's grammar and doubles (XD-14, XD-23 to XD-25).
ValueScan scan_values(const fs::path& file, std::optional<std::pair<double, double>> thresholds) {
  ValueScan v;
  std::error_code ec;
  if (!fs::is_regular_file(file, ec)) return v;
  const std::string bytes = fusiontest::slurp(file);
  if (bytes.starts_with("\xEF\xBB\xBF")) v.cr_or_bom = true;
  for (std::size_t k = 0; k < bytes.size(); ++k)
    if (bytes[k] == '\r' && (k + 1 == bytes.size() || bytes[k + 1] != '\n')) v.cr_or_bom = true;
  std::set<std::string> seen;
  for (const std::string& line : lines_of(bytes)) {
    const auto w = words(line);
    if (w.size() < 2) continue;
    if (!seen.insert(w[0]).second) v.repeats = true;
    const core::Decimal d = core::parse_decimal(w[1]);
    char* end = nullptr;
    (void)std::strtod(w[1].c_str(), &end);   // FuSION 1's stream extraction stops where the number does
    if (d.status != core::DecimalStatus::ok && end != w[1].c_str() && *end != '\0') v.prefix = true;
    if (d.status == core::DecimalStatus::ok && thresholds) {
      const auto cls = [](double x, double up, double down) { return x >= up ? 1 : x <= down ? -1 : 0; };
      const double fv = static_cast<float>(d.value), fu = static_cast<float>(thresholds->first),
                   fd = static_cast<float>(thresholds->second);
      if (cls(d.value, thresholds->first, thresholds->second) != cls(fv, fu, fd)) v.near = true;
    }
  }
  return v;
}

}  // namespace

Signals signals(const Case& c, const BatchView& b, const std::vector<Arm>& arms, const Facts& f1,
                const std::vector<const Facts*>& f2, const std::optional<fs::path>& data_root) {
  Signals s;
  for (const BatchLine& l : b.lines) s.commands.insert(l.command);
  std::vector<fs::path> raw_files;
  for (const BatchLine* mff : b.all("mff"))
    if (flag(*mff, "tool_written") == std::optional<bool>(false)) s.raw_merge = true;
  for (const BatchLine* rgx : b.all("rgx"))
    if (flag(*rgx, "tool_written") == std::optional<bool>(false)) s.single_kgml_read = true;
  // A raw merge's KGMLs, as FuSION 1 merged them: its shortlist, through the case's path map.
  const std::optional<fs::path> artefacts = [&]() -> std::optional<fs::path> {
    if (c.tier == Tier::t1 || !data_root) return std::nullopt;
    return *data_root / "reference" / c.id;
  }();
  if (s.raw_merge && c.fusion1.shortlist && artefacts && data_root)
    for (const std::string& line : testkit::f1::read_ids(*artefacts / *c.fusion1.shortlist))
      raw_files.push_back(core::path_from_utf8(apply_path_map(line, c.path_map, fs::path(), data_root)));
  if (raw_files.size() == 1) s.single_kgml_read = true;
  for (const fs::path& kgml : raw_files) {
    pugi::xml_document doc;
    if (!doc.load_file(kgml.c_str())) continue;
    std::map<std::string, int> boxes;
    for (pugi::xml_node e : doc.child("pathway").children("entry")) {
      if (std::string(e.attribute("type").value()) == "group") continue;
      std::set<std::string> ids;
      for (const std::string& id : words(e.attribute("name").value())) ids.insert(strip_colons(id));
      for (const std::string& id : ids) ++boxes[id];
    }
    if (raw_files.size() > 1 && std::any_of(boxes.begin(), boxes.end(), [](const auto& kv) { return kv.second > 1; }))
      s.pathway_id_in_two_boxes = true;
    for (pugi::xml_node r : doc.child("pathway").children("relation"))
      if (std::string(r.attribute("type").value()) == "maplink") s.kgml_maplink = true;
  }

  if (f1.merged) {
    std::map<std::string, int> nodes_of;
    for (const auto& n : f1.merged->nodes)
      if (!testkit::is_complex_key(n.key))
        for (const std::string& m : n.members) ++nodes_of[m];
    s.id_in_several_nodes = std::any_of(nodes_of.begin(), nodes_of.end(), [](const auto& kv) { return kv.second > 1; });
    for (std::size_t k = 1; k < f1.merged->edges.size(); ++k)
      if (f1.merged->edges[k].key == f1.merged->edges[k - 1].key) s.fusion1_graph_duplicates = true;
  }
  if (f1.corridor) {
    const CanonGraph& g = f1.corridor->graph;
    // XD-17: an edge that 2.0's corridor searches never follow (05.C2), as FuSION 1's corridor holds it.
    const auto& sources = f1.corridor->sources;
    const auto& targets = f1.corridor->targets;
    for (const auto& e : g.edges)
      if (e.key.src == e.key.tgt || std::find(sources.begin(), sources.end(), e.key.tgt) != sources.end() ||
          std::find(targets.begin(), targets.end(), e.key.src) != targets.end())
        s.fusion1_corridor_trimmable_edge = true;
    std::set<std::string> touched;
    for (const auto& e : g.edges) touched.insert({e.key.src, e.key.tgt});
    for (const auto& n : g.nodes) {
      if (touched.contains(n.key) || testkit::is_complex_key(n.key)) continue;
      for (const auto& group : g.nodes)
        if (testkit::is_complex_key(group.key) &&
            std::includes(group.members.begin(), group.members.end(), n.members.begin(), n.members.end()))
          s.fusion1_corridor_isolated_members = true;
    }
  }

  // The value files: cudf's, against its thresholds, and gsea's.
  if (const BatchLine* cudf = b.first("cudf"))
    if (const auto values = path(*cudf, "values")) {
      std::optional<std::pair<double, double>> thresholds;
      if (const auto up = real(*cudf, "up"), down = real(*cudf, "down"); up && down) thresholds = std::pair{*up, *down};
      const ValueScan v = scan_values(*values, thresholds);
      s.value_near_threshold = v.near;
      s.value_prefix_token = v.prefix;
      s.value_repeats_id = v.repeats;
      s.value_lone_cr_or_bom = v.cr_or_bom;
    }
  if (const BatchLine* gsea = b.first("gsea"))
    if (const auto values = path(*gsea, "values")) {
      const ValueScan v = scan_values(*values, std::nullopt);
      s.value_prefix_token = s.value_prefix_token || v.prefix;
      s.value_repeats_id = s.value_repeats_id || v.repeats;
      s.value_lone_cr_or_bom = s.value_lone_cr_or_bom || v.cr_or_bom;
    }

  for (const BatchLine* z : b.all("pathz3")) {
    if ((given(*z, "coexpression_csv") || given(*z, "frozen_threshold")) && !given(*z, "constraints"))
      s.pathz3_without_constraints_flag = true;
    if (f1.corridor_size && f1.corridor_size->second >= 0) {   // S5.09: FuSION 1's width ceil(log2(|E|+1))
      int width = 0;
      while ((1LL << width) < static_cast<long long>(f1.corridor_size->second) + 1) ++width;
      if (const auto e = integer(*z, "edge_max"); e && *e >= (1LL << width)) s.edge_bound_wraps = true;
    }
  }

  for (const Facts* side : f2)
    if (side && side->gsea && side->gsea->zero_weight_draws.value_or(0) > 0) s.gsea_zero_draw = true;
  std::vector<const GseaFacts*> tables;
  if (f1.gsea) tables.push_back(&*f1.gsea);
  for (const Facts* side : f2)
    if (side && side->gsea) tables.push_back(&*side->gsea);
  for (const GseaFacts* t : tables) {
    std::vector<double> nes;
    for (const auto& r : t->rows)
      if (std::isfinite(r.nes) && r.genes > 0) nes.push_back(std::fabs(r.nes));
    std::sort(nes.begin(), nes.end());
    for (std::size_t k = 1; k < nes.size(); ++k)
      if (nes[k] == nes[k - 1]) s.gsea_equal_nes = true;
  }

  std::set<std::string> excluded;
  if (const BatchLine* fb = b.first("fb_rch"))
    for (const std::string& id : list(*fb, "exclude")) excluded.insert(id);
  std::vector<const testkit::CanonNode*> complexes;
  for (const CanonGraph* g : {f1.corridor ? &f1.corridor->graph : nullptr, f1.merged ? &*f1.merged : nullptr})
    if (g)
      for (const auto& n : g->nodes)
        if (testkit::is_complex_key(n.key)) complexes.push_back(&n);
  for (const Arm& arm : arms) {
    Signals::Solve& out = s.solves[arm.name];
    const auto sf = std::find_if(f1.solves.begin(), f1.solves.end(), [&](const SolveFacts& x) { return x.name == arm.name; });
    if (arm.name == "in" && f1.down)
      for (const std::string& id : arm.fsc)
        if (std::find(f1.down->begin(), f1.down->end(), id) != f1.down->end()) out.fsc_down = true;
    if (f1.exceptions)
      for (const std::string& id : arm.fsc)
        if (std::find(f1.exceptions->begin(), f1.exceptions->end(), id) == f1.exceptions->end()) out.fsc_not_in_exceptions = true;
    std::set<std::string> avoided = excluded;
    if (sf != f1.solves.end()) avoided.insert(sf->spec.avoid.begin(), sf->spec.avoid.end());
    if (arm.name == "out") avoided.insert(arm.fsc.begin(), arm.fsc.end());
    for (const testkit::CanonNode* cx : complexes)
      for (const std::string& id : avoided)
        if (std::binary_search(cx->members.begin(), cx->members.end(), id)) out.excluded_or_avoided_in_complex = true;
    if (sf == f1.solves.end()) continue;
    const std::string source = f1.corridor && !f1.corridor->sources.empty() ? f1.corridor->sources.front() : "";
    const std::string target = f1.corridor && !f1.corridor->targets.empty() ? f1.corridor->targets.front() : "";
    const auto key_of = [&](const std::string& rep) { return f1.corridor ? node_named(f1.corridor->graph, rep).value_or(rep) : rep; };
    for (const testkit::f1::PointRecord& r : sf->records) {
      if (!r.witness) continue;
      for (const testkit::f1::EdgeKey& e : r.witness->relaxed_edges) {
        if (e.src_rep == e.tgt_rep) out.fusion1_relaxed.insert("self_loop");
        if (key_of(e.tgt_rep) == source) out.fusion1_relaxed.insert("into_source");
        if (key_of(e.src_rep) == target) out.fusion1_relaxed.insert("out_of_target");
      }
      const int n = static_cast<int>(r.witness->relaxed_nodes.size()), e = static_cast<int>(r.witness->relaxed_edges.size());
      if (n < r.point.n || e < r.point.e) out.fusion1_witness_below_point = true;
    }
  }
  return s;
}

}  // namespace fusion::reftest
