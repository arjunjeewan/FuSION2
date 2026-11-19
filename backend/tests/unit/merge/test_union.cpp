// The explicit, order-independent union (S2.17, 03 "The union"): keys, S2.08, exceptions, thresholds,
// complexes, edges, filters, type conflicts and duplicate pathways, on synthetic pathways.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "support.hpp"

using namespace merge_test;
using fusion::merge::merge_direct;
using fusion::network::BuildSettings;
using fusion::network::EdgeFlag;
using fusion::network::NodeFlag;
using Catch::Matchers::ContainsSubstring;

namespace {

std::shared_ptr<const fusion::network::Network> direct(const std::vector<DocPtr>& docs,
                                                       const fusion::merge::ExceptionSet& e = {}, int threshold = 1) {
    BuildSettings s;
    s.split_threshold = threshold;
    return merge_direct(docs, s, e);
}

}  // namespace

TEST_CASE("boxes that share an id are unioned across files, keyed by the smallest member", "[unit][merge][S2.17]") {
    Context c;
    const auto a = read(c, Kgml("path:hsa00001").entry(1, "hsa:1 hsa:2").entry(2, "hsa:9").rel(1, 2, {"activation"}));
    const auto b = read(c, Kgml("path:hsa00002").entry(1, "hsa:2 hsa:3").entry(2, "hsa:9").rel(1, 2, {"activation"}));
    const auto d = read(c, Kgml("path:hsa00003").entry(1, "hsa:3 hsa:10").entry(2, "hsa:9").rel(2, 1, {"inhibition"}));
    const auto net = direct({a, b, d});
    // A chain of overlapping boxes (the shape that crashed FuSION 1, S2.18) is one node; its key is its
    // smallest member in byte order (S2.26): "hsa1" < "hsa10" < "hsa2".
    CHECK(member_sets(*net) == std::vector<std::string>{"hsa1 hsa10 hsa2 hsa3", "hsa9"});
    REQUIRE(node(*net, "hsa1"));
    CHECK(has(net->nodes()[*node(*net, "hsa1")].flags, NodeFlag::family));
    REQUIRE(net->edges().size() == 2);
    const auto* act = edge(*net, "hsa1", "hsa9", "PPrel", {"activation"});
    REQUIRE(act != nullptr);
    CHECK(pathways(*net, *act) == std::vector<std::string>{"path:hsa00001", "path:hsa00002"});
    CHECK(edge(*net, "hsa9", "hsa1", "PPrel", {"inhibition"}) != nullptr);
}

TEST_CASE("a single KGML is a one-file merge: boxes sharing an id in one pathway share a node", "[unit][merge][03.C5]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00010")
                                 .entry(1, "hsa:1 hsa:2")
                                 .entry(2, "hsa:2 hsa:3")
                                 .entry(3, "hsa:7")
                                 .rel(1, 3, {"activation"})
                                 .rel(2, 3, {"activation"}));
    const auto net = direct({doc});
    CHECK(member_sets(*net) == std::vector<std::string>{"hsa1 hsa2 hsa3", "hsa7"});
    CHECK(net->edges().size() == 1);   // both relations give the same edge
}

TEST_CASE("S2.08: a later box whose key names an existing node keeps its other genes", "[unit][merge][S2.08]") {
    // hsa05220's AML1-EVI1 box: entry 16 {MECOM} is reached first, entry 5 {MECOM, RUNX1} later. FuSION 1
    // keyed entry 5 to the existing node and dropped RUNX1 (Linux/src/XMLParser.cc:991-1073).
    Context c;
    const auto doc = read(c, Kgml("path:hsa05220")
                                 .entry(16, "hsa:2122")
                                 .entry(5, "hsa:2122 hsa:861")
                                 .entry(20, "hsa:999")
                                 .rel(16, 20, {"activation"})
                                 .rel(5, 20, {"expression"}));
    const auto net = direct({doc});
    CHECK(member_sets(*net) == std::vector<std::string>{"hsa2122 hsa861", "hsa999"});
    CHECK(edge(*net, "hsa2122", "hsa999", "PPrel", {"expression"}) != nullptr);
}

