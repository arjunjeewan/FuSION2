// The model and its builder, views, labels, the id index and the binding table (02.C8 to 02.C13;
// S2.03, S2.07 to S2.10, S2.20, S2.21; 07.C10; items 1.22, 1.25, 2B.11).
#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "support.hpp"

using namespace network_test;
using net::EdgeFlag;
using net::NodeFlag;

namespace {

struct Built {
  core::Interner in;
  net::NetworkBuilder b{net::NetworkInfo{"test", {}, {}, {}, {}, false, std::nullopt}, in};
  net::NodeIdx node(std::initializer_list<const char*> ids, const char* type = "gene") {
    std::vector<core::GeneId> v;
    for (const char* id : ids) v.push_back(in.id(id));
    return b.add_node(in.sym(type), v);
  }
  net::EdgeIdx edge(net::NodeIdx s, net::NodeIdx t, const char* type, std::initializer_list<const char*> subs,
                    std::initializer_list<const char*> paths, std::optional<double> score, std::uint16_t flags = 0) {
    std::vector<core::Sym> ss, ps;
    for (const char* x : subs) ss.push_back(in.sym(x));
    for (const char* x : paths) ps.push_back(in.sym(x));
    return b.add_edge(s, t, in.sym(type), ss, ps, score, flags);
  }
};

net::Edge edge_of(const net::Network& n, const net::EdgeKey& key) {
  const auto e = n.find_edge(key);
  REQUIRE(e);
  return n.edges()[*e];
}

std::vector<std::string> pathways_of(const net::Network& n, net::EdgeIdx e) {
  std::vector<std::string> out;
  for (core::Sym p : n.pathways(e)) out.emplace_back(n.interner().str(p));
  return out;
}

}  // namespace

TEST_CASE("network: a node's key is its smallest member in byte order; add_members gives S2.08's fix", "[unit][network]") {
  Context c;
  Built x;
  const auto a = x.node({"t5", "t10"});
  const auto b = x.node({"t7"});
  x.b.add_members(b, std::vector<core::GeneId>{x.in.id("t3"), x.in.id("t7")});   // a later box: every id joins
  x.edge(a, b, "PPrel", {}, {"p"}, std::nullopt);
  const net::Network n = std::move(x.b).finish(c.ctx);
  REQUIRE(n.nodes().size() == 2);
  CHECK(n.node_key(0).value == "t10");                      // "t10" < "t5"
  CHECK(n.node_key(1).value == "t3");                       // the key moved with the new member
  std::vector<std::string> members;
  for (core::GeneId m : n.members(1)) members.emplace_back(n.interner().str(m));
  CHECK(members == std::vector<std::string>{"t3", "t7"});   // sorted, unique
  CHECK(has_flag(n.nodes()[0], NodeFlag::family));
  CHECK(n.kind(0) == net::NodeKind::gene);
}

