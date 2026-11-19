// Section 12's core row: build information and the build id (11.C16), fingerprints (09.C5), the
// check report behind `fusion --check --json` (1.9), and the install directory (S1.20, 11.C20).
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fusion/core/build_info.hpp"
#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"
#include "support.hpp"

using namespace fusion::core;
namespace fs = std::filesystem;

TEST_CASE("core: build information names VERSION, the commit and the pinned libraries", "[unit][core]") {
    const BuildInfo& b = build_info();
    std::string version = test::read_file(test::source_dir() / "../../VERSION");
    while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();
    CHECK(b.version == version);
    const bool hex40 = b.git_commit.size() == 40 &&
                       b.git_commit.find_first_not_of("0123456789abcdef") == std::string_view::npos;
    CHECK((hex40 || b.git_commit == "unknown"));
    if (b.git_commit == "unknown") CHECK(b.git_dirty);   // unknown provenance counts as uncommitted
    CHECK_FALSE(b.compiler.empty());
    CHECK(b.target.find('-') != std::string_view::npos);
    CHECK(b.source_date_epoch >= 0);
    REQUIRE(b.libraries.size() == 3);
    CHECK(b.libraries[0] == std::pair<std::string_view, std::string_view>{"pugixml", "1.16"});
    CHECK(b.libraries[1] == std::pair<std::string_view, std::string_view>{"nlohmann_json", "3.12.0"});
    CHECK(b.libraries[2] == std::pair<std::string_view, std::string_view>{"fast_float", "8.3.3"});
}

TEST_CASE("core: the build id is VERSION and the commit, plus the executable's SHA-256 when unclean", "[unit][core]") {
    const BuildInfo& b = build_info();
    const std::string expected_start = std::string(b.version) + "+" + std::string(b.git_commit);
    CHECK_THAT(std::string(build_id()), Catch::Matchers::StartsWith(expected_start));
    CHECK(clean_build() == !b.git_dirty);
    if (b.git_dirty)
        CHECK(build_id() == expected_start + ".dirty." + Sha256::of_file(executable_path()).hex());
    else
        CHECK(build_id() == expected_start);
    CHECK(build_id().data() == build_id().data());   // computed once
}

TEST_CASE("core: a fingerprint is the SHA-256 of canonical JSON with the build id added", "[unit][core]") {
    // 09.C5's corridor example: keys sorted at every level, no spaces.
    Json fields = Json::object();
    fields["settings"] = Json::object();
    fields["settings"]["corridor.target"] = Json::array({"hsa3576"});
    fields["settings"]["corridor.bound"] = 10;
    fields["command"] = "fb_rch";
    fields["inputs"] = Json::object({{"network", "9f1c5e"}});
    const std::string canonical = R"({"build":")" + std::string(build_id()) +
                                  R"(","command":"fb_rch","inputs":{"network":"9f1c5e"},)"
                                  R"("settings":{"corridor.bound":10,"corridor.target":["hsa3576"]}})";
    CHECK(fingerprint(fields) == Sha256::of(canonical).hex());

    Json reordered = Json::object();             // the order fields were given in never matters
    reordered["inputs"] = Json::object({{"network", "9f1c5e"}});
    reordered["command"] = "fb_rch";
    reordered["settings"] = Json::object();
    reordered["settings"]["corridor.bound"] = 10;
    reordered["settings"]["corridor.target"] = Json::array({"hsa3576"});
    CHECK(fingerprint(reordered) == fingerprint(fields));

    Json changed = fields;                       // any result-changing value changes it
    changed["settings"]["corridor.bound"] = 9;
    CHECK(fingerprint(changed) != fingerprint(fields));
    Json other_input = fields;
    other_input["inputs"]["network"] = "9f1c5f";
    CHECK(fingerprint(other_input) != fingerprint(fields));
}

