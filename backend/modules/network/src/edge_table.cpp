// fusion network: edge tables, the exchange format (02.C2; decision 0020; S2.30). A headered,
// tab-separated table with inline boxes (',') and complexes ('+'), or add's four-field lines. Each row
// is one relation of a document, which builds exactly as a KGML relation would.
#include <algorithm>
#include <set>

#include "detail.hpp"

namespace fusion::network {

namespace {

enum class Column : std::uint8_t { source, target, type, subtypes, pathways, score, source_type, target_type };

std::optional<Column> column_of(std::string_view name) {
  static constexpr std::pair<std::string_view, Column> kNames[] = {
      {"source", Column::source},     {"target", Column::target}, {"type", Column::type},
      {"subtypes", Column::subtypes}, {"pathways", Column::pathways}, {"score", Column::score},
      {"source_type", Column::source_type}, {"target_type", Column::target_type}};
  for (const auto& [n, c] : kNames)
    if (n == name) return c;
  return std::nullopt;
}

class TableReader {
 public:
  TableReader(const std::filesystem::path& path, const IdMap& ids, PathwayDoc::Impl& doc)
      : file_(detail::display(path)), ids_(ids), doc_(doc), stem_(core::to_utf8(path.stem())) {}

  void line(std::string_view line, std::uint32_t number) {
    const auto t = detail::trim(line);
    if (t.empty() || t.front() == '#') return;                 // blank and '#' lines are skipped
    if (!started_) {
      started_ = true;
      const auto fields = detail::split_on(line, '\t');
      bool source = false, target = false;
      for (auto f : fields) {
        source = source || detail::trim(f) == "source";
        target = target || detail::trim(f) == "target";
      }
      if (source && target) {                                  // the header: named columns, any order
        for (auto f : fields) {
          const auto name = detail::trim(f);
          const auto c = column_of(name);
          if (!c) detail::fail("network.bad_row", file_, number, "unknown column \"" + std::string(name) + "\"");
          if (std::find(columns_.begin(), columns_.end(), *c) != columns_.end())
            detail::fail("network.bad_row", file_, number, "column \"" + std::string(name) + "\" repeats");
          columns_.push_back(*c);
        }
        return;
      }
      add_form_ = true;
    }
    if (add_form_) add_row(line, number);
    else table_row(line, number);
  }

  void finish(core::Context& ctx) {
    doc_.index_entries();
    if (!unknown_types_.empty())
      ctx.log.warning("network.unknown_type", file_ + ": types outside KEGG's: " +
                      detail::join(std::vector<std::string>(unknown_types_.begin(), unknown_types_.end()), ", "));
    if (!unknown_subtypes_.empty())
      ctx.log.warning("network.unknown_subtype", file_ + ": subtypes outside KGML's: " +
                      detail::join(std::vector<std::string>(unknown_subtypes_.begin(), unknown_subtypes_.end()), ", "));
    if (bad_scores_)
      ctx.log.warning("network.bad_score", file_ + ": " + std::to_string(bad_scores_) +
                      " score(s) that are not numbers, read as unscored (" + bad_sample_ + (bad_scores_ > 5 ? ", ..." : "") + ")");
  }

  std::uint32_t unscored() const noexcept { return unscored_; }

 private:
  std::uint32_t entry(std::string type, const std::vector<std::string>& ids, const std::vector<std::uint32_t>& comps) {
    auto& in = doc_.interner;
    Entry e;
    e.local_id = next_++;
    e.type = in.sym(type);
    e.ids_first = static_cast<std::uint32_t>(doc_.ids.size());
    for (const auto& id : ids) doc_.ids.push_back(in.id(id));
    e.ids_count = static_cast<std::uint32_t>(ids.size());
    e.comps_first = static_cast<std::uint32_t>(doc_.comps.size());
    doc_.comps.insert(doc_.comps.end(), comps.begin(), comps.end());
    e.comps_count = static_cast<std::uint32_t>(comps.size());
    doc_.entries.push_back(e);
    return e.local_id;
  }

