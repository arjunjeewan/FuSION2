// The Pareto search (S5.18), its answer cache (S5.20, 06.C5) and the frontier's assembly with
// certification (06.C6, 06.C20). Nothing here knows Z3: the search asks an Oracle.
#include "fusion/solve/solve.hpp"

#include <algorithm>

namespace fusion::solve {

namespace {

bool in_box(Box box, Point p) noexcept { return below_or_equal(box.lo, p) && below_or_equal(p, box.hi); }

// An UNSAT answer proves its point UNSAT, unless it is an extra solution's "no more" (06.C11),
// which blocked the witnesses found before it.
bool unsat_proof(const Answer& a) noexcept { return a.outcome == Outcome::unsat && a.phase != Phase::extra; }

// What the answers prove at p, directly or by monotonicity, a model's own counts included (06.C5).
bool proves_sat(std::span<const Answer> answers, Point p) {
  return std::any_of(answers.begin(), answers.end(), [p](const Answer& a) {
    return a.outcome == Outcome::sat && (below_or_equal(a.point, p) || (a.model && below_or_equal(*a.model, p)));
  });
}
bool proves_unsat(std::span<const Answer> answers, Point p) {
  return std::any_of(answers.begin(), answers.end(),
                     [p](const Answer& a) { return unsat_proof(a) && below_or_equal(p, a.point); });
}

Answer from_cache(const Answer& proof, Source source) {
  Answer a = proof;
  a.source = source;
  a.wall_s = 0;
  a.cpu_s = 0;
  return a;
}

}  // namespace

// ---------------------------------------------------------------- the cache

// A few dozen answers per case (06, "Search and cache"), so plain scans serve every lookup.
AnswerCache::AnswerCache(Box) {}

void AnswerCache::record(const Answer& answer) { answers_.push_back(answer); }

std::span<const Answer> AnswerCache::answers() const noexcept { return answers_; }

bool AnswerCache::proven_sat(Point p) const noexcept { return proves_sat(answers_, p); }

bool AnswerCache::proven_unsat(Point p) const noexcept { return proves_unsat(answers_, p); }

// 06.C5's order: a proof (an answer at the point, then monotonicity, a model's own counts
// included), then an unknown answer at the point (S5.20), else nothing and the caller asks Z3.
// FuSION 1 cached SAT and UNSAT at exact points only (GraphManagerNew.cc:29423-29432,
// 29580-29589, 29695-29704) and asked TIMEOUT and UNDEFINED points again (S5.20).
// A stopped answer is never an answer (06, "Search and cache").
std::optional<Answer> AnswerCache::lookup(Point p) const {
  for (const Answer& a : answers_)
    if (a.point == p && (a.outcome == Outcome::sat || unsat_proof(a))) return from_cache(a, Source::cache);
  for (const Answer& a : answers_)
    if (a.outcome == Outcome::sat && below_or_equal(a.point, p)) return from_cache(a, Source::cache);
  for (const Answer& a : answers_)
    if (a.outcome == Outcome::sat && a.model && below_or_equal(*a.model, p)) return from_cache(a, Source::model);
  for (const Answer& a : answers_)
    if (unsat_proof(a) && below_or_equal(p, a.point)) return from_cache(a, Source::cache);
  for (auto it = answers_.rbegin(); it != answers_.rend(); ++it)
    if (it->point == p && (it->outcome == Outcome::timeout || it->outcome == Outcome::undefined))
      return from_cache(*it, Source::cache);
  return std::nullopt;
}

// ---------------------------------------------------------------- the search

namespace {

struct SearchStopped {};                                   // a stopped answer ends the search

// macros.h:79-80
int prev_along_diagonal(int value, int lower) noexcept { return value - 1 >= lower ? value - 1 : lower; }
int next_along_diagonal(int value, int upper) noexcept { return value + 1 <= upper ? value + 1 : upper; }

class Search {
public:
  Search(AnswerCache& cache, Oracle& oracle, SearchObserver& observer)
      : cache_(cache), oracle_(oracle), observer_(observer) {}

