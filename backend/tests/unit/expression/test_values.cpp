// The one value reader (section 04, "The value reader"; S3.03 to S3.05, S3.08, S3.14; 04.C1 to
// 04.C3, 04.C9, 04.C10), on synthetic inputs.
#include <cmath>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace fusion;
using namespace fusion::expression;
using namespace fusion::expression::test;

namespace {

ValueInput table_input(std::string value_column, std::optional<std::string> id_column = std::nullopt) {
    ValueInput input;
    input.path = "table.tsv";
    input.value_column = std::move(value_column);
    input.id_column = std::move(id_column);
    return input;
}

}  // namespace

TEST_CASE("expression: the pairs layout reads an id and a value per line, later fields ignored (S3.03)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa1\t1.5\textra\nhsa2  -0.25\n  hsa3\t\t2\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1", "hsa2", "hsa3"});
    CHECK(values.value(0) == 1.5);
    CHECK(values.value(1) == -0.25);
    CHECK(values.value(2) == 2.0);
    const ValueReport& report = values.report();
    CHECK(report.lines == 3);
    CHECK(report.parsed == 3);
    CHECK(report.header == 0);
    CHECK(report.skipped.empty());
    CHECK(report.repeated.empty());
    CHECK(h.sink->warnings("values.skipped_lines").empty());
}

TEST_CASE("expression: every row of S3.14's table is read by the one rule set (S3.04, S3.05, 04.C2)",
          "[unit][expression]") {
    Harness h;
    const std::string text =
        "gene\tlfc\n"      // 1: a header in the pairs layout
        "\n"               // 2: blank, skipped silently
        "hsa9\n"           // 3: no value
        "hsa1\tNA\n"       // 4
        "hsa2\tnan\n"      // 5
        "hsa3\tinf\n"      // 6
        "hsa4\t1,5\n"      // 7: a decimal comma, which FuSION 1 read as 1
        "hsa5\t1_000\n"    // 8
        "hsa6\t1.5abc\n"   // 9: FuSION 1 read the prefix, 1.5
        "hsa7\t2.0\n"      // 10
        "hsa8\t1.\n"       // 11
        "hsa7\t-2.0\n"     // 12: a repeat in the other class
        "hsa10\t.5\n"      // 13
        "hsa11\t+3e2\n"    // 14
        "hsa12\t0x1p0\n"   // 15
        "hsa13\t-\n"       // 16
        " \t \n";          // 17: spaces and tabs only, blank
    const GeneValues values = parse(h, text);
    const ValueReport& report = values.report();
    CHECK(report.lines == 17);
    CHECK(report.blank == 2);
    CHECK(report.parsed == 5);
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa7", "hsa8", "hsa10", "hsa11"});
    CHECK(values.value(1) == 1.0);
    CHECK(values.value(2) == 0.5);
    CHECK(values.value(3) == 300.0);

    struct Expected {
        std::uint32_t line;
        SkipReason reason;
        std::string text;
    };
    const std::vector<Expected> expected{
        {1, SkipReason::not_a_number, "gene\tlfc"}, {3, SkipReason::missing_value, "hsa9"},
        {4, SkipReason::not_a_number, "hsa1\tNA"},  {5, SkipReason::not_a_number, "hsa2\tnan"},
        {6, SkipReason::not_a_number, "hsa3\tinf"}, {7, SkipReason::not_a_number, "hsa4\t1,5"},
        {8, SkipReason::not_a_number, "hsa5\t1_000"}, {9, SkipReason::not_a_number, "hsa6\t1.5abc"},
        {15, SkipReason::not_a_number, "hsa12\t0x1p0"}, {16, SkipReason::not_a_number, "hsa13\t-"}};
    REQUIRE(report.skipped.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(report.skipped[i].line == expected[i].line);
        CHECK(report.skipped[i].reason == expected[i].reason);
        CHECK(report.skipped[i].text == expected[i].text);
    }
    // hsa7: both lines kept, in file order; the representative is the larger magnitude, the earlier
    // line winning the tie (04.C3).
    const std::size_t hsa7 = *values.find(h.interner.id("hsa7"));
    CHECK(std::vector<double>(values.line_values(hsa7).begin(), values.line_values(hsa7).end()) ==
          std::vector<double>{2.0, -2.0});
    CHECK(std::vector<std::uint32_t>(values.line_numbers(hsa7).begin(), values.line_numbers(hsa7).end()) ==
          std::vector<std::uint32_t>{10, 12});
    CHECK(values.value(hsa7) == 2.0);
    REQUIRE(h.sink->warnings("values.skipped_lines").size() == 1);
    CHECK(h.sink->warnings("values.skipped_lines")[0].find("line 1 (not_a_number) \"gene\\tlfc\"") !=
          std::string::npos);
}

