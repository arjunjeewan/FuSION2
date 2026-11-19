// fusion network: the id rule, the id map and the two label maps (02.C3; S1.16, S1.18, S1.19, S2.14,
// S2.15; items 1.12, 2B.4, 2B.14).
#include <algorithm>
#include <map>

#include "detail.hpp"
#include "fusion/core/install.hpp"

namespace fusion::network {

// ---------------------------------------------------------------- label maps (S1.18, S1.19)

struct LabelMap::Impl {
  struct Value { std::string name; std::uint32_t line = 0; };
  std::map<std::string, Value, std::less<>> rows;   // byte order of id
  core::Sha256 digest;
};

LabelMap::LabelMap() : impl_(std::make_shared<Impl>()) {}

// One reader for both maps, as S1.18 and S1.19 decided: the id is the text before the first tab, the
// name the rest of the line, with further tabs as '_'; blank lines are skipped and a later line wins.
// FuSION 1 kept only the first word of a name (Linux/src/GraphManagerNew.cc:8135-8137, 8164-8166).
LabelMap LabelMap::read(const std::filesystem::path& path, core::Context& ctx) {
  const std::string bytes = detail::read_input(path);
  auto impl = std::make_shared<Impl>();
  impl->digest = core::Sha256::of(bytes);
  detail::for_each_line(bytes, [&](std::string_view line, std::uint32_t number) {
    if (detail::trim(line).empty()) return;
    const std::size_t tab = line.find('\t');
    const std::string_view id = detail::trim(line.substr(0, tab));
    if (id.empty()) return;
    std::string name;
    if (tab != std::string_view::npos) {
      name = std::string(detail::trim(line.substr(tab + 1)));
      std::replace(name.begin(), name.end(), '\t', '_');
    }
    auto& slot = impl->rows[std::string(id)];
    slot.name = std::move(name);
    slot.line = number;
  });
  ctx.log.debug("read " + std::to_string(impl->rows.size()) + " labels from " + detail::display(path));
  LabelMap out;
  out.impl_ = std::move(impl);
  return out;
}

std::optional<std::string_view> LabelMap::find(std::string_view id) const noexcept {
  const auto it = impl_->rows.find(id);                     // never inserts (S2.15)
  if (it == impl_->rows.end() || it->second.name.empty()) return std::nullopt;
  return std::string_view(it->second.name);
}

// The fallback to the id for a missing or empty name (Linux/src/GraphManagerNew.cc:8777-8781).
std::string_view LabelMap::label_or_id(std::string_view id) const noexcept {
  if (auto name = find(id)) return *name;
  return id;
}

const core::Sha256& LabelMap::digest() const noexcept { return impl_->digest; }

std::vector<LabelMap::Row> LabelMap::rows() const {
  std::vector<Row> out;
  out.reserve(impl_->rows.size());
  for (const auto& [id, value] : impl_->rows) out.push_back({id, value.name, value.line});
  return out;
}

std::size_t LabelMap::size() const noexcept { return impl_->rows.size(); }

// ---------------------------------------------------------------- the id map (S1.16, 02.C3)

struct IdMap::Impl {
  bool none = true;
  std::map<std::string, std::string, std::less<>> map;   // left -> right, both through ':' deletion
  core::Sha256 digest;
};

IdMap::IdMap() : impl_(std::make_shared<Impl>()) {}
IdMap IdMap::none() { return IdMap(); }

namespace {
std::string sample(const std::vector<std::string>& items) {
  std::string out;
  for (std::size_t i = 0; i < items.size() && i < 5; ++i) out += (i ? ", " : "") + items[i];
  if (items.size() > 5) out += ", ...";
  return out;
}
}  // namespace

// S1.16's reader exactly (Linux/src/GraphManagerNew.cc:8006-8017): the first two whitespace-separated
// tokens, a later line winning, extra tokens ignored. A blank line maps "" to "" and a one-token line
// maps its id to "", so that id matches nothing. 2.0 adds the colon rule on both columns and warnings.
IdMap IdMap::read(const std::filesystem::path& two_column, core::Context& ctx) {
  const std::string bytes = detail::read_input(two_column);
  auto impl = std::make_shared<Impl>();
  impl->none = false;
  impl->digest = core::Sha256::of(bytes);
  std::vector<std::string> one_token, repeats;
  detail::for_each_line(bytes, [&](std::string_view line, std::uint32_t number) {
    const auto tokens = detail::split_ws(line);
    std::string left = tokens.empty() ? std::string() : detail::delete_colons(tokens[0]);
    std::string right = tokens.size() < 2 ? std::string() : detail::delete_colons(tokens[1]);
    if (tokens.size() == 1) one_token.push_back(std::to_string(number) + " (" + left + ")");
    auto [it, inserted] = impl->map.try_emplace(std::move(left), right);
    if (!inserted) {
      if (it->second != right && !it->first.empty())
        repeats.push_back(it->first + ": " + it->second + " then " + right);
      it->second = std::move(right);                         // a later line wins
    }
  });
  const std::string name = detail::display(two_column);
  if (!one_token.empty())
    ctx.log.warning("network.id_map_line", name + ": " + std::to_string(one_token.size()) +
                    " line(s) with one token map their id to nothing (line " + sample(one_token) + ")");
  if (!repeats.empty())
    ctx.log.warning("network.id_map_repeat", name + ": " + std::to_string(repeats.size()) +
                    " id(s) mapped twice; the later line wins (" + sample(repeats) + ")");
  IdMap out;
  out.impl_ = std::move(impl);
  return out;
}

// id_map=symbols (1.12): the symbol map read backwards, symbol -> id. A symbol that names two ids
// takes the later line's, and is reported (8 of 6,326 symbols in FuSION 1's map).
IdMap IdMap::reverse_symbols(const LabelMap& symbols, core::Context& ctx) {
  auto rows = symbols.rows();
  std::stable_sort(rows.begin(), rows.end(), [](const LabelMap::Row& a, const LabelMap::Row& b) {
    return a.line < b.line;
  });
  auto impl = std::make_shared<Impl>();
  impl->none = false;
  impl->digest = core::Sha256::of("fusion:reverse_symbols:" + symbols.digest().hex());
  std::map<std::string, std::vector<std::string>, std::less<>> named;
  for (const auto& row : rows) {
    if (row.name.empty()) continue;
    std::string symbol = detail::delete_colons(row.name);
    std::string id = detail::delete_colons(row.id);
    auto& ids = named[symbol];
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    impl->map[symbol] = std::move(id);
  }
  std::vector<std::string> ambiguous;
  for (const auto& [symbol, ids] : named)
    if (ids.size() > 1) ambiguous.push_back(symbol + ": " + detail::join(ids, ", "));
  if (!ambiguous.empty())
    ctx.log.warning("network.id_map_repeat", "id_map=symbols: " + std::to_string(ambiguous.size()) +
                    " symbol(s) name more than one id; the later line wins (" + sample(ambiguous) + ")");
  IdMap out;
  out.impl_ = std::move(impl);
  return out;
}

std::string IdMap::apply(std::string_view raw) const {
  std::string id = detail::delete_colons(raw);
  if (impl_->none) return id;
  const auto it = impl_->map.find(id);
  return it == impl_->map.end() ? id : it->second;           // mapped once; no chaining
}

GeneId IdMap::canonical(std::string_view raw, core::Interner& interner) const {
  return interner.id(apply(raw));
}

std::optional<GeneId> IdMap::lookup(std::string_view raw, core::Interner& interner) const {
  std::string id = detail::delete_colons(raw);
  if (impl_->none) return interner.id(id);
  const auto it = impl_->map.find(id);
  if (it == impl_->map.end() || it->second.empty()) return std::nullopt;
  return interner.id(it->second);
}

bool IdMap::is_none() const noexcept { return impl_->none; }
const core::Sha256& IdMap::digest() const noexcept { return impl_->digest; }

// ---------------------------------------------------------------- shipped maps (2B.4, 2B.14)

// Section 11's data/ in the home, found by core's install layer (11.C19, 11.C20); a missing file is
// core's install error (install.file_missing). No id map is shipped.
Maps shipped_maps(core::Context& ctx) {
  Maps maps;
  maps.symbols = LabelMap::read(core::home_file("data/hsa_to_gene_symbol_map.txt"), ctx);
  maps.pathway_names = LabelMap::read(core::home_file("data/hsa_path_to_path_name_map.txt"), ctx);
  return maps;
}

// ---------------------------------------------------------------- IdSet

IdSet::IdSet(std::vector<std::string> ids) : ids_(std::move(ids)) {
  std::sort(ids_.begin(), ids_.end());                       // std::string's order is byte order
  ids_.erase(std::unique(ids_.begin(), ids_.end()), ids_.end());
}

bool IdSet::contains(std::string_view id) const noexcept {
  return std::binary_search(ids_.begin(), ids_.end(), id,
                            [](std::string_view a, std::string_view b) { return a < b; });
}

}  // namespace fusion::network
