// The solve tests' kit (kit.hpp).
#include "kit.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>

namespace solvekit {

namespace {

std::string slurp(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path.string());
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<std::string> words(const std::string& text) {
  std::istringstream in(text);
  std::vector<std::string> out;
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    out.push_back(line);
  }
  return out;
}

bool starts_with(const std::string& s, const std::string& prefix) { return s.rfind(prefix, 0) == 0; }

std::string attribute(const std::string& tag, const std::string& name) {
  const std::string key = name + "=\"";
  const auto at = tag.find(key);
  if (at == std::string::npos) return {};
  const auto start = at + key.size();
  return tag.substr(start, tag.find('"', start) - start);
}

// The integers in a line, in order ("Relax bounds (node call): 5 2" gives 5, 2).
std::vector<int> integers(const std::string& line) {
  std::vector<int> out;
  std::size_t i = 0;
  while (i < line.size()) {
    if (std::isdigit(static_cast<unsigned char>(line[i]))) {
      int value = 0;
      const auto r = std::from_chars(line.data() + i, line.data() + line.size(), value);
      out.push_back(value);
      i = static_cast<std::size_t>(r.ptr - line.data());
    } else {
      ++i;
    }
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------- the spec's polarity (S5.13)

bool spec_relaxed(Effect effect, int source, int target) {
  if (source == 0 || target == 0) return false;            // both ends present
  if (effect == Effect::inhibition) return source == target;   // both active or both inactive
  return !(source == 2 && target == 2);                    // not both active
}

std::array<std::uint16_t, 2> spec_polarity() {
  std::array<std::uint16_t, 2> out{};
  for (Effect effect : {Effect::activation, Effect::inhibition})
    for (int s = 0; s < 3; ++s)
      for (int t = 0; t < 3; ++t)
        if (spec_relaxed(effect, s, t))
          out[static_cast<std::size_t>(effect)] |= static_cast<std::uint16_t>(1U << (3 * s + t));
  return out;
}

// ---------------------------------------------------------------- hand-built cases

Graph::Graph(std::uint32_t bound) {
  g.bound = bound;
  g.relaxed_states = spec_polarity();
}

std::uint32_t Graph::node(const std::string& key, std::vector<std::string> contains) {
  for (std::uint32_t v = 0; v < g.nodes.size(); ++v)
    if (g.nodes[v].key.value == key) return v;
  solve::CaseGraph::Node n;
  n.key = {key};
  n.label = key;
  std::sort(contains.begin(), contains.end());
  for (const std::string& c : contains) n.contains.push_back({c});
  g.nodes.push_back(n);
  return static_cast<std::uint32_t>(g.nodes.size() - 1);
}

std::uint32_t Graph::edge(const std::string& source, const std::string& target, const std::string& subtypes, double score,
                          const std::string& type, bool unscored) {
  solve::CaseGraph::Edge e;
  e.src = node(source);
  e.tgt = node(target);
  std::vector<std::string> list;
  std::stringstream in(subtypes);
  for (std::string s; std::getline(in, s, ',');)
    if (!s.empty()) list.push_back(s);
  const bool inhibits = std::find(list.begin(), list.end(), "inhibition") != list.end() ||
                        std::find(list.begin(), list.end(), "repression") != list.end();
  e.effect = inhibits ? Effect::inhibition : Effect::activation;
  e.key = EdgeKey{{source}, {target}, type, list};
  e.score = score;
  e.unscored = unscored;
  g.edges.push_back(e);
  return static_cast<std::uint32_t>(g.edges.size() - 1);
}

Graph& Graph::ends(const std::string& source, const std::string& target) {
  g.source = node(source);
  g.target = node(target);
  return *this;
}

std::uint32_t Graph::index(const std::string& key) const {
  for (std::uint32_t v = 0; v < g.nodes.size(); ++v)
    if (g.nodes[v].key.value == key) return v;
  throw std::runtime_error("no node " + key);
}

std::vector<solve::CaseSpec::Id> ids(std::initializer_list<std::string> keys) {
  std::vector<solve::CaseSpec::Id> out;
  for (const std::string& k : keys) out.push_back({k, {k}});
  return out;
}

void observe(solve::CaseSpec& spec, const solve::CaseGraph& graph, const std::string& gene, char direction) {
  solve::CaseSpec::Observation o;
  o.gene = gene;
  o.symbol = gene;
  o.regulation = direction == 'd' ? fusion::expression::Regulation::down : fusion::expression::Regulation::up;
  const bool inside = std::any_of(graph.nodes.begin(), graph.nodes.end(),
                                  [&](const solve::CaseGraph::Node& n) { return n.key.value == gene; });
  o.group = inside ? fusion::expression::MatchGroup::in_corridor : fusion::expression::MatchGroup::not_in_network;
  if (inside) o.node = NodeKey{gene};
  spec.observations.push_back(o);
}

const solve::Problem& built(const std::variant<solve::Problem, solve::Skipped>& v) {
  return std::holds_alternative<solve::Problem>(v) ? std::get<solve::Problem>(v) : *std::get<solve::Skipped>(v).problem;
}

solve::Problem problem_of(const solve::CaseGraph& graph, const solve::CaseSpec& spec, const solve::Settings& settings,
                          solve::Case kase) {
  return built(solve::make_case(graph, spec, settings, kase));
}

std::uint32_t at(const solve::Problem& p, const std::string& key) {
  for (std::uint32_t i = 0; i < p.nodes.size(); ++i)
    if (p.nodes[i].value == key) return i;
  throw std::runtime_error("no encoded node " + key);
}

// ---------------------------------------------------------------- the checker

namespace {

// The rules of S5.11 to S5.17 on one assignment, written from the spec. Stops at the first
// violation when `why` is null.
std::optional<Point> evaluate(const solve::Problem& p, const std::vector<int>& state, const std::vector<bool>& selected,
                              std::vector<std::string>* why) {
  bool ok = true;
  const auto fail = [&](const std::string& text) {
    ok = false;
    if (why) why->push_back(text);
    return why != nullptr;                                 // keep going only when explaining
  };
  const std::size_t n = p.nodes.size(), m = p.edges.size();
  std::vector<std::vector<std::uint32_t>> in(n), out(n);
  for (std::uint32_t j = 0; j < m; ++j)
    if (selected[j]) {
      const auto& e = p.edges[j];
      if ((state[e.src] == 0 || state[e.tgt] == 0) && !fail("selected edge " + std::to_string(j) + " has an absent end"))
        return std::nullopt;
      out[e.src].push_back(j);
      in[e.tgt].push_back(j);
    }
  for (std::uint32_t v = 0; v < n; ++v) {                  // S5.11: present nodes lie on paths
    if (state[v] == 0) continue;
    if (v != p.source && in[v].empty() && !fail("present " + p.nodes[v].value + " has no selected in-edge"))
      return std::nullopt;
    if (v != p.target && out[v].empty() && !fail("present " + p.nodes[v].value + " has no selected out-edge"))
      return std::nullopt;
  }
  // Distances: the least are the longest selected paths ending at each node (Kahn's order).
  std::vector<int> indegree(n, 0), depth(n, 0);
  for (std::uint32_t j = 0; j < m; ++j)
    if (selected[j]) ++indegree[p.edges[j].tgt];
  std::vector<std::uint32_t> order;
  for (std::uint32_t v = 0; v < n; ++v)
    if (indegree[v] == 0) order.push_back(v);
  for (std::size_t k = 0; k < order.size(); ++k)
    for (std::uint32_t j : out[order[k]]) {
      const std::uint32_t w = p.edges[j].tgt;
      depth[w] = std::max(depth[w], depth[order[k]] + 1);
      if (--indegree[w] == 0) order.push_back(w);
    }
  if (order.size() != n && !fail("the selected edges have a cycle")) return std::nullopt;
  const int width = std::max(1, static_cast<int>(std::bit_width(p.bound)));
  if (depth[p.target] > static_cast<int>(p.bound) && !fail("a selected path into the target exceeds the bound"))
    return std::nullopt;
  for (std::uint32_t v = 0; v < n; ++v)
    if (depth[v] > (1 << width) - 1 && !fail("a selected path is longer than the distances hold")) return std::nullopt;
  // S5.14: a selected edge is "clean" when its path so far has not crossed a DE node.
  std::vector<bool> clean(m, false);
  const auto de = [&](std::uint32_t v) { return p.regulation[v] != fusion::expression::Regulation::none; };
  for (std::uint32_t u : order)
    for (std::uint32_t j : out[u]) {
      bool from_clean = u == p.source;
      for (std::uint32_t i : in[u]) from_clean = from_clean || clean[i];
      clean[j] = !de(u) && from_clean;
    }
  for (std::uint32_t j : in[p.target])
    if (clean[j] && !fail("a selected path reaches the target without crossing a DE gene")) return std::nullopt;
  // S5.05, S5.06, S5.15 to S5.17: lists and families.
  for (std::uint32_t v : p.present)
    if (state[v] == 0 && !fail(p.nodes[v].value + " must be present")) return std::nullopt;
  for (std::uint32_t v : p.absent)
    if (state[v] != 0 && !fail(p.nodes[v].value + " must be absent")) return std::nullopt;
  for (std::uint32_t v : p.active)
    if (state[v] == 1 && !fail(p.nodes[v].value + " must be active")) return std::nullopt;
  for (std::uint32_t v : p.inactive)
    if (state[v] == 2 && !fail(p.nodes[v].value + " must be inactive")) return std::nullopt;
  for (std::uint32_t v : p.consistent) {
    const bool up = p.regulation[v] == fusion::expression::Regulation::up;
    if ((up ? state[v] != 2 : state[v] == 2) && !fail(p.nodes[v].value + " must stay consistent"))
      return std::nullopt;
  }
  const auto relaxed = [&](std::uint32_t j) {
    return spec_relaxed(p.edges[j].effect, state[p.edges[j].src], state[p.edges[j].tgt]);
  };
  for (std::uint32_t j : p.edge_selected)
    if (!selected[j] && !fail("essential edge " + std::to_string(j) + " not selected")) return std::nullopt;
  for (std::uint32_t j : p.edge_avoided)
    if (selected[j] && !fail("avoided edge " + std::to_string(j) + " selected")) return std::nullopt;
  for (std::uint32_t j : p.edge_nonrelaxed)
    if (relaxed(j) && !fail("non-relaxed edge " + std::to_string(j) + " relaxed")) return std::nullopt;
  for (std::uint32_t j : p.frozen)
    if (relaxed(j) && !fail("frozen edge " + std::to_string(j) + " relaxed")) return std::nullopt;
  for (const auto& [u, v] : p.coexpressed)
    if (state[u] != 0 && state[v] != 0 && state[u] != state[v] &&
        !fail("coexpressed " + p.nodes[u].value + " and " + p.nodes[v].value + " differ"))
      return std::nullopt;
  if (!ok) return std::nullopt;
  // S5.12, S5.13: the counts.
  Point cost;
  for (std::uint32_t v : p.counted) {
    const bool up = p.regulation[v] == fusion::expression::Regulation::up;
    cost.n += (up ? state[v] != 2 : state[v] == 2) ? 1 : 0;
  }
  for (std::uint32_t j = 0; j < m; ++j) cost.e += relaxed(j) ? 1 : 0;
  return cost;
}

}  // namespace

Verdict check(const solve::Problem& p, const std::vector<int>& state, const std::vector<bool>& selected) {
  Verdict v;
  const auto cost = evaluate(p, state, selected, &v.violations);
  v.valid = cost.has_value();
  if (cost) v.cost = *cost;
  return v;
}

Verdict check(const solve::Problem& p, const solve::Witness& w) {
  std::vector<int> state(p.nodes.size(), 0);
  std::vector<bool> selected(p.edges.size(), false);
  Verdict extra;
  const auto node = [&](const NodeKey& key) -> std::optional<std::uint32_t> {
    for (std::uint32_t i = 0; i < p.nodes.size(); ++i)
      if (p.nodes[i] == key) return i;
    extra.violations.push_back("unknown node " + key.value);
    return std::nullopt;
  };
  for (const NodeKey& key : w.inactive)
    if (auto i = node(key)) state[*i] = 1;
  for (const NodeKey& key : w.active)
    if (auto i = node(key)) state[*i] = 2;
  for (const EdgeKey& key : w.selected) {
    bool found = false;
    for (std::uint32_t j = 0; j < p.edges.size(); ++j)
      if (p.edges[j].key == key) selected[j] = found = true;
    if (!found) extra.violations.push_back("unknown edge");
  }
  Verdict v = check(p, state, selected);
  v.violations.insert(v.violations.end(), extra.violations.begin(), extra.violations.end());
  if (!v.valid) return v;
  if (v.cost != w.counts) v.violations.push_back("the witness's counts differ from its relaxations");
  // The relaxation lists (S5.27): exactly the relaxed counted nodes, with presence, and the
  // relaxed edges, with selection.
  std::vector<std::pair<NodeKey, bool>> nodes;
  for (std::uint32_t i : p.counted) {
    const bool up = p.regulation[i] == fusion::expression::Regulation::up;
    if (up ? state[i] != 2 : state[i] == 2) nodes.emplace_back(p.nodes[i], state[i] != 0);
  }
  std::vector<std::pair<EdgeKey, bool>> edges;
  for (std::uint32_t j = 0; j < p.edges.size(); ++j)
    if (spec_relaxed(p.edges[j].effect, state[p.edges[j].src], state[p.edges[j].tgt]))
      edges.emplace_back(p.edges[j].key, selected[j]);
  if (nodes != w.relaxed_nodes) v.violations.push_back("relaxed nodes differ");
  if (edges != w.relaxed_edges) v.violations.push_back("relaxed edges differ");
  v.valid = v.violations.empty();
  return v;
}

std::vector<Point> exhaustive_frontier(const solve::Problem& p) {
  const std::size_t n = p.nodes.size(), m = p.edges.size();
  std::set<Point> costs;
  std::vector<int> state(n, 0);
  for (;;) {
    std::vector<std::uint32_t> usable;
    for (std::uint32_t j = 0; j < m; ++j)
      if (state[p.edges[j].src] != 0 && state[p.edges[j].tgt] != 0) usable.push_back(j);
    std::vector<bool> selected(m, false);
    for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << usable.size()); ++mask) {
      for (std::size_t k = 0; k < usable.size(); ++k) selected[usable[k]] = (mask >> k) & 1U;
      if (auto cost = evaluate(p, state, selected, nullptr)) costs.insert(*cost);
    }
    std::size_t i = 0;                                     // the next state, in base 3
    while (i < n && state[i] == 2) state[i++] = 0;
    if (i == n) break;
    ++state[i];
  }
  std::set<Point> clamped;
  for (Point c : costs) {
    Point q{std::max(c.n, p.box.lo.n), std::max(c.e, p.box.lo.e)};
    if (solve::below_or_equal(q, p.box.hi)) clamped.insert(q);
  }
  std::vector<Point> out;
  for (Point c : clamped)
    if (std::none_of(clamped.begin(), clamped.end(), [&](Point o) { return o != c && solve::below_or_equal(o, c); }))
      out.push_back(c);
  return out;
}

std::vector<Point> points_of(const solve::Frontier& f) {
  std::vector<Point> out;
  for (const solve::FrontierPoint& fp : f.points) out.push_back(fp.point);
  return out;
}

// ---------------------------------------------------------------- solving in-process

Solved solve_problem(const solve::Problem& p, const solve::Settings& settings) {
  solve::CaseSolver solver(p, settings);
  solve::AnswerCache cache(p.box);
  SolverOracle oracle(solver);
  Quiet quiet;
  Solved s;
  s.search = solve::pareto_search(p.box, cache, oracle, quiet);
  s.log.assign(cache.answers().begin(), cache.answers().end());
  s.witnesses.assign(solver.witnesses().begin(), solver.witnesses().end());
  s.frontier = solve::assemble(p.box, cache.answers(), solver.witnesses(), s.search);
  return s;
}

// ---------------------------------------------------------------- the scripted oracle

ScriptedOracle::ScriptedOracle(std::vector<Point> staircase, std::map<Point, solve::Outcome> overrides)
    : staircase_(std::move(staircase)), overrides_(std::move(overrides)) {}

bool ScriptedOracle::truth(Point p) const {
  return std::any_of(staircase_.begin(), staircase_.end(), [&](Point s) { return solve::below_or_equal(s, p); });
}

int ScriptedOracle::calls(Point p) const {
  const auto it = calls_.find(p);
  return it == calls_.end() ? 0 : it->second;
}

solve::Answer ScriptedOracle::ask(Point p, Phase phase) {
  ++calls_[p];
  asked.emplace_back(p, phase);
  solve::Answer a{};
  a.point = p;
  a.phase = phase;
  a.source = solve::Source::z3;
  const auto it = overrides_.find(p);
  a.outcome = it != overrides_.end() ? it->second : truth(p) ? solve::Outcome::sat : solve::Outcome::unsat;
  if (a.outcome == solve::Outcome::timeout) a.reason = "timeout";
  if (a.outcome == solve::Outcome::undefined) a.reason = "unknown";
  if (a.outcome == solve::Outcome::sat) {
    a.model = model ? model(p).value_or(p) : p;
    solve::Witness w;
    w.counts = *a.model;
    witnesses.push_back(w);
    a.witness = static_cast<std::uint32_t>(witnesses.size() - 1);
  }
  return a;
}

// ---------------------------------------------------------------- FuSION 1's toy cases

std::filesystem::path tests_dir() { return std::filesystem::path(FUSION_TEST_SOURCE_DIR); }

std::vector<std::string> toy_names() {
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::directory_iterator(tests_dir() / "reference" / "synthetic"))
    if (entry.is_directory()) out.push_back(entry.path().filename().string());
  std::sort(out.begin(), out.end());
  return out;
}