TEST_CASE("exception ids get nodes of their own and break the bridges they made", "[unit][merge][S2.13][S2.07]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00020")
                                 .entry(1, "hsa:1 hsa:2")
                                 .entry(2, "hsa:2 hsa:3 hsa:4")
                                 .entry(3, "hsa:9")
                                 .rel(1, 3, {"activation"})
                                 .rel(2, 3, {"activation"}));
    SECTION("without exceptions the two boxes are one family node") {
        CHECK(member_sets(*direct({doc})) == std::vector<std::string>{"hsa1 hsa2 hsa3 hsa4", "hsa9"});
    }
    SECTION("with hsa2 in E the bridge breaks, and the exception node is marked") {
        const auto net = direct({doc}, exceptions({"hsa2"}));
        CHECK(member_sets(*net) == std::vector<std::string>{"hsa1", "hsa2", "hsa3 hsa4", "hsa9"});
        CHECK(has(net->nodes()[*node(*net, "hsa2")].flags, NodeFlag::exception));
        CHECK_FALSE(has(net->nodes()[*node(*net, "hsa1")].flags, NodeFlag::exception));
        // Each relation fans out over its box's nodes (S2.10): hsa1, hsa2 and the {hsa3, hsa4} node.
        CHECK(edge(*net, "hsa1", "hsa9", "PPrel", {"activation"}) != nullptr);
        CHECK(edge(*net, "hsa2", "hsa9", "PPrel", {"activation"}) != nullptr);
        CHECK(edge(*net, "hsa3", "hsa9", "PPrel", {"activation"}) != nullptr);
        CHECK(net->edges().size() == 3);
    }
    SECTION("an exception id that is in no box changes nothing") {
        CHECK(member_sets(*direct({doc}, exceptions({"hsa77"}))) ==
              std::vector<std::string>{"hsa1 hsa2 hsa3 hsa4", "hsa9"});
    }
}

TEST_CASE("the split threshold: more ids than the threshold collapse, counting exception ids", "[unit][merge][S2.07]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00030")
                                 .entry(1, "hsa:1 hsa:2")
                                 .entry(2, "hsa:2 hsa:3 hsa:4")
                                 .entry(3, "hsa:9")
                                 .rel(1, 3, {"activation"})
                                 .rel(2, 3, {"activation"}));
    SECTION("1 and 0 collapse every box of two or more ids") {
        const auto one = member_sets(*direct({doc}, {}, 1));
        CHECK(one == std::vector<std::string>{"hsa1 hsa2 hsa3 hsa4", "hsa9"});
        CHECK(member_sets(*direct({doc}, {}, 0)) == one);
    }
    SECTION("2 leaves the two-id box apart, so its ids join nodes one by one") {
        CHECK(member_sets(*direct({doc}, {}, 2)) == std::vector<std::string>{"hsa1", "hsa2 hsa3 hsa4", "hsa9"});
    }
    SECTION("at 2, an exception id still counts towards the box's size") {
        // The three-id box holds more than 2 ids even with hsa2 in E, so hsa3 and hsa4 still collapse.
        CHECK(member_sets(*direct({doc}, exceptions({"hsa2"}), 2)) ==
              std::vector<std::string>{"hsa1", "hsa2", "hsa3 hsa4", "hsa9"});
    }
    SECTION("a negative threshold and a large one never collapse") {
        const std::vector<std::string> singles{"hsa1", "hsa2", "hsa3", "hsa4", "hsa9"};
        CHECK(member_sets(*direct({doc}, {}, -1)) == singles);
        CHECK(member_sets(*direct({doc}, {}, 15000)) == singles);
    }
}

