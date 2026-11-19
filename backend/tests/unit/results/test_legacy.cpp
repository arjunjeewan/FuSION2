// results: the two-column file and FuSION 1's text formats (section 07, test hook "Legacy files"; section
// 12's "FuSION 1 text writers byte-exact on fixed inputs"). FuSION 1's own dumps of the regenerated toy
// cases (backend/tests/reference/synthetic, Booking 1) are rebuilt as 06's witnesses, rendered again and
// compared with FuSION 1's bytes, after only the changes S5.27 decides.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <iterator>
#include <regex>
#include <set>
#include <tuple>

#include "fixture.hpp"
#include "fusion/results/legacy.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

namespace {

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (const char c : text) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}

std::vector<std::string> lines_of(const std::string& text) {
  auto lines = split(text, '\n');
  if (!lines.empty() && lines.back().empty()) lines.pop_back();
  return lines;
}

// FuSION 1's dump reader, plot_explanation.py:72-129: blocks cut at "Current PO point:", fields 3 and 5
// of the header, and of "Solution 1" only the 6-field node lines and the 11-field edge lines.
struct Parsed {
  std::map<std::pair<int, int>, std::pair<std::size_t, std::size_t>> blocks;   // (n, e) -> nodes, edges
};
Parsed parse_fusion1(const std::string& text) {
  Parsed p;
  std::pair<int, int> po{-1, -1};
  int solution = 0;
  for (const std::string& line : lines_of(text)) {
    if (line.find_first_not_of(" \t") == std::string::npos) continue;
    if (line.rfind("Current PO point:", 0) == 0) {
      std::istringstream words(line);
      std::vector<std::string> parts{std::istream_iterator<std::string>(words), {}};
      po = {std::stoi(parts.at(3)), std::stoi(parts.at(5))};
      p.blocks[po];
      solution = 0;
      continue;
    }
    if (line.rfind("Solution", 0) == 0) {
      solution = std::stoi(split(line, ' ').at(1));
      continue;
    }
    if (solution != 1) continue;
    const auto fields = split(line, '\t');
    if (fields.size() == 6) ++p.blocks[po].first;
    else if (fields.size() >= 11) ++p.blocks[po].second;
  }
  return p;
}

// The toy runs' dumps rebuilt as 06's results: every point's witness 1, nodes named by key.
struct Rebuilt {
  fx::Catalogue cat;
  solve::Results results;
  SolveSpec spec;
};

network::EdgeKey edge_key(const std::vector<std::string>& f) { return ek(f[3], f[5], split(f[2], '_')); }

