// fusion corridor: the fb_rch step (section 05; spec stage 4, S4.01 to S4.22).
// Ported from FuSION 1's GraphManagerNew::bounded_forward_backward_reach
// (Linux/src/GraphManagerNew.cc:17759-18158) and its command handler (Linux/src/main.cc:2109-2232),
// with the spec's fixes and section 05's design.
#include "fusion/corridor/corridor.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "impl.hpp"

namespace fusion::corridor {

using network::BindStatus;
using network::Bitset;
using network::EdgeIdx;
using network::Network;
using network::NodeIdx;

// ---------------------------------------------------------------- the value (05.C1)
// Only const accessors and no lazily filled member, so both FSC cases and parallel solves can share one
// corridor, which no solve can change (S4.20, S5.29).

Corridor::Corridor(std::unique_ptr<const Impl> impl) : impl_(std::move(impl)) {}
Corridor::~Corridor() = default;
const Corridor::Impl& Corridor::impl() const noexcept { return *impl_; }
const network::GraphView& Corridor::view() const noexcept { return impl_->view; }
const network::Network& Corridor::network() const noexcept { return impl_->view.network(); }
const Request& Corridor::request() const noexcept { return impl_->request; }
std::span<const Bound> Corridor::bindings() const noexcept { return impl_->bindings; }
std::span<const Excluded> Corridor::excluded() const noexcept { return impl_->excluded; }
NodeIdx Corridor::source() const noexcept { return impl_->source; }
NodeIdx Corridor::target() const noexcept { return impl_->target; }
std::uint32_t Corridor::bound() const noexcept { return impl_->request.bound; }
bool Corridor::empty() const noexcept { return impl_->empty_reason != EmptyReason::none; }
EmptyReason Corridor::empty_reason() const noexcept { return impl_->empty_reason; }
std::optional<std::uint32_t> Corridor::shortest_path() const noexcept { return impl_->shortest_path; }
const Counts& Corridor::counts() const noexcept { return impl_->counts; }
std::uint32_t Corridor::ds(NodeIdx v) const noexcept { return impl_->ds[v]; }
std::uint32_t Corridor::dt(NodeIdx v) const noexcept { return impl_->dt[v]; }
const core::Sha256& Corridor::digest() const noexcept { return impl_->digest; }

// ---------------------------------------------------------------- settings (05.C8, 1.16)

namespace {

constexpr std::string_view kEdgesToTargetChoices[] = {"any", "activation", "expression"};

// The registry's view of fb_rch's arguments (section 05, "Settings"). FuSION 1 parsed them
// positionally with stoi (main.cc:2113-2205, S4.01); section 01 binds both forms to these.
constexpr core::SettingSpec kSettings[] = {
    {.name = "graph", .module = "corridor", .type = core::Type::integer, .need = core::Need::defaulted,
     .default_text = "0", .affects_results = false, .min = 0,
     .help = "the network to cut: a graph id, or 0 for the latest graph (S2.01, 01.C16)"},
    {.name = "source", .module = "corridor", .type = core::Type::text, .need = core::Need::required,
     .help = "the source's gene ids, which must bind to one node (S4.02, S4.10)"},
    {.name = "target", .module = "corridor", .type = core::Type::text, .need = core::Need::required,
     .help = "the target's gene ids, which must bind to one node (S4.02, S4.10)"},
    {.name = "exclude", .module = "corridor", .type = core::Type::text, .need = core::Need::defaulted,
     .default_text = "",
     .help = "gene ids whose nodes the corridor never enters, nor any complex that contains one (S4.06)"},
    {.name = "bound", .module = "corridor", .type = core::Type::integer, .need = core::Need::required,
     .min = 0, .max = 4294967295.0, .help = "the longest path, in edges, inclusive (S4.03)"},
    {.name = "edges_to_target", .module = "corridor", .type = core::Type::choice,
     .need = core::Need::defaulted, .default_text = "any", .choices = kEdgesToTargetChoices,
     .help = "the edges that may enter the target: any; activation (activation, inhibition or "
             "phosphorylation); or expression (expression or repression); FuSION 1's 0, 1 or 2 (S4.05)"},
    // S4.22: FuSION 1's compile-time ONLY_INTERAC_EDGES_FOR_MINCUT (Linux/Makefile:10) becomes this run
    // setting; DUP_CHECK_FLAG_SET and DUMMY_EDGE_FLAG changed no corridor and are dropped.
    {.name = "skip_subtypes", .module = "corridor", .type = core::Type::text, .need = core::Need::defaulted,
     .default_text = "component,association,dissociation,mapping",
     .help = "edge subtypes that make an edge non-traversable (S4.04)"},
    {.name = "observations", .module = "corridor", .type = core::Type::text, .need = core::Need::computed,
     .affects_results = false,
     .help = "the observations placed against the corridor, by default the latest cudf result; they "
             "change no corridor (05.C9)"},
};

constexpr std::string_view kDefaultSkips[] = {"association", "component", "dissociation", "mapping"};

bool byte_less(const std::string& a, const std::string& b) noexcept { return core::stable_compare(a, b) < 0; }

void sort_unique(std::vector<std::string>& items) {
    std::sort(items.begin(), items.end(), byte_less);
    items.erase(std::unique(items.begin(), items.end()), items.end());
}

std::string join(const std::vector<std::string>& items, std::string_view separator) {
    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) text += separator;
        text += items[i];
    }
    return text;
}

