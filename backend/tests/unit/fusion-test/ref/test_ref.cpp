// fusion-test ref, the reference runner (section 12, "Reference-test runner" and its test hooks; the C4 note's
// self-tests): verification points on staircases, the preparation of a case's folder and its edits, the
// expected-differences file, the arm-to-step mapping of a toy's chained run, and the runner on a toy case
// with an injected difference, which must report an unexpected difference, an allowed one, a failed
// prediction and a stale entry.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "fusion/core/core.hpp"
#include "fusion/testkit/catch.hpp"
#include "fusion/testkit/scratch.hpp"
#include "ref/runner.hpp"

#ifndef FUSION_EXE
#error "test_fusion_test is built with FUSION_EXE, the staged fusion"
#endif
#ifndef FUSION_TEST_EXE
#error "test_fusion_test is built with FUSION_TEST_EXE, the fusion-test that verification starts"
#endif

using namespace fusion::reftest;
using Catch::Matchers::ContainsSubstring;
using fusion::core::Json;
using fusion::testkit::Point;
namespace fs = std::filesystem;
namespace core = fusion::core;
namespace testkit = fusion::testkit;

namespace {

const fs::path kTests = FUSION_TEST_SOURCE_DIR;
constexpr const char* kToy = "toy-hsa11-wni-tsv-b10";

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

bool in_up_set(const std::vector<Point>& f, Point p) {
  return std::any_of(f.begin(), f.end(), [&](Point q) { return q.n <= p.n && q.e <= p.e; });
}

// A copy of backend/tests holding one toy case, so a test can change its FuSION 1 files and its entries.
struct ToyTree {
  testkit::ScratchDir dir{"ref-toy"};
  fs::path root = dir.path() / "tests";
  Case kase;
  explicit ToyTree(const std::string& id = kToy) {
    const auto cases = load_cases(kTests / "reference" / "cases.json");
    kase = *std::find_if(cases.begin(), cases.end(), [&](const Case& c) { return c.id == id; });
    fs::create_directories(root / "fixtures" / "toy-batches");
    fs::copy(kTests / "fixtures" / kase.batch, root / "fixtures" / kase.batch);
    fs::create_directories(root / "reference" / "synthetic");
    fs::copy(kTests / "reference" / "synthetic" / id, root / "reference" / "synthetic" / id, fs::copy_options::recursive);
    Json doc = Json::parse(read(kTests / "reference" / "cases.json"));
    Json one = Json::array();
    for (const Json& c : doc["cases"])
      if (c["id"] == id) one.push_back(c);
    doc["cases"] = one;
    write(root / "reference" / "cases.json", doc.dump(2));
  }
  fs::path f1() const { return root / "reference" / "synthetic" / kase.id / "f1"; }
  Roots roots() const { return Roots{root / "fixtures", root / "reference" / "synthetic", std::nullopt}; }
  Options options(const std::string& out, std::vector<Mode> modes = {}) const {
    Options o;
    o.tests = root;
    o.fusion = FUSION_EXE;
    o.worker = FUSION_TEST_EXE;
    o.out = dir.path() / out;
    o.expectations = root / "reference" / "expected-differences.json";
    o.modes = std::move(modes);
    o.keep = true;
    return o;
  }
};

const Finding* find(const Report& r, Mode m, Stage s, Outcome o) {
  for (const Finding& f : r.findings)
    if (f.mode == m && f.stage == s && f.outcome == o) return &f;
  return nullptr;
}

std::string all(const Report& r) {
  std::string out;
  for (const Finding& f : r.findings)
    out += std::string(name(f.mode)) + " " + std::string(name(f.stage)) + " " + f.solve_case + " " +
           std::string(name(f.outcome)) + ": " + f.detail.value("text", "") + "\n";
  return out;
}

}  // namespace

