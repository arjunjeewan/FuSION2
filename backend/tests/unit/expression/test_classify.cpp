// Classification, cudf's work (section 04, "Classification"; S3.02, S3.06 to S3.08; 04.C3 to 04.C5).
#include <cmath>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace fusion;
using namespace fusion::expression;
using namespace fusion::expression::test;

namespace {

ClassifyRequest request(double up, double down) {
    ClassifyRequest r;
    r.thresholds = Thresholds{up, down};
    return r;
}

}  // namespace

TEST_CASE("expression: up is >= up and down is <= down, both inclusive, on doubles (S3.06)", "[unit][expression]") {
    const Thresholds t{1.01, -1.01};
    CHECK(regulation_of(1.01, t) == Regulation::up);
    CHECK(regulation_of(5.19, t) == Regulation::up);
    CHECK(regulation_of(-1.01, t) == Regulation::down);
    CHECK(regulation_of(1.0, t) == Regulation::none);
    CHECK(regulation_of(0.0, t) == Regulation::none);
    // FuSION 1 compared 32-bit floats (Linux/src/main.cc:2632, 2681), so 1.0099999999 counted as up at
    // 1.01; with doubles on both sides it does not (S3.06, an expected difference under 0023).
    CHECK(regulation_of(1.0099999999, t) == Regulation::none);

    // SOX9 (hsa6662, 1.01) is up at 1.01: the value and the threshold parse to one double.
    Harness h;
    const GeneValues values = parse(h, "hsa6662\t1.01\nhsa1\t1.0099999999\nhsa2\t-1.01\n");
    const auto outcome = classify(values, request(core::parse_decimal("1.01").value, -1.01), h.ctx);
    CHECK(ids_of(outcome.value.up(), h.interner) == std::vector<std::string>{"hsa6662"});
    CHECK(ids_of(outcome.value.down(), h.interner) == std::vector<std::string>{"hsa2"});
}

TEST_CASE("expression: thresholds must be finite numbers with up above down (S3.02, S3.07)", "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa1\t0\nhsa2\t1\n");
    // FuSION 1's GUI script set both thresholds to 0.0 and every 0.00 gene went into both lists.
    CHECK(failure_code([&] { classify(values, request(0.0, 0.0), h.ctx); }) == "batch.bad_value");
    CHECK(failure_code([&] { classify(values, request(-1.0, 1.0), h.ctx); }) == "batch.bad_value");
    CHECK(failure_code([&] { classify(values, request(std::nan(""), -1.0), h.ctx); }) == "batch.bad_value");
    CHECK(failure_code([&] { classify(values, request(std::numeric_limits<double>::infinity(), -1.0), h.ctx); }) ==
          "batch.bad_value");
    CHECK(failure_code([&] { classify(values, request(1.0, -std::numeric_limits<double>::infinity()), h.ctx); }) ==
          "batch.bad_value");
    CHECK(h.events.of("expression.classified").empty());
}

TEST_CASE("expression: each gene is written once, in input order, and lines in two classes stop cudf (S3.08)",
          "[unit][expression]") {
    Harness h;
    SECTION("repeats in one class are one gene") {
        const GeneValues values = parse(h, "hsa1\t2\nhsa2\t-2\nhsa1\t3\nhsa3\t0\nhsa2\t-5\n");
        const auto outcome = classify(values, request(1, -1), h.ctx);
        CHECK(ids_of(outcome.value.up(), h.interner) == std::vector<std::string>{"hsa1"});
        CHECK(ids_of(outcome.value.down(), h.interner) == std::vector<std::string>{"hsa2"});
        CHECK_FALSE(outcome.report);
    }
    // The TNFa file's hsa5806 (9.82, 9.82, 1.95) at +-2: FuSION 1 wrote it into the up list twice.
    const std::string tnfa = "hsa1\t3\nhsa3280\t-1.48\nhsa3280\t-1.48\nhsa5806\t9.82\nhsa5806\t9.82\nhsa5806\t1.95\n";
    SECTION("a repeated id in two classes is observations.conflict, naming its lines and values") {
        const GeneValues values = parse(h, tnfa);
        CHECK(failure_code([&] { classify(values, request(2, -2), h.ctx); }) == "observations.conflict");
        CHECK(failure_message([&] { classify(values, request(2, -2), h.ctx); }) ==
              "hsa5806 is up on lines 4, 5 (9.82, 9.82) and none on line 6 (1.95) (up >= 2.0, down <= -2.0)");
        try {
            classify(values, request(2, -2), h.ctx);
        } catch (const core::Failure& failure) {
            CHECK(failure.where().line == 4);
        }
        // At +-1.45 the lines agree: hsa5806 up once, hsa3280 down once (33 and 6 in the real file).
        const auto outcome = classify(values, request(1.45, -1.45), h.ctx);
        CHECK(ids_of(outcome.value.up(), h.interner) == std::vector<std::string>{"hsa1", "hsa5806"});
        CHECK(ids_of(outcome.value.down(), h.interner) == std::vector<std::string>{"hsa3280"});
    }
    SECTION("Not DE settles the conflict (04.C4)") {
        const GeneValues values = parse(h, tnfa);
        ClassifyRequest r = request(2, -2);
        const std::vector<core::GeneId> not_de = intern(h.interner, {"hsa5806"});
        r.not_de = not_de;
        const auto outcome = classify(values, r, h.ctx);
        CHECK(ids_of(outcome.value.up(), h.interner) == std::vector<std::string>{"hsa1"});
        CHECK(ids_of(outcome.value.not_de_removed(), h.interner) == std::vector<std::string>{"hsa5806"});
    }
    SECTION("repeats=max_abs classifies the largest magnitude only (04.C3)") {
        ValueInput input;
        input.repeats = RepeatRule::max_abs;
        const GeneValues values = parse(h, tnfa, input);
        const auto outcome = classify(values, request(2, -2), h.ctx);
        CHECK(ids_of(outcome.value.up(), h.interner) == std::vector<std::string>{"hsa1", "hsa5806"});
        CHECK(outcome.value.down().empty());
    }
}

