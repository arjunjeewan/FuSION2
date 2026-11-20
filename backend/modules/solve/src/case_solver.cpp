// The Z3 layer (06, "Encoding" and "The Z3 layer"): one context and one QF_BV solver per case
// (06.C1), the encoding of S5.05 to S5.17, checks pushed as FuSION 1's search pushed them, the
// TIMEOUT label (06.C2), witnesses read from the search's own model (S5.26) and extra solutions
// (06.C11). This is the only source that includes z3++.h (11.C17).
#include "fusion/solve/solve.hpp"

#include <z3++.h>
#include <z3_version.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <ctime>
#include <map>
#include <mutex>

#include "fusion/core/platform.hpp"
#include "internal.hpp"

namespace fusion::solve {

namespace detail {

// The resolution of the thread CPU clock that core::thread_cpu_time() reads (06, "Platform notes"):
// clock_gettime(CLOCK_THREAD_CPUTIME_ID) on Linux and macOS, GetThreadTimes on Windows.
double thread_cpu_resolution() noexcept {
#if defined(_WIN32)
  return 0.015625;                                         // the scheduler tick GetThreadTimes advances by
#else
  timespec ts{};
  if (clock_getres(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) return 0;
  return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
#endif
}

}  // namespace detail

namespace {

// bit_width(k): the bits that hold k, so a clamped bound never wraps (S5.09). FuSION 1's widths were
// ceil(log2(x + 1)) and, for the node count, ceil(log2(|up| + |down|)) + 1 (macros.h:43-50).
unsigned width_for(std::uint64_t k) noexcept { return std::max(1U, static_cast<unsigned>(std::bit_width(k))); }

unsigned as_unsigned(std::uint64_t value) noexcept {
  return static_cast<unsigned>(std::min<std::uint64_t>(value, UINT_MAX));
}

// The Z3 terms of one case: the variable families of S5.09, named by index (06, "Encoding").
struct Encoding {
  explicit Encoding(z3::context& c) : count_n(c), count_e(c) {}
  std::vector<z3::expr> p, a, d, s, x, f;                  // present, active, distance; selected, relaxed, path flag
  std::vector<std::optional<z3::expr>> r;                  // relaxed, for counted observations only
  z3::expr count_n, count_e;                               // C_n and C_e, defined by the two sums
  unsigned width_n = 1, width_e = 1;
  std::uint64_t max_n = 0, max_e = 0;                      // R_n and R_e
};

std::string indexed(char family, std::size_t i) { return family + std::to_string(i); }

std::unique_ptr<Encoding> encode(z3::context& c, z3::solver& solver, const Problem& problem) {
  auto enc = std::make_unique<Encoding>(c);
  const std::size_t nodes = problem.nodes.size(), edges = problem.edges.size();
  const unsigned width_d = width_for(problem.bound);
  enc->r.resize(nodes);
  for (std::size_t i = 0; i < nodes; ++i) {
    enc->p.push_back(c.bool_const(indexed('p', i).c_str()));
    enc->a.push_back(c.bool_const(indexed('a', i).c_str()));
    enc->d.push_back(c.bv_const(indexed('d', i).c_str(), width_d));
  }
  for (std::uint32_t i : problem.counted) enc->r[i] = c.bool_const(indexed('r', i).c_str());
  for (std::size_t j = 0; j < edges; ++j) {
    enc->s.push_back(c.bool_const(indexed('s', j).c_str()));
    enc->x.push_back(c.bool_const(indexed('x', j).c_str()));
    enc->f.push_back(c.bool_const(indexed('f', j).c_str()));
  }
  const auto& p = enc->p;
  const auto& a = enc->a;
  const auto& d = enc->d;
  const auto& s = enc->s;
  const auto& x = enc->x;
  const auto& f = enc->f;
  std::vector<std::vector<std::uint32_t>> in(nodes), out(nodes);
  for (std::uint32_t j = 0; j < edges; ++j) {
    out[problem.edges[j].src].push_back(j);
    in[problem.edges[j].tgt].push_back(j);
  }
  const auto any_selected = [&](const std::vector<std::uint32_t>& list) {
    z3::expr_vector terms(c);
    for (std::uint32_t j : list) terms.push_back(s[j]);
    return z3::mk_or(terms);                               // empty: false, as FuSION 1's bool_val(false)
  };

  // Node and edge lists, the FSC and the endpoint defaults (S5.05, S5.06, S5.17, 06.C16), in the
  // order of get_query_assertions_as_constraints_with_z3 (GraphManagerNew.cc:28306-28411).
  for (std::uint32_t i : problem.present) solver.add(p[i]);
  for (std::uint32_t i : problem.absent) solver.add(!p[i]);
  for (std::uint32_t j : problem.edge_selected) solver.add(s[j]);
  for (std::uint32_t j : problem.edge_avoided) solver.add(!s[j]);
  for (std::uint32_t i : problem.active) solver.add(a[i]);
  for (std::uint32_t i : problem.inactive) solver.add(!a[i]);
  for (std::uint32_t i : problem.consistent)               // what FuSION 1's "not relaxed" implied (S5.05)
    solver.add(problem.regulation[i] == expression::Regulation::up ? (p[i] && a[i]) : !(p[i] && a[i]));
  for (std::uint32_t j : problem.edge_nonrelaxed) solver.add(!x[j]);

  // Paths (S5.11): a selected edge has present ends and increasing distances (28064-28119); the
  // target's distance is within the bound, without FuSION 1's __pathbound (28121-28141); a present
  // node has the selected edges its role needs (28143-28304).
  for (std::uint32_t j = 0; j < edges; ++j) {
    const auto& e = problem.edges[j];
    solver.add(z3::implies(s[j], p[e.src] && p[e.tgt]));
    solver.add(z3::implies(s[j], z3::ult(d[e.src], d[e.tgt])));
  }
  solver.add(z3::ule(d[problem.target], c.bv_val(problem.bound, width_d)));
  solver.add(z3::implies(p[problem.source], any_selected(out[problem.source])));
  solver.add(z3::implies(p[problem.target], any_selected(in[problem.target])));
  for (std::uint32_t i = 0; i < nodes; ++i)
    if (i != problem.source && i != problem.target)
      solver.add(z3::implies(p[i], any_selected(in[i]) && any_selected(out[i])));

  // Node relaxation (S5.12; 28456-28524): an up gene is relaxed unless present and active, a down
  // gene when present and active. Only counted observations take part.
  enc->max_n = problem.counted.size();
  enc->width_n = width_for(enc->max_n);
  z3::expr sum_n = c.bv_val(0, enc->width_n);
  for (std::uint32_t i : problem.counted) {
    const z3::expr& relaxed = *enc->r[i];
    const z3::expr on = p[i] && a[i];
    solver.add(relaxed == (problem.regulation[i] == expression::Regulation::up ? !on : on));
    sum_n = sum_n + z3::ite(relaxed, c.bv_val(1, enc->width_n), c.bv_val(0, enc->width_n));
  }
  // C_n is a variable defined once in the base, so a check pushes only its bound and the adder
  // chain is not internalised again in every scope (06, "Encoding": C_n = sum of ite(r_v, 1, 0)).
  enc->count_n = c.bv_const("cn", enc->width_n);
  solver.add(enc->count_n == sum_n);

  // Every selected path crosses a DE gene before the target (S5.14; 28731-28909), one assertion
  // per edge or node instead of FuSION 1's one conjunction.
  std::vector<bool> de(nodes);
  for (std::size_t i = 0; i < nodes; ++i) de[i] = problem.regulation[i] != expression::Regulation::none;
  for (std::uint32_t j : in[problem.target]) solver.add(z3::implies(s[j], f[j]));         // rule 1
  for (std::uint32_t i = 0; i < nodes; ++i) {                                              // rule 2
    if (de[i] || in[i].empty() || out[i].empty()) continue;
    z3::expr_vector cleared_in(c), cleared_out(c);
    for (std::uint32_t j : in[i]) cleared_in.push_back(s[j] && !f[j]);
    for (std::uint32_t j : out[i]) cleared_out.push_back(!f[j]);
    solver.add(z3::implies(z3::mk_or(cleared_in), z3::mk_and(cleared_out)));
  }
  if (!de[problem.source])                                                                // rule 3
    for (std::uint32_t j : out[problem.source]) solver.add(!f[j]);
  for (std::uint32_t i = 0; i < nodes; ++i)                                               // rule 4
    if (de[i])
      for (std::uint32_t j : out[i]) solver.add(f[j]);

  // Edge relaxation (S5.13; 28526-28599), by 02's one polarity rule (S6.14): the disjuncts are the
  // end states it calls relaxed, in FuSION 1's order. Every encoded edge counts, selected or not.
  const auto state = [&](std::uint32_t i, network::EndState st) -> z3::expr {
    switch (st) {
      case network::EndState::absent: return !p[i];
      case network::EndState::inactive: return p[i] && !a[i];
      case network::EndState::active: return p[i] && a[i];
    }
    return !p[i];
  };
  constexpr std::array<network::EndState, 3> order{network::EndState::active, network::EndState::inactive,
                                                   network::EndState::absent};
  enc->max_e = edges;
  enc->width_e = width_for(enc->max_e);
  z3::expr sum_e = c.bv_val(0, enc->width_e);
  for (std::uint32_t j = 0; j < edges; ++j) {
    const auto& e = problem.edges[j];
    const std::uint16_t mask = problem.relaxed_states[static_cast<std::size_t>(e.effect)];
    z3::expr_vector cases(c);
    for (network::EndState from : order)
      for (network::EndState to : order)
        if (mask & (1U << (3 * static_cast<unsigned>(from) + static_cast<unsigned>(to))))
          cases.push_back(state(e.src, from) && state(e.tgt, to));
    solver.add(x[j] == z3::mk_or(cases));
    sum_e = sum_e + z3::ite(x[j], c.bv_val(1, enc->width_e), c.bv_val(0, enc->width_e));
  }
  enc->count_e = c.bv_const("ce", enc->width_e);           // C_e, as C_n above
  solver.add(enc->count_e == sum_e);

  // Coexpression (S5.15; 28682-28709) and frozen edges (S5.16; 28711-28729).
  for (const auto& [u, v] : problem.coexpressed) solver.add(z3::implies(p[u] && p[v], a[u] == a[v]));
  for (std::uint32_t j : problem.frozen) solver.add(!x[j]);
  return enc;
}

}  // namespace

// ---------------------------------------------------------------- CaseSolver

struct CaseSolver::Impl {
  Impl(const Problem& p, const Settings& s, core::Context* c) : problem(p), settings(s), run(c) {
    for (std::uint32_t i = 0; i < problem.nodes.size(); ++i) node_index.emplace(problem.nodes[i].value, i);
    for (std::uint32_t j = 0; j < problem.edges.size(); ++j) edge_index.emplace(problem.edges[j].key, j);
  }

