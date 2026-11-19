// The merge's engine (section 03, "The union", "Merge once, split afterwards").
//
// FuSION 1 read each KGML into its own graph (Linux/src/XMLParser.cc:621-722) and folded the graphs left
// to right, two at a time, through a chain of string maps (Linux/src/GraphManagerNew.cc:10572-10621,
// 9804-10432). 2.0 replaces the fold by one explicit, order-independent union of the boxes that share an
// id (S2.17, S2.18, S2.22), and feeds section 02's NetworkBuilder, which merges edges on identity, sets
// the tag once and sorts (S2.04, S2.20, S2.21, 02.C10).
#include "engine.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>

namespace fusion::merge::detail {

using network::EdgeFlag;

std::uint16_t flag(EdgeFlag f) noexcept { return network::bit(f); }

// ---------------------------------------------------------------- ids

std::uint32_t IdTable::intern(std::string_view id) {
    if (const auto it = index_.find(id); it != index_.end()) return it->second;
    const auto h = static_cast<std::uint32_t>(names_.size());
    names_.emplace_back(id);
    index_.emplace(names_.back(), h);
    return h;
}

std::optional<std::uint32_t> IdTable::find(std::string_view id) const noexcept {
    if (const auto it = index_.find(id); it != index_.end()) return it->second;
    return std::nullopt;
}

namespace {

// Union by rank with path halving: O(N α(N)) over the box-id incidences (section 03, step 3).
class UnionFind {
public:
    explicit UnionFind(std::size_t n) : parent_(n), rank_(n, 0) {
        std::iota(parent_.begin(), parent_.end(), std::uint32_t{0});
    }
    std::uint32_t find(std::uint32_t x) noexcept {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }
    void unite(std::uint32_t a, std::uint32_t b) noexcept {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }
private:
    std::vector<std::uint32_t> parent_;
    std::vector<std::uint8_t> rank_;
};

std::string file_label(const network::SourceRef& s) {
    if (s.path && !s.path->empty()) return core::to_utf8(s.path->filename());
    return s.pathway;
}

std::string where_file(const network::SourceRef& s) {
    if (s.path && !s.path->empty()) return core::to_utf8(*s.path);
    return s.pathway;
}

// A group entry is one whose type is `group` and that lists components; FuSION 1 looked for components
// only under that type (Linux/src/XMLParser.cc:85-102), and section 02's reader rejects an empty group.
bool is_group(const network::PathwayDoc& doc, const network::Entry& e) {
    return network::kind_of(doc.interner().str(e.type)) == network::NodeKind::complex &&
           !doc.components(e).empty();
}

// The subtypes that make a relation undirected (S2.10; Linux/src/XMLParser.cc:1476-1478).
bool undirected_subtype(std::string_view s) noexcept {
    return s == "association" || s == "dissociation" || s == "mapping";
}

std::string_view article(std::string_view word) noexcept {
    if (!word.empty() && std::string_view("aeiouAEIOU").find(word.front()) != std::string_view::npos) return "an";
    return "a";
}

// One document, prepared: its kept relations, the entries they reach and each reached entry's ids.
PDoc prepare_doc(const DocPtr& ptr, Corpus& corpus) {
    const network::PathwayDoc& doc = *ptr;
    const core::Interner& in = doc.interner();
    PDoc pd;
    pd.doc = ptr;
    pd.pathway = doc.source().pathway;
    pd.file = file_label(doc.source());
    pd.edge_table = doc.source().format == network::Format::edge_table;

    // Reached entries (S2.06): an entry becomes a node only if a kept relation reaches it, directly or
    // as a component of a reached group; filtered relations make no nodes (S2.11, applied before nodes
    // are made, unlike FuSION 1's discard at Linux/src/XMLParser.cc:1481-1490).
    const std::vector<std::uint32_t> kept = network::kept_relations(doc, corpus.settings.filters);
    const auto relations = doc.relations();
    std::unordered_map<std::uint32_t, std::uint32_t, U32Hash> first_line;   // entry id -> line
    std::vector<std::uint32_t> stack;
    const auto reach = [&](std::uint32_t root, std::uint32_t line) {
        stack.push_back(root);
        while (!stack.empty()) {
            const std::uint32_t local = stack.back();
            stack.pop_back();
            if (!first_line.emplace(local, line).second) continue;
            const network::Entry* e = doc.entry(local);
            if (e != nullptr && is_group(doc, *e))
                for (const std::uint32_t c : doc.components(*e)) stack.push_back(c);
        }
    };
    for (const std::uint32_t ri : kept) {
        reach(relations[ri].entry1, relations[ri].line);
        reach(relations[ri].entry2, relations[ri].line);
    }

    for (const network::Entry& e : doc.entries()) {
        const auto it = first_line.find(e.local_id);
        if (it == first_line.end() || pd.by_local.contains(e.local_id)) continue;
        PEntry pe;
        pe.local_id = e.local_id;
        pe.entry = &e;
        pe.type = std::string(in.str(e.type));
        pe.group = is_group(doc, e);
        pe.line = it->second;
        if (!pe.group) {
            for (const core::GeneId g : doc.ids(e)) {
                const std::uint32_t h = corpus.ids.intern(in.str(g));
                if (std::find(pe.ids.begin(), pe.ids.end(), h) == pe.ids.end()) pe.ids.push_back(h);
            }
            ++corpus.boxes;
        } else {
            ++corpus.groups;
        }
        pd.by_local.emplace(e.local_id, static_cast<std::uint32_t>(pd.entries.size()));
        pd.entries.push_back(std::move(pe));
    }
    for (std::uint32_t i = 0; i < pd.entries.size(); ++i) {
        if (!pd.entries[i].group) continue;
        for (const std::uint32_t c : doc.components(*pd.entries[i].entry)) {
            const auto it = pd.by_local.find(c);
            if (it == pd.by_local.end()) continue;
            pd.entries[i].comps.push_back(it->second);
            auto& parents = pd.entries[it->second].parents;
            if (std::find(parents.begin(), parents.end(), i) == parents.end()) parents.push_back(i);
        }
    }

    // Kept relations, in document order, with the reader's normalised subtypes (S2.10).
    for (const std::uint32_t ri : kept) {
        const network::Relation& r = relations[ri];
        const auto a = pd.by_local.find(r.entry1);
        const auto b = pd.by_local.find(r.entry2);
        if (a == pd.by_local.end() || b == pd.by_local.end()) continue;   // the reader rejects these (S2.06)
        PRel pr;
        pr.index = ri;
        pr.e1 = a->second;
        pr.e2 = b->second;
        pr.type = std::string(in.str(r.type));
        for (const core::Sym s : doc.subtypes(r)) {
            const std::string_view t = in.str(s);
            if (t == "dummy_u_to_d") continue;                         // a flag in 2.0 (S2.21)
            if (undirected_subtype(t)) pr.flags |= flag(EdgeFlag::undirected);
            pr.subtypes.emplace_back(t);
        }
        // A KGML's relations carry its root's pathway id (FuSION 1: XMLParser.cc:1543); an edge table's
        // rows carry their own, by default the file's stem (02.C2).
        for (const core::Sym p : doc.pathways(r)) pr.pathways.emplace_back(in.str(p));
        if (pr.pathways.empty()) pr.pathways.push_back(pd.pathway);
        pr.score = r.score;
        pd.rels.push_back(std::move(pr));
        ++corpus.relations;
    }
    return pd;
}

std::string box_position(const PDoc& d, const PEntry& e) {
    if (d.edge_table) return d.file + ", line " + std::to_string(e.line);
    return d.file + ", entry " + std::to_string(e.local_id);
}

}  // namespace

std::shared_ptr<const Corpus> prepare(std::vector<DocPtr> docs, const network::BuildSettings& settings,
                                      const core::StopToken* stop) {
    // Canonical order (02.C4: pathway id, then SHA-256), so nothing depends on the list's order (S2.17).
    std::sort(docs.begin(), docs.end(), [](const DocPtr& a, const DocPtr& b) {
        const auto c = core::stable_compare(a->source().pathway, b->source().pathway);
        if (c != 0) return c < 0;
        return a->source().sha256 < b->source().sha256;
    });
    // One file listed twice is merged once (03.C6); two different files with one pathway id fail,
    // naming both, as the one list reader fails for gsea and mff alike (list.duplicate_pathway, 1.24).
    std::vector<DocPtr> unique;
    unique.reserve(docs.size());
    for (DocPtr& d : docs) {
        if (!unique.empty() && unique.back()->source().pathway == d->source().pathway) {
            if (unique.back()->source().sha256 == d->source().sha256) continue;
            const auto& a = unique.back()->source();
            const auto& b = d->source();
            throw core::Failure("list.duplicate_pathway",
                                "two files declare pathway " + a.pathway + ": " + where_file(a) + " and " +
                                    where_file(b),
                                core::Where{where_file(b), 0, {}});
        }
        unique.push_back(std::move(d));
    }

    auto corpus = std::make_shared<Corpus>();
    corpus->settings = settings;
    corpus->docs.reserve(unique.size());
    for (const DocPtr& d : unique) {
        if (stop != nullptr) stop->throw_if_stopped();   // between documents (section 03, "Errors")
        corpus->docs.push_back(prepare_doc(d, *corpus));
    }

    // The type table (S2.19): each id takes the type of the first box that holds it, in canonical
    // document order and then document order; a box of another type stops the run. FuSION 1 only
    // warned and dropped the whole later pathway (Linux/src/GraphManagerNew.cc:10097-10115).
    corpus->id_type.assign(corpus->ids.size(), std::string());
    std::vector<std::pair<std::uint32_t, std::uint32_t>> origin(corpus->ids.size(), {none, none});
    for (std::uint32_t di = 0; di < corpus->docs.size(); ++di) {
        const PDoc& d = corpus->docs[di];
        for (std::uint32_t ei = 0; ei < d.entries.size(); ++ei) {
            const PEntry& e = d.entries[ei];
            if (e.group) continue;
            for (const std::uint32_t h : e.ids) {
                if (origin[h].first == none) {
                    origin[h] = {di, ei};
                    corpus->id_type[h] = e.type;
                    continue;
                }
                if (corpus->id_type[h] == e.type) continue;
                const PDoc& d0 = corpus->docs[origin[h].first];
                const PEntry& e0 = d0.entries[origin[h].second];
                const std::string& id = corpus->ids.name(h);
                throw core::Failure(
                    "merge.type_conflict",
                    "node-type conflict: " + id + " is " + std::string(article(e0.type)) + " " + e0.type + " in " +
                        d0.pathway + " (" + box_position(d0, e0) + ") and " + std::string(article(e.type)) + " " +
                        e.type + " in " + d.pathway + " (" + box_position(d, e) + ")",
                    core::Where{where_file(d.doc->source()), d.edge_table ? e.line : 0, {}});
            }
        }
    }
    return corpus;
}

network::NetworkInfo info_for(const Corpus& c, const ExceptionSet& exceptions,
                              std::optional<network::MergeHeader> header) {
    network::NetworkInfo info;
    bool kgml = false, edges = false;
    for (const PDoc& d : c.docs) (d.edge_table ? edges : kgml) = true;
    info.reader = kgml && edges ? "kgml+edges" : edges ? "edges" : "kgml";
    for (const PDoc& d : c.docs) info.sources.push_back(d.doc->source());
    info.settings = c.settings;
    const auto ids = exceptions.ids().ids();
    info.exceptions.assign(ids.begin(), ids.end());
    info.id_map = c.docs.empty() ? network::IdMap::none().digest() : c.docs.front().doc->id_map();
    info.legacy = false;
    info.merge = std::move(header);
    return info;
}

// ---------------------------------------------------------------- a quiet context

namespace {

class NullEvents final : public core::Events {
public:
    void emit(std::string_view, core::Json) override {}
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
};
class NullJournal final : public core::Journal {
public:
    void append(const core::Json&) override {}
};
class NullFiles final : public core::Files {
public:
    std::filesystem::path input(const std::filesystem::path& as_named) override { return as_named; }
    core::OutputFile create(std::string_view name) override { refuse(name); }
    core::OutputFile create_debug(std::string_view name) override { refuse(name); }
    core::Journal& journal(std::string_view) override { return journal_; }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    std::filesystem::path step_dir() const override { return {}; }
private:
    [[noreturn]] static void refuse(std::string_view name) {
        throw core::Failure("io.write", "no step directory to write " + std::string(name) + " in");
    }
    NullJournal journal_;
};

}  // namespace

struct QuietContext::Impl {
    core::Log log{false};
    NullEvents events;
    core::Settings settings;
    NullFiles files;
    core::Context context{log, events, core::StopToken{}, settings, files, nullptr, "merge"};
};

QuietContext::QuietContext() : impl_(std::make_unique<Impl>()) {}
QuietContext::~QuietContext() = default;
core::Context& QuietContext::get() noexcept { return impl_->context; }

// ---------------------------------------------------------------- driving the builder

namespace {

bool ref_less(const Ref& a, const Ref& b) noexcept {
    if (a.doc != b.doc) return a.doc < b.doc;
    if (a.item != b.item) return a.item < b.item;
    return a.kind < b.kind;
}
bool ref_equal(const Ref& a, const Ref& b) noexcept {
    return a.doc == b.doc && a.item == b.item && a.kind == b.kind;
}

// One NetworkBuilder, and what the merge records beside it: each node's key, and the provenance of
// every edge and complex (2B.11), indexed by the builder's indices until finish() sorts them.
class Assembler {
public:
    explicit Assembler(network::NetworkInfo info)
        : builder_(std::make_unique<network::NetworkBuilder>(std::move(info), interner_)) {}
    Assembler(const Assembler&) = delete;
    Assembler& operator=(const Assembler&) = delete;

