// Helpers for run's tests (section 12's run row): scratch directories, readers for run.json and
// events.jsonl, a log sink that captures lines, and the shape every run.json and event must have.
// The JSON Schemas themselves are section 12's (backend/tests/schemas/, 2A.10); these checks hold
// the same required keys and types until they exist.
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "fusion/run/run.hpp"

namespace fusion::run::test {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// A fresh directory under the system's temporary directory, removed when the test ends.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        path_ = fs::temp_directory_path() / std::format("fusion-run-test-{}-{}", ::getpid(), counter++);
        fs::remove_all(path_);
        fs::create_directories(path_);
        path_ = fs::canonical(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }
    fs::path operator/(const fs::path& p) const { return path_ / p; }
private:
    fs::path path_;
};

inline std::string read_file(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return std::move(text).str();
}

inline void write_file(const fs::path& file, std::string_view text) {
    if (file.has_parent_path()) fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

inline core::Json read_json(const fs::path& file) { return core::Json::parse(read_file(file)); }

inline std::vector<core::Json> read_lines(const fs::path& file) {   // events.jsonl, every line whole
    std::vector<core::Json> lines;
    std::istringstream in(read_file(file));
    for (std::string line; std::getline(in, line);) lines.push_back(core::Json::parse(line));
    return lines;
}

inline std::vector<core::Json> of_type(const std::vector<core::Json>& events, std::string_view type) {
    std::vector<core::Json> found;
    for (const core::Json& e : events)
        if (e["type"] == type) found.push_back(e);
    return found;
}

inline std::vector<std::string> log_lines(const fs::path& file) {
    std::vector<std::string> lines;
    std::istringstream in(read_file(file));
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

struct Line {
    core::Level level;
    std::string code, text;
};

class Capture final : public core::LogSink {     // stands in for the executable's console
public:
    void write(core::Level level, std::string_view code, std::string_view text) override {
        std::lock_guard guard(mutex_);
        lines_.push_back({level, std::string(code), std::string(text)});
    }
    std::vector<Line> lines() const {
        std::lock_guard guard(mutex_);
        return lines_;
    }
    bool has(core::Level level, std::string_view code, std::string_view text_part) const {
        for (const Line& l : lines())
            if (l.level == level && l.code == code && l.text.find(text_part) != std::string::npos) return true;
        return false;
    }
private:
    mutable std::mutex mutex_;
    std::vector<Line> lines_;
};

// Options for a test run: a fast stop watcher and run.json written on every change.
inline Options options_in(const fs::path& root, std::string label = "test") {
    Options o;
    o.root = root;
    o.label = std::move(label);
    o.timing.stop_poll = 20ms;
    o.timing.manifest_every = 0ms;
    o.timing.progress_every = 1000ms;
    return o;
}

inline StepSpec step(std::string id, std::string command, std::uint32_t line) {
    StepSpec s;
    s.id = std::move(id);
    s.command = std::move(command);
    s.line = line;
    s.normalised = s.command;
    return s;
}

inline InputSpec input(std::string argument, fs::path path, bool required = true) {
    InputSpec i;
    i.argument = std::move(argument);
    i.path = std::move(path);
    i.required = required;
    return i;
}

// The code of the core::Failure that body throws; empty when it throws none.
template <class Body>
std::string failure_code(Body body) {
    try {
        body();
    } catch (const core::Failure& failure) {
        return failure.code();
    }
    return {};
}

template <class Predicate>
bool wait_for(Predicate done, std::chrono::milliseconds limit = 3000ms) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(5ms);
    }
    return true;
}

inline bool is_timestamp(const core::Json& value) {
    static const std::regex form(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)");
    return value.is_string() && std::regex_match(value.get<std::string>(), form);
}

inline bool is_sha256(const core::Json& value) {
    static const std::regex form("[0-9a-f]{64}");
    return value.is_string() && std::regex_match(value.get<std::string>(), form);
}

inline bool one_of(const core::Json& value, std::initializer_list<std::string_view> names) {
    if (!value.is_string()) return false;
    for (std::string_view n : names)
        if (value == n) return true;
    return false;
}

