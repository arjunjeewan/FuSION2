// The mff step (S2.16; section 03, "The step"): read the inputs, key them, get the base merge, assemble
// E, split, and write the saved step and the reports. FuSION 1's handler is Linux/src/main.cc:2330-2479:
// it read every listed file with rgx's reader (main.cc:2405-2416), folded the graphs
// (Linux/src/GraphManagerNew.cc:10499-10850) and printed "New graph: <id>" and the time taken.
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <unordered_set>
#include <utility>

#include "engine.hpp"

namespace fusion::merge {

namespace {

using detail::none;
using network::NodeIdx;
using Clock = std::chrono::steady_clock;

std::int64_t ms_since(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

std::string named(const network::SourceRef& s) {
    if (s.path && !s.path->empty()) return core::to_utf8(*s.path);
    return s.pathway;
}

std::filesystem::path copy_of(const network::SourceRef& s, core::Context& ctx) {
    if (!s.path) throw core::Failure("input.missing", "no path for the input of pathway " + s.pathway);
    return ctx.files.input(*s.path);                  // the checked copy (09.C3)
}

std::string plural(std::size_t n, std::string_view one, std::string_view many) {
    return std::to_string(n) + " " + std::string(n == 1 ? one : many);
}

// "exception set: 49 ids (43 up, 2 down, source, target, FSC, 3 not merged)" (section 03, "Console").
std::string exception_summary(const ExceptionSet& e) {
    std::array<std::size_t, 9> counts{};
    for (const std::string& id : e.ids().ids()) {
        const std::uint16_t r = e.reasons(std::string_view(id));
        for (std::size_t b = 0; b < counts.size(); ++b)
            if ((r >> b) & 1U) ++counts[b];
    }
    const auto one_or_many = [](std::size_t n, std::string_view one, std::string_view many) {
        return n == 1 ? std::string(one) : std::to_string(n) + " " + std::string(many);
    };
    std::vector<std::string> parts;
    if (counts[0] != 0) parts.push_back(std::to_string(counts[0]) + " up");
    if (counts[1] != 0) parts.push_back(std::to_string(counts[1]) + " down");
    if (counts[2] != 0) parts.push_back(std::to_string(counts[2]) + " confirmed up");
    if (counts[3] != 0) parts.push_back(std::to_string(counts[3]) + " confirmed down");
    if (counts[4] != 0) parts.push_back(one_or_many(counts[4], "source", "sources"));
    if (counts[5] != 0) parts.push_back(one_or_many(counts[5], "target", "targets"));
    if (counts[6] != 0) parts.push_back(one_or_many(counts[6], "FSC", "FSCs"));
    if (counts[7] != 0) parts.push_back(std::to_string(counts[7]) + " not merged");
    if (counts[8] != 0) parts.push_back(std::to_string(counts[8]) + " from the start file");
    std::string out = "exception set: " + plural(e.size(), "id", "ids");
    if (!parts.empty()) {
        out += " (";
        for (std::size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? ", " : "") + parts[i];
        out += ")";
    }
    return out;
}

// The third console line and the merge.done event (S2.28: multi-gene nodes and the largest one).
void report_graph(const MergedNetwork& m, int graph, std::int64_t ms, core::Context& ctx) {
    const network::GraphView view(m.network);
    const network::Summary s = network::summarize(view);
    std::string line = "mff: " + (graph > 0 ? "graph " + std::to_string(graph) : std::string("network")) + ": " +
                       plural(s.nodes, "node", "nodes") + " (" + plural(s.genes, "gene", "genes") + ", " +
                       plural(s.complexes, "complex", "complexes") + ", " + plural(s.compounds, "compound", "compounds");
    if (s.others != 0) line += ", " + plural(s.others, "other", "others");
    line += "), " + plural(s.edges, "edge", "edges") + "; ";
    core::Json largest = nullptr;
    if (s.multi_gene == 0) {
        line += "no node holds more than one gene";
    } else {
        const std::string key = m.network->node_key(s.largest).value;
        line += plural(s.multi_gene, "node holds", "nodes hold") + " more than one gene, the largest " + key +
                " with " + std::to_string(s.largest_size);
        largest = core::Json{{"key", key}, {"genes", s.largest_size}};
    }
    if (m.network->info().legacy) line += " (legacy)";
    ctx.log.info(line);
    ctx.events.emit("merge.done", core::Json{{"ms", ms},
                                             {"graph", graph},
                                             {"legacy", m.network->info().legacy},
                                             {"nodes", s.nodes},
                                             {"genes", s.genes},
                                             {"complexes", s.complexes},
                                             {"compounds", s.compounds},
                                             {"others", s.others},
                                             {"edges", s.edges},
                                             {"multi_gene_nodes", s.multi_gene},
                                             {"largest", largest}});
}

// The saved step (09.C4, S2.26: the written file is a saved step, not a round trip) and exceptions.tsv,
// through 09's Files and OutputFile (2A.6).
void write_step_files(const MergedNetwork& m, const BaseMerge* base, const network::Maps& maps, core::Context& ctx) {
    {
        core::OutputFile f = ctx.files.create("merged_graph.xml");
        network::write_fusion_network(network::GraphView(m.network), f.out(), maps, network::WriteOptions{});
        f.commit();
    }
    core::OutputFile f = ctx.files.create("exceptions.tsv");
    if (base != nullptr)
        write_exceptions(m, *base, maps, f);
    else
        write_exceptions(m, maps, f);
    f.commit();
}

// merged_edges.tsv when report= is given (S2.24); a relative or omitted name lands in mff/, an absolute
// path is honoured as well, with the file also kept in mff/ (2B.3).
void write_report(const MergedNetwork& m, const std::string& name, const network::Maps& maps, core::Context& ctx) {
    std::filesystem::path p = core::path_from_utf8(name);
    if (p.empty()) p = "merged_edges.tsv";
    core::OutputFile f = ctx.files.create(core::to_utf8(p.is_absolute() ? p.filename() : p));
    if (p.is_absolute()) f.also_at(p);
    write_edge_report(m, maps, f);
    f.commit();
}

// A network file holding add's transcription (02.C17): no embedded sources, and every node one id.
bool transcription(const network::Network& n) {
    const network::Provenance* p = n.provenance();
    if (p != nullptr && !p->docs.empty()) return false;
    for (NodeIdx i = 0; i < n.nodes().size(); ++i)
        if (n.kind(i) == network::NodeKind::complex || n.members(i).size() != 1) return false;
    return true;
}

// 03.C9: beside the legacy master, edge tables and add's output bind to its nodes by first claim (S2.03);
// an id the legacy graph lacks becomes a new one-id node, and nothing is unioned.
std::shared_ptr<const network::Network> bind_extras(const network::Network& legacy, const std::vector<DocPtr>& tables,
                                                    const std::vector<std::shared_ptr<const network::Network>>& adds,
                                                    const network::BuildSettings& settings,
                                                    const ExceptionSet& exceptions, core::Context& ctx) {
    using network::EdgeFlag;
    network::NetworkInfo info = legacy.info();
    for (const DocPtr& t : tables) info.sources.push_back(t->source());
    for (const auto& a : adds)
        for (const auto& s : a->info().sources) info.sources.push_back(s);
    std::sort(info.sources.begin(), info.sources.end(), [](const network::SourceRef& a, const network::SourceRef& b) {
        const auto c = core::stable_compare(a.pathway, b.pathway);
        return c != 0 ? c < 0 : a.sha256 < b.sha256;
    });
    info.legacy = true;
    core::Interner in;
    network::NetworkBuilder b(std::move(info), in);
    const core::Interner& lin = legacy.interner();

    // The legacy graph as it stands (02.C5): nodes, complexes, marks and edges, tags included, through
    // the builder's copies (2B.11).
    const std::size_t count = legacy.nodes().size();
    std::vector<NodeIdx> map(count, none);
    for (NodeIdx n = 0; n < count; ++n) map[n] = b.copy_node(legacy, n);
    for (network::EdgeIdx e = 0; e < legacy.edges().size(); ++e)
        b.copy_edge(legacy, e, map[legacy.edges()[e].src], map[legacy.edges()[e].tgt]);
    // First claims: the legacy graph's, in its claim order, then the new nodes'.
    std::vector<std::pair<core::GeneId, NodeIdx>> claims;
    std::unordered_set<std::string, detail::StableHash, std::equal_to<>> claimed;
    for (NodeIdx n = 0; n < count; ++n) {
        if (legacy.kind(n) == network::NodeKind::complex) continue;
        for (const core::GeneId g : legacy.members(n)) {
            if (!claimed.emplace(lin.str(g)).second) continue;
            for (const NodeIdx h : legacy.ids().holders(g)) claims.emplace_back(in.id(lin.str(g)), map[h]);
        }
    }
    std::map<std::string, NodeIdx, detail::ByteLess> fresh;
    const auto bind = [&](std::string_view id, std::string_view type) -> NodeIdx {
        if (const auto g = lin.find_id(id)) {
            const auto holders = legacy.ids().holders(*g);
            if (!holders.empty()) return map[holders.front()];
        }
        if (const auto it = fresh.find(id); it != fresh.end()) return it->second;
        const core::GeneId g = in.id(id);
        const NodeIdx n = b.add_node(in.sym(type), std::span<const core::GeneId>(&g, 1));
        if (exceptions.contains(id)) b.mark_exception(n);
        fresh.emplace(std::string(id), n);
        claims.emplace_back(g, n);
        return n;
    };
    // Edge tables: each row builds as a KGML relation would (02.C2), with its ids bound one by one.
    for (const DocPtr& d : tables) {
        const network::PathwayDoc& doc = *d;
        const core::Interner& din = doc.interner();
        const core::Sym own_pathway = in.sym(doc.source().pathway);
        std::map<std::uint32_t, std::vector<NodeIdx>> made;
        std::function<std::vector<NodeIdx>(std::uint32_t)> nodes = [&](std::uint32_t local) {
            if (const auto it = made.find(local); it != made.end()) return it->second;
            std::vector<NodeIdx> out;
            const network::Entry* e = doc.entry(local);
            if (e == nullptr) return out;
            const auto comps = doc.components(*e);
            if (network::kind_of(din.str(e->type)) == network::NodeKind::complex && !comps.empty()) {
                std::vector<std::vector<NodeIdx>> combos(1);
                for (const std::uint32_t c : comps) {
                    std::vector<std::vector<NodeIdx>> next;
                    for (const auto& combo : combos)
                        for (const NodeIdx n : nodes(c)) {
                            next.push_back(combo);
                            next.back().push_back(n);
                        }
                    combos = std::move(next);
                }
                for (const auto& combo : combos) {
                    const NodeIdx cx = b.add_complex(combo);
                    b.add_component_edges(cx, own_pathway);       // S2.09
                    out.push_back(cx);
                }
            } else {
                for (const core::GeneId g : doc.ids(*e)) out.push_back(bind(din.str(g), din.str(e->type)));
            }
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            made.emplace(local, out);
            return out;
        };
        for (const std::uint32_t ri : network::kept_relations(doc, settings.filters)) {
            const network::Relation& r = doc.relations()[ri];
            std::vector<core::Sym> subs, paths;
            std::uint16_t flags = 0;
            for (const core::Sym x : doc.subtypes(r)) {
                const std::string_view t = din.str(x);
                if (t == "association" || t == "dissociation" || t == "mapping") flags |= network::bit(EdgeFlag::undirected);
                subs.push_back(in.sym(t));
            }
            for (const core::Sym x : doc.pathways(r)) paths.push_back(in.sym(din.str(x)));
            if (paths.empty()) paths.push_back(own_pathway);
            const core::Sym type = in.sym(din.str(r.type));
            for (const NodeIdx s : nodes(r.entry1))
                for (const NodeIdx t : nodes(r.entry2)) {
                    b.add_edge(s, t, type, subs, paths, r.score, flags);
                    if ((flags & network::bit(EdgeFlag::undirected)) != 0 && s != t) b.add_edge(t, s, type, subs, paths, r.score, flags);
                }
        }
    }
    // add's output: its one-id nodes bind the same way, and its edges join as they stand (02.C17).
    for (const auto& a : adds) {
        const core::Interner& ain = a->interner();
        for (network::EdgeIdx e = 0; e < a->edges().size(); ++e) {
            const network::Edge& ed = a->edges()[e];
            b.copy_edge(*a, e, bind(ain.str(a->members(ed.src).front()), ain.str(a->nodes()[ed.src].type)),
                        bind(ain.str(a->members(ed.tgt).front()), ain.str(a->nodes()[ed.tgt].type)));
        }
    }
    b.set_claim_order(claims);
    return std::make_shared<const network::Network>(std::move(b).finish(ctx));
}

core::Outcome<MergedNetwork> legacy_step(const MergeRequest& req, const std::vector<const network::SourceRef*>& legacy,
                                         const std::vector<const network::SourceRef*>& documents,
                                         const std::vector<const network::SourceRef*>& networks, ExceptionSet exceptions,
                                         const network::Maps& maps, network::StepDocs& step_docs, core::Context& ctx) {
    const auto t0 = Clock::now();
    const network::SourceRef& master = *legacy.front();
    const auto mixed = [&](const network::SourceRef& other) {
        throw core::Failure("merge.legacy_mixed",
                            named(master) + " is a network FuSION 1 wrote, read as legacy (02.C5); it may share its list "
                                "only with edge tables and add's output, not with " + named(other) + " (03.C9)",
                            core::Where{named(other), 0, {}});
    };
    if (legacy.size() > 1) mixed(*legacy[1]);
    std::vector<DocPtr> tables;
    for (const network::SourceRef* in : documents) {
        if (in->format != network::Format::edge_table) mixed(*in);
        tables.push_back(step_docs.get(copy_of(*in, ctx), maps.ids, ctx));
    }
    std::vector<std::shared_ptr<const network::Network>> adds;
    for (const network::SourceRef* in : networks) {
        auto n = network::read_fusion_network(copy_of(*in, ctx), ctx);
        if (!transcription(*n)) mixed(*in);
        adds.push_back(std::move(n));
    }
    // FuSION 1's graph 2, with the run's E: a known FSC gets its own node in legacy reads too (S2.13).
    std::shared_ptr<const network::Network> net =
        network::read_legacy(copy_of(master, ctx), maps.ids, req.build, exceptions.ids(), ctx);
    if (!tables.empty() || !adds.empty()) net = bind_extras(*net, tables, adds, req.build, exceptions, ctx);

    MergedNetwork merged{net, std::move(exceptions), MergeKey{}, SplitStats{}, BaseSource::built};
    merged.stats.exception_ids = static_cast<std::uint32_t>(merged.exceptions.size());
    for (const std::string& id : merged.exceptions.ids().ids())
        if (const auto g = net->interner().find_id(id); g && !net->ids().holders(*g).empty()) ++merged.stats.in_network;
    ctx.log.info("mff: " + named(master) + " read as legacy, FuSION 1's way (S2.27, 02.C5)" +
                 (tables.size() + adds.size() != 0
                      ? ", with " + plural(tables.size() + adds.size(), "file", "files") + " bound by first claim (03.C9)"
                      : std::string()));
    ctx.log.info("mff: " + exception_summary(merged.exceptions) + "; " + std::to_string(merged.stats.in_network) +
                 " in the network");
    write_step_files(merged, nullptr, maps, ctx);
    if (req.report) ctx.log.info("mff: a legacy read merges nothing, so it writes no merge report (S2.24)");
    report_graph(merged, req.graph, ms_since(t0), ctx);
    return core::Outcome<MergedNetwork>{std::move(merged), std::nullopt, false};
}

}  // namespace

core::Outcome<MergedNetwork> merge_step(const MergeRequest& req, MergeStore& store, network::StepDocs& step_docs,
                                        core::Context& ctx) {
    if (req.inputs.empty()) throw core::Failure("merge.empty_list", "the list names no file");
    const network::Maps maps = req.maps != nullptr ? *req.maps : network::Maps{};

    // Stage 4 first, so a missing exception file fails before any parse: E from the run's inputs (03.C1).
    ExceptionSet exceptions = assemble_exceptions(req.exceptions, maps.ids, ctx);

    // The kinds of source in the list (section 03), as section 02's sniff told them to the handler.
    std::vector<const network::SourceRef*> legacy, documents, networks;
    for (const network::SourceRef& in : req.inputs) {
        switch (in.format) {
            case network::Format::legacy: legacy.push_back(&in); break;
            case network::Format::fusion_network: networks.push_back(&in); break;
            case network::Format::kgml:
            case network::Format::edge_table: documents.push_back(&in); break;
        }
    }
    if (!legacy.empty())
        return legacy_step(req, legacy, documents, networks, std::move(exceptions), maps, step_docs, ctx);

    // Stage 1: the documents, from the parse of the step's own inputs (02.C15, 09.C3; never gsea's, S7.19),
    // and the documents a FuSION network file embeds, as if its original files were listed (03.C7).
    const auto t0 = Clock::now();
    std::vector<DocPtr> docs;
    for (const network::SourceRef* in : documents) {
        ctx.stop.throw_if_stopped();
        docs.push_back(step_docs.get(copy_of(*in, ctx), maps.ids, ctx));
    }
    for (const network::SourceRef* in : networks) {
        ctx.stop.throw_if_stopped();
        const auto saved = network::read_fusion_network(copy_of(*in, ctx), ctx);
        if (!detail::same_id_map(saved->info().id_map, maps.ids.digest()))
            throw core::Failure("network.settings_mismatch",
                                named(*in) + " was built under another id map, so its documents' ids cannot be "
                                             "mapped again (02.C3, 03.C7)",
                                core::Where{named(*in), 0, {}});
        const network::Provenance* p = saved->provenance();
        if (p == nullptr || p->docs.empty())
            throw core::Failure("merge.no_sources",
                                named(*in) + " embeds no sources, so mff cannot split it exactly (03.C7)",
                                core::Where{named(*in), 0, {}});
        docs.insert(docs.end(), p->docs.begin(), p->docs.end());
    }

    // Stages 2 and 3: the key, and the base merge from memory or built (the shared store comes later).
    MergeStore::Hit hit = store.get_or_build(std::move(docs), req.build, ctx);
    if (hit.source == BaseSource::built && documents.empty()) hit.source = BaseSource::embedded;
    const BaseMerge& base = *hit.merge;
    const network::Network& base_net = *base.network();
    ctx.events.emit("merge.base", core::Json{{"ms", ms_since(t0)},
                                             {"key", base.key().hex()},
                                             {"source", std::string(source_name(hit.source))},
                                             {"pathways", base.docs().size()},
                                             {"boxes", base.box_count()},
                                             {"relations", base.relation_count()},
                                             {"nodes", base_net.nodes().size()},
                                             {"edges", base_net.edges().size()}});
    ctx.log.info("mff: " + plural(base.docs().size(), "pathway file", "pathway files") + "; base merge " +
                 base.key().hex().substr(0, 8) + "… " + std::string(source_name(hit.source)) + ": " +
                 plural(base_net.nodes().size(), "node", "nodes") + ", " +
                 plural(base_net.edges().size(), "edge", "edges"));

    // Stages 4 and 5: apply E to the base merge.
    const auto t1 = Clock::now();
    MergedNetwork merged = split(base, exceptions);
    merged.source = hit.source;
    const SplitStats& st = merged.stats;
    ctx.events.emit("merge.split", core::Json{{"ms", ms_since(t1)},
                                              {"exceptions", st.exception_ids},
                                              {"in_network", st.in_network},
                                              {"split_out", st.split_out},
                                              {"affected_nodes", st.affected_nodes},
                                              {"affected_boxes", st.affected_boxes},
                                              {"affected_groups", st.affected_groups},
                                              {"affected_relations", st.affected_relations},
                                              {"nodes", merged.network->nodes().size()},
                                              {"edges", merged.network->edges().size()}});
    ctx.log.info("mff: " + exception_summary(merged.exceptions) + "; " + std::to_string(st.in_network) +
                 " in the network, " + std::to_string(st.split_out) + " split out of family nodes");

    // Stage 6: the saved step and the reports.
    const auto t2 = Clock::now();
    write_step_files(merged, &base, maps, ctx);
    if (req.report) write_report(merged, *req.report, maps, ctx);
    report_graph(merged, req.graph, ms_since(t2), ctx);
    return core::Outcome<MergedNetwork>{std::move(merged), std::nullopt, false};
}

}  // namespace fusion::merge
