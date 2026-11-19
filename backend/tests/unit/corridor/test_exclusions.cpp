// Exclusions (S4.06 as prompt log entry 170 extends it: excluding a gene also removes every complex
// that contains it) and the excluded-endpoint failure (05.C5).
#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

namespace {

corridor::Request excluding(std::string source, std::string target, std::uint32_t bound, Strings exclude) {
    corridor::Request r = request(std::move(source), std::move(target), bound);
    r.exclude = std::move(exclude);
    return r;
}

std::string failure_code(TestNetwork& net, const corridor::Request& r, Harness& h) {
    network::BindingTable table(net.network, network::Maps{});
    try {
        (void)corridor::compute(net.network, table, r, h.ctx);
    } catch (const core::Failure& f) {
        return f.code();
    }
    return "no failure";
}

}  // namespace

TEST_CASE("corridor: an excluded node and its edges are absent", "[corridor][S4.06]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "x", "t"}), {}, {edge("s", "a"), edge("a", "t"), edge("s", "x"), edge("x", "t")}},
                    h.ctx);
    Cut c(net, excluding("s", "t", 2, {"x"}), h);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
    REQUIRE(c->excluded().size() == 1);
    CHECK(c->excluded()[0].node == net.node("x"));
    CHECK(c->excluded()[0].because == "x");
    CHECK_FALSE(c->excluded()[0].complex_of_excluded);
    CHECK(c->counts().into_target == 1);      // x -> t starts at an excluded node
    CHECK(c->counts().forward_set == 3);
    REQUIRE(c->bindings().size() == 3);
    CHECK(c->bindings()[2].role == corridor::Role::excluded);
    CHECK(c->bindings()[2].binding.given == "x");
    CHECK(corridor::check_invariants(*c).empty());
}

TEST_CASE("corridor: a complex that contains an excluded gene is excluded, and so is a complex around it",
          "[corridor][S4.06]") {
    Harness h;
    const std::string inner = complex_key({"g", "h"});
    const std::string outer = complex_key({inner, "k"});
    TestNetwork net({genes({"s", "a", "g", "h", "k", "t"}),
                     {{"g", "h"}, {inner, "k"}},
                     {edge("s", inner), edge(inner, "t"), edge("s", outer), edge(outer, "t"), edge("s", "a"),
                      edge("a", "t")},
                     true},
                    h.ctx);
    Cut c(net, excluding("s", "t", 2, {"g"}), h);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
    REQUIRE(c->excluded().size() == 3);
    CHECK(c->excluded()[0].node == net.node("g"));
    CHECK_FALSE(c->excluded()[0].complex_of_excluded);
    for (std::size_t i = 1; i < 3; ++i) {
        CHECK(c->excluded()[i].complex_of_excluded);
        CHECK(c->excluded()[i].because == "g");
    }
    CHECK(c->excluded()[1].node < c->excluded()[2].node);   // complexes by index
    CHECK(h.warnings("corridor.excluded_complex").size() == 2);
    CHECK(corridor::check_invariants(*c).empty());

    // Without the exclusion both complexes carry paths.
    Cut open(net, request("s", "t", 2), h);
    CHECK(open->view().has_node(net.node(inner)));
    CHECK(open->view().has_node(net.node(outer)));
}

TEST_CASE("corridor: an endpoint that is excluded fails, directly or as a complex of an excluded gene",
          "[corridor][S4.06][05.C5]") {
    Harness h;
    const std::string complex = complex_key({"g", "h"});
    TestNetwork net({genes({"s", "g", "h", "t"}), {{"g", "h"}}, {edge("s", complex), edge(complex, "t"), edge("s", "t")}},
                    h.ctx);
    CHECK(failure_code(net, excluding("s", complex, 1, {"g"}), h) == "corridor.endpoint_excluded");
    CHECK(failure_code(net, excluding("s", "t", 1, {"s"}), h) == "corridor.endpoint_excluded");
    CHECK(failure_code(net, excluding("s", "t", 1, {"t"}), h) == "corridor.endpoint_excluded");

    network::BindingTable table(net.network, network::Maps{});
    try {
        (void)corridor::compute(net.network, table, excluding("s", complex, 1, {"g"}), h.ctx);
        FAIL("no failure");
    } catch (const core::Failure& f) {
        CHECK(std::string(f.what()) == "target " + complex + " is " + complex + ", a complex that contains the excluded g");
    }
}

TEST_CASE("corridor: excluding one gene of a family node excludes the whole node, with a warning", "[corridor][S4.06][S2.03]") {
    Harness h;
    TestNetwork net({{{"s"}, {"t"}, {"f1", "f2", "f3"}, {"a"}},
                     {},
                     {edge("s", "f1"), edge("f1", "t"), edge("s", "a"), edge("a", "t")}},
                    h.ctx);
    Cut c(net, excluding("s", "t", 2, {"f2"}), h);
    CHECK(sorted(edge_names(*c)) == Strings{"a>t", "s>a"});
    REQUIRE(c->excluded().size() == 1);
    CHECK(c->excluded()[0].node == net.node("f1"));
    CHECK(c->excluded()[0].because == "f2");
    REQUIRE(c->bindings().size() == 3);
    CHECK(c->bindings()[2].binding.status == network::BindStatus::family);
    const Strings warnings = h.warnings("corridor.family_binding");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] == "excluded f2 binds to f1, which holds 3 genes (f1, f2, f3); the whole node is excluded");
}

TEST_CASE("corridor: exclusions are summarised on the console", "[corridor][S4.06][S4.17]") {
    Harness h;
    const std::string complex = complex_key({"g", "h"});
    TestNetwork net({genes({"s", "a", "g", "h", "t"}), {{"g", "h"}}, {edge("s", "a"), edge("a", "t"), edge("s", complex)}},
                    h.ctx);
    Cut c(net, excluding("s", "t", 2, {"g"}), h);
    const Strings lines = h.infos();
    REQUIRE(lines.size() >= 2);
    CHECK(lines[1] == "  Excluded: g, and the complex " + complex + " that contains it");
}