  enum class Frame : std::uint8_t { none, edges, nodes };

  const Problem& problem;
  Settings settings;
  core::Context* run;                                      // for debug dumps (06.C13), later
  std::map<std::string, std::uint32_t, std::less<>> node_index;
  std::map<network::EdgeKey, std::uint32_t> edge_index;

  std::mutex mutex;                                        // context creation against interrupt()
  std::atomic<bool> interrupted{false};
  std::unique_ptr<z3::context> context;
  std::unique_ptr<z3::solver> solver;
  std::unique_ptr<Encoding> enc;                           // destroyed before the context
  Frame frame = Frame::none;
  int frame_value = 0;

  std::vector<Witness> witnesses;
  Answer last{};

  void build() {
    std::lock_guard lock(mutex);
    if (context) return;
    context = std::make_unique<z3::context>();
    solver = std::make_unique<z3::solver>(*context, "QF_BV");   // main.cc:4721-4722
    z3::params params(*context);
    // main.cc:4727-4736: ":solver2_timeout" is the combined solver's incremental phase, ":timeout"
    // the cap per check (06.C2). ctrl_c=false lets Ctrl-C reach only fusion's handler (11.C18).
    if (settings.timeout.count() > 0) params.set("timeout", as_unsigned(static_cast<std::uint64_t>(settings.timeout.count())));
    if (settings.incremental_timeout.count() > 0)
      params.set("solver2_timeout", as_unsigned(static_cast<std::uint64_t>(settings.incremental_timeout.count())));
    if (settings.rlimit > 0) params.set("rlimit", as_unsigned(settings.rlimit));   // 2B.8
    params.set("random_seed", static_cast<unsigned>(settings.seed));               // S5.22
    params.set("ctrl_c", false);
    solver->set(params);
    enc = encode(*context, *solver, problem);
  }

