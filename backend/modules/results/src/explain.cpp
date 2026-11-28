// results/src/explain.cpp: the explanation export (section 07, "Explanation export"; 07.C2 to 07.C5,
// S6.07, S6.08, S6.17, S6.18): the view files, the page shells with one shared copy of the library,
// the edge table, the static DOT and SVG export, and the run page's HTML. It replaces FuSION 1's two
// renderers, pex (Linux/supporting_files/plot_explanation.py) and psp
// (Linux/src/GraphManagerNew.cc:8823-9144), with one (S6.26).
#include "fusion/results/explain.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

#include "fusion/core/process.hpp"
#include "fusion/results/analyse.hpp"
#include "fusion/results/assets.hpp"
#include "text.hpp"

namespace fusion::results {

namespace {

using Json = core::Json;

const CaseResult* find_case(const SolveResult& r, Case kind) {
  for (const CaseResult& c : r.cases) {
    if (c.kind == kind) return &c;
  }
  return nullptr;
}

const NodeRecord& node(const SolveResult& r, NodeIx i) { return r.tables.nodes.at(i); }

std::optional<double> rep_lfc(const NodeRecord& n) {
  for (const Member& m : n.members) {
    if (m.id == n.key.value) return m.lfc;
  }
  return n.members.size() == 1 ? n.members.front().lfc : std::nullopt;
}

Json lfc_json(const std::optional<double>& v) { return v ? Json(*v) : Json(nullptr); }

std::size_t first_witness(std::size_t asked, std::size_t count) {
  if (count == 0) return 0;
  return std::min(std::max<std::size_t>(asked, 1), count) - 1;
}

// The colours the page and the static export share (S6.16; plot_explanation.py:32-46).
constexpr std::string_view fill_up = "#8CCB4E", fill_down = "#F2A24C", fill_none = "#DEDEDE";
constexpr std::string_view border_normal = "#555555", border_source = "#1B5E20", border_target = "#0D3B66";
constexpr std::string_view edge_activation = "#8a8a8a", edge_inhibition = "#C62828", edge_flipped = "#8E44AD";
constexpr std::string_view faded = "73";        // 45 % opacity as a DOT alpha byte (07.C4)

std::string_view fill_of(Observed o) {
  return o == Observed::up ? fill_up : o == Observed::down ? fill_down : fill_none;
}

}  // namespace

// ---------------------------------------------------------------- the view file (07.C2)

std::string view_json(const PointView& view, const SolveResult& r, const PathFilter& paths, std::size_t witness,
                      bool edge_table) {
  const CaseResult* c = find_case(r, view.kind);
  Json j = Json::object();
  j["format"] = view_format;
  j["version"] = view_version;
  j["solve"] = view.solve;
  j["case"] = case_name(view.kind);
  j["label"] = c != nullptr ? c->label : std::string(case_name(view.kind));
  j["n"] = view.at.n;
  j["e"] = view.at.e;
  j["title"] = view.title;
  j["subtitle"] = view.subtitle;
  j["certification"] = view.certification_note;
  j["certified"] = view.certified;
  j["missing"] = view.missing_reason;
  j["enumeration"] = enumeration_name(view.enumeration);
  j["paths"] = path_filter_text(paths);
  j["witness"] = first_witness(witness, view.witnesses.size()) + 1;
  j["edge_table"] = edge_table;
  j["source"] = node(r, r.header.source).key.value;
  j["target"] = node(r, r.header.target).key.value;
  Json fsc = Json::array();
  for (const NodeIx f : r.header.fsc) fsc.push_back(node(r, f).key.value);
  j["fsc"] = std::move(fsc);
  // The case's observation counts by status, zeros left out (S5.12, S3.12, S4.16).
  Json observations = Json::object();
  if (c != nullptr) {
    std::map<ObservationStatus, std::size_t> counts;
    for (const Observation& o : c->observations) ++counts[o.status];
    for (const auto& [status, count] : counts) observations[std::string(observation_status_name(status))] = count;
  }
  j["observations"] = std::move(observations);
  std::set<std::string> used;
  for (const WitnessView& w : view.witnesses) {
    for (const ViewEdge& e : w.edges) {
      for (const EdgeIx m : e.members) used.insert(r.tables.edges[m].pathways.begin(), r.tables.edges[m].pathways.end());
    }
  }
  Json pathways = Json::object();
  for (const std::string& id : used) {
    const auto it = r.tables.pathway_names.find(id);
    pathways[id] = it != r.tables.pathway_names.end() ? it->second : id;
  }
  j["pathways"] = std::move(pathways);
  Json witnesses = Json::array();
  for (const WitnessView& w : view.witnesses) {
    Json wj = Json::object();
    Json counts = Json::object();
    counts["n"] = w.counts.n;
    counts["e"] = w.counts.e;
    wj["counts"] = std::move(counts);
    wj["shortest"] = w.shortest ? Json(*w.shortest) : Json(nullptr);
    wj["route"] = w.route_note;
    Json nodes = Json::array();
    for (const ViewNode& v : w.nodes) {
      const NodeRecord& rec = node(r, v.node);
      Json nj = Json::object();
      nj["key"] = rec.key.value;
      nj["label"] = rec.label;
      nj["kind"] = kind_name(rec.kind);
      nj["observed"] = observed_name(rec.observed);
      nj["present"] = v.present;
      nj["active"] = v.active;
      nj["relaxed"] = v.relaxed;
      nj["level"] = v.level;
      Json roles = Json::array();
      if (v.roles & role_source) roles.push_back("source");
      if (v.roles & role_target) roles.push_back("target");
      if (v.roles & role_fsc) roles.push_back("fsc");
      nj["roles"] = std::move(roles);
      Json members = Json::array();
      for (const Member& m : rec.members) {
        Json mj = Json::object();
        mj["id"] = m.id;
        mj["symbol"] = m.symbol;
        mj["lfc"] = lfc_json(m.lfc);
        members.push_back(std::move(mj));
      }
      nj["members"] = std::move(members);
      nodes.push_back(std::move(nj));
    }
    wj["nodes"] = std::move(nodes);
    Json edges = Json::array();
    for (const ViewEdge& e : w.edges) {
      Json ej = Json::object();
      ej["source"] = node(r, e.source).key.value;
      ej["target"] = node(r, e.target).key.value;
      ej["effect"] = effect_name(e.effect);
      ej["replaced"] = e.replaced ? Json(std::string(effect_name(*e.replaced))) : Json(nullptr);
      ej["draw"] = draw_name(e.draw);
      ej["in_witness"] = e.in_witness;
      ej["ecrel"] = e.ecrel;
      ej["on_route"] = e.on_route;
      ej["slack"] = e.slack ? Json(*e.slack) : Json(nullptr);
      Json members = Json::array();
      for (const EdgeIx m : e.members) {
        const EdgeRecord& rec = r.tables.edges[m];
        Json mj = Json::object();
        mj["key"] = rec.key.text();
        mj["type"] = rec.type;
        mj["subtypes"] = rec.subtypes;
        mj["pathways"] = rec.pathways;
        members.push_back(std::move(mj));
      }
      ej["members"] = std::move(members);
      edges.push_back(std::move(ej));
    }
    wj["edges"] = std::move(edges);
    Json parked = Json::array();
    for (const ParkedNode& p : w.parked) {
      const NodeRecord& rec = node(r, p.node);
      Json pj = Json::object();
      pj["key"] = rec.key.value;
      pj["label"] = rec.label;
      pj["observed"] = observed_name(rec.observed);
      pj["lfc"] = lfc_json(rep_lfc(rec));
      pj["reason"] = p.reason;
      parked.push_back(std::move(pj));
    }
    wj["parked"] = std::move(parked);
    witnesses.push_back(std::move(wj));
  }
  j["witnesses"] = std::move(witnesses);
  return detail::dump(j);
}

std::string view_json(const PointView& view, const SolveResult& r) {
  return view_json(view, r, PathFilter{}, 1, false);
}

// ---------------------------------------------------------------- the page shell (07.C2, S6.17, S6.18)

std::string page_html(std::string_view json, std::string_view lib_href, std::string_view title,
                      std::string_view json_name) {
  // Generated directly, not by editing a library's template (S6.17; FuSION 1 patched pyvis's page
  // with regular expressions, plot_explanation.py:486-510). The data are inline and the library is
  // relative, so the page fetches nothing and opens from file:// (S6.18). No inline executable script.
  std::string lib(lib_href);
  if (!lib.empty() && lib.back() != '/') lib += '/';
  const std::string escaped_lib = detail::html_escape(lib);
  std::string html = "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
                     "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n";
  html += "<title>" + detail::html_escape(title) + "</title>\n";
  html += "<link rel=\"stylesheet\" href=\"" + escaped_lib + "fusion-explain.css\">\n";
  html += "<script src=\"" + escaped_lib + "vis-network.min.js\"></script>\n";
  html += "<script src=\"" + escaped_lib + "fusion-explain.js\" defer></script>\n";
  html += "</head><body>\n";
  html += "<script type=\"application/json\" id=\"fusion-view\">" + detail::json_for_html(json) + "</script>\n";
  html += "<noscript>This page needs JavaScript.";
  if (!json_name.empty()) html += " The same explanation is in " + detail::html_escape(json_name) + ".";
  html += "</noscript>\n</body></html>\n";
  return html;
}

std::string page_html(std::string_view json, std::string_view lib_href, std::string_view title) {
  return page_html(json, lib_href, title, {});
}

// ---------------------------------------------------------------- the edge table (S6.08)

std::string edge_table_tsv(const WitnessView& view, const SolveResult& r, const PathFilter& filter) {
  // One row per drawn edge listing all its edges, fields free of tabs and line breaks, lists joined by
  // ';' (S6.08 read through E1). psp wrote one CSV row per edge line, so its rows did not match the
  // drawn edges (GraphManagerNew.cc:9114-9138).
  const auto kept = kept_by(r, view, filter);
  std::vector<const ViewEdge*> rows;
  for (std::size_t i = 0; i < view.edges.size(); ++i) {
    if (kept[i]) rows.push_back(&view.edges[i]);
  }
  std::stable_sort(rows.begin(), rows.end(), [&](const ViewEdge* a, const ViewEdge* b) {
    if (auto o = core::stable_compare(node(r, a->source).label, node(r, b->source).label); o != 0) return o < 0;
    if (auto o = core::stable_compare(node(r, a->target).label, node(r, b->target).label); o != 0) return o < 0;
    return a->effect < b->effect;
  });
  std::string out = "source\tsource_key\ttarget\ttarget_key\teffect\tdraw\tin_witness\ttypes\tsubtypes\tedge_keys\t"
                    "pathway_ids\tpathway_names\n";
  const auto add_unique = [](std::vector<std::string>& list, const std::string& item) {
    if (std::find(list.begin(), list.end(), item) == list.end()) list.push_back(item);
  };
  for (const ViewEdge* e : rows) {
    std::vector<std::string> types, subtypes, keys, pathways;
    for (const EdgeIx m : e->members) {
      const EdgeRecord& rec = r.tables.edges[m];
      add_unique(types, rec.type);
      add_unique(subtypes, detail::join(rec.subtypes, ","));
      keys.push_back(rec.key.text());
      for (const std::string& p : rec.pathways) add_unique(pathways, p);
    }
    std::sort(pathways.begin(), pathways.end(),
              [](const std::string& a, const std::string& b) { return core::stable_compare(a, b) < 0; });
    std::vector<std::string> names;
    for (const std::string& p : pathways) {
      const auto it = r.tables.pathway_names.find(p);
      names.push_back(it != r.tables.pathway_names.end() ? it->second : p);
    }
    const std::vector<std::string> fields{node(r, e->source).label, node(r, e->source).key.value,
                                          node(r, e->target).label, node(r, e->target).key.value,
                                          std::string(effect_name(e->effect)), std::string(draw_name(e->draw)),
                                          e->in_witness ? "yes" : "no", detail::join(types, ";"),
                                          detail::join(subtypes, ";"), detail::join(keys, ";"),
                                          detail::join(pathways, ";"), detail::join(names, ";")};
    for (std::size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) out += '\t';
      out += detail::tsv_field(fields[i]);
    }
    out += '\n';
  }
  return out;
}

