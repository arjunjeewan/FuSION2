// fusion run: one process's run: its directory, run.json, events, log, stop watcher and steps
// (section 09: "The run directory", "run.json", "Stop", "Saved steps"; 09.C2, 09.C4, 09.C7, E1).
//
// FuSION 1 had no run of its own. Batches built absolute paths from $WORK_DIR and moved each
// fixed-name output out of the launch directory with exec mv (spec S1.26, S5.30; 8 of the 34 exec
// lines of the reference batch), the GUI deleted and remade its run directory (Linux/advtempscript
// .txt:113-114, S8.21), and nothing recorded a run's settings, inputs, outputs or status. Here a run
// claims a new directory, gives every step one directory of its own, records every setting with its
// source, every input and every committed output in run.json, and ends with a recorded status.
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <format>
#include <fstream>
#include <thread>

#include "internal.hpp"

namespace fusion::run {

namespace detail {

namespace {

const core::Settings& no_settings() {
    static const core::Settings none;
    return none;
}

std::string_view status_name(RunStatus status) {
    switch (status) {
    case RunStatus::running: return "running";
    case RunStatus::ok: return "ok";
    case RunStatus::failed: return "failed";
    case RunStatus::stopped: return "stopped";
    }
    return "failed";
}

std::string_view status_name(StepStatus status) {
    switch (status) {
    case StepStatus::pending: return "pending";
    case StepStatus::running: return "running";
    case StepStatus::ok: return "ok";
    case StepStatus::reported: return "reported";
    case StepStatus::skipped: return "skipped";
    case StepStatus::failed: return "failed";
    case StepStatus::stopped: return "stopped";
    case StepStatus::loaded: return "loaded";
    }
    return "failed";
}

int exit_status(RunStatus status) {             // 01.C14 and 09.C14: 0, 1, and 3 for a stop
    switch (status) {
    case RunStatus::ok: return 0;
    case RunStatus::stopped: return 3;
    case RunStatus::running:
    case RunStatus::failed: return 1;
    }
    return 1;
}

double seconds_since(SteadyClock::time_point start) {
    return std::chrono::duration<double>(SteadyClock::now() - start).count();
}

// The top level of a run directory (09.C2): its own files, and the names section 01's table must not
// use for commands.
bool reserved_top_level(std::string_view name) {
    for (std::string_view r : {"run.json", "run.json.tmp", "events.jsonl", "run.lock", "stop", "run.batch",
                               "inputs", "debug", "points", "overrides"})
        if (name == r) return true;
    return false;
}

bool plain_name(std::string_view name) {
    return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string_view::npos;
}

// A name a module gives Files: relative, and inside the folder it is for.
fs::path inside(const fs::path& folder, std::string_view name, std::string_view what) {
    const fs::path rel = core::path_from_utf8(name).lexically_normal();
    if (name.empty() || rel.empty() || rel.has_root_path() || rel == "." || *rel.begin() == "..")
        throw core::Failure("internal", std::format("{} name '{}' must be a relative path inside {}", what, name,
                                                    core::to_utf8(folder)));
    return folder / rel;
}

std::string read_note(const fs::path& file) {  // the stop file's text, kept as the stop's note (09.C7)
    std::ifstream in(file, std::ios::binary);
    std::string note(4096, '\0');
    in.read(note.data(), static_cast<std::streamsize>(note.size()));
    note.resize(static_cast<std::size_t>(std::max<std::streamsize>(in.gcount(), 0)));
    while (!note.empty() && (note.back() == '\n' || note.back() == '\r')) note.pop_back();
    return note;
}

std::string_view version_of(std::string_view build) { return build.substr(0, build.find('+')); }

}  // namespace

class RunState;
class StepState;

class StepFiles final : public core::Files {    // what a step writes, and where (09.C2, 09.C4)
public:
    explicit StepFiles(StepState& step) : step_(step) {}
    fs::path input(const fs::path& as_named) override;
    core::OutputFile create(std::string_view name) override;
    core::OutputFile create_debug(std::string_view name) override;
    core::Journal& journal(std::string_view name) override;
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }   // no parent (09.C5)
    fs::path step_dir() const override;
private:
    StepState& step_;
};

class StepEvents final : public core::Events {  // the step's events carry step, line and point (1.3)
public:
    explicit StepEvents(StepState& step) : step_(step) {}
    void emit(std::string_view type, core::Json fields) override;
    void progress(std::uint64_t done, std::uint64_t total, std::string_view unit) override;
private:
    StepState& step_;
};

class RunEvents final : public core::Events {   // Run::events(): the current step's scope, if any
public:
    explicit RunEvents(RunState& run) : run_(run) {}
    void emit(std::string_view type, core::Json fields) override;
    void progress(std::uint64_t done, std::uint64_t total, std::string_view unit) override;
private:
    RunState& run_;
};

class LogEvents final : public core::LogSink {  // every warning and error line is also an event (09.C10)
public:
    explicit LogEvents(RunEvents& events) : events_(events) {}
    void write(core::Level level, std::string_view code, std::string_view text) override {
        if (level == core::Level::warning || level == core::Level::error)
            events_.emit(level == core::Level::warning ? "warning" : "error", {{"code", code}, {"message", text}});
    }
private:
    RunEvents& events_;
};

class StepState {
public:
    StepState(RunState& run, const StepSpec& spec);
    ~StepState();
    StepState(const StepState&) = delete;
    StepState& operator=(const StepState&) = delete;

    core::OutputFile open_output(const fs::path& folder, std::string_view name);
    void close_journals();                       // at the step's end; each is then recorded as an output

