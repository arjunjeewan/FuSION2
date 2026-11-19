// Section 12's core row: OutputFile's commit, and the temporary file's removal when it is not
// committed (09's rules for the run directory; S3.10, cross-section items 1.28, 2A.6 and 2B.3).
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"
#include "support.hpp"

using namespace fusion::core;
namespace fs = std::filesystem;

namespace {

struct Commits {
    std::vector<std::pair<fs::path, Sha256>> seen;
    OutputFile::OnCommit callback() {
        return [this](const fs::path& path, const Sha256& digest) { seen.emplace_back(path, digest); };
    }
};

}  // namespace

TEST_CASE("core: an output is written as <name>.tmp and appears under its name only when committed", "[unit][core]") {
    test::ScratchDir dir("commit");
    Commits commits;
    OutputFile file(dir / "up_reg", commits.callback());
    CHECK(file.path() == dir / "up_reg");
    CHECK(file.temp_path() == dir / "up_reg.tmp");
    file.out() << "hsa3725\nhsa2353\n";
    CHECK(fs::exists(dir / "up_reg.tmp"));
    CHECK_FALSE(fs::exists(dir / "up_reg"));
    file.commit();
    CHECK(test::read_file(dir / "up_reg") == "hsa3725\nhsa2353\n");
    CHECK_FALSE(fs::exists(dir / "up_reg.tmp"));
    REQUIRE(commits.seen.size() == 1);
    CHECK(commits.seen[0].first == dir / "up_reg");
    CHECK(commits.seen[0].second == Sha256::of("hsa3725\nhsa2353\n"));   // hashed in the same pass
    CHECK_THROWS_AS(file.commit(), Failure);
    CHECK_THROWS_AS(file.out(), Failure);
}

TEST_CASE("core: an output that is not committed leaves nothing, and never touches an earlier one", "[unit][core]") {
    test::ScratchDir dir("abandon");
    {
        OutputFile first(dir / "merged_graph.xml");
        first.out() << "<graph/>\n";
        first.commit();
    }
    {
        OutputFile second(dir / "merged_graph.xml");   // a failed step, or a stop, mid-write
        second.out() << "<graph><half";
        CHECK(fs::exists(dir / "merged_graph.xml.tmp"));
    }
    CHECK_FALSE(fs::exists(dir / "merged_graph.xml.tmp"));
    CHECK(test::read_file(dir / "merged_graph.xml") == "<graph/>\n");   // S3.10: not truncated
    {
        OutputFile never_opened(dir / "nothing.txt");
    }
    CHECK_FALSE(fs::exists(dir / "nothing.txt"));
    CHECK_FALSE(fs::exists(dir / "nothing.txt.tmp"));
}

TEST_CASE("core: an output with no bytes is committed as an empty file", "[unit][core]") {
    test::ScratchDir dir("empty");
    Commits commits;
    OutputFile shortlist(dir / "gsea_kgml_files.txt", commits.callback());   // S7.03: a 0-byte shortlist
    shortlist.commit();
    CHECK(fs::exists(dir / "gsea_kgml_files.txt"));
    CHECK(fs::file_size(dir / "gsea_kgml_files.txt") == 0);
    REQUIRE(commits.seen.size() == 1);
    CHECK(commits.seen[0].second == Sha256::of(""));
}

TEST_CASE("core: a program may write the temporary file itself, and commit hashes what it wrote", "[unit][core]") {
    test::ScratchDir dir("external");
    Commits commits;
    OutputFile png(dir / "pareto.png", commits.callback());   // section 07: gnuplot writes pareto.png.tmp
    test::write_file(png.temp_path(), "\x89PNG fake image");
    png.commit();
    CHECK(test::read_file(dir / "pareto.png") == "\x89PNG fake image");
    REQUIRE(commits.seen.size() == 1);
    CHECK(commits.seen[0].second == Sha256::of("\x89PNG fake image"));
}