TEST_CASE("complexes: one per combination of the components' nodes, with two-way component edges",
          "[unit][merge][S2.09]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00040", "Complex pathway")
                                 .entry(1, "hsa:1")
                                 .entry(2, "hsa:2 hsa:3")
                                 .group(3, {1, 2})
                                 .entry(4, "hsa:4")
                                 .rel(3, 4, {"activation"}, "PPrel", "500"));
    SECTION("the box collapses, so the group makes one complex") {
        const auto net = direct({doc});
        REQUIRE(node(*net, "c_s_hsa1_hsa2_c_e"));
        const auto cx = *node(*net, "c_s_hsa1_hsa2_c_e");
        CHECK(net->kind(cx) == fusion::network::NodeKind::complex);
        CHECK(net->components(cx).size() == 2);
        CHECK(edge(*net, "c_s_hsa1_hsa2_c_e", "hsa4", "PPrel", {"activation"}) != nullptr);
        // Component edges both ways, the tag on both, the group's own pathway (S2.09's fix).
        for (const char* k : {"hsa1", "hsa2"}) {
            const auto* in = edge(*net, k, "c_s_hsa1_hsa2_c_e", "PPrel", {"component"});
            const auto* out = edge(*net, "c_s_hsa1_hsa2_c_e", k, "PPrel", {"component"});
            REQUIRE(in != nullptr);
            REQUIRE(out != nullptr);
            CHECK(has(in->flags, EdgeFlag::component));
            CHECK(has(in->flags, EdgeFlag::dummy_u_to_d));
            CHECK(has(out->flags, EdgeFlag::dummy_u_to_d));
            CHECK(pathways(*net, *in) == std::vector<std::string>{"path:hsa00040"});
        }
        CHECK(net->edges().size() == 5);
    }
    SECTION("an exception id in the box makes the Cartesian product") {
        const auto net = direct({doc}, exceptions({"hsa3"}));
        CHECK(node(*net, "c_s_hsa1_hsa2_c_e"));
        CHECK(node(*net, "c_s_hsa1_hsa3_c_e"));
        CHECK(edge(*net, "c_s_hsa1_hsa2_c_e", "hsa4", "PPrel", {"activation"}) != nullptr);
        CHECK(edge(*net, "c_s_hsa1_hsa3_c_e", "hsa4", "PPrel", {"activation"}) != nullptr);
    }
}

TEST_CASE("a complex whose components fall into one node keeps one component; groups of groups",
          "[unit][merge][S2.09]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00050")
                                 .entry(1, "hsa:1 hsa:2")
                                 .entry(2, "hsa:2 hsa:5")
                                 .group(3, {1, 2})
                                 .entry(4, "hsa:7")
                                 .group(5, {3, 4})
                                 .entry(6, "hsa:8")
                                 .rel(3, 6, {"activation"})
                                 .rel(5, 6, {"inhibition"}));
    const auto net = direct({doc});
    REQUIRE(node(*net, "c_s_hsa1_c_e"));
    CHECK(net->components(*node(*net, "c_s_hsa1_c_e")).size() == 1);
    // The outer group's complex has the inner complex and hsa7 as components.
    REQUIRE(node(*net, "c_s_c_s_hsa1_c_e_hsa7_c_e"));
    CHECK(edge(*net, "c_s_c_s_hsa1_c_e_hsa7_c_e", "hsa8", "PPrel", {"inhibition"}) != nullptr);
    CHECK(edge(*net, "c_s_hsa1_c_e", "c_s_c_s_hsa1_c_e_hsa7_c_e", "PPrel", {"component"}) != nullptr);
}

