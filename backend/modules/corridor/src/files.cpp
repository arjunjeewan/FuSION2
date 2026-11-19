// fusion corridor: the saved step, corridor.json and corridor_nodes.tsv, and the debug traces
// (section 05, "Files"; 05.C6, S4.07, S4.19, S4.21). FuSION 1 kept its corridor only in memory
// (S4.21) and wrote two trace files into the launch directory on every call
// (Linux/src/GraphManagerNew.cc:17858, 18054).
#include <algorithm>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "fusion/corridor/corridor.hpp"
#include "impl.hpp"

namespace fusion::corridor {

using network::EdgeIdx;
using network::Network;
using network::NodeIdx;

namespace {

// JSON text without locale and with any byte that is not UTF-8 replaced, so a stray byte in a label
// cannot stop a save (determinism rules of section 09).
std::string dump(const core::Json& value) {
    return value.dump(-1, ' ', false, core::Json::error_handler_t::replace);
}

core::Json maybe(int graph) { return graph > 0 ? core::Json(graph) : core::Json(nullptr); }

core::Json request_json(const Request& request) {
    const Request n = normalised(request);
    core::Json j = core::Json::object();
    j["source"] = n.source;
    j["target"] = n.target;
    j["exclude"] = n.exclude;
    j["bound"] = n.bound;
    j["edges_to_target"] = name(n.edges_to_target);
    j["skip_subtypes"] = n.skip_subtypes;
    return j;
}

void write_json(const Corridor& c, std::span<const expression::ObservationMatch> matched, const Naming& naming,
                core::Context& ctx) {
    const Corridor::Impl& m = c.impl();
    const Network& net = c.network();
    const core::Interner& strings = net.interner();

    core::OutputFile file = ctx.files.create("corridor.json");
    std::ostream& out = file.out();
    auto field = [&](std::string_view key, const core::Json& value, bool last = false) {
        out << "  " << dump(core::Json(key)) << ": " << dump(value) << (last ? "\n" : ",\n");
    };
    out << "{\n";
    field("format", "fusion-corridor");
    field("format_version", 1);
    field("graph", maybe(naming.graph));
    field("network", core::Json{{"graph", maybe(naming.network_graph)},
                                {"content_hash", m.network_hash.hex()},
                                {"nodes", net.nodes().size()},
                                {"edges", net.edges().size()}});
    field("request", request_json(m.request));

    core::Json bindings = core::Json::array();
    for (const Bound& x : m.bindings)
        bindings.push_back(core::Json{{"role", detail::role_name(x.role)},
                                      {"given", x.binding.given},
                                      {"node", net.node_key(x.binding.node).value},
                                      {"label", detail::label(net, x.binding.node, naming.maps)},
                                      {"status", detail::status_name(x.binding.status)}});
    field("bindings", bindings);

    core::Json excluded = core::Json::array();
    for (const Excluded& x : m.excluded)
        excluded.push_back(core::Json{{"node", net.node_key(x.node).value},
                                      {"because", x.because},
                                      {"complex", x.complex_of_excluded}});
    field("excluded_nodes", excluded);

    field("status", c.empty() ? "empty" : "ok");
    field("empty_reason", c.empty() ? core::Json(detail::reason_name(m.empty_reason)) : core::Json(nullptr));
    field("shortest_path", m.shortest_path ? core::Json(*m.shortest_path) : core::Json(nullptr));
    field("counts", core::Json{{"traversable_edges", m.counts.traversable_edges},
                               {"into_target", m.counts.into_target},
                               {"into_target_dropped", m.counts.into_target_dropped},
                               {"forward_set", m.counts.forward_set},
                               {"backward_set", m.counts.backward_set},
                               {"nodes", m.nodes},
                               {"edges", m.edges}});

    // S3.12 and S4.16's groups, beside the corridor's parameters (05.C9); null without observations.
    // FuSION 1 printed only the genes inside the corridor, "Up: 3 Down: 0" (main.cc:4693-4714).
    core::Json observations(nullptr);
    if (!matched.empty()) {
        std::uint32_t inside = 0, outside = 0, absent = 0;
        for (const expression::ObservationMatch& x : matched) switch (x.group) {
                case expression::MatchGroup::in_corridor: ++inside; break;
                case expression::MatchGroup::outside_corridor: ++outside; break;
                case expression::MatchGroup::not_in_network: ++absent; break;
            }
        observations = core::Json{{"in_corridor", inside}, {"outside_corridor", outside}, {"not_in_network", absent}};
    }
    field("observations", observations);

    // Nodes and edges in the network's index order (02.C10), named by their stable keys (S4.14, 02.C11).
    out << "  \"nodes\": [";
    bool first = true;
    c.view().for_each_node([&](NodeIdx v) {
        const core::Json row{{"key", net.node_key(v).value},
                             {"label", detail::label(net, v, naming.maps)},
                             {"type", strings.str(net.nodes()[v].type)},
                             {"genes", net.members(v).size()},
                             {"ds", m.ds[v]},
                             {"dt", m.dt[v]}};
        out << (first ? "\n    " : ",\n    ") << dump(row);
        first = false;
    });
    out << (first ? "],\n" : "\n  ],\n");

    out << "  \"edges\": [";
    first = true;
    c.view().for_each_edge([&](EdgeIdx e) {
        const network::EdgeKey key = net.edge_key(e);
        core::Json pathways = core::Json::array();
        for (core::Sym p : net.pathways(e)) pathways.push_back(strings.str(p));
        const core::Json row{{"source", key.source.value},
                             {"target", key.target.value},
                             {"type", key.type},
                             {"subtypes", key.subtypes},
                             {"pathways", pathways},
                             {"score", net.edges()[e].score}};
        out << (first ? "\n    " : ",\n    ") << dump(row);
        first = false;
    });
    out << (first ? "],\n" : "\n  ],\n");
    field("digest", m.digest.hex(), true);
    out << "}\n";
    file.commit();
}

// corridor_nodes.tsv: one row per corridor node, sorted by ds, then dt, then key, so it reads from the
// source outwards (S4.07).
void write_node_table(const Corridor& c, const Naming& naming, core::Context& ctx) {
    const Corridor::Impl& m = c.impl();
    const Network& net = c.network();
    struct Row {
        NodeIdx node;
        std::string key;
    };
    std::vector<Row> rows;
    c.view().for_each_node([&](NodeIdx v) { rows.push_back({v, net.node_key(v).value}); });
    std::sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) {
        if (m.ds[a.node] != m.ds[b.node]) return m.ds[a.node] < m.ds[b.node];
        if (m.dt[a.node] != m.dt[b.node]) return m.dt[a.node] < m.dt[b.node];
        return core::stable_compare(a.key, b.key) < 0;
    });
    // Numbers go out as text made without the stream's locale (section 09's determinism rules).
    std::string text = "key\tlabel\ttype\tgenes\tds\tdt\n";
    for (const Row& r : rows)
        text += r.key + '\t' + detail::label(net, r.node, naming.maps) + '\t' +
                std::string(net.interner().str(net.nodes()[r.node].type)) + '\t' +
                std::to_string(net.members(r.node).size()) + '\t' + std::to_string(m.ds[r.node]) + '\t' +
                std::to_string(m.dt[r.node]) + '\n';
    core::OutputFile file = ctx.files.create("corridor_nodes.tsv");
    file.out() << text;
    file.commit();
}

