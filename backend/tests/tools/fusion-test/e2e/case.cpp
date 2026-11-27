// Reading case.json (case.hpp). Every field is checked, and an unknown one is an error, so a misspelt
// expectation cannot pass unseen.
#include "e2e/case.hpp"

#include <cmath>
#include <initializer_list>

#include "files.hpp"
#include "fusion/core/core.hpp"

namespace fusion::fusiontest::e2e {

namespace fs = std::filesystem;
using core::Json;

namespace {

class Reader {
 public:
  explicit Reader(fs::path file) : file_(std::move(file)) {}

  [[noreturn]] void fail(const std::string& what) const {
    throw core::Failure("fusion_test.case", core::to_utf8(file_) + ": " + what);
  }
  void only(const Json& object, const std::string& where, std::initializer_list<const char*> known) const {
    if (!object.is_object()) fail((where.empty() ? std::string("case.json") : where) + " must be an object");
    for (auto it = object.begin(); it != object.end(); ++it) {
      bool ok = false;
      for (const char* k : known) ok = ok || it.key() == k;
      if (!ok) fail("unknown field " + where + (where.empty() ? "" : ".") + it.key());
    }
  }
  std::string text(const Json& j, const std::string& where) const {
    if (!j.is_string()) fail(where + " must be a string");
    return j.get<std::string>();
  }
  std::vector<std::string> texts(const Json& j, const std::string& where) const {
    if (!j.is_array()) fail(where + " must be an array of strings");
    std::vector<std::string> out;
    for (const Json& item : j) out.push_back(text(item, where + "[]"));
    return out;
  }
  int integer(const Json& j, const std::string& where) const {
    if (!j.is_number_integer()) fail(where + " must be an integer");
    return j.get<int>();
  }

 private:
  fs::path file_;
};

testkit::Compare mode_of(const Reader& r, const std::string& mode) {
  if (mode == "exact") return testkit::Compare::exact;
  if (mode == "tsv_masked") return testkit::Compare::tsv_masked;
  if (mode == "results") return testkit::Compare::results;
  r.fail("compare mode " + mode + " is not exact, tsv_masked or results");
}

}  // namespace

Case load_case(const fs::path& dir) {
  const fs::path file = dir / "case.json";
  const Reader r(file);
  Json j;
  try {
    j = Json::parse(slurp(file));
  } catch (const Json::exception& e) {
    r.fail(e.what());
  }
  r.only(j, "", {"case", "args", "expect", "tags", "compare", "inputs", "env", "timeout_s", "processes", "actions",
                 "pending"});
  Case c;
  c.dir = dir;
  if (!j.contains("case")) r.fail("no case id");
  c.id = r.text(j["case"], "case");
  const std::string folder = core::to_utf8(dir.filename());
  if (c.id != folder) r.fail("case is " + c.id + ", but its folder is " + folder);
  if (!j.contains("args")) r.fail("no args");
  c.args = r.texts(j["args"], "args");
  if (!j.contains("expect")) r.fail("no expect");
  const Json& expect = j["expect"];
  r.only(expect, "expect", {"status", "exit", "line", "stderr"});
  if (!expect.contains("exit")) r.fail("expect has no exit");
  c.expect.exit = r.integer(expect["exit"], "expect.exit");
  if (expect.contains("status")) {
    c.expect.status = r.text(expect["status"], "expect.status");
    if (*c.expect.status != "ok" && *c.expect.status != "failed" && *c.expect.status != "stopped")
      r.fail("expect.status " + *c.expect.status + " is not ok, failed or stopped");
  }
  if (expect.contains("line")) {
    c.expect.line = r.integer(expect["line"], "expect.line");
    if (*c.expect.line < 1) r.fail("expect.line must be a batch line, 1 or more");
  }
  if (expect.contains("stderr")) c.expect.stderr_fragment = r.text(expect["stderr"], "expect.stderr");
  if (j.contains("tags")) c.tags = r.texts(j["tags"], "tags");
  if (j.contains("compare")) {
    if (!j["compare"].is_array()) r.fail("compare must be an array");
    for (const Json& rule : j["compare"]) {
      r.only(rule, "compare[]", {"glob", "mode", "masked_columns"});
      if (!rule.contains("glob") || !rule.contains("mode")) r.fail("each compare rule needs a glob and a mode");
      testkit::FileRule f;
      f.glob = r.text(rule["glob"], "compare[].glob");
      f.mode = mode_of(r, r.text(rule["mode"], "compare[].mode"));
      if (rule.contains("masked_columns")) {
        if (f.mode != testkit::Compare::tsv_masked) r.fail("masked_columns goes with mode tsv_masked, in " + f.glob);
        f.masked_columns = r.texts(rule["masked_columns"], "compare[].masked_columns");
      }
      c.compare.push_back(std::move(f));
    }
  }
  if (j.contains("inputs")) {
    c.inputs = r.texts(j["inputs"], "inputs");
    for (const std::string& set : c.inputs)
      if (set.empty() || set == "." || set == ".." || set.find_first_of("/\\") != std::string::npos)
        r.fail("inputs names a shared set by its folder name under e2e/inputs/, not " + set);
  }
  if (j.contains("env")) {
    if (!j["env"].is_object()) r.fail("env must be an object of strings");
    for (auto it = j["env"].begin(); it != j["env"].end(); ++it)
      c.env.emplace_back(it.key(), r.text(it.value(), "env." + it.key()));
  }
  if (j.contains("timeout_s")) {
    if (!j["timeout_s"].is_number() || !(j["timeout_s"].get<double>() > 0))
      r.fail("timeout_s must be a number above 0");
    c.timeout = std::chrono::milliseconds(static_cast<long long>(std::llround(j["timeout_s"].get<double>() * 1000)));
  }
  if (j.contains("processes")) {
    c.processes = r.integer(j["processes"], "processes");
    if (c.processes < 1) r.fail("processes must be 1 or more");
  }
  if (j.contains("actions")) {
    if (!j["actions"].is_array()) r.fail("actions must be an array");
    for (const Json& a : j["actions"]) {
      r.only(a, "actions[]", {"on_event", "fusion"});
      if (!a.contains("on_event") || !a.contains("fusion")) r.fail("each action needs on_event and fusion");
      c.actions.push_back({r.text(a["on_event"], "actions[].on_event"), r.texts(a["fusion"], "actions[].fusion")});
    }
  }
  if (j.contains("pending")) {
    const Json& p = j["pending"];
    r.only(p, "pending", {"waits_for", "refused_with"});
    if (!p.contains("waits_for") || !p.contains("refused_with")) r.fail("pending needs waits_for and refused_with");
    c.pending = Pending{r.text(p["waits_for"], "pending.waits_for"), r.text(p["refused_with"], "pending.refused_with")};
  }
  return c;
}

}  // namespace fusion::fusiontest::e2e
