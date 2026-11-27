// fusion/testkit/fusion1.hpp: readers of FuSION 1's files (section 12, "The test kit"), written from
// FuSION 1's formats as the spec describes them, sharing no code with 2.0's readers.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fusion/testkit/canon.hpp"

namespace fusion::testkit::f1 {

struct NodeState { std::string rep; bool present = false, active = false; };   // rep: FuSION 1's node key
struct EdgeKey {
  std::string src_rep, tgt_rep;
  std::vector<std::string> subtypes;        // as written, in stored order, without dummy_u_to_d
  auto operator<=>(const EdgeKey&) const = default;
};
struct Witness {                            // one solution of the solutions and relaxations files (S5.27)
  std::vector<NodeState> present_nodes;     // the solutions file's node lines
  std::vector<EdgeKey> selected_edges;      // the solutions file's edge lines
  std::vector<EdgeKey> relaxed_edges;       // the relaxations file's edge lines
  std::vector<std::string> relaxed_nodes;   // the relaxations file's node lines
};
struct PointRecord { Point point; std::optional<Witness> witness; std::string reason; };   // reason: "timeout", ...

// One query of a limits file (S5.24): its point, the search step ("diagonal", "smallest_n" or
// "smallest_e", section 06's phase names), the answer, the time FuSION 1 printed, and whether it was a
// cache hit ("Point e n already ...").
struct Check { Point point; std::string step; CheckOutcome answer = CheckOutcome::not_checked; double cpu_s = 0; bool cached = false; };
struct Limits {
  Point box_lo, box_hi;                     // the first line's window
  std::vector<Check> checks;                // checks and cache hits, in order
  std::vector<Point> found;                 // the "Point 1:" and "Point 2:" lines
};

struct ConsoleFacts {
  std::vector<std::pair<int, int>> up_down;          // "Up: 3 Down: 0", one per pathz3
  std::vector<std::pair<int, int>> corridor_sizes;   // "New graph 86 has 115 nodes 370 edges", one per fb_rch
  std::vector<int> coexpression_clauses, frozen_clauses;   // "[coexpression] emitted 1 clauses", ...
  // The same facts by pathz3 call, in batch order: each block runs from FuSION 1's echo of a pathz3 line
  // ("pathz3 0 0 ...") to the next, so a solve's counts stay with it when a family is off in another.
  struct Solve {
    std::optional<std::pair<int, int>> up_down;
    std::optional<int> coexpression_clauses, frozen_clauses;
  };
  std::vector<Solve> solves;
};

struct GseaRow {                            // one row of a .gsea.tsv table, by column name
  std::string term, pathway_name;
  int genes = 0;
  double es = 0, nes = 0, nom_p = 0, fdr_q = 0, fwer_p = 0;
  std::string tag_pct, gene_pct, lead_genes;
  bool selected = false;
  std::string kgml;
};

std::vector<Point> read_dat(const std::filesystem::path&);                      // sorted by n
std::vector<PointRecord> read_solutions(const std::filesystem::path& solutions,
                                        const std::filesystem::path& relaxations);
Limits read_limits(const std::filesystem::path&);
// A FuSION 1 graph file as FuSION 1's tool-written read builds it (S2.06, S2.12): nodes only for the
// entries a kept relation reaches, directly or as a component of a reached group; indirect relations
// dropped (S2.11); no duplicate test, reverse edges, added tags or component edges; pathways from the
// relation's attribute. A later entry whose key names an existing node joins it without its other ids (S2.08).
CanonGraph read_written_graph(const std::filesystem::path&);
std::vector<std::string> read_ids(const std::filesystem::path&);               // up_reg, down_reg, shortlists
std::vector<GseaRow> read_gsea_table(const std::filesystem::path&);
ConsoleFacts read_console(const std::filesystem::path&);
// S5.15 with its fix: columns found by name, ':' removed, each cell split on spaces; every cross pair of
// a row scoring strictly above the threshold.
std::vector<std::pair<std::string, std::string>> read_coexpression(const std::filesystem::path& csv, double threshold);

}  // namespace fusion::testkit::f1
