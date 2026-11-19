// fusion/run/run.hpp: run directories, steps, saved outputs, the shared store and locks
// (section 09, "Interface"). The declarations are section 09's outline; private state sits behind Impl.
// Lines marked "Added" go beyond the outline; everything else is the outline as written.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"

namespace fusion::run {

enum class RunStatus : std::uint8_t { running, ok, failed, stopped };   // readers infer "died"
enum class StepStatus : std::uint8_t { pending, running, ok, reported, skipped, failed, stopped, loaded };

struct InputSpec {                               // from section 01's plan
    std::string argument;                        // "values"
    std::filesystem::path path;                  // as named, after $ expansion; empty: not given (0021)
    bool required = true;                        // decision 0021
    bool is_list = false;                        // its lines name more inputs (KGML lists)
    std::optional<std::uint32_t> made_by_line;   // the batch makes it (01.C12): handled before its step
    // Added: for a list, the paths its lines name, as the owning module resolves them from the list's
    // checked copy (02.C12). Each member is copied to inputs/<list stem>/ and recorded with member_of.
    // Left empty, a member is copied and recorded when a step first opens it (core::Files::input).
    std::function<std::vector<std::filesystem::path>(const std::filesystem::path& copy)> members;
};

struct StepSpec {
    std::string id;                              // "fb_rch", "fb_rch-2"
    std::string command;                         // canonical name from section 01's table
    std::uint32_t line = 0;                      // batch line
    std::string normalised;                      // section 01's normalised line
    std::string point;                           // the run's sweep point label; empty outside sweeps
    const core::Settings* settings = nullptr;
    std::vector<InputSpec> inputs;
    // Added: the files the step will write, as named (01's CheckContext::output). A relative one lands
    // in the step directory; an absolute one is also copied there (2B.3). The check refuses an output
    // that is the same file as another output or an input (output.clash, 2A.7).
    std::vector<std::filesystem::path> outputs;
};

// Added: section 09's intervals. Tests shorten them; the defaults are the section's.
struct Timing {
    std::chrono::milliseconds stop_poll{500};        // the watcher looks for a stop this often (09.C7)
    std::chrono::milliseconds grace{30000};          // a stop's grace before the forced end (09.C7)
    std::chrono::milliseconds manifest_every{1000};  // run.json at most this often between steps (E1)
    std::chrono::milliseconds progress_every{1000};  // step.progress at most this often per step (09.C10)
};

struct Options {                                 // from the command line and the batch (section 01)
    std::filesystem::path root;
    std::string label;
    std::optional<std::filesystem::path> adopt;  // a directory the frontend made (A1)
    std::optional<std::filesystem::path> parent; // restart or resume from this run (09.C5)
    std::vector<std::string> command_line;
    std::map<std::string, std::string> labels;   // section 01's --tag, such as session=<id>
    std::vector<std::pair<std::string, std::string>> variables;   // section 01's, for run.json
    bool debug = false;                          // --debug only; the run line does not take it (01.C8)
    std::string log_name;                        // empty: "<run name>.log"
    bool cache = true;
    std::optional<std::filesystem::path> cache_dir;
    std::function<void(int exit_status)> on_grace_expired;  // set by the fusion executable (B1)
    // Added
    std::filesystem::path batch;                 // the batch file (09.C3, E1); empty when there is none.
                                                 // Relative inputs resolve against its folder (01.C2).
    std::vector<core::Setting> settings;         // the run-level settings with their sources (01, E1's
                                                 // run_settings); empty: run.* recorded with source default
    std::vector<std::shared_ptr<core::LogSink>> sinks;   // the console; added before the first line
    Timing timing;
};

class Run {
public:
    static Run start(const Options&);            // claim directory and lock; open run.json,
                                                 // events and log; start the stop watcher
    static core::CheckReport check_only(std::span<const StepSpec>);   // 09.C12, on the originals

    Run(Run&&) noexcept;
    Run& operator=(Run&&) noexcept;
    Run(const Run&) = delete;
    Run& operator=(const Run&) = delete;
    ~Run();

    const std::filesystem::path& dir() const noexcept;
    const std::string& id() const noexcept;
    core::Log& log() noexcept;
    core::Events& events() noexcept;             // inside a step, adds step, line and point (1.3)
    core::StopToken stop() const noexcept;

    void copy_inputs(std::span<const StepSpec>);             // 09.C3: copy and hash first (1.13)
    core::CheckReport check(std::span<const StepSpec>);      // decision 0021, on the copies

    class Step {                                 // a running step: its directory and Context
    public:
        Step(Step&&) noexcept;
        Step& operator=(Step&&) noexcept;
        Step(const Step&) = delete;
        Step& operator=(const Step&) = delete;
        ~Step();
        const StepSpec& spec() const noexcept;
        const std::filesystem::path& dir() const noexcept;
        core::Context& context() noexcept;       // what the step's module receives
        // Added: fields the owning module adds to the step's run.json entry, such as section 03's
        // merge key or section 07's results summary (2B.7).
        void detail(std::string_view key, core::Json value);
        // Added: names a committed output's kind and format in run.json, for Registry (09.C4); `file`
        // is relative to the step directory or absolute.
        void artefact(const std::filesystem::path& file, std::string_view kind, std::string_view format,
                      std::uint32_t format_version, std::string_view part = {});
    private:
        friend class Run;
        struct Impl;
        explicit Step(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> impl_;
    };
    Step begin(const StepSpec&);                 // checks and copies inputs the batch made
    bool load_from_parent(Step&);                // 09.C5: true if the parent's output was used
    void end(Step&, StepStatus, std::optional<core::Report> = {}, bool partial = false);
    void fail(Step&, const core::Failure&);
    core::OutputFile create_run_file(std::string_view name);  // top level, at the run's end, whatever its
                                                 // status: bindings.tsv (02), index.html (07); once each

