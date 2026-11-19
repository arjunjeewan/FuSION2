// Writers: the shortlist, the table and the console report (section 08 "Files written" and
// "Console report"; S7.15 to S7.17, 08.C5, 08.C10).
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "detail.hpp"
#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {

namespace detail {

std::string fixed(double value, int decimals) {
    char buf[400];   // a double's integer part has at most 309 digits
    const auto r = std::to_chars(buf, buf + sizeof buf, value, std::chars_format::fixed, decimals);
    return r.ec == std::errc{} ? std::string(buf, r.ptr) : std::string();
}

std::string signed_fixed(double value, int decimals) {
    std::string text = fixed(value, decimals);
    return !text.empty() && text.front() == '-' ? text : "+" + text;
}

std::string percent(double share) {
    // CPython formats "%" by multiplying by 100 in double arithmetic, then as "f".
    return fixed(share * 100.0, 2) + "%";
}

std::string gene_percent(std::uint32_t peak, std::uint32_t n, double es) {
    const double count = es >= 0.0 ? static_cast<double>(peak) + 1.0 : static_cast<double>(n - peak);
    return percent(count / static_cast<double>(n));
}

std::size_t code_points(std::string_view text) noexcept {
    std::size_t count = 0;
    for (const char ch : text) count += (static_cast<unsigned char>(ch) & 0xC0U) != 0x80U ? 1U : 0U;
    return count;
}

std::string tsv_field(std::string_view text) {
    std::string out(text);
    std::replace_if(out.begin(), out.end(), [](char ch) { return ch == '\t' || ch == '\r' || ch == '\n'; }, ' ');
    return out;
}

}  // namespace detail

namespace {

std::string pad_left(std::string_view text, std::size_t width) {   // Python's {:>N}
    const std::size_t len = detail::code_points(text);
    return std::string(len < width ? width - len : 0, ' ') + std::string(text);
}

std::string pad_right(std::string_view text, std::size_t width) {   // Python's {:<N}
    const std::size_t len = detail::code_points(text);
    return std::string(text) + std::string(len < width ? width - len : 0, ' ');
}

std::string join(const std::vector<std::string>& items, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += sep;
        out += items[i];
    }
    return out;
}

}  // namespace

// os.path.splitext(shortlist)[0] + ".gsea.tsv" (gsea_select_pathways.py:231): the last extension
// of the last path component goes, unless the name is only dots before it. The table's name
// therefore never equals the shortlist's.
std::string table_name(std::string_view shortlist_name) {
    const std::size_t sep = shortlist_name.find_last_of("/\\");
    const std::size_t start = sep == std::string_view::npos ? 0 : sep + 1;
    const std::size_t dot = shortlist_name.rfind('.');
    std::string_view stem = shortlist_name;
    if (dot != std::string_view::npos && dot >= start) {
        const std::string_view before = shortlist_name.substr(start, dot - start);
        if (before.find_first_not_of('.') != std::string_view::npos) stem = shortlist_name.substr(0, dot);
    }
    return std::string(stem) + ".gsea.tsv";
}

// S7.15 (gsea_select_pathways.py:227-230): one line per chosen pathway, the list line's text, in
// list order, "\n" on every platform; nothing at all when nothing is chosen.
void write_shortlist(std::ostream& out, const Shortlist& shortlist) {
    for (std::size_t c = 0; c < shortlist.candidates.size(); ++c)
        if (shortlist.selection.chosen(static_cast<std::uint32_t>(c))) out << shortlist.candidates[c].entry.text << '\n';
}

// S7.16 (gsea_select_pathways.py:213-224, 231-232): every candidate's row, tested ones by |NES|
// (08.C9), then the untested in list order with empty scores (S7.09); relation_share after genes
// (08.C5); numbers through core::format_number in Python's repr layout, as pandas wrote them.
void write_table(std::ostream& out, const Shortlist& shortlist, const core::Interner& ids) {
    out << "Term\tpathway_name\tgenes\trelation_share\tES\tNES\tNOM p-val\tFDR q-val\tFWER p-val\t"
           "Tag %\tGene %\tLead_genes\tselected\tkept_for\tkgml\n";
    const auto n = static_cast<std::uint32_t>(shortlist.ranking.id.size());
    // Integers go out as text, so the stream's locale cannot group their digits (09.C16).
    const auto count = [](std::size_t x) { return std::to_string(x); };
    for (const Row& row : shortlist.prerank.rows) {
        const Candidate& c = shortlist.candidates[row.candidate];
        out << detail::tsv_field(c.id) << '\t' << detail::tsv_field(c.title) << '\t' << count(row.ranked_genes) << '\t'
            << count(c.on_relations) << '/' << count(c.gene_text.size()) << '\t';
        if (row.score) {
            const Score& s = *row.score;
            std::vector<std::string> lead;
            lead.reserve(s.leading_edge.size());
            for (const std::uint32_t p : s.leading_edge) lead.emplace_back(ids.str(shortlist.ranking.id[p]));
            out << core::format_number(s.es) << '\t' << core::format_number(s.nes) << '\t'
                << core::format_number(s.nom_p) << '\t' << core::format_number(s.fdr_q) << '\t'
                << core::format_number(s.fwer_p) << '\t' << count(s.leading_edge.size()) << '/' << count(row.ranked_genes)
                << '\t' << detail::gene_percent(s.peak, n, s.es) << '\t' << detail::tsv_field(join(lead, ";")) << '\t';
        } else {
            out << "\t\t\t\t\t\t\t\t";
        }
        out << (shortlist.selection.chosen(row.candidate) ? "True" : "False") << '\t'
            << detail::tsv_field(join(shortlist.selection.kept_for[row.candidate], ",")) << '\t'
            << detail::tsv_field(c.entry.text) << '\n';
    }
}

