// fusion/testkit/catch.hpp: the kit's Catch2 side (header-only), so the kit's libraries need no Catch2.
//   const auto root = FUSION_REQUIRE_DATA("kegg/hsa_pathways_with_exp_score/", ...);
//       the data root, after every named manifest file passed its hash check; otherwise the test is
//       SKIPped with require()'s reason, never passed (section 12, "Test data").
//   FUSION_SEED(2026);   // declares fusion_seed: FUSION_TEST_SEED or 2026, printed with INFO (12.C9)
//   keep_failing_input("test", "case.txt", text)   // a property test's failing input, kept on disk
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fusion/testkit/data.hpp"
#include "fusion/testkit/generate.hpp"
#include "fusion/testkit/scratch.hpp"

#define FUSION_REQUIRE_DATA(...)                                                          \
  ([&]() -> ::fusion::testkit::DataRoot {                                                 \
    const std::vector<std::string> fusion_required_paths{__VA_ARGS__};                    \
    auto fusion_required = ::fusion::testkit::require(fusion_required_paths);             \
    if (std::holds_alternative<std::string>(fusion_required))                             \
      SKIP(std::get<std::string>(fusion_required));                                       \
    return std::get<::fusion::testkit::DataRoot>(fusion_required);                        \
  }())

#define FUSION_SEED(fixed)                                                                \
  const std::uint64_t fusion_seed = ::fusion::testkit::test_seed(fixed);                  \
  INFO("seed " << fusion_seed << " (FUSION_TEST_SEED changes it; 12.C9)")

namespace fusion::testkit {

// Writes a failing generated input into a scratch folder that is kept, and returns its path (12.C9).
inline std::filesystem::path keep_failing_input(std::string_view test, std::string_view file, std::string_view text) {
  ScratchDir dir(test);
  dir.keep();
  const std::filesystem::path p = dir.path() / std::filesystem::path(std::string(file));
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
  std::FILE* f = std::fopen(p.string().c_str(), "wb");
  if (f) {
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
  }
  return p;
}

}  // namespace fusion::testkit
