// fusion network: stable keys, node kinds, subtype names and the one polarity rule
// (section 02, "The polarity rule"; S2.10, S4.14, S6.14, 02.C11; items 2B.1, 2B.18).
#include <algorithm>

#include "detail.hpp"

namespace fusion::network {

// 02.C11: "src|tgt|type|sub1,sub2". '|' occurs in no KEGG, Entrez, Ensembl or UniProt id.
std::string EdgeKey::text() const {
  std::string out = source.value + '|' + target.value + '|' + type + '|';
  for (std::size_t i = 0; i < subtypes.size(); ++i) {
    if (i) out += ',';
    out += subtypes[i];
  }
  return out;
}

EdgeKey EdgeKey::parse(std::string_view text) {
  const auto parts = detail::split_on(text, '|');
  if (parts.size() != 4 || parts[0].empty() || parts[1].empty() || parts[2].empty())
    throw core::Failure("network.parse", "not an edge key (source|target|type|subtypes): " + std::string(text));
  EdgeKey key{{std::string(parts[0])}, {std::string(parts[1])}, std::string(parts[2]), {}};
  if (!parts[3].empty())
    for (auto s : detail::split_on(parts[3], ',')) key.subtypes.emplace_back(s);
  return key;
}

NodeKind kind_of(std::string_view entry_type) noexcept {
  if (entry_type == "gene") return NodeKind::gene;
  if (entry_type == "group") return NodeKind::complex;
  if (entry_type == "compound") return NodeKind::compound;
  return NodeKind::other;
}

// S5.13, S6.14: inhibition iff the list holds inhibition or repression; everything else, an edge
// with no subtype included, is in the activation family (Linux/src/GraphManagerNew.cc:28550).
Effect effect_of(std::span<const std::string> subtypes) {
  const bool inhibits = std::any_of(subtypes.begin(), subtypes.end(), [](const std::string& s) {
    return s == "inhibition" || s == "repression";
  });
  return inhibits ? Effect::inhibition : Effect::activation;
}

// FuSION 1's relaxation rule (Linux/src/GraphManagerNew.cc:28550-28594, S5.13): an inhibition edge
// is relaxed when both ends are present and both active or both inactive; an activation edge when
// both ends are present and not both active. An edge with an absent end is never relaxed.
bool is_relaxed(Effect effect, EndState source, EndState target) noexcept {
  if (source == EndState::absent || target == EndState::absent) return false;
  if (effect == Effect::inhibition) return source == target;
  return !(source == EndState::active && target == EndState::active);
}

// S2.10's table (Linux/src/XMLParser.cc:1463-1474); every other name is kept verbatim.
std::string_view normalise_subtype(std::string_view name) noexcept {
  if (name == "indirect effect") return "indirect";
  if (name == "missing interaction") return "missing";
  if (name == "state change") return "state";
  if (name == "binding/association") return "association";
  return name;
}

}  // namespace fusion::network
