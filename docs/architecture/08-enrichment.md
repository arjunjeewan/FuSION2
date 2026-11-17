# 08 · Enrichment

## Scope

This section designs the `enrichment` module (Choice C1). It rebuilds stage 7's `gsea`/`gsea_select_pathways` (S7.01 to S7.19) in C++, with no Python (Choice F1). In FuSION 1 the command is a C++ handler (`Linux/src/main.cc:5766-5830`) that runs `Linux/supporting_files/gsea_select_pathways.py` (243 lines) on gseapy 1.3.1. The command ranks the genes of a per-gene value file, scores every candidate pathway file by pre-ranked GSEA, selects a shortlist, and writes two files: the shortlist that `mff` merges next, and a table with a row for every candidate.

The module is a library. The `gsea` handler and checker live in `batch` (01.C9), which binds the arguments and calls this module (cross-section item 1.2). `fusion_enrichment` links only `network` and `expression` (11.C6), so it never sees `batch` or `run` types.

Left to other sections:
- Section 04 reads the value file with the one reader (S3.14, S7.04) and hands over one finite value per gene, the representative (04.C3).
- Section 02 reads the candidate list (02.C12), parses each candidate file in the check before the run (02.C15), maps ids (S2.14), and owns the edge settings (S2.11, S2.31).
- Section 03 merges the shortlist (S2.16).
- Section 09 owns the core vocabulary (cross-section item 1.1), the run directory and where outputs land (2B.3), `OutputFile` and the clash check (2A.6, 2A.7), `run.json`, events, the settings registry, error codes, core's random numbers and stable orderings, and `format_number`.
- Section 01 owns the batch syntax, the command table with `gsea`'s handler and checker (01.C9), and skipping a point's later lines after a reported result (01.C15).
- Sections 10, 11 and 12 own the frontend block and review, build flags and licences, and the test harness.

Names used below:
- **The real inputs** are `egf_hela_t120_lfc.tsv` (3054 genes, md5 `a1243fbc…`) and the 167-line candidate list (md5 `ac05b281…`) that all 19 `gsea` batches use. They were read from `runs/egfr_cxcl8_jun_b10_nr8_er8_gsea_fdr_sweep/fdr025/inputs/`.
- **gseapy's core** is the Rust code behind gseapy 1.3.1's `prerank`: `src/stats.rs`, `src/algorithm.rs` and `src/utils.rs` at tag `v1.3.1` of github.com/zqfang/GSEApy. It is compiled into the `.venv`'s `gse.cpython-312-x86_64-linux-gnu.so`, and draws its random numbers from rand 0.9.2's `SmallRng` (its `Cargo.lock`). gseapy's Python files are cited as in stage 7: "gseapy `gsea.py:493`" is the file in FuSION 1's `.venv`. Reading the core settles stage 7's open question 1.
- **The reference port** is a Python transcription of this section's algorithm, about 150 lines, written for this draft and run with FuSION 1's `.venv` (gseapy 1.3.1). It is kept outside the repository. It is the evidence for 08.C1, and the section's verifier wrote an independent port that agrees with it.
- **N, C, S, P and m** are the number of ranked genes, candidates, tested sets and permutations, and a set's number of ranked genes.
- About 28 FuSION solves were running on this machine while this section was drafted (load average 28.9 on 36 logical CPUs). Timings taken here are therefore upper bounds, and comparisons use stage 7's timings (S7.19).
- `<TAB>` stands for a tab character.

## Requirements carried in

From Phase 1 and the skeleton:
- **The command** (S7.01): five positional arguments, long name `gsea_select_pathways`, by position or by name ([decision 0018](../decisions/0018-batch-interface.md)). An absent fifth token or `0` means no keep list. A wrong argument count is reported before the analysis runs.
- **No helper process** (S7.02, F1, S1.20): no shell, no Python, no script lookup. The versions that shape the result are recorded (E1); here that is FuSION's own version.
- **Failures and the empty selection** (S7.03, S1.28): a failure stops the batch before `mff`, giving its own reason. An empty selection is a reported result, "no pathway passed '<selector>'", so one strict cut-off does not halt a sweep.
- **One value reader** (S7.04, S3.14, with S3.03 to S3.05 and S3.08): `gsea` ranks exactly the values `cudf` classifies. A non-finite value is skipped and reported by the reader, so it never reaches GSEA.
- **Ties** (S7.05): tied values are ordered by gene id, ascending, in byte order (08.C2). The shortlists change (fdr:0.25 gives 44 instead of 42), as [decision 0023](../decisions/0023-results-may-change-where-improved.md) allows.
- **Candidate list** (S7.06): read with the same reader and checks as `mff`'s list (S2.16). Every listed path is checked before the run starts ([decision 0021](../decisions/0021-required-and-optional-inputs.md)).
- **Duplicate pathway ids** (S7.07): an error that names both files.
- **Gene sets** (S7.08): every name of every `type="gene"` entry of the file, colon removed, repeats dropped. The table gains a column giving each pathway's share of genes that sit on its relations (08.C4, 08.C5).
- **Size filter** (S7.09): a set is tested when 15 to 500 of its genes are ranked. Every candidate gets a table row; an untested one has its ranked-gene count and no scores. No testable candidate is a `gsea` failure.
- **Enrichment score** (S7.10): the weight-1 running sum, leading edge, `Tag %` and `Gene %` are kept.
- **Significance** (S7.11): label permutations, NES, nominal p, FDR q and FWER p are kept. The seed (default 42) and the permutation count (default 1000) are run settings recorded in the output.
- **Pooled FDR** (S7.12): kept.
- **Selector** (S7.13): the keys `fdr`, `pval`, `nes` and `top`; only finite, in-range values (`fdr` and `pval` 0 to 1, `nes` at least 0, `top` a positive integer); thresholds apply before `top:N`, so term order never matters; an error names any bad term.
- **Keep list** (S7.14): kept. Id forms are handled by section 02's id mapping (prompt log entry 086).
- **Shortlist** (S7.15): kept. One line per chosen pathway, the path as the list gave it, in list order, no header, empty when nothing is chosen.
- **Table** (S7.16): `<stem>.gsea.tsv`, kept, with the changes from S7.04, S7.08 and S7.09.
- **Console report** (S7.17): kept, with two changes since decided: the tie warning's second sentence is reworded (08.C10), and the statistics table's three decimals are a recorded exception to S1.27 (cross-section item 4A.7, prompt log entry 168). The report's second line keeps "log-fold change" (entry 183). gseapy's NaN warning has no successor here: section 04's reader reports the skipped lines instead (S7.04).
- **No pause** (S7.18): review by stopping after `gsea` ([decision 0019](../decisions/0019-partial-runs.md)) or in the frontend.
- **Cost** (S7.19): the step is kept as it is, and repeats are avoided by starting from a saved shortlist (decision 0019). Prompt log entry 087 (Q3) offered sharing one parse of the candidate KGMLs between `gsea` and `mff`, and the answer was Keep. So no parse is shared between them (cross-section item 4A.5, entry 158); Cost below gives the measured price.
- **Outputs** (S1.26, S8.21, E1): every output goes to the run directory; tables are TSV with a header; settings go into `run.json`.
- **Settings and debug** (S1.29): every result-changing setting is recorded; debug output sits behind one option.
- **Frontend** (S8.27, S8.18): shortlisting is a pipeline block, and the default pipeline includes it.
- **Plug and play** ([decision 0020](../decisions/0020-plug-and-play-inputs.md)): the network and the values may come from any source or modality.
- **Checks up front** (decision 0021, S8.30): inputs are validated before the run, by the backend and by the frontend.
- **Skeleton:** events go to `events.jsonl` (A2); the library reports through the log and never prints (B1); seeded random numbers and stable orderings live in `core` (C1); each step's output is saved and loadable, and no step keeps state between calls (D1); the platforms are Linux, macOS and Windows, all native (G3).