// ---------------------------------------------------------------- the static export (S6.07, 07.C5)

std::string dot_source(const WitnessView& view, const SolveResult& r, const PathFilter& filter,
                       std::string_view graph_name) {
  // The page's colours and styles in DOT, every quoted string escaped (S6.07: psp wrote labels into
  // DOT unescaped, GraphManagerNew.cc:9073-9112).
  const auto kept = kept_by(r, view, filter);
  const auto drawn = drawn_nodes(r, view, filter, kept);
  std::string out = "digraph " + detail::dot_quote(graph_name) + " {\n";
  out += "  rankdir=LR;\n";
  out += "  node [shape=box, style=filled, fontname=\"Helvetica\", color=\"" + std::string(border_normal) +
         "\", fillcolor=\"" + std::string(fill_none) + "\"];\n";
  for (const NodeIx v : drawn) {
    const auto it = std::find_if(view.nodes.begin(), view.nodes.end(), [&](const ViewNode& n) { return n.node == v; });
    if (it == view.nodes.end()) continue;
    const NodeRecord& rec = node(r, v);
    const bool fade = !it->active;              // 07.C4
    std::string attrs = "label=" + detail::dot_quote(rec.label);
    const std::string_view fill = fill_of(rec.observed);
    if (fill != fill_none || fade) attrs += ", fillcolor=\"" + std::string(fill) + (fade ? std::string(faded) : "") + "\"";
    std::string_view border = border_normal;
    int width = 1;
    if (it->roles & role_source) {
      border = border_source;
      width = 4;
    } else if (it->roles & role_target) {
      border = border_target;
      width = 4;
    }
    if (border != border_normal || fade) attrs += ", color=\"" + std::string(border) + (fade ? std::string(faded) : "") + "\"";
    if (it->relaxed) width = std::max(width, 2);
    if (width != 1) attrs += ", penwidth=" + std::to_string(width);
    if (it->relaxed) attrs += ", style=\"filled,dotted\"";
    out += "  " + detail::dot_quote(rec.key.value) + " [" + attrs + "];\n";
  }
  for (std::size_t i = 0; i < view.edges.size(); ++i) {
    if (!kept[i]) continue;
    const ViewEdge& e = view.edges[i];
    std::string colour(e.draw == Draw::flipped ? edge_flipped
                       : e.effect == network::Effect::inhibition ? edge_inhibition
                                                                 : edge_activation);
    std::string width = e.draw == Draw::removed ? "3" : "2";
    std::string style = e.draw == Draw::removed ? "dashed" : e.draw == Draw::flipped ? "dotted" : "";
    if (!e.in_witness) {                        // drawn like a removed edge, faded (S6.15)
      colour += faded;
      width = "1.5";
      style = "dashed";
    }
    std::string attrs = "color=\"" + colour + "\", arrowhead=" +
                        (e.effect == network::Effect::inhibition ? "tee" : "normal") + ", penwidth=" + width;
    if (!style.empty()) attrs += ", style=" + style;
    if (e.ecrel) attrs += ", label=\"EC\"";
    out += "  " + detail::dot_quote(node(r, e.source).key.value) + " -> " +
           detail::dot_quote(node(r, e.target).key.value) + " [" + attrs + "];\n";
  }
  out += "}\n";
  return out;
}

