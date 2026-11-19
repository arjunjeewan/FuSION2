// Threshold suggestion (section 04, "Threshold suggestion"; S3.15 to S3.19; 04.C6, 04.C7). The
// expected numbers come from an independent Python mirror of the double arithmetic, the one that
// reproduces section 04's EGF table to the last digit.
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace fusion;
using namespace fusion::expression;
using namespace fusion::expression::test;

namespace {

// Eleven genes; their sorted values are -2, -1, -0.5, -0.25, 0, 0, 0.1, 0.2, 0.5, 1, 3.
const std::string eleven = "g1\t3\ng2\t-2\ng3\t0.5\ng4\t0\ng5\t-0.25\ng6\t1\ng7\t0.2\ng8\t-1\ng9\t0\ng10\t0.1\ng11\t-0.5\n";

std::size_t occurrences(const std::string& text, const std::string& part) {
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) ++count;
    return count;
}

}  // namespace

TEST_CASE("expression: the distribution and the ten candidate rows, to the last digit (S3.15, S3.17)",
          "[unit][expression]") {
    Harness h("suggest_thresholds");
    const GeneValues values = parse(h, eleven);
    const Suggestion s = suggest_thresholds(values, Thresholds{1, -1});
    const Distribution& d = s.distribution;
    CHECK(d.n == 11);
    CHECK(d.min == -2.0);
    CHECK(d.max == 3.0);
    CHECK(core::format_number(d.mean) == "0.09545454545454547");
    CHECK(core::format_number(d.sd) == "1.1875212046823618");
    CHECK(d.median == 0.0);
    CHECK(d.mad == 0.5);

    struct Row {
        std::string criterion, up, down;
        std::size_t n_up, n_down;
    };
    const std::vector<Row> expected{
        {"current", "1.0", "-1.0", 2, 2},
        {"quantile 1%", "2.8000000000000007", "-1.9000000000000001", 1, 1},
        {"quantile 2.5%", "2.5", "-1.75", 1, 1},
        {"quantile 5%", "2.0", "-1.5", 1, 1},
        {"MAD k=2", "1.4826", "-1.4826", 1, 1},
        {"MAD k=3", "2.2239", "-2.2239", 1, 0},
        {"SD k=2", "2.4704969548192692", "-2.279587863910178", 1, 0},
        {"SD k=3", "3.658018159501631", "-3.4671090685925394", 0, 0},
        {"SD about 0 k=2", "2.3750424093647236", "-2.3750424093647236", 1, 0},
        {"SD about 0 k=3", "3.562563614047085", "-3.562563614047085", 0, 0}};
    REQUIRE(s.rows.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("row " << expected[i].criterion);
        CHECK(s.rows[i].criterion == expected[i].criterion);
        CHECK(core::format_number(s.rows[i].up) == expected[i].up);
        CHECK(core::format_number(s.rows[i].down) == expected[i].down);
        CHECK(s.rows[i].n_up == expected[i].n_up);
        CHECK(s.rows[i].n_down == expected[i].n_down);
        CHECK(s.rows[i].conflicts == 0);
    }
    // Without current thresholds there are nine rows, starting with the quantiles.
    const Suggestion plain = suggest_thresholds(values, std::nullopt);
    REQUIRE(plain.rows.size() == 9);
    CHECK(plain.rows.front().criterion == "quantile 1%");

    // S3.19: 80 equal bins over [min, max].
    CHECK(s.histogram.lo == -2.0);
    CHECK(s.histogram.width == 0.0625);
    REQUIRE(s.histogram.counts.size() == histogram_bins);
    std::vector<std::uint32_t> counts(histogram_bins, 0);
    for (const auto& [bin, n] : std::vector<std::pair<std::size_t, std::uint32_t>>{
             {0, 1}, {16, 1}, {24, 1}, {28, 1}, {32, 2}, {33, 1}, {35, 1}, {40, 1}, {48, 1}, {79, 1}})
        counts[bin] = n;
    CHECK(s.histogram.counts == counts);
}

