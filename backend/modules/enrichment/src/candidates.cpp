// Candidates: each candidate file's id, title, gene set and relation count (section 08
// "Gene sets and the size filter"; S7.06 to S7.08, 08.C3 to 08.C5).
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/enrichment/enrichment.hpp"

namespace fusion::enrichment {
namespace {

// gsea_select_pathways.py:111 takes the root name with every "path:" removed.
std::string without_path_prefix(std::string_view name) {
    std::string out;
    for (std::size_t start = 0;;) {
        const std::size_t at = name.find("path:", start);
        out.append(name.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
        if (at == std::string_view::npos) return out;
        start = at + 5;
    }
}

bool by_handle(core::GeneId a, core::GeneId b) { return a.value < b.value; }

// The gene ids an endpoint puts on a relation: a gene entry's ids, and a group's components',
// through nested groups (08.C4; Phase 2's measurement counted them so). Other entry types add
// none. The reader refuses cyclic groups (S2.06); `seen` keeps this safe regardless.
void reached_genes(const network::PathwayDoc& doc, std::uint32_t local_id,
                   std::vector<std::uint32_t>& seen, std::vector<core::GeneId>& out) {
    const network::Entry* entry = doc.entry(local_id);
    if (entry == nullptr) return;
    const std::string_view type = doc.interner().str(entry->type);
    if (type == "gene") {
        const auto ids = doc.ids(*entry);
        out.insert(out.end(), ids.begin(), ids.end());
    } else if (type == "group" && std::find(seen.begin(), seen.end(), local_id) == seen.end()) {
        seen.push_back(local_id);
        for (const std::uint32_t component : doc.components(*entry)) reached_genes(doc, component, seen, out);
    }
}

// 08.C4: genes of the set that are an endpoint, or a component of a group endpoint, of a
// relation the merge keeps under the run's filters (S2.11, S2.31), as kept_relations has them.
std::uint32_t genes_on_relations(const network::PathwayDoc& doc, std::span<const core::GeneId> set,
                                 const network::RelationFilters& filters) {
    std::vector<core::GeneId> reached;
    std::vector<std::uint32_t> seen;
    const auto relations = doc.relations();
    for (const std::uint32_t r : network::kept_relations(doc, filters)) {
        if (r >= relations.size()) continue;
        seen.clear();
        reached_genes(doc, relations[r].entry1, seen, reached);
        seen.clear();
        reached_genes(doc, relations[r].entry2, seen, reached);
    }
    std::sort(reached.begin(), reached.end(), by_handle);
    reached.erase(std::unique(reached.begin(), reached.end()), reached.end());
    std::uint32_t count = 0;
    for (const core::GeneId g : set) count += std::binary_search(reached.begin(), reached.end(), g, by_handle) ? 1U : 0U;
    return count;
}

bool same_file(const std::filesystem::path& a, const std::filesystem::path& b) {
    return a.lexically_normal() == b.lexically_normal();
}

std::vector<Candidate> build(std::span<const network::ListLine> entries,
                             std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                             const network::RelationFilters& filters, const core::Interner* ids) {
    if (entries.size() != docs.size())
        throw std::invalid_argument("make_candidates: one document per list entry");
    std::vector<Candidate> candidates;
    candidates.reserve(entries.size());
    std::vector<const network::PathwayDoc*> taken;    // per candidate
    std::map<std::string, std::size_t> by_id;         // pathway id -> candidate; looked up only
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const network::PathwayDoc* doc = docs[i].get();
        if (doc == nullptr) throw std::invalid_argument("make_candidates: a list entry has no document");
        // One file listed twice is read once (cross-section item 1.24; the list reader warns).
        bool repeat = false;
        for (std::size_t c = 0; c < candidates.size() && !repeat; ++c)
            repeat = taken[c] == doc || same_file(candidates[c].entry.path, entries[i].path);
        if (repeat) continue;

        Candidate candidate;
        candidate.entry = entries[i];
        // The root name without "path:", else the file stem (gsea_select_pathways.py:111; 2B.12).
        candidate.id = without_path_prefix(doc->source().pathway);
        if (candidate.id.empty()) candidate.id = core::to_utf8(entries[i].path.stem());
        // S7.07: FuSION 1 kept the later of two files with one id, at the earlier one's place
        // (gsea_select_pathways.py:115); 2.0 refuses them, naming both (list.duplicate_pathway).
        if (const auto it = by_id.find(candidate.id); it != by_id.end()) {
            const std::string first = core::to_utf8(candidates[it->second].entry.path);
            const std::string second = core::to_utf8(entries[i].path);
            throw core::Failure("list.duplicate_pathway",
                                "pathway " + candidate.id + " is in two candidate files: " + first + " and " + second,
                                core::Where{second, 0, {}});
        }
        // The root title, else the id (gsea_select_pathways.py:115; entry 205).
        candidate.title = doc->source().title.empty() ? candidate.id : doc->source().title;

        // The gene set: every id of every gene entry, ':' removed by the id rule, repeats dropped
        // (gsea_select_pathways.py:112-114; S7.08, 08.C3).
        std::vector<core::GeneId> set = doc->gene_ids();
        std::sort(set.begin(), set.end(), by_handle);
        set.erase(std::unique(set.begin(), set.end()), set.end());
        candidate.gene_text.reserve(set.size());
        for (const core::GeneId g : set) candidate.gene_text.emplace_back(doc->interner().str(g));
        std::sort(candidate.gene_text.begin(), candidate.gene_text.end(),
                  [](const std::string& a, const std::string& b) { return core::stable_compare(a, b) < 0; });

        if (ids == nullptr || ids == &doc->interner()) {
            candidate.genes = set;
        } else {
            for (const std::string& text : candidate.gene_text)
                if (const auto g = ids->find_id(text)) candidate.genes.push_back(*g);
            std::sort(candidate.genes.begin(), candidate.genes.end(), by_handle);
        }
        candidate.on_relations = genes_on_relations(*doc, set, filters);

        by_id.emplace(candidate.id, candidates.size());
        taken.push_back(doc);
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

}  // namespace

std::vector<Candidate> make_candidates(std::span<const network::ListLine> entries,
                                       std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                                       const network::RelationFilters& filters) {
    return build(entries, docs, filters, nullptr);
}

std::vector<Candidate> make_candidates(std::span<const network::ListLine> entries,
                                       std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                                       const network::RelationFilters& filters, const core::Interner& ids) {
    return build(entries, docs, filters, &ids);
}

}  // namespace fusion::enrichment
