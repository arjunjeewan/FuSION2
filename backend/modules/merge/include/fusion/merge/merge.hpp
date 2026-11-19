// fusion/merge/merge.hpp: the exception set, the union of pathway documents into one network, merge
// keys, base merges and their split, reuse, the reports and the mff step (section 03, "Interface").
// The declarations are section 03's outline. What this header adds to it is marked "(added)".
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

namespace fusion::merge {

// ---------- the exception set (S1.17, S2.13, S8.22) ----------
enum class Reason : std::uint16_t {
    up = 1u << 0, down = 1u << 1, confirmed_up = 1u << 2, confirmed_down = 1u << 3,
    source = 1u << 4, target = 1u << 5, fsc = 1u << 6, not_merged = 1u << 7, start_file = 1u << 8,
};
std::string_view reason_name(Reason) noexcept;                 // (added) "up", "confirmed_up", ... (exceptions.tsv)
std::string reason_names(std::uint16_t reasons);               // (added) "up,target": the bits in bit order

struct ExceptionInputs;
class ExceptionSet {
public:
    ExceptionSet() = default;                                   // (added) empty, and bound to no interner
    explicit ExceptionSet(const core::Interner& ids);           // (added) the interner behind every GeneId this
                                                                // set is given or asked about; it must outlive them
    void add(core::GeneId, Reason);
    void add(std::string_view id, Reason);                      // (added) a canonical id as text (02.C3)
    bool contains(core::GeneId) const noexcept;
    bool contains(std::string_view id) const noexcept;          // (added)
    std::uint16_t reasons(core::GeneId) const noexcept;        // bit set of Reason
    std::uint16_t reasons(std::string_view id) const noexcept;  // (added)
    const network::IdSet& ids() const noexcept;                // what split_entry takes
    core::Sha256 digest() const;                               // over the sorted ids, for run.json
    std::size_t size() const noexcept;                          // (added)
    bool empty() const noexcept;                                // (added)

private:
    struct ByteLess {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept {
            return core::stable_compare(a, b) < 0;
        }
    };
    friend ExceptionSet assemble_exceptions(const ExceptionInputs&, const network::IdMap&, core::Context&);
    bool insert(std::string_view id, Reason);                   // without rebuilding ids_; true: a new id
    void rebuild();

    const core::Interner* interner_ = nullptr;
    std::map<std::string, std::uint16_t, ByteLess> reasons_;   // byte order (core::stable_compare)
    network::IdSet ids_;
};
struct ExceptionInputs {                                       // filled by section 01's handler
    const expression::Observations* observations = nullptr;   // observed(): up, then down (01.C16);
                                                               // null when no cudf ran
    std::span<const core::GeneId> sources, targets, fscs;      // defaults from 01.C17; the FSC is a set (06.C10)
    std::vector<std::filesystem::path> confirmed_up, confirmed_down;   // 03.C2; files, defaults from 01.C17
    std::optional<std::filesystem::path> not_merged;            // first token per line (S1.17), like every file here
    std::optional<std::filesystem::path> start_file;            // FuSION 1's start argument 2
    core::Interner* interner = nullptr;                         // (added) the run's interner: the one behind every
                                                                // GeneId above, and the one the files' ids join.
                                                                // Null: a private one, which takes no GeneId
};
// Throws core::Failure: input.missing, input.unreadable; internal (handles given without an interner).
ExceptionSet assemble_exceptions(const ExceptionInputs&, const network::IdMap&, core::Context&);

// ---------- keys (03.C8) ----------
inline constexpr std::uint32_t rules_version = 1;              // in every key; see Test hooks
struct MergeKey {
    core::Sha256 value;
    std::string hex() const;
    auto operator<=>(const MergeKey&) const = default;
};
// One line per input: its SHA-256, and for an edge table the default pathway it takes from its name.
MergeKey merge_key(std::span<const network::SourceRef> inputs, const network::BuildSettings&,
                   const network::IdMap&);
std::string merge_key_text(std::span<const network::SourceRef> inputs, const network::BuildSettings&,
                           const network::IdMap&);              // (added) the text merge_key hashes

// ---------- the base merge (S2.17, S2.22) ----------
using DocPtr = std::shared_ptr<const network::PathwayDoc>;
struct EntryRef { std::uint32_t doc; std::uint32_t entry; };   // document index, entry id

class BaseMerge {                                    // immutable; held by the interpreter's MergeStore
public:
    const MergeKey& key() const noexcept;
    const network::BuildSettings& settings() const noexcept;
    std::span<const DocPtr> docs() const noexcept;
    std::shared_ptr<const network::Network> network() const noexcept;   // E = {}; its Provenance holds
                                                                         // relations and group entries (2B.11)
    std::span<const EntryRef> boxes_of(network::NodeIdx) const noexcept;  // node -> its boxes, for the split
    std::uint32_t box_count() const noexcept;                  // (added) boxes reached, for merge.base
    std::uint32_t relation_count() const noexcept;             // (added) relations kept, for merge.base

