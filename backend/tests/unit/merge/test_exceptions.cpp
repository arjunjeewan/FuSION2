// The exception set (S1.17, S2.13, S8.22; 03.C1, 03.C2): every reason, the id rule, the files' format,
// and the failures.
#include <catch2/catch_test_macros.hpp>

#include "fusion/expression/expression.hpp"
#include "support.hpp"

using namespace merge_test;
using fusion::merge::assemble_exceptions;
using fusion::merge::ExceptionInputs;
using fusion::merge::ExceptionSet;
using fusion::merge::Reason;
using fusion::merge::reason_names;
using fusion::network::IdMap;

TEST_CASE("an exception set keeps each id once, with all its reasons, in byte order", "[unit][merge][S2.13]") {
    fusion::core::Interner in;
    ExceptionSet e(in);
    e.add(in.id("hsa3576"), Reason::up);
    e.add(in.id("hsa3576"), Reason::target);
    e.add(std::string_view("hsa1956"), Reason::source);
    e.add(in.id("hsa10"), Reason::not_merged);
    CHECK(e.size() == 3);
    CHECK(e.ids().ids()[0] == "hsa10");
    CHECK(e.ids().ids()[1] == "hsa1956");
    CHECK(e.ids().ids()[2] == "hsa3576");
    CHECK(e.contains(in.id("hsa1956")));
    CHECK(e.contains(std::string_view("hsa3576")));
    CHECK_FALSE(e.contains(in.id("hsa2")));
    CHECK(reason_names(e.reasons(std::string_view("hsa3576"))) == "up,target");
    CHECK(e.reasons(in.id("hsa10")) == static_cast<std::uint16_t>(Reason::not_merged));
    CHECK(e.reasons(std::string_view("hsa99")) == 0);
    CHECK(fusion::merge::reason_name(Reason::confirmed_down) == "confirmed_down");
    CHECK(reason_names(0x1FF) == "up,down,confirmed_up,confirmed_down,source,target,fsc,not_merged,start_file");
}

TEST_CASE("the digest covers the sorted ids, each ended by LF", "[unit][merge][S2.13]") {
    CHECK(ExceptionSet().digest().hex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const auto e = exceptions({"hsa1432", "hsa1026"});
    CHECK(e.digest().hex() == "1878d2eff661ca283c0f111977ac5c97cea7f4083f82b5c1a59bffaa368b6231");
}

TEST_CASE("a handle needs the interner it came from", "[unit][merge][S2.13]") {
    fusion::core::Interner in;
    ExceptionSet unbound;
    CHECK_THROWS_AS(unbound.add(in.id("hsa1"), Reason::up), fusion::core::Failure);
    CHECK_FALSE(unbound.contains(in.id("hsa1")));
}

TEST_CASE("assemble_exceptions takes every reason from the run's inputs", "[unit][merge][S1.17][S2.13][S8.22][03.C2]") {
    Context c;
    fusion::core::Interner ids;
    const auto up = c.scratch.write("up_reg", "hsa1026\nhsa3576\n");
    const auto down = c.scratch.write("down_reg", "hsa1843\n");
    const auto observations = fusion::expression::load_lists(up, down, IdMap::none(), ids, c.ctx);
    const std::vector<fusion::core::GeneId> sources = {ids.id("hsa1956")};
    const std::vector<fusion::core::GeneId> targets = {ids.id("hsa3576")};
    const std::vector<fusion::core::GeneId> fscs = {ids.id("hsa3725")};
    ExceptionInputs in;
    in.observations = &observations;
    in.sources = sources;
    in.targets = targets;
    in.fscs = fscs;
    // First token per line; CRLF, blank lines, leading blanks and a byte-order mark are all accepted, and
    // each id passes the id rule, so hsa:5594 is hsa5594 (02.C3).
    in.confirmed_up = {c.scratch.write("confirmed_up", "hsa:5594 the rest is ignored\r\n\r\n   hsa7\r\n")};
    in.confirmed_down = {c.scratch.write("confirmed_down", "\xEF\xBB\xBFhsa8\n")};
    in.not_merged = c.scratch.write("hsa_ids_not_to_be_merged.txt", "hsa1432\nhsa5594\nhsa5599");
    in.start_file = c.scratch.write("exception_file.txt", "hsa1026\n\nhsa9999\n");
    in.interner = &ids;
    const ExceptionSet e = assemble_exceptions(in, IdMap::none(), c.ctx);

    const std::vector<std::pair<std::string, std::string>> expected = {
        {"hsa1026", "up,start_file"}, {"hsa1432", "not_merged"},           {"hsa1843", "down"},
        {"hsa1956", "source"},        {"hsa3576", "up,target"},            {"hsa3725", "fsc"},
        {"hsa5594", "confirmed_up,not_merged"}, {"hsa5599", "not_merged"}, {"hsa7", "confirmed_up"},
        {"hsa8", "confirmed_down"},   {"hsa9999", "start_file"}};
    REQUIRE(e.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(e.ids().ids()[i] == expected[i].first);
        CHECK(reason_names(e.reasons(std::string_view(expected[i].first))) == expected[i].second);
    }
    // The set answers for the run's handles too.
    CHECK(e.contains(ids.id("hsa3725")));
    CHECK(e.contains(ids.id("hsa9999")));
}

TEST_CASE("the run's id map applies to the files' ids", "[unit][merge][02.C3]") {
    Context c;
    const IdMap map = IdMap::read(c.scratch.write("map.txt", "GENEX hsa42\n"), c.ctx);
    ExceptionInputs in;
    in.not_merged = c.scratch.write("not_merged.txt", "GENEX\nhsa:7\n");
    const ExceptionSet e = assemble_exceptions(in, map, c.ctx);
    CHECK(e.contains(std::string_view("hsa42")));
    CHECK(e.contains(std::string_view("hsa7")));
    CHECK_FALSE(e.contains(std::string_view("GENEX")));
}

TEST_CASE("an FSC that is not differentially expressed joins the set by itself (the RAS trap)", "[unit][merge][S2.13]") {
    Context c;
    fusion::core::Interner ids;
    const std::vector<fusion::core::GeneId> fscs = {ids.id("hsa3265"), ids.id("hsa3845")};   // a set (06.C10)
    ExceptionInputs in;
    in.fscs = fscs;
    in.interner = &ids;
    const ExceptionSet e = assemble_exceptions(in, IdMap::none(), c.ctx);
    CHECK(e.size() == 2);
    CHECK(reason_names(e.reasons(std::string_view("hsa3845"))) == "fsc");
}

TEST_CASE("a missing file and a handle without an interner fail", "[unit][merge][S2.13]") {
    Context c;
    ExceptionInputs missing;
    missing.not_merged = c.scratch.dir() / "nowhere.txt";
    try {
        assemble_exceptions(missing, IdMap::none(), c.ctx);
        FAIL("no failure");
    } catch (const fusion::core::Failure& f) {
        CHECK(f.code() == "input.missing");
    }
    fusion::core::Interner ids;
    const std::vector<fusion::core::GeneId> sources = {ids.id("hsa1")};
    ExceptionInputs no_interner;
    no_interner.sources = sources;
    try {
        assemble_exceptions(no_interner, IdMap::none(), c.ctx);
        FAIL("no failure");
    } catch (const fusion::core::Failure& f) {
        CHECK(f.code() == "internal");
    }
    // With nothing given, E is empty.
    CHECK(assemble_exceptions(ExceptionInputs{}, IdMap::none(), c.ctx).empty());
}
