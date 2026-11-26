// Merge once, split afterwards (S2.22; section 03, "Merge once, split afterwards"; section 12, "Merge then
// split"; 12.C14): the base merge and its split on worked examples, then section 12's three layers, each
// comparing split(base(R), E), merge_direct(R, E) and the test kit's exception_merge(R, E), an oracle
// written from S2.07 to S2.21 that shares no code with merge or network:
//   bounded exhaustive: about 50 sets over at most 10 ids, against every exception subset;
//   random: 1000 generated sets over up to 200 ids, each also merged in 20 orders (S2.17) and each id
//   in exactly one node (stage 2 open question 4);
//   real (T2, [kegg][slow]): the 167 KGMLs split by 100 random subsets of the EGF up and down genes.
// T0 runs the first two cut to fit its budget, the listed shapes alone and 50 sets; their copies at these
// sizes are tagged [slow] (prompt log entry 349).
#include <catch2/catch_test_macros.hpp>

#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/catch.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "fusion/testkit/oracles.hpp"
#include "support.hpp"

using namespace merge_test;
using fusion::merge::build_base_merge;
using fusion::merge::ExceptionSet;
using fusion::merge::merge_direct;
using fusion::merge::MergeKey;
using fusion::merge::split;
using fusion::network::BuildSettings;
namespace testkit = fusion::testkit;

