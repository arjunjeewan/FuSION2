// results/src/site.cpp: the site report's key check (section 07, "Site report"; S6.19). Publishing into
// a site directory (publish_site) waits for a later sprint (decision 0038's brief); its key check runs
// before the run, so it is here now.
#include "fusion/results/site.hpp"

#include <array>

namespace fusion::results {

namespace {

[[noreturn]] void bad_key(std::string_view key, std::string_view why) {
  throw core::Failure("batch.bad_value", "run_key '" + std::string(key) + "' " + std::string(why),
                      core::Where{{}, 0, "run_key"});
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }   // ASCII, no locale

bool equals_ignoring_case(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

}  // namespace

void validate_site_key(std::string_view key) {
  // [A-Za-z0-9][A-Za-z0-9._-]{0,63}, no trailing dot, and no Windows device name in any case, with or
  // without an extension (section 07, Rules). FuSION 1 joined the key into a path unchecked
  // (plot_explanation.py:703-706).
  const auto plain = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
  if (key.empty() || key.size() > 64) bad_key(key, "must be 1 to 64 characters long");
  if (!plain(key.front())) bad_key(key, "must start with a letter or a digit");
  for (const char c : key) {
    if (!plain(c) && c != '.' && c != '_' && c != '-') {
      bad_key(key, "may hold only letters, digits, '.', '_' and '-'");
    }
  }
  if (key.back() == '.') bad_key(key, "must not end in a dot");
  const std::string_view base = key.substr(0, key.find('.'));
  static constexpr std::array<std::string_view, 4> devices{"CON", "PRN", "AUX", "NUL"};
  bool reserved = false;
  for (const std::string_view d : devices) reserved = reserved || equals_ignoring_case(base, d);
  if (base.size() == 4 && (equals_ignoring_case(base.substr(0, 3), "COM") || equals_ignoring_case(base.substr(0, 3), "LPT")) &&
      base[3] >= '0' && base[3] <= '9') {
    reserved = true;
  }
  if (reserved) bad_key(key, "is a reserved file name on Windows");
}

void publish_site(std::span<const SolveResult* const>, const SiteOptions& options, core::Context& ctx) {
  validate_site_key(options.key.empty() ? std::string("run") : options.key);
  throw core::Failure("results.site_unavailable",
                      std::string(ctx.step) + ": publishing into a site directory (site_dir, S6.19) is not "
                                              "available in this build yet");
}

}  // namespace fusion::results
