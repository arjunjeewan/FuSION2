// fusion-test data on a toy root, never the real one (the C3 note, C3.2 and C3.4; section 12, "Test data"
// rules): verify through the kit's hash cache, init from a folder holding two toy FuSION trees, and record.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>

#include "data.hpp"
#include "fusion/core/core.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"

using namespace fusion::fusiontest;
using Catch::Matchers::ContainsSubstring;
using fusion::core::Json;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

Json entry(const std::string& path, const std::string& text, const std::string& from) {
  return Json{{"path", path}, {"size", text.size()}, {"sha256", fusion::core::Sha256::of(text).hex()}, {"from", from}};
}

std::string manifest_text(const std::vector<Json>& files) {
  return Json{{"schema", 1}, {"root_env", "FUSION_TEST_DATA"}, {"files", files}}.dump(2) + "\n";
}

struct Streams {
  std::ostringstream out, err;
  std::string all() const { return out.str() + err.str(); }
};

// The folder holding FuSION/ and the snapshot, as the manifest's from paths name them (C3.4), a root to fill,
// and a manifest of three copied files and one that a work item generated.
struct ToyTrees {
  fusion::testkit::ScratchDir dir{"fusion-test-data"};
  fs::path trees = dir.path() / "arjun", root = dir.path() / "root", manifest = dir.path() / "data-manifest.json";
  const std::string kgml = "<pathway name=\"path:t1\"/>\n", binary = "#!/bin/sh\nexit 0\n", script = "print(1)\n",
                    counts = "case\tnodes\nt1\t3\n";
  ToyTrees() {
    write(trees / "FuSION" / "auxilliary" / "t1.xml", kgml);
    write(trees / "FuSION" / "Linux" / "fusion", binary);
    fs::permissions(trees / "FuSION" / "Linux" / "fusion", fs::perms::owner_exec, fs::perm_options::add);
    write(trees / "FuSION1-reference-2026-11-10" / "Linux" / "supporting_files" / "s.py", script);
    write(manifest, manifest_text({entry("fusion1/Linux/fusion", binary, "FuSION/Linux/fusion"),
                                   entry("fusion1/Linux/supporting_files/s.py", script,
                                         "FuSION1-reference-2026-11-10/Linux/supporting_files/s.py"),
                                   entry("kegg/t1.xml", kgml, "FuSION/auxilliary/t1.xml"),
                                   entry("reports/w3/x/counts.tsv", counts, "W3 reports/w3/x")}));
  }
};

}  // namespace

TEST_CASE("fusion-test data verify: 0 when every file matches, 1 for a missing or changed one, 2 without a manifest",
          "[unit][fusion-test]") {
  ToyTrees t;
  write(t.root / "kegg" / "t1.xml", t.kgml);
  write(t.root / "fusion1" / "Linux" / "fusion", t.binary);
  write(t.root / "fusion1" / "Linux" / "supporting_files" / "s.py", t.script);
  write(t.root / "reports" / "w3" / "x" / "counts.tsv", t.counts);
  Streams s;
  CHECK(data::verify(t.root, t.manifest, s.out, s.err) == exit_ok);
  CHECK_THAT(s.out.str(), ContainsSubstring("4 of 4 files"));
  CHECK(fs::exists(t.root / ".fusion-test" / "hashes.tsv"));   // the kit's cache
  // Another size: the cache, keyed by path, size and modification time, cannot hide it within one clock tick.
  write(t.root / "kegg" / "t1.xml", "<pathway name=\"path:t22\"/>\n");
  fs::remove(t.root / "fusion1" / "Linux" / "supporting_files" / "s.py");
  Streams changed;
  CHECK(data::verify(t.root, t.manifest, changed.out, changed.err) == exit_failed);
  CHECK_THAT(changed.out.str(), ContainsSubstring("2 of 4 files"));
  CHECK_THAT(changed.out.str(), ContainsSubstring("1 missing:\n    fusion1/Linux/supporting_files/s.py"));
  CHECK_THAT(changed.out.str(), ContainsSubstring("1 differing in size or SHA-256:\n    kegg/t1.xml"));
  Streams broken;
  write(t.dir.path() / "broken.json", "{\"files\": [");
  CHECK(data::verify(t.root, t.dir.path() / "broken.json", broken.out, broken.err) == exit_harness);
  CHECK_THAT(broken.err.str(), ContainsSubstring("cannot be read"));
  Streams no_root;
  CHECK(data::verify(t.dir.path() / "nowhere", t.manifest, no_root.out, no_root.err) == exit_harness);
}

