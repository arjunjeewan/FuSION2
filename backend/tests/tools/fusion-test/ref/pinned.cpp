// Pinned mode (section 12, "Modes"; the C4 note's table): each stage at library level on FuSION 1's
// recorded input to that stage, so a difference belongs to its own stage. Each stage's arguments come from
// the case's own batch as the batch module binds it (BatchView); a file the batch made while FuSION 1 ran is
// read from FuSION 1's run folder (recorded()). The handlers' glue is restated here, since they are the batch
// module's own; the stages are the modules' public calls.
#include <algorithm>
#include <sstream>

#include "files.hpp"
#include "fusion/enrichment/enrichment.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/merge/merge.hpp"
#include "fusion/network/network.hpp"
#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/step_context.hpp"
#include "ref/runner.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using testkit::CheckOutcome;

struct PinnedBase {
  network::Maps maps;                                  // start's; a map it leaves out is empty (labels only)
  std::unique_ptr<core::Interner> interner = std::make_unique<core::Interner>();   // the run's ids (02.C3)
  std::shared_ptr<const expression::Observations> observations;   // FuSION 1's up and down lists
  std::optional<merge::ExceptionSet> exceptions;       // from FuSION 1's inputs (S1.17, 03.C1)
  std::shared_ptr<const network::Network> reread;      // FuSION 1's re-read graph through 2.0's legacy reader
  std::unique_ptr<network::BindingTable> bindings;
  std::shared_ptr<const corridor::Corridor> corridor;  // null for an empty corridor (S4.18)
  std::optional<core::Report> corridor_report;
  std::vector<expression::ObservationMatch> match;     // 05.C9
};

namespace {

std::vector<std::string> words(std::string_view s) {
  std::istringstream in{std::string(s)};
  std::vector<std::string> out;
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}

const BatchLine& line_of(const BatchView& b, std::string_view command) {
  const BatchLine* l = b.first(command);
  if (!l) throw core::Failure("ref.pinned", "the batch has no " + std::string(command) + " line");
  return *l;
}

fs::path named_file(const BatchLine& line, std::string_view arg, const CaseSetup& s) {
  const auto p = path(line, arg);
  if (!p) throw core::Failure("ref.pinned", line.command + " names no " + std::string(arg));
  return recorded(*p, s.batch, s.f1_run);
}

// The maps a value file is read with: none before the start line (FuSION 1's cudf read ids as written, 02.C3).
network::Maps value_maps(const CaseSetup& s, const BatchLine& line, const network::Maps& start_maps) {
  const BatchLine* start = s.batch.first("start");
  return start && start->number < line.number ? start_maps : network::Maps{};
}

expression::ValueInput value_input(const BatchLine& line, const CaseSetup& s) {
  expression::ValueInput in;
  in.path = named_file(line, "values", s);
  if (given(line, "id_column")) in.id_column = text(line, "id_column");
  if (given(line, "value_column")) in.value_column = text(line, "value_column");
  if (given(line, "id_map")) in.id_map = text(line, "id_map");
  if (text(line, "repeats").value_or("classify_each") == "max_abs") in.repeats = expression::RepeatRule::max_abs;
  return in;
}

std::vector<std::string> strings(std::span<const core::GeneId> ids, const core::Interner& in) {
  std::vector<std::string> out;
  for (core::GeneId id : ids) out.emplace_back(in.str(id));
  return out;
}

// A FuSION 1 list file's lines through the case's path map and, for paths into FuSION 1's own case folder
// (the toys' W4 scratch folders), the map of that folder to the case's: FuSION 1 echoed the mff line with
// its own path for the list, so the two paths share the part below the case folder.
std::vector<fs::path> listed_files(const fs::path& list, const CaseSetup& s, const fs::path& ours) {
  std::string text = fusiontest::slurp(list);
  std::vector<PathMap> map = s.kase->path_map;
  const std::string rel = core::to_utf8(ours.lexically_relative(s.prepared.dir));
  for (const std::string& line : file_lines(fusiontest::slurp(s.artefacts / s.kase->fusion1.console))) {
    if (!line.starts_with("mff ")) continue;
    const auto w = words(line);
    if (w.size() > 1 && !rel.empty() && w[1].size() > rel.size() && w[1].ends_with("/" + rel))
      map.push_back({w[1].substr(0, w[1].size() - rel.size() - 1), "$CASE"});
    break;
  }
  const std::optional<fs::path> data = s.roots.data ? std::optional<fs::path>(s.roots.data->root) : std::nullopt;
  text = apply_path_map(std::move(text), map, s.prepared.dir, data);
  if (const auto left = leftover_fusion1_path(text))
    throw core::Failure("ref.leftover", core::to_utf8(list) + " still names " + *left + " after the path map");
  std::vector<fs::path> out;
  for (const std::string& line : file_lines(text))
    if (!words(line).empty()) out.push_back(core::path_from_utf8(words(line).front()));
  return out;
}

std::vector<std::string> ids_without(std::vector<std::string> ids, const std::vector<std::string>& drop) {
  std::erase_if(ids, [&](const std::string& id) { return std::find(drop.begin(), drop.end(), id) != drop.end(); });
  return ids;
}

}  // namespace