    RunState& run;
    const StepSpec spec;
    const fs::path dir;                          // <run>/<id>, made when the first file is
    const fs::path debug_dir;                    // <run>/debug/<id>, only under the debug option
    const Scope scope;
    const SteadyClock::time_point started = SteadyClock::now();
    std::atomic<bool> ended{false};

    std::mutex mutex;                            // names, journals, progress
    std::set<std::string> names;
    std::map<std::string, std::unique_ptr<JournalFile>> journals;
    bool journals_closed = false;
    std::optional<SteadyClock::time_point> last_progress;

    StepFiles files{*this};
    StepEvents events{*this};
    core::Context context;                       // last: it refers to the members above
};

class RunState {
public:
    explicit RunState(const Options& given) : options(given), log(given.debug) {}
    ~RunState();
    RunState(const RunState&) = delete;
    RunState& operator=(const RunState&) = delete;

    void open();
    void shutdown() noexcept;

    // run.json; the callers hold `mutex`.
    core::Json skeleton() const;
    core::Json run_settings_json() const;
    bool write_manifest();
    void touch(bool now);
    void register_step(const StepSpec&);
    core::Json& step_record(const std::string& id) { return doc["steps"][step_index.at(id)]; }
    void record_input(const StepSpec&, std::string_view argument, const InputCopy&, std::string_view member_of);

    // Takes no lock of `mutex` on entry.
    void record_output(const std::string& step_id, const fs::path& path, const core::Sha256&);
    void record_batch();
    std::optional<InputCopy> take(const StepSpec&, std::string_view argument, const fs::path& as_named,
                                  const InputSpec* input, bool fresh, std::vector<core::Diagnostic>& problems);
    std::optional<int> finalize(RunStatus, bool forced);
    void set_current(StepState*);
    void clear_current(StepState*);
    std::optional<Scope> current_scope();

    // The stop watcher (09.C7).
    void watch();
    void poll_stop();
    void request(const std::string& source);
    void force();

    Options options;
    std::chrono::system_clock::time_point started_at;
    SteadyClock::time_point started_steady;
    std::string run_id, name, log_name;
    fs::path dir, base, batch, launch_dir;      // base: relative inputs resolve here (01.C2)
    std::optional<Lock> lock;                    // released last
    core::Log log;
    std::unique_ptr<EventLog> events;
    RunEvents run_events{*this};
    core::StopSource stop_source;

    std::mutex mutex;                            // run.json and the bookkeeping below
    core::Json doc;
    bool dirty = false;
    bool write_warned = false;
    SteadyClock::time_point last_write{};
    bool finished = false;
    int exit = 1;
    std::map<std::string, std::size_t> step_index;
    std::set<std::string> copied_steps;          // copy_inputs has handled them
    std::set<std::string> run_files;
    std::vector<core::Diagnostic> pending;       // the copy's problems, for check()
    bool batch_recorded = false;
    std::optional<core::Sha256> batch_sha256;

    std::mutex inputs_mutex;                     // taken before `mutex`, never after it
    std::optional<Inputs> inputs;

    std::mutex scope_mutex;
    StepState* current = nullptr;