TEST_CASE("fusion-test data init copies the missing files from both trees, read only, then verifies",
          "[unit][fusion-test]") {
  ToyTrees t;
  const auto source_time = fs::last_write_time(t.trees / "FuSION" / "Linux" / "fusion");
  Streams s;
  // The generated file has no source under --from, so the root still lacks it: a difference (1).
  CHECK(data::init(t.trees, t.root, t.manifest, s.out, s.err) == exit_failed);
  INFO(s.all());
  CHECK_THAT(s.out.str(), ContainsSubstring("copied 3 files"));
  CHECK_THAT(s.out.str(), ContainsSubstring("reports/w3/x/counts.tsv  (from W3 reports/w3/x)"));
  CHECK(read(t.root / "kegg" / "t1.xml") == t.kgml);
  CHECK(read(t.root / "fusion1" / "Linux" / "supporting_files" / "s.py") == t.script);   // from the snapshot
  // As cp -p: the snapshot binary stays executable, with its source's modification time.
  CHECK((fs::status(t.root / "fusion1" / "Linux" / "fusion").permissions() & fs::perms::owner_exec) != fs::perms::none);
  CHECK(fs::last_write_time(t.root / "fusion1" / "Linux" / "fusion") == source_time);
  CHECK(read(t.trees / "FuSION" / "Linux" / "fusion") == t.binary);   // the source is untouched
  CHECK_FALSE(fs::exists(t.root / "kegg" / "t1.xml.fusion-test-part"));
  // With the generated file in place too, init copies nothing and passes.
  write(t.root / "reports" / "w3" / "x" / "counts.tsv", t.counts);
  Streams again;
  CHECK(data::init(t.trees, t.root, t.manifest, again.out, again.err) == exit_ok);
  CHECK_THAT(again.out.str(), ContainsSubstring("copied 0 files"));
  CHECK_THAT(again.out.str(), ContainsSubstring("4 were already in"));
  CHECK_THAT(again.out.str(), ContainsSubstring("4 of 4 files"));
}

TEST_CASE("fusion-test data init never overwrites a file that differs, and refuses a source that differs",
          "[unit][fusion-test]") {
  ToyTrees t;
  write(t.root / "kegg" / "t1.xml", "changed in the root\n");
  Streams s;
  CHECK(data::init(t.trees, t.root, t.manifest, s.out, s.err) == exit_failed);
  CHECK_THAT(s.err.str(), ContainsSubstring("never overwrites"));
  CHECK_THAT(s.err.str(), ContainsSubstring("kegg/t1.xml"));
  CHECK(read(t.root / "kegg" / "t1.xml") == "changed in the root\n");
  CHECK_FALSE(fs::exists(t.root / "fusion1"));                       // it stopped before copying anything
  fs::remove(t.root / "kegg" / "t1.xml");
  write(t.trees / "FuSION" / "auxilliary" / "t1.xml", "<pathway name=\"path:t9\"/>\n");   // same size, other bytes
  Streams source;
  CHECK(data::init(t.trees, t.root, t.manifest, source.out, source.err) == exit_failed);
  CHECK_THAT(source.err.str(), ContainsSubstring("differs from data-manifest.json's kegg/t1.xml"));
  CHECK_FALSE(fs::exists(t.root / "kegg" / "t1.xml"));
  CHECK_FALSE(fs::exists(t.root / "kegg" / "t1.xml.fusion-test-part"));
}

