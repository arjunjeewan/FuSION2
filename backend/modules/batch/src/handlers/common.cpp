// The handlers' table and what they share (01.C9): a handler turns bound arguments into the owning
// module's typed options and calls its public interface, so the modules never see tokens or names.
#include <sstream>

#include "fusion/core/install.hpp"
#include "state.hpp"

namespace fusion::batch {

const StepState::Solve* StepState::find_solve(std::string_view name_or_step) const {
  for (auto it = solves.rbegin(); it != solves.rend(); ++it)
    if (it->name == name_or_step || it->step == name_or_step) return &*it;
  return nullptr;
}

StepState& state_of(Workspace& ws) {
  if (!ws.state) ws.state = std::make_shared<StepState>();
  return *ws.state;
}
const StepState* state_of(const Workspace& ws) { return ws.state.get(); }

core::Json map_record(const std::filesystem::path& named, const std::filesystem::path& copy, bool shipped) {
  return core::Json{{"path", core::to_utf8(named)}, {"sha256", core::Sha256::of_file(copy).hex()}, {"shipped", shipped}};
}

// Row 2B.4 and 02's "Maps": the shipped symbol and pathway-name maps, read from the home through the run's
// checked copies (09.C3), so run.json lists them as inputs, and each recorded by path and SHA-256.
network::Maps shipped_maps(core::Context& ctx, core::Json& record, bool symbols, bool pathway_names) {
  network::Maps maps;
  for (const auto& [wanted, rel, into, key] :
       {std::tuple{symbols, "data/hsa_to_gene_symbol_map.txt", &maps.symbols, "symbols"},
        std::tuple{pathway_names, "data/hsa_path_to_path_name_map.txt", &maps.pathway_names, "pathway_names"}}) {
    if (!wanted) continue;
    const auto named = core::home_file(rel);
    const auto copy = ctx.files.input(named);
    *into = network::LabelMap::read(copy, ctx);
    record[key] = map_record(named, copy, true);
  }
  return maps;
}

// Row 2B.4: start is optional; without it, and for any map it leaves out, the shipped maps.
const network::Maps& run_maps(Workspace& ws, core::Context& ctx) {
  if (!ws.maps) {
    core::Json record{{"id_map", nullptr}};
    ws.maps = std::make_shared<const network::Maps>(shipped_maps(ctx, record, true, true));
    ws.details["maps"] = std::move(record);
  }
  return *ws.maps;
}

const network::Maps& id_maps(const Workspace& ws) {
  static const network::Maps none;   // no id map: FuSION 1's cudf before start read ids as written
  return ws.maps ? *ws.maps : none;
}

network::BindingTable& bindings_for(Workspace& ws, int network_graph,
                                    const std::shared_ptr<const network::Network>& network, core::Context& ctx) {
  StepState& st = state_of(ws);
  auto it = st.bindings.find(network_graph);
  if (it == st.bindings.end())
    it = st.bindings.emplace(network_graph, std::make_unique<network::BindingTable>(network, run_maps(ws, ctx))).first;
  st.latest_bindings = network_graph;
  return *it->second;
}

namespace {
const BoundValue* value_of(const BoundArgs& args, std::string_view name) {
  for (const auto& v : args.values)
    if (v.spec && v.spec->name == name) return &v;
  return nullptr;
}
}  // namespace

bool given(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  return v && v->given && v->has_value;
}
std::optional<std::string> text_arg(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* s = std::get_if<std::string>(&v->value)) return *s;
  return value_text(v->value);
}
std::string text_or(const BoundArgs& args, std::string_view name, std::string fallback) {
  auto t = text_arg(args, name);
  return t ? *t : std::move(fallback);
}
std::optional<std::int64_t> int_arg(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* i = std::get_if<std::int64_t>(&v->value)) return *i;
  return std::nullopt;
}
std::optional<double> real_arg(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* d = std::get_if<double>(&v->value)) return *d;
  return std::nullopt;
}
std::optional<bool> bool_arg(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  if (!v || !v->has_value) return std::nullopt;
  if (const auto* b = std::get_if<bool>(&v->value)) return *b;
  return std::nullopt;
}
std::vector<std::string> list_arg(const BoundArgs& args, std::string_view name) {
  const auto* v = value_of(args, name);
  if (!v || !v->has_value) return {};
  if (const auto* l = std::get_if<std::vector<std::string>>(&v->value)) return *l;
  if (const auto* s = std::get_if<std::string>(&v->value)) return {*s};
  return {};
}
std::optional<std::filesystem::path> path_arg(const BoundArgs& args, std::string_view name) {
  if (!given(args, name)) return std::nullopt;
  const auto t = text_arg(args, name);
  if (!t || t->empty()) return std::nullopt;
  return core::path_from_utf8(*t);
}

std::shared_ptr<const network::Network> network_of(const Workspace& ws, int graph, std::string_view command) {
  const GraphEntry& entry = ws.graphs.get(graph);
  if (const auto* n = std::get_if<std::shared_ptr<const network::Network>>(&entry)) return *n;
  throw core::Failure("batch.bad_argument", std::string(command) + ": graph " + std::to_string(graph) +
                                                " is a corridor, not a network");
}

