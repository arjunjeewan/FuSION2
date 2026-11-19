// Helpers for core's unit tests: a scratch directory removed afterwards, an environment variable
// set for one scope, and whole-file reads and writes.
#pragma once

#include <stdlib.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace fusion::core::test {

class ScratchDir {
public:
    explicit ScratchDir(const std::string& name) {
        static std::atomic<int> counter{0};
        path_ = std::filesystem::temp_directory_path() /
                ("fusion-core-test-" + std::to_string(::getpid()) + "-" + std::to_string(counter++) + "-" + name);
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScratchDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    const std::filesystem::path& path() const { return path_; }
    std::filesystem::path operator/(const std::string& rel) const { return path_ / rel; }

private:
    std::filesystem::path path_;
};

class ScopedEnv {                              // sets (or, with nullopt, unsets) one variable
public:
    ScopedEnv(std::string name, const std::optional<std::string>& value) : name_(std::move(name)) {
        if (const char* old = ::getenv(name_.c_str())) old_ = old;
        if (value) ::setenv(name_.c_str(), value->c_str(), 1);
        else ::unsetenv(name_.c_str());
    }
    ~ScopedEnv() {
        if (old_) ::setenv(name_.c_str(), old_->c_str(), 1);
        else ::unsetenv(name_.c_str());
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    std::string name_;
    std::optional<std::string> old_;
};

inline std::string read_file(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

inline void write_file(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary);
    out << text;
}

// A shell script, made executable, standing in for a tool (section 11's test hooks).
inline std::filesystem::path write_script(const std::filesystem::path& file, const std::string& body) {
    write_file(file, "#!/bin/sh\n" + body);
    std::filesystem::permissions(file, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
    return file;
}

inline std::filesystem::path source_dir() { return FUSION_TEST_SOURCE_DIR; }   // backend/tests

}  // namespace fusion::core::test
