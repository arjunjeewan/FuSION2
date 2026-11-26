// fusion/testkit/step_context.hpp: a recording core::Context over a directory (fusion_testkit only;
// C2.3), for library-level stages run outside a run: the reference runner's pinned mode, and later suites.
// Log lines, events and committed files are recorded; inputs are read where they are named; journals keep
// their records in memory. Safe to use from several threads, as a solve's parallel cases do.
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"

namespace fusion::testkit {

class StepContext {
 public:
  explicit StepContext(std::filesystem::path step_dir, std::string step = "step", core::Settings settings = {},
                       bool debug = false);
  StepContext(const StepContext&) = delete;
  StepContext& operator=(const StepContext&) = delete;
  ~StepContext();

  core::Context& ctx();                                     // the context a stage receives
  const std::filesystem::path& dir() const;
  core::StopSource& stop();                                 // request a stop, as A1's watcher would

  struct Line { core::Level level; std::string code, text; };
  std::vector<Line> lines() const;                          // in order
  bool logged(std::string_view part) const;                 // any line's code or text contains it
  std::vector<core::Json> events(std::string_view type = {}) const;   // each with "type"; all when empty
  std::vector<std::filesystem::path> created() const;       // committed files, in commit order
  std::vector<core::Json> journal(std::string_view name) const;

  struct State;

 private:
  std::unique_ptr<State> state_;
};

}  // namespace fusion::testkit