    // A gene or compound node; its key is its smallest member (S2.07, S2.26).
    NodeIdx node(std::string_view type, std::vector<std::string_view> members) {
        std::sort(members.begin(), members.end(), ByteLess{});
        std::vector<core::GeneId> ids;
        ids.reserve(members.size());
        for (const std::string_view m : members) ids.push_back(interner_.id(m));
        const NodeIdx n = builder_->add_node(interner_.sym(type), ids);
        record(n, std::string(members.front()), false, static_cast<std::uint32_t>(members.size()));
        return n;
    }

    // A node of the base merge, with its members or components, its marks and the group entries behind
    // it, through the builder's copy (2B.11).
    NodeIdx copy_node(const network::Network& from, NodeIdx n) {
        const NodeIdx c = builder_->copy_node(from, n);
        const bool is_complex = from.kind(n) == network::NodeKind::complex;
        record(c, from.node_key(n).value, is_complex, static_cast<std::uint32_t>(from.members(n).size()));
        if (is_complex) {
            std::vector<NodeIdx> comps;
            for (const NodeIdx k : from.components(n)) comps.push_back(copy_node(from, k));
            std::sort(comps.begin(), comps.end(),
                      [this](NodeIdx a, NodeIdx b) { return core::stable_compare(keys_[a], keys_[b]) < 0; });
            comps.erase(std::unique(comps.begin(), comps.end()), comps.end());
            complexes_.try_emplace(std::move(comps), c);
        }
        return c;
    }

