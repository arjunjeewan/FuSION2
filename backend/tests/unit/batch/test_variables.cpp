// Section 01's test hooks for variables: longest match, single pass, "$$", unmatched '$' (S1.07, S1.08,
// 01.C2). FuSION 1 took the first key in byte order (Linux/src/main.cc:1197-1247), so $PLOT_FILE hid
// $PLOT_FILE_WO_GENE in the 12 runs/real_experiments/ batches.
#include <catch2/catch_test_macros.hpp>

#include "fusion/batch/batch.hpp"

using namespace fusion;
using Mode = batch::Variables::Mode;

TEST_CASE("batch: the longest defined key wins (S1.07)", "[batch][variables]") {
  batch::Variables v;
  REQUIRE(v.define("$PLOT_FILE", "PO.png", 1));
  REQUIRE(v.define("$PLOT_FILE_WO_GENE", "PO_wo.png", 2));
  CHECK(v.expand("$PLOT_FILE_WO_GENE", Mode::argument).text == "PO_wo.png");
  CHECK(v.expand("$PLOT_FILE", Mode::argument).text == "PO.png");
  CHECK(v.expand("$PLOT_FILE_WO", Mode::argument).text == "PO.png_WO");   // _WO alone is no key

  batch::Variables w;
  w.define("$ACTIVE_W", "w", 1);
  w.define("$ACTIVE_WO", "wo", 2);
  CHECK(w.expand("$WRITE/$ACTIVE_WO.txt", Mode::argument).text == "$WRITE/wo.txt");
  CHECK(w.expand("$ACTIVE_W.txt", Mode::argument).text == "w.txt");
}

TEST_CASE("batch: keys need no delimiter and values are never rescanned (S1.07)", "[batch][variables]") {
  batch::Variables v;
  v.define("$SRC_NODE", "hsa1956", 1);
  v.define("$TGT_NODE", "hsa3576", 2);
  v.define("$B", "10", 3);
  v.define("$LOOP", "$LOOP", 4);
  // Line 219 of the real batch's pattern.
  CHECK(v.expand("$SRC_NODE_to_$TGT_NODE_b$B", Mode::argument).text == "hsa1956_to_hsa3576_b10");
  CHECK(v.expand("$LOOP$LOOP", Mode::argument).text == "$LOOP$LOOP");
}

TEST_CASE("batch: \"$$\" is one '$' in arguments and stays for the shell (01.C2)", "[batch][variables]") {
  batch::Variables v;
  v.define("$X", "x", 1);
  CHECK(v.expand("a$$X", Mode::argument).text == "a$X");
  CHECK(v.expand("a$$X", Mode::exec).text == "a$$X");
  CHECK(v.expand("$$$X", Mode::argument).text == "$x");
  CHECK(v.expand("echo $$ $X", Mode::exec).text == "echo $$ x");
}

TEST_CASE("batch: an unmatched $ passes through, with a name to warn about outside exec", "[batch][variables]") {
  batch::Variables v;
  v.define("$WRITE_DIR", "/w", 1);
  auto arg = v.expand("$WRITE_DIR/$MISSPELT and $1 $(date) $", Mode::argument);
  CHECK(arg.text == "/w/$MISSPELT and $1 $(date) $");
  CHECK(arg.unmatched == std::vector<std::string>{"$MISSPELT"});
  auto shell = v.expand("awk '{print $1}' $(date) $HOME", Mode::exec);
  CHECK(shell.text == "awk '{print $1}' $(date) $HOME");
  CHECK(shell.unmatched.empty());
  CHECK(v.expand("$UNDEF $UNDEF", Mode::argument).unmatched.size() == 1);
}

TEST_CASE("batch: a second definition is refused, and the first is kept (S1.08)", "[batch][variables]") {
  batch::Variables v;
  CHECK(v.define("$X", "1", 3));
  CHECK_FALSE(v.define("$X", "2", 9));
  CHECK(v.expand("$X", Mode::argument).text == "1");
  CHECK(v.defined_at("$X") == std::optional<int>(3));
  CHECK_FALSE(v.defined_at("$Y"));
  CHECK_FALSE(v.defined_at("$"));
  v.define("$EMPTY", "", 4);
  CHECK(v.expand("[$EMPTY]", Mode::argument).text == "[]");
  const auto order = v.in_order();
  REQUIRE(order.size() == 2);
  CHECK(order[0] == std::make_tuple(std::string("$X"), std::string("1"), 3));
  CHECK(order[1] == std::make_tuple(std::string("$EMPTY"), std::string(""), 4));
}

TEST_CASE("batch: a let key is '$' then a letter or _, then letters, digits and _", "[batch][variables]") {
  CHECK(batch::valid_variable_key("$A"));
  CHECK(batch::valid_variable_key("$_x9"));
  CHECK(batch::valid_variable_key("$FUSION_HOME"));   // valid, but reserved: the check refuses it
  CHECK_FALSE(batch::valid_variable_key("$"));
  CHECK_FALSE(batch::valid_variable_key("A"));
  CHECK_FALSE(batch::valid_variable_key("$9"));
  CHECK_FALSE(batch::valid_variable_key("$A-B"));
  CHECK_FALSE(batch::valid_variable_key("$A$B"));
}
