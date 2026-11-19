// Private to the expression module: the state behind GeneValues and Observations, and the helpers
// its translation units share (section 04).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fusion/expression/expression.hpp"

namespace fusion::expression {

namespace detail {
enum class Layout : std::uint8_t { pairs, table };                       // 04.C1
enum class Separator : std::uint8_t { whitespace, tab, comma, spaces };  // whitespace: the pairs layout
}  // namespace detail

struct GeneValues::Impl {
    ValueInput input;
    ValueReport report;
    std::filesystem::path file;                     // the bytes read: 09's copy, or the original
    std::string path;                               // the file as outputs name it ("inputs/x.tsv")
    detail::Layout layout = detail::Layout::pairs;
    detail::Separator separator = detail::Separator::whitespace;
    std::optional<std::string> id_column;           // the id column read (table layout)
    std::vector<core::GeneId> ids;                  // per gene, first-occurrence order
    std::vector<std::string> names;                 // per gene: the id's text after the id rule
    std::vector<double> representative;             // per gene: the largest magnitude (04.C3)
    std::vector<std::uint32_t> first;               // per gene, plus one: bounds into the line arrays
    std::vector<double> line_values;                // grouped by gene, file order within a gene
    std::vector<std::uint32_t> line_numbers;
    std::unordered_map<core::GeneId, std::uint32_t, core::HandleHash> index;  // lookups only (09)
};

struct Observations::Impl {
    std::vector<core::GeneId> observed;             // up, then down; each id once, input order
    std::vector<std::string> names;                 // the ids' text, so a network with its own
                                                    // interner can still be matched (S3.12)
    std::size_t n_up = 0;
    std::optional<Thresholds> thresholds;           // none when loaded from lists
    std::unordered_map<core::GeneId, Regulation, core::HandleHash> regulation;  // lookups only
    std::optional<std::filesystem::path> not_de_file;   // Not DE (04.C4), as named
    std::vector<core::GeneId> not_de_removed, not_de_unchanged, not_de_unknown;
};

namespace detail {

std::string_view name(SkipReason) noexcept;
std::string_view name(Regulation) noexcept;
std::string_view name(MatchGroup) noexcept;

// Text that reaches JSON or the console: bytes that are not valid UTF-8 become U+FFFD (04 "Bytes").
std::string utf8_text(std::string_view);
// At most `limit` bytes, cut back to a whole UTF-8 sequence (the 60-byte excerpt of a skipped line).
std::string excerpt(std::string_view line, std::size_t limit);
// A TSV cell: tabs, CRs and LFs become spaces, so a row stays one row.
std::string tsv_cell(std::string_view);
std::string integer(std::uint64_t);                 // locale-free decimal digits
std::string join_numbers(std::span<const std::uint32_t>, std::string_view separator);
std::string count(std::uint64_t n, std::string_view one, std::string_view many);   // "1 gene", "2 genes"
// A line excerpt for the console: in double quotes, control characters escaped ("gene\tlfc").
std::string quoted(std::string_view);
core::Where where(std::string file, std::uint32_t line = 0);
// A file's bytes, read whole in binary mode; input.missing, input.not_file or input.unreadable (0021).
std::string read_bytes(const std::filesystem::path&);

// The command name a console line starts with: the step ("cudf", "cudf-2"), else the command.
std::string console_prefix(const core::Context&, std::string_view command);
// A path as outputs name it: relative to the run directory when inside it, else as given.
std::string run_relative(const std::filesystem::path&, const core::Context&);

core::Json values_json(const GeneValues&);          // the "values" object (04 "Files written")
core::Json read_json(const GeneValues&);            // the "read" object
core::Json thresholds_json(const std::optional<Thresholds>&);
core::Json not_de_json(const Observations&, const core::Interner&);
core::Json suggestion_fields(const Suggestion&);   // distribution, rows, histogram

// debug/<step>/values.tsv under the debug option only (S1.29): the values as read.
void save_debug_values(const GeneValues&, core::Context&);
// Writes text through a new OutputFile in the step directory and commits it.
void write_file(core::Context&, std::string_view name, std::string_view text);
std::string json_text(const core::Json&);           // indented, with a final LF

}  // namespace detail
}  // namespace fusion::expression
