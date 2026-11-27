// fusion-test data: verify, init and record (data.hpp).
#include "data.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <vector>

#include "files.hpp"
#include "fusion/core/core.hpp"
#include "fusion/testkit/data.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"

namespace fusion::fusiontest::data {

namespace fs = std::filesystem;
using testkit::DataRoot;
using testkit::ManifestEntry;

namespace {

// The first few of a list, then how many more.
void print_some(std::ostream& out, const std::string& what, const std::vector<std::string>& items) {
  if (items.empty()) return;
  out << "  " << items.size() << " " << what << ":\n";
  for (std::size_t k = 0; k < items.size() && k < 20; ++k) out << "    " << items[k] << "\n";
  if (items.size() > 20) out << "    and " << items.size() - 20 << " more\n";
}

std::optional<std::vector<ManifestEntry>> manifest_entries(const fs::path& manifest, std::ostream& err) {
  try {
    return testkit::load_manifest(manifest);
  } catch (const core::Failure& f) {
    err << "error: the data manifest cannot be read: " << f.what() << "\n";
    return std::nullopt;
  }
}

bool inside(const fs::path& path, const fs::path& folder) {
  const fs::path rel = fs::weakly_canonical(path).lexically_relative(fs::weakly_canonical(folder));
  return !rel.empty() && *rel.begin() != "..";
}

// One manifest file from its source: read only, into a file beside its place, which takes the place only
// when its size and SHA-256 are the manifest's. Permissions and modification time follow the source, as
// fill_data_root.sh's cp -p keeps them (the snapshot binary stays executable).
std::optional<std::string> copy_checked(const fs::path& source, const fs::path& dest, const ManifestEntry& e) {
  std::ifstream in(source, std::ios::binary);
  if (!in) return "cannot read " + core::to_utf8(source);
  fs::create_directories(dest.parent_path());
  fs::path part = dest;
  part += ".fusion-test-part";
  std::uint64_t size = 0;
  std::string hex;
  {
    std::ofstream out(part, std::ios::binary | std::ios::trunc);
    core::Sha256Builder hash;
    std::vector<char> buffer(1 << 20);
    for (;;) {
      in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize n = in.gcount();
      if (n > 0) {
        hash.update(std::string_view(buffer.data(), static_cast<std::size_t>(n)));
        out.write(buffer.data(), n);
        size += static_cast<std::uint64_t>(n);
      }
      if (!in) break;
    }
    hex = hash.finish().hex();
    if (in.bad() || !out) {
      out.close();
      fs::remove(part);
      return "cannot copy " + core::to_utf8(source) + " to " + core::to_utf8(dest);
    }
  }
  if (size != e.size || hex != e.sha256) {
    fs::remove(part);
    return core::to_utf8(source) + " differs from data-manifest.json's " + e.path + " (size " + std::to_string(size) +
           ", SHA-256 " + hex + ")";
  }
  fs::permissions(part, fs::status(source).permissions());
  fs::last_write_time(part, fs::last_write_time(source));
  fs::rename(part, dest);
  return std::nullopt;
}

}  // namespace

int verify(const fs::path& root, const fs::path& manifest, std::ostream& out, std::ostream& err) {
  const auto entries = manifest_entries(manifest, err);
  if (!entries) return exit_harness;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    err << "error: the data root " << core::to_utf8(root) << " is not a folder\n";
    return exit_harness;
  }
  const testkit::DataCheck check = testkit::verify(DataRoot{root}, *entries);
  std::uint64_t bytes = 0;
  for (const ManifestEntry& e : *entries) bytes += e.size;
  const std::size_t bad = check.missing.size() + check.mismatched.size();
  out << "data verify: " << entries->size() - bad << " of " << entries->size() << " files (" << bytes
      << " bytes in all) match " << core::to_utf8(manifest) << " in " << core::to_utf8(root) << "\n";
  print_some(out, "missing", check.missing);
  print_some(out, "differing in size or SHA-256", check.mismatched);
  return check.ok() ? exit_ok : exit_failed;
}

