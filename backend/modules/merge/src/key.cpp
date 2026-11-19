// The merge key (03.C8): the SHA-256 of a format line with the rules version, section 02's build
// settings by registry key, the id map's digest when the run gives one, and one line per input, sorted,
// each line once. Paths, list order, E, the label maps and FuSION's own version stay out, so byte-identical
// copies share a key and a rescored copy under the same name does not (section 03, "Reuse").
#include <algorithm>

#include "engine.hpp"

namespace fusion::merge {

namespace {

std::string_view text(network::IndirectMode m) noexcept {
    return m == network::IndirectMode::discard ? "discard" : "keep";
}
std::string_view text(network::EcrelMode m) noexcept { return m == network::EcrelMode::keep ? "keep" : "exclude"; }
std::string_view text(network::MaplinkMode m) noexcept { return m == network::MaplinkMode::skip ? "skip" : "keep"; }

bool no_map(const core::Sha256& digest) {
    return digest == network::IdMap::none().digest() || digest == core::Sha256{};
}

std::string key_text(std::span<const network::SourceRef> inputs, const network::BuildSettings& s,
                     const core::Sha256& id_map) {
    std::string out = "fusion-base-merge " + std::to_string(rules_version) + "\n";
    out += "network.split_threshold=" + std::to_string(s.split_threshold) + "\n";
    out += "network.indirect=" + std::string(text(s.filters.indirect)) + "\n";
    out += "network.ecrel=" + std::string(text(s.filters.ecrel)) + "\n";
    out += "network.maplink=" + std::string(text(s.filters.maplink)) + "\n";
    if (!no_map(id_map)) out += "network.id_map=" + id_map.hex() + "\n";   // it changes the documents' ids (02.C3)
    std::vector<std::string> lines;
    lines.reserve(inputs.size());
    for (const network::SourceRef& in : inputs) {
        std::string line = in.sha256.hex();
        // An edge table takes its default pathway id from its file name (02.C2, 2B.12), so two identical
        // tables under two names stay two inputs with two pathways.
        if (in.format == network::Format::edge_table) line += " " + in.pathway;
        lines.push_back(std::move(line));
    }
    std::sort(lines.begin(), lines.end(), detail::ByteLess{});
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    for (const std::string& line : lines) out += line + "\n";   // every line, the last included, ends with LF
    return out;
}

}  // namespace

std::string MergeKey::hex() const { return value.hex(); }

std::string merge_key_text(std::span<const network::SourceRef> inputs, const network::BuildSettings& s,
                           const network::IdMap& map) {
    return key_text(inputs, s, map.digest());
}

MergeKey merge_key(std::span<const network::SourceRef> inputs, const network::BuildSettings& s,
                   const network::IdMap& map) {
    return MergeKey{core::Sha256::of(merge_key_text(inputs, s, map))};
}

namespace detail {

MergeKey key_of_docs(std::span<const DocPtr> docs, const network::BuildSettings& s) {
    std::vector<network::SourceRef> inputs;
    inputs.reserve(docs.size());
    for (const DocPtr& d : docs) inputs.push_back(d->source());
    const core::Sha256 id_map = docs.empty() ? network::IdMap::none().digest() : docs.front()->id_map();
    return MergeKey{core::Sha256::of(key_text(inputs, s, id_map))};
}

bool same_id_map(const core::Sha256& a, const core::Sha256& b) { return a == b || (no_map(a) && no_map(b)); }

}  // namespace detail

}  // namespace fusion::merge
