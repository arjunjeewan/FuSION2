// The merge's reports (S2.24; section 03, "Files written"): exceptions.tsv, always, and the provenance
// report merged_edges.tsv when asked for. FuSION 1 wrote `edges_merged_filename` into the launch directory
// after every merge of two or more files, and nothing after a one-file merge
// (Linux/src/GraphManagerNew.cc:10798-10845). 2.0 writes it through 09's OutputFile, from the network's
// provenance (2B.11), so a one-file merge has a report too. TSV with a header, LF, sorted (E1).
#include <cmath>
#include <ostream>

#include "engine.hpp"

namespace fusion::merge {

namespace {

using detail::Ref;
using detail::RefKind;
using network::NodeIdx;

std::optional<NodeIdx> holder(const network::Network& net, std::string_view id) {
    const auto g = net.interner().find_id(id);
    if (!g) return std::nullopt;
    const auto h = net.ids().holders(*g);
    if (h.empty()) return std::nullopt;
    return h.front();                                 // first claim (S2.03); the only one outside legacy
}

void exceptions_tsv(const MergedNetwork& m, const BaseMerge* base, const network::Maps& maps,
                    core::OutputFile& file) {
    std::ostream& out = file.out();
    out << "id\tlabel\treasons\tnode\tbase_node\tbase_size\n";
    for (const std::string& id : m.exceptions.ids().ids()) {
        out << id << '\t' << maps.symbols.label_or_id(id) << '\t'
            << reason_names(m.exceptions.reasons(std::string_view(id))) << '\t';
        if (m.network != nullptr)
            if (const auto n = holder(*m.network, id)) out << m.network->node_key(*n).value;
        out << '\t';
        std::size_t size = 0;
        if (base != nullptr)
            if (const auto n = holder(*base->network(), id)) {
                out << base->network()->node_key(*n).value;
                size = base->network()->members(*n).size();
            }
        out << '\t' << size << '\n';
    }
}

// A relation's score as written: a whole score as an integer, FuSION 1's form, any other as the
// shortest text that reads back exactly (1.20); an unscored relation leaves the cell empty (1.25).
std::string score_text(const std::optional<double>& score) {
    if (!score) return {};
    const double v = *score;
    if (std::isfinite(v) && std::floor(v) == v && std::fabs(v) < 1e15) return std::to_string(static_cast<long long>(v));
    return core::format_number(v);
}

// An entry's ids as the report gives them: a box's ids, space-joined in byte order; a group's
// components joined by '+', a nested group in brackets.
std::string entry_ids(const network::PathwayDoc& doc, std::uint32_t local, int depth = 0) {
    const network::Entry* e = doc.entry(local);
    if (e == nullptr) return {};
    const core::Interner& in = doc.interner();
    const auto comps = doc.components(*e);
    if (network::kind_of(in.str(e->type)) == network::NodeKind::complex && !comps.empty()) {
        if (depth > 8) return "[…]";
        std::string out;
        for (std::size_t i = 0; i < comps.size(); ++i) {
            if (i != 0) out += '+';
            const network::Entry* c = doc.entry(comps[i]);
            const bool nested = c != nullptr && network::kind_of(in.str(c->type)) == network::NodeKind::complex &&
                                !doc.components(*c).empty();
            out += nested ? "[" + entry_ids(doc, comps[i], depth + 1) + "]" : entry_ids(doc, comps[i], depth + 1);
        }
        return out;
    }
    std::vector<std::string_view> ids;
    for (const core::GeneId g : doc.ids(*e)) ids.push_back(in.str(g));
    std::sort(ids.begin(), ids.end(), detail::ByteLess{});
    std::string out;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) out += ' ';
        out += ids[i];
    }
    return out;
}

std::span<const Ref> refs(const std::vector<std::uint32_t>& first, const std::vector<Ref>& all, std::uint32_t i) {
    if (first.size() <= static_cast<std::size_t>(i) + 1) return {};
    return std::span<const Ref>(all).subspan(first[i], first[i + 1] - first[i]);
}

// The component of group entry g whose nodes include `component`: a box holding one of its ids, or a
// group that made it. Empty when none does.
std::optional<std::uint32_t> component_entry(const network::Network& net, const network::Provenance& p,
                                             std::uint32_t doc_index, const network::Entry& g, NodeIdx component) {
    const network::PathwayDoc& doc = *p.docs[doc_index];
    const core::Interner& in = doc.interner();
    for (const std::uint32_t c : doc.components(g)) {
        const network::Entry* e = doc.entry(c);
        if (e == nullptr) continue;
        if (network::kind_of(in.str(e->type)) == network::NodeKind::complex && !doc.components(*e).empty()) {
            for (const Ref& r : refs(p.node_first, p.node_refs, component))
                if (r.doc == doc_index && r.item == c && r.kind == RefKind::group) return c;
            continue;
        }
        for (const core::GeneId id : doc.ids(*e))
            if (const auto n = holder(net, in.str(id)); n && *n == component) return c;
    }
    return std::nullopt;
}

}  // namespace

