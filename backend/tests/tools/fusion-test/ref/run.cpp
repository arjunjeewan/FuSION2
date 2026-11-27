// run(): the cases, their modes and their workers, the findings, stale entries and --record-seen (section 12,
// "Expected differences"; the C4 note's run.cpp); and the subcommand's command line.
#include <atomic>
#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <ostream>
#include <set>
#include <thread>

#include "files.hpp"
#include "fusion/core/build_info.hpp"
#include "fusion/core/install.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"
#include "ref/runner.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;

// Section 12, "Evidence used": the snapshot binary that made or reran every FuSION 1 reference.
constexpr std::string_view kFusion1Binary = "22b312fc006ce57ee07d9514f95fcf1996ad5c250c5282bd90a746f1041e0c50";
constexpr std::string_view kSnapshot = "FuSION1-reference-2026-11-10";
constexpr std::string_view kMarker = ".fusion-test-ref";   // a report directory this runner made

CaseSetup set_up(const Case& c, const Roots& roots, const fs::path& case_dir) {
  CaseSetup s;
  s.kase = &c;
  s.roots = roots;
  const auto artefacts = artefact_dir(c, roots);
  if (!artefacts) throw core::Failure("ref.prepare", c.id + " needs the data root");
  s.artefacts = *artefacts;
  s.f1_run = (s.artefacts / c.fusion1.up).parent_path();
  s.prepared = prepare(c, roots, case_dir);
  s.batch = read_batch(s.prepared.batch);
  s.arms = arms(c, s.batch, s.f1_run);
  return s;
}

CaseSetup open_setup(const Case& c, const Roots& roots, const fs::path& case_dir) {
  CaseSetup s;
  s.kase = &c;
  s.roots = roots;
  const auto artefacts = artefact_dir(c, roots);
  if (!artefacts) throw core::Failure("ref.prepare", c.id + " needs the data root");
  s.artefacts = *artefacts;
  s.f1_run = (s.artefacts / c.fusion1.up).parent_path();
  s.prepared.dir = case_dir;
  s.prepared.batch = case_dir / "case.batch";
  s.batch = read_batch(s.prepared.batch);
  s.arms = arms(c, s.batch, s.f1_run);
  return s;
}