// run.json's shape (09's run.json section, E1).
inline void check_manifest(const core::Json& m) {
    REQUIRE(m.is_object());
    CHECK(m["format"] == "fusion.run");
    CHECK(m["format_version"] == 1);
    CHECK(std::regex_match(m["run"].get<std::string>(), std::regex(R"(r-\d{8}T\d{6}Z-[0-9a-f]{8})")));
    CHECK(m["name"].is_string());
    CHECK(one_of(m["status"], {"running", "ok", "failed", "stopped"}));
    CHECK((m["exit"].is_null() || m["exit"].is_number_integer()));
    CHECK(m.contains("parent"));
    CHECK(m.contains("point"));
    CHECK(m["pid"].is_number_integer());
    CHECK(m["labels"].is_object());
    CHECK(is_timestamp(m["started"]));
    CHECK((m["finished"].is_null() || is_timestamp(m["finished"])));
    if (!m["resources"].is_null()) {
        CHECK(m["resources"]["wall_s"].is_number());
        CHECK(m["resources"]["cpu_s"].is_number());
        CHECK(m["resources"]["peak_rss_bytes"].is_number_integer());
    }
    CHECK(m["build_id"].is_string());
    for (const char* block : {"versions", "tools", "platform", "install", "environment"}) CHECK(m.contains(block));
    CHECK(m["command_line"].is_array());
    CHECK(m["launch_dir"].is_string());
    if (!m["batch"].is_null()) {
        CHECK(m["batch"]["path"].is_string());
        CHECK(is_sha256(m["batch"]["sha256"]));
        CHECK(m["batch"]["copy"].is_string());
    }
    CHECK(m["variables"].is_array());
    REQUIRE(m["run_settings"].is_array());
    for (const core::Json& s : m["run_settings"]) {
        CHECK(s["key"].is_string());
        CHECK(s.contains("value"));
        CHECK(one_of(s["source"], {"default", "positional", "named", "set", "run_line"}));
    }
    REQUIRE(m["inputs"].is_array());
    for (const core::Json& i : m["inputs"]) {
        CHECK(i["step"].is_string());
        CHECK(i["argument"].is_string());
        CHECK(i["path"].is_string());
        CHECK(i["size"].is_number_integer());
        CHECK(is_sha256(i["sha256"]));
        CHECK(i["copy"].is_string());
    }
    REQUIRE(m["steps"].is_array());
    for (const core::Json& s : m["steps"]) {
        CHECK(s["id"].is_string());
        CHECK(s["cmd"].is_string());
        CHECK(s["line"].is_number_integer());
        CHECK(one_of(s["status"], {"pending", "running", "ok", "reported", "skipped", "failed", "stopped", "loaded"}));
        CHECK(s["settings"].is_array());
        REQUIRE(s["outputs"].is_array());
        for (const core::Json& o : s["outputs"]) {
            CHECK(o["path"].is_string());
            CHECK(o["size"].is_number_integer());
            CHECK(is_sha256(o["sha256"]));
        }
    }
    CHECK(m["outputs"].is_array());
    CHECK(m["reports"].is_array());
    CHECK((m["error"].is_null() || (m["error"]["code"].is_string() && m["error"]["message"].is_string())));
    if (!m["stop"].is_null()) {
        CHECK(one_of(m["stop"]["source"], {"file", "signal"}));
        CHECK(is_timestamp(m["stop"]["time"]));
        CHECK(m["stop"]["note"].is_string());
        CHECK(m["stop"]["forced"].is_boolean());
    }
}

// events.jsonl's shape (09.C10): the envelope, dotted types (1.3) and each lifecycle type's fields.
inline void check_events(const std::vector<core::Json>& events) {
    static const std::regex dotted(R"([a-z_]+(\.[a-z_]+)+)");
    std::uint64_t seq = 0;
    for (const core::Json& e : events) {
        REQUIRE(e.is_object());
        auto it = e.begin();                     // the envelope comes first, in this order
        REQUIRE(e.size() >= 4);
        CHECK(it.key() == "v");
        CHECK((++it).key() == "seq");
        CHECK((++it).key() == "t");
        CHECK((++it).key() == "type");
        CHECK(e["v"] == 1);
        CHECK(e["seq"] == ++seq);                // rises by one, in file order
        CHECK(is_timestamp(e["t"]));
        const std::string type = e["type"];
        CHECK((type == "warning" || type == "error" || std::regex_match(type, dotted)));
        const auto needs = [&](std::initializer_list<const char*> keys) {
            for (const char* k : keys) CHECK(e.contains(k));
        };
        const auto scoped = [&] { needs({"step", "line", "point"}); };
        if (type == "run.started") needs({"run", "name", "dir", "pid", "fusion", "parent"});
        if (type == "inputs.checked") needs({"inputs", "bytes", "errors", "warnings"});
        if (type == "step.started") { scoped(); needs({"cmd"}); }
        if (type == "step.progress") { scoped(); needs({"done", "total", "unit"}); }
        if (type == "step.finished") {
            scoped();
            needs({"status", "wall_s", "outputs"});
        }
        if (type == "step.skipped") { scoped(); needs({"because"}); }
        if (type == "warning" || type == "error") needs({"code", "message"});
        if (type == "stop.requested") CHECK(one_of(e["source"], {"file", "signal"}));
        if (type == "run.finished") needs({"status", "exit", "wall_s", "forced"});
    }
}

}  // namespace fusion::run::test
