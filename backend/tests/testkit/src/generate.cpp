// Seeded generators (generate.hpp), on core::Rng only (12.C9, 09.C15).
#include "fusion/testkit/generate.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <set>

#include "fusion/testkit/oracles.hpp"
#include "util.hpp"

namespace fusion::testkit {

std::uint64_t test_seed(std::uint64_t fixed) {
  const char* value = std::getenv("FUSION_TEST_SEED");
  if (!value || !*value) return fixed;
  std::uint64_t seed = 0;
  const std::string_view text(value);
  const auto r = std::from_chars(text.data(), text.data() + text.size(), seed);
  if (r.ec != std::errc() || r.ptr != text.data() + text.size())
    throw core::Failure("testkit.seed", "FUSION_TEST_SEED is not a whole number: " + std::string(text));
  return seed;
}

namespace {

bool chance(core::Rng& rng, double p) { return rng.unit() < p; }
int below(core::Rng& rng, int n) { return static_cast<int>(rng.below(static_cast<std::uint64_t>(n))); }

std::string two_decimals(int hundredths) {   // locale-free "-1.05"
  const int a = std::abs(hundredths);
  std::string out = (hundredths < 0 ? "-" : "") + std::to_string(a / 100) + ".";
  out += static_cast<char>('0' + (a % 100) / 10);
  out += static_cast<char>('0' + a % 10);
  return out;
}

}  // namespace

// ---------------------------------------------------------------- pathways

std::vector<SyntheticPathway> pathways(core::Rng& rng, int count, const PathwayShape& shape) {
  static const std::vector<std::vector<std::string>> kDirected = {
      {"activation"}, {"inhibition"}, {"expression"}, {"repression"}, {"phosphorylation"},
      {"activation", "phosphorylation"}, {"inhibition", "dephosphorylation"}, {"state change"}, {}};
  static const std::vector<std::string> kScores = {"", "0", "5", "700", "850", "12.5"};
  const int pool = std::max(1, shape.entries);
  std::vector<SyntheticPathway> out;
  for (int p = 0; p < count; ++p) {
    std::string number = std::to_string(p + 1);
    number.insert(0, 5 - std::min<std::size_t>(5, number.size()), '0');
    SyntheticPathway pw;
    pw.id = "path:t" + number;
    std::string entries, relations;
    std::vector<int> ids_of_entries;            // entry ids, boxes and groups alike
    int next = 1;
    for (int b = 0; b < shape.entries; ++b) {
      std::string name, type = "gene";
      if (chance(rng, 0.08)) {
        name = "cpd:x" + std::to_string(1 + below(rng, std::max(2, pool / 5)));
        type = "compound";
      } else {
        std::set<int> ids;
        const int size = 1 + below(rng, std::max(1, shape.max_box));
        for (int k = 0; k < size; ++k)
          ids.insert(chance(rng, shape.shared_id) ? 1 + below(rng, pool) : pool * (p + 1) + 1 + below(rng, pool));
        for (int id : ids) name += (name.empty() ? "" : " ") + std::string("t:") + std::to_string(id);
      }
      entries += "  <entry id=\"" + std::to_string(next) + "\" name=\"" + name + "\" type=\"" + type +
                 "\">\n    <graphics name=\"x\"/>\n  </entry>\n";
      ids_of_entries.push_back(next++);
    }
    for (int g = 0; g < shape.groups && !ids_of_entries.empty(); ++g) {
      std::set<int> parts;
      const int size = 2 + below(rng, 2);
      for (int k = 0; k < size; ++k) parts.insert(ids_of_entries[static_cast<std::size_t>(below(rng, static_cast<int>(ids_of_entries.size())))]);
      entries += "  <entry id=\"" + std::to_string(next) + "\" name=\"undefined\" type=\"group\">\n";
      for (int c : parts) entries += "    <component id=\"" + std::to_string(c) + "\"/>\n";
      entries += "  </entry>\n";
      ids_of_entries.push_back(next++);           // later groups may hold this one
    }
    std::string last;
    for (int r = 0; r < shape.relations && !ids_of_entries.empty(); ++r) {
      if (!last.empty() && chance(rng, shape.duplicate)) {
        relations += last;                       // the same relation twice (S2.10's duplicate test)
        continue;
      }
      const auto pick = [&] { return ids_of_entries[static_cast<std::size_t>(below(rng, static_cast<int>(ids_of_entries.size())))]; };
      const int a = pick(), b = pick();
      std::string type = "PPrel";
      std::vector<std::string> subtypes;
      if (chance(rng, shape.ecrel)) {
        type = "ECrel";
        subtypes = {"compound"};
      } else if (chance(rng, shape.undirected)) {
        subtypes = {chance(rng, 0.7) ? "binding/association" : "dissociation"};
        if (chance(rng, 0.3)) subtypes.insert(subtypes.begin(), "activation");
      } else if (chance(rng, shape.indirect)) {
        subtypes = chance(rng, 0.5) ? std::vector<std::string>{"indirect effect"}
                                    : std::vector<std::string>{"activation", "indirect effect"};
      } else {
        subtypes = kDirected[static_cast<std::size_t>(below(rng, static_cast<int>(kDirected.size())))];
        if (chance(rng, 0.2)) type = chance(rng, 0.5) ? "GErel" : "PCrel";
      }
      const std::string& score = kScores[static_cast<std::size_t>(below(rng, static_cast<int>(kScores.size())))];
      std::string rel = "  <relation entry1=\"" + std::to_string(a) + "\" entry2=\"" + std::to_string(b) + "\" type=\"" +
                        type + "\"" + (score.empty() ? std::string() : " experiment_score=\"" + score + "\"") + ">\n";
      for (const std::string& s : subtypes) rel += "    <subtype name=\"" + s + "\" value=\"--&gt;\"/>\n";
      rel += "  </relation>\n";
      relations += rel;
      last = rel;
    }
    pw.kgml = "<?xml version=\"1.0\"?>\n<pathway name=\"" + pw.id + "\" org=\"t\" number=\"" + number +
              "\" title=\"Synthetic pathway " + std::to_string(p + 1) + "\">\n" + entries + relations + "</pathway>\n";
    out.push_back(std::move(pw));
  }
  return out;
}

// ---------------------------------------------------------------- value files

std::string value_file(core::Rng& rng, std::span<const std::string> ids, double tie_share, bool crlf) {
  std::string out;
  std::vector<int> drawn;
  for (const std::string& id : ids) {
    int v = 0;
    if (!drawn.empty() && chance(rng, tie_share)) v = drawn[static_cast<std::size_t>(below(rng, static_cast<int>(drawn.size())))];
    else v = below(rng, 601) - 300;
    drawn.push_back(v);
    out += id + "\t" + two_decimals(v) + (crlf ? "\r\n" : "\n");
  }
  return out;
}

// ---------------------------------------------------------------- tiny cases

namespace {

struct TinyEdge { int src, tgt; bool inhibition; int score; };

// The shortest distances over the edges, forward from S (node 0) or backward to T (node 1).
std::vector<int> distances(int n, const std::vector<TinyEdge>& edges, bool forward) {
  constexpr int far = std::numeric_limits<int>::max() / 4;
  std::vector<int> d(static_cast<std::size_t>(n), far);
  std::deque<int> queue{forward ? 0 : 1};
  d[static_cast<std::size_t>(queue.front())] = 0;
  while (!queue.empty()) {
    const int u = queue.front();
    queue.pop_front();
    for (const TinyEdge& e : edges) {
      const int from = forward ? e.src : e.tgt, to = forward ? e.tgt : e.src;
      if (from != u || d[static_cast<std::size_t>(to)] != far) continue;
      d[static_cast<std::size_t>(to)] = d[static_cast<std::size_t>(u)] + 1;
      queue.push_back(to);
    }
  }
  return d;
}

}  // namespace

TinyCase tiny_case(core::Rng& rng, int nodes, int edge_count, int bound) {
  if (nodes < 2 || bound < 1 || (nodes > 2 && bound < 2) || edge_count < 1)
    detail::fail("tiny_case: needs 2 nodes and 1 edge at least, and a bound of 2 for inner nodes");
  const int inner = nodes - 2;
  std::vector<std::string> keys{"S", "T"};
  for (int k = 1; k <= inner; ++k) keys.push_back("N" + std::to_string(k));
  std::vector<std::vector<std::string>> members;
  for (const std::string& k : keys) members.push_back({k});
  std::vector<bool> complex(static_cast<std::size_t>(nodes), false);
  if (inner >= 3 && below(rng, 3) == 0) {       // the last inner node is a complex of two other inner genes
    const int a = 2 + below(rng, inner - 1);
    int b = 2 + below(rng, inner - 2);
    if (b >= a) ++b;
    const auto last = static_cast<std::size_t>(nodes - 1);
    members[last] = {keys[static_cast<std::size_t>(a)], keys[static_cast<std::size_t>(b)]};
    keys[last] = complex_key(members[last]);
    complex[last] = true;
  }
  for (int attempt = 0; attempt < 1000; ++attempt) {
    // Levels place each inner node on a walk of at most `bound` edges: ds <= level, dt <= bound - level.
    std::vector<int> level(static_cast<std::size_t>(nodes), 0);
    level[1] = bound;
    for (int v = 2; v < nodes; ++v) level[static_cast<std::size_t>(v)] = 1 + below(rng, bound - 1);
    std::vector<TinyEdge> edges;
    std::set<std::tuple<int, int, bool>> used;
    const auto add = [&](int u, int v) {
      const bool inhibition = below(rng, 100) < 35;
      if (!used.insert({u, v, inhibition}).second) return false;
      edges.push_back({u, v, inhibition, below(rng, 1000)});
      return true;
    };
    const auto lower = [&](int v, bool before) {       // a node strictly before (or after) v's level
      std::vector<int> options;
      for (int u = 0; u < nodes; ++u) {
        const int lu = level[static_cast<std::size_t>(u)], lv = level[static_cast<std::size_t>(v)];
        if (u != v && (before ? lu < lv : lu > lv)) options.push_back(u);
      }
      return options[static_cast<std::size_t>(below(rng, static_cast<int>(options.size())))];
    };
    std::vector<int> order;
    for (int v = 2; v < nodes; ++v) order.push_back(v);
    rng.shuffle(order.begin(), order.end());
    std::vector<int> ins(static_cast<std::size_t>(nodes), 0), outs(static_cast<std::size_t>(nodes), 0);
    for (int v : order) {
      if (ins[static_cast<std::size_t>(v)] == 0) {
        const int u = lower(v, true);
        if (add(u, v)) ++outs[static_cast<std::size_t>(u)], ++ins[static_cast<std::size_t>(v)];
      }
      if (outs[static_cast<std::size_t>(v)] == 0) {
        const int w = lower(v, false);
        if (add(v, w)) ++outs[static_cast<std::size_t>(v)], ++ins[static_cast<std::size_t>(w)];
      }
    }
    if (inner == 0) add(0, 1);
    if (static_cast<int>(edges.size()) > edge_count) continue;
    for (int tries = 0; static_cast<int>(edges.size()) < edge_count && tries < 500; ++tries) {
      const int u = below(rng, nodes), v = below(rng, nodes);
      if (u == v || u == 1 || v == 0) continue;     // no self-loop, edge out of T or edge into S (05.C2)
      const auto ds = distances(nodes, edges, true), dt = distances(nodes, edges, false);
      if (ds[static_cast<std::size_t>(u)] + 1 + dt[static_cast<std::size_t>(v)] > bound) continue;
      add(u, v);
    }
    if (static_cast<int>(edges.size()) != edge_count) continue;

    TinyCase tc;
    for (int v = 0; v < nodes; ++v)
      tc.corridor.graph.nodes.push_back({keys[static_cast<std::size_t>(v)], members[static_cast<std::size_t>(v)],
                                         complex[static_cast<std::size_t>(v)] ? "group" : "gene"});
    for (const TinyEdge& e : edges) {
      CanonEdge c;
      c.key = {keys[static_cast<std::size_t>(e.src)], keys[static_cast<std::size_t>(e.tgt)], "PPrel",
               {e.inhibition ? "inhibition" : "activation"}};
      c.pathways = {"path:t00001"};
      c.score = e.score;
      tc.corridor.graph.edges.push_back(std::move(c));
    }
    normalise(tc.corridor.graph);
    tc.corridor.sources = {"S"};
    tc.corridor.targets = {"T"};
    tc.corridor.bound = bound;
    const std::vector<std::string> s{"S"}, t{"T"};
    if (walk_corridor(tc.corridor.graph, s, t, bound).graph != tc.corridor.graph) continue;   // a choice repeated

    // The case: what the solve suite's random cases drew before C2, now on core::Rng.
    CaseSpec& spec = tc.spec;
    for (int v = 0; v < nodes; ++v) {
      if (complex[static_cast<std::size_t>(v)]) continue;
      const int r = below(rng, 100);
      if (r < 35) spec.up.push_back(keys[static_cast<std::size_t>(v)]);
      else if (r < 55) spec.down.push_back(keys[static_cast<std::size_t>(v)]);
    }
    const auto pick = [&] { return inner > 0 ? keys[static_cast<std::size_t>(2 + below(rng, inner))] : std::string("S"); };
    const auto pick_edge = [&]() -> EdgeSpec {
      const CanonEdge& e = tc.corridor.graph.edges[static_cast<std::size_t>(below(rng, edge_count))];
      return {e.key.src, e.key.tgt, e.key.subtypes};
    };
    if (below(rng, 100) < 25) spec.essential = {pick()};
    if (below(rng, 100) < 25) spec.avoid = {pick()};
    if (below(rng, 100) < 20) spec.active = {pick()};
    if (below(rng, 100) < 20) spec.inactive = {pick()};
    if (below(rng, 100) < 25) spec.nonrelaxed = {pick()};
    if (below(rng, 100) < 25) spec.avoid_edges = {pick_edge()};
    if (below(rng, 100) < 20) spec.nonrelaxed_edges = {pick_edge()};
    if (below(rng, 100) < 10) spec.essential_edges = {pick_edge()};
    if (below(rng, 100) < 30) spec.coexpressed = {{pick(), pick()}};
    if (below(rng, 100) < 30) spec.frozen_threshold = 500;
    spec.named = below(rng, 100) >= 30;
    if (below(rng, 100) < 40) {
      spec.fsc = {pick()};
      spec.solve_case = below(rng, 2) == 0 ? CaseSpec::Case::in : CaseSpec::Case::out;
    }
    return tc;
  }
  detail::fail("tiny_case: no corridor of " + std::to_string(nodes) + " nodes and " + std::to_string(edge_count) +
               " edges within bound " + std::to_string(bound) + " after 1000 attempts");
}

}  // namespace fusion::testkit