Rebuilt rebuild(const std::string& solutions, const std::string& relaxations, const std::string& name) {
  Rebuilt out;
  solve::CaseResult c{};
  c.kase = solve::Case::free;   // one positional pathz3 line is one free case (06, positional lines)
  c.status = solve::CaseStatus::complete;
  std::map<std::pair<int, int>, solve::Witness> witness;
  std::vector<std::pair<int, int>> order;
  const auto read = [&](const std::string& text, bool relax) {
    std::pair<int, int> po{-1, -1};
    int solution = 0;
    for (const std::string& line : lines_of(text)) {
      if (line.rfind("Current PO point:", 0) == 0) {
        std::istringstream words(line);
        std::vector<std::string> parts{std::istream_iterator<std::string>(words), {}};
        po = {std::stoi(parts.at(3)), std::stoi(parts.at(5))};
        if (!relax) order.push_back(po);
        solution = 0;
        continue;
      }
      if (line.rfind("Solution", 0) == 0) {
        solution = std::stoi(split(line, ' ').at(1));
        continue;
      }
      if (solution != 1) continue;
      const auto f = split(line, '\t');
      solve::Witness& w = witness[po];
      if (f.size() == 6) {
        const Observed observed = f[3] == "up_reg" ? Observed::up : f[3] == "down_reg" ? Observed::down : Observed::none;
        out.cat.node(f[1], f[2], observed);
        if (relax) w.relaxed_nodes.emplace_back(network::NodeKey{f[1]}, f[4] == "present");
        else (f[5] == "active" ? w.active : w.inactive).push_back(network::NodeKey{f[1]});
      } else if (f.size() >= 11) {
        const auto key = edge_key(f);
        const auto lb = f[10].find('['), rb = f[10].find(']');
        std::vector<std::string> pathways;
        if (rb > lb + 1) pathways = split(f[10].substr(lb + 1, rb - lb - 1), ' ');
        out.cat.edge(key, pathways);
        if (relax) w.relaxed_edges.emplace_back(key, f[1] == "present");
        else w.selected.push_back(key);
      }
    }
  };
  read(solutions, false);
  read(relaxations, true);
  for (const auto& po : order) {
    solve::Witness w = witness[po];
    w.counts = pt(static_cast<int>(w.relaxed_nodes.size()), static_cast<int>(w.relaxed_edges.size()));
    solve::FrontierPoint fp;
    fp.point = pt(po.first, po.second);
    fp.certified = true;
    fp.witness = static_cast<std::uint32_t>(c.witnesses.size());
    c.witnesses.push_back(std::move(w));
    c.frontier.points.push_back(fp);
  }
  std::sort(c.frontier.points.begin(), c.frontier.points.end(),
            [](const solve::FrontierPoint& a, const solve::FrontierPoint& b) { return a.point.n < b.point.n; });
  out.results.cases.push_back(std::move(c));
  out.spec.name = name;
  out.spec.source.value = "hsa1";
  out.spec.target.value = "hsa22";
  return out;
}

// FuSION 1's dump as 2.0 writes it (S5.27): keys in column 1, NA for an absent node's activity, no empty
// "Solution 2" and no "No more solutions" without an enumeration (06.C11), lines in key order.
std::string as_2_0(const std::string& fusion1) {
  std::vector<std::string> out;
  std::vector<std::pair<std::string, std::string>> run;   // (sort key, line)
  const auto flush = [&] {
    std::sort(run.begin(), run.end());
    for (auto& [k, l] : run) out.push_back(l);
    run.clear();
  };
  for (const std::string& line : lines_of(fusion1)) {
    if (line == "Solution 2" || line == "No more solutions. Obtained unsat") continue;
    auto f = split(line, '\t');
    if (f.size() == 6) {
      f[0] = f[1];
      if (f[4] == "not_present") f[5] = "NA";
      std::string l;
      for (std::size_t i = 0; i < f.size(); ++i) l += (i ? "\t" : "") + f[i];
      run.emplace_back(f[1], l);
      continue;
    }
    if (f.size() >= 11) {
      const auto key = edge_key(f);
      f[0] = key.text();
      std::string l;
      for (std::size_t i = 0; i < f.size(); ++i) l += (i ? "\t" : "") + f[i];
      // EdgeKey order: source, target, type, subtypes.
      std::string sort_key = key.source.value + '\x01' + key.target.value + '\x01' + key.type;
      for (const auto& s : key.subtypes) sort_key += '\x01' + s;
      run.emplace_back(sort_key, l);
      continue;
    }
    flush();
    out.push_back(line);
  }
  flush();
  std::string text;
  for (const auto& l : out) text += l + "\n";
  return text;
}

std::filesystem::path synthetic() { return fx::source_dir() / "reference" / "synthetic"; }

}  // namespace

