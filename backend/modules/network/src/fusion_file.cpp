// fusion network: FuSION's network file, version 1 (02.C4, S2.25, S2.26): the writer (wgx and every
// saved network), the reader (load, and rgx on a saved network, 2A.4), the comparison rgx makes with
// the run's settings, and add's transcription (S2.30, 02.C17).
#include <algorithm>
#include <cctype>
#include <charconv>
#include <functional>
#include <map>
#include <set>
#include <sstream>

#include <pugixml.hpp>

#include "detail.hpp"
#include "fusion/core/build_info.hpp"

namespace fusion::network {

namespace {

constexpr std::uint16_t kComplex = bit(NodeFlag::complex);
constexpr std::uint16_t kException = bit(NodeFlag::exception);
constexpr std::uint16_t kDummy = bit(EdgeFlag::dummy_u_to_d);
constexpr std::uint16_t kUnscored = bit(EdgeFlag::unscored);

// Edge flags written by name in a relation's `flags` attribute. The tag is written as FuSION 1 wrote
// it, a <subtype> (S2.25), and an unscored edge has no experiment_score (1.25), so neither is listed.
constexpr std::pair<EdgeFlag, std::string_view> kEdgeFlagNames[] = {
    {EdgeFlag::component, "component"}, {EdgeFlag::undirected, "undirected"}, {EdgeFlag::ecrel, "ecrel"},
    {EdgeFlag::indirect, "indirect"},   {EdgeFlag::self_loop, "self_loop"},   {EdgeFlag::legacy, "legacy"}};

std::string edge_flag_text(std::uint16_t flags) {
  std::string out;
  for (const auto& [f, name] : kEdgeFlagNames)
    if (flags & bit(f)) (out += out.empty() ? "" : " ") += name;
  return out;
}

std::uint16_t edge_flags_from(std::string_view text, const std::string& file, std::uint32_t line) {
  std::uint16_t flags = 0;
  for (auto token : detail::split_ws(text)) {
    bool known = false;
    for (const auto& [f, name] : kEdgeFlagNames)
      if (token == name) { flags |= bit(f); known = true; }
    if (!known) detail::fail("network.parse", file, line, "unknown edge flag \"" + std::string(token) + "\"");
  }
  return flags;
}

std::string_view indirect_text(IndirectMode m) { return m == IndirectMode::keep ? "keep" : "discard"; }
std::string_view ecrel_text(EcrelMode m) { return m == EcrelMode::exclude ? "exclude" : "keep"; }
std::string_view maplink_text(MaplinkMode m) { return m == MaplinkMode::keep ? "keep" : "skip"; }

std::optional<Format> format_from(std::string_view text) {
  for (Format f : {Format::kgml, Format::fusion_network, Format::legacy, Format::edge_table})
    if (detail::format_name(f) == text) return f;
  return std::nullopt;
}

void set(pugi::xml_node node, const char* name, std::string_view value) {
  node.append_attribute(name).set_value(std::string(value).c_str());
}

// The entries a <source> section embeds (02.C4): every entry some relation reaches, directly or as a
// component of a reached group, since only those can ever become nodes.
std::vector<bool> reached_entries(const PathwayDoc& doc) {
  const auto entries = doc.entries();
  std::vector<bool> reached(entries.size(), false);
  std::vector<std::uint32_t> todo;
  auto reach = [&](std::uint32_t local) {
    if (const Entry* e = doc.entry(local)) {
      const auto pos = static_cast<std::size_t>(e - entries.data());
      if (!reached[pos]) { reached[pos] = true; todo.push_back(local); }
    }
  };
  for (const Relation& r : doc.relations()) { reach(r.entry1); reach(r.entry2); }
  while (!todo.empty()) {
    const Entry* e = doc.entry(todo.back());
    todo.pop_back();
    for (std::uint32_t c : doc.components(*e)) reach(c);
  }
  return reached;
}

void write_source(pugi::xml_node parent, const PathwayDoc& doc) {
  const auto& in = doc.interner();
  const SourceRef& src = doc.source();
  pugi::xml_node s = parent.append_child("source");
  set(s, "pathway", src.pathway);
  if (!src.title.empty()) set(s, "title", src.title);
  set(s, "sha256", src.sha256.hex());
  set(s, "size", std::to_string(src.size));
  set(s, "format", detail::format_name(src.format));
  const auto reached = reached_entries(doc);
  const auto entries = doc.entries();
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (!reached[i]) continue;
    const Entry& e = entries[i];
    pugi::xml_node x = s.append_child("e");
    set(x, "id", std::to_string(e.local_id));
    set(x, "type", in.str(e.type));
    std::vector<std::string> ids;
    for (GeneId id : doc.ids(e)) ids.emplace_back(in.str(id));
    set(x, "ids", detail::join(ids, " "));
    if (e.comps_count) {
      std::vector<std::string> comps;
      for (std::uint32_t c : doc.components(e)) comps.push_back(std::to_string(c));
      set(x, "comps", detail::join(comps, " "));
    }
  }
  for (const Relation& r : doc.relations()) {
    pugi::xml_node x = s.append_child("r");
    set(x, "e1", std::to_string(r.entry1));
    set(x, "e2", std::to_string(r.entry2));
    set(x, "type", in.str(r.type));
    if (r.score) set(x, "score", detail::format_score(*r.score));
    std::vector<std::string> subs;
    for (Sym s2 : doc.subtypes(r)) subs.emplace_back(in.str(s2));
    if (!subs.empty()) set(x, "sub", detail::join(subs, ","));
    const auto paths = doc.pathways(r);
    if (!(paths.size() == 1 && in.str(paths[0]) == src.pathway)) {
      std::vector<std::string> p;
      for (Sym s2 : paths) p.emplace_back(in.str(s2));
      set(x, "pathways", detail::join(p, ","));
    }
  }
}

}  // namespace

