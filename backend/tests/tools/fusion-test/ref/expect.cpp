// The expected differences (expect.hpp): their file, the closed list of predicates over a case's signals,
// and the predictions' shapes. The predictions are checked where their facts are, in rules.cpp.
#include "ref/expect.hpp"

#include <algorithm>
#include <functional>
#include <map>

#include "files.hpp"

namespace fusion::reftest {

namespace fs = std::filesystem;
using core::Json;

namespace {

using Solve = Signals::Solve;
// A predicate holds for the case (case level) or for one solve case. Each is one row of section 12's
// "Applies when" column.
struct Predicate {
  std::function<bool(const Json& argument, const Signals&)> on_case;          // null: per solve case
  std::function<bool(const Json& argument, const Solve&)> on_solve;
};

bool truthy(const Json& a) { return !(a.is_boolean() && !a.get<bool>()); }

const std::map<std::string, Predicate>& predicates() {
  static const std::map<std::string, Predicate> table = [] {
    std::map<std::string, Predicate> t;
    const auto flag = [&](const char* name, bool Signals::*field) {
      t[name].on_case = [field](const Json& a, const Signals& s) { return truthy(a) && s.*field; };
    };
    const auto per_solve = [&](const char* name, bool Solve::*field) {
      t[name].on_solve = [field](const Json& a, const Solve& s) { return truthy(a) && s.*field; };
    };
    t["always"].on_case = [](const Json& a, const Signals&) { return truthy(a); };                      // XD-11, XD-15
    t["stage_runs"].on_case = [](const Json& a, const Signals& s) {                                    // XD-01: every gsea
      return a.is_string() && s.commands.contains(a.get<std::string>());
    };
    flag("raw_merge", &Signals::raw_merge);                                   // XD-03 to XD-06, XD-16
    flag("id_in_several_nodes", &Signals::id_in_several_nodes);               // XD-08
    flag("edge_bound_wraps", &Signals::edge_bound_wraps);                     // XD-12: 2^width or more (S5.09)
    flag("pathz3_without_constraints_flag", &Signals::pathz3_without_constraints_flag);   // XD-13
    flag("value_near_threshold", &Signals::value_near_threshold);             // XD-14: float rounding
    flag("fusion1_corridor_trimmable_edge", &Signals::fusion1_corridor_trimmable_edge);       // XD-17 (poll 376)
    flag("fusion1_corridor_isolated_members", &Signals::fusion1_corridor_isolated_members);   // XD-18
    flag("fusion1_graph_duplicates", &Signals::fusion1_graph_duplicates);     // XD-21
    flag("single_kgml_read", &Signals::single_kgml_read);                     // XD-22
    flag("value_prefix_token", &Signals::value_prefix_token);                 // XD-23
    flag("value_repeats_id", &Signals::value_repeats_id);                     // XD-24
    flag("value_lone_cr_or_bom", &Signals::value_lone_cr_or_bom);             // XD-25
    flag("kgml_maplink", &Signals::kgml_maplink);                             // XD-26
    flag("gsea_zero_draw", &Signals::gsea_zero_draw);                         // XD-27
    flag("gsea_equal_nes", &Signals::gsea_equal_nes);                         // XD-28
    flag("pathway_id_in_two_boxes", &Signals::pathway_id_in_two_boxes);       // XD-29
    t["fusion1_witness_relaxes_any"].on_solve = [](const Json& a, const Solve& s) {   // XD-07
      if (!a.is_array()) return false;
      return std::any_of(a.begin(), a.end(), [&](const Json& k) { return k.is_string() && s.fusion1_relaxed.contains(k.get<std::string>()); });
    };
    per_solve("fsc_not_in_exceptions", &Solve::fsc_not_in_exceptions);       // XD-09
    per_solve("fsc_down", &Solve::fsc_down);                                  // XD-10
    per_solve("excluded_or_avoided_in_complex", &Solve::excluded_or_avoided_in_complex);   // XD-19
    per_solve("fusion1_witness_below_point", &Solve::fusion1_witness_below_point);         // XD-20
    return t;
  }();
  return table;
}

[[noreturn]] void bad(const std::string& where, const std::string& why) {
  throw core::Failure("ref.expectations", "expected-differences.json: " + where + ": " + why);
}

std::vector<std::string> strings(const Json& e, const char* key, const std::string& where, bool required) {
  std::vector<std::string> out;
  if (!e.contains(key)) {
    if (required) bad(where, std::string("no \"") + key + "\"");
    return out;
  }
  if (!e[key].is_array()) bad(where, std::string("\"") + key + "\" is not a list");
  for (const Json& s : e[key]) {
    if (!s.is_string()) bad(where, std::string("\"") + key + "\" holds a value that is not text");
    out.push_back(s.get<std::string>());
  }
  return out;
}

void check_prediction(const Json& p, const std::string& where) {
  if (!p.is_object() || !p.contains("rule") || !p["rule"].is_string()) bad(where, "a prediction needs a \"rule\"");
  const std::string rule = p["rule"].get<std::string>();
  if (rule == "rescore_bound") {
    if (p.value("semantics", "") != "fusion2") bad(where, "rescore_bound bounds under fusion2's semantics (R2)");
  } else if (rule == "counts") {
    if (!p.contains("corridor") || !p["corridor"].is_object()) bad(where, "counts needs {\"corridor\": {case: [nodes, edges]}}");
    for (const auto& [id, v] : p["corridor"].items())
      if (!v.is_array() || v.size() != 2) bad(where, "counts for " + id + " is not [nodes, edges]");
  } else if (rule == "unchanged") {
    for (const std::string& s : strings(p, "stages", where, true))
      if (!parse_stage(s)) bad(where, "unchanged names an unknown stage " + s);
  } else if (rule == "selection_size") {
    if (!p.contains("cases") || !p["cases"].is_object()) bad(where, "selection_size needs {\"cases\": {case: pathways}}");
  } else {
    bad(where, "unknown prediction rule " + rule + ": give rescore_bound, counts, unchanged or selection_size");
  }
}

// ---- the file's layout: one key per line, each value on one line, ", " between items
std::string inline_json(const Json& j) {
  if (j.is_object()) {
    std::string out = "{";
    bool first = true;
    for (const auto& [k, v] : j.items()) {
      out += (first ? "" : ", ") + Json(k).dump() + ": " + inline_json(v);
      first = false;
    }
    return out + "}";
  }
  if (j.is_array()) {
    std::string out = "[";
    for (std::size_t k = 0; k < j.size(); ++k) out += (k ? ", " : "") + inline_json(j[k]);
    return out + "]";
  }
  return j.dump();
}

}  // namespace

const std::vector<std::string>& known_facts() {
  static const std::vector<std::string> facts = {
      "classify.lists",       "exceptions.set",   "gsea.table",        "gsea.shortlist", "merge.graph",
      "corridor.graph",       "corridor.counts",  "observations.counts", "solve_inputs.counts",
      "solve.frontier",       "solve.answers",    "solve.witness",     "solve.dat"};
  return facts;
}

std::vector<Entry> load_expectations(const fs::path& file) {
  Json doc;
  try {
    doc = Json::parse(fusiontest::slurp(file));
  } catch (const Json::exception& e) {
    throw core::Failure("ref.expectations", core::to_utf8(file) + ": " + e.what());
  }
  if (!doc.is_object() || !doc.contains("entries") || !doc["entries"].is_array()) bad("the file", "no \"entries\" list");
  std::vector<Entry> out;
  for (const Json& e : doc["entries"]) {
    Entry x;
    if (!e.is_object() || !e.contains("id") || !e["id"].is_string()) bad("an entry", "no \"id\"");
    x.id = e["id"].get<std::string>();
    const std::string where = x.id;
    if (!e.contains("why") || !e["why"].is_string()) bad(where, "no \"why\"");
    x.why = e["why"].get<std::string>();
    x.spec = strings(e, "spec", where, true);
    x.allows = strings(e, "allows", where, true);
    x.seen_in = strings(e, "seen_in", where, true);
    for (const std::string& s : strings(e, "stages", where, true)) {
      const auto st = parse_stage(s);
      if (!st) bad(where, "unknown stage " + s);
      x.stages.push_back(*st);
    }
    for (const std::string& f : x.allows) {
      if (std::find(known_facts().begin(), known_facts().end(), f) == known_facts().end()) bad(where, "allows an unknown fact " + f);
      const auto stage = parse_stage(f.substr(0, f.find('.')));
      if (!stage || std::find(x.stages.begin(), x.stages.end(), *stage) == x.stages.end())
        bad(where, "allows " + f + ", whose stage is not in its \"stages\"");
    }
    if (!e.contains("applies_when") || !e["applies_when"].is_object() || e["applies_when"].size() != 1)
      bad(where, "\"applies_when\" must name one predicate");
    x.applies_when = e["applies_when"];
    const std::string predicate = x.applies_when.begin().key();
    if (!predicates().contains(predicate)) bad(where, "unknown predicate " + predicate);
    if (e.contains("predicts")) {
      x.predicts = e["predicts"];
      if (x.predicts->is_array()) {
        for (const Json& p : *x.predicts) check_prediction(p, where);
      } else {
        check_prediction(*x.predicts, where);
      }
    }
    out.push_back(std::move(x));
  }
  for (std::size_t a = 0; a < out.size(); ++a)
    for (std::size_t b = a + 1; b < out.size(); ++b)
      if (out[a].id == out[b].id) bad(out[a].id, "the id appears twice");
  return out;
}

bool applies(const Entry& e, const Signals& s, const std::string& solve_case) {
  const std::string name = e.applies_when.begin().key();
  const Json argument = e.applies_when.begin().value();   // a copy: the iterator is a temporary
  const auto& table = predicates();
  const Predicate& p = table.at(name);
  if (p.on_case) return p.on_case(argument, s);
  if (!solve_case.empty()) {
    const auto it = s.solves.find(solve_case);
    return it != s.solves.end() && p.on_solve(argument, it->second);
  }
  return std::any_of(s.solves.begin(), s.solves.end(), [&](const auto& kv) { return p.on_solve(argument, kv.second); });
}

std::vector<Json> predictions(const Entry& e) {
  if (!e.predicts) return {};
  if (e.predicts->is_array()) return std::vector<Json>(e.predicts->begin(), e.predicts->end());
  return {*e.predicts};
}

std::vector<Stage> unchanged_stages(const Entry& e) {
  std::vector<Stage> out;
  for (const Json& p : predictions(e))
    if (p.value("rule", "") == "unchanged")
      for (const Json& s : p["stages"]) out.push_back(*parse_stage(s.get<std::string>()));
  return out;
}

std::string layout(const Json& doc) {
  std::string out = "{\n";
  std::size_t k = 0;
  for (const auto& [key, value] : doc.items()) {
    out += "  " + Json(key).dump() + ": ";
    if (key == "entries" && value.is_array()) {
      out += "[\n";
      for (std::size_t i = 0; i < value.size(); ++i) {
        out += "    {\n";
        std::size_t f = 0;
        for (const auto& [field, v] : value[i].items())
          out += "      " + Json(field).dump() + ": " + inline_json(v) + (++f < value[i].size() ? ",\n" : "\n");
        out += std::string("    }") + (i + 1 < value.size() ? ",\n" : "\n");
      }
      out += "  ]";
    } else {
      out += inline_json(value);
    }
    out += ++k < doc.size() ? ",\n" : "\n";
  }
  return out + "}\n";
}

std::string with_seen(const fs::path& file, const std::vector<Entry>& entries) {
  Json doc = Json::parse(fusiontest::slurp(file));
  for (Json& e : doc["entries"]) {
    const std::string id = e.value("id", "");
    const auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& x) { return x.id == id; });
    if (it != entries.end()) e["seen_in"] = it->seen_in;
  }
  return layout(doc);
}

}  // namespace fusion::reftest
