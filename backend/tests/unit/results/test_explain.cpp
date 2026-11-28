// results: the explanation export: view files, page shells, the library copy, edge tables, the static
// DOT export and the run page (section 07, test hooks "View files", "Text formats"; 07.C2, S6.07, S6.08,
// S6.17, S6.18).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fixture.hpp"
#include "fusion/results/assets.hpp"
#include "fusion/results/explain.hpp"

using namespace fusion;
using namespace fusion::results;
using fx::ek;
using fx::pt;
using Catch::Matchers::ContainsSubstring;

namespace {

// EGFR -> JUN -> CXCL8 with JUN as the FSC; a symbol with markup, quotes and a tab in it, an ECrel edge.
SolveResult explained() {
  fx::Catalogue cat;
  cat.node("hsa1956", "EGFR");
  cat.node("hsa3725", "JUN</script><b>\"x\\y\"", Observed::up, 1.39);
  cat.node("hsa3576", "CXCL8", Observed::up);
  cat.node("hsa8660", "IRS2", Observed::up, 1.39);
  cat.node("hsa9", "TAB\tBED");
  const auto a = ek("hsa1956", "hsa3725", {"activation"});
  const auto b = ek("hsa3725", "hsa3576", {"expression"});
  const auto c = ek("hsa1956", "hsa9", {"compound"}, "ECrel");
  const auto d = ek("hsa9", "hsa3576", {"activation"});
  cat.edge(a, {"path:hsa04010"});
  cat.edge(b, {"path:hsa04010", "path:hsa05120"});
  cat.edge(c, {"path:hsa00010"});
  cat.edge(d);
  cat.pathway("path:hsa04010", "MAPK signaling pathway");
  cat.pathway("path:hsa05120", "Line\nbreak;and\ttab");
  fx::W w{{"hsa1956", "hsa3725", "hsa3576", "hsa9"}, {}, {a, b, c, d}, {{"hsa8660", false}}, {}};
  solve::Results results;
  results.cases.push_back(fx::frontier_case(solve::Case::in, {w}));
  solve::CaseResult out{};
  out.kase = solve::Case::out;
  out.status = solve::CaseStatus::no_point;
  results.cases.push_back(out);
  SolveSpec spec;
  spec.source.value = "hsa1956";
  spec.target.value = "hsa3576";
  spec.fsc = {network::NodeKey{"hsa3725"}};
  SolveResult r = build(results, spec, cat);
  REQUIRE(analyse(r).empty());
  return r;
}

}  // namespace

TEST_CASE("a symbol cannot close the page's script element (07.C2)", "[unit][results]") {
  const SolveResult r = explained();
  const PointView view = build_view(r, r.cases[0], r.cases[0].points[0]);
  const std::string json = view_json(view, r);
  CHECK_THAT(json, ContainsSubstring("JUN</script>"));   // the view file keeps the text as it is
  const std::string html = page_html(json, "../lib/", view.title, "in_n1_e0.json");
  const auto body = html.substr(html.find("id=\"fusion-view\">"));
  CHECK(body.find("</script>") == body.find("</script>\n<noscript>"));   // only the element's own end tag
  CHECK_THAT(html, ContainsSubstring("JUN\\u003c/script>\\u003cb>"));
  CHECK_THAT(html, ContainsSubstring("<title>EGFR → CXCL8 · with FSC (JUN&lt;/script&gt;&lt;b&gt;&quot;x\\y&quot;) · n 1, e 0</title>"));
  CHECK_THAT(html, ContainsSubstring("<script src=\"../lib/vis-network.min.js\"></script>"));
  CHECK_THAT(html, ContainsSubstring("<script src=\"../lib/fusion-explain.js\" defer></script>"));
  CHECK_THAT(html, ContainsSubstring("The same explanation is in in_n1_e0.json."));
  CHECK(html.find("http") == std::string::npos);   // fetches nothing (S6.18)
  // Deterministic: the same view gives the same bytes.
  CHECK(view_json(build_view(r, r.cases[0], r.cases[0].points[0]), r) == json);
  const auto parsed = core::Json::parse(json);
  CHECK(parsed["format"] == "fusion.view");
  CHECK(parsed["title"] == view.title);
  CHECK(parsed["observations"].empty());
  CHECK(parsed["witnesses"][0]["parked"][0]["key"] == "hsa8660");
  CHECK(parsed["witnesses"][0]["parked"][0]["lfc"] == 1.39);
  CHECK(parsed["pathways"]["path:hsa04010"] == "MAPK signaling pathway");
}