TEST_CASE("network: complexes: one per component set, keyed by their distinct component keys (S2.09)", "[unit][network]") {
  Context c;
  Built x;
  const auto t6 = x.node({"t6"});
  const auto box = x.node({"t3", "t2"});
  const auto cx = x.b.add_complex(std::vector<net::NodeIdx>{t6, box});
  CHECK(x.b.add_complex(std::vector<net::NodeIdx>{box, t6, box}) == cx);   // reused, repeats ignored
  const auto single = x.b.add_complex(std::vector<net::NodeIdx>{t6});
  const auto nested = x.b.add_complex(std::vector<net::NodeIdx>{cx, single});
  x.b.add_component_edges(cx, x.in.sym("tst00001"));
  const net::Network n = std::move(x.b).finish(c.ctx);
  const auto k = n.find_node({"c_s_t2_t6_c_e"});
  REQUIRE(k);
  CHECK(n.find_node({"c_s_t6_c_e"}));                       // one distinct key
  CHECK(n.find_node({"c_s_c_s_t2_t6_c_e_c_s_t6_c_e_c_e"}));   // a group of groups
  CHECK(n.kind(*k) == net::NodeKind::complex);
  CHECK(has_flag(n.nodes()[*k], NodeFlag::complex));
  REQUIRE(n.members(*k).size() == 1);                       // a complex's only member is its key
  CHECK(n.interner().str(n.members(*k)[0]) == "c_s_t2_t6_c_e");
  std::vector<std::string> comps;
  for (auto c2 : n.components(*k)) comps.push_back(n.node_key(c2).value);
  CHECK(comps == std::vector<std::string>{"t2", "t6"});     // key order
  // Two PPrel component edges per component, both tagged, the document's pathway, unscored.
  CHECK(n.edges().size() == 4);
  for (const char* comp : {"t2", "t6"}) {
    for (auto key : {net::EdgeKey{{comp}, {"c_s_t2_t6_c_e"}, "PPrel", {"component"}},
                     net::EdgeKey{{"c_s_t2_t6_c_e"}, {comp}, "PPrel", {"component"}}}) {
      const auto e = n.find_edge(key);
      REQUIRE(e);
      const net::Edge& edge = n.edges()[*e];
      CHECK(has_flag(edge, EdgeFlag::component));
      CHECK(has_flag(edge, EdgeFlag::dummy_u_to_d));
      CHECK(has_flag(edge, EdgeFlag::unscored));
      CHECK(pathways_of(n, *e) == std::vector<std::string>{"tst00001"});
    }
  }
  (void)nested;
}

TEST_CASE("network: edges merge on identity: pathways unioned, the larger score, unscored cleared (S2.10, S2.20, 1.25)",
          "[unit][network]") {
  Context c;
  Built x;
  const auto a = x.node({"a"});
  const auto b = x.node({"b"});
  const auto e1 = x.edge(a, b, "PPrel", {"activation"}, {"path:2"}, std::nullopt);
  CHECK(x.edge(a, b, "PPrel", {"activation"}, {"path:1"}, 625.0) == e1);
  CHECK(x.edge(a, b, "PPrel", {"activation"}, {"path:2", "path:3"}, 300.0) == e1);
  CHECK(x.edge(a, b, "PPrel", {"activation", "dummy_u_to_d"}, {"path:4"}, std::nullopt) == e1);   // the tag is no part of identity
  CHECK(x.edge(a, b, "GErel", {"activation"}, {"path:1"}, 1.0) != e1);                // the type is
  CHECK(x.edge(a, b, "PPrel", {"activation", "phosphorylation"}, {}, std::nullopt) != e1);
  CHECK(x.edge(a, b, "PPrel", {"phosphorylation", "activation"}, {}, std::nullopt) != e1);   // ordered list
  const auto u = x.edge(b, a, "PPrel", {"inhibition"}, {}, std::nullopt);
  const net::Network n = std::move(x.b).finish(c.ctx);
  CHECK(n.edges().size() == 5);
  const auto e = *n.find_edge({{"a"}, {"b"}, "PPrel", {"activation"}});
  CHECK(n.edges()[e].score == 625.0);
  CHECK_FALSE(has_flag(n.edges()[e], EdgeFlag::unscored));
  CHECK(pathways_of(n, e) == std::vector<std::string>{"path:1", "path:2", "path:3", "path:4"});
  CHECK(has_flag(n.edges()[e], EdgeFlag::dummy_u_to_d));   // given, and kept: not an undirected edge
  const auto ue = *n.find_edge({{"b"}, {"a"}, "PPrel", {"inhibition"}});
  CHECK(has_flag(n.edges()[ue], EdgeFlag::unscored));
  CHECK(n.edges()[ue].score == 0.0);
  CHECK(n.effect(ue) == net::Effect::inhibition);
  CHECK(n.effect(e) == net::Effect::activation);
  (void)u;
}

