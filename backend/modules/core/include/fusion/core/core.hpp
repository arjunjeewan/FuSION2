// fusion/core/core.hpp: the vocabulary every module shares (section 09, "Interface"; cross-section item 1.1).
// The declarations are section 09's outline; private state sits behind each class's Impl.
#pragma once

#include <array>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace fusion::core {

using Json = nlohmann::ordered_json;

// ---------------------------------------------------------------- failures and reported results

struct Where {                                   // where a failure was found
    std::string file{};                          // input file, if any
    std::uint32_t line = 0;                      // line in that file; 0 = none
    std::string setting{};                       // setting at fault, if any
};

class Failure : public std::runtime_error {      // stops the batch (S1.28); exit 1
public:
    Failure(std::string code, std::string message, Where where = {});
    const std::string& code() const noexcept;    // "input.missing", "corridor.no_source", ...
    const Where& where() const noexcept;
private:
    std::string code_;
    Where where_;
};
class UsageError : public Failure { using Failure::Failure; };     // bad command line; exit 2
class InstallError : public Failure { using Failure::Failure; };   // section 11's startup checks; exit 1

class Stopped : public std::exception {          // thrown at a safe point after a stop request
public:
    const char* what() const noexcept override { return "stopped"; }
};

struct Report {                                  // a reported result (S4.18, S7.03, 04.C5)
    std::string code{};                          // "corridor.empty", "gsea.empty_selection", "observations.none"
    std::string message{};                       // "no path from EGFR to CXCL8 within bound 3"
};

template <class T>
struct Outcome {
    T value{};
    std::optional<Report> report{};              // set: every later line of the point is skipped
    bool partial = false;                        // a resumable partial result (06.C3)
};
template <>
struct Outcome<void> {                           // a step with no value, such as section 01's handlers
    std::optional<Report> report{};
    bool partial = false;
};

// ---------------------------------------------------------------- stopping
// Core keeps its own stop token, with std's interface, so no target needs an experimental flag.

class StopState;
class StopToken {
public:
    StopToken() = default;                       // never stops
    bool stop_requested() const noexcept;
    void throw_if_stopped() const;               // throws Stopped
private:
    friend class StopSource;
    friend class StopCallback;
    explicit StopToken(std::shared_ptr<StopState> state) : state_(std::move(state)) {}
    std::shared_ptr<StopState> state_;
};
class StopSource {
public:
    StopSource();
    StopToken token() const noexcept;
    bool request_stop() noexcept;                // the first call runs the callbacks
private:
    std::shared_ptr<StopState> state_;
};
class StopCallback {                             // the solve registers z3::context::interrupt()
public:
    StopCallback(const StopToken&, std::function<void()> on_stop);
    ~StopCallback();
    StopCallback(const StopCallback&) = delete;
    StopCallback& operator=(const StopCallback&) = delete;
private:
    std::shared_ptr<StopState> state_;
    std::uint64_t id_ = 0;
};

// ---------------------------------------------------------------- log and events

enum class Level : std::uint8_t { debug, info, warning, error };

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(Level, std::string_view code, std::string_view text) = 0;
};