// ---------------------------------------------------------------- the writer

// 02.C4's format: the mark on the root, a <fusion> header, entries numbered from 1 in canonical order
// with sorted members and an explicit key, relations with sorted pathways, and the embedded sources.
// FuSION 1 wrote creation order, unsorted names, no escaping and other genes' aliases as labels
// (Linux/src/GraphManagerNew.cc:7895-7938, 2892-2953; S2.25); pugixml escapes every attribute.
void write_fusion_network(const GraphView& view, std::ostream& out, const Maps& maps, const WriteOptions& options) {
  const Network& net = view.network();
  const auto& in = net.interner();
  const NetworkInfo& info = net.info();
  const auto nodes = net.nodes();

  pugi::xml_document xml;
  pugi::xml_node decl = xml.append_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";
  pugi::xml_node root = xml.append_child("pathway");
  set(root, "name", options.root_name);
  set(root, "format", "fusion-network");
  set(root, "version", "1");

  // Written entries: the view's nodes, and the components of its complexes, so the file stays
  // self-contained; components outside the view are marked in_view="false".
  std::vector<std::uint32_t> number(nodes.size(), 0);
  std::vector<bool> written(nodes.size(), false);
  std::vector<NodeIdx> todo;
  view.for_each_node([&](NodeIdx n) { written[n] = true; todo.push_back(n); });
  while (!todo.empty()) {
    const NodeIdx n = todo.back();
    todo.pop_back();
    for (NodeIdx c : net.components(n))
      if (!written[c]) { written[c] = true; todo.push_back(c); }
  }
  std::uint32_t next = 0;
  for (NodeIdx n = 0; n < nodes.size(); ++n)
    if (written[n]) number[n] = ++next;

  pugi::xml_node header = root.append_child("fusion");
  set(header, "writer", "FuSION " + std::string(core::build_info().version));
  set(header, "reader", info.reader);
  set(header, "legacy", info.legacy ? "true" : "false");
  if (info.merge) {
    pugi::xml_node m = header.append_child("merge");
    set(m, "key", info.merge->key);
    set(m, "base", info.merge->base ? "true" : "false");
    set(m, "rules", std::to_string(info.merge->rules));
  }
  if (info.reader != "add") {                                // a transcription records no settings
    const BuildSettings& s = info.settings;
    for (auto [name, value] : {std::pair<const char*, std::string>{"network.split_threshold", std::to_string(s.split_threshold)},
                               {"network.indirect", std::string(indirect_text(s.filters.indirect))},
                               {"network.ecrel", std::string(ecrel_text(s.filters.ecrel))},
                               {"network.maplink", std::string(maplink_text(s.filters.maplink))}}) {
      pugi::xml_node x = header.append_child("setting");
      set(x, "name", name);
      set(x, "value", value);
    }
  }
  for (const SourceRef& s : info.sources) {                  // canonical order (finish sorted them)
    pugi::xml_node x = header.append_child("input");
    if (s.path && !options.store_entry) set(x, "path", core::to_utf8(*s.path));   // 03.C4: none in the store
    set(x, "sha256", s.sha256.hex());
    set(x, "size", std::to_string(s.size));
    set(x, "format", detail::format_name(s.format));
    set(x, "pathway", s.pathway);
    if (!s.title.empty()) set(x, "title", s.title);
  }
  set(header.append_child("exceptions"), "ids", detail::join(info.exceptions, " "));
  if (info.id_map != core::Sha256{}) set(header.append_child("id_map"), "sha256", info.id_map.hex());
  // A legacy network's ids with several holders keep FuSION 1's claim order (S2.03, 02.C5).
  std::set<std::string_view> shared;
  for (NodeIdx n = 0; n < nodes.size(); ++n)
    if (written[n])
      for (GeneId m : net.members(n))
        if (net.ids().holders(m).size() > 1) shared.insert(in.str(m));
  for (auto id : shared) {
    std::vector<std::string> holders;
    for (NodeIdx h : net.ids().holders(*in.find_id(id)))
      if (written[h]) holders.push_back(std::to_string(number[h]));
    if (holders.size() < 2) continue;
    pugi::xml_node x = header.append_child("claim");
    set(x, "id", id);
    set(x, "entries", detail::join(holders, " "));
  }

  for (NodeIdx n = 0; n < nodes.size(); ++n) {
    if (!written[n]) continue;
    const Node& node = nodes[n];
    pugi::xml_node x = root.append_child("entry");
    set(x, "id", std::to_string(number[n]));
    set(x, "key", in.str(node.key));
    std::vector<std::string> members, labels;
    for (GeneId m : net.members(n)) {
      members.emplace_back(in.str(m));
      labels.emplace_back(maps.symbols.label_or_id(in.str(m)));
    }
    set(x, "name", detail::join(members, " "));
    set(x, "type", in.str(node.type));
    if (node.flags & kException) set(x, "flags", "exception");
    if (!view.has_node(n)) set(x, "in_view", "false");
    set(x.append_child("graphics"), "name", (node.flags & kComplex) ? net.label(n, maps) : detail::join(labels, ", "));
    for (NodeIdx c : net.components(n)) set(x.append_child("component"), "id", std::to_string(number[c]));
  }

  view.for_each_edge([&](EdgeIdx e) {
    const Edge& edge = net.edges()[e];
    if (!view.has_node(edge.src) || !view.has_node(edge.tgt)) return;
    pugi::xml_node x = root.append_child("relation");
    set(x, "entry1", std::to_string(number[edge.src]));
    set(x, "entry2", std::to_string(number[edge.tgt]));
    set(x, "type", in.str(edge.type));
    std::vector<std::string> paths;
    for (Sym p : net.pathways(e)) paths.emplace_back(in.str(p));
    set(x, "pathways", detail::join(paths, " "));
    if (!(edge.flags & kUnscored)) set(x, "experiment_score", detail::format_score(edge.score));
    if (const std::string f = edge_flag_text(edge.flags); !f.empty()) set(x, "flags", f);
    for (Sym s : net.subtypes(e)) set(x.append_child("subtype"), "name", in.str(s));
    if (edge.flags & kDummy) set(x.append_child("subtype"), "name", "dummy_u_to_d");
  });

  if (options.embed_sources)
    if (const Provenance* p = net.provenance())
      for (const auto& doc : p->docs) write_source(root, *doc);

  xml.save(out, "  ", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
}

// ---------------------------------------------------------------- the reader

namespace {

struct SavedEntry {
  std::uint32_t line = 0;
  std::string key, type;
  std::vector<std::string> members;
  std::vector<std::uint32_t> comps;
  bool group = false, exception = false;
};

std::shared_ptr<const PathwayDoc> read_source(pugi::xml_node s, const NetworkInfo& info, const std::filesystem::path& path,
                                              const detail::LineIndex& lines) {
  const std::string file = detail::display(path);
  auto impl = std::make_unique<PathwayDoc::Impl>();
  auto& in = impl->interner;
  impl->source.pathway = s.attribute("pathway").value();
  impl->source.title = s.attribute("title").value();
  const auto sha = detail::sha_from_hex(s.attribute("sha256").value());
  if (!sha) detail::fail("network.parse", file, lines.line(s.offset_debug()), "a <source> has no valid sha256");
  impl->source.sha256 = *sha;
  impl->source.size = detail::parse_uint(s.attribute("size").value()).value_or(0);
  impl->source.format = format_from(s.attribute("format").value()).value_or(Format::kgml);
  for (const SourceRef& ref : info.sources)                  // the input it came from, if listed
    if (ref.sha256 == impl->source.sha256 && ref.pathway == impl->source.pathway) impl->source.path = ref.path;
  impl->id_map = info.id_map;
  std::vector<std::uint32_t> entry_lines;
  std::vector<bool> is_group;
  for (pugi::xml_node x : s.children("e")) {
    const std::uint32_t line = lines.line(x.offset_debug());
    const auto local = detail::parse_uint(x.attribute("id").value());
    if (!local) detail::fail("network.parse", file, line, "an embedded entry has no id");
    Entry e;
    e.local_id = *local;
    const std::string type = x.attribute("type").value();
    e.type = in.sym(type);
    e.ids_first = static_cast<std::uint32_t>(impl->ids.size());
    for (auto id : detail::split_ws(x.attribute("ids").value())) impl->ids.push_back(in.id(id));   // already mapped
    e.ids_count = static_cast<std::uint32_t>(impl->ids.size()) - e.ids_first;
    e.comps_first = static_cast<std::uint32_t>(impl->comps.size());
    for (auto c : detail::split_ws(x.attribute("comps").value())) {
      const auto v = detail::parse_uint(c);
      if (!v) detail::fail("network.parse", file, line, "an embedded component is not a number");
      impl->comps.push_back(*v);
    }
    e.comps_count = static_cast<std::uint32_t>(impl->comps.size()) - e.comps_first;
    impl->entries.push_back(e);
    entry_lines.push_back(line);
    is_group.push_back(type == "group");
  }
  impl->index_entries();
  if (impl->by_local_id.size() > 1)
    for (std::size_t i = 1; i < impl->by_local_id.size(); ++i)
      if (impl->by_local_id[i].first == impl->by_local_id[i - 1].first)
        detail::fail("network.duplicate_entry", file, entry_lines[impl->by_local_id[i].second],
                     "embedded entry id " + std::to_string(impl->by_local_id[i].first) + " repeats");
  detail::check_groups(*impl, file, entry_lines, is_group);
  const std::uint32_t root_list = impl->add_list({in.sym(impl->source.pathway)});
  for (pugi::xml_node x : s.children("r")) {
    const std::uint32_t line = lines.line(x.offset_debug());
    Relation r;
    r.line = line;
    const auto e1 = detail::parse_uint(x.attribute("e1").value());
    const auto e2 = detail::parse_uint(x.attribute("e2").value());
    if (!e1 || !e2) detail::fail("network.parse", file, line, "an embedded relation has no ends");
    if (!impl->position(*e1) || !impl->position(*e2))
      detail::fail("network.dangling_ref", file, line, "an embedded relation names a missing entry");
    r.entry1 = *e1;
    r.entry2 = *e2;
    r.type = in.sym(x.attribute("type").value());
    std::vector<Sym> subs;
    const std::string sub = x.attribute("sub").value();
    if (!sub.empty())
      for (auto t : detail::split_on(sub, ',')) subs.push_back(in.sym(t));
    r.subtypes = impl->add_list(subs);
    if (const pugi::xml_attribute p = x.attribute("pathways")) {
      std::vector<std::string> paths;
      for (auto t : detail::split_on(p.value(), ',')) if (!t.empty()) paths.emplace_back(t);
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      std::vector<Sym> psyms;
      for (const auto& t : paths) psyms.push_back(in.sym(t));
      r.pathways = impl->add_list(psyms);
    } else {
      r.pathways = root_list;
    }
    if (const pugi::xml_attribute sc = x.attribute("score")) {
      const core::Decimal d = core::parse_decimal(sc.value());
      if (d.status != core::DecimalStatus::ok) detail::fail("network.parse", file, line, "an embedded score is not a number");
      r.score = d.value;
    }
    impl->relations.push_back(r);
  }
  return std::make_shared<PathwayDoc>(std::move(impl));
}

}  // namespace

// Loads a network file as saved (load, 01.C6; 2A.4): entries become nodes and relations edges through
// the builder, with no box or relation rule; embedded sources become the network's Provenance.
std::shared_ptr<const Network> read_fusion_network(const std::filesystem::path& path, core::Context& ctx) {
  const auto started = std::chrono::steady_clock::now();
  const std::string bytes = detail::read_input(path);
  const std::string file = detail::display(path);
  const detail::LineIndex lines(bytes);
  pugi::xml_document xml;
  const pugi::xml_parse_result parsed = xml.load_buffer(bytes.data(), bytes.size(), pugi::parse_default);
  if (!parsed) detail::fail("network.parse", file, lines.line(parsed.offset), parsed.description());
  const pugi::xml_node root = xml.document_element();
  if (std::string_view(root.name()) != "pathway" || std::string_view(root.attribute("format").value()) != "fusion-network")
    detail::fail("network.parse", file, lines.line(root.offset_debug()), "not a FuSION network file (no format=\"fusion-network\")");
  const auto version = detail::parse_uint(root.attribute("version").value());
  if (!version || *version == 0)
    detail::fail("network.parse", file, lines.line(root.offset_debug()), "the network file has no version");
  if (*version > 1)
    throw core::Failure("artefact.newer_format", file + " is version " + std::to_string(*version) +
                        " of FuSION's network format, written by a newer FuSION; this one reads version 1", {file, 0, {}});

  NetworkInfo info;
  const pugi::xml_node header = root.child("fusion");
  info.reader = header.attribute("reader") ? header.attribute("reader").value() : "add";
  info.legacy = std::string_view(header.attribute("legacy").value()) == "true";
  if (const pugi::xml_node m = header.child("merge")) {
    MergeHeader mh;
    mh.key = m.attribute("key").value();
    mh.base = std::string_view(m.attribute("base").value()) == "true";
    mh.rules = static_cast<int>(detail::parse_uint(m.attribute("rules").value()).value_or(1));
    info.merge = mh;
  }
  bool settings_recorded = false;
  for (pugi::xml_node s : header.children("setting")) {
    settings_recorded = true;
    const std::string_view name = s.attribute("name").value();
    const std::string_view value = s.attribute("value").value();
    const std::uint32_t line = lines.line(s.offset_debug());
    auto bad = [&] { detail::fail("network.parse", file, line, "setting " + std::string(name) + " has the value \"" + std::string(value) + "\""); };
    if (name == "network.split_threshold") {
      std::int64_t v = 0;
      const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
      if (ec != std::errc() || ptr != value.data() + value.size()) bad();
      info.settings.split_threshold = static_cast<int>(v);
    } else if (name == "network.indirect") {
      if (value != "keep" && value != "discard") bad();
      info.settings.filters.indirect = value == "keep" ? IndirectMode::keep : IndirectMode::discard;
    } else if (name == "network.ecrel") {
      if (value != "keep" && value != "exclude") bad();
      info.settings.filters.ecrel = value == "exclude" ? EcrelMode::exclude : EcrelMode::keep;
    } else if (name == "network.maplink") {
      if (value != "skip" && value != "keep") bad();
      info.settings.filters.maplink = value == "keep" ? MaplinkMode::keep : MaplinkMode::skip;
    }
  }
  if (!settings_recorded && !header.attribute("reader")) info.reader = "add";
  for (pugi::xml_node x : header.children("input")) {
    SourceRef s;
    if (const pugi::xml_attribute p = x.attribute("path")) s.path = core::path_from_utf8(p.value());
    const auto sha = detail::sha_from_hex(x.attribute("sha256").value());
    if (!sha) detail::fail("network.parse", file, lines.line(x.offset_debug()), "an <input> has no valid sha256");
    s.sha256 = *sha;
    s.size = detail::parse_uint(x.attribute("size").value()).value_or(0);
    s.format = format_from(x.attribute("format").value()).value_or(Format::kgml);
    s.pathway = x.attribute("pathway").value();
    s.title = x.attribute("title").value();
    info.sources.push_back(std::move(s));
  }
  for (auto id : detail::split_ws(header.child("exceptions").attribute("ids").value())) info.exceptions.emplace_back(id);
  if (const pugi::xml_node m = header.child("id_map")) {
    const auto sha = detail::sha_from_hex(m.attribute("sha256").value());
    if (!sha) detail::fail("network.parse", file, lines.line(m.offset_debug()), "the <id_map> has no valid sha256");
    info.id_map = *sha;
  }

  std::map<std::uint32_t, SavedEntry> entries;
  for (pugi::xml_node x : root.children("entry")) {
    const std::uint32_t line = lines.line(x.offset_debug());
    const auto id = detail::parse_uint(x.attribute("id").value());
    if (!id) detail::fail("network.parse", file, line, "entry id \"" + std::string(x.attribute("id").value()) + "\" is not a number");
    SavedEntry e;
    e.line = line;
    e.key = x.attribute("key").value();
    e.type = x.attribute("type").value();
    for (auto m : detail::split_ws(x.attribute("name").value())) e.members.emplace_back(m);
    for (pugi::xml_node c : x.children("component")) {
      const auto v = detail::parse_uint(c.attribute("id").value());
      if (!v) detail::fail("network.parse", file, lines.line(c.offset_debug()), "a component id is not a number");
      e.comps.push_back(*v);
    }
    e.group = e.type == "group" || !e.comps.empty();
    for (auto f : detail::split_ws(x.attribute("flags").value())) e.exception = e.exception || f == "exception";
    if (e.group && e.comps.empty()) detail::fail("network.empty_group", file, line, "group entry " + std::to_string(*id) + " has no components");
    if (!e.group && e.members.empty()) detail::fail("network.parse", file, line, "entry " + std::to_string(*id) + " names no id");
    if (const auto [it, inserted] = entries.emplace(*id, std::move(e)); !inserted)
      detail::fail("network.duplicate_entry", file, line, "entry id " + std::to_string(*id) + " repeats the entry on line " +
                   std::to_string(it->second.line));
  }

  core::Interner local;
  NetworkBuilder builder(info, local);
  std::map<std::uint32_t, NodeIdx> made;
  std::map<std::uint32_t, int> state;                        // 1: on the path, 2: made
  std::function<NodeIdx(std::uint32_t, std::uint32_t)> make = [&](std::uint32_t id, std::uint32_t from_line) -> NodeIdx {
    const auto it = entries.find(id);
    if (it == entries.end()) detail::fail("network.dangling_ref", file, from_line, "entry " + std::to_string(id) + " is not in the file");
    if (const auto m = made.find(id); m != made.end()) return m->second;
    const SavedEntry& e = it->second;
    if (state[id] == 1) detail::fail("network.cyclic_group", file, e.line, "group entry " + std::to_string(id) + " contains itself");
    state[id] = 1;
    NodeIdx n = 0;
    if (e.group) {
      std::vector<NodeIdx> comps;
      for (std::uint32_t c : e.comps) comps.push_back(make(c, e.line));
      n = builder.add_complex(comps);
    } else {
      std::vector<GeneId> ids;
      for (const auto& m : e.members) ids.push_back(local.id(m));
      n = builder.add_node(local.sym(e.type), ids);
    }
    if (e.exception) builder.mark_exception(n);
    state[id] = 2;
    made.emplace(id, n);
    return n;
  };
  for (const auto& [id, e] : entries) make(id, e.line);

  std::uint32_t relations = 0, bad_scores = 0;
  for (pugi::xml_node x : root.children("relation")) {
    const std::uint32_t line = lines.line(x.offset_debug());
    const auto e1 = detail::parse_uint(x.attribute("entry1").value());
    const auto e2 = detail::parse_uint(x.attribute("entry2").value());
    if (!e1 || !e2) detail::fail("network.parse", file, line, "a relation's entry1 or entry2 is not a number");
    for (auto e : {*e1, *e2})
      if (!made.count(e)) detail::fail("network.dangling_ref", file, line, "the relation names entry " + std::to_string(e) + ", which is not in the file");
    std::vector<Sym> subs, paths;
    for (pugi::xml_node s : x.children("subtype")) subs.push_back(local.sym(s.attribute("name").value()));
    for (auto p : detail::split_ws(x.attribute("pathways").value())) paths.push_back(local.sym(p));
    std::optional<double> score;
    if (const pugi::xml_attribute sc = x.attribute("experiment_score")) {
      const core::Decimal d = core::parse_decimal(sc.value());
      if (d.status == core::DecimalStatus::ok) score = d.value;
      else ++bad_scores;
    }
    builder.add_edge(made[*e1], made[*e2], local.sym(x.attribute("type").value()), subs, paths, score,
                     edge_flags_from(x.attribute("flags").value(), file, line));
    ++relations;
  }
  if (bad_scores)
    ctx.log.warning("network.bad_score", file + ": " + std::to_string(bad_scores) + " score(s) that are not numbers, read as unscored");

  std::vector<std::pair<GeneId, NodeIdx>> claims;
  for (pugi::xml_node x : header.children("claim")) {
    const GeneId id = local.id(x.attribute("id").value());
    for (auto t : detail::split_ws(x.attribute("entries").value())) {
      const auto e = detail::parse_uint(t);
      if (!e || !made.count(*e)) detail::fail("network.dangling_ref", file, lines.line(x.offset_debug()), "a claim names a missing entry");
      claims.emplace_back(id, made[*e]);
    }
  }
  if (!claims.empty()) builder.set_claim_order(claims);

  Provenance prov;
  for (pugi::xml_node s : root.children("source")) prov.docs.push_back(read_source(s, info, path, lines));
  if (!prov.docs.empty()) builder.attach(std::move(prov));

  auto net = std::make_shared<Network>(std::move(builder).finish(ctx));
  for (const auto& [id, e] : entries)                        // the explicit key must be the computed one
    if (!e.key.empty() && !net->find_node(NodeKey{e.key}))
      detail::fail("network.parse", file, e.line, "entry " + std::to_string(id) + "'s key " + e.key +
                   " is not the key its " + (e.group ? "components" : "members") + " give");
  ctx.events.emit("network.read", core::Json{{"path", file}, {"format", "fusion"}, {"reader", info.reader},
                                             {"legacy", info.legacy}, {"entries", entries.size()},
                                             {"relations", relations}, {"nodes", net->nodes().size()},
                                             {"edges", net->edges().size()}, {"ms", detail::elapsed_ms(started)}});
  return net;
}

// 2A.4: rgx on a saved network compares the build settings, the exception set and the id map's
// digest it was built with against the run's. Empty: use it as saved. A file that records no build
// settings, such as add's output, is used as saved.
std::vector<std::string> saved_network_diff(const NetworkInfo& info, const BuildSettings& settings, const IdSet& exceptions,
                                            const IdMap& ids) {
  std::vector<std::string> out;
  if (info.reader == "add") return out;
  const BuildSettings& s = info.settings;
  if (s.split_threshold != settings.split_threshold)
    out.push_back("network.split_threshold: saved " + std::to_string(s.split_threshold) + ", run " + std::to_string(settings.split_threshold));
  if (s.filters.indirect != settings.filters.indirect)
    out.push_back("network.indirect: saved " + std::string(indirect_text(s.filters.indirect)) + ", run " +
                  std::string(indirect_text(settings.filters.indirect)));
  if (s.filters.ecrel != settings.filters.ecrel)
    out.push_back("network.ecrel: saved " + std::string(ecrel_text(s.filters.ecrel)) + ", run " +
                  std::string(ecrel_text(settings.filters.ecrel)));
  if (s.filters.maplink != settings.filters.maplink)
    out.push_back("network.maplink: saved " + std::string(maplink_text(s.filters.maplink)) + ", run " +
                  std::string(maplink_text(settings.filters.maplink)));
  const IdSet saved(info.exceptions);
  if (saved != exceptions) {
    std::vector<std::string> added, removed;
    std::set_difference(exceptions.ids().begin(), exceptions.ids().end(), saved.ids().begin(), saved.ids().end(),
                        std::back_inserter(added));
    std::set_difference(saved.ids().begin(), saved.ids().end(), exceptions.ids().begin(), exceptions.ids().end(),
                        std::back_inserter(removed));
    std::string line = "exceptions: saved " + std::to_string(saved.size()) + " ids, run " + std::to_string(exceptions.size());
    if (!added.empty()) line += "; run adds " + detail::join(std::vector<std::string>(added.begin(), added.begin() + std::min<std::size_t>(added.size(), 5)), " ");
    if (!removed.empty()) line += "; run lacks " + detail::join(std::vector<std::string>(removed.begin(), removed.begin() + std::min<std::size_t>(removed.size(), 5)), " ");
    out.push_back(line);
  }
  if (info.id_map != ids.digest()) {
    auto text = [](const core::Sha256& d) { return d == core::Sha256{} ? std::string("none") : d.hex().substr(0, 12); };
    out.push_back("id map: saved " + text(info.id_map) + ", run " + text(ids.digest()));
  }
  return out;
}

// ---------------------------------------------------------------- add (S2.30, 02.C17)

namespace detail {

// KEGG's relation types, spelled as KEGG spells them (S2.30: FuSION 1 compared against "PPRel").
bool kegg_type(std::string_view type, std::string& right) {
  static constexpr std::string_view kTypes[] = {"PPrel", "GErel", "PCrel", "ECrel", "maplink"};
  for (auto t : kTypes) {
    if (t == type) return true;
    if (t.size() == type.size() &&
        std::equal(t.begin(), t.end(), type.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) ==
                                                                                   std::tolower(static_cast<unsigned char>(b)); })) {
      right = std::string(t);
      return false;
    }
  }
  right.clear();
  return false;
}

