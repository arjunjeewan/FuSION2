// Section 12's batch row, and section 01's test hooks: tokens, quoting, comments, CRLF and UTF-8
// (S1.05, S1.06, 01.C2, 01.C19).
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "fusion/batch/batch.hpp"

using namespace fusion;
using batch::LineKind;

namespace {
std::vector<std::string> texts(const batch::Line& line) {
  std::vector<std::string> out;
  for (const auto& t : line.tokens) out.push_back(t.text);
  return out;
}
}  // namespace

TEST_CASE("batch: runs of spaces and tabs separate tokens (S1.05)", "[batch][lexer]") {
  const auto line = batch::lex(3, "  cudf\t$LFC   1.01 \t -1.01\t");
  CHECK(line.number == 3);
  CHECK(line.kind == LineKind::command);
  CHECK(texts(line) == std::vector<std::string>{"cudf", "$LFC", "1.01", "-1.01"});
  CHECK(line.tokens[0].column == 3);
  CHECK(line.tokens[1].column == 8);
  CHECK(line.rest == "$LFC   1.01 \t -1.01\t");
}

TEST_CASE("batch: comments and blank lines (S1.06)", "[batch][lexer]") {
  CHECK(batch::lex(1, "").kind == LineKind::blank);
  CHECK(batch::lex(1, " \t \t").kind == LineKind::blank);   // a tab-only line is blank, not a command
  CHECK(batch::lex(1, ";; a comment").kind == LineKind::comment);
  CHECK(batch::lex(1, "   ;;text").kind == LineKind::comment);
  CHECK(batch::lex(1, ";;").kind == LineKind::comment);
  const auto hash = batch::lex(1, "# not special");
  CHECK(hash.kind == LineKind::command);
  CHECK(hash.tokens[0].text == "#");
  const auto inline_comment = batch::lex(1, "sz 0 ;; trailing");
  CHECK(texts(inline_comment) == std::vector<std::string>{"sz", "0", ";;", "trailing"});
}

