// fusion/testkit/scratch.hpp: scratch directories for tests (section 12, "The test kit"), and where the
// test tree's own files are.
#pragma once

#include <filesystem>
#include <string_view>

namespace fusion::testkit {

// A fresh directory with a short unique path under the system's temporary folder. It is removed at the
// end unless keep() was called or the scope ends by an exception (a failed REQUIRE), so a failure's files
// stay for inspection and their path is printed. Removal is retried, as Windows refuses to delete a file
// that another process still has open for a moment.
class ScratchDir {
 public:
  explicit ScratchDir(std::string_view test_name);
  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;
  ~ScratchDir();
  const std::filesystem::path& path() const;
  void keep();
  bool kept() const;

 private:
  std::filesystem::path path_;
  bool keep_ = false;
  int exceptions_ = 0;
};

std::filesystem::path source_dir();   // backend/tests in the source tree

}  // namespace fusion::testkit