TEST_CASE("ref: verification points prove a staircase with at most 2k + 1 checks (12.C7)", "[unit][fusion-test][ref][12.C7]") {
  testkit::Frontier f;
  f.points = {{2, 7}, {3, 6}, {4, 5}, {6, 3}};          // a toy's frontier, as FuSION 1 found it
  const std::vector<Point> p = verification_points(f, {0, 0}, {7, 7});
  CHECK(p.size() == 9);
  for (Point corner : {Point{2, 6}, Point{3, 5}, Point{5, 4}, Point{1, 7}, Point{7, 2}})
    CHECK(std::find(p.begin(), p.end(), corner) != p.end());
  CHECK(verification_points(testkit::Frontier{}, {0, 0}, {7, 7}) == std::vector<Point>{{7, 7}});
  testkit::Frontier edge;                                // corners outside the box are not asked
  edge.points = {{0, 3}, {2, 0}};
  CHECK(verification_points(edge, {0, 0}, {4, 4}) == std::vector<Point>{{0, 3}, {2, 0}, {1, 2}});

  // On random staircases: the corners lie outside F's up-set, and every cell of the box outside it lies
  // below a corner, so the unsat corners prove F (S5.18's monotonicity).
  core::Rng rng = core::Rng::stream(2026, 4);
  for (int round = 0; round < 200; ++round) {
    const Point hi{static_cast<int>(rng.below(9)), static_cast<int>(rng.below(9))};
    std::vector<Point> stair;
    int e = hi.e;
    for (int n = 0; n <= hi.n && e >= 0; ++n)
      if (rng.below(3) == 0) {
        stair.push_back({n, e});
        e -= 1 + static_cast<int>(rng.below(3));
      }
    testkit::Frontier fr;
    fr.points = stair;
    const std::vector<Point> asked = verification_points(fr, {0, 0}, hi);
    INFO("round " << round);
    CHECK(asked.size() <= 2 * stair.size() + 1);
    std::vector<Point> corners;
    for (Point q : asked)
      if (std::find(stair.begin(), stair.end(), q) == stair.end()) corners.push_back(q);
    for (Point q : corners) CHECK_FALSE(in_up_set(stair, q));
    for (int n = 0; n <= hi.n; ++n)
      for (int k = 0; k <= hi.e; ++k)
        if (!in_up_set(stair, {n, k}))
          CHECK(std::any_of(corners.begin(), corners.end(), [&](Point c) { return n <= c.n && k <= c.e; }));
  }
}