// S4.19: one row per node the search dequeued, with the nodes it queued, all by key. FuSION 1 mixed
// node ids with rep ids (GraphManagerNew.cc:17867-17868, 17925, 18061-18062, 18095).
void write_trace(const Corridor& c, std::string_view file_name, const std::vector<TraceRow>& trace,
                 core::Context& ctx) {
    const Network& net = c.network();
    std::string text = "order\tnode\tdistance\tqueued\n";
    for (std::size_t i = 0; i < trace.size(); ++i) {
        text += std::to_string(i) + '\t' + net.node_key(trace[i].node).value + '\t' +
                std::to_string(trace[i].distance) + '\t';
        for (std::size_t k = 0; k < trace[i].queued.size(); ++k)
            text += (k > 0 ? "," : "") + net.node_key(trace[i].queued[k]).value;
        text += '\n';
    }
    core::OutputFile file = ctx.files.create_debug(file_name);
    file.out() << text;
    file.commit();
}

[[noreturn]] void unreadable(const std::string& where, const std::string& why) {
    throw core::Failure("input.unreadable", where + ": " + why, core::Where{where, 0, {}});
}

std::string short_hash(std::string_view hex) { return std::string(hex.substr(0, 12)) + "…"; }

}  // namespace

void save(const Corridor& c, std::span<const expression::ObservationMatch> matched, const Naming& naming,
          core::Context& ctx) {
    write_json(c, matched, naming, ctx);
    write_node_table(c, naming, ctx);
    if (ctx.log.debug_on() && c.impl().traced) {
        write_trace(c, "forward.tsv", c.impl().forward_trace, ctx);
        write_trace(c, "backward.tsv", c.impl().backward_trace, ctx);
    }
}

