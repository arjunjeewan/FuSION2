// The reference cases and their folders (case.hpp).
#include "ref/case.hpp"

#include <algorithm>

#include "files.hpp"
#include "fusion/batch/batch.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;

namespace {

// run_fusion1.py's FUSION1 and SNAPSHOT: a mapped batch or list file that still names either is refused.
constexpr std::string_view kFusion1Tree = "/home/ks-149-u24/Desktop/arjun/FuSION";
constexpr std::string_view kSnapshot = "/home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10";

[[noreturn]] void bad(const std::string& where, const std::string& why) {
  throw core::Failure("ref.cases", "cases.json: " + where + ": " + why);
}

const Json& field(const Json& j, const char* key, const std::string& where) {
  if (!j.is_object() || !j.contains(key)) bad(where, std::string("no field \"") + key + "\"");
  return j[key];
}
std::string text_of(const Json& j, const char* key, const std::string& where) {
  const Json& v = field(j, key, where);
  if (!v.is_string()) bad(where, std::string("\"") + key + "\" is not a string");
  return v.get<std::string>();
}
fs::path path_of(const Json& j, const char* key, const std::string& where) {
  return core::path_from_utf8(text_of(j, key, where));
}
std::optional<fs::path> optional_path(const Json& j, const char* key, const std::string& where) {
  if (!j.contains(key)) return std::nullopt;
  return path_of(j, key, where);
}

}  // namespace

std::string_view name(Tier t) noexcept {
  switch (t) {
    case Tier::t1: return "T1";
    case Tier::t2: return "T2";
    case Tier::t3: return "T3";
  }
  return "T1";
}

std::optional<Tier> parse_tier(std::string_view s) noexcept {
  if (s == "T1" || s == "t1") return Tier::t1;
  if (s == "T2" || s == "t2") return Tier::t2;
  if (s == "T3" || s == "t3") return Tier::t3;
  return std::nullopt;
}

std::vector<Case> load_cases(const fs::path& cases_json) {
  Json doc;
  try {
    doc = Json::parse(fusiontest::slurp(cases_json));
  } catch (const Json::exception& e) {
    throw core::Failure("ref.cases", core::to_utf8(cases_json) + ": " + e.what());
  }
  if (!doc.is_object() || !doc.contains("cases") || !doc["cases"].is_array()) bad("the file", "no \"cases\" list");
  std::vector<Case> out;
  for (const Json& c : doc["cases"]) {
    Case k;
    k.id = text_of(c, "id", "a case");
    const std::string where = "case " + k.id;
    k.batch = path_of(c, "batch", where);
    const auto tier = parse_tier(text_of(c, "tier", where));
    if (!tier) bad(where, "tier is not T1, T2 or T3");
    k.tier = *tier;
    k.source = text_of(c, "source", where);
    for (const Json& m : field(c, "path_map", where)) k.path_map.push_back({text_of(m, "from", where), text_of(m, "to", where)});
    const Json& f = field(c, "fusion1", where);
    k.fusion1.up = path_of(f, "up", where);
    k.fusion1.down = path_of(f, "down", where);
    k.fusion1.exceptions = path_of(f, "exceptions", where);
    k.fusion1.console = path_of(f, "console", where);
    k.fusion1.gsea_table = optional_path(f, "gsea_table", where);
    k.fusion1.shortlist = optional_path(f, "shortlist", where);
    k.fusion1.reread = path_of(f, "reread", where);
    k.fusion1.corridor = path_of(f, "corridor", where);
    for (const auto& [name, s] : field(f, "solve", where).items()) {
      if (name != "in" && name != "out" && name != "free") bad(where, "solve case " + name + " is not in, out or free");
      SolveArtefacts a;
      a.dat = path_of(s, "dat", where);
      a.limits = path_of(s, "limits", where);
      a.solutions = optional_path(s, "solutions", where);
      a.relaxations = optional_path(s, "relaxations", where);
      k.fusion1.solve.emplace(name, std::move(a));
    }
    if (c.contains("fusion1_seconds"))
      for (const auto& [name, v] : c["fusion1_seconds"].items()) k.fusion1_seconds[name] = v.get<double>();
    k.origin = c.value("origin", Json::object());
    if (c.contains("edits"))
      for (const Json& e : c["edits"]) {
        if (!e.is_object()) bad(where, "an edit is not an object");
        Edit edit;
        for (const auto& [key, value] : e.items()) {
          if (key == "why") {
            edit.why = value.get<std::string>();
          } else {
            if (!edit.op.empty()) bad(where, "an edit names two operations");
            edit.op = key;
            edit.arg = value.is_string() ? value.get<std::string>() : value.dump();
          }
        }
        if (edit.op.empty() || edit.why.empty()) bad(where, "an edit needs an operation and a \"why\"");
        k.edits.push_back(std::move(edit));
      }
    out.push_back(std::move(k));
  }
  for (std::size_t a = 0; a < out.size(); ++a)
    for (std::size_t b = a + 1; b < out.size(); ++b)
      if (out[a].id == out[b].id) bad("case " + out[a].id, "the id appears twice");
  return out;
}

std::optional<fs::path> artefact_dir(const Case& c, const Roots& roots) {
  if (c.tier == Tier::t1) return roots.synthetic / c.id;
  if (!roots.data) return std::nullopt;
  return roots.data->at("reference/" + c.id);
}