namespace {

struct CaseRun {                                       // one case's part of the report
  std::vector<Finding> findings;
  std::map<std::string, std::set<std::string>> seen;   // entry -> "<case>" and "<case>:<solve case>"
  bool covered = false;                                // every default mode ran: its seen_in can go stale
  Json times = Json::object(), edits = Json::array();
};

std::vector<Mode> default_modes(const Case& c, Tier max_tier) {
  switch (c.tier) {
    case Tier::t1: return {Mode::chained, Mode::pinned, Mode::verify};   // the C4 note checks verify on T1
    case Tier::t2: return {Mode::chained, Mode::pinned};
    case Tier::t3: return max_tier == Tier::t3 ? std::vector<Mode>{Mode::pinned, Mode::verify} : std::vector<Mode>{Mode::pinned};
  }
  return {};
}

Finding case_finding(const Case& c, Mode m, Stage st, Outcome o, const std::string& text) {
  Finding f;
  f.case_id = c.id;
  f.mode = m;
  f.stage = st;
  f.outcome = o;
  f.detail["text"] = text;
  return f;
}

std::string failure_text(const std::exception& e) {
  const auto* f = dynamic_cast<const core::Failure*>(&e);
  return f ? std::string(e.what()) + " [" + f->code() + "]" : std::string(e.what());
}

// The stage of a run-directory file: its step's command.
Stage stage_of_file(const std::string& rel) {
  const std::string step = rel.substr(0, rel.find('/'));
  if (step.starts_with("cudf")) return Stage::classify;
  if (step.starts_with("gsea")) return Stage::gsea;
  if (step.starts_with("mff") || step.starts_with("rgx") || step.starts_with("wgx")) return Stage::merge;
  if (step.starts_with("fb_rch")) return Stage::corridor;
  return Stage::solve;
}

// The data-root files a KEGG case reads: its own folder and every $DATA folder or file its path map names.
std::vector<std::string> data_paths(const Case& c, const testkit::DataRoot& root) {
  std::vector<std::string> out{"reference/" + c.id + "/"};
  for (const PathMap& m : c.path_map) {
    if (!m.to.starts_with("$DATA/")) continue;
    std::string rel = m.to.substr(6);
    std::error_code ec;
    if (fs::is_directory(root.at(rel), ec)) rel += "/";
    out.push_back(rel);
  }
  return out;
}

CaseRun run_case(const Case& c, std::span<const Entry> entries, const Options& o, const fs::path& cases_json) {
  CaseRun r;
  const std::vector<Mode> modes = o.modes.empty() ? default_modes(c, o.max_tier) : o.modes;
  const fs::path dir = o.out / "cases" / c.id;
  Roots roots{o.tests / "fixtures", o.tests / "reference" / "synthetic", o.data};
  if (c.tier != Tier::t1) {
    if (!o.data) {
      r.findings.push_back(case_finding(c, modes.front(), Stage::classify, Outcome::skipped,
                                        "a KEGG case: no data root (set FUSION_TEST_DATA or give --data)"));
      return r;
    }
    const std::vector<std::string> paths = data_paths(c, *o.data);
    const auto checked = testkit::require(paths, o.tests / "reference" / "data-manifest.json");
    if (const auto* why = std::get_if<std::string>(&checked)) {
      r.findings.push_back(case_finding(c, modes.front(), Stage::classify, Outcome::skipped, *why));
      return r;
    }
  }

  std::optional<Facts> f1;
  std::optional<CaseSetup> first_setup;
  std::map<Mode, std::vector<Comparison>> comparisons;
  std::map<Mode, Facts> fusion2;
  std::vector<Finding> extra;                          // the repeat check's
  bool ran_all = true;
  for (Mode m : modes) {
    const auto started = std::chrono::steady_clock::now();
    try {
      CaseSetup s = set_up(c, roots, dir / std::string(name(m)));
      if (!f1) {
        f1 = fusion1_facts(c, s.batch, s.artefacts);
        r.edits = s.prepared.edits;
      }
      std::vector<Stage> stages(std::begin(kStages), std::end(kStages));
      if (m == Mode::chained) {
        const ChainedRun run = run_chained(s, o.fusion);
        std::error_code ec;
        if (run.result.exit_code != 0 || run.run_dir.empty() || !fs::exists(run.run_dir / "run.json", ec)) {
          extra.push_back(case_finding(c, m, Stage::classify, Outcome::not_runnable,
                                       "fusion exited " + std::to_string(run.result.exit_code) + ": " +
                                           run.result.err.substr(run.result.err.size() > 400 ? run.result.err.size() - 400 : 0)));
          ran_all = false;
        } else {
          fusion2[m] = chained_facts(c, s.batch, s.prepared.dir, run.run_dir);
          comparisons[m] = compare(*f1, fusion2[m], stages);
          if (c.tier != Tier::t3) {   // C4.3: every T1 and T2 case runs twice
            // The second run uses the first's folder, moved aside meanwhile, so the two batches are the same
            // bytes and no input's size or SHA-256 differs; it then moves to chained-2.
            const fs::path first = dir / "chained", aside = dir / "chained-1", second_dir = dir / "chained-2";
            fs::rename(first, aside);
            CaseSetup again = set_up(c, roots, first);
            ChainedRun second = run_chained(again, o.fusion);
            fs::rename(first, second_dir);
            fs::rename(aside, first);
            second.run_dir = second_dir / second.run_dir.lexically_relative(first);
            const std::vector<std::string> d = repeat_differences(run, first, second, first, o.data);
            Finding f = case_finding(c, m, d.empty() ? Stage::solve : stage_of_file(d.front()),
                                     d.empty() ? Outcome::match : Outcome::unexpected_difference,
                                     d.empty() ? "repeat: the second run equals the first"
                                               : "repeat: " + std::to_string(d.size()) + " files differ, first " + d.front());
            f.detail["check"] = "repeat";
            if (!d.empty()) f.detail["differences"] = d;
            f.detail["seconds"] = Json::array({run.seconds, second.seconds});
            extra.push_back(std::move(f));
          }
        }
      } else if (m == Mode::pinned) {
        fusion2[m] = pinned_facts(s, c.tier != Tier::t3, s.prepared.dir / "work");
        if (c.tier == Tier::t3) stages.pop_back();     // every row but the solve (the C4 note's "Pinned")
        comparisons[m] = compare(*f1, fusion2[m], stages);
      } else {
        comparisons[m] = run_verify(s, *f1, o, cases_json);
      }
      if (!first_setup) first_setup = std::move(s);
    } catch (const std::exception& e) {
      extra.push_back(case_finding(c, m, Stage::classify, Outcome::not_runnable, failure_text(e)));
      ran_all = false;
    }
    r.times[std::string(name(m))] = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  }

  if (first_setup && f1) {
    std::vector<const Facts*> sides;
    for (const auto& [m, f] : fusion2) sides.push_back(&f);
    const Signals sig = signals(c, first_setup->batch, first_setup->arms, *f1, sides,
                                o.data ? std::optional<fs::path>(o.data->root) : std::nullopt);
    for (Mode m : modes)
      if (comparisons.contains(m))
        for (Finding& f : judge(comparisons[m], c, m, entries, sig)) r.findings.push_back(std::move(f));
  }
  r.findings.insert(r.findings.end(), extra.begin(), extra.end());
  for (const Finding& f : r.findings) {
    if (f.outcome == Outcome::not_runnable || f.outcome == Outcome::skipped) ran_all = false;
    if (f.outcome != Outcome::expected_difference && f.outcome != Outcome::prediction_failed) continue;
    for (const std::string& e : f.entries) r.seen[e].insert(f.solve_case.empty() ? c.id : c.id + ":" + f.solve_case);
  }
  // Stale applies to the cases a run covered: every default mode ran, and a T3 case's solves only by
  // verification under --tier t3.
  r.covered = ran_all && o.modes.empty() && (c.tier != Tier::t3 || o.max_tier == Tier::t3);

  // A failing case keeps its folders and its differences; a passing one is removed (12, Reports).
  const bool fails = std::any_of(r.findings.begin(), r.findings.end(), [](const Finding& f) { return failing(f.outcome); });
  std::error_code ec;
  if (fails) {
    std::string text;
    for (const Finding& f : r.findings)
      if (failing(f.outcome))
        text += std::string(name(f.mode)) + " " + std::string(name(f.stage)) + (f.solve_case.empty() ? "" : " " + f.solve_case) +
                ": " + std::string(name(f.outcome)) + "\n" + f.detail.dump(2) + "\n\n";
    fusiontest::write_file(dir / "diffs.txt", text);
  } else if (!o.keep) {
    fs::remove_all(dir, ec);
  }
  return r;
}

std::string utc_folder_name() {
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &now);
#else
  gmtime_r(&now, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y%m%dT%H%M%SZ", &tm);
  return buf;
}

}  // namespace

