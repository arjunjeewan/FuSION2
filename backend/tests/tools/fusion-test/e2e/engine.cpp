// The end-to-end engine (engine.hpp).
#include "e2e/engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <thread>

#include "files.hpp"
#include "fusion/core/core.hpp"
#include "fusion/core/process.hpp"
#include "fusion/testkit/e2e.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"

namespace fusion::fusiontest::e2e {

namespace fs = std::filesystem;
using core::Json;
using Outcome = CaseResult::Outcome;

namespace {

std::optional<std::string> value_after(const std::vector<std::string>& args, std::string_view option) {
  for (std::size_t k = 0; k + 1 < args.size(); ++k)
    if (args[k] == option) return args[k + 1];
  return std::nullopt;
}

bool has(const std::vector<std::string>& args, std::string_view option) {
  return std::find(args.begin(), args.end(), option) != args.end();
}

// The last lines of a child's stderr, indented, for a failure's report.
std::string tail(const std::string& text, std::size_t lines = 12) {
  std::vector<std::string> all;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) all.push_back(line);
  std::string out;
  for (std::size_t k = all.size() > lines ? all.size() - lines : 0; k < all.size(); ++k) out += "\n    | " + all[k];
  return out.empty() ? " (stderr was empty)" : "; stderr ends:" + out;
}

// <RUN>, <CASE> and <DATA> in a value of env or an action's arguments; nullopt when it names a token that has
// no path here (<DATA> without a data root, <RUN> before the run directory is known).
std::optional<std::string> expand(std::string text, const fs::path& run, const fs::path& scratch,
                                  const std::optional<testkit::DataRoot>& data) {
  const std::vector<std::pair<std::string, std::optional<fs::path>>> tokens{
      {"<RUN>", run.empty() ? std::nullopt : std::optional<fs::path>(run)},
      {"<CASE>", scratch},
      {"<DATA>", data ? std::optional<fs::path>(data->root) : std::nullopt}};
  for (const auto& [token, path] : tokens) {
    if (text.find(token) == std::string::npos) continue;
    if (!path) return std::nullopt;
    const std::string with = core::to_utf8(*path);
    for (std::size_t at = 0; (at = text.find(token, at)) != std::string::npos; at += with.size())
      text.replace(at, token.size(), with);
  }
  return text;
}

// The case's folder, but case.json and expected/, and each shared input set beside it.
void prepare(const Case& c, const Settings& s, const fs::path& scratch) {
  for (const auto& entry : fs::directory_iterator(c.dir)) {
    const std::string name = core::to_utf8(entry.path().filename());
    if (name == "case.json" || name == "expected") continue;
    fs::copy(entry.path(), scratch / entry.path().filename(), fs::copy_options::recursive);
  }
  for (const std::string& set : c.inputs) {
    const fs::path from = s.root / "inputs" / core::path_from_utf8(set);
    const fs::path to = scratch / core::path_from_utf8(set);
    if (!fs::is_directory(from)) throw core::Failure("fusion_test.case", "no shared input set " + core::to_utf8(from));
    if (fs::exists(to))
      throw core::Failure("fusion_test.case", "the case's own " + set + " has the name of a shared input set");
    fs::copy(from, to, fs::copy_options::recursive);
  }
}

fs::path under(const fs::path& scratch, const std::string& value) {
  const fs::path p = core::path_from_utf8(value);
  return p.is_absolute() ? p : scratch / p;
}

// The folders holding a run.json where fusion names its runs: under --root, else in the batch's folder, which is
// the launch directory for a batch at the top of the case (S1.26), in name order.
std::vector<fs::path> run_dirs(const std::vector<std::string>& args, const fs::path& scratch) {
  fs::path root = scratch;
  if (const auto given = value_after(args, "--root")) root = under(scratch, *given);
  else if (const auto batch = value_after(args, "-b")) root = under(scratch, *batch).parent_path();
  std::vector<fs::path> runs;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(root, ec))
    if (fs::is_regular_file(entry.path() / "run.json", ec)) runs.push_back(entry.path());
  std::sort(runs.begin(), runs.end());
  return runs;
}