int init(const fs::path& from, const fs::path& root, const fs::path& manifest, std::ostream& out, std::ostream& err) {
  const auto entries = manifest_entries(manifest, err);
  if (!entries) return exit_harness;
  std::error_code ec;
  if (!fs::is_directory(from, ec)) {
    err << "error: --from " << core::to_utf8(from) << " is not a folder\n";
    return exit_harness;
  }
  // The root lies outside the trees it is filled from and outside this repository (S8.31), as
  // fill_data_root.sh requires.
  std::set<std::string> trees;
  for (const ManifestEntry& e : *entries) {
    const std::string top = e.from.substr(0, e.from.find('/'));
    if (!top.empty() && fs::is_directory(from / core::path_from_utf8(top), ec)) trees.insert(top);
  }
  std::vector<fs::path> forbidden{testkit::source_dir().parent_path().parent_path()};
  for (const std::string& top : trees) forbidden.push_back(from / core::path_from_utf8(top));
  for (const fs::path& folder : forbidden)
    if (inside(root, folder)) {
      err << "error: the data root " << core::to_utf8(root) << " lies inside "
          << core::to_utf8(fs::weakly_canonical(folder))
          << "; it must lie outside FuSION 1's trees and this repository (S8.31)\n";
      return exit_harness;
    }
  fs::create_directories(root, ec);
  if (!fs::is_directory(root, ec)) {
    err << "error: cannot make the data root " << core::to_utf8(root) << "\n";
    return exit_harness;
  }
  const DataRoot data{root};
  const testkit::DataCheck before = testkit::verify(data, *entries);
  if (!before.mismatched.empty()) {
    err << "error: data init never overwrites a file of the root that differs from data-manifest.json, so it stops "
           "before copying anything\n";
    print_some(err, "differing in size or SHA-256", before.mismatched);
    return exit_failed;
  }
  const std::set<std::string> missing(before.missing.begin(), before.missing.end());
  std::size_t copied = 0;
  std::vector<std::string> no_source;
  for (const ManifestEntry& e : *entries) {
    if (!missing.contains(e.path)) continue;
    const fs::path source = from / core::path_from_utf8(e.from);
    if (e.from.empty() || !fs::is_regular_file(source, ec)) {   // a generated file: from names a work item
      no_source.push_back(e.path + "  (from " + (e.from.empty() ? "nothing" : e.from) + ")");
      continue;
    }
    if (const auto problem = copy_checked(source, data.at(e.path), e)) {
      err << "error: " << *problem << "; data init stops after copying " << copied << " files\n";
      return exit_failed;
    }
    ++copied;
  }
  out << "data init: copied " << copied << " files from " << core::to_utf8(from) << "; "
      << entries->size() - before.missing.size() << " were already in " << core::to_utf8(root) << " and matched\n";
  print_some(out, "with no source under --from, so not copied", no_source);
  return verify(root, manifest, out, err);
}

int record(const std::string& origin, std::span<const std::string> paths, const fs::path& root,
           const fs::path& manifest, std::ostream& out, std::ostream& err) {
  core::Json doc;
  try {
    doc = core::Json::parse(slurp(manifest));
  } catch (const std::exception& e) {
    err << "error: the data manifest cannot be read: " << e.what() << "\n";
    return exit_harness;
  }
  if (!doc.is_object() || !doc.contains("files") || !doc["files"].is_array()) {
    err << "error: " << core::to_utf8(manifest) << " has no files array\n";
    return exit_harness;
  }
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    err << "error: the data root " << core::to_utf8(root) << " is not a folder\n";
    return exit_harness;
  }
  const fs::path base = fs::absolute(root).lexically_normal();
  std::vector<std::string> files;
  for (const std::string& p : paths) {
    fs::path abs = core::path_from_utf8(p);
    if (!abs.is_absolute()) abs = base / abs;
    abs = abs.lexically_normal();
    const fs::path rel = abs.lexically_relative(base);
    if (rel.empty() || rel == "." || *rel.begin() == ".." || *rel.begin() == ".fusion-test") {
      err << "error: " << p << " is not a file or folder of the data root " << core::to_utf8(base)
          << (!rel.empty() && *rel.begin() == ".fusion-test" ? " (the hash cache is not data)" : "") << "\n";
      return exit_harness;
    }
    if (fs::is_regular_file(abs, ec)) {
      files.push_back(core::to_utf8(rel));
    } else if (fs::is_directory(abs, ec)) {
      for (auto it = fs::recursive_directory_iterator(abs, ec); !ec && it != fs::recursive_directory_iterator();
           it.increment(ec))
        if (it->is_regular_file(ec)) files.push_back(core::to_utf8(it->path().lexically_relative(base)));
    } else {
      err << "error: no file or folder " << p << " in the data root " << core::to_utf8(base) << "\n";
      return exit_harness;
    }
  }
  std::sort(files.begin(), files.end());
  files.erase(std::unique(files.begin(), files.end()), files.end());

  core::Json::array_t& listed = doc["files"].get_ref<core::Json::array_t&>();
  std::map<std::string, const core::Json*> by_path;
  for (const core::Json& e : listed) by_path[e.value("path", std::string())] = &e;
  std::vector<core::Json> added;
  std::vector<std::string> conflicts;
  std::size_t already = 0;
  std::uint64_t bytes = 0;
  for (const std::string& rel : files) {
    const fs::path file = base / core::path_from_utf8(rel);
    const std::uint64_t size = fs::file_size(file);
    const std::string sha256 = core::Sha256::of_file(file).hex();
    if (const auto it = by_path.find(rel); it != by_path.end()) {
      if (it->second->value("size", std::uint64_t{0}) == size && it->second->value("sha256", std::string()) == sha256)
        ++already;
      else
        conflicts.push_back(rel);
      continue;
    }
    added.push_back(core::Json{{"path", rel}, {"size", size}, {"sha256", sha256}, {"from", origin}});
    bytes += size;
  }
  if (!conflicts.empty()) {
    err << "error: data record never changes an entry, and these are in data-manifest.json with another size or "
           "SHA-256, so nothing was written\n";
    print_some(err, "differing", conflicts);
    return exit_failed;
  }
  const std::size_t count = added.size();
  if (count > 0) {   // the new entries join the others in path order; every other byte of the file stays
    for (core::Json& e : added) listed.push_back(std::move(e));
    std::stable_sort(listed.begin(), listed.end(), [](const core::Json& a, const core::Json& b) {
      return a.value("path", std::string()) < b.value("path", std::string());
    });
    write_file(manifest, doc.dump(2) + "\n");
  }
  out << "data record: added " << count << " files (" << bytes << " bytes) from \"" << origin << "\" to "
      << core::to_utf8(manifest) << "; " << already << " were listed already\n";
  return exit_ok;
}

}  // namespace fusion::fusiontest::data
