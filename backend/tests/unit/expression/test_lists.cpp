// The saved step (section 04, "Classification" and "Observations"; S3.09 to S3.13, decision 0019),
// checked against FuSION 1's own lists for the toy cases that Booking 1 regenerated.
#include <sstream>

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

std::vector<std::string> words(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    for (std::string word; in >> word;) out.push_back(word);
    return out;
}

// The value of `let $<name> <value>` in a batch file.
std::string let_value(const fs::path& batch, std::string_view name) {
    std::istringstream in(slurp(batch));
    for (std::string line; std::getline(in, line);) {
        const std::vector<std::string> w = words(line);
        if (w.size() >= 3 && w[0] == "let" && w[1] == "$" + std::string(name)) return w[2];
    }
    return {};
}

}  // namespace

TEST_CASE("expression: cudf's lists are byte-identical to FuSION 1's on every toy case (S3.09)",
          "[unit][expression]") {
    const core::Json cases = core::Json::parse(slurp(tests_dir() / "reference" / "cases.json"));
    std::size_t checked = 0;
    for (const core::Json& c : cases["cases"]) {
        if (c["tier"] != "T1") continue;
        const std::string id = c["id"];
        INFO("case " << id);
        const fs::path batch = tests_dir() / "fixtures" / c["batch"].get<std::string>();
        const fs::path dir = tests_dir() / "reference" / "synthetic" / id;
        const fs::path values_file = dir / "inputs" / fs::path(let_value(batch, "LOG_FOLD_CHANGE_FILE")).filename();
        const double up = core::parse_decimal(let_value(batch, "UP_REG_THRESH")).value;
        const double down = core::parse_decimal(let_value(batch, "DOWN_REG_THRESH")).value;
        Harness h;
        ValueInput input;
        input.path = values_file;
        const GeneValues values = read_values(values_file, input, network::Maps{}, h.interner, h.ctx);
        const auto outcome = classify(values, request(up, down), h.ctx);
        save_observations(outcome.value, values, ListNames{}, h.interner, h.ctx);
        CHECK(slurp(h.step_dir() / "up_reg") == slurp(dir / c["fusion1"]["up"].get<std::string>()));
        CHECK(slurp(h.step_dir() / "down_reg") == slurp(dir / c["fusion1"]["down"].get<std::string>()));
        ++checked;
    }
    CHECK(checked == 12);
}

