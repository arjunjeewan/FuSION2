// The id rule, the id map and the label maps (02.C3; S1.16, S1.18, S1.19, S2.15; item 1.12; entry 229).
#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

#include "fusion/core/install.hpp"
#include "support.hpp"

using namespace network_test;

TEST_CASE("network: one reader for both label maps (S1.18, S1.19)", "[unit][network]") {
  Context c;
  // A BOM, CRLF, a name with a tab inside, a blank line, a later line winning, an id with no name,
  // and a line that holds only a tab.
  const auto path = c.scratch.write("names.tsv",
                                    "\xEF\xBB\xBFp:1\tMetabolic_pathways\r\n"
                                    "pathway_9\tNectin/Necl\ttrans_heterodimerization\r\n"
                                    "\r\n"
                                    "t7\tOLD\n"
                                    "t7\tGGG\n"
                                    "t8\n"
                                    "\t\n"
                                    "#source\tsynthetic, 2026-11-18\n");
  const net::LabelMap map = net::LabelMap::read(path, c.ctx);
  CHECK(map.find("p:1") == "Metabolic_pathways");
  CHECK(map.find("pathway_9") == "Nectin/Necl_trans_heterodimerization");
  CHECK(map.find("t7") == "GGG");                           // a later line wins
  CHECK_FALSE(map.find("t8"));                              // no name: falls back to the id
  CHECK(map.label_or_id("t8") == "t8");
  CHECK(map.label_or_id("t99") == "t99");
  CHECK_FALSE(map.find("t99"));
  CHECK(map.size() == 5);                                   // p:1, pathway_9, t7, t8, #source
  CHECK_FALSE(map.find("t99"));                             // lookups never insert (S2.15)
  CHECK(map.size() == 5);
  CHECK(map.digest() == core::Sha256::of(read_file(path)));
  const auto rows = map.rows();
  REQUIRE(rows.size() == 5);
  CHECK(rows[0].id == "#source");                           // byte order of id
  CHECK(rows[1].id == "p:1");
  CHECK(rows[4].id == "t8");
  CHECK(rows[3].line == 5);                                 // t7's winning line
  const net::LabelMap empty;
  CHECK(empty.size() == 0);
  CHECK(empty.label_or_id("x") == "x");
}

TEST_CASE("network: S1.16's two-column reader, with the colon rule and warnings (02.C3)", "[unit][network]") {
  Context c;
  const auto path = c.scratch.write("ids.txt",
                                    "a1 t1\n"
                                    "x:2  t:2 extra tokens\n"
                                    "a3 t3\n"
                                    "a3 t30\n"
                                    "lonely\n"
                                    "\n");
  const net::IdMap map = net::IdMap::read(path, c.ctx);
  CHECK_FALSE(map.is_none());
  core::Interner in;
  CHECK(in.str(map.canonical("a1", in)) == "t1");
  CHECK(in.str(map.canonical("x2", in)) == "t2");          // ':' deleted on both columns
  CHECK(in.str(map.canonical("x:2", in)) == "t2");
  CHECK(in.str(map.canonical("a3", in)) == "t30");         // a later line wins
  CHECK(in.str(map.canonical("t1", in)) == "t1");          // mapped once, no chaining
  CHECK(in.str(map.canonical("hsa:3725", in)) == "hsa3725");   // an id the run-wide map lacks passes through
  CHECK(map.apply("lonely").empty());                      // a one-token line maps its id to nothing
  CHECK(in.str(*map.lookup("a1", in)) == "t1");
  CHECK_FALSE(map.lookup("hsa3725", in));                  // a value input's own map drops it (entry 229)
  CHECK_FALSE(map.lookup("lonely", in));
  CHECK(c.lines->count("network.id_map_line") == 1);
  CHECK(c.lines->text("network.id_map_line").find("line 5 (lonely)") != std::string::npos);
  CHECK(c.lines->count("network.id_map_repeat") == 1);
  CHECK(c.lines->text("network.id_map_repeat").find("a3: t3 then t30") != std::string::npos);
  CHECK(map.digest() == core::Sha256::of(read_file(path)));

  const net::IdMap none = net::IdMap::none();
  CHECK(none.is_none());
  CHECK(none.digest() == core::Sha256{});
  CHECK(in.str(none.canonical("hsa:1956", in)) == "hsa1956");
  CHECK(in.str(*none.lookup("hsa:1956", in)) == "hsa1956");   // no map: the id rule's colon step only
  CHECK(net::IdMap().digest() == none.digest());

  // An empty file is a map that maps nothing: every lookup misses, as FuSION 1's empty
  // inter_db_mapping_file did.
  const net::IdMap empty = net::IdMap::read(c.scratch.write("empty", ""), c.ctx);
  CHECK_FALSE(empty.is_none());
  CHECK_FALSE(empty.lookup("t1", in));
  CHECK(empty.apply("t:1") == "t1");
}