// S7.17 (gsea_select_pathways.py:138-160, 234-235), with 08.C10's wording and three decimals in
// the per-pathway lines (4A.7). Each line is one Log::info call; the CLI prints them on stdout.
void report(core::Log& log, const Shortlist& shortlist, const core::Interner& /*ids*/, const Shown& shown) {
    const Prerank& pr = shortlist.prerank;
    const Selection& sel = shortlist.selection;
    const std::vector<Candidate>& candidates = shortlist.candidates;

    double lowest = 0.0, highest = 0.0;
    bool first = true;
    for (const Row& row : pr.rows) {
        if (!row.score) continue;
        lowest = first ? row.score->nes : std::min(lowest, row.score->nes);
        highest = first ? row.score->nes : std::max(highest, row.score->nes);
        first = false;
    }
    log.info("GSEA pre-ranked on " + shown.values);
    log.info("  " + std::to_string(shortlist.ranking.id.size()) + " genes ranked by log-fold change, " +
             std::to_string(shortlist.settings.permutations) + " permutations");
    log.info("  " + std::to_string(pr.tested) + " of " + std::to_string(candidates.size()) +
             " candidate pathways tested (the rest have fewer than " + std::to_string(min_size) + " or more than " +
             std::to_string(max_size) + " ranked genes)");
    log.info("  NES over tested pathways: " + detail::signed_fixed(lowest, 3) + " to " + detail::signed_fixed(highest, 3));
    log.info("");

    // The rows shown: GSEA's choices by |NES|, then the kept ones in table order, the untested last.
    std::vector<const Row*> rows;
    std::size_t by_gsea = 0;
    for (const Row& row : pr.rows)
        if (sel.by_gsea[row.candidate]) rows.push_back(&row);
    by_gsea = rows.size();
    for (const Row& row : pr.rows)
        if (!sel.by_gsea[row.candidate] && !sel.kept_for[row.candidate].empty()) rows.push_back(&row);
    const bool keep = !sel.keep_counts.empty();

    log.info("selected " + std::to_string(by_gsea) + " of " + std::to_string(pr.tested) + " tested pathways (" +
             shortlist.selector.text + ")");
    if (keep) {
        // FuSION 1 named the script's --keep option here; 2.0's argument is keep (08.C11).
        std::vector<std::string> per_gene;
        for (const auto& [id, count] : sel.keep_counts) per_gene.push_back(id + " " + std::to_string(count));
        log.info("plus " + std::to_string(rows.size() - by_gsea) +
                 " kept for containing a keep gene (candidate pathways per gene: " + join(per_gene, ", ") + ")");
    }
    if (!rows.empty()) {
        std::size_t width = 4;   // len("name")
        for (const Row* row : rows) width = std::max(width, detail::code_points(detail::tsv_field(candidates[row->candidate].title)));
        const std::string header = "  " + pad_left("#", 3) + "  " + pad_right("pathway", 9) + " " + pad_right("name", width) +
                                   " " + pad_left("genes", 5) + " " + pad_left("ES", 7) + " " + pad_left("NES", 7) + " " +
                                   pad_left("NOM p", 7) + " " + pad_left("FDR q", 7) + (keep ? "  kept for" : "");
        log.info(header);
        log.info("  " + std::string(detail::code_points(header) - 2, '-'));
        std::size_t number = 0;
        for (const Row* row : rows) {
            const Candidate& c = candidates[row->candidate];
            const auto& s = row->score;
            std::string line = "  " + pad_left(std::to_string(++number), 3) + "  " + pad_right(c.id, 9) + " " +
                               pad_right(detail::tsv_field(c.title), width) + " " +
                               pad_left(std::to_string(row->ranked_genes), 5) + " " +
                               pad_left(s ? detail::signed_fixed(s->es, 3) : "-", 7) + " " +
                               pad_left(s ? detail::signed_fixed(s->nes, 3) : "-", 7) + " " +
                               pad_left(s ? detail::fixed(s->nom_p, 3) : "-", 7) + " " +
                               pad_left(s ? detail::fixed(s->fdr_q, 3) : "-", 7);
            if (keep) line += "  " + join(sel.kept_for[row->candidate], ",");
            log.info(line);
        }
    }
    std::size_t chosen = 0;
    for (std::size_t c = 0; c < candidates.size(); ++c) chosen += sel.chosen(static_cast<std::uint32_t>(c)) ? 1U : 0U;
    log.info("");
    log.info("  shortlist  -> " + shown.shortlist + "  (" + std::to_string(chosen) + " KGML files, in " +
             (shown.candidates.empty() ? std::string("candidate list") : shown.candidates) + " order)");
    log.info("  full table -> " + shown.table);
}

}  // namespace fusion::enrichment