std::shared_ptr<const corridor::Corridor> corridor_of(const Workspace& ws, int graph, std::string_view command) {
  const GraphEntry& entry = ws.graphs.get(graph);
  if (const auto* c = std::get_if<std::shared_ptr<const corridor::Corridor>>(&entry)) return *c;
  throw core::Failure("batch.bad_argument", std::string(command) + ": graph " + std::to_string(graph) +
                                                " is a network, not a corridor");
}

network::GraphView view_of(const Workspace& ws, int graph) {
  const GraphEntry& entry = ws.graphs.get(graph);
  if (const auto* n = std::get_if<std::shared_ptr<const network::Network>>(&entry)) return network::GraphView(*n);
  return std::get<std::shared_ptr<const corridor::Corridor>>(entry)->view();
}

core::OutputFile create_output(core::Context& ctx, const std::string& name) {
  const auto path = core::path_from_utf8(name);
  if (path.is_absolute()) {   // 2B.3: written in the step directory, and at the path as well
    core::OutputFile file = ctx.files.create(core::to_utf8(path.filename()));
    file.also_at(path);
    return file;
  }
  return ctx.files.create(name);
}

network::RelationFilters filters_of(const core::Context& ctx) {
  network::RelationFilters f;
  if (const auto v = ctx.settings.find<std::string>("indirect"); v && *v == "keep")
    f.indirect = network::IndirectMode::keep;
  if (const auto v = ctx.settings.find<std::string>("ecrel"); v && *v == "exclude")
    f.ecrel = network::EcrelMode::exclude;
  if (const auto v = ctx.settings.find<std::string>("maplink"); v && *v == "keep")
    f.maplink = network::MaplinkMode::keep;
  return f;
}

// 03.C1, 03.C2, 01.C17: the up and down genes of the latest cudf; the source, target, FSC and confirmed
// lists from the line, or else from the batch's later fb_rch and pathz3 lines; start's file (S1.17).
merge::ExceptionInputs exception_inputs(const BoundArgs* args, Workspace& ws, ExceptionIds& ids) {
  StepState& st = state_of(ws);
  merge::ExceptionInputs in;
  in.observations = ws.observations.get();
  in.interner = &st.interner;
  const auto texts = [&](std::string_view name, const std::vector<std::string>& facts) {
    return args && given(*args, name) ? list_arg(*args, name) : facts;
  };
  for (const auto& s : texts("source", ws.facts.sources)) ids.sources.push_back(st.interner.id(s));
  for (const auto& s : texts("target", ws.facts.targets)) ids.targets.push_back(st.interner.id(s));
  for (const auto& s : texts("fsc", ws.facts.fscs)) ids.fscs.push_back(st.interner.id(s));
  in.sources = ids.sources;
  in.targets = ids.targets;
  in.fscs = ids.fscs;
  if (args && given(*args, "confirmed_up")) {
    for (const auto& p : list_arg(*args, "confirmed_up")) in.confirmed_up.push_back(core::path_from_utf8(p));
  } else {
    in.confirmed_up = ws.facts.confirmed_up;
  }
  if (args && given(*args, "confirmed_down")) {
    for (const auto& p : list_arg(*args, "confirmed_down")) in.confirmed_down.push_back(core::path_from_utf8(p));
  } else {
    in.confirmed_down = ws.facts.confirmed_down;
  }
  if (args) in.not_merged = path_arg(*args, "not_merged");
  in.start_file = st.start_file;
  return in;
}

// help (S1.12): the listing, or one command's arguments, from the one table.
core::Outcome<void> run_help(const BoundArgs& args, Workspace&, core::Context& ctx) {
  std::ostringstream out;
  const auto name = text_arg(args, "command");
  if (name) {
    if (const auto* spec = command_table().find(*name)) command_table().print_command(out, *spec);
  } else {
    command_table().print_listing(out);
  }
  std::istringstream lines(out.str());
  for (std::string line; std::getline(lines, line);) ctx.log.info(line);
  return {};
}

// The run's end (entry 230): bindings.tsv, written once whatever the status.
void finish_run(Workspace& ws, run::Run& run) {
  StepState* st = ws.state.get();
  if (!st || st->bindings.empty()) return;
  const auto it = st->bindings.find(st->latest_bindings);
  if (it == st->bindings.end()) return;
  core::OutputFile file = run.create_run_file("bindings.tsv");
  it->second->write(file.out());
  file.commit();
}

namespace {
constexpr std::pair<std::string_view, Handler> kHandlers[] = {
    {"help", run_help}, {"start", run_start}, {"cudf", run_cudf}, {"suggest_thresholds", run_sth},
    {"gsea", run_gsea}, {"mff", run_mff}, {"rgx", run_rgx}, {"wgx", run_wgx}, {"sz", run_sz}, {"add", run_add},
    {"fb_rch", run_fb_rch}, {"pathz3", run_pathz3}, {"psp", run_psp}, {"pex", run_pex},
    {"plot_pareto", run_plot_pareto},
};
}  // namespace

const Handlers& step_handlers() {
  static const Handlers handlers{kHandlers, run_legacy_move, finish_run, describe_run};
  return handlers;
}

Interpreter::Interpreter(const Plan& plan, int point, run::Run& run)
    : Interpreter(plan, point, run, step_handlers()) {}

int run_batch(const Plan& plan, const RunOptions& options, core::Log& log) {
  return run_batch(plan, options, log, step_handlers());
}

}  // namespace fusion::batch
