// The OS lock (09.C9) and the signal handlers (09.C8), with child processes where a test needs one.
// The test process has no other thread when it forks: every Run of the earlier tests has ended.
#include <csignal>

#include <sys/wait.h>

#include "support.hpp"

namespace fusion::run::test {

namespace {

// Runs body in a child process, which ends with _exit, and returns its wait status.
template <class Body>
int in_child(Body body) {
    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        body();
        ::_exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    return status;
}

void restore_default_handlers() {
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    std::signal(SIGHUP, SIG_DFL);
}

}  // namespace

TEST_CASE("run: a child holding run.lock makes held() true, and false once the child is killed (09.C9)", "[unit][run]") {
    TempDir tmp;
    int ready[2];
    REQUIRE(::pipe(ready) == 0);
    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(ready[0]);
        try {
            const Lock lock = Lock::claim(tmp.path(), "4242 r-20261112T131400Z-7f3a9c2e");
            const char state = lock.locked() ? 'L' : 'U';
            if (::write(ready[1], &state, 1) != 1) ::_exit(3);
            for (;;) ::pause();
        } catch (...) {
            ::_exit(2);
        }
    }
    ::close(ready[1]);
    char state = 0;
    REQUIRE(::read(ready[0], &state, 1) == 1);
    ::close(ready[0]);
    CHECK(state == 'L');
    CHECK(Lock::held(tmp.path()));
    CHECK(read_file(tmp / "run.lock") == "4242 r-20261112T131400Z-7f3a9c2e\n");   // readable while locked
    CHECK(failure_code([&] { Lock::claim(tmp.path()); }) == "run.dir_used");
    ::kill(pid, SIGKILL);                        // kill -9: nothing in the child runs again
    int status = 0;
    ::waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK_FALSE(Lock::held(tmp.path()));         // the kernel dropped the lock with the process
}

TEST_CASE("run: a run holds its lock while it lasts, and probing it in the same process does not drop it (09.C9)",
          "[unit][run]") {
    TempDir tmp;
    fs::path dir;
    {
        Run run = Run::start(options_in(tmp.path()));
        dir = run.dir();
        CHECK(Lock::held(dir));
        CHECK(Lock::held(dir));                  // the first probe opened and closed run.lock
        CHECK(read_file(dir / "run.lock") == std::format("{} {}\n", ::getpid(), run.id()));
        CHECK(failure_code([&] { Lock::claim(dir); }) == "run.dir_used");
        run.finish(RunStatus::ok);
    }
    CHECK_FALSE(Lock::held(dir));
    CHECK_FALSE(Lock::held(tmp / "no-such-run"));
}

TEST_CASE("run: Lock::acquire locks any file and waits no longer than it is told (09.C9, 2B.17)", "[unit][run]") {
    TempDir tmp;
    const fs::path site = tmp / "site.lock";
    {
        const Lock first = Lock::acquire(site, 0ms);
        CHECK(first.locked());
        const auto start = std::chrono::steady_clock::now();
        CHECK(failure_code([&] { Lock::acquire(site, 100ms); }) == "lock.timeout");
        CHECK(std::chrono::steady_clock::now() - start >= 100ms);
    }
    const Lock again = Lock::acquire(site, 0ms);   // released with the first
    CHECK(again.locked());
}

TEST_CASE("run: a moved lock stays held; the lock-less probe leaves nothing behind (09.C9)", "[unit][run]") {
    TempDir tmp;
    Lock a = Lock::claim(tmp.path(), "line");
    Lock b = std::move(a);
    CHECK(b.locked());
    CHECK_FALSE(a.locked());                     // NOLINT(bugprone-use-after-move): the moved-from state is defined
    CHECK(Lock::held(tmp.path()));
    TempDir other;
    CHECK(Lock::supported(other.path()));
    CHECK(fs::is_empty(other.path()));
}

TEST_CASE("run: SIGTERM requests the same stop as the stop file (09.C8)", "[unit][run]") {
    TempDir tmp;
    install_signal_handlers();
    {
        Run run = Run::start(options_in(tmp.path()));
        std::raise(SIGTERM);                     // as kill, timeout or Java's Process.destroy() send it
        CHECK(wait_for([&] { return run.stop().stop_requested(); }));
        CHECK(run.finish(RunStatus::stopped) == 3);
        const auto events = read_lines(run.dir() / "events.jsonl");
        check_events(events);
        REQUIRE(of_type(events, "stop.requested").size() == 1);
        CHECK(of_type(events, "stop.requested")[0]["source"] == "signal");
        const core::Json manifest = read_json(run.dir() / "run.json");
        check_manifest(manifest);
        CHECK(manifest["stop"]["source"] == "signal");
        CHECK(manifest["status"] == "stopped");
    }
    restore_default_handlers();
}

TEST_CASE("run: one Ctrl-C requests a stop; a second ends the process at once with status 130 (09.C8)", "[unit][run]") {
    const int once = in_child([] {
        install_signal_handlers();
        std::raise(SIGINT);
        ::_exit(7);
    });
    CHECK(WIFEXITED(once));
    CHECK(WEXITSTATUS(once) == 7);
    const int twice = in_child([] {
        install_signal_handlers();
        std::raise(SIGINT);
        std::raise(SIGINT);
        ::_exit(7);
    });
    CHECK(WIFEXITED(twice));
    CHECK(WEXITSTATUS(twice) == 130);
}

TEST_CASE("run: SIGHUP is ignored, so a run outlives its terminal or its frontend (09.C8, 10.C16)", "[unit][run]") {
    const int ignored = in_child([] {
        install_signal_handlers();
        std::raise(SIGHUP);
        ::_exit(0);
    });
    CHECK(WIFEXITED(ignored));
    CHECK(WEXITSTATUS(ignored) == 0);
    const int default_action = in_child([] {
        std::signal(SIGHUP, SIG_DFL);
        std::raise(SIGHUP);
        ::_exit(0);
    });
    CHECK(WIFSIGNALED(default_action));
}

}  // namespace fusion::run::test
