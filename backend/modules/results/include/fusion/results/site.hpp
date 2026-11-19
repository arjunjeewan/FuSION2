// fusion/results/site.hpp: publishing a run into a site directory (section 07, "Site report"; S6.19).
// The declarations are section 07's outline.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "fusion/core/core.hpp"
#include "fusion/results/model.hpp"

namespace fusion::results {
struct SiteOptions {
  std::filesystem::path dir;
  std::string key;                              // default: the run directory's name
  std::string run_title;                        // default "<source> → <target>" from labels
  std::string takeaway;                         // default: the comparison sentence
  std::string site_title = "FuSION explanation subgraphs";
  std::optional<std::filesystem::path> pareto_png;   // positional token 14; default the solve's own plot
  bool replace = false;
};
void validate_site_key(std::string_view);       // batch check: batch.bad_value
void publish_site(std::span<const SolveResult* const>, const SiteOptions&, core::Context&);
}  // namespace fusion::results