TEST_CASE("network: undirected pairs are tagged once, on the larger source key; self-loops never (S2.10, S2.21)", "[unit][network]") {
  Context c;
  Built x;
  const auto t1 = x.node({"t1"});
  const auto t12 = x.node({"t12"});
  const auto t6 = x.node({"t6"});
  const std::uint16_t und = net::bit(EdgeFlag::undirected);
  x.edge(t12, t1, "PPrel", {"association"}, {"p"}, 300.0, und);
  x.edge(t1, t12, "PPrel", {"association"}, {"p"}, 300.0, und);
  x.edge(t6, t6, "PPrel", {"association"}, {"p"}, std::nullopt, und);
  x.edge(t6, t6, "PPrel", {"activation"}, {"p"}, std::nullopt);
  x.edge(t1, t6, "ECrel", {"compound"}, {"p"}, 0.0);
  x.edge(t6, t1, "PPrel", {"activation", "indirect"}, {"p"}, std::nullopt);
  const net::Network n = std::move(x.b).finish(c.ctx);
  CHECK(has_flag(edge_of(n, {{"t12"}, {"t1"}, "PPrel", {"association"}}), EdgeFlag::dummy_u_to_d));
  CHECK_FALSE(has_flag(edge_of(n, {{"t1"}, {"t12"}, "PPrel", {"association"}}), EdgeFlag::dummy_u_to_d));
  CHECK(has_flag(edge_of(n, {{"t1"}, {"t12"}, "PPrel", {"association"}}), EdgeFlag::undirected));
  const net::Edge loop = edge_of(n, {{"t6"}, {"t6"}, "PPrel", {"association"}});
  CHECK(has_flag(loop, EdgeFlag::self_loop));
  CHECK_FALSE(has_flag(loop, EdgeFlag::dummy_u_to_d));
  CHECK(has_flag(edge_of(n, {{"t6"}, {"t6"}, "PPrel", {"activation"}}), EdgeFlag::self_loop));
  CHECK(has_flag(edge_of(n, {{"t1"}, {"t6"}, "ECrel", {"compound"}}), EdgeFlag::ecrel));
  CHECK_FALSE(has_flag(edge_of(n, {{"t1"}, {"t6"}, "ECrel", {"compound"}}), EdgeFlag::unscored));   // a score of 0
  CHECK(has_flag(edge_of(n, {{"t6"}, {"t1"}, "PPrel", {"activation", "indirect"}}), EdgeFlag::indirect));
}

TEST_CASE("network: canonical order, adjacency and lookups (02.C8, 02.C10, S5.06)", "[unit][network]") {
  Context c;
  Built x;
  const auto z = x.node({"z"});
  const auto a = x.node({"a"});
  const auto m = x.node({"m"});
  x.edge(z, a, "PPrel", {"inhibition"}, {}, std::nullopt);
  x.edge(a, z, "PPrel", {"activation"}, {}, std::nullopt);
  x.edge(a, m, "PPrel", {"phosphorylation", "activation"}, {}, std::nullopt);
  x.edge(a, m, "GErel", {"expression"}, {}, std::nullopt);
  x.edge(a, m, "PPrel", {"activation"}, {}, std::nullopt);
  x.edge(m, a, "PPrel", {}, {}, std::nullopt);
  const net::Network n = std::move(x.b).finish(c.ctx);
  std::vector<std::string> keys;
  for (net::EdgeIdx e = 0; e < n.edges().size(); ++e) keys.push_back(n.edge_key(e).text());
  CHECK(keys == std::vector<std::string>{"a|m|GErel|expression", "a|m|PPrel|activation",
                                         "a|m|PPrel|phosphorylation,activation", "a|z|PPrel|activation",
                                         "m|a|PPrel|", "z|a|PPrel|inhibition"});
  for (net::EdgeIdx e = 0; e + 1 < n.edges().size(); ++e) CHECK(n.edge_key(e) < n.edge_key(e + 1));
  const auto ia = *n.find_node({"a"});
  CHECK(ia == 0);
  CHECK(n.find_node({"m"}) == 1u);
  CHECK_FALSE(n.find_node({"q"}));
  CHECK(std::vector<net::EdgeIdx>(n.out_edges(ia).begin(), n.out_edges(ia).end()) == std::vector<net::EdgeIdx>{0, 1, 2, 3});
  CHECK(std::vector<net::EdgeIdx>(n.in_edges(ia).begin(), n.in_edges(ia).end()) == std::vector<net::EdgeIdx>{4, 5});
  CHECK(n.find_edge({{"a"}, {"m"}, "PPrel", {"phosphorylation", "activation"}}) == 2u);
  CHECK_FALSE(n.find_edge({{"a"}, {"m"}, "PPrel", {"activation", "phosphorylation"}}));
  CHECK_FALSE(n.find_edge({{"a"}, {"q"}, "PPrel", {"activation"}}));
  // S5.06: a file's subtypes are a set; dummy_u_to_d is no subtype.
  const std::vector<std::string_view> set1{"activation", "phosphorylation", "dummy_u_to_d"};
  CHECK(n.find_edges(ia, 1, set1) == std::vector<net::EdgeIdx>{2});
  const std::vector<std::string_view> set2{"activation"};
  CHECK(n.find_edges(ia, 1, set2) == std::vector<net::EdgeIdx>{1});
  CHECK(n.find_edges(1, ia, std::vector<std::string_view>{}) == std::vector<net::EdgeIdx>{4});
  CHECK(n.has_subtype(2, "phosphorylation"));
  CHECK_FALSE(n.has_subtype(2, "inhibition"));
}

