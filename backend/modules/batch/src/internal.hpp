// Private to the batch module: helpers shared by the language's translation units, the checkers the
// command table points at, and the check's simulation of one point. Nothing here names a step module.
#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/batch/batch.hpp"

namespace fusion::batch {

// Diagnostics (row 1.9): every problem with its dotted code, line and column.
void add_diagnostic(core::CheckReport&, core::Severity, int line, int column, std::string_view code,
                    std::string message, std::string_view command = {}, std::string_view setting = {},
                    std::string_view path = {});
std::string join(std::span<const std::string> items, std::string_view separator);
std::string join_views(std::span<const std::string_view> items, std::string_view separator);
std::string_view type_name(core::Type) noexcept;
std::string_view need_name(core::Need) noexcept;
std::string_view source_name(core::Source) noexcept;
std::filesystem::path resolve_input(const std::filesystem::path& base, std::string_view text);   // 01.C2
std::string generic(const std::filesystem::path&);    // '/' separators, lexically normal

// The checkers the table points at (01.C9). Each sees its bound line and the point's simulation.
void check_start(const BoundArgs&, CheckContext&);
void check_cudf(const BoundArgs&, CheckContext&);
void check_sth(const BoundArgs&, CheckContext&);
void check_gsea(const BoundArgs&, CheckContext&);
void check_mff(const BoundArgs&, CheckContext&);
void check_rgx(const BoundArgs&, CheckContext&);
void check_wgx(const BoundArgs&, CheckContext&);
void check_add(const BoundArgs&, CheckContext&);
void check_fb_rch(const BoundArgs&, CheckContext&);
void check_pathz3(const BoundArgs&, CheckContext&);
void check_psp(const BoundArgs&, CheckContext&);
void check_pex(const BoundArgs&, CheckContext&);
void check_plot_pareto(const BoundArgs&, CheckContext&);
void check_labels(const BoundArgs&, CheckContext&);
void check_help(const BoundArgs&, CheckContext&);

// One solve the check has seen: what later lines may bind to (07.C9, 05.C7).
struct SolveFacts {
  std::string name;                      // "solve", or a positional pathz3's prefix (07.C9)
  std::string step;                      // "pathz3", "pathz3-2"
  int line = 0;
  std::string source, target;            // the corridor's endpoints, as the fb_rch line gave them
  std::vector<std::string> cases;        // in, out, free
};
struct CorridorFacts { int graph = 0; int line = 0; std::int64_t bound = 0;
                       std::vector<std::string> source, target; };

struct CheckContext::State {
  core::CheckReport* report = nullptr;
  std::optional<std::uint32_t> point;    // the diagnostics' point; empty without a sweep
  std::filesystem::path batch_dir;
  BatchFacts facts;
  // the simulation (01.C16): what exists before each line
  std::vector<ValueKind> graphs;         // kind of graph id i + 1
  std::map<int, CorridorFacts> corridors;   // by graph id
  bool observations = false, shortlist = false, any_cudf = false;
  std::vector<SolveFacts> solves;        // in batch order
  std::map<std::string, int> step_counts;   // for step ids: fb_rch, fb_rch-2 (09.C2)
  // files (01.C12)
  struct Output { std::string path; int line; std::string step; };
  std::vector<Output> outputs;           // declared outputs, absolute ones by path
  std::vector<std::pair<std::string, int>> exec_texts;   // earlier exec lines' text, with their line
  std::vector<std::pair<std::string, int>> inputs_seen;  // non-produced inputs, for clashes
  std::set<std::string> checked_paths;   // each distinct path is checked once
  // this line
  std::string step_id;
  std::vector<run::InputSpec> inputs;
  std::vector<core::deps::Requirement> tools;
  std::vector<std::pair<std::string, int>> line_outputs;   // this line's outputs (path, column)
  bool no_fsc_noted = false;
};

std::string step_id(CheckContext::State&, std::string_view command);   // fb_rch, fb_rch-2, ...

}  // namespace fusion::batch
