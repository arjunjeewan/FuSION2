// The data root, its manifest and the hash cache (data.hpp).
#include "fusion/testkit/data.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <map>
#include <tuple>

#include "fusion/core/core.hpp"
#include "fusion/testkit/scratch.hpp"
#include "util.hpp"

namespace fusion::testkit {

namespace fs = std::filesystem;

std::optional<DataRoot> DataRoot::from_env() {
  const char* value = std::getenv("FUSION_TEST_DATA");
  if (!value || !*value) return std::nullopt;
  return DataRoot{core::path_from_utf8(value)};
}

fs::path DataRoot::at(std::string_view relative) const { return root / core::path_from_utf8(relative); }

fs::path manifest_path() { return source_dir() / "reference" / "data-manifest.json"; }

std::vector<ManifestEntry> load_manifest(const fs::path& file) {
  core::Json json;
  try {
    json = core::Json::parse(detail::slurp(file));
  } catch (const core::Json::exception& e) {
    throw core::Failure("testkit.manifest", core::to_utf8(file) + ": " + e.what());
  }
  if (!json.contains("files") || !json["files"].is_array())
    throw core::Failure("testkit.manifest", core::to_utf8(file) + ": no files array");
  std::vector<ManifestEntry> out;
  for (const auto& f : json["files"]) {
    ManifestEntry e;
    e.path = f.at("path").get<std::string>();
    e.size = f.at("size").get<std::uint64_t>();
    e.sha256 = f.at("sha256").get<std::string>();
    if (f.contains("from") && f["from"].is_string()) e.from = f["from"].get<std::string>();
    out.push_back(std::move(e));
  }
  return out;
}

bool DataCheck::ok() const { return missing.empty() && mismatched.empty(); }

namespace {

using CacheKey = std::tuple<std::string, std::uint64_t, std::int64_t>;   // path, size, mtime

std::int64_t mtime_of(const fs::path& p) {
  return static_cast<std::int64_t>(fs::last_write_time(p).time_since_epoch().count());
}

std::map<CacheKey, std::string> read_cache(const fs::path& file) {
  std::map<CacheKey, std::string> out;
  std::error_code ec;
  if (!fs::exists(file, ec)) return out;
  try {
    for (const std::string& line : detail::lines_of(detail::slurp(file))) {
      const auto f = detail::split(line, '\t');
      if (f.size() != 4) continue;
      out[{f[0], std::stoull(f[1]), std::stoll(f[2])}] = f[3];
    }
  } catch (...) {
    out.clear();   // a damaged cache is only a cache: every file is hashed again
  }
  return out;
}

// Written to a file of its own, then renamed over the cache, so parallel tests never see half a cache.
void write_cache(const fs::path& file, const std::map<CacheKey, std::string>& cache) {
  static std::atomic<unsigned> counter{0};
  std::string text;
  for (const auto& [k, hash] : cache)
    text += std::get<0>(k) + "\t" + std::to_string(std::get<1>(k)) + "\t" + std::to_string(std::get<2>(k)) + "\t" +
            hash + "\n";
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  const fs::path temp = file.parent_path() /
                        ("hashes." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                         "." + std::to_string(counter++) + ".tmp");
  try {
    detail::write_text(temp, text);
    fs::rename(temp, file, ec);
    if (ec) fs::remove(temp, ec);
  } catch (...) {
    fs::remove(temp, ec);   // a read-only root keeps no cache
  }
}

}  // namespace

DataCheck verify(const DataRoot& root, std::span<const ManifestEntry> entries) {
  DataCheck check;
  const fs::path cache_file = root.root / ".fusion-test" / "hashes.tsv";
  std::map<CacheKey, std::string> cache = read_cache(cache_file);
  bool changed = false;
  for (const ManifestEntry& e : entries) {
    const fs::path p = root.at(e.path);
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
      check.missing.push_back(e.path);
      continue;
    }
    const std::uint64_t size = fs::file_size(p, ec);
    if (ec || size != e.size) {
      check.mismatched.push_back(e.path);
      continue;
    }
    const CacheKey key{e.path, size, mtime_of(p)};
    auto it = cache.find(key);
    if (it == cache.end()) {
      it = cache.emplace(key, core::Sha256::of_file(p).hex()).first;
      changed = true;
    }
    if (it->second != e.sha256) check.mismatched.push_back(e.path);
  }
  if (changed) write_cache(cache_file, cache);
  return check;
}

std::variant<DataRoot, std::string> require(std::span<const std::string> paths) {
  return require(paths, manifest_path());
}

std::variant<DataRoot, std::string> require(std::span<const std::string> paths, const fs::path& manifest) {
  const std::optional<DataRoot> root = DataRoot::from_env();
  if (!root) return std::string("FUSION_TEST_DATA is not set: the test needs the data root (S8.31)");
  std::error_code ec;
  if (!fs::is_directory(root->root, ec))
    return "FUSION_TEST_DATA=" + core::to_utf8(root->root) + " is not a folder";
  std::vector<ManifestEntry> all;
  try {
    all = load_manifest(manifest);
  } catch (const core::Failure& f) {
    return std::string("the data manifest cannot be read: ") + f.what();
  }
  std::vector<ManifestEntry> wanted;
  std::vector<std::string> unlisted;
  for (const std::string& path : paths) {
    const bool folder = !path.empty() && path.back() == '/';
    std::size_t found = 0;
    for (const ManifestEntry& e : all)
      if (folder ? e.path.starts_with(path) : e.path == path) {
        wanted.push_back(e);
        ++found;
      }
    if (found == 0) unlisted.push_back(path);
  }
  const auto first = [](const std::vector<std::string>& items) {
    return items.front() + (items.size() > 1 ? " and " + std::to_string(items.size() - 1) + " more" : "");
  };
  if (!unlisted.empty())
    return "not in data-manifest.json, so it cannot be hash-checked: " + first(unlisted);
  const DataCheck check = verify(*root, wanted);
  if (!check.missing.empty()) return "missing from the data root " + core::to_utf8(root->root) + ": " + first(check.missing);
  if (!check.mismatched.empty()) return "differs from data-manifest.json (size or SHA-256): " + first(check.mismatched);
  return *root;
}

}  // namespace fusion::testkit