TEST_CASE("network: the same content in any order gives the same network (02.C10)", "[unit][network]") {
  Context c;
  auto build = [&](bool reversed) {
    Built x;
    std::vector<std::vector<const char*>> nodes = {{"t3", "t2"}, {"t1"}, {"t6"}, {"t10"}};
    if (reversed) std::reverse(nodes.begin(), nodes.end());
    std::map<std::string, net::NodeIdx> idx;
    for (const auto& ids : nodes) {
      std::vector<core::GeneId> v;
      for (const char* id : ids) v.push_back(x.in.id(id));
      idx[ids.back()] = x.b.add_node(x.in.sym("gene"), v);
    }
    struct E { const char *s, *t, *type, *sub, *path; double score; };
    std::vector<E> edges = {{"t1", "t2", "PPrel", "activation", "p:1", 1}, {"t2", "t6", "GErel", "expression", "p:2", 2},
                            {"t1", "t2", "PPrel", "activation", "p:3", 5}, {"t10", "t1", "PPrel", "inhibition", "p:1", 3}};
    if (reversed) std::reverse(edges.begin(), edges.end());
    for (const auto& e : edges) x.edge(idx[e.s], idx[e.t], e.type, {e.sub}, {e.path}, e.score);
    const auto cx = x.b.add_complex(std::vector<net::NodeIdx>{idx["t6"], idx["t1"]});
    x.b.add_component_edges(cx, x.in.sym("p:1"));
    return std::move(x.b).finish(c.ctx);
  };
  const net::Network a = build(false), b = build(true);
  CHECK(dump(a) == dump(b));
  CHECK(a.content_hash() == b.content_hash());
  CHECK(a.edges().size() == 7);
}

TEST_CASE("network: edge keys are unique because they include the type (02.C11)", "[unit][network]") {
  Context c;
  Built x;
  const auto a = x.node({"a"});
  const auto b = x.node({"b"});
  x.edge(a, b, "PPrel", {"activation"}, {}, std::nullopt);
  x.edge(a, b, "GErel", {"activation"}, {}, std::nullopt);
  const net::Network n = std::move(x.b).finish(c.ctx);
  std::set<std::string> with_type, without_type;
  for (net::EdgeIdx e = 0; e < n.edges().size(); ++e) {
    const auto k = n.edge_key(e);
    with_type.insert(k.text());
    without_type.insert(k.source.value + "|" + k.target.value + "|" + (k.subtypes.empty() ? "" : k.subtypes[0]));
  }
  CHECK(with_type.size() == 2);
  CHECK(without_type.size() == 1);
}

