// fusion/enrichment/enrichment.hpp: pre-ranked GSEA and the pathway shortlist, the gsea step
// (section 08, "Public interface"). The declarations are section 08's outline; what this header
// adds to it is marked "added". Xoshiro256pp and shuffle_like_rand09, which the outline also
// lists, are core's (fusion/core/xoshiro.hpp; 08.C1, cross-section item 1.19).
#pragma once

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
#include "fusion/expression/expression.hpp"
#include "fusion/network/network.hpp"

// libfusion, module enrichment (Choice C1); links network and expression (11.C6).
// core:: names are section 09's (cross-section item 1.1); network:: and expression::
// names are sections 02 and 04's.
namespace fusion::enrichment {

// ---- inputs ------------------------------------------------------------------

/// Genes by value, descending; ties by id, ascending, by core::stable_compare (S7.05, 08.C2).
struct Ranking {
    std::vector<core::GeneId> id;        // position -> gene
    std::vector<double>       value;     // the representative (04.C3), finite (S7.04)
    std::vector<double>       weight;    // |value|: weight 1, no pow call (S7.10)
    std::uint32_t             distinct_values = 0;  // the tie warning reports N - distinct_values (08.C10)
};
[[nodiscard]] Ranking rank(const expression::GeneValues& values, const core::Interner& ids);

/// One candidate pathway file (S7.06 to S7.08).
struct Candidate {
    network::ListLine         entry;     // line text and resolved path (2B.12); the text is written back (S7.15)
    std::string               id;        // "hsa04010": the root name without "path:"; else the file stem (2B.12)
    std::string               title;     // the KGML root's title (S7.16, entry 205); else the id
    std::vector<core::GeneId> genes;     // every gene id in the file, sorted, unique (S7.08, 08.C3)
    std::uint32_t             on_relations = 0;  // genes on a relation the merge keeps (08.C4)
    // added: every gene id in the file as text, sorted in byte order, unique. The keep list
    // (S7.14) and the relation share's set size (08.C5) count every gene, ranked or not, and the
    // text does not depend on which interner a document's ids went through.
    std::vector<std::string>  gene_text;
};
/// From the list entries and the documents the check parsed (02.C15). Duplicate pathway ids
/// were already refused by the list reader (list.duplicate_pathway, 1.24); this refuses them
/// too, naming both files (S7.07), and takes a file listed twice once.
/// This form keeps each document's own GeneIds, so `genes` is in the run's id space only when
/// the documents were interned in the run's interner.
[[nodiscard]] std::vector<Candidate>
make_candidates(std::span<const network::ListLine> entries,
                std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                const network::RelationFilters& filters);       // S2.11, S2.31 (08.C4)
/// added: the same, with `genes` mapped by text into the run's interner `ids`, which the ranking
/// uses. A gene `ids` lacks cannot be ranked and is left out of `genes`; `gene_text` keeps it.
[[nodiscard]] std::vector<Candidate>
make_candidates(std::span<const network::ListLine> entries,
                std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                const network::RelationFilters& filters, const core::Interner& ids);

// ---- the method --------------------------------------------------------------

inline constexpr std::uint32_t min_size    = 15, max_size = 500;  // S7.09, fixed
inline constexpr double        walk_weight = 1.0;                  // S7.10, fixed: weights are |value|

struct Settings {                         // registry keys gsea.seed, gsea.permutations (1.16)
    std::uint64_t seed         = 42;      // seed= (S7.11, 08.C1)
    std::uint32_t permutations = 1000;    // permutations= (S7.11, 08.C8)
};
void check_settings(const Settings&);     // throws core::Failure("batch.bad_value"): permutations outside 1..100,000
inline constexpr std::uint32_t max_permutations = 100'000;        // added: 08.C8's upper limit

struct Score {
    double es, nes, nom_p, fdr_q, fwer_p;
    std::vector<std::uint32_t> leading_edge;  // ranked positions, in Lead_genes order
    std::uint32_t              peak;          // position of the running-sum extreme (Gene %)
};

struct Row {                                  // one per candidate (S7.09)
    std::uint32_t        candidate;           // index into the candidate list
    std::uint32_t        ranked_genes;        // m: the `genes` column
    std::optional<Score> score;               // empty when untested
};

struct Prerank {
    std::vector<Row> rows;      // tested by |NES| descending (08.C9), then untested in list order
    std::uint32_t    tested = 0;
    std::uint64_t    zero_weight_draws = 0;   // 08.C7
};

/// Scores every candidate. Throws core::Failure("gsea.no_testable") when no set has
/// 15 to 500 ranked genes (S7.09), and core::Stopped after a stop request (09.C14).
[[nodiscard]] Prerank prerank(const Ranking& ranking, std::span<const Candidate> candidates,
                              const Settings& settings, const core::StopToken& stop);

// ---- selection -----------------------------------------------------------------

struct Selector {                             // S7.13: a conjunction, order-independent
    std::optional<double>        max_fdr, max_pval, min_abs_nes;
    std::optional<std::uint32_t> top;
    std::string                  text;        // as written, for messages and the report
};
[[nodiscard]] Selector parse_selector(std::string_view text);   // throws core::Failure("gsea.selector")

struct Selection {
    std::vector<bool>                     by_gsea;    // per candidate
    std::vector<std::vector<std::string>> kept_for;   // per candidate: keep ids, if chosen by them alone
    std::vector<std::string>              unmatched;  // keep ids in no candidate (warning, S7.14)
    // added: each keep id once, in keep order, with the number of candidates that hold it, for
    // the report's "candidate pathways per gene" (S7.17); empty without a keep list.
    std::vector<std::pair<std::string, std::uint32_t>> keep_counts;
    [[nodiscard]] bool chosen(std::uint32_t candidate) const;
    [[nodiscard]] bool empty() const;                 // the reported result gsea.empty_selection (S7.03)
};
/// Keep ids match each candidate's `gene_text` (S7.14); `ids` is not needed for that.
[[nodiscard]] Selection select(const Prerank& prerank, std::span<const Candidate> candidates,
                               const Selector& selector, std::span<const std::string> keep_ids,
                               const core::Interner& ids);

// ---- the step's value (2B.14) ------------------------------------------------------

struct Shortlist {
    std::vector<Candidate> candidates;        // list order
    Ranking                ranking;
    Prerank                prerank;
    Selector               selector;
    Selection              selection;
    std::filesystem::path  base;              // what relative lines resolved against: kgml_dir, else
                                              // the list's folder; recorded in run.json (2B.10)
    Settings               settings;          // added: the seed and permutation count of the scores
                                              // (S7.11), which the report prints (S7.17)
    [[nodiscard]] std::vector<std::filesystem::path> chosen_paths() const;  // resolved, list order (2B.10)
};

// ---- outputs -------------------------------------------------------------------

[[nodiscard]] std::string table_name(std::string_view shortlist_name);   // <stem>.gsea.tsv (S7.16)
void write_shortlist(std::ostream& out, const Shortlist& shortlist);    // S7.15
void write_table(std::ostream& out, const Shortlist& shortlist,
                 const core::Interner& ids);                            // core::format_number (1.20)
struct Shown { std::string values, candidates, shortlist, table; };     // paths as the report prints them
void report(core::Log& log, const Shortlist& shortlist, const core::Interner& ids,
            const Shown& shown);                                        // S7.17

// ---- the step, called by batch's gsea handler (01.C9) -------------------------------

struct Options {                              // the handler builds it from the bound arguments (08.C11)
    Selector                 selector;        // selector=
    std::vector<std::string> keep;            // keep=; absent or "0": none
    Settings                 settings;        // seed=, permutations=
    std::string              shortlist_name;  // shortlist=: its file name in the step directory (2B.3);
                                              // an absolute path is honoured as well, the files then
                                              // also written there (2B.3)
    std::string              candidates;      // added: candidates= as named, which the report prints
                                              // (S7.17); empty: "candidate list"
};
/// Writes the shortlist and the table through ctx.files (2A.6) and emits the gsea.* events.
/// The Outcome carries the Report gsea.empty_selection when nothing is chosen (S7.03).
[[nodiscard]] core::Outcome<std::shared_ptr<const Shortlist>>
gsea(const expression::GeneValues& values,                  // values= and its reading arguments (1.23, 04.C9)
     std::span<const network::ListLine> entries,            // candidates=, kgml_dir= (02.C12, 1.23)
     std::span<const std::shared_ptr<const network::PathwayDoc>> docs,   // parsed by the check (02.C15)
     const std::filesystem::path& base,                     // 2B.10
     const network::RelationFilters& filters,               // 08.C4
     const core::Interner& ids, const Options& options, core::Context& ctx);

}  // namespace fusion::enrichment
