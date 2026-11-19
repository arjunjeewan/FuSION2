// Section 12's core row: the log (B1, S1.29) and the settings registry (every setting with its
// value and source, as run.json records it; E1, cross-section item 1.16).
#include <algorithm>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fusion/core/core.hpp"

using namespace fusion::core;

namespace {

struct Recorder final : LogSink {
    std::vector<std::tuple<Level, std::string, std::string>> lines;
    void write(Level level, std::string_view code, std::string_view text) override {
        lines.emplace_back(level, std::string(code), std::string(text));
    }
};

// Builds its record a character at a time, so lines from two threads would interleave unless the
// log hands over one whole line at a time.
struct SlowRecorder final : LogSink {
    std::string all;
    void write(Level, std::string_view, std::string_view text) override {
        for (const char c : text) {
            all.push_back(c);
            if (all.size() % 7 == 0) std::this_thread::yield();
        }
        all.push_back('\n');
    }
};

}  // namespace

TEST_CASE("core: the log hands every line to every sink, with its level and code", "[unit][core]") {
    Log log(false);
    auto a = std::make_shared<Recorder>();
    auto b = std::make_shared<Recorder>();
    log.add_sink(a);
    log.add_sink(b);
    log.info("fb_rch: corridor 115 nodes, 370 edges");
    log.warning("gsea.keep_unmatched", "hsa9999 is in no candidate");
    log.error("input.missing", "no such file");
    REQUIRE(a->lines.size() == 3);
    CHECK(a->lines == b->lines);
    CHECK(a->lines[0] == std::make_tuple(Level::info, std::string(), std::string("fb_rch: corridor 115 nodes, 370 edges")));
    CHECK(a->lines[1] == std::make_tuple(Level::warning, std::string("gsea.keep_unmatched"), std::string("hsa9999 is in no candidate")));
    CHECK(a->lines[2] == std::make_tuple(Level::error, std::string("input.missing"), std::string("no such file")));
}

TEST_CASE("core: debug lines reach the sinks only under the debug option (S1.29)", "[unit][core]") {
    for (const bool debug : {false, true}) {
        Log log(debug);
        auto sink = std::make_shared<Recorder>();
        log.add_sink(sink);
        CHECK(log.debug_on() == debug);
        log.debug("fb_rch: forward set 354 nodes");
        CHECK(sink->lines.size() == (debug ? 1u : 0u));
    }
}

TEST_CASE("core: lines logged from many threads arrive whole", "[unit][core]") {
    Log log(false);
    auto sink = std::make_shared<SlowRecorder>();
    log.add_sink(sink);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([&log, t] {
            for (int i = 0; i < 200; ++i) log.info("thread " + std::to_string(t) + " line " + std::to_string(i) + " end");
        });
    for (auto& t : threads) t.join();
    std::size_t lines = 0, start = 0;
    while (start < sink->all.size()) {
        const std::size_t end = sink->all.find('\n', start);
        const std::string line = sink->all.substr(start, end - start);
        CHECK_THAT(line, Catch::Matchers::StartsWith("thread ") && Catch::Matchers::EndsWith(" end"));
        CHECK(line.find("thread", 1) == std::string::npos);   // "thread" once: no two lines mixed
        ++lines;
        start = end + 1;
    }
    CHECK(lines == 1600);
}

namespace {

constexpr double inf = std::numeric_limits<double>::infinity();
constexpr std::string_view edge_choices[] = {"any", "activation", "expression"};
constexpr SettingSpec bound{.name = "bound", .module = "corridor", .type = Type::integer, .need = Need::required,
                            .min = 0, .max = 50, .help = "path length bound"};
constexpr SettingSpec edges{.name = "edges_to_target", .module = "corridor", .type = Type::choice,
                            .need = Need::defaulted, .default_text = "any", .choices = edge_choices};
constexpr SettingSpec exclude{.name = "exclude", .module = "corridor", .type = Type::input_list, .need = Need::optional};
constexpr SettingSpec seed{.name = "seed", .module = "solve", .type = Type::integer, .need = Need::defaulted,
                           .default_text = "42"};
constexpr SettingSpec timeout{.name = "timeout", .module = "solve", .type = Type::duration, .need = Need::defaulted,
                              .default_text = "4h", .on_resume = OnResume::may_change};
constexpr SettingSpec up{.name = "up", .module = "expression", .type = Type::real, .need = Need::required};
constexpr SettingSpec cache{.name = "cache", .module = "run", .type = Type::boolean, .need = Need::defaulted,
                            .default_text = "true", .affects_results = false};
constexpr SettingSpec log_name{.name = "log", .module = "run", .type = Type::output_name, .need = Need::computed,
                               .affects_results = false};

Settings sample() {
    // Given out of order: Settings sorts by key.
    return Settings({
        {&timeout, Value{std::int64_t{14400000}}, Source::set_option, {}, {}},
        {&up, Value{1.01}, Source::positional, {}, {}},
        {&bound, Value{std::int64_t{10}}, Source::named, {}, {}},
        {&cache, Value{true}, Source::run_line, {}, {}},
        {&exclude, std::nullopt, Source::default_value, {}, {}},
        {&seed, Value{std::int64_t{42}}, Source::default_value, {}, {}},
        {&edges, Value{std::string("any")}, Source::default_value, {}, {}},
        {&log_name, Value{std::string("egfr_20261112-184400.log")}, Source::default_value, {}, {}},
    });
}

}  // namespace

