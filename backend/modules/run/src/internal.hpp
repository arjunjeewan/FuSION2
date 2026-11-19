// fusion run: the module's private pieces, shared by its translation units (section 09).
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/run/run.hpp"

namespace fusion::run::detail {

namespace fs = std::filesystem;
using SteadyClock = std::chrono::steady_clock;

// ---------------------------------------------------------------- text and paths

// The absolute, lexically normal form of a path; relative paths resolve against base (01.C2).
fs::path normal(const fs::path& path, const fs::path& base);
bool is_inside(const fs::path& path, const fs::path& dir);           // both normal and absolute
std::string relative_utf8(const fs::path& path, const fs::path& dir); // "cudf/up_reg"
std::string json_line(const core::Json&);       // one compact line; invalid UTF-8 is replaced
double whole_ms(double seconds);                 // durations are recorded to the millisecond
std::FILE* open_append(const fs::path&);         // binary append; throws Failure("io.write")
// "<batch name>:<line>: " for messages (09.C14); "line <n>: " without a batch file.
std::string batch_prefix(const fs::path& batch, std::uint32_t line);

// ---------------------------------------------------------------- 09.C8 and the platform

bool take_signal_request() noexcept;             // true once per signal; the watcher calls it
std::int64_t process_id() noexcept;

// ---------------------------------------------------------------- 09.C10: events.jsonl

struct Scope {                                   // added to every event emitted inside a step (1.3)
    std::string step;
    std::uint32_t line = 0;
    std::string point;
};

class EventLog {
public:
    explicit EventLog(const fs::path& file);
    ~EventLog();
    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;
    // One line, with the envelope v, seq, t and type first, then the scope, then the fields;
    // written with one fwrite and flushed, so seq follows the file's order across threads.
    void emit(std::string_view type, const core::Json& fields, const Scope* scope);
    void close() noexcept;                       // after run.finished, later events are dropped
private:
    std::mutex mutex_;
    std::FILE* file_ = nullptr;
    std::uint64_t seq_ = 0;
};

// ---------------------------------------------------------------- 09.C11: the run log

class RunLogSink final : public core::LogSink {
public:
    explicit RunLogSink(const fs::path& file);
    ~RunLogSink() override;
    RunLogSink(const RunLogSink&) = delete;
    RunLogSink& operator=(const RunLogSink&) = delete;
    void write(core::Level, std::string_view code, std::string_view text) override;
private:
    std::mutex mutex_;
    std::FILE* file_ = nullptr;
};

// ---------------------------------------------------------------- 09.C6: journals

class JournalFile final : public core::Journal {
public:
    explicit JournalFile(fs::path file);
    ~JournalFile() override;
    JournalFile(const JournalFile&) = delete;
    JournalFile& operator=(const JournalFile&) = delete;
    void append(const core::Json& record) override;   // fwrite, fflush, fsync: durable on return
    const fs::path& path() const noexcept { return path_; }
    void close() noexcept;
private:
    std::mutex mutex_;
    fs::path path_;
    std::FILE* file_ = nullptr;
};

// ---------------------------------------------------------------- 09.C3: input copies

struct Problem {                                 // an input that cannot be used, for the check
    std::string code;                            // input.missing, input.not_file, input.unreadable
    std::string message;
};
std::optional<Problem> probe_input(const fs::path& file);   // stat and open, reading nothing

struct InputCopy {
    fs::path original;                           // normal, absolute
    fs::path copy;                               // the checked copy, or the file itself in the run directory
    std::string copy_rel;                        // relative to the run directory, '/' separators
    std::uint64_t size = 0;
    core::Sha256 sha256;
};

class Inputs {                                   // the copies in inputs/, keyed by original path
public:
    Inputs(fs::path run_dir, fs::path base);
    const fs::path& base() const noexcept { return base_; }
    fs::path resolve(const fs::path& as_named) const { return normal(as_named, base_); }
    std::optional<InputCopy> find(const fs::path& resolved) const;
    // Copies the file into folder (relative to the run directory, such as "inputs") and hashes it in
    // the same read; a file already inside the run directory is hashed where it lies (A1, S8.15).
    // A file copied before is copied again only when `fresh` (an input the batch made, 01.C12) and
    // its bytes changed. A source that cannot be read is a Problem; a failed write throws io.write.
    std::optional<InputCopy> take(const fs::path& resolved, const fs::path& folder, bool fresh,
                                  std::optional<Problem>& problem);
    std::size_t count() const noexcept { return count_; }
    std::uint64_t bytes() const noexcept { return bytes_; }
private:
    std::string unique_name(const fs::path& folder, const fs::path& file_name);
    fs::path run_dir_, base_;
    std::map<std::string, InputCopy> by_original_;   // the latest copy of each original
    std::set<std::string> names_;                    // copies' relative paths, lower-cased (macOS, Windows)
    std::size_t count_ = 0;                          // files copied or hashed in place
    std::uint64_t bytes_ = 0;
};

// ---------------------------------------------------------------- the generic checks (0021, 2A.7)

// Output clashes by file identity: two outputs that are one file, or an output that is an input.
void check_outputs(std::span<const StepSpec> steps, const fs::path& base, std::vector<core::Diagnostic>& out);
core::Diagnostic diagnostic(core::Severity, const StepSpec&, std::string_view argument, std::string code,
                            std::string message, const fs::path& path);
std::string member_message(std::string_view argument, const fs::path& list, const Problem&);   // input.list_member
void sort_diagnostics(std::vector<core::Diagnostic>&);

}  // namespace fusion::run::detail
