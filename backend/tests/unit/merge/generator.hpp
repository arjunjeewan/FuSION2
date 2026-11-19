// Random synthetic pathway sets for the merge's properties (section 12, "Merge then split"): boxes over a
// small id universe, so they overlap often; groups, groups of groups, undirected, indirect and ECrel
// relations; scores and unscored relations. Seeded through core's Rng, so every run draws the same sets.
#pragma once

#include <string>
#include <vector>

#include "support.hpp"

namespace merge_test {

struct GeneratedSet {
    std::vector<Kgml> pathways;
    std::vector<std::string> ids;                  // the canonical ids that can occur: "hsa1", ...
};

inline GeneratedSet generate(fusion::core::Rng& rng, int universe, int max_docs) {
    GeneratedSet set;
    for (int i = 1; i <= universe; ++i) set.ids.push_back("hsa" + std::to_string(i));
    const auto pick = [&](std::uint64_t n) { return static_cast<int>(rng.below(n)); };
    const int docs = 1 + pick(static_cast<std::uint64_t>(max_docs));
    static const std::vector<std::vector<std::string>> subtypes = {
        {"activation"}, {"inhibition"}, {"binding/association"}, {"activation", "phosphorylation"},
        {"dissociation"}, {"indirect effect"}, {"expression"}, {}};
    static const std::vector<std::string> scores = {"", "0", "5", "700", "12.5"};
    for (int d = 0; d < docs; ++d) {
        Kgml k("path:hsa9" + std::to_string(1000 + d));
        const int boxes = 2 + pick(5);
        int next = 1;
        std::vector<int> entries;
        for (int b = 0; b < boxes; ++b) {
            const int size = 1 + pick(4);
            std::vector<int> ids;
            for (int i = 0; i < size; ++i) {
                const int id = 1 + pick(static_cast<std::uint64_t>(universe));
                if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
            }
            std::string name;
            for (int id : ids) name += (name.empty() ? "" : " ") + std::string("hsa:") + std::to_string(id);
            k.entry(next, name);
            entries.push_back(next++);
        }
        if (pick(3) == 0) {                                          // a compound, alone in its box
            k.entry(next, "cpd:C0000" + std::to_string(1 + pick(3)), "compound");
            entries.push_back(next++);
        }
        const int groups = pick(3);
        for (int g = 0; g < groups; ++g) {                           // components among earlier entries
            std::vector<int> comps;
            const int size = 1 + pick(3);
            for (int i = 0; i < size; ++i) {
                const int c = entries[static_cast<std::size_t>(pick(entries.size()))];
                if (std::find(comps.begin(), comps.end(), c) == comps.end()) comps.push_back(c);
            }
            k.group(next, comps);
            entries.push_back(next++);
        }
        const int relations = 2 + pick(6);
        for (int r = 0; r < relations; ++r) {
            const int a = entries[static_cast<std::size_t>(pick(entries.size()))];
            const int b = entries[static_cast<std::size_t>(pick(entries.size()))];
            const auto& st = subtypes[static_cast<std::size_t>(pick(subtypes.size()))];
            const std::string type = pick(10) == 0 ? "ECrel" : pick(5) == 0 ? "GErel" : "PPrel";
            const std::string& score = scores[static_cast<std::size_t>(pick(scores.size()))];
            k.rel(a, b, st, type, score.empty() ? std::nullopt : std::optional<std::string>(score));
        }
        set.pathways.push_back(std::move(k));
    }
    return set;
}

inline std::vector<DocPtr> read_all(Context& c, const std::vector<Kgml>& pathways, const std::string& folder) {
    std::vector<DocPtr> docs;
    for (const auto& k : pathways)
        docs.push_back(read(c, k, folder + "/" + k.name().substr(k.name().find(':') + 1) + ".xml"));
    return docs;
}

}  // namespace merge_test