// Where the one run directory is, once fusion has made it: --run-dir's, or the one run_dirs() finds.
fs::path find_run(const std::vector<std::string>& args, const fs::path& scratch) {
  if (const auto dir = value_after(args, "--run-dir")) return under(scratch, *dir);
  const std::vector<fs::path> runs = run_dirs(args, scratch);
  return runs.size() == 1 ? runs.front() : fs::path();
}

struct Fired {
  bool ran = false;
  std::vector<std::string> args;
  core::ProcessResult result;
  std::string problem;   // the action could not be started
};

// Whether events.jsonl holds an event of this type: for an action that never ran, whether fusion ended before
// the watcher saw its event.
bool has_event(const fs::path& run, const std::string& type) {
  std::ifstream in(run / "events.jsonl", std::ios::binary);
  for (std::string line; std::getline(in, line);) {
    try {
      if (Json::parse(line).value("type", std::string()) == type) return true;
    } catch (const Json::exception&) {
    }
  }
  return false;
}

// fusion through testkit::run_fusion, with a watcher thread that reads events.jsonl as fusion writes it and
// runs each action once, at the first event of its type.
testkit::RunResult run_watched(const Case& c, const Settings& s, const std::vector<std::string>& args,
                               const testkit::ScratchDir& scratch,
                               const std::vector<std::pair<std::string, std::string>>& env, std::vector<Fired>& fired) {
  fired.assign(c.actions.size(), Fired{});
  std::atomic<bool> done{false};
  std::thread watcher([&]() noexcept {
    std::uintmax_t offset = 0;   // the bytes of events.jsonl read so far, up to the last whole line
    while (!done.load()) try {
      const fs::path run = find_run(args, scratch.path());
      std::error_code ec;
      if (!run.empty() && fs::is_regular_file(run / "events.jsonl", ec)) {
        std::ifstream in(run / "events.jsonl", std::ios::binary);
        in.seekg(static_cast<std::streamoff>(offset));
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::size_t start = 0;
        for (std::size_t end; (end = text.find('\n', start)) != std::string::npos; start = end + 1) {
          std::string type;
          try {
            type = Json::parse(text.substr(start, end - start)).value("type", std::string());
          } catch (const Json::exception&) {
            continue;
          }
          for (std::size_t k = 0; k < c.actions.size(); ++k) {
            if (fired[k].ran || c.actions[k].on_event != type) continue;
            fired[k].ran = true;
            core::ProcessSpec spec;
            spec.program = s.fusion;
            for (const std::string& a : c.actions[k].fusion) {
              const auto value = expand(a, run, scratch.path(), s.data);
              if (!value) fired[k].problem = "it names a token with no path here: " + a;
              spec.args.push_back(value.value_or(a));
            }
            fired[k].args = spec.args;
            if (!fired[k].problem.empty()) continue;
            spec.working_dir = scratch.path();
            spec.env_set = env;
            spec.timeout = c.timeout;
            fired[k].result = core::run_process(spec);
          }
        }
        offset += start;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } catch (const std::exception&) {   // a file read mid-write is read again on the next turn
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  });
  testkit::RunResult ran;
  try {
    ran = testkit::run_fusion(s.fusion, args, scratch, env, c.timeout);
  } catch (...) {
    done = true;
    watcher.join();
    throw;
  }
  done = true;
  watcher.join();
  return ran;
}

// --update: each file the rules match in the run, normalised, into expected/; a golden the run no longer
// writes is removed.
std::vector<std::string> write_goldens(const fs::path& run, const fs::path& expected, const testkit::Normaliser& n) {
  std::vector<std::string> notes;
  std::set<std::string> written;
  std::size_t unchanged = 0;
  for (const auto& [rel, rule] : testkit::matched_files(run, n)) {
    written.insert(rel);
    const std::string text = testkit::normalise_file(run / core::path_from_utf8(rel), rule, n);
    const fs::path golden = expected / core::path_from_utf8(rel);
    std::error_code ec;
    const bool existed = fs::is_regular_file(golden, ec);
    if (existed && slurp(golden) == text) {
      ++unchanged;
      continue;
    }
    write_file(golden, text);
    notes.push_back((existed ? "changed " : "added ") + rel);
  }
  for (const auto& [rel, rule] : testkit::matched_files(expected, n))
    if (!written.contains(rel)) {
      fs::remove(expected / core::path_from_utf8(rel));
      notes.push_back("removed " + rel);
    }
  notes.push_back(std::to_string(unchanged) + " golden files unchanged");
  return notes;
}

std::vector<std::string> all_cases(const fs::path& cases_dir) {
  std::vector<std::string> ids;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(cases_dir, ec))
    if (fs::is_regular_file(entry.path() / "case.json", ec)) ids.push_back(core::to_utf8(entry.path().filename()));
  std::sort(ids.begin(), ids.end());
  return ids;
}

std::vector<std::string> selection(const Settings& s, std::span<const std::string> ids,
                                   std::span<const std::string> update) {
  if (ids.empty() && update.empty()) return all_cases(s.root / "cases");
  std::vector<std::string> out;
  for (const auto& group : {ids, update})
    for (const std::string& id : group)
      if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
  return out;
}

}  // namespace

