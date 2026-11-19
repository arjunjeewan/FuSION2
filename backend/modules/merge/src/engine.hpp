// The merge's engine (section 03, "The union", "Merge once, split afterwards"): the documents
// prepared once, the union of their boxes, complexes and edges through section 02's builder, the
// base merge and its split. Private to the merge module.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/merge/merge.hpp"
#include "fusion/network/network.hpp"

namespace fusion::merge::detail {

using network::NodeIdx;
using network::EdgeIdx;
using Ref = network::Provenance::Ref;
using RefKind = network::Provenance::Kind;

inline constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();

struct ByteLess {                                     // byte order, never the locale (09's determinism)
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return core::stable_compare(a, b) < 0;
    }
};
struct StableHash {                                   // core's fixed hash; lookups only, never an order
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return static_cast<std::size_t>(core::stable_hash(s));
    }
};
struct U32Hash {
    std::size_t operator()(std::uint32_t v) const noexcept { return v; }
};

std::uint16_t flag(network::EdgeFlag f) noexcept;

// The merge's own handles for the canonical ids of every document: one table per corpus, so the
// union-find works on dense integers whatever interner each document used.
class IdTable {
public:
    std::uint32_t intern(std::string_view id);
    std::optional<std::uint32_t> find(std::string_view id) const noexcept;
    const std::string& name(std::uint32_t h) const noexcept { return names_[h]; }
    std::size_t size() const noexcept { return names_.size(); }
private:
    std::vector<std::string> names_;
    std::unordered_map<std::string, std::uint32_t, StableHash, std::equal_to<>> index_;
};

struct PEntry {                                       // a reached entry (S2.06)
    std::uint32_t local_id = 0;
    const network::Entry* entry = nullptr;            // in the document, for split_entry
    bool group = false;
    std::string type;                                 // verbatim
    std::vector<std::uint32_t> ids;                   // a box's ids (IdTable handles), document order
    std::vector<std::uint32_t> comps;                 // a group's components: indices into PDoc::entries
    std::vector<std::uint32_t> parents;               // the groups holding this entry as a component
    std::uint32_t line = 0;                           // the first kept relation that reaches it
};
struct PRel {                                         // a kept relation (S2.11, S2.31, 02.C7)
    std::uint32_t index = 0;                          // in PathwayDoc::relations()
    std::uint32_t e1 = 0, e2 = 0;                     // indices into PDoc::entries
    std::string type;
    std::vector<std::string> subtypes;                // as the reader normalised them, without dummy_u_to_d
    std::vector<std::string> pathways;                // a KGML's root id; an edge table's row (02.C2)
    std::optional<double> score;                      // empty: unscored
    std::uint16_t flags = 0;                          // undirected; the builder adds ecrel and indirect
};
struct PDoc {
    DocPtr doc;
    std::string pathway;
    std::string file;                                 // for messages: the file name, else the pathway
    bool edge_table = false;
    std::vector<PEntry> entries;                      // reached entries, document order
    std::vector<PRel> rels;                           // kept relations, document order
    std::unordered_map<std::uint32_t, std::uint32_t, U32Hash> by_local;   // entry id -> index in entries
};
// The documents of one merge, prepared once: independent of the exception set.
struct Corpus {
    network::BuildSettings settings;
    IdTable ids;
    std::vector<PDoc> docs;                           // canonical order: pathway id, then SHA-256 (02.C4)
    std::vector<std::string> id_type;                 // by handle: the type of every box holding it
    std::uint32_t boxes = 0, groups = 0, relations = 0;
};
// Sorts the documents canonically, merges repeats of one document (same pathway and SHA-256), and fails
// on two documents with one pathway id (list.duplicate_pathway, 03.C6) and on a node-type conflict
// (merge.type_conflict, S2.19). Checks the stop token between documents when one is given.
std::shared_ptr<const Corpus> prepare(std::vector<DocPtr> docs, const network::BuildSettings&,
                                      const core::StopToken* stop);

// The key of parsed documents, from their own records: SHA-256, format, pathway and id map (03.C8).
MergeKey key_of_docs(std::span<const DocPtr> docs, const network::BuildSettings&);
// Two id-map digests name the same map; "no map" may be recorded as IdMap::none()'s digest or as zeros.
bool same_id_map(const core::Sha256&, const core::Sha256&);

// The fields of a network built from documents (NetworkInfo, 02.C4).
network::NetworkInfo info_for(const Corpus&, const ExceptionSet&, std::optional<network::MergeHeader>);

// A quiet context for the builder's finish() where the interface passes none (split, merge_direct).
class QuietContext {
public:
    QuietContext();
    ~QuietContext();
    QuietContext(const QuietContext&) = delete;
    QuietContext& operator=(const QuietContext&) = delete;
    core::Context& get() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// What a build leaves for a base merge: each id's node and each node's boxes, in final indices.
struct BaseTables {
    std::vector<NodeIdx> node_of_id;                  // by handle; none: in no node
    std::vector<std::uint32_t> boxes_first;           // CSR over final node indices
    std::vector<EntryRef> boxes;
};

// merge_direct over a prepared corpus (section 03, "The union", steps 1 to 7).
std::shared_ptr<const network::Network> build_direct(const Corpus&, const ExceptionSet&,
                                                     std::optional<network::MergeHeader>, core::Context&,
                                                     BaseTables* base_tables);

struct SplitResult {
    std::shared_ptr<const network::Network> network;
    SplitStats stats;
};
SplitResult build_split(const Corpus&, const network::Network& base, const BaseTables&, const ExceptionSet&,
                        network::MergeHeader, core::Context&);

}  // namespace fusion::merge::detail

namespace fusion::merge {

struct BaseMerge::Impl {
    MergeKey key;
    std::shared_ptr<const detail::Corpus> corpus;
    std::vector<DocPtr> docs;                         // the corpus's documents, canonical order
    std::shared_ptr<const network::Network> network;
    detail::BaseTables tables;
};

}  // namespace fusion::merge
