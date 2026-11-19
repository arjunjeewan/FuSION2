// The corridor's private state and the helpers its sources share (section 05).
#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/corridor/corridor.hpp"

namespace fusion::corridor {

inline constexpr std::uint32_t kInf = std::numeric_limits<std::uint32_t>::max();   // not reached

struct TraceRow {                        // one node a search dequeued, with the nodes it queued (S4.19)
    network::NodeIdx node = 0;
    std::uint32_t distance = 0;
    std::vector<network::NodeIdx> queued;
};

struct Corridor::Impl {
    explicit Impl(network::GraphView v) : view(std::move(v)) {}

    network::GraphView view;             // the network, shared read-only, and one bit per node and edge (05.C1)
    Request request;                     // as given
    std::vector<Bound> bindings;
    std::vector<Excluded> excluded;
    network::NodeIdx source = 0, target = 0;
    EmptyReason empty_reason = EmptyReason::none;
    std::optional<std::uint32_t> shortest_path;
    Counts counts;
    std::uint32_t nodes = 0, edges = 0;  // the view's sizes
    std::vector<std::uint32_t> ds, dt;   // over the network's nodes; kInf where not reached (S4.07)
    core::Sha256 network_hash;
    core::Sha256 digest;
    bool traced = false;                 // computed under the debug option: the traces below are filled
    std::vector<TraceRow> forward_trace, backward_trace;   // S4.19
};

namespace detail {

std::string_view role_name(Role) noexcept;              // "source", "target", "excluded"
std::optional<Role> parse_role(std::string_view) noexcept;
std::string_view status_name(network::BindStatus) noexcept;
std::string_view reason_name(EmptyReason) noexcept;     // "none", "beyond_bound", ...
std::optional<EmptyReason> parse_reason(std::string_view) noexcept;

// A node's label (07.C10), or its key when no maps are given.
std::string label(const network::Network&, network::NodeIdx, const network::Maps*);
// "EGFR (hsa1956)", or the key alone when the label is the key.
std::string named(const network::Network&, network::NodeIdx, const network::Maps*);

std::uint32_t to_u32(std::size_t) noexcept;

// SHA-256 of the canonical text: the network's content hash, the normalised request, and the
// corridor's node keys with ds and dt and its edge keys, in index order (section 05, step 10).
core::Sha256 digest_of(const Corridor::Impl&);

}  // namespace detail
}  // namespace fusion::corridor
