// The command table (S1.11, S1.12, 01.C9) and the settings it points at (row 1.16).
// FuSION 1 kept three lists that disagreed: the if/else chain (Linux/src/main.cc:1305-5842), the listing
// (main.cc:889-911, which advertised fwd_bkwd_rch and missed upfx and create_up_down_file) and the GUI's
// blocks. One table now drives dispatch, help, named arguments, the check and fusion --commands.
// Argument names and defaults are the owning sections': 02 (start, rgx, wgx, sz, add, labels), 03 (mff),
// 04 (cudf, suggest_thresholds), 05 (fb_rch), 06 (pathz3), 07 (psp, pex, plot_pareto) and 08 (gsea).
#include <algorithm>
#include <array>
#include <limits>
#include <ostream>

#include "fusion/core/build_info.hpp"
#include "fusion/corridor/corridor.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/solve/solve.hpp"
#include "internal.hpp"

namespace fusion::batch {
namespace {

using core::Need;
using core::OnResume;
using core::SettingSpec;
using core::Type;

constexpr SettingSpec S(std::string_view module, std::string_view name, Type type, Need need,
                        std::string_view default_text, std::string_view help) {
  SettingSpec s{.name = name, .module = module, .type = type, .need = need};
  s.default_text = default_text;
  s.help = help;
  return s;
}
constexpr SettingSpec no_results(SettingSpec s) { s.affects_results = false; return s; }
constexpr SettingSpec may_change(SettingSpec s) { s.on_resume = OnResume::may_change; return s; }
constexpr SettingSpec at_least(SettingSpec s, double min) { s.min = min; return s; }
constexpr SettingSpec within(SettingSpec s, double min, double max) { s.min = min; s.max = max; return s; }
constexpr SettingSpec one_of(SettingSpec s, std::span<const std::string_view> c) { s.choices = c; return s; }
constexpr SettingSpec debug_only(SettingSpec s) { s.debug = true; s.affects_results = false; return s; }

constexpr std::string_view kFormats[] = {"auto", "kgml", "fusion", "legacy", "edges"};
constexpr std::string_view kIndirect[] = {"discard", "keep"};
constexpr std::string_view kEcrel[] = {"keep", "exclude"};
constexpr std::string_view kMaplink[] = {"skip", "keep"};
constexpr std::string_view kRepeats[] = {"classify_each", "max_abs"};
constexpr std::string_view kEdgesToTarget[] = {"any", "activation", "expression"};   // or 0, 1, 2 (05.C8)
constexpr std::string_view kCases[] = {"in", "out", "free"};                          // 06.C10
constexpr std::string_view kPexCases[] = {"in", "out", "free", "both", "incl", "excl"};   // positional:
                                                                                      // both, incl, excl (1.6)
constexpr std::string_view kRetry[] = {"automatic", "unknown", "none"};               // 06.C8
constexpr std::string_view kStatic[] = {"none", "svg"};                               // 07.C5

constexpr double kU64 = 9.2e18;   // an int64 holds the value; the bound keeps parse_value in range

// The settings this table names that no module exports yet (row 1.16): 01's own, and the arguments of
// the commands whose modules keep no settings() table. corridor, expression, solve and run export theirs,
// and registry() takes those first (09's "The settings registry").
constexpr SettingSpec kOwn[] = {
    // 01's own and 09's run-level settings (01's Settings, 09's table)
    no_results(S("batch", "command", Type::text, Need::optional, {}, "the command to describe")),
    S("gsea", "candidates", Type::input_list, Need::required, {}, "the candidate pathway list (S7.06)"),
    S("gsea", "id_column", Type::text, Need::optional, {}, "the id column of a table (04.C1)"),
    S("gsea", "id_map", Type::text, Need::optional, {}, "an id map file, or symbols (04.C1)"),
    S("gsea", "keep", Type::text, Need::optional, {}, "gene ids whose pathways are kept (S7.14)"),
    within(S("gsea", "permutations", Type::integer, Need::defaulted, "1000", "permutations (08.C8)"), 1, 100000),
    one_of(S("gsea", "repeats", Type::choice, Need::defaulted, "classify_each", "repeated ids (04.C3)"), kRepeats),
    within(S("gsea", "seed", Type::integer, Need::defaulted, "42", "the permutation seed (S7.11)"), 0, kU64),
    S("gsea", "selector", Type::text, Need::required, {}, "fdr:Q, pval:P, nes:X, top:N, joined by commas (S7.13)"),
    no_results(S("gsea", "shortlist", Type::output_name, Need::required, {}, "the shortlist file (S7.15)")),
    S("gsea", "value_column", Type::text, Need::optional, {}, "the value column of a table (04.C1)"),
    S("gsea", "values", Type::input_file, Need::required, {}, "the per-gene value file (S7.04)"),
    S("merge", "confirmed_down", Type::input_file, Need::optional, {}, "confirmed down genes (03.C2)"),
    S("merge", "confirmed_up", Type::input_file, Need::optional, {}, "confirmed up genes (03.C2)"),
    S("merge", "fsc", Type::text, Need::optional, {}, "the FSC; by default the batch's pathz3 lines (01.C17)"),
    S("merge", "list", Type::input_list, Need::optional, {}, "the files to merge; by default the latest shortlist"),
    S("merge", "not_merged", Type::input_file, Need::optional, {}, "ids never merged (S1.17)"),
    no_results(S("merge", "observations", Type::text, Need::optional, {}, "the observations whose genes stay apart")),
    no_results(S("merge", "report", Type::output_name, Need::optional, {}, "the merge report (S2.24)")),
    S("merge", "source", Type::text, Need::optional, {}, "the source; by default the batch's fb_rch lines (01.C17)"),
    S("merge", "start_file", Type::input_file, Need::optional, {}, "FuSION 1's exception file (S1.17)"),
    S("merge", "target", Type::text, Need::optional, {}, "the target; by default the batch's fb_rch lines (01.C17)"),
    no_results(S("network", "edges", Type::input_file, Need::required, {}, "the edge list (S2.30)")),
    no_results(S("network", "embed_sources", Type::boolean, Need::defaulted, "true", "embed the sources (02.C4)")),
    S("network", "file", Type::input_file, Need::required, {}, "the network file (S2.12)"),
    one_of(S("network", "format", Type::choice, Need::defaulted, "auto", "overrides detection (02.C1)"), kFormats),
    no_results(at_least(S("network", "graph", Type::integer, Need::defaulted, "0", "a graph id; 0 is the latest (S2.01)"), 0)),
    one_of(S("network", "ecrel", Type::choice, Need::defaulted, "keep", "ECrel relations (S2.31)"), kEcrel),
    S("network", "id_map", Type::input_file, Need::optional, {}, "the inter-database id map (S1.16)"),
    one_of(S("network", "indirect", Type::choice, Need::defaulted, "discard", "indirect relations (S2.11)"), kIndirect),
    no_results(S("network", "kgml_dir", Type::text, Need::optional, {}, "where relative list lines resolve (02.C12)")),
    no_results(S("network", "map", Type::input_file, Need::optional, {}, "a symbol map (2B.6)")),
    one_of(S("network", "maplink", Type::choice, Need::defaulted, "skip", "maplink relations (02.C7)"), kMaplink),
    no_results(S("network", "out", Type::output_name, Need::required, {}, "the file to write (2B.3)")),
    no_results(S("network", "pathway_names", Type::input_file, Need::optional, {}, "the pathway-name map (S1.19)")),
    at_least(S("network", "split_threshold", Type::integer, Need::defaulted, "1", "the split threshold (S2.07)"), 0),
    no_results(S("network", "symbols", Type::input_file, Need::optional, {}, "the gene-symbol map (S1.18)")),
    no_results(S("network", "tag", Type::text, Need::defaulted, "KEGG", "the root's name (S2.25)")),
    no_results(S("network", "tool_written", Type::boolean, Need::optional, {}, "FuSION 1's y|n flag (S2.12)")),
    no_results(S("results", "cases", Type::choice, Need::optional, {}, "the cases drawn; every solved case by default")),
    no_results(S("results", "edge_table", Type::boolean, Need::defaulted, "false", "write the edge table (S6.08)")),
    no_results(S("results", "excl_relax", Type::text, Need::optional, {}, "FuSION 1's file; binds by prefix (07.C9)")),
    no_results(S("results", "excl_soln", Type::text, Need::optional, {}, "FuSION 1's file; binds by prefix (07.C9)")),
    no_results(S("results", "font", Type::text, Need::defaulted, "Liberation Sans", "the plot's font (S6.23)")),
    no_results(S("results", "incl_relax", Type::text, Need::optional, {}, "FuSION 1's file; binds by prefix (07.C9)")),
    no_results(S("results", "incl_soln", Type::text, Need::optional, {}, "FuSION 1's file; binds by prefix (07.C9)")),
    no_results(S("results", "lfc", Type::input_file, Need::optional, {}, "FuSION 1's value file (07.C12)")),
    no_results(S("results", "mode", Type::text, Need::optional, {}, "subgraph, all, shortest or alt[:W] (S6.01)")),
    no_results(S("results", "pathmap", Type::input_file, Need::optional, {}, "FuSION 1's pathway-name map (07.C12)")),
    no_results(S("results", "paths", Type::text, Need::defaulted, "subgraph", "subgraph, all, shortest or alt[:W] (S6.04)")),
    no_results(S("results", "points", Type::text, Need::defaulted, "all", "all, least or <n>,<e> (S6.03)")),
    no_results(S("results", "po_png", Type::input_file, Need::optional, {}, "the Pareto image for the site (S6.19)")),
    no_results(S("results", "prefix", Type::text, Need::optional, {}, "the file-name prefix (S6.01)")),
    no_results(S("results", "replace", Type::boolean, Need::defaulted, "false", "replace the site entry (S6.19)")),
    no_results(S("results", "run_key", Type::text, Need::optional, {}, "the site's key (S6.19)")),
    no_results(S("results", "run_title", Type::text, Need::optional, {}, "the site's run title (S6.19)")),
    no_results(S("results", "site_dir", Type::text, Need::optional, {}, "the site directory (S6.19)")),
    no_results(S("results", "site_title", Type::text, Need::defaulted, "FuSION explanation subgraphs", "the site's title (S6.19)")),
    no_results(S("results", "solution", Type::text, Need::optional, {}, "FuSION 1's solution file; binds by prefix (07.C9)")),
    no_results(S("results", "solve", Type::text, Need::optional, {}, "the solves; the latest by default (07.C9)")),
    no_results(S("results", "src", Type::text, Need::optional, {}, "the source id")),
    no_results(one_of(S("results", "static", Type::choice, Need::defaulted, "none", "a static SVG through dot (S6.07)"), kStatic)),
    no_results(S("results", "takeaway", Type::text, Need::optional, {}, "the site's takeaway (S6.19)")),
    no_results(S("results", "tgt", Type::text, Need::optional, {}, "the target id")),
    no_results(at_least(S("results", "witness", Type::integer, Need::defaulted, "1", "the witness shown first (S6.02)"), 1)),
    no_results(S("run", "jobs", Type::text, Need::defaulted, "auto", "the core budget for sweep points (01.C4)")),
    no_results(S("run", "label", Type::text, Need::computed, {}, "the run directory's label (01.C7)")),
    no_results(S("run", "points", Type::text, Need::optional, {}, "the sweep points that run (01.C14)")),
    no_results(S("run", "root", Type::text, Need::computed, {}, "where the run directory is made (01.C7)")),
    may_change(at_least(S("solve", "bound", Type::integer, Need::optional, {}, "FuSION 1's path bound; must equal the corridor's (05.C7)"), 0)),
    no_results(S("solve", "count", Type::integer, Need::optional, {}, "FuSION 1's count; ignored (06.C11)")),
    S("solve", "down", Type::input_file, Need::optional, {}, "FuSION 1's down list (S5.01)"),
    no_results(S("solve", "fold_change", Type::text, Need::optional, {}, "FuSION 1's value file; recorded only")),
    at_least(S("solve", "graph", Type::integer, Need::defaulted, "0", "the corridor; 0 is the latest graph (S2.01)"), 0),
    within(S("solve", "mode", Type::integer, Need::defaulted, "0", "FuSION 1's mode; only 0 (S5.28)"), 0, 1),
    S("solve", "pairs", Type::input_file, Need::optional, {}, "FuSION 1's connect pairs; must match the corridor"),
    S("solve", "relaxed_edges", Type::input_file, Need::optional, {}, "FuSION 1's relaxed edges; ignored (S5.05)"),
    S("solve", "relaxed_nodes", Type::input_file, Need::optional, {}, "FuSION 1's relaxed nodes; ignored (S5.05)"),
    S("solve", "up", Type::input_file, Need::optional, {}, "FuSION 1's up list (S5.01)"),
};

// The keys a run line or --set may set (row 2B.2): every registry key except run.debug (01.C8).
constexpr std::string_view kRunLevel[] = {
    "corridor.skip_subtypes", "gsea.permutations", "gsea.seed", "network.ecrel", "network.indirect",
    "network.maplink", "run.cache", "run.cache_dir", "run.jobs", "run.label", "run.log", "run.root",
};

constexpr std::string_view kFilters[] = {"network.indirect", "network.ecrel", "network.maplink"};

// ------------------------------------------------------------------ the commands

constexpr ValueKind kNeedGraph[] = {ValueKind::network};
constexpr ValueKind kNeedCorridor[] = {ValueKind::corridor};
constexpr ValueKind kNeedResults[] = {ValueKind::results};

// 11's requirements (rows 2A.1, 2B.15): not constexpr, since they hold strings; the table points at them.
const core::deps::Requirement kShell[] = {{.id = "sh"}};
const core::deps::Requirement kGnuplot[] = {{.id = "gnuplot", .min_version = "5.4", .features = {"pngcairo"}},
                                            {.id = "font:Liberation Sans", .kind = core::deps::Kind::font}};

// help (h)
constexpr std::string_view kHelpAliases[] = {"h"};
constexpr ArgSpec kHelpArgs[] = {{"command", "batch.command"}};
constexpr Slot kHelpSlots[] = {{SlotKind::optional_tail, "", ""}, {SlotKind::one, "command", ""}};
constexpr PositionalForm kHelpForms[] = {{"FuSION 1", kHelpSlots, nullptr, ""}};

// run: named only (01.C7); the planner reads it
constexpr ArgSpec kRunArgs[] = {{"root", "run.root"}, {"label", "run.label"}, {"log", "run.log"},
                                {"cache", "run.cache"}, {"cache_dir", "run.cache_dir"}, {"jobs", "run.jobs"}};

// start (02): S1.15, optional since 2B.4
constexpr ArgSpec kStartArgs[] = {{"id_map", "network.id_map"},
                                  {"exception_file", "merge.start_file", false, std::nullopt, true},
                                  {"symbols", "network.symbols"},
                                  {"pathway_names", "network.pathway_names"}};
constexpr Slot kStartSlots[] = {{SlotKind::one, "id_map", ""}, {SlotKind::one, "exception_file", "-1"},
                                {SlotKind::one, "symbols", ""}, {SlotKind::one, "pathway_names", ""}};
constexpr PositionalForm kStartForms[] = {{"FuSION 1", kStartSlots, nullptr, ""}};

// cudf (04): S3.01
constexpr std::string_view kCudfAliases[] = {"create_up_down_file"};
constexpr ArgSpec kCudfArgs[] = {{"values", "expression.values"}, {"up", "expression.up"}, {"down", "expression.down"},
                                 {"up_out", "expression.up_out"}, {"down_out", "expression.down_out"},
                                 {"not_de", "expression.not_de"}, {"id_column", "expression.id_column"},
                                 {"value_column", "expression.value_column"}, {"id_map", "expression.id_map"},
                                 {"repeats", "expression.repeats"}};
constexpr Slot kCudfSlots[] = {{SlotKind::one, "values", ""}, {SlotKind::one, "up", ""}, {SlotKind::one, "down", ""},
                               {SlotKind::one, "up_out", ""}, {SlotKind::one, "down_out", ""}};
constexpr PositionalForm kCudfForms[] = {{"FuSION 1", kCudfSlots, nullptr, ""}};
constexpr Rule kCudfRules[] = {{RuleKind::less, "down", "up"}};   // S3.07

// suggest_thresholds (sth) (04): 04.C6
constexpr std::string_view kSthAliases[] = {"sth"};
constexpr ArgSpec kSthArgs[] = {{"values", "expression.values"}, {"current_up", "expression.current_up"},
                                {"current_down", "expression.current_down"}, {"plot", "expression.plot"},
                                {"id_column", "expression.id_column"}, {"value_column", "expression.value_column"},
                                {"id_map", "expression.id_map"}, {"repeats", "expression.repeats"}};
constexpr Slot kSthSlots[] = {{SlotKind::one, "values", ""}};
constexpr PositionalForm kSthForms[] = {{"2.0", kSthSlots, nullptr, ""}};
constexpr Rule kSthRules[] = {{RuleKind::requires_, "current_up", "current_down"},
                              {RuleKind::requires_, "current_down", "current_up"},
                              {RuleKind::less, "current_down", "current_up"}};

// labels (02): query only (2B.6)
constexpr ArgSpec kLabelsArgs[] = {{"map", "network.map"}};

// gsea (gsea_select_pathways) (08): S7.01, 08.C11
constexpr std::string_view kGseaAliases[] = {"gsea_select_pathways"};
constexpr ArgSpec kGseaArgs[] = {{"values", "gsea.values"}, {"candidates", "gsea.candidates"},
                                 {"selector", "gsea.selector"}, {"shortlist", "gsea.shortlist"},
                                 {"keep", "gsea.keep", true}, {"seed", "gsea.seed"},
                                 {"permutations", "gsea.permutations"}, {"kgml_dir", "network.kgml_dir"},
                                 {"id_column", "gsea.id_column"}, {"value_column", "gsea.value_column"},
                                 {"id_map", "gsea.id_map"}, {"repeats", "gsea.repeats"}};
constexpr Slot kGseaSlots[] = {{SlotKind::one, "values", ""}, {SlotKind::one, "candidates", ""},
                               {SlotKind::one, "selector", ""}, {SlotKind::one, "shortlist", ""},
                               {SlotKind::optional_tail, "", ""}, {SlotKind::one, "keep", "0"}};
constexpr PositionalForm kGseaForms[] = {{"FuSION 1", kGseaSlots, nullptr, ""}};

// mff (merge_from_file) (03): S2.16
constexpr std::string_view kMffAliases[] = {"merge_from_file"};
constexpr ArgSpec kMffArgs[] = {{"list", "merge.list", false, ValueKind::shortlist},
                                {"split_threshold", "network.split_threshold"},
                                {"tool_written", "network.tool_written"},
                                {"kgml_dir", "network.kgml_dir"},
                                {"source", "merge.source", true}, {"target", "merge.target", true},
                                {"fsc", "merge.fsc", true},
                                {"confirmed_up", "merge.confirmed_up", true},
                                {"confirmed_down", "merge.confirmed_down", true},
                                {"not_merged", "merge.not_merged"}, {"report", "merge.report"},
                                {"observations", "merge.observations", false, ValueKind::observations}};
bool mff_fusion1(std::span<const std::string> tokens) { return tokens.size() > 2 && tokens[2] == "xml"; }
constexpr Slot kMffSlots1[] = {{SlotKind::one, "list", ""}, {SlotKind::ignored, "", ""},
                               {SlotKind::one, "split_threshold", ""}, {SlotKind::one, "tool_written", ""},
                               {SlotKind::ignored_list, "", "-1"}};
constexpr Slot kMffSlots2[] = {{SlotKind::one, "list", ""}, {SlotKind::one, "split_threshold", ""},
                               {SlotKind::one, "tool_written", ""}};
constexpr PositionalForm kMffForms[] = {
    {"FuSION 1", kMffSlots1, mff_fusion1, "mff: FuSION 1's 'xml' token and map-file list are ignored"},
    {"2.0", kMffSlots2, nullptr, ""}};

// rgx (read_graph_xml) (02): S2.12
constexpr std::string_view kRgxAliases[] = {"read_graph_xml"};
constexpr ArgSpec kRgxArgs[] = {{"file", "network.file"}, {"tool_written", "network.tool_written"},
                                {"split_threshold", "network.split_threshold"}, {"format", "network.format"}};
constexpr Slot kRgxSlots[] = {{SlotKind::one, "file", ""}, {SlotKind::one, "tool_written", ""},
                              {SlotKind::one, "split_threshold", ""}};
constexpr PositionalForm kRgxForms[] = {{"FuSION 1", kRgxSlots, nullptr, ""}};

// wgx (write_graph_xml) (02): S2.25
constexpr std::string_view kWgxAliases[] = {"write_graph_xml"};
constexpr ArgSpec kWgxArgs[] = {{"graph", "network.graph"}, {"out", "network.out"}, {"tag", "network.tag"},
                                {"embed_sources", "network.embed_sources"}};
constexpr Slot kWgxSlots[] = {{SlotKind::one, "graph", ""}, {SlotKind::one, "out", ""}, {SlotKind::one, "tag", ""}};
constexpr PositionalForm kWgxForms[] = {{"FuSION 1", kWgxSlots, nullptr, ""}};

// sz (size) (02): S2.28
constexpr std::string_view kSzAliases[] = {"size"};
constexpr ArgSpec kSzArgs[] = {{"graph", "network.graph"}};
constexpr Slot kSzSlots[] = {{SlotKind::one, "graph", ""}};
constexpr PositionalForm kSzForms[] = {{"FuSION 1", kSzSlots, nullptr, ""}};

// add (02): S2.30
constexpr ArgSpec kAddArgs[] = {{"edges", "network.edges"}, {"out", "network.out"}};
constexpr Slot kAddSlots[] = {{SlotKind::one, "edges", ""}, {SlotKind::one, "out", ""}};
constexpr PositionalForm kAddForms[] = {{"FuSION 1", kAddSlots, nullptr, ""}};

// fb_rch (corridor) (05): S4.01, 05.C8
constexpr std::string_view kFbAliases[] = {"corridor"};
constexpr ArgSpec kFbArgs[] = {{"graph", "corridor.graph", false, ValueKind::network},
                               {"source", "corridor.source", true}, {"target", "corridor.target", true},
                               {"exclude", "corridor.exclude", true}, {"bound", "corridor.bound"},
                               {"edges_to_target", "corridor.edges_to_target"},
                               {"skip_subtypes", "corridor.skip_subtypes", true},
                               {"observations", "corridor.observations", false, ValueKind::observations}};
constexpr Slot kFbSlots[] = {{SlotKind::one, "graph", ""}, {SlotKind::list, "source", ""},
                             {SlotKind::list, "target", ""}, {SlotKind::list, "exclude", ""},
                             {SlotKind::one, "bound", ""}, {SlotKind::one, "edges_to_target", ""}};
constexpr PositionalForm kFbForms[] = {{"FuSION 1", kFbSlots, nullptr, ""}};

// pathz3 (get_path_with_z3) (06): S5.01, S5.02, row 1.7; FuSION 1's order (Linux/src/main.cc:4492-4532)
constexpr std::string_view kZ3Aliases[] = {"get_path_with_z3"};
constexpr ArgSpec kZ3Args[] = {
    {"graph", "solve.graph", false, ValueKind::corridor},
    {"fsc", "solve.fsc", true}, {"cases", "solve.cases", true},
    {"node_min", "solve.node_min"}, {"node_max", "solve.node_max"},
    {"edge_min", "solve.edge_min"}, {"edge_max", "solve.edge_max"},
    {"points", "solve.points"}, {"timeout", "solve.timeout"},
    {"incremental_timeout", "solve.incremental_timeout"}, {"budget", "solve.budget"},
    {"rlimit", "solve.rlimit"}, {"seed", "solve.seed"}, {"solutions", "solve.solutions"},
    {"threads", "solve.threads"}, {"retry", "solve.retry"}, {"answers_from", "solve.answers_from"},
    {"coexpression_csv", "solve.coexpression_csv"}, {"coexpression_threshold", "solve.coexpression_threshold"},
    {"frozen_threshold", "solve.frozen_threshold"}, {"constraints", "solve.constraints"},
    {"essential_nodes", "solve.essential_nodes"}, {"avoid_nodes", "solve.avoid_nodes"},
    {"active_nodes", "solve.active_nodes"}, {"inactive_nodes", "solve.inactive_nodes"},
    {"nonrelaxed_nodes", "solve.nonrelaxed_nodes"}, {"confirmed_up", "solve.confirmed_up"},
    {"confirmed_down", "solve.confirmed_down"}, {"essential_edges", "solve.essential_edges"},
    {"avoid_edges", "solve.avoid_edges"}, {"nonrelaxed_edges", "solve.nonrelaxed_edges"},
    {"legacy_prefix", "solve.legacy_prefix"},
    // FuSION 1's slots with no 2.0 name (06's "FuSION 1 positional lines")
    {"mode", "solve.mode", false, std::nullopt, true}, {"pairs", "solve.pairs", false, std::nullopt, true},
    {"bound", "solve.bound", false, std::nullopt, true}, {"up", "solve.up", false, std::nullopt, true},
    {"down", "solve.down", false, std::nullopt, true},
    {"relaxed_nodes", "solve.relaxed_nodes", false, std::nullopt, true},
    {"relaxed_edges", "solve.relaxed_edges", false, std::nullopt, true},
    {"fold_change", "solve.fold_change", false, std::nullopt, true},
    {"count", "solve.count", false, std::nullopt, true}};
constexpr Slot kZ3Slots[] = {
    {SlotKind::one, "graph", ""}, {SlotKind::one, "mode", ""}, {SlotKind::one, "pairs", ""},
    {SlotKind::one, "edge_min", ""}, {SlotKind::one, "node_min", ""}, {SlotKind::one, "edge_max", ""},
    {SlotKind::one, "node_max", ""}, {SlotKind::one, "bound", ""}, {SlotKind::one, "legacy_prefix", ""},
    {SlotKind::one, "up", ""}, {SlotKind::one, "down", ""}, {SlotKind::one, "essential_nodes", ""},
    {SlotKind::one, "avoid_nodes", ""}, {SlotKind::one, "essential_edges", ""}, {SlotKind::one, "avoid_edges", ""},
    {SlotKind::one, "active_nodes", ""}, {SlotKind::one, "inactive_nodes", ""}, {SlotKind::one, "confirmed_up", ""},
    {SlotKind::one, "confirmed_down", ""}, {SlotKind::one, "relaxed_nodes", ""},
    {SlotKind::one, "nonrelaxed_nodes", ""}, {SlotKind::one, "relaxed_edges", ""},
    {SlotKind::one, "nonrelaxed_edges", ""}, {SlotKind::one, "fold_change", ""}, {SlotKind::one, "count", ""},
    {SlotKind::one, "solutions", ""}, {SlotKind::one, "incremental_timeout", ""}, {SlotKind::one, "timeout", ""},
    {SlotKind::optional_tail, "", ""},
    {SlotKind::one, "coexpression_csv", ""}, {SlotKind::one, "coexpression_threshold", ""},
    {SlotKind::one, "frozen_threshold", ""}, {SlotKind::one, "constraints", ""}, {SlotKind::one, "seed", ""}};
constexpr PositionalForm kZ3Forms[] = {{"FuSION 1", kZ3Slots, nullptr, ""}};
constexpr Rule kZ3Rules[] = {{RuleKind::less_or_equal, "node_min", "node_max"},
                             {RuleKind::less_or_equal, "edge_min", "edge_max"},
                             {RuleKind::requires_, "coexpression_threshold", "coexpression_csv"}};

// psp (plot_solution_paths) (07): S6.01, 07.C9
constexpr std::string_view kPspAliases[] = {"plot_solution_paths"};
constexpr ArgSpec kPspArgs[] = {{"solution", "results.solution"}, {"src", "results.src"}, {"tgt", "results.tgt"},
                                {"mode", "results.mode"}, {"points", "results.points"}, {"prefix", "results.prefix"},
                                {"static", "results.static"}};
constexpr Slot kPspSlots[] = {{SlotKind::one, "solution", ""}, {SlotKind::one, "src", ""}, {SlotKind::one, "tgt", ""},
                              {SlotKind::one, "mode", ""}, {SlotKind::one, "points", ""}, {SlotKind::one, "prefix", ""}};
constexpr PositionalForm kPspForms[] = {{"FuSION 1", kPspSlots, nullptr, ""}};

// pex (plot_explanation) (07): S6.10, 07.C9, 07.C12
constexpr std::string_view kPexAliases[] = {"plot_explanation"};
constexpr ArgSpec kPexArgs[] = {
    {"solve", "results.solve", true, ValueKind::results}, {"cases", "results.cases", true},
    {"points", "results.points"}, {"paths", "results.paths"}, {"witness", "results.witness"},
    {"edge_table", "results.edge_table"}, {"static", "results.static"}, {"prefix", "results.prefix"},
    {"site_dir", "results.site_dir"}, {"run_key", "results.run_key"}, {"run_title", "results.run_title"},
    {"takeaway", "results.takeaway"}, {"site_title", "results.site_title"}, {"replace", "results.replace"},
    {"po_png", "results.po_png"},
    {"incl_soln", "results.incl_soln", false, std::nullopt, true},
    {"incl_relax", "results.incl_relax", false, std::nullopt, true},
    {"excl_soln", "results.excl_soln", false, std::nullopt, true},
    {"excl_relax", "results.excl_relax", false, std::nullopt, true},
    {"lfc", "results.lfc", false, std::nullopt, true}, {"pathmap", "results.pathmap", false, std::nullopt, true},
    {"src", "results.src", false, std::nullopt, true}, {"tgt", "results.tgt", false, std::nullopt, true}};
constexpr Slot kPexSlots[] = {
    {SlotKind::one, "incl_soln", "0"}, {SlotKind::one, "incl_relax", "0"}, {SlotKind::one, "excl_soln", "0"},
    {SlotKind::one, "excl_relax", "0"}, {SlotKind::one, "lfc", ""}, {SlotKind::one, "pathmap", ""},
    {SlotKind::one, "src", ""}, {SlotKind::one, "tgt", ""}, {SlotKind::one, "cases", ""},
    {SlotKind::one, "points", ""}, {SlotKind::one, "prefix", ""}, {SlotKind::optional_tail, "", ""},
    {SlotKind::one, "site_dir", ""}, {SlotKind::one, "run_key", ""}, {SlotKind::one, "po_png", ""}};
constexpr PositionalForm kPexForms[] = {{"FuSION 1", kPexSlots, nullptr, ""}};

// plot_pareto (07): S6.22
constexpr ArgSpec kParetoArgs[] = {{"solve", "results.solve", true, ValueKind::results}, {"font", "results.font"}};

}  // namespace

// ------------------------------------------------------------------ the table

namespace {
constexpr std::string_view kUpfxAliases[] = {"unique_prefix"};
constexpr CommandSpec kCommands[] = {
    {.name = "help", .aliases = kHelpAliases, .owner = "batch",
     .summary = "the command listing, or one command's arguments (S1.12)",
     .args = kHelpArgs, .forms = kHelpForms, .effects = {.needs_run_dir = false},
     .check = check_help},
    {.name = "let", .owner = "batch", .summary = "defines a variable: let <$name> [<value>] (S1.08)",
     .effects = {.needs_run_dir = false}, .step = false},
    {.name = "sweep", .owner = "batch",
     .summary = "one run per value: sweep <$name> <value>... (01.C3); not in this build yet",
     .step = false},
    {.name = "run", .owner = "batch",
     .summary = "the run directory's root and label, and run-level settings by key (01.C7)",
     .args = kRunArgs, .step = false},
    {.name = "load", .owner = "batch",
     .summary = "restores saved values: load <file>, or load run=<dir> step=<step> (01.C6); not in this build yet"},
    {.name = "exec", .owner = "batch",
     .summary = "runs the rest of the line in a POSIX shell (S1.14, 01.C10)", .tools = kShell},
    {.name = "exit", .owner = "batch", .summary = "ends the run (S1.13)", .step = false},
    {.name = "start", .owner = "network",
     .summary = "the maps: id map, exception file, symbols, pathway names (S1.15); optional (2B.4)",
     .args = kStartArgs, .forms = kStartForms, .effects = {.produces = ValueKind::maps},
     .check = check_start},
    {.name = "cudf", .aliases = kCudfAliases, .owner = "expression",
     .summary = "classifies per-gene values: up when value >= up, down when value <= down, up above down",
     .args = kCudfArgs, .forms = kCudfForms,
     .effects = {.produces = ValueKind::observations, .needs_run_dir = false}, .rules = kCudfRules,
     .check = check_cudf},
    {.name = "suggest_thresholds", .aliases = kSthAliases, .owner = "expression",
     .summary = "suggests up and down thresholds from the values (S3.15)", .args = kSthArgs, .forms = kSthForms,
     .effects = {.needs_run_dir = false}, .rules = kSthRules, .check = check_sth},
    {.name = "labels", .owner = "network", .summary = "id-label pairs of a symbol map; fusion --query only (2B.6)",
     .args = kLabelsArgs, .effects = {.needs_run_dir = false}, .check = check_labels},
    {.name = "gsea", .aliases = kGseaAliases, .owner = "enrichment",
     .summary = "GSEA prerank over the candidate pathways, and the shortlist (S7.01)",
     .args = kGseaArgs, .forms = kGseaForms, .effects = {.produces = ValueKind::shortlist},
     .check = check_gsea, .run_settings = kFilters},
    {.name = "mff", .aliases = kMffAliases, .owner = "merge",
     .summary = "merges the listed pathways into one network (S2.16)", .args = kMffArgs, .forms = kMffForms,
     .effects = {.produces = ValueKind::network, .creates_graph = true}, .check = check_mff,
     .run_settings = kFilters},
    {.name = "rgx", .aliases = kRgxAliases, .owner = "network", .summary = "reads one network file (S2.12)",
     .args = kRgxArgs, .forms = kRgxForms, .effects = {.produces = ValueKind::network, .creates_graph = true},
     .check = check_rgx, .run_settings = kFilters},
    {.name = "wgx", .aliases = kWgxAliases, .owner = "network", .summary = "writes a graph to a file (S2.25)",
     .args = kWgxArgs, .forms = kWgxForms, .check = check_wgx},
    {.name = "sz", .aliases = kSzAliases, .owner = "network", .summary = "prints a graph's size (S2.28)",
     .args = kSzArgs, .forms = kSzForms},
    {.name = "add", .owner = "network", .summary = "writes an edge list as a network file (S2.30)",
     .args = kAddArgs, .forms = kAddForms, .check = check_add},
    {.name = "fb_rch", .aliases = kFbAliases, .owner = "corridor",
     .summary = "the corridor from the source to the target within the bound (S4.01)",
     .args = kFbArgs, .forms = kFbForms, .effects = {.needs = kNeedGraph, .produces = ValueKind::corridor,
                                                     .creates_graph = true},
     .check = check_fb_rch},
    {.name = "pathz3", .aliases = kZ3Aliases, .owner = "solve",
     .summary = "the Pareto frontier of relaxations on a corridor, with Z3 (S5.01)",
     .args = kZ3Args, .forms = kZ3Forms, .effects = {.needs = kNeedCorridor, .produces = ValueKind::results},
     .rules = kZ3Rules, .check = check_pathz3},
    {.name = "psp", .aliases = kPspAliases, .owner = "results",
     .summary = "draws a solve's paths (S6.01)", .args = kPspArgs, .forms = kPspForms,
     .effects = {.needs = kNeedResults}, .check = check_psp},
    {.name = "pex", .aliases = kPexAliases, .owner = "results",
     .summary = "the explanation pages of a solve (S6.10)", .args = kPexArgs, .forms = kPexForms,
     .effects = {.needs = kNeedResults}, .check = check_pex},
    {.name = "plot_pareto", .owner = "results", .summary = "the Pareto plot of the latest solves (S6.22)",
     .args = kParetoArgs, .effects = {.needs = kNeedResults}, .tools = kGnuplot, .check = check_plot_pareto},
    {.name = "upfx", .aliases = kUpfxAliases, .owner = "batch", .summary = "dropped (S2.29)",
     .dropped = "upfx was dropped in FuSION 2.0; each run has its own directory (S2.29)"},
};
}  // namespace

// The modules' own tables first (corridor, expression, solve, run), then this table's (row 1.16).
std::span<const core::SettingSpec> registry() {
  static const std::vector<core::SettingSpec> all = [] {
    std::vector<core::SettingSpec> specs;
    for (const auto table : {corridor::settings(), expression::settings(), solve::settings(), run::settings()})
      specs.insert(specs.end(), table.begin(), table.end());
    for (const auto& own : kOwn) {
      const bool taken = std::any_of(specs.begin(), specs.end(), [&](const core::SettingSpec& s) {
        return s.module == own.module && s.name == own.name;
      });
      if (!taken) specs.push_back(own);
    }
    return specs;
  }();
  return all;
}

const core::SettingSpec* find_setting(std::string_view key) noexcept {
  const auto dot = key.find('.');
  if (dot == std::string_view::npos) return nullptr;
  const auto module = key.substr(0, dot), name = key.substr(dot + 1);
  for (const auto& s : registry())
    if (s.module == module && s.name == name) return &s;
  return nullptr;
}

std::span<const std::string_view> run_level_keys() { return kRunLevel; }

std::string_view kind_name(ValueKind kind) noexcept {
  switch (kind) {
    case ValueKind::maps: return "maps";
    case ValueKind::network: return "network";
    case ValueKind::corridor: return "corridor";
    case ValueKind::observations: return "observations";
    case ValueKind::shortlist: return "shortlist";
    case ValueKind::results: return "results";
  }
  return "?";
}

const CommandTable& command_table() {
  static const CommandTable table(kCommands);
  return table;
}

CommandTable::CommandTable(std::span<const CommandSpec> specs) : specs_(specs) {}

const CommandSpec* CommandTable::find(std::string_view name_or_alias) const {
  for (const auto& c : specs_) {
    if (c.name == name_or_alias) return &c;
    for (const auto a : c.aliases)
      if (a == name_or_alias) return &c;
  }
  return nullptr;
}

std::span<const CommandSpec> CommandTable::all() const { return specs_; }

// ------------------------------------------------------------------ help and --commands

namespace {

std::string slot_usage(const Slot& slot) {
  switch (slot.kind) {
    case SlotKind::one: return "<" + std::string(slot.arg) + ">";
    case SlotKind::list: return "<" + std::string(slot.arg) + ">... -1";
    case SlotKind::ignored: return "<ignored>";
    case SlotKind::ignored_list: return "<ignored>... -1";
    case SlotKind::optional_tail: return "[";
  }
  return {};
}

std::string form_usage(const PositionalForm& form) {
  std::string out;
  int open = 0;
  for (const auto& slot : form.slots) {
    if (slot.kind == SlotKind::optional_tail) {
      if (!out.empty()) out += ' ';
      out += '[';
      ++open;
      continue;
    }
    if (!out.empty() && out.back() != '[') out += ' ';
    out += slot_usage(slot);
  }
  out.append(static_cast<std::size_t>(open), ']');
  return out;
}

std::string head(const CommandSpec& c) {
  std::string h(c.name);
  if (!c.aliases.empty()) h += " (" + join_views(c.aliases, ", ") + ")";
  return h;
}

void column(std::ostream& out, const std::string& left, const std::string& right) {
  out << "  " << left;
  if (left.size() < 23) out << std::string(23 - left.size(), ' ');
  else out << "\n" << std::string(25, ' ');
  out << right << "\n";
}

std::string named_usage(const CommandSpec& c) {
  std::string out;
  for (const auto& a : c.args) {
    if (a.positional_only) continue;
    if (!out.empty()) out += ' ';
    out += std::string(a.name) + "=";
  }
  return out;
}

}  // namespace

void CommandTable::print_listing(std::ostream& out) const {
  out << "Commands:\n";
  for (const auto& c : specs_) {
    if (!c.dropped.empty()) continue;
    std::string usage;
    if (c.name == "let") usage = "<$name> [<value>]";
    else if (c.name == "exec") usage = "<shell text>";
    else if (c.name == "sweep") usage = "<$name> <value>... [label=<prefix>] | <$name>=<value>... [label=<text>]";
    else if (c.name == "load") usage = "<file> | run=<dir> step=<step>";
    else if (!c.forms.empty()) usage = form_usage(c.forms.front());
    column(out, head(c), usage);
    for (std::size_t i = 1; i < c.forms.size(); ++i) column(out, "", form_usage(c.forms[i]));
    const auto named = named_usage(c);
    if (!named.empty() && !c.forms.empty()) column(out, "", named);
    else if (!named.empty() && usage.empty()) column(out, "", named);
  }
}

void CommandTable::print_command(std::ostream& out, const CommandSpec& c) const {
  out << head(c) << ": " << c.summary << "\n";
  for (const auto& form : c.forms)
    out << "  " << c.name << " " << form_usage(form) << "    (" << form.origin << ")\n";
  if (!c.args.empty()) out << "Arguments:\n";
  for (const auto& a : c.args) {
    const auto* s = find_setting(a.setting);
    std::string left = std::string(a.name) + (a.positional_only ? "" : "=");
    std::string right;
    if (s) {
      right = std::string(type_name(s->type)) + (a.list ? " list" : "") + ", " + std::string(need_name(s->need));
      if (s->need == core::Need::defaulted) right += " (" + (s->default_text.empty() ? std::string("empty") : std::string(s->default_text)) + ")";
      if (a.takes) right += "; the latest " + std::string(kind_name(*a.takes)) + " by default";
      if (!s->choices.empty()) right += "; " + join_views(s->choices, ", ");
      right += "; " + std::string(a.setting);
      if (!s->help.empty()) right += ": " + std::string(s->help);
    }
    if (a.positional_only) right += " [positional only]";
    column(out, left, right);
  }
  for (const auto& r : c.rules) {
    const char* kind = r.kind == RuleKind::less ? "<" : r.kind == RuleKind::less_or_equal ? "<="
                       : r.kind == RuleKind::requires_ ? "requires" : "excludes";
    out << "  rule: " << r.a << " " << kind << " " << r.b << "\n";
  }
  if (!c.dropped.empty()) out << "  " << c.dropped << "\n";
}

nlohmann::json CommandTable::describe() const {
  using nlohmann::json;
  json commands = json::array();
  for (const auto& c : specs_) {
    json j;
    j["name"] = c.name;
    j["aliases"] = json::array();
    for (const auto a : c.aliases) j["aliases"].push_back(a);
    j["owner"] = c.owner;
    j["summary"] = c.summary;
    j["step"] = c.step;
    if (!c.dropped.empty()) j["dropped"] = c.dropped;
    j["needs_run_dir"] = c.effects.needs_run_dir;
    j["creates_graph"] = c.effects.creates_graph;
    j["produces"] = c.effects.produces ? json(kind_name(*c.effects.produces)) : json(nullptr);
    j["needs"] = json::array();
    for (const auto k : c.effects.needs) j["needs"].push_back(kind_name(k));
    j["arguments"] = json::array();
    for (const auto& a : c.args) {
      json arg;
      arg["name"] = a.name;
      arg["setting"] = a.setting;
      arg["list"] = a.list;
      arg["takes"] = a.takes ? json(kind_name(*a.takes)) : json(nullptr);
      arg["positional_only"] = a.positional_only;
      if (const auto* s = find_setting(a.setting)) {
        arg["type"] = type_name(s->type);
        arg["need"] = need_name(s->need);
        arg["default"] = s->need == core::Need::defaulted ? json(s->default_text) : json(nullptr);
        arg["affects_results"] = s->affects_results;
        arg["debug"] = s->debug;
        arg["on_resume"] = s->on_resume == core::OnResume::must_match ? "must_match" : "may_change";
        if (s->min != -std::numeric_limits<double>::infinity()) arg["min"] = s->min;
        if (s->max != std::numeric_limits<double>::infinity()) arg["max"] = s->max;
        if (!s->choices.empty()) {
          arg["choices"] = json::array();
          for (const auto ch : s->choices) arg["choices"].push_back(ch);
        }
        arg["help"] = s->help;
      }
      j["arguments"].push_back(std::move(arg));
    }
    j["forms"] = json::array();
    for (const auto& f : c.forms) {
      json form;
      form["origin"] = f.origin;
      form["usage"] = form_usage(f);
      form["warning"] = f.warning;
      form["slots"] = json::array();
      for (const auto& s : f.slots) {
        static constexpr const char* kinds[] = {"one", "list", "ignored", "optional_tail", "ignored_list"};
        form["slots"].push_back({{"kind", kinds[static_cast<int>(s.kind)]}, {"arg", s.arg},
                                 {"none_token", s.none_token}});
      }
      j["forms"].push_back(std::move(form));
    }
    j["rules"] = json::array();
    for (const auto& r : c.rules) {
      static constexpr const char* kinds[] = {"less", "less_or_equal", "requires", "excludes"};
      j["rules"].push_back({{"kind", kinds[static_cast<int>(r.kind)]}, {"a", r.a}, {"b", r.b}});
    }
    j["tools"] = json::array();
    for (const auto& t : c.tools) {
      json tool{{"id", t.id}, {"kind", t.kind == core::deps::Kind::font ? "font" : "program"},
                {"min_version", t.min_version}, {"features", json::array()}};
      for (const auto& f : t.features) tool["features"].push_back(f);
      j["tools"].push_back(std::move(tool));
    }
    j["run_settings"] = json::array();
    for (const auto k : c.run_settings) j["run_settings"].push_back(k);
    commands.push_back(std::move(j));
  }
  json run_settings = json::array();
  for (const auto key : kRunLevel) {
    const auto* s = find_setting(key);
    if (!s) continue;
    json r{{"key", key}, {"type", type_name(s->type)}, {"need", need_name(s->need)},
           {"default", s->need == core::Need::defaulted ? json(s->default_text) : json(nullptr)},
           {"affects_results", s->affects_results}, {"help", s->help}};
    if (!s->choices.empty()) {
      r["choices"] = json::array();
      for (const auto ch : s->choices) r["choices"].push_back(ch);
    }
    run_settings.push_back(std::move(r));
  }
  // "format" and "fusion" let the frontend check what it reads (10.C12's catalog).
  return json{{"format", 1}, {"fusion", std::string(core::build_info().version)}, {"commands", std::move(commands)},
              {"run_settings", std::move(run_settings)}};
}

}  // namespace fusion::batch