TEST_CASE("expression: counts are over genes, and the conflicts column names cuts cudf would stop on (S3.18, 04.C6)",
          "[unit][expression]") {
    Harness h("suggest_thresholds");
    // r1's lines are 5 and 1: at +-2 one is up and one is none, so cudf would stop there.
    const std::string text = "r1\t5\nr1\t1\ng2\t0\ng3\t-3\nr4\t-2.5\nr4\t-2.4\n";
    const Suggestion each = suggest_thresholds(parse(h, text), Thresholds{2, -2});
    CHECK(each.distribution.n == 4);   // genes, not FuSION 1's rows (6)
    CHECK(each.distribution.max == 5.0);
    REQUIRE(each.rows.front().criterion == "current");
    CHECK(each.rows.front().n_up == 1);
    CHECK(each.rows.front().n_down == 2);
    CHECK(each.rows.front().conflicts == 1);
    ValueInput max_abs;
    max_abs.repeats = RepeatRule::max_abs;
    const Suggestion collapsed = suggest_thresholds(parse(h, text, max_abs), Thresholds{2, -2});
    CHECK(std::all_of(collapsed.rows.begin(), collapsed.rows.end(), [](const CandidateRow& r) { return r.conflicts == 0; }));
}

TEST_CASE("expression: equal values give one bin over [v - 0.5, v + 0.5], so the plot is drawn (S3.19, 04.C7)",
          "[unit][expression]") {
    Harness h("suggest_thresholds");
    const Suggestion s = suggest_thresholds(parse(h, "a\t1.5\nb\t1.5\nc\t1.5\n"), std::nullopt);
    CHECK(s.distribution.sd == 0.0);
    CHECK(s.histogram.lo == 1.0);
    CHECK(s.histogram.width == 1.0);
    CHECK(s.histogram.counts == std::vector<std::uint32_t>{3});
    const std::string script = histogram_script(s, std::nullopt, "thresholds.png.tmp");
    CHECK(script.find("set xrange [1.0:2.0]\n") != std::string::npos);
    CHECK(script.find("set boxwidth 0.92\n") != std::string::npos);
    CHECK(script.find("$hist << EOD\n1.5\t3\nEOD\n") != std::string::npos);
}

TEST_CASE("expression: the histogram script keeps S3.19's style, with its data inline (04.C7)", "[unit][expression]") {
    Harness h("suggest_thresholds");
    Suggestion s = suggest_thresholds(parse(h, eleven), Thresholds{1, -1});
    const std::string script = histogram_script(s, Thresholds{1, -1}, "it's.png.tmp");
    CHECK(script.find("set terminal pngcairo enhanced size 980,620 font \"Liberation Sans,13\" background rgb "
                      "\"#fcfcfb\"\n") != std::string::npos);
    CHECK(script.find("set output 'it''s.png.tmp'\n") != std::string::npos);
    CHECK(script.find("11 genes  \xC2\xB7  dotted lines = candidate cuts  \xC2\xB7  solid = in use") !=
          std::string::npos);
    CHECK(script.find("set xrange [-2.0625:3.0625]\n") != std::string::npos);
    CHECK(script.find("set logscale y\n") != std::string::npos);
    CHECK(script.find("set boxwidth 0.0575\n") != std::string::npos);
    CHECK(script.find("$hist << EOD\n-1.96875\t1\n-0.96875\t1\n-0.46875\t1\n-0.21875\t1\n0.03125\t2\n0.09375\t1\n"
                      "0.21875\t1\n0.53125\t1\n1.03125\t1\n2.96875\t1\nEOD\n"
                      "plot $hist u 1:2 w boxes lc rgb \"#dcdbd3\"\n") != std::string::npos);
    // Eighteen distinct candidate cuts after rounding to four decimals, dotted.
    CHECK(occurrences(script, "dt 3 front") == 18);
    CHECK(script.find("set arrow from -3.5626,graph 0 to -3.5626,graph 0.92 nohead lc rgb \"#898781\" lw 1.0 dt 3 "
                      "front\n") != std::string::npos);
    CHECK(script.find("set arrow from 2.4705,graph 0") != std::string::npos);
    // The cuts in use, solid blue and red.
    CHECK(script.find("set arrow from 1.0,graph 0 to 1.0,graph 0.86 nohead lc rgb \"#2a78d6\" lw 2.4 front\n") !=
          std::string::npos);
    CHECK(script.find("set label \"up +1\" at 1.0,graph 0.90 center tc rgb \"#2a78d6\" font \"Liberation Sans,11\" "
                      "front\n") != std::string::npos);
    CHECK(script.find("set label \"down -1\" at -1.0,graph 0.90 center tc rgb \"#e34948\"") != std::string::npos);

    // Cuts that round to one four-decimal value are drawn once.
    s.rows = {CandidateRow{"current", 1.01, -1.01, 0, 0, 0}, CandidateRow{"a", 0.355824, -0.355824, 0, 0, 0},
              CandidateRow{"b", 0.35582, -0.35582, 0, 0, 0}, CandidateRow{"c", 1.0, -1.0, 0, 0, 0}};
    const std::string merged = histogram_script(s, Thresholds{1.01, -1.01});
    CHECK(occurrences(merged, "dt 3 front") == 4);
    CHECK(merged.find("set arrow from 0.3558,graph 0") != std::string::npos);
    CHECK(merged.find("set label \"up +1.01\"") != std::string::npos);
    CHECK(merged.find("set output") == std::string::npos);   // the two-argument form sets none
}

