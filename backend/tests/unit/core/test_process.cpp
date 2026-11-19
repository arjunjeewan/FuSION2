// Section 12's core row: 11's process layer (11.C19, cross-section item 2A.11), with section 11's
// test hook "a child started with a working directory reports that directory", and the CPU and
// memory figures run.json records.
#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fusion/core/core.hpp"
#include "fusion/core/platform.hpp"
#include "support.hpp"

using namespace fusion::core;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

ProcessSpec shell(std::string script) {
    ProcessSpec spec;
    spec.program = "/bin/sh";
    spec.args = {"-c", std::move(script)};
    return spec;
}

}  // namespace

TEST_CASE("core: a child's exit status, stdout and stderr are captured", "[unit][core]") {
    const ProcessResult r = run_process(shell("echo out; echo err >&2; exit 3"));
    CHECK(r.exit_code == 3);
    CHECK(r.stdout_text == "out\n");
    CHECK(r.stderr_text == "err\n");
    CHECK_FALSE(r.timed_out);
    CHECK(r.error.empty());
    CHECK(r.peak_rss_bytes > 0);
}

TEST_CASE("core: each argument reaches the child as it is, with no shell in between (S6.10)", "[unit][core]") {
    ProcessSpec spec = shell(R"(for a in "$@"; do printf '[%s]' "$a"; done)");
    spec.args.insert(spec.args.end(), {"sh", "two words", "$HOME", "q\"uote", "back\\slash", "", "*"});
    const ProcessResult r = run_process(spec);
    CHECK(r.stdout_text == "[two words][$HOME][q\"uote][back\\slash][][*]");
}

TEST_CASE("core: a child starts in the working directory it is given", "[unit][core]") {
    test::ScratchDir dir("cwd");
    ProcessSpec spec = shell("pwd -P");
    spec.working_dir = dir.path();
    CHECK(run_process(spec).stdout_text == fs::canonical(dir.path()).string() + "\n");
}

TEST_CASE("core: a child gets the environment with the given variables added or replaced", "[unit][core]") {
    const test::ScopedEnv mine("FUSION_TEST_INHERITED", "kept");
    const test::ScopedEnv replaced("FUSION_TEST_REPLACED", "old");
    ProcessSpec spec = shell("echo \"$FUSION_TEST_INHERITED $FUSION_TEST_REPLACED $FUSION_TEST_ADDED\"");
    spec.env_set = {{"FUSION_TEST_REPLACED", "new"}, {"FUSION_TEST_ADDED", "value with spaces"}};
    CHECK(run_process(spec).stdout_text == "kept new value with spaces\n");
}

TEST_CASE("core: stdin is the null device, or the text given", "[unit][core]") {
    CHECK(run_process(shell("cat; echo done")).stdout_text == "done\n");   // EOF at once
    ProcessSpec spec = shell("cat");
    spec.input = ProcessSpec::Input::text;
    std::string script;
    for (int i = 0; i < 40000; ++i) script += "plot $data using 1:2 # line " + std::to_string(i) + "\n";
    spec.stdin_text = script;                    // about 1.4 MB, more than any pipe holds
    const ProcessResult r = run_process(spec);
    CHECK(r.exit_code == 0);
    CHECK(r.stdout_text == script);
}

TEST_CASE("core: output larger than a pipe is captured whole from both streams", "[unit][core]") {
    const ProcessResult r = run_process(shell("i=0; while [ $i -lt 3000 ]; do echo \"out line $i of a long output\"; "
                                              "echo \"err line $i\" >&2; i=$((i+1)); done"));
    CHECK(r.exit_code == 0);
    CHECK(r.stdout_text.size() > 70000);
    CHECK_THAT(r.stdout_text, ContainsSubstring("out line 2999 of a long output\n"));
    CHECK_THAT(r.stderr_text, ContainsSubstring("err line 2999\n"));
}

TEST_CASE("core: stdout and stderr can go together, in order, to one file", "[unit][core]") {
    test::ScratchDir dir("file");
    ProcessSpec spec = shell("echo one; echo two >&2; echo three");
    spec.output = ProcessSpec::Output::file;     // how exec lines run (01, row 1.15)
    spec.output_file = dir / "exec.out";
    const ProcessResult r = run_process(spec);
    CHECK(r.exit_code == 0);
    CHECK(r.stdout_text.empty());
    CHECK(test::read_file(dir / "exec.out") == "one\ntwo\nthree\n");
}

