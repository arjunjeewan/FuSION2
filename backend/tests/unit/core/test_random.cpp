// Section 12's core row: core::Rng (09.C15) and GSEA's generator (08.C1, cross-section item 1.19).
// Every expected value below comes from golden_rng.py, an independent Python implementation of
// the published algorithms, which itself reproduces the standard's and rand 0.9.2's own values.
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"
#include "fusion/core/xoshiro.hpp"

using namespace fusion::core;

TEST_CASE("core: std::mt19937_64 is the standard's engine, bit for bit", "[unit][core]") {
    std::mt19937_64 engine;
    engine.discard(9999);
    CHECK(engine() == 9981545732273789042ULL);   // the 10,000th output, as [rand.predef] requires
}

TEST_CASE("core: Rng streams have fixed first values", "[unit][core]") {
    Rng a = Rng::stream(42, 0);
    CHECK(a.next() == 2576493707698874361ULL);
    CHECK(a.next() == 17880808640956396325ULL);
    CHECK(a.next() == 17896956056310571724ULL);
    Rng b = Rng::stream(42, 1);
    CHECK(b.next() == 11572511668999661906ULL);
    CHECK(b.next() == 15353713867457212013ULL);
    CHECK(b.next() == 10791255225403203334ULL);
}

TEST_CASE("core: Rng's bounded draws, unit doubles and shuffle are core's own, with fixed values", "[unit][core]") {
    Rng r = Rng::stream(42, 0);
    std::vector<std::uint64_t> draws;
    for (int i = 0; i < 8; ++i) draws.push_back(r.below(3054));
    CHECK(draws == std::vector<std::uint64_t>{426, 2960, 2962, 759, 2129, 1907, 1201, 1451});

    Rng u = Rng::stream(42, 0);
    CHECK(u.unit() == 0.13967200376411748);
    CHECK(u.unit() == 0.9693205787161252);
    CHECK(u.unit() == 0.9701959318564763);

    Rng s = Rng::stream(42, 0);
    std::vector<int> items(10);
    std::iota(items.begin(), items.end(), 0);
    s.shuffle(items.begin(), items.end());
    CHECK(items == std::vector<int>{2, 0, 5, 6, 3, 4, 9, 7, 8, 1});

    Rng big = Rng::stream(7, 3);                // a range above 2^63 takes Lemire's rejection path
    CHECK(big.below((1ULL << 63) + 1) == 1601773810146034146ULL);
    CHECK(big.below((1ULL << 63) + 1) == 8996656093051932975ULL);
}

TEST_CASE("core: Rng's bounded draw stays in range and refuses an empty one", "[unit][core]") {
    Rng r = Rng::stream(1, 0);
    for (int i = 0; i < 1000; ++i) CHECK(r.below(1) == 0);
    for (const std::uint64_t n : {2ULL, 3ULL, 7ULL, 1000ULL, (1ULL << 32) + 1, ~0ULL}) CHECK(r.below(n) < n);
    for (int i = 0; i < 1000; ++i) {
        const double x = r.unit();
        CHECK(x >= 0.0);
        CHECK(x < 1.0);
    }
    CHECK_THROWS_AS(r.below(0), Failure);
}

TEST_CASE("core: a stream depends only on its seed and id, whatever the thread count", "[unit][core]") {
    const auto draw = [](std::uint64_t id) {
        Rng r = Rng::stream(2026, id);
        std::vector<std::uint64_t> out;
        for (int i = 0; i < 100; ++i) out.push_back(r.below(1000));
        return out;
    };
    std::vector<std::vector<std::uint64_t>> one(8), eight(8);
    for (std::uint64_t id = 0; id < 8; ++id) one[id] = draw(id);
    std::vector<std::thread> threads;
    for (std::uint64_t id = 0; id < 8; ++id) threads.emplace_back([&, id] { eight[id] = draw(id); });
    for (auto& t : threads) t.join();
    CHECK(one == eight);
    CHECK(one[0] != one[1]);
}

