// Section 12's core row: format_number against Python's repr (S1.27, cross-section item 1.20),
// parse_decimal's grammar (04.C2, 11.C5, 3A.19), utc_timestamp, and UTF-8 paths.
#include <bit>
#include <charconv>
#include <chrono>
#include <clocale>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"
#include "support.hpp"

using namespace fusion::core;

TEST_CASE("core: format_number writes Python's repr: the shortest digits that read back exactly", "[unit][core]") {
    CHECK(format_number(0.0) == "0.0");
    CHECK(format_number(-0.0) == "-0.0");
    CHECK(format_number(1e-05) == "1e-05");
    CHECK(format_number(0.0001) == "0.0001");
    CHECK(format_number(13632.95) == "13632.95");
    CHECK(format_number(100.0) == "100.0");
    CHECK(format_number(0.004) == "0.004");     // FuSION 1's two fixed decimals showed 0.00 (S1.27)
    CHECK(format_number(1e16) == "1e+16");
    CHECK(format_number(9999999999999998.0) == "9999999999999998.0");
    CHECK(format_number(5e-324) == "5e-324");
    CHECK(format_number(std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(format_number(std::numeric_limits<double>::infinity()) == "inf");
    CHECK(format_number(-std::numeric_limits<double>::infinity()) == "-inf");
}

TEST_CASE("core: format_number equals Python 3.12's repr on 578 doubles", "[unit][core]") {
    // python_repr.tsv: <the double's bits in hex><TAB><repr>, from python_repr.py: edge cases, any
    // bit pattern, decimals and ratios like FuSION's numbers, and both sides of every layout switch.
    std::ifstream in(test::source_dir() / "unit/core/python_repr.tsv", std::ios::binary);
    REQUIRE(in);
    std::string line;
    int checked = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        std::uint64_t bits = 0;
        std::from_chars(line.data(), line.data() + tab, bits, 16);
        const double value = std::bit_cast<double>(bits);
        const std::string expected = line.substr(tab + 1);
        INFO(line);
        CHECK(format_number(value) == expected);
        ++checked;
    }
    CHECK(checked == 578);
}

TEST_CASE("core: format_number reproduces every number gseapy's references wrote with repr", "[unit][core]") {
    // backend/tests/fixtures/gsea: "Numbers are Python's repr of gseapy's doubles" (its README).
    // Each reads back through parse_decimal and is written again unchanged.
    int checked = 0;
    for (const char* fixture : {"n50", "n500", "n3054"}) {
        const auto base = test::source_dir() / "fixtures/gsea" / fixture;
        for (const auto& [file, first_number_column] : {std::pair{"stats.tsv", 2}, std::pair{"observed.tsv", 1}}) {
            std::ifstream in(base / file, std::ios::binary);
            REQUIRE(in);
            std::string line;
            std::getline(in, line);             // header
            while (std::getline(in, line)) {
                std::vector<std::string> fields;
                for (std::size_t start = 0;;) {
                    const auto tab = line.find('\t', start);
                    fields.push_back(line.substr(start, tab - start));
                    if (tab == std::string::npos) break;
                    start = tab + 1;
                }
                const int last = std::string_view(file) == "stats.tsv" ? 5 : 1;   // observed.tsv: ES only
                for (int c = first_number_column; c <= last; ++c) {
                    const std::string& token = fields[static_cast<std::size_t>(c)];
                    const Decimal d = parse_decimal(token);
                    INFO(fixture << " " << file << ": " << token);
                    REQUIRE(d.status == DecimalStatus::ok);
                    CHECK(format_number(d.value) == token);
                    ++checked;
                }
            }
        }
    }
    CHECK(checked > 2500);
}

TEST_CASE("core: parse_decimal accepts exactly 04.C2's grammar", "[unit][core]") {
    const std::vector<std::pair<std::string, double>> accepted = {
        {"0", 0.0},       {"1", 1.0},           {"-0.06", -0.06},   {"+1.01", 1.01},     {"1.", 1.0},
        {".5", 0.5},      {"-.5", -0.5},        {"1e3", 1000.0},    {"1E-3", 0.001},     {"2.5e+2", 250.0},
        {"007", 7.0},     {"0.67", 0.67},       {"14400000", 14400000.0},
        {"0.1", 0.1},     {"94.57", 94.57},     {"-1.56", -1.56},
        {"123456789012345678901234567890", 1.2345678901234568e29},
    };
    for (const auto& [token, value] : accepted) {
        INFO(token);
        const Decimal d = parse_decimal(token);
        CHECK(d.status == DecimalStatus::ok);
        CHECK(d.value == value);
    }
    // S3.02 to S3.04 and section 12's list: none of these is a number, as a whole token.
    for (const char* token : {"NA", "nan", "NaN", "inf", "-inf", "Infinity", "1_000", "1,5", "0x1p0", "0x10", "",
                              " 1", "1 ", "+", "-", ".", "-.", "e5", "1e", "1e+", "1.5abc", "1.0l", "--1", "+-1",
                              "1.2.3", "1e5.5", "\xef\xbc\x91", "gene", "lfc"}) {
        INFO("[" << token << "]");
        CHECK(parse_decimal(token).status == DecimalStatus::not_a_number);
    }
}

TEST_CASE("core: parse_decimal: overflow is not finite, and anything below the smallest normal reads as 0", "[unit][core]") {
    CHECK(parse_decimal("1e309").status == DecimalStatus::not_finite);
    CHECK(parse_decimal("-1e400").status == DecimalStatus::not_finite);
    CHECK(parse_decimal(std::string(400, '9')).status == DecimalStatus::not_finite);   // no exponent at all
    CHECK(parse_decimal("0." + std::string(300, '0') + "1").value == 1e-301);   // tiny but normal: kept
    CHECK(parse_decimal("1.7976931348623157e308").value == std::numeric_limits<double>::max());
    const std::string no_exponent = "0." + std::string(320, '0') + "1";   // 1e-321, subnormal
    for (const std::string& tiny : {std::string("1e-400"), std::string("4.9e-324"), std::string("1e-310"),
                                    std::string("2.225073858507201e-308"), no_exponent}) {
        INFO(tiny);
        const Decimal d = parse_decimal(tiny);
        CHECK(d.status == DecimalStatus::ok);
        CHECK(d.value == 0.0);
    }
    CHECK(parse_decimal("2.2250738585072014e-308").value == std::numeric_limits<double>::min());   // normal
    CHECK(std::signbit(parse_decimal("-1e-400").value));
    CHECK(std::signbit(parse_decimal("-0").value));
    CHECK(parse_decimal("-0.00").value == 0.0);  // gseapy's fixtures write some zeros as -0.00
}

TEST_CASE("core: numbers are read and written the same under any locale", "[unit][core]") {
    // section 12, Determinism: "Tests also run under ... decimal comma"; en_DK has one here.
    if (std::setlocale(LC_ALL, "en_DK.utf8") == nullptr) SKIP("no locale with a decimal comma is installed");
    std::locale::global(std::locale("en_DK.utf8"));
    const Decimal d = parse_decimal("1.5");
    const Decimal comma = parse_decimal("1,5");
    const std::string written = format_number(1234.5);
    std::locale::global(std::locale::classic());
    std::setlocale(LC_ALL, "C");
    CHECK(d.status == DecimalStatus::ok);
    CHECK(d.value == 1.5);
    CHECK(comma.status == DecimalStatus::not_a_number);
    CHECK(written == "1234.5");
}

TEST_CASE("core: utc_timestamp writes UTC with milliseconds, truncated", "[unit][core]") {
    using namespace std::chrono;
    const sys_days day = year{2026} / November / 12;
    CHECK(utc_timestamp(day + 13h + 14min + 0s + 412ms) == "2026-11-12T13:14:00.412Z");
    CHECK(utc_timestamp(time_point_cast<system_clock::duration>(day + 13h + 14min + 412999us)) ==
          "2026-11-12T13:14:00.412Z");
    CHECK(utc_timestamp(system_clock::time_point{}) == "1970-01-01T00:00:00.000Z");
    CHECK(utc_timestamp(system_clock::time_point{} - 1ms) == "1969-12-31T23:59:59.999Z");
    CHECK(utc_timestamp(sys_days{year{2028} / February / 29} + 23h + 59min + 59s) == "2028-02-29T23:59:59.000Z");
}

TEST_CASE("core: paths go to and from UTF-8 text, with '/' separators", "[unit][core]") {
    const std::string text = "runs/EGFR\xe2\x86\x92" "CXCL8/caf\xc3\xa9.tsv";   // EGFR→CXCL8, café
    const std::filesystem::path p = path_from_utf8(text);
    CHECK(to_utf8(p) == text);
    CHECK(p.filename() == path_from_utf8("caf\xc3\xa9.tsv"));
    CHECK(to_utf8(std::filesystem::path("a") / "b" / "c.txt") == "a/b/c.txt");
    test::ScratchDir dir("utf8");
    const auto file = dir.path() / path_from_utf8("\xc3\xa9t\xc3\xa9.txt");
    test::write_file(file, "x");
    CHECK(std::filesystem::exists(file));
}
