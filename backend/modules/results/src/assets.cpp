// results/src/assets.cpp: the table of the page assets compiled into libfusion (07.C3, 11.C11). The
// bytes and their SHA-256 come from the sources module.cmake generates with embed_asset.cmake.
#include "fusion/results/assets.hpp"

#include <array>
#include <cstddef>

namespace fusion::results {

namespace asset_data {
extern const unsigned char vis_network_min_js[];
extern const std::size_t vis_network_min_js_size;
extern const char vis_network_min_js_sha256[];
extern const unsigned char fusion_explain_js[];
extern const std::size_t fusion_explain_js_size;
extern const char fusion_explain_js_sha256[];
extern const unsigned char fusion_explain_css[];
extern const std::size_t fusion_explain_css_size;
extern const char fusion_explain_css_sha256[];
extern const unsigned char licenses_txt[];
extern const std::size_t licenses_txt_size;
extern const char licenses_txt_sha256[];
}  // namespace asset_data

namespace {

std::string_view bytes(const unsigned char* data, std::size_t size) {
  return {reinterpret_cast<const char*>(data), size};
}

}  // namespace

std::span<const PageAsset> page_assets() noexcept {
  using namespace asset_data;
  static const std::array<PageAsset, 4> table{{
      {"vis-network.min.js", bytes(vis_network_min_js, vis_network_min_js_size), vis_network_min_js_sha256},
      {"fusion-explain.js", bytes(fusion_explain_js, fusion_explain_js_size), fusion_explain_js_sha256},
      {"fusion-explain.css", bytes(fusion_explain_css, fusion_explain_css_size), fusion_explain_css_sha256},
      {"LICENSES.txt", bytes(licenses_txt, licenses_txt_size), licenses_txt_sha256},
  }};
  return table;
}

const PageAsset* find_page_asset(std::string_view name) noexcept {
  for (const PageAsset& asset : page_assets()) {
    if (asset.name == name) return &asset;
  }
  return nullptr;
}

}  // namespace fusion::results
