// The prerank engine (section 08 "Algorithms": the permutation chain, enrichment scores, the
// observed score and leading edge, and the statistics; S7.09 to S7.12, 08.C1, 08.C6 to 08.C9).
//
// FuSION 1 ran gseapy 1.3.1's prerank (gsea_select_pathways.py:185-193). This file ports the
// Rust core behind it, src/algorithm.rs and src/stats.rs at tag v1.3.1 of
// github.com/zqfang/GSEApy, operation for operation, so that its doubles are gseapy's (08.C1).
// That code carries this notice:
//
//   BSD 3-Clause License
//
//   Copyright (c) 2016-2020, Zhuoqing Fang
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The permutations come from core's port of rand 0.9.2 (fusion/core/xoshiro.hpp, 08.C1).
// Everything runs on one thread (08.C6). No exp, log or pow is called (09.C16).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "detail.hpp"
#include "fusion/core/xoshiro.hpp"
#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {

void check_settings(const Settings& settings) {
    // 08.C8: the null matrix takes 8·S·P bytes, so a typo's extra zeros fail here.
    if (settings.permutations < 1 || settings.permutations > max_permutations)
        throw core::Failure("batch.bad_value",
                            "permutations takes a count from 1 to 100000, not " +
                                std::to_string(settings.permutations),
                            core::Where{{}, 0, "gsea.permutations"});
}

