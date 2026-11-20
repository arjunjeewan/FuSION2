// The command table's checkers (01.C9): each declares its inputs and outputs (01.C12), its tool needs, the
// features its inputs turn on (decision 0021), and the cross-line facts of 05.C7 and 07.C9. They call no
// step module: a value's content is checked by its step.
#include <algorithm>

#include "internal.hpp"

namespace fusion::batch {
namespace {

const BoundValue* find(const BoundArgs& args, std::string_view name) {
  for (const auto& v : args.values)
    if (v.spec && v.spec->name == name) return &v;
  return nullptr;
}
bool given(const BoundArgs& args, std::string_view name) {
  const auto* v = find(args, name);
  return v && v->given && v->has_value;
}
std::string text(const BoundArgs& args, std::string_view name) {
  const auto* v = find(args, name);
  if (!v || !v->has_value) return {};
  if (const auto* s = std::get_if<std::string>(&v->value)) return *s;
  return value_text(v->value);
}
std::vector<std::string> strings(const BoundArgs& args, std::string_view name) {
  const auto* v = find(args, name);
  if (!v || !v->has_value) return {};
  if (const auto* l = std::get_if<std::vector<std::string>>(&v->value)) return *l;
  if (const auto* s = std::get_if<std::string>(&v->value)) return {*s};
  return {};
}
void inputs(const BoundArgs& args, CheckContext& ctx, std::initializer_list<std::string_view> names) {
  for (const auto name : names)
    if (const auto* v = find(args, name)) ctx.input(*v);
}

// An output name (rows 2B.3, 4A.11): relative or omitted, it lands in the step directory; absolute, it is
// honoured as well. Only absolute ones can meet another line's input.
void output(const BoundArgs& args, CheckContext& ctx, std::string_view name) {
  const auto* v = find(args, name);
  if (!v || !v->has_value) return;
  const auto p = core::path_from_utf8(std::get<std::string>(v->value));
  ctx.output(p);
}

// A value-input's id_map may be a file or the word "symbols" (04.C1, row 1.12).
void id_map_input(const BoundArgs& args, CheckContext& ctx, std::string_view name) {
  const auto* v = find(args, name);
  if (!v || !v->given || !v->has_value || std::get<std::string>(v->value) == "symbols") return;
  if (const auto* s = find_setting(v->spec->setting); s && s->type == core::Type::input_file) {
    ctx.input(*v);
    return;
  }
  BoundValue copy = *v;   // gsea's id_map, a text setting: resolved here (01.C2)
  copy.value = generic(resolve_input(ctx.state().batch_dir, std::get<std::string>(v->value)));
  static constexpr ArgSpec kAsFile{"id_map", "network.id_map"};
  copy.spec = &kAsFile;
  ctx.input(copy);
}

std::string strip_suffix(std::string_view file, std::string_view suffix) {
  if (file.size() > suffix.size() && file.ends_with(suffix)) return std::string(file.substr(0, file.size() - suffix.size()));
  return {};
}

// 07.C9: a positional file binds to the solve whose prefix wrote it; none is parsed back.
const SolveFacts* solve_for_file(CheckContext& ctx, std::string_view file) {
  const auto& solves = ctx.state().solves;
  for (const auto suffix : {std::string_view("_solutions.txt"), std::string_view("_relaxations.txt")}) {
    const auto prefix = strip_suffix(file, suffix);
    if (prefix.empty()) continue;
    for (auto it = solves.rbegin(); it != solves.rend(); ++it)
      if (it->name == prefix) return &*it;
  }
  return nullptr;
}

}  // namespace

void check_help(const BoundArgs& args, CheckContext& ctx) {
  if (given(args, "command") && !command_table().find(text(args, "command")))
    ctx.error("batch.bad_value", "unknown command '" + text(args, "command") + "'", "command");
}

void check_start(const BoundArgs& args, CheckContext& ctx) {
  inputs(args, ctx, {"id_map", "exception_file", "symbols", "pathway_names"});
}

void check_cudf(const BoundArgs& args, CheckContext& ctx) {
  inputs(args, ctx, {"values", "not_de"});
  id_map_input(args, ctx, "id_map");
  output(args, ctx, "up_out");
  output(args, ctx, "down_out");
  if (text(args, "up_out") == text(args, "down_out"))   // S3.10: no list overwrites the other
    ctx.error("output.clash", "up_out and down_out are the same file " + text(args, "up_out"), "down_out",
              text(args, "down_out"));
}

void check_sth(const BoundArgs& args, CheckContext& ctx) {
  inputs(args, ctx, {"values"});
  id_map_input(args, ctx, "id_map");
  if (given(args, "plot")) {
    output(args, ctx, "plot");
    ctx.require(core::deps::Requirement{.id = "gnuplot", .min_version = "5.4", .features = {"pngcairo"}});
  }
}

void check_labels(const BoundArgs&, CheckContext& ctx) {
  ctx.error("batch.unknown_command", "labels runs only through fusion --query (2B.6)");
}

void check_gsea(const BoundArgs& args, CheckContext& ctx) {
  inputs(args, ctx, {"values", "candidates"});
  id_map_input(args, ctx, "id_map");
  output(args, ctx, "shortlist");
  // S7.16: the table is <stem>.gsea.tsv beside the shortlist.
  if (given(args, "shortlist")) {
    auto table = core::path_from_utf8(text(args, "shortlist"));
    table.replace_extension();
    table += ".gsea.tsv";
    ctx.output(table);
  }
}

void check_mff(const BoundArgs& args, CheckContext& ctx) {
  auto& state = ctx.state();
  if (given(args, "list")) inputs(args, ctx, {"list"});
  else if (!state.shortlist)
    ctx.error("batch.bad_argument", "names no list, and no earlier gsea line makes a shortlist (01.C16)",
              "list");
  inputs(args, ctx, {"not_merged", "confirmed_up", "confirmed_down"});
  output(args, ctx, "report");
  // 03.C1: the up and down genes come from the latest cudf; a merge before any keeps none apart.
  if (!state.any_cudf && !given(args, "observations"))
    ctx.warning("batch.mff_before_cudf", "runs before any cudf, so no up or down gene is kept apart (03.C1)");
  // 01.C17, row 2A.9: a batch that names no FSC keeps its own exception file.
  if (!state.no_fsc_noted && ctx.facts().fscs.empty() && !given(args, "fsc")) {
    state.no_fsc_noted = true;
    ctx.note("batch.no_fsc",
             "the batch names no FSC, so the FSC is not kept apart by itself; name it with fsc= on pathz3 or mff "
             "to apply S2.13 (01.C17)");
  }
}

void check_rgx(const BoundArgs& args, CheckContext& ctx) { inputs(args, ctx, {"file"}); }

void check_wgx(const BoundArgs& args, CheckContext& ctx) { output(args, ctx, "out"); }

void check_add(const BoundArgs& args, CheckContext& ctx) {
  inputs(args, ctx, {"edges"});
  output(args, ctx, "out");
}

void check_fb_rch(const BoundArgs& args, CheckContext& ctx) {
  if (given(args, "source") && strings(args, "source").empty())
    ctx.error("batch.bad_argument", "the source list is empty (S4.01)", "source");
  if (given(args, "target") && strings(args, "target").empty())
    ctx.error("batch.bad_argument", "the target list is empty (S4.01)", "target");
}

void check_pathz3(const BoundArgs& args, CheckContext& ctx) {
  auto& state = ctx.state();
  // S5.28: mode 1 (the PO points file) was dropped; points= asks for listed points (06.C9).
  if (given(args, "mode") && std::get<std::int64_t>(find(args, "mode")->value) != 0)
    ctx.error("batch.bad_value", "mode 1 was dropped in FuSION 2.0 (S5.28); ask for points with points=<n>,<e>",
              "mode");
  const CorridorFacts* corridor = nullptr;
  if (const auto* g = find(args, "graph"); g && g->has_value) {
    const auto it = state.corridors.find(static_cast<int>(std::get<std::int64_t>(g->value)));
    if (it != state.corridors.end()) corridor = &it->second;
  }
  // 05.C7: the solve's bound must equal the corridor's.
  if (corridor && given(args, "bound")) {
    const auto bound = std::get<std::int64_t>(find(args, "bound")->value);
    if (bound != corridor->bound)
      ctx.error("batch.bad_value",
                "path bound " + std::to_string(bound) + " differs from the corridor's bound " +
                    std::to_string(corridor->bound) + " (line " + std::to_string(corridor->line) + "; 05.C7)",
                "bound");
  }
  inputs(args, ctx, {"pairs", "up", "down", "essential_nodes", "avoid_nodes", "active_nodes", "inactive_nodes",
                     "nonrelaxed_nodes", "confirmed_up", "confirmed_down", "essential_edges", "avoid_edges",
                     "nonrelaxed_edges", "relaxed_nodes", "relaxed_edges", "coexpression_csv"});
  // 06.C10: in and out need an FSC.
  const auto cases = strings(args, "cases");
  const bool fsc = !strings(args, "fsc").empty();
  if (!fsc && std::any_of(cases.begin(), cases.end(), [](const std::string& c) { return c != "free"; }))
    ctx.error("batch.bad_argument", "the cases in and out need an FSC; give fsc= or cases=free (06.C10)",
              "cases");
  // Decision 0021: the families the given inputs switch on, settled before the solve (S5.03).
  const bool off = given(args, "constraints") && !std::get<bool>(find(args, "constraints")->value);
  const bool csv = given(args, "coexpression_csv");
  if (csv && given(args, "coexpression_threshold"))
    ctx.feature("coexpression", !off, off ? "constraints=0 turns it off" : "a CSV and a threshold are given");
  if (csv && given(args, "frozen_threshold"))
    ctx.feature("frozen", !off, off ? "constraints=0 turns it off" : "a CSV and a frozen threshold are given");
  // Row 1.21, 07.C9: a positional prefix sets legacy_prefix, names the solve and writes FuSION 1's files.
  SolveFacts solve;
  solve.name = given(args, "legacy_prefix") ? text(args, "legacy_prefix") : "solve";
  solve.step = state.step_id;
  solve.line = args.line;
  solve.cases = cases.empty() ? std::vector<std::string>{fsc ? "in" : "free"} : cases;
  if (cases.empty() && fsc) solve.cases.push_back("out");
  if (corridor) {
    if (!corridor->source.empty()) solve.source = corridor->source.front();
    if (!corridor->target.empty()) solve.target = corridor->target.front();
  }
  if (given(args, "legacy_prefix")) {
    const auto prefix = text(args, "legacy_prefix");
    ctx.output(core::path_from_utf8(prefix + "_solutions.txt"));
    ctx.output(core::path_from_utf8(prefix + "_relaxations.txt"));
  }
  state.solves.push_back(std::move(solve));
}

void check_psp(const BoundArgs& args, CheckContext& ctx) {
  for (const auto name : {"solution", "src", "tgt", "mode", "points", "prefix"})
    if (!given(args, name)) ctx.error("batch.bad_argument", "missing argument '" + std::string(name) + "'");
  if (given(args, "solution") && !solve_for_file(ctx, text(args, "solution"))) {
    const auto& solves = ctx.state().solves;
    const bool by_name = std::any_of(solves.begin(), solves.end(),
                                     [&](const SolveFacts& s) { return s.name == text(args, "solution"); });
    if (!by_name)
      ctx.error("batch.unmatched_file", "solution file " + text(args, "solution") +
                                            " matches no solve of this batch (07.C9)",
                "solution", text(args, "solution"));
  }
  if (text(args, "static") == "svg") ctx.require(core::deps::Requirement{.id = "dot"});
}

void check_pex(const BoundArgs& args, CheckContext& ctx) {
  const auto& solves = ctx.state().solves;
  if (args.positional()) {
    // 07.C9: the four files bind by prefix; 07.C12: src and tgt must be the bound solve's endpoints.
    std::vector<const SolveFacts*> bound;
    for (const auto name : {"incl_soln", "incl_relax", "excl_soln", "excl_relax"}) {
      if (!given(args, name)) continue;
      const SolveFacts* s = solve_for_file(ctx, text(args, name));
      if (!s) {
        ctx.error("batch.unmatched_file", std::string(name) + " " + text(args, name) +
                                              " matches no solve of this batch (07.C9)",
                  name, text(args, name));
        continue;
      }
      bound.push_back(s);
    }
    for (const auto* s : bound) {
      if (given(args, "src") && !s->source.empty() && text(args, "src") != s->source)
        ctx.error("batch.bad_value", "src " + text(args, "src") + " is not the source " + s->source + " of solve " +
                                         s->name + " (07.C12)");
      if (given(args, "tgt") && !s->target.empty() && text(args, "tgt") != s->target)
        ctx.error("batch.bad_value", "tgt " + text(args, "tgt") + " is not the target " + s->target + " of solve " +
                                         s->name + " (07.C12)");
    }
    inputs(args, ctx, {"lfc", "pathmap", "po_png"});
  } else {
    for (const auto& c : strings(args, "cases"))
      if (c == "both" || c == "incl" || c == "excl")
        ctx.error("batch.bad_value", "case '" + c + "' is FuSION 1's positional form; name in, out or free (row 1.6)",
                  "cases");
    for (const auto& name : strings(args, "solve"))
      if (std::none_of(solves.begin(), solves.end(), [&](const SolveFacts& s) { return s.name == name || s.step == name; }))
        ctx.error("batch.bad_argument", "names solve '" + name + "', which no earlier pathz3 line makes", "solve");
    inputs(args, ctx, {"po_png"});
  }
  if (text(args, "static") == "svg") ctx.require(core::deps::Requirement{.id = "dot"});
}

void check_plot_pareto(const BoundArgs& args, CheckContext& ctx) {
  const auto& solves = ctx.state().solves;
  for (const auto& name : strings(args, "solve"))
    if (std::none_of(solves.begin(), solves.end(), [&](const SolveFacts& s) { return s.name == name || s.step == name; }))
      ctx.error("batch.bad_argument", "names solve '" + name + "', which no earlier pathz3 line makes", "solve");
}

}  // namespace fusion::batch