    // An edge of the base merge, with its flags, pathways, score and the relations behind it (2B.11).
    void copy_edge(const network::Network& from, EdgeIdx e, NodeIdx src, NodeIdx tgt) {
        builder_->copy_edge(from, e, src, tgt);
    }

    // A complex: the distinct component nodes of one combination, keyed c_s_<keys>_c_e (S2.09). One
    // complex per component set, which replaces FuSION 1's scan over every node
    // (Linux/src/GraphManagerNew.cc:2421-2436).
    NodeIdx complex(std::vector<NodeIdx> comps) {
        std::sort(comps.begin(), comps.end(),
                  [this](NodeIdx a, NodeIdx b) { return core::stable_compare(keys_[a], keys_[b]) < 0; });
        comps.erase(std::unique(comps.begin(), comps.end()), comps.end());
        const auto [it, fresh] = complexes_.try_emplace(comps, none);
        if (!fresh) return it->second;
        const NodeIdx c = builder_->add_complex(comps);
        std::string key = "c_s_";
        for (std::size_t i = 0; i < comps.size(); ++i) {
            if (i != 0) key += '_';
            key += keys_[comps[i]];
        }
        key += "_c_e";
        record(c, std::move(key), true, 1);
        it->second = c;
        return c;
    }

    EdgeIdx edge(NodeIdx s, NodeIdx t, std::string_view type, std::span<const std::string> subtypes,
                 std::span<const std::string> pathways, std::optional<double> score, std::uint16_t flags) {
        std::vector<core::Sym> subs, paths;
        subs.reserve(subtypes.size());
        paths.reserve(pathways.size());
        for (const std::string& x : subtypes) subs.push_back(interner_.sym(x));
        for (const std::string& p : pathways) paths.push_back(interner_.sym(p));
        // The builder sets ecrel, indirect, self_loop and unscored itself; the caller passes undirected.
        return builder_->add_edge(s, t, interner_.sym(type), subs, paths, score, flags);
    }