Report run(std::span<const Case> cases, std::span<const Entry> entries, const Options& o) {
  std::vector<const Case*> selected;
  for (const std::string& id : o.cases) {
    const auto it = std::find_if(cases.begin(), cases.end(), [&](const Case& c) { return c.id == id; });
    if (it == cases.end()) throw core::Failure("ref.cases", "no case " + id + " in cases.json");
    selected.push_back(&*it);
  }
  // T1 is the toys; T2 adds the KEGG cases, the T3 ones by their non-solve stages (12, Test tiers); T3 adds
  // the T3 cases' verification.
  if (o.cases.empty())
    for (const Case& c : cases)
      if (o.max_tier != Tier::t1 || c.tier == Tier::t1) selected.push_back(&c);
  const fs::path cases_json = o.tests / "reference" / "cases.json";

  // The cases, workers at a time; each case's modes one after the other.
  std::vector<CaseRun> runs(selected.size());
  std::atomic<std::size_t> next{0};
  const auto work = [&] {
    for (std::size_t k; (k = next++) < selected.size();) runs[k] = run_case(*selected[k], entries, o, cases_json);
  };
  const int n = std::max(1, std::min<int>(o.workers, static_cast<int>(selected.size())));
  std::vector<std::thread> pool;
  for (int k = 0; k < n; ++k) pool.emplace_back(work);
  for (std::thread& t : pool) t.join();

  Report report;
  std::map<std::string, std::set<std::string>> seen;
  std::set<std::string> covered;
  Json times = Json::object(), edits = Json::object();
  for (std::size_t k = 0; k < selected.size(); ++k) {
    for (Finding& f : runs[k].findings) report.findings.push_back(std::move(f));
    for (const auto& [e, where] : runs[k].seen) seen[e].insert(where.begin(), where.end());
    if (runs[k].covered) covered.insert(selected[k]->id);
    times[selected[k]->id] = runs[k].times;
    if (!runs[k].edits.empty()) edits[selected[k]->id] = runs[k].edits;
  }

  // Stale: a listed case the run covered that no longer shows the difference. A T2 run cannot make an entry
  // stale for a case it did not run, nor a run of some modes only.
  std::vector<Entry> recorded(entries.begin(), entries.end());
  for (Entry& e : recorded) {
    std::vector<std::string> keep;
    for (const std::string& item : e.seen_in) {
      const std::string id = item.substr(0, item.find(':'));
      if (!covered.contains(id)) {
        keep.push_back(item);
        continue;
      }
      if (!seen[e.id].contains(item)) {
        const auto* c = selected[static_cast<std::size_t>(
            std::find_if(selected.begin(), selected.end(), [&](const Case* x) { return x->id == id; }) - selected.begin())];
        Finding f = case_finding(*c, Mode::chained, e.stages.empty() ? Stage::solve : e.stages.front(), Outcome::stale,
                                 e.id + " is listed for " + item + " and no longer seen there");
        f.solve_case = item.find(':') == std::string::npos ? "" : item.substr(item.find(':') + 1);
        f.entries = {e.id};
        report.findings.push_back(std::move(f));
      }
    }
    for (const std::string& item : seen[e.id])
      if (covered.contains(item.substr(0, item.find(':')))) keep.push_back(item);
    std::sort(keep.begin(), keep.end());
    keep.erase(std::unique(keep.begin(), keep.end()), keep.end());
    e.seen_in = std::move(keep);
  }
  if (o.record_seen) fusiontest::write_file(o.expectations, with_seen(o.expectations, recorded));

  Json& m = report.meta;
  m["runner"] = "fusion-test " + std::string(core::build_info().version);
  m["fusion"] = Json{{"version", std::string(core::build_id())}, {"z3", solve::z3_version()}};
  m["fusion1"] = Json{{"binary_sha256", kFusion1Binary}, {"snapshot", kSnapshot}};
  std::error_code ec;
  const fs::path manifest = o.tests / "reference" / "data-manifest.json";
  m["data_manifest_sha256"] = fs::exists(manifest, ec) ? Json(core::Sha256::of_file(manifest).hex()) : Json();
  m["tier"] = name(o.max_tier);
  Json ids = Json::array(), modes = Json::array();
  for (const Case* c : selected) ids.push_back(c->id);
  for (Mode md : o.modes) modes.push_back(name(md));
  m["cases"] = ids;
  m["modes"] = o.modes.empty() ? Json("default") : modes;
  m["edits"] = edits;
  m["seconds"] = times;
  if (o.record_seen) m["recorded_seen"] = core::to_utf8(o.expectations);
  return report;
}