std::string dot_source(const WitnessView& view, const SolveResult& r, const PathFilter& filter) {
  return dot_source(view, r, filter, "explanation");
}

std::string page_stem(std::string_view prefix, std::string_view solve_name, Case kind, Point at) {
  // 07.C2 with cross-section item 1.6: in, out and free name the case; FuSION 1 wrote incl and excl.
  std::string stem;
  if (!prefix.empty()) stem += std::string(prefix) + "_";
  if (!solve_name.empty()) stem += detail::base_name(solve_name) + "_";
  stem += std::string(case_name(kind)) + "_n" + std::to_string(at.n) + "_e" + std::to_string(at.e);
  return stem;
}

// ---------------------------------------------------------------- the run page (S6.19, 1.4)

namespace {

constexpr std::string_view run_page_css =
    "*{box-sizing:border-box}"
    "body{margin:0;font-family:system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif;"
    "color:#111;background:#f4f4f1;line-height:1.5}"
    "a{color:#1c5cab;text-decoration:none}a:hover{text-decoration:underline}"
    ".wrap{max-width:1040px;margin:0 auto;padding:30px 22px 70px}"
    "h1{font-size:25px;margin:.1em 0}h2{font-size:19px;margin:1.4em 0 .2em}"
    ".sub{color:#54524e;font-size:14px;margin:.2em 0}"
    "figure{margin:18px 0 6px;background:#fcfcfb;border:1px solid #e3e2db;border-radius:12px;padding:10px}"
    "figure img{width:100%;height:auto;display:block;border-radius:6px}"
    ".panels{display:flex;gap:20px;flex-wrap:wrap;margin-top:16px}"
    ".panel{flex:1 1 320px;background:#fcfcfb;border:1px solid #e3e2db;border-radius:12px;padding:14px 16px}"
    ".panel h3{margin:0 0 8px;font-size:15px}"
    ".pos{display:flex;flex-direction:column;gap:7px}"
    ".po{display:flex;justify-content:space-between;background:#fff;border:1px solid #e3e2db;border-radius:8px;"
    "padding:8px 12px;font-size:13.5px;color:#222}"
    ".po b{color:#1c5cab}.po .nc{color:#8a5a00}"
    ".note{color:#54524e;font-size:13px}"
    ".bad{color:#a4161a;font-weight:600}.end{color:#a4161a;font-size:14px;margin:.2em 0}"
    "ul.rules{color:#54524e;font-size:13px}";

std::string esc(std::string_view s) { return detail::html_escape(s); }

std::string page_for(const RunPageInputs& in, const SolveResult& r, Case kind, Point at, bool name_needed) {
  // The page whose name ends in <case>_n<n>_e<e>.html, and holds the solve's name when several
  // solves were drawn (07.C2).
  const std::string tail = std::string(case_name(kind)) + "_n" + std::to_string(at.n) + "_e" +
                           std::to_string(at.e) + ".html";
  for (const std::string& page : in.pages) {
    const std::string base = detail::base_name(page);
    if (base.size() < tail.size() || base.compare(base.size() - tail.size(), tail.size(), tail) != 0) continue;
    if (base.size() > tail.size() && base[base.size() - tail.size() - 1] != '_') continue;
    if (name_needed && base.find(detail::base_name(r.header.name) + "_") == std::string::npos) continue;
    return page;
  }
  return {};
}

}  // namespace

