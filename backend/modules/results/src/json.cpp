// results/src/json.cpp: results.json, written in a fixed member order with arrays sorted by key, and its
// one reader (section 07, "results.json and the solve's other outputs"; 07.C1, S6.02, E1, 2A.2). FuSION 1
// had two readers of one text dump, psp's (GraphManagerNew.cc:8852-8927) and pex's
// (plot_explanation.py:72-129); S6.02 asks for one documented reader.
#include "fusion/results/json.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include "text.hpp"

namespace fusion::results {

namespace {

using Json = core::Json;

Json point_json(Point p) {
  Json j = Json::object();
  j["n"] = p.n;
  j["e"] = p.e;
  return j;
}

class Writer {
 public:
  explicit Writer(const SolveResult& r) : r_(r) {}

  const std::string& node_key(NodeIx i) const { return r_.tables.nodes.at(i).key.value; }
  std::string edge_key(EdgeIx i) const { return r_.tables.edges.at(i).key.text(); }

  Json node_keys(const std::vector<NodeIx>& list) const {
    Json a = Json::array();
    for (const NodeIx i : list) a.push_back(node_key(i));
    return a;
  }

  Json witness(const Witness& w) const {
    Json j = Json::object();
    j["counts"] = point_json(w.counts());
    j["active"] = node_keys(w.active);
    j["inactive"] = node_keys(w.inactive);
    // Relaxed nodes by key with their presence, relaxed edges by key with their selection (S6.15).
    std::vector<std::pair<NodeIx, bool>> nodes;
    for (const NodeIx i : w.relaxed_present) nodes.emplace_back(i, true);
    for (const NodeIx i : w.relaxed_absent) nodes.emplace_back(i, false);
    std::sort(nodes.begin(), nodes.end());
    Json relaxed_nodes = Json::array();
    for (const auto& [i, present] : nodes) {
      Json n = Json::object();
      n["key"] = node_key(i);
      n["present"] = present;
      relaxed_nodes.push_back(std::move(n));
    }
    j["relaxed_nodes"] = std::move(relaxed_nodes);
    Json selected = Json::array();
    for (const EdgeIx i : w.selected) selected.push_back(edge_key(i));
    j["selected"] = std::move(selected);
    std::vector<std::pair<EdgeIx, bool>> edges;
    for (const EdgeIx i : w.relaxed_selected) edges.emplace_back(i, true);
    for (const EdgeIx i : w.relaxed_unselected) edges.emplace_back(i, false);
    std::sort(edges.begin(), edges.end());
    Json relaxed_edges = Json::array();
    for (const auto& [i, sel] : edges) {
      Json e = Json::object();
      e["key"] = edge_key(i);
      e["selected"] = sel;
      relaxed_edges.push_back(std::move(e));
    }
    j["relaxed_edges"] = std::move(relaxed_edges);
    Json route = Json::object();
    route["exists"] = w.route.exists;
    route["fsc_on_route"] = node_keys(w.route.fsc_on_route);
    route["nodes"] = node_keys(w.route.nodes);
    j["route"] = std::move(route);
    return j;
  }

  Json witnesses(const std::vector<Witness>& list) const {
    Json a = Json::array();
    for (const Witness& w : list) a.push_back(witness(w));
    return a;
  }