TEST_CASE("batch: double quotes, \"\" and the literal backslash (01.C2)", "[batch][lexer]") {
  const auto line = batch::lex(1, R"(let $IN "C:\Users\Arjun Jeewan\FuSION\inputs")");
  REQUIRE(line.tokens.size() == 3);
  CHECK(line.tokens[2].text == R"(C:\Users\Arjun Jeewan\FuSION\inputs)");
  CHECK(line.tokens[2].quoted);
  CHECK_FALSE(line.tokens[1].quoted);

  const auto named = batch::lex(1, R"(cudf values="a b.tsv" up=1)");
  CHECK(named.tokens[1].text == "values=a b.tsv");
  REQUIRE(named.tokens[1].name_end);
  CHECK(*named.tokens[1].name_end == 6);

  const auto doubled = batch::lex(1, R"(x "say ""hi""" "")");
  CHECK(doubled.tokens[1].text == R"(say "hi")");
  CHECK(doubled.tokens[2].text.empty());
  CHECK(doubled.tokens[2].quoted);

  const auto mid = batch::lex(1, R"(x pre"mid dle"post)");
  CHECK(mid.tokens[1].text == "premid dlepost");

  const auto windows = batch::lex(1, R"(cudf values=C:\data\lfc.tsv)");
  CHECK(windows.tokens[1].text == R"(values=C:\data\lfc.tsv)");
}

TEST_CASE("batch: a quoted '=' never makes a name (01.C1)", "[batch][lexer]") {
  CHECK_FALSE(batch::lex(1, R"(x "x=y")").tokens[1].name_end);
  CHECK_FALSE(batch::lex(1, R"(x "name"=y)").tokens[1].name_end);
  CHECK_FALSE(batch::lex(1, "x Name=y").tokens[1].name_end);
  CHECK_FALSE(batch::lex(1, "x 9a=y").tokens[1].name_end);
  CHECK_FALSE(batch::lex(1, "x =y").tokens[1].name_end);
  CHECK_FALSE(batch::lex(1, "x $V=y").tokens[1].name_end);
  CHECK(batch::lex(1, "x a_9=").tokens[1].name_end == std::optional<std::size_t>(3));
  CHECK(batch::lex(1, "x selector=fdr:0.25=x").tokens[1].name_end == std::optional<std::size_t>(8));
  // A run line's dotted keys (row 2B.2) are keys, not names.
  const auto key = batch::lex(1, "run network.indirect=keep");
  CHECK_FALSE(key.tokens[1].name_end);
  CHECK(key.tokens[1].key_end == std::optional<std::size_t>(16));
}

TEST_CASE("batch: an unterminated quote is recorded with its column", "[batch][lexer]") {
  const auto line = batch::lex(7, R"(cudf values="a b)");
  REQUIRE(line.unterminated_quote);
  CHECK(*line.unterminated_quote == 13);
  CHECK_FALSE(batch::lex(7, R"(x "a")").unterminated_quote);
}

TEST_CASE("batch: exec keeps the rest of the line as written (S1.14)", "[batch][lexer]") {
  const auto line = batch::lex(244, R"~(exec echo "[egfr-cxcl8-jun] pathz3 wo  FSC  : start $(date)" >> $WRITE_DIR/timing.log)~");
  CHECK(line.rest == R"~(echo "[egfr-cxcl8-jun] pathz3 wo  FSC  : start $(date)" >> $WRITE_DIR/timing.log)~");
  const auto quoted = batch::lex(1, R"~(exec awk 'NR==FNR{a=$1;next}' "unterminated)~");
  CHECK(quoted.rest == R"~(awk 'NR==FNR{a=$1;next}' "unterminated)~");
  CHECK(quoted.unterminated_quote);   // an error only if the line is not exec
}

TEST_CASE("batch: the source: CRLF, a BOM and invalid UTF-8 (S1.01, 01.C19)", "[batch][lexer]") {
  const auto crlf = batch::source_from_text("/x/a.batch", "let $A 1\r\nsz 0\r\nexit");
  CHECK(crlf.lines == std::vector<std::string>{"let $A 1", "sz 0", "exit"});
  CHECK(crlf.sha256 == core::Sha256::of("let $A 1\r\nsz 0\r\nexit"));

  const auto bom = batch::source_from_text("/x/a.batch", "\xEF\xBB\xBFsz 0\n");
  CHECK(bom.lines == std::vector<std::string>{"sz 0"});

  const auto trailing = batch::source_from_text("/x/a.batch", "a\nb\n");
  CHECK(trailing.lines.size() == 2);

  const auto dash = batch::source_from_text("/x/a.batch", ";; a \xE2\x80\x94 dash\nexit\n");
  CHECK_FALSE(dash.invalid_utf8_line);

  const auto bad = batch::source_from_text("/x/a.batch", "exit\n;; \xC3\x28 bad\n");
  REQUIRE(bad.invalid_utf8_line);
  CHECK(*bad.invalid_utf8_line == 2);
  CHECK(batch::source_from_text("/x/a.batch", "\xC0\xAF").invalid_utf8_line);   // overlong
  CHECK(batch::source_from_text("/x/a.batch", "\xED\xA0\x80").invalid_utf8_line);   // a surrogate
}

TEST_CASE("batch: read_source refuses a missing or empty file with a usage error (S1.01)", "[batch][lexer]") {
  CHECK_THROWS_AS(batch::read_source(""), core::UsageError);
  CHECK_THROWS_AS(batch::read_source("/nonexistent/dir/x.batch"), core::UsageError);
  const auto dir = std::filesystem::temp_directory_path();
  CHECK_THROWS_AS(batch::read_source(dir), core::UsageError);
  const auto empty = dir / "fusion_test_batch_empty.batch";
  {
    std::ofstream(empty) << ";; only a comment\n\n";
  }
  CHECK_THROWS_AS(batch::read_source(empty), core::UsageError);
  std::filesystem::remove(empty);
}