std::string run_page_html(const RunPageInputs& in) {
  // Static HTML with no script, written once at the run's end (cross-section item 1.4). It keeps
  // FuSION 1's run page (plot_explanation.py:612-633): the Pareto image, then one panel per case with
  // its points in the order n + e, then n, each linked to its page.
  std::string title = "FuSION run";
  if (!in.solves.empty() && in.solves.front() != nullptr) {
    const SolveResult& r = *in.solves.front();
    title = node(r, r.header.source).label + " → " + node(r, r.header.target).label;
  }
  std::string body = "<h1>" + esc(title) + "</h1>\n";
  // The run's name and how it ended come first, so a failed run, or one with no solve, still says
  // something (Phase 3, prompt log entry 387).
  const std::string ended = in.end == RunEnd::finished ? "finished"
                            : in.end == RunEnd::failed ? "failed"
                                                       : "stopped on request";
  std::string sub = in.run_name.empty() ? std::string() : esc(in.run_name) + " · ";
  sub += in.end == RunEnd::finished ? ended : "<span class=\"bad\">" + ended + "</span>";
  body += "<p class=\"sub\">" + sub + "</p>\n";
  if (!in.end_note.empty()) body += "<p class=\"end\">" + esc(in.end_note) + "</p>\n";
  if (std::none_of(in.solves.begin(), in.solves.end(), [](const SolveResult* r) { return r != nullptr; })) {
    body += std::string("<p class=\"note\">") +
            (in.end == RunEnd::finished ? "This run has no solve, so there is no frontier to show."
                                        : "No solve finished before the run ended.") +
            " Its files are in this folder.</p>\n";
  }
  for (const std::string& plot : in.plots) {
    body += "<figure><img src=\"" + esc(plot) + "\" alt=\"Pareto frontier\"></figure>\n";
  }
  const bool several = in.solves.size() > 1;
  for (const SolveResult* rp : in.solves) {
    if (rp == nullptr) continue;
    const SolveResult& r = *rp;
    const Header& h = r.header;
    body += "<h2>" + esc(h.name) + " <span class=\"note\">(" + esc(h.step) + ")</span></h2>\n";
    std::vector<std::string> settings{"source " + node(r, h.source).label + " (" + node(r, h.source).key.value + ")",
                                      "target " + node(r, h.target).label + " (" + node(r, h.target).key.value + ")"};
    if (!h.fsc.empty()) {
      std::vector<std::string> fsc;
      for (const NodeIx f : h.fsc) fsc.push_back(node(r, f).label + " (" + node(r, f).key.value + ")");
      settings.push_back("FSC " + detail::join(fsc, ", "));
    }
    settings.push_back("bound " + std::to_string(h.bound));
    if (h.coexpression_threshold) settings.push_back("coexpression > " + detail::number_text(*h.coexpression_threshold));
    if (h.frozen_threshold) settings.push_back("frozen > " + detail::number_text(*h.frozen_threshold));
    if (!r.cases.empty()) {
      settings.push_back("box n ≤ " + std::to_string(r.cases.front().requested.n) + ", e ≤ " +
                         std::to_string(r.cases.front().requested.e));
    }
    settings.push_back("seed " + std::to_string(h.seed));
    body += "<p class=\"sub\">" + esc(detail::join(settings, " · ")) + "</p>\n";
    if (const auto cmp = compare_cases(r)) {
      body += "<p class=\"sub\"><b>" + esc(comparison_sentence(r, *cmp)) + "</b></p>\n";
    }
    body += "<div class=\"panels\">\n";
    for (const CaseResult& c : r.cases) {
      body += "<div class=\"panel\"><h3>" + esc(c.label) + "</h3>\n<p class=\"note\">" + esc(c.status_text) + "</p>\n";
      std::vector<const FrontierPoint*> points;
      for (const FrontierPoint& p : c.points) points.push_back(&p);
      std::stable_sort(points.begin(), points.end(), [](const FrontierPoint* a, const FrontierPoint* b) {
        const int sa = a->at.n + a->at.e, sb = b->at.n + b->at.e;
        return sa != sb ? sa < sb : a->at.n < b->at.n;
      });
      if (!points.empty()) body += "<div class=\"pos\">\n";
      for (const FrontierPoint* p : points) {
        const auto count = [](int n, std::string_view one, std::string_view many) {
          return std::to_string(n) + " " + std::string(n == 1 ? one : many);   // "1 node", not "1 nodes"
        };
        const std::string text = count(p->at.n, "node", "nodes") + " · " + count(p->at.e, "edge", "edges") + " relaxed";
        const std::string mark = p->certified ? "" : " <span class=\"nc\">not certified</span>";
        const std::string page = page_for(in, r, c.kind, p->at, several);
        if (page.empty()) {
          body += "<span class=\"po\"><span>" + esc(text) + mark + "</span></span>\n";
        } else {
          body += "<a class=\"po\" href=\"" + esc(page) + "\"><span>" + esc(text) + mark + "</span><b>open →</b></a>\n";
        }
      }
      if (!points.empty()) body += "</div>\n";
      std::map<ObservationStatus, std::size_t> counts;
      for (const Observation& o : c.observations) ++counts[o.status];
      std::vector<std::string> parts;
      for (const auto& [status, count] : counts) {
        std::string word(observation_status_name(status));
        std::replace(word.begin(), word.end(), '_', ' ');
        parts.push_back(std::to_string(count) + " " + word);
      }
      if (!parts.empty()) body += "<p class=\"note\">Observations: " + esc(detail::join(parts, ", ")) + ".</p>\n";
      body += "</div>\n";
    }
    body += "</div>\n";
    if (!h.rules.empty()) {
      body += "<ul class=\"rules\">\n";
      for (const std::string& rule : h.rules) body += "<li>" + esc(rule_sentence(rule)) + "</li>\n";
      body += "</ul>\n";
    }
    const std::string json = h.step + "/results.json";
    body += "<p class=\"note\">Results: <a href=\"" + esc(json) + "\">" + esc(json) + "</a></p>\n";
  }
  return "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n<title>" +
         esc(title) + "</title>\n<style>" + std::string(run_page_css) + "</style>\n</head><body><div class=\"wrap\">\n" +
         body + "</div></body></html>\n";
}

