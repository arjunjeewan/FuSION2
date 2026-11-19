// The merge key (03.C8): what it holds and what it leaves out.
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace merge_test;
using fusion::core::Sha256;
using fusion::merge::merge_key;
using fusion::merge::merge_key_text;
using fusion::network::BuildSettings;
using fusion::network::Format;
using fusion::network::IdMap;
using fusion::network::SourceRef;

namespace {

SourceRef input(const char* bytes, Format format, std::string pathway, std::string path) {
    SourceRef s;
    s.path = path;
    s.size = 1;
    s.sha256 = Sha256::of(bytes);
    s.format = format;
    s.pathway = std::move(pathway);
    return s;
}

}  // namespace

TEST_CASE("the key is the SHA-256 of a format line, the build settings and one sorted line per input",
          "[unit][merge][03.C8]") {
    const SourceRef a = input("A", Format::kgml, "path:hsa04010", "/u/kegg/hsa04010.xml");
    const SourceRef b = input("B", Format::kgml, "path:hsa04012", "/u/kegg/hsa04012.xml");
    const std::vector<SourceRef> list = {b, a};
    const std::string text = merge_key_text(list, BuildSettings{}, IdMap::none());
    CHECK(text ==
          "fusion-base-merge 1\n"
          "network.split_threshold=1\n"
          "network.indirect=discard\n"
          "network.ecrel=keep\n"
          "network.maplink=skip\n"
          "559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd\n"
          "df7e70e5021544f4834bbee64a9e3789febc4be81470df629cad6ddb03320a5c\n");
    // Every line, the last included, ends with LF; the digest is computed independently (Python's hashlib).
    CHECK(merge_key(list, BuildSettings{}, IdMap::none()).hex() ==
          "dc5d5fe6a2078cccf7bf49d2102529f3e2b81fca07d141f47aca4c471dc35c9b");
}

TEST_CASE("list order, paths and repeats leave the key unchanged", "[unit][merge][03.C8]") {
    const SourceRef a = input("A", Format::kgml, "path:hsa04010", "/u/kegg/hsa04010.xml");
    const SourceRef b = input("B", Format::kgml, "path:hsa04012", "/u/kegg/hsa04012.xml");
    const SourceRef moved = input("A", Format::kgml, "path:hsa04010", "/elsewhere/copy_of_hsa04010.xml");
    const auto key = merge_key(std::vector<SourceRef>{a, b}, BuildSettings{}, IdMap::none());
    CHECK(merge_key(std::vector<SourceRef>{b, a}, BuildSettings{}, IdMap::none()) == key);
    CHECK(merge_key(std::vector<SourceRef>{b, moved}, BuildSettings{}, IdMap::none()) == key);
    CHECK(merge_key(std::vector<SourceRef>{a, b, a}, BuildSettings{}, IdMap::none()) == key);
    // The same bytes under a new name are the same input: a KGML's line is its SHA-256 alone.
    CHECK(merge_key(std::vector<SourceRef>{input("A", Format::kgml, "path:hsa04010", "/x/renamed.xml"), b},
                    BuildSettings{}, IdMap::none()) == key);
}

TEST_CASE("an edge table renamed changes the key, through the pathway it takes from its name", "[unit][merge][03.C8]") {
    const SourceRef a = input("A", Format::kgml, "path:hsa04010", "/u/kegg/hsa04010.xml");
    const SourceRef table = input("T", Format::edge_table, "my_edges", "/u/my_edges.tsv");
    const SourceRef renamed = input("T", Format::edge_table, "other_edges", "/u/other_edges.tsv");
    const std::vector<SourceRef> list = {a, table};
    CHECK(merge_key_text(list, BuildSettings{}, IdMap::none()).ends_with(
        "559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd\n"
        "e632b7095b0bf32c260fa4c539e9fd7b852d0de454e9be26f24d0d6f91d069d3 my_edges\n"));
    CHECK(merge_key(list, BuildSettings{}, IdMap::none()).hex() ==
          "a7a6f568c52aba3f1bf36ebe82764ef13813d1852f11e3f868db7e20f7308590");
    CHECK(merge_key(std::vector<SourceRef>{a, renamed}, BuildSettings{}, IdMap::none()) !=
          merge_key(list, BuildSettings{}, IdMap::none()));
    // Identical tables under two names stay two inputs with two pathways.
    CHECK(merge_key(std::vector<SourceRef>{a, table, renamed}, BuildSettings{}, IdMap::none()) !=
          merge_key(list, BuildSettings{}, IdMap::none()));
}

TEST_CASE("the build settings and the id map enter the key; E and the label maps do not", "[unit][merge][03.C8]") {
    Context c;
    const std::vector<SourceRef> list = {input("A", Format::kgml, "path:hsa04010", "/u/kegg/hsa04010.xml")};
    const auto key = merge_key(list, BuildSettings{}, IdMap::none());
    BuildSettings t2;
    t2.split_threshold = 2;
    BuildSettings keep;
    keep.filters.indirect = fusion::network::IndirectMode::keep;
    BuildSettings exclude;
    exclude.filters.ecrel = fusion::network::EcrelMode::exclude;
    BuildSettings maplink;
    maplink.filters.maplink = fusion::network::MaplinkMode::keep;
    for (const auto& s : {t2, keep, exclude, maplink}) CHECK(merge_key(list, s, IdMap::none()) != key);
    CHECK(merge_key_text(list, keep, IdMap::none()).find("network.indirect=keep\n") != std::string::npos);

    const auto map_file = c.scratch.write("ids.txt", "GENEX hsa42\n");
    const IdMap map = IdMap::read(map_file, c.ctx);
    const std::string text = merge_key_text(list, BuildSettings{}, map);
    CHECK(text.find("network.id_map=" + map.digest().hex() + "\n") != std::string::npos);
    CHECK(merge_key(list, BuildSettings{}, map) != key);
}

TEST_CASE("the rules version is the key's first line", "[unit][merge][03.C8]") {
    const std::string text = merge_key_text({}, BuildSettings{}, IdMap::none());
    CHECK(text.starts_with("fusion-base-merge " + std::to_string(fusion::merge::rules_version) + "\n"));
}