TEST_CASE("expression: save_observations writes the lists, cudf.json and S3.11's console lines",
          "[unit][expression]") {
    Harness h;
    const fs::path file = h.run_dir() / "inputs" / "lfc.tsv";
    spit(file, "hsa1\t2\r\nhsa2\t-2\r\nhsa3\tNA\r\nhsa4\t1.5\r\nhsa2\t-3\r\n");
    ValueInput input;
    input.path = "lfc.tsv";
    const GeneValues values = read_values(file, input, network::Maps{}, h.interner, h.ctx);
    ClassifyRequest r = request(1.01, -1.01);
    const std::vector<core::GeneId> not_de = intern(h.interner, {"hsa4", "hsa9"});
    r.not_de = not_de;
    r.not_de_file = fs::path("overrides/not_de.txt");
    const auto outcome = classify(values, r, h.ctx);
    save_observations(outcome.value, values, ListNames{}, h.interner, h.ctx);

    CHECK(slurp(h.step_dir() / "up_reg") == "hsa1\n");
    CHECK(slurp(h.step_dir() / "down_reg") == "hsa2\n");
    const core::Json json = core::Json::parse(slurp(h.step_dir() / "cudf.json"));
    CHECK(json["command"] == "cudf");
    CHECK(json["values"]["path"] == "inputs/lfc.tsv");
    CHECK(json["values"]["bytes"] == fs::file_size(file));
    CHECK(json["values"]["sha256"] == core::Sha256::of(slurp(file)).hex());
    CHECK(json["values"]["layout"] == "pairs");
    CHECK(json["values"]["separator"] == "whitespace");
    CHECK(json["values"]["id_column"].is_null());
    CHECK(json["values"]["value_column"].is_null());
    CHECK(json["values"]["id_map"].is_null());
    CHECK(json["values"]["repeats"] == "classify_each");
    CHECK(json["read"]["lines"] == 5);
    CHECK(json["read"]["parsed"] == 4);
    CHECK(json["read"]["genes"] == 3);
    CHECK(json["read"]["skipped"] ==
          core::Json::parse(R"([{"line": 3, "reason": "not_a_number", "text": "hsa3\tNA"}])"));
    CHECK(json["read"]["repeated"] == core::Json::parse(R"([{"id": "hsa2", "lines": [2, 5], "values": [-2.0, -3.0]}])"));
    CHECK(json["read"]["unmapped"] == core::Json::parse(R"({"lines": 0, "ids": []})"));
    CHECK(json["thresholds"] == core::Json::parse(R"({"up": 1.01, "down": -1.01})"));
    CHECK(json["not_de"] == core::Json::parse(
                                R"({"path": "overrides/not_de.txt", "removed": ["hsa4"], "unchanged": [], "unknown": ["hsa9"]})"));
    CHECK(json["n_up"] == 1);
    CHECK(json["n_down"] == 1);
    CHECK(json["n_none"] == 1);
    CHECK(json["lists"] == core::Json::parse(R"({"up": "up_reg", "down": "down_reg"})"));

    const std::vector<std::string> infos = h.sink->infos();
    REQUIRE(infos.size() == 3);
    CHECK(infos[0] == "cudf: inputs/lfc.tsv: 5 lines, 3 genes, 1 skipped: line 3 (not_a_number), 1 repeated: "
                      "hsa2 (lines 2, 5)");
    CHECK(infos[1] == "cudf: up (>= 1.01) 1 gene, down (<= -1.01) 1 gene -> cudf/up_reg, cudf/down_reg");
    CHECK(infos[2] == "cudf: Not DE: 1 removed, 0 already none, 1 not in the values");
    // No debug file without the debug option (S1.29).
    CHECK_FALSE(fs::exists(h.run_dir() / "debug"));
}

TEST_CASE("expression: the debug option writes the values as read to debug/cudf/values.tsv (S1.29)",
          "[unit][expression]") {
    Harness h("cudf", true);
    const GeneValues values = parse(h, "hsa56999\t1.69\nhsa5806\t9.82\nhsa5806\t9.82\nhsa5806\t1.95\n");
    save_observations(classify(values, request(1.45, -1.45), h.ctx).value, values, ListNames{}, h.interner, h.ctx);
    CHECK(slurp(h.run_dir() / "debug" / "cudf" / "values.tsv") ==
          "id\tvalue\tlines\nhsa56999\t1.69\t1\nhsa5806\t9.82\t2,3,4\n");
}

TEST_CASE("expression: list names land in the step directory, and an absolute one there too (2B.3, 04.C11)",
          "[unit][expression]") {
    Harness h;
    const GeneValues values = parse(h, "hsa1\t2\nhsa2\t-2\n");
    const Observations obs = classify(values, request(1, -1), h.ctx).value;
    const fs::path elsewhere = h.run_dir() / "elsewhere";
    fs::create_directories(elsewhere);
    ListNames names;
    names.up = core::to_utf8(elsewhere / "up_list");
    names.down = "lists/down.txt";
    save_observations(obs, values, names, h.interner, h.ctx);
    CHECK(slurp(h.step_dir() / "up_list") == "hsa1\n");
    CHECK(slurp(elsewhere / "up_list") == "hsa1\n");
    CHECK(slurp(h.step_dir() / "lists" / "down.txt") == "hsa2\n");
    const core::Json json = core::Json::parse(slurp(h.step_dir() / "cudf.json"));
    CHECK(json["lists"]["up"] == "up_list");
    CHECK(json["lists"]["down"] == "lists/down.txt");
    CHECK(h.sink->infos()[1].find("-> cudf/up_list (also " + core::to_utf8(elsewhere / "up_list") + "), cudf/lists/down.txt") !=
          std::string::npos);
}