  std::vector<std::string> box(std::string_view cell, std::uint32_t number) {
    std::vector<std::string> ids;
    for (auto token : detail::split_on(cell, ',')) {
      const auto t = detail::trim(token);
      if (t.empty()) continue;
      std::string id = ids_.apply(t);                          // the id rule (02.C3)
      if (!id.empty()) ids.push_back(std::move(id));
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids.empty()) detail::fail("network.bad_row", file_, number, "an endpoint names no id");
    return ids;
  }

  // An endpoint cell: ',' joins the ids of one box, '+' the components of a complex; the type cell
  // holds one type for every component, or one per component in the same order.
  std::uint32_t endpoint(std::string_view cell, std::string_view types, std::uint32_t number) {
    const auto parts = detail::split_on(cell, '+');
    std::vector<std::string_view> kinds;
    if (!detail::trim(types).empty())
      for (auto k : detail::split_on(types, '+')) kinds.push_back(detail::trim(k));
    if (!kinds.empty() && kinds.size() != 1 && kinds.size() != parts.size())
      detail::fail("network.bad_row", file_, number, "the type cell \"" + std::string(types) + "\" gives " +
                   std::to_string(kinds.size()) + " types for " + std::to_string(parts.size()) + " components");
    std::vector<std::uint32_t> comps;
    for (std::size_t k = 0; k < parts.size(); ++k) {
      const std::string type = kinds.empty() ? std::string("gene") : std::string(kinds.size() == 1 ? kinds[0] : kinds[k]);
      comps.push_back(entry(type, box(parts[k], number), {}));
    }
    if (comps.size() == 1) return comps.front();
    return entry("group", {}, comps);
  }

  void relation(std::uint32_t e1, std::uint32_t e2, const std::string& type, std::vector<std::string> subtypes,
                std::vector<std::string> pathways, std::optional<double> score, std::uint32_t number) {
    auto& in = doc_.interner;
    Relation r;
    r.entry1 = e1;
    r.entry2 = e2;
    r.type = in.sym(type);
    std::vector<Sym> subs;
    for (const auto& s : subtypes) subs.push_back(in.sym(s));
    r.subtypes = doc_.add_list(subs);
    if (pathways.empty()) pathways.push_back(stem_);           // the file's stem by default
    std::sort(pathways.begin(), pathways.end());
    pathways.erase(std::unique(pathways.begin(), pathways.end()), pathways.end());
    std::vector<Sym> paths;
    for (const auto& p : pathways) paths.push_back(in.sym(p));
    r.pathways = doc_.add_list(paths);
    r.score = score;
    if (!score) ++unscored_;
    r.line = number;
    doc_.relations.push_back(r);
  }

  void check_type(const std::string& type, std::uint32_t number, bool warn) {
    std::string right;
    if (detail::kegg_type(type, right)) return;
    if (!right.empty()) detail::fail("network.type_spelling", file_, number, "type " + type + " is spelled " + right + " in KEGG");
    if (warn) unknown_types_.insert(type);
  }

  // add's form (S2.30): exactly four whitespace-separated fields, source target type subtype; FuSION 1
  // skipped short lines silently (Linux/src/main.cc:5878-5879).
  void add_row(std::string_view line, std::uint32_t number) {
    const auto f = detail::split_ws(line);
    if (f.size() != 4)
      detail::fail("network.bad_row", file_, number, "expected 4 fields (source target type subtype), found " + std::to_string(f.size()));
    const std::string type(f[2]);
    check_type(type, number, true);
    const std::string subtype(normalise_subtype(f[3]));
    if (!detail::kgml_subtype(subtype)) unknown_subtypes_.insert(subtype);
    const std::uint32_t s = entry("gene", box(f[0], number), {});
    const std::uint32_t t = entry("gene", box(f[1], number), {});
    relation(s, t, type, {subtype}, {}, std::nullopt, number);
  }