TEST_CASE("fusion-test data init refuses a root inside FuSION 1's trees or this repository (S8.31)",
          "[unit][fusion-test]") {
  ToyTrees t;
  Streams inside_tree;
  CHECK(data::init(t.trees, t.trees / "FuSION" / "data-root", t.manifest, inside_tree.out, inside_tree.err) ==
        exit_harness);
  CHECK_THAT(inside_tree.err.str(), ContainsSubstring("must lie outside"));
  CHECK_FALSE(fs::exists(t.trees / "FuSION" / "data-root"));
  const fs::path in_repo = fusion::testkit::source_dir() / "no-data-root-here";
  Streams inside_repo;
  CHECK(data::init(t.trees, in_repo, t.manifest, inside_repo.out, inside_repo.err) == exit_harness);
  CHECK_FALSE(fs::exists(in_repo));
  Streams no_trees;
  CHECK(data::init(t.dir.path() / "nowhere", t.root, t.manifest, no_trees.out, no_trees.err) == exit_harness);
}

TEST_CASE("fusion-test data record adds generated files in path order, and never changes an entry (C3.2)",
          "[unit][fusion-test]") {
  fusion::testkit::ScratchDir dir("fusion-test-record");
  const fs::path root = dir.path() / "root", manifest = dir.path() / "data-manifest.json";
  write(root / "kegg" / "k.xml", "k\n");
  write(root / "zzz" / "z.txt", "z\n");
  write(root / "reports" / "w9" / "r1" / "a.txt", "a\n");
  write(root / "reports" / "w9" / "r1" / "sub" / "b.txt", "bb\n");
  write(root / "reference" / "c" / "f1" / "capture" / "x.xml", "<x/>\n");
  write(root / "reference" / "c" / "f1" / "run" / "out.dat", "(1,2)\n");
  const Json k = entry("kegg/k.xml", "k\n", "FuSION/k.xml"), z = entry("zzz/z.txt", "z\n", "FuSION/z.txt");
  write(manifest, manifest_text({k, z}));
  const std::string origin = "W9 reports/w9/r1";
  const std::vector<std::string> paths{"reports/w9/r1",   // relative to the root, or absolute inside it
                                       fusion::core::to_utf8(root / "reference" / "c" / "f1" / "capture")};
  Streams s;
  REQUIRE(data::record(origin, paths, root, manifest, s.out, s.err) == exit_ok);
  CHECK_THAT(s.out.str(), ContainsSubstring("added 3 files (10 bytes)"));
  // Every byte but the new entries stays: the file is the same layout with the three entries in path order.
  const std::string after = read(manifest);
  CHECK(after == manifest_text({k, entry("reference/c/f1/capture/x.xml", "<x/>\n", origin),
                                entry("reports/w9/r1/a.txt", "a\n", origin),
                                entry("reports/w9/r1/sub/b.txt", "bb\n", origin), z}));
  Streams verified;
  CHECK(data::verify(root, manifest, verified.out, verified.err) == exit_ok);   // the recorded files pass
  // Again: nothing changes.
  Streams again;
  CHECK(data::record(origin, paths, root, manifest, again.out, again.err) == exit_ok);
  CHECK_THAT(again.out.str(), ContainsSubstring("added 0 files"));
  CHECK_THAT(again.out.str(), ContainsSubstring("3 were listed already"));
  CHECK(read(manifest) == after);
  // A recorded file that changed is refused, and nothing is written.
  write(root / "reports" / "w9" / "r1" / "a.txt", "A\n");
  write(root / "reports" / "w9" / "r1" / "new.txt", "new\n");
  Streams changed;
  CHECK(data::record(origin, paths, root, manifest, changed.out, changed.err) == exit_failed);
  CHECK_THAT(changed.err.str(), ContainsSubstring("never changes an entry"));
  CHECK_THAT(changed.err.str(), ContainsSubstring("reports/w9/r1/a.txt"));
  CHECK(read(manifest) == after);
  // Paths outside the root, the hash cache, the root itself and nothing at all are refused.
  for (const std::string& bad : {std::string("../elsewhere"), fusion::core::to_utf8(dir.path()),
                                 std::string(".fusion-test"), std::string("."), std::string("reports/w0")}) {
    Streams refused;
    const std::vector<std::string> one{bad};
    CHECK(data::record(origin, one, root, manifest, refused.out, refused.err) == exit_harness);
  }
  CHECK(read(manifest) == after);
}