    std::mutex watch_mutex;
    std::condition_variable watch_cv;
    bool shutting_down = false;
    std::atomic<bool> stop_seen{false};
    SteadyClock::time_point stop_at{};
    std::thread watcher;
};

// ---------------------------------------------------------------- steps

StepState::StepState(RunState& run_state, const StepSpec& step_spec)
    : run(run_state),
      spec(step_spec),
      dir(run_state.dir / core::path_from_utf8(step_spec.id)),
      debug_dir(run_state.dir / "debug" / core::path_from_utf8(step_spec.id)),
      scope{step_spec.id, step_spec.line, step_spec.point},
      context{run_state.log,       events, run_state.stop_source.token(),
              spec.settings ? *spec.settings : no_settings(),
              files,               nullptr, spec.id} {}

StepState::~StepState() {
    close_journals();
    run.clear_current(this);
}

core::OutputFile StepState::open_output(const fs::path& folder, std::string_view name) {
    const fs::path path = inside(folder, name, "an output");
    {
        std::lock_guard guard(mutex);
        std::error_code ec;
        // A committed output is never overwritten (09's rules), and two writers never share a file.
        if (!names.insert(core::to_utf8(path)).second || fs::exists(path, ec))
            throw core::Failure("output.clash", std::format("{}: {} is written twice", spec.command,
                                                            relative_utf8(path, run.dir)));
    }
    RunState* owner = &run;
    return core::OutputFile(path, [owner, id = spec.id](const fs::path& committed, const core::Sha256& sha) {
        owner->record_output(id, committed, sha);
    });
}

void StepState::close_journals() {
    std::vector<fs::path> closed;
    {
        std::lock_guard guard(mutex);
        if (journals_closed) return;
        journals_closed = true;
        // Closed, but kept until the step is destroyed: a module may still hold a Journal&, and an
        // append after the step's end is refused rather than undefined.
        for (auto& [key, journal] : journals) {
            journal->close();
            closed.push_back(journal->path());
        }
    }
    // A journal is complete once its step ends: it is recorded like a committed output (09.C6).
    for (const fs::path& path : closed) {
        std::error_code ec;
        if (fs::exists(path, ec)) run.record_output(spec.id, path, core::Sha256::of_file(path));
    }
}

fs::path StepFiles::input(const fs::path& as_named) {
    RunState& run = step_.run;
    const fs::path resolved = run.inputs->resolve(as_named);
    {
        std::lock_guard guard(run.inputs_mutex);
        if (auto copy = run.inputs->find(resolved)) return copy->copy;
    }
    if (is_inside(resolved, run.dir)) return resolved;   // an earlier step's output, read in place (09.C3)
    // Not in the plan: copied, hashed and recorded now, like an input the batch made (01.C12).
    std::vector<core::Diagnostic> problems;
    if (auto copy = run.take(step_.spec, {}, as_named, nullptr, false, problems)) return copy->copy;
    const core::Diagnostic& d = problems.front();
    throw core::Failure(d.code, d.message, core::Where{d.path.value_or(std::string()), 0, {}});
}

core::OutputFile StepFiles::create(std::string_view name) { return step_.open_output(step_.dir, name); }

core::OutputFile StepFiles::create_debug(std::string_view name) {
    // Debug outputs exist only under the debug option (S1.29, S4.19, S5.23): a module asks
    // ctx.log.debug_on() before it builds one.
    if (!step_.run.options.debug)
        throw core::Failure("run.debug_off", std::format("{}: the debug output {} needs the debug option (--debug)",
                                                         step_.spec.command, name));
    return step_.open_output(step_.debug_dir, name);
}

core::Journal& StepFiles::journal(std::string_view name) {
    const fs::path path = inside(step_.dir, name, "a journal");
    const std::string key = core::to_utf8(path);
    std::lock_guard guard(step_.mutex);
    if (step_.ended || step_.journals_closed)
        throw core::Failure("internal", "a journal was opened after its step ended: " + key);
    if (const auto it = step_.journals.find(key); it != step_.journals.end()) return *it->second;
    std::error_code ec;
    if (!step_.names.insert(key).second || fs::exists(path, ec))
        throw core::Failure("output.clash", std::format("{}: {} is written twice", step_.spec.command,
                                                        relative_utf8(path, step_.run.dir)));
    auto journal = std::make_unique<JournalFile>(path);
    JournalFile& ref = *journal;
    step_.journals.emplace(key, std::move(journal));
    return ref;
}

fs::path StepFiles::step_dir() const { return step_.dir; }

void StepEvents::emit(std::string_view type, core::Json fields) { step_.run.events->emit(type, fields, &step_.scope); }

void StepEvents::progress(std::uint64_t done, std::uint64_t total, std::string_view unit) {
    const auto now = SteadyClock::now();
    {
        std::lock_guard guard(step_.mutex);      // at most one a second per step (09.C10)
        if (step_.last_progress && now - *step_.last_progress < step_.run.options.timing.progress_every) return;
        step_.last_progress = now;
    }
    emit("step.progress", {{"done", done}, {"total", total}, {"unit", unit}});
}

void RunEvents::emit(std::string_view type, core::Json fields) {
    const std::optional<Scope> scope = run_.current_scope();
    run_.events->emit(type, fields, scope ? &*scope : nullptr);
}

void RunEvents::progress(std::uint64_t done, std::uint64_t total, std::string_view unit) {
    std::lock_guard guard(run_.scope_mutex);
    if (run_.current) run_.current->events.progress(done, total, unit);
    else run_.events->emit("step.progress", {{"done", done}, {"total", total}, {"unit", unit}}, nullptr);
}

// ---------------------------------------------------------------- the run

RunState::~RunState() {
    shutdown();
    try {
        std::lock_guard guard(mutex);
        if (!finished && dirty) write_manifest();
    } catch (...) {
        // A destructor reports nothing; run.json keeps its last whole version.
    }
}

void RunState::open() {
    started_at = std::chrono::system_clock::now();
    started_steady = SteadyClock::now();
    run_id = new_run_id(started_at);
    std::error_code ec;
    launch_dir = fs::current_path(ec);
    if (!options.batch.empty()) batch = normal(options.batch, launch_dir);
    base = batch.empty() ? launch_dir : batch.parent_path();
    if (!options.log_name.empty() &&
        (!plain_name(options.log_name) || reserved_top_level(options.log_name) ||
         options.log_name.find('.') == std::string::npos))
        throw core::Failure("run.log_name",
                            std::format("the run log's name '{}' must be a file name with an extension and no "
                                        "folder, such as egfr.log (S1.25)",
                                        options.log_name),
                            core::Where{{}, 0, "run.log"});

    if (options.adopt) {                         // the frontend made it and wrote run.batch there (A1)
        dir = normal(*options.adopt, launch_dir);
        fs::create_directories(dir, ec);
        if (ec || !fs::is_directory(dir))
            throw core::Failure("run.dir_create", "cannot use " + core::to_utf8(dir) + " as the run directory" +
                                                      (ec ? ": " + ec.message() : std::string()));
    } else {
        dir = claim_run_dir(options.root.empty() ? base : normal(options.root, launch_dir), options.label, started_at);
    }
    name = core::to_utf8(dir.filename());
    lock.emplace(Lock::claim(dir, std::format("{} {}", process_id(), run_id)));
    log_name = options.log_name.empty() ? name + ".log" : options.log_name;
    inputs.emplace(dir, base);

    events = std::make_unique<EventLog>(dir / "events.jsonl");
    for (const auto& sink : options.sinks)
        if (sink) log.add_sink(sink);
    log.add_sink(std::make_shared<RunLogSink>(dir / core::path_from_utf8(log_name)));
    log.add_sink(std::make_shared<LogEvents>(run_events));

    {
        std::lock_guard guard(mutex);
        doc = skeleton();
        if (!write_manifest())                   // pid and started are in the first version (section 10)
            throw core::Failure("io.write", "cannot write " + core::to_utf8(dir / "run.json"));
    }
    const std::string build(core::build_id());
    events->emit("run.started",
                 {{"run", run_id}, {"name", name}, {"dir", core::to_utf8(dir)}, {"pid", process_id()},
                  {"fusion", build}, {"parent", nullptr}},
                 nullptr);
    log.info(std::format("FuSION {} (build {}), run {}", version_of(build), build, run_id));
    log.info("Run directory: " + core::to_utf8(dir));
    if (!lock->locked())
        log.warning("run.lock_unsupported",
                    "the run root's filesystem cannot lock run.lock, so others cannot see that this run is alive");
    watcher = std::thread([this] { watch(); });
}

core::Json RunState::skeleton() const {
    core::Json d = core::Json::object();
    d["format"] = "fusion.run";
    d["format_version"] = 1;
    d["run"] = run_id;
    d["name"] = name;
    d["status"] = "running";
    d["exit"] = nullptr;
    d["parent"] = nullptr;
    d["point"] = nullptr;
    d["pid"] = process_id();
    d["labels"] = core::Json::object();
    for (const auto& [key, value] : options.labels) d["labels"][key] = value;
    d["started"] = core::utc_timestamp(started_at);
    d["finished"] = nullptr;
    d["resources"] = nullptr;
    d["build_id"] = std::string(core::build_id());
    for (const char* block : {"versions", "tools", "platform", "install", "environment"}) d[block] = nullptr;   // 11's
    d["command_line"] = core::Json::array();
    for (const std::string& arg : options.command_line) d["command_line"].push_back(arg);
    d["launch_dir"] = core::to_utf8(launch_dir);
    d["batch"] = nullptr;
    d["variables"] = core::Json::array();
    for (const auto& [variable, value] : options.variables)
        d["variables"].push_back({{"name", variable}, {"value", value}});
    d["run_settings"] = run_settings_json();
    d["inputs"] = core::Json::array();
    d["steps"] = core::Json::array();
    d["outputs"] = core::Json::array();          // the run-level files: bindings.tsv, index.html
    d["check"] = nullptr;
    d["reports"] = core::Json::array();
    d["error"] = nullptr;
    d["stop"] = nullptr;
    return d;
}

core::Json RunState::run_settings_json() const {
    // Every run-level setting with its value and the source that applied (E1; section 01 resolves
    // them). A computed default records the value it took (run.log).
    std::vector<core::Setting> list = options.settings;
    const auto spec_named = [](std::string_view setting) -> const core::SettingSpec* {
        for (const core::SettingSpec& spec : run::settings())
            if (spec.name == setting) return &spec;
        return nullptr;
    };
    if (list.empty()) {
        const auto add = [&](std::string_view setting, core::Value value, bool is_default) {
            core::Setting s;
            s.spec = spec_named(setting);
            s.value = std::move(value);
            s.source = is_default ? core::Source::default_value : core::Source::set_option;
            list.push_back(std::move(s));
        };
        add("cache", options.cache, options.cache);
        add("debug", options.debug, !options.debug);
        add("log", log_name, options.log_name.empty());
    }
    std::erase_if(list, [](const core::Setting& s) { return s.spec == nullptr; });
    for (core::Setting& s : list)
        if (core::key(*s.spec) == "run.log" && !s.value) s.value = log_name;
    return core::to_json(core::Settings(std::move(list)));
}

bool RunState::write_manifest() {
    // Through OutputFile, so a reader always sees a whole run.json (E1, 1.28).
    try {
        core::OutputFile out(dir / "run.json");
        out.out() << doc.dump(2, ' ', false, core::Json::error_handler_t::replace) << '\n';
        out.commit();
    } catch (const core::Failure& failure) {
        if (!write_warned) {
            write_warned = true;
            log.warning(failure.code(), failure.what());
        }
        return false;
    }
    dirty = false;
    last_write = SteadyClock::now();
    return true;
}

void RunState::touch(bool now) {
    // At most once a second between steps; the watcher writes the last change within a second (E1).
    dirty = true;
    if (finished) return;
    if (now || SteadyClock::now() - last_write >= options.timing.manifest_every) write_manifest();
    else watch_cv.notify_all();
}

void RunState::register_step(const StepSpec& spec) {
    if (step_index.contains(spec.id)) return;
    core::Json record = core::Json::object();
    record["id"] = spec.id;
    record["cmd"] = spec.command;
    record["line"] = spec.line;
    if (!spec.normalised.empty()) record["normalised"] = spec.normalised;
    if (!spec.point.empty()) record["point"] = spec.point;
    record["status"] = "pending";                // every planned step is listed from the start
    record["started"] = nullptr;
    record["wall_s"] = nullptr;
    record["settings"] = spec.settings ? core::to_json(*spec.settings) : core::Json::array();
    record["outputs"] = core::Json::array();
    step_index.emplace(spec.id, doc["steps"].size());
    doc["steps"].push_back(std::move(record));
}

void RunState::record_input(const StepSpec& spec, std::string_view argument, const InputCopy& copy,
                            std::string_view member_of) {
    core::Json entry = core::Json::object();
    entry["step"] = spec.id;
    entry["argument"] = argument;
    if (!member_of.empty()) entry["member_of"] = member_of;
    entry["path"] = core::to_utf8(copy.original);
    entry["size"] = copy.size;
    entry["sha256"] = copy.sha256.hex();
    entry["copy"] = copy.copy_rel;
    for (const core::Json& known : doc["inputs"])
        if (known == entry) return;
    doc["inputs"].push_back(std::move(entry));
    touch(false);
}

void RunState::record_output(const std::string& step_id, const fs::path& path, const core::Sha256& sha) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    const fs::path file = normal(path, dir);
    const std::string hex = sha.hex();
    std::lock_guard guard(mutex);
    if (finished) return;
    core::Json& outputs = step_id.empty() ? doc["outputs"] : step_record(step_id)["outputs"];
    if (!is_inside(file, dir)) {
        // The copy at an explicit absolute path follows its file's own commit (2B.3): "also".
        for (auto it = outputs.rbegin(); it != outputs.rend(); ++it)
            if ((*it)["sha256"] == hex && !it->contains("also")) {
                (*it)["also"] = core::to_utf8(file);
                touch(false);
                return;
            }
    }
    core::Json entry = core::Json::object();
    entry["path"] = is_inside(file, dir) ? relative_utf8(file, dir) : core::to_utf8(file);
    entry["size"] = ec ? 0 : size;
    entry["sha256"] = hex;
    outputs.push_back(std::move(entry));
    touch(false);
}

