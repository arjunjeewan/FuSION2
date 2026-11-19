// The edges-to-target rule (S4.05, fixed), its two counts (S4.17) and its names (05.C8).
#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

namespace {

// One edge into t per kind of subtype list, each from its own node one edge from s.
NetSpec into_target_spec() {
    NetSpec spec{genes({"s", "a", "b", "c", "d", "e", "f", "g", "t"}), {}, {}};
    for (std::string n : {"a", "b", "c", "d", "e", "f", "g"}) spec.edges.push_back(edge("s", n));
    spec.edges.push_back(edge("a", "t", {"activation"}));
    spec.edges.push_back(edge("b", "t", {"expression"}));
    spec.edges.push_back(edge("c", "t", {"activation", "expression"}));   // both kinds
    spec.edges.push_back(edge("d", "t", {}));                             // neither kind
    spec.edges.push_back(edge("e", "t", {"phosphorylation"}));
    spec.edges.push_back(edge("f", "t", {"repression"}));
    spec.edges.push_back(edge("g", "t", {"inhibition"}));
    return spec;
}

Strings tails_into_target(const corridor::Corridor& c) {
    Strings tails;
    for (const auto& name : edge_names(c))
        if (name.size() > 2 && name.substr(name.size() - 2) == ">t") tails.push_back(name.substr(0, name.size() - 2));
    return sorted(tails);
}

}  // namespace

TEST_CASE("corridor: each rule keeps the edges that carry its kind", "[corridor][S4.05]") {
    Harness h;
    TestNetwork net(into_target_spec(), h.ctx);
    corridor::Request r = request("s", "t", 2);

    SECTION("any") {
        Cut c(net, r, h);
        CHECK(tails_into_target(*c) == Strings{"a", "b", "c", "d", "e", "f", "g"});
        CHECK(c->counts().into_target == 7);
        CHECK(c->counts().into_target_dropped == 0);
    }
    SECTION("activation: activation, inhibition or phosphorylation; an edge with both kinds passes") {
        r.edges_to_target = corridor::EdgesToTarget::activation;
        Cut c(net, r, h);
        CHECK(tails_into_target(*c) == Strings{"a", "c", "e", "g"});
        CHECK(node_names(*c) == Strings{"a", "c", "e", "g", "s", "t"});
        CHECK(c->counts().into_target == 7);
        CHECK(c->counts().into_target_dropped == 3);   // b, d, f
        CHECK(corridor::check_invariants(*c).empty());
    }
    SECTION("expression: expression or repression; an edge with neither kind passes neither rule") {
        r.edges_to_target = corridor::EdgesToTarget::expression;
        Cut c(net, r, h);
        CHECK(tails_into_target(*c) == Strings{"b", "c", "f"});
        CHECK(c->counts().into_target == 7);
        CHECK(c->counts().into_target_dropped == 4);   // a, d, e, g
        CHECK(corridor::check_invariants(*c).empty());
    }
}

TEST_CASE("corridor: the rule acts only on edges into the target", "[corridor][S4.05]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "t"}), {}, {edge("s", "a", {"expression"}), edge("a", "t", {"activation"})}},
                    h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.edges_to_target = corridor::EdgesToTarget::activation;
    Cut c(net, r, h);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
}

TEST_CASE("corridor: the two target counts include a tail exactly b edges from the source", "[corridor][S4.05][S4.17]") {
    // FuSION 1's M counted every edge into a target examined from a node within the bound, including
    // edges whose head then lies beyond it (S4.17). q is exactly 2 edges from s, so q -> t counts.
    Harness h;
    TestNetwork net({genes({"s", "a", "p", "q", "t"}),
                     {},
                     {edge("s", "a"), edge("a", "t", {"activation"}), edge("s", "p"), edge("p", "q"),
                      edge("q", "t", {"expression"})}},
                    h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.edges_to_target = corridor::EdgesToTarget::activation;
    Cut c(net, r, h);
    REQUIRE_FALSE(c.outcome.report);
    CHECK(c->counts().into_target == 2);
    CHECK(c->counts().into_target_dropped == 1);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});

    r.edges_to_target = corridor::EdgesToTarget::any;
    Cut any(net, r, h);
    CHECK(any->counts().into_target == 2);
    CHECK(any->counts().into_target_dropped == 0);
    CHECK(sorted(edge_names(*any)) == Strings{"a>t", "s>a"});   // q -> t lies beyond the bound either way
}

TEST_CASE("corridor: an empty corridor names the rule when it dropped the edges within the bound", "[corridor][S4.18]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "t"}),
                     {},
                     {edge("s", "a"), edge("a", "t", {"expression"}), edge("s", "b"), edge("b", "t", {"expression"})}},
                    h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.edges_to_target = corridor::EdgesToTarget::activation;
    Cut c(net, r, h);
    REQUIRE(c.outcome.report);
    CHECK(c.outcome.report->code == "corridor.empty");
    CHECK(c->empty_reason() == corridor::EmptyReason::edges_to_target);
    CHECK_FALSE(c->shortest_path());
    CHECK(c.outcome.report->message ==
          "no path from s to t within bound 2: edges_to_target=activation dropped the 2 edges into t that lie "
          "within the bound");
    CHECK(c->counts().into_target_dropped == 2);
    CHECK(corridor::check_invariants(*c).empty());
}

TEST_CASE("corridor: edges_to_target takes its names and FuSION 1's numbers", "[corridor][S4.05][05.C8]") {
    using corridor::EdgesToTarget;
    CHECK(corridor::parse_edges_to_target("any") == EdgesToTarget::any);
    CHECK(corridor::parse_edges_to_target("0") == EdgesToTarget::any);
    CHECK(corridor::parse_edges_to_target("activation") == EdgesToTarget::activation);
    CHECK(corridor::parse_edges_to_target("1") == EdgesToTarget::activation);
    CHECK(corridor::parse_edges_to_target("expression") == EdgesToTarget::expression);
    CHECK(corridor::parse_edges_to_target("2") == EdgesToTarget::expression);
    CHECK_FALSE(corridor::parse_edges_to_target("3"));   // FuSION 1 read a typo as no rule
    CHECK_FALSE(corridor::parse_edges_to_target("both"));
    CHECK(corridor::name(EdgesToTarget::any) == "any");
    CHECK(corridor::name(EdgesToTarget::activation) == "activation");
    CHECK(corridor::name(EdgesToTarget::expression) == "expression");
}
