// Readers of FuSION 1's files (fusion1.hpp). Each follows the format as the spec records it (S2.06,
// S2.12, S5.15, S5.24, S5.25, S5.27) and shares no code with 2.0's readers.
#include "fusion/testkit/fusion1.hpp"

#include <algorithm>
#include <map>
#include <set>

#include <pugixml.hpp>

#include "fusion/core/core.hpp"
#include "util.hpp"

namespace fusion::testkit::f1 {

using detail::integers;
using detail::lines_of;
using detail::slurp;
using detail::split;
using detail::words;

std::vector<Point> read_dat(const std::filesystem::path& path) {
  std::vector<Point> out;
  for (const std::string& line : lines_of(slurp(path))) {
    const auto v = integers(line);
    if (v.size() >= 2) out.push_back({v[0], v[1]});
  }
  std::sort(out.begin(), out.end());
  return out;
}

// ---------------------------------------------------------------- solutions and relaxations (S5.27)

namespace {

// FuSION 1 joins an edge's subtypes with '_' and writes '-' for none; dummy_u_to_d holds '_' itself.
std::vector<std::string> edge_subtypes(const std::string& field) {
  std::vector<std::string> out;
  if (field == "-" || field.empty()) return out;
  const std::vector<std::string> parts = split(field, '_');
  for (std::size_t k = 0; k < parts.size(); ++k) {
    if (parts[k] == "dummy" && k + 3 < parts.size() && parts[k + 1] == "u" && parts[k + 2] == "to" && parts[k + 3] == "d") {
      k += 3;   // the tag is a flag, not a subtype (S2.21)
      continue;
    }
    if (!parts[k].empty()) out.push_back(parts[k]);
  }
  return out;
}

struct Section {
  Point point;
  std::string reason;
  bool solved = false;
  std::vector<std::vector<std::string>> nodes, edges;   // each line's tab-separated fields
};

// "Current PO point: <n> nodes, <e> edges relaxed", then "timeout" or "Solution 1", node lines, a blank
// line, edge lines; later solutions ("Solution 2", ...) are not read.
std::vector<Section> sections(const std::filesystem::path& path) {
  std::vector<Section> out;
  enum class State { header, nodes, edges, done } state = State::done;
  for (const std::string& line : lines_of(slurp(path))) {
    if (line.starts_with("Current PO point:")) {
      const auto v = integers(line);
      out.push_back({});
      if (v.size() >= 2) out.back().point = {v[0], v[1]};
      state = State::header;
      continue;
    }
    if (out.empty()) continue;
    Section& s = out.back();
    if (state == State::header) {
      if (line == "Solution 1") {
        s.solved = true;
        state = State::nodes;
      } else if (!line.empty() && s.reason.empty()) {
        s.reason = detail::trim(line);
      }
    } else if (state == State::nodes) {
      if (line.empty()) {
        state = State::edges;
      } else {
        const auto f = split(line, '\t');
        if (f.size() >= 7 && (f[1] == "present" || f[1] == "not_present")) {   // no node line: the edges began
          s.edges.push_back(f);
          state = State::edges;
        } else {
          s.nodes.push_back(f);
        }
      }
    } else if (state == State::edges) {
      if (line.empty() || line.starts_with("Solution ")) {
        if (!s.edges.empty() || line.starts_with("Solution ")) state = State::done;
      } else {
        s.edges.push_back(split(line, '\t'));
      }
    }
  }
  return out;
}

EdgeKey edge_of(const std::vector<std::string>& f) {
  if (f.size() < 7) detail::fail("a FuSION 1 edge line has fewer than 7 fields");
  return EdgeKey{f[3], f[5], edge_subtypes(f[2])};
}

}  // namespace

std::vector<PointRecord> read_solutions(const std::filesystem::path& solutions, const std::filesystem::path& relaxations) {
  std::vector<PointRecord> out;
  const std::vector<Section> sol = sections(solutions);
  std::vector<Section> rel;
  std::error_code ec;
  if (!relaxations.empty() && std::filesystem::exists(relaxations, ec)) rel = sections(relaxations);
  for (std::size_t k = 0; k < sol.size(); ++k) {
    PointRecord r;
    r.point = sol[k].point;
    r.reason = sol[k].reason;
    if (sol[k].solved) {
      Witness w;
      for (const auto& f : sol[k].nodes)
        if (f.size() >= 6) w.present_nodes.push_back({f[1], f[4] == "present", f[5] == "active"});
      for (const auto& f : sol[k].edges)
        if (f.size() >= 7 && f[1] == "present") w.selected_edges.push_back(edge_of(f));
      const auto match = std::find_if(rel.begin(), rel.end(), [&](const Section& s) { return s.point == r.point; });
      if (match != rel.end()) {
        for (const auto& f : match->nodes)
          if (f.size() >= 2) w.relaxed_nodes.push_back(f[1]);
        for (const auto& f : match->edges)
          if (f.size() >= 7) w.relaxed_edges.push_back(edge_of(f));
      }
      r.witness = std::move(w);
    }
    out.push_back(std::move(r));
  }
  return out;
}

// ---------------------------------------------------------------- limits files (S5.24)

Limits read_limits(const std::filesystem::path& path) {
  Limits limits;
  bool have_box = false;
  std::string phase = "diagonal";
  std::optional<Check> pending;
  for (const std::string& line : lines_of(slurp(path))) {
    if (line.starts_with("new call")) {
      phase = "diagonal";
      if (!have_box) {                                   // "node relax in [nl, nu] and edge relax in [el, eu]"
        const auto v = integers(line);
        if (v.size() >= 4) {
          limits.box_lo = {v[0], v[2]};
          limits.box_hi = {v[1], v[3]};
          have_box = true;
        }
      }
    } else if (line == "Finding smallest n") {
      phase = "smallest_n";
    } else if (line == "Finding smallest e") {
      phase = "smallest_e";
    } else if (line.starts_with("Relax bounds")) {      // e before n
      const auto v = integers(line);
      if (v.size() < 2) continue;
      const std::string step = line.find("node call") != std::string::npos   ? "smallest_n"
                               : line.find("edge call") != std::string::npos ? "smallest_e"
                                                                             : "diagonal";
      pending = Check{{v[1], v[0]}, step, CheckOutcome::not_checked, 0, false};
    } else if (line.starts_with("Time taken (sec):") && pending) {
      pending->cpu_s = detail::leading_double(line.substr(17)).value_or(0);
    } else if (line.starts_with("Point ") && line.find("already") != std::string::npos) {
      const auto v = integers(line);
      if (v.size() >= 2) pending = Check{{v[1], v[0]}, phase, CheckOutcome::not_checked, 0, true};
    } else if (line.starts_with("Point 1:") || line.starts_with("Point 2:")) {
      const auto v = integers(line.substr(8));
      if (v.size() >= 2) limits.found.push_back({v[1], v[0]});
    } else if ((line == "sat" || line == "unsat" || line == "unknown") && pending) {
      pending->answer = line == "sat" ? CheckOutcome::sat : line == "unsat" ? CheckOutcome::unsat : CheckOutcome::unknown;
      limits.checks.push_back(*pending);
      pending.reset();
    }
  }
  return limits;
}

// ---------------------------------------------------------------- written graphs (S2.06, S2.07, S2.09, S2.12)

namespace {

std::string normalise_subtype(const std::string& name) {      // S2.10
  if (name == "indirect effect") return "indirect";
  if (name == "missing interaction") return "missing";
  if (name == "state change") return "state";
  if (name == "binding/association") return "association";
  return name;
}

struct F1Node {
  std::string key;                         // FuSION 1's key
  std::vector<std::string> ids;            // all_id
  std::string type;
  std::vector<std::size_t> components;     // a complex's component nodes
  bool complex = false;
};

class WrittenGraph {
 public:
  explicit WrittenGraph(const std::filesystem::path& path) {
    const std::string text = slurp(path);
    pugi::xml_document doc;
    const auto parsed = doc.load_buffer(text.data(), text.size());
    if (!parsed) detail::fail(core::to_utf8(path) + ": " + parsed.description());
    const pugi::xml_node root = doc.child("pathway");
    if (!root) detail::fail(core::to_utf8(path) + ": no <pathway> root");
    for (pugi::xml_node e : root.children("entry")) {
      Entry entry;
      entry.type = e.attribute("type").value();
      for (const std::string& id : words(e.attribute("name").value())) entry.ids.push_back(detail::strip_colons(id));
      std::sort(entry.ids.begin(), entry.ids.end());
      entry.ids.erase(std::unique(entry.ids.begin(), entry.ids.end()), entry.ids.end());
      for (pugi::xml_node c : e.children("component")) entry.components.push_back(c.attribute("id").value());
      entries_[e.attribute("id").value()] = std::move(entry);
    }
    for (pugi::xml_node r : root.children("relation")) {
      const std::vector<std::size_t> from = nodes_of(r.attribute("entry1").value());
      const std::vector<std::size_t> to = nodes_of(r.attribute("entry2").value());
      std::vector<std::string> subtypes;
      bool tag = false, indirect = false;
      for (pugi::xml_node s : r.children("subtype")) {
        const std::string name = normalise_subtype(s.attribute("name").value());
        if (name == "dummy_u_to_d") {
          tag = true;
          continue;
        }
        if (name == "indirect") indirect = true;
        subtypes.push_back(name);
      }
      if (indirect) continue;                                 // S2.11, after the end nodes exist
      std::vector<std::string> pathways = words(r.attribute("pathways").value());
      const double score = detail::leading_double(r.attribute("experiment_score").value()).value_or(0);
      for (std::size_t s : from)
        for (std::size_t t : to) edges_.push_back({s, t, r.attribute("type").value(), subtypes, tag, pathways, score});
    }
  }