  void table_row(std::string_view line, std::uint32_t number) {
    const auto cells = detail::split_on(line, '\t');
    if (cells.size() > columns_.size())
      detail::fail("network.bad_row", file_, number, std::to_string(cells.size()) + " cells for " +
                   std::to_string(columns_.size()) + " columns");
    auto cell = [&](Column c) -> std::string_view {           // trailing empty cells may be omitted
      for (std::size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i] == c) return i < cells.size() ? detail::trim(cells[i]) : std::string_view();
      return {};
    };
    if (cell(Column::source).empty() || cell(Column::target).empty())
      detail::fail("network.bad_row", file_, number, "a row needs a source and a target");
    std::string type(cell(Column::type));
    if (type.empty()) type = "PPrel";
    check_type(type, number, false);                           // another source's terms pass
    std::vector<std::string> subtypes, pathways;
    for (auto s : detail::split_on(cell(Column::subtypes), ','))
      if (const auto t = detail::trim(s); !t.empty()) subtypes.emplace_back(normalise_subtype(t));
    for (auto p : detail::split_on(cell(Column::pathways), ','))
      if (const auto t = detail::trim(p); !t.empty()) pathways.emplace_back(t);
    std::optional<double> score;
    if (const auto text = cell(Column::score); !text.empty()) {   // one decimal grammar (3A.19)
      const core::Decimal d = core::parse_decimal(text);
      if (d.status == core::DecimalStatus::ok) {
        score = d.value;
      } else if (++bad_scores_ <= 5) {
        bad_sample_ += (bad_scores_ > 1 ? ", line " : "line ") + std::to_string(number) + " \"" + std::string(text) + "\"";
      }
    }
    const std::uint32_t s = endpoint(cell(Column::source), cell(Column::source_type), number);
    const std::uint32_t t = endpoint(cell(Column::target), cell(Column::target_type), number);
    relation(s, t, type, std::move(subtypes), std::move(pathways), score, number);
  }

  std::string file_;
  const IdMap& ids_;
  PathwayDoc::Impl& doc_;
  std::string stem_;
  bool started_ = false, add_form_ = false;
  std::vector<Column> columns_;
  std::uint32_t next_ = 1, unscored_ = 0, bad_scores_ = 0;
  std::string bad_sample_;
  std::set<std::string> unknown_types_, unknown_subtypes_;
};

}  // namespace

std::shared_ptr<const PathwayDoc> read_edge_table(const std::filesystem::path& path, const IdMap& ids, core::Context& ctx) {
  const auto started = std::chrono::steady_clock::now();
  const std::string bytes = detail::read_input(path);
  auto impl = std::make_unique<PathwayDoc::Impl>();
  impl->source.path = path;
  impl->source.size = bytes.size();
  impl->source.sha256 = core::Sha256::of(bytes);
  impl->source.format = Format::edge_table;
  impl->source.pathway = core::to_utf8(path.stem());
  impl->id_map = ids.digest();
  TableReader reader(path, ids, *impl);
  detail::for_each_line(bytes, [&](std::string_view line, std::uint32_t number) { reader.line(line, number); });
  reader.finish(ctx);
  std::shared_ptr<const PathwayDoc> doc = std::make_shared<PathwayDoc>(std::move(impl));
  const std::string file = detail::display(path);
  const auto kept = kept_relations(*doc, detail::filters_from(ctx.settings));
  if (kept.empty())
    ctx.log.warning("network.empty", file + (doc->relations().empty() ? " has no relations" : " has no relation left after the filters") +
                    ", so it gives no edges");
  ctx.events.emit("network.read", core::Json{{"path", file}, {"format", "edges"}, {"pathway", doc->source().pathway},
                                             {"entries", doc->entries().size()}, {"relations", doc->relations().size()},
                                             {"kept", kept.size()}, {"unscored", reader.unscored()},
                                             {"ms", detail::elapsed_ms(started)}});
  return doc;
}

}  // namespace fusion::network
