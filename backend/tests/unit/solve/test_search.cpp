// The Pareto search (S5.18), the answer cache (S5.20, 06.C5) and the frontier's assembly (06.C6,
// 06.C20), driven without Z3: by FuSION 1's own logged answers on the toy cases, and by scripted
// oracles over random staircases (section 06's test hooks; section 12's solve row).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>

#include "fusion/testkit/catch.hpp"
#include "kit.hpp"

using namespace solvekit;
using solve::Answer;
using solve::AnswerCache;
using solve::Box;
using solve::Outcome;
using solve::Source;

namespace {

// An oracle that answers each point as FuSION 1's search log first answered it.
struct LoggedOracle final : solve::Oracle {
  std::map<Point, bool> answers;
  std::vector<solve::Witness> witnesses;
  Answer ask(Point p, Phase phase) override {
    const auto it = answers.find(p);
    if (it == answers.end()) throw std::runtime_error("FuSION 1 never asked n=" + std::to_string(p.n) + " e=" + std::to_string(p.e));
    Answer a{};
    a.point = p;
    a.phase = phase;
    a.source = Source::z3;
    a.outcome = it->second ? Outcome::sat : Outcome::unsat;
    if (it->second) {
      solve::Witness w;
      w.counts = p;
      witnesses.push_back(w);
      a.witness = static_cast<std::uint32_t>(witnesses.size() - 1);
      a.model = p;
    }
    return a;
  }
};

std::vector<Point> minimal(std::set<Point> points) {
  std::vector<Point> out;
  for (Point p : points)
    if (std::none_of(points.begin(), points.end(), [&](Point o) { return o != p && solve::below_or_equal(o, p); }))
      out.push_back(p);
  return out;
}

// The frontier the box holds for a monotone staircase, by brute force.
std::vector<Point> expected_frontier(Box box, const ScriptedOracle& oracle) {
  std::set<Point> sat;
  for (int n = box.lo.n; n <= box.hi.n; ++n)
    for (int e = box.lo.e; e <= box.hi.e; ++e)
      if (oracle.truth({n, e})) sat.insert({n, e});
  return minimal(sat);
}

// Uniform in [lo, hi], from core's own distributions (12.C9, 09.C15).
int uniform(fusion::core::Rng& rng, int lo, int hi) {
  return lo + static_cast<int>(rng.below(static_cast<std::uint64_t>(hi - lo + 1)));
}

std::vector<Point> random_staircase(fusion::core::Rng& rng, int max_n, int max_e) {
  std::set<Point> raw;
  for (int k = uniform(rng, 0, 6); k > 0; --k) {
    const int n = uniform(rng, 0, max_n + 2);
    raw.insert({n, uniform(rng, 0, max_e + 2)});
  }
  return minimal(raw);
}

Box random_box(fusion::core::Rng& rng) {
  Box b;
  b.lo.n = uniform(rng, 0, 3);
  b.lo.e = uniform(rng, 0, 3);
  b.hi.n = b.lo.n + uniform(rng, 0, 19);
  b.hi.e = b.lo.e + uniform(rng, 0, 19);
  return b;
}

}  // namespace

TEST_CASE("solve: the search replays FuSION 1's searches on the toy cases query for query", "[unit][solve][S5.18]") {
  // Booking 1's regenerated toy runs: 12 cases, each solved with and without its FSC. Fed FuSION 1's
  // own answers, the port must ask the same points in the same steps, cache hits included, and
  // find the same points and the same frontier as its .dat file.
  int runs = 0;
  for (const std::string& name : toy_names()) {
    const ToyCase toy = read_toy(name);
    for (const ToyRun* run : {&toy.with, &toy.without}) {
      INFO(name << (run == &toy.with ? " with" : " without") << " the FSC");
      LoggedOracle oracle;
      const auto point = [](testkit::Point p) { return Point{p.n, p.e}; };
      for (const testkit::f1::Check& q : run->limits.checks)
        if (!q.cached) oracle.answers.emplace(point(q.point), q.answer == testkit::CheckOutcome::sat);
      AnswerCache cache(run->box());
      Quiet quiet;
      const std::vector<Point> found = solve::pareto_search(run->box(), cache, oracle, quiet);
      REQUIRE(cache.answers().size() == run->limits.checks.size());
      for (std::size_t k = 0; k < run->limits.checks.size(); ++k) {
        INFO("query " << k + 1);
        CHECK(cache.answers()[k].point == point(run->limits.checks[k].point));
        CHECK(solve::name(cache.answers()[k].phase) == run->limits.checks[k].step);
        CHECK((cache.answers()[k].outcome == Outcome::sat) == (run->limits.checks[k].answer == testkit::CheckOutcome::sat));
      }
      std::vector<Point> f1_found;
      for (testkit::Point p : run->limits.found) f1_found.push_back(point(p));
      CHECK(found == f1_found);
      const solve::Frontier frontier = solve::assemble(run->box(), cache.answers(), oracle.witnesses, found);
      CHECK(points_of(frontier) == run->dat);
      for (const solve::FrontierPoint& f : frontier.points) CHECK(f.certified);
      CHECK(frontier.undecided.empty());
      ++runs;
    }
  }
  CHECK(runs == 24);
}

