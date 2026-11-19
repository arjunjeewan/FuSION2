// fusion core, private: where an external tool is found (11.C20, cross-section item 1.27).
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fusion::core::detail {

struct Located {
    std::filesystem::path program;      // empty: not found
    std::string origin;                 // the override's name, "bundled" or "PATH"
    std::string tried;                  // where it looked, for a message
};

// The override variable if set (an override that names a missing file is reported, never silently
// replaced), else the bundled copy if one is given and present, else the program on PATH.
Located locate_tool(const char* override_variable, const std::filesystem::path& bundled, std::string_view name);

}  // namespace fusion::core::detail