  z3::expr node_bound(int n) {
    const auto value = std::min<std::uint64_t>(static_cast<std::uint64_t>(std::max(n, 0)), enc->max_n);
    return z3::ule(enc->count_n, context->bv_val(value, enc->width_n));
  }
  z3::expr edge_bound(int e) {
    const auto value = std::min<std::uint64_t>(static_cast<std::uint64_t>(std::max(e, 0)), enc->max_e);
    return z3::ule(enc->count_e, context->bv_val(value, enc->width_e));
  }

  // The binary searches' fixed bound, pushed once while the moving one is pushed per check
  // (GraphManagerNew.cc:29572-29675, 29687-29788; 06.C1).
  void set_frame(Frame wanted, int value) {
    if (frame == wanted && (wanted == Frame::none || frame_value == value)) return;
    if (frame != Frame::none) solver->pop();
    frame = Frame::none;
    if (wanted == Frame::none) return;
    solver->push();
    solver->add(wanted == Frame::edges ? edge_bound(value) : node_bound(value));
    frame = wanted;
    frame_value = value;
  }

  Answer start(Point point, Phase phase) const {
    Answer answer{};
    answer.point = point;
    answer.phase = phase;
    answer.source = Source::z3;
    answer.outcome = Outcome::stopped;
    answer.timeout = settings.timeout;
    answer.rlimit = settings.rlimit;
    return answer;
  }

