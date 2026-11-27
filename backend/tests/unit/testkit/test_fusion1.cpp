// The readers of FuSION 1's files, and the checker on FuSION 1's own witnesses (section 12, "Test
// hooks"): under fusion1 semantics the checker accepts every witnessed point with a cost equal to the
// point and rejects mutated witnesses, and under fusion2 it rejects r5's out witness at (4, 4) for its
// [STAT1+STAT3] complex (XD-19). The toy cases are in the repository; the KEGG cases need the data root.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

#include "fusion/core/core.hpp"
#include "fusion/testkit/catch.hpp"
#include "fusion/testkit/check.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "fusion/testkit/scratch.hpp"

using namespace fusion::testkit;
namespace fs = std::filesystem;

namespace {

const fs::path kToys = source_dir() / "reference" / "synthetic";

std::vector<std::string> lines(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::vector<std::string> out;
  for (std::string l; std::getline(in, l);) {
    if (!l.empty() && l.back() == '\r') l.pop_back();
    out.push_back(l);
  }
  return out;
}

std::vector<std::string> tokens(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) out.push_back(cur), cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

// An edge list file (S5.06): "<src> <tgt> <subtypes joined by _>".
std::vector<EdgeSpec> edge_lines(const fs::path& p) {
  std::vector<EdgeSpec> out;
  for (const std::string& l : lines(p)) {
    const auto t = tokens(l);
    if (t.size() < 3) continue;
    EdgeSpec e{t[0], t[1], {}};
    std::string part;
    for (char c : t[2] + "_") {
      if (c == '_') {
        if (!part.empty() && part != "-") e.subtypes.push_back(part);
        part.clear();
      } else {
        part += c;
      }
    }
    out.push_back(e);
  }
  return out;
}

// One FuSION 1 solve, from the pathz3 line its console echoed (S5.01's 34 tokens, S1.10): the captured
// corridor with the line's pair and bound, the case from its list files, and its witnesses.
struct Solve {
  std::string arm;                     // the prefix's file name: "temp_w" or "temp_wo"
  CanonCorridor corridor;
  CaseSpec spec;
  std::vector<f1::PointRecord> records;
};

std::vector<Solve> solves(const fs::path& case_dir, const fs::path& console) {
  const CanonGraph corridor = f1::read_written_graph(case_dir / "f1" / "capture" / "corridor.xml");
  // FuSION 1's absolute paths, read where the data root keeps the case: its run/ or inputs/ files.
  const auto local = [&](const std::string& path) {
    const std::string base = path.substr(path.find_last_of('/') + 1);
    for (const fs::path& p : {case_dir / "f1" / "run" / base, case_dir / "inputs" / base})
      if (fs::exists(p)) return p;
    return fs::path();
  };
  const auto ids = [&](const std::string& path) {
    const fs::path p = local(path);
    return p.empty() ? std::vector<std::string>{} : f1::read_ids(p);
  };
  std::vector<Solve> out;
  for (const std::string& line : lines(console)) {
    if (line.rfind("pathz3 ", 0) != 0) continue;
    const auto t = tokens(line);
    REQUIRE(t.size() >= 29);
    Solve s;
    s.arm = t[9].substr(t[9].find_last_of('/') + 1);
    s.corridor.graph = corridor;
    const auto pairs = ids(t[3]);
    REQUIRE(pairs.size() >= 2);
    s.corridor.sources = {pairs[0]};
    s.corridor.targets = {pairs[1]};
    s.corridor.bound = std::stoi(t[8]);
    s.spec.up = ids(t[10]);
    s.spec.down = ids(t[11]);
    s.spec.essential = ids(t[12]);
    s.spec.avoid = ids(t[13]);
    if (!local(t[14]).empty()) s.spec.essential_edges = edge_lines(local(t[14]));
    if (!local(t[15]).empty()) s.spec.avoid_edges = edge_lines(local(t[15]));
    s.spec.active = ids(t[16]);
    s.spec.inactive = ids(t[17]);
    s.spec.confirmed_up = ids(t[18]);
    s.spec.confirmed_down = ids(t[19]);
    s.spec.nonrelaxed = ids(t[21]);
    if (!local(t[23]).empty()) s.spec.nonrelaxed_edges = edge_lines(local(t[23]));
    // S5.02, S5.03: the optional tail; families on only with the flag, each off at a negative threshold.
    const bool enabled = t.size() > 32 && t[32] == "1";
    if (enabled) {
      const auto number = [](const std::string& x) { return fusion::core::parse_decimal(x); };
      const auto coexp = number(t[30]), frozen = number(t[31]);
      if (coexp.status == fusion::core::DecimalStatus::ok && coexp.value >= 0 && !local(t[29]).empty())
        s.spec.coexpressed = f1::read_coexpression(local(t[29]), coexp.value);
      if (frozen.status == fusion::core::DecimalStatus::ok && frozen.value >= 0) s.spec.frozen_threshold = frozen.value;
    }
    const fs::path solutions = case_dir / "f1" / "run" / (s.arm + "_solutions.txt");
    if (fs::exists(solutions)) s.records = f1::read_solutions(solutions, case_dir / "f1" / "run" / (s.arm + "_relaxations.txt"));
    out.push_back(std::move(s));
  }
  return out;
}

struct Tally { int witnessed = 0, accepted = 0, unwitnessed = 0, deselected = 0, flipped = 0; };

// R1 for FuSION 1: every witness passes under fusion1 and costs its point; each of its mutations fails.
void judge(const Solve& s, const std::string& name, Tally& tally) {
  const std::set<std::string> observed = [&] {
    std::set<std::string> o(s.spec.up.begin(), s.spec.up.end());
    o.insert(s.spec.down.begin(), s.spec.down.end());
    return o;
  }();
  for (const f1::PointRecord& r : s.records) {
    INFO(name << " " << s.arm << " (" << r.point.n << ", " << r.point.e << ")");
    if (!r.witness) {
      ++tally.unwitnessed;
      CHECK_FALSE(r.reason.empty());
      continue;
    }
    ++tally.witnessed;
    const Assignment a = from_f1(s.corridor, *r.witness);
    const Verdict v = check(s.corridor, s.spec, a, Semantics::fusion1);
    INFO((v.violations.empty() ? std::string() : v.violations.front()));
    CHECK(v.valid);
    CHECK(v.cost == r.point);
    if (v.valid && v.cost == r.point) ++tally.accepted;
    // One edge deselected: the only selected in-edge of a present node that is not a source.
    std::map<std::string, int> ins;
    for (const auto& e : a.selected) ++ins[e.tgt];
    for (const auto& e : a.selected)
      if (ins[e.tgt] == 1 && e.tgt != s.corridor.sources.front()) {
        Assignment m = a;
        m.selected.erase(std::find(m.selected.begin(), m.selected.end(), e));
        CHECK_FALSE(check(s.corridor, s.spec, m, Semantics::fusion1).valid);
        ++tally.deselected;
        break;
      }
    // One activity flipped: a present observed gene that is not an endpoint changes its relaxation.
    for (std::size_t k = 0; k < a.present.size(); ++k) {
      const CanonNode* n = s.corridor.graph.node(a.present[k].first);
      if (!n || n->type == "group") continue;
      const std::string key = module_key(*n);
      if (key == s.corridor.sources.front() || key == s.corridor.targets.front()) continue;
      if (std::none_of(n->members.begin(), n->members.end(), [&](const std::string& m) { return observed.contains(m); }))
        continue;
      Assignment m = a;
      m.present[k].second = !m.present[k].second;
      const Verdict flipped = check(s.corridor, s.spec, m, Semantics::fusion1);
      CHECK_FALSE((flipped.valid && flipped.cost == r.point));
      ++tally.flipped;
      break;
    }
  }
}

}  // namespace