// ---------------------------------------------------------------- the command line

namespace {

void usage(std::ostream& out) {
  out << "Run as \"fusion-test ref [options]\" (section 12, \"Reference-test runner\"; the C4 note)\n\n"
         "  --tier t1|t2|t3   the cases up to this tier (default t1); T3 cases run their non-solve stages,\n"
         "                    and verification only with --tier t3\n"
         "  --case ID         this case only (repeatable)\n"
         "  --mode M          chained, pinned or verify only (repeatable)\n"
         "  --workers N       cases at once, and verification points at once (default 1)\n"
         "  --out DIR         the report directory (default: <data root>/reports/ref/<UTC time>)\n"
         "  --record-seen     rewrite expected-differences.json's seen_in lists from this run, for review\n"
         "  --keep            keep every case folder, passing ones too\n"
         "  --fusion EXE      the fusion to run (default: the build's staged fusion)\n"
         "  --data DIR        the data root (default: FUSION_TEST_DATA)\n"
         "  --check-point SPEC   the verification worker: one check of one point (C4.2)\n\n"
         "Exit codes: 0 no failure, 1 a difference, a failed prediction, a stale entry or a case that could not\n"
         "run, 2 the harness could not run, 4 everything skipped.\n";
}

// A fresh report directory: the one given, empty or made by this runner before (its own files are
// replaced), or a new <root>/reports/ref/<UTC time>[-k].
fs::path report_dir(const std::optional<fs::path>& given, const std::optional<fs::path>& data) {
  std::error_code ec;
  if (given) {
    const fs::path d = fs::absolute(*given);
    if (fs::exists(d, ec) && !fs::is_empty(d, ec)) {
      if (!fs::exists(d / kMarker, ec))
        throw core::Failure("ref.out", core::to_utf8(d) + " holds other files; give an empty folder or a new one");
      for (const char* own : {"report.json", "summary.tsv", "cases"}) fs::remove_all(d / own, ec);
    }
    fs::create_directories(d);
    fusiontest::write_file(d / kMarker, "fusion-test ref\n");
    return d;
  }
  const fs::path root = data ? *data / "reports" / "ref" : fs::temp_directory_path() / "fusion-test" / "reports" / "ref";
  fs::create_directories(root);
  const std::string stamp = utc_folder_name();
  for (int k = 1;; ++k) {
    const fs::path d = root / (k == 1 ? stamp : stamp + "-" + std::to_string(k));
    if (fs::create_directory(d, ec)) {
      fusiontest::write_file(d / kMarker, "fusion-test ref\n");
      return d;
    }
  }
}

}  // namespace

