// fusion core: GSEA's random stream (08.C1, cross-section item 1.19).
//
// FuSION 1 ran gseapy 1.3.1 through python3 (Linux/supporting_files/gsea_select_pathways.py, called
// from Linux/src/main.cc:5796-5808; spec S7.02), whose Rust core draws its permutations from rand
// 0.9.2's SmallRng (gseapy core algorithm.rs:507-521, stats.rs:597-641). Each function below is a
// line-for-line port of the rand 0.9.2 source cited beside it, so 2.0's permutations are gseapy's.
//
// Copyright 2018-2023 Developers of the Rand project; ported under the MIT licence.
#include "fusion/core/xoshiro.hpp"

#include <utility>

#include "fusion/core/core.hpp"

namespace fusion::core {

namespace {

constexpr std::uint64_t rotl(std::uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }

// rand 0.9.2 src/seq/increasing_uniform.rs:81-108, calculate_bound_u32: the product
// m (m+1) ... (m+count-1), the largest such product below 2^32, and its count.
std::pair<std::uint32_t, std::uint8_t> calculate_bound_u32(std::uint32_t m) noexcept {
    std::uint64_t product = m;
    std::uint64_t current = std::uint64_t{m} + 1;
    while (product * current <= 0xffffffffULL) {
        product *= current;
        ++current;
    }
    return {static_cast<std::uint32_t>(product), static_cast<std::uint8_t>(current - m)};
}

}  // namespace

// rand 0.9.2 src/rngs/xoshiro256plusplus.rs:50-66, seed_from_u64: SplitMix64, four outputs.
Xoshiro256pp::Xoshiro256pp(std::uint64_t seed) {
    constexpr std::uint64_t phi = 0x9e3779b97f4a7c15ULL;
    for (std::uint64_t& word : s_) {
        seed += phi;
        std::uint64_t z = seed;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        word = z ^ (z >> 31);
    }
}

// xoshiro256plusplus.rs:78-96, next_u64.
std::uint64_t Xoshiro256pp::next_u64() {
    const std::uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
}

// xoshiro256plusplus.rs:70-75, next_u32: the upper bits, whose linear dependencies are weaker.
std::uint32_t Xoshiro256pp::next_u32() { return static_cast<std::uint32_t>(next_u64() >> 32); }

// rand 0.9.2 src/distr/uniform_int.rs:153-209 for u32 (sample_single, then sample_single_inclusive
// without the "unbiased" feature): Canon's method. The high half of a 32 x 32-bit product; when
// the low half exceeds 2^32 - range, a second draw's high half is added to it, and a carry adds 1.
std::uint32_t Xoshiro256pp::below(std::uint32_t bound) {
    if (bound == 0) throw Failure("internal", "Xoshiro256pp::below(0): the range is empty");
    const std::uint32_t range = bound;                        // high - low + 1 with low 0, high bound-1
    const std::uint64_t product = std::uint64_t{next_u32()} * range;
    std::uint32_t result = static_cast<std::uint32_t>(product >> 32);
    const auto low_order = static_cast<std::uint32_t>(product);
    if (low_order > static_cast<std::uint32_t>(0u - range)) {
        const auto new_high = static_cast<std::uint32_t>((std::uint64_t{next_u32()} * range) >> 32);
        const bool overflow = std::uint64_t{low_order} + new_high > 0xffffffffULL;
        result += overflow ? 1u : 0u;
    }
    return result;
}

// rand 0.9.2 src/seq/slice.rs:373-413 (shuffle, partial_shuffle with amount = len, so m = 0) and
// src/seq/increasing_uniform.rs:24-76 (IncreasingUniform::new and next_index): for i = 0 to N-1,
// swap element i with j, uniform in 0..=i; one bounded draw serves several consecutive i.
void shuffle_like_rand09(std::span<std::uint32_t> a, Xoshiro256pp& rng) {
    if (a.size() <= 1) return;                   // rand returns before drawing anything
    std::uint32_t n = 0;                         // IncreasingUniform::new(rng, 0)
    std::uint32_t chunk = 0;
    std::uint8_t chunk_remaining = 1;            // n == 0: the first index is 0, with no draw
    for (std::size_t i = 0; i < a.size(); ++i) {
        const std::uint32_t next_n = n + 1;
        std::uint8_t next_remaining;
        if (chunk_remaining == 0) {
            const auto [bound, remaining] = calculate_bound_u32(next_n);
            chunk = rng.below(bound);
            next_remaining = static_cast<std::uint8_t>(remaining - 1);
        } else {
            next_remaining = static_cast<std::uint8_t>(chunk_remaining - 1);
        }
        std::uint32_t index;
        if (next_remaining == 0) {
            index = chunk;
        } else {
            index = chunk % next_n;
            chunk /= next_n;
        }
        chunk_remaining = next_remaining;
        n = next_n;
        std::swap(a[i], a[index]);
    }
}

}  // namespace fusion::core
