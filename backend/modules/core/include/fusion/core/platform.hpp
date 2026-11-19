// fusion/core/platform.hpp: section 11's platform layer (11.C19): the process layer of
// process.hpp, per-thread CPU time and long paths. Process CPU time, peak memory and the UTF-8
// path conversions are declared in core.hpp, the executable's path in install.hpp.
#pragma once

#include <chrono>
#include <filesystem>

#include "fusion/core/process.hpp"

namespace fusion::core {

std::chrono::nanoseconds thread_cpu_time();             // per-check CPU time (S5.24, section 06)
std::filesystem::path long_path(const std::filesystem::path&);  // adds \\?\ on Windows when needed

}  // namespace fusion::core