std::string count_of(std::uint64_t n, std::string_view one, std::string_view many) {
    return std::to_string(n) + " " + std::string(n == 1 ? one : many);
}

std::string network_name(const Naming& naming) {
    return naming.network_graph > 0 ? "network " + std::to_string(naming.network_graph) : "the network";
}

std::string without_colons(std::string_view id) {
    std::string text;
    for (char c : id)
        if (c != ':') text += c;
    return text;
}

}  // namespace

std::span<const core::SettingSpec> settings() { return kSettings; }

std::string_view name(EdgesToTarget rule) noexcept {
    switch (rule) {
        case EdgesToTarget::any: return "any";
        case EdgesToTarget::activation: return "activation";
        case EdgesToTarget::expression: return "expression";
    }
    return "any";
}

// S4.05: only these values, or FuSION 1's numbers for them (macros.h:76-77 named the rules, and any
// other number silently meant no rule).
std::optional<EdgesToTarget> parse_edges_to_target(std::string_view text) noexcept {
    if (text == "any" || text == "0") return EdgesToTarget::any;
    if (text == "activation" || text == "1") return EdgesToTarget::activation;
    if (text == "expression" || text == "2") return EdgesToTarget::expression;
    return std::nullopt;
}

// FuSION 1 kept each list in a std::set<string> (main.cc:2124-2193): repeats collapsed, order lost.
Request normalised(const Request& request) {
    Request n = request;
    sort_unique(n.source);
    sort_unique(n.target);
    sort_unique(n.exclude);
    for (std::string& subtype : n.skip_subtypes) subtype = std::string(network::normalise_subtype(subtype));
    sort_unique(n.skip_subtypes);
    return n;
}

// 05.C3, decision 0021: FuSION 1 checked ids only when the step ran (GraphManagerNew.cc:17775-17822).
std::vector<std::string> ids_missing_from_inputs(const Request& request,
                                                 const std::unordered_set<std::string>& input_ids) {
    std::vector<std::string> missing;
    for (const auto* list : {&request.source, &request.target, &request.exclude})
        for (const std::string& id : *list)
            if (!input_ids.contains(without_colons(id)) &&
                std::find(missing.begin(), missing.end(), id) == missing.end())
                missing.push_back(id);
    return missing;
}

// ---------------------------------------------------------------- shared helpers

