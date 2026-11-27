// Verify mode (section 12, "Frontier verification"; 12.C7; C4.2): each point that proves FuSION 1's frontier
// in its box is asked once, in its own worker process with its own Z3 context, which builds the pinned
// solve's case and calls solve::CaseSolver::check(point, Phase::point). A point that disagrees triggers a
// full pinned solve of that case, judged by R1 to R4.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <semaphore>
#include <ostream>
#include <thread>

#include "files.hpp"
#include "fusion/core/install.hpp"
#include "fusion/core/process.hpp"
#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/step_context.hpp"
#include "ref/runner.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;
using testkit::Point;

namespace {

struct Job {
  std::string arm;
  Point point;
  bool expect_sat = false;
  fs::path spec;
  Json result;
};

}  // namespace

std::vector<Comparison> run_verify(const CaseSetup& s, const Facts& f1, const Options& o, const fs::path& cases_json) {
  std::vector<Job> jobs;
  for (const SolveFacts& sf : f1.solves) {
    for (Point p : verification_points(sf.frontier, sf.box_lo, sf.box_hi)) {
      Job j;
      j.arm = sf.name;
      j.point = p;
      j.expect_sat = std::find(sf.frontier.points.begin(), sf.frontier.points.end(), p) != sf.frontier.points.end();
      j.spec = s.prepared.dir / "points" / (sf.name + "-n" + std::to_string(p.n) + "-e" + std::to_string(p.e) + ".json");
      Json spec{{"tests", core::to_utf8(o.tests)}, {"cases_json", core::to_utf8(cases_json)}, {"case", s.kase->id},
                {"solve_case", sf.name}, {"case_dir", core::to_utf8(s.prepared.dir)}, {"point", Json::array({p.n, p.e})},
                {"data", o.data ? Json(core::to_utf8(o.data->root)) : Json()}};
      fusiontest::write_file(j.spec, spec.dump(2) + "\n");
      jobs.push_back(std::move(j));
    }
  }
  // The workers: fusion-test itself, one point per process, at most --workers of them at a time across the
  // whole run, however many cases verify at once.
  static std::counting_semaphore<4096> slots(std::clamp(o.workers, 1, 4096));
  const fs::path self = o.worker.empty() ? core::executable_path() : o.worker;
  std::atomic<std::size_t> next{0};
  const auto work = [&] {
    for (std::size_t k; (k = next++) < jobs.size();) {
      slots.acquire();
      struct Release {
        std::counting_semaphore<4096>& s;
        ~Release() { s.release(); }
      } release{slots};
      core::ProcessSpec p;
      p.program = self;
      p.args = {"ref", "--check-point", core::to_utf8(jobs[k].spec)};
      p.input = core::ProcessSpec::Input::null;
      p.output = core::ProcessSpec::Output::capture;
      const core::ProcessResult r = testkit::spawn(p).wait();   // one fork at a time (e2e.hpp)
      try {
        jobs[k].result = Json::parse(r.stdout_text);
      } catch (const Json::exception&) {
        jobs[k].result = Json{{"error", "the worker exited " + std::to_string(r.exit_code) + ": " + r.stderr_text.substr(0, 300) + r.error}};
      }
    }
  };
  std::vector<std::thread> pool;
  const int n = std::max(1, std::min<int>(o.workers, static_cast<int>(jobs.size())));
  for (int k = 0; k < n; ++k) pool.emplace_back(work);
  for (std::thread& t : pool) t.join();

  std::vector<Comparison> out;
  for (const SolveFacts& sf : f1.solves) {
    Comparison c;
    c.stage = Stage::solve;
    c.solve_case = sf.name;
    Json checks = Json::array();
    std::vector<std::string> disagree, unknown, errors;
    double seconds = 0;
    for (const Job& j : jobs) {
      if (j.arm != sf.name) continue;
      const Json& r = j.result;
      checks.push_back(Json{{"point", Json::array({j.point.n, j.point.e})}, {"expect", j.expect_sat ? "sat" : "unsat"},
                            {"outcome", r.value("outcome", "error")}, {"r1", r.value("r1", Json())}});
      seconds += r.value("wall_s", 0.0);
      const std::string at = "(" + std::to_string(j.point.n) + ", " + std::to_string(j.point.e) + ")";
      const std::string got = r.value("outcome", "");
      if (r.contains("error")) errors.push_back(at + ": " + r["error"].get<std::string>());
      else if (got == "sat" || got == "unsat") {
        if ((got == "sat") != j.expect_sat) disagree.push_back(at + " " + got);
        else if (got == "sat" && !r.value("r1", Json::object()).value("valid", false)) disagree.push_back(at + ": its witness fails R1");
      } else {
        unknown.push_back(at + " " + got);
      }
    }
    c.detail = Json{{"fusion1", Json::array()}, {"checks", checks}, {"check_s", seconds}};
    for (Point p : sf.frontier.points) c.detail["fusion1"].push_back(Json::array({p.n, p.e}));
    if (!errors.empty()) {
      c.not_runnable = "a verification worker failed: " + errors.front();
      out.push_back(std::move(c));
      continue;
    }
    if (!disagree.empty()) {
      // A failed verification triggers a full 2.0 search of the case, judged by R1 to R4 (12.C7).
      const Facts full = pinned_facts(s, true, s.prepared.dir / ("search-" + sf.name));
      Facts theirs, mine;
      theirs.solves.push_back(sf);
      for (const SolveFacts& x : full.solves)
        if (x.name == sf.name) mine.solves.push_back(x);
      const Stage stage[] = {Stage::solve};
      std::vector<Comparison> judged = compare(theirs, mine, stage);
      if (judged.empty()) {
        c.not_runnable = "the full search found no solve " + sf.name;
        out.push_back(std::move(c));
        continue;
      }
      Comparison search = std::move(judged.front());
      search.detail["verification"] = Json{{"disagree", disagree}, {"checks", checks}};
      search.detail["text"] = "verification disagrees at " + disagree.front() + "; full search: " + search.detail.value("text", "");
      out.push_back(std::move(search));
      continue;
    }
    if (!unknown.empty()) {
      c.inconclusive = true;
      c.detail["inconclusive"] = unknown;
    }
    c.detail["text"] = std::to_string(sf.frontier.points.size()) + " point" + (sf.frontier.points.size() == 1 ? "" : "s") +
                       " proven by " + std::to_string(checks.size()) + " checks" +
                       (unknown.empty() ? "" : ", " + std::to_string(unknown.size()) + " unknown");
    out.push_back(std::move(c));
  }
  return out;
}