    void component_edges(NodeIdx complex, std::string_view pathway) {   // S2.09: the group's own pathway
        builder_->add_component_edges(complex, interner_.sym(pathway));
    }

    void node_ref(NodeIdx n, Ref r) { grow(node_refs_, n).push_back(r); }
    void edge_ref(EdgeIdx e, Ref r) { grow(edge_refs_, e).push_back(r); }

    // Every single-id node whose id is in E is an exception node (S2.13).
    void mark_exceptions(const ExceptionSet& exceptions) {
        for (NodeIdx n = 0; n < keys_.size(); ++n)
            if (!complex_[n] && members_[n] == 1 && exceptions.contains(keys_[n])) builder_->mark_exception(n);
    }

    const std::string& key(NodeIdx n) const noexcept { return keys_[n]; }
    std::size_t node_slots() const noexcept { return keys_.size(); }
    bool known(NodeIdx n) const noexcept { return n < keys_.size() && !keys_[n].empty(); }

    std::shared_ptr<const network::Network> finish(std::vector<DocPtr> docs, core::Context& ctx) {
        network::Provenance p;
        p.docs = std::move(docs);
        csr(node_refs_, keys_.size(), p.node_first, p.node_refs);
        csr(edge_refs_, edge_refs_.size(), p.edge_first, p.edge_refs);
        builder_->attach(std::move(p));
        auto net = std::make_shared<const network::Network>(std::move(*builder_).finish(ctx));
        builder_.reset();
        return net;
    }

private:
    void record(NodeIdx n, std::string key, bool is_complex, std::uint32_t members) {
        if (keys_.size() <= n) {
            keys_.resize(n + 1);
            complex_.resize(n + 1, false);
            members_.resize(n + 1, 0);
        }
        keys_[n] = std::move(key);
        complex_[n] = is_complex;
        members_[n] = members;
    }
    static std::vector<Ref>& grow(std::vector<std::vector<Ref>>& table, std::uint32_t i) {
        if (table.size() <= i) table.resize(i + 1);
        return table[i];
    }
    static void csr(std::vector<std::vector<Ref>>& table, std::size_t slots, std::vector<std::uint32_t>& first,
                    std::vector<Ref>& refs) {
        table.resize(std::max(table.size(), slots));
        first.assign(1, 0);
        for (std::vector<Ref>& list : table) {
            std::sort(list.begin(), list.end(), ref_less);
            list.erase(std::unique(list.begin(), list.end(), ref_equal), list.end());
            refs.insert(refs.end(), list.begin(), list.end());
            first.push_back(static_cast<std::uint32_t>(refs.size()));
        }
    }