std::shared_ptr<PinnedBase> pinned_base(const CaseSetup& s, core::Context& ctx) {
  auto base = std::make_shared<PinnedBase>();
  if (const BatchLine* start = s.batch.first("start")) {
    if (const auto p = path(*start, "id_map")) base->maps.ids = network::IdMap::read(recorded(*p, s.batch, s.f1_run), ctx);
    if (const auto p = path(*start, "symbols")) base->maps.symbols = network::LabelMap::read(recorded(*p, s.batch, s.f1_run), ctx);
    if (const auto p = path(*start, "pathway_names"))
      base->maps.pathway_names = network::LabelMap::read(recorded(*p, s.batch, s.f1_run), ctx);
  }
  core::Interner& ids = *base->interner;
  base->observations = std::make_shared<const expression::Observations>(expression::load_lists(
      s.artefacts / s.kase->fusion1.up, s.artefacts / s.kase->fusion1.down, base->maps.ids, ids, ctx));

  // The exception set from FuSION 1's inputs: its lists, the endpoints, start's file (FuSION 1's own
  // exception file, which the batch made from them) and the not-merged list (S1.17, 03.C1, 03.C2).
  const BatchLine& fb = line_of(s.batch, "fb_rch");
  std::vector<core::GeneId> sources, targets;
  for (const std::string& id : list(fb, "source")) sources.push_back(ids.id(id));
  for (const std::string& id : list(fb, "target")) targets.push_back(ids.id(id));
  merge::ExceptionInputs in;
  in.observations = base->observations.get();
  in.sources = sources;
  in.targets = targets;
  in.interner = &ids;
  if (!s.batch.plan->facts.empty()) {
    for (const fs::path& p : s.batch.plan->facts.front().confirmed_up) in.confirmed_up.push_back(recorded(p, s.batch, s.f1_run));
    for (const fs::path& p : s.batch.plan->facts.front().confirmed_down) in.confirmed_down.push_back(recorded(p, s.batch, s.f1_run));
  }
  if (const BatchLine* mff = s.batch.first("mff"))
    if (const auto p = path(*mff, "not_merged")) in.not_merged = recorded(*p, s.batch, s.f1_run);
  if (const BatchLine* start = s.batch.first("start"))
    if (const auto p = path(*start, "exception_file")) in.start_file = recorded(*p, s.batch, s.f1_run);
  base->exceptions = merge::assemble_exceptions(in, base->maps.ids, ctx);

  // The corridor on FuSION 1's re-read graph, read as 2.0 reads any FuSION 1-written file (S2.27).
  network::BuildSettings build;
  if (const BatchLine* rgx = s.batch.first("rgx")) build.split_threshold = static_cast<int>(integer(*rgx, "split_threshold").value_or(1));
  base->reread = network::read_legacy(s.artefacts / s.kase->fusion1.reread, base->maps.ids, build, base->exceptions->ids(), ctx);
  base->bindings = std::make_unique<network::BindingTable>(base->reread, base->maps);
  corridor::Request request;
  request.source = list(fb, "source");
  request.target = list(fb, "target");
  request.exclude = list(fb, "exclude");
  request.bound = static_cast<std::uint32_t>(integer(fb, "bound").value_or(0));
  request.edges_to_target = corridor::parse_edges_to_target(text(fb, "edges_to_target").value_or("any")).value_or(corridor::EdgesToTarget::any);
  if (const auto skip = list(fb, "skip_subtypes"); !skip.empty()) request.skip_subtypes = skip;
  auto outcome = corridor::compute(base->reread, *base->bindings, request, corridor::Naming{&base->maps, 0, 0}, ctx);
  base->corridor = outcome.value;
  base->corridor_report = outcome.report;
  if (base->corridor) base->match = corridor::match_observations(*base->corridor, *base->observations, ctx);
  return base;
}