TEST_CASE("testkit: FuSION 1's .dat, limits, solutions and console files of a toy case", "[unit][testkit]") {
  const fs::path run = kToys / "toy-hsa5-wni-tsv-b10" / "f1" / "run";
  CHECK(f1::read_dat(run / "hsa1_to_hsa22_w_hsa5_with_node_included.tsv_b10.dat") ==
        std::vector<Point>{{1, 7}, {2, 5}, {5, 3}, {7, 2}});
  const f1::Limits limits = f1::read_limits(run / "hsa1_to_hsa22_w_hsa5_with_node_included.tsv_b10_limits_timefile.txt");
  CHECK(limits.box_lo == Point{0, 0});
  CHECK(limits.box_hi == Point{7, 7});
  REQUIRE(limits.checks.size() >= 3);
  CHECK(limits.checks[0].point == Point{3, 3});
  CHECK(limits.checks[0].step == "diagonal");
  CHECK(limits.checks[0].answer == CheckOutcome::unsat);
  CHECK(limits.checks[0].cpu_s > 0);
  const auto cached = std::find_if(limits.checks.begin(), limits.checks.end(), [](const f1::Check& c) { return c.cached; });
  REQUIRE(cached != limits.checks.end());
  CHECK(cached->point == Point{1, 5});                     // "Point 5 1 already unsat": e first
  CHECK(cached->step == "smallest_n");
  REQUIRE(limits.found.size() >= 2);
  CHECK(limits.found[0] == Point{2, 5});                   // "Point 1: (5, 2)": e first
  const auto records = f1::read_solutions(run / "temp_w_solutions.txt", run / "temp_w_relaxations.txt");
  REQUIRE(records.size() == 4);
  CHECK(records[0].point == Point{7, 2});
  REQUIRE(records[0].witness);
  CHECK(records[0].witness->present_nodes.size() == 16);
  CHECK(records[0].witness->selected_edges.size() == 16);
  CHECK(records[0].witness->relaxed_nodes.size() == 7);
  CHECK(records[0].witness->relaxed_edges.size() == 2);
  CHECK(records[0].witness->selected_edges[0] == f1::EdgeKey{"hsa1", "hsa2", {"activation"}});
  const f1::ConsoleFacts facts = f1::read_console(kToys / "toy-hsa5-wni-tsv-b10" / "f1" / "run_console.log");
  CHECK(facts.up_down == std::vector<std::pair<int, int>>{{16, 5}, {15, 5}});
  CHECK(facts.corridor_sizes == std::vector<std::pair<int, int>>{{22, 25}, {22, 25}});
  CHECK(f1::read_ids(run / "connect_pairs") == std::vector<std::string>{"hsa1", "hsa22"});
  // The written corridor as FuSION 1's tool-written read builds it: its 22 nodes and 25 edges.
  const CanonGraph corridor = f1::read_written_graph(kToys / "toy-hsa5-wni-tsv-b10" / "f1" / "capture" / "corridor.xml");
  CHECK(corridor.nodes.size() == 22);
  CHECK(corridor.edges.size() == 25);
}