namespace {

BuildSettings threshold(int t) {
    BuildSettings s;
    s.split_threshold = t;
    return s;
}

testkit::MergeOracleSettings oracle_settings(const BuildSettings& s) {
    testkit::MergeOracleSettings o;
    o.threshold = s.split_threshold;
    o.keep_indirect = s.filters.indirect == fusion::network::IndirectMode::keep;
    o.keep_ecrel = s.filters.ecrel == fusion::network::EcrelMode::keep;
    return o;
}

ExceptionSet exception_set(const std::set<std::string>& ids) {
    ExceptionSet e;
    for (const auto& id : ids) e.add(std::string_view(id), fusion::merge::Reason::up);
    return e;
}

std::string describe(const std::set<std::string>& e, const BuildSettings& s) {
    std::string out = "E = {";
    for (const auto& id : e) out += " " + id;
    return out + " } threshold " + std::to_string(s.split_threshold) +
           (s.filters.indirect == fusion::network::IndirectMode::keep ? " indirect kept" : "") +
           (s.filters.ecrel == fusion::network::EcrelMode::exclude ? " ecrel excluded" : "");
}

// The three sides agree: split equals merge_direct in every model field (dump, content hash), and both
// equal the kit's oracle as canonical graphs. Returns whether they did, for a failing input to be kept.
bool three_sides(const fusion::merge::BaseMerge& base, const std::vector<DocPtr>& docs,
                 const std::vector<std::string>& texts, const std::set<std::string>& ids) {
    const ExceptionSet e = exception_set(ids);
    const auto m = split(base, e);
    const auto d = merge_direct(docs, base.settings(), e);
    const testkit::CanonGraph oracle = testkit::exception_merge_texts(texts, ids, oracle_settings(base.settings()));
    const testkit::CanonGraph direct = testkit::adapt::canon(*d);
    const bool oracle_ok = direct == oracle;
    INFO(describe(ids, base.settings()) << "\n" << (oracle_ok ? std::string() : testkit::diff(direct, oracle).text()));
    const bool split_ok = dump(*m.network) == dump(*d) && m.network->content_hash() == d->content_hash();
    CHECK(split_ok);
    CHECK(oracle_ok);
    return split_ok && oracle_ok;
}

// The section 03 example: AP-1 {FOS, JUN}, JNK {MAPK8, MAPK9, MAPK10}, hsa05161's {JUN, MAPK8, MAPK9,
// MAPK10} and hsa05224's {FOS, JUN, SP1} make one base node keyed hsa2353 (FOS).
std::vector<Kgml> ap1_jnk_kgml() {
    return {Kgml("path:hsa04010", "MAPK signaling pathway")
                .entry(1, "hsa:2353 hsa:3725")
                .entry(2, "hsa:5599 hsa:5601 hsa:5602")
                .entry(3, "hsa:1")
                .rel(2, 1, {"phosphorylation"})
                .rel(1, 3, {"expression"}, "GErel"),
            Kgml("path:hsa05161").entry(1, "hsa:3725 hsa:5599 hsa:5601 hsa:5602").entry(2, "hsa:7").rel(1, 2, {"activation"}),
            Kgml("path:hsa05224").entry(1, "hsa:2353 hsa:3725 hsa:6667").entry(2, "hsa:8").rel(1, 2, {"expression"}, "GErel")};
}

std::vector<DocPtr> read_set(Context& c, const std::vector<Kgml>& set) {
    std::vector<DocPtr> docs;
    for (const auto& k : set) docs.push_back(read(c, k));
    return docs;
}

std::vector<std::string> texts_of(const std::vector<Kgml>& set) {
    std::vector<std::string> out;
    for (const auto& k : set) out.push_back(k.text());
    return out;
}

// ---- the bounded exhaustive layer's sets: each shape section 12 lists, at its thresholds and filters

struct Bounded {
    std::string name;
    std::vector<std::string> texts;   // KGML-format text, made-up ids (S8.31)
    BuildSettings settings;
};

std::vector<Bounded> bounded_sets() {
    std::vector<Bounded> out;
    const auto add = [&](std::string name, std::vector<Kgml> set, std::initializer_list<int> thresholds,
                         bool keep_indirect = false, bool exclude_ecrel = false) {
        for (int t : thresholds) {
            BuildSettings s = threshold(t);
            if (keep_indirect) s.filters.indirect = fusion::network::IndirectMode::keep;
            if (exclude_ecrel) s.filters.ecrel = fusion::network::EcrelMode::exclude;
            out.push_back({name + " t" + std::to_string(t), texts_of(set), s});
        }
    };
    // Chains and bridges across files, an undirected relation, a group over a collapsing box.
    add("chains",
        {Kgml("path:hsa00201")
             .entry(1, "hsa:1 hsa:2").entry(2, "hsa:2 hsa:3 hsa:4").entry(3, "hsa:5").group(4, {1, 3})
             .rel(1, 3, {"activation"}).rel(2, 3, {"binding/association"}).rel(4, 3, {"inhibition"}),
         Kgml("path:hsa00202")
             .entry(1, "hsa:4 hsa:5 hsa:6").entry(2, "hsa:7").entry(3, "hsa:1 hsa:7")
             .rel(1, 2, {"activation"}, "PPrel", "3").rel(3, 1, {"binding/association"}, "PPrel", "9")},
        {0, 1, 2, 3, 15000});
    // Complexes whose components hold exception ids, a group of groups, self-loops, dissociation.
    add("complexes",
        {Kgml("path:hsa00301")
             .entry(1, "hsa:1 hsa:2").entry(2, "hsa:3").group(3, {1, 2}).entry(4, "hsa:4 hsa:5").group(5, {3, 4})
             .rel(3, 4, {"activation"}).rel(5, 1, {"dissociation"}).rel(1, 1, {"activation"}),
         Kgml("path:hsa00302")
             .entry(1, "hsa:2 hsa:6").entry(2, "hsa:7 hsa:1").entry(3, "hsa:5 hsa:6 hsa:7")
             .rel(1, 2, {"binding/association"}).rel(3, 1, {"activation"})},
        {0, 1, 2, 3, 15000});
    // One file whose boxes overlap (03.C5).
    add("one file, overlapping boxes",
        {Kgml("path:hsa00401")
             .entry(1, "hsa:1 hsa:2 hsa:3").entry(2, "hsa:3 hsa:4").entry(3, "hsa:4 hsa:5 hsa:6 hsa:7").entry(4, "hsa:2 hsa:6")
             .rel(1, 2, {"activation"}).rel(2, 3, {"binding/association"}).rel(3, 4, {"activation"}).rel(4, 1, {"inhibition"})},
        {0, 1, 2, 3, 15000});
    // Two boxes that share only an exception id: the bridge must break.
    add("bridge", {Kgml("path:hsa00501").entry(1, "hsa:1 hsa:2").entry(2, "hsa:9").rel(1, 2, {"activation"}),
                   Kgml("path:hsa00502").entry(1, "hsa:2 hsa:3").entry(2, "hsa:8").rel(2, 1, {"inhibition"})},
        {1, 2});
    // Chains A and B, B and C under different keys, the shape that crashed FuSION 1 (S2.18).
    add("chain of three",
        {Kgml("path:hsa00601").entry(1, "hsa:1 hsa:2").entry(2, "hsa:10").rel(1, 2, {"activation"}),
         Kgml("path:hsa00602").entry(1, "hsa:2 hsa:3").entry(2, "hsa:10").rel(2, 1, {"expression"}, "GErel"),
         Kgml("path:hsa00603").entry(1, "hsa:3 hsa:4 hsa:5").entry(2, "hsa:6").rel(1, 2, {"phosphorylation"})},
        {1});
    // Complexes over components that hold exception ids (Cartesian product, S2.09).
    add("complex of families",
        {Kgml("path:hsa00701")
             .entry(1, "hsa:1 hsa:2").entry(2, "hsa:3 hsa:4").group(3, {1, 2}).entry(4, "hsa:5").entry(5, "hsa:6")
             .rel(3, 4, {"activation"}).rel(5, 3, {"activation", "phosphorylation"})},
        {1, 15000});
    // Relations between two boxes that both hold exceptions (fan-out, S2.10), and duplicates across files.
    add("fan-out",
        {Kgml("path:hsa00801").entry(1, "hsa:1 hsa:2 hsa:3").entry(2, "hsa:4 hsa:5 hsa:6").rel(1, 2, {"activation"}, "PPrel", "700"),
         Kgml("path:hsa00802").entry(1, "hsa:1").entry(2, "hsa:4").rel(1, 2, {"activation"}, "PPrel", "900")},
        {1, 2});
    // Undirected relations whose tag direction changes after the split (S2.21).
    add("undirected tags",
        {Kgml("path:hsa00901").entry(1, "hsa:2 hsa:7").entry(2, "hsa:3").rel(1, 2, {"binding/association"}),
         Kgml("path:hsa00902").entry(1, "hsa:7").entry(2, "hsa:3 hsa:8").rel(2, 1, {"binding/association"}, "PPrel", "5")},
        {1});
    // Self-loops made or removed by a split; duplicates that collapse or separate.
    add("self-loops and duplicates",
        {Kgml("path:hsa01001").entry(1, "hsa:1 hsa:2").entry(2, "hsa:2 hsa:3").entry(3, "hsa:4").entry(4, "hsa:4 hsa:5")
             .rel(1, 2, {"activation"}).rel(3, 4, {"inhibition"}).rel(1, 3, {"activation"}).rel(1, 3, {"activation"}),
         Kgml("path:hsa01002").entry(1, "hsa:1").entry(2, "hsa:4").rel(1, 2, {"activation"}, "PPrel", "12.5")},
        {1, 2});
    // Indirect and ECrel relations under both settings (S2.11, S2.31).
    const std::vector<Kgml> filters = {
        Kgml("path:hsa01101").entry(1, "hsa:1 hsa:2").entry(2, "hsa:3").entry(3, "hsa:4 hsa:5").entry(4, "cpd:C00001", "compound")
            .rel(1, 2, {"indirect effect"}).rel(2, 3, {"activation", "indirect effect"}).rel(3, 1, {"activation"})
            .rel(1, 4, {"compound"}, "ECrel").rel(4, 3, {"compound"}, "ECrel", "0")};
    add("filters", filters, {1});
    add("filters, indirect kept", filters, {1}, true);
    add("filters, ECrel excluded", filters, {1}, false, true);
    add("filters, both", filters, {1, 2}, true, true);
    // A one-file set with an id in two boxes of the same pathway (03.C5), at a large threshold.
    add("one file, id in two boxes",
        {Kgml("path:hsa01201").entry(1, "hsa:1 hsa:2 hsa:3").entry(2, "hsa:3 hsa:4").entry(3, "hsa:5")
             .rel(1, 3, {"activation"}).rel(2, 3, {"inhibition"}).rel(3, 2, {"dissociation"})},
        {1, 15000});
    return out;
}

}  // namespace