TEST_CASE("ref: a case's folder takes the path map, refuses FuSION 1's tree and lists its edits (C4.1)", "[unit][fusion-test][ref][C4.1]") {
  SECTION("the map is applied longest first, as run_fusion1.py does") {
    const std::vector<PathMap> map{{"/a/b", "$CASE/x"}, {"/a/b/c", "$DATA/y"}};
    CHECK(apply_path_map("/a/b/c/d /a/b/e", map, "/case", fs::path("/data")) == "/data/y/d /case/x/e");
    CHECK_THROWS_WITH(apply_path_map("/a/b/c", map, "/case", std::nullopt), ContainsSubstring("$DATA"));
  }
  SECTION("a path left in FuSION 1's tree or its snapshot is found, FuSION2's is not") {
    CHECK(leftover_fusion1_path("x /home/ks-149-u24/Desktop/arjun/FuSION/runs/a y") == "/home/ks-149-u24/Desktop/arjun/FuSION/");
    CHECK(leftover_fusion1_path("cd /home/ks-149-u24/Desktop/arjun/FuSION") == "/home/ks-149-u24/Desktop/arjun/FuSION");
    CHECK(leftover_fusion1_path("/home/ks-149-u24/Desktop/arjun/FuSION1-reference-2026-11-10/Linux").has_value());
    CHECK_FALSE(leftover_fusion1_path("/home/ks-149-u24/Desktop/arjun/FuSION2/backend /home/ks-149-u24/Desktop/arjun/FuSION2-test-data").has_value());
  }
  SECTION("drop_pathz3_tail keeps pathz3's 28 fixed tokens and is refused where it does not apply") {
    std::string fixed;
    for (int k = 0; k < 28; ++k) fixed += " t" + std::to_string(k);
    const std::string batch = "let $C %__a21__%\npathz3" + fixed + " $C 0.5 0.6 1 42\npathz3 fsc=hsa1\nexit\n";
    std::vector<std::string> applied;
    const std::vector<Edit> edit{{"drop_pathz3_tail", "%__a21__%", "S5.02"}};
    CHECK(apply_edits(batch, edit, applied) == "let $C %__a21__%\npathz3" + fixed + "\npathz3 fsc=hsa1\nexit\n");
    REQUIRE(applied.size() == 1);
    CHECK_THAT(applied.front(), ContainsSubstring("1 pathz3 line: S5.02"));
    CHECK_THROWS_WITH(apply_edits("pathz3" + fixed + " a\n", edit, applied), ContainsSubstring("does not hold"));
    CHECK_THROWS_WITH(apply_edits("%__a21__%\npathz3" + fixed + "\n", edit, applied), ContainsSubstring("no pathz3 line"));
    CHECK_THROWS_WITH(apply_edits(batch, std::vector<Edit>{{"fill", "x", "y"}}, applied), ContainsSubstring("unknown edit"));
  }
  SECTION("a toy's folder: its inputs, its batch mapped to the folder, its tails dropped") {
    ToyTree t;
    const Prepared p = prepare(t.kase, t.roots(), t.dir.path() / "case");
    const std::string batch = read(p.batch);
    CHECK(batch.find("/fusion-test/case") == std::string::npos);
    CHECK(batch.find(core::to_utf8(p.dir) + "/inputs/with_node_included.tsv") != std::string::npos);
    std::istringstream lines(batch);
    int pathz3 = 0;
    for (std::string line; std::getline(lines, line);)
      if (line.starts_with("pathz3 ")) {
        ++pathz3;
        CHECK(line.find("$COEXPRESSION_CSV") == std::string::npos);   // the optional tail is gone
        CHECK(line.ends_with("$CONSTR_SOLVER_TIMEOUT2"));
      }
    CHECK(pathz3 == 2);
    CHECK(fs::exists(p.dir / "inputs" / "with_node_included.xml"));
    REQUIRE(p.edits.size() == 1);
    CHECK_THAT(p.edits.front(), ContainsSubstring("on 2 pathz3 lines"));
    CHECK_THROWS_WITH(prepare(t.kase, t.roots(), t.dir.path() / "case"), ContainsSubstring("fresh folder"));
  }
  SECTION("a batch that still names FuSION 1's tree after the map is refused") {
    ToyTree t;
    const fs::path batch = t.root / "fixtures" / t.kase.batch;
    write(batch, read(batch) + "exec cat /home/ks-149-u24/Desktop/arjun/FuSION/runs/x/inputs/y\n");
    CHECK_THROWS_WITH(prepare(t.kase, t.roots(), t.dir.path() / "case"), ContainsSubstring("still names"));
  }
}

