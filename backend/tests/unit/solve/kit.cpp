// The solve tests' helpers (kit.hpp).
#include "kit.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
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

std::vector<Point> points(const std::vector<testkit::Point>& from) {
  std::vector<Point> out;
  for (const testkit::Point& p : from) out.push_back({p.n, p.e});
  return out;
}

std::string text(Point p) { return "(" + std::to_string(p.n) + "," + std::to_string(p.e) + ")"; }

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

std::vector<Point> points_of(const solve::Frontier& f) {
  std::vector<Point> out;
  for (const solve::FrontierPoint& fp : f.points) out.push_back(fp.point);
  return out;
}

// ---------------------------------------------------------------- the kit's judgement

testkit::Verdict kit_check(const CaseRef& c, const solve::Witness& w) {
  const testkit::CanonCorridor corridor = testkit::adapt::canon(c.graph);
  const testkit::CaseSpec spec = testkit::adapt::spec(c.graph, c.spec, c.settings, c.kase);
  const testkit::Assignment a = testkit::adapt::assignment(w);
  testkit::Verdict v = testkit::check(corridor, spec, a, testkit::Semantics::fusion2);
  if (v.valid && (v.cost.n != w.counts.n || v.cost.e != w.counts.e)) {
    v.valid = false;
    v.violations.push_back("the witness's counts " + text(w.counts) + " differ from the checker's cost (" +
                           std::to_string(v.cost.n) + "," + std::to_string(v.cost.e) + ")");
  }
  // S5.27: the relaxation lists, each entry relaxed by the spec's table and named once, as many as counted.
  std::map<std::string, int> state;                        // 1 inactive, 2 active
  for (const auto& [key, active] : a.present) state[key] = active ? 2 : 1;
  const auto state_of = [&](const std::string& key) {
    const auto it = state.find(key);
    return it == state.end() ? 0 : it->second;
  };
  std::set<std::string> up, down;
  for (const auto& o : c.spec.observations)
    if (o.node) (o.regulation == fusion::expression::Regulation::down ? down : up).insert(o.node->value);
  for (const auto& id : c.spec.confirmed_up) up.insert(id.node.value), down.erase(id.node.value);
  for (const auto& id : c.spec.confirmed_down) down.insert(id.node.value), up.erase(id.node.value);
  std::set<std::string> nodes_named;
  for (const auto& [key, present] : w.relaxed_nodes) {
    const int s = state_of(key.value);
    const bool relaxed = up.contains(key.value) ? s != 2 : down.contains(key.value) && s == 2;
    if (!relaxed || present != (s != 0) || !nodes_named.insert(key.value).second)
      v.violations.push_back("the relaxed node " + key.value + " is not relaxed as listed (S5.27)");
  }
  std::set<EdgeKey> edges_named;
  for (const auto& [key, selected] : w.relaxed_edges) {
    const bool is_selected = std::find(w.selected.begin(), w.selected.end(), key) != w.selected.end();
    const auto e = std::find_if(c.graph.edges.begin(), c.graph.edges.end(),
                                [&](const solve::CaseGraph::Edge& x) { return x.key == key; });
    const bool relaxed =
        e != c.graph.edges.end() && spec_relaxed(e->effect, state_of(key.source.value), state_of(key.target.value));
    if (!relaxed || selected != is_selected || !edges_named.insert(key).second)
      v.violations.push_back("the relaxed edge " + key.text() + " is not relaxed as listed (S5.27)");
  }
  if (static_cast<int>(w.relaxed_nodes.size()) != w.counts.n || static_cast<int>(w.relaxed_edges.size()) != w.counts.e)
    v.violations.push_back("the relaxation lists do not hold " + text(w.counts) + " entries (S5.27)");
  v.valid = v.valid && v.violations.empty();
  return v;
}

