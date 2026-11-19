// The preview's answer (04.C8, S8.14) and the module's settings table (section 04, "Settings").
#include <set>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace fusion;
using namespace fusion::expression;
using namespace fusion::expression::test;

namespace {

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) out.push_back(line);
    return out;
}

}  // namespace

TEST_CASE("expression: the preview's classes are the lists cudf writes, for random thresholds and Not DE lists (S8.14)",
          "[unit][expression]") {
    core::Rng rng = core::Rng::stream(7, 0);
    // 120 genes with two-decimal values in [-3, 3], as the real files have them, some ids repeated
    // with an equal value so their lines never disagree.
    std::string text;
    for (int g = 0; g < 120; ++g) {
        const auto hundredths = static_cast<long long>(rng.below(601)) - 300;
        const std::string value = core::format_number(static_cast<double>(hundredths) / 100.0);
        text += "hsa" + std::to_string(g) + "\t" + value + "\n";
        if (g % 17 == 0) text += "hsa" + std::to_string(g) + "\t" + value + "\n";
    }
    for (int trial = 0; trial < 40; ++trial) {
        INFO("trial " << trial);
        Harness h;
        const GeneValues values = parse(h, text);
        ClassifyRequest r;
        r.thresholds = Thresholds{static_cast<double>(rng.below(300)) / 100.0 + 0.01,
                                  -static_cast<double>(rng.below(300)) / 100.0 - 0.01};
        std::vector<std::string> not_de_ids;
        for (int k = 0; k < 10; ++k) not_de_ids.push_back("hsa" + std::to_string(rng.below(130)));
        const std::vector<core::GeneId> not_de = intern(h.interner, not_de_ids);
        r.not_de = not_de;
        const auto outcome = classify(values, r, h.ctx);
        save_observations(outcome.value, values, ListNames{}, h.interner, h.ctx);
        const core::Json answer = cudf_answer(values, outcome.value, h.interner);

        std::vector<std::string> up, down;
        const std::set<std::string> overridden(not_de_ids.begin(), not_de_ids.end());
        for (const core::Json& gene : answer["genes"]) {
            const std::string id = gene["id"];
            if (gene["class"] == "up") up.push_back(id);
            if (gene["class"] == "down") down.push_back(id);
            CHECK(gene.contains("not_de") == overridden.contains(id));
        }
        CHECK(up == lines_of(slurp(h.step_dir() / "up_reg")));
        CHECK(down == lines_of(slurp(h.step_dir() / "down_reg")));
        CHECK(answer["n_up"] == up.size());
        CHECK(answer["n_down"] == down.size());
        CHECK(answer["n_up"].get<std::size_t>() + answer["n_down"].get<std::size_t>() +
                  answer["n_none"].get<std::size_t>() ==
              values.size());
        CHECK(answer["genes"].size() == values.size());
    }
}

TEST_CASE("expression: cudf's answer carries cudf.json's fields, plus ok, query and genes (04.C8)", "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa1000\t0.12\nhsa2354\t1.04\nhsa3725\t2.41\n");
    ClassifyRequest r;
    r.thresholds = Thresholds{1.01, -1.01};
    const std::vector<core::GeneId> not_de = intern(h.interner, {"hsa2354"});
    r.not_de = not_de;
    r.not_de_file = fs::path("sessions/7f3c/overrides/not_de.txt");
    const core::Json answer = cudf_answer(values, classify(values, r, h.ctx).value, h.interner);
    CHECK(answer["ok"] == true);
    CHECK(answer["query"] == "cudf");
    CHECK(answer["values"]["layout"] == "pairs");
    CHECK(answer["read"]["lines"] == 3);
    CHECK(answer["thresholds"] == core::Json::parse(R"({"up": 1.01, "down": -1.01})"));
    CHECK(answer["not_de"] == core::Json::parse(R"({"path": "sessions/7f3c/overrides/not_de.txt",
                                                    "removed": ["hsa2354"], "unchanged": [], "unknown": []})"));
    CHECK(answer["n_up"] == 1);
    CHECK(answer["n_down"] == 0);
    CHECK(answer["n_none"] == 2);
    CHECK(answer["genes"] == core::Json::parse(R"([{"id": "hsa1000", "value": 0.12, "class": "none"},
                                                   {"id": "hsa2354", "value": 1.04, "class": "none", "not_de": true},
                                                   {"id": "hsa3725", "value": 2.41, "class": "up"}])"));
    // Bytes that are not UTF-8 stay in the id and are replaced only in JSON (04 "Bytes").
    Harness h2;
    const GeneValues latin1 = parse(h2, "caf\xE9\t2\n");
    ClassifyRequest r2;
    r2.thresholds = Thresholds{1, -1};
    const core::Json odd = cudf_answer(latin1, classify(latin1, r2, h2.ctx).value, h2.interner);
    CHECK(odd["genes"][0]["id"] == "caf\xEF\xBF\xBD");
    CHECK(odd.dump().find("caf") != std::string::npos);
}

TEST_CASE("expression: the settings table names every argument of cudf and suggest_thresholds (1.16)",
          "[unit][expression]") {
    std::set<std::string> names;
    for (const core::SettingSpec& spec : settings()) {
        CHECK(spec.module == "expression");
        CHECK_FALSE(spec.help.empty());
        names.insert(std::string(spec.name));
    }
    CHECK(names == std::set<std::string>{"values", "up", "down", "up_out", "down_out", "id_column", "value_column",
                                         "id_map", "repeats", "not_de", "current_up", "current_down", "plot"});
    for (const core::SettingSpec& spec : settings()) {
        if (spec.name == "repeats") {
            CHECK(spec.default_text == "classify_each");
            REQUIRE(spec.choices.size() == 2);
            CHECK(spec.choices[1] == "max_abs");
        }
        if (spec.name == "up" || spec.name == "down" || spec.name == "values") CHECK(spec.need == core::Need::required);
        if (spec.name == "up_out" || spec.name == "plot") CHECK_FALSE(spec.affects_results);
    }
}