TEST_CASE("relations: undirected pairs tagged once, self-loops, and repeats merged", "[unit][merge][S2.10][S2.20]") {
    Context c;
    SECTION("an association adds the reverse edge; the tag is on the direction whose source key is larger") {
        const auto doc = read(c, Kgml("path:hsa00060").entry(1, "hsa:5").entry(2, "hsa:40").rel(1, 2, {"binding/association"}));
        const auto net = direct({doc});
        const auto* fwd = edge(*net, "hsa5", "hsa40", "PPrel", {"association"});
        const auto* back = edge(*net, "hsa40", "hsa5", "PPrel", {"association"});
        REQUIRE(fwd != nullptr);
        REQUIRE(back != nullptr);
        CHECK(has(fwd->flags, EdgeFlag::undirected));
        CHECK(has(back->flags, EdgeFlag::undirected));
        CHECK(has(fwd->flags, EdgeFlag::dummy_u_to_d));    // "hsa5" > "hsa40" in byte order
        CHECK_FALSE(has(back->flags, EdgeFlag::dummy_u_to_d));
    }
    SECTION("a relation between two entries of one node is a self-loop: no reverse edge, no tag") {
        const auto doc = read(c, Kgml("path:hsa00061").entry(1, "hsa:1 hsa:2").entry(2, "hsa:2").rel(1, 2, {"association"}));
        const auto net = direct({doc});
        REQUIRE(net->edges().size() == 1);
        const auto& e = net->edges()[0];
        CHECK(e.src == e.tgt);
        CHECK(has(e.flags, EdgeFlag::self_loop));
        CHECK_FALSE(has(e.flags, EdgeFlag::dummy_u_to_d));
    }
    SECTION("one interaction in two pathways is one edge: pathways unioned, the larger score kept") {
        const auto a = read(c, Kgml("path:hsa04010").entry(45, "hsa:5906 hsa:5908").entry(41, "hsa:673").rel(45, 41, {"activation"}, "PPrel", "625"));
        const auto b = read(c, Kgml("path:hsa04722").entry(57, "hsa:5906 hsa:5908").entry(59, "hsa:673").rel(57, 59, {"activation"}, "PPrel", "700"));
        const auto net = direct({a, b});
        const auto* e = edge(*net, "hsa5906", "hsa673", "PPrel", {"activation"});
        REQUIRE(e != nullptr);
        CHECK(e->score == 700);
        CHECK(pathways(*net, *e) == std::vector<std::string>{"path:hsa04010", "path:hsa04722"});
        CHECK_FALSE(has(e->flags, EdgeFlag::unscored));
    }
    SECTION("an unscored relation merged with a scored one is scored") {
        const auto a = read(c, Kgml("path:hsa00070").entry(1, "hsa:1").entry(2, "hsa:2").rel(1, 2, {"activation"}));
        const auto b = read(c, Kgml("path:hsa00071").entry(1, "hsa:1").entry(2, "hsa:2").rel(1, 2, {"activation"}, "PPrel", "5"));
        CHECK(has(direct({a})->edges()[0].flags, EdgeFlag::unscored));
        const auto net = direct({a, b});
        CHECK_FALSE(has(net->edges()[0].flags, EdgeFlag::unscored));
        CHECK(net->edges()[0].score == 5);
    }
    SECTION("subtypes differ: activation and activation+phosphorylation stay two edges (S2.04)") {
        const auto doc = read(c, Kgml("path:hsa00072")
                                     .entry(1, "hsa:1")
                                     .entry(2, "hsa:2")
                                     .rel(1, 2, {"activation"})
                                     .rel(1, 2, {"activation", "phosphorylation"}));
        CHECK(direct({doc})->edges().size() == 2);
    }
}

TEST_CASE("S2.21: one association from two pathways is one edge per direction, tagged once on the merged keys",
          "[unit][merge][S2.21]") {
    // In FuSION 1 the pair's key order differed between the pathways, so one direction survived twice,
    // tagged and untagged. In 2.0 the tag is set once, on the merged graph.
    Context c;
    const auto a = read(c, Kgml("path:hsa00080").entry(1, "hsa:5").entry(2, "hsa:40").rel(1, 2, {"association"}));
    const auto b = read(c, Kgml("path:hsa00081").entry(1, "hsa:30 hsa:5").entry(2, "hsa:40").rel(1, 2, {"association"}));
    const auto net = direct({a, b});
    REQUIRE(net->edges().size() == 2);
    const auto* up = edge(*net, "hsa30", "hsa40", "PPrel", {"association"});
    const auto* down = edge(*net, "hsa40", "hsa30", "PPrel", {"association"});
    REQUIRE(up != nullptr);
    REQUIRE(down != nullptr);
    CHECK_FALSE(has(up->flags, EdgeFlag::dummy_u_to_d));
    CHECK(has(down->flags, EdgeFlag::dummy_u_to_d));
    CHECK(pathways(*net, *up) == std::vector<std::string>{"path:hsa00080", "path:hsa00081"});
}

