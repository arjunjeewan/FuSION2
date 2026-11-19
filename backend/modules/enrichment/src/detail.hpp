// The enrichment module's internal pieces, shared by its sources and reached by its unit tests,
// which include this file by its relative path. Not part of the public interface.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment::detail {

/// rank()'s sort, on (gene, finite value) pairs: value descending, then id ascending in byte
/// order (S7.05, 08.C2).
[[nodiscard]] Ranking rank_pairs(std::vector<std::pair<core::GeneId, double>> pairs,
                                 const core::Interner& ids);

/// The statistics of S tested sets, as gseapy 1.3.1's core computes them (stats.rs:142-216,
/// 377-445, 495-524). `observed` holds the S observed scores; `nulls` holds S rows of P null
/// scores, row s for set s in permutation order. The rows are normalised in place, and the
/// pooled FDR then sorts the whole matrix (08 "Statistics").
struct Statistics {
    std::vector<double> nes, nom_p, fdr_q, fwer_p;
};
[[nodiscard]] Statistics statistics(std::span<const double> observed, std::span<double> nulls,
                                    std::uint32_t permutations);

/// The leading edge and peak of one set, from the running sum gseapy writes out at every
/// position (gseapy core algorithm.rs:219-242) and gseapy base.py:657-672's reading of it.
/// `hits` are the set's ranked positions, ascending; `es` is its observed score.
struct LeadingEdge {
    std::vector<std::uint32_t> positions;     // Lead_genes order
    std::uint32_t              peak = 0;      // the running sum's first extreme
};
[[nodiscard]] LeadingEdge leading_edge(std::span<const double> weight,
                                       std::span<const std::uint32_t> hits, double es);

/// `value` with `decimals` digits after the point, correctly rounded, as Python's
/// format(value, ".Nf") writes it; locale-free (std::to_chars).
[[nodiscard]] std::string fixed(double value, int decimals);
/// As Python's format(value, "+.Nf"): the sign always written.
[[nodiscard]] std::string signed_fixed(double value, int decimals);
/// As Python's "{:.2%}".format(share): the share times 100, two decimals and "%".
[[nodiscard]] std::string percent(double share);
/// `Gene %` (gseapy base.py:661, 666): (peak + 1) / N for es >= 0, (N - peak) / N otherwise.
[[nodiscard]] std::string gene_percent(std::uint32_t peak, std::uint32_t n, double es);

/// Code points in UTF-8 text: what Python's len() and its padding count (S7.17's columns).
[[nodiscard]] std::size_t code_points(std::string_view text) noexcept;
/// Tab, CR and LF become spaces, so a field cannot break a TSV row (08 "Files written").
[[nodiscard]] std::string tsv_field(std::string_view text);

/// Table order (08.C9): |NES| descending, then pathway id ascending in byte order, then list order.
[[nodiscard]] bool table_before(const Row& a, const Row& b, std::span<const Candidate> candidates);

}  // namespace fusion::enrichment::detail