TEST_CASE("network: two nodes with one key fail; bad builder calls are internal failures", "[unit][network]") {
  Context c;
  Built x;
  x.node({"t1", "t2"});
  x.node({"t1"});
  CHECK_THROWS_AS(std::move(x.b).finish(c.ctx), core::Failure);
  Built y;
  CHECK_THROWS_AS(y.b.add_node(y.in.sym("gene"), std::vector<core::GeneId>{}), core::Failure);
  CHECK_THROWS_AS(y.edge(0, 1, "PPrel", {}, {}, std::nullopt), core::Failure);
  const auto n0 = y.node({"a"});
  CHECK_THROWS_AS(y.b.add_component_edges(n0, y.in.sym("p")), core::Failure);
  Built z;
  z.node({"t1"});
  z.node({"t1"});
  std::string code = "none";
  try {
    (void)std::move(z.b).finish(c.ctx);
  } catch (const core::Failure& f) {
    code = f.code();
  }
  CHECK(code == "network.duplicate_key");
}

TEST_CASE("network: copies keep members, flags, pathways, scores and provenance; provenance is canonical (2B.11)",
          "[unit][network]") {
  Context c;
  const auto doc_t = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const auto doc_a = net::read_kgml(c.scratch.write("aaa.xml", "<pathway name=\"path:aaa\" org=\"t\">"
                                                               "<entry id=\"1\" name=\"t:1\" type=\"gene\"/>"
                                                               "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\"/></pathway>"),
                                    net::IdMap::none(), c.ctx);
  Built x;
  const auto t6 = x.node({"t6"});
  const auto box = x.node({"t2", "t3"});
  const auto cx = x.b.add_complex(std::vector<net::NodeIdx>{t6, box});
  const auto e = x.edge(box, t6, "PPrel", {"activation"}, {"tst00001", "path:aaa"}, 7.0, net::bit(EdgeFlag::undirected));
  x.b.mark_exception(t6);
  net::Provenance p;
  p.docs = {doc_t, doc_a};                                  // not in canonical order
  p.edge_first = {0, 2};                                    // edge 0's refs: relations 0 of each
  p.edge_refs = {{0, 0, net::Provenance::Kind::relation}, {1, 0, net::Provenance::Kind::relation}};
  p.node_first = {0, 0, 0, 1};
  p.node_refs = {{0, 5, net::Provenance::Kind::group}};    // the complex came from group entry 6
  REQUIRE(e == 0);
  x.b.attach(p);
  const net::Network base = std::move(x.b).finish(c.ctx);
  const net::Provenance* bp = base.provenance();
  REQUIRE(bp != nullptr);
  REQUIRE(bp->docs.size() == 2);
  CHECK(bp->docs[0]->source().pathway == "path:aaa");      // (pathway id, SHA-256) order
  const auto be = *base.find_edge({{"t2"}, {"t6"}, "PPrel", {"activation"}});
  REQUIRE(bp->edge_first.size() == base.edges().size() + 1);
  REQUIRE(bp->edge_first[be + 1] - bp->edge_first[be] == 2);
  CHECK(bp->edge_refs[bp->edge_first[be]].doc == 0);
  CHECK(bp->docs[bp->edge_refs[bp->edge_first[be] + 1].doc]->source().pathway == "tst00001");
  const auto bc = *base.find_node({"c_s_t2_t6_c_e"});
  REQUIRE(bp->node_first[bc + 1] - bp->node_first[bc] == 1);
  CHECK(bp->node_refs[bp->node_first[bc]].kind == net::Provenance::Kind::group);
  CHECK(bp->node_refs[bp->node_first[bc]].item == 5);

  // 03's split copies the untouched part of a base merge.
  core::Interner in2;
  net::NetworkBuilder b2(base.info(), in2);
  const auto c1 = b2.copy_node(base, bc);                   // copies its components first
  CHECK(b2.copy_node(base, bc) == c1);
  const auto s = b2.copy_node(base, *base.find_node({"t2"}));
  const auto t = b2.copy_node(base, *base.find_node({"t6"}));
  b2.copy_edge(base, be, s, t);
  const net::Network copy = std::move(b2).finish(c.ctx);
  CHECK(dump(copy) == dump(base));
  CHECK(copy.content_hash() == base.content_hash());
  REQUIRE(copy.provenance() != nullptr);
  const auto ce = *copy.find_edge({{"t2"}, {"t6"}, "PPrel", {"activation"}});
  CHECK(copy.provenance()->edge_first[ce + 1] - copy.provenance()->edge_first[ce] == 2);
  CHECK(has_flag(copy.nodes()[*copy.find_node({"t6"})], NodeFlag::exception));
  (void)cx;
}

