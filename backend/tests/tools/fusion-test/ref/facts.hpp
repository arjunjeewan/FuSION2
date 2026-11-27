// The facts the reference runner compares (section 12, "Comparison levels"; the C4 note's facts.cpp): for one
// case, FuSION 1's from its recorded files and 2.0's from a chained run directory or from the pinned stages,
// each as the kit's types; the case's batch as the batch module binds it; and the signals the expected
// differences' predicates read. Internal to fusion-test ref.
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/batch/batch.hpp"
#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/check.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "ref/case.hpp"

namespace fusion::reftest {

// Section 12's stages, with the observations row of its comparison table (C4.4).
enum class Stage { classify, exceptions, gsea, merge, corridor, observations, solve_inputs, solve };
inline constexpr Stage kStages[] = {Stage::classify, Stage::exceptions, Stage::gsea, Stage::merge,
                                    Stage::corridor, Stage::observations, Stage::solve_inputs, Stage::solve};
std::string_view name(Stage) noexcept;
std::optional<Stage> parse_stage(std::string_view) noexcept;
enum class Mode { chained, pinned, verify };
std::string_view name(Mode) noexcept;
std::optional<Mode> parse_mode(std::string_view) noexcept;

// ---- the case's batch, parsed and bound by the batch module with no file checks (its corpus test's way)
struct BatchLine {
  std::string command;                                 // canonical: "cudf", "pathz3", ...
  int number = 0;
  const batch::BoundArgs* args = nullptr;
};
struct BatchView {
  std::shared_ptr<const batch::Plan> plan;             // owns the bound arguments
  std::vector<BatchLine> lines;                        // the step lines of the batch's first point, in order
  std::filesystem::path write_dir;                     // where the batch writes: the cudf line's up list's folder
  const BatchLine* first(std::string_view command) const;
  std::vector<const BatchLine*> all(std::string_view command) const;
};
BatchView read_batch(const std::filesystem::path& batch_file);   // throws core::Failure("ref.batch")

bool given(const BatchLine&, std::string_view arg);
std::optional<std::string> text(const BatchLine&, std::string_view arg);
std::optional<std::filesystem::path> path(const BatchLine&, std::string_view arg);
std::optional<std::int64_t> integer(const BatchLine&, std::string_view arg);
std::optional<double> real(const BatchLine&, std::string_view arg);
std::optional<bool> flag(const BatchLine&, std::string_view arg);
std::vector<std::string> list(const BatchLine&, std::string_view arg);

// One FuSION 1 solve of the batch and the pathz3 line that made it. FuSION 1's case names follow the
// legacy prefix (the C4 note's "Chained"): temp_w is in, temp_wo is out, and a batch's one solve is free.
// The pinned solve names the FSC as 06.C10's cases do: in the in case, the ids that only the with-FSC
// line's essential list holds; in the out case, the ids that only the without-FSC line's avoid list holds.
// Each case keeps the other lists as its line gives them, so the named FSC replaces the lists' FSC, as
// section 06 says the named form does ("FuSION 1 positional lines"), and the kit's checker puts it back
// on the lists under fusion1's semantics.
struct Arm {
  std::string name;                                    // "in", "out", "free"
  const BatchLine* line = nullptr;
  std::vector<std::string> fsc;                        // empty: the pinned case is free
};
std::vector<Arm> arms(const Case&, const BatchView&, const std::filesystem::path& f1_run);

// A path the batch names, as FuSION 1's run has it: a file under the batch's write folder is the one FuSION 1
// left in its own run folder (f1_run), since the batch's exec lines made it while FuSION 1 ran; any other
// path is the case folder's own.
std::filesystem::path recorded(const std::filesystem::path& named, const BatchView&, const std::filesystem::path& f1_run);
std::vector<std::string> ids_in(const std::optional<std::filesystem::path>&);   // f1::read_ids, or none

// ---- facts
using Answers = std::vector<std::pair<testkit::Point, testkit::CheckOutcome>>;   // each check, in order

struct SolveFacts {                                    // one solve case as one side ran it
  std::string name;                                    // FuSION 1's case name: "in", "out" or "free"
  std::string step;                                    // 2.0's step ("pathz3-2", "pinned"); FuSION 1's prefix
  std::string status;                                  // 2.0's case status; "recorded" for FuSION 1
  testkit::Frontier frontier;                          // each side's own certification (06.C6)
  Answers answers;                                     // FuSION 1's limits file; 2.0's search log
  testkit::Point box_lo, box_hi;                       // the box searched
  std::vector<std::optional<testkit::Assignment>> witnesses;   // 2.0: one per frontier point
  std::vector<testkit::f1::PointRecord> records;       // FuSION 1: its solutions and relaxations
  testkit::CaseSpec spec;                              // the case as that side ran it, in ids
  testkit::CanonCorridor corridor;                     // the corridor it was solved on
  std::optional<std::string> dat;                      // FuSION 1's .dat; 2.0's written under that name (chained)
  std::optional<std::pair<int, int>> up_down;          // the observations it counts (S5.07, FuSION 1's "Up: Down:")
  std::optional<int> coexpression, frozen;             // FuSION 1's clause counts; 2.0's pairs and frozen edges
  double seconds = 0;                                  // 2.0's solve time
  std::string error;                                   // why this side could not solve it
};

struct GseaFacts {
  std::vector<testkit::f1::GseaRow> rows;
  std::vector<std::string> shortlist;                  // the chosen pathways, by file stem
  std::optional<std::uint64_t> zero_weight_draws;      // 2.0 only (08.C7)
};

struct Facts {                                         // one side of one case
  std::optional<std::vector<std::string>> up, down, exceptions;
  std::optional<GseaFacts> gsea;
  std::optional<testkit::CanonGraph> merged;
  std::optional<testkit::CanonCorridor> corridor;
  std::optional<std::pair<int, int>> corridor_size;    // nodes, edges
  std::vector<SolveFacts> solves;
  std::map<Stage, std::string> missing;                // why a stage has no facts: it failed, or did not run
  int exit = 0;                                        // a chained run's exit status
  std::string error;                                   // a chained run's failure
};

// FuSION 1's counting of a case's observations (main.cc:4667-4679, as S5.07 records it), applied to the
// genes either side places in the corridor, each with 'u' or 'd': the up genes less the avoided and the
// confirmed-down ones, plus the confirmed-up ones in the corridor, a gene in both being up. An out case's FSC
// counts as avoided, so it leaves only the up genes; S5.17's own rule is the checker's, for R1.
std::pair<int, int> observed_counts(const std::vector<std::pair<std::string, char>>& in_corridor,
                                    const testkit::CaseSpec&, const std::set<std::string>& corridor_ids);
std::set<std::string> member_ids(const testkit::CanonGraph&);
// 06.C6 on a side's own answers: a point is certified when each lower neighbour in the box is proven UNSAT.
testkit::Frontier certify(std::vector<testkit::Point>, const Answers&, testkit::Point box_lo, testkit::Point box_hi);
std::vector<std::string> file_lines(const std::string& text);   // LF or CRLF, no trailing empty line
std::vector<std::string> split_on(std::string_view, char);

// FuSION 1's facts from the case's artefact folder (cases.json's fusion1 block) and the batch's lines.
Facts fusion1_facts(const Case&, const BatchView&, const std::filesystem::path& artefacts);

// 2.0's facts from a chained run: its run directory and the case folder its batch wrote into.
Facts chained_facts(const Case&, const BatchView&, const std::filesystem::path& case_dir, const std::filesystem::path& run_dir);

// ---- the signals the predicates of expected-differences.json read (expect.cpp)
struct Signals {
  std::set<std::string> commands;                      // stage_runs
  bool raw_merge = false, single_kgml_read = false, kgml_maplink = false, pathway_id_in_two_boxes = false;
  bool id_in_several_nodes = false, fusion1_graph_duplicates = false, fusion1_corridor_isolated_members = false;
  bool fusion1_corridor_trimmable_edge = false;        // a self-loop, an edge into a source or out of a target (05.C2)
  bool value_near_threshold = false, value_prefix_token = false, value_repeats_id = false, value_lone_cr_or_bom = false;
  bool pathz3_without_constraints_flag = false, edge_bound_wraps = false;
  bool gsea_zero_draw = false, gsea_equal_nes = false;
  struct Solve {
    bool fsc_down = false, fsc_not_in_exceptions = false, excluded_or_avoided_in_complex = false;
    bool fusion1_witness_below_point = false;
    std::set<std::string> fusion1_relaxed;             // what FuSION 1's witnesses relax: self_loop, into_source, out_of_target
  };
  std::map<std::string, Solve> solves;                 // by FuSION 1's case name
};
Signals signals(const Case&, const BatchView&, const std::vector<Arm>&, const Facts& fusion1,
                const std::vector<const Facts*>& fusion2, const std::optional<std::filesystem::path>& data_root);

}  // namespace fusion::reftest
