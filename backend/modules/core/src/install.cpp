// fusion core: the install directory and the checks of its home (section 11, "Finding the backend's own
// files (S1.20)"; 11.C19, 11.C20).
//
// FuSION 1 baked its build path into the binary (CWD.h, spec S1.24) and found its helpers next to
// /proc/self/exe on Linux but next to the unresolved _NSGetExecutablePath on macOS
// (Linux/src/main.cc:5728-5740, macOS/src/main.cc:5756-5767; S1.20). Here the install directory
// is worked out once, from the resolved executable or FUSION_HOME, the same way on every OS, and the
// home must hold the fusion-data.json that the build wrote for this version.
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>

#include "fusion/core/build_info.hpp"
#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"

namespace fusion::core {

namespace {

// The home that was tried and where it came from, for the messages (section 11's error table).
std::string tried(const InstallDir& dir) {
    return to_utf8(dir.home) + (dir.source == InstallDir::Source::env_override
                                    ? std::string(" (from FUSION_HOME)")
                                    : " (from " + to_utf8(executable_path()) + ")");
}

std::string remedy(const InstallDir& dir) {
    return dir.source == InstallDir::Source::env_override ? "point FUSION_HOME at an installed home, or unset it"
                                                         : "set FUSION_HOME or reinstall";
}

struct Manifest {
    std::string version;                                       // fusion_version
    std::map<std::string, std::string, std::less<>> sha256;    // each listed file's digest, by path
};

// The home's fusion-data.json, or nullopt when there is none.
std::optional<Manifest> read_manifest(const InstallDir& dir) {
    const std::filesystem::path file = dir.home / "fusion-data.json";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) return std::nullopt;
    std::ifstream in(file, std::ios::binary);
    Manifest m;
    try {
        const nlohmann::json json = nlohmann::json::parse(in);
        m.version = json.at("fusion_version").get<std::string>();
        if (json.contains("files"))
            for (const auto& [rel, entry] : json.at("files").items())
                if (entry.contains("sha256")) m.sha256.emplace(rel, entry.at("sha256").get<std::string>());
    } catch (const nlohmann::json::exception& e) {
        throw InstallError("install.file_damaged",
                           "fusion: " + to_utf8(file) + " cannot be read (" + e.what() + "); " + remedy(dir),
                           Where{to_utf8(file), 0, {}});
    }
    return m;
}

// What home_file checks the bundled files against: install_dir()'s fusion-data.json, read once.
const std::optional<Manifest>& home_manifest() {
    static const std::optional<Manifest> manifest = read_manifest(install_dir());
    return manifest;
}

}  // namespace

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

void check_home(const InstallDir& dir) {
    const std::optional<Manifest> manifest = read_manifest(dir);
    const Where where{to_utf8(dir.home / "fusion-data.json"), 0, {}};
    if (!manifest)
        throw InstallError("install.home_missing",
                           "fusion: fusion-data.json not found in " + tried(dir) + "; " + remedy(dir), where);
    if (manifest->version != build_info().version)
        throw InstallError("install.version_mismatch",
                           "fusion: the home " + tried(dir) + " belongs to FuSION " + manifest->version +
                               ", this binary is " + std::string(build_info().version) + "; " + remedy(dir),
                           where);
}

void check_home() { check_home(install_dir()); }

std::filesystem::path home_file(std::string_view rel) {
    const InstallDir& dir = install_dir();
    const std::filesystem::path file = dir.home / path_from_utf8(rel);
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        throw InstallError("install.file_missing", "fusion: " + to_utf8(file) + " is missing; " + remedy(dir),
                           Where{to_utf8(file), 0, {}});
    // A run hashes the bundled files it uses (section 11), so a file that fusion-data.json lists must
    // still have the SHA-256 listed there. A home without fusion-data.json is check_home's to refuse.
    if (const auto& manifest = home_manifest())
        if (const auto listed = manifest->sha256.find(rel);
            listed != manifest->sha256.end() && Sha256::of_file(file).hex() != listed->second)
            throw InstallError("install.file_damaged",
                               "fusion: " + std::string(rel) + " in " + to_utf8(dir.home) +
                                   " does not match its SHA-256 in fusion-data.json; " + remedy(dir),
                               Where{to_utf8(file), 0, {}});
    return file;
}

}  // namespace fusion::core
