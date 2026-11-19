// Input lists and the KGML directory (02.C12; S2.16, S8.31; items 1.24, 2B.5, 2B.10, 2B.12; 03.C6).
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace network_test;

namespace {

std::string kgml_named(const std::string& pathway) {
  return "<?xml version=\"1.0\"?>\n<pathway name=\"" + pathway + "\" org=\"tst\" number=\"1\">\n</pathway>\n";
}

std::string failure(const std::function<void()>& f, std::string* message = nullptr) {
  try {
    f();
  } catch (const core::Failure& e) {
    if (message) *message = e.what();
    return e.code();
  }
  return "none";
}

}  // namespace

TEST_CASE("network: list entries are trimmed and resolve against the base; bare pathway ids name <base>/<id>.xml (02.C12)",
          "[unit][network]") {
  Context c;
  const fs::path kegg = c.scratch.dir() / "kegg";
  c.scratch.write("kegg/hsa1.xml", kgml_named("path:hsa1"));
  c.scratch.write("kegg/hsa3.xml", kgml_named("path:hsa3"));
  c.scratch.write("kegg/sub/hsa2.xml", kgml_named("path:hsa2"));
  const fs::path absolute = c.scratch.write("elsewhere/edges.tsv", "source\ttarget\nt1\tt2\n");
  const fs::path list = c.scratch.write("lists/candidates.txt", "  hsa1  \r\n\r\npath:hsa3\nsub/hsa2.xml\n" +
                                                                 core::to_utf8(absolute) + "\n\n");
  const net::PathList l = net::read_input_list(list, kegg, c.ctx);
  CHECK(l.list == list);
  CHECK(l.base == kegg);
  REQUIRE(l.lines.size() == 4);
  CHECK(l.lines[0].text == "hsa1");
  CHECK(l.lines[0].path == kegg / "hsa1.xml");
  CHECK(l.lines[1].text == "path:hsa3");
  CHECK(l.lines[1].path == kegg / "hsa3.xml");
  CHECK(l.lines[2].text == "sub/hsa2.xml");
  CHECK(l.lines[2].path == kegg / "sub/hsa2.xml");
  CHECK(l.lines[3].path == absolute);
}

TEST_CASE("network: list_base: kgml_dir, else the base a step recorded, else the list's own folder (02.C12, 2B.10)", "[unit][network]") {
  const fs::path list = fs::path("/runs/r1/gsea/shortlist.txt");
  CHECK(net::list_base(list, fs::path("/data/kegg"), fs::path("/old")) == fs::path("/data/kegg"));
  CHECK(net::list_base(list, std::nullopt, fs::path("/old")) == fs::path("/old"));
  CHECK(net::list_base(list, std::nullopt, std::nullopt) == fs::path("/runs/r1/gsea"));
  CHECK(net::list_base(fs::path("list.txt"), std::nullopt, std::nullopt) == fs::path("."));
}

TEST_CASE("network: every missing listed file is named in one failure (open question 6)", "[unit][network]") {
  Context c;
  c.scratch.write("kegg/hsa1.xml", kgml_named("path:hsa1"));
  const fs::path list = c.scratch.write("l.txt", "hsa1\nhsa404\nhsa405\n\nmissing/file.xml\n");
  std::string message;
  CHECK(failure([&] { net::read_input_list(list, c.scratch.dir() / "kegg", c.ctx); }, &message) == "input.missing");
  CHECK(message.find("3 listed files are missing") != std::string::npos);
  CHECK(message.find("hsa404.xml (line 2)") != std::string::npos);
  CHECK(message.find("hsa405.xml (line 3)") != std::string::npos);
  CHECK(message.find("missing/file.xml (line 5)") != std::string::npos);
  CHECK(failure([&] { net::read_input_list(c.scratch.dir() / "none.txt", c.scratch.dir(), c.ctx); }) == "input.missing");
}

TEST_CASE("network: a file listed twice is read once, with a warning", "[unit][network]") {
  Context c;
  c.scratch.write("kegg/hsa1.xml", kgml_named("path:hsa1"));
  c.scratch.write("kegg/hsa2.xml", kgml_named("path:hsa2"));
  const fs::path list = c.scratch.write("l.txt", "hsa1\nhsa2\n./hsa1.xml\n");
  const net::PathList l = net::read_input_list(list, c.scratch.dir() / "kegg", c.ctx);
  REQUIRE(l.lines.size() == 2);
  CHECK(l.lines[0].text == "hsa1");
  CHECK(l.lines[1].text == "hsa2");
  CHECK(c.lines->count("list.duplicate_entry") == 1);
  CHECK(c.lines->text("list.duplicate_entry").find("line 3") != std::string::npos);
}

TEST_CASE("network: two KGMLs declaring one pathway id fail, naming both (1.24, 03.C6)", "[unit][network]") {
  Context c;
  c.scratch.write("a/hsa04010.xml", kgml_named("path:hsa04010"));
  c.scratch.write("b/hsa04010.xml", kgml_named("path:hsa04010") + "<!-- another copy -->\n");
  const fs::path list = c.scratch.write("l.txt", "a/hsa04010.xml\nb/hsa04010.xml\n");
  std::string message;
  CHECK(failure([&] { net::read_input_list(list, c.scratch.dir(), c.ctx); }, &message) == "list.duplicate_pathway");
  CHECK(message.find("a/hsa04010.xml") != std::string::npos);
  CHECK(message.find("b/hsa04010.xml") != std::string::npos);
  // Network files all carry the root name KEGG; only KGMLs are compared.
  c.scratch.write("n1.xml", "<pathway name=\"KEGG\" format=\"fusion-network\" version=\"1\"/>");
  c.scratch.write("n2.xml", "<pathway name=\"KEGG\"><entry id=\"1\" name=\"t1\" type=\"gene\"/></pathway>");
  const fs::path networks = c.scratch.write("n.txt", "n1.xml\nn2.xml\n");
  CHECK(net::read_input_list(networks, c.scratch.dir(), c.ctx).lines.size() == 2);
}
