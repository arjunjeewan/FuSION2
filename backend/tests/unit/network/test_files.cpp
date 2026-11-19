// FuSION's network file (02.C4, S2.25, S2.26, 2A.4), add's transcription (S2.30, 02.C17), sz's
// summary (S2.28) and the labels query (2B.6).
#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "fusion/core/build_info.hpp"
#include "support.hpp"

using namespace network_test;
using net::EdgeFlag;

namespace {

std::string write(const net::GraphView& view, const net::Maps& maps, net::WriteOptions options = {}) {
  std::ostringstream out;
  net::write_fusion_network(view, out, maps, options);
  return out.str();
}

std::string failure_code(const std::function<void()>& f) {
  try {
    f();
  } catch (const core::Failure& e) {
    return e.code();
  }
  return "none";
}

std::shared_ptr<const net::Network> tst_network(Context& c, bool embed = true) {
  const auto doc = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  return build_one(*doc, {}, net::IdSet({"t4"}), c.ctx, embed ? doc : nullptr);
}

}  // namespace

TEST_CASE("network: write then read is the identity on the canonical form (02.C4, S2.25, S2.26)", "[unit][network]") {
  Context c;
  net::Maps maps;
  maps.symbols = net::LabelMap::read(data("symbols.tsv"), c.ctx);
  const auto n = tst_network(c);
  const std::string text = write(net::GraphView(n), maps);
  CHECK(text.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">\n", 0) == 0);
  CHECK(text.find("<!DOCTYPE") == std::string::npos);
  CHECK(text.find("\r") == std::string::npos);
  CHECK(text.find("<setting name=\"network.split_threshold\" value=\"1\"") != std::string::npos);
  CHECK(text.find("<setting name=\"network.indirect\" value=\"discard\"") != std::string::npos);
  CHECK(text.find("<exceptions ids=\"t4\"") != std::string::npos);
  CHECK(text.find("title=\"Test signalling pathway\"") != std::string::npos);
  // Entries in canonical order from 1, members sorted, an explicit key, labels from the symbol map.
  CHECK(text.find("<entry id=\"1\" key=\"c_s_t2_t6_c_e\" name=\"c_s_t2_t6_c_e\" type=\"group\">") != std::string::npos);
  CHECK(text.find("<graphics name=\"[BBB+1+FFF]\"") != std::string::npos);
  CHECK(text.find("<entry id=\"4\" key=\"t10\" name=\"t10 t5\" type=\"gene\">") != std::string::npos);
  CHECK(text.find("<graphics name=\"t10, EEE\"") != std::string::npos);
  CHECK(text.find("<entry id=\"7\" key=\"t4\" name=\"t4\" type=\"gene\" flags=\"exception\">") != std::string::npos);
  // A whole score as an integer; an unscored edge has no experiment_score; the tag as a subtype.
  CHECK(text.find("type=\"PPrel\" pathways=\"tst00001\" experiment_score=\"700\">") != std::string::npos);
  CHECK(text.find("<relation entry1=\"4\" entry2=\"9\" type=\"GErel\" pathways=\"tst00001\">") != std::string::npos);
  CHECK(text.find("experiment_score=\"300\" flags=\"undirected\">\n    <subtype name=\"association\" />\n"
                  "    <subtype name=\"dummy_u_to_d\" />") != std::string::npos);
  CHECK(text.find("<source pathway=\"tst00001\" title=\"Test signalling pathway\"") != std::string::npos);
  CHECK(text.find("ids=\"t11\"") == std::string::npos);       // reached by no relation: not embedded
  CHECK(text.find("<e id=\"6\" type=\"group\" ids=\"undefined\" comps=\"4 2\" />") != std::string::npos);

  Context r;
  const auto path = r.scratch.write("network.xml", text);
  const auto back = net::read_fusion_network(path, r.ctx);
  CHECK(dump(*back) == dump(*n));
  CHECK(back->content_hash() == n->content_hash());
  CHECK(back->info().settings == n->info().settings);
  CHECK(back->info().exceptions == std::vector<std::string>{"t4"});
  REQUIRE(back->info().sources.size() == 1);
  CHECK(back->info().sources[0].title == "Test signalling pathway");
  CHECK(back->info().sources[0].sha256 == n->info().sources[0].sha256);
  CHECK(back->info().sources[0].size == n->info().sources[0].size);
  CHECK(back->info().reader == "kgml");
  CHECK_FALSE(back->info().legacy);
  CHECK(back->pathway_label("tst00001", maps) == "Test signalling pathway");
  REQUIRE(back->provenance() != nullptr);
  REQUIRE(back->provenance()->docs.size() == 1);
  const auto& doc = *back->provenance()->docs[0];
  CHECK(doc.relations().size() == 11);                      // every relation, so any setting can rebuild
  CHECK(doc.entries().size() == 11);                        // every entry a relation reaches
  CHECK(doc.source().sha256 == n->info().sources[0].sha256);
  CHECK(doc.source().path == n->info().sources[0].path);
  // The embedded document rebuilds the same network, and another under other settings.
  CHECK(build_one(doc, {}, net::IdSet({"t4"}), r.ctx)->content_hash() == n->content_hash());
  CHECK(build_one(doc, {1, {net::IndirectMode::keep, net::EcrelMode::keep, net::MaplinkMode::skip}}, net::IdSet({"t4"}), r.ctx)
            ->find_node({"t8"}));
  // Byte-identical files: written twice, and written again after a read.
  CHECK(write(net::GraphView(n), maps) == text);
  CHECK(write(net::GraphView(back), maps) == text);
  CHECK(r.events.find("network.read") != nullptr);
}