class Log {                                      // thread-safe; each call is one whole line
public:
    explicit Log(bool debug_on);
    ~Log();
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    void add_sink(std::shared_ptr<LogSink>);
    bool debug_on() const noexcept;
    void debug(std::string_view text);           // run log only, and only under the debug option
    void info(std::string_view text);            // stdout and the run log
    void warning(std::string_view code, std::string_view text);  // stderr, run log, "warning" event
    void error(std::string_view code, std::string_view text);    // stderr, run log, "error" event
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class Events {
public:
    virtual ~Events() = default;
    virtual void emit(std::string_view type, Json fields) = 0;   // adds v, seq and t; inside a step
                                                                 // also step, line and point
    virtual void progress(std::uint64_t done, std::uint64_t total,
                          std::string_view unit) = 0;            // "step.progress", at most 1 per second
};

// ---------------------------------------------------------------- settings (09's registry; 1.16)

enum class Type : std::uint8_t {
    boolean, integer, real, duration, text, choice, input_file, input_list, output_name };
enum class Need : std::uint8_t {
    required,                                    // must be given
    optional,                                    // may be left out: its feature is then off (0021)
    defaulted,                                   // default_text when left out
    computed };                                  // default worked out at run time (run.log, run.cache_dir)
enum class Source : std::uint8_t {               // precedence: step argument, --set, run line, default
    default_value, positional, named,            // E1's three: "default", "positional", "named"
    set_option, run_line };                      // "set" (fusion --set), "run_line" (entry 229)
enum class OnResume : std::uint8_t { must_match, may_change };

struct SettingSpec {                             // constexpr tables, one per module
    std::string_view name;                       // "bound"; the bare name on a command line
    std::string_view module;                     // "corridor"; the key is "corridor.bound"
    Type type;
    Need need;
    std::string_view default_text = {};          // canonical text, for Need::defaulted
    bool affects_results = true;                 // only these enter fingerprints
    bool debug = false;                          // hidden in the frontend (S1.29)
    OnResume on_resume = OnResume::must_match;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    std::span<const std::string_view> choices = {};
    std::string_view help = {};
};
std::string key(const SettingSpec&);             // "<module>.<name>"

using Value = std::variant<bool, std::int64_t, double, std::string, std::vector<std::string>>;
                                                 // a duration is an int64 of milliseconds
Json to_json(const Value&);                      // as run.json and fingerprints record it
std::string_view source_name(Source) noexcept;   // "default", "positional", "named", "set", "run_line"

struct Setting {
    const SettingSpec* spec = nullptr;
    std::optional<Value> value{};                // empty: an optional setting left out
    Source source = Source::default_value;
    std::optional<Value> effective{};            // when the step changed it (S4.16)
    std::string reason{};
};

class Settings {                                 // one step's settings, sorted by key
public:
    Settings() = default;
    explicit Settings(std::vector<Setting>);
    const Setting& at(std::string_view name) const;                 // bare name within the step;
                                                                    // throws Failure("setting.unknown")
    template <class T> const T& get(std::string_view name) const {  // throws if left out
        const Setting& s = at(name);
        if (!s.value) throw_left_out(name);
        return std::get<T>(*s.value);
    }
    template <class T> std::optional<T> find(std::string_view name) const {
        const Setting* s = find_setting(name);
        if (!s || !s->value) return std::nullopt;
        return std::get<T>(*s->value);
    }
    void set_effective(std::string_view name, Value, std::string reason);
    std::span<const Setting> all() const noexcept { return settings_; }
private:
    const Setting* find_setting(std::string_view name) const noexcept;
    [[noreturn]] static void throw_left_out(std::string_view name);
    std::vector<Setting> settings_;
};
Json to_json(const Settings&);

// ---------------------------------------------------------------- ids and stable orderings (2A.5)

struct GeneId {                                  // an interned entity id or node key
    std::uint32_t value = 0;
    friend auto operator<=>(const GeneId&, const GeneId&) = default;
};
struct Sym {                                     // other interned strings: types, subtypes, pathways
    std::uint32_t value = 0;
    friend auto operator<=>(const Sym&, const Sym&) = default;
};
struct HandleHash {                              // for hash maps keyed by a handle; never for output order
    std::size_t operator()(GeneId id) const noexcept { return id.value; }
    std::size_t operator()(Sym s) const noexcept { return s.value; }
};

class Interner {                                 // append-only; handles follow first appearance
public:
    Interner();
    ~Interner();
    Interner(Interner&&) noexcept;
    Interner& operator=(Interner&&) noexcept;
    GeneId id(std::string_view);
    Sym sym(std::string_view);
    std::optional<GeneId> find_id(std::string_view) const noexcept;  // never inserts (S2.15)
    std::optional<Sym> find_sym(std::string_view) const noexcept;
    std::string_view str(GeneId) const noexcept;
    std::string_view str(Sym) const noexcept;
    std::size_t id_count() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::strong_ordering stable_compare(std::string_view, std::string_view) noexcept;   // unsigned bytes
std::uint64_t stable_hash(std::string_view) noexcept;                               // never std::hash

// ---------------------------------------------------------------- digests

struct Sha256 {                                  // a 32-byte digest (cross-section item 1.1)
    std::array<std::byte, 32> bytes{};
    std::string hex() const;
    static Sha256 of(std::string_view data);
    static Sha256 of_file(const std::filesystem::path&);
    friend auto operator<=>(const Sha256&, const Sha256&) = default;
};
class Sha256Builder {                            // streaming, for copies and outputs
public:
    Sha256Builder();
    ~Sha256Builder();
    void update(std::span<const std::byte>);
    void update(std::string_view);
    Sha256 finish();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------- random numbers (09.C15)

class Rng {                                      // everything outside GSEA
public:
    static Rng stream(std::uint64_t seed, std::uint64_t stream_id);  // SplitMix64 seeds the engine
    std::uint64_t next();                        // std::mt19937_64, bit-exact by the standard
    std::uint64_t below(std::uint64_t n);        // uniform in [0, n): Lemire's method
    double unit();                               // uniform in [0, 1): 53 bits
    template <std::random_access_iterator It>
    void shuffle(It first, It last) {            // Fisher-Yates on below()
        const auto n = static_cast<std::uint64_t>(last - first);
        for (std::uint64_t i = n; i > 1; --i) {
            const auto j = below(i);
            using std::swap;
            swap(first[static_cast<std::ptrdiff_t>(i - 1)], first[static_cast<std::ptrdiff_t>(j)]);
        }
    }
private:
    explicit Rng(std::mt19937_64 engine) : engine_(engine) {}
    std::mt19937_64 engine_;
};
// GSEA's generator, Xoshiro256pp with shuffle_like_rand09, is section 08's (08.C1).

// ---------------------------------------------------------------- text, time and the platform layer

std::string format_number(double);              // shortest exact digits, Python repr layout (1.20)

// The decimal grammar of 04.C2, converted by fast_float (11.C5). In core, because the batch's
// real arguments (01) and edge scores (02) use it too (3A.19).
enum class DecimalStatus : std::uint8_t { ok, not_a_number, not_finite };
struct Decimal { DecimalStatus status = DecimalStatus::not_a_number; double value = 0; };
Decimal parse_decimal(std::string_view token) noexcept;
std::string utc_timestamp(std::chrono::system_clock::time_point);    // "2026-11-12T13:14:00.412Z"
std::string to_utf8(const std::filesystem::path&);                   // generic form, '/' separators
std::filesystem::path path_from_utf8(std::string_view);
double cpu_seconds();                            // process CPU time
std::uint64_t peak_rss_bytes();
// path_from_utf8, cpu_seconds, peak_rss_bytes and build_id are implemented by section 11's
// platform layer and build information, which also live in core (11.C19).

std::string_view build_id();                     // VERSION and commit; a build with uncommitted
                                                 // changes adds its executable's SHA-256 (11.C16)
bool clean_build() noexcept;                     // false: uncommitted changes
std::string fingerprint(const Json& fields);     // SHA-256 hex of canonical JSON, build id added

// ---------------------------------------------------------------- checks (1.9; decision 0021)

enum class Severity : std::uint8_t { error, warning, note };
struct Diagnostic {                              // the one type behind --check --json (1.9)
    Severity severity = Severity::error;
    std::uint32_t line = 0, column = 0;
    std::optional<std::uint32_t> point{};
    std::string code{};                          // dotted, e.g. "input.missing"
    std::string message{};
    std::optional<std::string> command{}, setting{}, path{};
};
struct Feature { std::uint32_t line = 0; std::string command{}, feature{}; bool on = false; std::string because{}; };
struct CheckReport {
    std::filesystem::path batch{};
    Sha256 batch_sha256{};
    std::uint32_t points = 1;
    std::vector<Diagnostic> diagnostics{};       // sorted by point, line, column
    std::vector<Feature> features{};             // decision 0021: what is on and why
    bool ok() const noexcept;                    // no error
    Json to_json() const;
};

// ---------------------------------------------------------------- outputs, journals, the store, files

class OutputFile {                               // writes "<name>.tmp"; commit() renames it
public:
    // Called once for the final path, then once for each also_at() copy, in that order.
    using OnCommit = std::function<void(const std::filesystem::path& final_path, const Sha256&)>;
    OutputFile(std::filesystem::path final_path, OnCommit on_commit = {});
    OutputFile(OutputFile&&) noexcept;
    OutputFile& operator=(OutputFile&&) noexcept;
    OutputFile(const OutputFile&) = delete;
    OutputFile& operator=(const OutputFile&) = delete;
    ~OutputFile();                               // not committed: the temp file is removed
    std::ostream& out();                         // binary mode
    void also_at(const std::filesystem::path& absolute);   // copy committed there too (2B.3)
    void commit();                               // flush, rename (retried on Windows), SHA-256,
                                                 // record in run.json through OnCommit
    const std::filesystem::path& path() const noexcept;    // the final path
    const std::filesystem::path& temp_path() const noexcept;  // "<name>.tmp": a program such as gnuplot
                                                 // may write it instead of out(); commit() then hashes it
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class Journal {
public:
    virtual ~Journal() = default;
    virtual void append(const Json& record) = 0; // on disk before it returns (09.C6)
};

class Cache {                                    // the shared store as a module sees it (09.C13)
public:
    virtual ~Cache() = default;
    virtual std::filesystem::path dir(std::string_view kind) = 0;          // <root>/<kind>/
    virtual bool publish(const std::filesystem::path& staged,
                         const std::filesystem::path& final_path) = 0;    // false: another run won
};

class Files {                                    // implemented by run
public:
    virtual ~Files() = default;
    virtual std::filesystem::path input(const std::filesystem::path& as_named) = 0;  // the checked copy
    virtual OutputFile create(std::string_view name) = 0;          // in the step directory
    virtual OutputFile create_debug(std::string_view name) = 0;    // in debug/<step>/
    virtual Journal& journal(std::string_view name) = 0;
    virtual std::span<const Json> resumed(std::string_view name) const = 0;  // the parent's records
    virtual std::filesystem::path step_dir() const = 0;            // where this step's files go
};

struct Context {                                 // what every step receives
    Log& log;
    Events& events;
    StopToken stop;
    const Settings& settings;
    Files& files;
    Cache* cache = nullptr;                      // null: cache=false, uncommitted build, or unusable
    std::string_view step{};                     // "fb_rch", "fb_rch-2"
};

}  // namespace fusion::core