TEST_CASE("core: a setting's key is <module>.<name>", "[unit][core]") {
    CHECK(key(bound) == "corridor.bound");
    CHECK(key(edges) == "corridor.edges_to_target");
    CHECK(edges.choices.size() == 3);
    CHECK(bound.max == 50);
    CHECK(exclude.max == inf);                  // no range unless given
    CHECK(bound.affects_results);
    CHECK_FALSE(cache.affects_results);
    CHECK(timeout.on_resume == OnResume::may_change);
}

TEST_CASE("core: a step's settings are sorted by key, in byte order", "[unit][core]") {
    const Settings s = sample();
    std::vector<std::string> keys;
    for (const Setting& setting : s.all()) keys.push_back(key(*setting.spec));
    CHECK(keys == std::vector<std::string>{"corridor.bound", "corridor.edges_to_target", "corridor.exclude",
                                           "expression.up", "run.cache", "run.log", "solve.seed", "solve.timeout"});
}

TEST_CASE("core: settings are found by their bare name, with their values and sources", "[unit][core]") {
    const Settings s = sample();
    CHECK(s.at("bound").source == Source::named);
    CHECK(s.get<std::int64_t>("bound") == 10);
    CHECK(s.get<double>("up") == 1.01);
    CHECK(s.get<bool>("cache"));
    CHECK(s.get<std::string>("edges_to_target") == "any");
    CHECK(s.get<std::int64_t>("timeout") == 14400000);   // a duration is whole milliseconds
    CHECK(s.find<std::int64_t>("seed") == 42);
    CHECK_FALSE(s.find<std::vector<std::string>>("exclude"));   // optional, left out (0021)
    CHECK_FALSE(s.find<std::int64_t>("no_such_setting"));
    try {
        s.at("no_such_setting");
        FAIL("an unknown setting must throw");
    } catch (const Failure& f) {
        CHECK(f.code() == "setting.unknown");
        CHECK(f.where().setting == "no_such_setting");
    }
    try {
        s.get<std::vector<std::string>>("exclude");
        FAIL("a setting left out must throw");
    } catch (const Failure& f) {
        CHECK(f.code() == "setting.left_out");
    }
}

TEST_CASE("core: a step that changes a setting records the effective value and the reason (S4.16)", "[unit][core]") {
    Settings s = sample();
    s.set_effective("bound", std::int64_t{3}, "3 changed genes lie in the corridor (S4.16)");
    CHECK(s.get<std::int64_t>("bound") == 10);  // the value given stays
    REQUIRE(s.at("bound").effective);
    CHECK(std::get<std::int64_t>(*s.at("bound").effective) == 3);
    CHECK_THROWS_AS(s.set_effective("nope", true, "x"), Failure);
}

TEST_CASE("core: settings go into run.json with their value and the source that applied (E1)", "[unit][core]") {
    Settings s = sample();
    s.set_effective("bound", std::int64_t{3}, "3 changed genes lie in the corridor (S4.16)");
    CHECK(to_json(s).dump() ==
          R"j([{"key":"corridor.bound","value":10,"source":"named","effective":3,)j"
          R"j("reason":"3 changed genes lie in the corridor (S4.16)"},)j"
          R"j({"key":"corridor.edges_to_target","value":"any","source":"default"},)j"
          R"j({"key":"corridor.exclude","value":null,"source":"default"},)j"
          R"j({"key":"expression.up","value":1.01,"source":"positional"},)j"
          R"j({"key":"run.cache","value":true,"source":"run_line"},)j"
          R"j({"key":"run.log","value":"egfr_20261112-184400.log","source":"default"},)j"
          R"j({"key":"solve.seed","value":42,"source":"default"},)j"
          R"j({"key":"solve.timeout","value":14400000,"source":"set"}])j");
    CHECK(to_json(Settings()).dump() == "[]");
}

TEST_CASE("core: values and sources have one spelling", "[unit][core]") {
    CHECK(to_json(Value{true}).dump() == "true");
    CHECK(to_json(Value{std::int64_t{-5}}).dump() == "-5");
    CHECK(to_json(Value{0.25}).dump() == "0.25");
    CHECK(to_json(Value{std::string("fdr:0.25")}).dump() == R"("fdr:0.25")");
    CHECK(to_json(Value{std::vector<std::string>{"hsa1956", "hsa2064"}}).dump() == R"(["hsa1956","hsa2064"])");
    CHECK(to_json(Value{std::vector<std::string>{}}).dump() == "[]");
    CHECK(source_name(Source::default_value) == "default");
    CHECK(source_name(Source::positional) == "positional");
    CHECK(source_name(Source::named) == "named");
    CHECK(source_name(Source::set_option) == "set");
    CHECK(source_name(Source::run_line) == "run_line");
}