TEST_CASE("network: attribute values are escaped, and scores read back exactly (S2.25)", "[unit][network]") {
  Context c;
  core::Interner in;
  net::NetworkInfo info;
  info.reader = "kgml";
  info.id_map = core::Sha256::of("a map");
  info.merge = net::MergeHeader{"b41c", true, 1};
  net::NetworkBuilder b(info, in);
  const auto x = b.add_node(in.sym("gene"), std::vector<core::GeneId>{in.id("a&b"), in.id("c<d")});
  const auto y = b.add_node(in.sym("gene"), std::vector<core::GeneId>{in.id("e\"f")});
  b.add_edge(x, y, in.sym("PPrel"), std::vector<core::Sym>{in.sym("activation")}, std::vector<core::Sym>{in.sym("p\"q&")},
             0.1, net::bit(EdgeFlag::legacy));
  b.add_edge(y, x, in.sym("PPrel"), std::vector<core::Sym>{}, std::vector<core::Sym>{}, 1e-05, 0);
  b.add_edge(y, y, in.sym("PPrel"), std::vector<core::Sym>{}, std::vector<core::Sym>{}, -3.0, 0);
  const auto n = std::make_shared<net::Network>(std::move(b).finish(c.ctx));
  net::Maps maps;
  maps.symbols = net::LabelMap::read(c.scratch.write("s.tsv", "a&b\tX<Y\n"), c.ctx);
  const std::string text = write(net::GraphView(n), maps);
  CHECK(text.find("name=\"a&amp;b c&lt;d\"") != std::string::npos);
  CHECK(text.find("<graphics name=\"X&lt;Y, c&lt;d\"") != std::string::npos);
  CHECK(text.find("pathways=\"p&quot;q&amp;\" experiment_score=\"0.1\" flags=\"legacy\"") != std::string::npos);
  CHECK(text.find("experiment_score=\"1e-05\"") != std::string::npos);
  CHECK(text.find("experiment_score=\"-3\"") != std::string::npos);
  CHECK(text.find("<merge key=\"b41c\" base=\"true\" rules=\"1\"") != std::string::npos);
  CHECK(text.find("<id_map sha256=\"" + info.id_map.hex() + "\"") != std::string::npos);
  const auto back = net::read_fusion_network(c.scratch.write("n.xml", text), c.ctx);
  CHECK(back->content_hash() == n->content_hash());
  CHECK(back->edges()[0].score == 0.1);
  CHECK(back->info().id_map == info.id_map);
  REQUIRE(back->info().merge);
  CHECK(back->info().merge->key == "b41c");
  CHECK(back->info().merge->base);
}