  // FuSION 1's get_PO_points_within_limits, line for line (GraphManagerNew.cc:29349-29833),
  // with (n, e) points where FuSION 1 wrote (e, n). Unknown answers steer as UNSAT does there.
  void window(int nl, int nu, int el, int eu) {
    if (nu < nl || eu < el) return;                        // "Invalid window" (29364-29368)

    // Step 1: the diagonal search (29374-29533).
    int N_h = nu, N_l = nl, E_h = eu, E_l = el;
    int curr_n = -1, curr_e = -1;
    bool seen_sat = false;
    int nd = -1, ed = -1;                                  // FuSION 1's first_sat_n, first_sat_e
    for (;;) {
      int n1 = (N_l + N_h) / 2;
      int e1 = (E_l + E_h) / 2;
      if (n1 == curr_n && e1 == curr_e) {                  // 29394-29415, kept as written
        n1 = next_along_diagonal(curr_n, N_h);
        e1 = next_along_diagonal(curr_e, E_h);
        if (n1 == curr_n && e1 == curr_e) break;
      } else {
        curr_n = n1;
        curr_e = e1;
      }
      if (sat({curr_n, curr_e}, Phase::diagonal)) {        // 29495-29504
        seen_sat = true;
        nd = curr_n;
        ed = curr_e;
        N_h = prev_along_diagonal(curr_n, N_l);
        E_h = prev_along_diagonal(curr_e, E_l);
      } else {                                             // 29505-29531
        N_l = next_along_diagonal(curr_n, N_h);
        E_l = next_along_diagonal(curr_e, E_h);
      }
    }
    if (!seen_sat) return;                                 // 29535-29538: no point in the window

    if (nl == nu || el == eu) {                            // a line or a point (29544-29558)
      found({nd, ed});
    } else {
      // Step 2: the smallest n with e <= ed (29565-29678).
      N_h = nd;
      N_l = nl;
      while (N_h > N_l) {
        const int cn = (N_h + N_l) / 2;
        if (sat({cn, ed}, Phase::smallest_n)) {
          N_h = cn;
        } else {
          if (N_l == cn) break;
          N_l = cn;
        }
      }
      // Step 3: the smallest e with n <= nd (29680-29791).
      E_h = ed;
      E_l = el;
      while (E_h > E_l) {
        const int ce = (E_h + E_l) / 2;
        if (sat({nd, ce}, Phase::smallest_e)) {
          E_h = ce;
        } else {
          if (E_l == ce) break;
          E_l = ce;
        }
      }
      // Step 4: one or two points (29793-29826).
      if (E_h != ed && N_h != nd) {
        found({N_h, ed});
        found({nd, E_h});
      } else {
        found({N_h, E_h});
      }
    }
    // Step 5: the two child windows (29828-29829).
    window(nd + 1, nu, el, E_h - 1);
    window(nl, N_h - 1, ed + 1, eu);
  }

  std::vector<Point> points;

private:
  bool sat(Point p, Phase phase) {
    std::optional<Answer> answer = cache_.lookup(p);
    if (!answer) {
      observer_.on_started(p, phase);
      answer = oracle_.ask(p, phase);
    }
    answer->point = p;
    answer->phase = phase;
    answer->check = static_cast<std::uint32_t>(cache_.answers().size() + 1);
    cache_.record(*answer);
    observer_.on_answer(*answer);
    if (answer->outcome == Outcome::stopped) throw SearchStopped{};
    return answer->outcome == Outcome::sat;
  }

  void found(Point p) {
    points.push_back(p);
    observer_.on_point(p);
  }