void RunState::record_batch() {
    if (batch.empty()) return;
    {
        std::lock_guard guard(mutex);
        if (batch_recorded) return;
        batch_recorded = true;
    }
    std::optional<Problem> problem;
    std::optional<InputCopy> copy;
    {
        std::lock_guard guard(inputs_mutex);
        copy = inputs->take(batch, "inputs", false, problem);
    }
    std::lock_guard guard(mutex);
    if (copy) {
        batch_sha256 = copy->sha256;
        doc["batch"] = {{"path", core::to_utf8(copy->original)}, {"size", copy->size},
                        {"sha256", copy->sha256.hex()}, {"copy", copy->copy_rel}};
    } else {
        StepSpec none;
        pending.push_back(diagnostic(core::Severity::error, none, {}, problem->code,
                                     "the batch file: " + problem->message, batch));
    }
    touch(false);
}

std::optional<InputCopy> RunState::take(const StepSpec& spec, std::string_view argument, const fs::path& as_named,
                                        const InputSpec* input, bool fresh, std::vector<core::Diagnostic>& problems) {
    const fs::path resolved = inputs->resolve(as_named);
    std::optional<Problem> problem;
    std::optional<InputCopy> copy;
    {
        std::lock_guard guard(inputs_mutex);
        copy = inputs->take(resolved, "inputs", fresh, problem);
    }
    if (!copy) {
        const std::string what = argument.empty() ? problem->message : std::string(argument) + ": " + problem->message;
        problems.push_back(diagnostic(core::Severity::error, spec, argument, problem->code, what, resolved));
        return std::nullopt;
    }
    {
        std::lock_guard guard(mutex);
        record_input(spec, argument, *copy, {});
    }
    if (!input || !input->is_list || !input->members) return copy;
    // A list's members, as the owning module reads them from the list's copy (02.C12): each is copied
    // to inputs/<list stem>/ and recorded with the list it came from. The resolver runs unlocked.
    std::vector<fs::path> members;
    try {
        members = input->members(copy->copy);
    } catch (const core::Failure& failure) {
        problems.push_back(diagnostic(core::Severity::error, spec, argument, failure.code(), failure.what(), resolved));
    }
    const fs::path folder = fs::path("inputs") / core::path_from_utf8(copy->copy_rel).stem();
    for (const fs::path& member : members) {
        const fs::path member_path = normal(member, resolved.parent_path());
        std::optional<InputCopy> member_copy;
        {
            std::lock_guard guard(inputs_mutex);
            member_copy = inputs->take(member_path, folder, false, problem);
        }
        if (!member_copy) {
            problems.push_back(diagnostic(core::Severity::error, spec, argument, "input.list_member",
                                          member_message(argument, resolved, *problem), member_path));
            continue;
        }
        std::lock_guard guard(mutex);
        record_input(spec, argument, *member_copy, copy->copy_rel);
    }
    return copy;
}

