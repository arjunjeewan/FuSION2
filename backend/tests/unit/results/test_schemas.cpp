// results: results.json and the view files validate against the JSON Schemas this section writes and
// section 12 keeps (2A.10). The validator covers the keywords the two schemas use, so no validator
// library is needed: type, enum, minimum, required, properties, additionalProperties, items, local $ref.
#include <catch2/catch_test_macros.hpp>

#include "fixture.hpp"
#include "fusion/results/explain.hpp"
#include "fusion/results/json.hpp"
#include "fusion/results/view.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Json = core::Json;

namespace {

bool has_type(const Json& v, const std::string& t) {
  if (t == "object") return v.is_object();
  if (t == "array") return v.is_array();
  if (t == "string") return v.is_string();
  if (t == "integer") return v.is_number_integer();
  if (t == "number") return v.is_number();
  if (t == "boolean") return v.is_boolean();
  if (t == "null") return v.is_null();
  return false;
}

void validate(const Json& root, const Json& schema, const Json& v, const std::string& at, std::vector<std::string>& errors) {
  if (schema.contains("$ref")) {
    const std::string ref = schema["$ref"].get<std::string>();
    validate(root, root["$defs"][ref.substr(std::string("#/$defs/").size())], v, at, errors);
    return;
  }
  if (schema.contains("type")) {
    const Json& t = schema["type"];
    bool ok = false;
    if (t.is_string()) ok = has_type(v, t.get<std::string>());
    else for (const Json& one : t) ok = ok || has_type(v, one.get<std::string>());
    if (!ok) {
      errors.push_back(at + ": wrong type " + std::string(v.type_name()));
      return;
    }
  }
  if (schema.contains("enum") && std::find(schema["enum"].begin(), schema["enum"].end(), v) == schema["enum"].end()) {
    errors.push_back(at + ": " + v.dump() + " is not allowed");
  }
  if (schema.contains("minimum") && v.is_number() && v.get<double>() < schema["minimum"].get<double>()) {
    errors.push_back(at + ": below the minimum");
  }
  if (v.is_object()) {
    for (const Json& key : schema.value("required", Json::array())) {
      if (!v.contains(key.get<std::string>())) errors.push_back(at + ": missing " + key.get<std::string>());
    }
    for (const auto& [key, value] : v.items()) {
      if (schema.contains("properties") && schema["properties"].contains(key)) {
        validate(root, schema["properties"][key], value, at + "." + key, errors);
      } else if (schema.contains("additionalProperties")) {
        const Json& extra = schema["additionalProperties"];
        if (extra.is_boolean() && !extra.get<bool>()) errors.push_back(at + ": unexpected " + key);
        else if (extra.is_object()) validate(root, extra, value, at + "." + key, errors);
      }
    }
  }
  if (v.is_array() && schema.contains("items")) {
    for (std::size_t i = 0; i < v.size(); ++i) validate(root, schema["items"], v[i], at + "[" + std::to_string(i) + "]", errors);
  }
}

std::vector<std::string> check(const std::string& schema_file, const std::string& document) {
  const Json schema = Json::parse(fx::read(fx::source_dir() / "unit" / "results" / "schemas" / schema_file));
  std::vector<std::string> errors;
  validate(schema, schema, Json::parse(document), "$", errors);
  return errors;
}

SolveResult sample() {
  fx::Catalogue cat;
  cat.node("s", "EGFR");
  cat.node("t", "CXCL8", Observed::up, 2.0);
  cat.node("f", "JUN", Observed::up, 1.25);
  cat.node("u", "IRS2", Observed::up);
  cat.node("d", "DUSP1", Observed::down, -0.5);
  const auto sf = ek("s", "f", {"activation"});
  const auto ft = ek("f", "t", {"expression"});
  const auto sd = ek("s", "d", {"activation"});
  const auto dt = ek("d", "t", {"inhibition"});
  for (const auto& e : {sf, ft, sd, dt}) cat.edge(e, {"path:1"});
  cat.gene("f", "f");
  fx::W in{{"s", "f", "t"}, {"d"}, {sf, ft, sd, dt}, {{"u", false}}, {{sd, true}}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {in}, {0}));
  results.cases[0].frontier.points[0].undecided_below = {pt(0, 1)};
  results.cases[0].frontier.undecided = {pt(0, 1)};
  results.cases[0].observations = {solve::ObservationRow{"f", "JUN", 'u', solve::Role::forced},
                                   solve::ObservationRow{"q", "Q", 'd', solve::Role::not_in_network}};
  solve::CaseResult out{};
  out.kase = solve::Case::out;
  out.status = solve::CaseStatus::no_point;
  results.cases.push_back(out);
  solve::CaseResult asked{};
  asked.kase = solve::Case::free;
  asked.status = solve::CaseStatus::complete;
  asked.witnesses.push_back(in.make());
  asked.log = {fx::answer(pt(1, 1), solve::Phase::point, solve::Outcome::sat, 0),
               fx::answer(pt(0, 0), solve::Phase::point, solve::Outcome::unsat)};
  results.cases.push_back(asked);
  SolveSpec spec;
  spec.source.value = "s";
  spec.target.value = "t";
  spec.fsc = {network::NodeKey{"f"}};
  spec.frozen_threshold = 800;
  spec.rules = {std::string(rule_de_on_every_path)};
  return build(results, spec, cat);
}

}  // namespace

TEST_CASE("results.json validates against its schema (2A.10)", "[unit][results]") {
  SolveResult r = sample();
  const auto issues = analyse(r);
  const std::string json = to_json(r, issues, compare_cases(r));
  const auto errors = check("results.schema.json", json);
  for (const auto& e : errors) UNSCOPED_INFO(e);
  CHECK(errors.empty());
  // With an issue and no comparison, too.
  const std::vector<Issue> one{{Issue::Kind::internal_error, "results.fsc", "internal error: …"}};
  CHECK(check("results.schema.json", to_json(r, one, std::nullopt)).empty());
  // The validator does fail a document that breaks the schema.
  Json broken = Json::parse(json);
  broken["cases"][0]["status"] = "finished";
  broken["nodes"][0].erase("label");
  broken["corridor"]["extra"] = 1;
  CHECK(check("results.schema.json", broken.dump()).size() == 3);
}

TEST_CASE("view files validate against their schema (2A.10)", "[unit][results]") {
  const SolveResult r = sample();
  std::size_t views = 0;
  for (const CaseResult& c : r.cases) {
    for (const FrontierPoint& p : c.points) {
      const auto errors = check("view.schema.json", view_json(build_view(r, c, p), r, parse_path_filter("alt:3"), 1, true));
      for (const auto& e : errors) UNSCOPED_INFO(e);
      CHECK(errors.empty());
      ++views;
    }
    for (const RequestedPoint& q : c.requested_points) {
      CHECK(check("view.schema.json", view_json(build_view(r, c, q), r)).empty());
      ++views;
    }
  }
  CHECK(views == 3);
}