// KGML's subtype vocabulary, normalised (S2.10), plus `component`.
bool kgml_subtype(std::string_view s) {
  static constexpr std::string_view kSubtypes[] = {
      "compound",        "hidden compound", "activation",      "inhibition",     "expression",     "repression",
      "indirect",        "state",           "association",     "dissociation",   "missing",        "phosphorylation",
      "dephosphorylation", "glycosylation", "ubiquitination",  "methylation",    "component"};
  return std::find(std::begin(kSubtypes), std::end(kSubtypes), s) != std::end(kSubtypes);
}

}  // namespace detail

// add <edges_file> <out_xml>: one gene entry per id, labelled with the id, one relation per line,
// pathways="KEGG", no score, and no build rule, so an association stays one-way. FuSION 1 wrote the
// same elements (Linux/src/main.cc:5842-5934); 2.0 adds the mark and drops the DOCTYPE (02.C17),
// rejects a line without four fields and a misspelt KEGG type, and passes ids through the id rule.
void add_edges(const std::filesystem::path& edges, std::ostream& out, const IdMap& ids, core::Context& ctx) {
  const std::string bytes = detail::read_input(edges);
  const std::string file = detail::display(edges);
  struct Row { std::string src, tgt, type, subtype; };
  std::vector<Row> rows;
  std::map<std::string, std::uint32_t> numbers;              // id -> number, first appearance from 0
  std::set<std::string> unknown_types, unknown_subtypes;
  detail::for_each_line(bytes, [&](std::string_view line, std::uint32_t number) {
    const auto t = detail::trim(line);
    if (t.empty() || t.front() == '#') return;
    const auto fields = detail::split_ws(line);
    if (fields.size() != 4)
      detail::fail("network.bad_row", file, number, "expected 4 fields (source target type subtype), found " + std::to_string(fields.size()));
    Row r{ids.apply(fields[0]), ids.apply(fields[1]), std::string(fields[2]), std::string(normalise_subtype(fields[3]))};
    if (r.src.empty() || r.tgt.empty()) detail::fail("network.bad_row", file, number, "an id the id map sends to nothing");
    std::string right;
    if (!detail::kegg_type(r.type, right)) {
      if (!right.empty())
        detail::fail("network.type_spelling", file, number, "type " + r.type + " is spelled " + right + " in KEGG");
      unknown_types.insert(r.type);
    }
    if (!detail::kgml_subtype(r.subtype)) unknown_subtypes.insert(r.subtype);
    for (const std::string* id : {&r.src, &r.tgt})
      numbers.try_emplace(*id, static_cast<std::uint32_t>(numbers.size()));
    rows.push_back(std::move(r));
  });
  if (!unknown_types.empty())
    ctx.log.warning("network.unknown_type", file + ": types outside KEGG's: " +
                    detail::join(std::vector<std::string>(unknown_types.begin(), unknown_types.end()), ", "));
  if (!unknown_subtypes.empty())
    ctx.log.warning("network.unknown_subtype", file + ": subtypes outside KGML's: " +
                    detail::join(std::vector<std::string>(unknown_subtypes.begin(), unknown_subtypes.end()), ", "));

  pugi::xml_document xml;
  pugi::xml_node decl = xml.append_child(pugi::node_declaration);
  decl.append_attribute("version") = "1.0";
  decl.append_attribute("encoding") = "UTF-8";
  pugi::xml_node root = xml.append_child("pathway");
  set(root, "name", "KEGG");
  set(root, "format", "fusion-network");
  set(root, "version", "1");
  pugi::xml_node header = root.append_child("fusion");
  set(header, "writer", "FuSION " + std::string(core::build_info().version));
  set(header, "reader", "add");
  set(header, "legacy", "false");
  for (const auto& [id, n] : numbers) {                      // string order of id, as FuSION 1
    pugi::xml_node x = root.append_child("entry");
    set(x, "id", std::to_string(n));
    set(x, "key", id);
    set(x, "name", id);
    set(x, "type", "gene");
    set(x.append_child("graphics"), "name", id);
  }
  for (const Row& r : rows) {
    pugi::xml_node x = root.append_child("relation");
    set(x, "entry1", std::to_string(numbers[r.src]));
    set(x, "entry2", std::to_string(numbers[r.tgt]));
    set(x, "type", r.type);
    set(x, "pathways", "KEGG");
    set(x.append_child("subtype"), "name", r.subtype);
  }
  xml.save(out, "  ", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
}

}  // namespace fusion::network