  Json case_json(const CaseResult& c) const {
    Json j = Json::object();
    j["case"] = case_name(c.kind);
    j["label"] = c.label;
    j["status"] = status_name(c.status);
    j["status_text"] = c.status_text;
    j["message"] = c.message;
    Json box = Json::object();
    box["requested"] = point_json(c.requested);
    box["searched"] = point_json(c.searched);
    j["box"] = std::move(box);
    Json checks = Json::object();
    checks["sat"] = c.checks.sat;
    checks["unsat"] = c.checks.unsat;
    checks["unknown"] = c.checks.unknown;
    j["checks"] = std::move(checks);
    Json undecided = Json::array();
    for (const UndecidedCell& u : c.undecided) {
      Json cell = point_json(u.at);
      cell["label"] = u.label;
      undecided.push_back(std::move(cell));
    }
    j["undecided"] = std::move(undecided);
    Json observations = Json::array();
    for (const Observation& o : c.observations) {
      Json ob = Json::object();
      ob["gene"] = o.gene;
      ob["node"] = o.node ? Json(node_key(*o.node)) : Json(nullptr);
      ob["observed"] = observed_name(o.observed);
      ob["status"] = observation_status_name(o.status);
      observations.push_back(std::move(ob));
    }
    j["observations"] = std::move(observations);
    Json points = Json::array();
    for (const FrontierPoint& p : c.points) {
      Json pj = point_json(p.at);
      pj["certified"] = p.certified;
      Json below = Json::array();
      for (const Point b : p.undecided_below) below.push_back(point_json(b));
      pj["undecided_below"] = std::move(below);
      pj["enumeration"] = enumeration_name(p.enumeration);
      pj["witnesses"] = witnesses(p.witnesses);
      points.push_back(std::move(pj));
    }
    j["points"] = std::move(points);
    Json requested = Json::array();
    for (const RequestedPoint& q : c.requested_points) {
      Json qj = point_json(q.at);
      qj["answer"] = outcome_name(q.answer);
      qj["label"] = q.label;
      qj["witnesses"] = witnesses(q.witnesses);
      requested.push_back(std::move(qj));
    }
    j["requested_points"] = std::move(requested);
    return j;
  }

  Json document(std::span<const Issue> issues, const std::optional<Comparison>& cmp) const {
    const Header& h = r_.header;
    Json j = Json::object();
    j["format"] = results_format;
    j["version"] = results_version;
    Json solve = Json::object();
    solve["name"] = h.name;
    solve["step"] = h.step;
    solve["source"] = node_key(h.source);
    solve["target"] = node_key(h.target);
    solve["fsc"] = node_keys(h.fsc);
    solve["bound"] = h.bound;
    solve["seed"] = h.seed;
    Json constraints = Json::object();       // only the families the solve used (S5.03)
    if (h.coexpression_threshold) constraints["coexpression"] = Json{{"threshold", *h.coexpression_threshold}};
    if (h.frozen_threshold) constraints["frozen"] = Json{{"threshold", *h.frozen_threshold}};
    solve["constraints"] = std::move(constraints);
    solve["rules"] = h.rules;
    j["solve"] = std::move(solve);
    Json corridor = Json::object();
    corridor["nodes"] = h.corridor.nodes;
    corridor["edges"] = h.corridor.edges;
    corridor["bound"] = h.corridor.bound;
    corridor["artefact"] = h.corridor.artefact;
    j["corridor"] = std::move(corridor);
    Json pathways = Json::object();
    for (const auto& [id, name] : r_.tables.pathway_names) pathways[id] = name;   // byte order
    j["pathways"] = std::move(pathways);
    Json nodes = Json::array();
    for (const NodeRecord& n : r_.tables.nodes) {
      Json nj = Json::object();
      nj["key"] = n.key.value;
      nj["label"] = n.label;
      nj["kind"] = kind_name(n.kind);
      nj["observed"] = observed_name(n.observed);
      Json members = Json::array();
      for (const Member& m : n.members) {
        Json mj = Json::object();
        mj["id"] = m.id;
        mj["symbol"] = m.symbol;
        mj["lfc"] = m.lfc ? Json(*m.lfc) : Json(nullptr);
        mj["observed"] = observed_name(m.observed);
        members.push_back(std::move(mj));
      }
      nj["members"] = std::move(members);
      Json distance = Json::object();
      distance["from_source"] = n.from_source ? Json(*n.from_source) : Json(nullptr);
      distance["to_target"] = n.to_target ? Json(*n.to_target) : Json(nullptr);
      nj["distance"] = std::move(distance);
      nodes.push_back(std::move(nj));
    }
    j["nodes"] = std::move(nodes);
    Json edges = Json::array();
    for (const EdgeRecord& e : r_.tables.edges) {
      // The key's four fields as well as its text (02.C11).
      Json ej = Json::object();
      ej["key"] = e.key.text();
      ej["source"] = e.key.source.value;
      ej["target"] = e.key.target.value;
      ej["type"] = e.type;
      ej["subtypes"] = e.subtypes;
      ej["effect"] = effect_name(e.effect);
      ej["pathways"] = e.pathways;
      ej["frozen"] = e.frozen;
      edges.push_back(std::move(ej));
    }
    j["edges"] = std::move(edges);
    Json cases = Json::array();
    for (const CaseResult& c : r_.cases) cases.push_back(case_json(c));
    j["cases"] = std::move(cases);
    if (cmp) {
      Json cj = Json::object();
      cj["verdict"] = verdict_name(cmp->verdict);
      cj["delta_n_at_e0"] = cmp->delta_n_at_e0 ? Json(*cmp->delta_n_at_e0) : Json(nullptr);
      cj["certified"] = cmp->certified;
      j["comparison"] = std::move(cj);
    } else {
      j["comparison"] = nullptr;
    }
    Json list = Json::array();
    for (const Issue& i : issues) {
      Json ij = Json::object();
      ij["kind"] = issue_kind_name(i.kind);
      ij["code"] = i.code;
      ij["message"] = i.message;
      list.push_back(std::move(ij));
    }
    j["issues"] = std::move(list);
    return j;
  }