TEST_CASE("solve: exact staircases give the exact frontier, each point asked at most once", "[unit][solve][S5.18][S5.20]") {
  FUSION_SEED(20261118);
  auto rng = fusion::core::Rng::stream(fusion_seed, 0);
  for (int round = 0; round < 400; ++round) {
    const Box box = random_box(rng);
    ScriptedOracle oracle(random_staircase(rng, box.hi.n, box.hi.e));
    AnswerCache cache(box);
    Quiet quiet;
    const std::vector<Point> found = solve::pareto_search(box, cache, oracle, quiet);
    const solve::Frontier frontier = solve::assemble(box, cache.answers(), oracle.witnesses, found);
    INFO("round " << round);
    CHECK(points_of(frontier) == expected_frontier(box, oracle));
    std::vector<Point> sorted_found = found;
    std::sort(sorted_found.begin(), sorted_found.end());
    CHECK(sorted_found == points_of(frontier));            // exact answers: the search's own points
    for (const Answer& a : cache.answers()) {
      CHECK((a.outcome == Outcome::sat) == oracle.truth(a.point));   // the cache never changes an answer
      CHECK(oracle.calls(a.point) <= 1);
    }
    for (const solve::FrontierPoint& f : frontier.points) {
      CHECK(f.certified);
      CHECK_FALSE(f.found_at);
    }
    CHECK(frontier.undecided.empty());
  }
}

TEST_CASE("solve: unknown answers are cached, and certification names each blocker", "[unit][solve][S5.19][S5.20][06.C6]") {
  FUSION_SEED(42);
  auto rng = fusion::core::Rng::stream(fusion_seed, 0);
  int uncertified = 0;
  for (int round = 0; round < 400; ++round) {
    const Box box = random_box(rng);
    std::map<Point, Outcome> overrides;
    for (int k = 0; k < 4; ++k) {
      const int n = uniform(rng, box.lo.n, box.hi.n), e = uniform(rng, box.lo.e, box.hi.e);
      overrides[{n, e}] = uniform(rng, 0, 1) ? Outcome::timeout : Outcome::undefined;
    }
    ScriptedOracle oracle(random_staircase(rng, box.hi.n, box.hi.e), overrides);
    AnswerCache cache(box);
    Quiet quiet;
    const std::vector<Point> found = solve::pareto_search(box, cache, oracle, quiet);
    const solve::Frontier frontier = solve::assemble(box, cache.answers(), oracle.witnesses, found);
    INFO("round " << round);
    for (const Answer& a : cache.answers()) CHECK(oracle.calls(a.point) <= 1);   // S5.20
    // Brute force over the answers: what they prove, directly or by monotonicity.
    const auto sat = [&](Point p) {
      return std::any_of(cache.answers().begin(), cache.answers().end(), [&](const Answer& a) {
        return a.outcome == Outcome::sat && (solve::below_or_equal(a.point, p) || solve::below_or_equal(*a.model, p));
      });
    };
    const auto unsat = [&](Point p) {
      return std::any_of(cache.answers().begin(), cache.answers().end(),
                         [&](const Answer& a) { return a.outcome == Outcome::unsat && solve::below_or_equal(p, a.point); });
    };
    std::vector<Point> undecided;
    for (int n = box.lo.n; n <= box.hi.n; ++n)
      for (int e = box.lo.e; e <= box.hi.e; ++e)
        if (!sat({n, e}) && !unsat({n, e})) undecided.push_back({n, e});
    CHECK(frontier.undecided == undecided);
    for (const solve::FrontierPoint& f : frontier.points) {
      CHECK(oracle.truth(f.point));                         // a frontier point is proven SAT
      std::vector<Point> blockers;
      bool certified = true;
      for (Point below : {Point{f.point.n - 1, f.point.e}, Point{f.point.n, f.point.e - 1}}) {
        if (!solve::below_or_equal(box.lo, below) || unsat(below)) continue;
        certified = false;
        if (!sat(below)) blockers.push_back(below);
      }
      CHECK(f.certified == certified);
      CHECK(f.undecided_below == blockers);
      if (!certified) {
        CHECK_FALSE(f.undecided_below.empty());            // each uncertified point names its blocker
        ++uncertified;
      }
    }
    // A missing frontier point can lie only among the undecided cells.
    for (Point truth : expected_frontier(box, oracle)) {
      const auto& pts = frontier.points;
      const bool reported = std::any_of(pts.begin(), pts.end(), [&](const solve::FrontierPoint& f) { return f.point == truth; });
      if (!reported) CHECK(std::find(undecided.begin(), undecided.end(), truth) != undecided.end());
    }
  }
  CHECK(uncertified > 0);
}