  CanonGraph canonical() const {
    CanonGraph g;
    std::vector<std::string> keys(nodes_.size());
    std::vector<std::vector<std::string>> members(nodes_.size());
    for (std::size_t v = 0; v < nodes_.size(); ++v) canon_of(v, keys, members);
    for (std::size_t v = 0; v < nodes_.size(); ++v)
      g.nodes.push_back({keys[v], members[v], nodes_[v].complex ? "group" : nodes_[v].type});
    for (const Edge& e : edges_) {
      CanonEdge c;
      c.key = {keys[e.src], keys[e.tgt], e.type, e.subtypes};
      c.dummy_tag = e.tag;
      c.pathways = e.pathways;
      c.score = e.score;
      g.edges.push_back(std::move(c));
    }
    normalise(g);
    return g;
  }

 private:
  struct Entry {
    std::vector<std::string> ids;
    std::string type;
    std::vector<std::string> components;
    bool visiting = false;
  };
  struct Edge {
    std::size_t src, tgt;
    std::string type;
    std::vector<std::string> subtypes;
    bool tag;
    std::vector<std::string> pathways;
    double score;
  };

  // The entry's node set (S2.07, S2.09), made on first use.
  std::vector<std::size_t> nodes_of(const std::string& entry_id) {
    const auto found = made_.find(entry_id);
    if (found != made_.end()) return found->second;
    auto it = entries_.find(entry_id);
    if (it == entries_.end()) detail::fail("a relation or component names a missing entry " + entry_id);
    Entry& entry = it->second;
    if (entry.visiting) detail::fail("a group contains itself: entry " + entry_id);
    entry.visiting = true;
    std::vector<std::size_t> out;
    if (entry.type == "group") {
      std::vector<std::vector<std::size_t>> parts;
      for (const std::string& c : entry.components) parts.push_back(nodes_of(c));
      std::vector<std::vector<std::size_t>> combos{{}};
      for (const auto& part : parts) {
        std::vector<std::vector<std::size_t>> next;
        for (const auto& combo : combos)
          for (std::size_t n : part) {
            auto c = combo;
            c.push_back(n);
            next.push_back(std::move(c));
          }
        combos = std::move(next);
      }
      for (auto combo : combos) {
        std::sort(combo.begin(), combo.end());
        combo.erase(std::unique(combo.begin(), combo.end()), combo.end());
        if (combo.empty()) continue;
        auto [c, fresh] = complexes_.try_emplace(combo, nodes_.size());
        if (fresh) {
          F1Node node;
          node.complex = true;
          node.type = "group";
          node.components = combo;
          std::vector<std::string> keys;
          for (std::size_t k : combo) keys.push_back(nodes_[k].key);
          detail::sort_unique(keys);
          node.key = "c_s_" + detail::join(keys, "_") + "_c_e";
          nodes_.push_back(std::move(node));
        }
        out.push_back(c->second);
      }
    } else if (!entry.ids.empty()) {
      const std::string key = entry.ids.front();             // threshold 1, no exception: one node, min id
      auto [k, fresh] = by_key_.try_emplace(key, nodes_.size());
      if (fresh) nodes_.push_back({key, entry.ids, entry.type, {}, false});
      out.push_back(k->second);                              // an existing key keeps its ids (S2.08)
    }
    entry.visiting = false;
    made_[entry_id] = out;
    return out;
  }

