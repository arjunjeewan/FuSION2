// Merge once, split afterwards (S2.22; section 03, "Merge once, split afterwards"; 12.C14): the base merge,
// its split, and the split's equality with merge_direct on worked examples, on every exception subset of
// small sets, at thresholds 0, 1, 2, 3 and 15000, and on random sets.
#include <catch2/catch_test_macros.hpp>

#include "generator.hpp"
#include "support.hpp"

using namespace merge_test;
using fusion::merge::build_base_merge;
using fusion::merge::ExceptionSet;
using fusion::merge::merge_direct;
using fusion::merge::MergeKey;
using fusion::merge::split;
using fusion::network::BuildSettings;

namespace {

BuildSettings threshold(int t) {
    BuildSettings s;
    s.split_threshold = t;
    return s;
}

// split(base(R), E) and merge_direct(R, E) give the same model, exception marks included.
void check_split(const fusion::merge::BaseMerge& base, const std::vector<DocPtr>& docs, const ExceptionSet& e) {
    const auto m = split(base, e);
    const auto d = merge_direct(docs, base.settings(), e);
    INFO("E = " << [&] {
        std::string s;
        for (const auto& id : e.ids().ids()) s += id + " ";
        return s;
    }() << " threshold " << base.settings().split_threshold);
    REQUIRE(dump(*m.network) == dump(*d));
    REQUIRE(m.network->content_hash() == d->content_hash());
}

// The section 03 example: AP-1 {FOS, JUN}, JNK {MAPK8, MAPK9, MAPK10}, hsa05161's {JUN, MAPK8, MAPK9,
// MAPK10} and hsa05224's {FOS, JUN, SP1} make one base node keyed hsa2353 (FOS).
std::vector<DocPtr> ap1_jnk(Context& c) {
    return {read(c, Kgml("path:hsa04010", "MAPK signaling pathway")
                        .entry(1, "hsa:2353 hsa:3725")
                        .entry(2, "hsa:5599 hsa:5601 hsa:5602")
                        .entry(3, "hsa:1")
                        .rel(2, 1, {"phosphorylation"})
                        .rel(1, 3, {"expression"}, "GErel")),
            read(c, Kgml("path:hsa05161").entry(1, "hsa:3725 hsa:5599 hsa:5601 hsa:5602").entry(2, "hsa:7").rel(1, 2, {"activation"})),
            read(c, Kgml("path:hsa05224").entry(1, "hsa:2353 hsa:3725 hsa:6667").entry(2, "hsa:8").rel(1, 2, {"expression"}, "GErel"))};
}

}  // namespace

TEST_CASE("the base merge: E empty, the base mark, documents in canonical order, each node's boxes",
          "[unit][merge][S2.22]") {
    Context c;
    auto docs = ap1_jnk(c);
    std::reverse(docs.begin(), docs.end());
    MergeKey key;
    key.value = fusion::core::Sha256::of("k");
    const auto base = build_base_merge(docs, BuildSettings{}, key, c.ctx);
    CHECK(base->key() == key);
    REQUIRE(base->docs().size() == 3);
    CHECK(base->docs()[0]->source().pathway == "path:hsa04010");
    CHECK(base->docs()[2]->source().pathway == "path:hsa05224");
    CHECK(base->box_count() == 7);
    CHECK(base->relation_count() == 4);
    const auto& net = *base->network();
    CHECK(member_sets(net) ==
          std::vector<std::string>{"hsa1", "hsa2353 hsa3725 hsa5599 hsa5601 hsa5602 hsa6667", "hsa7", "hsa8"});
    REQUIRE(net.info().merge);
    CHECK(net.info().merge->base);
    CHECK(net.info().merge->key == key.hex());
    CHECK(net.info().merge->rules == static_cast<int>(fusion::merge::rules_version));
    CHECK(net.info().exceptions.empty());
    const auto family = *node(net, "hsa2353");
    std::vector<std::pair<std::uint32_t, std::uint32_t>> boxes;
    for (const auto& r : base->boxes_of(family)) boxes.emplace_back(r.doc, r.entry);
    CHECK(boxes == std::vector<std::pair<std::uint32_t, std::uint32_t>>{{0, 1}, {0, 2}, {1, 1}, {2, 1}});
    CHECK(base->boxes_of(*node(net, "hsa1")).size() == 1);
    // The base merge keeps the relations behind each edge (2B.11).
    REQUIRE(net.provenance() != nullptr);
    CHECK(net.provenance()->docs.size() == 3);
}

