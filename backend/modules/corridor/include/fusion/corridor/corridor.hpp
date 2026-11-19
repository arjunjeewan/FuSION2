// fusion/corridor/corridor.hpp: the fb_rch step, which cuts out of a network the nodes and edges that
// lie on bounded walks from one source to one target, with each node's distances to both (section 05,
// "Interface"). The declarations from EdgesToTarget to load() are section 05's outline, with default
// member values added; everything marked "Added" extends it. Private state sits behind Corridor::Impl.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

namespace fusion::corridor {

enum class EdgesToTarget : std::uint8_t { any = 0, activation = 1, expression = 2 };   // S4.05

struct Request {                                    // the fb_rch arguments (S4.01, 05.C8)
    std::vector<std::string> source;                // must bind to one node (S4.10, S5.04)
    std::vector<std::string> target;                // must bind to one node
    std::vector<std::string> exclude;               // may be empty (S4.06)
    std::uint32_t bound = 0;                        // edges, inclusive (S4.03)
    EdgesToTarget edges_to_target = EdgesToTarget::any;
    std::vector<std::string> skip_subtypes{"component", "association", "dissociation", "mapping"};  // S4.04
    bool operator==(const Request&) const = default;   // Added: as given; reuse compares normalised()
};

enum class Role : std::uint8_t { source, target, excluded };
struct Bound { Role role = Role::source; network::Binding binding; };   // 02's given id, status, node, holders

struct Excluded {                                   // S4.06, extended in prompt log entry 170
    network::NodeIdx node = 0;
    std::string because;                            // the excluded id that removed it
    bool complex_of_excluded = false;               // true: a complex containing an excluded node
};

enum class EmptyReason : std::uint8_t { none, beyond_bound, edges_to_target, no_path };   // S4.18

struct Counts {                          // the counts S4.17 keeps, plus the first one
    std::uint32_t traversable_edges = 0; // network edges with no skipped subtype
    std::uint32_t into_target = 0;       // edges into the target from forward-set nodes (FuSION 1's M)
    std::uint32_t into_target_dropped = 0;  // of those, dropped by edges_to_target (FuSION 1's N)
    std::uint32_t forward_set = 0;       // nodes with ds <= b
    std::uint32_t backward_set = 0;      // forward-set nodes with dt <= b
};

class Corridor {                         // immutable once built
public:
    const network::GraphView& view() const noexcept;      // membership and adjacency (02.C9)
    const network::Network& network() const noexcept;
    const Request& request() const noexcept;              // as given
    std::span<const Bound> bindings() const noexcept;     // sources, targets, excluded; each list in
                                                          // byte order of the given ids
    std::span<const Excluded> excluded() const noexcept;  // the named nodes by index, then the complexes
    network::NodeIdx source() const noexcept;
    network::NodeIdx target() const noexcept;
    std::uint32_t bound() const noexcept;                 // the solve's bound must equal it (05.C7)

    bool empty() const noexcept;
    EmptyReason empty_reason() const noexcept;
    std::optional<std::uint32_t> shortest_path() const noexcept;   // edges; known even when > bound
    const Counts& counts() const noexcept;

    std::uint32_t ds(network::NodeIdx) const noexcept;    // S4.07; corridor nodes only
    std::uint32_t dt(network::NodeIdx) const noexcept;
    const core::Sha256& digest() const noexcept;          // of the canonical content

