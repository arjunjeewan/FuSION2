// fusion/core/deps.hpp: the external tools a batch needs, checked before its first step (section 11,
// "The dependency check before a run (S6.11)"; 11.C19, 11.C20, cross-section items 2A.1 and 2B.15).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fusion::core::deps {

enum class Kind { program, font };

struct Requirement {                    // one per command line that needs a tool
    std::string id{};                   // "gnuplot", "dot", "sh", "font:Liberation Sans"
    Kind kind = Kind::program;
    std::string min_version{};          // "5.4" for gnuplot (07)
    std::vector<std::string> features{};   // for gnuplot, terminals, e.g. {"pngcairo"}
    std::string requested_by{};         // command and batch line, e.g. "plot_pareto (line 262)"
};

struct Probe {                          // one per id; requirements with the same id are merged
    std::string id{};
    bool ok = false;
    std::filesystem::path path{};
    std::string version{};                // "6.0.5"
    std::string origin{};                 // "bundled", "FUSION_GNUPLOT", "FUSION_DOT", "PATH", "system"
    std::vector<std::string> features{};  // requested features that were found
    std::vector<std::string> missing{};   // requested features or files not found
    std::vector<std::string> needed_by{}; // every requested_by merged into this probe
    std::string message{};                // what to do about it
};

class Registry {
public:
    void require(Requirement r);                   // commands add theirs from the command table (01)
    std::vector<Probe> check() const;              // probes each id once; cached for the process;
                                                   // sorted by id. A probe that is not ok is 11's
                                                   // deps.missing, or deps.font_missing for a font
private:
    std::vector<Requirement> requirements_;
};

}  // namespace fusion::core::deps