TEST_CASE("deleting E's ids from a base node is wrong; the split re-joins the node's boxes", "[unit][merge][S2.22]") {
    Context c;
    const auto docs = ap1_jnk(c);
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    ExceptionSet e;
    e.add(std::string_view("hsa3725"), fusion::merge::Reason::fsc);          // JUN
    e.add(std::string_view("hsa5599"), fusion::merge::Reason::not_merged);   // MAPK8
    const auto m = split(*base, e);
    // Every link between {FOS, SP1} and {MAPK9, MAPK10} ran through JUN or MAPK8. Deleting them from the
    // base node would leave {FOS, MAPK9, MAPK10, SP1}.
    CHECK(member_sets(*m.network) ==
          std::vector<std::string>{"hsa1", "hsa2353 hsa6667", "hsa3725", "hsa5599", "hsa5601 hsa5602", "hsa7", "hsa8"});
    CHECK(m.stats.exception_ids == 2);
    CHECK(m.stats.in_network == 2);
    CHECK(m.stats.split_out == 2);
    CHECK(m.stats.affected_nodes == 1);
    CHECK(m.stats.affected_boxes == 4);
    CHECK(m.stats.affected_groups == 0);
    CHECK(m.stats.affected_relations == 4);
    CHECK(m.base == base->key());
    CHECK(has(m.network->nodes()[*node(*m.network, "hsa3725")].flags, fusion::network::NodeFlag::exception));
    CHECK(m.network->info().exceptions == std::vector<std::string>{"hsa3725", "hsa5599"});
    REQUIRE(m.network->info().merge);
    CHECK_FALSE(m.network->info().merge->base);
    check_split(*base, docs, e);
}

TEST_CASE("split with an empty E returns the base network", "[unit][merge][S2.22]") {
    Context c;
    const auto docs = ap1_jnk(c);
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    const auto m = split(*base, ExceptionSet{});
    CHECK(dump(*m.network) == dump(*base->network()));
    CHECK(m.stats.affected_nodes == 0);
}

TEST_CASE("at threshold 2 a box that does not collapse keeps its units outside A in their base nodes",
          "[unit][merge][S2.22]") {
    // The prototype's first version gave such units new nodes, and failed one case in 60 (section 03).
    Context c;
    const std::vector<DocPtr> docs = {read(c, Kgml("path:hsa00110")
                                                  .entry(1, "hsa:1 hsa:2")
                                                  .entry(2, "hsa:1 hsa:3 hsa:4")
                                                  .entry(3, "hsa:2 hsa:5 hsa:6")
                                                  .entry(4, "hsa:9")
                                                  .rel(1, 4, {"activation"})
                                                  .rel(2, 4, {"binding/association"})
                                                  .rel(3, 4, {"inhibition"}))};
    const auto base = build_base_merge(docs, threshold(2), MergeKey{}, c.ctx);
    CHECK(member_sets(*base->network()) == std::vector<std::string>{"hsa1 hsa3 hsa4", "hsa2 hsa5 hsa6", "hsa9"});
    const auto e = exceptions({"hsa3"});
    const auto m = split(*base, e);
    CHECK(member_sets(*m.network) == std::vector<std::string>{"hsa1 hsa4", "hsa2 hsa5 hsa6", "hsa3", "hsa9"});
    CHECK(m.stats.affected_boxes == 2);
    check_split(*base, docs, e);
}