    core::Interner interner_;                         // the builder's handles; declared before it
    std::unique_ptr<network::NetworkBuilder> builder_;
    std::vector<std::string> keys_;
    std::vector<bool> complex_;
    std::vector<std::uint32_t> members_;
    std::map<std::vector<NodeIdx>, NodeIdx> complexes_;
    std::vector<std::vector<Ref>> node_refs_, edge_refs_;
};

// Steps 5 and 6 of the union over one corpus, with the node of each id given by node_of: a group's
// complexes and component edges, and a relation's fan-out (S2.09, S2.10).
class Engine {
public:
    Engine(const Corpus& c, const ExceptionSet& e, Assembler& a) : c_(c), e_(e), a_(a) {
        units_.resize(c.docs.size());
        sets_.resize(c.docs.size());
        for (std::size_t i = 0; i < c.docs.size(); ++i) {
            units_[i].resize(c.docs[i].entries.size());
            sets_[i].resize(c.docs[i].entries.size());
        }
    }

    std::function<NodeIdx(std::uint32_t)> node_of;   // id handle -> node

    // The units of a box under E (S2.07, as section 02's split_entry computes them): each id of E alone,
    // the rest as one unit when the box holds more ids than the threshold, counting E's ids
    // (Linux/src/XMLParser.cc:899-900); otherwise every id alone.
    const std::vector<std::vector<std::uint32_t>>& units(std::uint32_t di, std::uint32_t ei) {
        auto& slot = units_[di][ei];
        if (slot) return *slot;
        const PDoc& d = c_.docs[di];
        const network::EntrySplit s =
            network::split_entry(*d.doc, *d.entries[ei].entry, e_.ids(), c_.settings.split_threshold);
        const core::Interner& in = d.doc->interner();
        std::vector<std::vector<std::uint32_t>> out;
        for (const core::GeneId g : s.singles)
            if (const auto h = c_.ids.find(in.str(g))) out.push_back({*h});
        std::vector<std::uint32_t> box;
        for (const core::GeneId g : s.box)
            if (const auto h = c_.ids.find(in.str(g)); h && std::find(box.begin(), box.end(), *h) == box.end())
                box.push_back(*h);
        if (!box.empty()) out.push_back(std::move(box));
        slot = std::move(out);
        return *slot;
    }