TEST_CASE("relation filters act before nodes are made", "[unit][merge][S2.11][S2.31]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00090")
                                 .entry(1, "hsa:1")
                                 .entry(2, "hsa:2")
                                 .entry(3, "hsa:3")
                                 .entry(4, "hsa:1 hsa:9")
                                 .entry(5, "cpd:C00076", "compound")
                                 .rel(1, 2, {"activation"})
                                 .rel(2, 3, {"indirect effect"})
                                 .rel(4, 2, {"activation", "indirect effect"})
                                 .rel(2, 5, {"compound"}, "ECrel"));
    SECTION("indirect relations are discarded by default: boxes they alone reach make no nodes and join no unions") {
        const auto net = direct({doc});
        CHECK(member_sets(*net) == std::vector<std::string>{"cpdC00076", "hsa1", "hsa2"});
        const auto* ec = edge(*net, "hsa2", "cpdC00076", "ECrel", {"compound"});
        REQUIRE(ec != nullptr);
        CHECK(has(ec->flags, EdgeFlag::ecrel));
    }
    SECTION("indirect=keep keeps them, flagged") {
        BuildSettings s;
        s.filters.indirect = fusion::network::IndirectMode::keep;
        const auto net = merge_direct(std::vector<DocPtr>{doc}, s, {});
        CHECK(member_sets(*net) == std::vector<std::string>{"cpdC00076", "hsa1 hsa9", "hsa2", "hsa3"});
        const auto* e = edge(*net, "hsa2", "hsa3", "PPrel", {"indirect"});
        REQUIRE(e != nullptr);
        CHECK(has(e->flags, EdgeFlag::indirect));
    }
    SECTION("ecrel=exclude drops ECrel relations, and the entries only they reach") {
        BuildSettings s;
        s.filters.ecrel = fusion::network::EcrelMode::exclude;
        CHECK(member_sets(*merge_direct(std::vector<DocPtr>{doc}, s, {})) == std::vector<std::string>{"hsa1", "hsa2"});
    }
}

TEST_CASE("no node is isolated: entries no kept relation reaches make no nodes", "[unit][merge][S2.06]") {
    Context c;
    const auto doc = read(c, Kgml("path:hsa00100").entry(1, "hsa:1").entry(2, "hsa:2").entry(3, "hsa:3").rel(1, 2, {"activation"}));
    const auto net = direct({doc});
    CHECK(member_sets(*net) == std::vector<std::string>{"hsa1", "hsa2"});
    for (fusion::network::NodeIdx n = 0; n < net->nodes().size(); ++n)
        CHECK(net->out_edges(n).size() + net->in_edges(n).size() > 0);
}

TEST_CASE("a node-type conflict stops the run, naming the id, both types and both boxes", "[unit][merge][S2.19]") {
    Context c;
    const auto a = read(c, Kgml("path:hsa04020").entry(35, "cpd:C00076", "compound").entry(36, "hsa:5530").rel(35, 36, {"activation"}),
                        "hsa04020.xml");
    const auto b = read(c, Kgml("path:hsa00002").entry(12, "cpd:C00076").entry(13, "hsa:1").rel(12, 13, {"activation"}), "my.xml");
    const std::string expected =
        "node-type conflict: cpdC00076 is a gene in path:hsa00002 (my.xml, entry 12) and a compound in path:hsa04020 "
        "(hsa04020.xml, entry 35)";
    for (const auto& docs : {std::vector<DocPtr>{a, b}, std::vector<DocPtr>{b, a}}) {   // the same whatever the order
        try {
            direct(docs);
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "merge.type_conflict");
            CHECK(std::string(f.what()) == expected);
        }
    }
    SECTION("the base merge fails the same way, so a stored merge never fails later") {
        try {
            fusion::merge::build_base_merge({a, b}, BuildSettings{}, fusion::merge::MergeKey{}, c.ctx);
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "merge.type_conflict");
        }
    }
    SECTION("within one file too") {
        const auto one = read(c, Kgml("path:hsa00003").entry(1, "hsa:1").entry(2, "hsa:1", "compound").rel(1, 2, {"activation"}));
        CHECK_THROWS_AS(direct({one}), fusion::core::Failure);
    }
}