TEST_CASE("network: a store entry leaves input paths out; embed_sources=false leaves the sources out (03.C4)", "[unit][network]") {
  Context c;
  const auto n = tst_network(c);
  const std::string full = write(net::GraphView(n), {});
  CHECK(full.find("<input path=") != std::string::npos);
  const std::string store = write(net::GraphView(n), {}, {"KEGG", true, true});
  CHECK(store.find("<input path=") == std::string::npos);
  CHECK(store.find("<input sha256=") != std::string::npos);
  const std::string bare = write(net::GraphView(n), {}, {"MYTAG", false, false});
  CHECK(bare.find("<source") == std::string::npos);
  CHECK(bare.find("<pathway name=\"MYTAG\"") != std::string::npos);   // wgx's tag (S2.25)
  CHECK(net::read_fusion_network(c.scratch.write("bare.xml", bare), c.ctx)->content_hash() == n->content_hash());
}

TEST_CASE("network: a view is written with the components it lacks marked in_view=\"false\" (02.C4)", "[unit][network]") {
  Context c;
  const auto n = tst_network(c, false);
  net::Bitset nodes(n->nodes().size(), true), edges(n->edges().size(), true);
  nodes.reset(*n->find_node({"t6"}));
  const std::string text = write(net::GraphView(n, nodes, edges), {});
  CHECK(text.find("key=\"t6\" name=\"t6\" type=\"gene\" in_view=\"false\"") != std::string::npos);
  const auto back = net::read_fusion_network(c.scratch.write("view.xml", text), c.ctx);
  CHECK(back->nodes().size() == n->nodes().size());
  CHECK(back->edges().size() == n->edges().size() - 3);    // t6's self-loop and its two component edges
}

TEST_CASE("network: the reader refuses what it cannot load as saved (02.C4, 09's artefact codes)", "[unit][network]") {
  Context c;
  auto read = [&](const std::string& text) {
    const auto p = c.scratch.write("f.xml", text);
    return failure_code([&] { net::read_fusion_network(p, c.ctx); });
  };
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"2\"/>") == "artefact.newer_format");
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\"/>") == "network.parse");
  CHECK(read("<pathway name=\"KEGG\"/>") == "network.parse");
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">"
             "<entry id=\"1\" key=\"t2\" name=\"t1 t2\" type=\"gene\"/></pathway>") == "network.parse");   // key
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">"
             "<entry id=\"1\" name=\"t1\" type=\"gene\"/><relation entry1=\"1\" entry2=\"2\" type=\"PPrel\"/></pathway>") ==
        "network.dangling_ref");
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">"
             "<entry id=\"1\" name=\"x\" type=\"group\"><component id=\"2\"/></entry>"
             "<entry id=\"2\" name=\"y\" type=\"group\"><component id=\"1\"/></entry></pathway>") == "network.cyclic_group");
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">"
             "<entry id=\"1\" name=\"t1\" type=\"gene\"/><entry id=\"1\" name=\"t2\" type=\"gene\"/></pathway>") ==
        "network.duplicate_entry");
  CHECK(read("<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">"
             "<entry id=\"1\" name=\"t1\" type=\"gene\"/><relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" flags=\"odd\"/></pathway>") ==
        "network.parse");
}

