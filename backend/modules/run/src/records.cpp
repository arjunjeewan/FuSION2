// fusion run: the append-only records of a run: events.jsonl (09.C10), the run log (09.C11) and
// the step journals (09.C6), with the small path and text helpers the module shares.
//
// FuSION 1 kept one global debug log, "mylog.txt", opened in the launch directory before main ran
// (Linux/src/main.cc:42-51), renamed only by upfx (main.cc:1331-1340; dropped, S2.29); the console
// was its only other record, captured by each launcher's shell redirection, and exec output went
// to that console through system() (main.cc:1318-1329). Its GUI guessed progress from the command
// echo (S1.10, S8.28). Here each run writes its own log, with every console line, the exec output
// that the interpreter passes through core::Log, and, under the debug option, the debug records
// (S1.25, 09.C11); and a structured event per line for the frontend (A2, 09.C10).
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "internal.hpp"

namespace fusion::run {

namespace detail {

fs::path normal(const fs::path& path, const fs::path& base) {
    if (path.empty()) return {};
    fs::path p = path.is_absolute() ? path : base / path;
    std::error_code ec;
    if (!p.is_absolute()) p = fs::absolute(p, ec);
    return p.lexically_normal();
}

bool is_inside(const fs::path& path, const fs::path& dir) {
    auto d = dir.begin();
    auto p = path.begin();
    for (; d != dir.end(); ++d, ++p) {
        if (d->empty()) continue;                // a trailing separator gives an empty last element
        if (p == path.end() || *p != *d) return false;
    }
    return true;
}

std::string relative_utf8(const fs::path& path, const fs::path& dir) {
    return core::to_utf8(path.lexically_relative(dir));
}

std::string json_line(const core::Json& value) {
    return value.dump(-1, ' ', false, core::Json::error_handler_t::replace);
}

double whole_ms(double seconds) { return std::round(seconds * 1000.0) / 1000.0; }

std::FILE* open_append(const fs::path& file) {
    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
    std::FILE* f = std::fopen(file.c_str(), "ab");   // binary: '\n' on every platform (09)
    if (!f) throw core::Failure("io.write", "cannot open " + core::to_utf8(file) + " for writing");
    return f;
}

std::string batch_prefix(const fs::path& batch, std::uint32_t line) {
    if (batch.empty()) return line ? std::format("line {}: ", line) : std::string();
    const std::string name = core::to_utf8(batch.filename());
    return line ? std::format("{}:{}: ", name, line) : name + ": ";
}

// ---------------------------------------------------------------- events.jsonl

EventLog::EventLog(const fs::path& file) : file_(open_append(file)) {}

EventLog::~EventLog() { close(); }

void EventLog::close() noexcept {
    std::lock_guard lock(mutex_);
    if (file_) std::fclose(file_);
    file_ = nullptr;
}

void EventLog::emit(std::string_view type, const core::Json& fields, const Scope* scope) {
    std::lock_guard lock(mutex_);
    if (!file_) return;
    core::Json event = core::Json::object();
    event["v"] = 1;
    event["seq"] = ++seq_;
    event["t"] = core::utc_timestamp(std::chrono::system_clock::now());
    event["type"] = type;
    if (scope) {
        event["step"] = scope->step;
        event["line"] = scope->line;
        event["point"] = scope->point;
    }
    if (fields.is_object())
        for (const auto& [key, value] : fields.items())
            if (!event.contains(key)) event[key] = value;   // the envelope wins
    std::string text = json_line(event);
    text += '\n';
    // One fwrite per line, then a flush, so a reader following the file sees whole lines (09.C10).
    // An event that cannot be written is not worth stopping a run for: run.json and the log stand.
    if (std::fwrite(text.data(), 1, text.size(), file_) == text.size()) std::fflush(file_);
}

// ---------------------------------------------------------------- the run log

RunLogSink::RunLogSink(const fs::path& file) : file_(open_append(file)) {}

RunLogSink::~RunLogSink() {
    if (file_) std::fclose(file_);
}

void RunLogSink::write(core::Level level, std::string_view code, std::string_view text) {
    static constexpr std::string_view names[] = {"debug", "info", "warning", "error"};
    const std::string_view name = names[static_cast<std::size_t>(level)];
    // "2026-11-12T13:14:00.412Z info  FuSION 2.0.0 ...": UTC time, level, the console's text (09.C11).
    std::string line = std::format("{} {:<5} {}", core::utc_timestamp(std::chrono::system_clock::now()), name, text);
    if (!code.empty()) line += std::format(" [{}]", code);
    line += '\n';
    std::lock_guard lock(mutex_);
    if (std::fwrite(line.data(), 1, line.size(), file_) == line.size()) std::fflush(file_);
}

// ---------------------------------------------------------------- journals

JournalFile::JournalFile(fs::path file) : path_(std::move(file)), file_(open_append(path_)) {}

JournalFile::~JournalFile() { close(); }

void JournalFile::close() noexcept {
    std::lock_guard lock(mutex_);
    if (file_) std::fclose(file_);
    file_ = nullptr;
}

void JournalFile::append(const core::Json& record) {
    std::string text = json_line(record);
    text += '\n';
    std::lock_guard lock(mutex_);
    if (!file_) throw core::Failure("internal", "journal " + core::to_utf8(path_) + " appended after its step ended");
    // Each record is on disk before the next unit of work starts: fflush, then fsync (09.C6).
    bool ok = std::fwrite(text.data(), 1, text.size(), file_) == text.size() && std::fflush(file_) == 0;
#if !defined(_WIN32)
    ok = ok && ::fsync(::fileno(file_)) == 0;
#endif
    if (!ok) throw core::Failure("io.write", "cannot append to " + core::to_utf8(path_) + " (is the disk full?)");
}

}  // namespace detail

std::vector<core::Json> read_journal(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw core::Failure("input.unreadable", "cannot read the journal " + core::to_utf8(file));
    std::ostringstream text;
    text << in.rdbuf();
    const std::string all = std::move(text).str();
    std::vector<core::Json> records;
    std::size_t start = 0;
    // Only lines ended by '\n' are whole; a torn last line, cut by a kill, is ignored (09.C6). A
    // whole line that does not parse holds no record either: its unit of work is simply done again.
    for (std::size_t end = all.find('\n'); end != std::string::npos; end = all.find('\n', start)) {
        std::string_view line(all.data() + start, end - start);
        if (line.ends_with('\r')) line.remove_suffix(1);   // readers accept \r\n (09's text rules)
        start = end + 1;
        core::Json record = core::Json::parse(line, nullptr, false);
        if (!record.is_discarded()) records.push_back(std::move(record));
    }
    return records;
}

}  // namespace fusion::run
