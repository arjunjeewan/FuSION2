// Chained mode (section 12, "Modes"; the C4 note's "Chained"): the case's batch, after the path map and its
// edits, through fusion, and the repeat check of C4.3 (12, Determinism, "Repeat").
#include <chrono>
#include <map>

#include "files.hpp"
#include "ref/runner.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;

ChainedRun run_chained(const CaseSetup& s, const fs::path& fusion) {
  const std::vector<std::string> args{"-b", "case.batch", "--run-dir", "run"};
  const auto started = std::chrono::steady_clock::now();
  ChainedRun r;
  r.result = testkit::run_fusion(fusion, args, s.prepared.dir, {}, std::chrono::milliseconds(0));   // stdin from the null device
  r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  r.run_dir = r.ran_at = r.result.run_dir;
  fusiontest::write_file(s.prepared.dir / "stdout.txt", r.result.out);
  fusiontest::write_file(s.prepared.dir / "stderr.txt", r.result.err);
  return r;
}

namespace {

// FuSION 1's limits rendering (01.C11) carries each check's time: "Time taken (sec): 0.0110".
std::string mask_limits_times(std::string text) {
  const std::string mark = "Time taken (sec): ";
  for (std::size_t at = 0; (at = text.find(mark, at)) != std::string::npos;) {
    const std::size_t from = at + mark.size();
    const std::size_t end = text.find('\n', from);
    text.replace(from, (end == std::string::npos ? text.size() : end) - from, "<masked>");
    at = from;
  }
  return text;
}

std::map<std::string, fs::path> files_under(const fs::path& root) {
  std::map<std::string, fs::path> out;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec || !it->is_regular_file(ec)) continue;
    const std::string rel = core::to_utf8(fs::relative(it->path(), root, ec));
    if (rel == "run.log" || rel == "run.lock") continue;   // the log and the lock: neither is a result
    out.emplace(rel, it->path());
  }
  return out;
}

}  // namespace

std::vector<std::string> repeat_differences(const ChainedRun& a, const fs::path& case_a, const ChainedRun& b,
                                            const fs::path& case_b, const std::optional<testkit::DataRoot>& data) {
  std::vector<std::string> out;
  if (a.result.exit_code != b.result.exit_code)
    out.push_back("exit " + std::to_string(a.result.exit_code) + " then " + std::to_string(b.result.exit_code));
  // Every file of the run directory, each through the kit's normaliser: section 09's volatile fields and the
  // path tokens; the search logs' times; and the times of FuSION 1's limits rendering.
  const testkit::Normaliser na = testkit::fusion_normaliser(a.ran_at, case_a, data);
  const testkit::Normaliser nb = testkit::fusion_normaliser(b.ran_at, case_b, data);
  const auto first = files_under(a.run_dir), second = files_under(b.run_dir);
  for (const auto& [rel, file] : first) {
    const auto other = second.find(rel);
    if (other == second.end()) {
      out.push_back(rel + ": only in the first run");
      continue;
    }
    testkit::FileRule rule{rel, testkit::Compare::exact, {}};
    if (rel.ends_with("search_log.tsv")) rule = {rel, testkit::Compare::tsv_masked, {"wall_s", "cpu_s"}};
    std::string x = testkit::normalise_file(file, rule, na), y = testkit::normalise_file(other->second, rule, nb);
    if (rel.ends_with("_limits_timefile.txt")) {
      x = mask_limits_times(std::move(x));
      y = mask_limits_times(std::move(y));
    }
    if (x == y) continue;
    const auto lx = file_lines(x), ly = file_lines(y);
    std::size_t k = 0;
    while (k < lx.size() && k < ly.size() && lx[k] == ly[k]) ++k;
    out.push_back(rel + ": line " + std::to_string(k + 1) + ": " + (k < lx.size() ? lx[k].substr(0, 120) : "<end>") +
                  " | " + (k < ly.size() ? ly[k].substr(0, 120) : "<end>"));
  }
  for (const auto& [rel, file] : second)
    if (!first.contains(rel)) out.push_back(rel + ": only in the second run");
  return out;
}

}  // namespace fusion::reftest
