// fusion run: the platform code, the OS lock (09.C9) and the signal handlers (09.C8).
//
// FuSION 1 had neither: nothing showed whether a run was alive, and Ctrl-C or SIGTERM ended the
// process at once with nothing recorded (spec S8.28; the candidate set's runs r9 to r16, ended by
// SIGTERM, left nothing resumable, 09.C6). Here run.lock carries an OS lock that the kernel drops
// when the process ends for any reason, and the executable's handlers only request a stop.
//
// Linux uses open file description locks (F_OFD_SETLK): they belong to the one descriptor, so
// opening and closing run.lock elsewhere in the process (Lock::held) cannot drop them, unlike
// classic fcntl locks, and they conflict with the classic locks Java's FileChannel.tryLock() takes.
// macOS has no such locks, so it takes the classic ones, the kind tryLock() takes there (decision
// 0041). Those belong to the process: its own lock never conflicts with itself, and closing any
// descriptor of the file drops it. So Lock remembers the files this process locks, and held()
// answers for them without opening the file (09's platform notes). The Windows port (LockFileEx,
// SetConsoleCtrlHandler) comes with the Windows build (decision 0038).
#if defined(_WIN32)
#error "run: the Windows lock and console handler (09.C8, 09.C9) come with the Windows build (decision 0038)"
#endif

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <fcntl.h>
#include <unistd.h>

#include "internal.hpp"