namespace detail {

std::string_view role_name(Role role) noexcept {
    switch (role) {
        case Role::source: return "source";
        case Role::target: return "target";
        case Role::excluded: return "excluded";
    }
    return "source";
}

std::optional<Role> parse_role(std::string_view text) noexcept {
    if (text == "source") return Role::source;
    if (text == "target") return Role::target;
    if (text == "excluded") return Role::excluded;
    return std::nullopt;
}

std::string_view status_name(BindStatus status) noexcept {
    switch (status) {
        case BindStatus::own: return "own";
        case BindStatus::family: return "family";
        case BindStatus::ambiguous: return "ambiguous";
        case BindStatus::absent: return "absent";
    }
    return "absent";
}

std::string_view reason_name(EmptyReason reason) noexcept {
    switch (reason) {
        case EmptyReason::none: return "none";
        case EmptyReason::beyond_bound: return "beyond_bound";
        case EmptyReason::edges_to_target: return "edges_to_target";
        case EmptyReason::no_path: return "no_path";
    }
    return "none";
}

std::optional<EmptyReason> parse_reason(std::string_view text) noexcept {
    if (text == "beyond_bound") return EmptyReason::beyond_bound;
    if (text == "edges_to_target") return EmptyReason::edges_to_target;
    if (text == "no_path") return EmptyReason::no_path;
    return std::nullopt;
}

std::string label(const Network& net, NodeIdx v, const network::Maps* maps) {
    return maps ? net.label(v, *maps) : net.node_key(v).value;
}

std::string named(const Network& net, NodeIdx v, const network::Maps* maps) {
    std::string key = net.node_key(v).value;
    std::string text = label(net, v, maps);
    return text == key ? key : text + " (" + key + ")";
}

std::uint32_t to_u32(std::size_t n) noexcept { return static_cast<std::uint32_t>(n); }

core::Sha256 digest_of(const Corridor::Impl& c) {
    const Network& net = c.view.network();
    const Request n = normalised(c.request);
    std::string text = "fusion-corridor 1\nnetwork " + c.network_hash.hex() + "\n";
    text += "source\t" + join(n.source, "\t") + "\n";
    text += "target\t" + join(n.target, "\t") + "\n";
    text += "exclude\t" + join(n.exclude, "\t") + "\n";
    text += "bound " + std::to_string(n.bound) + "\n";
    text += "edges_to_target " + std::string(name(n.edges_to_target)) + "\n";
    text += "skip_subtypes " + join(n.skip_subtypes, ",") + "\n";
    c.view.for_each_node([&](NodeIdx v) {
        text += "node\t" + net.node_key(v).value + "\t" + std::to_string(c.ds[v]) + "\t" + std::to_string(c.dt[v]) + "\n";
    });
    c.view.for_each_edge([&](EdgeIdx e) { text += "edge\t" + net.edge_key(e).text() + "\n"; });
    return core::Sha256::of(text);
}

}  // namespace detail

// ---------------------------------------------------------------- the step

namespace {

struct Endpoint {
    NodeIdx node = 0;
    std::string given;
};

// The edge test (05's step 3). Subtype names are compared as the network's interned handles, which
// the network's readers normalised (S2.10); a name no edge carries has no handle and matches nothing.
class SubtypeTest {
public:
    SubtypeTest(const Network& net, const std::vector<std::string>& skip, EdgesToTarget rule)
        : net_(net), skip_(syms(net, skip)), rule_(rule),
          activation_(syms(net, {"activation", "inhibition", "phosphorylation"})),
          expression_(syms(net, {"expression", "repression"})) {}

    // S4.04: one skipped subtype makes an edge non-traversable, as NOT_INTERAC_EDGE does
    // (Linux/include/macros.h:69-74, applied at GraphManagerNew.cc:17883-17886 and 18076-18079).
    bool skipped(EdgeIdx e) const noexcept { return carries(e, skip_); }

    // S4.05, fixed: each rule keeps the edges that carry its kind, so an edge carrying both kinds passes
    // both rules and one carrying neither passes neither. FuSION 1 dropped the other kind instead
    // (GraphManagerNew.cc:17901-17910, macros.h:76-77).
    bool passes_rule(EdgeIdx e) const noexcept {
        switch (rule_) {
            case EdgesToTarget::any: return true;
            case EdgesToTarget::activation: return carries(e, activation_);
            case EdgesToTarget::expression: return carries(e, expression_);
        }
        return true;
    }

    std::span<const core::Sym> skip_syms() const noexcept { return skip_; }

private:
    static std::vector<core::Sym> syms(const Network& net, const std::vector<std::string>& names) {
        std::vector<core::Sym> out;
        for (const std::string& n : names)
            if (auto s = net.interner().find_sym(n)) out.push_back(*s);
        std::sort(out.begin(), out.end());        // for lookups only
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }
    bool carries(EdgeIdx e, const std::vector<core::Sym>& set) const noexcept {
        for (core::Sym s : net_.subtypes(e))
            if (std::binary_search(set.begin(), set.end(), s)) return true;
        return false;
    }