TEST_CASE("expression: Not DE sets genes to none before classification and reports what it did (04.C4)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa2354\t1.04\nhsa1\t0.1\nhsa2\t-2\nhsa3\t1.5\n");
    ClassifyRequest r = request(1.01, -1.01);
    const std::vector<core::GeneId> not_de = intern(h.interner, {"hsa2354", "hsa1", "hsa999", "hsa2354"});
    r.not_de = not_de;
    r.interner = &h.interner;
    r.not_de_file = fs::path("overrides/not_de.txt");
    const auto outcome = classify(values, r, h.ctx);
    const Observations& obs = outcome.value;
    CHECK(ids_of(obs.up(), h.interner) == std::vector<std::string>{"hsa3"});
    CHECK(ids_of(obs.down(), h.interner) == std::vector<std::string>{"hsa2"});
    CHECK(obs.regulation(h.interner.id("hsa2354")) == Regulation::none);
    CHECK(ids_of(obs.not_de_removed(), h.interner) == std::vector<std::string>{"hsa2354"});
    CHECK(cudf_answer(values, obs, h.interner)["not_de"] ==
          core::Json::parse(R"({"path": "overrides/not_de.txt", "removed": ["hsa2354"], "unchanged": ["hsa1"],
                                "unknown": ["hsa999"]})"));
    const auto warnings = h.sink->warnings("not_de.unknown_ids");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0] == "1 Not DE id is not in : hsa999");
    const auto events = h.events.of("expression.classified");
    REQUIRE(events.size() == 1);
    CHECK(events[0]["n_up"] == 1);
    CHECK(events[0]["n_down"] == 1);
    CHECK(events[0]["n_not_de_removed"] == 1);
    CHECK(events[0]["thresholds"]["up"] == 1.01);
    CHECK(events[0]["thresholds"]["down"] == -1.01);
}

TEST_CASE("expression: no gene past the thresholds is the reported result observations.none (04.C5)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa1\t5.19\nhsa2\t-1.56\nhsa3\t0\n");
    const auto outcome = classify(values, request(5.2, -5.2), h.ctx);
    REQUIRE(outcome.report);
    CHECK(outcome.report->code == "observations.none");
    CHECK(outcome.report->message == "no gene passes up >= 5.2 or down <= -5.2");
    CHECK(outcome.value.empty());
    // The two lists are still written, empty (S3.09).
    save_observations(outcome.value, values, ListNames{}, h.interner, h.ctx);
    CHECK(fs::file_size(h.step_dir() / "up_reg") == 0);
    CHECK(fs::file_size(h.step_dir() / "down_reg") == 0);
    CHECK_FALSE(classify(values, request(5.19, -5.2), h.ctx).report);
}

TEST_CASE("expression: Observations gives the lists, the observed ids and each id's regulation",
          "[unit][expression]") {
    const Observations empty;
    CHECK(empty.empty());
    CHECK(empty.up().empty());
    CHECK_FALSE(empty.thresholds());
    CHECK(empty.regulation(core::GeneId{7}) == Regulation::none);

    Harness h;
    const GeneValues values = parse(h, "hsa1\t-2\nhsa2\t2\nhsa3\t0\nhsa4\t3\n");
    const Observations obs = classify(values, request(1, -1), h.ctx).value;
    CHECK(ids_of(obs.up(), h.interner) == std::vector<std::string>{"hsa2", "hsa4"});
    CHECK(ids_of(obs.down(), h.interner) == std::vector<std::string>{"hsa1"});
    CHECK(ids_of(obs.observed(), h.interner) == std::vector<std::string>{"hsa2", "hsa4", "hsa1"});
    CHECK(obs.regulation(h.interner.id("hsa1")) == Regulation::down);
    CHECK(obs.regulation(h.interner.id("hsa4")) == Regulation::up);
    CHECK(obs.regulation(h.interner.id("hsa3")) == Regulation::none);
    REQUIRE(obs.thresholds());
    CHECK(obs.thresholds()->up == 1.0);
    CHECK(obs.thresholds()->down == -1.0);
}