TEST_CASE("core: the check report is section 09's JSON, diagnostics sorted by point, line and column", "[unit][core]") {
    CheckReport report;
    report.batch = "/home/u/exp/egfr.batch";
    report.batch_sha256 = Sha256::of("batch text");
    report.points = 2;
    report.diagnostics.push_back({Severity::warning, 4, 18, std::nullopt, "batch.undefined_variable",
                                  "$PLOT_FILE_WO_GENE matches no defined variable", std::nullopt, std::nullopt,
                                  std::nullopt});
    report.diagnostics.push_back({Severity::error, 9, 5, 1u, "input.missing",
                                  "listed in /home/u/kegg/list.txt, but no such file", "mff", "list",
                                  "/home/u/kegg/hsa04010.xml"});
    report.diagnostics.push_back({Severity::note, 2, 1, std::nullopt, "batch.note", "first", std::nullopt,
                                  std::nullopt, std::nullopt});
    report.features.push_back({11, "pathz3", "coexpression", true, "a CSV and a threshold are given"});
    CHECK_FALSE(report.ok());
    CHECK(report.to_json().dump() ==
          R"({"ok":false,"batch":"/home/u/exp/egfr.batch","sha256":")" + Sha256::of("batch text").hex() +
              R"(","points":2,"diagnostics":[)"
              R"({"severity":"note","line":2,"column":1,"point":null,"code":"batch.note","command":null,)"
              R"("setting":null,"path":null,"message":"first"},)"
              R"({"severity":"warning","line":4,"column":18,"point":null,"code":"batch.undefined_variable",)"
              R"("command":null,"setting":null,"path":null,"message":"$PLOT_FILE_WO_GENE matches no defined variable"},)"
              R"({"severity":"error","line":9,"column":5,"point":1,"code":"input.missing","command":"mff",)"
              R"("setting":"list","path":"/home/u/kegg/hsa04010.xml",)"
              R"("message":"listed in /home/u/kegg/list.txt, but no such file"}],)"
              R"("features":[{"line":11,"command":"pathz3","feature":"coexpression","on":true,)"
              R"("because":"a CSV and a threshold are given"}]})");
    report.diagnostics.erase(report.diagnostics.begin() + 1);
    CHECK(report.ok());                          // warnings and notes alone pass
}

TEST_CASE("core: the executable's own path is absolute and resolved", "[unit][core]") {
    const fs::path exe = executable_path();
    CHECK(exe.is_absolute());
    CHECK(fs::exists(exe));
    CHECK(exe.filename() == "test_core");
    CHECK(fs::canonical(exe) == exe);
}

TEST_CASE("core: the home is FUSION_HOME when set, else share/fusion beside bin/ (S1.20, 11.C20)", "[unit][core]") {
    test::ScratchDir dir("home");
    {
        const test::ScopedEnv home("FUSION_HOME", dir.path().string());
        const InstallDir found = find_install_dir();
        CHECK(found.source == InstallDir::Source::env_override);
        CHECK(found.home == dir.path());
        CHECK(found.tools_dir == dir.path() / "tools");
        CHECK(found.bin_dir == executable_path().parent_path());
    }
    {
        const test::ScopedEnv home("FUSION_HOME", std::nullopt);
        const InstallDir found = find_install_dir();
        CHECK(found.source == InstallDir::Source::executable);
        CHECK(found.prefix == executable_path().parent_path().parent_path());
        CHECK(found.home == found.prefix / "share" / "fusion");
    }
}

TEST_CASE("core: a bundled file that is missing is the install error install.file_missing", "[unit][core]") {
    test::ScratchDir dir("home-files");
    const test::ScopedEnv home("FUSION_HOME", dir.path().string());
    if (install_dir().home != dir.path()) SKIP("the install directory was worked out earlier in this process");
    fs::create_directories(dir / "data");
    test::write_file(dir / "data" / "kegg_pathways.txt", "hsa04010\n");
    CHECK(home_file("data/kegg_pathways.txt") == dir / "data" / "kegg_pathways.txt");
    try {
        home_file("data/hsa_to_gene_symbol_map.txt");
        FAIL("a missing bundled file must throw");
    } catch (const InstallError& e) {
        CHECK(e.code() == "install.file_missing");
    }
}
