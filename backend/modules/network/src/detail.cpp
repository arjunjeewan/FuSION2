// network's private helpers (see detail.hpp).
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>
#include <variant>

namespace fusion::network::detail {

std::string display(const std::filesystem::path& path) { return core::to_utf8(path); }

std::string read_input(const std::filesystem::path& path) {
  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec || status.type() == std::filesystem::file_type::not_found) {
    if (ec && ec != std::errc::no_such_file_or_directory)
      throw core::Failure("input.unreadable", "cannot read " + display(path) + ": " + ec.message(),
                          {display(path), 0, {}});
    throw core::Failure("input.missing", "cannot read " + display(path) + ": no such file",
                        {display(path), 0, {}});
  }
  if (!std::filesystem::is_regular_file(status))
    throw core::Failure("input.not_file", display(path) + " is not a file", {display(path), 0, {}});
  std::ifstream in(path, std::ios::binary);
  if (!in) throw core::Failure("input.unreadable", "cannot open " + display(path), {display(path), 0, {}});
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) throw core::Failure("input.unreadable", "cannot read " + display(path), {display(path), 0, {}});
  return bytes;
}

std::string read_head(const std::filesystem::path& path, std::size_t max_bytes) {
  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec || status.type() == std::filesystem::file_type::not_found)
    throw core::Failure("input.missing", "cannot read " + display(path) + ": no such file", {display(path), 0, {}});
  if (!std::filesystem::is_regular_file(status))
    throw core::Failure("input.not_file", display(path) + " is not a file", {display(path), 0, {}});
  std::ifstream in(path, std::ios::binary);
  if (!in) throw core::Failure("input.unreadable", "cannot open " + display(path), {display(path), 0, {}});
  std::string bytes(max_bytes, '\0');
  in.read(bytes.data(), static_cast<std::streamsize>(max_bytes));
  bytes.resize(static_cast<std::size_t>(in.gcount()));
  if (in.bad()) throw core::Failure("input.unreadable", "cannot read " + display(path), {display(path), 0, {}});
  return bytes;
}

void fail(std::string code, const std::string& file, std::uint32_t line, const std::string& what) {
  throw core::Failure(std::move(code), file + ":" + std::to_string(line) + ": " + what, {file, line, {}});
}

LineIndex::LineIndex(std::string_view text) {
  for (std::size_t i = 0; i < text.size(); ++i)
    if (text[i] == '\n') newlines.push_back(i);
}

std::uint32_t LineIndex::line(std::ptrdiff_t offset) const {
  if (offset < 0) return 0;
  const auto it = std::lower_bound(newlines.begin(), newlines.end(), static_cast<std::size_t>(offset));
  return static_cast<std::uint32_t>(it - newlines.begin()) + 1;
}

std::optional<std::string> XmlTag::attr(std::string_view key) const {
  for (const auto& [k, v] : attrs)
    if (k == key) return v;
  return std::nullopt;
}

namespace {
std::string decode_entities(std::string_view v) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (v[i] != '&') { out += v[i]; continue; }
    const std::size_t semi = v.find(';', i);
    if (semi == std::string_view::npos) { out += v[i]; continue; }
    const auto name = v.substr(i + 1, semi - i - 1);
    if (name == "amp") out += '&';
    else if (name == "lt") out += '<';
    else if (name == "gt") out += '>';
    else if (name == "quot") out += '"';
    else if (name == "apos") out += '\'';
    else { out += v.substr(i, semi - i + 1); }
    i = semi;
  }
  return out;
}
bool name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
         c == '.' || c == ':' || static_cast<unsigned char>(c) >= 0x80;
}
}  // namespace

std::optional<XmlTag> scan_root(std::string_view text) {
  text = strip_bom(text);
  std::size_t i = 0;
  auto skip_ws = [&] { while (i < text.size() && is_space(text[i])) ++i; };
  for (;;) {
    skip_ws();
    if (i >= text.size() || text[i] != '<') return std::nullopt;
    if (text.substr(i, 4) == "<!--") {
      const auto end = text.find("-->", i + 4);
      if (end == std::string_view::npos) return std::nullopt;
      i = end + 3;
    } else if (text.substr(i, 2) == "<?") {
      const auto end = text.find("?>", i + 2);
      if (end == std::string_view::npos) return std::nullopt;
      i = end + 2;
    } else if (text.substr(i, 2) == "<!") {               // DOCTYPE, with a possible internal subset
      int depth = 0;
      char quote = 0;
      for (i += 2; i < text.size(); ++i) {
        const char c = text[i];
        if (quote) { if (c == quote) quote = 0; continue; }
        if (c == '"' || c == '\'') quote = c;
        else if (c == '[') ++depth;
        else if (c == ']') --depth;
        else if (c == '>' && depth <= 0) break;
      }
      if (i >= text.size()) return std::nullopt;
      ++i;
    } else {
      break;
    }
  }
  ++i;                                                    // past '<'
  XmlTag tag;
  while (i < text.size() && name_char(text[i])) tag.name += text[i++];
  if (tag.name.empty()) return std::nullopt;
  for (;;) {
    skip_ws();
    if (i >= text.size()) return std::nullopt;
    if (text[i] == '>' || text[i] == '/') return tag;
    std::string key;
    while (i < text.size() && name_char(text[i])) key += text[i++];
    if (key.empty()) return std::nullopt;
    skip_ws();
    if (i >= text.size() || text[i] != '=') return std::nullopt;
    ++i;
    skip_ws();
    if (i >= text.size() || (text[i] != '"' && text[i] != '\'')) return std::nullopt;
    const char quote = text[i++];
    const auto end = text.find(quote, i);
    if (end == std::string_view::npos) return std::nullopt;
    tag.attrs.emplace_back(std::move(key), decode_entities(text.substr(i, end - i)));
    i = end + 1;
  }
}