From the Phase 2 polls:
- **Core vocabulary** (cross-section items 1.1 and 2A.5, entry 158): section 09's names. A failure is a `core::Failure` exception; a reported result is a `core::Report` inside a step's `core::Outcome<T>`; a stop is `core::Stopped` (09.C14, entry 186). Steps receive a `core::Context` with `core::Log` and `core::Events`. Ids compare by `core::stable_compare`, in unsigned byte order.
- **Reported results** (cross-section items 1.8 and 3A.14, entry 167): `gsea.empty_selection` is one of three Reports. After it, every later line of that run or sweep point is skipped, and the run ends ok with exit 0 (01.C15, entry 167).
- **Error codes** (cross-section item 1.17, entry 158): a condition found by the generic check or the binder takes 09's or 01's code (`input.*`, `io.write`, `output.clash`, `batch.*`). A module keeps codes only for its own faults. Entry 230 names `batch.bad_argument` for argument problems and `batch.bad_value` for out-of-range values, `gsea.permutations` included.
- **Events** (cross-section item 1.3, entry 190): dotted names with the key `type`. The envelope is `v`, `seq`, `t` and `type`, and `core::Events` adds `step`, `line` and `point`. Warnings go only through `warning`, with a dotted code and a message; reported results go only through `step.finished`'s `report`.
- **Settings** (cross-section items 1.16 and 2B.2, entry 190): the registry's key is `<module>.<name>`, such as `gsea.seed`, and a command line uses the bare name. The `run` line and `fusion --set` set run-level values, and `fusion --commands` exports them.
- **Outputs** (cross-section items 2A.6 and 2A.7, entry 158; 2B.3 and 4A.11, entry 169): every module writes through 09's `OutputFile`, and 09's check refuses clashing outputs by file identity. A relative output path lands in the step's directory. An explicit absolute path is honoured as well: the file is also kept in the step directory, and both are recorded in `run.json`.
- **Lists** (cross-section items 2B.10, 2B.12 and 2B.14, entry 158): section 02's list reader returns each line's text with its resolved path, and a KGML without a root `name` takes its file stem as id. `gsea` records in `run.json` the base its relative lines resolved against. `mff` uses the in-memory resolved paths, and reading a saved shortlist uses the recorded base. This section defines `enrichment::Shortlist`.
- **Arguments** (cross-section item 1.23, entry 158): `gsea` takes `kgml_dir` (02.C12, entry 162) and the value-reading arguments `id_column`, `value_column`, `id_map` and `repeats` (04.C9, entry 168).
- **Values** (04.C2 and 04.C3, entry 167): one decimal grammar for every real argument; a repeated id's representative is the value with the largest magnitude, the earlier line winning a tie.
- **Candidate files** (02.C15, entry 159): the check before the run fully parses every candidate file, and the `gsea` step uses that parse. Two files with one pathway id fail with `list.duplicate_pathway`, naming both files, for `gsea` and `mff` alike; one file listed twice is read once, with a warning (03.C6 and cross-section item 1.24, entry 166).
- **Pathway names** (section 11's open question 4, entry 205): KEGG pathway names are not shipped. They come at run time from each KGML root's `title`, as FuSION 1's script already took them (`gsea_select_pathways.py:115`).
- **Numbers in tables** (cross-section item 1.20, entry 180): `core::format_number` writes the shortest exact digits in Python's `repr` layout; JSON is written by nlohmann/json.
- **Random numbers** (08.C1 and cross-section item 1.19, entry 159): core holds this section's generator for GSEA, and 09's `core::Rng` (09.C15, entry 186) for everything else.
- **Floating point** (09.C16 with 11.C7, 12.C11 and cross-section items 1.18 and 3A.18, entry 186): no build contracts `a*b+c` into a fused multiply-add, and canonical outputs are byte-identical on all three platforms. A result path avoids the platform's `exp`, `log` and `pow`, or uses one correctly rounded implementation.
- **Module edges** (11.C6 and cross-section item 1.2, entry 201): one static library per module; `fusion_enrichment` links `network` and `expression`; command handlers live in `batch`.

## Design

### Components

1. **Ranking.** Turns section 04's representatives into the ranked list.
2. **Candidates.** Builds each candidate's gene set and relation count from section 02's list entries and the documents the check parsed.
3. **Prerank engine.** Runs the permutation chain, the observed and null enrichment scores, and the statistics.
4. **Selection.** Applies the selector and the keep list.
5. **Writers.** Write the shortlist and the table to streams, and the console report to the log.
6. **The step.** `enrichment::gsea` runs 1 to 5 for `batch`'s handler, writes the two files through 09's `Files`, emits the events, and returns the `Shortlist`. The checker calls `parse_selector` and `check_settings`.

### Data and sizes

Measured on the real inputs with a Python reader of the same files:
- **Ranking:** N = 3054 genes with 190 distinct values. 2994 genes share their value with another gene, and 2864 (93.78 %) repeat a value that occurs earlier, gseapy's count. The largest blocks are 145 genes at 0.00, 71 at −0.06, and 68 each at 0.03 and −0.11. The file is already in byte-wise id order, and no id repeats.
- **Candidates:** C = 167 files, 6.85 MB of KGML, the largest 181 KB. Their gene sets hold 8 to 533 genes (median 103): 21,205 memberships over 5615 distinct genes. 3024 of the 3054 ranked genes are in at least one set, so 30 are in none. Every file has a root `name` and `title`.
- **Tested sets:** S = 166. Their ranked sizes m run from 18 to 490 (median 84.5); Σm = 16,323.

Structures:
- **`Ranking`:** interned gene ids, representatives and weights (|value|, since the weight is 1) by position, and the number of distinct values.
- **`Candidate`:** the list entry (line text and resolved path), the pathway id and title, the sorted gene ids, and the count of genes on relations (08.C4).
- **Hit lists:** for each tested set, the ranked positions of its genes, ascending; Σm = 16,323 integers.
- **Membership index:** the inverse of the hit lists. For each ranked position, the tested sets that hold that gene, stored as an offsets array of N + 1 entries and one array of Σm set numbers (78 KB here).
- **Observed scores:** S doubles.
- **Null matrix:** S × P doubles in one block, one row per set: 166 × 1000 × 8 B = 1.3 MB. The normalised null scores overwrite it, and the FDR sorts it in place once FWER has read it.
- **Walk state:** per set, the weight sum, the running sum, its extreme, the last hit position and the two step sizes: 48 bytes per set.
- **Permutation:** one array of N 32-bit integers.
- **`Shortlist`:** the step's value (cross-section item 2B.14): the candidates, the scores, the selector, the selection and the base. `batch` keeps it in the run's workspace for `mff`.

The engine's peak memory on the real inputs is about 2 MB, plus section 02's parsed candidate files. FuSION 1's step peaks at 318 MB (S7.19).

### Public interface

```cpp
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
};
/// From the list entries and the documents the check parsed (02.C15). Duplicate pathway ids
/// were already refused by the list reader (list.duplicate_pathway, 1.24).
[[nodiscard]] std::vector<Candidate>
make_candidates(std::span<const network::ListLine> entries,
                std::span<const std::shared_ptr<const network::PathwayDoc>> docs,
                const network::RelationFilters& filters);       // S2.11, S2.31 (08.C4)

// ---- the method --------------------------------------------------------------

inline constexpr std::uint32_t min_size    = 15, max_size = 500;  // S7.09, fixed
inline constexpr double        walk_weight = 1.0;                  // S7.10, fixed: weights are |value|

struct Settings {                         // registry keys gsea.seed, gsea.permutations (1.16)
    std::uint64_t seed         = 42;      // seed= (S7.11, 08.C1)
    std::uint32_t permutations = 1000;    // permutations= (S7.11, 08.C8)
};
void check_settings(const Settings&);     // throws core::Failure("batch.bad_value"): permutations outside 1..100,000

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
    [[nodiscard]] bool chosen(std::uint32_t candidate) const;
    [[nodiscard]] bool empty() const;                 // the reported result gsea.empty_selection (S7.03)
};
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
    std::string              shortlist_name;  // shortlist=: its file name in the step directory (2B.3)
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

// Needed from core (section 09), with exactly this stream (08.C1); 09's core::Rng serves everything else.
namespace fusion::core {

class Xoshiro256pp {                               // rand 0.9.2's SmallRng on 64-bit targets
public:
    explicit Xoshiro256pp(std::uint64_t seed);     // four SplitMix64 steps
    std::uint64_t next_u64();
    std::uint32_t next_u32();                      // the top 32 bits of next_u64()
    std::uint32_t below(std::uint32_t bound);      // Canon's method, as rand's random_range(..bound)
};

/// Fisher-Yates as rand 0.9.2's SliceRandom::shuffle, with IncreasingUniform.
void shuffle_like_rand09(std::span<std::uint32_t> a, Xoshiro256pp& rng);

}  // namespace fusion::core
```

### Algorithms

#### Ranking (S7.05)

- Sort the (id, representative) pairs by value, descending, then by id, ascending, in byte order: `core::stable_compare` on the ids' text (08.C2). This is the order of the real file and of S7.05's measured row: `hsa1000` < `hsa10000` < `hsa999`.
- A repeated id is ranked once, at its representative (04.C3); the real inputs have no repeats.
- Every gene of the value file is ranked, including the 30 that no candidate holds, as gseapy does. N enters every score through the miss step 1/(N − m).
- O(N log N) time, O(N) memory.

#### Gene sets and the size filter (S7.08, S7.09)

- Each candidate's document was parsed by the check before the run (02.C15); the step reads nothing again.
- Its gene set is every gene id in the document, as FuSION 1's script collected them (`gsea_select_pathways.py:110-115`): for KGML every `type="gene"` entry, for other formats as 08.C3 says. Its id is the root `name` without `path:`, or the file stem (2B.12). Its title is the KGML root's `title` (entry 205), or the id.
- The set's hits are the ranked positions of its genes, ascending.
- A set is tested when 15 ≤ m ≤ 500 and m < N, gseapy's filter (gseapy `base.py:460-520`).
- O(Σ|G|) lookups in a hash map from id to position: 21,205 here.

#### The permutation chain (08.C1)

- gseapy permutes gene labels: one sequence of permutations of the N positions serves every set (gseapy core `algorithm.rs:507-521`, `stats.rs:597-641`).
- Permutation 0 is the identity and gives the observed scores. Permutation k, for k = 1 to P, is permutation k − 1 shuffled in place, so the shuffles form a chain.
- **Generator:** Xoshiro256++, seeded from the 64-bit seed by four SplitMix64 steps (rand 0.9.2 `src/rngs/xoshiro256plusplus.rs:50-66`). A 32-bit draw is the top half of the next 64-bit output (`:70-75`).
- **Shuffle:** rand 0.9.2's Fisher–Yates (`src/seq/slice.rs:384-413`). For i = 0 to N − 1 it swaps element i with element j, j uniform in 0 to i. j comes from `IncreasingUniform` (`src/seq/increasing_uniform.rs:40-108`): one bounded draw below (i+1)(i+2)…, the largest such product under 2^32, serves several consecutive i, each taking its digit with `%` and `/`.
- **Bounded draw** below b: Canon's method (`src/distr/uniform_int.rs:176-209`). The result is the high half of a 32 × 32-bit product. When the low half exceeds 2^32 − b, a second draw's high half is added to the low half, and an overflow adds 1 to the result.
- O(P·N) time, sequential, and O(N) memory. A shuffle of 3054 genes makes 1228 bounded draws, always. Canon's method sometimes draws twice, so a shuffle takes 1353 to 1409 32-bit draws (the first takes 1378, the mean is 1382), and P = 1000 takes 1,381,866.

#### Enrichment scores (S7.10)

- In permutation k, position p holds ranked gene π_k(p). Set s has a hit at p when π_k(p) is one of its genes. The weights are w_p = |r_p|.
- **Pass 1:** for p = 0 to N − 1, add w_p to W_s for every set s in the membership list of π_k(p).
- **Pass 2:** for p = 0 to N − 1, for every such s:
  - `cur_s −= q1_s · (p − last_s − 1)`, and keep cur_s as the extreme if |cur_s| is larger;
  - `cur_s += q2_s · w_p`, and test again;
  - `last_s = p`;
  - where q1_s = 1/(N − m_s), q2_s = 1/W_s, and the start values are cur = 0, extreme = 0, last = −1.
- When W_s is 0, every hit of the draw has value 0, and q2_s = 1/m_s instead (08.C7).
- The extreme is the first value with the largest |cur|, by strict comparison. It is set s's score for permutation k: the observed score for k = 0, row s, column k − 1 of the null matrix otherwise.
- This is gseapy's `fast_random_walk` (gseapy core `algorithm.rs:302-338`) with the same operations in the same order: W_s summed in position order, and multiplication by the two reciprocals. The doubles are therefore identical, as the reference port shows (08.C1).
- O(P·(N + Σm)) time: two passes of 1001 × 19,377 steps, about 39 million. gseapy instead builds an N-length indicator for every set and permutation and scans it, O(P·S·N): about 507 million positions, each visited several times.

#### Observed score, leading edge, `Tag %` and `Gene %`

- The observed score is permutation 0's.
- The leading edge comes from the running sum written out at every position, as gseapy's `running_enrichment_score` computes it (gseapy core `algorithm.rs:219-242`): w_p/W for a hit and −1/(N − m) for a miss, accumulated in position order.
- For ES ≥ 0, its first maximum, at position i, gives the hits at positions ≤ i, in ranking order, and `Gene %` = (i + 1)/N. For ES < 0, its first minimum, at i, gives the hits at positions ≥ i, listed from the bottom up, and `Gene %` = (N − i)/N (gseapy `base.py:639-715`).
- `Tag %` is `<leading-edge size>/<m>`. `Gene %` is a percentage with two decimals.
- O(S·N) time: 0.5 million steps.

#### Statistics (S7.11, S7.12)

As gseapy's core computes them (`stats.rs:142-221`, `272-313`, `377-450`, `495-527`), for a set with observed score e and null scores n_1 to n_P:
- **Nominal p:** for e ≥ 0, #(n_k ≥ e) / #(n_k ≥ 0); for e < 0, #(n_k ≤ e) / #(n_k < 0); 1 when the denominator is 0.
- **NES:** e / mean(n_k ≥ 0) for e ≥ 0, and e / |mean(n_k < 0)| for e < 0. A mean is the sum in permutation order divided by the count. When no null score has the needed sign, the mean is e itself, so NES is 1 or −1.
- **Normalised null scores:** n_k / mean(n ≥ 0) when n_k ≥ 0, and n_k / |mean(n < 0)| otherwise.
- **FDR q,** pooled over all tested sets of the call (S7.12), for a set with NES*:
  - for NES* ≥ 0: [#(normalised nulls ≥ NES*) / #(normalised nulls ≥ 0)] / [#(NES ≥ NES*) / #(NES ≥ 0)];
  - for NES* < 0: [#(normalised nulls ≤ NES*) / #(normalised nulls < 0)] / [#(NES ≤ NES*) / #(NES < 0)];
  - a share with a zero denominator is 0; the q-value is 1 when the observed share is 0, and is otherwise capped to 0 to 1;
  - there is no monotone adjustment: gseapy's `adjust_fdr` (`stats.rs:453`) is never called.
- **FWER p:** for each permutation k, take the largest non-negative normalised null over all sets (0 if none) and the smallest negative one (0 if none). For NES* ≥ 0, FWER is #(k: largest ≥ NES*) / P. For NES* < 0, it is #(k: smallest < NES*) / #(k: smallest < 0), and 1.0 when that denominator is 0 (08.C7).
- gseapy splits the pool by any `__` prefix in set names (`stats.rs:277-286`). No pathway id has one, and 2.0 always pools all tested sets of the call (S7.12).
- No `exp`, `log` or `pow` is called anywhere: weight 1 makes |r|^1 the absolute value itself, and the statistics use only counts, sums, products, quotients and comparisons. So the byte-identical promise (09.C16) needs nothing more here.
- O(S·P) time for p, NES and FWER. O(SP log SP) for the FDR: one in-place sort of the null matrix's 166,000 normalised scores, after FWER has read them.

#### Selection (S7.13, S7.14)

- `parse_selector` splits the text on `,`. Each term is `key:value` with a key of exactly `fdr`, `pval`, `nes` or `top`. A value for `fdr`, `pval` or `nes` must match section 04's decimal grammar (04.C2) and lie in range; `top` takes an integer of at least 1. Anything else is `gsea.selector`, naming the term.
- Thresholds first. A tested pathway passes when FDR q ≤ Q, nominal p ≤ P and |NES| ≥ X for every `fdr`, `pval` and `nes` term given. A repeated key applies every time it appears. (FuSION 1 applied the terms in turn, `gsea_select_pathways.py:119-130`; S7.13 replaced that.)
- Then `top:N` keeps the first N passing pathways in table order (|NES| descending, 08.C9). With several `top` terms, the smallest N applies.
- The keep list: each keep id chooses every candidate whose gene set (all its genes, ranked or not) holds it, tested or not, as FuSION 1 does (`gsea_select_pathways.py:205-211`). An id in no candidate is a warning.
- O(S log S) for the thresholds and `top`, plus O(K·C·log |G|) for K keep ids, by binary search in the C sorted gene sets.

#### Cost

Estimates from the operation counts above; Phase 3's benchmark measures them (Test hooks):
- parsing the 167 KGMLs, which section 02's check does before the run (02.C15) with pugixml: expat only tokenises them in 123 ms here, under load (56 MB/s), and pugixml is commonly several times faster, so tens of ms;
- the chain: 1,381,866 32-bit draws and 3.05 million index steps, tens of ms;
- the walks: about 39 million steps, about 0.1 s on one thread;
- statistics, selection and writing: a few ms.

The estimated total is about 0.2 s on one thread, against 3.3 s wall and 13.4 s CPU for FuSION 1's step, of which 1.7 s is Python start-up and 1.3 s is GSEA on 4 threads (S7.19). Memory falls from 318 MB to a few MB. At 10,000 permutations, which S7.11 measured at 11.6 s in gseapy, the estimate is about 1.5 s.

The second parse that S7.19 keeps costs little. `mff` reads the shortlisted files again: 2.0 MB (42 files) to 4.3 MB (90 files) of the candidates' 6.85 MB, on the order of 10 ms, against merges of seconds (S2.22) and solves of hours. Section 03's merge reuse (S2.22) may serve `mff` from a saved merge without parsing at all.

### Arguments

`gsea`, long name `gsea_select_pathways` (S7.01, 08.C11):

| Position | Name | Type | Default | Notes |
|---|---|---|---|---|
| 1 | `values` | path | required | the per-gene value file (S7.04), read by section 04's reader (04.C9) |
| 2 | `candidates` | path | required | the candidate list (S7.06), read by section 02's list reader (02.C12); the shipped `data/kegg_pathways.txt` of bare pathway ids (entry 204) works with `kgml_dir` |
| 3 | `selector` | text | required | S7.13 |
| 4 | `shortlist` | path | required | S7.15; where it lands is 2B.3's rule |
| 5 | `keep` | comma-separated gene ids | none; `0` also means none | S7.14 |
| | `seed` | unsigned 64-bit integer | 42, from `gsea.seed` | S7.11, 08.C1 |
| | `permutations` | integer, 1 to 100,000 | 1000, from `gsea.permutations` | S7.11, 08.C8 |
| | `kgml_dir` | path | none: the list's folder | 02.C12; cross-section item 1.23 |
| | `id_column`, `value_column`, `id_map`, `repeats` | | | as for `cudf` (04.C9; cross-section item 1.23) |

The positional form takes four or five arguments (S7.01). All 19 census lines use positions only, such as `gsea $LOG_FOLD_CHANGE_FILE $LIST_OF_MERGED_KEGG_XML_FILE $GSEA_SELECTOR $GSEA_KGML_LIST`.

### Files written

Both files come from the `shortlist` argument. A relative path lands in the step's directory, `gsea/` (09.C2 and cross-section item 1.4, entry 177). An absolute path, as every FuSION 1 batch gives (`$WORK_DIR/run/gsea_kgml_files.txt`), is honoured as well: the file is written there and kept in `gsea/`, and both are recorded in `run.json` (2B.3). The table follows the shortlist to both places. Each file is written through 09's `Files::create` and `OutputFile`: under a temporary name, then renamed on commit (2A.6, 08.C13). The step's entry in `run.json` also records the base that the list's relative lines resolved against (2B.10).

**The shortlist, `shortlist`** (S7.15), as FuSION 1 writes it (`gsea_select_pathways.py:227-230`). Example: 2.0 on the real inputs at `fdr:0.25`, 44 lines (FuSION 1 wrote 42), in `gsea/gsea_kgml_files.txt`:

```
/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score/hsa04010.xml
/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score/hsa04062.xml
/home/ks-149-u24/Desktop/arjun/FuSION/auxilliary/hsa_pathways_with_exp_score/hsa04064.xml
…
/home/ks-149-u24/Desktop/arjun/FuSION/updated_pathways/xml_files/hsa05224.xml
```

- One line per chosen pathway: its list line's text, in list order, ending in `\n` on every platform.
- 0 bytes when nothing is chosen.

**The table, `<stem>.gsea.tsv`** (S7.16), next to the shortlist (`gsea/gsea_kgml_files.txt` gives `gsea/gsea_kgml_files.gsea.tsv`; FuSION 1: `gsea_select_pathways.py:213-224`, `231-232`). Its name can never equal the shortlist's.
- **Columns:** `Term`, `pathway_name`, `genes` (m), `relation_share` (new; 08.C4, 08.C5), `ES`, `NES`, `NOM p-val`, `FDR q-val`, `FWER p-val`, `Tag %`, `Gene %`, `Lead_genes`, `selected`, `kept_for`, `kgml`.
- **Rows:** every tested candidate by |NES|, descending (ties as 08.C9), then every untested candidate in list order, with empty score fields (S7.09).
- **Values:**
  - numbers through `core::format_number`: the shortest exact digits in Python's `repr` layout, as FuSION 1's pandas tables wrote them (`0.0`, `0.001`, `0.0009217452670540877`; cross-section item 1.20);
  - booleans `True` and `False`, and missing values as empty fields;
  - `pathway_name` is the candidate's title (entry 205); `kept_for` holds the keep ids as given, joined by `,`; `kgml` holds the list line's text;
  - lines end in `\n`, and text is UTF-8; a tab, CR or LF inside a title becomes a space (none of the 167 titles has one).
- **Example:** 2.0 on the real inputs, as the reference port computes it, with `relation_share` as 08.C4 and 08.C5 have it. A `…` line stands for rows left out; paths and gene lists are shortened with `…`:

```
Term<TAB>pathway_name<TAB>genes<TAB>relation_share<TAB>ES<TAB>NES<TAB>NOM p-val<TAB>FDR q-val<TAB>FWER p-val<TAB>Tag %<TAB>Gene %<TAB>Lead_genes<TAB>selected<TAB>kept_for<TAB>kgml
hsa05219<TAB>Bladder cancer<TAB>39<TAB>28/41<TAB>0.7483846298094701<TAB>2.0382134604945144<TAB>0.0<TAB>0.0<TAB>0.0<TAB>10/39<TAB>5.96%<TAB>hsa3576;hsa7057;hsa1026;…<TAB>True<TAB><TAB>/home/…/hsa05219.xml
…
hsa05033<TAB>Nicotine addiction<TAB>32<TAB>35/41<TAB>-0.4868931423280892<TAB>-1.6524929368801782<TAB>0.007194244604316547<TAB>0.2024096385542169<TAB>0.125<TAB>16/32<TAB>30.81%<TAB>hsa2902;hsa1137;hsa773;…<TAB>True<TAB><TAB>/home/…/hsa05033.xml
…
hsa05203<TAB>Viral carcinogenesis<TAB>138<TAB>6/205<TAB>0.43192568656153046<TAB>1.4094582855165823<TAB>0.05116796440489433<TAB>0.23445752305597656<TAB>0.995<TAB>48/138<TAB>20.76%<TAB>hsa3725;hsa5366;hsa1026;…<TAB>True<TAB><TAB>/home/…/hsa05203.xml
…
hsa04012<TAB>ErbB signaling pathway<TAB>80<TAB>86/86<TAB>0.44446888910237936<TAB>1.352990626076861<TAB>0.08788598574821853<TAB>0.2731591691305312<TAB>1.0<TAB>15/80<TAB>12.34%<TAB>hsa374;hsa3725;hsa1026;…<TAB>False<TAB><TAB>/home/…/hsa04012.xml
…
hsa04122<TAB>Sulfur relay system<TAB>6<TAB>8/8<TAB><TAB><TAB><TAB><TAB><TAB><TAB><TAB><TAB>False<TAB><TAB>/home/…/hsa04122.xml
```

  The last row is the one untested candidate (6 ranked genes), which FuSION 1's table left out (S7.09).

No debug output: the table already holds every score, and the null scores are not written.

### Console report (S7.17)

The report goes through `core::Log::info`, which the CLI prints on stdout. Example, 2.0 on the real inputs (the name column is narrowed here; S7.17 sizes it to the longest name shown):

```
GSEA pre-ranked on /home/…/inputs/egf_hela_t120_lfc.tsv
  3054 genes ranked by log-fold change, 1000 permutations
  166 of 167 candidate pathways tested (the rest have fewer than 15 or more than 500 ranked genes)
  NES over tested pathways: -1.652 to +2.038

selected 44 of 166 tested pathways (fdr:0.25)
    #  pathway   name                     genes      ES     NES   NOM p   FDR q
  ----------------------------------------------------------------------------
    1  hsa05219  Bladder cancer              39  +0.748  +2.038   0.000   0.000
    2  hsa04668  TNF signaling pathway      106  +0.626  +2.005   0.000   0.002
  …

  shortlist  -> /home/…/run/gsea_kgml_files.txt  (44 KGML files, in /home/…/inputs/list_of_kegg_xml_files.txt order)
  full table -> /home/…/run/gsea_kgml_files.gsea.tsv
```

- **Three decimals** stay in the per-pathway lines and the NES range, a recorded exception to S1.27 (4A.7, entry 168). The table holds the exact values.
- **The tie warning** goes through `core::Log::warning` with the code `gsea.ties`, so it reaches stderr, the run log and `events.jsonl`. As 08.C10 has it, it keeps gseapy's count and states the rule that now orders ties: `Duplicated values found in the ranking: 93.78% of genes. Tied genes are ordered by gene id.`
- **Other warnings** take the same path: a keep id in no candidate (`gsea.keep_unmatched`, S7.14) and draws whose hits all have value 0 (`gsea.zero_weight_draws`, 08.C7).

### Settings

| Key | Type | Default | Given as | Source |
|---|---|---|---|---|
| `gsea.seed` | unsigned 64-bit integer | 42 | `seed=` on the `gsea` line | S7.11; the same meaning as gseapy's `seed` (08.C1) |
| `gsea.permutations` | integer, 1 to 100,000 | 1000 | `permutations=` on the `gsea` line | S7.11; range 08.C8 |

- Both are registry keys (1.16). A `gsea` line's `seed=` or `permutations=` wins. Without it, the registry's value applies, which the `run` line or `fusion --set gsea.seed=7` may change (2B.2); a seed sweep can therefore leave the `gsea` line as it is. `fusion --commands` exports both.
- Both are recorded in `run.json` with their source (E1). The command's other arguments are recorded as `run.json` records every argument; `kgml_dir` itself changes no result, and the resolved paths and hashes are recorded instead (02.C12).
- Fixed parts of the method, not settings: sizes 15 to 500 (S7.09), weight 1 (S7.10), ties by id (S7.05, 08.C2), pooling over the call's tested sets (S7.12) and the generator (08.C1). They change only with FuSION's version, which `run.json` records.

### Events (A2)

Events follow 09's envelope (`v`, `seq`, `t`, `type`), with `step` and `line` added by `core::Events` (cross-section item 1.3). The examples are for the real inputs at `fdr:0.25`, with a keep list naming PHLDA1 (`hsa22822`, in no KGML, S7.14) for the second warning:

```
{"v":1,"seq":14,"t":"2026-11-16T10:02:11.204Z","type":"gsea.ranked","step":"gsea","line":169,"genes":3054,"distinct_values":190,"in_no_candidate":30}
{"v":1,"seq":15,"t":"2026-11-16T10:02:11.205Z","type":"warning","step":"gsea","line":169,"code":"gsea.ties","message":"Duplicated values found in the ranking: 93.78% of genes. Tied genes are ordered by gene id."}
{"v":1,"seq":16,"t":"2026-11-16T10:02:11.391Z","type":"gsea.tested","step":"gsea","line":169,"candidates":167,"tested":166,"permutations":1000,"seed":42}
{"v":1,"seq":17,"t":"2026-11-16T10:02:11.392Z","type":"warning","step":"gsea","line":169,"code":"gsea.keep_unmatched","message":"keep id hsa22822 is in none of the 167 candidate pathways"}
{"v":1,"seq":18,"t":"2026-11-16T10:02:11.396Z","type":"gsea.selected","step":"gsea","line":169,"selector":"fdr:0.25","by_gsea":44,"by_keep":0,"shortlist":"gsea/gsea_kgml_files.txt","table":"gsea/gsea_kgml_files.gsea.tsv"}
```

An empty selection is reported only through 09's `step.finished`, and the point's later lines are skipped (01.C15). With `nes:2.5`, which nothing passes on the real inputs (no |NES| exceeds 2.038, while every `fdr:` cut-off keeps Bladder cancer, FDR q 0.0):

```
{"v":1,"seq":19,"t":"2026-11-16T10:02:11.398Z","type":"step.finished","step":"gsea","line":169,"status":"reported","wall_s":0.2,"report":{"code":"gsea.empty_selection","message":"no pathway passed 'nes:2.5'; loosen the selector"}}
{"v":1,"seq":20,"t":"2026-11-16T10:02:11.399Z","type":"step.skipped","step":"mff","line":171,"because":"gsea reported an empty selection"}
```

`step.started`, `step.finished` on success or failure, and `step.skipped` are 09's generic events.

### Errors and reported results

**Failures** are `core::Failure` exceptions (09.C14). The interpreter reports each with its batch line and stops the batch with a non-zero exit (S1.28). Those found by the check also appear in `fusion --check --json` (cross-section item 1.9).

| Code | When | Found in |
|---|---|---|
| `batch.bad_argument` | a wrong number of positional arguments; an unknown, duplicate or missing argument; a positional argument after a named one (S7.01; entry 230) | the check |
| `batch.bad_value` | a value not of its type, such as a seed that is not an unsigned 64-bit integer; permutations outside 1 to 100,000 (08.C8; entry 230) | the check |
| `gsea.selector` | a bad selector term, named in the message (S7.13) | the check |
| 09's `input.missing`, `input.unreadable`, `input.not_file` | the value file, the candidate list or a listed file missing or unreadable (S7.06, 0021) | the check |
| section 04's `values.*` codes | the value file has no value, a wrong encoding or a missing column | the check |
| section 02's reader codes | a malformed candidate file (S2.06) | the check (02.C15) |
| `list.duplicate_pathway` | two listed files with one pathway id; the message names both files (S7.07; 03.C6, 1.24) | the check |
| `output.clash` | the shortlist or table path names the value file, the candidate list or a candidate file (08.C12, by 2A.7's check) | the check |
| `io.write` | an output cannot be written, for example on a full disk | the step |
| `gsea.no_testable` | "no candidate has 15 to 500 ranked genes" (S7.09) | the step |

**Reported result** (S7.03): `gsea.empty_selection`, a `core::Report` in the step's `core::Outcome`. It is not a failure. The table and an empty shortlist are written, the console says `no pathway passed '<selector>'; loosen the selector`, and every later line of that run or sweep point is skipped; the run ends ok with exit 0 (01.C15, 1.8).

**Warnings:** `gsea.ties` (08.C10), `gsea.keep_unmatched` (S7.14), `gsea.zero_weight_draws` (08.C7), and the list reader's warning for one file listed twice, which is then read once (entry 166).

**Stop** (A1): the step calls `ctx.stop.throw_if_stopped()` every 64 permutations, which throws `core::Stopped`. An uncommitted `OutputFile` removes its temporary file, so a stop leaves no output of this step and no resume state; the step takes well under a second.

### Portability (G3)

- **No process:** no shell, interpreter or helper is started (S7.02), so nothing platform-specific runs.
- **Integers:** the generator uses `std::uint64_t` and `std::uint32_t` only. `unsigned long` is 32 bits on Windows and 64 on Linux and macOS, so it must not appear.
- **Floating point:** identical doubles on x86-64 and ARM64 need IEEE double arithmetic with no fused multiply-adds and no fast-math. Clang's default (`-ffp-contract=on`) fuses `a − b·c` into one instruction on Apple silicon, which would change the last bits. The build turns contraction off everywhere (09.C16, 11.C7, entry 186), and this module calls no `exp`, `log` or `pow`.
- **No standard-library randomness:** `std::uniform_int_distribution` and `std::shuffle` give different results under libstdc++, libc++ and MSVC's library. `std::sort` leaves equal elements in an unspecified order, so every sort here uses a total order.
- **Text:** numbers are written by `core::format_number` and parsed by section 04's grammar (04.C2), never by `strtod` or iostreams, which follow the locale. Apple's libc++ offers floating-point `std::to_chars` only from macOS 13.3, so section 11 sets that deployment target or bundles a formatter.
- **Files:** written in binary mode with `\n`. Lists are read through section 02's reader, which accepts LF and CRLF. Paths are `std::filesystem::path` built from UTF-8. A Windows path in a list (`C:\kgml\hsa04010.xml`) goes back into the shortlist as written. Replacing a file on Windows follows `OutputFile`'s retry rule (cross-section item 1.28).
- **No file locking:** nothing is locked. Each run's copies sit in its own step directory (S8.21). Two concurrent runs given the same absolute `shortlist` path both write it, the later rename winning, while each run keeps its own copy in `gsea/`.

### Determinism

The shortlist and the table depend only on the value file and its reading arguments, the candidate files, the selector, the keep list, the seed, the permutation count and FuSION's version. They are the same bytes on every platform and in every run (09.C16):
- ties in the ranking are ordered by id in byte order (S7.05, 08.C2);
- the generator and the shuffle are fully specified (08.C1) and seeded from `gsea.seed`;
- floating-point operations run in one fixed order (sums in position or permutation order), with no contraction and no `exp`, `log` or `pow`;
- the table is ordered by |NES|, then by id (08.C9), with untested rows in list order; the shortlist keeps list order;
- nothing depends on a thread count (08.C6), on hash-map iteration (maps are only looked up), or on the candidates' order. ES, NES and nominal p depend only on a set's own genes; FDR and FWER depend on the whole set of tested candidates, not on their order.

## Choices

### 08.C1 · Random stream: reproduce gseapy 1.3.1's permutations exactly

- **Proposed:** port gseapy 1.3.1's stream and statistics exactly, as described under Algorithms:
  - Xoshiro256++ seeded by SplitMix64, which is rand 0.9.2's `SmallRng::seed_from_u64` on 64-bit targets;
  - rand 0.9.2's Fisher–Yates with `IncreasingUniform` and Canon's bounded draw;
  - chained permutations from the identity, shared by all sets;
  - the core's formulas for NES, nominal p, FDR and FWER.

  2.0 then gives gseapy's numbers, to the last bit, whenever gseapy is given the same ranking order, sets, seed and permutation count, and Phase 3 requires that equality (Test hooks). Core holds this generator for GSEA, and 09's `core::Rng` (09.C15) serves everything else (cross-section item 1.19). This amends skeleton Choice F1's sentence that the C++ GSEA agrees with gseapy "statistically but not byte for byte"; accepted in prompt log entry 159, where 12.C10's statistical test lapses and gseapy's and rand's notices ship.
- **Alternatives:**
  - **A generator of FuSION's own,** fully specified: for example `std::mt19937_64`, whose output the C++ standard fixes, with Lemire's bounded draw and unchained shuffles. Agreement with gseapy is then statistical. ES, leading edges, `Tag %` and `Gene %` still match exactly, since they use no random numbers; NES, p, FDR and FWER match only within Monte Carlo error.
  - **A counter-based generator** (Philox) that derives permutation k from (seed, k). Permutations can then be made in parallel and recomputed one at a time. Agreement is statistical, as in the first alternative.
- **Why:**
  - **Exact agreement is achievable, and was checked.** The reference port, with this stream and the core's formulas, reproduced gseapy 1.3.1 exactly on the real inputs with ties by gene id: all 166 observed scores, all 166,000 null scores, and every NES, nominal p, FDR q and FWER p, compared as doubles. The verifier's own port agrees. The selections at fdr:0.25, 0.4, 0.44 and 0.5 are 44, 79, 86 and 91, S7.05's measured "stable sort, ties by gene id" row.
  - **A different stream acts like a different seed.** On the same ranking, seeds 1 to 20 against seed 42 moved NES by up to 0.082, nominal p by up to 0.153 and FDR q by up to 0.373. fdr:0.4 selected 68 to 84 pathways, with up to 12 (median 6) differing from seed 42's 79; fdr:0.25 selected 39 to 45. A statistical test can only bound differences of that size, so a real porting error of the same size would pass it.
  - **Every changed number has one decided cause.** With exact agreement, every number that differs from FuSION 1's tables comes from S7.05's tie order; the other expected differences are decided changes of form (Test hooks). Seed 42 means what it meant in FuSION 1.
  - **Cost:** about 100 lines of integer code (SplitMix64, Xoshiro256++, the bounded draw, `IncreasingUniform`) and no library. Xoshiro256++ is a well-tested generator. Neither option may use `std::uniform_int_distribution` or `std::shuffle`, whose output differs between standard libraries.
  - **Limits:**
    - The chain is sequential: 1,381,866 32-bit draws for 1000 permutations of 3054 genes, estimated at tens of ms.
    - The stream follows rand 0.9.2's internals. That is acceptable because 2.0 pins its reference to gseapy 1.3.1 (S7.02) and links no Rust.
    - The ported code carries gseapy's BSD-3-Clause and rand's MIT notices (section 11).
- **Decision:** as proposed, amending skeleton F1 (prompt log entry 159)

### 08.C2 · How gene ids compare when values tie

- **Proposed:** byte order, as `core::stable_compare` compares the ids' text: `hsa1000` < `hsa10000` < `hsa999`. This is a reading of S7.05.
- **Alternatives:** numeric-aware ("natural") order, which compares runs of digits as numbers: `hsa999` < `hsa1000` < `hsa10000`.
- **Why:**
  - S7.05 decided "by gene id, ascending" without naming the comparison. Its measured row (44/79/86/91) used the file's order, which is byte order.
  - The choice changes results. Against byte order, numeric order puts 2735 of the 3054 genes at a different rank, changes 159 of the 166 ES values and moves NES by up to 1.231. It selects 45, 75, 87 and 90 pathways at fdr:0.25, 0.4, 0.44 and 0.5, against 44, 79, 86 and 91; at fdr:0.4, 8 pathways differ.
  - Byte order needs no rules for other id forms (symbols such as `CXCL8`, Ensembl ids; decision 0020) and no locale, and it is the one comparison core provides (cross-section item 2A.5). Numeric order reads more naturally for Entrez-based ids, but needs rules for leading zeros and for ids with several digit runs.
- **Decision:** as proposed (prompt log entry 181)

### 08.C3 · Gene sets for other pathway sources

- **Proposed:** a candidate is one network file in any format section 02 reads (02.C1). Its gene set is every gene id the reader finds in that file: for KGML every `type="gene"` entry (S7.08), for an edge table every gene endpoint. The ids are in the run's id space, after section 02's id mapping. GSEA reads gene sets from nowhere else.
- **Alternatives:**
  - Also accept a GMT file of named gene sets (MSigDB or Reactome exports), each name paired with a network file to merge.
  - KGML only, as in FuSION 1, with other sources later.
- **Why:**
  - S7.08 kept file-derived sets because "every tested set is exactly one file that mff can merge". That holds for every format section 02 reads, with no new input.
  - A GMT file is a second input that must stay in step with the network files. The script's docstring says MSigDB's KEGG sets match only 90 of the 167 KGMLs (not re-measured, S7.08), so mismatches are real.
  - The keep list (S7.14) and the relation share (08.C4) need the file's own genes and relations anyway.
  - For an edge table, every gene sits on a relation, so its relation share is always complete.
- **Decision:** as proposed (prompt log entry 181)

### 08.C4 · Relation share: which relations count

- **Proposed:** a gene counts as on a relation when it is an endpoint, or a component of a group endpoint, of a relation that the merge keeps under the run's settings: `network.indirect` (S2.11, indirect-effect relations discarded by default) and `network.ecrel` (S2.31, ECrel relations kept by default), as section 02's `kept_relations` filters them. This is a reading of S7.08.
- **Alternatives:** count relations of every type in the file, as S7.08 measured (3301 of 21,205 memberships on no relation).
- **Why:**
  - The column exists because GSEA scores membership while `mff` takes relations (S7.08). A relation the merge drops connects nothing. Both readings fit S7.08's words, "genes that sit on its relations".
  - The counts differ. Of the 21,205 memberships, 17,904 are on some relation and 17,396 on a relation the default merge keeps. The share changes for 76 of the 167 candidates, 21 of them among the 42 that FuSION 1 shortlisted at fdr:0.25.
  - Examples: Chagas disease (hsa05142) has 102 of its 103 genes on some relation, but 80 on kept ones; Amoebiasis (hsa05146) has 43 and 27 of 103.
  - The alternative needs nothing from section 02's settings. The proposal needs section 02's keep filter, which the merge uses anyway.
- **Decision:** as proposed (prompt log entry 181)

### 08.C5 · Relation share: form and place of the column

- **Proposed:** one column, `relation_share`, right after `genes`, written `<genes on relations>/<set size>` like `Tag %` (`80/103`, `6/205`). The set size counts all the file's genes, ranked or not, as S7.08 counted them (21 of 143 for Systemic lupus erythematosus).
- **Alternatives:**
  - A decimal share (`0.777`).
  - Two integer columns.
  - Counting only ranked genes, the ones GSEA scores.
- **Why:**
  - The a/b form keeps both counts visible, and size matters when reading a share: 2 of 3 is not 66 of 100.
  - It matches the table's existing `Tag %` style.
  - A decimal sorts more easily in a spreadsheet.
  - Nothing reads the table by position (S7.16), so a new middle column breaks no script.
- **Decision:** as proposed (prompt log entry 181)

### 08.C6 · Threads

- **Proposed:** one thread.
- **Alternatives:** split the permutations into blocks and score the blocks in parallel, each worker with its own walk state and its results written by index. The output bytes are the same for any thread count.
- **Why:**
  - The estimate for the real inputs is about 0.2 s on one thread (Cost), against 1.3 s for gseapy on 4 threads and 3.7 s on 1 (S7.19).
  - Parallel walks would save about 0.1 s on 4 threads; the chain stays sequential either way.
  - At 10,000 permutations, one thread takes an estimated 1.5 s, against gseapy's 11.6 s.
  - One thread needs no pool, no thread-count setting and no test that thread counts agree.
- **Decision:** as proposed (prompt log entry 182)

### 08.C7 · Draws whose hits all have value 0

- **Proposed:**
  - When every hit of a draw has value 0, give each hit the step 1/m, and log the warning `gsea.zero_weight_draws` with the number of such draws. This is fgsea's rule for the case, which gseapy's own port of fgsea's `calcGseaStat` also uses (gseapy core `algorithm.rs:70-80`).
  - When the FWER of a negative NES has no negative per-permutation minimum to compare with (0 of 0), write 1.0, as gseapy does for an undefined nominal p.
- **Alternatives:** copy gseapy's arithmetic. Its step 1/W becomes infinite, the walk turns into NaN at the first hit, and the score is the extreme reached before it. The FWER becomes NaN, written as an empty field.
- **Why:**
  - These cases never occur on the real inputs. 145 of the 3054 values are 0.00, so a 15-gene draw lands only on zeros with a probability of about 10⁻²⁰.
  - They can occur with other modalities (decision 0020) where most values are exactly 0: at 80 % zeros, about 3.5 % of 15-gene draws.
  - gseapy's result there is an artefact of dividing by zero. Defining it costs one branch per draw. Phase 3's exact comparison with gseapy then keeps such inputs out of its fixtures.
- **Decision:** as proposed (prompt log entry 182)

### 08.C8 · Permutation count limits

- **Proposed:** `gsea.permutations` accepts 1 to 100,000. Anything else fails in the check with section 01's generic code `batch.bad_value`. This amends entry 182's naming, which had `gsea.setting`; accepted in prompt log entry 230.
- **Alternatives:** any positive count, limited only by memory.
- **Why:**
  - The null matrix takes 8·S·P bytes: 1.3 MB at the default, 133 MB at 100,000 for 166 sets, and 1.3 GB at a million.
  - The cap turns a typo, such as an extra zero or two, into a message instead of a long run or a failed allocation.
  - 100,000 is ten times the largest count S7.11 measured; it takes an estimated 15 s on one thread.
- **Decision:** as proposed (prompt log entry 182)

### 08.C9 · Order of pathways with equal |NES|

- **Proposed:** pathway id, ascending, compared as 08.C2 compares gene ids, in the table, in the report and for `top:N`.
- **Alternatives:** the candidate list's order.
- **Why:**
  - FuSION 1 sorted with pandas' default, unstable sort (`gsea_select_pathways.py:200`, gseapy `base.py:811`), so equal |NES| had no defined order.
  - Equal values need two candidates with the same ranked genes; shared permutations then give them equal scores. None of the 167 candidates has such a twin, and the two closest |NES| on the real inputs differ by 4.2 × 10⁻⁶.
  - Id order mirrors S7.05's gene-id rule and does not change when the list is reordered.
- **Decision:** as proposed (prompt log entry 182)

### 08.C10 · The tie warning

- **Proposed:** keep the warning at warning level, through `core::Log::warning` with the code `gsea.ties` (stderr in the CLI, the run log and a `warning` event). It keeps gseapy's count and replaces its second sentence with the rule that now orders ties: `Duplicated values found in the ranking: 93.78% of genes. Tied genes are ordered by gene id.` This amends the wording that S7.17 kept; accepted in prompt log entry 183.
- **Alternatives:**
  - Drop the warning from the console, and keep the count only in `events.jsonl` (`distinct_values`).
  - Keep gseapy's text word for word, as S7.17 kept it.
- **Why:**
  - S7.17 kept the report, and gseapy's tie warning with it. S7.05 makes that warning's second sentence untrue: "The order of those genes will be arbitrary, which may produce unexpected results."
  - The count still matters. S7.05 measured that the tie order alone moved NES by up to 1.37 and the fdr:0.4 shortlist by up to 9 pathways, and 08.C2's comparison alone moves it by 8. A ranking that is 93.78 % repeats leans on the tie rule.
  - The count is gseapy's (genes whose value occurs earlier: 2864 of 3054), so the line reads as FuSION 1's did.
- **Decision:** as proposed, amending S7.17's wording (prompt log entry 183)

### 08.C11 · Names of `gsea`'s arguments

- **Proposed:** `values`, `candidates`, `selector`, `shortlist` and `keep` at positions 1 to 5 (S7.01), with `seed` and `permutations` by name only. The reading arguments `kgml_dir`, `id_column`, `value_column`, `id_map` and `repeats` keep the names sections 02 and 04 give them (cross-section item 1.23).
- **Alternatives:**
  - FuSION 1's help-line names (`Linux/src/main.cc:899`): `log_fold_change_file`, `kgml_list_file`, the selector, `out_kgml_list_file` and `keep_gene_ids`.
  - Shorter names: `lfc`, `list`, `select`, `out` and `keep`.
- **Why:**
  - `values` stays true when the values are not log-fold changes (decision 0020), and it is the name `cudf` and `suggest_thresholds` use for the value file (section 04).
  - `candidates` and `shortlist` are the terms S7.06 and S7.15 use, and `shortlist` names the file written.
  - No batch file uses names yet; FuSION 1's names appear only in its help text.
  - Section 01 owns the syntax and the command table (S1.12, 01.C9).
- **Decision:** as proposed (prompt log entry 183)

### 08.C12 · Outputs that name an input

- **Proposed:** a shortlist or table path that names the value file, the candidate list or a candidate file is refused before the run, with the code `output.clash`. 09's check finds it by file identity for every command (cross-section items 2A.7 and 1.17). This extends to `gsea` the rule S3.10 set for `cudf`.
- **Alternatives:** allow it, as FuSION 1 does.
- **Why:**
  - FuSION 1's script reads its inputs first (`gsea_select_pathways.py:178-179`) and writes its outputs last (`227-232`). So `gsea values.tsv list.txt fdr:0.25 values.tsv` replaces the value file with the shortlist, and an out path equal to the list replaces the candidate list, with no message.
  - The check costs one comparison of resolved paths per input.
- **Decision:** as proposed (prompt log entry 183)

### 08.C13 · Writing the outputs whole

- **Proposed:** write each output through 09's `Files::create` and `OutputFile`: under a temporary name in the step directory, then renamed on commit, as every module writes (cross-section item 2A.6). A failure or a stop then leaves no partial file.
- **Alternatives:** write each file in place, as FuSION 1 does.
- **Why:**
  - `mff` merges whatever a list file holds (S7.03). A truncated shortlist left by a failed or stopped `gsea` would be merged without a message by a run started from that saved step (decision 0019).
  - It costs one rename per file, and 09's retry rule handles Windows (cross-section item 1.28).
- **Decision:** as proposed (prompt log entry 158)

## Interfaces with other sections

- **04 Expression:**
  - Needs `expression::GeneValues` from `ValueInputs::at`: one entry per gene in the network's id space, with its representative (04.C3), built by the check's single read of the input (04.C9).
  - The `values` argument has the reading arguments of `cudf` (1.23), and the selector's values follow 04.C2's decimal grammar.
- **02 Network:**
  - Needs `read_input_list` with `kgml_dir` (02.C12), returning each line's text with its resolved path; a KGML without a root `name` takes its file stem as id (2B.12). The reader fails on `list.duplicate_pathway` (1.24).
  - Needs the documents the check parsed (02.C15), with every entry, including those no relation reaches (S7.08), groups with their components, relations with their subtypes, and the root's `title` (entry 205).
  - Needs `kept_relations` under the run's `RelationFilters` (08.C4).
  - Needs id mapping for keep ids and value ids (S2.14, S7.14).
- **03 Merge:** `mff` merges the `Shortlist`'s resolved paths in memory, and a saved shortlist through its recorded base (2B.10). No parse is shared (S7.19, 4A.5).
- **09 Runs and core:**
  - Supplies the vocabulary this outline uses: `Failure`, `Stopped`, `Report`, `Outcome<T>`, `Log`, `Events`, `Context`, `Files`, `OutputFile`, `Interner`, `GeneId`, `stable_compare` and `format_number` (1.1, 2A.5, 1.20).
  - Supplies the two registry keys (1.16), the event envelope (1.3), the clash check (2A.7), and the output-path rule (2B.3).
  - Records the step's base in `run.json` (2B.10).
  - Holds this section's generator for GSEA beside `core::Rng` (1.19).
- **01 Batch:**
  - Holds `gsea`'s command-table entry, handler and checker (01.C9): the handler reads the inputs that the check prepared, builds `Options` and calls `enrichment::gsea`; the checker calls `parse_selector` and `check_settings`.
  - Keeps the `Shortlist` in the workspace (2B.14) and skips the point's later lines after `gsea.empty_selection` (01.C15).
- **10 Frontend:**
  - A "shortlist pathways" block with these arguments, built from `fusion --commands` (S8.27, 09.C12).
  - Validation through `fusion --check --json`, so the selector grammar has one implementation (decision 0021, S8.30).
  - S7.18's review reads the table and the `gsea.selected` event. An empty selection shows as a reported result, not as an error.
- **11 Build:** contraction off on every platform (11.C7), 64-bit targets only, the macOS 13.3 deployment target or a bundled formatter, and gseapy's BSD-3-Clause and rand's MIT notices (entry 159).
- **12 Tests:** the hooks below, with a pinned gseapy 1.3.1 in a test-only environment for the exact comparison; 12.C10's statistical test lapses (entry 159).

## Test hooks

**Library level (Catch2):**
- **Generator:** `Xoshiro256pp(0)` starts 5987356902031041503, 7051070477665621255, 6633766593972829180, the values in rand 0.9.2's own test `stable_seed_from_u64_and_from_seed`. The first shuffle of 0 to 3053 with seed 42 starts 2614, 515, 2774, 958, 1504, 1071, 1924, 2135, 1637, 2881 (the reference port, which matches gseapy). It makes 1228 bounded draws and 1378 32-bit draws. A fixture holds the first three permutations in full.
- **Walk and statistics:** small hand-checked cases, including e < 0 with a null score equal to e (`≤` counts it), a set with no null score of one sign, and 08.C7's cases.
- **Ranking:** ties ordered `hsa1000`, `hsa10000`, `hsa999` (08.C2). Shuffling the value file's lines leaves the table unchanged.
- **Selector:** each case S7.13 measured fails with `gsea.selector`, naming its term: `FDR:0.25`, `fdr:`, `fdr:0.25,`, `top:2.5`, the empty selector, `top:-5`, `top:0`, `fdr:nan`, `fdr:inf`, `nes:-1` and `fdr:0_5`; so does the out-of-range `fdr:1.5`. `top:10,fdr:0.06` and `fdr:0.06,top:10` give the same selection.
- **Candidates:** two files with one pathway id fail with `list.duplicate_pathway`, naming both; one file listed twice is read once, with a warning; a missing path stops the run in the check; a CRLF list reads like an LF list; a KGML without a root `name` takes its file stem.
- **Outputs:**
  - every candidate has a row; untested rows have empty scores; booleans are `True` and `False`; numbers read back to the same doubles;
  - the empty selection writes a 0-byte shortlist and the table, and returns the Report `gsea.empty_selection`;
  - "no candidate has 15 to 500 ranked genes" fails with `gsea.no_testable`;
  - an out path equal to an input fails in the check with `output.clash` (08.C12);
  - a failure injected while writing leaves no shortlist (08.C13);
  - an absolute `shortlist` path gets the file there and in `gsea/` (2B.3).
- **Keep list:** an id in no candidate gives `gsea.keep_unmatched`. A kept, untested candidate gets a row with `kept_for` (Sulfur relay system, kept for `hsa4357`).

**Against gseapy 1.3.1** (08.C1):
- The harness runs gseapy's `prerank(rnk=<Series ordered by value descending, id ascending>, gene_sets=<the same sets>, permutation_num=P, min_size=15, max_size=500, weight=1.0, seed=S, ascending=None, outdir=None)`. With `ascending=None`, gseapy keeps the given order (gseapy `gsea.py:493`). Checked in Phase 2: this gives the same results as calling the core directly.
- Pass: ES, NES, nominal p, FDR q and FWER p are equal as doubles, and `Tag %`, `Gene %` and `Lead_genes` are equal as text, for every set. The reference port met this on the real inputs: all 166 rows, every column.
- Fixtures:
  - synthetic value files with heavy ties (two decimals, like the EGF file) and random sets, committed;
  - the real inputs, read from the local KGML directories, not committed (S8.31).
- Fixture ids avoid `__` (gseapy's library split) and the mixed-case pattern that makes gseapy upper-case genes (gseapy `base.py:379-399`, `460-480`). 2.0 never changes case.

**Against FuSION 1's real tables:** the expected differences recorded under decision 0023 are:
- **The tie order** (S7.05, 08.C2), the only one that changes numbers. fdr:0.25, 0.4, 0.44 and 0.5 select 44, 79, 86 and 91 pathways instead of 42, 71, 87 and 90. At fdr:0.25, Renal cell carcinoma (hsa05211) and Thyroid cancer (hsa05216) join, and none leaves. With the keep run's keep list at fdr:0.4, 79 + 11 = 90 pathways are chosen instead of 71 + 16 = 87.
- **The `relation_share` column** (S7.08; 08.C4, 08.C5).
- **A row for every untested candidate** (S7.09): Sulfur relay system appears.
- **`genes` counts only finite values** (S7.04, S7.16). This has no effect on the real inputs, which have none that are not finite.
- **The console:** the tie warning's wording (08.C10).

**Determinism:** the same table bytes on Linux, macOS (Apple silicon and x86-64) and Windows, on real and synthetic inputs, as 12.C11's one golden set requires (09.C16).

**Performance** (section 12's benchmark):
- The real inputs on one thread, with the check's KGML parse: at most 0.5 s (FuSION 1: 3.3 s wall and 318 MB).
- 10,000 permutations: at most 3 s.
- Peak memory: at most 50 MB.

**End to end:**
- A batch that runs `gsea` and then `mff` on synthetic KGMLs, with `mff` merging the in-memory shortlist.
- A selector sweep in which one point selects nothing: that point's later lines are skipped, and the sweep carries on (S7.03, 01.C15).
- A run started from a saved shortlist with relative lines resolves them through the recorded base (2B.10).

## Open questions

Every open question of this section is settled:
1. **F1's text.** Settled by prompt log entry 159: 08.C1 was accepted, amending F1's sentence.
2. **Stage 7's text.** Settled by entry 183: Claude adds a correction note to S7.11's text (the nominal p for ES < 0, NES's fallback mean, and the FWER formula); its decision stands.
3. **Console precision.** Settled by entry 168 (cross-section item 4A.7): copyable numbers are exact, and the GSEA statistics table keeps three decimals as a recorded exception.
4. **Repeated ids without thresholds.** Settled by entry 167 (04.C3): GSEA ranks the representative, the value with the largest magnitude, the earlier line winning a tie.
5. **Number text in TSV files.** Settled by entry 180 (cross-section item 1.20): Python's `repr` layout.
6. **Relative paths in lists.** Settled by entry 158 (cross-section item 2B.10): the base is recorded in `run.json`.
7. **"log-fold change" in the report.** Settled by entry 183: the line stays as S7.17 decided.
8. **Output arguments outside the run directory.** Settled by entry 169 (cross-section items 2B.3 and 4A.11): the step directory always, and an absolute path as well.
9. **08.C10 and S7.17.** Settled by entry 183: 08.C10 amends S7.17's wording.
