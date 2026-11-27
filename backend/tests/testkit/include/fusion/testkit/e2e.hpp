// fusion/testkit/e2e.hpp: running fusion as a child and comparing its run directory with goldens
// (section 12, "End-to-end tests"; the C5 note's choices C5.3 and C5.4).
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "fusion/core/process.hpp"
#include "fusion/testkit/data.hpp"
#include "fusion/testkit/scratch.hpp"

namespace fusion::testkit {

enum class Compare {
  exact,          // bytes, after the JSON masks and the path tokens
  tsv_masked,     // bytes, with the named columns blanked
  results,        // the frontier and its flags exactly; each witness must pass check() at its point
};
struct FileRule { std::string glob; Compare mode = Compare::exact; std::vector<std::string> masked_columns; };

// json_pointers: "/a/b", '*' for any member or element ("/steps/*/started"); a prefix "<file name>:"
// limits one to files of that name ("run.json:/pid"), and "@<type>" to objects whose "type" is <type>, the
// events of events.jsonl ("events.jsonl:@run.started/pid"). A masked value becomes "<masked>".
// path_tokens: each absolute path becomes its token in every compared file (C5.4), longest path first.
// globs: '*' matches within one path segment, '**' across segments, '?' one character.
struct Normaliser {
  std::vector<std::string> json_pointers;
  std::vector<std::pair<std::filesystem::path, std::string>> path_tokens;   // <RUN>, <CASE>, <DATA>
  std::vector<FileRule> rules;                                             // one per compared output
};

struct RunResult { int exit_code = -1; std::string out, err; std::filesystem::path run_dir; };
// Starts a child through core::Process, one fork at a time in this process; the caller waits for it. Several
// threads forking at once hung the reference runner (four threads spinning in user space and no child
// started, about one run in three of two cases at once), so the kit's callers start children here and
// wait for them in parallel.
core::Process spawn(const core::ProcessSpec&);
// Starts fusion through core::Process in the scratch directory, with stdin from the null device and env
// added to its environment (11.C20's FUSION_HOME, FUSION_GNUPLOT, FUSION_DOT). run_dir is the run
// directory it made: the --run-dir argument's, else the one run under <scratch>/runs/. The child is
// headless: DISPLAY and WAYLAND_DISPLAY are set empty, which batch::display_available() takes as unset,
// except one that env names itself, so no window-plot line opens a window wherever a test runs (01.C20).
RunResult run_fusion(const std::filesystem::path& exe, std::span<const std::string> args, const ScratchDir&,
                     std::span<const std::pair<std::string, std::string>> env = {},
                     std::chrono::milliseconds timeout = std::chrono::minutes(2));
// The same in a folder the caller owns, such as the reference runner's case folder under its report
// directory (the C4 note's "Preparing a case"). A timeout of 0 means none.
RunResult run_fusion(const std::filesystem::path& exe, std::span<const std::string> args,
                     const std::filesystem::path& working_dir,
                     std::span<const std::pair<std::string, std::string>> env = {},
                     std::chrono::milliseconds timeout = std::chrono::minutes(2));

// The masks of section 09's volatile run.json fields (09-runs.md:292), section 12's event masks with
// C5.3's seq, FuSION's own version and commit, and the three path tokens; with run.json and events.jsonl
// compared in exact mode. A case adds its own rules.
Normaliser fusion_normaliser(const std::filesystem::path& run_dir, const std::filesystem::path& case_dir,
                             const std::optional<DataRoot>&);

// The normalised text of one file under a rule: what compare_tree compares and what an update writes.
// events.jsonl in exact mode is one stream per (step, point, case), each in its own order (C5.3).
std::string normalise_file(const std::filesystem::path& file, const FileRule&, const Normaliser&);
// The files under root (relative, '/'-separated, sorted) that a rule's glob matches, with that rule.
std::vector<std::pair<std::string, FileRule>> matched_files(const std::filesystem::path& root, const Normaliser&);

// compare_tree compares only the files its rules' globs match: `missing` is a golden with no actual file,
// `unexpected` an actual matched file with no golden, `differing` a pair whose comparison fails (with the
// reason after a tab). In results mode the witnesses are checked on the actual run (read_run()).
struct TreeDiff {
  std::vector<std::string> missing, unexpected, differing;
  bool empty() const;
};
TreeDiff compare_tree(const std::filesystem::path& actual, const std::filesystem::path& expected, const Normaliser&);

}  // namespace fusion::testkit