TEST_CASE("solve: models' own counts answer later queries without changing any answer", "[unit][solve][06.C5]") {
  FUSION_SEED(7);
  auto rng = fusion::core::Rng::stream(fusion_seed, 0);
  int saved = 0;
  for (int round = 0; round < 300; ++round) {
    const Box box = random_box(rng);
    const std::vector<Point> staircase = random_staircase(rng, box.hi.n, box.hi.e);
    ScriptedOracle plain(staircase), modelled(staircase);
    modelled.model = [&](Point p) -> std::optional<Point> {  // the lowest staircase point below p
      for (Point s : staircase)
        if (solve::below_or_equal(s, p)) return Point{std::max(s.n, box.lo.n), std::max(s.e, box.lo.e)};
      return std::nullopt;
    };
    AnswerCache a(box), b(box);
    Quiet quiet;
    const auto found_a = solve::pareto_search(box, a, plain, quiet);
    const auto found_b = solve::pareto_search(box, b, modelled, quiet);
    INFO("round " << round);
    CHECK(found_a == found_b);                              // the steps are unchanged
    REQUIRE(a.answers().size() == b.answers().size());
    for (std::size_t k = 0; k < a.answers().size(); ++k) CHECK(a.answers()[k].outcome == b.answers()[k].outcome);
    CHECK(modelled.asked.size() <= plain.asked.size());
    saved += static_cast<int>(plain.asked.size() - modelled.asked.size());
    const bool any_model = std::any_of(b.answers().begin(), b.answers().end(),
                                       [](const Answer& x) { return x.source == Source::model; });
    if (any_model) CHECK(modelled.asked.size() < plain.asked.size());
    CHECK(points_of(solve::assemble(box, b.answers(), modelled.witnesses, found_b)) == expected_frontier(box, plain));
  }
  CHECK(saved > 0);
}

TEST_CASE("solve: the cache answers proofs first, then unknowns, and never a stopped check", "[unit][solve][S5.20][06.C5]") {
  const auto answer = [](Point p, Outcome o, Phase phase = Phase::diagonal) {
    Answer a{};
    a.point = p;
    a.phase = phase;
    a.source = Source::z3;
    a.outcome = o;
    a.reason = o == Outcome::timeout ? "timeout" : "";
    return a;
  };
  AnswerCache cache(Box{});
  CHECK_FALSE(cache.lookup({2, 2}));
  cache.record(answer({2, 2}, Outcome::timeout));
  REQUIRE(cache.lookup({2, 2}));
  CHECK(cache.lookup({2, 2})->outcome == Outcome::timeout);   // S5.20: not asked again
  CHECK(cache.lookup({2, 2})->source == Source::cache);
  CHECK_FALSE(cache.lookup({3, 3}));                         // an unknown proves nothing elsewhere

  Answer sat = answer({4, 4}, Outcome::sat);
  sat.model = Point{1, 1};
  sat.witness = 0;
  cache.record(sat);
  CHECK(cache.lookup({2, 2})->outcome == Outcome::sat);      // a proof after an unknown wins over it
  CHECK(cache.lookup({2, 2})->source == Source::model);      // by the model's own counts
  CHECK(cache.lookup({5, 4})->source == Source::cache);      // by monotonicity from the asked point
  CHECK(cache.lookup({5, 4})->witness == 0U);
  CHECK(cache.lookup({4, 4})->source == Source::cache);
  CHECK(cache.proven_sat({1, 1}));
  CHECK_FALSE(cache.proven_sat({0, 3}));

  cache.record(answer({0, 6}, Outcome::unsat));
  CHECK(cache.lookup({0, 3})->outcome == Outcome::unsat);    // below a proven-UNSAT point
  CHECK(cache.proven_unsat({0, 0}));
  CHECK_FALSE(cache.proven_unsat({1, 0}));

  cache.record(answer({1, 0}, Outcome::stopped));
  CHECK_FALSE(cache.lookup({1, 0}));                         // a stopped check is never an answer
  cache.record(answer({3, 0}, Outcome::unsat, Phase::extra));
  CHECK_FALSE(cache.proven_unsat({3, 0}));                   // "no more solutions" is not a proof
  CHECK_FALSE(cache.lookup({3, 0}));
}