void RunState::set_current(StepState* step) {
    std::lock_guard guard(scope_mutex);
    current = step;
}

void RunState::clear_current(StepState* step) {
    std::lock_guard guard(scope_mutex);
    if (current == step) current = nullptr;
}

std::optional<Scope> RunState::current_scope() {
    std::lock_guard guard(scope_mutex);
    if (!current) return std::nullopt;
    return current->scope;
}

std::optional<int> RunState::finalize(RunStatus status, bool forced) {
    if (status == RunStatus::running) status = RunStatus::failed;
    const int code = exit_status(status);
    const std::string because = std::format("the run ended {}", status_name(status));
    std::vector<Scope> skipped;
    double wall = 0, cpu = 0;
    std::uint64_t rss = 0;
    {
        std::lock_guard guard(mutex);
        if (finished) return std::nullopt;
        // Every planned step ends with a status: one still running takes the run's; the rest are skipped.
        for (core::Json& step : doc["steps"]) {
            const std::string state = step["status"].get<std::string>();
            if (state == "running") {
                step["status"] = status == RunStatus::stopped ? "stopped" : "failed";
            } else if (state == "pending") {
                step["status"] = "skipped";
                step["because"] = because;
                skipped.push_back(Scope{step["id"].get<std::string>(), step["line"].get<std::uint32_t>(),
                                        step.value("point", std::string())});
            }
        }
        wall = whole_ms(seconds_since(started_steady));
        cpu = whole_ms(core::cpu_seconds());
        rss = core::peak_rss_bytes();
        doc["status"] = status_name(status);
        doc["exit"] = code;
        doc["finished"] = core::utc_timestamp(std::chrono::system_clock::now());
        doc["resources"] = {{"wall_s", wall}, {"cpu_s", cpu}, {"peak_rss_bytes", rss}};
        if (forced && doc["stop"].is_object()) doc["stop"]["forced"] = true;
        exit = code;
        write_manifest();                        // run.json is final before run.finished is emitted
        finished = true;
    }
    for (const Scope& scope : skipped) events->emit("step.skipped", {{"because", because}}, &scope);
    log.info(std::format("Finished: {}, exit {}, {} s wall, {} s CPU, peak RSS {} bytes", status_name(status), code,
                         core::format_number(wall), core::format_number(cpu), rss));
    events->emit("run.finished", {{"status", status_name(status)}, {"exit", code}, {"wall_s", wall}, {"forced", forced}},
                 nullptr);
    events->close();                             // run.finished is the last line (09.C10)
    return code;
}

