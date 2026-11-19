// fusion corridor: check_invariants, section 05's test hook. It recomputes the traversal graph by
// subtype name and both distances by relaxation to a fixed point, which are other methods than
// compute()'s interned handles and breadth-first searches, and compares the corridor with them.
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/corridor/corridor.hpp"
#include "impl.hpp"

namespace fusion::corridor {

using network::EdgeIdx;
using network::Network;
using network::NodeIdx;

namespace {

// Shortest distances from `from` over the usable edges, forwards or backwards, by Bellman-Ford rounds.
std::vector<std::uint32_t> relax(const Network& net, const std::vector<bool>& usable, NodeIdx from, bool forward) {
    std::vector<std::uint32_t> d(net.nodes().size(), kInf);
    d[from] = 0;
    for (bool changed = true; changed;) {
        changed = false;
        for (EdgeIdx e = 0; e < usable.size(); ++e) {
            if (!usable[e]) continue;
            const network::Edge& edge = net.edges()[e];
            const NodeIdx a = forward ? edge.src : edge.tgt;
            const NodeIdx z = forward ? edge.tgt : edge.src;
            if (d[a] != kInf && d[a] + 1 < d[z]) {
                d[z] = d[a] + 1;
                changed = true;
            }
        }
    }
    return d;
}

std::string text(std::optional<std::uint32_t> d) { return d ? std::to_string(*d) : "none"; }

}  // namespace

std::vector<std::string> check_invariants(const Corridor& c) {
    std::vector<std::string> problems;
    const Network& net = c.network();
    const network::GraphView& view = c.view();
    const Request n = normalised(c.request());
    const std::uint32_t b = c.bound();
    const NodeIdx s = c.source();
    const NodeIdx t = c.target();
    const std::size_t n_nodes = net.nodes().size();
    const std::size_t n_edges = net.edges().size();
    auto key = [&](NodeIdx v) { return net.node_key(v).value; };
    auto within = [&](std::uint32_t d) { return d != kInf && d <= b; };

    // Exclusions: the named nodes, and every complex with an excluded component (S4.06, entry 170).
    std::vector<bool> excluded(n_nodes, false);
    for (const Excluded& x : c.excluded()) excluded[x.node] = true;
    for (const Bound& x : c.bindings())
        if (x.role == Role::excluded && !excluded[x.binding.node])
            problems.push_back("excluded id " + x.binding.given + " binds to " + key(x.binding.node) +
                               ", which is not excluded");
    for (const Excluded& x : c.excluded()) {
        const auto parts = net.components(x.node);
        const bool has_excluded_part =
            std::any_of(parts.begin(), parts.end(), [&](NodeIdx p) { return excluded[p]; });
        if (x.complex_of_excluded && !has_excluded_part)
            problems.push_back("complex " + key(x.node) + " is excluded but contains no excluded node");
    }
    for (NodeIdx v = 0; v < n_nodes; ++v)
        for (NodeIdx part : net.components(v))
            if (excluded[part] && !excluded[v]) {
                problems.push_back("complex " + key(v) + " contains the excluded " + key(part) + " but is not excluded");
                break;
            }

    // The traversal graph by subtype name (S4.04, S4.05, S4.06, S4.11 with 05.C2).
    auto carries = [&](EdgeIdx e, std::initializer_list<std::string_view> names) {
        for (core::Sym st : net.subtypes(e))
            if (std::find(names.begin(), names.end(), net.interner().str(st)) != names.end()) return true;
        return false;
    };
    auto skipped = [&](EdgeIdx e) {
        for (core::Sym st : net.subtypes(e))
            if (std::find(n.skip_subtypes.begin(), n.skip_subtypes.end(), net.interner().str(st)) != n.skip_subtypes.end())
                return true;
        return false;
    };
    auto rule = [&](EdgeIdx e) {
        switch (n.edges_to_target) {
            case EdgesToTarget::any: return true;
            case EdgesToTarget::activation: return carries(e, {"activation", "inhibition", "phosphorylation"});
            case EdgesToTarget::expression: return carries(e, {"expression", "repression"});
        }
        return true;
    };
    std::vector<bool> base(n_edges), in_t(n_edges);
    std::uint32_t traversable = 0;
    for (EdgeIdx e = 0; e < n_edges; ++e) {
        const network::Edge& edge = net.edges()[e];
        if (!skipped(e)) ++traversable;
        base[e] = !skipped(e) && edge.src != edge.tgt && edge.tgt != s && edge.src != t && !excluded[edge.src] &&
                  !excluded[edge.tgt];
        in_t[e] = base[e] && (edge.tgt != t || rule(e));
    }
    const std::vector<std::uint32_t> ds = relax(net, in_t, s, true);
    const std::vector<std::uint32_t> dt = relax(net, in_t, t, false);

    // Edges: kept exactly when in T with ds(u) + 1 + dt(v) <= b (S4.09).
    std::vector<bool> end_of_kept(n_nodes, false);
    std::uint32_t kept = 0;
    for (EdgeIdx e = 0; e < n_edges; ++e) {
        const network::Edge& edge = net.edges()[e];
        const bool expected = in_t[e] && ds[edge.src] != kInf && dt[edge.tgt] != kInf &&
                              std::uint64_t{ds[edge.src]} + 1 + dt[edge.tgt] <= b;
        if (view.has_edge(e) != expected)
            problems.push_back("edge " + net.edge_key(e).text() +
                               (expected ? " lies on a walk within the bound but is not kept"
                                         : " is kept but lies on no walk within the bound"));
        if (view.has_edge(e)) {
            end_of_kept[edge.src] = end_of_kept[edge.tgt] = true;
            ++kept;
        }
    }

    // Nodes: exactly the ends of kept edges (S4.12), none excluded, with their distances (S4.07).
    for (NodeIdx v = 0; v < n_nodes; ++v) {
        if (view.has_node(v) != end_of_kept[v])
            problems.push_back("node " + key(v) + (end_of_kept[v] ? " ends a kept edge but is not kept"
                                                                   : " is kept but ends no kept edge"));
        if (!view.has_node(v)) continue;
        if (excluded[v]) problems.push_back("node " + key(v) + " is excluded but kept");
        if (c.ds(v) != ds[v] || c.dt(v) != dt[v])
            problems.push_back("node " + key(v) + " has ds " + std::to_string(c.ds(v)) + " and dt " +
                               std::to_string(c.dt(v)) + ", but its distances are " + std::to_string(ds[v]) +
                               " and " + std::to_string(dt[v]));
    }

    // Emptiness, its reason and the shortest path (S4.18).
    const bool empty = kept == 0;
    if (c.empty() != empty) problems.push_back(c.empty() ? "the corridor is empty but keeps edges" : "the corridor has no edge");
    const std::optional<std::uint32_t> shortest = ds[t] != kInf ? std::optional(ds[t]) : std::nullopt;
    if (c.shortest_path() != shortest)
        problems.push_back("shortest path " + text(c.shortest_path()) + ", expected " + text(shortest));
    bool dropped_within = false;
    Counts expected;
    expected.traversable_edges = traversable;
    for (NodeIdx v = 0; v < n_nodes; ++v)
        if (within(ds[v])) ++expected.forward_set;
    for (EdgeIdx e : net.in_edges(t)) {
        const NodeIdx u = net.edges()[e].src;
        if (!base[e] || !within(ds[u])) continue;
        ++expected.into_target;
        if (!rule(e)) {
            ++expected.into_target_dropped;
            if (ds[u] < b) dropped_within = true;
        }
    }
    const EmptyReason reason = !empty           ? EmptyReason::none
                               : dropped_within ? EmptyReason::edges_to_target
                               : shortest       ? EmptyReason::beyond_bound
                                                : EmptyReason::no_path;
    if (c.empty_reason() != reason)
        problems.push_back("empty reason " + std::string(detail::reason_name(c.empty_reason())) + ", expected " +
                           std::string(detail::reason_name(reason)));

    // The backward set: forward-set nodes within b edges of the target inside the forward set.
    if (!empty) {
        std::vector<bool> in_f(n_edges);
        for (EdgeIdx e = 0; e < n_edges; ++e)
            in_f[e] = in_t[e] && within(ds[net.edges()[e].src]) && within(ds[net.edges()[e].tgt]);
        const std::vector<std::uint32_t> dt_f = relax(net, in_f, t, false);
        for (NodeIdx v = 0; v < n_nodes; ++v)
            if (within(ds[v]) && within(dt_f[v])) ++expected.backward_set;
    }
    const Counts& k = c.counts();
    auto compare = [&](std::string_view what, std::uint32_t got, std::uint32_t want) {
        if (got != want)
            problems.push_back(std::string(what) + " is " + std::to_string(got) + ", expected " + std::to_string(want));
    };
    compare("traversable_edges", k.traversable_edges, expected.traversable_edges);
    compare("into_target", k.into_target, expected.into_target);
    compare("into_target_dropped", k.into_target_dropped, expected.into_target_dropped);
    compare("forward_set", k.forward_set, expected.forward_set);
    compare("backward_set", k.backward_set, expected.backward_set);
    if (detail::digest_of(c.impl()) != c.digest()) problems.push_back("the digest does not match the content");
    return problems;
}

}  // namespace fusion::corridor
