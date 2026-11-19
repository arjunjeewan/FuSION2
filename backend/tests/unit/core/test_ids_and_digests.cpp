// Section 12's core row: stable orderings and ids (2A.5, S4.14, S7.05) and SHA-256 (1.1).
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"
#include "support.hpp"

using namespace fusion::core;

TEST_CASE("core: interned handles follow first appearance, one per string", "[unit][core]") {
    Interner ids;
    const GeneId egfr = ids.id("hsa1956");
    const GeneId jun = ids.id("hsa3725");
    CHECK(egfr.value == 0);
    CHECK(jun.value == 1);
    CHECK(ids.id("hsa1956") == egfr);
    CHECK(ids.id_count() == 2);
    CHECK(ids.str(egfr) == "hsa1956");
    CHECK(ids.str(jun) == "hsa3725");
    // Other strings are a table of their own; a Sym never names a gene.
    const Sym activation = ids.sym("activation");
    CHECK(activation.value == 0);
    CHECK(ids.sym("hsa1956").value == 1);
    CHECK(ids.id_count() == 2);
    CHECK(ids.str(activation) == "activation");
}

TEST_CASE("core: finding an id never inserts it (S2.15)", "[unit][core]") {
    Interner ids;
    ids.id("hsa3725");
    CHECK(ids.find_id("hsa3725") == GeneId{0});
    CHECK_FALSE(ids.find_id("hsa9999"));
    CHECK_FALSE(ids.find_sym("hsa3725"));
    CHECK(ids.id_count() == 1);
}

TEST_CASE("core: interned strings stay put while the interner grows, and survive a move", "[unit][core]") {
    Interner ids;
    std::vector<std::string_view> views;
    for (int i = 0; i < 20000; ++i) views.push_back(ids.str(ids.id("g" + std::to_string(i))));
    for (int i = 0; i < 20000; ++i) CHECK(views[static_cast<std::size_t>(i)] == "g" + std::to_string(i));
    Interner moved = std::move(ids);
    CHECK(moved.str(GeneId{19999}) == "g19999");
    CHECK(views.front().data() == moved.str(GeneId{0}).data());
    std::unordered_map<GeneId, int, HandleHash> by_id{{GeneId{3}, 1}};   // lookups only, never iterated
    CHECK(by_id.at(GeneId{3}) == 1);
}

TEST_CASE("core: stable_compare orders by unsigned bytes", "[unit][core]") {
    CHECK(stable_compare("hsa1000", "hsa10000") < 0);   // S7.05's measured order
    CHECK(stable_compare("hsa10000", "hsa999") < 0);
    CHECK(stable_compare("g10", "g100") < 0);
    CHECK(stable_compare("g100", "g11") < 0);
    CHECK(stable_compare("a", "ab") < 0);
    CHECK(stable_compare("", "a") < 0);
    CHECK(stable_compare("abc", "abc") == 0);
    CHECK(stable_compare("Z", "a") < 0);                 // no case folding
    CHECK(stable_compare("z", "\xc3\xa9") < 0);          // é's bytes are above ASCII: unsigned, not signed char
    CHECK(stable_compare(std::string_view("a\0b", 3), std::string_view("a\0c", 3)) < 0);
}

TEST_CASE("core: stable_hash is one fixed function, the same on every platform", "[unit][core]") {
    // FNV-1a then SplitMix64's finaliser; values from an independent Python computation.
    CHECK(stable_hash("") == 17665956581633026203ULL);
    CHECK(stable_hash("a") == 198367012849983736ULL);
    CHECK(stable_hash("hsa3725") == 12338298223601806889ULL);
    CHECK(stable_hash("\xc3\xa9") == 2536656207217688990ULL);
}

TEST_CASE("core: SHA-256 matches FIPS 180-4's test vectors", "[unit][core]") {
    CHECK(Sha256::of("").hex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256::of("abc").hex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256::of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").hex() ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(Sha256::of("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrs"
                     "mnopqrstnopqrstu")
              .hex() == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    CHECK(Sha256::of(std::string(1000000, 'a')).hex() ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("core: streaming SHA-256 in any pieces equals one pass", "[unit][core]") {
    std::string data;
    for (int i = 0; i < 300; ++i)
        for (int b = 0; b < 256; ++b) data.push_back(static_cast<char>(b));
    const std::string expected = "f8b0585eb91f58c007a5634362c9f90d8543822c113f702523bc7b73408a9392";
    CHECK(Sha256::of(data).hex() == expected);
    for (const std::size_t piece : {std::size_t{1}, std::size_t{3}, std::size_t{55}, std::size_t{63}, std::size_t{64},
                                    std::size_t{65}, std::size_t{4097}}) {
        Sha256Builder builder;
        for (std::size_t at = 0; at < data.size(); at += piece) {
            const std::string_view part = std::string_view(data).substr(at, piece);
            builder.update(std::span(reinterpret_cast<const std::byte*>(part.data()), part.size()));
        }
        CHECK(builder.finish().hex() == expected);
    }
}

TEST_CASE("core: a file's SHA-256 is the SHA-256 of its bytes", "[unit][core]") {
    test::ScratchDir dir("sha");
    const std::string bytes = std::string("hsa1000\t0.12\r\nhsa10000\t-0.18\n") + std::string(70000, 'x');
    test::write_file(dir / "values.tsv", bytes);
    CHECK(Sha256::of_file(dir / "values.tsv") == Sha256::of(bytes));
    CHECK(Sha256::of(bytes) != Sha256::of(bytes + " "));
    try {
        Sha256::of_file(dir / "missing.tsv");
        FAIL("a missing file must throw");
    } catch (const Failure& f) {
        CHECK(f.code() == "input.unreadable");
    }
    Sha256Builder used;
    used.finish();
    CHECK_THROWS_AS(used.finish(), Failure);
}
