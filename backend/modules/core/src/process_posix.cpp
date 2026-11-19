// fusion core: the process layer on Linux and macOS (section 11's platform table; 11.C19,
// cross-section item 2A.11).
//
// FuSION 1 ran exec lines and its helpers through system(), so every argument passed through
// /bin/sh -c and its quoting, the status was discarded, and nothing could stop a child
// (Linux/src/main.cc:1318-1330 for exec, 5728-5760 for pex; spec S1.14, S6.10, S7.02). Here a
// child gets an argument vector, an environment and a working directory without a shell, its
// stdin from /dev/null or a given text, its stdout and stderr captured or in one file, a timeout,
// and its own process group, so a stop ends it and everything it started.
//
// The child is made with fork, and does only async-signal-safe calls (setpgid, chdir, dup2,
// sigprocmask, sigaction, execve) before execve: glibc's posix_spawn gained its working-directory
// action only in 2.29, above section 11's glibc 2.28 floor. A pipe closed on exec reports a
// failed chdir or execve back to the parent.
#if !defined(_WIN32)

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "fusion/core/core.hpp"
#include "fusion/core/process.hpp"

extern char** environ;

namespace fusion::core {

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

class Fd {                                       // an owned file descriptor
public:
    Fd() = default;
    explicit Fd(int fd) : fd_(fd) {}
    Fd(Fd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    ~Fd() { reset(); }
    int get() const noexcept { return fd_; }
    explicit operator bool() const noexcept { return fd_ >= 0; }
    void reset() noexcept {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

private:
    int fd_ = -1;
};

std::string errno_text(int error) { return std::strerror(error); }

// Keeps a descriptor clear of 0, 1 and 2, so the child's dup2 calls never overwrite one another.
bool lift_above_stdio(Fd& fd) {
    if (fd.get() > 2) return true;
    const int moved = ::fcntl(fd.get(), F_DUPFD_CLOEXEC, 3);
    if (moved < 0) return false;
    fd = Fd(moved);
    return true;
}

bool make_pipe(Fd& read_end, Fd& write_end) {
    int fds[2];
#if defined(__linux__)
    if (::pipe2(fds, O_CLOEXEC) != 0) return false;
#else
    if (::pipe(fds) != 0) return false;
    ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);
#endif
    read_end = Fd(fds[0]);
    write_end = Fd(fds[1]);
    return lift_above_stdio(read_end) && lift_above_stdio(write_end);
}

bool write_all(int fd, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

// The text for the child's stdin goes into an unlinked temporary file, so the parent never blocks
// writing a pipe the child does not read, and never meets SIGPIPE.
Fd stdin_from_text(const std::string& text, std::string& error) {
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec);
    std::string pattern = (ec ? std::string("/tmp") : dir.string()) + "/fusion-stdin-XXXXXX";
    Fd fd(::mkostemp(pattern.data(), O_CLOEXEC));
    if (!fd) {
        error = "cannot create a temporary file for its input: " + errno_text(errno);
        return {};
    }
    ::unlink(pattern.c_str());
    if (!write_all(fd.get(), text.data(), text.size()) || ::lseek(fd.get(), 0, SEEK_SET) != 0) {
        error = "cannot write its input: " + errno_text(errno);
        return {};
    }
    return fd;
}

std::vector<std::string> merged_environment(const std::vector<std::pair<std::string, std::string>>& env_set) {
    std::vector<std::string> merged;
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) merged.emplace_back(*entry);
    for (const auto& [name, value] : env_set) {
        const std::string prefix = name + "=";
        const auto it = std::find_if(merged.begin(), merged.end(),
                                     [&](const std::string& e) { return e.starts_with(prefix); });
        if (it != merged.end())
            *it = prefix + value;
        else
            merged.push_back(prefix + value);
    }
    return merged;
}

std::vector<char*> pointers(std::vector<std::string>& strings) {
    std::vector<char*> out;
    out.reserve(strings.size() + 1);
    for (std::string& s : strings) out.push_back(s.data());
    out.push_back(nullptr);
    return out;
}

enum class Stage : int { setup = 1, chdir = 2, exec = 3 };

// After fork, in the child: async-signal-safe calls only. Never returns.
[[noreturn]] void run_child(bool own_group, const char* working_dir, int in_fd, int out_fd, int err_fd,
                            int report_fd, const char* program, char* const* argv, char* const* envp) {
    const auto fail = [report_fd](Stage stage) {
        const std::array<int, 2> report = {static_cast<int>(stage), errno};
        [[maybe_unused]] const ssize_t ignored = ::write(report_fd, report.data(), sizeof(report));
        ::_exit(127);
    };
    if (own_group && ::setpgid(0, 0) != 0) fail(Stage::setup);
    sigset_t none;
    ::sigemptyset(&none);
    ::sigprocmask(SIG_SETMASK, &none, nullptr);  // the forking thread's mask is not the child's
    struct sigaction default_action {};
    default_action.sa_handler = SIG_DFL;
    ::sigaction(SIGPIPE, &default_action, nullptr);   // tools in a pipeline expect SIGPIPE
    if (working_dir != nullptr && ::chdir(working_dir) != 0) fail(Stage::chdir);
    if (::dup2(in_fd, 0) < 0 || ::dup2(out_fd, 1) < 0 || ::dup2(err_fd, 2) < 0) fail(Stage::setup);
    ::execve(program, argv, envp);
    fail(Stage::exec);
    ::_exit(127);
}

void read_available(Fd& fd, std::string& into) {
    std::array<char, 1 << 16> buffer{};
    while (fd) {
        const ssize_t n = ::read(fd.get(), buffer.data(), buffer.size());
        if (n > 0) {
            into.append(buffer.data(), static_cast<std::size_t>(n));
        } else if (n == 0) {
            fd.reset();                          // end of file
        } else if (errno != EINTR) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) fd.reset();
            return;
        }
    }
}

}  // namespace

