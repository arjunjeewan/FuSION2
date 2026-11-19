// The legacy reader (02.C5; S2.27, S2.12, S2.08, S2.03, S2.13, S4.13): FuSION 1's graph 2 from a file
// FuSION 1 wrote, checked against FuSION 1's own outputs on the relabelled toy networks (booking 1's
// regenerated references) and against a synthetic master built to exercise every rule.
#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "support.hpp"

using namespace network_test;
using net::BindStatus;
using net::EdgeFlag;

namespace {

net::IdSet exception_file(const fs::path& p) {            // S1.17: the first token of each line
  std::vector<std::string> ids;
  std::istringstream in(read_file(p));
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string first;
    if (words >> first) ids.push_back(first);
  }
  return net::IdSet(std::move(ids));
}

std::vector<std::string> edge_lines(const net::Network& n) {   // keys and pathways, no scores
  std::vector<std::string> out;
  for (net::EdgeIdx e = 0; e < n.edges().size(); ++e) {
    std::string line = n.edge_key(e).text() + " [";
    for (core::Sym p : n.pathways(e)) (line += n.interner().str(p)) += ' ';
    out.push_back(line + "]");
  }
  return out;
}

std::vector<std::string> members(const net::Network& n, const std::string& key) {
  std::vector<std::string> out;
  for (core::GeneId m : n.members(*n.find_node({key}))) out.emplace_back(n.interner().str(m));
  return out;
}

}  // namespace

TEST_CASE("network: the legacy reader gives FuSION 1's graph 2 on the regenerated toy cases (02.C5)", "[unit][network]") {
  // Three relabelled toy networks, each read by FuSION 1 with `mff <list> xml 1 y`, written by wgx and
  // re-read by `rgx … y`; the console printed graph 2's size, and the capture holds graph 2 itself.
  for (const char* name : {"toy-hsa13-wni-tsv-b10", "toy-hsa11-report-tsv-b10", "toy-hsa9-wni-tsv-b10"}) {
    INFO(name);
    Context c;
    const fs::path dir = reference(std::string("synthetic/") + name);
    fs::path input;
    for (const auto& f : fs::directory_iterator(dir / "inputs"))
      if (f.path().extension() == ".xml") input = f.path();
    REQUIRE(!input.empty());
    CHECK(net::sniff(input, true, c.ctx).format == net::Format::legacy);
    const net::IdSet e = exception_file(dir / "f1/run/exception_file.txt");
    const auto n = net::read_legacy(input, net::IdMap::none(), {}, e, c.ctx);
    CHECK(n->info().legacy);
    CHECK(n->info().reader == "legacy");
    const auto lines = net::summary_lines(net::summarize(net::GraphView(n)), net::GraphView(n), {});
    const std::string console = read_file(dir / "f1/run_console.log");
    const auto at = console.find("Read graph: 2 from file");
    REQUIRE(at != std::string::npos);
    CHECK(console.find(lines[0], at) != std::string::npos);   // "Number of nodes: 22 (22 genes, ...) 0 isolated"
    CHECK(console.find(lines[1], at) != std::string::npos);   // "Number of edges: 25"
    // FuSION 1's own graph 2, written out, has the same edges, by key and pathways.
    Context r;
    const auto theirs = net::read_legacy(dir / "f1/capture/graph_reread.xml", net::IdMap::none(), {}, e, r.ctx);
    CHECK(edge_lines(*n) == edge_lines(*theirs));
    for (net::NodeIdx i = 0; i < n->nodes().size(); ++i) CHECK(n->node_key(i) == theirs->node_key(i));
  }
}

TEST_CASE("network: a synthetic master: first claims, S2.08's loss, the replay and the duplicate merge (02.C5)", "[unit][network]") {
  Context c;
  const auto n = net::read_legacy(data("legacy_master.xml"), net::IdMap::none(), {}, net::IdSet(), c.ctx);
  std::vector<std::string> keys;
  for (net::NodeIdx i = 0; i < n->nodes().size(); ++i) keys.push_back(n->node_key(i).value);
  CHECK(keys == std::vector<std::string>{"c_s_t13_t7_c_e", "t1", "t10", "t13", "t2", "t3", "t5", "t7"});
  CHECK(members(*n, "t2") == std::vector<std::string>{"t2", "t3", "t4"});
  CHECK(members(*n, "t5") == std::vector<std::string>{"t5", "t9"});   // t6 lost: S2.08, kept in legacy reads
  CHECK(members(*n, "t10") == std::vector<std::string>{"t10", "t11"});   // made by a box reached only indirectly
  CHECK_FALSE(n->find_node({"t8"}));                        // its only relation was indirect: dropped by the replay
  CHECK_FALSE(n->find_node({"t14"}));
  CHECK(edge_lines(*n) == std::vector<std::string>{
                              "c_s_t13_t7_c_e|t1|PPrel|association [path:a ]",   // one-way, no component edges
                              "t10|t7|PPrel|activation [path:c ]",
                              "t2|t1|PPrel|activation [path:a path:c ]",         // the duplicate, merged (S4.13)
                              "t3|t7|PPrel|activation [path:a ]",
                              "t5|t1|PPrel|activation [path:b ]",
                              "t5|t7|PPrel|inhibition [path:b ]",
                              "t7|t1|PPrel|activation [path:a ]"});
  const auto merged = *n->find_edge({{"t2"}, {"t1"}, "PPrel", {"activation"}});
  CHECK(n->edges()[merged].score == 9.0);
  for (const net::Edge& e : n->edges()) {
    CHECK(has_flag(e, EdgeFlag::legacy));
    CHECK_FALSE(has_flag(e, EdgeFlag::undirected));
    CHECK_FALSE(has_flag(e, EdgeFlag::component));
  }
  CHECK(has_flag(n->edges()[*n->find_edge({{"t7"}, {"t1"}, "PPrel", {"activation"}})], EdgeFlag::dummy_u_to_d));
  const auto* ev = c.events.find("network.read");
  REQUIRE(ev != nullptr);
  CHECK((*ev)["format"] == "legacy");
  CHECK((*ev)["edges_before_merge"] == 8);
  CHECK((*ev)["edges"] == 7);
  const net::Summary s = net::summarize(net::GraphView(n));
  CHECK(s.isolated == 1);                                   // t13: a complex's component, with no component edges
  CHECK(s.complexes == 1);

  // S2.03: t3 sits in the t2 box and alone; its first claim is the box (the RAS trap).
  net::BindingTable table(n, {});
  const net::Binding& t3 = table.bind("essential", "t3");
  CHECK(t3.status == BindStatus::ambiguous);
  CHECK(n->node_key(t3.node).value == "t2");
  CHECK(t3.holders == 2);
  CHECK(table.bind("up", "t2").status == BindStatus::family);
  CHECK(table.bind("up", "t11").status == BindStatus::family);
  CHECK(table.bind("up", "t1").status == BindStatus::own);
  for (const char* lost : {"t6", "t12", "t8", "t14"}) CHECK(table.bind("up", lost).status == BindStatus::absent);
}

