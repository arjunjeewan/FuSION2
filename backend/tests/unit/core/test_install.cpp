// Section 11's checks of the home (S1.20, "Finding the backend's own files", steps 1 to 3, and its error
// table): fake homes named by FUSION_HOME, one per failure and its code.
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fusion/core/build_info.hpp"
#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"
#include "support.hpp"

using namespace fusion::core;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

std::string manifest(std::string_view version, std::string_view files = "") {
    return "{\n  \"fusion_version\": \"" + std::string(version) + "\",\n  \"git_commit\": \"unknown\",\n"
           "  \"files\": {" + std::string(files) + "},\n  \"tools\": {}\n}\n";
}

// The check's failure for the home that FUSION_HOME names, as {code, message}; empty when it passes.
std::pair<std::string, std::string> check(const fs::path& home) {
    const test::ScopedEnv env("FUSION_HOME", home.string());
    try {
        check_home(find_install_dir());
        return {};
    } catch (const InstallError& e) {
        return {e.code(), e.what()};
    }
}

}  // namespace

TEST_CASE("core: a home whose fusion-data.json has this build's version passes the check (S1.20)", "[unit][core]") {
    test::ScratchDir home("home-ok");
    test::write_file(home / "fusion-data.json", manifest(build_info().version));
    CHECK(check(home.path()).first.empty());
}

TEST_CASE("core: a home without fusion-data.json is install.home_missing, naming the paths tried", "[unit][core]") {
    test::ScratchDir home("home-empty");
    const auto [code, message] = check(home.path());
    CHECK(code == "install.home_missing");
    CHECK_THAT(message, ContainsSubstring(to_utf8(home.path())) && ContainsSubstring("FUSION_HOME"));
    // Without FUSION_HOME the home is share/fusion beside bin/, and the message names the executable.
    const test::ScopedEnv unset("FUSION_HOME", std::nullopt);
    const InstallDir beside = find_install_dir();
    if (fs::exists(beside.home / "fusion-data.json")) SKIP("this test executable sits beside a home");
    try {
        check_home(beside);
        FAIL("a home without fusion-data.json must throw");
    } catch (const InstallError& e) {
        CHECK(e.code() == "install.home_missing");
        CHECK_THAT(std::string(e.what()),
                   ContainsSubstring(to_utf8(beside.home)) && ContainsSubstring(to_utf8(executable_path())));
    }
}

TEST_CASE("core: a home of another version is install.version_mismatch", "[unit][core]") {
    test::ScratchDir home("home-other");
    test::write_file(home / "fusion-data.json", manifest("0.0.0-other"));
    const auto [code, message] = check(home.path());
    CHECK(code == "install.version_mismatch");
    CHECK_THAT(message, ContainsSubstring("0.0.0-other") && ContainsSubstring(std::string(build_info().version)) &&
                            ContainsSubstring(to_utf8(home.path())));
}

TEST_CASE("core: a fusion-data.json that cannot be read is install.file_damaged", "[unit][core]") {
    test::ScratchDir home("home-garbled");
    test::write_file(home / "fusion-data.json", "{\"fusion_version\": ");
    CHECK(check(home.path()).first == "install.file_damaged");
}

TEST_CASE("core: a bundled file that differs from its SHA-256 in fusion-data.json is install.file_damaged",
          "[unit][core]") {
    test::ScratchDir home("home-damaged");
    const test::ScopedEnv env("FUSION_HOME", home.path().string());
    if (install_dir().home != home.path()) SKIP("the install directory was worked out earlier in this process");
    fs::create_directories(home / "data");
    test::write_file(home / "data" / "kegg_pathways.txt", "hsa04010\n");
    test::write_file(home / "data" / "hsa_to_gene_symbol_map.txt", "hsa1\tA1BG\n");
    test::write_file(home / "fusion-data.json",
                     manifest(build_info().version,
                              "\"data/kegg_pathways.txt\": {\"bytes\": 9, \"sha256\": \"" +
                                  Sha256::of("hsa04010\n").hex() + "\"}, \"data/hsa_to_gene_symbol_map.txt\": "
                                  "{\"bytes\": 11, \"sha256\": \"" + Sha256::of("hsa1\tA1BG\n").hex() + "\"}"));
    CHECK(home_file("data/kegg_pathways.txt") == home / "data" / "kegg_pathways.txt");
    test::write_file(home / "data" / "hsa_to_gene_symbol_map.txt", "hsa1\tA1BG\nhsa2\tA2M\n");
    try {
        home_file("data/hsa_to_gene_symbol_map.txt");
        FAIL("a damaged bundled file must throw");
    } catch (const InstallError& e) {
        CHECK(e.code() == "install.file_damaged");
        CHECK_THAT(std::string(e.what()), ContainsSubstring("data/hsa_to_gene_symbol_map.txt"));
    }
    try {
        home_file("data/hsa_path_to_path_name_map.txt");
        FAIL("a missing bundled file must throw");
    } catch (const InstallError& e) {
        CHECK(e.code() == "install.file_missing");
    }
}