TEST_CASE("ref: the reference files load, and --record-seen keeps their layout (12.C6)", "[unit][fusion-test][ref][12.C6]") {
  const std::vector<Case> cases = load_cases(kTests / "reference" / "cases.json");
  CHECK(cases.size() == 51);
  const auto toys = std::count_if(cases.begin(), cases.end(), [](const Case& c) { return c.tier == Tier::t1; });
  CHECK(toys == 12);
  for (const Case& c : cases) CHECK(c.edits.size() == (c.tier == Tier::t1 ? 1u : 0u));
  const fs::path file = kTests / "reference" / "expected-differences.json";
  const std::vector<Entry> entries = load_expectations(file);
  REQUIRE(entries.size() == 28);                         // XD-01, XD-03 to XD-29 (XD-02 withdrawn)
  CHECK(entries.front().id == "XD-01");
  CHECK(entries.back().id == "XD-29");
  CHECK(layout(Json::parse(read(file))) == read(file));  // a review sees only the seen_in lists that change
  std::vector<Entry> changed = entries;
  changed[5].seen_in.push_back("toy-x:in");               // XD-07
  const std::string rewritten = with_seen(file, changed);
  CHECK(rewritten != read(file));
  CHECK(rewritten.find("\"toy-x:in\"") != std::string::npos);

  testkit::ScratchDir d("ref-entries");
  const auto refused = [&](const std::string& entry) {
    write(d.path() / "x.json", R"({"schema": 1, "entries": [)" + entry + "]}");
    return load_expectations(d.path() / "x.json");
  };
  const std::string base = R"("id": "XD-90", "spec": [], "why": "w", "stages": ["corridor"], "seen_in": [])";
  CHECK_THROWS_WITH(refused("{" + base + R"(, "applies_when": {"sometimes": true}, "allows": []})"), ContainsSubstring("unknown predicate"));
  CHECK_THROWS_WITH(refused("{" + base + R"(, "applies_when": {"always": true}, "allows": ["corridor.colour"]})"), ContainsSubstring("unknown fact"));
  CHECK_THROWS_WITH(refused("{" + base + R"(, "applies_when": {"always": true}, "allows": ["solve.frontier"]})"), ContainsSubstring("not in its"));
  CHECK_THROWS_WITH(refused("{" + base + R"(, "applies_when": {"always": true}, "allows": [], "predicts": {"rule": "guess"}})"),
                    ContainsSubstring("unknown prediction rule"));
}

TEST_CASE("ref: observations are counted by FuSION 1's rule on both sides (S5.07)", "[unit][fusion-test][ref][S5.07]") {
  const std::vector<std::pair<std::string, char>> genes{{"a", 'u'}, {"b", 'u'}, {"f", 'd'}, {"g", 'd'}};
  testkit::CaseSpec s;
  s.avoid = {"b"};
  CHECK(observed_counts(genes, s, {"a", "b", "f", "g"}) == std::pair{1, 2});   // an avoided gene leaves the up genes
  s.avoid.clear();
  s.fsc = {"f"};
  s.solve_case = testkit::CaseSpec::Case::out;
  CHECK(observed_counts(genes, s, {"a", "b", "f", "g"}) == std::pair{2, 2});   // so does an out case's FSC: a down one stays
  s.fsc = {"a"};
  CHECK(observed_counts(genes, s, {"a", "b", "f", "g"}) == std::pair{1, 2});
}

TEST_CASE("ref: a toy's chained run maps FuSION 1's arms to 2.0's steps by legacy prefix", "[unit][fusion-test][ref]") {
  ToyTree t;
  const CaseSetup s = set_up(t.kase, t.roots(), t.dir.path() / "chained");
  REQUIRE(s.arms.size() == 2);
  for (const Arm& a : s.arms) {
    INFO(a.name);
    REQUIRE(a.line);
    CHECK(a.fsc == std::vector<std::string>{"hsa11"});  // the ids only one arm's list holds
  }
  const ChainedRun run = run_chained(s, FUSION_EXE);
  INFO(run.result.out << run.result.err);
  REQUIRE(run.result.exit_code == 0);
  const Facts f = chained_facts(t.kase, s.batch, s.prepared.dir, run.run_dir);
  REQUIRE(f.solves.size() == 2);
  const auto step = [&](const std::string& kase) {
    for (const SolveFacts& x : f.solves)
      if (x.name == kase) return x.step;
    return std::string();
  };
  CHECK(step("in") == "pathz3");                         // temp_w
  CHECK(step("out") == "pathz3-2");                      // temp_wo
  const Facts f1 = fusion1_facts(t.kase, s.batch, s.artefacts);
  // No self-loop, edge into the source or edge out of the target in the toy networks: XD-17 does not apply.
  CHECK_FALSE(signals(t.kase, s.batch, s.arms, f1, {}, std::nullopt).fusion1_corridor_trimmable_edge);
  for (const SolveFacts& x : f.solves) {
    const auto mine = std::find_if(f1.solves.begin(), f1.solves.end(), [&](const SolveFacts& y) { return y.name == x.name; });
    REQUIRE(mine != f1.solves.end());
    CHECK(x.dat == mine->dat);                           // the .dat written natively under FuSION 1's name (01.C11)
  }
}