TEST_CASE("network: id_map=symbols reads the symbol map backwards and reports ambiguous symbols (1.12)", "[unit][network]") {
  Context c;
  const auto path = c.scratch.write("symbols.tsv", "t1\tAAA\nt2\tBBB\nt3\tAAA\nt4\t\nt5\tCCC\n");
  const net::LabelMap symbols = net::LabelMap::read(path, c.ctx);
  const net::IdMap reverse = net::IdMap::reverse_symbols(symbols, c.ctx);
  core::Interner in;
  CHECK(in.str(*reverse.lookup("AAA", in)) == "t3");        // the later line wins
  CHECK(in.str(*reverse.lookup("BBB", in)) == "t2");
  CHECK_FALSE(reverse.lookup("t4", in));
  CHECK_FALSE(reverse.lookup("DDD", in));
  CHECK(c.lines->count("network.id_map_repeat") == 1);
  CHECK(c.lines->text("network.id_map_repeat").find("1 symbol(s)") != std::string::npos);
  CHECK(c.lines->text("network.id_map_repeat").find("AAA: t1, t3") != std::string::npos);
  CHECK(reverse.digest() != symbols.digest());
  CHECK(reverse.digest() != core::Sha256{});
}

TEST_CASE("network: missing map files fail as input errors (decision 0021)", "[unit][network]") {
  Context c;
  try {
    net::LabelMap::read(c.scratch.dir() / "nope.tsv", c.ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "input.missing");
  }
  try {
    net::IdMap::read(c.scratch.dir(), c.ctx);
    FAIL("no failure");
  } catch (const core::Failure& f) {
    CHECK(f.code() == "input.not_file");
  }
}

TEST_CASE("network: IdSet sorts by content and removes repeats", "[unit][network]") {
  const net::IdSet s({"t2", "t10", "t1", "t2"});
  REQUIRE(s.size() == 3);
  CHECK(s.ids()[0] == "t1");
  CHECK(s.ids()[1] == "t10");                               // byte order
  CHECK(s.ids()[2] == "t2");
  CHECK(s.contains("t10"));
  CHECK_FALSE(s.contains("t3"));
  CHECK(net::IdSet().empty());
}

TEST_CASE("network: the shipped maps come from the home's data/ (2B.4, 11.C20)", "[unit][network]") {
  Context c;
  const fs::path home = data("home");
  ::setenv("FUSION_HOME", home.c_str(), 1);                 // read once per process by core
  if (fusion::core::install_dir().home != fs::absolute(home)) {
    WARN("the install directory was worked out before this test; skipped");
    return;
  }
  const net::Maps maps = net::shipped_maps(c.ctx);
  CHECK(maps.symbols.find("t2") == "BBB");
  CHECK(maps.pathway_names.find("path:r1") == "A synthetic pathway");
  CHECK(maps.ids.is_none());                                // no id map ships
}
