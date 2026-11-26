// fusion/testkit/oracles.hpp: the oracles of section 12, each written from the spec and sharing no code
// with the module it checks (12.C14).
#pragma once

#include <filesystem>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/check.hpp"

namespace fusion::testkit {

// ---- S2.22's second oracle (12.C14): the merge with the exceptions, from S2.06 to S2.21 and section 03's
// union, with its own KGML reading (pugixml).
struct MergeOracleSettings {
  int threshold = 1;                     // S2.07: a box collapses when it holds more ids than this
  bool keep_indirect = false;            // S2.11 default: discard
  bool keep_ecrel = true;                // S2.31 default: keep
};
CanonGraph exception_merge(std::span<const std::filesystem::path> kgmls, const std::set<std::string>& exceptions,
                           const MergeOracleSettings&);
// The same on KGML texts already in memory, for generated sets (12, "Merge then split").
CanonGraph exception_merge_texts(std::span<const std::string> kgml_texts, const std::set<std::string>& exceptions,
                                 const MergeOracleSettings&);

// ---- the corridor's walk definition (S4.04 to S4.11 with 05.C2), by enumerating every walk
struct WalkRules {                                                     // as the corridor suite tests them
  std::vector<std::string> exclude;                                    // ids (S4.06), with their complexes
  enum class ToTarget { any, activation, expression } to_target = ToTarget::any;   // S4.05
  std::vector<std::string> skip_subtypes{"association", "component", "dissociation", "mapping"};   // S4.04
};
// Every edge of every walk of at most `bound` edges from a source to a target that never re-enters a
// source, never leaves a target and never follows a self-loop, an excluded node or a skipped edge, and
// the ends of those edges. Explicit walks: meant for small graphs (V <= 12, b <= 5).
CanonCorridor walk_corridor(const CanonGraph&, std::span<const std::string> sources,
                            std::span<const std::string> targets, int bound, const WalkRules& = {});

// ---- the frontier of a case by trying every assignment (3^V node states, 2^E selections)
// The Pareto-minimal costs of the valid assignments, each raised to box_lo (06.C20), within box_hi.
// Every point is certified: the enumeration is exact. Meant for V <= 7 and E <= 10.
Frontier exhaustive_frontier(const CanonCorridor&, const CaseSpec&, Semantics, Point box_lo, Point box_hi);

}  // namespace fusion::testkit