namespace fusion::run {

namespace {

namespace fs = std::filesystem;

#if defined(F_OFD_SETLK)
constexpr int set_lock = F_OFD_SETLK, get_lock = F_OFD_GETLK;
constexpr bool process_locks = false;            // Linux: each lock belongs to its descriptor
#elif defined(__APPLE__)
constexpr int set_lock = F_SETLK, get_lock = F_GETLK;
constexpr bool process_locks = true;             // macOS: classic locks belong to the process (0041)
#else
#error "run: 09.C9's lock has no implementation for this platform"
#endif

// The files this process holds classic locks on, by descriptor: macOS only (decision 0041).
std::mutex own_mutex;
std::map<int, fs::path> own_locks;

fs::path lock_key(const fs::path& file) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(file, ec);
    return ec ? file.lexically_normal() : canonical;
}

void remember(int fd, const fs::path& file) {
    if constexpr (process_locks) {
        const fs::path key = lock_key(file);
        const std::lock_guard guard(own_mutex);
        own_locks[fd] = key;
    }
}

void forget(int fd) {
    if constexpr (process_locks) {
        const std::lock_guard guard(own_mutex);
        own_locks.erase(fd);
    }
}

bool locked_here(const fs::path& file) {
    if constexpr (process_locks) {
        const fs::path key = lock_key(file);
        const std::lock_guard guard(own_mutex);
        for (const auto& [fd, held] : own_locks)
            if (held == key) return true;
    }
    return false;
}

// One byte at offset 2^31, past run.lock's readable line, so Windows' mandatory locks will not stop
// a reader; a whole-file tryLock() still meets it (09's platform notes).
constexpr off_t lock_offset = off_t{1} << 31;

int lock_byte(int fd, int command, short type = F_WRLCK) {
    struct flock fl {};
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = lock_offset;
    fl.l_len = 1;
    fl.l_pid = 0;                                // required for open file description locks
    return ::fcntl(fd, command, &fl);
}

// A filesystem that cannot lock at all (NFS without a lock manager): the run goes on with a warning
// (09.C9, run.lock_unsupported).
bool cannot_lock(int error) noexcept {
    return error == ENOLCK || error == EOPNOTSUPP || error == ENOSYS || error == EINVAL;
}

bool write_all(int fd, std::string_view text) {
    while (!text.empty()) {
        const ssize_t n = ::write(fd, text.data(), text.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        text.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

std::string os_error(int error) { return std::error_code(error, std::generic_category()).message(); }

// 09.C8: set by the handlers, read by every Run's watcher.
std::atomic<int> stop_signal{0};
std::atomic<int> interrupts{0};
static_assert(std::atomic<int>::is_always_lock_free, "a signal handler may only touch lock-free atomics");

extern "C" void on_stop_signal(int signal) {
    // A second Ctrl-C ends the process at once with status 130 (09.C8); _Exit is async-signal-safe.
    if (signal == SIGINT && interrupts.fetch_add(1) >= 1) std::_Exit(130);
    stop_signal.store(1);
}

}  // namespace

// ---------------------------------------------------------------- 09.C9

Lock::Lock(int handle) : handle_(handle) {}

Lock::Lock(Lock&& other) noexcept : handle_(other.handle_), locked_(other.locked_) {
    other.handle_ = -1;
    other.locked_ = false;
}

Lock& Lock::operator=(Lock&& other) noexcept {
    if (this != &other) {
        if (handle_ >= 0) {
            forget(handle_);
            ::close(handle_);
        }
        handle_ = other.handle_;
        locked_ = other.locked_;
        other.handle_ = -1;
        other.locked_ = false;
    }
    return *this;
}

Lock::~Lock() {
    if (handle_ >= 0) {
        forget(handle_);
        ::close(handle_);                        // closing the one descriptor releases the lock
    }
}

bool Lock::locked() const noexcept { return locked_; }

Lock Lock::claim(const fs::path& run_dir) { return claim(run_dir, std::to_string(::getpid())); }

Lock Lock::claim(const fs::path& run_dir, std::string_view line) {
    const fs::path file = run_dir / "run.lock";
    // The exclusive create claims the directory: one that already holds run.lock has been used (09.C1).
    const int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) {
        const int error = errno;
        if (error == EEXIST)
            throw core::Failure("run.dir_used", core::to_utf8(run_dir) +
                                                    " already holds run.lock: another run has used it (S8.21)");
        throw core::Failure("run.dir_create", "cannot create " + core::to_utf8(file) + ": " + os_error(error));
    }
    Lock lock(fd);
    // The line is written through the one descriptor that holds the lock: a classic fcntl lock is
    // dropped when the process closes any descriptor of the file (09's platform notes).
    if (!write_all(fd, std::string(line) + "\n"))
        throw core::Failure("io.write", "cannot write " + core::to_utf8(file) + ": " + os_error(errno));
    if (lock_byte(fd, set_lock) == 0) {
        lock.locked_ = true;
        remember(fd, file);
    } else if (const int error = errno; !cannot_lock(error)) {
        throw core::Failure("run.dir_used", core::to_utf8(file) + " is locked by another process");
    }
    return lock;
}

bool Lock::held(const fs::path& run_dir) {
    const fs::path file = run_dir / "run.lock";
    if (locked_here(file)) return true;          // macOS: opening the file here would drop the lock
    const int fd = ::open(file.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    struct flock fl {};
    fl.l_type = F_WRLCK;                         // "could a writer lock it?"
    fl.l_whence = SEEK_SET;
    fl.l_start = lock_offset;
    fl.l_len = 1;
    fl.l_pid = 0;
    const int result = ::fcntl(fd, get_lock, &fl);
    ::close(fd);
    return result == 0 && fl.l_type != F_UNLCK;
}

Lock Lock::acquire(const fs::path& file, std::chrono::milliseconds wait) {
    const int fd = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) throw core::Failure("io.write", "cannot open " + core::to_utf8(file) + ": " + os_error(errno));
    Lock lock(fd);
    const auto deadline = std::chrono::steady_clock::now() + wait;
    for (;;) {
        if (lock_byte(fd, set_lock) == 0) {
            lock.locked_ = true;
            remember(fd, file);
            return lock;
        }
        const int error = errno;
        if (cannot_lock(error)) return lock;     // locked() says false; the caller decides
        if (error != EAGAIN && error != EACCES)
            throw core::Failure("io.write", "cannot lock " + core::to_utf8(file) + ": " + os_error(error));
        if (std::chrono::steady_clock::now() >= deadline)
            throw core::Failure("lock.timeout", core::to_utf8(file) + " is locked by another process");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

bool Lock::supported(const fs::path& dir) {
    // A probe file of our own, named *.tmp: the only kind of file FuSION removes (09's rules).
    const fs::path probe = dir / (".run-lock-probe-" + std::to_string(::getpid()) + ".tmp");
    const int fd = ::open(probe.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) return false;
    const bool ok = lock_byte(fd, set_lock) == 0 || !cannot_lock(errno);
    ::close(fd);
    ::unlink(probe.c_str());
    return ok;
}

// ---------------------------------------------------------------- 09.C8

void install_signal_handlers() {
    struct sigaction stop {};
    stop.sa_handler = on_stop_signal;
    sigemptyset(&stop.sa_mask);
    stop.sa_flags = SA_RESTART;
    ::sigaction(SIGINT, &stop, nullptr);
    ::sigaction(SIGTERM, &stop, nullptr);
    // A run outlives a closed terminal or a frontend that exits (09.C8, 10's platform notes).
    struct sigaction ignore {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ::sigaction(SIGHUP, &ignore, nullptr);
}

namespace detail {

bool take_signal_request() noexcept { return stop_signal.exchange(0) != 0; }

std::int64_t process_id() noexcept { return ::getpid(); }

}  // namespace detail

}  // namespace fusion::run
