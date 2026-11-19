// network's private helpers: file and text handling shared by the readers and writers, and the
// state behind PathwayDoc, which several readers fill. Not part of the module's interface.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/network/network.hpp"

namespace fusion::network::detail {

// ---- files (decision 0021: a named file that cannot be read stops the run) ----
// Reads a whole input file in binary mode. Throws core::Failure input.missing, input.not_file or
// input.unreadable, naming the file.
std::string read_input(const std::filesystem::path& path);
std::string read_head(const std::filesystem::path& path, std::size_t max_bytes);   // the same failures
std::string display(const std::filesystem::path& path);   // UTF-8, '/' separators, for messages

// A failure naming the file and line: "<file>:<line>: <what>".
[[noreturn]] void fail(std::string code, const std::string& file, std::uint32_t line, const std::string& what);

struct LineIndex {                                       // byte offset -> line number, from 1
  std::vector<std::size_t> newlines;
  explicit LineIndex(std::string_view text);
  std::uint32_t line(std::ptrdiff_t offset) const;
};

// ---- the first XML element, without a parser (format detection reads at most 64 KiB) ----
struct XmlTag {
  std::string name;
  std::vector<std::pair<std::string, std::string>> attrs;
  std::optional<std::string> attr(std::string_view key) const;
};
std::optional<XmlTag> scan_root(std::string_view text);   // skips the declaration, comments, DOCTYPE

// ---- text ----
inline bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}
std::string_view strip_bom(std::string_view text) noexcept;   // a leading UTF-8 byte-order mark
std::string_view trim(std::string_view text) noexcept;        // ASCII whitespace at both ends
std::vector<std::string_view> split_ws(std::string_view text);   // runs of ASCII whitespace
std::vector<std::string_view> split_on(std::string_view text, char sep);   // every field, empty ones too
std::string delete_colons(std::string_view id);               // the id rule's first step (02.C3)
std::string join(const std::vector<std::string>& parts, std::string_view sep);

// Calls f(line, number) for every line, numbered from 1. Accepts LF, CRLF and a lone CR, and skips a
// UTF-8 byte-order mark; the line ends are not passed on.
template <class F>
void for_each_line(std::string_view text, F&& f) {
  text = strip_bom(text);
  std::uint32_t number = 0;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t end = pos;
    while (end < text.size() && text[end] != '\n' && text[end] != '\r') ++end;
    f(text.substr(pos, end - pos), ++number);
    if (end < text.size() && text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n') ++end;
    pos = end + 1;
  }
}

// ---- numbers ----
std::optional<std::uint32_t> parse_uint(std::string_view text) noexcept;   // digits only
// A score as FuSION writes it (02.C4): a whole value as an integer (FuSION 1's form), anything else
// as the shortest text that reads back exactly. Never the locale.
std::string format_score(double value);
std::optional<core::Sha256> sha_from_hex(std::string_view hex) noexcept;

// ---- lookups only; never iterated for output (09's determinism rules) ----
struct TextHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view text) const noexcept;   // core::stable_hash
};
struct TextEq {
  using is_transparent = void;
  bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
};

// ---- settings and formats ----
RelationFilters filters_from(const core::Settings& settings);   // the run's values, else defaults
std::string_view format_name(Format format) noexcept;           // "kgml", "fusion", "legacy", "edges"
std::uint64_t elapsed_ms(std::chrono::steady_clock::time_point start) noexcept;

// The subtypes that make a relation undirected (S2.10; Linux/src/XMLParser.cc:1476-1478).
bool is_undirected_subtype(std::string_view subtype) noexcept;

// KEGG's relation types as KEGG spells them (S2.30): true for an exact match; a case variant returns
// false with the right spelling in `right`, anything else false with `right` empty.
bool kegg_type(std::string_view type, std::string& right);
bool kgml_subtype(std::string_view subtype);   // KGML's vocabulary, normalised (S2.10), plus component

}  // namespace fusion::network::detail

namespace fusion::network {

// The state behind a PathwayDoc. Readers fill it and hand it to PathwayDoc's constructor.
struct PathwayDoc::Impl {
  SourceRef source;
  core::Interner interner;
  std::vector<Entry> entries;                  // file order
  std::vector<GeneId> ids;                     // entry id ranges
  std::vector<std::uint32_t> comps;            // entry component ranges (local ids)
  std::vector<Relation> relations;             // file order
  std::vector<Sym> list_items;                 // subtype lists and pathway sets, by list id:
  std::vector<std::uint32_t> list_first{0};    // list i is list_items[list_first[i], list_first[i+1])
  std::vector<std::pair<std::uint32_t, std::uint32_t>> by_local_id;   // (local id, position), sorted
  core::Sha256 id_map;

  std::uint32_t add_list(const std::vector<Sym>& items);   // a new list id
  void index_entries();                                    // fills by_local_id
  std::optional<std::uint32_t> position(std::uint32_t local_id) const noexcept;
};

namespace detail {

// Reads a KGML-shaped file (<pathway> with <entry> and <relation> children) into a document, with
// S2.06's checks. A KGML's relations carry the root's pathway id; a FuSION 1-written file's carry
// their own `pathways` attribute (S2.12). Counts what it read for the network.read event.
struct ReadCounts { std::uint32_t unscored = 0, bad_scores = 0; };
std::unique_ptr<PathwayDoc::Impl> parse_pathway_xml(const std::string& bytes, const std::filesystem::path& path,
                                                    const IdMap& ids, Format format, core::Context& ctx,
                                                    ReadCounts& counts);

// S2.06's group checks over a filled document: components that name no entry, groups with no
// components, and cycles. entry_lines[i] is entry i's line; is_group[i] whether it is a group.
void check_groups(const PathwayDoc::Impl& doc, const std::string& file, const std::vector<std::uint32_t>& entry_lines,
                  const std::vector<bool>& is_group);

}  // namespace detail
}  // namespace fusion::network
