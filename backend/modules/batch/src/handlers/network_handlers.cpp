// The network commands (section 02): start, rgx, wgx, sz and add.
#include <algorithm>

#include "state.hpp"

namespace fusion::batch {

namespace {
std::string shown(const std::optional<std::filesystem::path>& p, std::string_view none) {
  return p ? core::to_utf8(*p) : std::string(none);
}
}  // namespace

// start (S1.15; row 2B.4). FuSION 1 built a GraphManagerNew from [arg1, arg2 + " -1", arg3, arg4]
// (Linux/src/main.cc:1305-1317, GraphManagerNew.cc:7988-8181); a map it could not open became an empty
// map with a message. 2.0's check has refused such a file before the run (decision 0021), every map
// is read by 02's one reader (S1.18, S1.19), and the exception file joins mff's exception set (S1.17).
core::Outcome<void> run_start(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const auto id_map = path_arg(args, "id_map");
  const auto symbols = path_arg(args, "symbols");
  const auto names = path_arg(args, "pathway_names");
  // Every map the run uses, by path and SHA-256: start's files, and the shipped ones it leaves out (2B.4).
  core::Json record = core::Json::object();
  network::Maps maps = shipped_maps(ctx, record, !symbols, !names);
  maps.ids = network::IdMap::none();
  record["id_map"] = nullptr;
  if (id_map) {
    const auto copy = ctx.files.input(*id_map);
    maps.ids = network::IdMap::read(copy, ctx);
    record["id_map"] = map_record(*id_map, copy, false);
  }
  if (symbols) {
    const auto copy = ctx.files.input(*symbols);
    maps.symbols = network::LabelMap::read(copy, ctx);
    record["symbols"] = map_record(*symbols, copy, false);
  }
  if (names) {
    const auto copy = ctx.files.input(*names);
    maps.pathway_names = network::LabelMap::read(copy, ctx);
    record["pathway_names"] = map_record(*names, copy, false);
  }
  ws.details["maps"] = std::move(record);
  st.start_file = path_arg(args, "exception_file");
  ws.maps = std::make_shared<const network::Maps>(std::move(maps));
  ctx.log.info("start: id map " + shown(id_map, "none") + "; exception file " + shown(st.start_file, "none") +
               "; symbols " + shown(symbols, "shipped") + "; pathway names " + shown(names, "shipped"));
  return {};
}

// rgx (S2.12, 02's "FuSION network files", 03.C5). FuSION 1 parsed the file again
// (Linux/src/main.cc:1341-1390); 2.0 takes a file this run wrote from memory (01.C13, S2.26), compares a
// saved network's settings and exceptions and rebuilds when they differ (2A.4), reads a FuSION 1 file
// as legacy (S2.27), and runs a raw KGML through the one-file merge (03.C5).
core::Outcome<void> run_rgx(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = run_maps(ws, ctx);
  const auto file = *path_arg(args, "file");
  network::BuildSettings build;
  build.split_threshold = static_cast<int>(int_arg(args, "split_threshold").value_or(1));
  build.filters = filters_of(ctx);
  std::shared_ptr<const network::Network> net;
  std::string how;
  bool merged = false;
  if (const auto remembered = ws.written.get(file))
    if (const auto* n = std::get_if<std::shared_ptr<const network::Network>>(&*remembered)) {
      net = *n;
      how = ", from memory: this run wrote the file (S2.26)";
    }
  if (!net) {
    const auto copy = ctx.files.input(file);
    const std::optional<bool> tool_written = given(args, "tool_written") ? bool_arg(args, "tool_written") : std::nullopt;
    network::Format format = network::sniff(copy, tool_written, ctx).format;
    const std::string forced = text_or(args, "format", "auto");
    if (forced == "kgml") format = network::Format::kgml;
    else if (forced == "fusion") format = network::Format::fusion_network;
    else if (forced == "legacy") format = network::Format::legacy;
    else if (forced == "edges") format = network::Format::edge_table;
    ExceptionIds ids;
    const merge::ExceptionInputs in = exception_inputs(nullptr, ws, ids);
    switch (format) {
      case network::Format::fusion_network: {
        auto saved = network::read_fusion_network(copy, ctx);
        const merge::ExceptionSet exceptions = merge::assemble_exceptions(in, maps.ids, ctx);
        const auto diff = network::saved_network_diff(saved->info(), build, exceptions.ids(), maps.ids);
        if (diff.empty()) {
          net = std::move(saved);
        } else if (saved->provenance() && !saved->provenance()->docs.empty()) {
          const auto hit = st.merges.from_embedded(*saved, build, ctx);
          net = merge::split(*hit.merge, exceptions).network;
          how = ", rebuilt from its embedded sources: " + join(diff, "; ");
        } else {
          throw core::Failure("network.settings_mismatch",
                              core::to_utf8(file) + " was built under other settings and embeds no sources: " +
                                  join(diff, "; "));
        }
        break;
      }
      case network::Format::legacy: {
        const merge::ExceptionSet exceptions = merge::assemble_exceptions(in, maps.ids, ctx);
        net = network::read_legacy(copy, maps.ids, build, exceptions.ids(), ctx);
        how = ", read as FuSION 1's legacy file (S2.27)";
        break;
      }
      case network::Format::kgml:
      case network::Format::edge_table: {
        merge::MergeRequest request;
        network::SourceRef ref;
        ref.path = file;
        ref.format = format;
        ref.sha256 = core::Sha256::of_file(copy);
        ref.size = std::filesystem::file_size(copy);
        request.inputs.push_back(std::move(ref));
        request.build = build;
        request.exceptions = in;
        request.maps = ws.maps;
        request.graph = static_cast<int>(ws.graphs.size()) + 1;
        network::StepDocs docs;
        auto outcome = merge::merge_step(request, st.merges, docs, ctx);   // writes merged_graph.xml
        net = outcome.value.network;
        st.merge = std::move(outcome.value);
        merged = true;
        how = ", a one-file merge (03.C5)";
        break;
      }
    }
  }
  const int id = ws.graphs.add(net);
  if (!merged) {   // the saved step (09.C4)
    core::OutputFile out = ctx.files.create("network.xml");
    network::write_fusion_network(network::GraphView(net), out.out(), maps, network::WriteOptions{});
    out.commit();
  }
  ctx.log.info("rgx: graph " + std::to_string(id) + ": " + std::to_string(net->nodes().size()) + " nodes, " +
               std::to_string(net->edges().size()) + " edges" + how);
  return {};
}

// wgx (S2.25): FuSION's network format through OutputFile; FuSION 1 wrote with an ofstream and printed
// "Error: couldn't open file" on failure (Linux/src/main.cc:2585-2628).
core::Outcome<void> run_wgx(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const network::Maps& maps = run_maps(ws, ctx);
  const int graph = static_cast<int>(int_arg(args, "graph").value_or(0));
  const network::GraphView view = view_of(ws, graph);
  network::WriteOptions options;
  options.root_name = text_or(args, "tag", "KEGG");
  options.embed_sources = bool_arg(args, "embed_sources").value_or(true);
  const std::string name = *text_arg(args, "out");
  core::OutputFile out = create_output(ctx, name);
  network::write_fusion_network(view, out.out(), maps, options);
  out.commit();
  // 01.C13: a later rgx of this file takes the network from memory.
  const GraphEntry& entry = ws.graphs.get(graph);
  if (const auto* n = std::get_if<std::shared_ptr<const network::Network>>(&entry)) {
    ws.written.put(out.path(), *n);
    const auto p = core::path_from_utf8(name);
    if (p.is_absolute()) ws.written.put(p, *n);
  }
  ctx.log.info("wgx: graph " + std::to_string(graph == 0 ? ws.graphs.latest() : graph) + " written to " + name);
  return {};
}

// sz (S2.28): FuSION 1 printed node and edge counts (Linux/src/main.cc:1581-1641); 02 adds multi-gene nodes.
core::Outcome<void> run_sz(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const network::Maps& maps = run_maps(ws, ctx);
  const network::GraphView view = view_of(ws, static_cast<int>(int_arg(args, "graph").value_or(0)));
  const network::Summary summary = network::summarize(view);
  for (const auto& line : network::summary_lines(summary, view, maps)) ctx.log.info(line);
  return {};
}

// add (S2.30, 02.C17): an edge list transcribed into FuSION's marked format.
core::Outcome<void> run_add(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const network::Maps& maps = run_maps(ws, ctx);
  const auto edges = *path_arg(args, "edges");
  core::OutputFile out = create_output(ctx, *text_arg(args, "out"));
  network::add_edges(ctx.files.input(edges), out.out(), maps.ids, ctx);
  out.commit();
  return {};
}

}  // namespace fusion::batch