TEST_CASE("the base merge: E empty, the base mark, documents in canonical order, each node's boxes",
          "[unit][merge][S2.22]") {
    Context c;
    auto docs = read_set(c, ap1_jnk_kgml());
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
    const auto set = ap1_jnk_kgml();
    const auto docs = read_set(c, set);
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
    three_sides(*base, docs, texts_of(set), {"hsa3725", "hsa5599"});
}

TEST_CASE("split with an empty E returns the base network", "[unit][merge][S2.22]") {
    Context c;
    const auto set = ap1_jnk_kgml();
    const auto docs = read_set(c, set);
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    const auto m = split(*base, ExceptionSet{});
    CHECK(dump(*m.network) == dump(*base->network()));
    CHECK(m.stats.affected_nodes == 0);
    three_sides(*base, docs, texts_of(set), {});
}

TEST_CASE("at threshold 2 a box that does not collapse keeps its units outside A in their base nodes",
          "[unit][merge][S2.22]") {
    // The prototype's first version gave such units new nodes, and failed one case in 60 (section 03).
    Context c;
    const std::vector<Kgml> set = {Kgml("path:hsa00110")
                                       .entry(1, "hsa:1 hsa:2")
                                       .entry(2, "hsa:1 hsa:3 hsa:4")
                                       .entry(3, "hsa:2 hsa:5 hsa:6")
                                       .entry(4, "hsa:9")
                                       .rel(1, 4, {"activation"})
                                       .rel(2, 4, {"binding/association"})
                                       .rel(3, 4, {"inhibition"})};
    const auto docs = read_set(c, set);
    const auto base = build_base_merge(docs, threshold(2), MergeKey{}, c.ctx);
    CHECK(member_sets(*base->network()) == std::vector<std::string>{"hsa1 hsa3 hsa4", "hsa2 hsa5 hsa6", "hsa9"});
    const auto e = exceptions({"hsa3"});
    const auto m = split(*base, e);
    CHECK(member_sets(*m.network) == std::vector<std::string>{"hsa1 hsa4", "hsa2 hsa5 hsa6", "hsa3", "hsa9"});
    CHECK(m.stats.affected_boxes == 2);
    three_sides(*base, docs, texts_of(set), {"hsa3"});
}