 private:
  const SolveResult& r_;
};

// ---------------------------------------------------------------- the reader

[[noreturn]] void invalid(const std::string& what) {
  throw core::Failure("artefact.invalid", "results.json: " + what);
}

template <class T>
T word(std::optional<T> parsed, std::string_view field, const std::string& text) {
  if (!parsed) invalid("unknown " + std::string(field) + " '" + text + "'");
  return *parsed;
}

class Reader {
 public:
  explicit Reader(const Json& j) : j_(j) {}

  ResultsFile read() {
    ResultsFile file;
    SolveResult& r = file.result;
    for (const Json& n : j_.at("nodes")) {
      NodeRecord rec;
      rec.key.value = n.at("key").get<std::string>();
      rec.label = n.at("label").get<std::string>();
      rec.kind = word(parse_kind(n.at("kind").get<std::string>()), "kind", n.at("kind").get<std::string>());
      rec.observed = observed(n.at("observed"));
      for (const Json& m : n.at("members")) {
        Member mem;
        mem.id = m.at("id").get<std::string>();
        mem.symbol = m.at("symbol").get<std::string>();
        if (!m.at("lfc").is_null()) mem.lfc = m.at("lfc").get<double>();
        mem.observed = observed(m.at("observed"));
        rec.members.push_back(std::move(mem));
      }
      const Json& d = n.at("distance");
      if (!d.at("from_source").is_null()) rec.from_source = d.at("from_source").get<std::int32_t>();
      if (!d.at("to_target").is_null()) rec.to_target = d.at("to_target").get<std::int32_t>();
      if (!node_index_.emplace(rec.key.value, static_cast<NodeIx>(r.tables.nodes.size())).second) {
        invalid("node " + rec.key.value + " is listed twice");
      }
      r.tables.nodes.push_back(std::move(rec));
    }
    for (const Json& e : j_.at("edges")) {
      EdgeRecord rec;
      rec.key.source.value = e.at("source").get<std::string>();
      rec.key.target.value = e.at("target").get<std::string>();
      rec.key.type = e.at("type").get<std::string>();
      rec.key.subtypes = e.at("subtypes").get<std::vector<std::string>>();
      rec.type = rec.key.type;
      rec.subtypes = rec.key.subtypes;
      rec.source = node(rec.key.source.value);
      rec.target = node(rec.key.target.value);
      rec.effect = word(parse_effect(e.at("effect").get<std::string>()), "effect", e.at("effect").get<std::string>());
      rec.pathways = e.at("pathways").get<std::vector<std::string>>();
      rec.frozen = e.at("frozen").get<bool>();
      const std::string text = e.at("key").get<std::string>();
      if (!edge_index_.emplace(text, static_cast<EdgeIx>(r.tables.edges.size())).second) {
        invalid("edge " + text + " is listed twice");
      }
      r.tables.edges.push_back(std::move(rec));
    }
    for (const auto& [id, name] : j_.at("pathways").items()) r.tables.pathway_names.emplace(id, name.get<std::string>());

    const Json& s = j_.at("solve");
    Header& h = r.header;
    h.name = s.at("name").get<std::string>();
    h.step = s.at("step").get<std::string>();
    h.source = node(s.at("source").get<std::string>());
    h.target = node(s.at("target").get<std::string>());
    h.fsc = nodes(s.at("fsc"));
    h.bound = s.at("bound").get<std::int32_t>();
    h.seed = s.at("seed").get<std::uint64_t>();
    const Json& constraints = s.at("constraints");
    if (constraints.contains("coexpression")) {
      h.coexpression_threshold = constraints.at("coexpression").at("threshold").get<double>();
    }
    if (constraints.contains("frozen")) h.frozen_threshold = constraints.at("frozen").at("threshold").get<double>();
    h.rules = s.at("rules").get<std::vector<std::string>>();
    const Json& c = j_.at("corridor");
    h.corridor.nodes = c.at("nodes").get<std::uint32_t>();
    h.corridor.edges = c.at("edges").get<std::uint32_t>();
    h.corridor.bound = c.at("bound").get<std::int32_t>();
    h.corridor.artefact = c.at("artefact").get<std::string>();

    for (const Json& cj : j_.at("cases")) r.cases.push_back(case_result(cj));

    const Json& cmp = j_.at("comparison");
    if (!cmp.is_null()) {
      Comparison comparison;
      const std::string verdict = cmp.at("verdict").get<std::string>();
      comparison.verdict = word(parse_verdict(verdict), "verdict", verdict);
      if (!cmp.at("delta_n_at_e0").is_null()) comparison.delta_n_at_e0 = cmp.at("delta_n_at_e0").get<std::int32_t>();
      comparison.certified = cmp.at("certified").get<bool>();
      file.comparison = comparison;
    }
    for (const Json& ij : j_.at("issues")) {
      Issue issue;
      const std::string kind = ij.at("kind").get<std::string>();
      if (kind == "internal_error") issue.kind = Issue::Kind::internal_error;
      else if (kind == "warning") issue.kind = Issue::Kind::warning;
      else invalid("unknown issue kind '" + kind + "'");
      issue.code = ij.at("code").get<std::string>();
      issue.message = ij.at("message").get<std::string>();
      file.issues.push_back(std::move(issue));
    }
    return file;
  }

