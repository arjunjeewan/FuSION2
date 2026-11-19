// Shared fixtures for the corridor tests: a recording core::Context, a Files that writes into a fresh
// temporary directory, and synthetic networks built through section 02's NetworkBuilder (S8.31: no
// KEGG files).
#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "fusion/corridor/corridor.hpp"

namespace corridor_test {

namespace core = fusion::core;
namespace network = fusion::network;
namespace expression = fusion::expression;
namespace corridor = fusion::corridor;

// ---------------------------------------------------------------- the context

struct RecordingSink final : core::LogSink {
    struct Line {
        core::Level level;
        std::string code, text;
    };
    std::vector<Line> lines;
    void write(core::Level level, std::string_view code, std::string_view text) override {
        lines.push_back({level, std::string(code), std::string(text)});
    }
};

struct RecordingEvents final : core::Events {
    std::vector<std::pair<std::string, core::Json>> events;
    void emit(std::string_view type, core::Json fields) override { events.emplace_back(type, std::move(fields)); }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
};

class NoJournal final : public core::Journal {
public:
    void append(const core::Json&) override {}
};

// Writes each file into a fresh directory under the system's temporary directory, removed afterwards.
class TempFiles final : public core::Files {
public:
    TempFiles() {
        static std::atomic<unsigned> counter{0};
        dir_ = std::filesystem::temp_directory_path() /
               ("fusion-corridor-test-" + std::to_string(std::random_device{}()) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(dir_);
    }
    ~TempFiles() override {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }
    TempFiles(const TempFiles&) = delete;
    TempFiles& operator=(const TempFiles&) = delete;

    std::filesystem::path input(const std::filesystem::path& as_named) override { return as_named; }
    core::OutputFile create(std::string_view name) override {
        created.emplace_back(name);
        return core::OutputFile(dir_ / name);
    }
    core::OutputFile create_debug(std::string_view name) override {
        created.push_back("debug/" + std::string(name));
        std::filesystem::create_directories(dir_ / "debug");
        return core::OutputFile(dir_ / "debug" / name);
    }
    core::Journal& journal(std::string_view) override { return journal_; }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    std::filesystem::path step_dir() const override { return dir_; }

    const std::filesystem::path& dir() const noexcept { return dir_; }
    std::string read(std::string_view name) const {
        std::ifstream in(dir_ / name, std::ios::binary);
        if (!in) throw std::runtime_error("no file " + std::string(name));
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
    void write(std::string_view name, std::string_view text) const {
        std::ofstream out(dir_ / name, std::ios::binary);
        out << text;
    }

    std::vector<std::string> created;   // in creation order

private:
    std::filesystem::path dir_;
    NoJournal journal_;
};

// One step's context, recording every log line and event.
struct Harness {
    explicit Harness(bool debug = false) : log(debug), sink(std::make_shared<RecordingSink>()) {
        log.add_sink(sink);
    }
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    std::vector<std::string> warning_codes() const {
        std::vector<std::string> codes;
        for (const auto& l : sink->lines)
            if (l.level == core::Level::warning) codes.push_back(l.code);
        return codes;
    }
    std::vector<std::string> warnings(std::string_view code) const {
        std::vector<std::string> texts;
        for (const auto& l : sink->lines)
            if (l.level == core::Level::warning && l.code == code) texts.push_back(l.text);
        return texts;
    }
    std::vector<std::string> infos() const {
        std::vector<std::string> texts;
        for (const auto& l : sink->lines)
            if (l.level == core::Level::info) texts.push_back(l.text);
        return texts;
    }
    std::vector<core::Json> events_of(std::string_view type) const {
        std::vector<core::Json> out;
        for (const auto& [t, fields] : events.events)
            if (t == type) out.push_back(fields);
        return out;
    }

    core::Log log;
    std::shared_ptr<RecordingSink> sink;
    RecordingEvents events;
    core::Settings settings;
    TempFiles files;
    core::Context ctx{log, events, core::StopToken{}, settings, files, nullptr, "fb_rch"};
};

// ---------------------------------------------------------------- synthetic networks

struct EdgeSpec {
    std::string source, target;
    std::vector<std::string> subtypes;
    std::string type = "PPrel";
    std::vector<std::string> pathways = {"path:hsa04010"};
    double score = 0;
};

struct NetSpec {
    std::vector<std::vector<std::string>> nodes;        // each a gene node's member ids; key = smallest
    std::vector<std::vector<std::string>> complexes;    // component keys; a complex may hold an earlier one
    std::vector<EdgeSpec> edges;
    bool component_edges = false;                       // S2.09's component edges for every complex
};

// S2.09's complex key: "c_s_" + the sorted component keys joined by "_" + "_c_e".
inline std::string complex_key(std::vector<std::string> components) {
    std::sort(components.begin(), components.end());
    components.erase(std::unique(components.begin(), components.end()), components.end());
    std::string key = "c_s_";
    for (std::size_t i = 0; i < components.size(); ++i) key += (i ? "_" : "") + components[i];
    return key + "_c_e";
}

inline std::vector<core::Sym> syms(core::Interner& strings, const std::vector<std::string>& names) {
    std::vector<core::Sym> out;
    for (const auto& n : names) out.push_back(strings.sym(n));
    return out;
}

// A network built from a spec, with the interner its handles come from. Not movable: the network may
// refer to the interner.
class TestNetwork {
public:
    TestNetwork(const NetSpec& spec, core::Context& ctx) {
        network::NetworkBuilder builder(network::NetworkInfo{}, strings_);
        std::vector<std::pair<std::string, network::NodeIdx>> by_key;
        auto find = [&](const std::string& key) {
            for (const auto& [k, v] : by_key)
                if (k == key) return v;
            throw std::runtime_error("test network: no node " + key);
        };
        for (const auto& members : spec.nodes) {
            std::vector<core::GeneId> ids;
            for (const auto& m : members) ids.push_back(strings_.id(m));
            const network::NodeIdx v = builder.add_node(strings_.sym("gene"), ids);
            by_key.emplace_back(*std::min_element(members.begin(), members.end()), v);
        }
        for (const auto& parts : spec.complexes) {
            std::vector<network::NodeIdx> comps;
            for (const auto& p : parts) comps.push_back(find(p));
            const network::NodeIdx c = builder.add_complex(comps);
            by_key.emplace_back(complex_key(parts), c);
            if (spec.component_edges) builder.add_component_edges(c, strings_.sym("path:hsa04010"));
        }
        for (const auto& e : spec.edges)
            builder.add_edge(find(e.source), find(e.target), strings_.sym(e.type), syms(strings_, e.subtypes),
                             syms(strings_, e.pathways), e.score);
        network = std::make_shared<const network::Network>(std::move(builder).finish(ctx));
    }
    TestNetwork(const TestNetwork&) = delete;
    TestNetwork& operator=(const TestNetwork&) = delete;

    network::NodeIdx node(std::string_view key) const {
        const auto v = network->find_node(network::NodeKey{std::string(key)});
        REQUIRE(v.has_value());
        return *v;
    }
    core::Interner& strings() noexcept { return strings_; }

private:
    core::Interner strings_;

public:
    std::shared_ptr<const network::Network> network;
};

// ---------------------------------------------------------------- reading a corridor

// The corridor's edges as "source>target", in index order.
inline std::vector<std::string> edge_names(const corridor::Corridor& c) {
    std::vector<std::string> out;
    c.view().for_each_edge([&](network::EdgeIdx e) {
        const auto key = c.network().edge_key(e);
        out.push_back(key.source.value + ">" + key.target.value);
    });
    return out;
}

// The corridor's nodes by key, in index order.
inline std::vector<std::string> node_names(const corridor::Corridor& c) {
    std::vector<std::string> out;
    c.view().for_each_node([&](network::NodeIdx v) { out.push_back(c.network().node_key(v).value); });
    return out;
}

inline std::vector<std::string> sorted(std::vector<std::string> items) {
    std::sort(items.begin(), items.end());
    return items;
}

inline EdgeSpec edge(std::string source, std::string target, std::vector<std::string> subtypes = {"activation"}) {
    return EdgeSpec{std::move(source), std::move(target), std::move(subtypes)};
}

inline std::vector<std::vector<std::string>> genes(std::initializer_list<std::string> keys) {
    std::vector<std::vector<std::string>> out;
    for (const auto& k : keys) out.push_back({k});
    return out;
}

inline corridor::Request request(std::string source, std::string target, std::uint32_t bound) {
    corridor::Request r;
    r.source = {std::move(source)};
    r.target = {std::move(target)};
    r.bound = bound;
    return r;
}

// Runs compute() with a fresh BindingTable and no maps.
struct Cut {
    Cut(TestNetwork& net, const corridor::Request& req, Harness& h)
        : table(net.network, network::Maps{}), outcome(corridor::compute(net.network, table, req, h.ctx)) {}
    const corridor::Corridor& operator*() const { return *outcome.value; }
    const corridor::Corridor* operator->() const { return outcome.value.get(); }
    network::BindingTable table;
    core::Outcome<std::shared_ptr<const corridor::Corridor>> outcome;
};

}  // namespace corridor_test
