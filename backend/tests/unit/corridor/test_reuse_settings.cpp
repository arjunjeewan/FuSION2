// Reuse within a run (05.C4), the request's normal form, the settings registry (05.C8, 1.16) and the
// observation counts that fb_rch reports beside the corridor (05.C9).
#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

namespace {

NetSpec small() {
    return {genes({"s", "a", "b", "t"}), {}, {edge("s", "a"), edge("a", "t"), edge("s", "b"), edge("b", "t")}};
}

}  // namespace

TEST_CASE("corridor: a repeated fb_rch with the same normalised request reuses the corridor", "[corridor][05.C4][S4.20]") {
    Harness h;
    TestNetwork net(small(), h.ctx);
    corridor::Request first = request("s", "t", 2);
    first.exclude = {"b"};
    Cut c(net, first, h);
    corridor::save(*c, {}, h.ctx);
    const std::size_t files_after_first = h.files.created.size();

    // As the batch does it: an equal request on the same network registers the earlier object again.
    corridor::Request second = first;
    second.exclude = {"b", "b"};
    second.skip_subtypes = {"mapping", "dissociation", "association", "component", "component"};
    REQUIRE(corridor::reusable(*c, *net.network, second));
    std::shared_ptr<const corridor::Corridor> registered = c.outcome.value;
    corridor::report_reuse(*registered, 4, 3, h.ctx);
    CHECK(registered.get() == c.outcome.value.get());
    CHECK(h.files.created.size() == files_after_first);   // a reused corridor writes no file
    CHECK(h.infos().back() == "Corridor, graph 4: same as graph 3");
    const auto events = h.events_of("corridor.summary");
    REQUIRE(events.size() == 2);
    CHECK(events[1]["graph"] == 4);
    CHECK(events[1]["same_as"] == 3);
    CHECK(events[1]["digest"] == c->digest().hex());

    // The same content built separately is the same network (by content hash).
    Harness h2;
    TestNetwork rebuilt(small(), h2.ctx);
    CHECK(corridor::reusable(*c, *rebuilt.network, second));
}

TEST_CASE("corridor: any changed argument or another network computes a new corridor", "[corridor][05.C4]") {
    Harness h;
    TestNetwork net(small(), h.ctx);
    Cut c(net, request("s", "t", 2), h);
    corridor::Request r = request("s", "t", 2);
    CHECK(corridor::reusable(*c, *net.network, r));
    r.bound = 3;
    CHECK_FALSE(corridor::reusable(*c, *net.network, r));
    r = request("s", "t", 2);
    r.edges_to_target = corridor::EdgesToTarget::expression;
    CHECK_FALSE(corridor::reusable(*c, *net.network, r));
    r = request("s", "t", 2);
    r.exclude = {"a"};
    CHECK_FALSE(corridor::reusable(*c, *net.network, r));
    r = request("s", "t", 2);
    r.skip_subtypes = {"component"};
    CHECK_FALSE(corridor::reusable(*c, *net.network, r));
    r = request("s", "b", 2);
    CHECK_FALSE(corridor::reusable(*c, *net.network, r));

    NetSpec other = small();
    other.edges.push_back(edge("a", "b"));
    TestNetwork net2(other, h.ctx);
    CHECK_FALSE(corridor::reusable(*c, *net2.network, request("s", "t", 2)));
}

TEST_CASE("corridor: the normal form sorts and deduplicates the lists and normalises subtype names", "[corridor][05.C4]") {
    corridor::Request r;
    r.source = {"s2", "s1", "s2"};
    r.target = {"t"};
    r.exclude = {"b", "B", "a"};
    r.skip_subtypes = {"mapping", "component", "mapping"};
    const corridor::Request n = corridor::normalised(r);
    CHECK(n.source == Strings{"s1", "s2"});
    CHECK(n.exclude == Strings{"B", "a", "b"});   // byte order
    CHECK(n.skip_subtypes == Strings{"component", "mapping"});
    CHECK(corridor::normalised(n) == n);
}