namespace detail {

bool table_before(const Row& a, const Row& b, std::span<const Candidate> candidates) {
    // gseapy base.py:811 and gsea_select_pathways.py:200 sorted by |NES| with pandas' unstable
    // sort, leaving equal |NES| in no defined order; 08.C9 orders them by pathway id.
    const double x = std::fabs(a.score->nes), y = std::fabs(b.score->nes);
    if (x != y) return x > y;
    if (const auto c = core::stable_compare(candidates[a.candidate].id, candidates[b.candidate].id); c != 0)
        return c < 0;
    return a.candidate < b.candidate;
}

Statistics statistics(std::span<const double> observed, std::span<double> nulls, std::uint32_t permutations) {
    const std::size_t sets = observed.size();
    const std::size_t p_count = permutations;
    Statistics st;
    st.nes.resize(sets);
    st.nom_p.resize(sets);
    st.fdr_q.resize(sets);
    st.fwer_p.resize(sets);

    for (std::size_t s = 0; s < sets; ++s) {
        const double e = observed[s];
        const std::span<double> row = nulls.subspan(s * p_count, p_count);
        // Nominal p (gseapy core stats.rs:197-216): for e < 0 the core counts null <= e, where
        // gseapy's Python reference counted < e (S7.11's Phase 2 correction).
        std::size_t deno = 0, nomi = 0;
        for (const double x : row) {
            if (e < 0.0) {
                deno += x < 0.0 ? 1U : 0U;
                nomi += x <= e ? 1U : 0U;
            } else {
                deno += x >= 0.0 ? 1U : 0U;
                nomi += x >= e ? 1U : 0U;
            }
        }
        st.nom_p[s] = deno == 0 ? 1.0 : static_cast<double>(nomi) / static_cast<double>(deno);
        // NES and the normalised nulls (stats.rs:142-195): each mean is the sum in permutation
        // order over the count; with no null of the needed sign, the mean is e itself.
        double pos_sum = 0.0, neg_sum = 0.0;
        std::size_t pos_count = 0, neg_count = 0;
        for (const double x : row) {
            if (x >= 0.0) {
                pos_sum += x;
                ++pos_count;
            } else if (x < 0.0) {
                neg_sum += x;
                ++neg_count;
            }
        }
        const double pos_mean = pos_count > 0 ? pos_sum / static_cast<double>(pos_count) : e;
        const double neg_mean = neg_count > 0 ? neg_sum / static_cast<double>(neg_count) : e;
        st.nes[s] = e >= 0.0 ? e / pos_mean : e / std::fabs(neg_mean);
        for (double& x : row) x = x >= 0.0 ? x / pos_mean : x / std::fabs(neg_mean);
    }

    // FWER p (stats.rs:495-524): per permutation, the largest non-negative and the smallest
    // negative normalised null over all sets, each 0 when there is none.
    std::vector<double> largest(p_count, 0.0), smallest(p_count, 0.0);
    for (std::size_t s = 0; s < sets; ++s) {
        for (std::size_t k = 0; k < p_count; ++k) {
            const double x = nulls[s * p_count + k];
            if (x >= 0.0) {
                if (x > largest[k]) largest[k] = x;
            } else if (x < smallest[k]) {
                smallest[k] = x;
            }
        }
    }
    for (std::size_t s = 0; s < sets; ++s) {
        const double e = st.nes[s];
        if (e < 0.0) {
            std::size_t below = 0, negative = 0;
            for (const double x : smallest) {
                below += x < e ? 1U : 0U;
                negative += x < 0.0 ? 1U : 0U;
            }
            // 08.C7: gseapy divides 0 by 0 here and gets NaN; 2.0 writes 1.0, as gseapy does
            // for an undefined nominal p.
            st.fwer_p[s] = negative == 0 ? 1.0 : static_cast<double>(below) / static_cast<double>(negative);
        } else {
            std::size_t above = 0;
            for (const double x : largest) above += x >= e ? 1U : 0U;
            st.fwer_p[s] = static_cast<double>(above) / static_cast<double>(p_count);
        }
    }

    // FDR q, pooled over every tested set (stats.rs:377-445; S7.12), counted by binary search
    // in the sorted normalised nulls, after FWER has read them. No monotone adjustment: gseapy
    // never calls adjust_fdr (stats.rs:439).
    std::sort(nulls.begin(), nulls.end());
    std::vector<double> nes_sorted = st.nes;
    std::sort(nes_sorted.begin(), nes_sorted.end());
    const auto count_below = [](std::span<const double> v, double x) {   // #(v < x)
        return static_cast<std::size_t>(std::lower_bound(v.begin(), v.end(), x) - v.begin());
    };
    const auto count_at_most = [](std::span<const double> v, double x) {   // #(v <= x)
        return static_cast<std::size_t>(std::upper_bound(v.begin(), v.end(), x) - v.begin());
    };
    const std::span<const double> pooled = nulls;
    const std::size_t null_negative = count_below(pooled, 0.0);
    const std::size_t nes_negative = count_below(nes_sorted, 0.0);
    for (std::size_t s = 0; s < sets; ++s) {
        const double e = st.nes[s];
        std::size_t nes_higher, all_higher, all_pos, nes_pos;
        if (e < 0.0) {
            nes_higher = count_at_most(nes_sorted, e);
            all_higher = count_at_most(pooled, e);
            all_pos = null_negative;
            nes_pos = nes_negative;
        } else {
            nes_higher = nes_sorted.size() - count_below(nes_sorted, e);
            all_higher = pooled.size() - count_below(pooled, e);
            all_pos = pooled.size() - null_negative;
            nes_pos = nes_sorted.size() - nes_negative;
        }
        const double phi_norm = all_pos > 0 ? static_cast<double>(all_higher) / static_cast<double>(all_pos) : 0.0;
        const double phi_obs = nes_pos > 0 ? static_cast<double>(nes_higher) / static_cast<double>(nes_pos) : 0.0;
        st.fdr_q[s] = phi_obs == 0.0 ? 1.0 : std::clamp(phi_norm / phi_obs, 0.0, 1.0);
    }
    return st;
}

LeadingEdge leading_edge(std::span<const double> weight, std::span<const std::uint32_t> hits, double es) {
    // The running sum at every position (gseapy core algorithm.rs:219-242): w_p · (1/W) at a
    // hit and −1/(N − m) at a miss, accumulated in position order, W summed in position order.
    const std::size_t n = weight.size(), m = hits.size();
    double sum = 0.0;
    for (const std::uint32_t h : hits) sum += weight[h];
    const bool uniform = sum == 0.0;   // 08.C7: every hit has value 0, so each steps by 1/m
    const double norm_tag = uniform ? 1.0 / static_cast<double>(m) : 1.0 / sum;
    const double norm_notag = 1.0 / (static_cast<double>(n) - static_cast<double>(m));
    // gseapy base.py:657-668: the first maximum for ES >= 0, else the first minimum.
    double running = 0.0, best = 0.0;
    std::uint32_t peak = 0;
    std::size_t next = 0;
    for (std::size_t p = 0; p < n; ++p) {
        const bool hit = next < m && hits[next] == p;
        if (hit) ++next;
        running += hit ? (uniform ? norm_tag : weight[p] * norm_tag) : -norm_notag;
        if (p == 0 || (es >= 0.0 ? running > best : running < best)) {
            best = running;
            peak = static_cast<std::uint32_t>(p);
        }
    }
    // The hits up to the peak in ranking order, or from the peak on, listed bottom up.
    LeadingEdge le;
    le.peak = peak;
    if (es >= 0.0) {
        for (const std::uint32_t h : hits)
            if (h <= peak) le.positions.push_back(h);
    } else {
        for (auto it = hits.rbegin(); it != hits.rend(); ++it)
            if (*it >= peak) le.positions.push_back(*it);
    }
    return le;
}

}  // namespace detail