    struct Impl;                                               // (added)
    explicit BaseMerge(std::unique_ptr<Impl>);                 // (added) for build_base_merge
    BaseMerge(BaseMerge&&) noexcept;
    BaseMerge& operator=(BaseMerge&&) noexcept;
    ~BaseMerge();
    const Impl& impl() const noexcept;                         // (added) for split
private:
    std::unique_ptr<Impl> impl_;
};
// Throws core::Failure: merge.type_conflict (S2.19). Throws core::Stopped between documents.
std::shared_ptr<const BaseMerge> build_base_merge(std::vector<DocPtr>, const network::BuildSettings&,
                                                  MergeKey, core::Context&);

// ---------- the split, and the oracle it is tested against (12.C14) ----------
struct SplitStats {
    std::uint32_t exception_ids = 0, in_network = 0, split_out = 0;   // split_out: left a multi-id base node
    std::uint32_t affected_nodes = 0, affected_boxes = 0, affected_groups = 0, affected_relations = 0;
};
enum class BaseSource : std::uint8_t { memory, store, embedded, built };   // (added) MergeStore::Source
std::string_view source_name(BaseSource) noexcept;             // (added) "memory", "store", "embedded", "built"
struct MergedNetwork {
    std::shared_ptr<const network::Network> network;      // canonical order (02.C10)
    ExceptionSet exceptions;                               // as applied; their nodes marked
    MergeKey base;
    SplitStats stats;
    BaseSource source = BaseSource::built;                 // (added) where the base merge came from (run.json)
};
MergedNetwork split(const BaseMerge&, const ExceptionSet&);
std::shared_ptr<const network::Network> merge_direct(std::span<const DocPtr>, const network::BuildSettings&,
                                                     const ExceptionSet&);   // throws merge.type_conflict
// (added) run.json's details for the mff step (09): merge_key, base_merge, exceptions, exceptions_sha256;
// a legacy read has no key and no base merge.
core::Json step_details(const MergedNetwork&);

// ---------- reuse (03.C3) ----------
class MergeStore {                                   // owned by the batch interpreter (D1)
public:
    using Source = BaseSource;                       // memory, store, embedded, built
    struct Hit { std::shared_ptr<const BaseMerge> merge; Source source; };
    // Memory, then the shared store (ctx.cache; null: bypassed), then build from the documents.
    // The inputs carry the SHA-256 the run computed while copying them (09.C3). Never changes the result.
    Hit get_or_build(std::span<const network::SourceRef> inputs, const network::BuildSettings&,
                     const network::IdMap&, network::StepDocs&, core::Context&);
    // The base merge of a saved network's embedded documents: for rgx's rebuild (2A.4) and for a
    // network file in an mff list (03.C7). Throws core::Failure: merge.no_sources (an mff list member).
    Hit from_embedded(const network::Network& saved, const network::BuildSettings&, core::Context&);
    // (added) Parsed documents, keyed by their own records (SHA-256, pathway, id map): a list that
    // mixes KGMLs, edge tables and network files' embedded documents (03.C7).
    Hit get_or_build(std::vector<DocPtr> docs, const network::BuildSettings&, core::Context&);
    std::size_t size() const noexcept;               // (added) base merges held in memory
private:
    std::map<MergeKey, std::shared_ptr<const BaseMerge>> memory_;
};

// ---------- reports (S2.24), written through 09's OutputFile (2A.6); labels and names from Maps ----------
void write_exceptions(const MergedNetwork&, const BaseMerge&, const network::Maps&, core::OutputFile&);
void write_exceptions(const MergedNetwork&, const network::Maps&, core::OutputFile&);   // (added) no base
                                                       // merge (a legacy read): base_node empty, base_size 0
void write_edge_report(const MergedNetwork&, const network::Maps&, core::OutputFile&);   // also for 01.C11

// ---------- the step that section 01's mff handler calls ----------
struct MergeRequest {
    std::vector<network::SourceRef> inputs;                 // resolved by the handler (02.C12, 2B.10)
    network::BuildSettings build;                           // split threshold and filters (02)
    ExceptionInputs exceptions;
    std::optional<std::string> report;                      // S2.24; placed by 2B.3's rule
    std::shared_ptr<const network::Maps> maps;              // (added) start's maps or the shipped ones (2B.4):
                                                            // the id map, and the files' labels; null: none
    int graph = 0;                                          // (added) the graph id the handler gives the result
                                                            // (S2.01), for the console and merge.done; 0: unknown
};
// The merge has no reported result of its own, so the Outcome never carries a Report.
core::Outcome<MergedNetwork> merge_step(const MergeRequest&, MergeStore&, network::StepDocs&,
                                        core::Context&);

}  // namespace fusion::merge
