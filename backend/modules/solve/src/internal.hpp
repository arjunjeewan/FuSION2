// Private helpers shared by the solve module's sources.
#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "fusion/core/core.hpp"

namespace fusion::solve::detail {

// Where a failure was found: an input file, a line in it, a setting (09's core::Where).
inline core::Where where(std::string file, std::uint32_t line = 0, std::string setting = {}) {
  core::Where w;
  w.file = std::move(file);
  w.line = line;
  w.setting = std::move(setting);
  return w;
}

// The resolution, in seconds, of the clock behind core::thread_cpu_time() (06, "Platform notes").
double thread_cpu_resolution() noexcept;

}  // namespace fusion::solve::detail
