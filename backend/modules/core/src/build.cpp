// fusion core: the build id and fingerprints (09.C5, 11.C16), and the install directory (S1.20,
// 11.C20).
//
// FuSION 1 baked its build path into the binary (CWD.h, spec S1.24) and found its helpers next to
// /proc/self/exe on Linux but next to the unresolved _NSGetExecutablePath on macOS
// (Linux/src/main.cc:5728-5740, macOS/src/main.cc:5756-5767; S1.20). Here the install directory
// is worked out once, from the resolved executable or FUSION_HOME, the same way on every OS.
#include <cstdlib>

#include "fusion/core/build_info.hpp"
#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"

namespace fusion::core {

std::string_view build_id() {
    // VERSION and the commit; a build with uncommitted changes adds its own executable's SHA-256,
    // computed once, so two such builds never share a fingerprint (09, Saved steps; 11.C16).
    static const std::string id = [] {
        const BuildInfo& b = build_info();
        std::string text = std::string(b.version) + "+" + std::string(b.git_commit);
        if (b.git_dirty) text += ".dirty." + Sha256::of_file(executable_path()).hex();
        return text;
    }();
    return id;
}

bool clean_build() noexcept { return !build_info().git_dirty; }

std::string fingerprint(const Json& fields) {
    // The SHA-256 of canonical JSON: keys sorted in byte order at every level and no spaces
    // (09.C5). nlohmann::json keeps its objects in a std::map, which sorts them that way.
    nlohmann::json canonical(fields);
    canonical["build"] = std::string(build_id());
    return Sha256::of(canonical.dump()).hex();
}

InstallDir find_install_dir() {
    InstallDir dir;
    const std::filesystem::path exe = executable_path();
    dir.bin_dir = exe.parent_path();
    dir.prefix = dir.bin_dir.parent_path();
    if (const char* home = std::getenv("FUSION_HOME"); home != nullptr && *home != '\0') {
        dir.home = std::filesystem::absolute(path_from_utf8(home));   // 11.C20, recorded in run.json
        dir.source = InstallDir::Source::env_override;
    } else {
        dir.home = dir.prefix / "share" / "fusion";
        dir.source = InstallDir::Source::executable;
    }
    dir.tools_dir = dir.home / "tools";
    return dir;
}

const InstallDir& install_dir() {
    static const InstallDir dir = find_install_dir();   // thread-safe initialisation
    return dir;
}

std::filesystem::path home_file(std::string_view rel) {
    const std::filesystem::path file = install_dir().home / path_from_utf8(rel);
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        throw InstallError("install.file_missing", "fusion: " + to_utf8(file) + " is missing; set FUSION_HOME or reinstall",
                           Where{to_utf8(file), 0, {}});
    return file;
}

}  // namespace fusion::core
