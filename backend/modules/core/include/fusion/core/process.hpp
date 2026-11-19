// fusion/core/process.hpp: child processes without a shell (section 11, "The portability layer in
// core"; 11.C19, cross-section item 2A.11). The batch's exec lines (01.C10), the gnuplot runner
// (2B.13), section 07's dot and section 12's test kit start their children here.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fusion::core {

struct ProcessSpec {
    std::filesystem::path program{};                          // absolute; never looked up by a shell
    std::vector<std::string> args{};                          // each passed as one argument
    std::filesystem::path working_dir{};                      // empty: the current directory
    std::vector<std::pair<std::string, std::string>> env_set{};   // added or replaced variables
    enum class Input { null, text } input = Input::null;      // null: /dev/null (NUL on Windows)
    std::string stdin_text{};                                 // e.g. a gnuplot script
    enum class Output { capture, file } output = Output::capture;
    std::filesystem::path output_file{};                      // stdout and stderr together, for Output::file
    bool own_group = false;           // own process group (POSIX) or job object (Windows)
    std::chrono::milliseconds timeout{0};                     // 0 = none
};

struct ProcessResult {
    int exit_code = -1;               // -1 if it could not start or was killed
    bool timed_out = false;
    std::string stdout_text{}, stderr_text{};   // empty for Output::file
    std::chrono::milliseconds wall{0};
    std::string error{};              // why it could not start, or be waited for; else empty
    std::uint64_t peak_rss_bytes = 0; // the child's own peak resident memory, where the OS reports it
};

class Process {                       // move-only handle; the sweep coordinator and exec use it (01)
public:
    static Process spawn(const ProcessSpec&);   // never throws for a child that cannot start:
                                                // wait() then gives exit_code -1 and error
    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    ~Process();                       // a child still running is stopped and reaped
    std::optional<int> poll();        // exit code once finished; also reads captured output so far
    ProcessResult wait();             // until it ends or its timeout passes (then it is stopped)
    void stop();                      // ends the whole group or job; safe from another thread
private:
    struct Impl;
    explicit Process(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};

ProcessResult run_process(const ProcessSpec&);          // spawn, then wait
std::optional<std::filesystem::path> find_on_path(std::string_view program);

}  // namespace fusion::core
