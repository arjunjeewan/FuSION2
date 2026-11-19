// fusion core: SHA-256 (FIPS 180-4), "a small vendored implementation" (section 09, Interfaces
// with 11; cross-section item 1.1). It hashes inputs as they are copied (09.C3), outputs as they
// are committed, the batch file, and fingerprints (09.C5). FuSION 1 hashed nothing; its users ran
// sha256sum by hand to compare re-solves (09.C5's Why).
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

#include "fusion/core/core.hpp"

namespace fusion::core {

namespace {

constexpr std::array<std::uint32_t, 64> k_round = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept { return (x >> n) | (x << (32 - n)); }

}  // namespace

struct Sha256Builder::Impl {
    std::array<std::uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> block{};
    std::size_t used = 0;                        // bytes waiting in block
    std::uint64_t total = 0;                     // message length in bytes
    bool finished = false;

    void compress(const unsigned char* p) noexcept {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i)
            w[i] = (std::uint32_t{p[4 * i]} << 24) | (std::uint32_t{p[4 * i + 1]} << 16) |
                   (std::uint32_t{p[4 * i + 2]} << 8) | std::uint32_t{p[4 * i + 3]};
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + s1 + ch + k_round[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const unsigned char* data, std::size_t size) noexcept {
        total += size;
        if (used > 0) {
            const std::size_t take = std::min(size, block.size() - used);
            std::memcpy(block.data() + used, data, take);
            used += take;
            data += take;
            size -= take;
            if (used < block.size()) return;
            compress(block.data());
            used = 0;
        }
        for (; size >= 64; data += 64, size -= 64) compress(data);
        if (size > 0) {
            std::memcpy(block.data(), data, size);
            used = size;
        }
    }
};

Sha256Builder::Sha256Builder() : impl_(std::make_unique<Impl>()) {}
Sha256Builder::~Sha256Builder() = default;

void Sha256Builder::update(std::span<const std::byte> data) {
    impl_->update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

void Sha256Builder::update(std::string_view data) {
    impl_->update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

Sha256 Sha256Builder::finish() {
    Impl& s = *impl_;
    if (s.finished) throw Failure("internal", "Sha256Builder::finish called twice");
    s.finished = true;
    const std::uint64_t bits = s.total * 8;
    const unsigned char pad = 0x80;
    s.update(&pad, 1);
    const unsigned char zero = 0;
    while (s.used != 56) s.update(&zero, 1);
    std::array<unsigned char, 8> length{};
    for (std::size_t i = 0; i < 8; ++i) length[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
    s.update(length.data(), length.size());
    Sha256 digest;
    for (std::size_t i = 0; i < 8; ++i)
        for (std::size_t j = 0; j < 4; ++j)
            digest.bytes[4 * i + j] = static_cast<std::byte>(s.h[i] >> (24 - 8 * j));
    return digest;
}

std::string Sha256::hex() const {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (const std::byte b : bytes) {
        const auto v = std::to_integer<unsigned>(b);
        out.push_back(digits[v >> 4]);
        out.push_back(digits[v & 0xF]);
    }
    return out;
}

Sha256 Sha256::of(std::string_view data) {
    Sha256Builder builder;
    builder.update(data);
    return builder.finish();
}

Sha256 Sha256::of_file(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw Failure("input.unreadable", "cannot read " + to_utf8(file), Where{to_utf8(file), 0, {}});
    Sha256Builder builder;
    std::array<char, 1 << 16> buffer{};
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto got = in.gcount();
        if (got > 0) builder.update(std::string_view(buffer.data(), static_cast<std::size_t>(got)));
    }
    if (in.bad()) throw Failure("input.unreadable", "error reading " + to_utf8(file), Where{to_utf8(file), 0, {}});
    return builder.finish();
}

}  // namespace fusion::core
