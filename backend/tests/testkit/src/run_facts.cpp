// A 2.0 run directory as kit types (run_facts.hpp).
#include "fusion/testkit/run_facts.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include <pugixml.hpp>

#include "fusion/core/core.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "util.hpp"

namespace fusion::testkit {

namespace fs = std::filesystem;
using core::Json;

namespace {

Json read_json(const fs::path& p) {
  try {
    return Json::parse(detail::slurp(p));
  } catch (const Json::exception& e) {
    throw core::Failure("testkit.run", core::to_utf8(p) + ": " + e.what());
  }
}

// One of the run's saved networks (02's fusion-network format): each entry's key, members, type and
// components, and the identities of its tagged edges.
struct SavedNetwork {
  struct Node { std::vector<std::string> members; std::string type; std::vector<std::string> components; };
  std::map<std::string, Node> nodes;                                      // by network key
  std::set<std::tuple<std::string, std::string, std::string, std::vector<std::string>>> tagged;
};

std::vector<SavedNetwork> saved_networks(const fs::path& run_dir) {
  std::vector<SavedNetwork> out;
  std::error_code ec;
  std::vector<fs::path> files;
  for (const auto& step : fs::directory_iterator(run_dir, ec)) {
    if (!step.is_directory(ec)) continue;
    for (const auto& f : fs::directory_iterator(step.path(), ec))
      if (f.is_regular_file(ec) && f.path().extension() == ".xml") files.push_back(f.path());
  }
  std::sort(files.begin(), files.end());
  for (const fs::path& file : files) {
    pugi::xml_document doc;
    if (!doc.load_file(file.c_str())) continue;
    const pugi::xml_node root = doc.child("pathway");
    if (std::string(root.attribute("format").value()) != "fusion-network") continue;
    SavedNetwork net;
    std::map<std::string, std::string> key_of_entry;
    for (pugi::xml_node e : root.children("entry")) {
      std::string key = e.attribute("key").value();
      if (key.empty()) key = e.attribute("name").value();
      key_of_entry[e.attribute("id").value()] = key;
    }
    for (pugi::xml_node e : root.children("entry")) {
      const std::string key = key_of_entry[e.attribute("id").value()];
      SavedNetwork::Node node;
      node.type = e.attribute("type").value();
      if (node.type != "group")
        for (const std::string& id : detail::words(e.attribute("name").value())) node.members.push_back(detail::strip_colons(id));
      for (pugi::xml_node c : e.children("component")) node.components.push_back(key_of_entry[c.attribute("id").value()]);
      net.nodes[key] = std::move(node);
    }
    for (pugi::xml_node r : root.children("relation")) {
      std::vector<std::string> subtypes;
      bool tag = false;
      for (pugi::xml_node s : r.children("subtype")) {
        const std::string name = s.attribute("name").value();
        if (name == "dummy_u_to_d") tag = true;
        else subtypes.push_back(name);
      }
      if (tag)
        net.tagged.insert({key_of_entry[r.attribute("entry1").value()], key_of_entry[r.attribute("entry2").value()],
                           r.attribute("type").value(), subtypes});
    }
    out.push_back(std::move(net));
  }
  return out;
}

// Canonical keys for a solve's node keys: from the network that holds them, else from results.json.
class Keys {
 public:
  Keys(const SavedNetwork* net, std::map<std::string, std::vector<std::string>> listed)
      : net_(net), listed_(std::move(listed)) {}

  const CanonNode& node(const std::string& key) {
    if (const auto it = done_.find(key); it != done_.end()) return it->second;
    CanonNode n;
    const SavedNetwork::Node* saved = nullptr;
    if (net_)
      if (const auto it = net_->nodes.find(key); it != net_->nodes.end()) saved = &it->second;
    if (saved && saved->type == "group" && !saved->components.empty()) {
      std::vector<std::string> parts, ids;
      for (const std::string& c : saved->components) {
        const CanonNode& part = node(c);
        parts.push_back(part.key);
        ids.insert(ids.end(), part.members.begin(), part.members.end());
      }
      detail::sort_unique(ids);
      n = {complex_key(parts), ids, "group"};
    } else if (saved && !saved->members.empty()) {
      n = {node_key(saved->members), saved->members, saved->type};
    } else if (const auto it = listed_.find(key); it != listed_.end() && !it->second.empty()) {
      n = {node_key(it->second), it->second, "gene"};
    } else {
      n = {key, {key}, "gene"};
    }
    detail::sort_unique(n.members);
    return done_.emplace(key, std::move(n)).first->second;
  }
  bool tagged(const std::string& s, const std::string& t, const std::string& type, const std::vector<std::string>& sub) const {
    return net_ && net_->tagged.contains({s, t, type, sub});
  }

