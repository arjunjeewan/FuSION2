// The reference cases and their folders (section 12, "Reference-test runner" and "Reference cases"; the C4
// note's "Preparing a case"). cases.json is the one contract between this runner and run_fusion1.py
// (decision 0037): its path-map rules are restated here, not shared, and the case's edits (C4.1) follow.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/testkit/data.hpp"

namespace fusion::reftest {

enum class Tier { t1, t2, t3 };
std::string_view name(Tier) noexcept;                  // "T1", "T2", "T3", as cases.json writes them
std::optional<Tier> parse_tier(std::string_view) noexcept;   // "T1" or "t1"

struct PathMap { std::string from, to; };              // applied to batch tokens and to list files
// C4.1: a change to a batch line that 2.0 refuses by decision, applied after the path map, each with its
// reason, and listed in the report. The one operation so far, drop_pathz3_tail, drops the optional tail of
// every pathz3 line that has one (FuSION 1's tokens after its 28 fixed ones, 06's "FuSION 1 positional
// lines"); its argument is the text that shows the tail is unfilled, which must appear in the batch.
struct Edit { std::string op, arg, why; };

struct SolveArtefacts {                                // one FSC case
  std::filesystem::path dat, limits;
  std::optional<std::filesystem::path> solutions, relaxations;   // absent when there is no point
};
struct Fusion1Artefacts {                              // paths relative to the case's artefact folder
  std::filesystem::path up, down, exceptions, console;
  std::optional<std::filesystem::path> gsea_table, shortlist;    // GSEA cases only
  std::filesystem::path reread, corridor;                        // from the capture batch
  std::map<std::string, SolveArtefacts> solve;                   // "in", "out" or "free" (06.C10)
};
struct Case {                                          // one entry of cases.json, field for field
  std::string id;
  std::filesystem::path batch;                         // relative to backend/tests/fixtures/
  Tier tier = Tier::t1;
  std::string source;                                  // "recorded 2026-09-28", "regenerated"
  std::vector<PathMap> path_map;
  Fusion1Artefacts fusion1;
  std::map<std::string, double> fusion1_seconds;       // "in", "out" or "free"
  core::Json origin;                                   // C4.4: the FuSION 1 files the case comes from
  std::vector<Edit> edits;                             // C4.4, C4.1
};

// Throws core::Failure("ref.cases") naming the field at fault.
std::vector<Case> load_cases(const std::filesystem::path& cases_json);

// Where a case's files are: the batch corpus, the toy cases' folders and the data root.
struct Roots {
  std::filesystem::path fixtures;                      // backend/tests/fixtures
  std::filesystem::path synthetic;                     // backend/tests/reference/synthetic
  std::optional<testkit::DataRoot> data;
};
// The folder that cases.json's fusion1 paths are relative to, and the one its inputs/ come from: a toy
// case's in the repository, a KEGG case's in the data root (none without one).
std::optional<std::filesystem::path> artefact_dir(const Case&, const Roots&);

// run_fusion1.py's rules (decision 0037). The map is applied as text, longest "from" first; $CASE and $DATA
// become the case's folder and the data root.
std::string apply_path_map(std::string text, std::span<const PathMap>, const std::filesystem::path& case_dir,
                           const std::optional<std::filesystem::path>& data_root);
// The FuSION 1 tree or snapshot that a mapped text still names, if any.
std::optional<std::string> leftover_fusion1_path(std::string_view text);
// C4.1: each edit applied to the batch text, in order; `applied` gets one line per edit for the report.
// Throws core::Failure("ref.edit") for an unknown operation, or one whose argument the batch does not hold
// or that changes nothing, so an edit cannot outlive the batch line it was written for.
std::string apply_edits(std::string text, std::span<const Edit>, std::vector<std::string>& applied);

struct Prepared {
  std::filesystem::path dir;                           // $CASE
  std::filesystem::path batch;                         // <dir>/case.batch
  std::vector<std::string> edits;                      // the edits applied, for the report
};
// A fresh case folder: the case's inputs/ copied in (copies, never links), the path map applied to the batch
// and to every inputs/ file whose name holds "list", a leftover FuSION 1 path refused, then the edits.
// Throws core::Failure: ref.prepare (a folder that exists, a missing batch or inputs/), ref.leftover, ref.edit.
Prepared prepare(const Case&, const Roots&, const std::filesystem::path& case_dir);

}  // namespace fusion::reftest