void write_exceptions(const MergedNetwork& m, const BaseMerge& base, const network::Maps& maps, core::OutputFile& f) {
    exceptions_tsv(m, &base, maps, f);
}

void write_exceptions(const MergedNetwork& m, const network::Maps& maps, core::OutputFile& f) {
    exceptions_tsv(m, nullptr, maps, f);
}

// One row per merged edge and relation it came from, and one per component edge and group entry that
// made its complex. Nodes by key and label, pathways by id and name (a KEGG pathway's name is its KGML's
// root title, entry 205), and each relation's entries with their ids, so a family node's edge can be
// traced to the box that made it.
void write_edge_report(const MergedNetwork& m, const network::Maps& maps, core::OutputFile& file) {
    std::ostream& out = file.out();
    out << "source\ttarget\ttype\tsubtypes\tsource_label\ttarget_label\tpathway\tpathway_name\tentry1\tentry2\t"
           "entry1_ids\tentry2_ids\tscore\n";
    if (m.network == nullptr || m.network->provenance() == nullptr) return;
    const network::Network& net = *m.network;
    const network::Provenance& p = *net.provenance();
    const core::Interner& in = net.interner();
    const auto edges = net.edges();
    for (network::EdgeIdx e = 0; e < edges.size(); ++e) {
        const network::Edge& ed = edges[e];
        std::string subtypes;
        for (const core::Sym s : net.subtypes(e)) {
            if (!subtypes.empty()) subtypes += ',';
            subtypes += in.str(s);
        }
        const std::string head = net.node_key(ed.src).value + '\t' + net.node_key(ed.tgt).value + '\t' +
                                 std::string(in.str(ed.type)) + '\t' + subtypes + '\t' + net.label(ed.src, maps) +
                                 '\t' + net.label(ed.tgt, maps);
        for (const Ref& r : refs(p.edge_first, p.edge_refs, e)) {
            if (r.kind != RefKind::relation || r.doc >= p.docs.size()) continue;
            const network::PathwayDoc& doc = *p.docs[r.doc];
            if (r.item >= doc.relations().size()) continue;
            const network::Relation& rel = doc.relations()[r.item];
            const std::string& pathway = doc.source().pathway;
            out << head << '\t' << pathway << '\t' << net.pathway_label(pathway, maps) << '\t' << rel.entry1 << '\t'
                << rel.entry2 << '\t' << entry_ids(doc, rel.entry1) << '\t' << entry_ids(doc, rel.entry2) << '\t'
                << score_text(rel.score) << '\n';
        }
        if ((ed.flags & network::bit(network::EdgeFlag::component)) == 0) continue;
        // A component edge: the complex end is the one whose components hold the other end.
        const auto holds = [&net](NodeIdx cx, NodeIdx n) {
            const auto comps = net.components(cx);
            return net.kind(cx) == network::NodeKind::complex && std::find(comps.begin(), comps.end(), n) != comps.end();
        };
        const bool into_complex = holds(ed.tgt, ed.src);
        if (!into_complex && !holds(ed.src, ed.tgt)) continue;
        const NodeIdx cx = into_complex ? ed.tgt : ed.src;
        const NodeIdx part = into_complex ? ed.src : ed.tgt;
        for (const Ref& r : refs(p.node_first, p.node_refs, cx)) {
            if (r.kind != RefKind::group || r.doc >= p.docs.size()) continue;
            const network::PathwayDoc& doc = *p.docs[r.doc];
            const network::Entry* g = doc.entry(r.item);
            if (g == nullptr) continue;
            const std::optional<std::uint32_t> part_local = component_entry(net, p, r.doc, *g, part);
            const std::string part_entry = part_local ? std::to_string(*part_local) : std::string();
            const std::string part_ids = part_local ? entry_ids(doc, *part_local) : std::string();
            const std::string group_entry = std::to_string(r.item);
            const std::string group_ids = entry_ids(doc, r.item);
            const std::string& pathway = doc.source().pathway;
            out << head << '\t' << pathway << '\t' << net.pathway_label(pathway, maps) << '\t'
                << (into_complex ? part_entry : group_entry) << '\t' << (into_complex ? group_entry : part_entry)
                << '\t' << (into_complex ? part_ids : group_ids) << '\t' << (into_complex ? group_ids : part_ids)
                << "\t\n";
        }
    }
}

}  // namespace fusion::merge