int command(std::span<const std::string> args, std::ostream& out, std::ostream& err) {
  fusiontest::Command c;
  Options o;
  std::optional<fs::path> out_dir, spec;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    const auto value = [&]() -> std::optional<std::string> {
      if (i + 1 >= args.size() || args[i + 1].starts_with("--")) return std::nullopt;
      return args[++i];
    };
    std::optional<std::string> v;
    if (a == "-h" || a == "--help") {
      usage(out);
      return fusiontest::exit_ok;
    } else if (a == "--record-seen") {
      o.record_seen = true;
      continue;
    } else if (a == "--keep") {
      o.keep = true;
      continue;
    }
    v = value();
    if (!v) {
      err << "error: " << (a.starts_with("-") ? a + " needs a value" : "unexpected argument " + a) << "\n\n";
      usage(err);
      return fusiontest::exit_harness;
    }
    if (a == "--tier") {
      const auto t = parse_tier(*v);
      if (!t) {
        err << "error: --tier takes t1, t2 or t3\n";
        return fusiontest::exit_harness;
      }
      o.max_tier = *t;
    } else if (a == "--case") {
      o.cases.push_back(*v);
    } else if (a == "--mode") {
      const auto m = parse_mode(*v);
      if (!m) {
        err << "error: --mode takes chained, pinned or verify\n";
        return fusiontest::exit_harness;
      }
      o.modes.push_back(*m);
    } else if (a == "--workers") {
      o.workers = std::atoi(v->c_str());
      if (o.workers < 1) {
        err << "error: --workers takes a number of at least 1\n";
        return fusiontest::exit_harness;
      }
    } else if (a == "--out") {
      out_dir = core::path_from_utf8(*v);
    } else if (a == "--fusion") {
      c.fusion = core::path_from_utf8(*v);
    } else if (a == "--data") {
      c.data = core::path_from_utf8(*v);
    } else if (a == "--check-point") {
      spec = core::path_from_utf8(*v);
    } else {
      err << "error: unknown option " << a << " for fusion-test ref\n\n";
      usage(err);
      return fusiontest::exit_harness;
    }
  }
  if (spec) return check_point(*spec, out, err);
  try {
    o.tests = testkit::source_dir();
    o.expectations = o.tests / "reference" / "expected-differences.json";
    o.fusion = fusiontest::fusion_exe(c);
    o.worker = core::executable_path();
    if (const auto d = fusiontest::data_dir(c)) o.data = testkit::DataRoot{*d};
    const std::vector<Case> cases = load_cases(o.tests / "reference" / "cases.json");
    const std::vector<Entry> entries = load_expectations(o.expectations);
    o.out = report_dir(out_dir, o.data ? std::optional<fs::path>(o.data->root) : std::nullopt);
    const Report report = run(cases, entries, o);
    write(report, o.out);
    for (const Finding& f : report.findings)
      if (failing(f.outcome) || f.outcome == Outcome::skipped)
        out << name(f.outcome) << ": " << f.case_id << " " << name(f.mode) << " " << name(f.stage)
            << (f.solve_case.empty() ? "" : " " + f.solve_case) << ": " << f.detail.value("text", "") << "\n";
    std::map<std::string, int> counts;
    for (const Finding& f : report.findings) ++counts[std::string(name(f.outcome))];
    out << report.findings.size() << " findings:";
    for (const auto& [k, n] : counts) out << " " << k << " " << n;
    out << "\nreport: " << core::to_utf8(o.out) << "\n";
    return report.exit_code();
  } catch (const std::exception& e) {
    err << "error: " << failure_text(e) << "\n";
    return fusiontest::exit_harness;
  }
}

}  // namespace fusion::reftest