Limits read_limits(const std::filesystem::path& path) {
  Limits limits;
  bool have_box = false;
  Phase phase = Phase::diagonal;
  std::optional<Limits::Query> pending;
  for (const std::string& line : lines_of(slurp(path))) {
    if (starts_with(line, "new call")) {
      phase = Phase::diagonal;
      if (!have_box) {                                     // [nl, nu] and [el, eu]
        const auto v = integers(line);
        limits.box = {{v[0], v[2]}, {v[1], v[3]}};
        have_box = true;
      }
    } else if (line == "Finding smallest n") {
      phase = Phase::smallest_n;
    } else if (line == "Finding smallest e") {
      phase = Phase::smallest_e;
    } else if (starts_with(line, "Relax bounds")) {        // e before n (S5.24)
      const auto v = integers(line);
      const Phase p = line.find("node call") != std::string::npos   ? Phase::smallest_n
                      : line.find("edge call") != std::string::npos ? Phase::smallest_e
                                                                    : Phase::diagonal;
      pending = Limits::Query{{v[1], v[0]}, p, true, false};
    } else if (starts_with(line, "Point ") && line.find("already") != std::string::npos) {
      const auto v = integers(line);
      pending = Limits::Query{{v[1], v[0]}, phase, false, false};
    } else if (starts_with(line, "Point 1:") || starts_with(line, "Point 2:")) {
      const auto v = integers(line.substr(8));
      limits.found.push_back({v[1], v[0]});
    } else if ((line == "sat" || line == "unsat" || line == "unknown") && pending) {
      pending->sat = line == "sat";
      limits.queries.push_back(*pending);
      pending.reset();
    }
  }
  return limits;
}

