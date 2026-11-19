// fusion/core/install.hpp: where the running backend and its bundled files are (section 11,
// "Finding the backend's own files (S1.20)"; 11.C19, 11.C20).
#pragma once

#include <filesystem>
#include <string_view>

namespace fusion::core {

struct InstallDir {
    enum class Source { env_override, executable };
    std::filesystem::path prefix{};    // the directory above bin/
    std::filesystem::path bin_dir{};   // prefix/bin
    std::filesystem::path home{};      // prefix/share/fusion: every bundled file; $FUSION_HOME (01)
    std::filesystem::path tools_dir{}; // home/tools
    Source source = Source::executable;
};

std::filesystem::path executable_path();                  // absolute, symlinks resolved;
                                                          // throws InstallError (install.executable)
const InstallDir& install_dir();                          // computed once, thread-safe
InstallDir find_install_dir();                            // the same, worked out afresh (tests)
std::filesystem::path home_file(std::string_view rel);    // throws InstallError (install.file_missing) if missing
// InstallError is section 09's, derived from core::Failure; this section raises it with install.* codes.
// Not yet: the startup check of the home's fusion-data.json (install.home_missing,
// install.version_mismatch), which waits for the build's staging tree (11, Repository layout).

}  // namespace fusion::core