TEST_CASE("network: labels show the family size; complexes their components; pathways their KGML title (07.C10, entry 205)",
          "[unit][network]") {
  Context c;
  net::Maps maps;
  maps.symbols = net::LabelMap::read(data("symbols.tsv"), c.ctx);
  maps.pathway_names = net::LabelMap::read(c.scratch.write("names.tsv", "path:r1\tA Reactome pathway\ntst00001\tOverridden\n"), c.ctx);
  core::Interner in;
  net::NetworkInfo info;
  info.sources.push_back({std::nullopt, 0, core::Sha256::of("x"), net::Format::kgml, "tst00001", "Test signalling pathway"});
  net::NetworkBuilder b(info, in);
  auto node = [&](std::initializer_list<const char*> ids) {
    std::vector<core::GeneId> v;
    for (auto id : ids) v.push_back(in.id(id));
    return b.add_node(in.sym("gene"), v);
  };
  const auto t1 = node({"t1"});
  const auto box = node({"t2", "t3", "t99"});
  const auto t6 = node({"t6"});
  const auto bare = node({"t40"});
  const auto cx = b.add_complex(std::vector<net::NodeIdx>{t6, box});
  b.add_complex(std::vector<net::NodeIdx>{bare, t1});
  const net::Network n = std::move(b).finish(c.ctx);
  CHECK(n.label(*n.find_node({"t1"}), maps) == "AAA");
  CHECK(n.label(*n.find_node({"t2"}), maps) == "BBB+2");    // "RRAS2+5"
  CHECK(n.label(*n.find_node({"t40"}), maps) == "t40");
  CHECK(n.label(*n.find_node({"c_s_t2_t6_c_e"}), maps) == "[BBB+2+FFF]");   // "[STAT1+6]"
  CHECK(n.label(*n.find_node({"c_s_t1_t40_c_e"}), maps) == "[AAA+t40]");
  net::Maps named = maps;
  named.symbols = net::LabelMap::read(c.scratch.write("cx.tsv", "c_s_t2_t6_c_e\tThe complex\n"), c.ctx);
  CHECK(n.label(*n.find_node({"c_s_t2_t6_c_e"}), named) == "The complex");
  CHECK(n.pathway_label("tst00001", maps) == "Test signalling pathway");   // the KGML's root title
  CHECK(n.pathway_label("path:r1", maps) == "A Reactome pathway");
  CHECK(n.pathway_label("path:none", maps) == "path:none");
  (void)cx;
}

