// Helpers for the enrichment module's unit tests: a context whose log, events and files are
// recorded in memory or in a scratch folder, and readers for the gseapy fixtures.
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/enrichment/enrichment.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

namespace enrichment_test {

namespace core = fusion::core;
namespace enrichment = fusion::enrichment;

inline std::filesystem::path tests_dir() { return FUSION_TEST_SOURCE_DIR; }
inline std::filesystem::path gsea_fixtures() { return tests_dir() / "fixtures" / "gsea"; }
inline std::filesystem::path data_dir() { return tests_dir() / "unit" / "enrichment" / "data"; }

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

inline std::vector<std::string> split(std::string_view text, char sep) {
    std::vector<std::string> parts;
    for (std::size_t start = 0;;) {
        const std::size_t at = text.find(sep, start);
        parts.emplace_back(text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
        if (at == std::string_view::npos) return parts;
        start = at + 1;
    }
}

inline std::vector<std::string> lines(std::string_view text) {
    std::vector<std::string> out = split(text, '\n');
    if (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

inline double number(std::string_view text) {   // Python repr text back to its double
    const core::Decimal d = core::parse_decimal(text);
    if (d.status != core::DecimalStatus::ok) throw std::runtime_error("not a number: " + std::string(text));
    return d.value;
}

struct LogLine {
    core::Level level;
    std::string code, text;
};

class CaptureSink : public core::LogSink {
public:
    void write(core::Level level, std::string_view code, std::string_view text) override {
        lines.push_back({level, std::string(code), std::string(text)});
    }
    std::vector<LogLine> lines;
};

class RecordingEvents : public core::Events {
public:
    void emit(std::string_view type, core::Json fields) override { events.emplace_back(std::string(type), std::move(fields)); }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
    std::vector<std::pair<std::string, core::Json>> events;
};

class NoJournal : public core::Journal {
public:
    void append(const core::Json&) override {}
};

// A fresh folder under the system's temporary folder, removed with everything in it.
class ScratchDir {
public:
    ScratchDir() {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("fusion-enrichment-test-" + std::to_string(stamp) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(path_ / "gsea");
    }
    ~ScratchDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// core::Files over a step folder; `fail_on_create` makes the n-th create() throw (08.C13's test).
class StepFiles : public core::Files {
public:
    explicit StepFiles(std::filesystem::path step_dir) : dir_(std::move(step_dir)) {}
    std::filesystem::path input(const std::filesystem::path& as_named) override { return as_named; }
    core::OutputFile create(std::string_view name) override {
        if (++created_ == fail_on_create) throw core::Failure("io.write", "injected failure");
        return core::OutputFile(dir_ / core::path_from_utf8(name));
    }
    core::OutputFile create_debug(std::string_view name) override { return core::OutputFile(dir_ / core::path_from_utf8(name)); }
    core::Journal& journal(std::string_view) override { return journal_; }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    std::filesystem::path step_dir() const override { return dir_; }
    int fail_on_create = 0;

private:
    std::filesystem::path dir_;
    int created_ = 0;
    NoJournal journal_;
};

// What a step receives (core::Context), with everything recorded.
struct Harness {
    Harness() : files(scratch.path() / "gsea") { log.add_sink(sink); }
    core::Context context(core::StopToken stop = {}) { return core::Context{log, events, stop, settings, files, nullptr, "gsea"}; }
    std::vector<LogLine> warnings(std::string_view code) const {
        std::vector<LogLine> out;
        for (const LogLine& l : sink->lines)
            if (l.level == core::Level::warning && l.code == code) out.push_back(l);
        return out;
    }
    std::vector<std::string> info() const {
        std::vector<std::string> out;
        for (const LogLine& l : sink->lines)
            if (l.level == core::Level::info) out.push_back(l.text);
        return out;
    }
    ScratchDir scratch;
    std::shared_ptr<CaptureSink> sink = std::make_shared<CaptureSink>();
    core::Log log{false};
    RecordingEvents events;
    core::Settings settings;
    StepFiles files;
};

// Per-gene values through section 04's one reader (S7.04), from text such as "g1\t0.5\n".
inline fusion::expression::GeneValues read_values(std::string_view text, core::Interner& ids, Harness& h,
                                                  const std::string& name = "values.tsv") {
    fusion::expression::ValueInput input;
    input.path = name;
    core::Context ctx = h.context();
    return fusion::expression::parse_values(text, input, fusion::network::IdMap::none(), false, ids, ctx);
}

// A candidate made by hand, as make_candidates would make it from a file with these genes:
// `genes` holds those the interner knows, `gene_text` all of them (S7.08).
inline enrichment::Candidate candidate(const std::string& id, const std::vector<std::string>& genes,
                                       const core::Interner& ids, const std::string& title = {}) {
    enrichment::Candidate c;
    c.entry = fusion::network::ListLine{"/kgml/" + id + ".xml", "/kgml/" + id + ".xml"};
    c.id = id;
    c.title = title.empty() ? id : title;
    c.gene_text = genes;
    std::sort(c.gene_text.begin(), c.gene_text.end(),
              [](const std::string& a, const std::string& b) { return core::stable_compare(a, b) < 0; });
    c.gene_text.erase(std::unique(c.gene_text.begin(), c.gene_text.end()), c.gene_text.end());
    for (const std::string& g : c.gene_text)
        if (const auto handle = ids.find_id(g)) c.genes.push_back(*handle);
    std::sort(c.genes.begin(), c.genes.end(), [](core::GeneId a, core::GeneId b) { return a.value < b.value; });
    return c;
}

// One gseapy fixture (backend/tests/fixtures/gsea/<name>): its ranking and sets, as 2.0 reads them.
struct GmtSet {
    std::string id, kind;
    std::vector<std::string> genes;
};
inline std::vector<GmtSet> read_gmt(const std::filesystem::path& path) {
    std::vector<GmtSet> sets;
    for (const std::string& line : lines(read_file(path))) {
        std::vector<std::string> cells = split(line, '\t');
        GmtSet s{cells.at(0), cells.at(1), {}};
        s.genes.assign(cells.begin() + 2, cells.end());
        sets.push_back(std::move(s));
    }
    return sets;
}

struct Fixture {
    std::string name;
    core::Interner ids;
    enrichment::Ranking ranking;
    std::vector<enrichment::Candidate> candidates;   // sets.gmt order
};
inline std::unique_ptr<Fixture> load_fixture(const std::string& name, Harness& h) {
    auto f = std::make_unique<Fixture>();
    f->name = name;
    const std::filesystem::path dir = gsea_fixtures() / name;
    const auto values = read_values(read_file(dir / "values.tsv"), f->ids, h);
    f->ranking = enrichment::rank(values, f->ids);
    for (const GmtSet& s : read_gmt(dir / "sets.gmt")) f->candidates.push_back(candidate(s.id, s.genes, f->ids));
    return f;
}

}  // namespace enrichment_test
