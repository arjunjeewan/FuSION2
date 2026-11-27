// The sets of S2.22's bounded exhaustive layer (section 12, "Merge then split"): each shape section 12
// lists, at its thresholds and filters, and the generated sets that fill them up to about 50. Shared by
// test_split.cpp's bounded layer and by test_reference.cpp, which checks both oracles against FuSION 1's
// merge of the same sets (W7), so the sets FuSION 1 ran and the suite's sets cannot drift apart.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/network/network.hpp"
#include "fusion/testkit/generate.hpp"
#include "support.hpp"

namespace merge_test {

struct Bounded {
    std::string name;
    std::vector<std::string> texts;   // KGML-format text, made-up ids (S8.31)
    fusion::network::BuildSettings settings;
};

// The listed shapes, 32 sets.
inline std::vector<Bounded> bounded_sets() {
    using fusion::network::BuildSettings;
    std::vector<Bounded> out;
    const auto add = [&](std::string name, std::vector<Kgml> set, std::initializer_list<int> thresholds,
                         bool keep_indirect = false, bool exclude_ecrel = false) {
        std::vector<std::string> texts;
        for (const auto& k : set) texts.push_back(k.text());
        for (int t : thresholds) {
            BuildSettings s;
            s.split_threshold = t;
            if (keep_indirect) s.filters.indirect = fusion::network::IndirectMode::keep;
            if (exclude_ecrel) s.filters.ecrel = fusion::network::EcrelMode::exclude;
            out.push_back({name + " t" + std::to_string(t), texts, s});
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

// Generated sets (the kit's pathways(), on core::Rng stream 1 of `seed`) over at most 10 ids, each at its
// threshold and filters, appended until `sets` holds 50.
inline void add_generated_sets(std::vector<Bounded>& sets, std::uint64_t seed) {
    namespace testkit = fusion::testkit;
    auto rng = fusion::core::Rng::stream(seed, 1);
    while (sets.size() < 50) {
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
        b.settings.split_threshold = std::vector<int>{0, 1, 2, 15000}[rng.below(4)];
        if (rng.below(4) == 0) b.settings.filters.indirect = fusion::network::IndirectMode::keep;
        if (rng.below(6) == 0) b.settings.filters.ecrel = fusion::network::EcrelMode::exclude;
        sets.push_back(std::move(b));
    }
}

}  // namespace merge_test