std::vector<Point> kit_frontier(const CaseRef& c) {
  const testkit::Frontier f = testkit::exhaustive_frontier(
      testkit::adapt::canon(c.graph), testkit::adapt::spec(c.graph, c.spec, c.settings, c.kase), testkit::Semantics::fusion2,
      {c.settings.box.lo.n, c.settings.box.lo.e}, {c.settings.box.hi.n, c.settings.box.hi.e});
  return points(f.points);
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

// ---------------------------------------------------------------- FuSION 1's toy cases

solve::Box ToyRun::box() const {
  return {{limits.box_lo.n, limits.box_lo.e}, {limits.box_hi.n, limits.box_hi.e}};
}

std::filesystem::path tests_dir() { return std::filesystem::path(FUSION_TEST_SOURCE_DIR); }

std::vector<std::string> toy_names() {
  std::vector<std::string> out;
  for (const auto& entry : std::filesystem::directory_iterator(tests_dir() / "reference" / "synthetic"))
    if (entry.is_directory()) out.push_back(entry.path().filename().string());
  std::sort(out.begin(), out.end());
  return out;
}

ToyCase read_toy(const std::string& name) {
  namespace f1 = testkit::f1;
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

  // FuSION 1's corridor as captured, read by the kit's reader of FuSION 1's written graphs.
  Graph graph(static_cast<std::uint32_t>(std::stoul(lets.at("$REACH_PATH_BOUND"))));
  const testkit::CanonGraph corridor = f1::read_written_graph(dir / "f1" / "capture" / "corridor.xml");
  for (const testkit::CanonNode& n : corridor.nodes) graph.node(n.key);
  for (const testkit::CanonEdge& e : corridor.edges) {
    std::string subtypes;
    for (const std::string& s : e.key.subtypes) subtypes += (subtypes.empty() ? "" : ",") + s;
    graph.edge(e.key.src, e.key.tgt, subtypes, e.score, e.key.type);
  }
  graph.ends(lets.at("$SRC_NODE"), lets.at("$TGT_NODE"));
  toy.graph = graph.g;

  for (const std::string& gene : f1::read_ids(run / "up_reg")) observe(toy.observations, toy.graph, gene, 'u');
  for (const std::string& gene : f1::read_ids(run / "down_reg")) observe(toy.observations, toy.graph, gene, 'd');

  // FuSION 1 resolved list ids in the corridor and dropped the rest (GraphManagerNew.cc:18688-18708).
  const auto list = [&](const std::string& file) {
    std::vector<solve::CaseSpec::Id> out;
    for (const std::string& id : f1::read_ids(run / file))
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
        return f1::read_limits(entry.path());
    }
    throw std::runtime_error("no limits file in " + run.string());
  };
  toy.with.spec = toy.observations;
  toy.with.spec.essential = list("essential_nodes_file_w_" + toy.fsc);
  toy.with.spec.active = list("active_nodes_file_w_" + toy.fsc);
  toy.with.spec.non_relaxed = list("nonrelaxed_nodes_file_w_" + toy.fsc);
  toy.with.dat = points(f1::read_dat(find("_w_", ".dat")));
  toy.with.limits = limits_of("_w_");
  toy.without.spec = toy.observations;
  toy.without.spec.essential = list("essential_nodes_file_wo_" + toy.fsc);
  toy.without.spec.active = list("active_nodes_file_wo_" + toy.fsc);
  toy.without.spec.non_relaxed = list("nonrelaxed_nodes_file_wo_" + toy.fsc);
  toy.without.spec.avoid = list("avoid_" + toy.fsc + "_file");
  toy.without.dat = points(f1::read_dat(find("_wo_", ".dat")));
  toy.without.limits = limits_of("_wo_");
  return toy;
}

// ---------------------------------------------------------------- generated tiny cases

