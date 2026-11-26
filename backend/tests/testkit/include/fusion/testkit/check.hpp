// fusion/testkit/check.hpp: the independent witness checker (section 12, "Witness checker and
// re-scoring"; rule R1). It evaluates one assignment on a corridor under FuSION 1's semantics or under
// 2.0's decided rules, from the spec (S5.05 to S5.17) and the polarity table of S5.13, sharing no code
// with the solve. It re-derives everything the solve derives: the encoded nodes and edges, the observed
// sets, the counted genes, the FSC cases and the complex rule.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/fusion1.hpp"

namespace fusion::testkit {

// fusion1: S5.10 to S5.16 as FuSION 1 implements them. The encoded set is S5.10's forward search from
// the source that never expands the target, so self-loops and edges into the source count, and nodes
// kept only through them are encoded; one FSC, given through the lists as FuSION 1's batches do; the
// complexes of an avoided gene are allowed; only the source is forced active (S5.04).
// fusion2: the decided rules. The corridor is first trimmed as 05.C2 reads S4.11 (no self-loop, edge into
// a source or edge out of a target; then the keep rule of S4.09), so trimmed edges, the nodes kept only
// through them and isolated complex members (S4.12) are absent; FSC sets with the cases in, out and free
// (06.C10); every avoided gene, on the avoid list or as an out case's FSC, forbids every complex that
// contains it (entries 170 and 229); in the named form the source and the target are present and active (06.C16).
enum class Semantics { fusion1, fusion2 };

struct EdgeSpec {                                 // S5.06: subtypes compared as a set
  std::string src, tgt;                           // ids, as a list file gives them
  std::vector<std::string> subtypes;
};

struct CaseSpec {                                 // ids, resolved on the corridor by the checker
  std::vector<std::string> up, down, confirmed_up, confirmed_down;          // S5.07
  std::vector<std::string> essential, avoid, active, inactive, nonrelaxed;  // S5.05
  std::vector<EdgeSpec> essential_edges, avoid_edges, nonrelaxed_edges;     // S5.06
  std::vector<std::pair<std::string, std::string>> coexpressed;   // pairs above threshold (S5.15)
  std::optional<double> frozen_threshold;                         // S5.16: strict >
  std::vector<std::string> fsc;                                   // fusion2: a set (06.C10)
  enum class Case { in, out, free } solve_case = Case::free;      // fusion2: 06.C10's case names
  bool named = true;                                              // 06.C16 holds for the named form only
};

struct Assignment {                               // keys, so it moves between corridors
  std::vector<std::pair<std::string, bool>> present;              // node key, active
  std::vector<CanonEdgeKey> selected;
};

struct Verdict { bool valid = false; Point cost; std::vector<std::string> violations; };

// An id names a node of the corridor by its canonical key, by its module key (S2.07, S2.09), or as a
// member of a node that is not a complex (the family binding of S2.03); unresolved ids are dropped, as
// S5.05 drops them.
Verdict check(const CanonCorridor&, const CaseSpec&, const Assignment&, Semantics);

// A FuSION 1 witness as an assignment on a corridor: its nodes by FuSION 1's keys, its selected edges by
// endpoints and subtypes (every parallel edge that matches is selected, since FuSION 1's lines carry no
// relation type). A node or edge the corridor lacks keeps its FuSION 1 text, which check() then rejects.
Assignment from_f1(const CanonCorridor&, const f1::Witness&);

}  // namespace fusion::testkit
