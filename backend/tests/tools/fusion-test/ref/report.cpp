// The reports (section 12, "Reports"): report.json and summary.tsv, in a report directory outside any run
// directory.
#include <map>

#include "files.hpp"
#include "ref/ref.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;

std::string_view name(Outcome o) noexcept {
  switch (o) {
    case Outcome::match: return "match";
    case Outcome::expected_difference: return "expected_difference";
    case Outcome::unexpected_difference: return "unexpected_difference";
    case Outcome::prediction_failed: return "prediction_failed";
    case Outcome::stale: return "stale";
    case Outcome::inconclusive: return "inconclusive";
    case Outcome::skipped: return "skipped";
    case Outcome::not_runnable: return "not_runnable";
  }
  return "match";
}

bool failing(Outcome o) noexcept {
  return o == Outcome::unexpected_difference || o == Outcome::prediction_failed || o == Outcome::stale ||
         o == Outcome::not_runnable;
}

int Report::exit_code() const {
  bool all_skipped = true;
  for (const Finding& f : findings) {
    if (failing(f.outcome)) return 1;
    if (f.outcome != Outcome::skipped) all_skipped = false;
  }
  return all_skipped ? 4 : 0;
}

namespace {
std::string one_line(std::string s) {
  for (char& c : s)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  return s;
}
}  // namespace

void write(const Report& r, const fs::path& out) {
  Json doc = r.meta;
  Json findings = Json::array();
  std::map<std::string, int> counts;
  for (Outcome o : {Outcome::match, Outcome::expected_difference, Outcome::unexpected_difference, Outcome::prediction_failed,
                    Outcome::stale, Outcome::inconclusive, Outcome::skipped, Outcome::not_runnable})
    counts[std::string(name(o))] = 0;
  std::string tsv = "case\tmode\tstage\tsolve_case\toutcome\tentries\tdetail\n";
  for (const Finding& f : r.findings) {
    Json j{{"case", f.case_id}, {"mode", name(f.mode)}, {"stage", name(f.stage)}};
    if (!f.solve_case.empty()) j["solve_case"] = f.solve_case;
    j["outcome"] = name(f.outcome);
    if (!f.entries.empty()) j["entries"] = f.entries;
    for (const auto& [key, value] : f.detail.items())
      if (key != "text") j[key] = value;
    j["detail"] = f.detail.value("text", "");
    findings.push_back(std::move(j));
    ++counts[std::string(name(f.outcome))];
    std::string entries;
    for (const std::string& e : f.entries) entries += (entries.empty() ? "" : ",") + e;
    tsv += f.case_id + "\t" + std::string(name(f.mode)) + "\t" + std::string(name(f.stage)) + "\t" +
           (f.solve_case.empty() ? "-" : f.solve_case) + "\t" + std::string(name(f.outcome)) + "\t" +
           (entries.empty() ? "-" : entries) + "\t" + one_line(f.detail.value("text", "")) + "\n";
  }
  doc["findings"] = std::move(findings);
  Json summary = Json::object();
  for (Outcome o : {Outcome::match, Outcome::expected_difference, Outcome::unexpected_difference, Outcome::prediction_failed,
                    Outcome::stale, Outcome::inconclusive, Outcome::skipped, Outcome::not_runnable})
    summary[std::string(name(o))] = counts[std::string(name(o))];
  doc["summary"] = std::move(summary);
  fusiontest::write_file(out / "report.json", doc.dump(2) + "\n");
  fusiontest::write_file(out / "summary.tsv", tsv);
}

}  // namespace fusion::reftest
