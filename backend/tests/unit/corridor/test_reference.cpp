// FuSION 1's own corridors on booking 1's synthetic toy cases (backend/tests/reference/synthetic/,
// read only, through the test kit's FuSION 1 readers): the re-read graph each fb_rch ran on
// (f1/capture/graph_reread.xml), its fb_rch line and printed counts (f1/run_console.log), its corridor as
// wgx wrote it (f1/capture/corridor.xml) and its traces (f1/run/fb_rch, fb_rch_back). None of these networks holds a complex, a self-loop, an edge into
// the source or an edge out of the target, so neither expected difference, XD-17 (05.C2) nor XD-18
// (S4.12), applies: 2.0 must give FuSION 1's corridor exactly, with its target counts M and N (S4.17),
// its forward set, and the distances its two searches recorded (S4.07).
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "corridor_support.hpp"
#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/fusion1.hpp"

using namespace corridor_test;
namespace testkit = fusion::testkit;

namespace {

const std::filesystem::path kCases = std::filesystem::path(FUSION_TEST_SOURCE_DIR) / "reference" / "synthetic";

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    REQUIRE(in);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> words(const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> out;
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

// FuSION 1's written graph as the kit reads it (f1::read_written_graph), as a network built through 02's
// builder, with every attribute the canonical form compares: members, types, subtypes, pathways, scores.
NetSpec spec_of(const testkit::CanonGraph& g) {
    NetSpec s;
    for (const testkit::CanonNode& n : g.nodes) {
        REQUIRE(n.type == "gene");                     // no complex: no XD-18 difference
        s.nodes.push_back(n.members);
    }
    const auto key_of = [&](const std::string& canonical) { return testkit::module_key_of(canonical); };
    for (const testkit::CanonEdge& e : g.edges)
        s.edges.push_back(EdgeSpec{key_of(e.key.src), key_of(e.key.tgt), e.key.subtypes, e.key.type, e.pathways, e.score});
    return s;
}

// The first fb_rch line of the console log and the target and forward counts FuSION 1 printed for it,
// which f1::read_console does not collect (its corridor sizes are compared below).
struct F1Call {
    corridor::Request request;
    std::string flag;
    int dropped = -1, into_target = -1, forward = -1, nodes = -1, edges = -1;

    explicit F1Call(const std::filesystem::path& log) {
        const std::vector<std::string> lines = read_lines(log);
        std::size_t at = 0;
        while (at < lines.size() && lines[at].rfind("fb_rch ", 0) != 0) ++at;
        REQUIRE(at < lines.size());
        const std::vector<std::string> tokens = words(lines[at]);
        std::size_t k = 2;   // after the command and the graph id (S4.01's positional form)
        for (auto* list : {&request.source, &request.target, &request.exclude}) {
            for (; tokens.at(k) != "-1"; ++k) list->push_back(tokens.at(k));
            ++k;
        }
        request.bound = static_cast<std::uint32_t>(std::stoul(tokens.at(k)));
        flag = tokens.at(k + 1);
        for (std::size_t i = at + 1; i < lines.size() && nodes < 0; ++i) {
            const std::vector<std::string> w = words(lines[i]);
            if (lines[i].find("edges to target excluded") != std::string::npos) {
                dropped = std::stoi(w.at(0));
                into_target = std::stoi(w.at(3));
            } else if (lines[i].rfind("fwd nodes ", 0) == 0) {
                forward = std::stoi(w.at(2));
            } else if (lines[i].rfind("New graph ", 0) == 0) {
                nodes = std::stoi(w.at(4));
                edges = std::stoi(w.at(6));
            }
        }
    }
};

// A trace's distances by key: each queued node lies one edge beyond the node that queued it. Forward
// lines read "k: <nid>: <nid>(<key>): ..."; backward lines "k: <key> <nid>(<key>): ..." (S4.19).
std::map<std::string, std::uint32_t> trace_distances(const std::filesystem::path& file, const std::string& start,
                                                     bool forward) {
    std::map<std::string, std::uint32_t> by_key{{start, 0}};
    std::map<std::string, std::string> key_of_nid;
    for (const std::string& line : read_lines(file)) {
        if (line.empty()) continue;
        const auto colon = line.find(": ");
        REQUIRE(colon != std::string::npos);
        std::string rest = line.substr(colon + 2);
        std::string parent;
        if (forward) {
            const std::string nid = rest.substr(0, rest.find(':'));
            parent = key_of_nid.empty() ? start : key_of_nid.at(nid);   // the first line is the source
            key_of_nid.emplace(nid, parent);
            rest = rest.substr(nid.size() + 1);
        } else {
            parent = rest.substr(0, rest.find(' '));
            rest = rest.substr(parent.size());
        }
        const std::uint32_t d = by_key.at(parent) + 1;
        for (std::size_t open = rest.find('('); open != std::string::npos; open = rest.find('(', open + 1)) {
            std::size_t digits = open;
            while (digits > 0 && std::isdigit(static_cast<unsigned char>(rest[digits - 1]))) --digits;
            const std::string nid = rest.substr(digits, open - digits);
            const std::string key = rest.substr(open + 1, rest.find(')', open) - open - 1);
            key_of_nid.emplace(nid, key);
            by_key.emplace(key, d);
        }
    }
    return by_key;
}

std::vector<std::string> case_names() {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(kCases))
        if (entry.is_directory()) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace

TEST_CASE("corridor: 2.0 reproduces FuSION 1's corridors on the synthetic toy cases", "[corridor][reference][S4.09][S4.17]") {
    const std::vector<std::string> names = case_names();
    REQUIRE(names.size() >= 12);
    for (const std::string& name : names) {
        DYNAMIC_SECTION(name) {
            const std::filesystem::path dir = kCases / name / "f1";
            const testkit::CanonGraph graph = testkit::f1::read_written_graph(dir / "capture" / "graph_reread.xml");
            const testkit::CanonGraph f1_corridor = testkit::f1::read_written_graph(dir / "capture" / "corridor.xml");
            const testkit::f1::ConsoleFacts console = testkit::f1::read_console(dir / "run_console.log");
            const F1Call call(dir / "run_console.log");

            Harness h;
            TestNetwork net(spec_of(graph), h.ctx);
            corridor::Request r = call.request;
            const auto rule = corridor::parse_edges_to_target(call.flag);
            REQUIRE(rule);
            r.edges_to_target = *rule;
            Cut c(net, r, h);
            REQUIRE_FALSE(c.outcome.report);

            // No self-loop, edge into the source or edge out of the target: no XD-17 difference.
            for (const testkit::CanonEdge& e : graph.edges) {
                REQUIRE(e.key.src != e.key.tgt);
                REQUIRE(e.key.tgt != r.source.front());
                REQUIRE(e.key.src != r.target.front());
            }

            // 2.0's corridor equals FuSION 1's as canonical graphs: nodes, edges, pathways and scores.
            const testkit::CanonCorridor cut = testkit::adapt::canon(*c);
            const testkit::GraphDiff difference = testkit::diff(cut.graph, f1_corridor);
            INFO(difference.text());
            CHECK(difference.empty());
            REQUIRE(console.corridor_sizes.size() >= 1);
            CHECK(static_cast<int>(cut.graph.nodes.size()) == console.corridor_sizes.front().first);
            CHECK(static_cast<int>(cut.graph.edges.size()) == console.corridor_sizes.front().second);
            CHECK(console.corridor_sizes.front().first == call.nodes);
            CHECK(console.corridor_sizes.front().second == call.edges);
            const std::vector<std::string> nodes = node_names(*c);
            CHECK(static_cast<int>(c->counts().into_target) == call.into_target);
            CHECK(static_cast<int>(c->counts().into_target_dropped) == call.dropped);
            CHECK(static_cast<int>(c->counts().forward_set) == call.forward);

            const auto ds = trace_distances(dir / "run" / "fb_rch", r.source.front(), true);
            const auto dt = trace_distances(dir / "run" / "fb_rch_back", r.target.front(), false);
            CHECK(static_cast<int>(ds.size()) == call.forward);
            for (const std::string& key : nodes) {
                const network::NodeIdx v = net.node(key);
                CHECK(c->ds(v) == ds.at(key));
                CHECK(c->dt(v) == dt.at(key));
            }
            CHECK(corridor::check_invariants(*c).empty());
        }
    }
}