  AnswerCache& cache_;
  Oracle& oracle_;
  SearchObserver& observer_;
};

}  // namespace

std::vector<Point> pareto_search(Box box, AnswerCache& cache, Oracle& oracle, SearchObserver& observer) {
  Search search(cache, oracle, observer);
  try {
    search.window(box.lo.n, box.hi.n, box.lo.e, box.hi.e);  // main.cc:4880: [x, z] x [w, y]
  } catch (const SearchStopped&) {
  }
  return search.points;
}

// ---------------------------------------------------------------- the frontier

namespace {

// Where a witness was found: the point of the first answer that carries it.
std::optional<Point> found_point(std::span<const Answer> answers, std::uint32_t witness) {
  for (const Answer& a : answers)
    if (a.outcome == Outcome::sat && a.witness == witness) return a.point;
  return std::nullopt;
}

Frontier assemble_with(Box box, std::span<const Answer> answers, std::span<const Witness> witnesses,
                       std::optional<std::span<const Point>> search_points) {
  // 06.C20: the candidates are the counts of the witnesses of every SAT answer in the box. A
  // witness's counts may lie below the box's lower corner, where nothing was asked; any point
  // above them is SAT, so they count at the box's corner.
  struct Candidate { Point point; std::uint32_t witness; };
  std::vector<Candidate> candidates;
  for (const Answer& a : answers) {
    if (a.outcome != Outcome::sat || !a.witness || *a.witness >= witnesses.size() || !in_box(box, a.point)) continue;
    Point c = witnesses[*a.witness].counts;
    c.n = std::max(c.n, box.lo.n);
    c.e = std::max(c.e, box.lo.e);
    if (in_box(box, c)) candidates.push_back({c, *a.witness});
  }

  Frontier frontier;
  for (const Candidate& c : candidates) {
    const bool dominated = std::any_of(candidates.begin(), candidates.end(), [&](const Candidate& o) {
      return below_or_equal(o.point, c.point) && o.point != c.point;
    });
    if (dominated) continue;
    auto same = std::find_if(frontier.points.begin(), frontier.points.end(),
                             [&](const FrontierPoint& f) { return f.point == c.point; });
    if (same != frontier.points.end()) {
      same->witness = std::min(same->witness, c.witness);  // the first witness found at those counts
      continue;
    }
    FrontierPoint f;
    f.point = c.point;
    f.witness = c.witness;
    frontier.points.push_back(f);
  }
  std::sort(frontier.points.begin(), frontier.points.end(),
            [](const FrontierPoint& a, const FrontierPoint& b) { return a.point < b.point; });

  for (FrontierPoint& f : frontier.points) {
    // 06.C6: certified when each lower neighbour inside the box is proven UNSAT.
    f.certified = true;
    for (Point below : {Point{f.point.n - 1, f.point.e}, Point{f.point.n, f.point.e - 1}}) {
      if (!in_box(box, below) || proves_unsat(answers, below)) continue;
      f.certified = false;
      if (!proves_sat(answers, below)) f.undecided_below.push_back(below);
    }
    // 06.C20: a point the search did not report replaces the one it found the witness at.
    std::optional<Point> where = found_point(answers, f.witness);
    if (where == f.point) where.reset();
    if (search_points) {
      const auto& sp = *search_points;
      if (std::find(sp.begin(), sp.end(), f.point) == sp.end()) {
        auto replaced = std::find_if(sp.begin(), sp.end(), [&](Point p) { return below_or_equal(f.point, p); });
        f.found_at = replaced != sp.end() ? std::optional<Point>(*replaced) : where;
      }
    } else {
      f.found_at = where;
    }
  }

  // The undecided cells: a missing frontier point can lie only among them (S5.19).
  for (int n = box.lo.n; n <= box.hi.n; ++n)
    for (int e = box.lo.e; e <= box.hi.e; ++e)
      if (!proves_sat(answers, {n, e}) && !proves_unsat(answers, {n, e})) frontier.undecided.push_back({n, e});
  return frontier;
}

}  // namespace

Frontier assemble(Box box, std::span<const Answer> answers, std::span<const Witness> witnesses) {
  return assemble_with(box, answers, witnesses, std::nullopt);
}

Frontier assemble(Box box, std::span<const Answer> answers, std::span<const Witness> witnesses,
                  std::span<const Point> search_points) {
  return assemble_with(box, answers, witnesses, search_points);
}

}  // namespace fusion::solve
