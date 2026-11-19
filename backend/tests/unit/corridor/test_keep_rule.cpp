// The keep rule (S4.09), the bound (S4.03), distances (S4.07), trimming during the search (S4.11 as
// 05.C2 reads it) and complex members (S4.12), on small synthetic networks (section 05, "Test hooks").
#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

TEST_CASE("corridor: a diamond keeps both branches with their distances", "[corridor][S4.09][S4.07]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "t"}), {}, {edge("s", "a"), edge("s", "b"), edge("a", "t"), edge("b", "t")}},
                    h.ctx);
    Cut c(net, request("s", "t", 2), h);
    REQUIRE_FALSE(c.outcome.report);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "b>t", "s>a", "s>b"});
    CHECK(node_names(*c) == Strings{"a", "b", "s", "t"});
    CHECK(c->ds(net.node("s")) == 0);
    CHECK(c->ds(net.node("a")) == 1);
    CHECK(c->ds(net.node("t")) == 2);
    CHECK(c->dt(net.node("s")) == 2);
    CHECK(c->dt(net.node("b")) == 1);
    CHECK(c->dt(net.node("t")) == 0);
    CHECK(c->shortest_path() == 2u);
    CHECK(c->source() == net.node("s"));
    CHECK(c->target() == net.node("t"));
    CHECK(c->bound() == 2);
    const corridor::Counts& k = c->counts();
    CHECK(k.traversable_edges == 4);
    CHECK(k.into_target == 2);
    CHECK(k.into_target_dropped == 0);
    CHECK(k.forward_set == 4);
    CHECK(k.backward_set == 4);
    CHECK(corridor::check_invariants(*c).empty());
}

TEST_CASE("corridor: the bound is inclusive and counts edges", "[corridor][S4.03]") {
    // s -> a -> b -> t reaches the target at exactly b = 3; z is also 3 edges away, so its edge into the
    // target lies beyond the bound, and the side branch x -> y -> z is not kept.
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "t", "x", "y", "z"}),
                     {},
                     {edge("s", "a"), edge("a", "b"), edge("b", "t"), edge("s", "x"), edge("x", "y"), edge("y", "z"),
                      edge("z", "t")}},
                    h.ctx);
    Cut c(net, request("s", "t", 3), h);
    REQUIRE_FALSE(c.outcome.report);
    CHECK(edge_names(*c) == Strings{"a>b", "b>t", "s>a"});
    CHECK(node_names(*c) == Strings{"a", "b", "s", "t"});
    CHECK(c->counts().forward_set == 7);    // s; a, x; b, y; t, z
    CHECK(c->counts().into_target == 2);    // b -> t and z -> t, from tails within the bound
    CHECK(c->counts().backward_set == 7);   // every forward-set node is within 3 of the target
    CHECK(corridor::check_invariants(*c).empty());

    Cut wider(net, request("s", "t", 4), h);
    CHECK(sorted(edge_names(*wider)) == Strings{"a>b", "b>t", "s>a", "s>x", "x>y", "y>z", "z>t"});
}

TEST_CASE("corridor: bound 0 keeps nothing and reports the shortest path", "[corridor][S4.03][S4.18]") {
    Harness h;
    TestNetwork net({genes({"s", "t"}), {}, {edge("s", "t")}}, h.ctx);
    Cut c(net, request("s", "t", 0), h);
    REQUIRE(c.outcome.report);
    CHECK(c.outcome.report->code == "corridor.empty");
    CHECK(c.outcome.report->message == "no path from s to t within bound 0: the shortest path has 1 edge");
    CHECK(c->empty());
    CHECK(c->empty_reason() == corridor::EmptyReason::beyond_bound);
    CHECK(c->shortest_path() == 1u);
    CHECK(edge_names(*c).empty());
    CHECK(node_names(*c).empty());
    CHECK(c->counts().forward_set == 1);
    CHECK(c->counts().into_target == 1);
    CHECK(c->counts().backward_set == 0);
    CHECK(corridor::check_invariants(*c).empty());
}

TEST_CASE("corridor: the corridor keeps walks, not only simple paths", "[corridor][S4.11]") {
    // b lies on no simple path from s to t, but on the walk s a b a t of 4 edges (05's walk rule).
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "t"}), {}, {edge("s", "a"), edge("a", "b"), edge("b", "a"), edge("a", "t")}},
                    h.ctx);
    Cut four(net, request("s", "t", 4), h);
    CHECK(sorted(edge_names(*four)) == Strings{"a>b", "a>t", "b>a", "s>a"});
    CHECK(four->dt(net.node("b")) == 2);
    Cut three(net, request("s", "t", 3), h);
    CHECK(sorted(edge_names(*three)) == Strings{"a>t", "s>a"});
    CHECK(corridor::check_invariants(*four).empty());
    CHECK(corridor::check_invariants(*three).empty());
}