void save(const Corridor& c, std::span<const expression::ObservationMatch> matched, core::Context& ctx) {
    save(c, matched, Naming{}, ctx);
}

// The loader (05.C6): maps the saved keys back to the network's indices and re-binds every given id,
// without rerunning the search, so a resumed solve runs on exactly the corridor it started on.
std::shared_ptr<const Corridor> load(std::shared_ptr<const network::Network> network_ptr,
                                     network::BindingTable& table, const std::filesystem::path& corridor_json,
                                     core::Context& /*ctx*/) {
    const Network& net = *network_ptr;
    const std::string where = core::to_utf8(corridor_json);
    std::ifstream in(corridor_json, std::ios::binary);
    if (!in) unreadable(where, "cannot be opened");
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

    core::Json doc;
    try {
        doc = core::Json::parse(text);
    } catch (const core::Json::exception& e) {
        unreadable(where, std::string("not valid JSON: ") + e.what());
    }

    try {
        if (!doc.is_object() || doc.value("format", "") != "fusion-corridor")
            unreadable(where, "not a FuSION corridor file");
        const int version = doc.at("format_version").get<int>();
        if (version > 1)
            throw core::Failure("artefact.newer_format",
                                where + " has format version " + std::to_string(version) +
                                    ", written by a newer FuSION; this one reads version 1",
                                core::Where{where, 0, {}});
        if (version < 1) unreadable(where, "unknown format version " + std::to_string(version));

        const std::string saved_hash = doc.at("network").at("content_hash").get<std::string>();
        const core::Sha256 hash = net.content_hash();
        if (saved_hash != hash.hex())
            throw core::Failure("corridor.network_mismatch",
                                where + " was cut from network " + short_hash(saved_hash) +
                                    ", but the network given is " + short_hash(hash.hex()),
                                core::Where{where, 0, {}});
        auto mismatch = [&](const std::string& why) -> core::Failure {
            return core::Failure("corridor.network_mismatch", where + ": " + why, core::Where{where, 0, {}});
        };
        auto node_of = [&](const std::string& key) {
            const auto v = net.find_node(network::NodeKey{key});
            if (!v) throw mismatch("node " + key + " is not in the network");
            return *v;
        };

        const core::Json& r = doc.at("request");
        Request request;
        request.source = r.at("source").get<std::vector<std::string>>();
        request.target = r.at("target").get<std::vector<std::string>>();
        request.exclude = r.at("exclude").get<std::vector<std::string>>();
        request.bound = r.at("bound").get<std::uint32_t>();
        const auto rule = parse_edges_to_target(r.at("edges_to_target").get<std::string>());
        if (!rule) unreadable(where, "unknown edges_to_target value");
        request.edges_to_target = *rule;
        request.skip_subtypes = r.at("skip_subtypes").get<std::vector<std::string>>();

        // Every given id re-binds through the run's BindingTable to the same node (S4.15).
        std::vector<Bound> bindings;
        std::optional<NodeIdx> source, target;
        for (const core::Json& row : doc.at("bindings")) {
            const auto role = detail::parse_role(row.at("role").get<std::string>());
            if (!role) unreadable(where, "unknown binding role");
            const std::string given = row.at("given").get<std::string>();
            const std::string key = row.at("node").get<std::string>();
            const network::Binding& b = table.bind(detail::role_name(*role), given);
            if (b.status == network::BindStatus::absent || b.node >= net.nodes().size() ||
                net.node_key(b.node).value != key)
                throw mismatch(std::string(detail::role_name(*role)) + " " + given + " bound to " + key +
                               " when the corridor was cut, but binds to " +
                               (b.status == network::BindStatus::absent || b.node >= net.nodes().size()
                                    ? std::string("no node")
                                    : net.node_key(b.node).value) +
                               " now");
            bindings.push_back({*role, b});
            if (*role == Role::source && !source) source = b.node;
            if (*role == Role::target && !target) target = b.node;
        }
        if (!source || !target) unreadable(where, "the bindings name no source or no target");

        std::vector<Excluded> excluded;
        for (const core::Json& row : doc.at("excluded_nodes"))
            excluded.push_back({node_of(row.at("node").get<std::string>()), row.at("because").get<std::string>(),
                                row.value("complex", false)});

        const auto n_nodes = net.nodes().size();
        network::Bitset nodes(n_nodes), edges(net.edges().size());
        std::vector<std::uint32_t> ds(n_nodes, kInf), dt(n_nodes, kInf);
        std::uint32_t kept_nodes = 0, kept_edges = 0;
        for (const core::Json& row : doc.at("nodes")) {
            const NodeIdx v = node_of(row.at("key").get<std::string>());
            if (!nodes.test(v)) ++kept_nodes;
            nodes.set(v);
            ds[v] = row.at("ds").get<std::uint32_t>();
            dt[v] = row.at("dt").get<std::uint32_t>();
        }
        for (const core::Json& row : doc.at("edges")) {
            network::EdgeKey key{network::NodeKey{row.at("source").get<std::string>()},
                                 network::NodeKey{row.at("target").get<std::string>()},
                                 row.at("type").get<std::string>(), row.at("subtypes").get<std::vector<std::string>>()};
            const auto e = net.find_edge(key);
            if (!e) throw mismatch("edge " + key.text() + " is not in the network");
            if (!edges.test(*e)) ++kept_edges;
            edges.set(*e);
        }

        auto impl = std::make_unique<Corridor::Impl>(network::GraphView(network_ptr, std::move(nodes), std::move(edges)));
        impl->request = std::move(request);
        impl->bindings = std::move(bindings);
        impl->excluded = std::move(excluded);
        impl->source = *source;
        impl->target = *target;
        if (doc.at("status").get<std::string>() == "empty") {
            const auto reason = detail::parse_reason(doc.at("empty_reason").get<std::string>());
            if (!reason) unreadable(where, "unknown empty_reason");
            impl->empty_reason = *reason;
        }
        if (!doc.at("shortest_path").is_null()) impl->shortest_path = doc.at("shortest_path").get<std::uint32_t>();
        const core::Json& counts = doc.at("counts");
        impl->counts.traversable_edges = counts.at("traversable_edges").get<std::uint32_t>();
        impl->counts.into_target = counts.at("into_target").get<std::uint32_t>();
        impl->counts.into_target_dropped = counts.at("into_target_dropped").get<std::uint32_t>();
        impl->counts.forward_set = counts.at("forward_set").get<std::uint32_t>();
        impl->counts.backward_set = counts.at("backward_set").get<std::uint32_t>();
        impl->nodes = kept_nodes;
        impl->edges = kept_edges;
        impl->ds = std::move(ds);
        impl->dt = std::move(dt);
        impl->network_hash = hash;
        impl->digest = detail::digest_of(*impl);
        if (impl->digest.hex() != doc.at("digest").get<std::string>())
            throw core::Failure("artefact.changed", where + ": its digest does not match its content",
                                core::Where{where, 0, {}});
        return std::make_shared<Corridor>(std::move(impl));
    } catch (const core::Json::exception& e) {
        unreadable(where, e.what());
    }
}

}  // namespace fusion::corridor
