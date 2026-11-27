// Whole-file reads and writes for fusion-test's own code (internal to the driver).
#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "fusion/core/core.hpp"

namespace fusion::fusiontest {

inline std::string slurp(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw core::Failure("fusion_test.unreadable", "cannot read " + core::to_utf8(path));
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Written beside the file under another name, then renamed over it, so a reader never sees half a file.
inline void write_file(const std::filesystem::path& path, std::string_view text) {
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::filesystem::path part = path;
  part += ".fusion-test-part";
  {
    std::ofstream out(part, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) throw core::Failure("fusion_test.unwritable", "cannot write " + core::to_utf8(part));
  }
  std::filesystem::rename(part, path);
}

}  // namespace fusion::fusiontest