std::vector<std::string> case_clashes(const fs::path& dir) {
  std::vector<std::string> out;
  std::map<std::string, std::string> seen;   // the ASCII-folded path, and the first path that folds to it
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return out;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    const std::string rel = core::to_utf8(it->path().lexically_relative(dir));
    std::string folded = rel;
    for (char& ch : folded)
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    const auto [at, fresh] = seen.emplace(folded, rel);
    if (!fresh) out.push_back(std::min(at->second, rel) + " | " + std::max(at->second, rel));
  }
  std::sort(out.begin(), out.end());
  return out;
}

CaseResult run_case(const Case& c, const Settings& s, bool update) {
  CaseResult r;
  const auto harness = [&](std::string why) {
    r.outcome = Outcome::harness;
    r.notes.push_back(std::move(why));
    return r;
  };
  const bool many = c.processes > 1;
  const std::string processes = std::to_string(c.processes) + " fusion processes";
  if (many && has(c.args, "--run-dir"))
    return harness(processes + " name their own run directories, so the case gives no --run-dir");
  if (many && !c.actions.empty()) return harness(processes + ": actions go with one process");
  if (update && c.pending)
    return harness("the case is pending (it waits for " + c.pending->waits_for +
                   "): remove pending from its case.json before recording its golden");
  testkit::ScratchDir scratch("e2e-" + c.id);
  r.scratch = scratch.path();
  const auto finish = [&](Outcome outcome) {
    r.outcome = outcome;
    if (s.keep || outcome == Outcome::failed || outcome == Outcome::harness) scratch.keep();
    r.kept = scratch.kept();
    return r;
  };
  try {
    prepare(c, s, scratch.path());
  } catch (const std::exception& e) {
    r.notes.push_back(e.what());
    return finish(Outcome::harness);
  }
  // C3.4: --run-dir run for a case that does not place its run; several processes each name their own run
  // directory in the launch directory (prompt log entry 362).
  std::vector<std::string> args = c.args;
  if (!many && !has(args, "--run-dir") && !has(args, "--root")) {
    args.push_back("--run-dir");
    args.push_back("run");
  }
  const fs::path planned = value_after(args, "--run-dir") ? find_run(args, scratch.path()) : fs::path();
  std::vector<std::pair<std::string, std::string>> env;
  for (const auto& [name, value] : c.env) {
    const auto expanded = expand(value, planned, scratch.path(), s.data);
    if (!expanded) {
      const bool needs_data = !s.data && value.find("<DATA>") != std::string::npos;
      r.notes.push_back("env " + name + "=" + value + ": " +
                        (needs_data ? "<DATA> needs the data root, and neither FUSION_TEST_DATA nor --data is set"
                                    : "<RUN> is not known before fusion starts: the case gives --root or runs "
                                      "several processes"));
      return finish(needs_data ? Outcome::skipped : Outcome::harness);
    }
    env.emplace_back(name, *expanded);
  }

  std::vector<Fired> fired;
  std::vector<testkit::RunResult> ran(static_cast<std::size_t>(c.processes));
  if (!c.actions.empty()) {
    ran[0] = run_watched(c, s, args, scratch, env, fired);
  } else {   // every process at once, in the one launch directory (S1.26)
    std::vector<std::thread> threads;
    for (testkit::RunResult& one : ran)
      threads.emplace_back([&, at = &one] {
        try {
          *at = testkit::run_fusion(s.fusion, args, scratch, env, c.timeout);
        } catch (const std::exception& e) {
          at->err = e.what();
        }
      });
    for (std::thread& t : threads) t.join();
  }
  std::vector<fs::path> runs;
  if (!many) runs.push_back(!ran[0].run_dir.empty() ? ran[0].run_dir : find_run(args, scratch.path()));
  else runs = run_dirs(args, scratch.path());
  const auto who = [&](std::size_t k) { return many ? "fusion " + std::to_string(k + 1) + ": " : std::string(); };
  const auto where = [&](const fs::path& run) { return many ? core::to_utf8(run.filename()) + ": " : std::string(); };

  if (c.pending) {   // C5.2: refused with its code while its feature waits, stale once fusion runs it
    const std::string code = "[" + c.pending->refused_with + "]";
    for (std::size_t k = 0; k < ran.size(); ++k)
      if (ran[k].exit_code == 0 || ran[k].err.find(code) == std::string::npos) {
        r.notes.push_back(who(k) + "stale: the case is pending (it waits for " + c.pending->waits_for +
                          "), but fusion " +
                          (ran[k].exit_code == 0 ? "now runs it (exit 0)"
                                                 : "exits " + std::to_string(ran[k].exit_code) + " without " + code) +
                          ": remove pending and record its golden with --update " + c.id + tail(ran[k].err));
        return finish(Outcome::failed);
      }
    r.notes.push_back("pending: it waits for " + c.pending->waits_for + ", and fusion still refuses it with " + code);
    return finish(Outcome::skipped);
  }

  // 1. The exit status, run.json's status, and for a failure the batch line and the stderr fragment; with
  // several processes, each of them and each of their run directories, one per process.
  std::vector<std::string> problems, notes;
  for (std::size_t k = 0; k < ran.size(); ++k) {
    if (ran[k].exit_code != c.expect.exit)
      problems.push_back(who(k) + "exit " + std::to_string(ran[k].exit_code) + ", expected " +
                         std::to_string(c.expect.exit) + tail(ran[k].err));
    if (c.expect.stderr_fragment && ran[k].err.find(*c.expect.stderr_fragment) == std::string::npos)
      problems.push_back(who(k) + "stderr does not hold \"" + *c.expect.stderr_fragment + "\"" + tail(ran[k].err));
  }
  if (many && runs.size() != ran.size())
    problems.push_back(processes + " made " + std::to_string(runs.size()) + " run directories, not one each");
  bool line_in_a_run = false;
  for (const fs::path& run : runs) {
    std::optional<Json> record;
    std::error_code ec;
    if (!run.empty() && fs::is_regular_file(run / "run.json", ec)) {
      try {
        record = Json::parse(slurp(run / "run.json"));
      } catch (const std::exception& e) {
        problems.push_back(where(run) + "run.json cannot be read: " + e.what());
      }
    }
    if (c.expect.status) {
      if (!record) problems.push_back("no run.json, so no status; expected " + *c.expect.status);
      else if (record->value("status", std::string()) != *c.expect.status)
        problems.push_back(where(run) + "run.json's status is " + record->value("status", std::string("missing")) +
                           ", expected " + *c.expect.status);
    }
    if (c.expect.line && record && record->contains("error") && (*record)["error"].is_object()) {
      line_in_a_run = true;
      const int line = (*record)["error"].value("line", 0);
      if (line != *c.expect.line)
        problems.push_back(where(run) + "run.json's error is at batch line " + std::to_string(line) + ", expected " +
                           std::to_string(*c.expect.line));
    }
  }
  if (c.expect.line && !line_in_a_run) {   // a check error ends fusion before a run directory exists
    const auto batch = value_after(args, "-b");
    const std::string at = (batch ? core::to_utf8(core::path_from_utf8(*batch).filename()) : std::string()) + ":" +
                           std::to_string(*c.expect.line) + ":";
    for (std::size_t k = 0; k < ran.size(); ++k)
      if (ran[k].err.find(at) == std::string::npos)
        problems.push_back(who(k) + "stderr names no failure at " + at + ", the expected batch line" +
                           tail(ran[k].err));
  }
  for (std::size_t k = 0; k < c.actions.size(); ++k) {
    const Action& a = c.actions[k];
    std::string command = "fusion";
    for (const std::string& arg : fired[k].args) command += " " + arg;
    if (!fired[k].ran)
      problems.push_back("the action on " + a.on_event + " never ran: " +
                         (!runs[0].empty() && has_event(runs[0], a.on_event)
                              ? "fusion ended before the engine saw the event"
                              : "events.jsonl holds no such event"));
    else if (!fired[k].problem.empty())
      problems.push_back("the action on " + a.on_event + " was not run: " + fired[k].problem);
    else if (fired[k].result.exit_code != 0)
      problems.push_back("the action " + command + " exited " + std::to_string(fired[k].result.exit_code) +
                         tail(fired[k].result.stderr_text + fired[k].result.error));
  }
  // 2. No two files of a run directory differ only in case.
  for (const fs::path& run : runs)
    for (const std::string& clash : case_clashes(run))
      problems.push_back(where(run) + "these differ only in case: " + clash);

  // 3. Each run directory against the one expected/, or --update. A case with no golden compares no file
  // (C5.1; prompt log entry 362).
  const auto normaliser = [&](const fs::path& run) {
    testkit::Normaliser n = testkit::fusion_normaliser(run, scratch.path(), s.data);
    n.rules.insert(n.rules.end(), c.compare.begin(), c.compare.end());
    return n;
  };
  const auto compare = [&](const fs::path& run, const fs::path& expected) {
    const testkit::TreeDiff diff = testkit::compare_tree(run, expected, normaliser(run));
    for (const std::string& rel : diff.missing)
      problems.push_back(where(run) + "missing " + rel + ": the golden has it, the run did not write it");
    for (const std::string& rel : diff.unexpected)
      problems.push_back(where(run) + "unexpected " + rel + ": expected/ has no golden of it (--update " + c.id +
                         " records one)");
    for (const std::string& d : diff.differing) {
      const auto tab = d.find('\t');
      problems.push_back(where(run) + "differs " + d.substr(0, tab) +
                         (tab == std::string::npos ? "" : ": " + d.substr(tab + 1)));
    }
  };
  const fs::path expected = c.dir / "expected";
  std::error_code ec;
  const bool golden = fs::is_directory(expected, ec) && !fs::is_empty(expected, ec);
  const auto done = [&](Outcome outcome) {
    r.notes = problems;
    r.notes.insert(r.notes.end(), notes.begin(), notes.end());
    return finish(outcome);
  };
  if (update) {
    if (!problems.empty()) {
      problems.push_back("expected/ was not written");
      return done(Outcome::failed);
    }
    try {
      notes = write_goldens(runs.front(), expected, normaliser(runs.front()));
    } catch (const std::exception& e) {
      problems.push_back(std::string("expected/ could not be written: ") + e.what());
      return done(Outcome::harness);
    }
    for (std::size_t k = 1; k < runs.size(); ++k) compare(runs[k], expected);   // the other processes' runs
    return done(problems.empty() ? Outcome::passed : Outcome::failed);
  }
  if (golden) {
    for (const fs::path& run : runs) compare(run, expected);
  } else if (const std::size_t n =
                 runs.empty() ? 0 : testkit::matched_files(runs.front(), normaliser(runs.front())).size();
             n > 0) {
    notes.push_back("no golden: expected/ is empty or absent, so the run's " + std::to_string(n) +
                    " files are not compared (--update " + c.id + " records them)");
  }
  return done(problems.empty() ? Outcome::passed : Outcome::failed);
}