TEST_CASE("FuSION 1's dumps of the toy runs come back byte for byte, with S5.27's fixes", "[unit][results]") {
  std::size_t files = 0, points = 0;
  for (const auto& entry : std::filesystem::directory_iterator(synthetic())) {
    const auto run = entry.path() / "f1" / "run";
    for (const std::string side : {"w", "wo"}) {
      const auto solutions = run / ("temp_" + side + "_solutions.txt");
      if (!std::filesystem::exists(solutions)) continue;
      const std::string fusion1 = fx::read(solutions);
      const std::string fusion1_relax = fx::read(run / ("temp_" + side + "_relaxations.txt"));
      Rebuilt rb = rebuild(fusion1, fusion1_relax, "temp_" + side);
      SolveResult r = build(rb.results, rb.spec, rb.cat);
      INFO(solutions.string());
      // FuSION 1's own witnesses pass every check: shape, the one polarity rule, counts and draw cases.
      const auto issues = analyse(r);
      for (const auto& i : issues) UNSCOPED_INFO(i.message);
      CHECK(issues.empty());
      CHECK(legacy_solutions(r, Case::free) == as_2_0(fusion1));
      CHECK(legacy_relaxations(r, Case::free) == as_2_0(fusion1_relax));
      // FuSION 1's own reader finds the same points and the same line counts in both.
      CHECK(parse_fusion1(legacy_solutions(r, Case::free)).blocks == parse_fusion1(fusion1).blocks);
      CHECK(parse_fusion1(legacy_relaxations(r, Case::free)).blocks == parse_fusion1(fusion1_relax).blocks);
      // The two-column file equals FuSION 1's moved PO.dat (S5.25; its dominance filter never fired).
      for (const auto& f : std::filesystem::directory_iterator(run)) {
        const auto name = f.path().filename().string();
        if (name.ends_with(".dat") && name.find("_" + side + "_") != std::string::npos) {
          CHECK(two_column(r.cases[0]) == fx::read(f.path()));
        }
      }
      ++files;
      points += r.cases[0].points.size();
    }
  }
  CHECK(files >= 20);
  CHECK(points >= 40);
}

TEST_CASE("a case without points gets empty files that exist (S5.26, S6.21)", "[unit][results]") {
  // toy-hsa15: FuSION 1 found no point with the FSC, wrote an empty .dat and no dumps at all.
  const auto run = synthetic() / "toy-hsa15-wni-tsv-b10" / "f1" / "run";
  CHECK_FALSE(std::filesystem::exists(run / "temp_w_solutions.txt"));
  for (const auto& f : std::filesystem::directory_iterator(run)) {
    const auto name = f.path().filename().string();
    if (name.ends_with(".dat") && name.find("_w_") != std::string::npos) CHECK(fx::read(f.path()).empty());
  }
  SolveResult r;
  r.tables.nodes.resize(2);
  r.header.target = 1;
  CaseResult none;
  none.kind = Case::free;
  none.status = CaseStatus::no_point;
  r.cases = {none};
  CHECK(two_column(r.cases[0]).empty());
  CHECK(legacy_solutions(r, Case::free).empty());
  CHECK(legacy_relaxations(r, Case::free).empty());

  fx::Step step("pathz3");
  auto ctx = step.context();
  solve::Results results;
  write_legacy(r, results, "temp_w", ctx);
  CHECK(std::filesystem::exists(step.dir() / "legacy" / "temp_w_solutions.txt"));
  CHECK(std::filesystem::file_size(step.dir() / "legacy" / "temp_w_solutions.txt") == 0);
  CHECK(std::filesystem::exists(step.dir() / "legacy" / "temp_w_relaxations.txt"));
}

namespace {

SolveResult small_result(bool certified) {
  fx::Catalogue cat;
  cat.node("hsa1", "EGFR");
  cat.node("hsa2", "MID", Observed::down);
  cat.node("hsa3", "CXCL8", Observed::up);
  cat.node("hsa4", "GONE", Observed::up);
  const auto a = ek("hsa1", "hsa2", {"inhibition", "dephosphorylation"});
  const auto b = ek("hsa2", "hsa3", {"inhibition"});
  cat.edge(a, {"path:hsa04010", "path:hsa04722"});
  cat.edge(b);
  fx::W w{{"hsa1", "hsa3"}, {"hsa2"}, {a, b}, {{"hsa4", false}}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {w}, certified ? std::vector<std::size_t>{} : std::vector<std::size_t>{0}));
  if (!certified) {
    results.cases[0].frontier.points[0].undecided_below = {pt(0, 0)};
    results.cases[0].frontier.undecided = {pt(0, 0)};
    results.cases[0].log.push_back(fx::answer(pt(0, 0), solve::Phase::smallest_e, solve::Outcome::timeout,
                                              std::nullopt, solve::Source::z3, 14396.2, "timeout"));
  }
  results.cases.push_back(fx::frontier_case(solve::Case::out, {w}));
  SolveSpec spec;
  spec.source.value = "hsa1";
  spec.target.value = "hsa3";
  return build(results, spec, cat);
}

}  // namespace