// Section 12's test hooks for the runner: a toy case with an injected difference must give an unexpected
// difference, an allowed one, a failed prediction and a stale entry.
TEST_CASE("ref: on a toy with an injected difference the runner reports each outcome (12, Test hooks)", "[unit][fusion-test][ref][12.C6]") {
  ToyTree t;
  // FuSION 1's printed corridor size, the one fact of corridor.counts, says 21 nodes where both 2.0 corridors have 20.
  const fs::path console = t.f1() / "run_console.log";
  std::string text = read(console);
  const std::string was = "New graph 4 has 20 nodes 22 edges";
  REQUIRE(text.find(was) != std::string::npos);
  text.replace(text.find(was), was.size(), "New graph 4 has 21 nodes 22 edges");
  write(console, text);
  const std::vector<Case> cases = load_cases(t.root / "reference" / "cases.json");

  SECTION("no entry allows it: an unexpected difference, and the case keeps its folder") {
    write(t.root / "reference" / "expected-differences.json", R"({"schema": 1, "entries": []})");
    const Options o = t.options("out-a", {Mode::pinned});
    const Report r = run(cases, load_expectations(o.expectations), o);
    INFO(all(r));
    const Finding* f = find(r, Mode::pinned, Stage::corridor, Outcome::unexpected_difference);
    REQUIRE(f);
    CHECK(f->detail["differing"] == Json::array({"corridor.counts"}));
    CHECK(r.exit_code() == 1);
    CHECK(fs::exists(o.out / "cases" / t.kase.id / "diffs.txt"));
  }
  SECTION("an entry allows it, another's prediction fails, and a third is stale") {
    write(t.root / "reference" / "expected-differences.json", R"({"schema": 1, "entries": [
      {"id": "XD-91", "spec": [], "why": "allowed", "stages": ["corridor"], "applies_when": {"always": true},
       "allows": ["corridor.counts"], "seen_in": []},
      {"id": "XD-92", "spec": [], "why": "predicted", "stages": ["corridor"], "applies_when": {"always": true},
       "allows": [], "predicts": {"rule": "counts", "corridor": {"toy-hsa11-wni-tsv-b10": [21, 22]}}, "seen_in": []},
      {"id": "XD-93", "spec": [], "why": "stale", "stages": ["classify"], "applies_when": {"always": true},
       "allows": ["classify.lists"], "seen_in": ["toy-hsa11-wni-tsv-b10"]}]})");
    Options o = t.options("out-b");
    o.record_seen = true;
    const Report r = run(cases, load_expectations(o.expectations), o);
    INFO(all(r));
    const Finding* allowed = find(r, Mode::pinned, Stage::corridor, Outcome::prediction_failed);
    REQUIRE(allowed);                                    // allowed by XD-91, and XD-92's counts fail
    CHECK(allowed->entries == std::vector<std::string>{"XD-91", "XD-92"});
    CHECK(allowed->detail["failed_predictions"][0]["entry"] == "XD-92");
    const auto stale = std::find_if(r.findings.begin(), r.findings.end(), [](const Finding& f) { return f.outcome == Outcome::stale; });
    REQUIRE(stale != r.findings.end());
    CHECK(stale->entries == std::vector<std::string>{"XD-93"});
    CHECK(r.exit_code() == 1);
    // --record-seen: XD-91 is now seen in the case, and XD-93's stale listing goes.
    const std::vector<Entry> seen = load_expectations(o.expectations);
    CHECK(seen[0].seen_in == std::vector<std::string>{"toy-hsa11-wni-tsv-b10"});
    CHECK(seen[2].seen_in.empty());
    // The reports, outside any run directory.
    ::fusion::reftest::write(r, o.out);
    const Json report = Json::parse(read(o.out / "report.json"));
    CHECK(report["summary"]["stale"] == 1);
    CHECK(report["edits"].contains(t.kase.id));
    CHECK(read(o.out / "summary.tsv").starts_with("case\tmode\tstage\tsolve_case\toutcome\tentries\tdetail\n"));
    CHECK_FALSE(fs::exists(o.out / "run.json"));
  }
}

