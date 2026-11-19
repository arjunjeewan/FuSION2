// Section 12's core row: core::Failure, core::Outcome<T> and core::Stopped (09.C14), and the stop
// token (09's test hook "Stop: callbacks run once, including one registered after the request").
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "fusion/core/core.hpp"

using namespace fusion::core;
using namespace std::chrono_literals;

TEST_CASE("core: a failure carries its dotted code, its message and where it was found", "[unit][core]") {
    try {
        throw Failure("input.missing", "no such file", Where{"lists/kegg.txt", 9, "list"});
    } catch (const std::runtime_error& e) {   // the interpreter catches one type per command (S1.28)
        const auto& f = dynamic_cast<const Failure&>(e);
        CHECK(f.code() == "input.missing");
        CHECK(std::string(f.what()) == "no such file");
        CHECK(f.where().file == "lists/kegg.txt");
        CHECK(f.where().line == 9);
        CHECK(f.where().setting == "list");
    }
    const Failure plain("internal", "x");
    CHECK(plain.where().file.empty());
    CHECK(plain.where().line == 0);
    const Failure at_line("batch.syntax", "x", Where{"run.batch", 4});
    CHECK(at_line.where().setting.empty());
}

TEST_CASE("core: usage and install errors are failures with their own types", "[unit][core]") {
    CHECK_THROWS_AS(throw UsageError("usage.unknown_option", "--nope"), Failure);
    CHECK_THROWS_AS(throw InstallError("install.home_missing", "no home"), Failure);
    try {
        throw InstallError("install.file_missing", "data/kegg_pathways.txt");
    } catch (const UsageError&) {
        FAIL("an install error is not a usage error");
    } catch (const InstallError& e) {
        CHECK(e.code() == "install.file_missing");
    }
}

TEST_CASE("core: a reported result is a return value, not an exception", "[unit][core]") {
    const Outcome<int> plain{7};   // the short forms every module writes compile without warnings
    CHECK(plain.value == 7);
    CHECK_FALSE(plain.report);
    CHECK_FALSE(plain.partial);
    const Outcome<int> reported{0, Report{"corridor.empty", "no path from EGFR to CXCL8 within bound 3"}};
    REQUIRE(reported.report);
    CHECK(reported.report->code == "corridor.empty");
    const Outcome<void> nothing;                // section 01's handlers return no value
    CHECK_FALSE(nothing.report);
    const Outcome<void> partial{std::nullopt, true};   // a solve ended by its budget (06.C3)
    CHECK(partial.partial);
    CHECK(std::string(Stopped().what()) == "stopped");
}

TEST_CASE("core: a default stop token never stops", "[unit][core]") {
    const StopToken token;
    CHECK_FALSE(token.stop_requested());
    CHECK_NOTHROW(token.throw_if_stopped());
    int calls = 0;
    const StopCallback callback(token, [&] { ++calls; });
    CHECK(calls == 0);
}

TEST_CASE("core: a stop request reaches every token and makes throw_if_stopped throw Stopped", "[unit][core]") {
    StopSource source;
    const StopToken before = source.token();
    CHECK_FALSE(before.stop_requested());
    CHECK(source.request_stop());
    CHECK_FALSE(source.request_stop());         // only the first call makes the request
    CHECK(before.stop_requested());
    CHECK(source.token().stop_requested());
    CHECK_THROWS_AS(before.throw_if_stopped(), Stopped);
}

TEST_CASE("core: callbacks run once, including one registered after the request", "[unit][core]") {
    StopSource source;
    int early = 0, late = 0, order = 0, first = 0, second = 0;
    const StopCallback a(source.token(), [&] { ++early; });
    const StopCallback b(source.token(), [&] { first = ++order; });
    const StopCallback c(source.token(), [&] { second = ++order; });
    source.request_stop();
    source.request_stop();
    CHECK(early == 1);
    CHECK(first == 1);                          // in registration order
    CHECK(second == 2);
    const StopCallback d(source.token(), [&] { ++late; });
    CHECK(late == 1);                           // runs at once, in the registering thread
    source.request_stop();
    CHECK(late == 1);
}

TEST_CASE("core: a callback destroyed before the request never runs", "[unit][core]") {
    StopSource source;
    int calls = 0;
    {
        const StopCallback gone(source.token(), [&] { ++calls; });
    }
    source.request_stop();
    CHECK(calls == 0);
}

TEST_CASE("core: a stop requested by another thread reaches the callbacks and the tokens", "[unit][core]") {
    StopSource source;
    std::atomic<int> calls{0};
    const StopCallback callback(source.token(), [&] { ++calls; });
    std::thread watcher([&] { source.request_stop(); });   // as section 09's stop watcher does
    watcher.join();
    CHECK(calls == 1);
    CHECK(source.token().stop_requested());
}

TEST_CASE("core: a callback's destructor waits while the callback runs on another thread", "[unit][core]") {
    StopSource source;
    std::atomic<bool> started{false}, finished{false};
    auto callback = std::make_unique<StopCallback>(source.token(), [&] {
        started = true;
        std::this_thread::sleep_for(100ms);     // z3's interrupt, standing in
        finished = true;
    });
    std::thread watcher([&] { source.request_stop(); });
    while (!started) std::this_thread::yield();
    callback.reset();                           // must not return while the callback runs
    CHECK(finished);
    watcher.join();
}