TEST_CASE("two files with one pathway id fail, naming both; one file listed twice is merged once",
          "[unit][merge][03.C6]") {
    Context c;
    const Kgml k = Kgml("path:hsa04010").entry(1, "hsa:1").entry(2, "hsa:2").rel(1, 2, {"activation"});
    const auto a = read(c, k, "a/hsa04010.xml");
    SECTION("listed twice") {
        const auto copy = read(c, k, "b/hsa04010.xml");   // the same bytes elsewhere
        CHECK(dump(*direct({a, a})) == dump(*direct({a})));
        CHECK(dump(*direct({a, copy})) == dump(*direct({a})));
    }
    SECTION("another file declaring the same pathway") {
        const auto other =
            read(c, Kgml("path:hsa04010").entry(1, "hsa:1").entry(2, "hsa:3").rel(1, 2, {"activation"}), "b/hsa04010.xml");
        try {
            direct({a, other});
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "list.duplicate_pathway");
            CHECK_THAT(std::string(f.what()), ContainsSubstring("a/hsa04010.xml") && ContainsSubstring("b/hsa04010.xml"));
        }
    }
}

TEST_CASE("an empty merge is an empty network", "[unit][merge]") {
    const auto net = direct({});
    CHECK(net->nodes().empty());
    CHECK(net->edges().empty());
}

TEST_CASE("edge tables join the union like KGMLs; a conflict names the table's line", "[unit][merge][02.C2][S2.19]") {
    Context c;
    const auto kgml = read(c, Kgml("path:hsa04020").entry(35, "cpd:C00076", "compound").entry(36, "hsa:5530 hsa:5531").rel(35, 36, {"activation"}),
                           "hsa04020.xml");
    SECTION("a box in a table cell unions with a KGML box that shares an id") {
        const auto table = fusion::network::read_edge_table(
            c.scratch.write("in/my_edges.tsv", "source\ttarget\ttype\tsubtypes\nhsa5531,hsa5532\thsa9\tPPrel\tactivation\n"),
            fusion::network::IdMap::none(), c.ctx);
        const auto net = direct({kgml, table});
        CHECK(member_sets(*net) == std::vector<std::string>{"cpdC00076", "hsa5530 hsa5531 hsa5532", "hsa9"});
        const auto* e = edge(*net, "hsa5530", "hsa9", "PPrel", {"activation"});
        REQUIRE(e != nullptr);
        CHECK(pathways(*net, *e) == std::vector<std::string>{"my_edges"});   // the table's stem (02.C2)
    }
    SECTION("an edge table without a type column gives its endpoints the type gene") {
        const auto table = fusion::network::read_edge_table(
            c.scratch.write("in/my_edges.tsv", "# calcium, named by hand\nsource\ttarget\nhsa1\tcpdC00076\n"),
            fusion::network::IdMap::none(), c.ctx);
        try {
            direct({kgml, table});
            FAIL("no failure");
        } catch (const fusion::core::Failure& f) {
            CHECK(f.code() == "merge.type_conflict");
            CHECK(std::string(f.what()) ==
                  "node-type conflict: cpdC00076 is a gene in my_edges (my_edges.tsv, line 3) and a compound in "
                  "path:hsa04020 (hsa04020.xml, entry 35)");
            CHECK(f.where().file.ends_with("hsa04020.xml"));
        }
    }
}