TEST_CASE("expression: a line without a value never takes another line's id or value (S3.05)",
          "[unit][expression]") {
    Harness h;
    // FuSION 1 wrote hsa1 twice for the blank line, and gave hsa9 the value 2.0 (S3.05).
    const GeneValues values = parse(h, "hsa1\t2.0\n\nhsa2\t0.1\nhsa9\nhsa3:2.0\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1", "hsa2"});
    CHECK(values.line_values(0).size() == 1);
    REQUIRE(values.report().skipped.size() == 2);
    CHECK(values.report().skipped[0].line == 4);
    CHECK(values.report().skipped[0].reason == SkipReason::missing_value);
    CHECK(values.report().skipped[1].line == 5);
    CHECK(values.report().skipped[1].reason == SkipReason::missing_value);
    CHECK(values.report().blank == 1);
}

TEST_CASE("expression: LF, CRLF and a lone CR end lines, and the last line needs none (S3.03, 04.C10)",
          "[unit][expression]") {
    Harness h;
    SECTION("CRLF without a final newline, as in the TNFa file") {
        const GeneValues values = parse(h, "hsa1\t1\r\nhsa2\t-2\r\nhsa3\t3");
        CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1", "hsa2", "hsa3"});
        CHECK(values.value(2) == 3.0);
        CHECK(values.report().lines == 3);
    }
    SECTION("a lone CR, as classic Mac text has it") {
        const GeneValues values = parse(h, "hsa1\t1\rhsa2\t2\r");
        CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1", "hsa2"});
        CHECK(values.report().lines == 2);
    }
    SECTION("LF then CR is two line ends, with a blank line between") {
        const GeneValues values = parse(h, "hsa1\t1\n\rhsa2\t2\n");
        CHECK(values.report().lines == 3);
        CHECK(values.report().blank == 1);
        CHECK(values.line_numbers(1)[0] == 3);
    }
}

TEST_CASE("expression: a UTF-8 byte-order mark is dropped; UTF-16 and NUL bytes fail (04.C10)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "\xEF\xBB\xBFhsa1\t1\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1"});
    CHECK(values.report().bytes == 10);

    using namespace std::string_view_literals;
    CHECK(failure_code([&] { parse(h, "\xFF\xFEh\0s\0a\0"sv); }) == "values.encoding");
    CHECK(failure_message([&] { parse(h, "\xFF\xFEh\0s\0a\0"sv); }).find("UTF-16") != std::string::npos);
    CHECK(failure_code([&] { parse(h, "\xFE\xFF\0h\0s"sv); }) == "values.encoding");
    // An .xlsx file is a ZIP archive with a NUL as its sixth byte.
    const std::string_view zip = "PK\x03\x04\x14\0\x06\0"sv;
    CHECK(failure_code([&] { parse(h, zip); }) == "values.encoding");
    CHECK(failure_message([&] { parse(h, zip); }).find("NUL byte at offset 5") != std::string::npos);
}

