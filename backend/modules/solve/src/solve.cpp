// The step (06, "The driver"): the cases built once from one corridor (S4.20), solved in parallel
// (06.C4) under the per-query limits, the budget (06.C3) and Stop (A1), with the extra solutions
// (06.C11), the search log, the events (A2) and each case's files (S5.30). FuSION 1's handler is
// main.cc:4365-5073. Nothing is static: everything lives inside one call (S5.29).
#include "fusion/solve/solve.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

#include "fusion/corridor/corridor.hpp"
#include "internal.hpp"

namespace fusion::solve {

namespace {

using Clock = std::chrono::steady_clock;

std::string fixed(double value, int decimals) {           // locale-free
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
  return std::string(buffer, result.ptr);
}

std::string seconds_text(double s) { return fixed(s, s < 10 ? 3 : 1) + " s"; }

std::string duration_text(std::chrono::milliseconds ms) {
  const auto count = ms.count();
  if (count != 0 && count % 3'600'000 == 0) return std::to_string(count / 3'600'000) + "h";
  if (count != 0 && count % 60'000 == 0) return std::to_string(count / 60'000) + "min";
  if (count != 0 && count % 1000 == 0) return std::to_string(count / 1000) + "s";
  return std::to_string(count) + "ms";
}

std::string point_text(Point p) { return "n=" + std::to_string(p.n) + " e=" + std::to_string(p.e); }

std::string plural(std::size_t count, const char* word) {
  return std::to_string(count) + " " + word + (count == 1 ? "" : "s");
}

core::Json box_json(Box box) {
  return core::Json{{"n", {box.lo.n, box.hi.n}}, {"e", {box.lo.e, box.hi.e}}};
}

// What ends the cases early: Stop (A1) or the budget (06.C3). The monitor interrupts every running
// check again until its thread returns, because Z3_interrupt reaches only a check under way.
struct Control {
  enum class Ending : std::uint8_t { none, stop, budget, failure };
  std::mutex mutex;
  std::condition_variable wake;
  bool done = false;
  Ending ending = Ending::none;
  std::vector<CaseSolver*> running;
  std::mutex output;                                       // events, files and console from case threads

  void end(Ending why) {
    {
      std::lock_guard lock(mutex);
      if (ending == Ending::none) ending = why;
    }
    wake.notify_all();
  }
  Ending current() {
    std::lock_guard lock(mutex);
    return ending;
  }
};

void monitor(Control& control, std::chrono::milliseconds budget, Clock::time_point start) {
  std::unique_lock lock(control.mutex);
  const auto ending = [&] { return control.done || control.ending != Control::Ending::none; };
  if (budget.count() > 0) {
    if (!control.wake.wait_until(lock, start + budget, ending)) control.ending = Control::Ending::budget;
  } else {
    control.wake.wait(lock, ending);
  }
  while (!control.done) {
    for (CaseSolver* solver : control.running) solver->interrupt();
    control.wake.wait_for(lock, std::chrono::milliseconds(20), [&] { return control.done; });
  }
}

// The search's view of one case: Z3 answers through its CaseSolver, and every query reported as
// an event and a console line (A2, S5.31).
class CaseRun final : public Oracle, public SearchObserver {
public:
  CaseRun(CaseSolver& solver, AnswerCache& cache, Control& control, core::Context& ctx, Case kase)
      : solver_(solver), cache_(cache), control_(control), ctx_(ctx), case_(name(kase)) {}

  Answer ask(Point p, Phase phase) override { return solver_.check(p, phase); }

  void on_started(Point p, Phase phase) override {         // solve.check.started (09.C10)
    std::lock_guard lock(control_.output);
    ctx_.events.emit("solve.check.started",
                     core::Json{{"case", case_},
                                {"check", cache_.answers().size() + 1},
                                {"phase", name(phase)},
                                {"n", p.n},
                                {"e", p.e}});
  }

  void on_answer(const Answer& a) override { report(a); }

  void on_point(Point p) override {
    std::lock_guard lock(control_.output);
    ctx_.events.emit("solve.point", core::Json{{"case", case_}, {"n", p.n}, {"e", p.e}});
  }