// ---------------------------------------------------------------- the step

namespace {

void write_text(core::Context& ctx, const std::string& name, std::string_view text,
                const std::optional<std::filesystem::path>& also = std::nullopt) {
  core::OutputFile file = ctx.files.create(name);
  if (!text.empty()) file.out() << text;
  if (also) file.also_at(*also);
  file.commit();
}

std::optional<std::filesystem::path> find_dot() {
  // Section 11's lookup for Graphviz: FUSION_DOT, then PATH; it is not bundled (07.C5).
  if (const char* env = std::getenv("FUSION_DOT"); env != nullptr && *env != '\0') {
    return core::path_from_utf8(env);
  }
  return core::find_on_path("dot");
}

void write_svg(core::Context& ctx, const std::string& dot_name, const std::string& svg_name) {
  core::OutputFile svg = ctx.files.create(svg_name);
  const std::filesystem::path temp = svg.temp_path();
  const auto dot = find_dot();
  if (!dot) {
    throw core::Failure("results.dot_failed", std::string(ctx.step) + ": static=svg needs Graphviz dot; none found "
                                                                       "(FUSION_DOT unset; not on PATH)");
  }
  // An argument vector and no shell, run in explain/ so that only fixed relative names reach dot
  // (S6.07: psp ran system("dot -Tsvg <dot> -o <svg>") with the names unquoted).
  core::ProcessSpec spec;
  spec.program = *dot;
  spec.args = {"-Tsvg", "-o", core::to_utf8(temp.filename()), detail::base_name(dot_name)};
  spec.working_dir = temp.parent_path();
  spec.timeout = std::chrono::milliseconds(120000);
  const core::ProcessResult result = core::run_process(spec);
  std::error_code ec;
  const bool written = std::filesystem::exists(temp, ec) && std::filesystem::file_size(temp, ec) > 0;
  if (result.exit_code != 0 || !written) {
    std::string what = !result.error.empty()     ? "dot could not start: " + result.error
                       : result.exit_code != 0 ? "dot exited with status " + std::to_string(result.exit_code)
                                               : std::string("dot wrote no image");
    const std::string tail = detail::last_lines(result.stderr_text, 20);
    if (!tail.empty()) what += ": " + tail;
    throw core::Failure("results.dot_failed", std::string(ctx.step) + ": " + what);
  }
  svg.commit();
}

std::string point_list(const CaseResult& c) {
  std::vector<std::string> parts;
  if (selects_requested(c)) {
    for (const RequestedPoint& q : c.requested_points) parts.push_back(point_text(q.at));
  } else {
    for (const FrontierPoint& p : c.points) parts.push_back(point_text(p.at));
  }
  return parts.empty() ? std::string("none") : detail::join(parts, ", ");
}

}  // namespace

