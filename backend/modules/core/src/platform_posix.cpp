// fusion core: section 11's platform layer on Linux and macOS (11.C19; section 09, Platform notes).
// Windows' side (GetModuleFileNameW, GetProcessTimes, GetThreadTimes, \\?\ paths) comes with the
// Windows build, in a later iteration (decision 0038).
#if !defined(_WIN32)

#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"
#include "fusion/core/platform.hpp"

namespace fusion::core {

std::filesystem::path executable_path() {
    // FuSION 1 read /proc/self/exe on Linux and left _NSGetExecutablePath's answer unresolved on
    // macOS (Linux/src/main.cc:5728-5740, macOS/src/main.cc:5756-5767); S1.20 resolves symlinks
    // the same way on both.
#if defined(__linux__)
    std::array<char, 4096> buffer{};
    const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
        throw InstallError("install.executable", std::string("fusion: cannot find its own executable: ") +
                                                     std::strerror(errno));
    return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length)));
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw InstallError("install.executable", "fusion: cannot find its own executable");
    return std::filesystem::canonical(std::filesystem::path(buffer.c_str()));
#else
#error "executable_path: no implementation for this platform"
#endif
}

double cpu_seconds() {
    // getrusage, not std::clock(), which measures wall time on Windows (09, Platform notes).
    rusage usage{};
    ::getrusage(RUSAGE_SELF, &usage);
    const auto seconds = [](const timeval& t) {
        return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_usec) / 1e6;
    };
    return seconds(usage.ru_utime) + seconds(usage.ru_stime);
}

std::uint64_t peak_rss_bytes() {
    rusage usage{};
    ::getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);          // bytes on macOS
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;   // KiB on Linux
#endif
}

std::chrono::nanoseconds thread_cpu_time() {
    timespec t{};
    ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
}

std::filesystem::path long_path(const std::filesystem::path& path) { return path; }   // no limit here

}  // namespace fusion::core

#endif  // !_WIN32