int run(const Settings& s, std::span<const std::string> ids, std::span<const std::string> update, std::ostream& out,
        std::ostream& err) {
  const fs::path cases_dir = s.root / "cases";
  const std::vector<std::string> selected = selection(s, ids, update);
  if (selected.empty()) {
    out << "e2e: no end-to-end case in " << core::to_utf8(cases_dir) << "\n";
    return exit_skipped;
  }
  std::error_code ec;
  if (!fs::is_regular_file(s.fusion, ec)) {
    err << "error: no fusion at " << core::to_utf8(s.fusion) << "\n";
    return exit_harness;
  }
  std::map<Outcome, int> count;
  for (const std::string& id : selected) {
    const bool updating = std::find(update.begin(), update.end(), id) != update.end();
    const auto started = std::chrono::steady_clock::now();
    CaseResult r;
    if (!fs::is_regular_file(cases_dir / core::path_from_utf8(id) / "case.json", ec)) {
      r.outcome = Outcome::harness;
      r.notes.push_back("no case " + id + " in " + core::to_utf8(cases_dir));
    } else {
      try {
        r = run_case(load_case(cases_dir / core::path_from_utf8(id)), s, updating);
      } catch (const std::exception& e) {
        r.outcome = Outcome::harness;
        r.notes.push_back(e.what());
      }
    }
    ++count[r.outcome];
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const char* word = r.outcome == Outcome::passed    ? (updating ? "updated" : "passed")
                       : r.outcome == Outcome::failed  ? "FAILED"
                       : r.outcome == Outcome::skipped ? "skipped"
                                                       : "NOT RUN";
    out << "e2e " << id << ": " << word << " (" << core::format_number(std::round(seconds * 10) / 10) << " s)\n";
    for (const std::string& note : r.notes) out << "  " << note << "\n";
    if (r.kept) out << "  scratch folder kept: " << core::to_utf8(r.scratch) << "\n";
    out.flush();
  }
  out << "e2e: " << selected.size() << " cases, " << count[Outcome::passed] << " passed, " << count[Outcome::failed]
      << " failed, " << count[Outcome::skipped] << " skipped, " << count[Outcome::harness] << " not run\n";
  if (count[Outcome::harness] > 0) return exit_harness;
  if (count[Outcome::failed] > 0) return exit_failed;
  if (count[Outcome::passed] == 0) return exit_skipped;
  return exit_ok;
}

int list(const Settings& s, std::span<const std::string> ids, std::ostream& out, std::ostream& err) {
  const fs::path cases_dir = s.root / "cases";
  out << "case\tstate\ttags\n";
  int code = exit_ok;
  for (const std::string& id : selection(s, ids, {})) {
    try {
      const Case c = load_case(cases_dir / core::path_from_utf8(id));
      std::error_code ec;
      const bool golden = fs::is_directory(c.dir / "expected", ec) && !fs::is_empty(c.dir / "expected", ec);
      std::string tags;
      for (const std::string& t : c.tags) tags += (tags.empty() ? "" : ",") + t;
      out << c.id << "\t" << (c.pending ? "pending " + c.pending->waits_for : golden ? "golden" : "no golden") << "\t"
          << tags << "\n";
    } catch (const std::exception& e) {
      err << "error: " << e.what() << "\n";
      code = exit_harness;
    }
  }
  return code;
}

}  // namespace fusion::fusiontest::e2e