  // One check inside the current push: timed, labelled, and on SAT the witness read before pop.
  void run_check(Answer& answer) {
    const auto wall0 = std::chrono::steady_clock::now();
    const auto cpu0 = core::thread_cpu_time();
    const z3::check_result result = solver->check();
    answer.cpu_s = std::chrono::duration<double>(core::thread_cpu_time() - cpu0).count();
    answer.wall_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
    if (result == z3::sat) {
      answer.outcome = Outcome::sat;
      witnesses.push_back(read_witness(solver->get_model()));
      answer.witness = static_cast<std::uint32_t>(witnesses.size() - 1);
      answer.model = witnesses.back().counts;
    } else if (result == z3::unsat) {
      answer.outcome = Outcome::unsat;
    } else {
      answer.reason = solver->reason_unknown();            // before pop (06.C2)
      answer.outcome = unknown_outcome(answer.reason, settings.rlimit > 0, interrupted.load());
    }
  }

  // S5.26, S5.27: present nodes by activity, selected edges, relaxed nodes with presence and
  // relaxed edges with selection, by stable keys; absent nodes carry no activity. Variables are
  // evaluated through their handles, where FuSION 1 parsed names (GraphManagerNew.cc:30729-30846).
  Witness read_witness(const z3::model& model) {
    Witness w;
    const auto truth = [&](const z3::expr& e) { return model.eval(e, true).is_true(); };
    std::vector<bool> present(problem.nodes.size()), selected(problem.edges.size());
    for (std::size_t i = 0; i < problem.nodes.size(); ++i) {
      present[i] = truth(enc->p[i]);
      if (present[i]) (truth(enc->a[i]) ? w.active : w.inactive).push_back(problem.nodes[i]);
    }
    for (std::size_t j = 0; j < problem.edges.size(); ++j) {
      selected[j] = truth(enc->s[j]);
      if (selected[j]) w.selected.push_back(problem.edges[j].key);
    }
    for (std::uint32_t i : problem.counted)
      if (truth(*enc->r[i])) w.relaxed_nodes.emplace_back(problem.nodes[i], present[i]);
    for (std::size_t j = 0; j < problem.edges.size(); ++j)
      if (truth(enc->x[j])) w.relaxed_edges.emplace_back(problem.edges[j].key, selected[j]);
    w.counts.n = static_cast<int>(model.eval(enc->count_n, true).get_numeral_uint());
    w.counts.e = static_cast<int>(model.eval(enc->count_e, true).get_numeral_uint());
    return w;
  }