TEST_CASE("core: Xoshiro256pp(0) starts with rand 0.9.2's own test values", "[unit][core]") {
    Xoshiro256pp rng(0);   // rand's stable_seed_from_u64_and_from_seed
    CHECK(rng.next_u64() == 5987356902031041503ULL);
    CHECK(rng.next_u64() == 7051070477665621255ULL);
    CHECK(rng.next_u64() == 6633766593972829180ULL);
    Xoshiro256pp a(9), b(9);
    CHECK(a.next_u32() == static_cast<std::uint32_t>(b.next_u64() >> 32));   // the top half
    CHECK(a == b);
}

TEST_CASE("core: Xoshiro256pp's bounded draw is Canon's method, as rand's random_range", "[unit][core]") {
    Xoshiro256pp rng(42);
    std::vector<std::uint32_t> got;
    for (const std::uint32_t bound : {1u, 2u, 3u, 10u, 479001600u, 4294967295u, 4294967295u}) got.push_back(rng.below(bound));
    CHECK(got == std::vector<std::uint32_t>{0, 0, 2, 7, 380089920, 2525863679, 2598981126});
    CHECK_THROWS_AS(rng.below(0), Failure);
}

namespace {

// The bounded draws a shuffle of n items makes: one per chunk of IncreasingUniform (rand 0.9.2
// increasing_uniform.rs), counted independently of the shuffle itself.
int chunks_for(std::uint32_t n) {
    int chunks = 0;
    for (std::uint64_t m = 2; m <= n;) {
        std::uint64_t product = m, next = m + 1;
        while (product * next <= 0xffffffffULL) product *= next++;
        m = next;
        ++chunks;
    }
    return chunks;
}

}  // namespace

TEST_CASE("core: shuffle_like_rand09 gives gseapy's permutations (08.C1)", "[unit][core]") {
    // Section 08's test hook: the first shuffle of 0 to 3053 with seed 42, its 1228 bounded draws
    // and 1378 32-bit draws; the permutations chain, each shuffling the last in place.
    Xoshiro256pp rng(42);
    std::vector<std::uint32_t> p(3054);
    std::iota(p.begin(), p.end(), 0u);
    shuffle_like_rand09(p, rng);
    CHECK(std::vector<std::uint32_t>(p.begin(), p.begin() + 10) ==
          std::vector<std::uint32_t>{2614, 515, 2774, 958, 1504, 1071, 1924, 2135, 1637, 2881});
    CHECK(chunks_for(3054) == 1228);
    Xoshiro256pp counter(42);
    int u32_draws = 0;
    while (!(counter == rng) && u32_draws < 5000) {
        counter.next_u64();
        ++u32_draws;
    }
    CHECK(u32_draws == 1378);
    std::vector<std::uint32_t> sorted = p;
    std::sort(sorted.begin(), sorted.end());
    CHECK(sorted.front() == 0);
    CHECK(sorted.back() == 3053);
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());   // a permutation

    shuffle_like_rand09(p, rng);
    CHECK(std::vector<std::uint32_t>(p.begin(), p.begin() + 10) ==
          std::vector<std::uint32_t>{507, 1573, 571, 237, 635, 2754, 234, 809, 142, 240});

    Xoshiro256pp small(5);
    std::vector<std::uint32_t> q(13);
    std::iota(q.begin(), q.end(), 0u);
    shuffle_like_rand09(q, small);              // 13 items: a chunk boundary at 12 (rand's RESULT2)
    CHECK(q == std::vector<std::uint32_t>{2, 7, 1, 11, 8, 12, 5, 3, 4, 6, 9, 0, 10});
}

TEST_CASE("core: shuffling zero or one item draws nothing", "[unit][core]") {
    Xoshiro256pp rng(42), untouched(42);
    std::vector<std::uint32_t> none, one{7};
    shuffle_like_rand09(none, rng);
    shuffle_like_rand09(one, rng);
    CHECK(one == std::vector<std::uint32_t>{7});
    CHECK(rng == untouched);
}