TEST_CASE("network: views select nodes and edges by the network's own indices (02.C9)", "[unit][network]") {
  Context c;
  Built x;
  const auto a = x.node({"a"});
  const auto b = x.node({"b"});
  const auto d = x.node({"d"});
  x.edge(a, b, "PPrel", {}, {}, std::nullopt);
  x.edge(b, d, "PPrel", {}, {}, std::nullopt);
  x.edge(a, d, "PPrel", {}, {}, std::nullopt);
  auto n = std::make_shared<net::Network>(std::move(x.b).finish(c.ctx));
  const net::GraphView all(n);
  CHECK(all.node_count() == 3);
  CHECK(all.edge_count() == 3);
  net::Bitset nodes(3), edges(3, true);
  nodes.set(0);
  nodes.set(1);
  CHECK(nodes.count() == 2);
  CHECK(edges.count() == 3);
  const net::GraphView view(n, nodes, edges);
  CHECK(view.node_count() == 2);
  std::vector<net::EdgeIdx> out;
  view.for_each_out_edge(0, [&](net::EdgeIdx e) { out.push_back(e); });
  CHECK(out == std::vector<net::EdgeIdx>{0});               // a -> d leaves the view
  CHECK(&view.network() == n.get());
  CHECK_THROWS_AS(net::GraphView(n, net::Bitset(2), net::Bitset(3)), core::Failure);
  net::Bitset big(130, true);
  CHECK(big.count() == 130);
  big.reset(129);
  CHECK(big.count() == 129);
}

TEST_CASE("network: the binding table resolves each id once and writes bindings.tsv (02.C13, S2.03, 1.22)", "[unit][network]") {
  Context c;
  net::Maps maps;
  maps.symbols = net::LabelMap::read(data("symbols.tsv"), c.ctx);
  Built x;
  x.node({"t1"});
  const auto box = x.node({"t2", "t3"});
  const auto cx = x.b.add_complex(std::vector<net::NodeIdx>{box});
  (void)cx;
  auto n = std::make_shared<net::Network>(std::move(x.b).finish(c.ctx));
  net::BindingTable table(n, maps);
  const net::Binding& own = table.bind("source", "t:1");
  CHECK(own.status == net::BindStatus::own);
  CHECK(own.canonical == "t1");                             // the id rule: ':' deleted (02.C3)
  CHECK(n->node_key(own.node).value == "t1");
  CHECK(&table.bind("source", "t1") == &own);              // one binding per (role, id)
  const net::Binding& fam = table.bind("up", "t3", 54);
  CHECK(fam.status == net::BindStatus::family);
  CHECK(n->node_key(fam.node).value == "t2");
  CHECK(fam.holders == 1);
  const net::Binding& none = table.bind("coexpression", "hsa:999", 7);
  CHECK(none.status == net::BindStatus::absent);
  CHECK(none.id == net::absent_id);
  CHECK(table.bind("target", "c_s_t2_c_e").status == net::BindStatus::own);
  table.add_row({"essential", "t1 t2 activation", std::nullopt, 3, "no such edge"});
  const auto counts = table.counts("source");
  CHECK(counts.ids == 1);
  CHECK(counts.own == 1);
  CHECK(table.counts("coexpression").absent == 1);
  CHECK(table.counts("essential").ids == 0);
  std::ostringstream out;
  table.write(out);
  CHECK(out.str() ==
        "role\tgiven\tid\tstatus\tnode\tlabel\tsize\tmembers\talso_in\tline\tnote\n"
        "source\tt:1\tt1\town\tt1\tAAA\t1\tAAA\t\t\t\n"
        "up\tt3\tt3\tfamily\tt2\tBBB+1\t2\tBBB,CCC\t\t54\t\n"
        "coexpression\thsa:999\thsa999\tabsent\t\t\t\t\t\t7\t\n"
        "target\tc_s_t2_c_e\tc_s_t2_c_e\town\tc_s_t2_c_e\t[BBB+1]\t1\tc_s_t2_c_e\t\t\t\n"
        "essential\tt1 t2 activation\t\t\t\t\t\t\t\t3\tno such edge\n");
}