TEST_CASE("corridor: self-loops, edges into the source and edges out of the target are trimmed", "[corridor][S4.11][05.C2]") {
    Harness h;
    SECTION("a self-loop") {
        TestNetwork net({genes({"s", "a", "t"}), {}, {edge("s", "a"), edge("a", "a"), edge("a", "t")}}, h.ctx);
        Cut c(net, request("s", "t", 3), h);   // untrimmed, a -> a would fit: 1 + 1 + 1 <= 3
        CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
        CHECK(corridor::check_invariants(*c).empty());
    }
    SECTION("an edge into the source") {
        TestNetwork net({genes({"s", "a", "t"}), {}, {edge("s", "a"), edge("a", "s"), edge("a", "t")}}, h.ctx);
        Cut c(net, request("s", "t", 4), h);   // untrimmed, a -> s would fit: 1 + 1 + 2 <= 4
        CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
    }
    SECTION("an edge out of the target, and a node reached only through the target (the CXCR6-family case)") {
        TestNetwork net({genes({"s", "c", "t"}), {}, {edge("s", "t"), edge("t", "c"), edge("c", "t")}}, h.ctx);
        Cut c(net, request("s", "t", 3), h);   // untrimmed, t -> c and c -> t would fit
        CHECK(edge_names(*c) == Strings{"s>t"});
        CHECK(node_names(*c) == Strings{"s", "t"});
        CHECK(c->counts().into_target == 1);   // c is reachable only through the target
        CHECK(c->counts().forward_set == 2);
    }
    SECTION("a node whose only route within the bound passes back through the source (the AREG case)") {
        // x is 3 edges from s; x -> s -> a -> b -> t has 4 edges and fits b = 7 untrimmed, but the route that
        // avoids s, x q1 q2 q3 q4 t, has 5.
        TestNetwork net({genes({"s", "a", "b", "t", "p1", "p2", "x", "q1", "q2", "q3", "q4"}),
                         {},
                         {edge("s", "a"), edge("a", "b"), edge("b", "t"), edge("s", "p1"), edge("p1", "p2"),
                          edge("p2", "x"), edge("x", "s"), edge("x", "q1"), edge("q1", "q2"), edge("q2", "q3"),
                          edge("q3", "q4"), edge("q4", "t")}},
                        h.ctx);
        Cut c(net, request("s", "t", 7), h);
        CHECK(sorted(edge_names(*c)) == Strings{"a>b", "b>t", "s>a"});
        CHECK(corridor::check_invariants(*c).empty());
        Cut wider(net, request("s", "t", 8), h);   // within 8, x's own route fits
        CHECK(wider->view().has_node(net.node("x")));
        CHECK_FALSE(wider->view().has_edge(*net.network->find_edge(
            network::EdgeKey{{"x"}, {"s"}, "PPrel", {"activation"}})));
    }
}

TEST_CASE("corridor: complex members are corridor nodes only when kept in their own right", "[corridor][S4.12]") {
    Harness h;
    const std::string complex = complex_key({"m1", "m2"});
    NetSpec spec{genes({"s", "t", "m1", "m2"}), {{"m1", "m2"}}, {edge("s", complex), edge(complex, "t")}, true};
    SECTION("members off every path stay out") {
        TestNetwork net(spec, h.ctx);
        Cut c(net, request("s", "t", 2), h);
        CHECK(node_names(*c) == Strings{complex, "s", "t"});
        CHECK(sorted(edge_names(*c)) == Strings{complex + ">t", "s>" + complex});
        CHECK(c->counts().traversable_edges == 2);   // the component edges are skipped (S4.04)
        CHECK(corridor::check_invariants(*c).empty());
    }
    SECTION("a member on a path of its own is kept") {
        spec.edges.push_back(edge("s", "m1"));
        spec.edges.push_back(edge("m1", "t"));
        TestNetwork net(spec, h.ctx);
        Cut c(net, request("s", "t", 2), h);
        CHECK(node_names(*c) == Strings{complex, "m1", "s", "t"});
        CHECK_FALSE(c->view().has_node(net.node("m2")));
    }
}

TEST_CASE("corridor: skipped subtypes make an edge non-traversable", "[corridor][S4.04]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "t"}),
                     {},
                     {edge("s", "a", {"activation", "association"}), edge("a", "t"), edge("s", "b", {"inhibition"}),
                      edge("b", "t", {})}},
                    h.ctx);
    Cut c(net, request("s", "t", 2), h);
    CHECK(sorted(edge_names(*c)) == Strings{"b>t", "s>b"});   // a subtype-less edge is traversable
    CHECK(c->counts().traversable_edges == 3);

    corridor::Request open = request("s", "t", 2);
    open.skip_subtypes = {"component"};
    Cut all(net, open, h);
    CHECK(sorted(edge_names(*all)) == Strings{"a>t", "b>t", "s>a", "s>b"});
    CHECK(all->counts().traversable_edges == 4);
}

TEST_CASE("corridor: distances are breadth-first and the corridor is the set the solve considers", "[corridor][S4.07][S5.10]") {
    // Two routes of different lengths: ds and dt are the shortest ones.
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "c", "t"}),
                     {},
                     {edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "t"), edge("s", "c"), edge("a", "t")}},
                    h.ctx);
    Cut c(net, request("s", "t", 4), h);
    CHECK(c->ds(net.node("c")) == 1);
    CHECK(c->dt(net.node("a")) == 1);
    CHECK(c->dt(net.node("b")) == 2);
    CHECK(c->ds(net.node("b")) == 2);
    CHECK(c->shortest_path() == 2u);
    CHECK(sorted(edge_names(*c)) == Strings{"a>b", "a>t", "b>c", "c>t", "s>a", "s>c"});
    CHECK(corridor::check_invariants(*c).empty());
}