namespace {

using merge_test::texts_of;   // the generated sets' overload, beside this file's own for Kgml builders

// The bounded exhaustive layer: the hand-made shapes above and, with `generated`, generated sets (the kit's
// pathways(), on core::Rng) up to section 12's about 50 over at most 10 ids, each at its threshold and
// filters, against every subset of its ids.
void bounded_layer(bool generated) {
    Context c;
    std::vector<Bounded> sets = bounded_sets();
    const std::size_t shapes = sets.size();
    FUSION_SEED(2022);
    auto rng = fusion::core::Rng::stream(fusion_seed, 1);
    while (generated && sets.size() < 50) {
        testkit::PathwayShape shape;
        const int count = 1 + static_cast<int>(rng.below(2));
        shape.entries = count == 1 ? 5 : 3;                  // at most entries * (count + 1) = 10 ids
        shape.relations = 3 + static_cast<int>(rng.below(5));
        shape.max_box = 4;
        shape.groups = static_cast<int>(rng.below(3));
        shape.shared_id = 0.4;
        shape.undirected = 0.25;
        shape.indirect = 0.1;
        shape.ecrel = 0.05;
        shape.duplicate = 0.1;
        Bounded b;
        b.name = "generated " + std::to_string(sets.size());
        b.texts = texts_of(testkit::pathways(rng, count, shape));
        if (ids_of(b.texts).size() < 8) continue;           // sets near section 12's 10 ids
        b.settings = threshold(std::vector<int>{0, 1, 2, 15000}[rng.below(4)]);
        if (rng.below(4) == 0) b.settings.filters.indirect = fusion::network::IndirectMode::keep;
        if (rng.below(6) == 0) b.settings.filters.ecrel = fusion::network::EcrelMode::exclude;
        sets.push_back(std::move(b));
    }
    std::size_t comparisons = 0, largest = 0;
    bool kept = false;
    for (std::size_t k = 0; k < sets.size(); ++k) {
        const Bounded& set = sets[k];
        const std::vector<std::string>& texts = set.texts;
        const auto docs = read_texts(c, texts, "b" + std::to_string(k));
        const std::vector<std::string> universe = ids_of(texts);
        REQUIRE(universe.size() <= 10);
        largest = std::max(largest, universe.size());
        INFO(set.name);
        const auto base = build_base_merge(docs, set.settings, MergeKey{}, c.ctx);
        for (unsigned mask = 0; mask < (1U << universe.size()); ++mask) {
            std::set<std::string> e;
            for (std::size_t i = 0; i < universe.size(); ++i)
                if ((mask >> i) & 1U) e.insert(universe[i]);
            if (!three_sides(*base, docs, texts, e) && !kept) {
                std::string all;
                for (const auto& t : texts) all += t;
                testkit::keep_failing_input("s222-bounded", "set.xml", all + "\n" + describe(e, set.settings) + "\n");
                kept = true;
            }
            ++comparisons;
        }
    }
    CHECK(sets.size() == (generated ? 50 : shapes));
    CHECK(largest >= (generated ? 9U : 7U));
    CHECK(comparisons >= (generated ? 5000U : 2000U));
}

// The random layer: `rounds` generated sets over up to 200 ids, each with an exception set biased towards
// members of multi-id nodes and complex components (section 03); the same inputs also check order
// independence over 20 orders of the list and of each file (S2.17), and that every id lies in exactly one
// node. One seed, so a shorter run's sets are the first of a longer one's.
void random_layer(int rounds, std::size_t largest_at_least) {
    Context c;
    FUSION_SEED(2023);
    auto rng = fusion::core::Rng::stream(fusion_seed, 2);
    std::size_t largest = 0;
    bool kept = false;
    for (int round = 0; round < rounds; ++round) {
        testkit::PathwayShape shape;
        shape.entries = 5 + static_cast<int>(rng.below(36));
        shape.relations = shape.entries + static_cast<int>(rng.below(static_cast<std::uint64_t>(shape.entries) + 1));
        shape.max_box = 4;
        shape.groups = 1 + static_cast<int>(rng.below(3));
        shape.shared_id = 0.3;
        const int count = 1 + static_cast<int>(rng.below(4));
        const std::vector<std::string> texts = texts_of(testkit::pathways(rng, count, shape));
        BuildSettings s = threshold(std::vector<int>{0, 1, 2, 3, 15000}[rng.below(5)]);
        if (rng.below(4) == 0) s.filters.indirect = fusion::network::IndirectMode::keep;
        if (rng.below(4) == 0) s.filters.ecrel = fusion::network::EcrelMode::exclude;
        const auto docs = read_texts(c, texts, "r" + std::to_string(round));
        const std::vector<std::string> universe = ids_of(texts);
        largest = std::max(largest, universe.size());
        REQUIRE(universe.size() <= 200);
        // E: biased towards members of multi-id base nodes and of complexes.
        const testkit::CanonGraph whole = testkit::exception_merge_texts(texts, {}, oracle_settings(s));
        std::set<std::string> boxed;
        for (const auto& n : whole.nodes)
            if (n.members.size() > 1 || n.type == "group") boxed.insert(n.members.begin(), n.members.end());
        std::set<std::string> e;
        for (const auto& id : universe)
            if (rng.below(boxed.contains(id) ? 3 : 20) == 0) e.insert(id);
        INFO("round " << round);
        const auto base = build_base_merge(docs, s, MergeKey{}, c.ctx);
        bool ok = three_sides(*base, docs, texts, e);
        // Order independence (S2.17) and one node per id (stage 2 open question 4).
        const ExceptionSet exceptions = exception_set(e);
        const std::string reference = dump(*merge_direct(docs, s, exceptions));
        for (int p = 0; p < 20; ++p) {
            std::vector<std::string> shuffled;
            for (const auto& t : texts) shuffled.push_back(shuffled_kgml(t, rng));
            rng.shuffle(shuffled.begin(), shuffled.end());
            const auto again = merge_direct(read_texts(c, shuffled, "r" + std::to_string(round) + "p" + std::to_string(p)), s,
                                            exceptions);
            const bool same = dump(*again) == reference;
            CHECK(same);
            ok = ok && same;
        }
        std::map<std::string, int> holders;
        for (const auto& n : testkit::adapt::canon(*merge_direct(docs, s, exceptions)).nodes)
            if (n.type != "group")
                for (const auto& m : n.members) ++holders[m];
        for (const auto& [id, k] : holders) {
            INFO(id);
            CHECK(k == 1);
            ok = ok && k == 1;
        }
        if (!ok && !kept) {
            std::string all;
            for (const auto& t : texts) all += t;
            testkit::keep_failing_input("s222-random", "set.xml", all + "\n" + describe(e, s) + "\n");
            kept = true;
        }
    }
    CHECK(largest >= largest_at_least);
}

}  // namespace