 private:
  static Observed observed(const Json& j) {
    const std::string text = j.get<std::string>();
    return word(parse_observed(text), "observed class", text);
  }
  static Point point(const Json& j) {
    Point p;
    p.n = j.at("n").get<decltype(p.n)>();
    p.e = j.at("e").get<decltype(p.e)>();
    return p;
  }
  NodeIx node(const std::string& key) const {
    const auto it = node_index_.find(key);
    if (it == node_index_.end()) invalid("node " + key + " is not in the node table");
    return it->second;
  }
  EdgeIx edge(const std::string& key) const {
    const auto it = edge_index_.find(key);
    if (it == edge_index_.end()) invalid("edge " + key + " is not in the edge table");
    return it->second;
  }
  std::vector<NodeIx> nodes(const Json& list) const {
    std::vector<NodeIx> out;
    for (const Json& k : list) out.push_back(node(k.get<std::string>()));
    return out;
  }

  Witness witness(const Json& j) const {
    Witness w;
    w.active = nodes(j.at("active"));
    w.inactive = nodes(j.at("inactive"));
    for (const Json& n : j.at("relaxed_nodes")) {
      (n.at("present").get<bool>() ? w.relaxed_present : w.relaxed_absent).push_back(node(n.at("key").get<std::string>()));
    }
    for (const Json& k : j.at("selected")) w.selected.push_back(edge(k.get<std::string>()));
    for (const Json& e : j.at("relaxed_edges")) {
      (e.at("selected").get<bool>() ? w.relaxed_selected : w.relaxed_unselected)
          .push_back(edge(e.at("key").get<std::string>()));
    }
    const Json& route = j.at("route");
    w.route.exists = route.at("exists").get<bool>();
    w.route.fsc_on_route = nodes(route.at("fsc_on_route"));
    w.route.nodes = nodes(route.at("nodes"));
    const Point counts = point(j.at("counts"));
    if (!(counts == w.counts())) invalid("a witness's counts " + point_text(counts) + " do not match its lists");
    return w;
  }