  // 06.C11's blocking clause: not the same presence on N, the same activity on the present nodes
  // and the same selection on E. FuSION 1 blocked every Boolean, don't-cares included (S5.26).
  z3::expr blocking(const Witness& w) {
    z3::context& c = *context;
    std::vector<int> state(problem.nodes.size(), 0);       // 0 absent, 1 inactive, 2 active
    for (const auto& key : w.inactive)
      if (auto it = node_index.find(key.value); it != node_index.end()) state[it->second] = 1;
    for (const auto& key : w.active)
      if (auto it = node_index.find(key.value); it != node_index.end()) state[it->second] = 2;
    std::vector<bool> selected(problem.edges.size(), false);
    for (const auto& key : w.selected)
      if (auto it = edge_index.find(key); it != edge_index.end()) selected[it->second] = true;
    z3::expr_vector same(c);
    for (std::size_t i = 0; i < problem.nodes.size(); ++i) {
      same.push_back(state[i] ? enc->p[i] : !enc->p[i]);
      if (state[i]) same.push_back(state[i] == 2 ? enc->a[i] : !enc->a[i]);
    }
    for (std::size_t j = 0; j < problem.edges.size(); ++j) same.push_back(selected[j] ? enc->s[j] : !enc->s[j]);
    return !z3::mk_and(same);
  }
};

CaseSolver::CaseSolver(const Problem& problem, const Settings& settings, core::Context& ctx)
    : impl_(std::make_unique<Impl>(problem, settings, &ctx)) {}

CaseSolver::CaseSolver(const Problem& problem, const Settings& settings)
    : impl_(std::make_unique<Impl>(problem, settings, nullptr)) {}

CaseSolver::~CaseSolver() = default;

// A check at (n, e) (06, "A check"): the bounds pushed as FuSION 1's search pushed them (each
// diagonal check both, GraphManagerNew.cc:29438-29465; each binary search its fixed bound once),
// then check, label, read the model and pop.
Answer CaseSolver::check(Point point, Phase phase) {
  Impl& m = *impl_;
  Answer answer = m.start(point, phase);
  if (m.interrupted.load()) {                              // an interrupted context is not reused
    answer.reason = "interrupted";
    return answer;
  }
  try {
    m.build();
    if (phase == Phase::smallest_n) m.set_frame(Impl::Frame::edges, point.e);
    else if (phase == Phase::smallest_e) m.set_frame(Impl::Frame::nodes, point.n);
    else m.set_frame(Impl::Frame::none, 0);
    m.solver->push();
    if (phase != Phase::smallest_e) m.solver->add(m.node_bound(point.n));
    if (phase != Phase::smallest_n) m.solver->add(m.edge_bound(point.e));
    m.run_check(answer);
    m.solver->pop();
  } catch (const z3::exception& e) {
    // An interrupt that lands outside check(), in an add, a push or reading the model, comes back as
    // the exception "canceled": the case is ending, so the check counts as stopped, not as a failure.
    if (m.interrupted.load()) {
      answer = m.start(point, phase);
      answer.reason = "interrupted";
      return answer;
    }
    throw core::Failure("solve.z3", std::string("Z3 failed at n=") + std::to_string(point.n) + " e=" +
                                        std::to_string(point.e) + ": " + e.msg());
  }
  return answer;
}

// 06.C11: one more witness at a point, blocking every witness seen there so far. FuSION 1 kept the
// point's bounds pushed and added each blocking clause inside them (main.cc:4980-5062).
std::optional<Witness> CaseSolver::next_solution(Point point, std::span<const Witness> seen) {
  Impl& m = *impl_;
  m.last = m.start(point, Phase::extra);
  if (m.interrupted.load()) {
    m.last.reason = "interrupted";
    return std::nullopt;
  }
  try {
    m.build();
    m.set_frame(Impl::Frame::none, 0);
    m.solver->push();
    m.solver->add(m.node_bound(point.n));
    m.solver->add(m.edge_bound(point.e));
    for (const Witness& w : seen) m.solver->add(m.blocking(w));
    m.run_check(m.last);
    m.solver->pop();
  } catch (const z3::exception& e) {
    if (m.interrupted.load()) {                            // as in check(): an interrupt, not a failure
      m.last = m.start(point, Phase::extra);
      m.last.reason = "interrupted";
      return std::nullopt;
    }
    throw core::Failure("solve.z3", std::string("Z3 failed on an extra solution at n=") + std::to_string(point.n) +
                                        " e=" + std::to_string(point.e) + ": " + e.msg());
  }
  if (m.last.outcome != Outcome::sat) return std::nullopt;
  return m.witnesses[*m.last.witness];
}

// Z3_interrupt stops only a check that is running, so the solve's monitor calls this again until
// the case's thread returns (06, "Interrupt").
void CaseSolver::interrupt() noexcept {
  Impl& m = *impl_;
  std::lock_guard lock(m.mutex);
  m.interrupted.store(true);
  if (m.context) m.context->interrupt();
}

std::span<const Witness> CaseSolver::witnesses() const noexcept { return impl_->witnesses; }
const Answer& CaseSolver::last_answer() const noexcept { return impl_->last; }
bool CaseSolver::interrupted() const noexcept { return impl_->interrupted.load(); }

// ---------------------------------------------------------------- the Z3 library (11.C17)

std::string z3_version() {
  unsigned major = 0, minor = 0, build = 0, revision = 0;
  Z3_get_version(&major, &minor, &build, &revision);
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build) + "." +
         std::to_string(revision);
}

// Compares the loaded library's numbers with the headers solve was built against; never the
// version string, whose format changed between releases (section 11, "Z3").
void verify_z3() {
  unsigned major = 0, minor = 0, build = 0, revision = 0;
  Z3_get_version(&major, &minor, &build, &revision);
  if (major == Z3_MAJOR_VERSION && minor == Z3_MINOR_VERSION && build == Z3_BUILD_NUMBER &&
      revision == Z3_REVISION_NUMBER)
    return;
  const std::string built = std::to_string(Z3_MAJOR_VERSION) + "." + std::to_string(Z3_MINOR_VERSION) + "." +
                            std::to_string(Z3_BUILD_NUMBER) + "." + std::to_string(Z3_REVISION_NUMBER);
  throw core::InstallError("install.z3_mismatch",
                           "the Z3 library loaded is " + z3_version() + ", but fusion was built against " + built +
                               " (11.C17)");
}

}  // namespace fusion::solve