TEST_CASE("network: rgx compares a saved network's settings, exceptions and id map with the run's (2A.4)", "[unit][network]") {
  Context c;
  const auto n = tst_network(c);
  const net::NetworkInfo& info = n->info();
  CHECK(net::saved_network_diff(info, {}, net::IdSet({"t4"}), net::IdMap::none()).empty());
  const auto d1 = net::saved_network_diff(info, {15000, {net::IndirectMode::keep, net::EcrelMode::exclude, net::MaplinkMode::keep}},
                                          net::IdSet({"t4", "t9"}), net::IdMap::none());
  REQUIRE(d1.size() == 5);
  CHECK(d1[0] == "network.split_threshold: saved 1, run 15000");
  CHECK(d1[1] == "network.indirect: saved discard, run keep");
  CHECK(d1[2] == "network.ecrel: saved keep, run exclude");
  CHECK(d1[3] == "network.maplink: saved skip, run keep");
  CHECK(d1[4] == "exceptions: saved 1 ids, run 2; run adds t9");
  const auto map = net::IdMap::read(c.scratch.write("map.txt", "a b\n"), c.ctx);
  const auto d2 = net::saved_network_diff(info, {}, net::IdSet(), map);
  REQUIRE(d2.size() == 2);
  CHECK(d2[0] == "exceptions: saved 1 ids, run 0; run lacks t4");
  CHECK(d2[1] == "id map: saved none, run " + map.digest().hex().substr(0, 12));
  net::NetworkInfo add;
  add.reader = "add";
  CHECK(net::saved_network_diff(add, {15000, {}}, net::IdSet({"t1"}), map).empty());   // used as saved
}

TEST_CASE("network: add transcribes edge lines into FuSION's marked format (S2.30, 02.C17)", "[unit][network]") {
  Context c;
  const auto edges = c.scratch.write("extra.txt", "\n# extra edges\nhsa:7 hsa2 PPrel activation\r\nhsa2 hsa10 PPrel binding/association\n"
                                                  "hsa7 hsa2 GErel expression\n");
  std::ostringstream out;
  net::add_edges(edges, out, net::IdMap::none(), c.ctx);
  const std::string text = out.str();
  CHECK(text ==
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\">\n"
        "  <fusion writer=\"FuSION " + std::string(core::build_info().version) + "\" reader=\"add\" legacy=\"false\" />\n"
        "  <entry id=\"2\" key=\"hsa10\" name=\"hsa10\" type=\"gene\">\n    <graphics name=\"hsa10\" />\n  </entry>\n"
        "  <entry id=\"1\" key=\"hsa2\" name=\"hsa2\" type=\"gene\">\n    <graphics name=\"hsa2\" />\n  </entry>\n"
        "  <entry id=\"0\" key=\"hsa7\" name=\"hsa7\" type=\"gene\">\n    <graphics name=\"hsa7\" />\n  </entry>\n"
        "  <relation entry1=\"0\" entry2=\"1\" type=\"PPrel\" pathways=\"KEGG\">\n    <subtype name=\"activation\" />\n  </relation>\n"
        "  <relation entry1=\"1\" entry2=\"2\" type=\"PPrel\" pathways=\"KEGG\">\n    <subtype name=\"association\" />\n  </relation>\n"
        "  <relation entry1=\"0\" entry2=\"1\" type=\"GErel\" pathways=\"KEGG\">\n    <subtype name=\"expression\" />\n  </relation>\n"
        "</pathway>\n");
  const auto path = c.scratch.write("extra.xml", text);
  CHECK(net::sniff(path, true, c.ctx).format == net::Format::fusion_network);
  CHECK(c.lines->count("network.flag_mismatch") == 0);
  const auto n = net::read_fusion_network(path, c.ctx);
  CHECK(n->info().reader == "add");
  CHECK(n->nodes().size() == 3);
  CHECK(n->edges().size() == 3);
  const auto e = n->find_edge({{"hsa2"}, {"hsa10"}, "PPrel", {"association"}});
  REQUIRE(e);
  CHECK_FALSE(has_flag(n->edges()[*e], EdgeFlag::undirected));   // one-way, as FuSION 1's add output read with y
  CHECK_FALSE(has_flag(n->edges()[*e], EdgeFlag::dummy_u_to_d));
  CHECK(has_flag(n->edges()[*e], EdgeFlag::unscored));
  CHECK_FALSE(n->find_edge({{"hsa10"}, {"hsa2"}, "PPrel", {"association"}}));
  CHECK(net::saved_network_diff(n->info(), {15000, {}}, net::IdSet({"x"}), net::IdMap::none()).empty());

  auto bad = [&](const std::string& lines) {
    const auto p = c.scratch.write("bad.txt", lines);
    std::ostringstream o;
    return failure_code([&] { net::add_edges(p, o, net::IdMap::none(), c.ctx); });
  };
  CHECK(bad("t1 t2 PPrel\n") == "network.bad_row");
  CHECK(bad("t1 t2 PPrel activation more\n") == "network.bad_row");
  CHECK(bad("t1 t2 PPRel activation\n") == "network.type_spelling");
  CHECK(bad("t1 t2 Ecrel compound\n") == "network.type_spelling");
  CHECK(failure_code([&] { std::ostringstream o; net::add_edges(c.scratch.dir() / "none.txt", o, net::IdMap::none(), c.ctx); }) ==
        "input.missing");
  const auto warn = c.scratch.write("warn.txt", "t1 t2 Custom activation\nt1 t2 PPrel wobble\n");
  std::ostringstream o;
  net::add_edges(warn, o, net::IdMap::none(), c.ctx);
  CHECK(c.lines->count("network.unknown_type") == 1);
  CHECK(c.lines->count("network.unknown_subtype") == 1);
}