PinnedSolve pinned_solve(const PinnedBase& base, const CaseSetup& s, const Arm& arm, core::Context& ctx) {
  if (!arm.line) throw core::Failure("ref.pinned", "no pathz3 line of the batch makes FuSION 1's " + arm.name + " solve");
  if (!base.corridor)
    throw core::Failure("ref.pinned", "the corridor is empty" + (base.corridor_report ? ": " + base.corridor_report->message : std::string()));
  const BatchLine& line = *arm.line;
  PinnedSolve out;
  solve::Settings& st = out.settings;
  const solve::Case kase = arm.fsc.empty() ? solve::Case::free : arm.name == "out" ? solve::Case::out : solve::Case::in;
  st.fsc = arm.fsc;
  st.cases = {kase};
  st.box.lo = {static_cast<int>(integer(line, "node_min").value_or(0)), static_cast<int>(integer(line, "edge_min").value_or(0))};
  st.box.hi = {static_cast<int>(integer(line, "node_max").value_or(8)), static_cast<int>(integer(line, "edge_max").value_or(8))};
  st.timeout = std::chrono::milliseconds(integer(line, "timeout").value_or(14'400'000));
  st.incremental_timeout = std::chrono::milliseconds(integer(line, "incremental_timeout").value_or(3'600'000));
  st.seed = static_cast<std::uint32_t>(integer(line, "seed").value_or(42));
  st.solutions = static_cast<int>(integer(line, "solutions").value_or(1));
  st.threads = 1;
  if (given(line, "constraints")) st.families.enabled = flag(line, "constraints");
  st.families.coexpression_threshold = real(line, "coexpression_threshold");
  st.families.frozen_threshold = real(line, "frozen_threshold");
  st.positional = false;                               // 2.0's named case (06.C10, 06.C16)

  // The line's lists as FuSION 1's run left them; the case's FSC rule replaces their FSC (S5.17).
  const auto node_list = [&](const char* arg) {
    const auto p = path(line, arg);
    return p ? solve::read_node_list(recorded(*p, s.batch, s.f1_run)) : std::vector<std::string>{};
  };
  const auto edge_list = [&](const char* arg) {
    const auto p = path(line, arg);
    return p ? solve::read_edge_list(recorded(*p, s.batch, s.f1_run)) : std::vector<solve::EdgeLine>{};
  };
  const bool in = kase == solve::Case::in, without = kase == solve::Case::out;
  solve::NodeLists nodes;
  nodes.essential = in ? ids_without(node_list("essential_nodes"), arm.fsc) : node_list("essential_nodes");
  nodes.avoid = without ? ids_without(node_list("avoid_nodes"), arm.fsc) : node_list("avoid_nodes");
  nodes.active = in ? ids_without(node_list("active_nodes"), arm.fsc) : node_list("active_nodes");
  nodes.inactive = node_list("inactive_nodes");
  nodes.non_relaxed = in ? ids_without(node_list("nonrelaxed_nodes"), arm.fsc) : node_list("nonrelaxed_nodes");
  nodes.confirmed_up = node_list("confirmed_up");
  nodes.confirmed_down = node_list("confirmed_down");
  solve::EdgeLists edges;
  edges.essential = edge_list("essential_edges");
  edges.avoid = edge_list("avoid_edges");
  edges.non_relaxed = edge_list("nonrelaxed_edges");
  std::optional<solve::CoexpressionTable> table;
  std::optional<fs::path> csv;
  if (const auto p = path(line, "coexpression_csv")) {
    csv = recorded(*p, s.batch, s.f1_run);
    table = solve::read_coexpression(*csv, ctx);
  }

  // The same case in ids, for the kit's checker (R1, R2).
  testkit::CaseSpec& k = out.spec;
  k.up = strings(base.observations->up(), *base.interner);
  k.down = strings(base.observations->down(), *base.interner);
  k.essential = nodes.essential;
  k.avoid = nodes.avoid;
  k.active = nodes.active;
  k.inactive = nodes.inactive;
  k.nonrelaxed = nodes.non_relaxed;
  k.confirmed_up = nodes.confirmed_up;
  k.confirmed_down = nodes.confirmed_down;
  const auto edge_specs = [](const std::vector<solve::EdgeLine>& lines) {
    std::vector<testkit::EdgeSpec> v;
    for (const solve::EdgeLine& e : lines) v.push_back({e.source, e.target, e.subtypes});
    return v;
  };
  k.essential_edges = edge_specs(edges.essential);
  k.avoid_edges = edge_specs(edges.avoid);
  k.nonrelaxed_edges = edge_specs(edges.non_relaxed);
  const bool allowed = st.families.enabled.value_or(true);   // S5.03: a given flag decides, else the inputs given
  if (allowed && csv && st.families.coexpression_threshold && *st.families.coexpression_threshold >= 0)
    k.coexpressed = testkit::f1::read_coexpression(*csv, *st.families.coexpression_threshold);
  if (allowed && st.families.frozen_threshold && *st.families.frozen_threshold >= 0) k.frozen_threshold = st.families.frozen_threshold;
  k.fsc = arm.fsc;
  k.solve_case = in ? testkit::CaseSpec::Case::in : without ? testkit::CaseSpec::Case::out : testkit::CaseSpec::Case::free;
  k.named = true;

  const solve::Inputs inputs{*base.corridor, *base.observations, base.match, *base.bindings, std::move(nodes),
                             std::move(edges), table ? &*table : nullptr, &base.maps, std::nullopt, base.interner.get()};
  const solve::CaseGraph graph = solve::read_corridor(*base.corridor, &base.maps);
  const solve::CaseSpec spec = solve::bind_inputs(inputs, graph, st, ctx);
  out.problem = solve::make_case(graph, spec, st, kase);
  out.corridor = testkit::adapt::canon(*base.corridor);
  out.source = base.corridor;
  return out;
}

