// fusion/results/explain.hpp: the explanation pages, view files, edge tables, static export, the page
// library's one copy, and the run page's HTML (section 07, "Explanation export"; 07.C2, S6.07, S6.08,
// S6.17, S6.18). The declarations up to run_page_html() are section 07's outline; everything marked
// "Added" extends it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/results/model.hpp"
#include "fusion/results/site.hpp"
#include "fusion/results/view.hpp"

namespace fusion::results {

struct ExplainOptions {
  std::vector<Case> cases;                      // empty: every solved case
  PointSelector points;                         // default all
  PathFilter paths;                             // the page's initial view, the edge table and the static export
  std::size_t witness = 1;                      // shown first; the page offers the others
  bool edge_table = false;                      // S6.08
  bool static_svg = false;                      // S6.07, through Graphviz dot (07.C5)
  std::string prefix;                           // file-name prefix; positional lines pass "pex" or "psp_w"
  std::optional<std::filesystem::path> absolute_prefix;   // an absolute out_prefix, honoured too (2B.3)
  std::optional<SiteOptions> site;              // S6.19
};

void explain(std::span<const SolveResult* const>, const ExplainOptions&, core::Context&);

std::string view_json(const PointView&, const SolveResult&);            // deterministic
std::string page_html(std::string_view view_json, std::string_view lib_href, std::string_view title);
std::string edge_table_tsv(const WitnessView&, const SolveResult&, const PathFilter&);
std::string dot_source(const WitnessView&, const SolveResult&, const PathFilter&);    // escaped (S6.07)

enum class RunEnd : std::uint8_t { finished, failed, stopped };   // Added: how the run ended
struct RunPageInputs {                          // what run gathers from run.json at the end of a run
  std::vector<const SolveResult*> solves;       // every results.json the run wrote or read
  std::vector<std::string> pages, plots;        // relative paths of the pages and Pareto images
  // Added: the page opens with the run's name and how it ended, so a failed run, or one with no
  // solve, still says something (Phase 3, prompt log entry 387).
  std::string run_name;
  RunEnd end = RunEnd::finished;
  std::string end_note;                         // a failure's batch line and message
};
std::string run_page_html(const RunPageInputs&);   // the run page; run writes it once (1.4)

// ---- Added beyond the outline ----

inline constexpr std::string_view view_format = "fusion.view";
inline constexpr int view_version = 1;

// The same as view_json(), with the page's settings: the initial path filter, which witness the page
// shows first (1-based), and whether it shows the edge table (S6.08).
std::string view_json(const PointView&, const SolveResult&, const PathFilter&, std::size_t first_witness,
                      bool edge_table);

// page_html() naming the view file in its <noscript> text ("The same explanation is in in_n1_e0.json.").
std::string page_html(std::string_view view_json, std::string_view lib_href, std::string_view title,
                      std::string_view json_name);

// dot_source() with the graph's name, "in_n1_e0" for the page of that name.
std::string dot_source(const WitnessView&, const SolveResult&, const PathFilter&, std::string_view graph_name);

// The page's file stem, "[<prefix>_][<solve>_]<case>_n<n>_e<e>" (07.C2, 1.6); the solve's name is added
// when one pex line draws several solves that share a case.
std::string page_stem(std::string_view prefix, std::string_view solve_name, Case, Point);

// A positional pex line's slots (07.C2): its incl files name their solve's pages "in", its excl files
// "out", whatever case the solve ran. One entry per solve passed to explain(), in that order.
struct SlotNames { std::vector<Case> per_solve; };
void explain(std::span<const SolveResult* const>, const ExplainOptions&, const SlotNames&, core::Context&);

// 07.C12: a positional pex line's src and tgt must be the bound solve's source and target. Throws
// core::Failure results.endpoint_mismatch, naming `command` ("pex (line 269)") and the solve.
void check_endpoints(const SolveResult&, std::string_view src, std::string_view tgt, std::string_view command);

}  // namespace fusion::results
