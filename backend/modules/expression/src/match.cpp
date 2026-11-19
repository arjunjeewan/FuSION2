// fusion expression: observed genes against the network and the corridor (section 04, "The match
// report"; S3.12, S4.16, S2.03; 05.C9). FuSION 1 looked each list entry up in the solved graph's
// id map (Linux/src/GraphManagerNew.cc:797-808, 876-887, 18688-18708) and dropped the misses
// without a word: of the reference run's 45 listed genes, it printed only "Up: 3 Down: 0"
// (Linux/src/main.cc:4693).
#include <algorithm>
#include <array>

#include "internal.hpp"

namespace fusion::expression {

std::vector<ObservationMatch> match_observations(const Observations& observations,
                                                 const network::GraphView& corridor) {
    const Observations::Impl& impl = observations.impl();
    const network::Network& net = corridor.network();
    std::vector<ObservationMatch> matches;
    matches.reserve(impl.observed.size());
    for (std::size_t i = 0; i < impl.observed.size(); ++i) {
        ObservationMatch match;
        match.id = impl.observed[i];
        match.regulation = i < impl.n_up ? Regulation::up : Regulation::down;
        match.group = MatchGroup::not_in_network;
        // The exact lookup (S3.12, S2.03), by the id's text, so a network read with an interner of
        // its own matches as well. A legacy network may hold an id in several nodes: the first claim
        // is the binding, as in 02's BindingTable.
        if (const std::optional<core::GeneId> id = net.interner().find_id(impl.names[i])) {
            const std::span<const network::NodeIdx> holders = net.ids().holders(*id);
            if (!holders.empty()) {
                const network::NodeIdx node = holders.front();
                match.node = net.node_key(node);
                match.members = static_cast<std::uint32_t>(net.members(node).size());
                match.group = corridor.has_node(node) ? MatchGroup::in_corridor : MatchGroup::outside_corridor;
            }
        }
        matches.push_back(std::move(match));
    }
    // The report's order: by group, then up before down, then input order.
    std::stable_sort(matches.begin(), matches.end(),
                     [](const ObservationMatch& a, const ObservationMatch& b) { return a.group < b.group; });
    return matches;
}

void save_match(std::span<const ObservationMatch> matches, const GeneValues* values, const network::LabelMap& symbols,
                const core::Interner& interner, core::Context& ctx) {
    std::vector<ObservationMatch> rows(matches.begin(), matches.end());
    std::stable_sort(rows.begin(), rows.end(), [](const ObservationMatch& a, const ObservationMatch& b) {
        if (a.group != b.group) return a.group < b.group;
        return a.regulation > b.regulation;   // up (1) before down (-1)
    });
    std::array<std::size_t, 3> counts{};
    std::string text = "id\tsymbol\tregulation\tvalue\tgroup\tnode\tmembers\n";
    for (const ObservationMatch& row : rows) {
        const std::string_view id = interner.str(row.id);
        text += detail::tsv_cell(id);
        text += '\t';
        if (const std::optional<std::string_view> symbol = symbols.find(id)) text += detail::tsv_cell(*symbol);
        text += '\t';
        text += detail::name(row.regulation);
        text += '\t';
        if (values != nullptr)
            if (const std::optional<std::size_t> gene = values->find(row.id))
                text += core::format_number(values->value(*gene));
        text += '\t';
        text += detail::name(row.group);
        text += '\t';
        if (row.node) text += detail::tsv_cell(row.node->value);
        text += '\t';
        text += detail::integer(row.members);
        text += '\n';
        ++counts[static_cast<std::size_t>(row.group)];
    }
    detail::write_file(ctx, "observations_match.tsv", text);

    const std::size_t not_in_network = counts[static_cast<std::size_t>(MatchGroup::not_in_network)];
    const std::size_t outside = counts[static_cast<std::size_t>(MatchGroup::outside_corridor)];
    const std::size_t inside = counts[static_cast<std::size_t>(MatchGroup::in_corridor)];
    ctx.log.info(detail::console_prefix(ctx, "fb_rch") + ": observed genes: " + detail::integer(inside) +
                 " in the corridor, " + detail::integer(outside) + " outside it, " + detail::integer(not_in_network) +
                 " not in the network");
    core::Json event = core::Json::object();
    event["n_observed"] = rows.size();
    event["n_not_in_network"] = not_in_network;
    event["n_outside_corridor"] = outside;
    event["n_in_corridor"] = inside;
    ctx.events.emit("expression.matched", std::move(event));
}

}  // namespace fusion::expression
