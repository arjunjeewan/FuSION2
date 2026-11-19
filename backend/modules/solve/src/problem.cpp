// Building a case (06, "Building a case", steps 2 to 10): the encoded sets, the lists, the
// observations and their roles, the FSC, the endpoint defaults, the clamps, the pre-checks and the
// constraint families, from the corridor and the lists already bound to node keys. Nothing here
// needs the network or Z3, so tests build cases directly.
#include "fusion/solve/solve.hpp"

#include <algorithm>
#include <charconv>
#include <deque>
#include <map>
#include <set>
#include <string>

namespace fusion::solve {

namespace {

using expression::MatchGroup;
using expression::Regulation;
using KeySet = std::set<std::string, std::less<>>;

constexpr int encoder_version = 1;                         // in every fingerprint (06, step 10)

KeySet keys_of(const std::vector<CaseSpec::Id>& ids) {
  KeySet out;
  for (const CaseSpec::Id& id : ids) out.insert(id.node.value);
  return out;
}

std::string given_of(const std::vector<CaseSpec::Id>& ids, const std::string& key) {
  for (const CaseSpec::Id& id : ids)
    if (id.node.value == key) return id.given == key ? key : id.given + " (" + key + ")";
  return key;
}

std::string number(double value) {                         // shortest round trip, locale-free
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
  return std::string(buffer, result.ptr);
}

// The requirements on one node, each with what imposed it, for S5.05's contradiction check.
struct Requirement {
  std::string present, absent, active, inactive;
};

[[noreturn]] void contradiction(const std::string& key, const std::string& a, const std::string& b) {
  throw core::Failure("solve.contradictory_lists", key + " is both " + a + " and " + b + " (S5.05)");
}

}  // namespace

// S5.06's fix: the subtype field is compared as a set, so a user need not know the stored order.
// FuSION 1 compared "<src>__<tgt>__<subtypes joined by _>" with every edge's label in stored order
// (Linux/src/GraphManagerNew.cc:18739-18819). A line with no relation type can match edges of two
// types between the same nodes; it then matches both (06, "Building a case", step 3).
std::vector<std::uint32_t> match_edge_line(const CaseGraph& graph, const network::NodeKey& source,
                                           const network::NodeKey& target,
                                           std::span<const std::string> subtypes) {
  const KeySet wanted(subtypes.begin(), subtypes.end());
  std::vector<std::uint32_t> out;
  for (std::uint32_t j = 0; j < graph.edges.size(); ++j) {
    const CaseGraph::Edge& e = graph.edges[j];
    if (graph.nodes[e.src].key != source || graph.nodes[e.tgt].key != target) continue;
    if (KeySet(e.key.subtypes.begin(), e.key.subtypes.end()) == wanted) out.push_back(j);
  }
  return out;
}

std::variant<Problem, Skipped> make_case(const CaseGraph& graph, const CaseSpec& spec, const Settings& settings,
                                         Case kase) {
  Problem p;
  p.kase = kase;
  p.named = !settings.positional;
  p.bound = graph.bound;
  p.seed = settings.seed;
  p.relaxed_states = graph.relaxed_states;
  p.requested = settings.box;

  // Step 1 (05.C7, entry 171): a positional line's bound must be the corridor's.
  if (settings.path_bound && *settings.path_bound != graph.bound)
    throw core::Failure("solve.bound_mismatch", "bound " + std::to_string(*settings.path_bound) +
                                                    " differs from the corridor's " + std::to_string(graph.bound));
  if (kase != Case::free && spec.fsc.empty())
    throw core::UsageError("batch.bad_argument", "the cases in and out need an fsc (06.C10)");

  const std::uint32_t corridor_size = static_cast<std::uint32_t>(graph.nodes.size());
  std::map<std::string, std::uint32_t, std::less<>> corridor_index;   // key -> corridor index
  for (std::uint32_t v = 0; v < corridor_size; ++v) corridor_index.emplace(graph.nodes[v].key.value, v);
  const std::string& source_key = graph.nodes[graph.source].key.value;
  const std::string& target_key = graph.nodes[graph.target].key.value;

  const KeySet fsc = keys_of(spec.fsc);
  const bool fsc_in = kase == Case::in, fsc_out = kase == Case::out;
  const KeySet essential = keys_of(spec.essential), avoid = keys_of(spec.avoid);
  const KeySet active_list = keys_of(spec.active), inactive_list = keys_of(spec.inactive);
  const KeySet non_relaxed = keys_of(spec.non_relaxed);
  const KeySet confirmed_up = keys_of(spec.confirmed_up), confirmed_down = keys_of(spec.confirmed_down);

  // ---- S5.05, S3.07: contradictory lists are failures before solving.
  for (const std::string& key : confirmed_up)
    if (confirmed_down.contains(key))
      contradiction(given_of(spec.confirmed_up, key), "confirmed up", "confirmed down");
  for (const std::string& key : fsc)
    if (avoid.contains(key)) contradiction(given_of(spec.fsc, key), "the FSC", "on an avoid list");

  // ---- Step 2 (S5.10): one breadth-first search from the source over corridor out-edges, never
  // expanding the target. FuSION 1 walked (node, target) pairs from the connect pairs, adding each
  // out-edge of a node it expanded (GraphManagerNew.cc:27812-28056); essential nodes joined the
  // node set whether reached or not (28330-28347).
  std::vector<std::vector<std::uint32_t>> out_edges(corridor_size);
  for (std::uint32_t j = 0; j < graph.edges.size(); ++j) out_edges[graph.edges[j].src].push_back(j);
  std::vector<bool> reached(corridor_size, false), traversed(graph.edges.size(), false);
  std::deque<std::uint32_t> queue{graph.source};
  reached[graph.source] = true;
  while (!queue.empty()) {
    const std::uint32_t u = queue.front();
    queue.pop_front();
    if (u == graph.target) continue;
    for (std::uint32_t j : out_edges[u]) {
      traversed[j] = true;
      const std::uint32_t v = graph.edges[j].tgt;
      if (!reached[v]) {
        reached[v] = true;
        queue.push_back(v);
      }
    }
  }

  // The nodes that must be present: the essential list, the in case's FSC, and in the named form the
  // endpoints (06.C16). They join N if they are in the corridor.
  std::map<std::string, std::string, std::less<>> must_be_present;   // key -> why
  for (const std::string& key : essential) must_be_present.emplace(key, "essential");
  if (fsc_in)
    for (const std::string& key : fsc) must_be_present.emplace(key, "the FSC of the in case");
  if (p.named) {
    must_be_present.emplace(source_key, "the source");
    must_be_present.emplace(target_key, "the target");
  }

  // The source and the target are always in N, as both ends of FuSION 1's connect pair were
  // (GraphManagerNew.cc:27867-27868), even when no path reaches the target.
  std::vector<std::int64_t> problem_index(corridor_size, -1);
  for (std::uint32_t v = 0; v < corridor_size; ++v) {
    if (!reached[v] && v != graph.target && !must_be_present.contains(graph.nodes[v].key.value)) continue;
    problem_index[v] = static_cast<std::int64_t>(p.nodes.size());
    p.nodes.push_back(graph.nodes[v].key);
    p.labels.push_back(graph.nodes[v].label.empty() ? graph.nodes[v].key.value : graph.nodes[v].label);
  }
  p.source = static_cast<std::uint32_t>(problem_index[graph.source]);
  p.target = static_cast<std::uint32_t>(problem_index[graph.target]);
  p.outside_encoding = corridor_size - static_cast<std::uint32_t>(p.nodes.size());

  // Self-loops leave E altogether (S5.13; 06, "Encoding"): the distance rule forbids selecting one,
  // and S5.13 leaves them out of the count.
  std::vector<std::int64_t> edge_index(graph.edges.size(), -1);
  for (std::uint32_t j = 0; j < graph.edges.size(); ++j) {
    const CaseGraph::Edge& e = graph.edges[j];
    if (!traversed[j] || e.src == e.tgt) continue;
    edge_index[j] = static_cast<std::int64_t>(p.edges.size());
    p.edges.push_back({static_cast<std::uint32_t>(problem_index[e.src]), static_cast<std::uint32_t>(problem_index[e.tgt]),
                       e.key, e.effect, e.score, e.unscored});
  }
  const auto in_n = [&](const std::string& key) -> std::optional<std::uint32_t> {
    auto it = corridor_index.find(key);
    if (it == corridor_index.end() || problem_index[it->second] < 0) return std::nullopt;
    return static_cast<std::uint32_t>(problem_index[it->second]);
  };

  // ---- Step 4 (S5.07, S5.17): the observations at the node level, inside the corridor.
  // FuSION 1: up_use = ((up - avoid) - confirmed_down) + confirmed_up and
  // down_use = (down - confirmed_up) + confirmed_down (main.cc:4667-4679).
  KeySet up, down;
  for (const CaseSpec::Observation& o : spec.observations) {
    if (!o.node || !corridor_index.contains(o.node->value)) continue;
    (o.regulation == Regulation::up ? up : down).insert(o.node->value);
  }
  const auto in_corridor = [&](const std::string& key) { return corridor_index.contains(key); };
  for (const std::string& key : avoid) up.erase(key);
  for (const std::string& key : confirmed_down) up.erase(key);
  for (const std::string& key : confirmed_up) if (in_corridor(key)) up.insert(key);
  for (const std::string& key : confirmed_up) down.erase(key);
  for (const std::string& key : confirmed_down) if (in_corridor(key)) down.insert(key);
  if (fsc_out)
    for (const std::string& key : fsc) { up.erase(key); down.erase(key); }   // S5.17: out removes it
  for (const std::string& key : up) down.erase(key);       // a node in both is up (S3.07; 28476-28518)

  p.regulation.assign(p.nodes.size(), Regulation::none);
  for (std::uint32_t i = 0; i < p.nodes.size(); ++i) {
    const std::string& key = p.nodes[i].value;
    if (up.contains(key)) p.regulation[i] = Regulation::up;
    else if (down.contains(key)) p.regulation[i] = Regulation::down;
  }
  const auto regulation_of = [&](const std::string& key) {
    if (up.contains(key)) return Regulation::up;
    if (down.contains(key)) return Regulation::down;
    return Regulation::none;
  };

  // The complexes in the corridor that hold a gene (entries 170 and 229): a complex cannot form
  // without its member.
  const auto complexes_holding = [&](const std::string& key) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t v = 0; v < corridor_size; ++v)
      for (const network::NodeKey& member : graph.nodes[v].contains)
        if (member.value == key) { out.push_back(v); break; }
    return out;
  };

  // ---- Steps 3, 5 and 6: what each node must be, with the reason, and S5.05's contradictions.
  std::map<std::string, Requirement, std::less<>> need;
  const auto require = [&](std::string Requirement::*field, const std::string& key, const std::string& why) {
    std::string& slot = need[key].*field;
    if (slot.empty()) slot = why;
  };
  for (const auto& [key, why] : must_be_present) require(&Requirement::present, key, why);
  for (const std::string& key : avoid) {                   // S5.05, extended by entry 229
    require(&Requirement::absent, key, "avoided");
    for (std::uint32_t c : complexes_holding(key))
      require(&Requirement::absent, graph.nodes[c].key.value, "a complex holding avoided " + key);
  }
  if (fsc_out)
    for (const std::string& key : fsc) {                   // S5.17, extended by entry 170
      require(&Requirement::absent, key, "the FSC of the out case");
      for (std::uint32_t c : complexes_holding(key))
        require(&Requirement::absent, graph.nodes[c].key.value, "a complex holding the FSC " + key);
    }
  for (const std::string& key : active_list) require(&Requirement::active, key, "active");
  for (const std::string& key : inactive_list) require(&Requirement::inactive, key, "inactive");
  if (p.named) {                                           // 06.C16
    require(&Requirement::active, source_key, "the source");
    require(&Requirement::active, target_key, "the target");
  } else {
    // FuSION 1 asserted only the first pair's source active (GraphManagerNew.cc:27807-27809).
    require(&Requirement::active, source_key, "the source");
  }
  if (fsc_in)                                              // S5.17: activity matching the observation
    for (const std::string& key : fsc) {
      if (regulation_of(key) == Regulation::down) require(&Requirement::inactive, key, "the down-regulated FSC");
      else require(&Requirement::active, key, "the FSC of the in case");
    }

  // A non-relaxed observation is held consistent (S5.05): FuSION 1's not-relaxed assertion
  // (28394-28401) on a counted node meant present and active for up, not both for down.
  KeySet consistent;
  for (const std::string& key : non_relaxed) {
    if (key == source_key || key == target_key || regulation_of(key) == Regulation::none) continue;
    if (fsc.contains(key) && kase != Case::free) continue;  // the FSC rule holds it already
    consistent.insert(key);
  }

  // S5.05's contradictions: present and absent, or active and inactive, whatever imposed each. A
  // non-relaxed down-regulated gene forced present and active is not one: positional lines keep
  // S5.17's trap, whose with-FSC solve is UNSAT at every point, as in FuSION 1.
  for (const auto& [key, r] : need) {
    if (!r.present.empty() && !r.absent.empty()) contradiction(key, r.present, r.absent);
    if (!r.active.empty() && !r.inactive.empty()) contradiction(key, r.active, r.inactive);
  }

  // ---- The constraints, as problem indices (S5.05, S5.06, S5.17, 06.C16).
  for (const auto& [key, r] : need) {
    const std::optional<std::uint32_t> i = in_n(key);
    if (!i) continue;
    if (!r.present.empty()) p.present.push_back(*i);
    if (!r.absent.empty()) p.absent.push_back(*i);
    if (!r.active.empty()) p.active.push_back(*i);
    if (!r.inactive.empty()) p.inactive.push_back(*i);
  }
  for (const std::string& key : consistent)
    if (auto i = in_n(key)) p.consistent.push_back(*i);
  const auto edges_in_e = [&](const std::vector<std::uint32_t>& lines, std::vector<std::uint32_t>& out) {
    for (std::uint32_t j : lines)
      if (j < edge_index.size() && edge_index[j] >= 0) out.push_back(static_cast<std::uint32_t>(edge_index[j]));
  };
  edges_in_e(spec.essential_edges, p.edge_selected);
  edges_in_e(spec.avoid_edges, p.edge_avoided);
  edges_in_e(spec.nonrelaxed_edges, p.edge_nonrelaxed);
  for (auto* list : {&p.present, &p.absent, &p.active, &p.inactive, &p.consistent, &p.edge_selected,
                     &p.edge_avoided, &p.edge_nonrelaxed}) {
    std::sort(list->begin(), list->end());
    list->erase(std::unique(list->begin(), list->end()), list->end());
  }

  // ---- The counted observations (S5.12): in N, not an endpoint, not held by a list or the FSC.
  for (std::uint32_t i = 0; i < p.nodes.size(); ++i) {
    const std::string& key = p.nodes[i].value;
    if (p.regulation[i] == Regulation::none || i == p.source || i == p.target) continue;
    if (non_relaxed.contains(key) || avoid.contains(key) || (fsc_in && fsc.contains(key))) continue;
    p.counted.push_back(i);
  }
  for (const std::string& key : fsc) p.fsc.push_back(key);

  // ---- The roles of every observed gene of the run (07's seven words, 1.5; S5.12).
  const auto role_of = [&](const std::optional<network::NodeKey>& node, MatchGroup group) {
    if (group == MatchGroup::not_in_network && !node) return Role::not_in_network;
    if (!node || !corridor_index.contains(node->value)) return Role::outside_corridor;
    const std::string& key = node->value;
    const std::optional<std::uint32_t> i = in_n(key);
    if (!i) return Role::outside_encoding;
    if (*i == p.source || *i == p.target) return Role::exempt_endpoint;
    if (fsc_out && fsc.contains(key)) return Role::removed_with_fsc;
    if (std::binary_search(p.counted.begin(), p.counted.end(), *i)) return Role::counted;
    return Role::forced;
  };
  const auto direction = [&](const std::optional<network::NodeKey>& node, Regulation run) {
    const Regulation in_case = node ? regulation_of(node->value) : Regulation::none;
    return (in_case == Regulation::none ? run : in_case) == Regulation::down ? 'd' : 'u';
  };
  KeySet listed;
  for (const CaseSpec::Observation& o : spec.observations) {
    p.observations.push_back({o.gene, o.symbol, direction(o.node, o.regulation), role_of(o.node, o.group)});
    listed.insert(o.gene);
  }
  for (const auto* confirmed : {&spec.confirmed_up, &spec.confirmed_down})
    for (const CaseSpec::Id& id : *confirmed) {
      if (!listed.insert(id.given).second) continue;
      auto it = corridor_index.find(id.node.value);
      const std::string symbol = it == corridor_index.end() ? id.node.value : graph.nodes[it->second].label;
      const Regulation run = confirmed == &spec.confirmed_up ? Regulation::up : Regulation::down;
      p.observations.push_back({id.given, symbol, direction(id.node, run),
                                role_of(id.node, it == corridor_index.end() ? MatchGroup::outside_corridor
                                                                            : MatchGroup::in_corridor)});
    }

  // ---- Step 9 (S5.03, S5.15, S5.16): the constraint families. A given flag decides; without
  // one, the inputs given turn each family on. FuSION 1 gated both on its flag and treated a
  // negative threshold as off (main.cc:4561-4563; GraphManagerNew.cc:28603, 28713).
  const bool allowed = settings.families.enabled.value_or(true);
  const auto& coexpression_threshold = settings.families.coexpression_threshold;
  const auto& frozen_threshold = settings.families.frozen_threshold;
  p.families.coexpression = allowed && spec.coexpression_table && coexpression_threshold && *coexpression_threshold >= 0;
  p.families.frozen = allowed && frozen_threshold && *frozen_threshold >= 0;
  if (p.families.coexpression) {
    p.families.coexpression_threshold = coexpression_threshold;
    std::set<std::pair<std::uint32_t, std::uint32_t>> pairs;
    for (const CaseSpec::Pair& pair : spec.coexpression) {
      if (!(pair.score > *coexpression_threshold)) continue;   // strict > (S5.15)
      const auto a = in_n(pair.a.value), b = in_n(pair.b.value);
      if (!a || !b || *a == *b) continue;
      pairs.insert({std::min(*a, *b), std::max(*a, *b)});
    }
    p.coexpressed.assign(pairs.begin(), pairs.end());
  }
  if (p.families.frozen) {
    p.families.frozen_threshold = frozen_threshold;
    for (std::uint32_t j = 0; j < p.edges.size(); ++j)
      if (p.edges[j].score > *frozen_threshold) p.frozen.push_back(j);   // strict > (S5.16)
  }
  for (const Problem::Edge& e : p.edges) p.unscored += e.unscored ? 1U : 0U;

  // ---- Step 7 (S5.07, 06.C14): both node bounds to the counted observations, both edge bounds to
  // the counted edges. FuSION 1 clamped the node bounds to |up| + |down| (main.cc:4699-4714) and
  // never the edge bounds, whose bit-vector values then wrapped (S5.09).
  const int r_n = static_cast<int>(p.counted.size()), r_e = static_cast<int>(p.edges.size());
  p.box = settings.box;
  const auto clamp = [&](int& value, int limit, const char* what, const std::string& because) {
    if (value <= limit) return;
    p.warnings.emplace_back("solve.bound_clamped", std::string(what) + " " + std::to_string(value) + " -> " +
                                                       std::to_string(limit) + " (" + because + ")");
    value = limit;
  };
  const std::string counted_text = std::to_string(r_n) + " counted observation" + (r_n == 1 ? "" : "s");
  const std::string edges_text = std::to_string(r_e) + " encoded edge" + (r_e == 1 ? "" : "s");
  clamp(p.box.lo.n, r_n, "node_min", counted_text);
  clamp(p.box.hi.n, r_n, "node_max", counted_text);
  clamp(p.box.lo.e, r_e, "edge_min", edges_text);
  clamp(p.box.hi.e, r_e, "edge_max", edges_text);

  // ---- Step 8 (S5.11, 06.C15): the pre-checks, which skip the case's checks.
  const auto skipped = [&](CaseStatus status, std::string message) -> std::variant<Problem, Skipped> {
    auto shared = std::make_shared<const Problem>(std::move(p));
    return Skipped{status, std::move(message), std::move(shared)};
  };
  const auto path_text = graph.nodes[graph.source].label + " to " + graph.nodes[graph.target].label;
  for (const auto& [key, why] : must_be_present) {
    auto it = corridor_index.find(key);
    const std::string label = it == corridor_index.end() ? key : graph.nodes[it->second].label;
    const std::optional<std::uint32_t> i = in_n(key);
    bool has_in = false, has_out = false;
    if (i)
      for (const Problem::Edge& e : p.edges) {
        has_in = has_in || e.tgt == *i;
        has_out = has_out || e.src == *i;
      }
    if (!i || (*i != p.source && !has_in) || (*i != p.target && !has_out))
      return skipped(CaseStatus::infeasible, label + " (" + why + ") cannot lie on a path from " + path_text);
  }
  for (std::uint32_t j : spec.essential_edges)
    if (j >= edge_index.size() || edge_index[j] < 0)
      return skipped(CaseStatus::infeasible, "the essential edge " + graph.edges[j].key.source.value + " -> " +
                                                 graph.edges[j].key.target.value + " cannot lie on a path from " +
                                                 path_text);
  if (up.empty() && down.empty())
    return skipped(CaseStatus::no_de_gene,
                   "no differentially expressed gene in the corridor; no path can cross one (S5.14)");
  return p;
}