std::string apply_path_map(std::string text, std::span<const PathMap> map, const fs::path& case_dir,
                           const std::optional<fs::path>& data_root) {
  std::vector<PathMap> order(map.begin(), map.end());
  std::stable_sort(order.begin(), order.end(), [](const PathMap& a, const PathMap& b) { return a.from.size() > b.from.size(); });
  for (const PathMap& m : order) {
    std::string to = m.to;
    const auto expand = [&](std::string_view token, const std::string& value) {
      for (std::size_t at; (at = to.find(token)) != std::string::npos;) to.replace(at, token.size(), value);
    };
    expand("$CASE", core::to_utf8(case_dir));
    if (to.find("$DATA") != std::string::npos) {
      if (!data_root) throw core::Failure("ref.prepare", "the path map names $DATA, and no data root is set");
      expand("$DATA", core::to_utf8(*data_root));
    }
    if (m.from.empty()) continue;
    for (std::size_t at = 0; (at = text.find(m.from, at)) != std::string::npos; at += to.size()) text.replace(at, m.from.size(), to);
  }
  return text;
}

std::optional<std::string> leftover_fusion1_path(std::string_view text) {
  const std::string padded = std::string(text) + "\n";
  for (const std::string& tree : {std::string(kFusion1Tree) + "/", std::string(kFusion1Tree) + "\n", std::string(kSnapshot)})
    if (padded.find(tree) != std::string::npos) return std::string(tree.back() == '\n' ? tree.substr(0, tree.size() - 1) : tree);
  return std::nullopt;
}

std::string apply_edits(std::string text, std::span<const Edit> edits, std::vector<std::string>& applied) {
  for (const Edit& e : edits) {
    if (e.op != "drop_pathz3_tail")
      throw core::Failure("ref.edit", "unknown edit '" + e.op + "': the runner knows drop_pathz3_tail (C4.1)");
    if (text.find(e.arg) == std::string::npos)
      throw core::Failure("ref.edit", "edit drop_pathz3_tail: the batch does not hold '" + e.arg + "'");
    // pathz3's 28 fixed tokens (06, "FuSION 1 positional lines"); the optional tail follows them.
    constexpr std::size_t kFixed = 28;
    std::string out;
    int lines = 0, number = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
      std::size_t end = text.find('\n', start);
      const bool last = end == std::string::npos;
      if (last) end = text.size();
      std::string line = text.substr(start, end - start);
      const batch::Line lexed = batch::lex(++number, line);
      if (lexed.kind == batch::LineKind::command && !lexed.tokens.empty() &&
          (lexed.tokens[0].text == "pathz3" || lexed.tokens[0].text == "get_path_with_z3") &&
          lexed.tokens.size() > kFixed + 1 && !lexed.tokens[1].name_end) {
        line = line.substr(0, static_cast<std::size_t>(lexed.tokens[kFixed + 1].column - 1));
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        ++lines;
      }
      out += line;
      if (!last) out += '\n';
      start = end + 1;
      if (last) break;
    }
    if (lines == 0) throw core::Failure("ref.edit", "edit drop_pathz3_tail: no pathz3 line has an optional tail");
    text = std::move(out);
    applied.push_back("drop_pathz3_tail " + e.arg + " on " + std::to_string(lines) + " pathz3 line" +
                      (lines == 1 ? "" : "s") + ": " + e.why);
  }
  return text;
}

Prepared prepare(const Case& c, const Roots& roots, const fs::path& case_dir) {
  std::error_code ec;
  if (fs::exists(case_dir, ec)) throw core::Failure("ref.prepare", core::to_utf8(case_dir) + " exists; every case gets a fresh folder");
  const auto artefacts = artefact_dir(c, roots);
  if (!artefacts) throw core::Failure("ref.prepare", c.id + " needs the data root");
  const fs::path batch_file = roots.fixtures / c.batch;
  if (!fs::is_regular_file(batch_file, ec)) throw core::Failure("ref.prepare", c.id + ": no batch " + core::to_utf8(batch_file));
  fs::create_directories(case_dir);
  Prepared p;
  p.dir = fs::absolute(case_dir);
  const std::optional<fs::path> data = roots.data ? std::optional<fs::path>(roots.data->root) : std::nullopt;
  const fs::path inputs = *artefacts / "inputs";
  if (fs::is_directory(inputs, ec)) {
    fs::copy(inputs, p.dir / "inputs", fs::copy_options::recursive, ec);
    if (ec) throw core::Failure("ref.prepare", c.id + ": cannot copy " + core::to_utf8(inputs) + ": " + ec.message());
    for (auto it = fs::recursive_directory_iterator(p.dir / "inputs"); it != fs::recursive_directory_iterator(); ++it) {
      if (!it->is_regular_file() || core::to_utf8(it->path().filename()).find("list") == std::string::npos) continue;
      const std::string mapped = apply_path_map(fusiontest::slurp(it->path()), c.path_map, p.dir, data);
      if (const auto left = leftover_fusion1_path(mapped))
        throw core::Failure("ref.leftover", c.id + ": the list file " + core::to_utf8(it->path().filename()) +
                                                " still names " + *left + " after the path map");
      fusiontest::write_file(it->path(), mapped);
    }
  }
  std::string text = apply_path_map(fusiontest::slurp(batch_file), c.path_map, p.dir, data);
  if (const auto left = leftover_fusion1_path(text))
    throw core::Failure("ref.leftover", c.id + ": the batch still names " + *left + " after the path map");
  text = apply_edits(std::move(text), c.edits, p.edits);
  p.batch = p.dir / "case.batch";
  fusiontest::write_file(p.batch, text);
  return p;
}

}  // namespace fusion::reftest