TEST_CASE("network: an id held by several nodes binds by claim order and is reported ambiguous (S2.03)", "[unit][network]") {
  Context c;
  auto build = [&](bool claims) {
    Built x;
    const auto ras = x.node({"t22800", "t3265", "t3845"});   // the family box, keyed by its smallest id
    const auto hras = x.node({"t3265"});
    if (claims)
      x.b.set_claim_order(std::vector<std::pair<core::GeneId, net::NodeIdx>>{{x.in.id("t3265"), hras}, {x.in.id("t3265"), ras}});
    return std::make_shared<net::Network>(std::move(x.b).finish(c.ctx));
  };
  for (bool claims : {false, true}) {
    const auto n = build(claims);
    const auto holders = n->ids().holders(*n->interner().find_id("t3265"));
    REQUIRE(holders.size() == 2);
    CHECK(n->node_key(holders[0]).value == (claims ? "t3265" : "t22800"));
    net::BindingTable table(n, net::Maps{});
    const net::Binding& b = table.bind("essential", "t3265");
    CHECK(b.status == net::BindStatus::ambiguous);
    CHECK(b.holders == 2);
    CHECK(n->node_key(b.node).value == (claims ? "t3265" : "t22800"));
    std::ostringstream out;
    table.write(out);
    CHECK(out.str().find(claims ? "\tt22800\t\t\n" : "\tt3265\t\t\n") != std::string::npos);   // also_in
  }
}

TEST_CASE("network: building tst00001.xml by section 02's rules (S2.07 to S2.11, S2.31)", "[unit][network]") {
  Context c;
  const auto doc = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const auto n = build_one(*doc, {}, net::IdSet({"t4"}), c.ctx);
  std::vector<std::string> keys;
  for (net::NodeIdx i = 0; i < n->nodes().size(); ++i) keys.push_back(n->node_key(i).value);
  CHECK(keys == std::vector<std::string>{"c_s_t2_t6_c_e", "cpdC00001", "t1", "t10", "t12", "t2", "t4", "t6", "t7", "t9"});
  CHECK(has_flag(n->nodes()[*n->find_node({"t4"})], NodeFlag::exception));
  CHECK(n->kind(*n->find_node({"cpdC00001"})) == net::NodeKind::compound);
  CHECK(n->edges().size() == 15);
  CHECK(edge_of(*n, {{"t1"}, {"t2"}, "PPrel", {"activation"}}).score == 700.0);     // the duplicate's larger score
  CHECK(has_flag(edge_of(*n, {{"t2"}, {"t10"}, "PPrel", {"inhibition", "phosphorylation"}}), EdgeFlag::unscored));
  CHECK(n->find_edge({{"t2"}, {"t4"}, "PPrel", {"inhibition", "phosphorylation"}}));  // fan-out over the split
  CHECK(has_flag(edge_of(*n, {{"t12"}, {"t1"}, "PPrel", {"association"}}), EdgeFlag::dummy_u_to_d));
  CHECK(has_flag(edge_of(*n, {{"t9"}, {"t7"}, "ECrel", {"compound"}}), EdgeFlag::ecrel));
  CHECK(has_flag(edge_of(*n, {{"t6"}, {"t6"}, "PPrel", {"activation"}}), EdgeFlag::self_loop));
  CHECK_FALSE(n->find_node({"t8"}));                        // only an indirect relation reached it
  CHECK_FALSE(n->find_node({"pathtst00002"}));              // maplink skipped
  CHECK_FALSE(n->find_node({"t11"}));                       // no relation reaches it
  const auto kept = build_one(*doc, {1, {net::IndirectMode::keep, net::EcrelMode::exclude, net::MaplinkMode::keep}},
                              net::IdSet({"t4"}), c.ctx);
  CHECK(has_flag(edge_of(*kept, {{"t7"}, {"t8"}, "PPrel", {"activation", "indirect"}}), EdgeFlag::indirect));
  CHECK_FALSE(kept->find_node({"t9"}));                     // only ECrel reached it
  CHECK(kept->kind(*kept->find_node({"pathtst00002"})) == net::NodeKind::other);
  const auto flat = build_one(*doc, {15000, {}}, net::IdSet(), c.ctx);   // no box collapses
  CHECK(flat->find_node({"t3"}));
  CHECK(flat->find_node({"c_s_t3_t6_c_e"}));
}
