// fusion/results/assets.hpp: the explanation pages' library, compiled into libfusion as byte arrays with
// their SHA-256 (07.C3, 11.C11). Added beyond section 07's outline, which names the four files. pex writes
// them byte for byte, once per step (S6.18).
#pragma once

#include <span>
#include <string_view>

namespace fusion::results {

inline constexpr std::string_view vis_network_version = "10.1.5";   // 07.C3, pinned in module.cmake

struct PageAsset {
  std::string_view name;      // "vis-network.min.js", "fusion-explain.js", "fusion-explain.css", "LICENSES.txt"
  std::string_view bytes;     // the file, byte for byte
  std::string_view sha256;    // lower-case hex, computed by the build from the same bytes
};

std::span<const PageAsset> page_assets() noexcept;   // in the order above
const PageAsset* find_page_asset(std::string_view name) noexcept;

}  // namespace fusion::results
