// fusion/expression/expression.hpp: per-gene values, classification (cudf), observations against the
// network, and threshold suggestion (section 04, "Interface"). The declarations are section 04's
// outline; private state sits behind Impl. core::parse_decimal, which 04 defines, is in core.hpp.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/network/network.hpp"

namespace fusion::expression {

// ---------- the one reader (S3.14) ----------
enum class RepeatRule : std::uint8_t { classify_each, max_abs };              // 04.C3

struct ValueInput {                                       // a file and its reading arguments (04.C1, 04.C9)
    std::filesystem::path path;                           // as named; 09 supplies the checked copy
    std::optional<std::string> id_column;                 // table layout; default: the first column
    std::optional<std::string> value_column;              // given => table layout
    std::optional<std::string> id_map;                    // an S1.16 file, or "symbols" (1.12)
    RepeatRule repeats = RepeatRule::classify_each;
    bool operator==(const ValueInput&) const = default;
};

enum class SkipReason : std::uint8_t { missing_id, missing_value, bad_quote, not_a_number, not_finite };
struct SkippedLine { std::uint32_t line = 0; SkipReason reason = SkipReason::missing_id; std::string text; };  // first 60 bytes
struct RepeatedId  { core::GeneId id; std::vector<std::uint32_t> lines; std::vector<double> values; };

struct ValueReport {                                      // the "read" object of every JSON output
    std::uint64_t bytes = 0;
    core::Sha256 sha256;                                  // of exactly the bytes parsed
    std::uint32_t lines = 0, header = 0, blank = 0, parsed = 0;
    std::vector<SkippedLine> skipped;                     // file order
    std::vector<RepeatedId> repeated;                     // first-occurrence order
    std::uint32_t unmapped_lines = 0;
    std::vector<std::string> unmapped_ids;                // distinct, first-occurrence order
};

class GeneValues {                                        // immutable, one entry per gene
public:
    std::size_t size() const noexcept;
    core::GeneId id(std::size_t i) const noexcept;        // first-occurrence order
    double value(std::size_t i) const noexcept;           // the representative (04.C3)
    std::span<const double> line_values(std::size_t i) const noexcept;         // file order
    std::span<const std::uint32_t> line_numbers(std::size_t i) const noexcept; // same order
    std::optional<std::size_t> find(core::GeneId) const noexcept;              // O(1)
    const ValueInput& input() const noexcept;
    const ValueReport& report() const noexcept;
    struct Impl;
    explicit GeneValues(std::shared_ptr<const Impl>);     // for the readers
    const Impl& impl() const noexcept;                    // added: the module's own state
private:
    std::shared_ptr<const Impl> impl_;
};

// Reads one value input: 09's checked copy in a run, the original in --check and --query.
// The map is the input's own (id_map=; ids IdMap::lookup lacks are dropped) or else the run's (02.C3).
// Throws core::Failure: values.encoding, values.empty, values.column.
GeneValues read_values(const std::filesystem::path& file, const ValueInput&,
                       const network::Maps& run_maps, core::Interner&, core::Context&);
GeneValues parse_values(std::string_view bytes, const ValueInput&, const network::IdMap& map,
                        bool own_map, core::Interner&, core::Context&);

class ValueInputs {                                       // one per run, held by batch (04.C9)
public:
    ValueInputs();
    ~ValueInputs();
    ValueInputs(ValueInputs&&) noexcept;
    ValueInputs& operator=(ValueInputs&&) noexcept;
    const GeneValues& read(const ValueInput&, const std::filesystem::path& file,
                           const network::Maps&, core::Interner&, core::Context&);  // at the check; once each
    const GeneValues& at(const ValueInput&) const;                                  // by the steps
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
};

// ---------- classification: cudf ----------
enum class Regulation : std::int8_t { down = -1, none = 0, up = 1 };

struct Thresholds { double up = 0; double down = 0; };    // up > down: the table's rule (S3.07)
Regulation regulation_of(double value, Thresholds) noexcept;              // >= up, <= down (S3.06)

class Observations {                                      // network ids, not nodes
public:
    Observations();                                       // empty
    std::span<const core::GeneId> up() const noexcept;    // each id once, input order
    std::span<const core::GeneId> down() const noexcept;
    std::span<const core::GeneId> observed() const noexcept;               // up, then down
    Regulation regulation(core::GeneId) const noexcept;   // none when not observed
    bool empty() const noexcept;
    std::optional<Thresholds> thresholds() const noexcept;                // none when loaded from lists
    std::span<const core::GeneId> not_de_removed() const noexcept;
    struct Impl;
    explicit Observations(std::shared_ptr<const Impl>);   // for classify and load_lists
    const Impl& impl() const noexcept;                    // added: the module's own state
private:
    std::shared_ptr<const Impl> impl_;
};

struct ClassifyRequest {
    Thresholds thresholds;
    std::span<const core::GeneId> not_de;                 // applied before classification (04.C4)
    // Added by the module (both optional): the run's interner names Not DE ids that are not in the
    // values in the warning not_de.unknown_ids; the Not DE file, as named, goes into cudf.json.
    const core::Interner* interner = nullptr;
    std::optional<std::filesystem::path> not_de_file = std::nullopt;
};
// Report "observations.none" when nothing passes (04.C5); throws core::Failure observations.conflict.
core::Outcome<Observations> classify(const GeneValues&, const ClassifyRequest&, core::Context&);

// ---------- the saved step (S3.09, decision 0019) ----------
struct ListNames { std::string up = "up_reg"; std::string down = "down_reg"; };  // as the batch gave them (2B.3)
void save_observations(const Observations&, const GeneValues&, const ListNames&,
                       const core::Interner&, core::Context&);   // the lists and cudf.json, through Files
                                                          // (module note: it also prints S3.11's two
                                                          // console lines; an absolute name is written
                                                          // there too, through OutputFile::also_at)
std::vector<core::GeneId> read_id_list(const std::filesystem::path&, const network::IdMap&,
                                       core::Interner&);  // S3.12's rules
Observations load_lists(const std::filesystem::path& up, const std::filesystem::path& down,
                        const network::IdMap&, core::Interner&, core::Context&);   // an id in both is up (S5.12)

// ---------- observed genes against the network and the corridor (S3.12, S4.16) ----------
enum class MatchGroup : std::uint8_t { not_in_network, outside_corridor, in_corridor };
struct ObservationMatch {
    core::GeneId id;
    Regulation regulation = Regulation::none;
    MatchGroup group = MatchGroup::not_in_network;
    std::optional<network::NodeKey> node;                 // the node the id binds to (S2.03)
    std::uint32_t members = 0;                            // that node's member ids
};
std::vector<ObservationMatch> match_observations(const Observations&, const network::GraphView& corridor);
void save_match(std::span<const ObservationMatch>, const GeneValues*, const network::LabelMap& symbols,
                const core::Interner&, core::Context&);   // fb_rch/observations_match.tsv (05.C9)
                                                          // (module note: it also prints the three
                                                          // counts and emits expression.matched)

// ---------- threshold suggestion (S3.15 to S3.19) ----------
inline constexpr double mad_to_sd = 1.4826;
inline constexpr std::uint32_t histogram_bins = 80;      // S3.19
struct Distribution { std::size_t n = 0; double min = 0, max = 0, mean = 0, sd = 0, median = 0, mad = 0; };
                                                          // population sd, raw mad
struct CandidateRow { std::string criterion; double up = 0, down = 0; std::size_t n_up = 0, n_down = 0, conflicts = 0; };
struct Histogram { double lo = 0; double width = 0; std::vector<std::uint32_t> counts; };
struct Suggestion { Distribution distribution; std::vector<CandidateRow> rows; Histogram histogram; };

Suggestion suggest_thresholds(const GeneValues&, std::optional<Thresholds> current);
void save_suggestion(const Suggestion&, const GeneValues&, core::Context&);   // thresholds.tsv, thresholds.json
std::string histogram_script(const Suggestion&, std::optional<Thresholds> current);  // for core's runner,
                                                          // which returns a result; the step raises
                                                          // expression.gnuplot_failed (S3.19)
// Added by the module: the same script with "set output '<output>'" (a single-quoted gnuplot string,
// ' doubled), the name gnuplot writes relative to its working folder, such as "thresholds.png.tmp" for
// OutputFile("thresholds.png"). The two-argument form sets no output, so it needs this line added.
// Under the debug option the handler also keeps the script as debug/suggest_thresholds/thresholds.gp.
std::string histogram_script(const Suggestion&, std::optional<Thresholds> current, std::string_view output);

// ---------- answers for fusion --query (04.C8) ----------
core::Json cudf_answer(const GeneValues&, const Observations&, const core::Interner&);   // writes nothing
core::Json suggestion_answer(const Suggestion&, const GeneValues&);

// ---------- added by the module ----------
// The settings of cudf and suggest_thresholds, keys expression.<name> (section 04 "Settings", 1.16),
// for 01's command table and 09's registry. Output names change no result.
std::span<const core::SettingSpec> settings();

}  // namespace fusion::expression