std::string_view strip_bom(std::string_view text) noexcept {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
    text.remove_prefix(3);
  return text;
}

std::string_view trim(std::string_view text) noexcept {
  while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
  while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
  return text;
}

std::vector<std::string_view> split_ws(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && is_space(text[i])) ++i;
    const std::size_t start = i;
    while (i < text.size() && !is_space(text[i])) ++i;
    if (i > start) out.push_back(text.substr(start, i - start));
  }
  return out;
}

std::vector<std::string_view> split_on(std::string_view text, char sep) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t at = text.find(sep, start);
    if (at == std::string_view::npos) {
      out.push_back(text.substr(start));
      return out;
    }
    out.push_back(text.substr(start, at - start));
    start = at + 1;
  }
}

std::string delete_colons(std::string_view id) {
  std::string out;
  out.reserve(id.size());
  for (char c : id)
    if (c != ':') out.push_back(c);
  return out;
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) out.append(sep);
    out.append(parts[i]);
  }
  return out;
}

std::optional<std::uint32_t> parse_uint(std::string_view text) noexcept {
  if (text.empty() || text.size() > 10) return std::nullopt;
  std::uint32_t value = 0;
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc() || ptr != text.data() + text.size()) return std::nullopt;
  for (char c : text)
    if (c < '0' || c > '9') return std::nullopt;     // from_chars takes no sign, but be explicit
  return value;
}

std::string format_score(double value) {
  std::array<char, 64> buf{};
  if (std::isfinite(value) && value == std::trunc(value) && std::fabs(value) < 9.0e15) {
    const auto whole = static_cast<long long>(value);
    const auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), whole);
    (void)ec;
    return std::string(buf.data(), ptr);
  }
  const auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);   // shortest
  (void)ec;
  return std::string(buf.data(), ptr);
}

std::optional<core::Sha256> sha_from_hex(std::string_view hex) noexcept {
  if (hex.size() != 64) return std::nullopt;
  core::Sha256 out;
  for (std::size_t i = 0; i < 32; ++i) {
    unsigned value = 0;
    for (std::size_t k = 0; k < 2; ++k) {
      const char c = hex[2 * i + k];
      unsigned digit = 0;
      if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
      else return std::nullopt;
      value = value * 16 + digit;
    }
    out.bytes[i] = static_cast<std::byte>(value);
  }
  return out;
}

std::size_t TextHash::operator()(std::string_view text) const noexcept {
  return static_cast<std::size_t>(core::stable_hash(text));   // core's fixed hash, never std::hash (09)
}

namespace {
std::optional<std::string> setting_text(const core::Settings& settings, std::string_view name) {
  try {
    return settings.find<std::string>(name);
  } catch (const std::bad_variant_access&) {
    return std::nullopt;
  } catch (const core::Failure&) {
    return std::nullopt;
  }
}
}  // namespace

RelationFilters filters_from(const core::Settings& settings) {
  RelationFilters f;
  if (settings.all().empty()) return f;
  if (auto v = setting_text(settings, "indirect")) f.indirect = *v == "keep" ? IndirectMode::keep : IndirectMode::discard;
  if (auto v = setting_text(settings, "ecrel")) f.ecrel = *v == "exclude" ? EcrelMode::exclude : EcrelMode::keep;
  if (auto v = setting_text(settings, "maplink")) f.maplink = *v == "keep" ? MaplinkMode::keep : MaplinkMode::skip;
  return f;
}

std::string_view format_name(Format format) noexcept {
  switch (format) {
    case Format::kgml: return "kgml";
    case Format::fusion_network: return "fusion";
    case Format::legacy: return "legacy";
    case Format::edge_table: return "edges";
  }
  return "kgml";
}

std::uint64_t elapsed_ms(std::chrono::steady_clock::time_point start) noexcept {
  const auto d = std::chrono::steady_clock::now() - start;
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(d).count());
}

bool is_undirected_subtype(std::string_view subtype) noexcept {
  return subtype == "association" || subtype == "dissociation" || subtype == "mapping";
}

}  // namespace fusion::network::detail

namespace fusion::network {

std::uint32_t PathwayDoc::Impl::add_list(const std::vector<Sym>& items) {
  list_items.insert(list_items.end(), items.begin(), items.end());
  list_first.push_back(static_cast<std::uint32_t>(list_items.size()));
  return static_cast<std::uint32_t>(list_first.size() - 2);
}

void PathwayDoc::Impl::index_entries() {
  by_local_id.clear();
  by_local_id.reserve(entries.size());
  for (std::size_t i = 0; i < entries.size(); ++i)
    by_local_id.emplace_back(entries[i].local_id, static_cast<std::uint32_t>(i));
  std::sort(by_local_id.begin(), by_local_id.end());
}

std::optional<std::uint32_t> PathwayDoc::Impl::position(std::uint32_t local_id) const noexcept {
  const auto it = std::lower_bound(by_local_id.begin(), by_local_id.end(), std::make_pair(local_id, std::uint32_t{0}));
  if (it == by_local_id.end() || it->first != local_id) return std::nullopt;
  return it->second;
}

}  // namespace fusion::network