TEST_CASE("expression: a value past a double is not finite, one below a normal double reads as 0 (04.C2)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values =
        parse(h, "hsa1\t1e400\nhsa2\t-1e400\nhsa3\t1e-400\nhsa4\t1e-310\nhsa5\t2.2250738585072014e-308\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa3", "hsa4", "hsa5"});
    CHECK(values.value(0) == 0.0);
    CHECK(values.value(1) == 0.0);
    CHECK(values.value(2) == std::numeric_limits<double>::min());
    REQUIRE(values.report().skipped.size() == 2);
    CHECK(values.report().skipped[0].reason == SkipReason::not_finite);
    CHECK(values.report().skipped[1].reason == SkipReason::not_finite);
}

TEST_CASE("expression: ids pass the id rule, so hsa:3725 is hsa3725 (02.C3)", "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa:3725\t2.41\nhsa3725\t2.41\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa3725"});
    REQUIRE(values.report().repeated.size() == 1);
    CHECK(values.report().repeated[0].lines == std::vector<std::uint32_t>{1, 2});
}

TEST_CASE("expression: a repeated id is kept once, with every line, and its largest magnitude (04.C3)",
          "[unit][expression]") {
    Harness h;
    // The TNFa file's three repeated ids, in miniature (section 04, "Repeated ids").
    const GeneValues values = parse(h,
                                    "hsa57018\t0\nhsa1\t1\nhsa57018\t0\nhsa5806\t9.82\nhsa5806\t9.82\n"
                                    "hsa5806\t1.95\nhsa3\t-3\nhsa3\t3\n");
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa57018", "hsa1", "hsa5806", "hsa3"});
    CHECK(values.size() == 4);
    CHECK(values.value(2) == 9.82);
    CHECK(values.value(3) == -3.0);   // a tie in magnitude: the earlier line wins
    const ValueReport& report = values.report();
    REQUIRE(report.repeated.size() == 3);
    CHECK(h.interner.str(report.repeated[1].id) == "hsa5806");
    CHECK(report.repeated[1].lines == std::vector<std::uint32_t>{4, 5, 6});
    CHECK(report.repeated[1].values == std::vector<double>{9.82, 9.82, 1.95});
    const auto warnings = h.sink->warnings("values.repeated_ids");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] ==
          "3 repeated ids, each kept once: hsa57018 (lines 1, 3), hsa5806 (4, 5, 6), hsa3 (7, 8)");
}

TEST_CASE("expression: a file in which no line gives a value fails as values.empty", "[unit][expression]") {
    Harness h;
    CHECK(failure_code([&] { parse(h, ""); }) == "values.empty");
    CHECK(failure_code([&] { parse(h, "\n\n"); }) == "values.empty");
    // A CSV read as pairs: every line is one field.
    CHECK(failure_message([&] { parse(h, "gene,lfc\nhsa1,1.5\n"); }).find("value_column") != std::string::npos);
    CHECK(failure_code([&] { parse(h, "probe\tvalue\n", table_input("value")); }) == "values.empty");
}

TEST_CASE("expression: a skipped line is reported with at most its first 60 bytes, whole characters only",
          "[unit][expression]") {
    Harness h;
    const std::string long_value(100, 'x');
    // 58 bytes, then a 3-byte character that would end past byte 60.
    const std::string accented = "hsa2\t" + std::string(53, 'y') + "\xE2\x82\xAC" + "z";
    const GeneValues values = parse(h, "hsa1\t" + long_value + "\n" + accented + "\nhsa3\t1\n");
    REQUIRE(values.report().skipped.size() == 2);
    CHECK(values.report().skipped[0].text == ("hsa1\t" + long_value).substr(0, 60));
    CHECK(values.report().skipped[1].text == "hsa2\t" + std::string(53, 'y'));
}