TEST_CASE("the legacy dump of a hand case, field by field", "[unit][results]") {
  const SolveResult r = small_result(false);
  CHECK(legacy_solutions(r, Case::in) ==
        "Current PO point: 1 nodes, 0 edges relaxed (not certified: blocked by n=0 e=0, TIMEOUT)\n"
        "Solution 1\n"
        "hsa1\thsa1\tEGFR\tnot_diff_expr\tpresent\tactive\n"
        "hsa2\thsa2\tMID\tdown_reg\tpresent\tinactive\n"
        "hsa3\thsa3\tCXCL8\tup_reg\tpresent\tactive\n"
        "\n"
        "hsa1|hsa2|PPrel|inhibition,dephosphorylation\tpresent\tinhibition_dephosphorylation\thsa1\tEGFR\thsa2\tMID\t"
        "src_present\tsrc_active\ttgt_present\ttgt_inactive [path:hsa04010 path:hsa04722]\n"
        "hsa2|hsa3|PPrel|inhibition\tpresent\tinhibition\thsa2\tMID\thsa3\tCXCL8\tsrc_present\tsrc_inactive\t"
        "tgt_present\ttgt_active []\n"
        "\n\n\n");
  CHECK(legacy_relaxations(r, Case::in) ==
        "Current PO point: 1 nodes, 0 edges relaxed (not certified: blocked by n=0 e=0, TIMEOUT)\n"
        "Solution 1\n"
        "hsa4\thsa4\tGONE\tup_reg\tnot_present\tNA\n"
        "\n\n\n\n");
  // FuSION 1's reader still takes fields 3 and 5 of the suffixed header.
  CHECK(parse_fusion1(legacy_solutions(r, Case::in)).blocks.contains({1, 0}));
  CHECK(two_column(r.cases[0]) == "1\t0\n");
}