TEST_CASE("corridor: the settings are registered as corridor.<name> with 05.C8's names and defaults", "[corridor][05.C8]") {
    const auto specs = corridor::settings();
    Strings names;
    for (const core::SettingSpec& s : specs) {
        CHECK(s.module == "corridor");
        CHECK(core::key(s) == "corridor." + std::string(s.name));
        names.emplace_back(s.name);
    }
    CHECK(names == Strings{"graph", "source", "target", "exclude", "bound", "edges_to_target", "skip_subtypes",
                           "observations"});
    auto spec = [&](std::string_view name) -> const core::SettingSpec& {
        for (const core::SettingSpec& s : specs)
            if (s.name == name) return s;
        FAIL("no setting " << name);
        return specs.front();
    };
    CHECK(spec("graph").type == core::Type::integer);
    CHECK(spec("graph").default_text == "0");
    CHECK(spec("source").need == core::Need::required);
    CHECK(spec("target").need == core::Need::required);
    CHECK(spec("exclude").default_text == "");
    CHECK(spec("bound").need == core::Need::required);
    CHECK(spec("bound").min == 0);
    CHECK(spec("edges_to_target").type == core::Type::choice);
    CHECK(spec("edges_to_target").default_text == "any");
    REQUIRE(spec("edges_to_target").choices.size() == 3);
    CHECK(spec("edges_to_target").choices[1] == "activation");
    CHECK(spec("skip_subtypes").default_text == "component,association,dissociation,mapping");
    CHECK(spec("skip_subtypes").affects_results);
    CHECK_FALSE(spec("observations").affects_results);
    // The Request's own defaults agree with the registry's.
    const corridor::Request defaults;
    CHECK(defaults.skip_subtypes == Strings{"component", "association", "dissociation", "mapping"});
    CHECK(defaults.edges_to_target == corridor::EdgesToTarget::any);
}

TEST_CASE("corridor: corridor.json reports where the observed genes fall", "[corridor][05.C9][S4.16][S3.12]") {
    Harness h;
    NetSpec spec = small();
    spec.nodes.push_back({"x"});
    spec.edges.push_back(edge("x", "s"));
    TestNetwork net(spec, h.ctx);
    Cut c(net, request("s", "t", 2), h);

    h.files.write("up_reg", "a\nzz\nx\n");
    h.files.write("down_reg", "t\n");
    core::Interner run_strings;
    const expression::Observations observations = expression::load_lists(
        h.files.dir() / "up_reg", h.files.dir() / "down_reg", network::IdMap{}, run_strings, h.ctx);
    const std::size_t lines_before = h.infos().size();
    const auto matched = corridor::match_observations(*c, observations, h.ctx);
    CHECK(h.infos().size() == lines_before);   // the counts' console line is save_match()'s
    REQUIRE(matched.size() == 4);
    for (const auto& m : matched) {
        const std::string id(run_strings.str(m.id));
        if (id == "a" || id == "t") CHECK(m.group == expression::MatchGroup::in_corridor);
        if (id == "x") CHECK(m.group == expression::MatchGroup::outside_corridor);
        if (id == "zz") CHECK(m.group == expression::MatchGroup::not_in_network);
    }
    corridor::save(*c, matched, h.ctx);
    const core::Json j = core::Json::parse(h.files.read("corridor.json"));
    CHECK(j["observations"]["in_corridor"] == 2);        // a and t
    CHECK(j["observations"]["outside_corridor"] == 1);   // x: an edge into the source only
    CHECK(j["observations"]["not_in_network"] == 1);     // zz

    Harness none;
    TestNetwork net2(spec, none.ctx);
    Cut c2(net2, request("s", "t", 2), none);
    corridor::save(*c2, {}, none.ctx);   // a run without observations
    CHECK(core::Json::parse(none.files.read("corridor.json"))["observations"].is_null());
}

TEST_CASE("corridor: each id is bound once per run, through the run's table", "[corridor][S4.15]") {
    // FuSION 1's corridor rebuilt its own id lookup, so an id could bind elsewhere in pathz3 (S4.15).
    Harness h;
    TestNetwork net(small(), h.ctx);
    network::BindingTable table(net.network, network::Maps{});
    const auto first = corridor::compute(net.network, table, request("s", "t", 2), h.ctx);
    const auto second = corridor::compute(net.network, table, request("s", "t", 3), h.ctx);
    CHECK(table.counts("source").ids == 1);
    CHECK(table.counts("target").ids == 1);
    CHECK(first.value->bindings()[0].binding.node == second.value->bindings()[0].binding.node);
    CHECK(first.value->source() == net.node("s"));   // the network's own index (S4.14, 05.C1)
    CHECK(&first.value->network() == net.network.get());
}