TinyInputs inputs_of(const testkit::TinyCase& tc) {
  TinyInputs in;
  Graph g(static_cast<std::uint32_t>(tc.corridor.bound));
  std::map<std::string, std::string> key_of;              // canonical key -> the case graph's key
  for (const testkit::CanonNode& n : tc.corridor.graph.nodes) {
    const std::string key = testkit::module_key(n);
    key_of[n.key] = key;
    g.node(key, n.type == "group" ? n.members : std::vector<std::string>{});
  }
  for (const testkit::CanonEdge& e : tc.corridor.graph.edges) {
    std::string subtypes;
    for (const std::string& s : e.key.subtypes) subtypes += (subtypes.empty() ? "" : ",") + s;
    g.edge(key_of.at(e.key.src), key_of.at(e.key.tgt), subtypes, e.score, e.key.type);
  }
  g.ends(key_of.at(tc.corridor.sources.at(0)), key_of.at(tc.corridor.targets.at(0)));
  in.graph = g.g;
  const auto id_list = [&](const std::vector<std::string>& keys) {
    std::vector<solve::CaseSpec::Id> out;
    for (const std::string& k : keys) out.push_back({key_of.at(k), {key_of.at(k)}});
    return out;
  };
  for (const std::string& k : tc.spec.up) observe(in.spec, in.graph, key_of.at(k), 'u');
  for (const std::string& k : tc.spec.down) observe(in.spec, in.graph, key_of.at(k), 'd');
  in.spec.essential = id_list(tc.spec.essential);
  in.spec.avoid = id_list(tc.spec.avoid);
  in.spec.active = id_list(tc.spec.active);
  in.spec.inactive = id_list(tc.spec.inactive);
  in.spec.non_relaxed = id_list(tc.spec.nonrelaxed);
  in.spec.fsc = id_list(tc.spec.fsc);
  const auto edge_list = [&](const std::vector<testkit::EdgeSpec>& lines) {
    std::vector<std::uint32_t> out;
    for (const testkit::EdgeSpec& line : lines)
      for (std::uint32_t j : solve::match_edge_line(in.graph, NodeKey{key_of.at(line.src)}, NodeKey{key_of.at(line.tgt)},
                                                     line.subtypes))
        out.push_back(j);
    return out;
  };
  in.spec.essential_edges = edge_list(tc.spec.essential_edges);
  in.spec.avoid_edges = edge_list(tc.spec.avoid_edges);
  in.spec.nonrelaxed_edges = edge_list(tc.spec.nonrelaxed_edges);
  if (!tc.spec.coexpressed.empty()) {                      // a table row scoring above the threshold
    in.spec.coexpression_table = true;
    for (const auto& [a, b] : tc.spec.coexpressed)
      in.spec.coexpression.push_back({NodeKey{key_of.at(a)}, NodeKey{key_of.at(b)}, 900});
    in.settings.families.coexpression_threshold = 500;
  }
  in.settings.families.frozen_threshold = tc.spec.frozen_threshold;
  in.settings.positional = !tc.spec.named;
  in.kase = tc.spec.solve_case == testkit::CaseSpec::Case::in    ? solve::Case::in
            : tc.spec.solve_case == testkit::CaseSpec::Case::out ? solve::Case::out
                                                                 : solve::Case::free;
  return in;
}

std::string describe(const testkit::TinyCase& tc) {
  std::ostringstream o;
  o << tc.corridor.graph.text() << "sources " << tc.corridor.sources.at(0) << " targets " << tc.corridor.targets.at(0)
    << " bound " << tc.corridor.bound << "\n";
  const auto list = [&](const char* name, const std::vector<std::string>& v) {
    o << name;
    for (const auto& s : v) o << " " << s;
    o << "\n";
  };
  const auto& s = tc.spec;
  list("up", s.up);
  list("down", s.down);
  list("essential", s.essential);
  list("avoid", s.avoid);
  list("active", s.active);
  list("inactive", s.inactive);
  list("nonrelaxed", s.nonrelaxed);
  list("fsc", s.fsc);
  for (const auto* edges : {&s.essential_edges, &s.avoid_edges, &s.nonrelaxed_edges})
    for (const auto& e : *edges) o << (edges == &s.essential_edges ? "essential_edge " : edges == &s.avoid_edges ? "avoid_edge " : "nonrelaxed_edge ")
                                   << e.src << " " << e.tgt << "\n";
  for (const auto& [a, b] : s.coexpressed) o << "coexpressed " << a << " " << b << "\n";
  if (s.frozen_threshold) o << "frozen > " << *s.frozen_threshold << "\n";
  o << "case " << (s.solve_case == testkit::CaseSpec::Case::in ? "in" : s.solve_case == testkit::CaseSpec::Case::out ? "out" : "free")
    << (s.named ? " named" : " positional") << "\n";
  return o.str();
}

}  // namespace solvekit