TEST_CASE("the edge table has one row per drawn edge and no tab or line break in a field (S6.08)",
          "[unit][results]") {
  const SolveResult r = explained();
  const WitnessView v = build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]);
  const std::string tsv = edge_table_tsv(v, r, PathFilter{});
  std::vector<std::string> rows;
  std::istringstream lines(tsv);
  for (std::string line; std::getline(lines, line);) rows.push_back(line);
  REQUIRE(rows.size() == 5);   // header and four drawn edges
  for (const auto& row : rows) CHECK(std::count(row.begin(), row.end(), '\t') == 11);
  CHECK(rows[0] == "source\tsource_key\ttarget\ttarget_key\teffect\tdraw\tin_witness\ttypes\tsubtypes\tedge_keys\t"
                   "pathway_ids\tpathway_names");
  CHECK_THAT(tsv, ContainsSubstring("EGFR\thsa1956\tTAB BED\thsa9\tactivation\thonoured\tyes\tECrel\tcompound\t"
                                    "hsa1956|hsa9|ECrel|compound\tpath:hsa00010\tpath:hsa00010"));
  CHECK_THAT(tsv, ContainsSubstring("path:hsa04010;path:hsa05120\tMAPK signaling pathway;Line break;and tab"));
}

TEST_CASE("the static export quotes \" and \\ and draws the page's styles (S6.07)", "[unit][results]") {
  const SolveResult r = explained();
  const WitnessView v = build_witness_view(r, r.cases[0], r.cases[0].points[0].witnesses[0]);
  const std::string dot = dot_source(v, r, PathFilter{}, "in_n1_e0");
  CHECK(dot.rfind("digraph \"in_n1_e0\" {\n  rankdir=LR;\n", 0) == 0);
  CHECK_THAT(dot, ContainsSubstring("\"hsa3725\" [label=\"JUN</script><b>\\\"x\\\\y\\\"\", fillcolor=\"#8CCB4E\"];"));
  CHECK_THAT(dot, ContainsSubstring("\"hsa1956\" [label=\"EGFR\", color=\"#1B5E20\", penwidth=4];"));
  CHECK_THAT(dot, ContainsSubstring("\"hsa3576\" [label=\"CXCL8\", fillcolor=\"#8CCB4E\", color=\"#0D3B66\", penwidth=4];"));
  CHECK_THAT(dot, ContainsSubstring("\"hsa1956\" -> \"hsa9\" [color=\"#8a8a8a\", arrowhead=normal, penwidth=2, label=\"EC\"];"));
  CHECK_THAT(dot, ContainsSubstring("\"hsa9\" [label=\"TAB BED\"];"));
  CHECK(dot.find("hsa8660") == std::string::npos);   // parked, not drawn
}

TEST_CASE("pex writes one view and one shell per point, and the library once (07.C2, S6.18)", "[unit][results]") {
  const SolveResult r = explained();
  fx::Step step("pex");
  auto ctx = step.context();
  ExplainOptions options;
  options.edge_table = true;
  const auto abs = step.root() / "write" / "pex";
  std::filesystem::create_directories(abs.parent_path());
  options.prefix = "pex";
  options.absolute_prefix = abs;
  const SolveResult* solves[] = {&r};
  explain(solves, options, ctx);
  const auto dir = step.dir();
  CHECK(std::filesystem::exists(dir / "explain" / "pex_in_n1_e0.json"));
  CHECK(std::filesystem::exists(dir / "explain" / "pex_in_n1_e0.html"));
  CHECK(std::filesystem::exists(dir / "explain" / "pex_in_n1_e0_edges.tsv"));
  CHECK_FALSE(std::filesystem::exists(dir / "explain" / "pex_out_n0_e0.html"));
  for (const PageAsset& asset : page_assets()) {
    CHECK(fx::read(dir / "lib" / std::string(asset.name)) == asset.bytes);
    // An absolute out_prefix gets its shells and a library beside it, the step directory a copy (2B.3).
    CHECK(fx::read(abs.parent_path() / "pex_lib" / std::string(asset.name)) == asset.bytes);
  }
  const std::string shell = fx::read(abs.parent_path() / "pex_in_n1_e0.html");
  CHECK_THAT(shell, ContainsSubstring("src=\"pex_lib/vis-network.min.js\""));
  CHECK(fx::read(dir / "pex_in_n1_e0.html") == shell);
  CHECK(step.file("explain/pex_in_n1_e0.html").find(step.root().string()) == std::string::npos);
  CHECK(step.file("explain/pex_in_n1_e0.json").find(step.root().string()) == std::string::npos);
  // The console counts what is drawn (S6.09, S6.20), and a case without points is a result (S6.21).
  const auto& lines = step.lines();
  CHECK(std::find(lines.begin(), lines.end(),
                  "pex: with FSC (JUN</script><b>\"x\\y\") n=1 e=0 -> pex/explain/pex_in_n1_e0.html (4 nodes, 4 edges; "
                  "1 relaxed node not drawn)") != lines.end());
  CHECK(std::find(lines.begin(), lines.end(),
                  "pex: without FSC (JUN</script><b>\"x\\y\"): no Pareto point within the bounds; nothing to draw") !=
        lines.end());

  ExplainOptions missing;
  missing.points = parse_point_selector("7,1");
  fx::Step other("pex");
  auto octx = other.context();
  try {
    explain(solves, missing, octx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "results.no_such_point");
    CHECK_THAT(std::string(f.what()), ContainsSubstring("has no point n=7 e=1; its points are n=1 e=0"));
  }
}