Facts pinned_facts(const CaseSetup& s, bool with_solve, const fs::path& work) {
  Facts f;
  testkit::StepContext sc(work / "stages", "pinned");
  core::Context& ctx = sc.ctx();
  const auto failed = [&](std::initializer_list<Stage> stages, const std::exception& e) {
    const auto* failure = dynamic_cast<const core::Failure*>(&e);
    const std::string why = failure ? std::string(e.what()) + " [" + failure->code() + "]" : std::string(e.what());
    for (Stage st : stages) f.missing.emplace(st, why);
  };

  // classify: the LFC file at the cudf line's thresholds, by expression's reader and classifier.
  network::Maps start_maps;
  if (const BatchLine* cudf = s.batch.first("cudf")) {
    try {
      core::Interner ids;
      const BatchLine* start = s.batch.first("start");
      if (start && start->number < cudf->number)
        if (const auto p = path(*start, "id_map")) start_maps.ids = network::IdMap::read(recorded(*p, s.batch, s.f1_run), ctx);
      const network::Maps maps = value_maps(s, *cudf, start_maps);
      const expression::ValueInput in = value_input(*cudf, s);
      const expression::GeneValues values = expression::read_values(in.path, in, maps, ids, ctx);
      std::vector<core::GeneId> not_de;
      if (const auto p = path(*cudf, "not_de")) not_de = expression::read_id_list(recorded(*p, s.batch, s.f1_run), maps.ids, ids);
      expression::ClassifyRequest request;
      request.thresholds = expression::Thresholds{real(*cudf, "up").value_or(0), real(*cudf, "down").value_or(0)};
      request.not_de = not_de;
      request.interner = &ids;
      const auto outcome = expression::classify(values, request, ctx);
      f.up = strings(outcome.value.up(), ids);
      f.down = strings(outcome.value.down(), ids);
    } catch (const std::exception& e) {
      failed({Stage::classify}, e);
    }
  }

  std::shared_ptr<PinnedBase> base;
  try {
    base = pinned_base(s, ctx);
    std::vector<std::string> ids(base->exceptions->ids().ids().begin(), base->exceptions->ids().ids().end());
    f.exceptions = std::move(ids);
  } catch (const std::exception& e) {
    failed({Stage::exceptions, Stage::merge, Stage::corridor, Stage::observations, Stage::solve_inputs, Stage::solve}, e);
  }

  // GSEA: the LFC file and the candidate list, on one thread (08.C6).
  if (const BatchLine* gsea = s.batch.first("gsea")) {
    try {
      testkit::StepContext gc(work / "gsea", "gsea");
      core::Interner ids;
      network::Maps maps;
      if (base) maps = value_maps(s, *gsea, base->maps);
      const expression::ValueInput in = value_input(*gsea, s);
      const expression::GeneValues values = expression::read_values(in.path, in, maps, ids, gc.ctx());
      const fs::path candidates = named_file(*gsea, "candidates", s);
      std::optional<fs::path> kgml_dir;
      if (given(*gsea, "kgml_dir")) kgml_dir = path(*gsea, "kgml_dir");
      const network::PathList listed =
          network::read_input_list(candidates, network::list_base(candidates, kgml_dir, std::nullopt), gc.ctx());
      network::StepDocs docs;
      std::vector<std::shared_ptr<const network::PathwayDoc>> parsed;
      for (const network::ListLine& l : listed.lines) parsed.push_back(docs.get(l.path, maps.ids, gc.ctx()));
      enrichment::Options options;
      options.selector = enrichment::parse_selector(text(*gsea, "selector").value_or(""));
      for (const std::string& id : list(*gsea, "keep"))
        if (id != "0") options.keep.emplace_back(ids.str(maps.ids.canonical(id, ids)));
      options.settings.seed = static_cast<std::uint64_t>(integer(*gsea, "seed").value_or(42));
      options.settings.permutations = static_cast<std::uint32_t>(integer(*gsea, "permutations").value_or(1000));
      const auto shortlist = path(*gsea, "shortlist");
      options.shortlist_name = shortlist ? core::to_utf8(shortlist->filename()) : std::string("shortlist.txt");
      options.candidates = core::to_utf8(candidates);
      const auto outcome =
          enrichment::gsea(values, listed.lines, parsed, listed.base, network::RelationFilters{}, ids, options, gc.ctx());
      GseaFacts g;
      const fs::path table = gc.dir() / (core::to_utf8(core::path_from_utf8(options.shortlist_name).stem()) + ".gsea.tsv");
      g.rows = testkit::f1::read_gsea_table(table);
      for (const std::string& p : testkit::f1::read_ids(gc.dir() / options.shortlist_name))
        g.shortlist.push_back(core::to_utf8(core::path_from_utf8(p).stem()));
      g.zero_weight_draws = outcome.value ? outcome.value->prerank.zero_weight_draws : 0;
      f.gsea = std::move(g);
    } catch (const std::exception& e) {
      failed({Stage::gsea}, e);
    }
  }

  // merge: FuSION 1's shortlist, or the network file its list named, with FuSION 1's exception set.
  if (base && s.batch.first("mff")) {
    try {
      const BatchLine& mff = *s.batch.first("mff");
      std::vector<fs::path> files;
      if (s.kase->fusion1.shortlist && s.batch.first("gsea")) {
        files = listed_files(s.artefacts / *s.kase->fusion1.shortlist, s, path(mff, "list").value_or(fs::path()));
      } else {
        const auto list_path = path(mff, "list");
        if (!list_path) throw core::Failure("ref.pinned", "the mff line names no list");
        files = listed_files(recorded(*list_path, s.batch, s.f1_run), s, *list_path);
      }
      network::BuildSettings build;
      build.split_threshold = static_cast<int>(integer(mff, "split_threshold").value_or(1));
      const std::optional<bool> tool_written = given(mff, "tool_written") ? flag(mff, "tool_written") : std::nullopt;
      std::vector<merge::DocPtr> docs;
      std::shared_ptr<const network::Network> merged;
      for (const fs::path& file : files) {
        const network::Format format = network::sniff(file, tool_written, ctx).format;
        if (format == network::Format::legacy) {
          if (files.size() != 1) throw core::Failure("ref.pinned", "a legacy file merged with others");
          merged = network::read_legacy(file, base->maps.ids, build, base->exceptions->ids(), ctx);
        } else {
          docs.push_back(network::read_kgml(file, base->maps.ids, ctx));
        }
      }
      if (!merged) merged = merge::merge_direct(docs, build, *base->exceptions);
      f.merged = testkit::adapt::canon(*merged);
    } catch (const std::exception& e) {
      failed({Stage::merge}, e);
    }
  }

  if (base && base->corridor) {
    testkit::CanonCorridor c = testkit::adapt::canon(*base->corridor);
    f.corridor_size = std::pair{static_cast<int>(c.graph.nodes.size()), static_cast<int>(c.graph.edges.size())};
    f.corridor = std::move(c);
  } else if (base) {
    f.missing.emplace(Stage::corridor, base->corridor_report ? base->corridor_report->message : "no corridor");
  }

  // Each FuSION 1 solve as 2.0's case on that corridor: its observations, solve inputs and, unless it is a
  // T3 case's run, the solve.
  if (base && base->corridor) {
    std::vector<std::pair<std::string, char>> in_corridor;
    for (const expression::ObservationMatch& m : base->match)
      if (m.group == expression::MatchGroup::in_corridor)
        in_corridor.emplace_back(std::string(base->interner->str(m.id)), m.regulation == expression::Regulation::down ? 'd' : 'u');
    const std::set<std::string> corridor_ids = member_ids(f.corridor->graph);
    for (const Arm& arm : s.arms) {
      SolveFacts out;
      out.name = arm.name;
      out.step = "pinned";
      try {
        PinnedSolve ps = pinned_solve(*base, s, arm, ctx);
        out.spec = ps.spec;
        out.corridor = ps.corridor;
        out.box_lo = {ps.settings.box.lo.n, ps.settings.box.lo.e};
        out.box_hi = {ps.settings.box.hi.n, ps.settings.box.hi.e};
        out.up_down = observed_counts(in_corridor, ps.spec, corridor_ids);
        const solve::Problem* problem = std::get_if<solve::Problem>(&ps.problem);
        if (!problem)
          if (const auto* skipped = std::get_if<solve::Skipped>(&ps.problem)) problem = skipped->problem.get();
        if (problem && problem->families.coexpression) out.coexpression = static_cast<int>(problem->coexpressed.size());
        if (problem && problem->families.frozen) out.frozen = static_cast<int>(problem->frozen.size());
        if (with_solve) {
          testkit::StepContext solve_ctx(work / ("solve-" + arm.name), "pathz3");
          std::vector<std::variant<solve::Problem, solve::Skipped>> cases;
          cases.push_back(std::move(ps.problem));
          const core::Outcome<solve::Results> result = solve::solve_cases(std::move(cases), ps.settings, solve_ctx.ctx());
          if (result.value.cases.empty()) throw core::Failure("ref.pinned", "the solve returned no case");
          const solve::CaseResult& r = result.value.cases.front();
          out.status = std::string(solve::name(r.status));
          out.seconds = r.stats.wall_s;
          for (const solve::FrontierPoint& p : r.frontier.points) {
            out.frontier.points.push_back({p.point.n, p.point.e});
            out.frontier.certified.push_back(p.certified);
            if (p.witness < r.witnesses.size()) out.witnesses.emplace_back(testkit::adapt::assignment(*ps.source, r.witnesses[p.witness]));
            else out.witnesses.emplace_back(std::nullopt);
          }
          for (const solve::Answer& a : r.log) {
            const CheckOutcome o = a.outcome == solve::Outcome::sat     ? CheckOutcome::sat
                                   : a.outcome == solve::Outcome::unsat ? CheckOutcome::unsat
                                                                        : CheckOutcome::unknown;
            out.answers.emplace_back(testkit::Point{a.point.n, a.point.e}, o);
          }
        }
      } catch (const std::exception& e) {
        const auto* failure = dynamic_cast<const core::Failure*>(&e);
        out.error = failure ? std::string(e.what()) + " [" + failure->code() + "]" : std::string(e.what());
      }
      f.solves.push_back(std::move(out));
    }
  }
  if (!with_solve) f.missing.emplace(Stage::solve, "a T3 case: verification proves its frontier (12.C7)");
  return f;
}

}  // namespace fusion::reftest
