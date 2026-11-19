// Selection: the selector and the keep list (section 08 "Algorithms: Selection"; S7.13, S7.14).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {
namespace {

[[noreturn]] void bad_term(std::string_view term, std::string_view why) {
    throw core::Failure("gsea.selector", "bad selector term '" + std::string(term) + "' (" + std::string(why) + ")");
}

// A threshold's value: section 04's decimal grammar (04.C2), finite, within [low, high].
// FuSION 1 read it with Python's float(), which takes nan, inf and 1_000 (gsea_select_pathways.py:80).
double threshold(std::string_view term, std::string_view value, double low, double high, std::string_view why) {
    const core::Decimal d = core::parse_decimal(value);
    if (d.status != core::DecimalStatus::ok || !(d.value >= low && d.value <= high)) bad_term(term, why);
    return d.value;
}

// top's value: a positive integer, digits only. FuSION 1 read it with int(), which takes -5 and
// 0 (S7.13). A count beyond 2^32 − 1 keeps every pathway, as 2^32 − 1 does.
std::uint32_t top_count(std::string_view term, std::string_view value) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        bad_term(term, "top takes a positive integer");
    std::uint64_t n = 0;
    for (const char ch : value) {
        n = n * 10 + static_cast<std::uint64_t>(ch - '0');
        if (n > std::numeric_limits<std::uint32_t>::max()) return std::numeric_limits<std::uint32_t>::max();
    }
    if (n == 0) bad_term(term, "top takes a positive integer");
    return static_cast<std::uint32_t>(n);
}

}  // namespace

// gsea_select_pathways.py:72-83 split the text on ',' and each term at its first ':'. S7.13
// keeps the four keys, takes only finite, in-range values, and makes the terms a conjunction:
// a repeated key applies every time, so the strictest value of each key stands.
Selector parse_selector(std::string_view text) {
    Selector selector;
    selector.text = std::string(text);
    for (std::size_t start = 0;;) {
        const std::size_t comma = text.find(',', start);
        const std::string_view term = text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        const std::size_t colon = term.find(':');
        const std::string_view key = term.substr(0, colon);
        const std::string_view value = colon == std::string_view::npos ? std::string_view{} : term.substr(colon + 1);
        if (key == "fdr") {
            const double q = threshold(term, value, 0.0, 1.0, "fdr takes a number from 0 to 1");
            selector.max_fdr = selector.max_fdr ? std::min(*selector.max_fdr, q) : q;
        } else if (key == "pval") {
            const double p = threshold(term, value, 0.0, 1.0, "pval takes a number from 0 to 1");
            selector.max_pval = selector.max_pval ? std::min(*selector.max_pval, p) : p;
        } else if (key == "nes") {
            const double x = threshold(term, value, 0.0, std::numeric_limits<double>::max(),
                                       "nes takes a finite number of at least 0");
            selector.min_abs_nes = selector.min_abs_nes ? std::max(*selector.min_abs_nes, x) : x;
        } else if (key == "top") {
            const std::uint32_t n = top_count(term, value);
            selector.top = selector.top ? std::min(*selector.top, n) : n;
        } else {
            bad_term(term, "use fdr:Q, pval:P, nes:X or top:N");
        }
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return selector;
}

bool Selection::chosen(std::uint32_t candidate) const {
    return by_gsea[candidate] || !kept_for[candidate].empty();
}

bool Selection::empty() const {
    for (std::size_t c = 0; c < by_gsea.size(); ++c)
        if (chosen(static_cast<std::uint32_t>(c))) return false;
    return true;
}

Selection select(const Prerank& prerank, std::span<const Candidate> candidates, const Selector& selector,
                 std::span<const std::string> keep_ids, const core::Interner& /*ids: keep ids match gene_text*/) {
    Selection selection;
    selection.by_gsea.assign(candidates.size(), false);
    selection.kept_for.assign(candidates.size(), {});
    // S7.13: the thresholds first, then top:N keeps the first N passing pathways in table order
    // (|NES| descending, 08.C9). FuSION 1 applied the terms in turn (gsea_select_pathways.py:119-130),
    // so top:10,fdr:0.06 and fdr:0.06,top:10 differed. Untested candidates never pass (S7.09).
    std::uint32_t taken = 0;
    for (const Row& row : prerank.rows) {
        if (!row.score) continue;
        const Score& s = *row.score;
        if (selector.max_fdr && !(s.fdr_q <= *selector.max_fdr)) continue;
        if (selector.max_pval && !(s.nom_p <= *selector.max_pval)) continue;
        if (selector.min_abs_nes && !(std::fabs(s.nes) >= *selector.min_abs_nes)) continue;
        if (selector.top && taken >= *selector.top) break;
        selection.by_gsea[row.candidate] = true;
        ++taken;
    }
    // S7.14: each keep id chooses every candidate whose gene set holds it, tested or not, and
    // whatever its scores (gsea_select_pathways.py:205-211). kept_for lists the keep ids, in keep
    // order, of a candidate GSEA did not choose (:222). An id in no candidate is reported once.
    for (const std::string& id : keep_ids) {
        if (id.empty()) continue;   // gsea_select_pathways.py:176 drops empty items
        std::uint32_t holders = 0;
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            const auto& set = candidates[c].gene_text;
            const bool holds = std::binary_search(set.begin(), set.end(), id, [](const std::string& a, const std::string& b) {
                return core::stable_compare(a, b) < 0;
            });
            if (!holds) continue;
            ++holders;
            if (!selection.by_gsea[c]) selection.kept_for[c].push_back(id);
        }
        // gsea_select_pathways.py:206-209 counts per distinct id, in keep order.
        const bool repeated = std::any_of(selection.keep_counts.begin(), selection.keep_counts.end(),
                                          [&id](const auto& kc) { return kc.first == id; });
        if (repeated) continue;
        selection.keep_counts.emplace_back(id, holders);
        if (holders == 0) selection.unmatched.push_back(id);
    }
    return selection;
}

std::vector<std::filesystem::path> Shortlist::chosen_paths() const {
    std::vector<std::filesystem::path> paths;
    for (std::size_t c = 0; c < candidates.size(); ++c)
        if (selection.chosen(static_cast<std::uint32_t>(c))) paths.push_back(candidates[c].entry.path);
    return paths;
}

}  // namespace fusion::enrichment