TEST_CASE("the run page lists each case's points in FuSION 1's order and links their pages", "[unit][results]") {
  const SolveResult r = explained();
  RunPageInputs in;
  in.solves = {&r};
  in.pages = {"pex/explain/in_n1_e0.html"};
  in.plots = {"plot_pareto/pareto.png"};
  const std::string html = run_page_html(in);
  CHECK(html.rfind("<!doctype html>", 0) == 0);
  CHECK(html.find("<script") == std::string::npos);   // static, no script
  CHECK_THAT(html, ContainsSubstring("<img src=\"plot_pareto/pareto.png\""));
  CHECK_THAT(html, ContainsSubstring("href=\"pex/explain/in_n1_e0.html\""));
  CHECK_THAT(html, ContainsSubstring("JUN&lt;/script&gt;"));
  CHECK_THAT(html, ContainsSubstring("no Pareto point within the bounds"));
  CHECK_THAT(html, ContainsSubstring("href=\"pathz3/results.json\""));
  CHECK_THAT(html, ContainsSubstring("is essential: there is no point without it"));
  CHECK(page_stem("pex", "temp_w", Case::out, pt(2, 7)) == "pex_temp_w_out_n2_e7");
  CHECK(page_stem("", "", Case::in, pt(1, 0)) == "in_n1_e0");
}

TEST_CASE("the run page opens with the run's name and how it ended", "[unit][results]") {
  // Phase 3, prompt log entry 387: a failed run, or one with no solve, still says something.
  const SolveResult r = explained();
  RunPageInputs done;
  done.solves = {&r};
  done.run_name = "toy_20261128-130638";
  const std::string finished = run_page_html(done);
  CHECK_THAT(finished, ContainsSubstring("<p class=\"sub\">toy_20261128-130638 · finished</p>"));
  CHECK_THAT(finished, ContainsSubstring("1 node · 0 edges relaxed"));   // not "1 nodes"
  CHECK(finished.find("class=\"end\"") == std::string::npos);
  CHECK(finished.find("no solve") == std::string::npos);
  CHECK(finished.find("No solve") == std::string::npos);

  RunPageInputs failed;
  failed.run_name = "toy_20261128-130639";
  failed.end = RunEnd::failed;
  failed.end_note = "line 3, fb_rch: hsa999 is not <in> the network";
  const std::string html = run_page_html(failed);
  CHECK_THAT(html, ContainsSubstring("toy_20261128-130639 · <span class=\"bad\">failed</span>"));
  CHECK_THAT(html, ContainsSubstring("<p class=\"end\">line 3, fb_rch: hsa999 is not &lt;in&gt; the network</p>"));
  CHECK_THAT(html, ContainsSubstring("No solve finished before the run ended. Its files are in this folder."));
  CHECK(html.find("<script") == std::string::npos);

  RunPageInputs stopped;
  stopped.end = RunEnd::stopped;
  CHECK_THAT(run_page_html(stopped), ContainsSubstring("<p class=\"sub\"><span class=\"bad\">stopped on request</span></p>"));
  CHECK_THAT(run_page_html(RunPageInputs{}),
             ContainsSubstring("This run has no solve, so there is no frontier to show. Its files are in this folder."));
}
