// The KGML reader, its rejections, format detection, the relation filters, the split rule, the
// step's document cache and the exchange format (S2.05 to S2.07, S2.11, S2.12, S2.31; 02.C1, 02.C2,
// 02.C6, 02.C7; entry 205).
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace network_test;

namespace {

std::vector<std::string> ids_of(const net::PathwayDoc& doc, const net::Entry& e) {
  std::vector<std::string> out;
  for (core::GeneId id : doc.ids(e)) out.emplace_back(doc.interner().str(id));
  return out;
}

std::vector<std::string> subtypes_of(const net::PathwayDoc& doc, const net::Relation& r) {
  std::vector<std::string> out;
  for (core::Sym s : doc.subtypes(r)) out.emplace_back(doc.interner().str(s));
  return out;
}

std::string failure_code(const std::function<void()>& f) {
  try {
    f();
  } catch (const core::Failure& e) {
    return e.code();
  }
  return "none";
}

std::string kgml(const std::string& body) {
  return "<?xml version=\"1.0\"?>\n<pathway name=\"path:tst9\" org=\"tst\" number=\"9\" title=\"T\">\n" + body + "</pathway>\n";
}

}  // namespace

TEST_CASE("network: read_kgml keeps what S2.06 reads, with ids through the id rule", "[unit][network]") {
  Context c;
  const auto doc = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const auto& src = doc->source();
  CHECK(src.pathway == "tst00001");
  CHECK(src.title == "Test signalling pathway");            // entry 205: the pathway's name
  CHECK(src.format == net::Format::kgml);
  CHECK(src.sha256 == core::Sha256::of(read_file(data("tst00001.xml"))));
  CHECK(src.size == read_file(data("tst00001.xml")).size());
  REQUIRE(doc->entries().size() == 12);
  REQUIRE(doc->relations().size() == 11);

  // ':' deleted, split on runs of spaces (no empty id from the double space, S2.06), sorted by content.
  CHECK(ids_of(*doc, *doc->entry(1)) == std::vector<std::string>{"t1"});
  CHECK(ids_of(*doc, *doc->entry(3)) == std::vector<std::string>{"t10", "t4", "t5"});
  CHECK(ids_of(*doc, *doc->entry(7)) == std::vector<std::string>{"cpdC00001"});
  CHECK(doc->interner().str(doc->entry(7)->type) == "compound");
  const net::Entry& group = *doc->entry(6);
  CHECK(doc->interner().str(group.type) == "group");
  CHECK(std::vector<std::uint32_t>(doc->components(group).begin(), doc->components(group).end()) ==
        std::vector<std::uint32_t>{4, 2});
  CHECK(doc->entry(99) == nullptr);

  const auto rels = doc->relations();
  CHECK(rels[0].entry1 == 1);
  CHECK(rels[0].entry2 == 2);
  CHECK(rels[0].score == 625.0);
  CHECK_FALSE(rels[1].score);                               // "625x": unscored, with a warning
  CHECK(subtypes_of(*doc, rels[1]) == std::vector<std::string>{"inhibition", "phosphorylation"});
  CHECK_FALSE(rels[2].score);                               // absent: unscored, no warning
  CHECK(subtypes_of(*doc, rels[4]) == std::vector<std::string>{"association"});   // binding/association
  CHECK(subtypes_of(*doc, rels[5]) == std::vector<std::string>{"activation", "indirect"});
  CHECK(rels[6].score == 0.0);                              // "0" is a score
  CHECK(doc->interner().str(rels[7].type) == "maplink");
  CHECK(rels[0].line == 26);
  const auto paths = doc->pathways(rels[0]);
  REQUIRE(paths.size() == 1);
  CHECK(doc->interner().str(paths[0]) == "tst00001");

  CHECK(c.lines->count("network.bad_score") == 1);
  CHECK(c.lines->text("network.bad_score").find("\"625x\"") != std::string::npos);
  const auto* ev = c.events.find("network.read");
  REQUIRE(ev != nullptr);
  CHECK((*ev)["entries"] == 12);
  CHECK((*ev)["relations"] == 11);
  CHECK((*ev)["kept"] == 9);                                // the indirect and maplink relations dropped
  CHECK((*ev)["dropped"]["indirect"] == 1);
  CHECK((*ev)["dropped"]["maplink"] == 1);
  CHECK((*ev)["dropped"]["ecrel"] == 0);
  CHECK((*ev)["unscored"] == 6);

  // The GSEA gene set: every id of every gene entry, sorted (S7.08, 08.C3).
  std::vector<std::string> genes;
  for (core::GeneId g : doc->gene_ids()) genes.emplace_back(doc->interner().str(g));
  CHECK(genes == std::vector<std::string>{"t1", "t10", "t11", "t12", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9"});
}

TEST_CASE("network: a KGML root without a name takes the file stem (2B.12)", "[unit][network]") {
  Context c;
  const auto p = c.scratch.write("hsa_test.xml",
                                 "<pathway org=\"tst\"><entry id=\"1\" name=\"a:1\" type=\"gene\"/>"
                                 "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\"/></pathway>");
  const auto doc = net::read_kgml(p, net::IdMap::none(), c.ctx);
  CHECK(doc->source().pathway == "hsa_test");
  CHECK(doc->source().title.empty());
}

TEST_CASE("network: KGML rejections each fail with their code (S2.06)", "[unit][network]") {
  Context c;
  auto read = [&](const std::string& body) {
    const auto p = c.scratch.write("bad.xml", kgml(body));
    return failure_code([&] { net::read_kgml(p, net::IdMap::none(), c.ctx); });
  };
  const std::string e1 = "<entry id=\"1\" name=\"t:1\" type=\"gene\"/>\n";
  const std::string e2 = "<entry id=\"2\" name=\"t:2\" type=\"gene\"/>\n";
  CHECK(read(e1 + "<relation entry1=\"1\" entry2=\"7\" type=\"PPrel\"/>\n") == "network.dangling_ref");
  CHECK(read(e1 + "<entry id=\"3\" name=\"undefined\" type=\"group\"><component id=\"1\"/><component id=\"8\"/></entry>\n") ==
        "network.dangling_ref");
  CHECK(read(e1 + e2 + "<entry id=\"1\" name=\"t:3\" type=\"gene\"/>\n") == "network.duplicate_entry");
  CHECK(read(e1 + "<entry id=\"3\" name=\"undefined\" type=\"group\"/>\n") == "network.empty_group");
  CHECK(read(e1 + "<entry id=\"3\" name=\"undefined\" type=\"group\"><component id=\"4\"/></entry>\n"
                  "<entry id=\"4\" name=\"undefined\" type=\"group\"><component id=\"3\"/><component id=\"1\"/></entry>\n") ==
        "network.cyclic_group");
  CHECK(read(e1 + "<entry id=\"3\" name=\"undefined\" type=\"group\"><component id=\"3\"/></entry>\n") == "network.cyclic_group");
  CHECK(read(e1 + "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\">") == "network.parse");
  CHECK(read("<entry id=\"x\" name=\"t:1\" type=\"gene\"/>\n") == "network.parse");
  // The message names the file and the line.
  const auto p = c.scratch.write("dangling.xml", kgml(e1 + "<relation entry1=\"1\" entry2=\"7\" type=\"PPrel\"/>\n"));
  try {
    net::read_kgml(p, net::IdMap::none(), c.ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.where().line == 4);
    CHECK(std::string(f.what()).find("dangling.xml:4:") != std::string::npos);
    CHECK(std::string(f.what()).find("entry 7") != std::string::npos);
  }
  CHECK(failure_code([&] { net::read_kgml(c.scratch.dir() / "none.xml", net::IdMap::none(), c.ctx); }) == "input.missing");
  // Two spaces in a name no longer make an empty id; a groups of groups is fine.
  const auto ok = c.scratch.write("ok.xml", kgml("<entry id=\"1\" name=\"t:1  t:2 \" type=\"gene\"/>\n"
                                                  "<entry id=\"2\" name=\"undefined\" type=\"group\"><component id=\"1\"/></entry>\n"
                                                  "<entry id=\"3\" name=\"undefined\" type=\"group\"><component id=\"2\"/><component id=\"1\"/></entry>\n"
                                                  "<relation entry1=\"3\" entry2=\"1\" type=\"PPrel\"/>\n"));
  const auto doc = net::read_kgml(ok, net::IdMap::none(), c.ctx);
  CHECK(ids_of(*doc, *doc->entry(1)) == std::vector<std::string>{"t1", "t2"});
}

TEST_CASE("network: scores: 625 reads as 625; 625x and abc are unscored with network.bad_score; absent is quiet", "[unit][network]") {
  Context c;
  const auto p = c.scratch.write("scores.xml", kgml("<entry id=\"1\" name=\"t:1\" type=\"gene\"/>\n"
                                                    "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" experiment_score=\"625\"/>\n"
                                                    "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" experiment_score=\"625x\"/>\n"
                                                    "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" experiment_score=\"abc\"/>\n"
                                                    "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\" experiment_score=\"1.5e2\"/>\n"
                                                    "<relation entry1=\"1\" entry2=\"1\" type=\"PPrel\"/>\n"));
  const auto doc = net::read_kgml(p, net::IdMap::none(), c.ctx);
  const auto r = doc->relations();
  CHECK(r[0].score == 625.0);
  CHECK_FALSE(r[1].score);
  CHECK_FALSE(r[2].score);
  CHECK(r[3].score == 150.0);
  CHECK_FALSE(r[4].score);
  REQUIRE(c.lines->count("network.bad_score") == 1);        // one warning, naming both
  CHECK(c.lines->text("network.bad_score").find("2 score(s)") != std::string::npos);
}

TEST_CASE("network: relation filters: indirect, ECrel and maplink (S2.11, S2.31, 02.C6, 02.C7)", "[unit][network]") {
  Context c;
  const auto doc = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  using V = std::vector<std::uint32_t>;
  CHECK(net::kept_relations(*doc, {}) == V{0, 1, 2, 3, 4, 6, 8, 9, 10});
  CHECK(net::kept_relations(*doc, {net::IndirectMode::keep, net::EcrelMode::keep, net::MaplinkMode::skip}) ==
        V{0, 1, 2, 3, 4, 5, 6, 8, 9, 10});
  CHECK(net::kept_relations(*doc, {net::IndirectMode::discard, net::EcrelMode::exclude, net::MaplinkMode::skip}) ==
        V{0, 1, 2, 3, 4, 8, 9, 10});
  CHECK(net::kept_relations(*doc, {net::IndirectMode::discard, net::EcrelMode::keep, net::MaplinkMode::keep}) ==
        V{0, 1, 2, 3, 4, 6, 7, 8, 9, 10});
}

TEST_CASE("network: split_entry: exceptions first, then the box above the threshold (S2.07)", "[unit][network]") {
  Context c;
  const auto doc = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const net::Entry& e = *doc->entry(3);                     // t10, t4, t5
  auto texts = [&](const std::vector<core::GeneId>& v) {
    std::vector<std::string> out;
    for (auto g : v) out.emplace_back(doc->interner().str(g));
    return out;
  };
  using V = std::vector<std::string>;
  const net::IdSet none, e4({"t4"}), all({"t10", "t4", "t5"});
  auto s = net::split_entry(*doc, e, none, 1);
  CHECK(texts(s.box) == V{"t10", "t4", "t5"});              // key t10: "t10" < "t4" in byte order
  CHECK(s.singles.empty());
  s = net::split_entry(*doc, e, e4, 1);                     // the exception stands alone
  CHECK(texts(s.singles) == V{"t4"});
  CHECK(texts(s.box) == V{"t10", "t5"});
  s = net::split_entry(*doc, e, e4, 0);                     // 0 behaves like 1
  CHECK(texts(s.box) == V{"t10", "t5"});
  s = net::split_entry(*doc, e, none, 3);                   // not more than the threshold: singles
  CHECK(texts(s.singles) == V{"t10", "t4", "t5"});
  CHECK(s.box.empty());
  s = net::split_entry(*doc, e, e4, 2);                     // 3 ids counting the exception: collapses
  CHECK(texts(s.box) == V{"t10", "t5"});
  s = net::split_entry(*doc, e, none, -1);                  // negative: never collapses
  CHECK(texts(s.singles) == V{"t10", "t4", "t5"});
  s = net::split_entry(*doc, e, net::IdSet({"t10", "t4"}), 1);   // a box of one id is a single
  CHECK(texts(s.singles) == V{"t10", "t4", "t5"});
  CHECK(s.box.empty());
  s = net::split_entry(*doc, e, all, 1);
  CHECK(texts(s.singles) == V{"t10", "t4", "t5"});
}

TEST_CASE("network: format detection reaches the right reader, and y|n is checked against it (02.C1, S2.12)", "[unit][network]") {
  Context c;
  using net::Format;
  struct Case { std::string name, text; Format format; };
  const std::vector<Case> cases = {
      {"kgml.xml", "\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<!DOCTYPE pathway SYSTEM \"x.dtd\">\n<!-- <pathway> -->\n"
                   "<pathway name=\"path:t1\" org=\"tst\" number=\"1\">\n</pathway>\n", Format::kgml},
      {"fusion.xml", "<?xml version=\"1.0\"?>\n<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\"/>\n",
       Format::fusion_network},
      {"wgx.xml", "<?xml version=\"1.0\"?>\n<!DOCTYPE pathway SYSTEM \"http://www.kegg.jp/kegg/xml/KGML_v0.7.1_.dtd\">\n"
                  "<pathway name=\"KEGG\">\n</pathway>\n", Format::legacy},
      {"add.xml", "<?xml version=\"1.0\"?>\n<!DOCTYPE pathway SYSTEM \"Additional edges added\">\n<pathway name=\"KEGG\">\n",
       Format::legacy},
      {"table.tsv", "# a comment\n\nsource\ttarget\ttype\nt1\tt2\tPPrel\n", Format::edge_table},
      {"add.txt", "\n# edges\n\nt1 t2 PPrel activation\n", Format::edge_table},
  };
  for (const auto& k : cases) {
    INFO(k.name);
    CHECK(net::sniff(c.scratch.write(k.name, k.text), std::nullopt, c.ctx).format == k.format);
  }
  CHECK(net::sniff(c.scratch.write("v.xml", "<pathway format=\"fusion-network\" version=\"1\"/>"), std::nullopt, c.ctx).version == 1);
  CHECK(failure_code([&] { net::sniff(c.scratch.write("u.txt", "hello world\n"), std::nullopt, c.ctx); }) == "network.unknown_format");
  CHECK(failure_code([&] { net::sniff(c.scratch.write("u.xml", "<sbml level=\"3\"/>"), std::nullopt, c.ctx); }) == "network.unknown_format");
  CHECK(failure_code([&] { net::sniff(c.scratch.write("e.txt", "\n# only\n"), std::nullopt, c.ctx); }) == "network.unknown_format");
  CHECK(c.lines->count("network.flag_mismatch") == 0);
  // y on a raw KGML would have given edges with no pathway in FuSION 1: one warning, read as KGML.
  net::sniff(c.scratch.dir() / "kgml.xml", true, c.ctx);
  REQUIRE(c.lines->count("network.flag_mismatch") == 1);
  CHECK(c.lines->text("network.flag_mismatch") == "rgx was given y, but " + core::to_utf8(c.scratch.dir() / "kgml.xml") +
                                                      " is KGML; read as KGML");
  net::sniff(c.scratch.dir() / "wgx.xml", true, c.ctx);       // y on a FuSION 1-written file agrees
  net::sniff(c.scratch.dir() / "fusion.xml", true, c.ctx);
  net::sniff(c.scratch.dir() / "kgml.xml", false, c.ctx);
  CHECK(c.lines->count("network.flag_mismatch") == 1);
  net::sniff(c.scratch.dir() / "wgx.xml", false, c.ctx);
  CHECK(c.lines->count("network.flag_mismatch") == 2);
}

TEST_CASE("network: a step's documents are parsed once per file and map (09.C3, S7.19)", "[unit][network]") {
  Context c;
  net::StepDocs docs;
  const auto a = docs.get(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const auto b = docs.get(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  CHECK(a.get() == b.get());
  const auto t = docs.get(data("tst00001.tsv"), net::IdMap::none(), c.ctx);
  CHECK(t->source().format == net::Format::edge_table);
  const auto f = c.scratch.write("net.xml", "<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\"/>");
  CHECK(failure_code([&] { docs.get(f, net::IdMap::none(), c.ctx); }) == "network.unknown_format");
  net::StepDocs other;                                      // never shared between steps
  CHECK(other.get(data("tst00001.xml"), net::IdMap::none(), c.ctx).get() != a.get());
}

TEST_CASE("network: the exchange format: boxes, complexes, per-component types and defaults (02.C2)", "[unit][network]") {
  Context c;
  const auto doc = net::read_edge_table(data("tst00001.tsv"), net::IdMap::none(), c.ctx);
  CHECK(doc->source().format == net::Format::edge_table);
  CHECK(doc->source().pathway == "tst00001");               // the stem
  REQUIRE(doc->relations().size() == 11);
  const auto& in = doc->interner();
  const net::Relation& cx = doc->relations()[3];           // t6+t2,t3 -> t7
  const net::Entry& group = *doc->entry(cx.entry1);
  CHECK(in.str(group.type) == "group");
  REQUIRE(group.comps_count == 2);
  CHECK(ids_of(*doc, *doc->entry(doc->components(group)[1])) == std::vector<std::string>{"t2", "t3"});
  const net::Relation& pc = doc->relations()[10];          // cpd:C00001 -> t1, through the id rule
  CHECK(ids_of(*doc, *doc->entry(pc.entry1)) == std::vector<std::string>{"cpdC00001"});
  CHECK(in.str(doc->entry(pc.entry1)->type) == "compound");
  CHECK(in.str(doc->entry(pc.entry2)->type) == "gene");     // the default type
  CHECK(subtypes_of(*doc, doc->relations()[4]) == std::vector<std::string>{"association"});
  CHECK_FALSE(doc->relations()[1].score);                   // 625x
  CHECK(c.lines->count("network.bad_score") == 1);
  CHECK(in.str(doc->entry(doc->relations()[7].entry2)->type) == "map");
  std::vector<std::string> genes;
  for (core::GeneId g : doc->gene_ids()) genes.emplace_back(in.str(g));
  CHECK(genes == std::vector<std::string>{"t1", "t10", "t12", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9"});
}

TEST_CASE("network: edge-table and add-form rows are checked (02.C2, S2.30)", "[unit][network]") {
  Context c;
  auto read = [&](const std::string& text) {
    const auto p = c.scratch.write("t.tsv", text);
    return failure_code([&] { net::read_edge_table(p, net::IdMap::none(), c.ctx); });
  };
  CHECK(read("source\ttarget\tweight\nt1\tt2\t3\n") == "network.bad_row");            // unknown column
  CHECK(read("source\ttarget\tsource\nt1\tt2\tt3\n") == "network.bad_row");           // repeated column
  CHECK(read("source\ttarget\nt1\tt2\tt3\n") == "network.bad_row");                   // too many cells
  CHECK(read("source\ttarget\ttype\nt1\t\tPPrel\n") == "network.bad_row");            // no target
  CHECK(read("source\ttarget\tsource_type\nt1+t2\tt3\tgene+gene+gene\n") == "network.bad_row");   // type cell
  CHECK(read("source\ttarget\ttype\nt1\tt2\tPPRel\n") == "network.type_spelling");
  CHECK(read("t1 t2 PPrel activation\nt1 t2 PPrel\n") == "network.bad_row");          // add's form: 4 fields
  CHECK(read("t1 t2 GERel expression\n") == "network.type_spelling");
  try {
    const auto p = c.scratch.write("t.tsv", "t1 t2 PPrel activation\n\nt1 t2 PPrel activation extra\n");
    net::read_edge_table(p, net::IdMap::none(), c.ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "network.bad_row");
    CHECK(f.where().line == 3);
  }
  // add's form: unknown types and subtypes are warnings; ids pass the id rule.
  const auto p = c.scratch.write("add.txt", "# extra edges\nhsa:1 hsa:2 PPrel activation\nhsa2 hsa3 NEWrel binding/association\nhsa3 hsa1 PPrel wobble\n");
  const auto doc = net::read_edge_table(p, net::IdMap::none(), c.ctx);
  REQUIRE(doc->relations().size() == 3);
  CHECK(ids_of(*doc, *doc->entry(doc->relations()[0].entry1)) == std::vector<std::string>{"hsa1"});
  CHECK(subtypes_of(*doc, doc->relations()[1]) == std::vector<std::string>{"association"});
  CHECK(c.lines->count("network.unknown_type") == 1);
  CHECK(c.lines->text("network.unknown_type").find("NEWrel") != std::string::npos);
  CHECK(c.lines->count("network.unknown_subtype") == 1);
  CHECK(c.lines->text("network.unknown_subtype").find("wobble") != std::string::npos);
}

TEST_CASE("network: a KGML and its edge-table transcription build equal networks (02.C2 test hook)", "[unit][network]") {
  Context c;
  const auto a = net::read_kgml(data("tst00001.xml"), net::IdMap::none(), c.ctx);
  const auto b = net::read_edge_table(data("tst00001.tsv"), net::IdMap::none(), c.ctx);
  for (const net::BuildSettings& s : {net::BuildSettings{},
                                      net::BuildSettings{1, {net::IndirectMode::keep, net::EcrelMode::exclude, net::MaplinkMode::keep}},
                                      net::BuildSettings{15000, {}}}) {
    for (const net::IdSet& e : {net::IdSet(), net::IdSet({"t4", "t3"})}) {
      const auto na = build_one(*a, s, e, c.ctx);
      const auto nb = build_one(*b, s, e, c.ctx);
      CHECK(dump(*na) == dump(*nb));
      CHECK(na->content_hash() == nb->content_hash());
    }
  }
}