std::vector<Point> read_dat(const std::filesystem::path& path) {
  std::vector<Point> out;
  for (const std::string& line : lines_of(slurp(path))) {
    const auto v = integers(line);
    if (v.size() >= 2) out.push_back({v[0], v[1]});
  }
  std::sort(out.begin(), out.end());
  return out;
}

ToyCase read_toy(const std::string& name) {
  ToyCase toy;
  toy.name = name;
  const auto dir = tests_dir() / "reference" / "synthetic" / name;
  const auto run = dir / "f1" / "run";
  std::map<std::string, std::string> lets;                 // the batch's let lines
  for (const std::string& line : lines_of(slurp(tests_dir() / "fixtures" / "toy-batches" / (name + ".batch")))) {
    const auto w = words(line);
    if (w.size() >= 3 && w[0] == "let") lets[w[1]] = w[2];
  }
  toy.fsc = lets.at("$NODE_TO_TEST_FOR_SIGNIFICANCE");
  toy.box = {{std::stoi(lets.at("$NODE_RELAX_LB")), std::stoi(lets.at("$EDGE_RELAX_LB"))},
             {std::stoi(lets.at("$NODE_RELAX_UB")), std::stoi(lets.at("$EDGE_RELAX_UB"))}};

  // FuSION 1's corridor as captured (KGML-like: entries, then relations with their subtypes).
  Graph graph(static_cast<std::uint32_t>(std::stoul(lets.at("$REACH_PATH_BOUND"))));
  std::map<std::string, std::string> entry_names;
  const std::string xml = slurp(dir / "f1" / "capture" / "corridor.xml");
  std::size_t at = 0;
  while ((at = xml.find('<', at)) != std::string::npos) {
    const std::size_t end = xml.find('>', at);
    const std::string tag = xml.substr(at, end - at + 1);
    if (starts_with(tag, "<entry ")) {
      entry_names[attribute(tag, "id")] = attribute(tag, "name");
      graph.node(attribute(tag, "name"));
    } else if (starts_with(tag, "<relation ")) {
      const std::size_t close = xml.find("</relation>", end);
      const std::string body = xml.substr(end, close - end);
      std::string subtypes;
      for (std::size_t s = 0; (s = body.find("<subtype ", s)) != std::string::npos; ++s)
        subtypes += (subtypes.empty() ? "" : ",") + attribute(body.substr(s, body.find('>', s) - s), "name");
      const std::string score = attribute(tag, "experiment_score");
      graph.edge(entry_names.at(attribute(tag, "entry1")), entry_names.at(attribute(tag, "entry2")), subtypes,
                 score.empty() ? 0.0 : std::stod(score), attribute(tag, "type"));
    }
    at = end;
  }
  graph.ends(lets.at("$SRC_NODE"), lets.at("$TGT_NODE"));
  toy.graph = graph.g;

  for (const std::string& gene : words(slurp(run / "up_reg"))) observe(toy.observations, toy.graph, gene, 'u');
  for (const std::string& gene : words(slurp(run / "down_reg"))) observe(toy.observations, toy.graph, gene, 'd');

  // FuSION 1 resolved list ids in the corridor and dropped the rest (GraphManagerNew.cc:18688-18708).
  const auto list = [&](const std::string& file) {
    std::vector<solve::CaseSpec::Id> out;
    for (const std::string& id : words(slurp(run / file)))
      if (std::any_of(toy.graph.nodes.begin(), toy.graph.nodes.end(),
                      [&](const solve::CaseGraph::Node& n) { return n.key.value == id; }))
        out.push_back({id, {id}});
    return out;
  };
  const auto find = [&](const std::string& tag, const std::string& suffix) {
    for (const auto& entry : std::filesystem::directory_iterator(run)) {
      const std::string file = entry.path().filename().string();
      if (file.find(tag + toy.fsc + "_") != std::string::npos && file.size() >= suffix.size() &&
          file.compare(file.size() - suffix.size(), suffix.size(), suffix) == 0 && file.find("_limits") == std::string::npos)
        return entry.path();
    }
    throw std::runtime_error("no " + tag + suffix + " in " + run.string());
  };
  const auto limits_of = [&](const std::string& tag) {
    for (const auto& entry : std::filesystem::directory_iterator(run)) {
      const std::string file = entry.path().filename().string();
      if (file.find(tag + toy.fsc + "_") != std::string::npos && file.find("_limits_timefile.txt") != std::string::npos)
        return read_limits(entry.path());
    }
    throw std::runtime_error("no limits file in " + run.string());
  };
  toy.with.spec = toy.observations;
  toy.with.spec.essential = list("essential_nodes_file_w_" + toy.fsc);
  toy.with.spec.active = list("active_nodes_file_w_" + toy.fsc);
  toy.with.spec.non_relaxed = list("nonrelaxed_nodes_file_w_" + toy.fsc);
  toy.with.dat = read_dat(find("_w_", ".dat"));
  toy.with.limits = limits_of("_w_");
  toy.without.spec = toy.observations;
  toy.without.spec.essential = list("essential_nodes_file_wo_" + toy.fsc);
  toy.without.spec.active = list("active_nodes_file_wo_" + toy.fsc);
  toy.without.spec.non_relaxed = list("nonrelaxed_nodes_file_wo_" + toy.fsc);
  toy.without.spec.avoid = list("avoid_" + toy.fsc + "_file");
  toy.without.dat = read_dat(find("_wo_", ".dat"));
  toy.without.limits = limits_of("_wo_");
  return toy;
}

}  // namespace solvekit