TEST_CASE("expression: the table layout picks columns by header name (04.C1)", "[unit][expression]") {
    Harness h;
    SECTION("tabs, with id_column") {
        const GeneValues values = parse(h,
                                        "\nprobe\tGene_Symbol\tHeLa.EGF.120\n"
                                        "P1\thsa3725\t2.41\nP2\t\t1.0\nP3\thsa2353\tNA\nP4\thsa1950\n",
                                        table_input("HeLa.EGF.120", "Gene_Symbol"));
        CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa3725"});
        const ValueReport& report = values.report();
        CHECK(report.header == 1);
        CHECK(report.blank == 1);
        REQUIRE(report.skipped.size() == 3);
        CHECK(report.skipped[0].reason == SkipReason::missing_id);
        CHECK(report.skipped[0].line == 4);
        CHECK(report.skipped[1].reason == SkipReason::not_a_number);
        CHECK(report.skipped[2].reason == SkipReason::missing_value);
    }
    SECTION("commas, quotes and trimmed fields; the first column by default") {
        const GeneValues values = parse(h,
                                        "id,\"value, log2\",note\n"
                                        "\"hsa1\",1.5,a\n"
                                        "\"hs\"\"a2\" , 2 ,\"b, c\"\n"
                                        "hsa3,3,\"open\n",
                                        table_input("value, log2"));
        CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1", "hs\"a2"});
        CHECK(values.value(1) == 2.0);
        REQUIRE(values.report().skipped.size() == 1);
        CHECK(values.report().skipped[0].reason == SkipReason::bad_quote);
        CHECK(values.report().skipped[0].line == 4);
    }
    SECTION("runs of spaces") {
        const GeneValues values = parse(h, "gene  lfc   p\nhsa1   -1.25  0.01\n", table_input("lfc"));
        CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa1"});
        CHECK(values.value(0) == -1.25);
    }
    SECTION("a header that lacks a named column, or has it twice, fails as values.column") {
        CHECK(failure_code([&] { parse(h, "id\tlfc\nhsa1\t1\n", table_input("logFC")); }) == "values.column");
        CHECK(failure_code([&] { parse(h, "id\tlfc\tlfc\nhsa1\t1\t2\n", table_input("lfc")); }) ==
              "values.column");
        CHECK(failure_code([&] { parse(h, "id\tlfc\nhsa1\t1\n", table_input("lfc", "gene")); }) == "values.column");
        ValueInput id_only;
        id_only.id_column = "id";
        CHECK(failure_code([&] { parse(h, "hsa1\t1\n", id_only); }) == "values.column");
    }
}

TEST_CASE("expression: a value input's own map drops the ids it lacks and reports them (04.C1)",
          "[unit][expression]") {
    Harness h;
    const fs::path map_file = h.run_dir() / "map.txt";
    spit(map_file, "JUN hsa3725\nFOS hsa:2353\n");
    const network::IdMap map = network::IdMap::read(map_file, h.ctx);
    ValueInput input = table_input("v");
    input.id_map = core::to_utf8(map_file);
    const GeneValues values =
        parse_values("sym\tv\nJUN\t2\nXYZ\t5\nFOS\t1\nXYZ\t6\nABC\t1\n", input, map, true, h.interner, h.ctx);
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa3725", "hsa2353"});
    const ValueReport& report = values.report();
    CHECK(report.parsed == 5);
    CHECK(report.unmapped_lines == 3);
    CHECK(report.unmapped_ids == std::vector<std::string>{"XYZ", "ABC"});
    const auto warnings = h.sink->warnings("values.unmapped_ids");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].find("XYZ, ABC") != std::string::npos);

    // An empty map file is an empty map: every id dropped, and values.empty names the map (0021).
    const fs::path empty_map = h.run_dir() / "empty_map.txt";
    spit(empty_map, "");
    input.id_map = core::to_utf8(empty_map);
    const network::IdMap empty = network::IdMap::read(empty_map, h.ctx);
    const std::string message =
        failure_message([&] { parse_values("sym\tv\nJUN\t2\n", input, empty, true, h.interner, h.ctx); });
    CHECK(message.find("empty_map.txt") != std::string::npos);
}

