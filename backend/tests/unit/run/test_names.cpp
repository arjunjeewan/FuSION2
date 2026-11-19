// Run directory names, run ids and run's settings (09.C1, cross-section item 1.26, S8.21, S8.28).
#include <ctime>
#include <set>

#include "support.hpp"

namespace fusion::run::test {

TEST_CASE("run: labels reduce by item 1.26's rule, as the golden list shared with section 10 says", "[unit][run]") {
    const fs::path golden = fs::path(FUSION_TEST_SOURCE_DIR) / "unit/run/golden/labels.tsv";
    std::istringstream in(read_file(golden));
    std::string line;
    std::getline(in, line);
    CHECK(line == "label\treduced");
    int rows = 0;
    while (std::getline(in, line)) {
        const auto tab = line.find('\t');
        REQUIRE(tab != std::string::npos);
        const std::string label = line.substr(0, tab);
        INFO("label: '" << label << "'");
        CHECK(reduce_label(label) == line.substr(tab + 1));
        ++rows;
    }
    CHECK(rows == 27);
}

TEST_CASE("run: a reduced label holds only [A-Za-z0-9._-], at most its length, and never ends with - or .",
          "[unit][run]") {
    CHECK(reduce_label(std::string(40, 'b'), 32) == std::string(32, 'b'));   // a sweep point's label (01)
    CHECK(reduce_label("point fdr0.25", 32) == "point-fdr0.25");
    CHECK(reduce_label("tab\tand\nnewline") == "tab-and-newline");
    CHECK(reduce_label(std::string("nul\0byte", 8)) == "nul-byte");
    const std::string awkward[] = {"a\xff\xfe" "b", "..--..", "-.-x-.-", "CON.", "com9.tar.gz", "\xe2\x86\x92" "CON",
                                   std::string(100, '.') + "z", "z" + std::string(100, '-')};
    for (const std::string& label : awkward) {
        const std::string reduced = reduce_label(label);
        INFO("label: '" << label << "' reduced: '" << reduced << "'");
        CHECK(reduced.size() <= 44);
        CHECK(reduced.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-") ==
              std::string::npos);
        if (!reduced.empty()) {
            CHECK(reduced.back() != '-');
            CHECK(reduced.back() != '.');
            CHECK(reduced.front() != '-');
        }
    }
    CHECK(reduce_label("com9.tar.gz") == "_com9.tar.gz");   // Windows reserves a device name with any extension
    CHECK(reduce_label("\xe2\x86\x92" "CON") == "_CON");
}

TEST_CASE("run: a run's name is its label, then the local time; run_<time> without a label (09.C1)", "[unit][run]") {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&t, &local);
    const std::string stamp = std::format("{:04}{:02}{:02}-{:02}{:02}{:02}", local.tm_year + 1900, local.tm_mon + 1,
                                          local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    CHECK(run_name("EGFR→CXCL8 / b10", now) == "EGFR-CXCL8-b10_" + stamp);
    CHECK(run_name("", now) == "run_" + stamp);
    CHECK(run_name("→ / ←", now) == "run_" + stamp);
    CHECK(run_name(std::string(60, 'z'), now).size() == 44 + 1 + 15);   // 64 with a -NNN suffix (09.C1)
}

TEST_CASE("run: a run id is r-<UTC time>-<8 hex digits>, new for every run (09.C1)", "[unit][run]") {
    using namespace std::chrono;
    const auto t = sys_days{year{2026} / 11 / 12} + 13h + 14min;
    const std::string a = new_run_id(t);
    const std::string b = new_run_id(t);
    CHECK(std::regex_match(a, std::regex(R"(r-20261112T131400Z-[0-9a-f]{8})")));
    CHECK(a != b);
}

TEST_CASE("run: 16 threads claiming one label in the same second get 16 directories (09.C1, S8.28)", "[unit][run]") {
    TempDir tmp;
    const auto now = std::chrono::system_clock::now();
    std::vector<fs::path> dirs(16);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < dirs.size(); ++i)
        threads.emplace_back([&, i] { dirs[i] = claim_run_dir(tmp.path(), "same label", now); });
    for (std::thread& thread : threads) thread.join();
    const std::set<fs::path> distinct(dirs.begin(), dirs.end());
    CHECK(distinct.size() == 16);
    const std::string base = run_name("same label", now);
    CHECK(distinct.contains(tmp / base));
    for (int n = 2; n <= 16; ++n) CHECK(distinct.contains(tmp / std::format("{}-{}", base, n)));
    for (const fs::path& dir : dirs) CHECK(fs::is_directory(dir));
}

TEST_CASE("run: a name taken by a file is skipped, and an unusable root is run.dir_create", "[unit][run]") {
    TempDir tmp;
    const auto now = std::chrono::system_clock::now();
    write_file(tmp / run_name("blocked", now), "not a directory");
    CHECK(claim_run_dir(tmp.path(), "blocked", now) == tmp / (run_name("blocked", now) + "-2"));
    write_file(tmp / "file", "x");
    CHECK(failure_code([&] { claim_run_dir(tmp / "file" / "runs", "x", now); }) == "run.dir_create");
    CHECK(claim_run_dir(tmp / "made" / "on" / "demand", "x", now).parent_path() == tmp / "made" / "on" / "demand");
}

TEST_CASE("run: a run directory is never reused: a second run gets its own, and the first is untouched (S8.21)",
          "[unit][run]") {
    TempDir tmp;
    const Options options = options_in(tmp.path(), "rerun");
    fs::path first;
    {
        Run run = Run::start(options);
        first = run.dir();
        Run::Step s = run.begin(step("cudf", "cudf", 3));
        core::OutputFile out = s.context().files.create("up_reg");
        out.out() << "hsa1\n";
        out.commit();
        run.end(s, StepStatus::ok);
        CHECK(run.finish(RunStatus::ok) == 0);
    }
    const std::string manifest = read_file(first / "run.json");
    const std::string events = read_file(first / "events.jsonl");
    fs::path second;
    {
        Run run = Run::start(options);
        second = run.dir();
        Run::Step s = run.begin(step("cudf", "cudf", 3));
        core::OutputFile out = s.context().files.create("up_reg");
        out.out() << "hsa2\n";
        out.commit();
        run.end(s, StepStatus::ok);
        run.finish(RunStatus::ok);
    }
    CHECK(first != second);
    CHECK(first.parent_path() == second.parent_path());
    CHECK(read_file(first / "run.json") == manifest);
    CHECK(read_file(first / "events.jsonl") == events);
    CHECK(read_file(first / "cudf/up_reg") == "hsa1\n");
    CHECK(read_file(second / "cudf/up_reg") == "hsa2\n");
}

TEST_CASE("run: run's settings are run.cache, run.cache_dir, run.debug and run.log; none changes results",
          "[unit][run]") {
    std::set<std::string> keys;
    for (const core::SettingSpec& spec : settings()) {
        keys.insert(core::key(spec));
        CHECK(spec.module == "run");
        CHECK_FALSE(spec.affects_results);
        CHECK(spec.on_resume == core::OnResume::may_change);
        CHECK_FALSE(spec.help.empty());
        if (spec.name == "debug") {
            CHECK(spec.debug);                   // hidden in the frontend (S1.29)
            CHECK(spec.default_text == "false");
        }
        if (spec.name == "cache") CHECK(spec.default_text == "true");
        if (spec.name == "log" || spec.name == "cache_dir") CHECK(spec.need == core::Need::computed);
    }
    CHECK(keys == std::set<std::string>{"run.cache", "run.cache_dir", "run.debug", "run.log"});
}

}  // namespace fusion::run::test
