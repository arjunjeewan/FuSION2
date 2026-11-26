// Scratch directories (scratch.hpp).
#include "fusion/testkit/scratch.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <thread>

#include "fusion/core/core.hpp"

#ifndef FUSION_TEST_SOURCE_DIR
#error "the test kit is built with FUSION_TEST_SOURCE_DIR, the source tree's backend/tests"
#endif

namespace fusion::testkit {

namespace fs = std::filesystem;

namespace {

// At most 20 characters of the test's name, letters, digits, '-' and '_' only: paths stay short.
std::string short_name(std::string_view name) {
  std::string out;
  for (char c : name) {
    if (out.size() == 20) break;
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    out += ok ? c : '-';
  }
  return out.empty() ? "test" : out;
}

}  // namespace

ScratchDir::ScratchDir(std::string_view test_name) : exceptions_(std::uncaught_exceptions()) {
  static std::atomic<std::uint64_t> counter{0};
  const fs::path base = fs::temp_directory_path();
  const std::string stem = "ft-" + short_name(test_name) + "-";
  for (int attempt = 0;; ++attempt) {
    const std::uint64_t mix = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
                              (counter.fetch_add(1) * 0x9e3779b97f4a7c15ULL) ^
                              (reinterpret_cast<std::uintptr_t>(&counter) << 7);
    const std::string unique = core::Sha256::of(std::to_string(mix)).hex().substr(0, 10);
    const fs::path candidate = base / (stem + unique);
    std::error_code ec;
    if (fs::create_directory(candidate, ec)) {
      path_ = candidate;
      return;
    }
    if (attempt == 100) throw core::Failure("testkit.scratch", "cannot create a folder under " + core::to_utf8(base));
  }
}

ScratchDir::~ScratchDir() {
  if (keep_ || std::uncaught_exceptions() > exceptions_) {
    std::fprintf(stderr, "kept scratch folder: %s\n", core::to_utf8(path_).c_str());
    return;
  }
  for (int attempt = 0; attempt < 5; ++attempt) {
    std::error_code ec;
    fs::remove_all(path_, ec);
    if (!ec) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(50 << attempt));
  }
}

const fs::path& ScratchDir::path() const { return path_; }
void ScratchDir::keep() { keep_ = true; }
bool ScratchDir::kept() const { return keep_; }

fs::path source_dir() { return core::path_from_utf8(FUSION_TEST_SOURCE_DIR); }

}  // namespace fusion::testkit
