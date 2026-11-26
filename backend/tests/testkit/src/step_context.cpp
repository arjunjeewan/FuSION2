// The recording step context (step_context.hpp).
#include "fusion/testkit/step_context.hpp"

namespace fusion::testkit {

namespace fs = std::filesystem;

struct StepContext::State {
  struct Sink final : core::LogSink {
    explicit Sink(State& s) : state(s) {}
    void write(core::Level level, std::string_view code, std::string_view text) override {
      std::lock_guard lock(state.mutex);
      state.lines.push_back({level, std::string(code), std::string(text)});
    }
    State& state;
  };
  struct Events final : core::Events {
    explicit Events(State& s) : state(s) {}
    void emit(std::string_view type, core::Json fields) override {
      std::lock_guard lock(state.mutex);
      fields["type"] = std::string(type);
      state.events.push_back(std::move(fields));
    }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
    State& state;
  };
  struct Journal final : core::Journal {
    explicit Journal(State& s) : state(s) {}
    void append(const core::Json& record) override {
      std::lock_guard lock(state.mutex);
      records.push_back(record);
    }
    State& state;
    std::vector<core::Json> records;
  };
  struct Files final : core::Files {
    explicit Files(State& s) : state(s) {}
    fs::path input(const fs::path& as_named) override { return as_named; }
    core::OutputFile create(std::string_view name) override { return file(state.dir / core::path_from_utf8(name)); }
    core::OutputFile create_debug(std::string_view name) override {
      return file(state.dir / "debug" / core::path_from_utf8(name));
    }
    core::Journal& journal(std::string_view name) override {
      std::lock_guard lock(state.mutex);
      auto& slot = state.journals[std::string(name)];
      if (!slot) slot = std::make_unique<Journal>(state);
      return *slot;
    }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    fs::path step_dir() const override { return state.dir; }
    core::OutputFile file(const fs::path& p) {
      fs::create_directories(p.parent_path());
      return core::OutputFile(p, [this](const fs::path& final_path, const core::Sha256&) {
        std::lock_guard lock(state.mutex);
        state.created.push_back(final_path);
      });
    }
    State& state;
  };

  State(fs::path d, std::string s, core::Settings set, bool debug)
      : dir(std::move(d)), step(std::move(s)), settings(std::move(set)), log(debug), sink(std::make_shared<Sink>(*this)),
        events_sink(*this), files(*this),
        ctx{log, events_sink, stop.token(), settings, files, nullptr, step} {
    fs::create_directories(dir);
    log.add_sink(sink);
  }

  mutable std::mutex mutex;
  fs::path dir;
  std::string step;
  core::Settings settings;
  core::Log log;
  std::shared_ptr<Sink> sink;
  Events events_sink;
  Files files;
  core::StopSource stop;
  core::Context ctx;
  std::vector<Line> lines;
  std::vector<core::Json> events;
  std::vector<fs::path> created;
  std::map<std::string, std::unique_ptr<Journal>> journals;
};

StepContext::StepContext(fs::path step_dir, std::string step, core::Settings settings, bool debug)
    : state_(std::make_unique<State>(std::move(step_dir), std::move(step), std::move(settings), debug)) {}
StepContext::~StepContext() = default;

core::Context& StepContext::ctx() { return state_->ctx; }
const fs::path& StepContext::dir() const { return state_->dir; }
core::StopSource& StepContext::stop() { return state_->stop; }

std::vector<StepContext::Line> StepContext::lines() const {
  std::lock_guard lock(state_->mutex);
  return state_->lines;
}

bool StepContext::logged(std::string_view part) const {
  std::lock_guard lock(state_->mutex);
  for (const Line& l : state_->lines)
    if (l.code.find(part) != std::string::npos || l.text.find(part) != std::string::npos) return true;
  return false;
}

std::vector<core::Json> StepContext::events(std::string_view type) const {
  std::lock_guard lock(state_->mutex);
  std::vector<core::Json> out;
  for (const core::Json& e : state_->events)
    if (type.empty() || e["type"] == type) out.push_back(e);
  return out;
}

std::vector<fs::path> StepContext::created() const {
  std::lock_guard lock(state_->mutex);
  return state_->created;
}

std::vector<core::Json> StepContext::journal(std::string_view name) const {
  std::lock_guard lock(state_->mutex);
  const auto it = state_->journals.find(std::string(name));
  return it == state_->journals.end() ? std::vector<core::Json>{} : it->second->records;
}

}  // namespace fusion::testkit
