// fusion/core/build_info.hpp: what this build is (section 11, "The portability layer in core";
// 11.C16, 11.C19). Generated at configure time by core's module.cmake from build_info.cpp.in.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace fusion::core {

struct BuildInfo {
    std::string_view version;            // contents of VERSION, e.g. "2.0.0-dev"
    std::string_view git_commit;         // 40 hex digits, or "unknown" for a source archive
    bool git_dirty;                      // 09's build_id() adds the executable's SHA-256 when true;
                                         // also true when the commit is unknown
    std::string_view build_type;         // "Release", "Debug"
    std::string_view compiler;           // "GNU 14.2.1", "AppleClang 17.0.0", "MSVC 19.44.35207"
    std::string_view target;             // "linux-x86_64", "macos-arm64", "windows-x86_64"
    std::int64_t source_date_epoch;      // commit time; no wall-clock build time is embedded
    std::span<const std::pair<std::string_view, std::string_view>> libraries;
                                         // pinned versions as text from cmake/FusionVersions.cmake:
                                         // {"pugixml", "1.16"}, {"nlohmann_json", "3.12.0"}, {"fast_float", "8.3.3"}
};

const BuildInfo& build_info() noexcept;  // generated from build_info.cpp.in; links no library

}  // namespace fusion::core
