// fusion network: pathway documents, format detection, the KGML reader, the relation filters and
// the split rule (section 02, "Reading network inputs"; 02.C1, 02.C6, 02.C7; S2.05 to S2.07, S2.11,
// S2.12, S2.31; entry 205).
#include <algorithm>
#include <fstream>
#include <map>

#include <pugixml.hpp>

#include "detail.hpp"

namespace fusion::network {

// ---------------------------------------------------------------- PathwayDoc

PathwayDoc::PathwayDoc(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PathwayDoc::PathwayDoc(PathwayDoc&&) noexcept = default;
PathwayDoc& PathwayDoc::operator=(PathwayDoc&&) noexcept = default;
PathwayDoc::~PathwayDoc() = default;

const SourceRef& PathwayDoc::source() const noexcept { return impl_->source; }
std::span<const Entry> PathwayDoc::entries() const noexcept { return impl_->entries; }
std::span<const Relation> PathwayDoc::relations() const noexcept { return impl_->relations; }
std::span<const GeneId> PathwayDoc::ids(const Entry& e) const noexcept {
  return {impl_->ids.data() + e.ids_first, e.ids_count};
}
std::span<const std::uint32_t> PathwayDoc::components(const Entry& e) const noexcept {
  return {impl_->comps.data() + e.comps_first, e.comps_count};
}
std::span<const Sym> PathwayDoc::subtypes(const Relation& r) const noexcept {
  return {impl_->list_items.data() + impl_->list_first[r.subtypes], impl_->list_first[r.subtypes + 1] - impl_->list_first[r.subtypes]};
}
std::span<const Sym> PathwayDoc::pathways(const Relation& r) const noexcept {
  return {impl_->list_items.data() + impl_->list_first[r.pathways], impl_->list_first[r.pathways + 1] - impl_->list_first[r.pathways]};
}
const Entry* PathwayDoc::entry(std::uint32_t local_id) const noexcept {
  const auto& idx = impl_->by_local_id;
  const auto it = std::lower_bound(idx.begin(), idx.end(), std::make_pair(local_id, std::uint32_t{0}));
  if (it == idx.end() || it->first != local_id) return nullptr;
  return &impl_->entries[it->second];
}

// The file's GSEA gene set (S7.08, 08.C3): every id of every type="gene" entry, sorted by content.
std::vector<GeneId> PathwayDoc::gene_ids() const {
  std::vector<GeneId> out;
  for (const Entry& e : impl_->entries)
    if (impl_->interner.str(e.type) == "gene")
      for (GeneId id : ids(e)) out.push_back(id);
  std::sort(out.begin(), out.end(), [&](GeneId a, GeneId b) { return impl_->interner.str(a) < impl_->interner.str(b); });
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
const core::Sha256& PathwayDoc::id_map() const noexcept { return impl_->id_map; }
const core::Interner& PathwayDoc::interner() const noexcept { return impl_->interner; }

// ---------------------------------------------------------------- format detection (02.C1)

namespace {
std::string describe(Format f) {
  switch (f) {
    case Format::kgml: return "KGML";
    case Format::fusion_network: return "a FuSION network file";
    case Format::legacy: return "a FuSION 1 network file";
    case Format::edge_table: return "an edge table";
  }
  return "KGML";
}
}  // namespace

// Reads at most 64 KiB. XML by its root <pathway>: format="fusion-network" is FuSION's own file, an
// `org` attribute is KGML (KEGG requires it; FuSION 1's tool-written files have none), anything else
// a file FuSION 1 wrote. Text: a headered edge table, or add's four-field lines. FuSION 1 trusted the
// y|n flag instead (S2.12; Linux/src/XMLParser.cc:1493-1544); 2.0 compares it with the content.
Sniff sniff(const std::filesystem::path& path, std::optional<bool> tool_written, core::Context& ctx) {
  const std::string head = detail::read_head(path, 64 * 1024);
  const std::string_view text = detail::strip_bom(head);
  std::size_t first = 0;
  while (first < text.size() && detail::is_space(text[first])) ++first;
  Sniff out;
  const std::string name = detail::display(path);
  if (first < text.size() && text[first] == '<') {
    const auto root = detail::scan_root(text);
    if (!root) throw core::Failure("network.unknown_format", name + ": no XML root element in its first 64 KiB", {name, 0, {}});
    if (root->name != "pathway")
      throw core::Failure("network.unknown_format", name + ": the root element is <" + root->name +
                          ">, not <pathway>", {name, 0, {}});
    if (root->attr("format") == std::optional<std::string>("fusion-network")) {
      out.format = Format::fusion_network;
      out.version = static_cast<int>(detail::parse_uint(root->attr("version").value_or("0")).value_or(0));
    } else if (root->attr("org")) {
      out.format = Format::kgml;
    } else {
      out.format = Format::legacy;
    }
  } else {
    bool found = false;
    detail::for_each_line(text, [&](std::string_view line, std::uint32_t) {
      if (found) return;
      const auto t = detail::trim(line);
      if (t.empty() || t.front() == '#') return;
      found = true;
      const auto fields = detail::split_on(line, '\t');
      bool source = false, target = false;
      for (auto f : fields) {
        source = source || detail::trim(f) == "source";
        target = target || detail::trim(f) == "target";
      }
      if (source && target) out.format = Format::edge_table;
      else if (detail::split_ws(line).size() == 4) out.format = Format::edge_table;
      else throw core::Failure("network.unknown_format", name + ": neither XML nor an edge table", {name, 0, {}});
    });
    if (!found) throw core::Failure("network.unknown_format", name + ": no content to read", {name, 0, {}});
  }
  if (tool_written) {
    const bool written = out.format == Format::fusion_network || out.format == Format::legacy;
    if (*tool_written != written) {
      const std::string step = ctx.step.empty() ? std::string("the command") : std::string(ctx.step);
      ctx.log.warning("network.flag_mismatch", step + " was given " + (*tool_written ? "y" : "n") + ", but " +
                      name + " is " + describe(out.format) + "; read as " + describe(out.format));
    }
  }
  return out;
}

// ---------------------------------------------------------------- the KGML reader

namespace detail {

// S2.06's checks for a document's structure, shared by every reader of entries and relations:
// components that name a missing entry, groups with no components, and cyclic groups, found by a
// three-colour depth-first search in O(entries + components). FuSION 1 had the cycle check but its
// call is commented out (Linux/src/XMLParser.cc:104-138, 657-661), so a cycle overflowed the stack;
// a missing component dereferenced end() (1084) and an empty group failed an assert (99).
void check_groups(const PathwayDoc::Impl& doc, const std::string& file, const std::vector<std::uint32_t>& entry_lines,
                  const std::vector<bool>& is_group) {
  for (std::size_t i = 0; i < doc.entries.size(); ++i) {
    const Entry& e = doc.entries[i];
    if (is_group[i] && e.comps_count == 0)
      fail("network.empty_group", file, entry_lines[i], "group entry " + std::to_string(e.local_id) + " has no components");
    for (std::uint32_t k = 0; k < e.comps_count; ++k) {
      const std::uint32_t c = doc.comps[e.comps_first + k];
      if (!doc.position(c))
        fail("network.dangling_ref", file, entry_lines[i], "entry " + std::to_string(e.local_id) +
             " names component " + std::to_string(c) + ", which is not an entry");
    }
  }
  std::vector<std::uint8_t> colour(doc.entries.size(), 0);   // 0 white, 1 grey, 2 black
  for (std::size_t start = 0; start < doc.entries.size(); ++start) {
    if (colour[start] != 0) continue;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> stack{{static_cast<std::uint32_t>(start), 0}};
    colour[start] = 1;
    while (!stack.empty()) {
      const std::uint32_t node = stack.back().first;
      const Entry& e = doc.entries[node];
      if (stack.back().second < e.comps_count) {
        const std::uint32_t child = *doc.position(doc.comps[e.comps_first + stack.back().second++]);
        if (colour[child] == 1)
          fail("network.cyclic_group", file, entry_lines[child], "group entry " +
               std::to_string(doc.entries[child].local_id) + " contains itself through its components");
        if (colour[child] == 0) {
          colour[child] = 1;
          stack.emplace_back(child, 0);
        }
      } else {
        colour[node] = 2;
        stack.pop_back();
      }
    }
  }
}

// Reads what S2.06 keeps: the root's name (else the file stem, 2B.12) and title (entry 205), each
// entry's id, name, type and components, and each relation's ends, type, subtypes and score.
// Graphics, coordinates, links and reactions are ignored. FuSION 1: Linux/src/XMLParser.cc:621-722,
// 875-888 (names: ':' deleted, split into a sorted set), 1451-1479 (subtypes), 1519-1522 (scores),
// 1533-1544 (a tool-written file's `pathways` attribute, split into a sorted set).
std::unique_ptr<PathwayDoc::Impl> parse_pathway_xml(const std::string& bytes, const std::filesystem::path& path,
                                                    const IdMap& ids, Format format, core::Context& ctx,
                                                    ReadCounts& counts) {
  const std::string file = display(path);
  const LineIndex lines(bytes);
  pugi::xml_document xml;                                   // never loads DTDs or external entities
  const pugi::xml_parse_result parsed = xml.load_buffer(bytes.data(), bytes.size(), pugi::parse_default);
  if (!parsed) fail("network.parse", file, lines.line(parsed.offset), parsed.description());
  const pugi::xml_node root = xml.document_element();
  if (std::string_view(root.name()) != "pathway")
    fail("network.parse", file, lines.line(root.offset_debug()), "the root element is <" + std::string(root.name()) + ">, not <pathway>");

  auto impl = std::make_unique<PathwayDoc::Impl>();
  impl->source.path = path;
  impl->source.size = bytes.size();
  impl->source.sha256 = core::Sha256::of(bytes);
  impl->source.format = format;
  impl->source.pathway = root.attribute("name").value();
  if (impl->source.pathway.empty()) impl->source.pathway = core::to_utf8(path.stem());
  impl->source.title = root.attribute("title").value();
  impl->id_map = ids.digest();
  auto& in = impl->interner;

  std::vector<std::uint32_t> entry_lines;
  std::vector<bool> is_group;
  std::map<std::uint32_t, std::uint32_t> seen;              // local id -> line of its first entry
  for (pugi::xml_node node : root.children("entry")) {
    const std::uint32_t line = lines.line(node.offset_debug());
    const auto local = parse_uint(node.attribute("id").value());
    if (!local) fail("network.parse", file, line, "entry id \"" + std::string(node.attribute("id").value()) + "\" is not a number");
    // S2.06: a repeated id silently replaced the earlier entry in FuSION 1 (XMLParser.cc:646).
    if (const auto [it, inserted] = seen.emplace(*local, line); !inserted)
      fail("network.duplicate_entry", file, line, "entry id " + std::to_string(*local) +
           " repeats the entry on line " + std::to_string(it->second));
    Entry e;
    e.local_id = *local;
    const std::string type = node.attribute("type").value();
    e.type = in.sym(type);
    // S2.06's fix: split on runs of spaces, so two spaces make no empty id (FuSION 1's split_string
    // gave "", Linux/src/main.cc:954-973). Each id passes the id rule (02.C3); an id the map sends to
    // nothing is left out.
    std::vector<std::string> names;
    for (auto token : split_ws(node.attribute("name").value())) {
      std::string id = ids.apply(token);
      if (!id.empty()) names.push_back(std::move(id));
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    e.ids_first = static_cast<std::uint32_t>(impl->ids.size());
    for (const auto& n : names) impl->ids.push_back(in.id(n));
    e.ids_count = static_cast<std::uint32_t>(names.size());
    e.comps_first = static_cast<std::uint32_t>(impl->comps.size());
    if (type == "group") {                                   // XMLParser.cc:85-102: groups only
      for (pugi::xml_node comp : node.children("component")) {
        const auto c = parse_uint(comp.attribute("id").value());
        if (!c) fail("network.parse", file, lines.line(comp.offset_debug()), "component id \"" +
                     std::string(comp.attribute("id").value()) + "\" is not a number");
        impl->comps.push_back(*c);
      }
    }
    e.comps_count = static_cast<std::uint32_t>(impl->comps.size()) - e.comps_first;
    impl->entries.push_back(e);
    entry_lines.push_back(line);
    is_group.push_back(type == "group");
  }
  impl->index_entries();
  check_groups(*impl, file, entry_lines, is_group);

  const std::uint32_t root_list = impl->add_list({in.sym(impl->source.pathway)});
  std::string bad_sample;
  for (pugi::xml_node node : root.children("relation")) {
    const std::uint32_t line = lines.line(node.offset_debug());
    Relation r;
    r.line = line;
    for (auto [attr, slot] : {std::pair{"entry1", &r.entry1}, std::pair{"entry2", &r.entry2}}) {
      const auto v = parse_uint(node.attribute(attr).value());
      if (!v) fail("network.parse", file, line, std::string(attr) + " \"" + node.attribute(attr).value() + "\" is not a number");
      // S2.06: FuSION 1 stored a null pointer for a missing entry and dereferenced it (XMLParser.cc:675, 881).
      if (!impl->position(*v))
        fail("network.dangling_ref", file, line, "the relation's " + std::string(attr) + " names entry " +
             std::to_string(*v) + ", which is not in the file");
      *slot = *v;
    }
    r.type = in.sym(node.attribute("type").value());
    std::vector<Sym> subs;
    for (pugi::xml_node s : node.children("subtype")) subs.push_back(in.sym(normalise_subtype(s.attribute("name").value())));
    r.subtypes = impl->add_list(subs);
    if (format == Format::kgml) {
      r.pathways = root_list;
    } else {
      std::vector<std::string> paths;
      for (auto p : split_ws(node.attribute("pathways").value())) paths.emplace_back(p);
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      std::vector<Sym> psyms;
      for (const auto& p : paths) psyms.push_back(in.sym(p));
      r.pathways = impl->add_list(psyms);
    }
    // One decimal grammar for scores (04.C2, 3A.19); FuSION 1's stof read "625x" as 625
    // (XMLParser.cc:1519-1522). An absent score is unscored; a malformed one too, with a warning.
    if (const pugi::xml_attribute score = node.attribute("experiment_score")) {
      const core::Decimal d = core::parse_decimal(score.value());
      if (d.status == core::DecimalStatus::ok) {
        r.score = d.value;
      } else {
        ++counts.bad_scores;
        if (counts.bad_scores <= 5)
          bad_sample += (counts.bad_scores > 1 ? ", line " : "line ") + std::to_string(line) + " \"" + score.value() + "\"";
      }
    }
    if (!r.score) ++counts.unscored;
    impl->relations.push_back(r);
  }
  if (counts.bad_scores)
    ctx.log.warning("network.bad_score", file + ": " + std::to_string(counts.bad_scores) +
                    " score(s) that are not numbers, read as unscored (" + bad_sample +
                    (counts.bad_scores > 5 ? ", ..." : "") + ")");
  return impl;
}

}  // namespace detail

std::shared_ptr<const PathwayDoc> read_kgml(const std::filesystem::path& path, const IdMap& ids, core::Context& ctx) {
  const auto started = std::chrono::steady_clock::now();
  const std::string bytes = detail::read_input(path);
  detail::ReadCounts counts;
  std::shared_ptr<const PathwayDoc> doc =
      std::make_shared<PathwayDoc>(detail::parse_pathway_xml(bytes, path, ids, Format::kgml, ctx, counts));
  const std::string file = detail::display(path);
  const RelationFilters filters = detail::filters_from(ctx.settings);
  const auto kept = kept_relations(*doc, filters);
  std::uint32_t indirect = 0, ecrel = 0, maplink = 0;
  for (const Relation& r : doc->relations()) {
    const auto type = doc->interner().str(r.type);
    const auto subs = doc->subtypes(r);
    const bool is_indirect = std::any_of(subs.begin(), subs.end(), [&](Sym s) { return doc->interner().str(s) == "indirect"; });
    if (filters.ecrel == EcrelMode::exclude && type == "ECrel") ++ecrel;
    else if (filters.maplink == MaplinkMode::skip && type == "maplink") ++maplink;
    else if (filters.indirect == IndirectMode::discard && is_indirect) ++indirect;
  }
  if (kept.empty())   // FuSION 1 printed "<file> has no edges" (XMLParser.cc:716-718)
    ctx.log.warning("network.empty", file + (doc->relations().empty() ? " has no relations" : " has no relation left after the filters") +
                    ", so it gives no edges");
  ctx.events.emit("network.read", core::Json{{"path", file}, {"format", "kgml"}, {"pathway", doc->source().pathway},
                                             {"entries", doc->entries().size()}, {"relations", doc->relations().size()},
                                             {"kept", kept.size()},
                                             {"dropped", {{"indirect", indirect}, {"ecrel", ecrel}, {"maplink", maplink}}},
                                             {"unscored", counts.unscored}, {"ms", detail::elapsed_ms(started)}});
  return doc;
}

// ---------------------------------------------------------------- filters and the split rule

// S2.11, S2.31, 02.C6, 02.C7: applied before any node is made. The document keeps every relation,
// so one parse serves every setting; 08's relation share counts the same list (08.C4).
std::vector<std::uint32_t> kept_relations(const PathwayDoc& doc, const RelationFilters& filters) {
  std::vector<std::uint32_t> out;
  const auto& in = doc.interner();
  const auto relations = doc.relations();
  for (std::uint32_t i = 0; i < relations.size(); ++i) {
    const Relation& r = relations[i];
    const auto type = in.str(r.type);
    if (filters.ecrel == EcrelMode::exclude && type == "ECrel") continue;
    if (filters.maplink == MaplinkMode::skip && type == "maplink") continue;
    if (filters.indirect == IndirectMode::discard) {        // XMLParser.cc:1481-1490, now a setting
      const auto subs = doc.subtypes(r);
      if (std::any_of(subs.begin(), subs.end(), [&](Sym s) { return in.str(s) == "indirect"; })) continue;
    }
    out.push_back(i);
  }
  return out;
}

// S2.07 (Linux/src/XMLParser.cc:899-900, 922-940, 958-962): ids in the exception set stand alone; the
// rest form one box when the entry holds more than `thres` ids, counting the exception ids, else each
// stands alone. A negative threshold never collapses (FuSION 1's comparison is unsigned), and 0
// behaves like 1. A box of one id is returned as a single.
EntrySplit split_entry(const PathwayDoc& doc, const Entry& entry, const IdSet& exceptions, int split_threshold) {
  EntrySplit out;
  const auto ids = doc.ids(entry);
  const auto& in = doc.interner();
  const bool collapse = split_threshold >= 0 && ids.size() > static_cast<std::size_t>(std::max(split_threshold, 1));
  for (GeneId id : ids) {
    if (collapse && !exceptions.contains(in.str(id))) out.box.push_back(id);
    else out.singles.push_back(id);
  }
  if (out.box.size() == 1) {
    out.singles.push_back(out.box.front());
    out.box.clear();
  }
  auto by_text = [&](GeneId a, GeneId b) { return in.str(a) < in.str(b); };
  std::sort(out.singles.begin(), out.singles.end(), by_text);
  std::sort(out.box.begin(), out.box.end(), by_text);
  return out;
}

// ---------------------------------------------------------------- StepDocs (09.C3, S7.19)

struct StepDocs::Impl {
  std::map<std::pair<std::string, std::string>, std::shared_ptr<const PathwayDoc>> docs;
};

StepDocs::StepDocs() : impl_(std::make_unique<Impl>()) {}
StepDocs::~StepDocs() = default;
StepDocs::StepDocs(StepDocs&&) noexcept = default;
StepDocs& StepDocs::operator=(StepDocs&&) noexcept = default;

std::shared_ptr<const PathwayDoc> StepDocs::get(const std::filesystem::path& copy, const IdMap& ids, core::Context& ctx) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(copy, ec);
  if (ec) canonical = copy;
  auto key = std::make_pair(core::to_utf8(canonical), ids.digest().hex());
  if (const auto it = impl_->docs.find(key); it != impl_->docs.end()) return it->second;
  ctx.stop.throw_if_stopped();
  const Sniff s = sniff(copy, std::nullopt, ctx);
  std::shared_ptr<const PathwayDoc> doc;
  if (s.format == Format::kgml) doc = read_kgml(copy, ids, ctx);
  else if (s.format == Format::edge_table) doc = read_edge_table(copy, ids, ctx);
  else
    throw core::Failure("network.unknown_format", detail::display(copy) + " is a network file, not a pathway document",
                        {detail::display(copy), 0, {}});
  impl_->docs.emplace(std::move(key), doc);
  return doc;
}

}  // namespace fusion::network
