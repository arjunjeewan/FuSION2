// Property tests on random small networks (section 05, "Test hooks"; section 12's corridor row): on
// 1000 random graphs the corridor equals the walk definition, found by enumerating every walk
// (section 12's walk_corridor, with trimming during the search, 05.C2); every edge of every simple
// path within the bound is kept; the corridor is the set S5.10's search considers; it never holds an
// excluded node; check_invariants() holds; save then load keeps the digest; and a network built in
// another order gives the same digest.
#include <functional>
#include <map>
#include <random>
#include <set>
#include <tuple>

#include "corridor_support.hpp"

using namespace corridor_test;

namespace {

const std::vector<std::vector<std::string>> kTraversable = {
    {"activation"}, {"inhibition"}, {"expression"}, {"repression"}, {"phosphorylation"},
    {"activation", "phosphorylation"}, {"activation", "expression"}, {}, {"compound"}};
const std::vector<std::vector<std::string>> kSkipped = {
    {"association"}, {"component"}, {"dissociation"}, {"inhibition", "association"}};

struct GraphEdge {
    int src = 0, tgt = 0;
    std::vector<std::string> subtypes;
};

struct Trial {
    std::vector<std::string> keys;              // genes, then the complex if any
    std::vector<std::vector<int>> components;   // per node
    std::vector<GraphEdge> edges;
    int s = 0, t = 0;
    std::vector<int> named_excluded;
    std::uint32_t bound = 0;
    corridor::EdgesToTarget rule = corridor::EdgesToTarget::any;
};

Trial random_trial(std::mt19937_64& rng) {
    auto below = [&](std::size_t n) { return static_cast<int>(rng() % n); };
    Trial tr;
    const int genes = 3 + below(6);
    for (int i = 0; i < genes; ++i) tr.keys.push_back("n" + std::to_string(i));
    tr.components.assign(static_cast<std::size_t>(genes), {});
    if (below(3) == 0) {   // a complex of two genes (S4.06's closure, S4.12)
        const int a = below(static_cast<std::size_t>(genes));
        const int b = (a + 1 + below(static_cast<std::size_t>(genes - 1))) % genes;
        tr.keys.push_back(complex_key({tr.keys[static_cast<std::size_t>(a)], tr.keys[static_cast<std::size_t>(b)]}));
        tr.components.push_back({std::min(a, b), std::max(a, b)});
    }
    const auto n = tr.keys.size();
    std::set<std::tuple<int, int, int>> seen;   // a network holds no two edges with one identity
    const auto m = n + static_cast<std::size_t>(below(2 * n + 1));
    for (std::size_t k = 0; k < m; ++k) {
        const int u = below(n), v = below(n);
        const bool skipped = below(4) == 0;
        const auto& lists = skipped ? kSkipped : kTraversable;
        const int list = below(lists.size()) + (skipped ? 100 : 0);
        if (seen.insert({u, v, list}).second) tr.edges.push_back({u, v, lists[static_cast<std::size_t>(list % 100)]});
    }
    tr.s = below(n);
    tr.t = (tr.s + 1 + below(n - 1)) % static_cast<int>(n);
    if (below(3) == 0) {   // never an endpoint itself: it can then fail only as a complex of the gene
        const int x = below(n);
        if (x != tr.s && x != tr.t) tr.named_excluded.push_back(x);
    }
    tr.bound = static_cast<std::uint32_t>(below(10) == 0 ? below(2) : 2 + below(4));
    tr.rule = static_cast<corridor::EdgesToTarget>(below(3));
    return tr;
}

NetSpec spec_of(const Trial& tr, bool reversed) {
    NetSpec spec;
    for (std::size_t i = 0; i < tr.keys.size(); ++i)
        if (tr.components[i].empty()) spec.nodes.push_back({tr.keys[i]});
        else spec.complexes.push_back({tr.keys[static_cast<std::size_t>(tr.components[i][0])],
                                       tr.keys[static_cast<std::size_t>(tr.components[i][1])]});
    for (const GraphEdge& e : tr.edges)
        spec.edges.push_back(edge(tr.keys[static_cast<std::size_t>(e.src)], tr.keys[static_cast<std::size_t>(e.tgt)],
                                  e.subtypes));
    if (reversed) {
        std::reverse(spec.nodes.begin(), spec.nodes.end());
        std::reverse(spec.edges.begin(), spec.edges.end());
    }
    return spec;
}

corridor::Request request_of(const Trial& tr) {
    corridor::Request r = request(tr.keys[static_cast<std::size_t>(tr.s)], tr.keys[static_cast<std::size_t>(tr.t)], tr.bound);
    for (int x : tr.named_excluded) r.exclude.push_back(tr.keys[static_cast<std::size_t>(x)]);
    r.edges_to_target = tr.rule;
    return r;
}

bool has_any(const GraphEdge& e, std::initializer_list<std::string_view> names) {
    for (const auto& s : e.subtypes)
        if (std::find(names.begin(), names.end(), s) != names.end()) return true;
    return false;
}

std::string identity(const Trial& tr, const GraphEdge& e) {
    std::string text = tr.keys[static_cast<std::size_t>(e.src)] + "|" + tr.keys[static_cast<std::size_t>(e.tgt)] + "|PPrel|";
    for (std::size_t k = 0; k < e.subtypes.size(); ++k) text += (k ? "," : "") + e.subtypes[k];
    return text;
}

std::string identity(const network::Network& net, network::EdgeIdx e) {
    const network::EdgeKey key = net.edge_key(e);
    std::string text = key.source.value + "|" + key.target.value + "|" + key.type + "|";
    for (std::size_t k = 0; k < key.subtypes.size(); ++k) text += (k ? "," : "") + key.subtypes[k];
    return text;
}

// The oracle's own reading of section 05: the excluded closure, the edge classes, and the walks.
struct Oracle {
    explicit Oracle(const Trial& tr) : tr_(tr), excluded(tr.keys.size(), false) {
        for (int x : tr.named_excluded) excluded[static_cast<std::size_t>(x)] = true;
        for (std::size_t c = 0; c < tr.keys.size(); ++c)
            for (int part : tr.components[c])
                if (excluded[static_cast<std::size_t>(part)]) excluded[c] = true;
    }
    bool endpoint_excluded() const {
        return excluded[static_cast<std::size_t>(tr_.s)] || excluded[static_cast<std::size_t>(tr_.t)];
    }
    bool usable(const GraphEdge& e) const {   // S4.04, S4.05, S4.06; no trimming
        if (has_any(e, {"component", "association", "dissociation", "mapping"})) return false;
        if (excluded[static_cast<std::size_t>(e.src)] || excluded[static_cast<std::size_t>(e.tgt)]) return false;
        if (e.tgt != tr_.t) return true;
        switch (tr_.rule) {
            case corridor::EdgesToTarget::any: return true;
            case corridor::EdgesToTarget::activation: return has_any(e, {"activation", "inhibition", "phosphorylation"});
            case corridor::EdgesToTarget::expression: return has_any(e, {"expression", "repression"});
        }
        return true;
    }
    bool in_t(const GraphEdge& e) const {     // and the trim of 05.C2
        return usable(e) && e.src != e.tgt && e.tgt != tr_.s && e.src != tr_.t;
    }

