// The saved step (05.C6, decision 0019): corridor.json, corridor_nodes.tsv, the debug traces (S4.19),
// loading (S4.21) and byte-identical output from a network built in another order (section 09).
#include <filesystem>

#include "corridor_support.hpp"

using namespace corridor_test;
using Strings = std::vector<std::string>;

namespace {

NetSpec diamond_spec() {
    return {genes({"s", "a", "b", "t", "x"}),
            {},
            {edge("s", "a"), edge("s", "b", {"inhibition"}), edge("a", "t", {"expression"}), edge("b", "t"),
             edge("x", "t")}};
}

core::Json saved_json(const TempFiles& files) { return core::Json::parse(files.read("corridor.json")); }

std::string load_failure(TestNetwork& net, const std::filesystem::path& file, Harness& h) {
    network::BindingTable table(net.network, network::Maps{});
    try {
        (void)corridor::load(net.network, table, file, h.ctx);
    } catch (const core::Failure& f) {
        return f.code();
    }
    return "no failure";
}

}  // namespace

TEST_CASE("corridor: corridor.json holds the request, bindings, counts, nodes, edges and digest", "[corridor][05.C6][S4.21]") {
    Harness h;
    TestNetwork net(diamond_spec(), h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.skip_subtypes = {"mapping", "component", "association", "dissociation"};
    network::BindingTable table(net.network, network::Maps{});
    corridor::Naming naming;
    naming.network_graph = 2;
    naming.graph = 3;
    const auto out = corridor::compute(net.network, table, r, naming, h.ctx);
    corridor::save(*out.value, {}, naming, h.ctx);
    CHECK(h.files.created == Strings{"corridor.json", "corridor_nodes.tsv"});

    const core::Json j = saved_json(h.files);
    CHECK(j["format"] == "fusion-corridor");
    CHECK(j["format_version"] == 1);
    CHECK(j["graph"] == 3);
    CHECK(j["network"]["graph"] == 2);
    CHECK(j["network"]["content_hash"] == net.network->content_hash().hex());
    CHECK(j["network"]["nodes"] == 5);
    CHECK(j["network"]["edges"] == 5);
    CHECK(j["request"]["source"] == Strings{"s"});
    CHECK(j["request"]["exclude"] == Strings{});
    CHECK(j["request"]["bound"] == 2);
    CHECK(j["request"]["edges_to_target"] == "any");
    CHECK(j["request"]["skip_subtypes"] == Strings{"association", "component", "dissociation", "mapping"});
    REQUIRE(j["bindings"].size() == 2);
    CHECK(j["bindings"][0]["role"] == "source");
    CHECK(j["bindings"][0]["given"] == "s");
    CHECK(j["bindings"][0]["node"] == "s");
    CHECK(j["bindings"][0]["status"] == "own");
    CHECK(j["bindings"][1]["role"] == "target");
    CHECK(j["excluded_nodes"].empty());
    CHECK(j["status"] == "ok");
    CHECK(j["empty_reason"].is_null());
    CHECK(j["shortest_path"] == 2);
    CHECK(j["counts"]["traversable_edges"] == 5);
    CHECK(j["counts"]["into_target"] == 2);   // x -> t: x is not reachable from s
    CHECK(j["counts"]["forward_set"] == 4);
    CHECK(j["counts"]["backward_set"] == 4);
    CHECK(j["counts"]["nodes"] == 4);
    CHECK(j["counts"]["edges"] == 4);
    CHECK(j["observations"].is_null());
    REQUIRE(j["nodes"].size() == 4);
    CHECK(j["nodes"][0]["key"] == "a");       // the network's index order (02.C10)
    CHECK(j["nodes"][0]["type"] == "gene");
    CHECK(j["nodes"][0]["genes"] == 1);
    CHECK(j["nodes"][0]["ds"] == 1);
    CHECK(j["nodes"][0]["dt"] == 1);
    CHECK(j["nodes"][2]["key"] == "s");
    REQUIRE(j["edges"].size() == 4);
    CHECK(j["edges"][0]["source"] == "a");
    CHECK(j["edges"][0]["target"] == "t");
    CHECK(j["edges"][0]["type"] == "PPrel");
    CHECK(j["edges"][0]["subtypes"] == Strings{"expression"});
    CHECK(j["edges"][0]["pathways"] == Strings{"path:hsa04010"});
    CHECK(j["digest"] == out.value->digest().hex());
    CHECK(j["digest"].get<std::string>().size() == 64);

    const std::string text = h.files.read("corridor.json");
    CHECK(text.substr(0, 2) == "{\n");
    CHECK(text.find('\r') == std::string::npos);
    CHECK(text.back() == '\n');
}

TEST_CASE("corridor: corridor_nodes.tsv reads from the source outwards", "[corridor][05.C6][S4.07]") {
    Harness h;
    TestNetwork net({genes({"s", "a", "b", "c", "t"}),
                     {},
                     {edge("s", "c"), edge("s", "b"), edge("c", "t"), edge("b", "a"), edge("a", "t")}},
                    h.ctx);
    Cut c(net, request("s", "t", 3), h);
    corridor::save(*c, {}, h.ctx);
    CHECK(h.files.read("corridor_nodes.tsv") ==
          "key\tlabel\ttype\tgenes\tds\tdt\n"
          "s\ts\tgene\t1\t0\t2\n"
          "c\tc\tgene\t1\t1\t1\n"
          "b\tb\tgene\t1\t1\t2\n"
          "t\tt\tgene\t1\t2\t0\n"
          "a\ta\tgene\t1\t2\t1\n");
}

TEST_CASE("corridor: labels come from the run's symbol map when the batch passes it", "[corridor][07.C10]") {
    Harness h;
    TestNetwork net(diamond_spec(), h.ctx);
    h.files.write("symbols.tsv", "s\tEGFR\nt\tCXCL8\n");
    network::Maps maps;
    maps.symbols = network::LabelMap::read(h.files.dir() / "symbols.tsv", h.ctx);
    network::BindingTable table(net.network, maps);
    corridor::Naming naming;
    naming.maps = &maps;
    const auto out = corridor::compute(net.network, table, request("s", "t", 3), naming, h.ctx);
    CHECK(h.infos().front() == "Corridor from EGFR (s) to CXCL8 (t), bound 3, edges_to_target any");
    corridor::save(*out.value, {}, naming, h.ctx);
    const core::Json j = saved_json(h.files);
    CHECK(j["bindings"][0]["label"] == "EGFR");
    CHECK(j["nodes"][2]["label"] == "EGFR");
    CHECK(j["nodes"][0]["label"] == "a");

    Harness h3;
    network::BindingTable table3(net.network, maps);
    const auto empty = corridor::compute(net.network, table3, request("s", "t", 1), naming, h3.ctx);
    CHECK(empty.report->message == "no path from EGFR (s) to CXCL8 (t) within bound 1: the shortest path has 2 edges");
}

TEST_CASE("corridor: an empty corridor is saved with its reason", "[corridor][05.C6][S4.18]") {
    Harness h;
    TestNetwork net(diamond_spec(), h.ctx);
    Cut c(net, request("s", "t", 1), h);
    corridor::save(*c, {}, h.ctx);
    const core::Json j = saved_json(h.files);
    CHECK(j["status"] == "empty");
    CHECK(j["empty_reason"] == "beyond_bound");
    CHECK(j["shortest_path"] == 2);
    CHECK(j["nodes"].empty());
    CHECK(j["edges"].empty());
    CHECK(j["counts"]["nodes"] == 0);
}

TEST_CASE("corridor: traces are written only under the debug option, naming nodes by key", "[corridor][S4.19]") {
    SECTION("debug") {
        Harness h(true);
        TestNetwork net(diamond_spec(), h.ctx);
        Cut c(net, request("s", "t", 2), h);
        CHECK(h.files.created.empty());   // compute() writes no file, even under debug (section 05's hooks)
        corridor::save(*c, {}, h.ctx);
        CHECK(h.files.created == Strings{"corridor.json", "corridor_nodes.tsv", "debug/forward.tsv", "debug/backward.tsv"});
        CHECK(h.files.read("debug/forward.tsv") ==
              "order\tnode\tdistance\tqueued\n"
              "0\ts\t0\ta,b\n"
              "1\ta\t1\tt\n"
              "2\tb\t1\t\n"
              "3\tt\t2\t\n");
        CHECK(h.files.read("debug/backward.tsv") ==
              "order\tnode\tdistance\tqueued\n"
              "0\tt\t0\ta,b\n"
              "1\ta\t1\ts\n"
              "2\tb\t1\t\n"
              "3\ts\t2\t\n");
    }
    SECTION("no debug") {
        Harness h(false);
        TestNetwork net(diamond_spec(), h.ctx);
        Cut c(net, request("s", "t", 2), h);
        corridor::save(*c, {}, h.ctx);
        CHECK(h.files.created == Strings{"corridor.json", "corridor_nodes.tsv"});
        CHECK_FALSE(std::filesystem::exists(h.files.dir() / "debug"));
    }
}

TEST_CASE("corridor: a saved corridor loads with the same digest, without rerunning the search", "[corridor][05.C6][0019]") {
    Harness h;
    TestNetwork net(diamond_spec(), h.ctx);
    corridor::Request r = request("s", "t", 2);
    r.exclude = {"x"};
    Cut c(net, r, h);
    corridor::save(*c, {}, h.ctx);

    network::BindingTable table(net.network, network::Maps{});
    const auto loaded = corridor::load(net.network, table, h.files.dir() / "corridor.json", h.ctx);
    CHECK(loaded->digest() == c->digest());
    CHECK(edge_names(*loaded) == edge_names(*c));
    CHECK(node_names(*loaded) == node_names(*c));
    CHECK(loaded->source() == c->source());
    CHECK(loaded->target() == c->target());
    CHECK(loaded->bound() == 2);
    CHECK(loaded->ds(net.node("b")) == 1);
    CHECK(loaded->dt(net.node("s")) == 2);
    CHECK(loaded->counts().forward_set == c->counts().forward_set);
    REQUIRE(loaded->excluded().size() == 1);
    CHECK(loaded->excluded()[0].node == net.node("x"));
    CHECK(loaded->bindings().size() == 3);
    CHECK(corridor::check_invariants(*loaded).empty());

    Cut empty(net, request("s", "t", 1), h);
    Harness h2;
    corridor::save(*empty, {}, h2.ctx);
    network::BindingTable table2(net.network, network::Maps{});
    const auto loaded_empty = corridor::load(net.network, table2, h2.files.dir() / "corridor.json", h2.ctx);
    CHECK(loaded_empty->empty_reason() == corridor::EmptyReason::beyond_bound);
    CHECK(loaded_empty->shortest_path() == 2u);
    CHECK(loaded_empty->digest() == empty->digest());
}

TEST_CASE("corridor: loading checks the network, the bindings, the format and the digest", "[corridor][05.C6]") {
    Harness h;
    TestNetwork net(diamond_spec(), h.ctx);
    Cut c(net, request("s", "t", 2), h);
    corridor::save(*c, {}, h.ctx);
    const std::string text = h.files.read("corridor.json");
    const auto path = h.files.dir() / "edited.json";
    auto edited = [&](const std::string& from, const std::string& to) {
        std::string copy = text;
        const auto at = copy.find(from);
        REQUIRE(at != std::string::npos);
        copy.replace(at, from.size(), to);
        h.files.write("edited.json", copy);
    };

    SECTION("another network") {
        NetSpec other = diamond_spec();
        other.edges.push_back(edge("a", "b"));
        TestNetwork net2(other, h.ctx);
        CHECK(load_failure(net2, h.files.dir() / "corridor.json", h) == "corridor.network_mismatch");
    }
    SECTION("a binding that now names another node") {
        h.files.write("ids.txt", "s a\n");
        network::Maps maps;
        maps.ids = network::IdMap::read(h.files.dir() / "ids.txt", h.ctx);
        network::BindingTable mapped(net.network, maps);
        try {
            (void)corridor::load(net.network, mapped, h.files.dir() / "corridor.json", h.ctx);
            FAIL("no failure");
        } catch (const core::Failure& f) {
            CHECK(f.code() == "corridor.network_mismatch");
        }
    }
    SECTION("a newer format") {
        edited("\"format_version\": 1", "\"format_version\": 2");
        CHECK(load_failure(net, path, h) == "artefact.newer_format");
    }
    SECTION("changed content") {
        edited("\"ds\":1", "\"ds\":0");
        CHECK(load_failure(net, path, h) == "artefact.changed");
    }
    SECTION("not JSON") {
        h.files.write("edited.json", "{\"format\": ");
        CHECK(load_failure(net, path, h) == "input.unreadable");
    }
    SECTION("not a corridor") {
        h.files.write("edited.json", "{\"format\": \"fusion-network\"}");
        CHECK(load_failure(net, path, h) == "input.unreadable");
    }
    SECTION("a missing file") {
        CHECK(load_failure(net, h.files.dir() / "none.json", h) == "input.unreadable");
    }
}

TEST_CASE("corridor: the same network built in another order gives byte-identical files", "[corridor][determinism]") {
    NetSpec spec = diamond_spec();
    spec.edges.push_back(edge("a", "b"));
    spec.edges.push_back(edge("b", "a", {"inhibition", "phosphorylation"}));
    NetSpec reversed = spec;
    std::reverse(reversed.nodes.begin(), reversed.nodes.end());
    std::reverse(reversed.edges.begin(), reversed.edges.end());

    Harness h1, h2;
    TestNetwork net1(spec, h1.ctx), net2(reversed, h2.ctx);
    REQUIRE(net1.network->content_hash() == net2.network->content_hash());
    corridor::Request r1 = request("s", "t", 3);
    r1.exclude = {"x", "b"};
    corridor::Request r2 = r1;
    r2.exclude = {"b", "x", "b"};
    Cut c1(net1, r1, h1), c2(net2, r2, h2);
    CHECK(c1->digest() == c2->digest());
    corridor::save(*c1, {}, h1.ctx);
    corridor::save(*c2, {}, h2.ctx);
    CHECK(h1.files.read("corridor.json") == h2.files.read("corridor.json"));
    CHECK(h1.files.read("corridor_nodes.tsv") == h2.files.read("corridor_nodes.tsv"));
}
