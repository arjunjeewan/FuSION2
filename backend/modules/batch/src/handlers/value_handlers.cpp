// The commands that read per-gene values: cudf and suggest_thresholds (section 04), and gsea (section 08).
#include <fstream>

#include "fusion/core/gnuplot.hpp"
#include "state.hpp"

namespace fusion::batch {

namespace {

// 04.C1, 04.C9, row 1.23: a value input is its file and its reading arguments, read once per run.
expression::ValueInput value_input(const BoundArgs& args, const Workspace& ws) {
  expression::ValueInput in;
  in.path = *path_arg(args, "values");
  if (given(args, "id_column")) in.id_column = text_arg(args, "id_column");
  if (given(args, "value_column")) in.value_column = text_arg(args, "value_column");
  if (given(args, "id_map")) {
    const auto map = *text_arg(args, "id_map");
    in.id_map = map == "symbols" ? map : generic(resolve_input(ws.batch_dir, map));
  }
  if (text_or(args, "repeats", "classify_each") == "max_abs") in.repeats = expression::RepeatRule::max_abs;
  return in;
}

const expression::GeneValues& read_values(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = id_maps(ws);   // values use the id map only (02.C3)
  const expression::ValueInput in = value_input(args, ws);
  return st.values.read(in, ctx.files.input(in.path), maps, st.interner, ctx);
}

}  // namespace

// cudf (S3.01 to S3.11). FuSION 1 read "gene value" pairs with stringstream >> and wrote every line
// whose value passed, repeats included (Linux/src/main.cc:2629-2699); 2.0 reads with 04's one reader,
// removes Not DE genes first (04.C4), fails on a repeated id in two classes (S3.08), and reports
// observations.none when nothing passes (04.C5), which skips the point's later lines (01.C15).
core::Outcome<void> run_cudf(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = id_maps(ws);
  const expression::GeneValues& values = read_values(args, ws, ctx);
  std::vector<core::GeneId> not_de;
  const auto not_de_file = path_arg(args, "not_de");
  if (not_de_file) not_de = expression::read_id_list(ctx.files.input(*not_de_file), maps.ids, st.interner);
  expression::ClassifyRequest request;
  request.thresholds = expression::Thresholds{*real_arg(args, "up"), *real_arg(args, "down")};
  request.not_de = not_de;
  request.interner = &st.interner;
  request.not_de_file = not_de_file;
  auto outcome = expression::classify(values, request, ctx);
  expression::ListNames names;
  names.up = text_or(args, "up_out", "up_reg");
  names.down = text_or(args, "down_out", "down_reg");
  expression::save_observations(outcome.value, values, names, st.interner, ctx);
  auto observations = std::make_shared<const expression::Observations>(std::move(outcome.value));
  ws.observations = observations;
  st.observation_values = &values;
  // 01.C13: a positional pathz3 that names these lists takes the observations from memory.
  for (const auto& name : {names.up, names.down}) {
    const auto p = core::path_from_utf8(name);
    if (p.is_absolute()) ws.written.put(p, observations);
  }
  core::Outcome<void> result;
  result.report = outcome.report;
  return result;
}

// suggest_thresholds (S3.15 to S3.19, 04.C6, 04.C7): FuSION 1's suggest_thresholds.py, in C++.
core::Outcome<void> run_sth(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  const expression::GeneValues& values = read_values(args, ws, ctx);
  std::optional<expression::Thresholds> current;
  if (const auto up = real_arg(args, "current_up"), down = real_arg(args, "current_down"); up && down)
    current = expression::Thresholds{*up, *down};
  const expression::Suggestion suggestion = expression::suggest_thresholds(values, current);
  expression::save_suggestion(suggestion, values, ctx);
  if (const auto plot = text_arg(args, "plot"); plot && given(args, "plot")) {
    core::OutputFile png = create_output(ctx, *plot);
    const auto tmp = png.path().filename().string() + ".tmp";
    core::GnuplotJob job;
    job.script = expression::histogram_script(suggestion, current, tmp);
    job.output = tmp;
    job.working_dir = png.path().parent_path();
    const core::GnuplotResult result = core::run_gnuplot(job);
    if (!result.output_written)
      throw core::Failure("expression.gnuplot_failed",
                          "gnuplot exited with status " + std::to_string(result.exit_code) + " and wrote no " +
                              *plot + (result.stderr_text.empty() ? std::string() : ": " + result.stderr_text));
    // The PNG gnuplot wrote under the temporary name becomes the output.
    std::ifstream in(job.working_dir / tmp, std::ios::binary);
    png.out() << in.rdbuf();
    in.close();
    std::error_code ec;
    std::filesystem::remove(job.working_dir / tmp, ec);
    png.commit();
  }
  return {};
}

// gsea (S7.01 to S7.19, 08). FuSION 1 ran gsea_select_pathways.py through python3 (Linux/src/main.cc:
// 5777-5830); 2.0 runs 08's C++ port of gseapy 1.3.1's prerank, and an empty selection is a reported
// result (S7.03).
core::Outcome<void> run_gsea(const BoundArgs& args, Workspace& ws, core::Context& ctx) {
  StepState& st = state_of(ws);
  const network::Maps& maps = id_maps(ws);
  const expression::GeneValues& values = read_values(args, ws, ctx);
  const auto candidates = *path_arg(args, "candidates");
  std::optional<std::filesystem::path> kgml_dir;
  if (given(args, "kgml_dir")) kgml_dir = resolve_input(ws.batch_dir, *text_arg(args, "kgml_dir"));
  const auto base = network::list_base(candidates, kgml_dir, std::nullopt);
  const network::PathList list = network::read_input_list(ctx.files.input(candidates), base, ctx);
  network::StepDocs step_docs;
  std::vector<std::shared_ptr<const network::PathwayDoc>> docs;
  for (const auto& line : list.lines) {
    ctx.stop.throw_if_stopped();
    docs.push_back(step_docs.get(ctx.files.input(line.path), maps.ids, ctx));
  }
  enrichment::Options options;
  options.selector = enrichment::parse_selector(*text_arg(args, "selector"));
  // S7.14: 0 means none; each keep id goes through the run's id map, as every other id (08's "Keep list",
  // entry 086; 02.C3), so "hsa:3725" keeps the pathways that hold hsa3725.
  for (const auto& id : list_arg(args, "keep"))
    if (id != "0") options.keep.emplace_back(st.interner.str(maps.ids.canonical(id, st.interner)));
  options.settings.seed = static_cast<std::uint64_t>(int_arg(args, "seed").value_or(42));
  options.settings.permutations = static_cast<std::uint32_t>(int_arg(args, "permutations").value_or(1000));
  enrichment::check_settings(options.settings);
  options.shortlist_name = *text_arg(args, "shortlist");
  options.candidates = core::to_utf8(candidates);
  auto outcome = enrichment::gsea(values, list.lines, docs, list.base, filters_of(ctx), st.interner, options, ctx);
  ws.shortlist = outcome.value;
  if (outcome.value) {   // 2B.10: the base relative lines resolved against; S7.11: the scores' seed and count
    ws.details["base"] = core::to_utf8(outcome.value->base);
    ws.details["seed"] = outcome.value->settings.seed;
    ws.details["permutations"] = outcome.value->settings.permutations;
  }
  // 01.C13: mff reads the shortlist from memory while its file is unchanged.
  const auto named = core::path_from_utf8(options.shortlist_name);
  if (named.is_absolute()) ws.written.put(named, outcome.value);
  ws.written.put(ctx.files.step_dir() / named.filename(), outcome.value);
  core::Outcome<void> result;
  result.report = outcome.report;
  return result;
}

}  // namespace fusion::batch