    // Every edge of every walk of at most b edges from s to t in T.
    std::set<std::string> walk_corridor() const {
        std::set<std::string> kept;
        std::vector<const GraphEdge*> path;
        std::function<void(int)> walk = [&](int u) {
            if (u == tr_.t) {
                for (const GraphEdge* e : path) kept.insert(identity(tr_, *e));
                return;
            }
            if (path.size() == tr_.bound) return;
            for (const GraphEdge& e : tr_.edges)
                if (e.src == u && in_t(e)) {
                    path.push_back(&e);
                    walk(e.tgt);
                    path.pop_back();
                }
        };
        walk(tr_.s);
        return kept;
    }

    // Every edge of every simple path of at most b edges from s to t, over the untrimmed edges.
    std::set<std::string> simple_paths() const {
        std::set<std::string> kept;
        std::vector<const GraphEdge*> path;
        std::vector<bool> on(tr_.keys.size(), false);
        std::function<void(int)> walk = [&](int u) {
            if (u == tr_.t) {
                for (const GraphEdge* e : path) kept.insert(identity(tr_, *e));
                return;
            }
            if (path.size() == tr_.bound) return;
            on[static_cast<std::size_t>(u)] = true;
            for (const GraphEdge& e : tr_.edges)
                if (e.src == u && usable(e) && !on[static_cast<std::size_t>(e.tgt)]) {
                    path.push_back(&e);
                    walk(e.tgt);
                    path.pop_back();
                }
            on[static_cast<std::size_t>(u)] = false;
        };
        walk(tr_.s);
        return kept;
    }