    int finish(RunStatus);                       // final run.json and event; the exit status

    // Added
    void skip(const StepSpec&, std::string_view because);   // step.skipped, after a report or a stop
                                                             // (01.C15); the step gets no directory
    void set(std::string_view key, core::Json value);       // a top-level run.json block: section 11's
                                                             // versions, tools, platform, install, environment
private:
    struct Impl;
    explicit Run(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};

struct Artefact {                                // one saved output, as run.json lists it
    std::string kind;                            // "network", "corridor", "solve", ...
    std::string part;                            // "in", "out", "free"; empty for one-part steps
    std::string format;
    std::uint32_t format_version = 1;
    std::string path;                            // relative to its run directory
    std::uint64_t size = 0;
    core::Sha256 sha256;
    std::string step;
    std::string fingerprint;
};

// A step's fingerprint: core::fingerprint over the command, its must_match result-changing
// settings by key, and the SHA-256 of each input by role (09.C5).
std::string step_fingerprint(std::string_view command, const core::Settings&,
                             const std::map<std::string, core::Sha256>& inputs);

class Registry {                                 // the saved steps of one run directory
public:
    static Registry open(const std::filesystem::path& run_dir);
    Registry(Registry&&) noexcept;
    Registry& operator=(Registry&&) noexcept;
    ~Registry();
    RunStatus status() const;
    bool live() const;                           // its run.lock is held
    std::vector<Artefact> find(std::string_view kind, std::string_view up_to_step = {}) const;
    std::vector<Artefact> match(std::string_view step, std::string_view fingerprint) const;
    std::filesystem::path verified(const Artefact&) const;   // throws Failure("artefact.changed")
private:
    struct Impl;
    explicit Registry(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};

class Store final : public core::Cache {         // 09.C13
public:
    static std::optional<Store> open(const Options&);        // nullopt: off, unclean build or unusable
    Store(Store&&) noexcept;
    Store& operator=(Store&&) noexcept;
    ~Store() override;
    std::filesystem::path dir(std::string_view kind) override;
    bool publish(const std::filesystem::path& staged,
                 const std::filesystem::path& final_path) override;
private:
    explicit Store(std::filesystem::path root);
    std::filesystem::path root_;
};

class Lock {                                     // 09.C9; OS lock on one byte at offset 2^31
public:
    static Lock claim(const std::filesystem::path& run_dir);  // exclusive create of run.lock, then lock
    static bool held(const std::filesystem::path& run_dir);
    static Lock acquire(const std::filesystem::path& file,
                        std::chrono::milliseconds wait);      // any file, e.g. section 07's site (2B.17)
    static bool supported(const std::filesystem::path& dir);  // false on a lock-less filesystem
    Lock(Lock&&) noexcept;
    Lock& operator=(Lock&&) noexcept;
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    ~Lock();                                     // releases the lock
    // Added
    static Lock claim(const std::filesystem::path& run_dir, std::string_view line);  // writes the line
                                                 // ("<pid> <run id>") through the one descriptor
    bool locked() const noexcept;                // false: the filesystem cannot lock (run.lock_unsupported)
private:
    explicit Lock(int handle);
    int handle_ = -1;
    bool locked_ = false;                        // Added
};

// ---------------------------------------------------------------- Added: the pieces behind the classes

// run's registry settings: run.debug, run.log, run.cache and run.cache_dir (section 09's table).
std::span<const core::SettingSpec> settings();

// Cross-section item 1.26: each run of characters outside [A-Za-z0-9._-] becomes one '-', the ends
// are trimmed, the label is cut to max_length (44 for a run, 32 for a sweep point), and Windows'
// rules hold on every platform (no trailing dot; a device name such as CON gets a leading '_').
std::string reduce_label(std::string_view label, std::size_t max_length = 44);
std::string run_name(std::string_view label, std::chrono::system_clock::time_point);   // 09.C1, local time
std::string new_run_id(std::chrono::system_clock::time_point);   // "r-20261112T131400Z-7f3a9c2e"
// Claims <root>/<run_name> with one atomic create_directory, trying -2, -3, ... on a collision
// (09.C1). Throws Failure("run.dir_create") when the root cannot be created or written.
std::filesystem::path claim_run_dir(const std::filesystem::path& root, std::string_view label,
                                    std::chrono::system_clock::time_point);

// 09.C8: Ctrl-C and SIGTERM request the same stop as the stop file; a second Ctrl-C ends the process
// at once with status 130; SIGHUP is ignored. Only the fusion executable calls it (B1): the handlers
// only set a flag, which every Run's stop watcher reads.
void install_signal_handlers();

// 09.C6: a journal's whole records, in order; a torn last line is ignored.
std::vector<core::Json> read_journal(const std::filesystem::path&);

}  // namespace fusion::run