void RunState::shutdown() noexcept {
    {
        std::lock_guard guard(watch_mutex);
        shutting_down = true;
    }
    watch_cv.notify_all();
    if (!watcher.joinable()) return;
    if (watcher.get_id() == std::this_thread::get_id()) watcher.detach();   // the grace hook ended the run
    else watcher.join();
}

// ---------------------------------------------------------------- the stop watcher (09.C7, 09.C8)

void RunState::watch() {
    auto next_poll = SteadyClock::now();
    std::unique_lock wait_lock(watch_mutex);
    while (!shutting_down) {
        wait_lock.unlock();
        auto now = SteadyClock::now();
        if (now >= next_poll) {
            poll_stop();                         // one stat every 0.5 s (09.C7)
            next_poll = now + options.timing.stop_poll;
        }
        auto wake = next_poll;
        bool expired = false;
        now = SteadyClock::now();
        {
            std::lock_guard guard(mutex);
            if (!finished) {
                if (dirty) {
                    const auto due = last_write + options.timing.manifest_every;
                    if (now >= due) write_manifest();
                    else wake = std::min(wake, due);
                }
                if (stop_seen) {
                    const auto grace_end = stop_at + options.timing.grace;
                    if (now >= grace_end) expired = true;
                    else wake = std::min(wake, grace_end);
                }
            }
        }
        if (expired) {
            force();
            return;
        }
        wait_lock.lock();
        watch_cv.wait_until(wait_lock, wake, [this] { return shutting_down; });
    }
}

void RunState::poll_stop() {
    if (stop_seen) return;
    std::error_code ec;
    // Only the stop file's existence matters; anyone may make it (09.C7). The executable's handlers
    // for Ctrl-C and SIGTERM set the flag read here (09.C8).
    if (fs::exists(dir / "stop", ec)) request("file");
    else if (take_signal_request()) request("signal");
}

void RunState::request(const std::string& source) {
    stop_at = SteadyClock::now();
    stop_seen = true;
    stop_source.request_stop();                  // the callbacks run here, once: the solve's interrupt
    const std::string note = source == "file" ? read_note(dir / "stop") : std::string();
    events->emit("stop.requested", {{"source", source}}, nullptr);
    log.info(source == "file" ? "Stop requested by the stop file; stopping at the next safe point"
                              : "Stop requested by a signal; stopping at the next safe point");
    std::lock_guard guard(mutex);
    doc["stop"] = {{"source", source}, {"time", core::utc_timestamp(std::chrono::system_clock::now())},
                   {"note", note}, {"forced", false}};
    touch(true);
}

void RunState::force() {
    {
        std::lock_guard guard(mutex);
        if (finished) return;
    }
    // A step ignored its stop token, for example inside a Z3 tactic that ignores the interrupt. The
    // journal already holds every finished check (09.C6), so ending the process loses nothing.
    log.warning("stop.forced", std::format("the run did not end within {} s of the stop request",
                                           core::format_number(std::chrono::duration<double>(options.timing.grace).count())));
    const std::optional<int> code = finalize(RunStatus::stopped, true);
    if (code && options.on_grace_expired) options.on_grace_expired(*code);   // the executable's std::_Exit(3)
}

}  // namespace detail

// ---------------------------------------------------------------- Run

struct Run::Impl : detail::RunState {
    using RunState::RunState;
};

struct Run::Step::Impl : detail::StepState {
    using StepState::StepState;
};