    const Network& net_;
    std::vector<core::Sym> skip_;
    EdgesToTarget rule_;
    std::vector<core::Sym> activation_, expression_;
};

// Repeated ids collapse with a warning (S4.02).
void warn_repeated(Role role, const std::vector<std::string>& ids, core::Context& ctx) {
    std::vector<std::string> sorted = ids;
    std::sort(sorted.begin(), sorted.end(), byte_less);
    for (std::size_t i = 1; i < sorted.size(); ++i)
        if (sorted[i] == sorted[i - 1] && (i < 2 || sorted[i] != sorted[i - 2]))
            ctx.log.warning("corridor.repeated_id", std::string(detail::role_name(role)) + " " + sorted[i] +
                                                        " is listed more than once");
}

// The excluded set: the nodes the excluded ids bind to, closed upwards over complex membership, so a
// complex that contains an excluded node, directly or through a complex inside it, is excluded too
// (S4.06, extended in prompt log entry 170). One pass over the components builds the parent lists.
std::vector<Excluded> close_exclusions(const Network& net, std::vector<Endpoint> named, Bitset& excluded) {
    std::sort(named.begin(), named.end(), [](const Endpoint& a, const Endpoint& b) { return a.node < b.node; });
    std::vector<Excluded> list;
    for (const Endpoint& x : named) {
        excluded.set(x.node);
        list.push_back({x.node, x.given, false});
    }
    if (list.empty()) return list;

    const auto n = net.nodes().size();
    std::vector<std::uint32_t> first(n + 1, 0);
    for (NodeIdx c = 0; c < n; ++c)
        for (NodeIdx part : net.components(c)) ++first[part + 1];
    for (std::size_t i = 0; i < n; ++i) first[i + 1] += first[i];
    std::vector<NodeIdx> parents(first[n]);
    std::vector<std::uint32_t> fill(first.begin(), first.end() - 1);
    for (NodeIdx c = 0; c < n; ++c)
        for (NodeIdx part : net.components(c)) parents[fill[part]++] = c;

    const std::size_t direct = list.size();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const NodeIdx x = list[i].node;
        const std::string because = list[i].because;
        for (std::uint32_t k = first[x]; k < first[x + 1]; ++k) {
            const NodeIdx parent = parents[k];
            if (excluded.test(parent)) continue;
            excluded.set(parent);
            list.push_back({parent, because, true});
        }
    }
    std::sort(list.begin() + static_cast<std::ptrdiff_t>(direct), list.end(),
              [](const Excluded& a, const Excluded& b) { return a.node < b.node; });
    return list;
}

void print_summary(const Corridor& c, const Naming& naming, core::Context& ctx) {
    const Network& net = c.network();
    const Corridor::Impl& m = c.impl();
    const Counts& k = m.counts;
    auto named = [&](NodeIdx v) { return detail::named(net, v, naming.maps); };
    const std::string within = count_of(c.bound(), "edge", "edges");

    // S4.17: FuSION 1's counts, each labelled with what it counts (main.cc:2210 and
    // GraphManagerNew.cc:17933, 17953, 18156); the forward-graph count left with S4.08.
    ctx.log.info("Corridor from " + named(c.source()) + " to " + named(c.target()) + ", bound " +
                 std::to_string(c.bound()) + ", edges_to_target " + std::string(name(m.request.edges_to_target)));
    if (!m.excluded.empty()) {
        std::vector<std::string> direct, complexes;
        for (const Excluded& x : m.excluded) (x.complex_of_excluded ? complexes : direct).push_back(named(x.node));
        std::string line = "  Excluded: " + join(direct, ", ");
        if (!complexes.empty())
            line += std::string(", and the ") + (complexes.size() == 1 ? "complex " : "complexes ") +
                    join(complexes, ", ") + (complexes.size() == 1 ? " that contains " : " that contain ") +
                    (direct.size() == 1 ? "it" : "them");
        ctx.log.info(line);
    }
    const std::string skipping = m.request.skip_subtypes.empty() ? "none" : join(m.request.skip_subtypes, ", ");
    ctx.log.info("  Traversable edges: " + std::to_string(k.traversable_edges) + " of " +
                 std::to_string(net.edges().size()) + " (skipping " + skipping + ")");
    ctx.log.info("  Edges into the target from the forward set: " + std::to_string(k.into_target) +
                 "; dropped by edges_to_target: " + std::to_string(k.into_target_dropped));
    ctx.log.info("  Forward set: " + count_of(k.forward_set, "node", "nodes") + " within " + within +
                 " of the source");
    if (!c.empty()) {
        ctx.log.info("  Backward set: " + std::to_string(k.backward_set) + " of them within " + within +
                     " of the target");
        ctx.log.info(std::string("  Corridor") + (naming.graph > 0 ? ", graph " + std::to_string(naming.graph) : "") +
                     ": " + count_of(m.nodes, "node", "nodes") + ", " + count_of(m.edges, "edge", "edges") +
                     "; shortest path " + count_of(*m.shortest_path, "edge", "edges"));
    }

    core::Json event = core::Json::object();
    if (naming.graph > 0) event["graph"] = naming.graph;
    event["nodes"] = m.nodes;
    event["edges"] = m.edges;
    event["shortest_path"] = m.shortest_path ? core::Json(*m.shortest_path) : core::Json(nullptr);
    event["forward_set"] = k.forward_set;
    event["backward_set"] = k.backward_set;
    event["digest"] = m.digest.hex();
    ctx.events.emit("corridor.summary", std::move(event));
}

