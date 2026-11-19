// fusion/core/xoshiro.hpp: GSEA's random stream, an exact port of the one gseapy 1.3.1 draws from
// (08.C1; cross-section item 1.19). 09's core::Rng serves everything else.
//
// Ported from the Rust crate rand 0.9.2 (MIT or Apache-2.0; its MIT notice ships with libfusion,
// section 11's licences): SmallRng on 64-bit targets, which is Xoshiro256++ seeded by SplitMix64;
// Canon's bounded draw; and SliceRandom::shuffle with IncreasingUniform.
#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace fusion::core {

class Xoshiro256pp {                               // rand 0.9.2's SmallRng on 64-bit targets
public:
    explicit Xoshiro256pp(std::uint64_t seed);     // four SplitMix64 steps
    std::uint64_t next_u64();
    std::uint32_t next_u32();                      // the top 32 bits of next_u64()
    std::uint32_t below(std::uint32_t bound);      // Canon's method, as rand's random_range(..bound);
                                                   // bound must be at least 1
    friend bool operator==(const Xoshiro256pp&, const Xoshiro256pp&) = default;   // same state
private:
    std::array<std::uint64_t, 4> s_{};
};

/// Fisher-Yates as rand 0.9.2's SliceRandom::shuffle, with IncreasingUniform.
void shuffle_like_rand09(std::span<std::uint32_t> a, Xoshiro256pp& rng);

}  // namespace fusion::core