TEST_CASE("expression: read_values reads id_map=symbols through the run's symbol map read backwards",
          "[unit][expression]") {
    Harness h;
    const fs::path symbols_file = h.run_dir() / "symbols.txt";
    spit(symbols_file, "hsa3725\tJUN\nhsa2353\tFOS\nhsa1026\tCDKN1A\n");
    network::Maps maps;
    maps.symbols = network::LabelMap::read(symbols_file, h.ctx);
    const fs::path table = h.run_dir() / "inputs" / "probes.csv";
    spit(table, "probe,Gene_Symbol,HeLa.EGF.120\nP1,JUN,2.4\nP2,JUN,-2.5\nP3,,1\nP4,FOS,0.5\nP5,NOPE,3\n");
    ValueInput input;
    input.path = "probes.csv";
    input.id_column = "Gene_Symbol";
    input.value_column = "HeLa.EGF.120";
    input.id_map = "symbols";
    input.repeats = RepeatRule::max_abs;
    const GeneValues values = read_values(table, input, maps, h.interner, h.ctx);
    CHECK(ids_of(values, h.interner) == std::vector<std::string>{"hsa3725", "hsa2353"});
    CHECK(values.value(0) == -2.5);
    CHECK(values.report().skipped.size() == 1);
    CHECK(values.report().unmapped_ids == std::vector<std::string>{"NOPE"});
}

TEST_CASE("expression: read_values hashes the bytes it parsed and names the file from the run directory",
          "[unit][expression]") {
    Harness h;
    const std::string bytes = "hsa1\t1\nhsa2\t-1\n";
    const fs::path copy = h.run_dir() / "inputs" / "lfc.tsv";
    spit(copy, bytes);
    ValueInput input;
    input.path = "/elsewhere/lfc.tsv";
    const GeneValues values = read_values(copy, input, network::Maps{}, h.interner, h.ctx);
    CHECK(values.report().bytes == bytes.size());
    CHECK(values.report().sha256 == core::Sha256::of(bytes));
    CHECK(values.input() == input);
    const auto events = h.events.of("expression.read");
    REQUIRE(events.size() == 1);
    CHECK(events[0]["path"] == "inputs/lfc.tsv");
    CHECK(events[0]["layout"] == "pairs");
    CHECK(events[0]["lines"] == 2);
    CHECK(events[0]["genes"] == 2);
    CHECK(events[0]["n_skipped"] == 0);
    CHECK(events[0]["n_repeated"] == 0);
    CHECK(events[0]["n_unmapped"] == 0);

    CHECK(failure_code([&] { read_values(h.run_dir() / "missing.tsv", input, network::Maps{}, h.interner, h.ctx); }) ==
          "input.missing");
    CHECK(failure_code([&] { read_values(h.run_dir(), input, network::Maps{}, h.interner, h.ctx); }) ==
          "input.not_file");
}

TEST_CASE("expression: ValueInputs reads each input once, and warns when a file is read two ways (04.C9)",
          "[unit][expression]") {
    Harness h;
    const fs::path file = h.run_dir() / "inputs" / "t.tsv";
    spit(file, "id\tlfc\tp\nhsa1\t1\t0.5\n");
    ValueInput pairs;
    pairs.path = "t.tsv";
    ValueInput table = pairs;
    table.value_column = "lfc";
    ValueInputs inputs;
    // Read as pairs, the header and the row are one-number lines: "lfc" is not a number, 1 is.
    const GeneValues& first = inputs.read(pairs, file, network::Maps{}, h.interner, h.ctx);
    const GeneValues& again = inputs.read(pairs, file, network::Maps{}, h.interner, h.ctx);
    CHECK(&first == &again);
    CHECK(h.events.of("expression.read").size() == 1);
    CHECK(h.sink->warnings("values.read_two_ways").empty());
    const GeneValues& second = inputs.read(table, file, network::Maps{}, h.interner, h.ctx);
    CHECK(&second != &first);
    const auto warnings = h.sink->warnings("values.read_two_ways");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].find("value_column none and \"lfc\"") != std::string::npos);
    CHECK(&inputs.at(pairs) == &first);
    CHECK(&inputs.at(table) == &second);
    ValueInput unread = pairs;
    unread.repeats = RepeatRule::max_abs;
    CHECK(failure_code([&] { inputs.at(unread); }) == "internal");
}