TEST_CASE("network: legacy reads follow the run's exception set, threshold and filters (S2.13, S2.07, S2.11)", "[unit][network]") {
  Context c;
  const auto e = net::read_legacy(data("legacy_master.xml"), net::IdMap::none(), {}, net::IdSet({"t3"}), c.ctx);
  CHECK(members(*e, "t2") == std::vector<std::string>{"t2", "t4"});
  CHECK(has_flag(e->nodes()[*e->find_node({"t3"})], net::NodeFlag::exception));
  net::BindingTable table(e, {});
  CHECK(table.bind("fsc", "t3").status == BindStatus::own);   // a known FSC gets its own node (entry 164)
  CHECK(e->info().exceptions == std::vector<std::string>{"t3"});

  const auto flat = net::read_legacy(data("legacy_master.xml"), net::IdMap::none(), {-1, {}}, net::IdSet(), c.ctx);
  CHECK(members(*flat, "t5") == std::vector<std::string>{"t5"});
  CHECK(flat->find_node({"t6"}));                           // no box, no loss
  CHECK(flat->find_node({"t12"}));
  CHECK(flat->find_edge({{"t4"}, {"t1"}, "PPrel", {"activation"}}));   // fan-out over the singles

  const auto kept = net::read_legacy(data("legacy_master.xml"), net::IdMap::none(),
                                     {1, {net::IndirectMode::keep, net::EcrelMode::keep, net::MaplinkMode::skip}}, net::IdSet(), c.ctx);
  const auto ind = kept->find_edge({{"t8"}, {"t1"}, "PPrel", {"activation", "indirect"}});
  REQUIRE(ind);
  CHECK(has_flag(kept->edges()[*ind], EdgeFlag::indirect));
  CHECK(kept->find_edge({{"t10"}, {"t7"}, "PPrel", {"indirect"}}));
}

TEST_CASE("network: a legacy network written and loaded keeps its claim order and content (2A.4)", "[unit][network]") {
  Context c;
  const auto n = net::read_legacy(data("legacy_master.xml"), net::IdMap::none(), {}, net::IdSet(), c.ctx);
  std::ostringstream out;
  net::write_fusion_network(net::GraphView(n), out, {}, {});
  CHECK(out.str().find("legacy=\"true\"") != std::string::npos);
  CHECK(out.str().find("<claim id=\"t3\" entries=\"5 6\"") != std::string::npos);
  const auto back = net::read_fusion_network(c.scratch.write("legacy.xml", out.str()), c.ctx);
  CHECK(back->info().legacy);
  CHECK(back->content_hash() == n->content_hash());
  CHECK(dump(*back) == dump(*n));
  const auto h = back->ids().holders(*back->interner().find_id("t3"));
  REQUIRE(h.size() == 2);
  CHECK(back->node_key(h[0]).value == "t2");
  CHECK(net::saved_network_diff(back->info(), {}, net::IdSet(), net::IdMap::none()).empty());
  CHECK_FALSE(net::saved_network_diff(back->info(), {}, net::IdSet({"t3"}), net::IdMap::none()).empty());
}

TEST_CASE("network: legacy files get S2.06's checks too", "[unit][network]") {
  Context c;
  const auto p = c.scratch.write("bad.xml", "<pathway name=\"KEGG\"><entry id=\"1\" name=\"t1\" type=\"gene\"/>"
                                            "<relation entry1=\"1\" entry2=\"9\" type=\"PPrel\" pathways=\"p\"/></pathway>");
  try {
    net::read_legacy(p, net::IdMap::none(), {}, net::IdSet(), c.ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "network.dangling_ref");
  }
}
