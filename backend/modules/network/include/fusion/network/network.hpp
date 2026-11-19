// fusion/network/network.hpp: the graph model, inputs, id mapping, readers and writers
// (section 02, "Interface"). The declarations are section 02's outline; private state sits behind Impl.
#pragma once

#include <compare>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"

namespace fusion::network {

using core::GeneId;                                     // an interned entity id or node key (03, 04 use it)
using core::Sym;                                        // other interned strings: types, subtypes, pathways
using NodeIdx = std::uint32_t;                          // position in canonical node order
using EdgeIdx = std::uint32_t;                          // position in canonical edge order

// ---- keys, kinds and the polarity rule (S4.14, S6.14; items 2B.1, 2B.18) ----
struct NodeKey {                                        // "hsa1956", "c_s_hsa4149_hsa4609_c_e"
  std::string value;
  auto operator<=>(const NodeKey&) const = default;     // byte order
};
struct EdgeKey {                                        // 02.C11: unique within a network
  NodeKey source, target;
  std::string type;                                     // "PPrel", "GErel", "PCrel", "ECrel", ...
  std::vector<std::string> subtypes;                    // ordered, without dummy_u_to_d
  auto operator<=>(const EdgeKey&) const = default;     // byte order; equals the canonical edge order
  std::string text() const;                             // "src|tgt|type|a,b"
  static EdgeKey parse(std::string_view text);
};
enum class NodeKind : std::uint8_t { gene, complex, compound, other };
NodeKind kind_of(std::string_view entry_type) noexcept; // "gene", "group", "compound", anything else

enum class Effect : std::uint8_t { activation, inhibition };
enum class EndState : std::uint8_t { absent, inactive, active };
Effect effect_of(std::span<const std::string> subtypes);              // inhibition iff inhibition or repression
bool is_relaxed(Effect, EndState source, EndState target) noexcept;   // S5.13's 3 x 3 x 2 table
std::string_view normalise_subtype(std::string_view name) noexcept;   // S2.10: "indirect effect" -> "indirect", ...

// ---- maps and labels (02.C3; items 1.12, 2B.4, 2B.14) ----
class LabelMap {                                        // S1.18, S1.19: one reader
 public:
  LabelMap();                                           // empty
  static LabelMap read(const std::filesystem::path&, core::Context&);
  std::optional<std::string_view> find(std::string_view id) const noexcept;   // never inserts (S2.15);
                                                        // empty for an id with no name
  std::string_view label_or_id(std::string_view id) const noexcept;
  const core::Sha256& digest() const noexcept;
  struct Row { std::string_view id, name; std::uint32_t line = 0; };   // line: the line that won
  std::vector<Row> rows() const;                        // byte order of id (the labels query, 2B.6)
  std::size_t size() const noexcept;                    // distinct ids (start prints it, S1.15)
  struct Impl;
 private:
  std::shared_ptr<const Impl> impl_;
};
class IdMap {                                           // S1.16's layout: <foreign id> <network id>
 public:
  IdMap();                                              // the same as none()
  static IdMap none();
  static IdMap read(const std::filesystem::path& two_column, core::Context&);
  static IdMap reverse_symbols(const LabelMap& symbols, core::Context&);    // id_map=symbols
  GeneId canonical(std::string_view raw, core::Interner&) const;  // ':' removed, mapped once; an id the
                                                                  // map lacks passes through (run-wide map)
  std::optional<GeneId> lookup(std::string_view raw, core::Interner&) const;  // ':' removed; empty when the
                                                                  // map lacks the id (a value input's own map
                                                                  // drops it, and 04 reports it; entry 229)
                                                                  // or maps it to nothing (a one-token line);
                                                                  // none() passes every id through
  std::string apply(std::string_view raw) const;        // the rule as text: ':' removed, mapped once;
                                                        // "" for an id mapped to nothing (S1.16)
  bool is_none() const noexcept;
  const core::Sha256& digest() const noexcept;          // all zero for none()
  struct Impl;
 private:
  std::shared_ptr<const Impl> impl_;
};
struct Maps {                                           // what start sets; the shipped files otherwise
  IdMap ids;
  LabelMap symbols, pathway_names;
};
Maps shipped_maps(core::Context&);                      // section 11's data/, no id map

class IdSet {                                           // canonical ids, sorted by content (exceptions)
 public:
  IdSet() = default;
  explicit IdSet(std::vector<std::string> ids);         // sorts and removes repeats
  bool contains(std::string_view id) const noexcept;
  std::span<const std::string> ids() const noexcept { return ids_; }
  std::size_t size() const noexcept { return ids_.size(); }
  bool empty() const noexcept { return ids_.empty(); }
  auto operator<=>(const IdSet&) const = default;
 private:
  std::vector<std::string> ids_;
};

// ---- model ----
enum class NodeFlag : std::uint16_t { complex = 1, exception = 2, family = 4 };
enum class EdgeFlag : std::uint16_t { dummy_u_to_d = 1, component = 2, undirected = 4, ecrel = 8,
                                      indirect = 16, self_loop = 32, legacy = 64, unscored = 128 };
constexpr std::uint16_t bit(NodeFlag f) noexcept { return static_cast<std::uint16_t>(f); }
constexpr std::uint16_t bit(EdgeFlag f) noexcept { return static_cast<std::uint16_t>(f); }
enum class IndirectMode : std::uint8_t { discard, keep };
enum class EcrelMode    : std::uint8_t { keep, exclude };
enum class MaplinkMode  : std::uint8_t { skip, keep };

struct RelationFilters {                                // S2.11, S2.31, 02.C7; 08's draft calls it EdgeSettings
  IndirectMode indirect = IndirectMode::discard;
  EcrelMode ecrel = EcrelMode::keep;
  MaplinkMode maplink = MaplinkMode::skip;
  auto operator<=>(const RelationFilters&) const = default;
};
struct BuildSettings {
  int split_threshold = 1;                              // S2.07
  RelationFilters filters;
  auto operator<=>(const BuildSettings&) const = default;
};

struct Node { GeneId key; Sym type; std::uint32_t members_first = 0, members_count = 0,
              comps_first = 0, comps_count = 0; std::uint16_t flags = 0; };
struct Edge { NodeIdx src = 0, tgt = 0; Sym type; std::uint32_t subtypes = 0, pathways = 0;
              std::uint16_t flags = 0; double score = 0; };   // score 0 when unscored

enum class Format { kgml, fusion_network, legacy, edge_table };
struct SourceRef {
  std::optional<std::filesystem::path> path;            // as named; left out in store entries (03.C4)
  std::uint64_t size = 0;
  core::Sha256 sha256;
  Format format = Format::kgml;
  std::string pathway;                                  // root name, else the file stem (item 2B.12)
  std::string title;                                    // KGML root title, the pathway's name (entry 205)
};
struct MergeHeader { std::string key; bool base = false; int rules = 1; };  // 03's <merge> element
struct NetworkInfo {
  std::string reader;
  std::vector<SourceRef> sources;                       // canonical order
  BuildSettings settings;
  std::vector<std::string> exceptions;                  // sorted by content
  core::Sha256 id_map;
  bool legacy = false;
  std::optional<MergeHeader> merge;
};

class PathwayDoc;
struct Provenance {                                     // optional; filled by 03
  enum class Kind : std::uint8_t { relation, group };   // a relation behind an edge; a group behind a complex
  struct Ref { std::uint32_t doc = 0, item = 0; Kind kind = Kind::relation; };
  std::vector<std::shared_ptr<const PathwayDoc>> docs;
  std::vector<std::uint32_t> edge_first, node_first;    // offsets into the two tables: edge e's refs
  std::vector<Ref> edge_refs, node_refs;                // are edge_refs[edge_first[e], edge_first[e+1]),
};                                                      // so edge_first has one entry per edge, plus one.
                                                        // Given to attach(), the indices are the
                                                        // builder's; finish() makes them canonical and
                                                        // orders docs by (pathway id, SHA-256)

class IdIndex {                                         // hash table with core's fixed hash
 public:
  IdIndex();
  ~IdIndex();
  IdIndex(IdIndex&&) noexcept;
  IdIndex& operator=(IdIndex&&) noexcept;
  std::span<const NodeIdx> holders(GeneId) const noexcept;   // O(1); first claim first
  struct Impl;
 private:
  friend class NetworkBuilder;
  std::unique_ptr<Impl> impl_;
};

// A Network owns its interner: its GeneId and Sym handles are its own (interner()), assigned in
// canonical order when finish() runs. Compare ids across networks, documents and other modules'
// objects by their strings, never by handle.
class Network {                                         // immutable; shared as shared_ptr<const Network>
 public:
  Network(Network&&) noexcept;
  Network& operator=(Network&&) noexcept;
  Network(const Network&) = delete;
  Network& operator=(const Network&) = delete;
  ~Network();
  std::span<const Node> nodes() const noexcept;
  std::span<const Edge> edges() const noexcept;
  std::span<const GeneId> members(NodeIdx) const noexcept;
  std::span<const NodeIdx> components(NodeIdx) const noexcept;
  std::span<const EdgeIdx> out_edges(NodeIdx) const noexcept;   // ascending EdgeIdx
  std::span<const EdgeIdx> in_edges(NodeIdx) const noexcept;
  std::span<const Sym> subtypes(EdgeIdx) const noexcept;        // ordered
  std::span<const Sym> pathways(EdgeIdx) const noexcept;        // sorted by content
  bool has_subtype(EdgeIdx, std::string_view) const noexcept;
  NodeKey node_key(NodeIdx) const;                      // section 02 names both key(); NodeIdx and
  EdgeKey edge_key(EdgeIdx) const;                      // EdgeIdx are one type, so they cannot overload
  NodeKind kind(NodeIdx) const noexcept;
  Effect effect(EdgeIdx) const noexcept;
  std::optional<NodeIdx> find_node(const NodeKey&) const noexcept;   // O(1)
  std::optional<EdgeIdx> find_edge(const EdgeKey&) const noexcept;   // binary search in out_edges: O(log deg)
  std::vector<EdgeIdx> find_edges(NodeIdx src, NodeIdx tgt,
                                  std::span<const std::string_view> subtype_set) const;  // S5.06
  std::string label(NodeIdx, const Maps&) const;        // "EGFR", "RRAS2+5", "[STAT1+6]" (07.C10)
  std::string pathway_label(std::string_view pathway, const Maps&) const;   // KGML title, map, id
  const IdIndex& ids() const noexcept;
  const NetworkInfo& info() const noexcept;
  const Provenance* provenance() const noexcept;
  const core::Interner& interner() const noexcept;      // the strings behind GeneId and Sym
  core::Sha256 content_hash() const;                    // model fields only
  struct Impl;
 private:
  friend class NetworkBuilder;
  explicit Network(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};

// The builder takes handles in the caller's interner (the one given to the constructor). Indices it
// returns are its own; finish() renumbers nodes and edges into canonical order (02.C10).
// Edge flags: add_edge sets ecrel (type ECrel), indirect (subtype indirect), self_loop (src == tgt)
// and unscored (no score) itself, and turns a "dummy_u_to_d" subtype into the flag. The caller passes
// undirected (both directions of an association, dissociation or mapping relation, S2.10), legacy,
// and a tag read from a file; add_component_edges sets component. finish() tags every component edge
// and every undirected edge whose source key is the larger, never a self-loop (S2.09, S2.10, S2.21);
// other edges keep the tag they were given.
class NetworkBuilder {
 public:
  explicit NetworkBuilder(NetworkInfo, core::Interner&);
  NetworkBuilder(NetworkBuilder&&) noexcept;
  NetworkBuilder& operator=(NetworkBuilder&&) noexcept;
  ~NetworkBuilder();
  NodeIdx add_node(Sym type, std::span<const GeneId> members);     // key = smallest by content
  void add_members(NodeIdx, std::span<const GeneId> ids);
  NodeIdx add_complex(std::span<const NodeIdx> components);        // S2.09 key
  void mark_exception(NodeIdx);
  EdgeIdx add_edge(NodeIdx src, NodeIdx tgt, Sym type, std::span<const Sym> subtypes,
                   std::span<const Sym> pathways, std::optional<double> score,
                   std::uint16_t extra_flags = 0);                   // merges on identity
  void add_component_edges(NodeIdx complex, Sym pathway);           // S2.09
  NodeIdx copy_node(const Network& from, NodeIdx);                  // with members (item 2B.11)
  EdgeIdx copy_edge(const Network& from, EdgeIdx, NodeIdx src, NodeIdx tgt);   // flags, pathways,
                                                                     // score, provenance
  void set_claim_order(std::span<const std::pair<GeneId, NodeIdx>> claims);   // legacy only
  void attach(Provenance);
  Network finish(core::Context&) &&;                    // tag once, sort, adjacency, IdIndex
  struct Impl;
 private:
  std::unique_ptr<Impl> impl_;
};

// ---- views (02.C9, 05.C1) ----
class Bitset {                                          // fixed-size bit vector
 public:
  Bitset() = default;
  explicit Bitset(std::size_t size, bool value = false)
      : size_(size), words_((size + 63) / 64, value ? ~std::uint64_t{0} : 0) { trim(); }
  std::size_t size() const noexcept { return size_; }
  bool test(std::size_t i) const noexcept { return (words_[i / 64] >> (i % 64)) & 1U; }
  void set(std::size_t i) noexcept { words_[i / 64] |= std::uint64_t{1} << (i % 64); }
  void reset(std::size_t i) noexcept { words_[i / 64] &= ~(std::uint64_t{1} << (i % 64)); }
  std::size_t count() const noexcept;
  bool operator==(const Bitset&) const = default;
 private:
  void trim() noexcept {
    if (size_ % 64 != 0 && !words_.empty()) words_.back() &= (std::uint64_t{1} << (size_ % 64)) - 1;
  }
  std::size_t size_ = 0;
  std::vector<std::uint64_t> words_;
};

class GraphView {
 public:
  explicit GraphView(std::shared_ptr<const Network>);   // every node and edge
  GraphView(std::shared_ptr<const Network>, Bitset nodes, Bitset edges);
  const Network& network() const noexcept { return *network_; }
  const std::shared_ptr<const Network>& shared() const noexcept { return network_; }
  bool has_node(NodeIdx i) const noexcept { return !nodes_ || nodes_->test(i); }
  bool has_edge(EdgeIdx i) const noexcept { return !edges_ || edges_->test(i); }
  std::size_t node_count() const noexcept;
  std::size_t edge_count() const noexcept;
  template <class F> void for_each_node(F&& f) const {             // ascending index
    const auto n = static_cast<NodeIdx>(network_->nodes().size());
    for (NodeIdx i = 0; i < n; ++i) if (has_node(i)) f(i);
  }
  template <class F> void for_each_edge(F&& f) const {
    const auto n = static_cast<EdgeIdx>(network_->edges().size());
    for (EdgeIdx i = 0; i < n; ++i) if (has_edge(i)) f(i);
  }
  template <class F> void for_each_out_edge(NodeIdx v, F&& f) const {
    for (EdgeIdx e : network_->out_edges(v))
      if (has_edge(e) && has_node(network_->edges()[e].tgt)) f(e);
  }
  template <class F> void for_each_in_edge(NodeIdx v, F&& f) const {
    for (EdgeIdx e : network_->in_edges(v))
      if (has_edge(e) && has_node(network_->edges()[e].src)) f(e);
  }
 private:
  std::shared_ptr<const Network> network_;
  std::optional<Bitset> nodes_, edges_;                 // empty: all of them
};

// ---- lookup and bindings.tsv (S2.03, S4.15; 02.C13; item 1.22) ----
enum class BindStatus : std::uint8_t { own, family, ambiguous, absent };
inline constexpr GeneId absent_id{~std::uint32_t{0}};   // Binding::id of an id the network never saw
struct Binding { std::string given; GeneId id; BindStatus status = BindStatus::absent;
                 NodeIdx node = 0; std::uint32_t holders = 0;
                 std::string canonical; };              // id: the network's handle (or absent_id);
                                                        // canonical: the id after the id rule
struct BindingRow {                                     // one line of bindings.tsv
  std::string role, given;
  std::optional<Binding> binding;                       // empty for 06's edge lines
  std::uint32_t line = 0;                               // 0: not from a file
  std::string note;                                     // 06's remark
};
class BindingTable {
 public:
  BindingTable(std::shared_ptr<const Network>, const Maps&);
  BindingTable(BindingTable&&) noexcept;
  BindingTable& operator=(BindingTable&&) noexcept;
  ~BindingTable();
  const Binding& bind(std::string_view role, std::string_view given, std::uint32_t line = 0);
  void add_row(BindingRow);                             // 06's edge-line rows
  struct Counts { std::uint32_t ids = 0, own = 0, family = 0, ambiguous = 0, absent = 0; };
  Counts counts(std::string_view role) const;
  void write(std::ostream&) const;                      // once, at the run's end, into the stream of
                                                        // 09's Run::create_run_file("bindings.tsv")
  struct Impl;
 private:
  std::unique_ptr<Impl> impl_;
};

// ---- documents and readers ----
struct Entry { std::uint32_t local_id = 0; Sym type; std::uint32_t ids_first = 0, ids_count = 0,
               comps_first = 0, comps_count = 0; };
struct Relation { std::uint32_t entry1 = 0, entry2 = 0; Sym type; std::uint32_t subtypes = 0, pathways = 0;
                  std::optional<double> score; std::uint32_t line = 0; };   // empty score: unscored
// A document owns its interner (interner()): its handles are its own. An entry's ids are canonical
// (the id rule applied), sorted by content and unique.
class PathwayDoc {
 public:
  PathwayDoc(PathwayDoc&&) noexcept;
  PathwayDoc& operator=(PathwayDoc&&) noexcept;
  ~PathwayDoc();
  const SourceRef& source() const noexcept;             // pathway id and title included
  std::span<const Entry> entries() const noexcept;      // every entry
  std::span<const Relation> relations() const noexcept;
  std::span<const GeneId> ids(const Entry&) const noexcept;
  std::span<const std::uint32_t> components(const Entry&) const noexcept;
  std::span<const Sym> subtypes(const Relation&) const noexcept;
  std::span<const Sym> pathways(const Relation&) const noexcept;   // sorted by content; a KGML's
                                                        // relations carry the root's pathway id
  const Entry* entry(std::uint32_t local_id) const noexcept;
  std::vector<GeneId> gene_ids() const;                 // the file's GSEA gene set (S7.08, 08.C3)
  const core::Sha256& id_map() const noexcept;          // the map its ids went through
  const core::Interner& interner() const noexcept;
  struct Impl;
  explicit PathwayDoc(std::unique_ptr<Impl>);           // for the readers
 private:
  std::unique_ptr<Impl> impl_;
};

struct Sniff { Format format = Format::kgml; int version = 0; };
Sniff sniff(const std::filesystem::path&, std::optional<bool> tool_written,
            core::Context&);                            // raises network.flag_mismatch
std::shared_ptr<const PathwayDoc> read_kgml(const std::filesystem::path&, const IdMap&, core::Context&);
std::shared_ptr<const PathwayDoc> read_edge_table(const std::filesystem::path&, const IdMap&, core::Context&);
std::shared_ptr<const Network> read_fusion_network(const std::filesystem::path&,
                                                   core::Context&);   // as saved (load); sources as Provenance
std::vector<std::string> saved_network_diff(const NetworkInfo&, const BuildSettings&,
                                            const IdSet& exceptions, const IdMap&);  // rgx; empty: as saved
std::shared_ptr<const Network> read_legacy(const std::filesystem::path&, const IdMap&,
                                           const BuildSettings&, const IdSet& exceptions,
                                           core::Context&);

class StepDocs {                                        // one per step: the check fills it from the copies
 public:                                                // (09.C3); never shared between steps (S7.19)
  StepDocs();
  ~StepDocs();
  StepDocs(StepDocs&&) noexcept;
  StepDocs& operator=(StepDocs&&) noexcept;
  std::shared_ptr<const PathwayDoc> get(const std::filesystem::path& copy, const IdMap&, core::Context&);
  struct Impl;
 private:
  std::unique_ptr<Impl> impl_;
};

std::vector<std::uint32_t> kept_relations(const PathwayDoc&, const RelationFilters&);  // also 08.C4
struct EntrySplit { std::vector<GeneId> singles, box; };
EntrySplit split_entry(const PathwayDoc&, const Entry&, const IdSet& exceptions, int split_threshold);

struct ListLine { std::string text; std::filesystem::path path; };          // item 2B.12
struct PathList { std::filesystem::path list, base; std::vector<ListLine> lines; };
std::filesystem::path list_base(const std::filesystem::path& list,
                                const std::optional<std::filesystem::path>& kgml_dir,
                                const std::optional<std::filesystem::path>& recorded);   // 02.C12, item 2B.10
PathList read_input_list(const std::filesystem::path& list, const std::filesystem::path& base,
                         core::Context&);               // input.missing names every missing file

// ---- writers, summaries and the labels query ----
struct WriteOptions { std::string root_name = "KEGG"; bool embed_sources = true; bool store_entry = false; };
void write_fusion_network(const GraphView&, std::ostream& out, const Maps&,
                          const WriteOptions&);         // out: a core::OutputFile's stream
void add_edges(const std::filesystem::path& edges, std::ostream& out, const IdMap&,
               core::Context&);                         // S2.30, 02.C17
struct Summary { std::uint32_t nodes = 0, genes = 0, complexes = 0, compounds = 0, others = 0,
                 isolated = 0, edges = 0, multi_gene = 0; NodeIdx largest = 0; std::uint32_t largest_size = 0; };
Summary summarize(const GraphView&);
std::vector<std::string> summary_lines(const Summary&, const GraphView&, const Maps&);   // sz
core::Json labels_answer(const LabelMap& symbols, const SourceRef& map_file);   // the labels query

}  // namespace fusion::network