TEST_CASE("expression: save_suggestion writes thresholds.tsv and thresholds.json and prints the report (S3.15)",
          "[unit][expression]") {
    Harness h("suggest_thresholds");
    const fs::path file = h.run_dir() / "inputs" / "lfc.tsv";
    spit(file, eleven);
    ValueInput input;
    input.path = "lfc.tsv";
    const GeneValues values = read_values(file, input, network::Maps{}, h.interner, h.ctx);
    const Suggestion s = suggest_thresholds(values, Thresholds{1, -1});
    save_suggestion(s, values, h.ctx);
    CHECK(slurp(h.step_dir() / "thresholds.tsv") ==
          "criterion\tup\tdown\tn_up\tn_down\tn_de\tconflicts\n"
          "current\t1.0\t-1.0\t2\t2\t4\t0\n"
          "quantile 1%\t2.8000000000000007\t-1.9000000000000001\t1\t1\t2\t0\n"
          "quantile 2.5%\t2.5\t-1.75\t1\t1\t2\t0\n"
          "quantile 5%\t2.0\t-1.5\t1\t1\t2\t0\n"
          "MAD k=2\t1.4826\t-1.4826\t1\t1\t2\t0\n"
          "MAD k=3\t2.2239\t-2.2239\t1\t0\t1\t0\n"
          "SD k=2\t2.4704969548192692\t-2.279587863910178\t1\t0\t1\t0\n"
          "SD k=3\t3.658018159501631\t-3.4671090685925394\t0\t0\t0\t0\n"
          "SD about 0 k=2\t2.3750424093647236\t-2.3750424093647236\t1\t0\t1\t0\n"
          "SD about 0 k=3\t3.562563614047085\t-3.562563614047085\t0\t0\t0\t0\n");
    const core::Json json = core::Json::parse(slurp(h.step_dir() / "thresholds.json"));
    CHECK(json["command"] == "suggest_thresholds");
    CHECK(json["values"]["path"] == "inputs/lfc.tsv");
    CHECK(json["read"]["genes"] == 11);
    CHECK(json["distribution"]["n"] == 11);
    CHECK(json["distribution"]["mad_scaled"] == 0.7413);
    CHECK(json["distribution"]["wider_tail"] == "up");
    CHECK(json["distribution"]["tail_ratio"] == 1.5);
    CHECK(json["rows"].size() == 10);
    CHECK(json["rows"][0] == core::Json::parse(
                                 R"({"criterion": "current", "up": 1.0, "down": -1.0, "n_up": 2, "n_down": 2, "n_de": 4, "conflicts": 0})"));
    CHECK(json["histogram"]["lo"] == -2.0);
    CHECK(json["histogram"]["width"] == 0.0625);
    CHECK(json["histogram"]["counts"].size() == 80);

    const std::vector<std::string> infos = h.sink->infos();
    REQUIRE(infos.size() == 21);
    CHECK(infos[0] == "suggest_thresholds: inputs/lfc.tsv: 11 genes (0 lines skipped, 0 ids repeated)");
    CHECK(infos[2] == "    min / max      -2.0 / 3.0");
    CHECK(infos[3] == "    mean / sd      0.09545454545454547 / 1.1875212046823618");
    CHECK(infos[4] == "    median / MAD   0.0 / 0.5   (MAD*1.4826 = 0.7413)");
    CHECK(infos[5] == "    tail extents   up tail reaches 1.5x further than the other");
    CHECK(infos[8].find("current") != std::string::npos);
    CHECK(infos[8].find("36.36363636363637") != std::string::npos);
    CHECK(infos[20] == "  MAD is the outlier-resistant counterpart of SD; on skewed data it cuts tighter.");
    const auto events = h.events.of("expression.suggested");
    REQUIRE(events.size() == 1);
    CHECK(events[0] == core::Json::parse(R"({"genes": 11, "n_rows": 10, "plot": null})"));

    const core::Json answer = suggestion_answer(s, values);
    CHECK(answer["ok"] == true);
    CHECK(answer["query"] == "suggest_thresholds");
    CHECK(answer["rows"] == json["rows"]);
    CHECK(answer["distribution"] == json["distribution"]);
    CHECK(answer["histogram"] == json["histogram"]);
}