// S4.18: the empty corridor's report, naming its real cause (FuSION 1 blamed the edge rule for every
// case, GraphManagerNew.cc:17934-17938, or printed a false "Exiting.", 18046-18050).
std::string empty_message(const Network& net, NodeIdx s, NodeIdx t, const Request& request, EmptyReason reason,
                          std::optional<std::uint32_t> shortest, std::uint32_t dropped, const Naming& naming) {
    const std::string pair = detail::named(net, s, naming.maps) + " to " + detail::named(net, t, naming.maps);
    const std::string within = " within bound " + std::to_string(request.bound);
    switch (reason) {
        case EmptyReason::beyond_bound:
            return "no path from " + pair + within + ": the shortest path has " + count_of(*shortest, "edge", "edges");
        case EmptyReason::edges_to_target:
            return "no path from " + pair + within + ": edges_to_target=" + std::string(name(request.edges_to_target)) +
                   (dropped == 1 ? " dropped the edge into " : " dropped the " + std::to_string(dropped) + " edges into ") +
                   detail::named(net, t, naming.maps) +
                   (dropped == 1 ? " that lies within the bound" : " that lie within the bound");
        case EmptyReason::no_path:
            return "no path from " + pair + " over traversable edges";
        case EmptyReason::none: break;
    }
    return {};
}

}  // namespace