TEST_CASE("core: large outputs are hashed exactly, whatever the size of each write", "[unit][core]") {
    test::ScratchDir dir("large");
    Commits commits;
    std::string expected;
    OutputFile dump(dir / "n0_e4.smt2", commits.callback());
    for (int i = 0; i < 2000; ++i) {
        const std::string line = "(assert (or node_" + std::to_string(i) + " edge_" + std::to_string(i) + "))\n";
        dump.out() << line;
        expected += line;
        if (i % 500 == 0) {
            const std::string block(70000 + static_cast<std::size_t>(i), static_cast<char>('a' + i % 26));
            dump.out().write(block.data(), static_cast<std::streamsize>(block.size()));   // above the buffer
            expected += block;
        }
    }
    dump.out() << 3.5 << ' ' << 42 << '\n';
    expected += "3.5 42\n";
    dump.commit();
    CHECK(test::read_file(dir / "n0_e4.smt2") == expected);
    CHECK(commits.seen.at(0).second == Sha256::of(expected));
}

TEST_CASE("core: a file in a directory not yet made gets its directories", "[unit][core]") {
    test::ScratchDir dir("nested");
    OutputFile journal(dir / "pathz3" / "in" / "case.json");   // section 06's per-case files
    journal.out() << "{}\n";
    journal.commit();
    CHECK(test::read_file(dir / "pathz3" / "in" / "case.json") == "{}\n");
}

TEST_CASE("core: an explicit absolute path gets a copy too, recorded after the file itself (2B.3)", "[unit][core]") {
    test::ScratchDir dir("also");
    fs::create_directories(dir / "elsewhere");
    test::write_file(dir / "elsewhere" / "up.txt", "an older copy\n");   // replaced, as FuSION 1 did
    Commits commits;
    OutputFile up(dir / "cudf" / "up.txt", commits.callback());
    up.also_at(dir / "elsewhere" / "up.txt");
    up.out() << "hsa3725\n";
    up.commit();
    CHECK(test::read_file(dir / "cudf" / "up.txt") == "hsa3725\n");
    CHECK(test::read_file(dir / "elsewhere" / "up.txt") == "hsa3725\n");
    REQUIRE(commits.seen.size() == 2);
    CHECK(commits.seen[0].first == dir / "cudf" / "up.txt");
    CHECK(commits.seen[1].first == dir / "elsewhere" / "up.txt");
    CHECK(commits.seen[0].second == commits.seen[1].second);
    for (const auto& entry : fs::directory_iterator(dir / "elsewhere"))   // no temporary copy left
        CHECK(entry.path().filename() == "up.txt");
    CHECK_THROWS_AS(up.also_at("relative/up.txt"), Failure);
}

TEST_CASE("core: a write that cannot happen is the failure io.write", "[unit][core]") {
    test::ScratchDir dir("fail");
    test::write_file(dir / "plain-file", "x");
    try {
        OutputFile impossible(dir / "plain-file" / "under-a-file.txt");   // its directory is a file
        impossible.out() << "x";
        impossible.commit();
        FAIL("writing under a regular file must fail");
    } catch (const Failure& f) {
        CHECK(f.code() == "io.write");
        CHECK_FALSE(f.where().file.empty());
    }
    OutputFile copy_fails(dir / "ok.txt");
    copy_fails.also_at(dir / "no-such-dir" / "ok.txt");
    copy_fails.out() << "x";
    try {
        copy_fails.commit();
        FAIL("a copy into a missing directory must fail");
    } catch (const Failure& f) {
        CHECK(f.code() == "io.write");
    }
    CHECK(test::read_file(dir / "ok.txt") == "x");   // the file itself was committed first
}

TEST_CASE("core: an output can be moved before it is committed", "[unit][core]") {
    test::ScratchDir dir("move");
    const auto make = [&] {
        OutputFile f(dir / "corridor.json");
        f.out() << "{\"nodes\":[]}";
        return f;                                // as Files::create returns one
    };
    OutputFile moved = make();
    OutputFile target(dir / "other.json");
    target = std::move(moved);                   // the abandoned "other.json" leaves nothing
    target.commit();
    CHECK(test::read_file(dir / "corridor.json") == "{\"nodes\":[]}");
    CHECK_FALSE(fs::exists(dir / "other.json"));
    CHECK_FALSE(fs::exists(dir / "corridor.json.tmp"));
}