Run::Run(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Run::Run(Run&&) noexcept = default;
Run& Run::operator=(Run&&) noexcept = default;
Run::~Run() = default;

Run Run::start(const Options& options) {
    if (options.parent)
        throw core::Failure("run.resume_unavailable",
                            "restarting or resuming from a parent run (09.C5) comes in a later version of FuSION 2.0");
    auto impl = std::make_unique<Impl>(options);
    impl->open();
    return Run(std::move(impl));
}

const std::filesystem::path& Run::dir() const noexcept { return impl_->dir; }
const std::string& Run::id() const noexcept { return impl_->run_id; }
core::Log& Run::log() noexcept { return impl_->log; }
core::Events& Run::events() noexcept { return impl_->run_events; }
core::StopToken Run::stop() const noexcept { return impl_->stop_source.token(); }

void Run::copy_inputs(std::span<const StepSpec> steps) {
    detail::RunState& run = *impl_;
    std::vector<const StepSpec*> todo;
    {
        std::lock_guard guard(run.mutex);
        for (const StepSpec& step : steps) {
            run.register_step(step);
            if (run.copied_steps.insert(step.id).second) todo.push_back(&step);
        }
        run.touch(false);
    }
    run.record_batch();
    std::vector<core::Diagnostic> problems;
    for (const StepSpec* step : todo)
        for (const InputSpec& input : step->inputs) {
            if (input.made_by_line) continue;    // checked, copied and hashed just before its step (01.C12)
            if (input.path.empty()) {
                if (input.required)
                    problems.push_back(detail::diagnostic(core::Severity::error, *step, input.argument,
                                                          "input.missing",
                                                          "the required input " + input.argument + " is not given",
                                                          {}));
                continue;                        // an optional input left out turns its feature off (0021)
            }
            run.take(*step, input.argument, input.path, &input, false, problems);
        }
    std::lock_guard guard(run.mutex);
    run.pending.insert(run.pending.end(), problems.begin(), problems.end());
}

core::CheckReport Run::check(std::span<const StepSpec> steps) {
    detail::RunState& run = *impl_;
    copy_inputs(steps);                          // whatever was not copied yet (1.13: copy first)
    core::CheckReport report;
    report.batch = run.batch;
    {
        std::lock_guard guard(run.mutex);
        report.diagnostics = std::move(run.pending);
        run.pending.clear();
        if (run.batch_sha256) report.batch_sha256 = *run.batch_sha256;
    }
    detail::check_outputs(steps, run.base, report.diagnostics);
    std::error_code ec;
    if (const auto space = std::filesystem::space(run.dir, ec); !ec && space.available < (std::uint64_t{1} << 30)) {
        core::Diagnostic low;
        low.severity = core::Severity::warning;
        low.code = "run.low_disk";
        low.message = std::format("only {} bytes are free on the run directory's volume", space.available);
        low.path = core::to_utf8(run.dir);
        report.diagnostics.push_back(std::move(low));
    }
    detail::sort_diagnostics(report.diagnostics);

    std::size_t errors = 0, warnings = 0;
    for (const core::Diagnostic& d : report.diagnostics) {
        const std::string text = detail::batch_prefix(run.batch, d.line) +
                                 (d.command ? *d.command + ": " : std::string()) + d.message;
        if (d.severity == core::Severity::error) {
            ++errors;
            run.log.error(d.code, text);
        } else if (d.severity == core::Severity::warning) {
            ++warnings;
            run.log.warning(d.code, text);
        }
    }
    std::size_t count = 0;
    std::uint64_t bytes = 0;
    {
        std::lock_guard guard(run.inputs_mutex);
        count = run.inputs->count();
        bytes = run.inputs->bytes();
    }
    {
        std::lock_guard guard(run.mutex);
        run.doc["check"] = report.to_json();
        run.touch(errors > 0);                   // a failed check is written at once
    }
    run.events->emit("inputs.checked", {{"inputs", count}, {"bytes", bytes}, {"errors", errors}, {"warnings", warnings}},
                     nullptr);
    run.log.info(std::format("Checked {} inputs ({} bytes) and copied them to inputs/", count, bytes));
    return report;
}

Run::Step Run::begin(const StepSpec& spec) {
    detail::RunState& run = *impl_;
    if (!detail::plain_name(spec.id) || detail::reserved_top_level(spec.id))
        throw core::Failure("internal", "'" + spec.id + "' cannot name a step directory (09.C2)");
    auto state = std::make_unique<Step::Impl>(run, spec);
    {
        std::lock_guard guard(run.mutex);
        if (run.finished) throw core::Failure("internal", "step " + spec.id + " began after the run finished");
        run.register_step(spec);
        core::Json& record = run.step_record(spec.id);
        if (record["status"] != "pending") throw core::Failure("internal", "step " + spec.id + " began twice");
        record["status"] = "running";
        record["started"] = core::utc_timestamp(std::chrono::system_clock::now());
        run.touch(false);
    }
    run.set_current(state.get());
    run.events->emit("step.started", {{"cmd", spec.command}}, &state->scope);
    Step step(std::move(state));

    // An input that an earlier line made (01.C12) is checked, copied and hashed now; one inside the run
    // directory, an earlier step's output, is hashed where it lies.
    std::vector<core::Diagnostic> problems;
    for (const InputSpec& input : spec.inputs) {
        if (!input.made_by_line || input.path.empty()) continue;
        if (!run.take(spec, input.argument, input.path, &input, true, problems)) {
            const core::Diagnostic& d = problems.front();
            const core::Failure failure(d.code, std::format("{} (it should have been made by line {})", d.message,
                                                            *input.made_by_line),
                                        core::Where{d.path.value_or(std::string()), 0, input.argument});
            fail(step, failure);
            throw failure;
        }
    }
    return step;
}

bool Run::load_from_parent(Step&) {
    return false;                                // 09.C5 comes later; start() refuses a parent until then
}

void Run::end(Step& step, StepStatus status, std::optional<core::Report> report, bool partial) {
    detail::RunState& run = *impl_;
    detail::StepState& s = *step.impl_;
    if (report) status = StepStatus::reported;
    if (status == StepStatus::failed) {
        fail(step, core::Failure("internal", "the step failed"));
        return;
    }
    if (status == StepStatus::pending || status == StepStatus::running)
        throw core::Failure("internal", "Run::end needs the step's final status");
    if (s.ended.exchange(true)) return;
    s.close_journals();
    const double wall = detail::whole_ms(detail::seconds_since(s.started));
    // The owner's message is printed with the batch line (01, "Running a point").
    if (report) run.log.info(detail::batch_prefix(run.batch, s.spec.line) + "result: " + report->message);
    core::Json outputs = core::Json::array();
    {
        std::lock_guard guard(run.mutex);
        core::Json& record = run.step_record(s.spec.id);
        record["status"] = detail::status_name(status);
        record["wall_s"] = wall;
        if (partial) record["partial"] = true;
        if (s.spec.settings) record["settings"] = core::to_json(*s.spec.settings);   // with effective values
        if (report) {
            record["report"] = {{"code", report->code}, {"message", report->message}};
            run.doc["reports"].push_back(
                {{"code", report->code}, {"message", report->message}, {"step", s.spec.id}, {"line", s.spec.line}});
        }
        for (const core::Json& output : record["outputs"]) outputs.push_back(output["path"]);
        run.touch(false);
    }
    core::Json fields = {{"status", detail::status_name(status)}, {"wall_s", wall}, {"outputs", outputs}};
    if (report) fields["report"] = {{"code", report->code}, {"message", report->message}};
    if (partial) fields["partial"] = true;
    run.events->emit("step.finished", fields, &s.scope);
    run.clear_current(&s);
}

void Run::fail(Step& step, const core::Failure& failure) {
    detail::RunState& run = *impl_;
    detail::StepState& s = *step.impl_;
    if (s.ended.exchange(true)) return;
    s.close_journals();
    const double wall = detail::whole_ms(detail::seconds_since(s.started));
    // stderr through the console sink, the run log, and an "error" event in the step's scope (09.C14):
    // "egfr.batch:9: mff: cannot read ... [input.unreadable]".
    run.log.error(failure.code(), detail::batch_prefix(run.batch, s.spec.line) + s.spec.command + ": " + failure.what());
    core::Json outputs = core::Json::array();
    {
        std::lock_guard guard(run.mutex);
        core::Json& record = run.step_record(s.spec.id);
        record["status"] = "failed";
        record["wall_s"] = wall;
        if (s.spec.settings) record["settings"] = core::to_json(*s.spec.settings);
        record["error"] = {{"code", failure.code()}, {"message", failure.what()}};
        for (const core::Json& output : record["outputs"]) outputs.push_back(output["path"]);
        if (run.doc["error"].is_null()) {        // the batch stops at its first failure (S1.28)
            core::Json error = {{"code", failure.code()}, {"message", failure.what()}, {"step", s.spec.id},
                                {"line", s.spec.line}};
            const core::Where& where = failure.where();
            if (!where.file.empty()) error["file"] = where.file;
            if (where.line) error["file_line"] = where.line;
            if (!where.setting.empty()) error["setting"] = where.setting;
            run.doc["error"] = std::move(error);
        }
        run.touch(true);                         // a failure is written at once
    }
    run.events->emit("step.finished",
                     {{"status", "failed"}, {"wall_s", wall}, {"outputs", outputs},
                      {"error", {{"code", failure.code()}, {"message", failure.what()}}}},
                     &s.scope);
    run.clear_current(&s);
}

void Run::skip(const StepSpec& spec, std::string_view because) {
    detail::RunState& run = *impl_;
    const detail::Scope scope{spec.id, spec.line, spec.point};
    {
        std::lock_guard guard(run.mutex);
        if (run.finished) return;
        run.register_step(spec);
        core::Json& record = run.step_record(spec.id);
        if (record["status"] != "pending") return;
        record["status"] = "skipped";
        record["because"] = because;
        run.touch(false);
    }
    run.events->emit("step.skipped", {{"because", because}}, &scope);
}

core::OutputFile Run::create_run_file(std::string_view name) {
    detail::RunState& run = *impl_;
    if (!detail::plain_name(name) || detail::reserved_top_level(name) || name == run.log_name)
        throw core::Failure("internal", "'" + std::string(name) + "' cannot name a run file (09.C2)");
    const std::filesystem::path path = run.dir / core::path_from_utf8(name);
    {
        std::lock_guard guard(run.mutex);
        if (run.finished) throw core::Failure("internal", "run files are written before Run::finish: " + std::string(name));
        std::error_code ec;
        if (!run.run_files.insert(std::string(name)).second || std::filesystem::exists(path, ec))
            throw core::Failure("output.clash", std::string(name) + " is written once per run");
    }
    detail::RunState* owner = &run;
    return core::OutputFile(path, [owner](const std::filesystem::path& committed, const core::Sha256& sha) {
        owner->record_output({}, committed, sha);
    });
}

int Run::finish(RunStatus status) {
    const std::optional<int> code = impl_->finalize(status, false);
    impl_->shutdown();
    if (code) return *code;
    std::lock_guard guard(impl_->mutex);
    return impl_->exit;                          // the watcher's forced end came first
}

void Run::set(std::string_view key, core::Json value) {
    detail::RunState& run = *impl_;
    for (std::string_view own : {"format", "format_version", "run", "name", "status", "exit", "parent", "pid", "labels",
                                 "started", "finished", "resources", "build_id", "command_line", "launch_dir", "batch",
                                 "variables", "run_settings", "inputs", "steps", "outputs", "check", "reports",
                                 "error", "stop"})
        if (key == own) throw core::Failure("internal", "run.json's " + std::string(key) + " is run's own");
    std::lock_guard guard(run.mutex);
    run.doc[std::string(key)] = std::move(value);
    run.touch(false);
}

// ---------------------------------------------------------------- Run::Step

Run::Step::Step(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Run::Step::Step(Step&&) noexcept = default;
Run::Step& Run::Step::operator=(Step&&) noexcept = default;
Run::Step::~Step() = default;

const StepSpec& Run::Step::spec() const noexcept { return impl_->spec; }
const std::filesystem::path& Run::Step::dir() const noexcept { return impl_->dir; }
core::Context& Run::Step::context() noexcept { return impl_->context; }

void Run::Step::detail(std::string_view key, core::Json value) {
    detail::RunState& run = impl_->run;
    std::lock_guard guard(run.mutex);
    core::Json& record = run.step_record(impl_->spec.id);
    if (!record.contains("details")) record["details"] = core::Json::object();
    record["details"][std::string(key)] = std::move(value);
    run.touch(false);
}

void Run::Step::artefact(const std::filesystem::path& file, std::string_view kind, std::string_view format,
                         std::uint32_t format_version, std::string_view part) {
    detail::RunState& run = impl_->run;
    const std::string rel = detail::relative_utf8(detail::normal(file, impl_->dir), run.dir);
    std::lock_guard guard(run.mutex);
    for (core::Json& output : run.step_record(impl_->spec.id)["outputs"]) {
        if (output["path"] != rel) continue;
        core::Json described = core::Json::object();   // E1's order: kind, part, format, version, then the file
        described["kind"] = kind;
        if (!part.empty()) described["part"] = part;
        described["format"] = format;
        described["format_version"] = format_version;
        for (const auto& [k, v] : output.items())
            if (!described.contains(k)) described[k] = v;
        output = std::move(described);
        run.touch(false);
        return;
    }
    throw core::Failure("internal", "step " + impl_->spec.id + " has committed no output " + rel);
}

}  // namespace fusion::run