// FuSION 1's facts and the predicates' signals on two recorded KEGG cases, read only (nothing is run): the
// figures section 12 gives for the reference run and the full run.
TEST_CASE("ref: FuSION 1's facts and signals on the reference run and the full run", "[kegg][fusion-test][ref]") {
  const testkit::DataRoot root = FUSION_REQUIRE_DATA("reference/jun-b10-fdr025/", "reference/jun-b10-old/",
                                                     "kegg/hsa_pathways_with_exp_score/", "kegg/updated_pathways/",
                                                     "fusion1/Linux/supporting_files/");
  const std::vector<Case> cases = load_cases(kTests / "reference" / "cases.json");
  const Roots roots{kTests / "fixtures", kTests / "reference" / "synthetic", root};
  testkit::ScratchDir d("ref-kegg");
  const auto facts = [&](const std::string& id, Signals& sig) {
    const Case& c = *std::find_if(cases.begin(), cases.end(), [&](const Case& x) { return x.id == id; });
    const CaseSetup s = set_up(c, roots, d.path() / id);
    const Facts f = fusion1_facts(c, s.batch, s.artefacts);
    sig = signals(c, s.batch, s.arms, f, {}, root.root);
    for (const Arm& a : s.arms) CHECK(a.fsc == std::vector<std::string>{"hsa3725"});   // JUN, in both arms
    return f;
  };
  SECTION("the reference run: 115 nodes and 370 edges, a raw merge with isolated complex members (XD-18)") {
    Signals sig;
    const Facts f = facts("jun-b10-fdr025", sig);
    REQUIRE(f.corridor_size);
    CHECK(*f.corridor_size == std::pair{115, 370});
    CHECK(sig.commands.contains("gsea"));
    CHECK(sig.raw_merge);
    CHECK(sig.fusion1_corridor_isolated_members);
    CHECK(sig.fusion1_corridor_trimmable_edge);          // XD-17: 370 edges in FuSION 1, 358 in 2.0
    CHECK_FALSE(sig.single_kgml_read);
    REQUIRE(f.gsea);
    CHECK(f.gsea->shortlist.size() == 42);
  }
  SECTION("the full run: 592 and 2138, an old master with ids in several nodes (XD-08), duplicates (XD-21) and a self-loop relaxed (XD-07)") {
    Signals sig;
    const Facts f = facts("jun-b10-old", sig);
    REQUIRE(f.corridor_size);
    CHECK(*f.corridor_size == std::pair{592, 2138});
    CHECK_FALSE(sig.raw_merge);
    CHECK(sig.id_in_several_nodes);
    CHECK(sig.fusion1_graph_duplicates);
    CHECK(sig.fusion1_corridor_isolated_members);
    CHECK(sig.fusion1_corridor_trimmable_edge);          // XD-17: 2138 edges in FuSION 1, 2117 in 2.0
    CHECK(sig.solves["out"].fusion1_relaxed.contains("self_loop"));   // NFKB1 -| NFKB1 at (7, 1)
    const auto out = std::find_if(f.solves.begin(), f.solves.end(), [](const SolveFacts& s) { return s.name == "out"; });
    REQUIRE(out != f.solves.end());
    CHECK(out->frontier.points == std::vector<Point>{{2, 7}, {3, 5}, {4, 4}, {5, 3}, {6, 2}, {7, 1}, {8, 0}});
  }
}