struct Process::Impl {
    pid_t pid = -1;                              // -1: never started, or reaped
    bool own_group = false;
    std::chrono::milliseconds timeout{0};
    Clock::time_point start{};
    Clock::time_point end{};
    Fd out_read, err_read;                       // Output::capture
    std::string out_text, err_text;
    std::string error;                           // why it could not start
    std::string wait_error;                      // why its status is unknown
    std::mutex mutex;                            // reaping and signalling, so stop() is thread-safe
    bool reaped = false;
    bool timed_out = false;
    int status = 0;
    rusage usage{};
    std::optional<ProcessResult> result;

    // Sends SIGKILL to the child, or its whole group, unless it has been reaped (its pid could
    // then name another process).
    void kill_locked() {
        if (pid > 0 && !reaped) ::kill(own_group ? -pid : pid, SIGKILL);
    }
    bool try_reap() {
        std::lock_guard lock(mutex);
        if (reaped || pid <= 0) return true;
        int raw = 0;
        pid_t got;
        do {
            got = ::wait4(pid, &raw, WNOHANG, &usage);
        } while (got < 0 && errno == EINTR);
        if (got == 0) return false;
        if (got < 0) {                           // e.g. ECHILD where SIGCHLD is ignored: status unknown
            wait_error = "cannot wait for it: " + errno_text(errno);
            raw = 0;
        }
        status = raw;
        reaped = true;
        end = Clock::now();
        return true;
    }
    void check_timeout() {
        if (timeout.count() <= 0 || Clock::now() - start < timeout) return;
        std::lock_guard lock(mutex);
        if (reaped || timed_out) return;
        timed_out = true;
        kill_locked();
    }
    void drain(int wait_ms) {
        std::array<pollfd, 2> fds{};
        nfds_t count = 0;
        if (out_read) fds[count++] = pollfd{out_read.get(), POLLIN, 0};
        if (err_read) fds[count++] = pollfd{err_read.get(), POLLIN, 0};
        if (count == 0) {
            if (wait_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            return;
        }
        if (::poll(fds.data(), count, wait_ms) <= 0) return;
        read_available(out_read, out_text);
        read_available(err_read, err_text);
    }
    int exit_code() const {
        if (!error.empty() || !wait_error.empty() || timed_out) return -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;   // killed by a signal: -1
    }
};

Process::Process(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Process::Process(Process&&) noexcept = default;
Process& Process::operator=(Process&&) noexcept = default;

Process::~Process() {
    if (!impl_ || impl_->pid <= 0 || impl_->reaped) return;
    {
        std::lock_guard lock(impl_->mutex);
        impl_->kill_locked();
    }
    int raw = 0;
    while (::waitpid(impl_->pid, &raw, 0) < 0 && errno == EINTR) {
    }
}

Process Process::spawn(const ProcessSpec& spec) {
    auto impl = std::make_unique<Impl>();
    impl->own_group = spec.own_group;
    impl->timeout = spec.timeout;
    impl->start = Clock::now();
    impl->end = impl->start;
    const auto failed = [&](std::string why) {
        impl->error = "cannot start " + to_utf8(spec.program) + ": " + std::move(why);
        return Process(std::move(impl));
    };

    // Everything the child needs is prepared here: after fork it may not allocate.
    std::vector<std::string> argv_strings;
    argv_strings.reserve(spec.args.size() + 1);
    std::error_code ec;
    const fs::path program = fs::absolute(spec.program, ec);
    argv_strings.push_back(program.string());
    for (const std::string& a : spec.args) argv_strings.push_back(a);
    std::vector<std::string> env_strings = merged_environment(spec.env_set);
    std::vector<char*> argv = pointers(argv_strings);
    std::vector<char*> envp = pointers(env_strings);
    const std::string working_dir = spec.working_dir.empty() ? std::string() : spec.working_dir.string();

    Fd in_fd;
    if (spec.input == ProcessSpec::Input::text) {
        std::string why;
        in_fd = stdin_from_text(spec.stdin_text, why);
        if (!in_fd) return failed(why);
    } else {
        in_fd = Fd(::open("/dev/null", O_RDONLY | O_CLOEXEC));
        if (!in_fd) return failed("cannot open /dev/null: " + errno_text(errno));
    }
    Fd out_write, err_write;
    if (spec.output == ProcessSpec::Output::file) {
        out_write = Fd(::open(spec.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
        if (!out_write) return failed("cannot open " + to_utf8(spec.output_file) + ": " + errno_text(errno));
    } else if (!make_pipe(impl->out_read, out_write) || !make_pipe(impl->err_read, err_write)) {
        return failed("cannot create a pipe: " + errno_text(errno));
    }
    Fd report_read, report_write;
    if (!make_pipe(report_read, report_write)) return failed("cannot create a pipe: " + errno_text(errno));
    if (!lift_above_stdio(in_fd) || !lift_above_stdio(out_write) || (err_write && !lift_above_stdio(err_write)))
        return failed("cannot arrange its descriptors: " + errno_text(errno));

    const int err_fd = err_write ? err_write.get() : out_write.get();   // Output::file: one file for both
    const pid_t pid = ::fork();
    if (pid < 0) return failed("fork: " + errno_text(errno));
    if (pid == 0)
        run_child(spec.own_group, working_dir.empty() ? nullptr : working_dir.c_str(), in_fd.get(), out_write.get(),
                  err_fd, report_write.get(), argv[0], argv.data(), envp.data());

    impl->pid = pid;
    if (spec.own_group) ::setpgid(pid, pid);     // also from this side, so a stop never races it
    in_fd.reset();
    out_write.reset();
    err_write.reset();
    report_write.reset();
    for (Fd* fd : {&impl->out_read, &impl->err_read})
        if (*fd) ::fcntl(fd->get(), F_SETFL, ::fcntl(fd->get(), F_GETFL) | O_NONBLOCK);

    std::array<int, 2> report{};
    ssize_t got;
    do {
        got = ::read(report_read.get(), report.data(), sizeof(report));
    } while (got < 0 && errno == EINTR);
    if (got == static_cast<ssize_t>(sizeof(report))) {   // chdir or execve failed in the child
        int raw = 0;
        while (::waitpid(pid, &raw, 0) < 0 && errno == EINTR) {
        }
        impl->reaped = true;
        impl->pid = -1;
        impl->out_read.reset();
        impl->err_read.reset();
        const auto stage = static_cast<Stage>(report[0]);
        if (stage == Stage::chdir)
            return failed("cannot change to " + to_utf8(spec.working_dir) + ": " + errno_text(report[1]));
        return failed(errno_text(report[1]));
    }
    return Process(std::move(impl));
}

std::optional<int> Process::poll() {
    Impl& s = *impl_;
    if (!s.error.empty()) return -1;
    s.drain(0);
    s.check_timeout();
    if (!s.try_reap()) return std::nullopt;
    return s.exit_code();
}

ProcessResult Process::wait() {
    Impl& s = *impl_;
    if (s.result) return *s.result;
    int pause_ms = 1;
    while (s.error.empty() && !s.try_reap()) {
        s.check_timeout();
        s.drain(pause_ms);
        pause_ms = std::min(pause_ms * 2, 20);   // the poll backs off to 20 ms
    }
    // The child has ended. Its output may still sit in the pipes, or a child of its own may hold
    // them open: read until both close, for at most a second more.
    const auto give_up = Clock::now() + std::chrono::seconds(1);
    while ((s.out_read || s.err_read) && Clock::now() < give_up) s.drain(20);
    s.out_read.reset();
    s.err_read.reset();

    ProcessResult r;
    r.exit_code = s.exit_code();
    r.timed_out = s.timed_out;
    r.stdout_text = std::move(s.out_text);
    r.stderr_text = std::move(s.err_text);
    r.wall = std::chrono::duration_cast<std::chrono::milliseconds>(s.end - s.start);
    r.error = s.error.empty() ? s.wait_error : s.error;
#if defined(__APPLE__)
    r.peak_rss_bytes = static_cast<std::uint64_t>(s.usage.ru_maxrss);
#else
    r.peak_rss_bytes = static_cast<std::uint64_t>(s.usage.ru_maxrss) * 1024;
#endif
    s.result = r;
    return r;
}

void Process::stop() {
    std::lock_guard lock(impl_->mutex);
    impl_->kill_locked();
}

ProcessResult run_process(const ProcessSpec& spec) { return Process::spawn(spec).wait(); }

std::optional<std::filesystem::path> find_on_path(std::string_view program) {
    const auto executable = [](const fs::path& p) {
        struct stat info {};
        return ::stat(p.c_str(), &info) == 0 && S_ISREG(info.st_mode) && ::access(p.c_str(), X_OK) == 0;
    };
    if (program.find('/') != std::string_view::npos) {
        const fs::path p = path_from_utf8(program);
        if (executable(p)) return fs::absolute(p);
        return std::nullopt;
    }
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) return std::nullopt;
    std::string_view rest(path_env);
    while (true) {
        const std::size_t colon = rest.find(':');
        const std::string_view dir = rest.substr(0, colon);
        if (!dir.empty()) {                      // an empty entry would mean the launch directory
            const fs::path candidate = path_from_utf8(dir) / path_from_utf8(program);
            if (executable(candidate)) return fs::absolute(candidate);
        }
        if (colon == std::string_view::npos) break;
        rest.remove_prefix(colon + 1);
    }
    return std::nullopt;
}

}  // namespace fusion::core

#endif  // !_WIN32