TEST_CASE("core: a child past its timeout is stopped", "[unit][core]") {
    ProcessSpec spec = shell("sleep 30");
    spec.timeout = 200ms;
    spec.own_group = true;
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult r = run_process(spec);
    CHECK(r.timed_out);
    CHECK(r.exit_code == -1);
    CHECK(std::chrono::steady_clock::now() - start < 5s);
    CHECK(r.wall >= 200ms);
}

TEST_CASE("core: stop ends the child and everything it started (its process group)", "[unit][core]") {
    test::ScratchDir dir("group");
    ProcessSpec spec = shell("sleep 30 & echo $! > \"$1\"; wait");
    spec.args.push_back("sh");
    spec.args.push_back((dir / "grandchild.pid").string());
    spec.own_group = true;
    Process p = Process::spawn(spec);
    std::string pid_text;
    for (int i = 0; i < 400 && pid_text.empty(); ++i) {
        std::this_thread::sleep_for(5ms);
        pid_text = test::read_file(dir / "grandchild.pid");
    }
    REQUIRE_FALSE(pid_text.empty());
    const pid_t grandchild = std::stoi(pid_text);
    CHECK_FALSE(p.poll());                      // still running
    std::thread stopper([&] { p.stop(); });     // from another thread, as a stop callback does
    stopper.join();
    const ProcessResult r = p.wait();
    CHECK(r.exit_code == -1);
    bool gone = false;
    for (int i = 0; i < 400 && !gone; ++i) {    // reaped by init once killed
        gone = ::kill(grandchild, 0) != 0;
        if (!gone) std::this_thread::sleep_for(5ms);
    }
    CHECK(gone);
}

TEST_CASE("core: poll reports the exit status once the child has ended", "[unit][core]") {
    Process p = Process::spawn(shell("exit 5"));
    std::optional<int> status;
    for (int i = 0; i < 1000 && !status; ++i) {
        status = p.poll();
        if (!status) std::this_thread::sleep_for(2ms);
    }
    CHECK(status == 5);
    CHECK(p.wait().exit_code == 5);
    CHECK(p.wait().exit_code == 5);             // the result is kept
}

TEST_CASE("core: a child that cannot start is exit status -1, with the reason", "[unit][core]") {
    ProcessSpec missing;
    missing.program = "/no/such/program";
    const ProcessResult r = run_process(missing);
    CHECK(r.exit_code == -1);
    CHECK_THAT(r.error, ContainsSubstring("/no/such/program"));

    ProcessSpec bad_dir = shell("true");
    bad_dir.working_dir = "/no/such/directory";
    const ProcessResult d = run_process(bad_dir);
    CHECK(d.exit_code == -1);
    CHECK_THAT(d.error, ContainsSubstring("/no/such/directory"));

    ProcessSpec bad_file = shell("true");
    bad_file.output = ProcessSpec::Output::file;
    bad_file.output_file = "/no/such/directory/out.txt";
    CHECK(run_process(bad_file).exit_code == -1);
    Process never = Process::spawn(missing);
    CHECK(never.poll() == -1);
}

TEST_CASE("core: a program is found on PATH, never in the launch directory", "[unit][core]") {
    const auto sh = find_on_path("sh");
    REQUIRE(sh);
    CHECK(sh->is_absolute());
    CHECK(sh->filename() == "sh");
    CHECK_FALSE(find_on_path("no-such-program-fusion-test"));
    CHECK(find_on_path("/bin/sh") == fs::path("/bin/sh"));
    CHECK_FALSE(find_on_path("/no/such/sh"));
    test::ScratchDir dir("path");
    test::write_script(dir / "fusion-test-tool", "exit 0\n");
    const test::ScopedEnv path("PATH", ":/no/such/dir:" + dir.path().string());
    CHECK(find_on_path("fusion-test-tool") == dir / "fusion-test-tool");
}

TEST_CASE("core: CPU time and peak memory are measured", "[unit][core]") {
    const auto thread_before = thread_cpu_time();
    const double process_before = cpu_seconds();
    volatile double sink = 0;
    for (int i = 0; i < 20000000; ++i) sink = sink + 1.0;
    CHECK(thread_cpu_time() > thread_before);
    CHECK(cpu_seconds() > process_before);
    CHECK(peak_rss_bytes() > 1024 * 1024);
    CHECK(long_path("/a/b") == fs::path("/a/b"));
}
