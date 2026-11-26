// The data root, its manifest and the hash cache; scratch directories; the test seed (section 12, "Test
// data" rules and "Test hooks"; 12.C9). A root that is missing, a file that differs or a file the manifest
// does not list must skip a test with the reason, never pass it.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>
#include <fstream>
#include <optional>

#include "fusion/core/core.hpp"
#include "fusion/testkit/catch.hpp"
#include "fusion/testkit/data.hpp"
#include "fusion/testkit/scratch.hpp"

using namespace fusion::testkit;
using Catch::Matchers::ContainsSubstring;

namespace {

// Sets an environment variable for one scope, then puts the old value back.
class EnvVar {
 public:
  EnvVar(const char* name, std::optional<std::string> value) : name_(name) {
    if (const char* old = std::getenv(name)) old_ = old;
    set(value);
  }
  ~EnvVar() { set(old_); }
  EnvVar(const EnvVar&) = delete;
  EnvVar& operator=(const EnvVar&) = delete;

 private:
  void set(const std::optional<std::string>& value) {
#ifdef _WIN32
    _putenv_s(name_, value ? value->c_str() : "");
#else
    if (value) ::setenv(name_, value->c_str(), 1);
    else ::unsetenv(name_);
#endif
  }
  const char* name_;
  std::optional<std::string> old_;
};

void write(const std::filesystem::path& p, const std::string& text) {
  std::filesystem::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

std::string reason(const std::variant<DataRoot, std::string>& r) {
  return std::holds_alternative<std::string>(r) ? std::get<std::string>(r) : std::string();
}

// A root of two files and its manifest, in a scratch folder.
struct ToyRoot {
  ScratchDir dir{"toy-root"};
  std::filesystem::path root = dir.path() / "root", manifest = dir.path() / "data-manifest.json";
  ToyRoot() {
    write(root / "a" / "one.txt", "one\n");
    write(root / "a" / "two.txt", "two two\n");
    const auto entry = [&](const std::string& path, const std::string& text) {
      return fusion::core::Json{{"path", path}, {"size", text.size()}, {"sha256", fusion::core::Sha256::of(text).hex()},
                                {"from", "test"}};
    };
    fusion::core::Json m{{"schema", 1}, {"root_env", "FUSION_TEST_DATA"},
                         {"files", {entry("a/one.txt", "one\n"), entry("a/two.txt", "two two\n")}}};
    write(manifest, m.dump(2));
  }
};

}  // namespace

TEST_CASE("testkit: require() gives the root once every file passes its hash check", "[unit][testkit]") {
  ToyRoot t;
  const EnvVar env("FUSION_TEST_DATA", fusion::core::to_utf8(t.root));
  const std::vector<std::string> both{"a/one.txt", "a/two.txt"}, folder{"a/"};
  const auto ok = require(both, t.manifest);
  REQUIRE(std::holds_alternative<DataRoot>(ok));
  CHECK(std::get<DataRoot>(ok).at("a/one.txt") == t.root / "a" / "one.txt");
  CHECK(std::holds_alternative<DataRoot>(require(folder, t.manifest)));   // a folder stands for its files
  // The second check reads the cache, keyed by path, size and modification time.
  CHECK(std::filesystem::exists(t.root / ".fusion-test" / "hashes.tsv"));
  const auto entries = load_manifest(t.manifest);
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].from == "test");
  CHECK(verify(DataRoot{t.root}, entries).ok());
}