TEST_CASE("split equals merge_direct for every exception subset of small sets", "[unit][merge][S2.22][12.C14]") {
    Context c;
    // Chains and bridges across files, an undirected relation, a group over a collapsing box.
    const std::vector<DocPtr> chains = {
        read(c, Kgml("path:hsa00201")
                    .entry(1, "hsa:1 hsa:2")
                    .entry(2, "hsa:2 hsa:3 hsa:4")
                    .entry(3, "hsa:5")
                    .group(4, {1, 3})
                    .rel(1, 3, {"activation"})
                    .rel(2, 3, {"binding/association"})
                    .rel(4, 3, {"inhibition"})),
        read(c, Kgml("path:hsa00202")
                    .entry(1, "hsa:4 hsa:5 hsa:6")
                    .entry(2, "hsa:7")
                    .entry(3, "hsa:1 hsa:7")
                    .rel(1, 2, {"activation"}, "PPrel", "3")
                    .rel(3, 1, {"binding/association"}, "PPrel", "9"))};
    // Complexes whose components hold exception ids, a group of groups, self-loops, dissociation.
    const std::vector<DocPtr> complexes = {
        read(c, Kgml("path:hsa00301")
                    .entry(1, "hsa:1 hsa:2")
                    .entry(2, "hsa:3")
                    .group(3, {1, 2})
                    .entry(4, "hsa:4 hsa:5")
                    .group(5, {3, 4})
                    .rel(3, 4, {"activation"})
                    .rel(5, 1, {"dissociation"})
                    .rel(1, 1, {"activation"})),
        read(c, Kgml("path:hsa00302")
                    .entry(1, "hsa:2 hsa:6")
                    .entry(2, "hsa:7 hsa:1")
                    .entry(3, "hsa:5 hsa:6 hsa:7")
                    .rel(1, 2, {"binding/association"})
                    .rel(3, 1, {"activation"}))};
    // One file whose boxes overlap (03.C5), at every threshold.
    const std::vector<DocPtr> overlaps = {read(c, Kgml("path:hsa00401")
                                                      .entry(1, "hsa:1 hsa:2 hsa:3")
                                                      .entry(2, "hsa:3 hsa:4")
                                                      .entry(3, "hsa:4 hsa:5 hsa:6 hsa:7")
                                                      .entry(4, "hsa:2 hsa:6")
                                                      .rel(1, 2, {"activation"})
                                                      .rel(2, 3, {"binding/association"})
                                                      .rel(3, 4, {"activation"})
                                                      .rel(4, 1, {"inhibition"}))};
    const std::vector<std::string> universe = {"hsa1", "hsa2", "hsa3", "hsa4", "hsa5", "hsa6", "hsa7"};
    for (const auto* docs : {&chains, &complexes, &overlaps})
        for (const int t : {0, 1, 2, 3, 15000}) {
            const auto base = build_base_merge(*docs, threshold(t), MergeKey{}, c.ctx);
            for (unsigned mask = 0; mask < (1U << universe.size()); ++mask) {
                ExceptionSet e;
                for (std::size_t i = 0; i < universe.size(); ++i)
                    if ((mask >> i) & 1U) e.add(std::string_view(universe[i]), fusion::merge::Reason::up);
                check_split(*base, *docs, e);
            }
        }
}

TEST_CASE("split equals merge_direct on random sets", "[unit][merge][S2.22][12.C14]") {
    Context c;
    auto rng = fusion::core::Rng::stream(2026, 3);
    for (int round = 0; round < 60; ++round) {
        const auto set = generate(rng, 12, 3);
        const auto docs = read_all(c, set.pathways, "r" + std::to_string(round));
        const int t = std::vector<int>{0, 1, 2, 3, 15000}[rng.below(5)];
        const auto base = build_base_merge(docs, threshold(t), MergeKey{}, c.ctx);
        for (int k = 0; k < 8; ++k) {
            ExceptionSet e;
            // E biased towards members of multi-id base nodes and complex components (section 03).
            for (const auto& id : set.ids)
                if (rng.below(4) == 0) e.add(std::string_view(id), fusion::merge::Reason::down);
            check_split(*base, docs, e);
        }
    }
}
