// The gsea step, called by batch's handler (section 08 "Design: the step", "Files written",
// "Events", "Errors and reported results"; S7.01 to S7.03, S7.15 to S7.17, 08.C12, 08.C13).
// FuSION 1 shelled out to gsea_select_pathways.py (Linux/src/main.cc:5796-5820); 2.0 runs no
// helper process (S7.02).
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "detail.hpp"
#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {
namespace {

// The run-relative name of a file in a step directory, as the events give it: "gsea/<name>".
std::string in_step(const std::filesystem::path& file) {
    return core::to_utf8(file.parent_path().filename() / file.filename());
}

// Ranked genes that no candidate holds (the gsea.ranked event; 30 on the real inputs).
std::uint32_t in_no_candidate(const Ranking& ranking, const std::vector<Candidate>& candidates) {
    std::vector<core::GeneId> held;
    for (const Candidate& c : candidates) held.insert(held.end(), c.genes.begin(), c.genes.end());
    const auto by_handle = [](core::GeneId a, core::GeneId b) { return a.value < b.value; };
    std::sort(held.begin(), held.end(), by_handle);
    std::uint32_t count = 0;
    for (const core::GeneId g : ranking.id) count += std::binary_search(held.begin(), held.end(), g, by_handle) ? 0U : 1U;
    return count;
}

}  // namespace

core::Outcome<std::shared_ptr<const Shortlist>>
gsea(const expression::GeneValues& values, std::span<const network::ListLine> entries,
     std::span<const std::shared_ptr<const network::PathwayDoc>> docs, const std::filesystem::path& base,
     const network::RelationFilters& filters, const core::Interner& ids, const Options& options,
     core::Context& ctx) {
    check_settings(options.settings);
    if (options.shortlist_name.empty())
        throw core::Failure("batch.bad_argument", "gsea needs a shortlist file name (shortlist=)");

    auto shortlist = std::make_shared<Shortlist>();
    shortlist->base = base;
    shortlist->selector = options.selector;
    shortlist->settings = options.settings;
    shortlist->candidates = make_candidates(entries, docs, filters, ids);
    shortlist->ranking = rank(values, ids);
    const Ranking& ranking = shortlist->ranking;
    const auto n = static_cast<std::uint32_t>(ranking.id.size());

    ctx.events.emit("gsea.ranked", core::Json{{"genes", n},
                                              {"distinct_values", ranking.distinct_values},
                                              {"in_no_candidate", in_no_candidate(ranking, shortlist->candidates)}});
    // gseapy's tie warning (gsea.py:516-520) with 08.C10's second sentence: it counts the genes
    // whose value occurs earlier in the ranking.
    if (const std::uint32_t repeats = n - ranking.distinct_values; repeats > 0)
        ctx.log.warning("gsea.ties", "Duplicated values found in the ranking: " +
                                         detail::percent(static_cast<double>(repeats) / static_cast<double>(n)) +
                                         " of genes. Tied genes are ordered by gene id.");

    shortlist->prerank = prerank(ranking, shortlist->candidates, options.settings, ctx.stop);
    const Prerank& pr = shortlist->prerank;
    ctx.events.emit("gsea.tested", core::Json{{"candidates", shortlist->candidates.size()},
                                              {"tested", pr.tested},
                                              {"permutations", options.settings.permutations},
                                              {"seed", options.settings.seed}});
    if (pr.zero_weight_draws > 0)
        ctx.log.warning("gsea.zero_weight_draws",
                        std::to_string(pr.zero_weight_draws) +
                            " draws had hits that all have value 0; each of their hits took the step 1/m (08.C7)");

    shortlist->selection = select(pr, shortlist->candidates, options.selector, options.keep, ids);
    const Selection& sel = shortlist->selection;
    for (const std::string& id : sel.unmatched)
        ctx.log.warning("gsea.keep_unmatched", "keep id " + id + " is in none of the " +
                                                   std::to_string(shortlist->candidates.size()) + " candidate pathways");

    // The outputs (2B.3): a relative name lands in the step directory; an absolute path is
    // honoured as well, the files also kept in the step directory. Both files are written whole
    // (08.C13): under temporary names, committed only once both are written, the shortlist last,
    // so a failure or a stop leaves no shortlist for mff to merge.
    const std::filesystem::path given = core::path_from_utf8(options.shortlist_name);
    std::string name = options.shortlist_name;
    std::optional<std::filesystem::path> also, also_table;
    if (given.is_absolute()) {
        name = core::to_utf8(given.filename());
        also = given;
        also_table = given.parent_path() / core::path_from_utf8(table_name(name));
    }
    core::OutputFile list_file = ctx.files.create(name);
    core::OutputFile table_file = ctx.files.create(table_name(name));
    if (also) {
        list_file.also_at(*also);
        table_file.also_at(*also_table);
    }
    write_shortlist(list_file.out(), *shortlist);
    write_table(table_file.out(), *shortlist, ids);
    if (!list_file.out() || !table_file.out())
        throw core::Failure("io.write", "cannot write the shortlist or its table in " + core::to_utf8(ctx.files.step_dir()));
    table_file.commit();
    list_file.commit();

    const Shown shown{core::to_utf8(values.input().path), options.candidates,
                      core::to_utf8(also ? *also : list_file.path()),
                      core::to_utf8(also ? *also_table : table_file.path())};
    report(ctx.log, *shortlist, ids, shown);

    std::size_t by_gsea = 0, by_keep = 0;
    for (std::size_t c = 0; c < sel.by_gsea.size(); ++c) {
        by_gsea += sel.by_gsea[c] ? 1U : 0U;
        by_keep += !sel.by_gsea[c] && !sel.kept_for[c].empty() ? 1U : 0U;
    }
    ctx.events.emit("gsea.selected", core::Json{{"selector", options.selector.text},
                                                {"by_gsea", by_gsea},
                                                {"by_keep", by_keep},
                                                {"shortlist", in_step(list_file.path())},
                                                {"table", in_step(table_file.path())}});

    core::Outcome<std::shared_ptr<const Shortlist>> outcome{shortlist, std::nullopt, false};
    // S7.03: an empty selection is a reported result, not a failure; the batch skips the point's
    // later lines (01.C15). FuSION 1 printed this line and exited 1 (gsea_select_pathways.py:236-238).
    if (sel.empty())
        outcome.report = core::Report{"gsea.empty_selection",
                                      "no pathway passed '" + options.selector.text + "'; loosen the selector"};
    return outcome;
}

}  // namespace fusion::enrichment