 private:
  const SavedNetwork* net_;
  std::map<std::string, std::vector<std::string>> listed_;
  std::map<std::string, CanonNode> done_;
};

std::vector<std::string> subtypes_of(const Json& list) {
  std::vector<std::string> out;
  for (const auto& s : list) out.push_back(s.get<std::string>());
  return out;
}

// "<src>|<tgt>|<type>|a,b": 02's edge key text.
std::tuple<std::string, std::string, std::string, std::vector<std::string>> parse_edge_key(const std::string& text) {
  const std::vector<std::string> f = detail::split(text, '|');
  if (f.size() < 4) throw core::Failure("testkit.run", "not an edge key: " + text);
  std::vector<std::string> subtypes;
  if (!f[3].empty()) subtypes = detail::split(f[3], ',');
  return {f[0], f[1], f[2], subtypes};
}

// An edge list line (S5.06): "<source> <target> <subtypes>", subtypes joined by ',' or by '_'; '-' is none.
std::vector<EdgeSpec> read_edge_lines(const fs::path& file) {
  std::vector<EdgeSpec> out;
  for (const std::string& line : detail::lines_of(detail::slurp(file))) {
    const std::vector<std::string> w = detail::words(line);
    if (w.size() < 2) continue;
    EdgeSpec e{w[0], w[1], {}};
    if (w.size() >= 3 && w[2] != "-") {
      const char sep = w[2].find(',') != std::string::npos ? ',' : '_';
      const std::vector<std::string> parts = detail::split(w[2], sep);
      for (std::size_t k = 0; k < parts.size(); ++k) {
        if (sep == '_' && parts[k] == "dummy" && k + 3 < parts.size()) {
          k += 3;
          continue;
        }
        if (!parts[k].empty()) e.subtypes.push_back(parts[k]);
      }
    }
    out.push_back(std::move(e));
  }
  return out;
}

}  // namespace

RunFacts read_run(const fs::path& run_dir) {
  const Json run = read_json(run_dir / "run.json");
  RunFacts facts;
  facts.status = run.value("status", "");
  facts.exit = run.contains("exit") && run["exit"].is_number_integer() ? run["exit"].get<int>() : 0;
  const std::vector<SavedNetwork> networks = saved_networks(run_dir);
  if (!run.contains("steps")) return facts;
  for (const Json& step : run["steps"]) {
    const std::string id = step.value("id", "");
    const fs::path results_file = run_dir / core::path_from_utf8(id) / "results.json";
    std::error_code ec;
    if (id.empty() || !fs::exists(results_file, ec)) continue;
    const Json r = read_json(results_file);
    SolveFacts solve;
    solve.step = id;

    // The step's settings and its copied inputs, by argument.
    std::map<std::string, Json> settings;
    bool positional = false;
    if (step.contains("settings"))
      for (const Json& s : step["settings"]) {
        settings[s.value("key", "")] = s.contains("value") ? s["value"] : Json();
        if (s.value("source", "") == "positional") positional = true;
      }
    std::map<std::string, fs::path> copies;
    if (run.contains("inputs"))
      for (const Json& in : run["inputs"])
        if (in.value("step", "") == id && in.contains("copy") && in["copy"].is_string())
          copies.emplace(in.value("argument", ""), run_dir / core::path_from_utf8(in["copy"].get<std::string>()));
    const auto copy_of = [&](const std::string& argument) -> std::optional<fs::path> {
      if (const auto it = copies.find(argument); it != copies.end()) return it->second;
      return std::nullopt;
    };

    // The corridor: corridor.json's nodes and edges, keyed by the network that holds its nodes.
    std::map<std::string, std::vector<std::string>> listed;
    for (const Json& n : r.value("nodes", Json::array())) {
      std::vector<std::string> ids;
      for (const Json& m : n.value("members", Json::array())) ids.push_back(m.value("id", ""));
      listed[n.value("key", "")] = ids;
    }
    Json corridor_json;
    if (r.contains("corridor") && r["corridor"].contains("artefact")) {
      const fs::path artefact = run_dir / core::path_from_utf8(r["corridor"]["artefact"].get<std::string>());
      if (fs::exists(artefact, ec)) corridor_json = read_json(artefact);
    }
    std::vector<std::string> node_keys;
    if (corridor_json.contains("nodes"))
      for (const Json& n : corridor_json["nodes"]) node_keys.push_back(n.value("key", ""));
    else
      for (const auto& [key, ids] : listed) node_keys.push_back(key);
    const SavedNetwork* net = nullptr;
    for (const SavedNetwork& candidate : networks)
      if (std::all_of(node_keys.begin(), node_keys.end(), [&](const std::string& k) { return candidate.nodes.contains(k); })) {
        net = &candidate;
        break;
      }
    Keys keys(net, listed);
    for (const std::string& k : node_keys) solve.corridor.graph.nodes.push_back(keys.node(k));
    const auto add_edge = [&](const std::string& s, const std::string& t, const std::string& type,
                              const std::vector<std::string>& subtypes, const Json& pathways, double score) {
      CanonEdge e;
      e.key = {keys.node(s).key, keys.node(t).key, type, subtypes};
      e.dummy_tag = keys.tagged(s, t, type, subtypes);
      for (const Json& p : pathways) e.pathways.push_back(p.get<std::string>());
      e.score = score;
      solve.corridor.graph.edges.push_back(std::move(e));
    };
    if (corridor_json.contains("edges")) {
      for (const Json& e : corridor_json["edges"])
        add_edge(e.value("source", ""), e.value("target", ""), e.value("type", ""), subtypes_of(e.value("subtypes", Json::array())),
                 e.value("pathways", Json::array()), e.value("score", 0.0));
    } else {
      for (const Json& e : r.value("edges", Json::array()))
        add_edge(e.value("source", ""), e.value("target", ""), e.value("type", ""), subtypes_of(e.value("subtypes", Json::array())),
                 e.value("pathways", Json::array()), 0.0);
    }
    normalise(solve.corridor.graph);
    const Json& about = r.value("solve", Json::object());
    solve.corridor.sources = {keys.node(about.value("source", "")).key};
    solve.corridor.targets = {keys.node(about.value("target", "")).key};
    solve.corridor.bound = about.value("bound", 0);

    // The case spec shared by every case: lists, families and the form.
    CaseSpec common;
    common.named = !positional;
    for (const Json& f : about.value("fsc", Json::array())) common.fsc.push_back(f.get<std::string>());
    const auto ids_of = [&](const std::string& argument, std::vector<std::string>& into) {
      if (auto p = copy_of(argument)) into = f1::read_ids(*p);
    };
    ids_of("essential_nodes", common.essential);
    ids_of("avoid_nodes", common.avoid);
    ids_of("active_nodes", common.active);
    ids_of("inactive_nodes", common.inactive);
    ids_of("nonrelaxed_nodes", common.nonrelaxed);
    ids_of("confirmed_up", common.confirmed_up);
    ids_of("confirmed_down", common.confirmed_down);
    if (auto p = copy_of("essential_edges")) common.essential_edges = read_edge_lines(*p);
    if (auto p = copy_of("avoid_edges")) common.avoid_edges = read_edge_lines(*p);
    if (auto p = copy_of("nonrelaxed_edges")) common.nonrelaxed_edges = read_edge_lines(*p);
    const Json& constraints = about.value("constraints", Json::object());
    if (constraints.contains("frozen")) common.frozen_threshold = constraints["frozen"].value("threshold", 0.0);
    if (constraints.contains("coexpression"))
      if (auto p = copy_of("coexpression_csv"))
        common.coexpressed = f1::read_coexpression(*p, constraints["coexpression"].value("threshold", 0.0));

    for (const Json& c : r.value("cases", Json::array())) {
      SolveFacts::Case kase;
      kase.name = c.value("case", "");
      kase.status = c.value("status", "");
      kase.spec = common;
      kase.spec.solve_case = kase.name == "in" ? CaseSpec::Case::in : kase.name == "out" ? CaseSpec::Case::out : CaseSpec::Case::free;
      for (const Json& o : c.value("observations", Json::array())) {
        const std::string observed = o.value("observed", "");
        if (observed == "up") kase.spec.up.push_back(o.value("gene", ""));
        else if (observed == "down") kase.spec.down.push_back(o.value("gene", ""));
      }
      for (const Json& p : c.value("points", Json::array())) {
        kase.frontier.points.push_back({p.value("n", 0), p.value("e", 0)});
        kase.frontier.certified.push_back(p.value("certified", false));
        Assignment a;
        const Json& ws = p.value("witnesses", Json::array());
        if (!ws.empty()) {
          const Json& w = ws[0];
          for (const Json& k : w.value("active", Json::array())) a.present.emplace_back(keys.node(k.get<std::string>()).key, true);
          for (const Json& k : w.value("inactive", Json::array())) a.present.emplace_back(keys.node(k.get<std::string>()).key, false);
          for (const Json& k : w.value("selected", Json::array())) {
            auto [s, t, type, subtypes] = parse_edge_key(k.get<std::string>());
            std::sort(subtypes.begin(), subtypes.end());
            a.selected.push_back({keys.node(s).key, keys.node(t).key, type, subtypes});
          }
        }
        kase.witnesses.push_back(std::move(a));
      }
      solve.cases.push_back(std::move(kase));
    }
    facts.solves.push_back(std::move(solve));
  }
  return facts;
}

}  // namespace fusion::testkit
