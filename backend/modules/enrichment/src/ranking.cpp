// The ranking (section 08 "Algorithms: Ranking"; S7.04, S7.05, 08.C2).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "detail.hpp"
#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {

// FuSION 1 handed the values to gseapy (gsea_select_pathways.py:178, 185-193), which sorted them
// with pandas' unstable quicksort (gseapy gsea.py:493-494), so tied values fell in a CPU-dependent
// order (S7.05). 2.0 orders ties by gene id in byte order (08.C2); its tests give gseapy this
// order with ascending=None.
Ranking rank(const expression::GeneValues& values, const core::Interner& ids) {
    std::vector<std::pair<core::GeneId, double>> pairs;
    pairs.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        // S7.04: section 04's reader drops and reports non-finite values, so none should reach
        // here; one that did would break the sort's total order, so it is left out.
        if (const double v = values.value(i); std::isfinite(v)) pairs.emplace_back(values.id(i), v);
    }
    return detail::rank_pairs(std::move(pairs), ids);
}

namespace detail {

Ranking rank_pairs(std::vector<std::pair<core::GeneId, double>> pairs, const core::Interner& ids) {
    // -0.0 and 0.0 compare equal, so they tie and are ordered by id, as pandas counts them as
    // one value (gseapy gsea.py:516).
    std::sort(pairs.begin(), pairs.end(), [&ids](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return core::stable_compare(ids.str(a.first), ids.str(b.first)) < 0;
    });
    Ranking ranking;
    ranking.id.reserve(pairs.size());
    ranking.value.reserve(pairs.size());
    ranking.weight.reserve(pairs.size());
    for (std::size_t p = 0; p < pairs.size(); ++p) {
        const double v = pairs[p].second;
        ranking.id.push_back(pairs[p].first);
        ranking.value.push_back(v);
        ranking.weight.push_back(std::fabs(v));   // |v|^1, as gseapy core stats.rs:600 with weight 1
        if (p == 0 || v != pairs[p - 1].second) ++ranking.distinct_values;
    }
    return ranking;
}

}  // namespace detail
}  // namespace fusion::enrichment