Prerank prerank(const Ranking& ranking, std::span<const Candidate> candidates, const Settings& settings,
                const core::StopToken& stop) {
    check_settings(settings);
    const auto n = static_cast<std::uint32_t>(ranking.id.size());
    const std::uint32_t permutations = settings.permutations;

    // Each set's hits: the ranked positions of its genes, ascending (gseapy core stats.rs:609-610).
    std::vector<std::pair<std::uint32_t, std::uint32_t>> where(n);   // (gene handle, position)
    for (std::uint32_t p = 0; p < n; ++p) where[p] = {ranking.id[p].value, p};
    std::sort(where.begin(), where.end());
    std::vector<std::vector<std::uint32_t>> hits(candidates.size());
    std::vector<std::uint32_t> tested;   // candidate indices, list order
    for (std::size_t c = 0; c < candidates.size(); ++c) {
        for (const core::GeneId g : candidates[c].genes) {
            const auto it = std::lower_bound(where.begin(), where.end(), std::pair{g.value, std::uint32_t{0}});
            if (it != where.end() && it->first == g.value) hits[c].push_back(it->second);
        }
        std::sort(hits[c].begin(), hits[c].end());
        hits[c].erase(std::unique(hits[c].begin(), hits[c].end()), hits[c].end());
        // gseapy's filter (base.py:497): 15 to 500 ranked genes, and fewer than N (S7.09).
        const auto m = static_cast<std::uint32_t>(hits[c].size());
        if (m >= min_size && m <= max_size && m < n) tested.push_back(static_cast<std::uint32_t>(c));
    }
    // S7.09: FuSION 1 ended here in gseapy's LookupError traceback (base.py:517).
    if (tested.empty()) throw core::Failure("gsea.no_testable", "no candidate has 15 to 500 ranked genes");
    const std::size_t sets = tested.size();

    // The membership index: for each ranked gene, the tested sets that hold it (08 "Data").
    std::vector<std::uint32_t> first(static_cast<std::size_t>(n) + 1, 0);
    for (const std::uint32_t c : tested)
        for (const std::uint32_t g : hits[c]) ++first[g + 1];
    std::partial_sum(first.begin(), first.end(), first.begin());
    std::vector<std::uint32_t> member(first[n]);
    {
        std::vector<std::uint32_t> fill(first.begin(), first.end() - 1);
        for (std::size_t s = 0; s < sets; ++s)
            for (const std::uint32_t g : hits[tested[s]]) member[fill[g]++] = static_cast<std::uint32_t>(s);
    }

    // The walks of every set over one chain of permutations (gseapy core algorithm.rs:302-340,
    // 507-520; stats.rs:597-641). Permutation 0 is the identity and gives the observed scores;
    // permutation k is permutation k − 1 shuffled in place. Position p holds ranked gene
    // perm[p], weighted by the value ranked at p.
    std::vector<double> observed(sets), nulls(sets * permutations);
    std::vector<double> q1(sets), weight_sum(sets), q2(sets), cur(sets), extreme(sets), last(sets);
    std::vector<unsigned char> uniform(sets);
    for (std::size_t s = 0; s < sets; ++s)
        q1[s] = 1.0 / (static_cast<double>(n) - static_cast<double>(hits[tested[s]].size()));
    std::vector<std::uint32_t> perm(n);
    std::iota(perm.begin(), perm.end(), 0U);
    core::Xoshiro256pp rng(settings.seed);
    Prerank result;
    for (std::uint32_t k = 0; k <= permutations; ++k) {
        if (k % 64 == 0) stop.throw_if_stopped();   // 08 "Stop": every 64 permutations
        if (k > 0) core::shuffle_like_rand09(perm, rng);
        // Pass 1: W_s, summed in position order, as fast_random_walk's ns (algorithm.rs:304-308).
        std::fill(weight_sum.begin(), weight_sum.end(), 0.0);
        for (std::uint32_t p = 0; p < n; ++p) {
            const std::uint32_t g = perm[p];
            const double w = ranking.weight[p];
            for (std::uint32_t i = first[g]; i < first[g + 1]; ++i) weight_sum[member[i]] += w;
        }
        for (std::size_t s = 0; s < sets; ++s) {
            // 08.C7: when every hit of the draw has value 0, gseapy's 1/W is infinite; each hit
            // instead steps by 1/m, fgsea's rule (gseapy core algorithm.rs:70-76). The count for
            // the warning includes the observed walk, permutation 0, when its hits are all 0.
            uniform[s] = weight_sum[s] == 0.0 ? 1 : 0;
            if (uniform[s] != 0) {
                q2[s] = 1.0 / static_cast<double>(hits[tested[s]].size());
                ++result.zero_weight_draws;
            } else {
                q2[s] = 1.0 / weight_sum[s];
            }
        }
        // Pass 2: the walk (algorithm.rs:322-332), each set's steps in position order; the score
        // is the first value with the largest |running sum|.
        std::fill(cur.begin(), cur.end(), 0.0);
        std::fill(extreme.begin(), extreme.end(), 0.0);
        std::fill(last.begin(), last.end(), -1.0);
        for (std::uint32_t p = 0; p < n; ++p) {
            const std::uint32_t g = perm[p];
            const double w = ranking.weight[p];
            const auto pos = static_cast<double>(p);
            for (std::uint32_t i = first[g]; i < first[g + 1]; ++i) {
                const std::uint32_t s = member[i];
                double c = cur[s];
                c -= q1[s] * (pos - last[s] - 1.0);
                if (std::fabs(c) > std::fabs(extreme[s])) extreme[s] = c;
                c += q2[s] * (uniform[s] != 0 ? 1.0 : w);
                if (std::fabs(c) > std::fabs(extreme[s])) extreme[s] = c;
                cur[s] = c;
                last[s] = pos;
            }
        }
        if (k == 0) {
            observed = extreme;
        } else {
            for (std::size_t s = 0; s < sets; ++s) nulls[s * permutations + (k - 1)] = extreme[s];
        }
    }

    const detail::Statistics st = detail::statistics(observed, nulls, permutations);
    result.tested = static_cast<std::uint32_t>(sets);
    result.rows.reserve(candidates.size());
    for (std::size_t s = 0; s < sets; ++s) {
        const std::uint32_t c = tested[s];
        detail::LeadingEdge le = detail::leading_edge(ranking.weight, hits[c], observed[s]);
        result.rows.push_back(Row{c, static_cast<std::uint32_t>(hits[c].size()),
                                  Score{observed[s], st.nes[s], st.nom_p[s], st.fdr_q[s], st.fwer_p[s],
                                        std::move(le.positions), le.peak}});
    }
    std::sort(result.rows.begin(), result.rows.end(),
              [candidates](const Row& a, const Row& b) { return detail::table_before(a, b, candidates); });
    // S7.09: every candidate gets a row; the untested follow in list order, without scores.
    std::size_t next_tested = 0;
    for (std::size_t c = 0; c < candidates.size(); ++c) {
        if (next_tested < tested.size() && tested[next_tested] == c) {
            ++next_tested;
            continue;
        }
        result.rows.push_back(Row{static_cast<std::uint32_t>(c), static_cast<std::uint32_t>(hits[c].size()), std::nullopt});
    }
    return result;
}

}  // namespace fusion::enrichment