TEST_CASE("solve: a stopped answer ends the search with the points so far", "[unit][solve][A1][06.C3]") {
  ScriptedOracle oracle({{2, 2}}, {{{1, 1}, Outcome::stopped}});
  AnswerCache cache(Box{{0, 0}, {4, 4}});
  Quiet quiet;
  const auto found = solve::pareto_search(Box{{0, 0}, {4, 4}}, cache, oracle, quiet);
  CHECK(found.empty());
  REQUIRE_FALSE(cache.answers().empty());
  CHECK(cache.answers().back().outcome == Outcome::stopped);
  CHECK(cache.answers().back().point == Point{1, 1});
  CHECK_FALSE(cache.lookup({1, 1}));                         // a resume asks it again
}

TEST_CASE("solve: a point beaten by its own witness is replaced by the witness's counts", "[unit][solve][06.C20]") {
  // n in [0, 3] at e = 0. The check at (1, 0) times out, so the line search settles on (2, 0), whose
  // model relaxes one node only: the frontier takes (1, 0) and keeps (2, 0) in found_at.
  const Box box{{0, 0}, {3, 0}};
  ScriptedOracle oracle({{1, 0}}, {{{1, 0}, Outcome::timeout}});
  oracle.model = [](Point p) -> std::optional<Point> { return p == Point{2, 0} ? std::optional<Point>(Point{1, 0}) : std::nullopt; };
  AnswerCache cache(box);
  Quiet quiet;
  const auto found = solve::pareto_search(box, cache, oracle, quiet);
  CHECK(found == std::vector<Point>{{2, 0}});
  const solve::Frontier frontier = solve::assemble(box, cache.answers(), oracle.witnesses, found);
  REQUIRE(frontier.points.size() == 1);
  CHECK(frontier.points[0].point == Point{1, 0});
  CHECK(frontier.points[0].found_at == Point{2, 0});
  CHECK_FALSE(frontier.points[0].certified);                 // (0, 0) was never decided
  CHECK(frontier.points[0].undecided_below == std::vector<Point>{{0, 0}});
  // Without the search's points, found_at names where the witness was found.
  CHECK(solve::assemble(box, cache.answers(), oracle.witnesses).points[0].found_at == Point{2, 0});
}

TEST_CASE("solve: a witness below the box's lower corner counts at the corner", "[unit][solve][06.C20]") {
  // FuSION 1's resolve_wo_n7_e0 batch asked a box with n from 7: a model may relax fewer nodes.
  const Box box{{7, 0}, {9, 2}};
  ScriptedOracle oracle({{3, 1}});
  oracle.model = [](Point) -> std::optional<Point> { return Point{3, 1}; };
  AnswerCache cache(box);
  Quiet quiet;
  const auto found = solve::pareto_search(box, cache, oracle, quiet);
  const solve::Frontier frontier = solve::assemble(box, cache.answers(), oracle.witnesses, found);
  CHECK(points_of(frontier) == std::vector<Point>{{7, 1}});
  CHECK(frontier.points[0].certified);                       // (6, 1) is outside; (7, 0) is unsat
}

TEST_CASE("solve: the observer sees each started check, each answer and each point", "[unit][solve][A2]") {
  struct Recorder final : solve::SearchObserver {
    std::vector<std::string> events;
    void on_started(Point p, Phase) override { events.push_back("started " + std::to_string(p.n) + "," + std::to_string(p.e)); }
    void on_answer(const Answer& a) override {
      events.push_back("answer " + std::to_string(a.check) + " " + std::string(solve::name(a.source)));
    }
    void on_point(Point p) override { events.push_back("point " + std::to_string(p.n) + "," + std::to_string(p.e)); }
  } recorder;
  ScriptedOracle oracle({{1, 1}});
  AnswerCache cache(Box{{0, 0}, {2, 2}});
  solve::pareto_search(Box{{0, 0}, {2, 2}}, cache, oracle, recorder);
  std::size_t started = 0, answers = 0, points = 0;
  for (const std::string& e : recorder.events) {
    started += e.rfind("started", 0) == 0;
    answers += e.rfind("answer", 0) == 0;
    points += e.rfind("point", 0) == 0;
  }
  CHECK(started == oracle.asked.size());                     // only Z3's checks start
  CHECK(answers == cache.answers().size());                  // every query is reported
  CHECK(points == 1);
  for (std::size_t k = 0; k < cache.answers().size(); ++k) CHECK(cache.answers()[k].check == k + 1);
}
