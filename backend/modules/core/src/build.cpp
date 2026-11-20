// fusion core: the build id and fingerprints (09.C5, 11.C16). The install directory is in install.cpp.
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

}  // namespace fusion::core
