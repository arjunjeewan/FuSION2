// Private to the batch module's handlers (01.C9): what they keep between lines, and how they read their
// bound arguments. Only the handlers' translation units include the step modules' headers.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../internal.hpp"
#include "fusion/batch/batch.hpp"
#include "fusion/corridor/corridor.hpp"
#include "fusion/enrichment/enrichment.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/merge/merge.hpp"
#include "fusion/network/network.hpp"
#include "fusion/results/model.hpp"
#include "fusion/solve/solve.hpp"

namespace fusion::batch {

struct StepState {
  core::Interner interner;                       // the run's ids (02.C3): values, observations, exceptions
  expression::ValueInputs values;                // each value input read once (04.C9)
  merge::MergeStore merges;                      // base merges held in memory (03.C3)
  std::map<int, std::unique_ptr<network::BindingTable>> bindings;   // per network graph id (02.C13)
  int latest_bindings = 0;
  std::optional<std::filesystem::path> start_file;   // start's exception file (S1.17)
  std::optional<merge::MergedNetwork> merge;     // the latest merge, for "mv edges_merged_filename" (01.C11)
  const expression::GeneValues* observation_values = nullptr;   // the values the latest cudf classified
  struct Corridor { int network = 0; std::string step; std::vector<expression::ObservationMatch> match; };
  std::map<int, Corridor> corridors;             // by graph id
  struct Solve {
    std::string name, step;                      // 07.C9: "solve", or a positional prefix
    int corridor = 0;
    std::shared_ptr<const solve::Results> results;
    std::shared_ptr<const results::SolveResult> published;
  };
  std::vector<Solve> solves;                     // batch order
  std::map<std::string, solve::CoexpressionTable> coexpression;   // parsed once per run (S5.15)
  std::vector<std::string> pages, plots;         // the run page's links: the explanation pages and Pareto
                                                 // images pex and plot_pareto wrote, relative to the run
                                                 // directory (07.C2; prompt log entry 387)

  const Solve* find_solve(std::string_view name_or_step) const;
  const Solve* latest_solve() const { return solves.empty() ? nullptr : &solves.back(); }
};

StepState& state_of(Workspace&);
const StepState* state_of(const Workspace&);
const network::Maps& run_maps(Workspace&, core::Context&);       // start's, or the shipped ones (2B.4)
network::Maps shipped_maps(core::Context&, core::Json& record, bool symbols, bool pathway_names);
core::Json map_record(const std::filesystem::path& named, const std::filesystem::path& copy, bool shipped);
const network::Maps& id_maps(const Workspace&);                  // start's, or none: for reading values and
                                                                 // lists, which use only the id map (02.C3)
network::BindingTable& bindings_for(Workspace&, int network_graph, const std::shared_ptr<const network::Network>&,
                                    core::Context&);

// Bound arguments, as the check left them.
bool given(const BoundArgs&, std::string_view name);
std::optional<std::string> text_arg(const BoundArgs&, std::string_view name);
std::string text_or(const BoundArgs&, std::string_view name, std::string fallback);
std::optional<std::int64_t> int_arg(const BoundArgs&, std::string_view name);
std::optional<double> real_arg(const BoundArgs&, std::string_view name);
std::optional<bool> bool_arg(const BoundArgs&, std::string_view name);
std::vector<std::string> list_arg(const BoundArgs&, std::string_view name);
std::optional<std::filesystem::path> path_arg(const BoundArgs&, std::string_view name);   // inputs: resolved

// Graphs (S2.01): the network a graph id names, and a view of any graph.
std::shared_ptr<const network::Network> network_of(const Workspace&, int graph, std::string_view command);
std::shared_ptr<const corridor::Corridor> corridor_of(const Workspace&, int graph, std::string_view command);
network::GraphView view_of(const Workspace&, int graph);

// An output name (rows 2B.3, 4A.11): relative or omitted, in the step directory; absolute, there as well.
core::OutputFile create_output(core::Context&, const std::string& name);

// The relation filters a step reads from the run (S2.11, S2.31, 02.C7).
network::RelationFilters filters_of(const core::Context&);

// The exception set's inputs from the run (03.C1, 03.C2, 01.C17); used by mff and rgx.
struct ExceptionIds { std::vector<core::GeneId> sources, targets, fscs; };
merge::ExceptionInputs exception_inputs(const BoundArgs*, Workspace&, ExceptionIds& ids);

// The handlers (01.C9), by command.
core::Outcome<void> run_help(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_start(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_rgx(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_wgx(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_sz(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_add(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_cudf(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_sth(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_gsea(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_mff(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_fb_rch(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_pathz3(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_psp(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_pex(const BoundArgs&, Workspace&, core::Context&);
core::Outcome<void> run_plot_pareto(const BoundArgs&, Workspace&, core::Context&);
void run_legacy_move(const LegacyMove&, const Workspace&, core::Context&);
void finish_run(Workspace&, run::Run&, const RunEnding&);

// Section 11's blocks (versions.cpp): fusion --version --json, and run.json beside it.
core::Json versions_block();
core::Json platform_block();
core::Json install_block();
core::Json environment_block();
core::Json probes_block(std::span<const core::deps::Probe>);
struct DataCheck { core::Json json; bool damaged = false; };
DataCheck data_block();
std::vector<core::deps::Probe> installation_probes();
void describe_run(run::Run&, const Plan&);

}  // namespace fusion::batch