TEST_CASE("expression: lists that clash with each other or with the input are refused, nothing written (S3.10)",
          "[unit][expression]") {
    Harness h;
    const fs::path file = h.run_dir() / "lfc.tsv";
    const std::string bytes = "hsa1\t2\nhsa2\t-2\n";
    spit(file, bytes);
    ValueInput input;
    input.path = file;
    const GeneValues values = read_values(file, input, network::Maps{}, h.interner, h.ctx);
    const Observations obs = classify(values, request(1, -1), h.ctx).value;
    const auto save = [&](std::string up, std::string down) {
        ListNames names;
        names.up = std::move(up);
        names.down = std::move(down);
        return failure_code([&] { save_observations(obs, values, names, h.interner, h.ctx); });
    };
    // FuSION 1 truncated both outputs first: one path for both lists mixed their bytes, and a list
    // over the input emptied it before it was read (S3.10).
    CHECK(save("same", "same") == "output.clash");
    CHECK(save("dir/x", "dir/../dir/x") == "output.clash");
    const fs::path absolute = h.run_dir() / "out";
    fs::create_directories(absolute);
    CHECK(save(core::to_utf8(absolute / "a" / ".." / "list"), core::to_utf8(absolute / "list")) == "output.clash");
    CHECK(save(core::to_utf8(file), "down") == "output.clash");
    CHECK(slurp(file) == bytes);
    CHECK_FALSE(fs::exists(h.step_dir()));
    CHECK_FALSE(fs::exists(absolute / "list"));
}

TEST_CASE("expression: read_id_list takes whitespace-separated ids, once each, through the id rule (S3.12)",
          "[unit][expression]") {
    Harness h;
    const fs::path file = h.run_dir() / "ids.txt";
    spit(file, "\xEF\xBB\xBFhsa1 hsa2\r\n\r\n\thsa:3  hsa1\fhsa4\n");
    CHECK(ids_of(read_id_list(file, network::IdMap::none(), h.interner), h.interner) ==
          std::vector<std::string>{"hsa1", "hsa2", "hsa3", "hsa4"});
    const fs::path empty = h.run_dir() / "empty.txt";
    spit(empty, "");
    CHECK(read_id_list(empty, network::IdMap::none(), h.interner).empty());
    // FuSION 1 printed a line and went on with an empty list (GraphManagerNew.cc:18693-18697).
    CHECK(failure_code([&] { read_id_list(h.run_dir() / "nope.txt", network::IdMap::none(), h.interner); }) ==
          "input.missing");
}

TEST_CASE("expression: load_lists reads the saved lists back; an id in both is up (S5.12, 0019)",
          "[unit][expression]") {
    Harness h;
    SECTION("an overlap counts as up, with a warning") {
        const fs::path up = h.run_dir() / "up_reg", down = h.run_dir() / "down_reg";
        spit(up, "hsa1\nhsa2\n");
        spit(down, "hsa2\nhsa3\nhsa3\n");
        const Observations obs = load_lists(up, down, network::IdMap::none(), h.interner, h.ctx);
        CHECK(ids_of(obs.up(), h.interner) == std::vector<std::string>{"hsa1", "hsa2"});
        CHECK(ids_of(obs.down(), h.interner) == std::vector<std::string>{"hsa3"});
        CHECK(obs.regulation(h.interner.id("hsa2")) == Regulation::up);
        CHECK_FALSE(obs.thresholds());
        const auto warnings = h.sink->warnings("lists.overlap");
        REQUIRE(warnings.size() == 1);
        CHECK(warnings[0].find("each counts as up: hsa2") != std::string::npos);
    }
    SECTION("save then load gives the same observations") {
        const GeneValues values = parse(h, "hsa5\t3\nhsa6\t-3\nhsa7\t2\nhsa8\t0\n");
        const Observations saved = classify(values, request(1, -1), h.ctx).value;
        save_observations(saved, values, ListNames{}, h.interner, h.ctx);
        const Observations loaded = load_lists(h.step_dir() / "up_reg", h.step_dir() / "down_reg",
                                               network::IdMap::none(), h.interner, h.ctx);
        CHECK(ids_of(loaded.up(), h.interner) == ids_of(saved.up(), h.interner));
        CHECK(ids_of(loaded.down(), h.interner) == ids_of(saved.down(), h.interner));
        CHECK(h.sink->warnings("lists.overlap").empty());
    }
}