    struct Impl;
    explicit Corridor(std::unique_ptr<const Impl>);       // Added: for compute() and load()
    const Impl& impl() const noexcept;                    // Added: for this module's own functions
    ~Corridor();
    Corridor(const Corridor&) = delete;
    Corridor& operator=(const Corridor&) = delete;

private:
    std::unique_ptr<const Impl> impl_;
};

std::span<const core::SettingSpec> settings();            // keys corridor.<name> (1.16)

// Up-front check (05.C3): the listed ids that appear in no network input. Called by the
// pre-run check with the canonical entry ids that 02's pre-run parse found (02.C15).
// Each listed id is compared after the id rule's colon deletion (02.C3); a run-wide id map, if any,
// is the caller's to apply. Each missing id is returned once, sources first, then targets, then
// excluded ids.
std::vector<std::string> ids_missing_from_inputs(const Request&,
                                                 const std::unordered_set<std::string>& input_ids);

// The step. Throws core::Failure for the errors that stop the batch (S1.28, 09.C14). An empty
// corridor is a value with a core::Report whose code is "corridor.empty" (S4.18). The step is
// too short to poll for a stop, so it never throws core::Stopped. Writes no file.
core::Outcome<std::shared_ptr<const Corridor>>
compute(std::shared_ptr<const network::Network> network, network::BindingTable& bindings,
        const Request& request, core::Context& ctx);

// Saved step (decision 0019, 05.C6). `matched` is section 04's match_observations() result for
// this corridor (05.C9), empty when the run has no observations. Files go through
// ctx.files.create(); the traces (S4.19) through ctx.files.create_debug(), which section 09
// enables only under the debug option.
// save() writes corridor.json and corridor_nodes.tsv, with the three observation counts from
// `matched`, and the traces under the debug option. The caller gets `matched` from
// match_observations() below and writes observations_match.tsv through section 04's save_match(),
// which needs the values, the symbol map and the interner that only it holds.
void save(const Corridor&, std::span<const expression::ObservationMatch> matched, core::Context& ctx);
std::shared_ptr<const Corridor> load(std::shared_ptr<const network::Network> network,
                                     network::BindingTable& bindings,
                                     const std::filesystem::path& corridor_json, core::Context& ctx);

// ---- Added beyond the outline ----

// The names of 05.C8's values; parsing also takes FuSION 1's 0, 1 and 2 (S4.05, decision 0017).
std::string_view name(EdgesToTarget) noexcept;
std::optional<EdgesToTarget> parse_edges_to_target(std::string_view) noexcept;

// The request in canonical form: each id list in byte order without repeats, and skip_subtypes
// through network::normalise_subtype (S2.10), in byte order without repeats. corridor.json, the
// digest and reuse (05.C4) use it.
Request normalised(const Request&);

// What the batch knows and the step does not: the run's maps, for labels (07.C10), and the graph
// ids of the network and of the new corridor (S2.01, S4.08). The outline's compute() and save()
// use the default: node keys for labels, and no graph ids.
struct Naming {
    const network::Maps* maps = nullptr;
    int network_graph = 0;               // 0: not known
    int graph = 0;                       // the corridor's own id; 0: not known
};
core::Outcome<std::shared_ptr<const Corridor>>
compute(std::shared_ptr<const network::Network> network, network::BindingTable& bindings,
        const Request& request, const Naming& naming, core::Context& ctx);
void save(const Corridor&, std::span<const expression::ObservationMatch> matched, const Naming& naming,
          core::Context& ctx);

// Reuse within a run (05.C4): true when `earlier` was cut from a network with the same content hash
// and with an equal normalised request. The command then registers `earlier` again, calls
// report_reuse() and writes no file.
bool reusable(const Corridor& earlier, const network::Network& network, const Request& request);
void report_reuse(const Corridor& earlier, int graph, int same_as, core::Context& ctx);
                                         // "Corridor, graph 4: same as graph 3", corridor.summary

// The observations report (05.C9): places every observed gene against the corridor through section
// 04's match_observations(observations, corridor.view()). It prints nothing: 04's save_match(),
// which writes observations_match.tsv, prints the three counts.
std::vector<expression::ObservationMatch> match_observations(const Corridor&,
                                                             const expression::Observations&,
                                                             core::Context&);

// Test hook (section 05, "Test hooks"): recomputes the traversal graph and both distances
// independently and returns each way the corridor departs from the keep rule, its distances, its
// exclusions and its counts; empty when all hold. compute() also runs it under the debug option.
std::vector<std::string> check_invariants(const Corridor&);

}  // namespace fusion::corridor