void explain(std::span<const SolveResult* const> solves, const ExplainOptions& options, core::Context& ctx) {
  explain(solves, options, SlotNames{}, ctx);
}

void check_endpoints(const SolveResult& r, std::string_view src, std::string_view tgt, std::string_view command) {
  // 07.C12: in the batch census all 51 pex lines passed their batch's connect pair, while a typo would
  // relabel the drawing (S6.13's one-column collapse came from a src that names no drawn node).
  const auto check = [&](std::string_view given, NodeIx end, std::string_view role) {
    const std::string& key = node(r, end).key.value;
    if (given != key) {
      throw core::Failure("results.endpoint_mismatch", std::string(command) + ": " + std::string(role == "source" ? "src " : "tgt ") +
                                                           std::string(given) + " is not the " + std::string(role) +
                                                           " of solve " + detail::base_name(r.header.name) + " (" + key + ")");
    }
  };
  check(src, r.header.source, "source");
  check(tgt, r.header.target, "target");
}

void explain(std::span<const SolveResult* const> solves, const ExplainOptions& options, const SlotNames& slots,
             core::Context& ctx) {
  const std::string step(ctx.step);
  // The case a page's file name gives: the solve's own, or a positional line's slot (07.C2).
  const auto name_case = [&](std::size_t solve, const CaseResult& c) {
    return solve < slots.per_solve.size() ? slots.per_solve[solve] : c.kind;
  };
  // A case name drawn from several solves puts each solve's name in its file names (07.C2).
  std::map<Case, std::size_t> drawn_from;
  const auto wanted = [&](const CaseResult& c) {
    if (options.cases.empty()) return c.status != CaseStatus::not_requested;
    return std::find(options.cases.begin(), options.cases.end(), c.kind) != options.cases.end();
  };
  for (std::size_t si = 0; si < solves.size(); ++si) {
    for (const CaseResult& c : solves[si]->cases) {
      if (wanted(c)) ++drawn_from[name_case(si, c)];
    }
  }
  // An absolute out_prefix is honoured as well (2B.3): its shells and a copy of the library beside it.
  std::optional<std::filesystem::path> abs_dir;
  std::string abs_base;
  if (options.absolute_prefix && options.absolute_prefix->is_absolute()) {
    abs_dir = options.absolute_prefix->parent_path();
    abs_base = core::to_utf8(options.absolute_prefix->filename());
    // OutputFile::also_at copies into an existing directory; the library's beside the shells is ours.
    std::error_code ec;
    std::filesystem::create_directories(*abs_dir / core::path_from_utf8(abs_base + "_lib"), ec);
    if (ec) {
      throw core::Failure("io.write", "cannot create " + core::to_utf8(*abs_dir / core::path_from_utf8(abs_base + "_lib")) +
                                          ": " + ec.message());
    }
  }

  // The library once per step (S6.18): FuSION 1 inlined 688 KB of it into every page
  // (plot_explanation.py:198-200).
  for (const PageAsset& asset : page_assets()) {
    write_text(ctx, "lib/" + std::string(asset.name), asset.bytes);
    if (abs_dir) {
      write_text(ctx, abs_base + "_lib/" + std::string(asset.name), asset.bytes,
                 *abs_dir / core::path_from_utf8(abs_base + "_lib") / core::path_from_utf8(asset.name));
    }
  }

  std::size_t pages = 0;
  for (std::size_t si = 0; si < solves.size(); ++si) {
    const SolveResult& r = *solves[si];
    for (const CaseResult& c : r.cases) {
      if (!wanted(c)) continue;
      const Case named = name_case(si, c);
      if (c.points.empty() && c.requested_points.empty()) {
        // S6.21: a case without points is a result, reported as such, not a failure (FuSION 1 exited 1
        // when pex drew nothing, plot_explanation.py:720-724).
        ctx.log.info(step + ": " + c.label + ": " + c.status_text + "; nothing to draw");
        continue;
      }
      const auto indices = select_points(c, options.points);
      if (options.points.kind == PointSelector::Kind::one && indices.empty()) {
        throw core::Failure("results.no_such_point", step + ": " + c.label + " has no point " +
                                                         point_text(options.points.at) + "; its points are " +
                                                         point_list(c));
      }
      const bool requested = selects_requested(c);
      const std::string solve_name = drawn_from[named] > 1 ? r.header.name : std::string();
      for (const std::size_t index : indices) {
        const PointView view = requested ? build_view(r, c, c.requested_points[index]) : build_view(r, c, c.points[index]);
        const std::string stem = page_stem(options.prefix, solve_name, named, view.at);
        const std::string json = view_json(view, r, options.paths, options.witness, options.edge_table);
        write_text(ctx, "explain/" + stem + ".json", json);
        write_text(ctx, "explain/" + stem + ".html", page_html(json, "../lib/", view.title, stem + ".json"));
        if (abs_dir) {
          const std::string abs_stem = page_stem(abs_base, solve_name, named, view.at);
          write_text(ctx, abs_stem + ".html", page_html(json, abs_base + "_lib/", view.title, stem + ".json"),
                     *abs_dir / core::path_from_utf8(abs_stem + ".html"));
        }
        ++pages;
        const std::string page = step + "/explain/" + stem + ".html";
        const std::string where = c.label + " " + point_text(view.at);
        if (view.witnesses.empty()) {
          ctx.log.info(step + ": " + where + " -> " + page + " (no explanation: " + view.missing_reason + ")");
          continue;
        }
        const WitnessView& shown = view.witnesses[first_witness(options.witness, view.witnesses.size())];
        const auto kept = kept_by(r, shown, options.paths);
        const auto drawn = drawn_nodes(r, shown, options.paths, kept);
        const auto edges = static_cast<std::size_t>(std::count(kept.begin(), kept.end(), true));
        if (options.edge_table) write_text(ctx, "explain/" + stem + "_edges.tsv", edge_table_tsv(shown, r, options.paths));
        if (options.static_svg) {
          write_text(ctx, "explain/" + stem + ".dot", dot_source(shown, r, options.paths, stem));
          write_svg(ctx, "explain/" + stem + ".dot", "explain/" + stem + ".svg");
        }
        // S6.09, S6.20: counts of what is drawn, under the page's initial path filter.
        std::string line = step + ": " + where + " -> " + page + " (" + std::to_string(drawn.size()) + " nodes, " +
                           std::to_string(edges) + " edges";
        if (!shown.parked.empty()) {
          line += "; " + std::to_string(shown.parked.size()) +
                  (shown.parked.size() == 1 ? " relaxed node not drawn" : " relaxed nodes not drawn");
        }
        if (options.paths.mode != PathMode::subgraph && edges == 0) line += "; no path in this mode";
        ctx.log.info(line + ")");
      }
    }
  }
  ctx.log.info(step + ": wrote " + std::to_string(pages) + (pages == 1 ? " page" : " pages"));
  if (options.site) publish_site(solves, *options.site, ctx);
}

}  // namespace fusion::results
