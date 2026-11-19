// A step's context for the solve tests: events and log lines recorded, files in a fresh directory
// that is removed at the end (09's Context, Files and OutputFile).
#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"

namespace solvekit {

namespace core = fusion::core;

struct Recorder final : core::Events {
  std::mutex mutex;
  std::vector<std::pair<std::string, core::Json>> events;
  void emit(std::string_view type, core::Json fields) override {
    std::lock_guard lock(mutex);
    events.emplace_back(std::string(type), std::move(fields));
  }
  void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
  std::vector<core::Json> of(std::string_view type) {
    std::lock_guard lock(mutex);
    std::vector<core::Json> out;
    for (const auto& [t, f] : events)
      if (t == type) out.push_back(f);
    return out;
  }
};

struct Lines final : core::LogSink {
  std::mutex mutex;
  std::vector<std::string> lines;
  void write(core::Level, std::string_view code, std::string_view text) override {
    std::lock_guard lock(mutex);
    lines.push_back((code.empty() ? "" : std::string(code) + ": ") + std::string(text));
  }
  bool any(std::string_view part) {
    std::lock_guard lock(mutex);
    return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(part) != std::string::npos; });
  }
};

struct StepFiles final : core::Files {
  explicit StepFiles(std::filesystem::path d) : dir(std::move(d)) {}
  std::filesystem::path input(const std::filesystem::path& p) override { return p; }
  core::OutputFile create(std::string_view name) override { return core::OutputFile(dir / name); }
  core::OutputFile create_debug(std::string_view name) override { return core::OutputFile(dir / "debug" / name); }
  core::Journal& journal(std::string_view) override { throw std::logic_error("journals come with resume"); }
  std::span<const core::Json> resumed(std::string_view) const override { return {}; }
  std::filesystem::path step_dir() const override { return dir; }
  std::filesystem::path dir;
};

// One step's context over a fresh directory, removed at the end.
struct Step {
  Step()
      : dir(std::filesystem::temp_directory_path() /
            ("fusion-solve-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))),
        files(dir) {
    log.add_sink(lines);
  }
  ~Step() {
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
  }
  core::Context context() { return core::Context{log, events, stop.token(), settings, files, nullptr, "pathz3"}; }
  std::string read(const std::string& name) const {
    std::ifstream in(dir / name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  std::filesystem::path dir;
  core::Log log{false};
  std::shared_ptr<Lines> lines = std::make_shared<Lines>();
  Recorder events;
  core::Settings settings;
  StepFiles files;
  core::StopSource stop;
};

}  // namespace solvekit