core::Outcome<std::shared_ptr<const Corridor>>
compute(std::shared_ptr<const network::Network> network_ptr, network::BindingTable& table, const Request& request,
        const Naming& naming, core::Context& ctx) {
    const Network& net = *network_ptr;
    const auto n_nodes = detail::to_u32(net.nodes().size());
    const auto n_edges = detail::to_u32(net.edges().size());
    const std::uint32_t b = request.bound;
    const Request norm = normalised(request);
    auto named = [&](NodeIdx v) { return detail::named(net, v, naming.maps); };

    // 1. Resolve every listed id once, through the run's BindingTable (S4.02, S4.15). FuSION 1 resolved
    //    them in the input graph and skipped an unknown one with a warning (GraphManagerNew.cc:17775-17822).
    warn_repeated(Role::source, request.source, ctx);
    warn_repeated(Role::target, request.target, ctx);
    warn_repeated(Role::excluded, request.exclude, ctx);
    std::vector<Bound> bound;
    for (const auto& [role, ids] : {std::pair{Role::source, &norm.source}, std::pair{Role::target, &norm.target},
                                    std::pair{Role::excluded, &norm.exclude}})
        for (const std::string& id : *ids) bound.push_back({role, table.bind(detail::role_name(role), id)});

    // An id that binds to no node stops the step: a source or target (S4.02, decision 0021) and an
    // excluded id as well (05.C5), where FuSION 1 ignored it (GraphManagerNew.cc:17815).
    std::vector<std::string> unknown;
    for (const Bound& x : bound) {
        if (x.binding.status == BindStatus::absent)
            unknown.push_back(std::string(detail::role_name(x.role)) + " " + x.binding.given);
        else if (x.binding.node >= n_nodes)
            throw core::Failure("internal", "fb_rch: the binding table belongs to another network");
    }
    if (!unknown.empty())
        throw core::Failure("corridor.unknown_id", join(unknown, ", ") + (unknown.size() == 1 ? " is" : " are") +
                                                       " not in " + network_name(naming));

    for (const Bound& x : bound) {
        const std::string who = std::string(detail::role_name(x.role)) + " " + x.binding.given;
        if (x.binding.status == BindStatus::ambiguous) {      // a legacy network's first claim (S2.27)
            ctx.log.warning("corridor.ambiguous_binding", who + " is held by " + std::to_string(x.binding.holders) +
                                                              " nodes; it binds to the first claim, " +
                                                              named(x.binding.node));
        } else if (x.binding.status == BindStatus::family && x.role == Role::excluded) {   // S2.03, S4.06
            std::vector<std::string> genes;
            for (core::GeneId id : net.members(x.binding.node)) genes.emplace_back(net.interner().str(id));
            ctx.log.warning("corridor.family_binding", who + " binds to " + named(x.binding.node) + ", which holds " +
                                                           count_of(genes.size(), "gene", "genes") + " (" +
                                                           join(genes, ", ") + "); the whole node is excluded");
        }
    }

    // 2. One source node and one target node, different from each other (S4.10 with S5.04; 05.C5).
    //    Ids that land on one node collapse with a warning (S4.02); FuSION 1 seeded such a node twice.
    auto collapse = [&](Role role) {
        std::vector<Endpoint> nodes;
        for (const Bound& x : bound) {
            if (x.role != role) continue;
            auto it = std::find_if(nodes.begin(), nodes.end(), [&](const Endpoint& e) { return e.node == x.binding.node; });
            if (it == nodes.end())
                nodes.push_back({x.binding.node, x.binding.given});
            else
                ctx.log.warning("corridor.repeated_id", std::string(detail::role_name(role)) + " " + x.binding.given +
                                                            " and " + it->given + " bind to one node, " +
                                                            named(x.binding.node));
        }
        return nodes;
    };
    const std::vector<Endpoint> sources = collapse(Role::source);
    const std::vector<Endpoint> targets = collapse(Role::target);
    std::vector<Endpoint> excluded_ids = collapse(Role::excluded);
    for (const auto& [role, list] : {std::pair{"source", &sources}, std::pair{"target", &targets}}) {
        if (list->size() == 1) continue;
        std::vector<std::string> keys;
        for (const Endpoint& e : *list) keys.push_back(net.node_key(e.node).value);
        throw core::Failure("corridor.one_pair",
                            (list->empty() ? "no " + std::string(role) + " node"
                                           : std::to_string(list->size()) + " " + role + " nodes (" + join(keys, ", ") + ")") +
                                "; a corridor has one source and one target");
    }
    const NodeIdx s = sources[0].node;
    const NodeIdx t = targets[0].node;
    if (s == t)
        throw core::Failure("corridor.same_node", "source " + sources[0].given + " and target " + targets[0].given +
                                                      " are the same node" +
                                                      (sources[0].given == targets[0].given ? "" : ", " + named(s)));

    // 3. Exclusions, closed over complexes; an excluded endpoint stops the step (05.C5), where FuSION 1
    //    failed later blaming the edge rule (S4.06, S4.18 cases 2 and 3).
    Bitset excluded(n_nodes);
    std::vector<Excluded> excluded_list = close_exclusions(net, std::move(excluded_ids), excluded);
    for (const Endpoint* e : {&sources[0], &targets[0]}) {
        if (!excluded.test(e->node)) continue;
        const std::string role = e == &sources[0] ? "source" : "target";
        const auto x = std::find_if(excluded_list.begin(), excluded_list.end(),
                                    [&](const Excluded& y) { return y.node == e->node; });
        throw core::Failure("corridor.endpoint_excluded",
                            x->complex_of_excluded
                                ? role + " " + e->given + " is " + named(e->node) +
                                      ", a complex that contains the excluded " + x->because
                                : role + " " + e->given + " is also in the exclusion list");
    }
    for (const Excluded& x : excluded_list)
        if (x.complex_of_excluded)
            ctx.log.warning("corridor.excluded_complex", "excluding " + x.because + " also removes the complex " +
                                                             named(x.node) + ", which contains it");

    // 4. The traversal graph T: traversable edges (S4.04) minus the edges into the target the rule drops
    //    (S4.05), the edges with an excluded end (S4.06), and every self-loop, edge into the source and
    //    edge out of the target, trimmed during the search (S4.11 as 05.C2 reads it, after S5.13).
    const SubtypeTest test(net, norm.skip_subtypes, request.edges_to_target);
    const std::span<const core::Sym> skip_syms = test.skip_syms();
    std::vector<bool> skip_seen(skip_syms.size(), false);
    Counts counts;
    Bitset in_t(n_edges);
    for (EdgeIdx e = 0; e < n_edges; ++e) {
        bool skipped = false;
        for (core::Sym st : net.subtypes(e)) {
            const auto it = std::lower_bound(skip_syms.begin(), skip_syms.end(), st);
            if (it != skip_syms.end() && *it == st) {
                skipped = true;
                skip_seen[static_cast<std::size_t>(it - skip_syms.begin())] = true;
            }
        }
        if (skipped) continue;
        ++counts.traversable_edges;
        const network::Edge& edge = net.edges()[e];
        if (edge.src == edge.tgt || edge.tgt == s || edge.src == t) continue;
        if (excluded.test(edge.src) || excluded.test(edge.tgt)) continue;
        if (edge.tgt == t && !test.passes_rule(e)) continue;
        in_t.set(e);
    }
    // A name the batch gave that no edge carries is likely a typo in a plug-and-play vocabulary
    // (decision 0020); the four defaults are never warned about.
    for (const std::string& n : norm.skip_subtypes) {
        if (std::find(std::begin(kDefaultSkips), std::end(kDefaultSkips), n) != std::end(kDefaultSkips)) continue;
        const auto sym = net.interner().find_sym(n);
        const auto it = sym ? std::lower_bound(skip_syms.begin(), skip_syms.end(), *sym) : skip_syms.end();
        if (it == skip_syms.end() || *it != *sym || !skip_seen[static_cast<std::size_t>(it - skip_syms.begin())])
            ctx.log.warning("corridor.unused_skip_subtype",
                            "skip_subtypes names " + n + ", which no edge of " + network_name(naming) + " carries");
    }

    // 5. Forward search (GraphManagerNew.cc:17858-17929, S4.07): breadth first from the source over T, in
    //    the network's edge order. A node is queued when first reached, where FuSION 1 searched the queue
    //    with std::find (17921). Nodes with ds < b are expanded; while the target is not reached the
    //    search goes on past b, so an empty corridor can report its shortest path (S4.18).
    const bool tracing = ctx.log.debug_on();
    std::vector<TraceRow> forward_trace, backward_trace;
    std::vector<std::uint32_t> ds(n_nodes, kInf);
    std::vector<NodeIdx> queue{s};
    ds[s] = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const NodeIdx u = queue[head];
        const bool found = ds[t] != kInf;
        if (ds[u] > b && found) break;
        if (ds[u] <= b) ++counts.forward_set;
        TraceRow row{u, ds[u], {}};
        if (ds[u] < b || !found) {
            for (EdgeIdx e : net.out_edges(u)) {
                if (!in_t.test(e)) continue;
                const NodeIdx v = net.edges()[e].tgt;
                if (ds[v] != kInf) continue;
                ds[v] = ds[u] + 1;
                queue.push_back(v);
                if (tracing) row.queued.push_back(v);
            }
        }
        if (tracing) forward_trace.push_back(std::move(row));
    }

    // 6. The two target counts (S4.17): FuSION 1's M and N, the edges into the target examined from
    //    nodes within the bound and those the rule dropped (GraphManagerNew.cc:17899, 17908, printed at
    //    17933). They pass every test of T but the rule, from a tail with ds <= b.
    std::uint32_t dropped_within = 0;   // dropped edges u -> target with ds(u) + 1 <= b
    for (EdgeIdx e : net.in_edges(t)) {
        const network::Edge& edge = net.edges()[e];
        if (edge.src == t || test.skipped(e) || excluded.test(edge.src)) continue;
        if (ds[edge.src] == kInf || ds[edge.src] > b) continue;
        ++counts.into_target;
        if (!test.passes_rule(e)) {
            ++counts.into_target_dropped;
            if (ds[edge.src] < b) ++dropped_within;
        }
    }

    // 7. Empty check (S4.18): an empty corridor is a reported result, with its real cause.
    const bool reached = ds[t] != kInf;
    EmptyReason reason = EmptyReason::none;
    if (!reached || ds[t] > b)
        reason = dropped_within > 0 ? EmptyReason::edges_to_target
                 : reached          ? EmptyReason::beyond_bound
                                    : EmptyReason::no_path;

    // 8. Backward search (GraphManagerNew.cc:18052-18099) on the forward set inside the network, with no
    //    intermediate forward graph (S4.08): breadth first from the target over the in-edges of T whose
    //    tail has ds <= b, expanding nodes with dt < b.
    std::vector<std::uint32_t> dt(n_nodes, kInf);
    Bitset keep_nodes(n_nodes), keep_edges(n_edges);
    std::uint32_t kept_nodes = 0, kept_edges = 0;
    if (reason == EmptyReason::none) {
        std::vector<NodeIdx> back{t};
        dt[t] = 0;
        for (std::size_t head = 0; head < back.size(); ++head) {
            const NodeIdx v = back[head];
            TraceRow row{v, dt[v], {}};
            if (dt[v] < b) {
                for (EdgeIdx e : net.in_edges(v)) {
                    if (!in_t.test(e)) continue;
                    const NodeIdx u = net.edges()[e].src;
                    if (ds[u] == kInf || ds[u] > b || dt[u] != kInf) continue;
                    dt[u] = dt[v] + 1;
                    back.push_back(u);
                    if (tracing) row.queued.push_back(u);
                }
            }
            if (tracing) backward_trace.push_back(std::move(row));
        }
        counts.backward_set = detail::to_u32(back.size());

        // 9. Keep (S4.09; GraphManagerNew.cc:18111, 18132): edge u -> v when ds(u) + 1 + dt(v) <= b, and its
        //    ends. Nodes enter only as ends of kept edges, so a complex member is a corridor node only when
        //    kept in its own right (S4.12), and no node is isolated. Nothing is copied, so FuSION 1's
        //    duplicate_edge merge (9388-9503) has no counterpart: 02.C5 merges duplicates at reading (S4.13).
        for (NodeIdx v : back) {
            if (std::uint64_t{ds[v]} + dt[v] > b) continue;
            for (EdgeIdx e : net.in_edges(v)) {
                if (!in_t.test(e)) continue;
                const NodeIdx u = net.edges()[e].src;
                if (ds[u] == kInf || std::uint64_t{ds[u]} + 1 + dt[v] > b) continue;
                keep_edges.set(e);
                ++kept_edges;
                for (NodeIdx end : {u, v})
                    if (!keep_nodes.test(end)) {
                        keep_nodes.set(end);
                        ++kept_nodes;
                    }
            }
        }
    }

    auto impl = std::make_unique<Corridor::Impl>(
        network::GraphView(network_ptr, std::move(keep_nodes), std::move(keep_edges)));
    impl->request = request;
    impl->bindings = std::move(bound);
    impl->excluded = std::move(excluded_list);
    impl->source = s;
    impl->target = t;
    impl->empty_reason = reason;
    if (reached) impl->shortest_path = ds[t];
    impl->counts = counts;
    impl->nodes = kept_nodes;
    impl->edges = kept_edges;
    impl->ds = std::move(ds);
    impl->dt = std::move(dt);
    impl->network_hash = net.content_hash();
    impl->digest = detail::digest_of(*impl);
    impl->traced = tracing;
    impl->forward_trace = std::move(forward_trace);
    impl->backward_trace = std::move(backward_trace);
    std::shared_ptr<const Corridor> corridor = std::make_shared<Corridor>(std::move(impl));

    // The debug option's self-check (run.debug turns on the modules' slow self-checks, 09).
    if (ctx.log.debug_on()) {
        const std::vector<std::string> problems = check_invariants(*corridor);
        if (!problems.empty())
            throw core::Failure("internal", "fb_rch: the corridor breaks its own rule: " + problems.front());
    }

    print_summary(*corridor, naming, ctx);
    core::Outcome<std::shared_ptr<const Corridor>> outcome{corridor, std::nullopt, false};
    if (reason != EmptyReason::none)
        outcome.report = core::Report{"corridor.empty", empty_message(net, s, t, request, reason, corridor->shortest_path(),
                                                                      dropped_within, naming)};
    return outcome;
}