  std::vector<Witness> witnesses(const Json& list) const {
    std::vector<Witness> out;
    for (const Json& w : list) out.push_back(witness(w));
    return out;
  }

  CaseResult case_result(const Json& j) const {
    CaseResult c;
    const std::string kind = j.at("case").get<std::string>();
    c.kind = word(parse_case(kind), "case", kind);
    c.label = j.at("label").get<std::string>();
    const std::string status = j.at("status").get<std::string>();
    c.status = word(parse_status(status), "status", status);
    c.status_text = j.at("status_text").get<std::string>();
    c.message = j.at("message").get<std::string>();
    c.requested = point(j.at("box").at("requested"));
    c.searched = point(j.at("box").at("searched"));
    const Json& checks = j.at("checks");
    c.checks.sat = checks.at("sat").get<std::uint32_t>();
    c.checks.unsat = checks.at("unsat").get<std::uint32_t>();
    c.checks.unknown = checks.at("unknown").get<std::uint32_t>();
    for (const Json& u : j.at("undecided")) c.undecided.push_back({point(u), u.at("label").get<std::string>()});
    for (const Json& o : j.at("observations")) {
      Observation ob;
      ob.gene = o.at("gene").get<std::string>();
      if (!o.at("node").is_null()) ob.node = node(o.at("node").get<std::string>());
      ob.observed = observed(o.at("observed"));
      const std::string st = o.at("status").get<std::string>();
      ob.status = word(parse_observation_status(st), "observation status", st);
      c.observations.push_back(std::move(ob));
    }
    for (const Json& p : j.at("points")) {
      FrontierPoint fp;
      fp.at = point(p);
      fp.certified = p.at("certified").get<bool>();
      for (const Json& b : p.at("undecided_below")) fp.undecided_below.push_back(point(b));
      const std::string en = p.at("enumeration").get<std::string>();
      fp.enumeration = word(parse_enumeration(en), "enumeration", en);
      fp.witnesses = witnesses(p.at("witnesses"));
      c.points.push_back(std::move(fp));
    }
    for (const Json& q : j.at("requested_points")) {
      RequestedPoint rp;
      rp.at = point(q);
      const std::string answer = q.at("answer").get<std::string>();
      rp.answer = word(parse_outcome(answer), "answer", answer);
      rp.label = q.at("label").get<std::string>();
      rp.witnesses = witnesses(q.at("witnesses"));
      c.requested_points.push_back(std::move(rp));
    }
    return c;
  }

  const Json& j_;
  std::map<std::string, NodeIx> node_index_;
  std::map<std::string, EdgeIx> edge_index_;
};

}  // namespace

std::string to_json(const SolveResult& r, std::span<const Issue> issues, const std::optional<Comparison>& cmp) {
  // One-space indentation, so runs diff line by line; numbers in nlohmann/json's shortest round-trip
  // form; no time, path or host (E1, 09.C16).
  return detail::dump(Writer(r).document(issues, cmp), 1) + "\n";
}

ResultsFile read_results(std::string_view text) {
  const Json j = Json::parse(text.begin(), text.end(), nullptr, false);
  if (j.is_discarded() || !j.is_object()) invalid("not a JSON object");
  const auto format = j.find("format");
  if (format == j.end() || !format->is_string() || format->get<std::string>() != results_format) {
    invalid("not a fusion.results file");
  }
  const auto version = j.find("version");
  if (version == j.end() || !version->is_number_integer()) invalid("no format version");
  const auto v = version->get<long long>();
  if (v > results_version) {
    throw core::Failure("artefact.newer_format", "results.json: format fusion.results version " + std::to_string(v) +
                                                     " is newer than this FuSION reads (" +
                                                     std::to_string(results_version) + ")");
  }
  if (v < 1) invalid("format version " + std::to_string(v) + " is not known");
  try {
    return Reader(j).read();
  } catch (const Json::exception& e) {
    invalid(std::string("malformed: ") + e.what());
  }
}

SolveResult from_json(std::string_view text) { return read_results(text).result; }

}  // namespace fusion::results