// Each synthetic layer runs twice (prompt log entry 349): a copy cut to fit T0's budget, and a copy at
// section 12's sizes, tagged [slow] instead of [unit], which runs where the real layer runs.
TEST_CASE("S2.22, bounded exhaustive: split, merge_direct and exception_merge agree on every exception subset "
          "of the listed shapes",
          "[unit][merge][S2.22][12.C14]") {
    bounded_layer(false);
}

TEST_CASE("S2.22, bounded exhaustive, full: every exception subset of about 50 sets, the listed shapes and generated "
          "ones",
          "[merge][S2.22][12.C14][slow]") {
    bounded_layer(true);
}

TEST_CASE("S2.22, random: split, merge_direct and exception_merge agree on 50 generated sets, in any order",
          "[unit][merge][S2.22][S2.17][12.C14][property]") {
    random_layer(50, 60);
}

TEST_CASE("S2.22, random, full: the same on 1000 generated sets over up to 200 ids",
          "[merge][S2.22][S2.17][12.C14][property][slow]") {
    random_layer(1000, 101);
}

TEST_CASE("S2.22, real: the 167 KGMLs split by 100 random subsets of the EGF up and down genes",
          "[merge][S2.22][12.C14][kegg][slow]") {
    // The reference run's up and down genes (section 12, "The reference run") as the pool; each subset takes
    // each gene with probability one half. Needs the data root and is meant for a Release build (C2 note).
    const auto root = FUSION_REQUIRE_DATA("kegg/hsa_pathways_with_exp_score/", "kegg/updated_pathways/",
                                          "reference/jun-b10-fdr025/f1/run/up_reg",
                                          "reference/jun-b10-fdr025/f1/run/down_reg");
    Context c;
    std::vector<std::filesystem::path> files;
    for (const char* folder : {"kegg/hsa_pathways_with_exp_score", "kegg/updated_pathways"})
        for (const auto& entry : std::filesystem::directory_iterator(root.at(folder)))
            if (entry.path().extension() == ".xml") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 167);
    std::vector<DocPtr> docs;
    for (const auto& f : files) docs.push_back(fusion::network::read_kgml(f, fusion::network::IdMap::none(), c.ctx));
    std::vector<std::string> genes = testkit::f1::read_ids(root.at("reference/jun-b10-fdr025/f1/run/up_reg"));
    for (const auto& g : testkit::f1::read_ids(root.at("reference/jun-b10-fdr025/f1/run/down_reg"))) genes.push_back(g);
    REQUIRE(genes.size() == 45);
    const BuildSettings s;
    const auto base = build_base_merge(docs, s, MergeKey{}, c.ctx);
    FUSION_SEED(2024);
    auto rng = fusion::core::Rng::stream(fusion_seed, 3);
    for (int round = 0; round < 100; ++round) {
        std::set<std::string> e;
        for (const auto& g : genes)
            if (rng.below(2) == 0) e.insert(g);
        INFO("subset " << round);
        const ExceptionSet exceptions = exception_set(e);
        const auto m = split(*base, exceptions);
        const auto d = merge_direct(docs, s, exceptions);
        CHECK(dump(*m.network) == dump(*d));
        const testkit::CanonGraph oracle = testkit::exception_merge(files, e, oracle_settings(s));
        const testkit::GraphDiff difference = testkit::diff(testkit::adapt::canon(*d), oracle);
        INFO(difference.text());
        CHECK(difference.empty());
    }
}