int check_point(const fs::path& spec_file, std::ostream& out, std::ostream& err) {
  try {
    const Json spec = Json::parse(fusiontest::slurp(spec_file));
    const fs::path tests = core::path_from_utf8(spec.at("tests").get<std::string>());
    const std::vector<Case> cases = load_cases(core::path_from_utf8(spec.at("cases_json").get<std::string>()));
    const std::string id = spec.at("case").get<std::string>();
    const auto c = std::find_if(cases.begin(), cases.end(), [&](const Case& x) { return x.id == id; });
    if (c == cases.end()) throw core::Failure("ref.check_point", "no case " + id);
    Roots roots{tests / "fixtures", tests / "reference" / "synthetic", std::nullopt};
    if (spec.contains("data") && spec["data"].is_string()) roots.data = testkit::DataRoot{core::path_from_utf8(spec["data"].get<std::string>())};
    const CaseSetup s = open_setup(*c, roots, core::path_from_utf8(spec.at("case_dir").get<std::string>()));
    const std::string arm_name = spec.at("solve_case").get<std::string>();
    const auto arm = std::find_if(s.arms.begin(), s.arms.end(), [&](const Arm& a) { return a.name == arm_name; });
    if (arm == s.arms.end()) throw core::Failure("ref.check_point", "no solve case " + arm_name);
    const Point point{spec.at("point")[0].get<int>(), spec.at("point")[1].get<int>()};
    testkit::StepContext sc(spec_file.parent_path() / ("work-" + core::to_utf8(spec_file.stem())), "check-point");
    const std::shared_ptr<PinnedBase> base = pinned_base(s, sc.ctx());
    const PinnedSolve ps = pinned_solve(*base, s, *arm, sc.ctx());
    Json result;
    if (const auto* skipped = std::get_if<solve::Skipped>(&ps.problem)) {
      // An infeasible case has no solution at any point; one with no DE gene in the corridor is not a search.
      result = Json{{"outcome", skipped->status == solve::CaseStatus::infeasible ? "unsat" : "unknown"},
                    {"status", std::string(solve::name(skipped->status))}, {"reason", skipped->message}};
    } else {
      solve::CaseSolver solver(std::get<solve::Problem>(ps.problem), ps.settings);
      const solve::Answer a = solver.check(solve::Point{point.n, point.e}, solve::Phase::point);
      result = Json{{"outcome", std::string(solve::name(a.outcome))}, {"wall_s", a.wall_s}, {"cpu_s", a.cpu_s}, {"reason", a.reason}};
      if (a.outcome == solve::Outcome::sat && a.witness && *a.witness < solver.witnesses().size()) {
        const solve::Witness& w = solver.witnesses()[*a.witness];
        const testkit::Verdict v = testkit::check(ps.corridor, ps.spec, testkit::adapt::assignment(*ps.source, w), testkit::Semantics::fusion2);
        const bool within = v.cost.n <= point.n && v.cost.e <= point.e;
        result["counts"] = Json::array({w.counts.n, w.counts.e});
        result["r1"] = Json{{"valid", v.valid && within}, {"cost", Json::array({v.cost.n, v.cost.e})},
                            {"why", v.valid ? (within ? "" : "costs more than the point") : (v.violations.empty() ? "invalid" : v.violations.front())}};
      }
    }
    out << result.dump() << "\n";
    return 0;
  } catch (const std::exception& e) {
    out << Json{{"error", e.what()}}.dump() << "\n";
    err << "error: " << e.what() << "\n";
    return 2;
  }
}

}  // namespace fusion::reftest