TEST_CASE("testkit: require() skips on a missing root, a changed or missing file, and an unlisted file", "[unit][testkit]") {
  ToyRoot t;
  const std::vector<std::string> one{"a/one.txt"}, two{"a/two.txt"}, unlisted{"a/three.txt"};
  {
    const EnvVar env("FUSION_TEST_DATA", std::nullopt);
    CHECK_THAT(reason(require(one, t.manifest)), ContainsSubstring("FUSION_TEST_DATA is not set"));
  }
  const EnvVar env("FUSION_TEST_DATA", fusion::core::to_utf8(t.root));
  CHECK_THAT(reason(require(unlisted, t.manifest)), ContainsSubstring("not in data-manifest.json"));
  write(t.root / "a" / "one.txt", "One\n");                 // the same size, another hash
  CHECK_THAT(reason(require(one, t.manifest)), ContainsSubstring("differs from data-manifest.json"));
  CHECK_THAT(reason(require(one, t.manifest)), ContainsSubstring("a/one.txt"));   // the cache keeps the new hash
  std::filesystem::remove(t.root / "a" / "two.txt");
  CHECK_THAT(reason(require(two, t.manifest)), ContainsSubstring("missing from the data root"));
  const auto check = verify(DataRoot{t.root}, load_manifest(t.manifest));
  CHECK(check.mismatched == std::vector<std::string>{"a/one.txt"});
  CHECK(check.missing == std::vector<std::string>{"a/two.txt"});
  CHECK_FALSE(check.ok());
}

TEST_CASE("testkit: a root with one corrupted file skips with the hash reason, the real root untouched",
          "[testkit][kegg]") {
  // A copy of the repository's manifest with one hash changed, so the real root is never written to.
  const auto root = FUSION_REQUIRE_DATA("reference/jun-b10-fdr025/f1/run/up_reg");
  CHECK(std::filesystem::exists(root.at("reference/jun-b10-fdr025/f1/run/up_reg")));
  fusion::core::Json m = fusion::core::Json::parse([] {
    std::ifstream in(manifest_path(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }());
  bool changed = false;
  for (auto& f : m["files"])
    if (f["path"] == "reference/jun-b10-fdr025/f1/run/up_reg") {
      f["sha256"] = std::string(64, '0');
      changed = true;
    }
  REQUIRE(changed);
  ScratchDir dir("corrupted-manifest");
  write(dir.path() / "data-manifest.json", m.dump());
  const std::vector<std::string> paths{"reference/jun-b10-fdr025/f1/run/up_reg"};
  CHECK_THAT(reason(require(paths, dir.path() / "data-manifest.json")),
             ContainsSubstring("differs from data-manifest.json"));
  CHECK(std::holds_alternative<DataRoot>(require(paths)));   // the repository's manifest still passes
}

TEST_CASE("testkit: a scratch folder is removed, unless kept or left by an exception", "[unit][testkit]") {
  std::filesystem::path removed, kept, thrown;
  {
    ScratchDir d("removed");
    removed = d.path();
    CHECK(std::filesystem::is_directory(removed));
    CHECK(removed.filename().string().rfind("ft-removed-", 0) == 0);
  }
  CHECK_FALSE(std::filesystem::exists(removed));
  {
    ScratchDir d("kept");
    kept = d.path();
    d.keep();
    CHECK(d.kept());
  }
  CHECK(std::filesystem::is_directory(kept));
  try {
    ScratchDir d("thrown");
    thrown = d.path();
    throw std::runtime_error("a failed REQUIRE");
  } catch (const std::runtime_error&) {
  }
  CHECK(std::filesystem::is_directory(thrown));
  std::filesystem::remove_all(kept);
  std::filesystem::remove_all(thrown);
  ScratchDir a("same"), b("same");
  CHECK(a.path() != b.path());
  CHECK(std::filesystem::exists(source_dir() / "reference" / "data-manifest.json"));
  CHECK(manifest_path() == source_dir() / "reference" / "data-manifest.json");
}

TEST_CASE("testkit: FUSION_TEST_SEED replaces a test's fixed seed (12.C9)", "[unit][testkit]") {
  {
    const EnvVar env("FUSION_TEST_SEED", std::nullopt);
    CHECK(test_seed(42) == 42);
  }
  {
    const EnvVar env("FUSION_TEST_SEED", "20261126");
    CHECK(test_seed(42) == 20261126);
  }
  const EnvVar env("FUSION_TEST_SEED", "x1");
  CHECK_THROWS_AS(test_seed(42), fusion::core::Failure);
}