TEST_CASE("testkit: the checker accepts every witness of FuSION 1's toy cases under fusion1, and rejects mutations",
          "[unit][testkit]") {
  Tally tally;
  for (const auto& entry : fs::directory_iterator(kToys)) {
    const std::string name = entry.path().filename().string();
    if (!name.starts_with("toy-")) continue;
    for (const Solve& s : solves(entry.path(), entry.path() / "f1" / "run_console.log")) judge(s, name, tally);
  }
  CHECK(tally.witnessed > 40);
  CHECK(tally.accepted == tally.witnessed);
  CHECK(tally.deselected > 0);
  CHECK(tally.flipped > 0);
}

TEST_CASE("testkit: the checker accepts FuSION 1's witnessed points of the 23 recorded cases under fusion1",
          "[testkit][kegg][S5.13]") {
  // Section 12's 177 witnessed points of the 24 valid runs count the sweep's fdr025, which repeats
  // jun-b10-fdr025 byte for byte and is not a case of its own: the 23 cases hold 176 (178 points, two of
  // them with no witness, r5's out case at (7, 0) and r6's at (2, 8)).
  const fs::path cases_json = source_dir() / "reference" / "cases.json";
  const fusion::core::Json cases = fusion::core::Json::parse([&] {
    std::ifstream in(cases_json, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }());
  std::vector<std::string> paths;
  std::vector<std::pair<std::string, std::string>> recorded;   // id, console
  for (const auto& c : cases["cases"]) {
    if (c["source"].get<std::string>().rfind("recorded", 0) != 0) continue;
    const std::string id = c["id"], console = c["fusion1"]["console"];
    recorded.emplace_back(id, console);
    paths.push_back("reference/" + id + "/f1/run/");
    paths.push_back("reference/" + id + "/inputs/");
    paths.push_back("reference/" + id + "/" + console);
    paths.push_back("reference/" + id + "/f1/capture/corridor.xml");
  }
  REQUIRE(recorded.size() == 23);
  const auto required = require(paths);
  if (std::holds_alternative<std::string>(required)) SKIP(std::get<std::string>(required));
  const DataRoot root = std::get<DataRoot>(required);
  Tally tally;
  for (const auto& [id, console] : recorded)
    for (const Solve& s : solves(root.at("reference/" + id), root.at("reference/" + id + "/" + console))) judge(s, id, tally);
  CHECK(tally.witnessed == 176);
  CHECK(tally.accepted == 176);
  CHECK(tally.unwitnessed == 2);
  CHECK(tally.deselected > 150);
  CHECK(tally.flipped > 150);
}

TEST_CASE("testkit: under fusion2 the checker rejects r5's out witness at (4, 4) for its [STAT1+STAT3] complex",
          "[testkit][kegg][XD-19]") {
  const auto root = FUSION_REQUIRE_DATA("reference/fdr044-r5-mcl1-stat3/f1/run/", "reference/fdr044-r5-mcl1-stat3/inputs/",
                                        "reference/fdr044-r5-mcl1-stat3/f1/run_console.log",
                                        "reference/fdr044-r5-mcl1-stat3/f1/capture/corridor.xml");
  const fs::path dir = root.at("reference/fdr044-r5-mcl1-stat3");
  const std::vector<Solve> all = solves(dir, dir / "f1" / "run_console.log");
  const auto out = std::find_if(all.begin(), all.end(), [](const Solve& s) { return s.arm == "temp_wo"; });
  REQUIRE(out != all.end());
  const auto at = std::find_if(out->records.begin(), out->records.end(), [](const f1::PointRecord& r) { return r.point == Point{4, 4}; });
  REQUIRE(at != out->records.end());
  REQUIRE(at->witness);
  const Assignment a = from_f1(out->corridor, *at->witness);
  const Verdict f1 = check(out->corridor, out->spec, a, Semantics::fusion1);
  CHECK(f1.valid);                                         // FuSION 1's own rules allow the complex
  const auto rejected_for_the_complex = [](const Verdict& v) {
    return !v.valid && std::any_of(v.violations.begin(), v.violations.end(), [](const std::string& s) {
      return s.find("a complex holding the avoided hsa6774") != std::string::npos;
    });
  };
  // With r5's own avoid list (STAT3 avoided), and with STAT3 named as the out case's FSC.
  CHECK(std::find(out->spec.avoid.begin(), out->spec.avoid.end(), "hsa6774") != out->spec.avoid.end());
  CHECK(rejected_for_the_complex(check(out->corridor, out->spec, a, Semantics::fusion2)));
  CaseSpec named = out->spec;
  named.avoid.clear();
  named.fsc = {"hsa6774"};
  named.solve_case = CaseSpec::Case::out;
  CHECK(rejected_for_the_complex(check(out->corridor, named, a, Semantics::fusion2)));
}

TEST_CASE("testkit: read_written_graph gives W3's node and edge counts for all 39 captures", "[testkit][kegg]") {
  const std::string counts = "reports/w3/20261117T183723/counts.tsv";
  const std::vector<std::string> wanted{counts};
  const auto required = require(wanted);
  if (std::holds_alternative<std::string>(required)) SKIP(std::get<std::string>(required));
  const DataRoot root = std::get<DataRoot>(required);
  std::vector<std::string> captures;
  const auto rows = lines(root.at(counts));
  for (std::size_t k = 1; k < rows.size(); ++k) {
    const auto f = tokens(rows[k]);
    if (f.size() < 11) continue;
    captures.push_back("reference/" + f[0] + "/f1/capture/graph_reread.xml");
    captures.push_back("reference/" + f[0] + "/f1/capture/corridor.xml");
  }
  REQUIRE(captures.size() == 78);
  const auto checked = require(captures);
  if (std::holds_alternative<std::string>(checked)) SKIP(std::get<std::string>(checked));
  for (std::size_t k = 1; k < rows.size(); ++k) {
    const auto f = tokens(rows[k]);
    INFO(f[0]);
    const CanonGraph reread = f1::read_written_graph(root.at("reference/" + f[0] + "/f1/capture/graph_reread.xml"));
    const CanonGraph corridor = f1::read_written_graph(root.at("reference/" + f[0] + "/f1/capture/corridor.xml"));
    CHECK(reread.nodes.size() == std::stoul(f[6]));
    CHECK(reread.edges.size() == std::stoul(f[7]));
    CHECK(corridor.nodes.size() == std::stoul(f[9]));
    CHECK(corridor.edges.size() == std::stoul(f[10]));
  }
}