core::Outcome<std::shared_ptr<const Corridor>>
compute(std::shared_ptr<const network::Network> network_ptr, network::BindingTable& table, const Request& request,
        core::Context& ctx) {
    return compute(std::move(network_ptr), table, request, Naming{}, ctx);
}

// ---------------------------------------------------------------- reuse (05.C4)

bool reusable(const Corridor& earlier, const network::Network& net, const Request& request) {
    if (normalised(earlier.request()) != normalised(request)) return false;
    return &earlier.network() == &net || earlier.impl().network_hash == net.content_hash();
}

// ---------------------------------------------------------------- observations (05.C9)

// FuSION 1 printed only "Up: 3 Down: 0" for the genes inside the corridor (main.cc:4693-4714, S4.16);
// 2.0 places every observed gene in one of S3.12's groups through section 04's matcher. The counts'
// console line is save_match()'s, so this prints nothing.
std::vector<expression::ObservationMatch> match_observations(const Corridor& c,
                                                             const expression::Observations& observations,
                                                             core::Context& /*ctx*/) {
    return expression::match_observations(observations, c.view());
}

void report_reuse(const Corridor& earlier, int graph, int same_as, core::Context& ctx) {
    ctx.log.info("Corridor, graph " + std::to_string(graph) + ": same as graph " + std::to_string(same_as));
    const Corridor::Impl& m = earlier.impl();
    core::Json event = core::Json::object();
    event["graph"] = graph;
    event["same_as"] = same_as;
    event["nodes"] = m.nodes;
    event["edges"] = m.edges;
    event["shortest_path"] = m.shortest_path ? core::Json(*m.shortest_path) : core::Json(nullptr);
    event["forward_set"] = m.counts.forward_set;
    event["backward_set"] = m.counts.backward_set;
    event["digest"] = m.digest.hex();
    ctx.events.emit("corridor.summary", std::move(event));
}

}  // namespace fusion::corridor
