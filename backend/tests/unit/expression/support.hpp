// Test support for the expression module: a step context over a temporary run directory, a log
// sink and an event recorder that keep what they receive, and small file helpers.
#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

namespace fusion::expression::test {

namespace fs = std::filesystem;

inline fs::path tests_dir() { return fs::path(FUSION_TEST_SOURCE_DIR); }

class TempDir {
public:
    TempDir() {
        static std::atomic<unsigned> counter{0};
        std::random_device entropy;
        path_ = fs::temp_directory_path() /
                ("fusion-expression-" + std::to_string(entropy()) + "-" + std::to_string(counter++));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

struct LogLine {
    core::Level level = core::Level::info;
    std::string code, text;
};

class CaptureSink final : public core::LogSink {
public:
    void write(core::Level level, std::string_view code, std::string_view text) override {
        lines.push_back(LogLine{level, std::string(code), std::string(text)});
    }
    std::vector<std::string> warnings(std::string_view code) const {
        std::vector<std::string> out;
        for (const LogLine& line : lines)
            if (line.level == core::Level::warning && line.code == code) out.push_back(line.text);
        return out;
    }
    std::vector<std::string> infos() const {
        std::vector<std::string> out;
        for (const LogLine& line : lines)
            if (line.level == core::Level::info) out.push_back(line.text);
        return out;
    }
    std::vector<LogLine> lines;
};

class RecordingEvents final : public core::Events {
public:
    void emit(std::string_view type, core::Json fields) override {
        events.emplace_back(std::string(type), std::move(fields));
    }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
    std::vector<core::Json> of(std::string_view type) const {
        std::vector<core::Json> out;
        for (const auto& [name, fields] : events)
            if (name == type) out.push_back(fields);
        return out;
    }
    std::vector<std::pair<std::string, core::Json>> events;
};

// A run directory with one step directory, as 09's Files lays them out.
class DirFiles final : public core::Files {
public:
    DirFiles(fs::path run, std::string step) : run_(std::move(run)), step_(std::move(step)) {}
    fs::path input(const fs::path& as_named) override { return as_named; }
    core::OutputFile create(std::string_view name) override {
        return core::OutputFile(step_dir() / core::path_from_utf8(name));
    }
    core::OutputFile create_debug(std::string_view name) override {
        return core::OutputFile(run_ / "debug" / step_ / core::path_from_utf8(name));
    }
    core::Journal& journal(std::string_view) override { throw core::Failure("internal", "no journal in tests"); }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    fs::path step_dir() const override { return run_ / step_; }

private:
    fs::path run_;
    std::string step_;
};

// One step's context: its log, events, settings and files, over a fresh run directory.
struct Harness {
    explicit Harness(std::string step = "cudf", bool debug = false)
        : log(debug), files(dir.path(), step), step_name(std::move(step)),
          ctx{log, events, core::StopToken{}, settings, files, nullptr, step_name} {
        log.add_sink(sink);
    }
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    fs::path run_dir() const { return dir.path(); }
    fs::path step_dir() const { return files.step_dir(); }

    TempDir dir;
    std::shared_ptr<CaptureSink> sink = std::make_shared<CaptureSink>();
    core::Log log;
    RecordingEvents events;
    core::Settings settings;
    DirFiles files;
    std::string step_name;
    core::Context ctx;
    core::Interner interner;
};

inline std::string slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline void spit(const fs::path& path, std::string_view bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// Values read from in-memory text with the run-wide map left empty (ids pass through, 02.C3).
inline GeneValues parse(Harness& h, std::string_view text, const ValueInput& input = ValueInput{}) {
    return parse_values(text, input, network::IdMap::none(), false, h.interner, h.ctx);
}

inline std::vector<std::string> ids_of(const GeneValues& values, const core::Interner& interner) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < values.size(); ++i) out.emplace_back(interner.str(values.id(i)));
    return out;
}

inline std::vector<std::string> ids_of(std::span<const core::GeneId> ids, const core::Interner& interner) {
    std::vector<std::string> out;
    for (const core::GeneId id : ids) out.emplace_back(interner.str(id));
    return out;
}

inline std::vector<core::GeneId> intern(core::Interner& interner, const std::vector<std::string>& ids) {
    std::vector<core::GeneId> out;
    for (const std::string& id : ids) out.push_back(interner.id(id));
    return out;
}

// The code of the core::Failure that f throws, or "none".
template <class F>
std::string failure_code(F&& f) {
    try {
        std::forward<F>(f)();
    } catch (const core::Failure& failure) {
        return failure.code();
    }
    return "none";
}

template <class F>
std::string failure_message(F&& f) {
    try {
        std::forward<F>(f)();
    } catch (const core::Failure& failure) {
        return failure.what();
    }
    return "none";
}

}  // namespace fusion::expression::test
