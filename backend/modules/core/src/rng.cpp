// fusion core: random numbers outside GSEA (09.C15; cross-section item 3A.17, 12.C9).
//
// FuSION 1's only randomness is Z3's own seed (Linux/src/main.cc:4531, 4732; spec S5.22). 2.0's
// other uses (section 12's property tests, the test kit's generators) draw from std::mt19937_64,
// whose output the standard fixes bit for bit, through FuSION's own distributions, because
// <random>'s distributions differ between libstdc++, libc++ and MSVC's library.
#include "fusion/core/core.hpp"

namespace fusion::core {

namespace {

constexpr std::uint64_t golden_gamma = 0x9e3779b97f4a7c15ULL;

// SplitMix64 (Steele, Lea and Flood, 2014), as in Vigna's splitmix64.c.
std::uint64_t splitmix64_next(std::uint64_t& state) noexcept {
    state += golden_gamma;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// The full 128-bit product of two 64-bit values, from 32-bit halves, so every platform computes
// it the same way without compiler extensions.
void multiply_128(std::uint64_t a, std::uint64_t b, std::uint64_t& high, std::uint64_t& low) noexcept {
    const std::uint64_t a_lo = a & 0xffffffffULL, a_hi = a >> 32;
    const std::uint64_t b_lo = b & 0xffffffffULL, b_hi = b >> 32;
    const std::uint64_t lo_lo = a_lo * b_lo;
    const std::uint64_t hi_lo = a_hi * b_lo;
    const std::uint64_t lo_hi = a_lo * b_hi;
    const std::uint64_t hi_hi = a_hi * b_hi;
    const std::uint64_t cross = (lo_lo >> 32) + (hi_lo & 0xffffffffULL) + lo_hi;
    high = hi_hi + (hi_lo >> 32) + (cross >> 32);
    low = (cross << 32) | (lo_lo & 0xffffffffULL);
}

}  // namespace

Rng Rng::stream(std::uint64_t seed, std::uint64_t stream_id) {
    // Stream k of a seed is seeded with output k + 1 of SplitMix64 started at the seed, so a
    // stream depends only on (seed, k), whatever the thread count (09.C15). Skipping k outputs of
    // SplitMix64 is adding k times its increment to the state.
    std::uint64_t state = seed + stream_id * golden_gamma;
    return Rng(std::mt19937_64(splitmix64_next(state)));
}

std::uint64_t Rng::next() { return engine_(); }

std::uint64_t Rng::below(std::uint64_t n) {
    // Lemire's nearly divisionless method ("Fast random integer generation in an interval",
    // ACM TOMACS 29(1), 2019): the high half of x * n, rejecting the few low halves below 2^64 mod n.
    if (n == 0) throw Failure("internal", "Rng::below(0): the range is empty");
    std::uint64_t high = 0, low = 0;
    multiply_128(next(), n, high, low);
    if (low < n) {
        const std::uint64_t threshold = (0 - n) % n;   // 2^64 mod n
        while (low < threshold) multiply_128(next(), n, high, low);
    }
    return high;
}

double Rng::unit() {
    return static_cast<double>(next() >> 11) * 0x1.0p-53;   // 53 random bits: k / 2^53
}

}  // namespace fusion::core
