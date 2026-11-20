// The commands that make graphs from pathways and corridors: mff (section 03) and fb_rch (section 05).
#include "state.hpp"

namespace fusion::batch {

// mff (S2.16 to S2.24, 03). FuSION 1 read every listed file into its own graph, then merged them with
// its map list (Linux/src/main.cc:2330-2481); 2.0 merges the documents once, order-independently, keeps
// the exception set apart (S1.17, S2.13, 03.C1, 03.C2), and takes gsea's shortlist from memory (01.C16).
core::Outcome<void> run_mff(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  run_maps(ws, ctx);   // ws.maps: start's, or the shipped ones (2B.4)
  std::vector<std::filesystem::path> files;
  const auto list = path_arg(args, "list");
  std::shared_ptr<const enrichment::Shortlist> shortlist = ws.shortlist;
  if (list) {   // 01.C13: gsea's own shortlist, unchanged, comes from memory; a changed one is read again
    shortlist = nullptr;
    if (const auto remembered = ws.written.get(*list))
      if (const auto* s = std::get_if<std::shared_ptr<const enrichment::Shortlist>>(&*remembered)) shortlist = *s;
  }
  if (shortlist) {
    files = shortlist->chosen_paths();
  } else if (list) {
    std::optional<std::filesystem::path> kgml_dir;
    if (given(args, "kgml_dir")) kgml_dir = resolve_input(ws.batch_dir, *text_arg(args, "kgml_dir"));
    const auto base = network::list_base(*list, kgml_dir, std::nullopt);
    for (const auto& line : network::read_input_list(ctx.files.input(*list), base, ctx).lines) files.push_back(line.path);
  } else {
    throw core::Failure("batch.bad_argument", "mff names no list, and no earlier gsea made a shortlist (01.C16)");
  }
  merge::MergeRequest request;
  const std::optional<bool> tool_written = given(args, "tool_written") ? bool_arg(args, "tool_written") : std::nullopt;
  for (const auto& file : files) {
    ctx.stop.throw_if_stopped();
    const auto copy = ctx.files.input(file);
    network::SourceRef ref;
    ref.path = file;
    ref.format = network::sniff(copy, tool_written, ctx).format;   // S2.12: the content decides
    ref.sha256 = core::Sha256::of_file(copy);
    ref.size = std::filesystem::file_size(copy);
    request.inputs.push_back(std::move(ref));
  }
  request.build.split_threshold = static_cast<int>(int_arg(args, "split_threshold").value_or(1));
  request.build.filters = filters_of(ctx);
  ExceptionIds ids;
  request.exceptions = exception_inputs(&args, ws, ids);
  if (given(args, "report")) request.report = text_arg(args, "report");
  request.maps = ws.maps;
  request.graph = static_cast<int>(ws.graphs.size()) + 1;
  network::StepDocs docs;
  auto outcome = merge::merge_step(request, st.merges, docs, ctx);
  ws.graphs.add(outcome.value.network);
  const core::Json details = merge::step_details(outcome.value);   // 03: the merge key and its source
  for (const auto& [key, value] : details.items()) ws.details[key] = value;
  st.merge = std::move(outcome.value);
  return {};
}

// fb_rch (S4.01 to S4.22, 05). FuSION 1 parsed its -1 lists by hand and printed "Reachability Graph: N"
// (Linux/src/main.cc:2109-2234); 2.0 binds every id through the run's binding table (S2.03), reports an
// empty corridor as corridor.empty (S4.18, 01.C15), reuses a repeated corridor (05.C4), and reports where
// every observed gene falls (05.C9).
core::Outcome<void> run_fb_rch(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = run_maps(ws, ctx);
  const int network_graph = static_cast<int>(int_arg(args, "graph").value_or(0));
  const int network_id = network_graph == 0 ? ws.graphs.latest() : network_graph;
  const auto net = network_of(ws, network_id, "fb_rch");
  corridor::Request request;
  request.source = list_arg(args, "source");
  request.target = list_arg(args, "target");
  request.exclude = list_arg(args, "exclude");
  request.bound = static_cast<std::uint32_t>(int_arg(args, "bound").value_or(0));
  request.edges_to_target =
      corridor::parse_edges_to_target(text_or(args, "edges_to_target", "any")).value_or(corridor::EdgesToTarget::any);
  request.skip_subtypes = list_arg(args, "skip_subtypes");
  network::BindingTable& bindings = bindings_for(ws, network_id, net, ctx);
  const int id = static_cast<int>(ws.graphs.size()) + 1;
  // 05.C4: the same network and the same normalised request register the earlier corridor again.
  for (const auto& [earlier_id, info] : st.corridors) {
    if (info.network != network_id) continue;
    const auto earlier = corridor_of(ws, earlier_id, "fb_rch");
    if (!corridor::reusable(*earlier, *net, request)) continue;
    ws.graphs.add(earlier);
    corridor::report_reuse(*earlier, id, earlier_id, ctx);
    st.corridors[id] = StepState::Corridor{network_id, std::string(ctx.step), info.match};
    core::Outcome<void> result;
    if (earlier->empty()) result.report = core::Report{"corridor.empty", "graph " + std::to_string(id) + " is the empty corridor of graph " + std::to_string(earlier_id)};
    return result;
  }
  const corridor::Naming naming{&maps, network_id, id};
  auto outcome = corridor::compute(net, bindings, request, naming, ctx);
  const auto corridor = outcome.value;
  ws.graphs.add(corridor);
  std::vector<expression::ObservationMatch> match;
  if (ws.observations && corridor) {
    match = expression::match_observations(*ws.observations, corridor->view());   // 05.C9
    expression::save_match(match, st.observation_values, maps.symbols, st.interner, ctx);
  }
  if (corridor) corridor::save(*corridor, match, naming, ctx);
  st.corridors[id] = StepState::Corridor{network_id, std::string(ctx.step), std::move(match)};
  core::Outcome<void> result;
  result.report = outcome.report;
  return result;
}

}  // namespace fusion::batch