TEST_CASE("network: sz counts nodes by type, isolated nodes, edges and multi-gene nodes (S2.28)", "[unit][network]") {
  Context c;
  const auto n = tst_network(c, false);
  const net::GraphView all(n);
  const net::Summary s = net::summarize(all);
  CHECK(s.nodes == 10);
  CHECK(s.genes == 8);
  CHECK(s.complexes == 1);
  CHECK(s.compounds == 1);
  CHECK(s.others == 0);
  CHECK(s.isolated == 0);
  CHECK(s.edges == 15);
  CHECK(s.multi_gene == 2);
  CHECK(n->node_key(s.largest).value == "t10");             // ties: the first in key order
  CHECK(s.largest_size == 2);
  CHECK(net::summary_lines(s, all, {}) ==
        std::vector<std::string>{"Number of nodes: 10 (8 genes, 1 complexes, 1 compounds, 0 others) 0 isolated",
                                 "Number of edges: 15", "Multi-gene nodes: 2; largest t10 (2 genes)"});
  net::Bitset nodes(n->nodes().size(), true), edges(n->edges().size(), true);
  nodes.reset(*n->find_node({"t7"}));
  const net::GraphView view(n, nodes, edges);
  const net::Summary v = net::summarize(view);
  CHECK(v.nodes == 9);
  CHECK(v.isolated == 1);                                   // t9's only edge went to t7
  CHECK(v.edges == 11);
  const auto one = net::read_legacy(c.scratch.write("one.xml", "<pathway name=\"KEGG\"><entry id=\"1\" name=\"t1\" type=\"gene\"/>"
                                                              "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" pathways=\"p\"/></pathway>"),
                                    net::IdMap::none(), {}, net::IdSet(), c.ctx);
  const net::Summary o = net::summarize(net::GraphView(one));
  CHECK(net::summary_lines(o, net::GraphView(one), {})[2] == "Multi-gene nodes: 0");
}

TEST_CASE("network: the labels query lists a map's pairs in byte order of id (2B.6)", "[unit][network]") {
  Context c;
  const auto path = data("symbols.tsv");
  const net::LabelMap symbols = net::LabelMap::read(path, c.ctx);
  net::SourceRef ref;
  ref.path = path;
  const core::Json answer = net::labels_answer(symbols, ref);
  CHECK(answer["map"]["path"] == core::to_utf8(path));
  CHECK(answer["map"]["sha256"] == symbols.digest().hex());
  REQUIRE(answer["labels"].size() == 7);
  CHECK(answer["labels"][0] == core::Json::array({"cpdC00001", "Water"}));
  CHECK(answer["labels"][1] == core::Json::array({"t1", "AAA"}));
  CHECK(answer["labels"][6] == core::Json::array({"t7", "GGG"}));
  CHECK(answer.dump().rfind("{\"map\":", 0) == 0);
}