    // An entry's nodes: the nodes of a box's units (S2.08: every id of a box joins its node), or a group's
    // complexes, one per combination of its components' nodes (S2.09; Linux/src/XMLParser.cc:42-62,
    // 1078-1125). A group runs once: its complexes, their group entries and their component edges.
    std::vector<NodeIdx> nodes(std::uint32_t di, std::uint32_t ei) {
        auto& slot = sets_[di][ei];
        if (slot) return *slot;
        const PEntry& e = c_.docs[di].entries[ei];
        std::vector<NodeIdx> out;
        if (!e.group) {
            for (const auto& u : units(di, ei)) out.push_back(node_of(u.front()));
        } else {
            std::vector<std::vector<NodeIdx>> combos(1);
            for (const std::uint32_t ci : e.comps) {
                const std::vector<NodeIdx> options = nodes(di, ci);
                std::vector<std::vector<NodeIdx>> next;
                next.reserve(combos.size() * options.size());
                for (const auto& combo : combos)
                    for (const NodeIdx n : options) {
                        next.push_back(combo);
                        next.back().push_back(n);
                    }
                combos = std::move(next);
            }
            for (auto& combo : combos)
                if (!combo.empty()) out.push_back(a_.complex(std::move(combo)));
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        if (e.group)
            for (const NodeIdx cx : out) {
                a_.node_ref(cx, Ref{di, e.local_id, RefKind::group});
                a_.component_edges(cx, c_.docs[di].pathway);
            }
        slot = out;
        return out;
    }

    // A kept relation fans out over its two entries' nodes; an undirected one also adds the reverse
    // edge, except on a self-loop (S2.10; Linux/src/XMLParser.cc:682-713, 1451-1602). The builder merges
    // repeats on identity, unions pathways and keeps the larger score (S2.04, S2.10, S2.20).
    void relation(std::uint32_t di, std::uint32_t ri) {
        const PDoc& d = c_.docs[di];
        const PRel& r = d.rels[ri];
        const std::vector<NodeIdx> sources = nodes(di, r.e1);
        const std::vector<NodeIdx> targets = nodes(di, r.e2);
        const Ref ref{di, r.index, RefKind::relation};
        const bool undirected = (r.flags & flag(EdgeFlag::undirected)) != 0;
        for (const NodeIdx s : sources)
            for (const NodeIdx t : targets) {
                a_.edge_ref(a_.edge(s, t, r.type, r.subtypes, r.pathways, r.score, r.flags), ref);
                if (undirected && s != t)
                    a_.edge_ref(a_.edge(t, s, r.type, r.subtypes, r.pathways, r.score, r.flags), ref);
            }
    }

private:
    const Corpus& c_;
    const ExceptionSet& e_;
    Assembler& a_;
    std::vector<std::vector<std::optional<std::vector<std::vector<std::uint32_t>>>>> units_;
    std::vector<std::vector<std::optional<std::vector<NodeIdx>>>> sets_;
};

// Nodes from union-find components: members sorted, key the smallest member (step 4). Components are
// made in key order, so the builder sees the same calls whatever the input order.
std::vector<NodeIdx> make_nodes(const Corpus& c, UnionFind& uf, const std::vector<std::uint32_t>& handles,
                                Assembler& a) {
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>, U32Hash> by_root;
    for (const std::uint32_t h : handles) by_root[uf.find(h)].push_back(h);
    std::vector<std::vector<std::string_view>> groups;
    groups.reserve(by_root.size());
    for (auto& [root, members] : by_root) {
        std::vector<std::string_view> names;
        names.reserve(members.size());
        for (const std::uint32_t h : members) names.push_back(c.ids.name(h));
        std::sort(names.begin(), names.end(), ByteLess{});
        groups.push_back(std::move(names));
    }
    std::sort(groups.begin(), groups.end(),
              [](const auto& x, const auto& y) { return core::stable_compare(x.front(), y.front()) < 0; });
    std::vector<NodeIdx> node_of(c.ids.size(), none);
    for (const auto& names : groups) {
        const std::uint32_t first = *c.ids.find(names.front());
        const NodeIdx n = a.node(c.id_type[first], names);
        for (const std::string_view m : names) node_of[*c.ids.find(m)] = n;
    }
    return node_of;
}

std::vector<DocPtr> docs_of(const Corpus& c) {
    std::vector<DocPtr> out;
    out.reserve(c.docs.size());
    for (const PDoc& d : c.docs) out.push_back(d.doc);
    return out;
}

NodeIdx final_index(const network::Network& net, const std::string& key) {
    if (const auto n = net.find_node(network::NodeKey{key})) return *n;
    throw core::Failure("internal", "node " + key + " is missing from the built network");
}

}  // namespace

// ---------------------------------------------------------------- merge_direct (steps 1 to 7)

std::shared_ptr<const network::Network> build_direct(const Corpus& c, const ExceptionSet& exceptions,
                                                     std::optional<network::MergeHeader> header,
                                                     core::Context& ctx, BaseTables* tables) {
    Assembler a(info_for(c, exceptions, std::move(header)));
    Engine engine(c, exceptions, a);

    // Step 3: the ids of each unit are joined (S2.17: boxes that share an id are unioned, within and
    // across documents, which also gives S2.08's fix by construction).
    UnionFind uf(c.ids.size());
    for (std::uint32_t di = 0; di < c.docs.size(); ++di)
        for (std::uint32_t ei = 0; ei < c.docs[di].entries.size(); ++ei) {
            if (c.docs[di].entries[ei].group) continue;
            for (const auto& u : engine.units(di, ei))
                for (std::size_t k = 1; k < u.size(); ++k) uf.unite(u[0], u[k]);
        }
    // Step 4: one node per component.
    std::vector<std::uint32_t> all(c.ids.size());
    std::iota(all.begin(), all.end(), std::uint32_t{0});
    const std::vector<NodeIdx> node_of = make_nodes(c, uf, all, a);
    engine.node_of = [&node_of](std::uint32_t h) { return node_of[h]; };

    // Steps 5 and 6: every reached group's complexes, every kept relation's edges.
    for (std::uint32_t di = 0; di < c.docs.size(); ++di) {
        for (std::uint32_t ei = 0; ei < c.docs[di].entries.size(); ++ei)
            if (c.docs[di].entries[ei].group) engine.nodes(di, ei);
        for (std::uint32_t ri = 0; ri < c.docs[di].rels.size(); ++ri) engine.relation(di, ri);
    }
    a.mark_exceptions(exceptions);

    // Step 7: the builder tags once and sorts (S2.21, 02.C10).
    auto net = a.finish(docs_of(c), ctx);

    if (tables != nullptr) {                          // for the base merge: ids and boxes by final node
        std::vector<NodeIdx> final_of(a.node_slots(), none);
        for (NodeIdx n = 0; n < a.node_slots(); ++n)
            if (a.known(n)) final_of[n] = final_index(*net, a.key(n));
        tables->node_of_id.assign(c.ids.size(), none);
        for (std::uint32_t h = 0; h < c.ids.size(); ++h)
            if (node_of[h] != none) tables->node_of_id[h] = final_of[node_of[h]];
        std::vector<std::vector<EntryRef>> boxes(net->nodes().size());
        for (std::uint32_t di = 0; di < c.docs.size(); ++di)
            for (std::uint32_t ei = 0; ei < c.docs[di].entries.size(); ++ei) {
                const PEntry& e = c.docs[di].entries[ei];
                if (e.group) continue;
                for (const auto& u : engine.units(di, ei)) {
                    auto& list = boxes[tables->node_of_id[u.front()]];
                    if (list.empty() || list.back().doc != di || list.back().entry != e.local_id)
                        list.push_back(EntryRef{di, e.local_id});
                }
            }
        tables->boxes_first.assign(1, 0);
        tables->boxes.clear();
        for (const auto& list : boxes) {
            tables->boxes.insert(tables->boxes.end(), list.begin(), list.end());
            tables->boxes_first.push_back(static_cast<std::uint32_t>(tables->boxes.size()));
        }
    }
    return net;
}

// ---------------------------------------------------------------- the split (S2.22)

SplitResult build_split(const Corpus& c, const network::Network& base, const BaseTables& t,
                        const ExceptionSet& exceptions, network::MergeHeader header, core::Context& ctx) {
    SplitStats st{};
    st.exception_ids = static_cast<std::uint32_t>(exceptions.size());
    const core::Interner& in0 = base.interner();
    const std::size_t node_count = base.nodes().size();

    // 1. A: the base nodes with two or more members that hold an id of E.
    std::vector<char> in_a(node_count, 0);
    for (const std::string& id : exceptions.ids().ids()) {
        const auto h = c.ids.find(id);
        if (!h || t.node_of_id[*h] == none) continue;   // in no box: listed in exceptions.tsv only
        ++st.in_network;
        const NodeIdx n = t.node_of_id[*h];
        if (base.members(n).size() >= 2) {
            ++st.split_out;
            if (in_a[n] == 0) ++st.affected_nodes;
            in_a[n] = 1;
        }
    }

    // 2. The affected boxes (a unit in A), groups (an affected component, to a fixpoint) and relations
    //    (an affected entry).
    std::vector<std::vector<char>> affected(c.docs.size());
    for (std::size_t di = 0; di < c.docs.size(); ++di) affected[di].assign(c.docs[di].entries.size(), 0);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> queue;
    for (NodeIdx n = 0; n < node_count; ++n) {
        if (in_a[n] == 0) continue;
        for (std::uint32_t i = t.boxes_first[n]; i < t.boxes_first[n + 1]; ++i) {
            const EntryRef& r = t.boxes[i];
            const std::uint32_t ei = c.docs[r.doc].by_local.at(r.entry);
            if (affected[r.doc][ei] != 0) continue;
            affected[r.doc][ei] = 1;
            ++st.affected_boxes;
            queue.emplace_back(r.doc, ei);
        }
    }
    for (std::size_t q = 0; q < queue.size(); ++q) {
        const auto [di, ei] = queue[q];
        for (const std::uint32_t g : c.docs[di].entries[ei].parents) {
            if (affected[di][g] != 0) continue;
            affected[di][g] = 1;
            ++st.affected_groups;
            queue.emplace_back(di, g);
        }
    }
    std::vector<std::vector<char>> relation_affected(c.docs.size());
    for (std::size_t di = 0; di < c.docs.size(); ++di) {
        relation_affected[di].assign(c.docs[di].rels.size(), 0);
        for (std::size_t ri = 0; ri < c.docs[di].rels.size(); ++ri) {
            const PRel& r = c.docs[di].rels[ri];
            if (affected[di][r.e1] != 0 || affected[di][r.e2] != 0) {
                relation_affected[di][ri] = 1;
                ++st.affected_relations;
            }
        }
    }

    // Persisting base nodes: outside A, and complexes whose components all persist. A base node without
    // ids of E is made only of boxes without ids of E, whose units are the same under E, so it reappears
    // unchanged; so do the complexes and edges that touch only such nodes.
    std::vector<signed char> persist(node_count, -1);
    std::function<bool(NodeIdx)> persists = [&](NodeIdx n) -> bool {
        if (persist[n] >= 0) return persist[n] != 0;
        bool p = in_a[n] == 0;
        if (base.kind(n) == network::NodeKind::complex)
            for (const NodeIdx k : base.components(n)) p = persists(k) && p;
        persist[n] = p ? 1 : 0;
        return p;
    };

    Assembler a(info_for(c, exceptions, std::move(header)));
    Engine engine(c, exceptions, a);

    // 4 (copy). Every persisting node and complex, and every edge between persisting nodes, comes from
    // the base merge through the builder's copy_node and copy_edge, which keep each one's provenance
    // (2B.11): component edges and the group entries behind each complex included.
    std::vector<NodeIdx> copied(node_count, none);
    for (NodeIdx n = 0; n < node_count; ++n)
        if (persists(n)) copied[n] = a.copy_node(base, n);
    const auto edges = base.edges();
    for (EdgeIdx e = 0; e < edges.size(); ++e)
        if (persists(edges[e].src) && persists(edges[e].tgt)) a.copy_edge(base, e, copied[edges[e].src], copied[edges[e].tgt]);

    // 3. The ids of A are joined again from the affected boxes' units under E. A box that does not
    //    collapse can have units in several nodes; its units outside A keep their base nodes.
    std::vector<char> id_in_a(c.ids.size(), 0);
    std::vector<std::uint32_t> a_ids;
    for (NodeIdx n = 0; n < node_count; ++n) {
        if (in_a[n] == 0) continue;
        for (const core::GeneId g : base.members(n))
            if (const auto h = c.ids.find(in0.str(g))) {
                id_in_a[*h] = 1;
                a_ids.push_back(*h);
            }
    }
    UnionFind uf(c.ids.size());
    for (std::uint32_t di = 0; di < c.docs.size(); ++di)
        for (std::uint32_t ei = 0; ei < c.docs[di].entries.size(); ++ei) {
            if (affected[di][ei] == 0 || c.docs[di].entries[ei].group) continue;
            for (const auto& u : engine.units(di, ei))
                for (std::size_t k = 1; k < u.size(); ++k) uf.unite(u[0], u[k]);
        }
    const std::vector<NodeIdx> fresh = make_nodes(c, uf, a_ids, a);
    engine.node_of = [&](std::uint32_t h) { return id_in_a[h] != 0 ? fresh[h] : copied[t.node_of_id[h]]; };

    // 4 (recompute). Steps 5 and 6 for the affected groups and relations, with the code merge_direct runs.
    for (std::uint32_t di = 0; di < c.docs.size(); ++di) {
        for (std::uint32_t ei = 0; ei < c.docs[di].entries.size(); ++ei)
            if (affected[di][ei] != 0 && c.docs[di].entries[ei].group) engine.nodes(di, ei);
        for (std::uint32_t ri = 0; ri < c.docs[di].rels.size(); ++ri)
            if (relation_affected[di][ri] != 0) engine.relation(di, ri);
    }
    // 5. Every single-id node whose id is in E, copied ones included.
    a.mark_exceptions(exceptions);
    return SplitResult{a.finish(docs_of(c), ctx), st};
}

}  // namespace fusion::merge::detail
