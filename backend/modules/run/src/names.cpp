// fusion run: run directory names, run ids and run's registry settings (section 09, "The run
// directory"; 09.C1; cross-section item 1.26).
//
// FuSION 1 named directories in two ways. The GUI's template deleted any earlier directory of the
// same name and made it again (Linux/advtempscript.txt:110-114; spec S8.21), so a rerun lost the
// earlier results; and each pathz3 made "<lfc>_smt-files_<YYYYMMDD_HHMMSS>" in the launch
// directory, reusing it when the name existed (Linux/src/main.cc:866-887, make_smt_dump_dir_for;
// S1.26, S5.30). Here every run claims a new directory with one atomic create_directory, under a
// label first and a local timestamp, with -2, -3 on a collision, and never deletes one (S8.21).
#include <array>
#include <ctime>
#include <format>
#include <random>
#include <system_error>

#include "internal.hpp"

namespace fusion::run {

namespace {

namespace fs = std::filesystem;

bool allowed(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '_' || c == '-';
}

char lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// Windows' device names, which it reserves with any extension too ("NUL.txt"). Applied on every
// platform, so a name made on Linux is valid on Windows (09.C1).
bool device_name(std::string_view label) {
    const std::string_view stem = label.substr(0, label.find('.'));
    std::string low;
    for (char c : stem) low += lower(c);
    for (std::string_view name : {"con", "prn", "aux", "nul"})
        if (low == name) return true;
    return low.size() == 4 && (low.starts_with("com") || low.starts_with("lpt")) && low[3] >= '0' &&
           low[3] <= '9';
}

void trim_end(std::string& s) {                  // Windows forbids a trailing dot; '-' is trimmed too
    while (!s.empty() && (s.back() == '-' || s.back() == '.')) s.pop_back();
}

std::tm broken_down(std::chrono::system_clock::time_point time, bool local) {
    const std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::tm tm{};
#if defined(_WIN32)
    if (local) localtime_s(&tm, &t); else gmtime_s(&tm, &t);
#else
    if (local) localtime_r(&t, &tm); else gmtime_r(&t, &tm);
#endif
    return tm;
}

// run's settings (section 09's table of run.* settings). None changes results.
constexpr std::array<core::SettingSpec, 4> run_settings{{
    {.name = "cache", .module = "run", .type = core::Type::boolean, .need = core::Need::defaulted,
     .default_text = "true", .affects_results = false, .on_resume = core::OnResume::may_change,
     .help = "use the shared store of reusable results (09.C13)"},
    {.name = "cache_dir", .module = "run", .type = core::Type::text, .need = core::Need::computed,
     .affects_results = false, .on_resume = core::OnResume::may_change,
     .help = "where the shared store lives; FUSION_CACHE_DIR overrides the platform's place (09.C13)"},
    {.name = "debug", .module = "run", .type = core::Type::boolean, .need = core::Need::defaulted,
     .default_text = "false", .affects_results = false, .debug = true, .on_resume = core::OnResume::may_change,
     .help = "the one debug option (S1.29), set only by --debug (01.C8)"},
    {.name = "log", .module = "run", .type = core::Type::text, .need = core::Need::computed,
     .affects_results = false, .on_resume = core::OnResume::may_change,
     .help = "the run log's file name, with no directory part; by default <run name>.log (S1.25)"},
}};

}  // namespace

std::span<const core::SettingSpec> settings() { return run_settings; }

std::string reduce_label(std::string_view label, std::size_t max_length) {
    // Byte by byte: every byte of a non-ASCII code point lies outside the set, so a run of such
    // code points is a run of such bytes, and both become one '-' (1.26).
    std::string out;
    bool in_run = false;
    for (char ch : label) {
        const auto c = static_cast<unsigned char>(ch);
        if (allowed(c)) {
            out += ch;
            in_run = false;
        } else if (!in_run) {
            out += '-';
            in_run = true;
        }
    }
    const auto first = out.find_first_not_of('-');
    out = first == std::string::npos ? std::string() : out.substr(first);
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.size() > max_length) out.resize(max_length);
    trim_end(out);
    if (!out.empty() && device_name(out)) {      // "CON" -> "_CON", "nul.x" -> "_nul.x"
        out.insert(out.begin(), '_');
        if (out.size() > max_length) out.resize(max_length);
        trim_end(out);
    }
    return out;
}

std::string run_name(std::string_view label, std::chrono::system_clock::time_point time) {
    std::string reduced = reduce_label(label);
    if (reduced.empty()) reduced = "run";        // with no label, run_<timestamp> (09.C1)
    const std::tm t = broken_down(time, true);   // local time reads naturally; run.json keeps UTC
    return std::format("{}_{:04}{:02}{:02}-{:02}{:02}{:02}", reduced, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                       t.tm_hour, t.tm_min, t.tm_sec);
}

std::string new_run_id(std::chrono::system_clock::time_point time) {
    // The random part never affects a result (09.C1), so std::random_device is fine here.
    std::random_device device;
    const std::uint32_t random = device();
    const std::tm t = broken_down(time, false);
    return std::format("r-{:04}{:02}{:02}T{:02}{:02}{:02}Z-{:08x}", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                       t.tm_hour, t.tm_min, t.tm_sec, random);
}

fs::path claim_run_dir(const fs::path& root, std::string_view label, std::chrono::system_clock::time_point time) {
    std::error_code ec;
    const fs::path base = fs::absolute(root, ec).lexically_normal();
    if (ec) throw core::Failure("run.dir_create", "cannot use the run root " + core::to_utf8(root) + ": " + ec.message());
    fs::create_directories(base, ec);
    if (ec || !fs::is_directory(base))
        throw core::Failure("run.dir_create",
                            "cannot create the run root " + core::to_utf8(base) + (ec ? ": " + ec.message() : ""));
    const std::string name = run_name(label, time);
    // create_directory is atomic on every platform: it makes the directory or reports that the name
    // exists, so two runs started in one second under one label never share a directory (S8.28).
    for (int n = 1; n <= 9999; ++n) {
        const fs::path dir = base / (n == 1 ? name : name + "-" + std::to_string(n));
        if (fs::create_directory(dir, ec)) return dir;
        if (!ec || ec == std::errc::file_exists) continue;   // the name is taken, by a directory or a file
        throw core::Failure("run.dir_create", "cannot create the run directory " + core::to_utf8(dir) + ": " +
                                                  ec.message());
    }
    throw core::Failure("run.dir_create", "every name from " + name + " to " + name + "-9999 is taken in " +
                                              core::to_utf8(base));
}

}  // namespace fusion::run