TEST_CASE("the limits rendering is what the user's comparison scripts read", "[unit][results]") {
  solve::CaseResult c{};
  c.log = {fx::answer(pt(1, 4), solve::Phase::diagonal, solve::Outcome::sat, 0, solve::Source::z3, 0.68651),
           fx::answer(pt(0, 1), solve::Phase::diagonal, solve::Outcome::unsat, std::nullopt, solve::Source::z3, 9.69526),
           fx::answer(pt(0, 4), solve::Phase::smallest_n, solve::Outcome::unsat, std::nullopt, solve::Source::cache, 0),
           fx::answer(pt(7, 0), solve::Phase::smallest_e, solve::Outcome::timeout, std::nullopt, solve::Source::z3, 14396.2, "timeout"),
           fx::answer(pt(2, 6), solve::Phase::smallest_n, solve::Outcome::undefined, std::nullopt, solve::Source::z3, 3.5, "incomplete"),
           fx::answer(pt(1, 0), solve::Phase::extra, solve::Outcome::sat, 1, solve::Source::z3, 2)};
  const std::string text = legacy_limits(c);
  CHECK(text ==
        "Relax bounds: 4 1\nTime taken (sec): 0.68651\nsat\n"
        "Relax bounds: 1 0\nTime taken (sec): 9.69526\nunsat\n"
        "Point 4 0 already unsat\nunsat\n"
        "Relax bounds (edge call): 0 7\nTime taken (sec): 14396.2\n"
        "TIMEOUT (smallest-e cache) at point e=0 n=7 (z3 reason: timeout). Consider increasing the solver timeout "
        "($CONSTR_SOLVER_TIMEOUT1/$CONSTR_SOLVER_TIMEOUT2 in the batch file).\n"
        "unknown\n"
        "TIMEOUT (smallest-e steering) at point e=0 n=7 (z3 reason: timeout). Consider increasing the solver timeout "
        "($CONSTR_SOLVER_TIMEOUT1/$CONSTR_SOLVER_TIMEOUT2 in the batch file).\n"
        "Relax bounds (node call): 6 2\nTime taken (sec): 3.5\n"
        "UNDEFINED (smallest-n cache; z3::unknown, reason: incomplete) at point e=6 n=2\n"
        "unknown\n"
        "UNDEFINED (smallest-n steering; z3::unknown, reason: incomplete) at point e=6 n=2\n");
  // compare_candidates.py's call_outcomes() and compare_sweep.py's parse_limits() patterns.
  const std::regex call(R"(^Relax bounds(?: \((?:node|edge) call\))?: (\d+) (\d+)$)");
  const std::regex time(R"(^Time taken \(sec\): ([0-9.eE+-]+)$)");
  std::vector<std::tuple<int, int, double, std::string>> calls;
  std::optional<std::tuple<int, int, double, std::string>> cur;
  for (const std::string& line : lines_of(text)) {
    std::smatch m;
    if (std::regex_match(line, m, call)) {
      cur = std::make_tuple(std::stoi(m[1]), std::stoi(m[2]), -1.0, std::string());
    } else if (cur && std::regex_match(line, m, time)) {
      std::get<2>(*cur) = std::stod(m[1]);
    } else if (cur && std::get<2>(*cur) >= 0 && (line == "sat" || line == "unsat" || line == "unknown")) {
      std::get<3>(*cur) = line;
      calls.push_back(*cur);
      cur.reset();
    }
  }
  REQUIRE(calls.size() == 4);   // Z3 checks only: no cache hit, no extra-solution check
  CHECK(calls[2] == std::make_tuple(0, 7, 14396.2, std::string("unknown")));
  const std::regex undecided(R"(Time taken \(sec\): ([0-9.eE+-]+)\n(?:TIMEOUT|UNDEFINED) \([^)]*\) at point e=(\d+) n=(\d+))");
  CHECK(std::distance(std::sregex_iterator(text.begin(), text.end(), undecided), std::sregex_iterator()) == 2);
}

TEST_CASE("legacy_prefix writes in pathz3/legacy/ and at an absolute stem (1.21, 2B.3)", "[unit][results]") {
  const SolveResult r = small_result(true);
  fx::Step step("pathz3");
  auto ctx = step.context();
  solve::Results results;
  results.cases.resize(2);
  results.cases[0].kase = solve::Case::in;
  results.cases[0].log = {fx::answer(pt(1, 0), solve::Phase::diagonal, solve::Outcome::sat, 0, solve::Source::z3, 0.25)};
  results.cases[1].kase = solve::Case::out;
  const auto stem = step.root() / "write" / "temp_w";
  std::filesystem::create_directories(stem.parent_path());
  write_legacy(r, results, core::to_utf8(stem), ctx);
  // Two cases: <P>_<case>_solutions.txt and so on.
  for (const char* name : {"temp_w_in_solutions.txt", "temp_w_in_relaxations.txt", "temp_w_out_solutions.txt",
                           "temp_w_out_relaxations.txt"}) {
    CHECK(std::filesystem::exists(step.dir() / "legacy" / name));
    CHECK(fx::read(step.dir() / "legacy" / name) == fx::read(stem.parent_path() / name));
  }
  CHECK(step.file("legacy/temp_w_in_solutions.txt") == legacy_solutions(r, Case::in));
  CHECK(step.file("legacy/temp_w_in_limits_timefile.txt") == "Relax bounds: 0 1\nTime taken (sec): 0.25\nsat\n");
  CHECK(fx::read(step.dir() / "legacy" / "temp_w_in_solutions.txt").find(step.root().string()) == std::string::npos);
}