// Step 10: a SHA-256 over a canonical text of everything that can change an answer, and nothing
// else: no limits, box, points, solutions, retry or threads (06, "Building a case").
core::Sha256 fingerprint(const Problem& p) {
  std::string text = "fusion.solve " + std::to_string(encoder_version) + "\ncase " + std::string(name(p.kase)) +
                     "\nnamed " + (p.named ? "1" : "0") + "\nseed " + std::to_string(p.seed) +
                     "\nbound " + std::to_string(p.bound) + "\nsource " + std::to_string(p.source) +
                     "\ntarget " + std::to_string(p.target) + "\npolarity " +
                     std::to_string(p.relaxed_states[0]) + " " + std::to_string(p.relaxed_states[1]) + "\n";
  for (std::uint32_t i = 0; i < p.nodes.size(); ++i)
    text += "node " + p.nodes[i].value + " " + std::to_string(static_cast<int>(p.regulation[i])) + "\n";
  for (const Problem::Edge& e : p.edges) {
    text += "edge " + std::to_string(e.src) + " " + std::to_string(e.tgt) + " " + e.key.source.value + "|" +
            e.key.target.value + "|" + e.key.type + "|";
    for (std::size_t k = 0; k < e.key.subtypes.size(); ++k) text += (k ? "," : "") + e.key.subtypes[k];
    text += " " + std::to_string(static_cast<int>(e.effect)) + " " + number(e.score) + "\n";
  }
  const auto list = [&text](const char* label, const std::vector<std::uint32_t>& values) {
    text += label;
    for (std::uint32_t v : values) text += " " + std::to_string(v);
    text += "\n";
  };
  list("counted", p.counted);
  list("present", p.present);
  list("absent", p.absent);
  list("active", p.active);
  list("inactive", p.inactive);
  list("consistent", p.consistent);
  list("edge_selected", p.edge_selected);
  list("edge_avoided", p.edge_avoided);
  list("edge_nonrelaxed", p.edge_nonrelaxed);
  list("frozen", p.frozen);
  text += "coexpressed";
  for (const auto& [a, b] : p.coexpressed) text += " " + std::to_string(a) + "-" + std::to_string(b);
  text += "\nfsc";
  for (const std::string& key : p.fsc) text += " " + key;
  text += "\nroles";
  for (const ObservationRow& row : p.observations) text += " " + row.gene + ":" + std::string(name(row.role));
  text += "\n";
  return core::Sha256::of(text);
}

}  // namespace fusion::solve