  // One query's event and, for a Z3 answer, its console line (S5.31; the reason of an unknown,
  // as FuSION 1's TIMEOUT and UNDEFINED lines gave it, GraphManagerNew.cc:29475-29523).
  void report(const Answer& a) {
    core::Json fields{{"case", case_},
                      {"check", a.check},
                      {"phase", name(a.phase)},
                      {"n", a.point.n},
                      {"e", a.point.e},
                      {"source", name(a.source)},
                      {"outcome", name(a.outcome)},
                      {"wall_s", a.wall_s},
                      {"cpu_s", a.cpu_s},
                      {"reason", a.reason}};
    if (a.model) {
      fields["model_n"] = a.model->n;
      fields["model_e"] = a.model->e;
    }
    std::lock_guard lock(control_.output);
    ctx_.events.emit("solve.check", std::move(fields));
    std::string line = "[solve " + case_ + "] check " + point_text(a.point) + " (" + std::string(name(a.phase)) +
                       "): " + std::string(name(a.outcome));
    if (a.source != Source::z3) {
      line += a.source == Source::model ? ", from a model's counts" : ", from the cache";
      if (a.outcome != Outcome::sat && a.outcome != Outcome::unsat) line += " (asked before)";
      ctx_.log.debug(line);
      return;
    }
    line += a.outcome == Outcome::timeout || a.outcome == Outcome::undefined ? " after " : " in ";
    line += seconds_text(a.wall_s);
    if (a.model) line += "; model " + point_text(*a.model);
    if (!a.reason.empty() && a.outcome != Outcome::stopped) line += " (z3 reason: " + a.reason + ")";
    ctx_.log.info(line);
  }

private:
  CaseSolver& solver_;
  AnswerCache& cache_;
  Control& control_;
  core::Context& ctx_;
  std::string case_;
};

// The modelling rules the outputs state (S5.12, S5.14, S5.16).
core::Json rules() {
  return core::Json::array({"every selected path crosses a DE gene before the target (S5.14)",
                            "an absent up-regulated gene is relaxed; an absent down-regulated gene is not (S5.12)",
                            "source and target are exempt from node relaxation (S5.12)",
                            "frozen: score > threshold; coexpression: score > threshold (S5.15, S5.16)"});
}

core::Json case_json(const CaseResult& r, const Problem& p, const Settings& settings) {
  core::Json families = core::Json::object();
  families["coexpression"] = p.families.coexpression
                                 ? core::Json{{"threshold", *p.families.coexpression_threshold},
                                              {"pairs", p.coexpressed.size()}}
                                 : core::Json(nullptr);
  families["frozen"] = p.families.frozen ? core::Json{{"threshold", *p.families.frozen_threshold},
                                                      {"edges", p.frozen.size()},
                                                      {"unscored", p.unscored}}
                                         : core::Json(nullptr);
  core::Json fsc = core::Json::array();
  for (const std::string& key : p.fsc) fsc.push_back(key);
  return core::Json{
      {"case", name(r.kase)},
      {"fsc", fsc},
      {"status", name(r.status)},
      {"message", r.message},
      {"fingerprint", r.stats.fingerprint},
      {"encoder", 1},
      {"z3", z3_version()},
      {"seed", p.seed},
      {"box", {{"requested", box_json(p.requested)}, {"used", box_json(p.box)}}},
      {"encoded", {{"nodes", r.stats.nodes}, {"edges", r.stats.edges}, {"counted", r.stats.counted}, {"de", r.stats.de}}},
      {"families", families},
      {"limits",
       {{"timeout_ms", settings.timeout.count()},
        {"incremental_timeout_ms", settings.incremental_timeout.count()},
        {"budget_ms", settings.budget.count()},
        {"rlimit", settings.rlimit}}},
      {"checks", {{"z3", r.stats.z3}, {"cache", r.stats.cache}, {"model", r.stats.model}}},
      {"wall_s", r.stats.wall_s},
      {"cpu_clock_s", r.stats.cpu_clock_s},
      {"rules", rules()}};
}

// observations.tsv (S5.12, S5.17) and search_log.tsv (S5.24): n before e throughout.
std::string observations_tsv(const std::vector<ObservationRow>& rows) {
  std::string out = "gene\tsymbol\tobserved\trole\n";
  for (const ObservationRow& row : rows)
    out += row.gene + "\t" + row.symbol + "\t" + (row.observed == 'd' ? "down" : "up") + "\t" +
           std::string(name(row.role)) + "\n";
  return out;
}

std::string search_log_tsv(const std::vector<Answer>& log) {
  std::string out = "check\tphase\tn\te\tsource\toutcome\twall_s\tcpu_s\ttimeout_ms\trlimit\tmodel_n\tmodel_e\treason\n";
  for (const Answer& a : log) {
    out += std::to_string(a.check) + "\t" + std::string(name(a.phase)) + "\t" + std::to_string(a.point.n) + "\t" +
           std::to_string(a.point.e) + "\t" + std::string(name(a.source)) + "\t" + std::string(name(a.outcome)) + "\t" +
           core::format_number(a.wall_s) + "\t" + core::format_number(a.cpu_s) + "\t" +
           std::to_string(a.timeout.count()) + "\t" + std::to_string(a.rlimit) + "\t" +
           (a.model ? std::to_string(a.model->n) : "") + "\t" + (a.model ? std::to_string(a.model->e) : "") + "\t" +
           a.reason + "\n";
  }
  return out;
}

void write_file(core::Context& ctx, const std::string& path, const std::string& content) {
  core::OutputFile file = ctx.files.create(path);
  file.out() << content;
  file.commit();
}

// Every requested case writes its files whatever its status, so a missing file never stands for
// "no solution" (S6.21); FuSION 1 wrote no solution files when no point was found (main.cc:4951-4955).
void write_case_files(core::Context& ctx, Control& control, const CaseResult& r, const Problem& p,
                      const Settings& settings) {
  const std::string dir = std::string(name(r.kase)) + "/";
  const std::string json = case_json(r, p, settings).dump(1) + "\n";
  std::lock_guard lock(control.output);
  write_file(ctx, dir + "case.json", json);
  write_file(ctx, dir + "observations.tsv", observations_tsv(r.observations));
  write_file(ctx, dir + "search_log.tsv", search_log_tsv(r.log));
}

std::string observation_names(const Problem& p) {
  std::string out;
  for (std::size_t k = 0; k < p.counted.size() && k < 5; ++k) out += (k ? ", " : "") + p.labels[p.counted[k]];
  if (p.counted.size() > 5) out += ", ...";
  return out;
}

// The case's opening lines (S5.31): the FSC, the counted observations, the clamps, and the families.
void announce(core::Context& ctx, const Problem& p, const std::string& tag) {
  std::string line = tag;
  if (!p.fsc.empty() && p.kase != Case::free) {
    line += " FSC";
    for (const std::string& key : p.fsc) {
      const auto it = std::find_if(p.nodes.begin(), p.nodes.end(), [&](const network::NodeKey& k) { return k.value == key; });
      const std::string label = it == p.nodes.end() ? key : p.labels[static_cast<std::size_t>(it - p.nodes.begin())];
      line += " " + (label == key ? key : label + " (" + key + ")");
    }
    line += p.kase == Case::in ? " present on a path;" : " absent, with its complexes;";
  }
  line += " " + plural(p.counted.size(), "counted observation");
  if (!p.counted.empty()) line += " (" + observation_names(p) + ")";
  if (p.box.hi.n != p.requested.hi.n) line += "; node bound " + std::to_string(p.requested.hi.n) + " -> " + std::to_string(p.box.hi.n);
  if (p.box.hi.e != p.requested.hi.e) line += "; edge bound " + std::to_string(p.requested.hi.e) + " -> " + std::to_string(p.box.hi.e);
  ctx.log.info(line);
  std::string encoded = tag + " encoded " + plural(p.nodes.size(), "node") + ", " + plural(p.edges.size(), "edge");
  if (p.families.coexpression)
    encoded += "; coexpression " + plural(p.coexpressed.size(), "pair") + " (> " +
               core::format_number(*p.families.coexpression_threshold) + ")";
  if (p.families.frozen)
    encoded += "; frozen " + plural(p.frozen.size(), "edge") + " (> " + core::format_number(*p.families.frozen_threshold) + ")";
  ctx.log.info(encoded);
}

CaseStats stats_of(const Problem& p) {
  CaseStats s;
  s.nodes = static_cast<std::uint32_t>(p.nodes.size());
  s.edges = static_cast<std::uint32_t>(p.edges.size());
  s.counted = static_cast<std::uint32_t>(p.counted.size());
  s.de = static_cast<std::uint32_t>(std::count_if(p.regulation.begin(), p.regulation.end(),
                                                  [](expression::Regulation r) { return r != expression::Regulation::none; }));
  s.coexpression_pairs = static_cast<std::uint32_t>(p.coexpressed.size());
  s.frozen_edges = static_cast<std::uint32_t>(p.frozen.size());
  s.unscored_edges = p.unscored;
  s.cpu_clock_s = detail::thread_cpu_resolution();
  s.fingerprint = "sha256:" + fingerprint(p).hex();
  return s;
}

void finish(core::Context& ctx, Control& control, CaseResult& r, const Settings& settings, Clock::time_point start,
            const std::string& tag) {
  r.stats.wall_s = std::chrono::duration<double>(Clock::now() - start).count();
  for (const Answer& a : r.log) {
    if (a.source == Source::z3) ++r.stats.z3;
    else if (a.source == Source::model) ++r.stats.model;
    else ++r.stats.cache;
  }
  write_case_files(ctx, control, r, *r.problem, settings);
  const auto certified = std::count_if(r.frontier.points.begin(), r.frontier.points.end(),
                                       [](const FrontierPoint& f) { return f.certified; });
  std::lock_guard lock(control.output);
  ctx.events.emit("solve.case.finished",
                  core::Json{{"case", name(r.kase)},
                             {"status", name(r.status)},
                             {"points", r.frontier.points.size()},
                             {"certified", certified},
                             {"undecided", r.frontier.undecided.size()},
                             {"checks", r.log.size()},
                             {"wall_s", r.stats.wall_s}});
  const Box box = r.problem->box;
  switch (r.status) {
    case CaseStatus::infeasible:
      ctx.log.info(tag + " infeasible: " + r.message);
      return;
    case CaseStatus::no_de_gene:
      ctx.log.info("[solve] " + r.message);
      return;
    case CaseStatus::no_point:
      ctx.log.info(tag + " no point within n<=" + std::to_string(box.hi.n) + ", e<=" + std::to_string(box.hi.e) + "; " +
                   plural(r.log.size(), "query") + ", " + std::to_string(r.stats.z3) + " to Z3; " +
                   seconds_text(r.stats.wall_s));
      return;
    default:
      break;
  }
  for (const FrontierPoint& f : r.frontier.points) {
    std::string line = tag + " point " + point_text(f.point);
    if (f.certified) {
      line += ", certified";
    } else {
      line += ", not certified:";
      for (Point u : f.undecided_below) line += " " + point_text(u);
      line += f.undecided_below.empty() ? " a lower neighbour is not proven unsat" : " undecided";
    }
    if (f.found_at) line += "; found at " + point_text(*f.found_at) + " (06.C20)";
    ctx.log.info(line);
  }
  const std::string summary = plural(r.frontier.points.size(), "point") + ", " + std::to_string(certified) +
                              " certified; " + plural(r.log.size(), "query") + ", " + std::to_string(r.stats.z3) +
                              " to Z3, " + std::to_string(r.stats.model) + " from a model; " +
                              seconds_text(r.stats.wall_s);
  if (r.status == CaseStatus::complete) {
    ctx.log.info(tag + " complete: " + summary);
  } else if (r.status == CaseStatus::budget_exhausted) {
    ctx.log.info(tag + " budget " + duration_text(settings.budget) + " used: " + summary + "; " +
                 plural(r.frontier.undecided.size(), "undecided cell") + "; resumable");
  } else {
    ctx.log.info(tag + " stopped: " + summary + "; resumable");
  }
}

// One case, from its problem to its files (06, "The driver").
CaseResult run_case(std::variant<Problem, Skipped>& built, const Settings& settings, Control& control,
                    core::Context& ctx) {
  const auto start = Clock::now();
  CaseResult r{};
  if (auto* skipped = std::get_if<Skipped>(&built)) {        // S5.11, 06.C15: no check is made
    r.problem = skipped->problem;
    r.kase = r.problem->kase;
    r.status = skipped->status;
    r.message = skipped->message;
  } else {
    r.problem = std::make_shared<const Problem>(std::move(std::get<Problem>(built)));
    r.kase = r.problem->kase;
  }
  const Problem& p = *r.problem;
  const std::string tag = "[solve " + std::string(name(r.kase)) + "]";
  r.observations = p.observations;
  r.stats = stats_of(p);
  if (std::holds_alternative<Skipped>(built)) {
    finish(ctx, control, r, settings, start, tag);
    return r;
  }

  {
    std::lock_guard lock(control.output);
    announce(ctx, p, tag);
    core::Json fsc = core::Json::array();
    for (const std::string& key : p.fsc) fsc.push_back(key);
    ctx.events.emit("solve.case.started", core::Json{{"case", name(r.kase)},
                                                     {"fsc", fsc},
                                                     {"nodes", p.nodes.size()},
                                                     {"edges", p.edges.size()},
                                                     {"counted", p.counted.size()},
                                                     {"box", box_json(p.box)},
                                                     {"resumed_answers", 0}});
  }

  CaseSolver solver(p, settings, ctx);
  {
    std::lock_guard lock(control.mutex);
    control.running.push_back(&solver);
    if (control.ending != Control::Ending::none) solver.interrupt();
  }
  struct Unregister {
    Control& control;
    CaseSolver* solver;
    ~Unregister() {
      std::lock_guard lock(control.mutex);
      control.running.erase(std::remove(control.running.begin(), control.running.end(), solver), control.running.end());
    }
  } unregister{control, &solver};

  AnswerCache cache(p.box);
  CaseRun run(solver, cache, control, ctx, r.kase);
  const std::vector<Point> points = pareto_search(p.box, cache, run, run);
  r.log.assign(cache.answers().begin(), cache.answers().end());
  bool stopped = std::any_of(r.log.begin(), r.log.end(), [](const Answer& a) { return a.outcome == Outcome::stopped; });
  r.frontier = assemble(p.box, cache.answers(), solver.witnesses(), points);

  // 06.C11: up to `solutions` witnesses per frontier point, blocking those found so far. FuSION 1
  // re-solved every point at least twice for count = explore = 1 (main.cc:4964-5062, S5.26).
  if (settings.solutions > 1 && !stopped) {
    for (FrontierPoint& f : r.frontier.points) {
      std::vector<Witness> seen{solver.witnesses()[f.witness]};
      while (static_cast<int>(seen.size()) < settings.solutions) {
        const std::optional<Witness> next = solver.next_solution(f.point, seen);
        Answer a = solver.last_answer();
        a.check = static_cast<std::uint32_t>(r.log.size() + 1);
        r.log.push_back(a);
        run.report(a);
        if (!next) break;
        seen.push_back(*next);
        f.more.push_back(*a.witness);
      }
      if (r.log.back().outcome == Outcome::stopped) {
        stopped = true;
        break;
      }
    }
  }
  r.witnesses.assign(solver.witnesses().begin(), solver.witnesses().end());

  if (stopped) {
    const Control::Ending why = control.current();
    r.status = why == Control::Ending::budget ? CaseStatus::budget_exhausted : CaseStatus::stopped;
    r.message = r.status == CaseStatus::budget_exhausted ? "the budget of " + duration_text(settings.budget) + " ran out"
                                                         : "stopped by request";
  } else {
    r.status = r.frontier.points.empty() ? CaseStatus::no_point : CaseStatus::complete;
  }
  r.stats.cpu_s = 0;
  for (const Answer& a : r.log) r.stats.cpu_s += a.cpu_s;
  finish(ctx, control, r, settings, start, tag);
  return r;
}

void validate(const Settings& settings) {
  const Box b = settings.box;
  if (b.lo.n < 0 || b.lo.e < 0 || b.lo.n > b.hi.n || b.lo.e > b.hi.e)
    throw core::UsageError("batch.bad_argument", "the box needs 0 <= node_min <= node_max and 0 <= edge_min <= edge_max",
                           detail::where({}, 0, "node_min"));
  if (settings.solutions < 1)
    throw core::UsageError("batch.bad_argument", "solutions must be at least 1 (06.C11)", detail::where({}, 0, "solutions"));
  if (!settings.points.empty())
    throw core::Failure("solve.unsupported", "points= is not ported yet (06.C9)", detail::where({}, 0, "points"));
  if (settings.answers_from)
    throw core::Failure("solve.unsupported", "answers_from= is not ported yet (06.C12)",
                        detail::where({}, 0, "answers_from"));
}

}  // namespace

core::Outcome<Results> solve_cases(std::vector<std::variant<Problem, Skipped>> cases, const Settings& settings,
                                   core::Context& ctx) {
  validate(settings);
  const auto start = Clock::now();
  Control control;
  Results results;
  results.cases.resize(cases.size());
  std::exception_ptr failure;
  std::mutex failure_mutex;

  std::thread watcher(monitor, std::ref(control), settings.budget, start);
  {
    core::StopCallback on_stop(ctx.stop, [&control] { control.end(Control::Ending::stop); });
    std::atomic<std::size_t> next{0};
    const auto work = [&] {
      for (std::size_t i = next++; i < cases.size(); i = next++) {
        try {
          results.cases[i] = run_case(cases[i], settings, control, ctx);
        } catch (...) {
          std::lock_guard lock(failure_mutex);
          if (!failure) failure = std::current_exception();
          control.end(Control::Ending::failure);
        }
      }
    };
    const std::size_t workers = settings.threads == 0 ? cases.size() : std::min<std::size_t>(settings.threads, cases.size());
    std::vector<std::thread> threads;
    for (std::size_t w = 0; w < workers; ++w) threads.emplace_back(work);
    for (std::thread& t : threads) t.join();
  }
  {
    std::lock_guard lock(control.mutex);
    control.done = true;
  }
  control.wake.notify_all();
  watcher.join();
  if (failure) std::rethrow_exception(failure);

  bool partial = false;
  for (const CaseResult& r : results.cases)
    partial = partial || r.status == CaseStatus::stopped || r.status == CaseStatus::budget_exhausted;
  const double wall = std::chrono::duration<double>(Clock::now() - start).count();
  if (control.current() == Control::Ending::stop) {
    std::size_t checks = 0;
    for (const CaseResult& r : results.cases) checks += r.log.size();
    ctx.log.info("[solve] stopped by request after " + plural(checks, "check") + "; resumable");
  }
  ctx.log.info("[solve] done in " + seconds_text(wall) + " wall");   // FuSION 1's [pathz3 timing] (main.cc:5069-5072)
  return core::Outcome<Results>{std::move(results), std::nullopt, partial};
}

core::Outcome<Results> solve(const Inputs& inputs, const Settings& settings, core::Context& ctx) {
  validate(settings);
  const std::vector<Case> cases = requested_cases(settings);
  if (cases.empty()) throw core::UsageError("batch.bad_argument", "no case to solve (06.C10)", detail::where({}, 0, "cases"));
  const CaseGraph graph = read_corridor(inputs.corridor, inputs.maps);
  std::string names;
  for (Case c : cases) names += (names.empty() ? "" : ", ") + std::string(name(c));
  ctx.log.info("[solve] case" + std::string(cases.size() == 1 ? " " : "s ") + names + " on " +
               graph.nodes[graph.source].label + " -> " + graph.nodes[graph.target].label + ", bound " +
               std::to_string(graph.bound) + ": corridor " + plural(graph.nodes.size(), "node") + ", " +
               plural(graph.edges.size(), "edge"));
  const CaseSpec spec = bind_inputs(inputs, graph, settings, ctx);
  std::vector<std::variant<Problem, Skipped>> built;
  for (Case c : cases) {
    built.push_back(make_case(graph, spec, settings, c));
    const Problem& p = std::holds_alternative<Problem>(built.back()) ? std::get<Problem>(built.back())
                                                                     : *std::get<Skipped>(built.back()).problem;
    for (const auto& [code, text] : p.warnings) ctx.log.warning(code, "[solve " + std::string(name(c)) + "] " + text);
  }
  // S5.16: the encoded edges with no score can never be frozen; N and E are the same in every case.
  const Problem& first = std::holds_alternative<Problem>(built.front()) ? std::get<Problem>(built.front())
                                                                        : *std::get<Skipped>(built.front()).problem;
  if (first.families.frozen && first.unscored > 0)
    ctx.log.warning("solve.unscored_edges", plural(first.unscored, "encoded edge") + " of " +
                                                std::to_string(first.edges.size()) +
                                                " have no score, so they are never frozen (S5.16)");
  return solve_cases(std::move(built), settings, ctx);
}

// The saved solve's loader (D1, 06.C7) replays the journal, which comes with resume.
Results load_saved(const std::filesystem::path& step_dir) {
  throw core::Failure("solve.unsupported",
                      "loading a saved solve from " + core::to_utf8(step_dir) + " is not ported yet (06.C7)");
}

}  // namespace fusion::solve
