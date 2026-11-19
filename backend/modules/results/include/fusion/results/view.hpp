// fusion/results/view.hpp: the one explanation renderer's view model, point selectors and the path
// filter (section 07, "The explanation view: the one renderer"; S6.03 to S6.06, S6.13 to S6.17, S6.26).
// The declarations up to kept_by() are section 07's outline; everything marked "Added" extends it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/network/network.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {

struct PointSelector { enum class Kind : std::uint8_t { all, least, one } kind = Kind::all; Point at; };
PointSelector parse_point_selector(std::string_view);   // "all", "least", "<n>,<e>"; "7,1x" -> core::Failure (S6.03)
std::vector<std::size_t> select_points(const CaseResult&, const PointSelector&);

enum class PathMode : std::uint8_t { subgraph, all, shortest, alt };
struct PathFilter {
  PathMode mode = PathMode::subgraph;
  std::int32_t window = 2;                      // alt only (S6.01)
  std::optional<network::NodeKey> from, to;     // psp's src and tgt; default the solve's source and target
};
PathFilter parse_path_filter(std::string_view);         // "subgraph", "all", "shortest", "alt", "alt:W" with W >= 0 (S6.01)

enum class Draw : std::uint8_t { honoured, removed, flipped };   // S6.15 cases 1 to 3
enum Role : std::uint8_t { role_source = 1, role_target = 2, role_fsc = 4 };

struct ViewNode { NodeIx node; bool present, active, relaxed; std::int32_t level; std::uint8_t roles; };

struct ViewEdge {                               // one drawn edge (S6.14, S6.15)
  NodeIx source, target;
  network::Effect effect;                       // the head drawn: the group's effect; when flipped, the kept effect
  std::optional<network::Effect> replaced;      // flipped only: the relaxed effect
  Draw draw;
  bool in_witness;                              // false: relaxed but not selected (S6.15)
  bool ecrel;                                   // a member edge is ECrel (S2.31)
  bool on_route;                                // on a sign-consistent route (S5.13)
  std::optional<std::int32_t> slack;            // through-length minus L over selected edges (S6.04)
  std::vector<EdgeIx> members;                  // every edge it stands for, both effects' when flipped (S6.08)
};

struct ParkedNode { NodeIx node; std::string reason; };    // S6.13

struct WitnessView {                            // one recorded witness, ready to draw
  Point counts;
  std::vector<ViewNode> nodes;                  // present nodes, sorted by key
  std::vector<ViewEdge> edges;                  // sorted by (source key, target key, effect)
  std::vector<ParkedNode> parked;               // relaxed nodes outside the witness, by label
  std::optional<std::int32_t> shortest;         // L
  std::string route_note;
};

struct PointView {                              // one view file: a point and every recorded witness (S6.02)
  std::string solve; Case kind; Point at;
  Enumeration enumeration; bool certified;
  std::string title, subtitle, certification_note, missing_reason;
  std::vector<WitnessView> witnesses;           // empty for a requested point that is not sat
};

PointView build_view(const SolveResult&, const CaseResult&, const FrontierPoint&);
PointView build_view(const SolveResult&, const CaseResult&, const RequestedPoint&);   // 06.C9
std::vector<bool> kept_by(const SolveResult&, const WitnessView&, const PathFilter&);   // per ViewEdge

// ---- Added beyond the outline ----

// select_points() indexes CaseResult::points, or CaseResult::requested_points when a points request
// (06.C9) left the frontier empty; this says which.
bool selects_requested(const CaseResult&) noexcept;

std::string_view path_mode_name(PathMode) noexcept;               // "subgraph", "all", "shortest", "alt"
std::string path_filter_text(const PathFilter&);                  // "subgraph", "alt:2", ...
std::string_view draw_name(Draw) noexcept;                        // "honoured", "removed", "flipped"

// One witness drawn on its own, as build_view() draws each of a point's witnesses.
WitnessView build_witness_view(const SolveResult&, const CaseResult&, const Witness&);

// The nodes the filter draws: the ends of the kept edges plus the filter's two endpoints when present
// (S6.04), sorted by key. `kept` is kept_by()'s answer for the same view and filter.
std::vector<NodeIx> drawn_nodes(const SolveResult&, const WitnessView&, const PathFilter&,
                                const std::vector<bool>& kept);

}  // namespace fusion::results
