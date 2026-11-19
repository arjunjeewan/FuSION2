// fusion core: ids and stable orderings (section 09; section 02's outline, cross-section item 2A.5;
// 02.C8, 02.C10).
//
// FuSION 1 keyed nodes and edges by strings in std::map and std::unordered_map members of
// GraphManagerNew and EdgeNew (Linux/include/GraphManagerNew.h:127-194), so its output order
// followed whatever container it used (spec S4.14). Here every string is interned once to a
// 32-bit handle, handles follow first appearance and are never used for order, and every order
// compares the strings' bytes with stable_compare.
#include <algorithm>
#include <cstring>
#include <deque>
#include <unordered_map>

#include "fusion/core/core.hpp"

namespace fusion::core {

std::strong_ordering stable_compare(std::string_view a, std::string_view b) noexcept {
    const std::size_t n = std::min(a.size(), b.size());
    const int c = n == 0 ? 0 : std::memcmp(a.data(), b.data(), n);   // memcmp compares unsigned bytes
    if (c != 0) return c < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    return a.size() <=> b.size();
}

std::uint64_t stable_hash(std::string_view text) noexcept {
    // FNV-1a over the bytes, then SplitMix64's finaliser so the low bits that hash tables use are
    // well mixed. A fixed function, the same on every platform; never std::hash (09, Determinism).
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ULL;
    }
    h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
    return h ^ (h >> 31);
}

namespace {

struct StableHash {
    std::size_t operator()(std::string_view s) const noexcept { return static_cast<std::size_t>(stable_hash(s)); }
};

// One table of strings: a deque never moves its elements, so the views that index them, and those
// str() hands out, stay valid while the interner lives.
struct Table {
    std::deque<std::string> strings;
    std::unordered_map<std::string_view, std::uint32_t, StableHash> index;

    std::uint32_t intern(std::string_view s) {
        if (auto it = index.find(s); it != index.end()) return it->second;
        const auto handle = static_cast<std::uint32_t>(strings.size());
        const std::string& stored = strings.emplace_back(s);
        index.emplace(std::string_view(stored), handle);
        return handle;
    }
    std::optional<std::uint32_t> find(std::string_view s) const noexcept {
        if (auto it = index.find(s); it != index.end()) return it->second;
        return std::nullopt;
    }
    std::string_view str(std::uint32_t handle) const noexcept {
        return handle < strings.size() ? std::string_view(strings[handle]) : std::string_view();
    }
};

}  // namespace

struct Interner::Impl {
    Table ids;                                   // entity ids and node keys
    Table syms;                                  // types, subtypes, pathway ids and other strings
};

Interner::Interner() : impl_(std::make_unique<Impl>()) {}
Interner::~Interner() = default;
Interner::Interner(Interner&&) noexcept = default;
Interner& Interner::operator=(Interner&&) noexcept = default;

GeneId Interner::id(std::string_view s) { return GeneId{impl_->ids.intern(s)}; }
Sym Interner::sym(std::string_view s) { return Sym{impl_->syms.intern(s)}; }

std::optional<GeneId> Interner::find_id(std::string_view s) const noexcept {
    if (auto h = impl_->ids.find(s)) return GeneId{*h};
    return std::nullopt;
}
std::optional<Sym> Interner::find_sym(std::string_view s) const noexcept {
    if (auto h = impl_->syms.find(s)) return Sym{*h};
    return std::nullopt;
}

std::string_view Interner::str(GeneId id) const noexcept { return impl_->ids.str(id.value); }
std::string_view Interner::str(Sym s) const noexcept { return impl_->syms.str(s.value); }
std::size_t Interner::id_count() const noexcept { return impl_->ids.strings.size(); }

}  // namespace fusion::core
