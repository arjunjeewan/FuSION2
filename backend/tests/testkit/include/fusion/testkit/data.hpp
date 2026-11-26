// fusion/testkit/data.hpp: the data root outside the repository (section 12, "Test data and the KEGG
// licence"). Every file is hash-checked before use, hashes are cached by path, size and modification
// time in <root>/.fusion-test/hashes.tsv, and a test whose files are missing, differ or are not in the
// manifest is skipped with that reason, never passed (S8.31: KEGG data never enters the repository).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fusion::testkit {

struct DataRoot {
  std::filesystem::path root;
  static std::optional<DataRoot> from_env();                  // FUSION_TEST_DATA
  std::filesystem::path at(std::string_view relative) const;  // a manifest path ('/'-separated) under the root
};

struct ManifestEntry { std::string path; std::uint64_t size = 0; std::string sha256; std::string from; };
std::vector<ManifestEntry> load_manifest(const std::filesystem::path& data_manifest_json);   // throws core::Failure
std::filesystem::path manifest_path();                       // backend/tests/reference/data-manifest.json

struct DataCheck {
  std::vector<std::string> missing, mismatched;              // manifest paths
  bool ok() const;
};
DataCheck verify(const DataRoot&, std::span<const ManifestEntry>);

// The root, or why to skip. Each path is a manifest path, or a folder ending in '/' that stands for every
// manifest file under it. A path the manifest does not list is a reason to skip: it cannot be hash-checked.
std::variant<DataRoot, std::string> require(std::span<const std::string> paths);
std::variant<DataRoot, std::string> require(std::span<const std::string> paths,
                                            const std::filesystem::path& data_manifest_json);

}  // namespace fusion::testkit