    const Trial& tr_;
    std::vector<bool> excluded;
};

}  // namespace

TEST_CASE("corridor: on 1000 random networks the corridor is exactly the walk definition", "[corridor][property][05.C2][S4.09]") {
    std::mt19937_64 rng(20261117);
    Harness h;
    int nonempty = 0, failures = 0, saved = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        const Trial tr = random_trial(rng);
        const Oracle oracle(tr);
        TestNetwork net(spec_of(tr, false), h.ctx);
        network::BindingTable table(net.network, network::Maps{});
        const corridor::Request r = request_of(tr);
        INFO("trial " << trial << ": " << tr.keys[static_cast<std::size_t>(tr.s)] << " -> "
                      << tr.keys[static_cast<std::size_t>(tr.t)] << ", bound " << tr.bound << ", "
                      << tr.edges.size() << " edges");

        if (oracle.endpoint_excluded()) {   // 05.C5
            try {
                (void)corridor::compute(net.network, table, r, h.ctx);
                FAIL("no failure for an excluded endpoint");
            } catch (const core::Failure& f) {
                CHECK(f.code() == "corridor.endpoint_excluded");
            }
            ++failures;
            continue;
        }
        const auto out = corridor::compute(net.network, table, r, h.ctx);
        const corridor::Corridor& c = *out.value;
        const network::Network& nw = *net.network;

        std::set<std::string> kept;
        std::set<std::string> kept_nodes;
        c.view().for_each_edge([&](network::EdgeIdx e) {
            kept.insert(identity(nw, e));
            kept_nodes.insert(nw.node_key(nw.edges()[e].src).value);
            kept_nodes.insert(nw.node_key(nw.edges()[e].tgt).value);
        });
        const std::set<std::string> expected = oracle.walk_corridor();
        REQUIRE(kept == expected);
        const std::vector<std::string> nodes = node_names(c);
        CHECK(std::set<std::string>(nodes.begin(), nodes.end()) == kept_nodes);   // S4.12: no isolated node
        CHECK(c.empty() == expected.empty());
        CHECK(out.report.has_value() == expected.empty());

        const std::set<std::string> simple = oracle.simple_paths();
        CHECK(std::includes(kept.begin(), kept.end(), simple.begin(), simple.end()));

        for (std::size_t v = 0; v < tr.keys.size(); ++v)
            if (oracle.excluded[v]) CHECK_FALSE(c.view().has_node(net.node(tr.keys[v])));

        // S5.10: from the source, without passing the target, the search reaches every corridor edge.
        if (!c.empty()) {
            std::vector<bool> reached(nw.nodes().size(), false), considered(nw.edges().size(), false);
            std::vector<network::NodeIdx> queue{c.source()};
            reached[c.source()] = true;
            for (std::size_t i = 0; i < queue.size(); ++i) {
                if (queue[i] == c.target()) continue;
                c.view().for_each_out_edge(queue[i], [&](network::EdgeIdx e) {
                    considered[e] = true;
                    const network::NodeIdx v = nw.edges()[e].tgt;
                    if (!reached[v]) {
                        reached[v] = true;
                        queue.push_back(v);
                    }
                });
            }
            c.view().for_each_edge([&](network::EdgeIdx e) { CHECK(considered[e]); });
            c.view().for_each_node([&](network::NodeIdx v) { CHECK(reached[v]); });
            ++nonempty;
        }

        CHECK(corridor::check_invariants(c).empty());

        if (trial % 20 == 0) {   // save then load keeps the digest (05.C6)
            corridor::save(c, {}, h.ctx);
            network::BindingTable again(net.network, network::Maps{});
            const auto loaded = corridor::load(net.network, again, h.files.dir() / "corridor.json", h.ctx);
            CHECK(loaded->digest() == c.digest());
            CHECK(corridor::check_invariants(*loaded).empty());
            ++saved;
        }
        if (trial % 10 == 0) {   // the same network built in another order (02.C10)
            TestNetwork other(spec_of(tr, true), h.ctx);
            network::BindingTable other_table(other.network, network::Maps{});
            const auto again = corridor::compute(other.network, other_table, r, h.ctx);
            CHECK(again.value->digest() == c.digest());
        }
    }
    // The generator must reach every outcome often enough to mean something (263, 7 and 50 with this
    // seed; std::mt19937_64 is the same on every platform).
    CHECK(nonempty >= 250);
    CHECK(failures >= 5);
    CHECK(saved >= 50);
}
