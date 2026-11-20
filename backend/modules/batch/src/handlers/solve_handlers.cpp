// The solve and its results: pathz3 (section 06), psp, pex and plot_pareto (section 07), and FuSION 1's
// fixed-name moves of their outputs (01.C11).
#include <algorithm>
#include <charconv>
#include <fstream>

#include "fusion/results/explain.hpp"
#include "fusion/results/legacy.hpp"
#include "fusion/results/pareto.hpp"
#include "fusion/results/publish.hpp"
#include "fusion/results/view.hpp"
#include "state.hpp"

namespace fusion::batch {

namespace {

// 06.C9: "n,e;n,e".
std::vector<solve::Point> parse_points(std::string_view text) {
  std::vector<solve::Point> points;
  while (!text.empty()) {
    const auto semi = text.find(';');
    const auto item = text.substr(0, semi);
    const auto comma = item.find(',');
    const auto number = [&](std::string_view t) {
      int v = 0;
      const auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
      if (ec != std::errc() || p != t.data() + t.size() || v < 0)
        throw core::Failure("batch.bad_value", "points: '" + std::string(item) + "' is not <n>,<e> (06.C9)");
      return v;
    };
    if (comma == std::string_view::npos)
      throw core::Failure("batch.bad_value", "points: '" + std::string(item) + "' is not <n>,<e> (06.C9)");
    points.push_back(solve::Point{number(item.substr(0, comma)), number(item.substr(comma + 1))});
    if (semi == std::string_view::npos) break;
    text.remove_prefix(semi + 1);
  }
  return points;
}

std::vector<std::string> node_list(const BoundArgs& args, std::string_view name, core::Context& ctx) {
  const auto p = path_arg(args, name);
  return p ? solve::read_node_list(ctx.files.input(*p)) : std::vector<std::string>{};
}
std::vector<solve::EdgeLine> edge_list(const BoundArgs& args, std::string_view name, core::Context& ctx) {
  const auto p = path_arg(args, name);
  return p ? solve::read_edge_list(ctx.files.input(*p)) : std::vector<solve::EdgeLine>{};
}

// S5.04: a positional line's connect pair, "<source> <target>" on its first line.
std::optional<std::pair<std::string, std::string>> connect_pair(const BoundArgs& args, core::Context& ctx) {
  const auto p = path_arg(args, "pairs");
  if (!p) return std::nullopt;
  std::ifstream in(ctx.files.input(*p), std::ios::binary);
  std::string a, b;
  if (!(in >> a >> b)) return std::nullopt;
  return std::make_pair(a, b);
}

}  // namespace

// pathz3 (S5.01 to S5.31, 06). FuSION 1 parsed 28 tokens and up to 5 optional ones and exited on an
// unknown graph (Linux/src/main.cc:4364-4560); 2.0 solves the cases of 06.C10 on one corridor and hands
// the results to 07's publish_solve (row 1.5); a stop publishes the partial results, then stops (A1).
core::Outcome<void> run_pathz3(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = run_maps(ws, ctx);
  const int graph = static_cast<int>(int_arg(args, "graph").value_or(0));
  const int corridor_id = graph == 0 ? ws.graphs.latest() : graph;
  const auto corridor = corridor_of(ws, corridor_id, "pathz3");
  const auto& info = st.corridors[corridor_id];
  const auto network = network_of(ws, info.network, "pathz3");
  network::BindingTable& bindings = bindings_for(ws, info.network, network, ctx);

  solve::Settings settings;
  settings.fsc = list_arg(args, "fsc");
  for (const auto& c : list_arg(args, "cases"))
    if (const auto kase = solve::parse_case(c)) settings.cases.push_back(*kase);
  settings.box.lo = solve::Point{static_cast<int>(int_arg(args, "node_min").value_or(0)),
                                 static_cast<int>(int_arg(args, "edge_min").value_or(0))};
  settings.box.hi = solve::Point{static_cast<int>(int_arg(args, "node_max").value_or(8)),
                                 static_cast<int>(int_arg(args, "edge_max").value_or(8))};
  if (given(args, "points")) settings.points = parse_points(*text_arg(args, "points"));
  settings.timeout = std::chrono::milliseconds(int_arg(args, "timeout").value_or(14'400'000));
  settings.incremental_timeout = std::chrono::milliseconds(int_arg(args, "incremental_timeout").value_or(3'600'000));
  settings.budget = std::chrono::milliseconds(int_arg(args, "budget").value_or(0));
  settings.rlimit = static_cast<std::uint64_t>(int_arg(args, "rlimit").value_or(0));
  settings.seed = static_cast<std::uint32_t>(int_arg(args, "seed").value_or(42));
  settings.solutions = static_cast<int>(int_arg(args, "solutions").value_or(1));
  settings.threads = static_cast<unsigned>(int_arg(args, "threads").value_or(0));
  const auto retry = text_or(args, "retry", "automatic");
  settings.retry = retry == "unknown" ? solve::Retry::unknown : retry == "none" ? solve::Retry::none : solve::Retry::automatic;
  if (given(args, "answers_from")) settings.answers_from = resolve_input(ws.batch_dir, *text_arg(args, "answers_from"));
  if (given(args, "constraints")) settings.families.enabled = bool_arg(args, "constraints");
  settings.families.coexpression_threshold = real_arg(args, "coexpression_threshold");
  settings.families.frozen_threshold = real_arg(args, "frozen_threshold");
  settings.dumps = ws.debug;
  settings.positional = args.positional();
  if (given(args, "bound")) settings.path_bound = static_cast<std::uint32_t>(*int_arg(args, "bound"));

  // The observations: the latest cudf's (01.C16), or a positional line's up and down lists, taken from
  // memory when cudf wrote them in this run and they are unchanged (01.C13).
  std::shared_ptr<const expression::Observations> observations = ws.observations;
  std::vector<expression::ObservationMatch> match = info.match;
  if (const auto up = path_arg(args, "up"), down = path_arg(args, "down"); up && down) {
    const auto a = ws.written.get(*up), b = ws.written.get(*down);
    const auto* oa = a ? std::get_if<std::shared_ptr<const expression::Observations>>(&*a) : nullptr;
    const auto* ob = b ? std::get_if<std::shared_ptr<const expression::Observations>>(&*b) : nullptr;
    if (oa && ob && *oa == *ob) {
      observations = *oa;
    } else {
      observations = std::make_shared<const expression::Observations>(
          expression::load_lists(ctx.files.input(*up), ctx.files.input(*down), maps.ids, st.interner, ctx));
    }
    if (observations != ws.observations) match = expression::match_observations(*observations, corridor->view());
  }
  if (!observations) observations = std::make_shared<const expression::Observations>();

  // S5.05: FuSION 1's relaxed lists are accepted and ignored, with a warning when they hold ids.
  for (const auto name : {"relaxed_nodes", "relaxed_edges"})
    if (const auto p = path_arg(args, name); p && !solve::read_node_list(ctx.files.input(*p)).empty())
      ctx.log.warning("solve.relaxed_ignored", std::string("pathz3: the ") + name + " list " + core::to_utf8(*p) +
                                                   " is ignored in FuSION 2.0 (S5.05)");

  const solve::CoexpressionTable* coexpression = nullptr;
  if (const auto csv = path_arg(args, "coexpression_csv")) {
    const std::string key = core::to_utf8(*csv);
    auto it = st.coexpression.find(key);
    if (it == st.coexpression.end())
      it = st.coexpression.emplace(key, solve::read_coexpression(ctx.files.input(*csv), ctx)).first;
    coexpression = &it->second;
  }

  solve::NodeLists nodes;
  nodes.essential = node_list(args, "essential_nodes", ctx);
  nodes.avoid = node_list(args, "avoid_nodes", ctx);
  nodes.active = node_list(args, "active_nodes", ctx);
  nodes.inactive = node_list(args, "inactive_nodes", ctx);
  nodes.non_relaxed = node_list(args, "nonrelaxed_nodes", ctx);
  nodes.confirmed_up = node_list(args, "confirmed_up", ctx);
  nodes.confirmed_down = node_list(args, "confirmed_down", ctx);
  solve::EdgeLists edges;
  edges.essential = edge_list(args, "essential_edges", ctx);
  edges.avoid = edge_list(args, "avoid_edges", ctx);
  edges.non_relaxed = edge_list(args, "nonrelaxed_edges", ctx);

  const solve::Inputs inputs{*corridor, *observations, match, bindings, std::move(nodes), std::move(edges),
                             coexpression, &maps, connect_pair(args, ctx), &st.interner};
  auto outcome = solve::solve(inputs, settings, ctx);
  auto results = std::make_shared<const solve::Results>(std::move(outcome.value));

  // Row 1.5: the batch layer publishes the results, a stopped solve's partial ones included (07.C9: the
  // solve is named by its positional prefix).
  const std::string name = given(args, "legacy_prefix") ? *text_arg(args, "legacy_prefix") : "solve";
  const results::PublishInputs publish{*results, inputs, *corridor, match, maps, name, settings,
                                       ws.observations == observations ? st.observation_values : nullptr,
                                       info.step + "/corridor.json", &st.interner};
  auto published = std::make_shared<results::SolveResult>();
  const core::Json summary = results::publish_solve(publish, ctx, *published);
  for (const auto& [key, value] : summary.items()) ws.details[key] = value;   // 2B.7
  st.solves.push_back(StepState::Solve{name, std::string(ctx.step), corridor_id, results, published});
  ws.solves[name] = results;
  ws.latest_solve = name;
  ctx.stop.throw_if_stopped();
  core::Outcome<void> result;
  result.partial = outcome.partial;
  return result;
}

namespace {

// 07.C9: the solves a line draws: named ones, else the latest.
std::vector<const StepState::Solve*> solves_named(const BoundArgs& args, const StepState& st) {
  std::vector<const StepState::Solve*> out;
  for (const auto& name : list_arg(args, "solve")) {
    const auto* s = st.find_solve(name);
    if (!s) throw core::Failure("batch.bad_argument", "no solve of this run is named " + name);
    out.push_back(s);
  }
  if (out.empty() && st.latest_solve()) out.push_back(st.latest_solve());
  if (out.empty()) throw core::Failure("batch.bad_argument", "no solve of this run has results to draw");
  return out;
}

// 07.C9: FuSION 1's "<prefix>_solutions.txt" binds to the solve with that prefix.
const StepState::Solve* solve_for_file(const StepState& st, std::string_view file) {
  for (const auto suffix : {std::string_view("_solutions.txt"), std::string_view("_relaxations.txt")})
    if (file.size() > suffix.size() && file.ends_with(suffix))
      if (const auto* s = st.find_solve(file.substr(0, file.size() - suffix.size()))) return s;
  return st.find_solve(file);
}

std::vector<const results::SolveResult*> published(const std::vector<const StepState::Solve*>& solves) {
  std::vector<const results::SolveResult*> out;
  for (const auto* s : solves) out.push_back(s->published.get());
  return out;
}

// An output prefix (2B.3): its base name names the files, and an absolute one is honoured as well.
void prefix_into(results::ExplainOptions& options, const std::string& prefix) {
  const auto p = core::path_from_utf8(prefix);
  options.prefix = core::to_utf8(p.filename());
  if (p.is_absolute()) options.absolute_prefix = p;
}

}  // namespace

// pex (S6.10 to S6.21, 07.C2): FuSION 1 ran plot_explanation.py on the four text files
// (Linux/src/main.cc:5702-5776); 2.0 draws the published results the files bind to (07.C9).
core::Outcome<void> run_pex(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const StepState& st = state_of(ws);
  results::ExplainOptions options;
  std::vector<const StepState::Solve*> solves;
  if (args.positional()) {
    const std::string cases = text_or(args, "cases", "both");
    const auto add = [&](std::string_view a, std::string_view b) {
      for (const auto name : {a, b})
        if (const auto file = text_arg(args, name); file && given(args, name))
          if (const auto* s = solve_for_file(st, *file);
              s && std::find(solves.begin(), solves.end(), s) == solves.end())
            solves.push_back(s);
    };
    if (cases != "excl") add("incl_soln", "incl_relax");   // FuSION 1's both, incl and excl (row 1.6)
    if (cases != "incl") add("excl_soln", "excl_relax");
    if (solves.empty()) throw core::Failure("batch.unmatched_file", "pex: no file binds to a solve of this run (07.C9)");
    if (given(args, "prefix")) prefix_into(options, *text_arg(args, "prefix"));
  } else {
    solves = solves_named(args, st);
    for (const auto& c : list_arg(args, "cases"))
      if (const auto kase = solve::parse_case(c)) options.cases.push_back(*kase);
    if (given(args, "prefix")) prefix_into(options, *text_arg(args, "prefix"));
  }
  options.points = results::parse_point_selector(text_or(args, "points", "all"));
  options.paths = results::parse_path_filter(text_or(args, "paths", "subgraph"));
  options.witness = static_cast<std::size_t>(int_arg(args, "witness").value_or(1));
  options.edge_table = bool_arg(args, "edge_table").value_or(false);
  options.static_svg = text_or(args, "static", "none") == "svg";
  if (given(args, "site_dir"))
    ctx.log.warning("results.site_later", "pex: the site export (S6.19) comes in a later build; site_dir is ignored");
  const auto drawn = published(solves);
  results::explain(drawn, options, ctx);
  return {};
}

// psp (S6.01): one case of the solve the file binds to (07.C9), with FuSION 1's free endpoints.
core::Outcome<void> run_psp(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const StepState& st = state_of(ws);
  const auto* solve = solve_for_file(st, *text_arg(args, "solution"));
  if (!solve) throw core::Failure("batch.unmatched_file", "psp: " + *text_arg(args, "solution") + " binds to no solve (07.C9)");
  results::ExplainOptions options;
  options.points = results::parse_point_selector(*text_arg(args, "points"));
  options.paths = results::parse_path_filter(*text_arg(args, "mode"));
  options.paths.from = network::NodeKey{*text_arg(args, "src")};
  options.paths.to = network::NodeKey{*text_arg(args, "tgt")};
  options.edge_table = true;
  options.static_svg = text_or(args, "static", "none") == "svg";
  prefix_into(options, *text_arg(args, "prefix"));
  const std::vector<const results::SolveResult*> drawn{solve->published.get()};
  results::explain(drawn, options, ctx);
  return {};
}

// plot_pareto (S6.22 to S6.24): the built-in Pareto plot, in place of plot_po.sh.
core::Outcome<void> run_plot_pareto(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const StepState& st = state_of(ws);
  results::ParetoOptions options;
  options.font = text_or(args, "font", "Liberation Sans");
  const auto drawn = published(solves_named(args, st));
  results::plot_pareto(drawn, options, ctx);
  return {};
}

// 01.C11: FuSION 1's "exec mv <fixed name> <dest>" lines, written natively from 2.0's own outputs.
void run_legacy_move(const LegacyMove& move, const Workspace& ws, core::Context& ctx) {
  const StepState* st = state_of(ws);
  const std::string dest = core::to_utf8(move.dest);
  const std::string name(legacy_file_name(move.name));
  const auto note = [&](const std::string& why) { ctx.log.info("exec: mv " + name + ": " + why + " (01.C11)"); };
  const auto write = [&](const std::string& text, const std::string& from_step) {
    core::OutputFile out = ctx.files.create(core::to_utf8(move.dest.filename()));
    out.also_at(move.dest);
    out.out() << text;
    out.commit();
    ctx.events.emit("legacy.written", core::Json{{"name", name}, {"dest", dest}, {"from_step", from_step}});
    note("written natively to " + dest + " from " + from_step);
  };
  switch (move.name) {
    case LegacyName::po_dat:
    case LegacyName::limits_timefile: {
      const StepState::Solve* solve = st ? st->latest_solve() : nullptr;
      if (!solve || solve->results->cases.empty()) {
        note("no solve has run yet, so nothing is written");
        return;
      }
      if (move.name == LegacyName::po_dat)   // the latest solve's frontier, n<TAB>e (S5.25)
        write(results::two_column(solve->published->cases.front()), solve->step);
      else                                   // its search log in FuSION 1's rendering (rows 3A.29, 1.21)
        write(results::legacy_limits(solve->results->cases.front()), solve->step);
      return;
    }
    case LegacyName::edges_merged: {
      if (!st || !st->merge || st->merge->network->info().legacy) {   // S2.16: a legacy read merges nothing
        note("the latest mff read FuSION 1's merged master and merged nothing, so it has no report");
        return;
      }
      core::OutputFile out = ctx.files.create(core::to_utf8(move.dest.filename()));
      out.also_at(move.dest);
      merge::write_edge_report(*st->merge, *ws.maps, out);
      out.commit();
      ctx.events.emit("legacy.written", core::Json{{"name", name}, {"dest", dest}, {"from_step", "mff"}});
      note("the latest merge's report, written natively to " + dest);
      return;
    }
    case LegacyName::fb_rch:
    case LegacyName::fb_rch_back: {
      std::string step;
      if (st && !st->corridors.empty()) step = st->corridors.rbegin()->second.step;
      if (!ws.debug || step.empty()) {
        note("the corridor's traces are written only under --debug (S4.19)");
        return;
      }
      const auto trace = ctx.files.step_dir().parent_path() / "debug" / step /
                         (move.name == LegacyName::fb_rch ? "forward.tsv" : "backward.tsv");
      std::ifstream in(trace, std::ios::binary);
      if (!in) {
        note("the trace " + core::to_utf8(trace) + " is missing");
        return;
      }
      std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      write(text, step);
      return;
    }
    case LegacyName::mylog:
      note(ws.debug ? "the run log holds the debug records (S1.25)" : "the debug log needs --debug (S1.25)");
      return;
  }
}

}  // namespace fusion::batch
