// Failures (S4.02, S4.10, 05.C5), reported empty corridors (S4.18), warnings, and the check before the
// first step (05.C3).
#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

namespace {

struct Thrown {
    std::string code, message;
};

Thrown failure(TestNetwork& net, const corridor::Request& r, Harness& h, const corridor::Naming& naming = {}) {
    network::BindingTable table(net.network, network::Maps{});
    try {
        (void)corridor::compute(net.network, table, r, naming, h.ctx);
    } catch (const core::Failure& f) {
        return {f.code(), f.what()};
    }
    return {"no failure", ""};
}

NetSpec chain() {
    return {genes({"s", "a", "b", "c", "t", "u"}),
            {},
            {edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "t"), edge("s", "u")}};
}

}  // namespace

TEST_CASE("corridor: an id that binds to no node fails, whatever its role", "[corridor][S4.02][05.C5]") {
    Harness h;
    TestNetwork net(chain(), h.ctx);
    corridor::Naming naming;
    naming.network_graph = 2;
    CHECK(failure(net, request("zz", "t", 4), h, naming).code == "corridor.unknown_id");
    const Thrown target = failure(net, request("s", "t2", 4), h, naming);
    CHECK(target.code == "corridor.unknown_id");
    CHECK(target.message == "target t2 is not in network 2");
    corridor::Request r = request("s", "t", 4);
    r.exclude = {"zz", "yy"};
    const Thrown excluded = failure(net, r, h);
    CHECK(excluded.code == "corridor.unknown_id");
    CHECK(excluded.message == "excluded yy, excluded zz are not in the network");
    CHECK(h.events.events.empty());   // a failure reports nothing and leaves no corridor
}

TEST_CASE("corridor: a corridor has one source node and one target node, different from each other",
          "[corridor][S4.10][05.C5]") {
    Harness h;
    TestNetwork net(chain(), h.ctx);
    corridor::Request two = request("s", "t", 4);
    two.target = {"t", "c"};
    const Thrown pair = failure(net, two, h);
    CHECK(pair.code == "corridor.one_pair");
    CHECK(pair.message == "2 target nodes (c, t); a corridor has one source and one target");

    corridor::Request none = request("s", "t", 4);
    none.source.clear();
    CHECK(failure(net, none, h).code == "corridor.one_pair");

    const Thrown same = failure(net, request("s", "s", 4), h);
    CHECK(same.code == "corridor.same_node");
    CHECK(same.message == "source s and target s are the same node");
    const Thrown spelled = failure(net, request("s", ":s", 4), h);   // the id rule deletes ':' (02.C3)
    CHECK(spelled.code == "corridor.same_node");
    CHECK(spelled.message == "source s and target :s are the same node, s");
}

TEST_CASE("corridor: each empty reason is a reported result with its shortest path", "[corridor][S4.18]") {
    Harness h;
    TestNetwork net(chain(), h.ctx);
    SECTION("beyond the bound") {
        Cut c(net, request("s", "t", 3), h);
        REQUIRE(c.outcome.report);
        CHECK(c.outcome.report->code == "corridor.empty");
        CHECK(c.outcome.report->message == "no path from s to t within bound 3: the shortest path has 4 edges");
        CHECK(c->empty_reason() == corridor::EmptyReason::beyond_bound);
        CHECK(c->shortest_path() == 4u);
        CHECK(c->counts().forward_set == 5);   // s; a, u; b; c
        CHECK(corridor::check_invariants(*c).empty());
        const auto events = h.events_of("corridor.summary");
        REQUIRE(events.size() == 1);
        CHECK(events[0]["nodes"] == 0);
        CHECK(events[0]["edges"] == 0);
        CHECK(events[0]["shortest_path"] == 4);
    }
    SECTION("no path at all") {
        Cut c(net, request("t", "s", 3), h);
        REQUIRE(c.outcome.report);
        CHECK(c.outcome.report->message == "no path from t to s over traversable edges");
        CHECK(c->empty_reason() == corridor::EmptyReason::no_path);
        CHECK_FALSE(c->shortest_path());
        CHECK(corridor::check_invariants(*c).empty());
    }
    SECTION("no path over traversable edges") {
        TestNetwork bound_only({genes({"s", "t"}), {}, {edge("s", "t", {"association"})}}, h.ctx);
        Cut c(bound_only, request("s", "t", 3), h);
        CHECK(c->empty_reason() == corridor::EmptyReason::no_path);
    }
    SECTION("the step goes on past the bound only to find the shortest path") {
        Cut c(net, request("s", "t", 1), h);
        CHECK(c->shortest_path() == 4u);
        CHECK(c->counts().forward_set == 3);   // s, a, u
    }
}