  void canon_of(std::size_t v, std::vector<std::string>& keys, std::vector<std::vector<std::string>>& members) const {
    if (!keys[v].empty()) return;
    const F1Node& n = nodes_[v];
    if (!n.complex) {
      keys[v] = node_key(n.ids);
      members[v] = n.ids;
      return;
    }
    std::vector<std::string> parts, ids;
    for (std::size_t c : n.components) {
      canon_of(c, keys, members);
      parts.push_back(keys[c]);
      ids.insert(ids.end(), members[c].begin(), members[c].end());
    }
    keys[v] = complex_key(parts);
    detail::sort_unique(ids);
    members[v] = ids;
  }

  std::map<std::string, Entry> entries_;
  std::map<std::string, std::vector<std::size_t>> made_;
  std::map<std::string, std::size_t> by_key_;
  std::map<std::vector<std::size_t>, std::size_t> complexes_;
  std::vector<F1Node> nodes_;
  std::vector<Edge> edges_;
};

}  // namespace

CanonGraph read_written_graph(const std::filesystem::path& path) { return WrittenGraph(path).canonical(); }

std::vector<std::string> read_ids(const std::filesystem::path& path) { return words(slurp(path)); }

// ---------------------------------------------------------------- GSEA tables, consoles, coexpression

std::vector<GseaRow> read_gsea_table(const std::filesystem::path& path) {
  const std::vector<std::string> lines = lines_of(slurp(path));
  std::vector<GseaRow> out;
  if (lines.empty()) return out;
  const std::vector<std::string> header = split(lines[0], '\t');
  const auto column = [&](const char* name) -> std::optional<std::size_t> {
    const auto it = std::find(header.begin(), header.end(), name);
    if (it == header.end()) return std::nullopt;
    return static_cast<std::size_t>(it - header.begin());
  };
  const auto text = [&](const std::vector<std::string>& f, const char* name) {
    const auto c = column(name);
    return c && *c < f.size() ? f[*c] : std::string();
  };
  const auto number = [&](const std::vector<std::string>& f, const char* name) {
    return detail::leading_double(text(f, name)).value_or(0);
  };
  for (std::size_t k = 1; k < lines.size(); ++k) {
    if (lines[k].empty()) continue;
    const std::vector<std::string> f = split(lines[k], '\t');
    GseaRow r;
    r.term = text(f, "Term");
    r.pathway_name = text(f, "pathway_name");
    r.genes = static_cast<int>(number(f, "genes"));
    r.es = number(f, "ES");
    r.nes = number(f, "NES");
    r.nom_p = number(f, "NOM p-val");
    r.fdr_q = number(f, "FDR q-val");
    r.fwer_p = number(f, "FWER p-val");
    r.tag_pct = text(f, "Tag %");
    r.gene_pct = text(f, "Gene %");
    r.lead_genes = text(f, "Lead_genes");
    r.selected = text(f, "selected") == "True";
    r.kgml = text(f, "kgml");
    out.push_back(std::move(r));
  }
  return out;
}

ConsoleFacts read_console(const std::filesystem::path& path) {
  ConsoleFacts facts;
  for (const std::string& line : lines_of(slurp(path))) {
    if (line.starts_with("Up: ") && line.find(" Down: ") != std::string::npos) {
      const auto v = integers(line);
      if (v.size() >= 2) facts.up_down.emplace_back(v[0], v[1]);
    } else if (line.starts_with("New graph ") && line.find(" has ") != std::string::npos &&
               line.find(" nodes ") != std::string::npos) {
      const auto v = integers(line);
      if (v.size() >= 3) facts.corridor_sizes.emplace_back(v[1], v[2]);
    } else if (line.starts_with("[coexpression] emitted ")) {
      const auto v = integers(line);
      if (!v.empty()) facts.coexpression_clauses.push_back(v[0]);
    } else if (line.starts_with("[frozen-edge] emitted ")) {
      const auto v = integers(line);
      if (!v.empty()) facts.frozen_clauses.push_back(v[0]);
    }
  }
  return facts;
}

std::vector<std::pair<std::string, std::string>> read_coexpression(const std::filesystem::path& csv, double threshold) {
  const std::vector<std::string> lines = lines_of(slurp(csv));
  std::vector<std::pair<std::string, std::string>> out;
  if (lines.empty()) return out;
  const std::vector<std::string> header = split(lines[0], ',');
  const auto column = [&](const char* name) {
    const auto it = std::find_if(header.begin(), header.end(), [&](const std::string& h) { return detail::trim(h) == name; });
    if (it == header.end()) detail::fail(core::to_utf8(csv) + ": no column " + name);
    return static_cast<std::size_t>(it - header.begin());
  };
  const std::size_t a = column("hsa_id_node1"), b = column("hsa_id_node2"), s = column("coexpression_score");
  for (std::size_t k = 1; k < lines.size(); ++k) {
    const std::vector<std::string> f = split(lines[k], ',');
    if (f.size() <= std::max({a, b, s})) continue;
    const std::optional<double> score = detail::leading_double(f[s]);
    if (!score || !(*score > threshold)) continue;            // strict > (S5.15)
    for (const std::string& x : words(f[a]))
      for (const std::string& y : words(f[b])) {
        std::string u = detail::strip_colons(x), v = detail::strip_colons(y);
        if (u == v) continue;
        if (v < u) std::swap(u, v);
        out.emplace_back(u, v);
      }
  }
  detail::sort_unique(out);
  return out;
}

}  // namespace fusion::testkit::f1