TEST_CASE("corridor: the console names each count", "[corridor][S4.17]") {
    Harness h;
    TestNetwork net(chain(), h.ctx);
    corridor::Naming naming;
    naming.graph = 3;
    network::BindingTable table(net.network, network::Maps{});
    const auto out = corridor::compute(net.network, table, request("s", "t", 4), naming, h.ctx);
    CHECK(h.infos() == Strings{"Corridor from s to t, bound 4, edges_to_target any",
                               "  Traversable edges: 5 of 5 (skipping component, association, dissociation, mapping)",
                               "  Edges into the target from the forward set: 1; dropped by edges_to_target: 0",
                               "  Forward set: 6 nodes within 4 edges of the source",
                               "  Backward set: 5 of them within 4 edges of the target",
                               "  Corridor, graph 3: 5 nodes, 4 edges; shortest path 4 edges"});
    const auto events = h.events_of("corridor.summary");
    REQUIRE(events.size() == 1);
    CHECK(events[0]["graph"] == 3);
    CHECK(events[0]["nodes"] == 5);
    CHECK(events[0]["edges"] == 4);
    CHECK(events[0]["forward_set"] == 6);
    CHECK(events[0]["backward_set"] == 5);
    CHECK(events[0]["digest"] == out.value->digest().hex());
}

TEST_CASE("corridor: repeated ids and ids on one node collapse with a warning", "[corridor][S4.02]") {
    Harness h;
    TestNetwork net({{{"s"}, {"t"}, {"f1", "f2"}}, {}, {edge("s", "f1"), edge("f1", "t")}}, h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.source = {"s", "s"};
    r.target = {"t"};
    Cut c(net, r, h);
    CHECK(h.warnings("corridor.repeated_id") == Strings{"source s is listed more than once"});
    CHECK(c->bindings().size() == 2);

    Harness h2;
    corridor::Request family = request("f1", "t", 2);
    family.source = {"f1", "f2"};
    family.target = {"t"};
    TestNetwork net2({{{"s"}, {"t"}, {"f1", "f2"}}, {}, {edge("f1", "t")}}, h2.ctx);
    Cut c2(net2, family, h2);
    CHECK(h2.warnings("corridor.repeated_id") == Strings{"source f2 and f1 bind to one node, f1"});
    CHECK(c2->source() == net2.node("f1"));
}

TEST_CASE("corridor: a skipped subtype that no edge carries gets a warning, unless it is a default",
          "[corridor][S4.04]") {
    Harness h;
    TestNetwork net(chain(), h.ctx);
    corridor::Request r = request("s", "t", 4);
    r.skip_subtypes = {"component", "mapping", "activaton"};
    Cut c(net, r, h);
    CHECK(h.warnings("corridor.unused_skip_subtype") ==
          Strings{"skip_subtypes names activaton, which no edge of the network carries"});
}

TEST_CASE("corridor: an id held by several nodes binds to the first claim with a warning", "[corridor][S2.27]") {
    Harness h;
    TestNetwork net({{{"s"}, {"t"}, {"a", "x"}, {"b", "x"}}, {}, {edge("s", "a"), edge("a", "t")}}, h.ctx);
    network::BindingTable probe(net.network, network::Maps{});
    if (probe.bind("probe", "x").status != network::BindStatus::ambiguous)
        SKIP("the network keeps each id in one node, so no id is ambiguous");
    corridor::Request r = request("s", "t", 2);
    r.exclude = {"x"};
    Cut c(net, r, h);
    CHECK(h.warnings("corridor.ambiguous_binding").size() == 1);
}

TEST_CASE("corridor: ids that appear in no network input are found before the first step", "[corridor][05.C3]") {
    corridor::Request r;
    r.source = {"hsa1956"};
    r.target = {"hsa35766"};
    r.exclude = {"hsa:3725", "hsa35766"};
    const std::unordered_set<std::string> inputs{"hsa1956", "hsa3576", "hsa3725"};
    CHECK(corridor::ids_missing_from_inputs(r, inputs) == Strings{"hsa35766"});
    r.target = {"hsa3576"};
    CHECK(corridor::ids_missing_from_inputs(r, inputs) == Strings{"hsa35766"});
    r.exclude.clear();
    CHECK(corridor::ids_missing_from_inputs(r, inputs).empty());
}
