// fusion expression: the one reader of per-gene values, used by every command that reads them
// (section 04, "The value reader"; S3.03 to S3.05, S3.08, S3.14; 04.C1 to 04.C3, 04.C9, 04.C10).
// FuSION 1 had five parsers with five rule sets: cudf's (Linux/src/main.cc:2677-2689),
// suggest_thresholds.py:67-81, gsea_select_pathways.py:86-96, plot_explanation.py:132-143 and the
// GUI's preview (S3.14). This file is the one that replaces them all.
#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_set>
#include <utility>

#include "internal.hpp"

namespace fusion::expression {

// ---------------------------------------------------------------- GeneValues

GeneValues::GeneValues(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
const GeneValues::Impl& GeneValues::impl() const noexcept { return *impl_; }
std::size_t GeneValues::size() const noexcept { return impl_->ids.size(); }
core::GeneId GeneValues::id(std::size_t i) const noexcept { return impl_->ids[i]; }
double GeneValues::value(std::size_t i) const noexcept { return impl_->representative[i]; }

std::span<const double> GeneValues::line_values(std::size_t i) const noexcept {
    const std::uint32_t begin = impl_->first[i];
    return std::span<const double>(impl_->line_values).subspan(begin, impl_->first[i + 1] - begin);
}

std::span<const std::uint32_t> GeneValues::line_numbers(std::size_t i) const noexcept {
    const std::uint32_t begin = impl_->first[i];
    return std::span<const std::uint32_t>(impl_->line_numbers).subspan(begin, impl_->first[i + 1] - begin);
}

std::optional<std::size_t> GeneValues::find(core::GeneId id) const noexcept {
    const auto at = impl_->index.find(id);
    if (at == impl_->index.end()) return std::nullopt;
    return at->second;
}

const ValueInput& GeneValues::input() const noexcept { return impl_->input; }
const ValueReport& GeneValues::report() const noexcept { return impl_->report; }

namespace {

using detail::Layout;
using detail::Separator;

constexpr std::size_t excerpt_bytes = 60;   // a skipped line is reported with its first 60 bytes
constexpr std::size_t console_names = 10;   // the console names at most ten lines or ids

bool is_blank(char c) noexcept { return c == ' ' || c == '\t'; }

bool blank_line(std::string_view line) noexcept { return std::all_of(line.begin(), line.end(), is_blank); }

// Lines end at LF, CRLF or a lone CR (S3.03, 04.C10); the last one needs no terminator. FuSION 1's
// getline (main.cc:2677) ended lines at LF only, so a classic Mac file read as one line.
class LineReader {
public:
    explicit LineReader(std::string_view text) : text_(text) {}
    bool next(std::string_view& line) {
        if (pos_ >= text_.size()) return false;
        std::size_t end = pos_;
        while (end < text_.size() && text_[end] != '\n' && text_[end] != '\r') ++end;
        line = text_.substr(pos_, end - pos_);
        if (end < text_.size()) {
            if (text_[end] == '\r' && end + 1 < text_.size() && text_[end + 1] == '\n') ++end;
            ++end;
        }
        pos_ = end;
        return true;
    }

private:
    std::string_view text_;
    std::size_t pos_ = 0;
};

// The pairs layout (S3.03): fields are runs of spaces and tabs apart, the id first and the value
// second, later fields ignored; FuSION 1 read `stringstream(s) >> gene_name >> fold_change`
// (main.cc:2679). Each line gets fresh fields, so S3.05's carry-over cannot happen.
struct Pair {
    std::string_view id, value;
};

Pair split_pair(std::string_view line) {
    std::size_t i = 0;
    const auto token = [&]() {
        while (i < line.size() && is_blank(line[i])) ++i;
        const std::size_t begin = i;
        while (i < line.size() && !is_blank(line[i])) ++i;
        return line.substr(begin, i - begin);
    };
    Pair pair;
    pair.id = token();
    pair.value = token();
    return pair;
}

// The table layout (04.C1): one separator for the file, chosen from its header. With tabs or commas
// a field may be in double quotes, "" inside quotes is one quote, and quotes never span lines;
// spaces around an unquoted field are trimmed. Returns false when a quote is left open: that field
// then runs to the end of the line.
bool split_table(std::string_view line, Separator separator, std::vector<std::string>& fields) {
    fields.clear();
    if (separator == Separator::spaces) {
        std::size_t i = 0;
        while (true) {
            while (i < line.size() && is_blank(line[i])) ++i;
            if (i >= line.size()) return true;
            const std::size_t begin = i;
            while (i < line.size() && !is_blank(line[i])) ++i;
            fields.emplace_back(line.substr(begin, i - begin));
        }
    }
    const char split = separator == Separator::tab ? '\t' : ',';
    const auto pad = [split](char c) { return c == ' ' || (c == '\t' && split != '\t'); };
    const auto trim = [&](std::string_view text) {
        while (!text.empty() && pad(text.front())) text.remove_prefix(1);
        while (!text.empty() && pad(text.back())) text.remove_suffix(1);
        return text;
    };
    std::size_t start = 0;
    while (true) {
        std::size_t i = start;
        while (i < line.size() && pad(line[i])) ++i;
        std::string field;
        std::size_t end = 0;
        if (i < line.size() && line[i] == '"') {
            ++i;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        i += 2;
                        continue;
                    }
                    closed = true;
                    ++i;
                    break;
                }
                field += line[i++];
            }
            if (!closed) {
                fields.push_back(std::move(field));
                return false;
            }
            end = std::min(line.find(split, i), line.size());
            field += trim(line.substr(i, end - i));
        } else {
            end = std::min(line.find(split, start), line.size());
            field = trim(line.substr(start, end - start));
        }
        fields.push_back(std::move(field));
        if (end >= line.size()) return true;
        start = end + 1;
    }
}

Separator detect_separator(std::string_view header) {
    if (header.find('\t') != std::string_view::npos) return Separator::tab;
    if (header.find(',') != std::string_view::npos) return Separator::comma;
    return Separator::spaces;
}

std::size_t find_column(const std::vector<std::string>& header, const std::string& name, const std::string& path,
                        std::uint32_t line) {
    std::size_t found = header.size();
    std::size_t times = 0;
    for (std::size_t i = 0; i < header.size(); ++i) {
        if (header[i] != name) continue;
        if (times == 0) found = i;
        ++times;
    }
    if (times == 0)
        throw core::Failure("values.column",
                            "the header of " + path + " (line " + detail::integer(line) + ") has no column " +
                                detail::quoted(name),
                            detail::where(path, line));
    if (times > 1)
        throw core::Failure("values.column",
                            "the header of " + path + " (line " + detail::integer(line) + ") has the column " +
                                detail::quoted(name) + " " + detail::integer(times) + " times",
                            detail::where(path, line));
    return found;
}

struct Number {
    bool ok = false;
    SkipReason reason = SkipReason::not_a_number;
    double value = 0;
};

// Rules 5 and 6 of the line rules (04.C2): the whole token must match the decimal grammar, which
// core::parse_decimal applies, converting with fast_float (11.C5). A magnitude past a double is
// not_finite, and one below the smallest normal double reads as 0. FuSION 1 read the longest numeric
// prefix as a float and stored 0 for a token with none (S3.04, main.cc:2679), so a header line, NA or
// nan counted as 0, 1,5 as 1 and 1.5abc as 1.5.
Number read_number(std::string_view token) {
    const core::Decimal decimal = core::parse_decimal(token);
    Number number;
    if (decimal.status == core::DecimalStatus::not_a_number) return number;
    if (decimal.status == core::DecimalStatus::not_finite) {
        number.reason = SkipReason::not_finite;
        return number;
    }
    number.ok = true;
    number.value = decimal.value;
    return number;
}

std::string skipped_summary(const ValueReport& report) {
    std::string text;
    for (std::size_t i = 0; i < report.skipped.size() && i < console_names; ++i) {
        const SkippedLine& line = report.skipped[i];
        if (i > 0) text += ", ";
        text += "line " + detail::integer(line.line) + " (" + std::string(detail::name(line.reason)) + ") " +
                detail::quoted(line.text);
    }
    if (report.skipped.size() > console_names) text += ", and " + detail::integer(report.skipped.size() - console_names) + " more";
    return text;
}

// The layout of section 04's example: "hsa57018 (lines 10, 11), hsa3280 (34, 35)".
std::string repeated_summary(const GeneValues::Impl& impl) {
    const ValueReport& report = impl.report;
    std::string text;
    for (std::size_t i = 0; i < report.repeated.size() && i < console_names; ++i) {
        const RepeatedId& id = report.repeated[i];
        if (i > 0) text += ", ";
        text += detail::utf8_text(impl.names[impl.index.at(id.id)]) + " (" + (i == 0 ? "lines " : "") +
                detail::join_numbers(id.lines, ", ") + ")";
    }
    if (report.repeated.size() > console_names) text += ", and " + detail::integer(report.repeated.size() - console_names) + " more";
    return text;
}

std::string unmapped_summary(const ValueReport& report) {
    std::string text;
    for (std::size_t i = 0; i < report.unmapped_ids.size() && i < console_names; ++i) {
        if (i > 0) text += ", ";
        text += detail::utf8_text(report.unmapped_ids[i]);
    }
    if (report.unmapped_ids.size() > console_names) text += ", and " + detail::integer(report.unmapped_ids.size() - console_names) + " more";
    return text;
}

std::string map_name(const ValueInput& input) { return input.id_map ? detail::utf8_text(*input.id_map) : "the run's id map"; }

GeneValues parse(std::string_view bytes, const ValueInput& input, const network::IdMap& map, bool own_map,
                 std::filesystem::path file, std::string path, core::Interner& interner, core::Context& ctx) {
    auto impl = std::make_shared<GeneValues::Impl>();
    impl->input = input;
    impl->file = std::move(file);
    impl->path = detail::utf8_text(path);
    ValueReport& report = impl->report;
    // Bytes (04.C10): the hash is taken from exactly the bytes parsed.
    report.bytes = bytes.size();
    report.sha256 = core::Sha256::of(bytes);
    const std::string& shown = impl->path;
    if (bytes.starts_with("\xFE\xFF") || bytes.starts_with("\xFF\xFE"))
        throw core::Failure("values.encoding",
                            shown + " is UTF-16 text (byte-order mark " + (bytes[0] == '\xFE' ? "FE FF" : "FF FE") +
                                "); save it as UTF-8",
                            detail::where(shown));
    if (const std::size_t nul = bytes.find('\0'); nul != std::string_view::npos)
        throw core::Failure("values.encoding",
                            shown + " is not a text file (NUL byte at offset " + detail::integer(nul) +
                                "); save it as CSV or TSV",
                            detail::where(shown));
    std::string_view text = bytes;
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);   // a UTF-8 byte-order mark is dropped

    const bool table = input.value_column.has_value();
    if (!table && input.id_column)
        throw core::Failure("values.column",
                            "id_column=" + detail::utf8_text(*input.id_column) + " needs value_column=, which reads " +
                                shown + " as a table",
                            detail::where(shown));
    impl->layout = table ? Layout::table : Layout::pairs;

    // Each parsed line, before grouping by gene.
    std::vector<std::uint32_t> gene_of_line, numbers, per_gene;
    std::vector<double> values;
    std::unordered_set<std::string> unmapped;   // lookups only
    std::vector<std::string> fields;
    std::size_t id_index = 0, value_index = 0;
    bool header_seen = !table;

    LineReader lines(text);
    std::string_view line;
    std::uint32_t number = 0;
    while (lines.next(line)) {
        ++number;
        // Rule 1: a blank line is skipped silently (S3.05).
        if (blank_line(line)) {
            ++report.blank;
            continue;
        }
        if (!header_seen) {   // the table layout: the first non-blank line is the header (04.C1)
            header_seen = true;
            ++report.header;
            impl->separator = detect_separator(line);
            split_table(line, impl->separator, fields);
            value_index = find_column(fields, *input.value_column, shown, number);
            if (input.id_column) id_index = find_column(fields, *input.id_column, shown, number);
            impl->id_column = fields.empty() ? std::string() : fields[id_index];
            continue;
        }
        std::string_view id_text, value_text;
        bool quotes_closed = true;
        if (table) {
            quotes_closed = split_table(line, impl->separator, fields);
            if (id_index < fields.size()) id_text = fields[id_index];
            if (value_index < fields.size()) value_text = fields[value_index];
        } else {
            const Pair pair = split_pair(line);
            id_text = pair.id;
            value_text = pair.value;
        }
        const auto skip = [&](SkipReason reason) {
            report.skipped.push_back(SkippedLine{number, reason, detail::excerpt(line, excerpt_bytes)});
        };
        // Rules 2 to 6, in this order.
        if (id_text.empty()) {
            skip(SkipReason::missing_id);
            continue;
        }
        if (value_text.empty()) {   // S3.05: FuSION 1 gave such an id the previous line's value
            skip(SkipReason::missing_value);
            continue;
        }
        if (!quotes_closed) {
            skip(SkipReason::bad_quote);
            continue;
        }
        const Number value = read_number(value_text);
        if (!value.ok) {
            skip(value.reason);
            continue;
        }
        ++report.parsed;
        // The id rule (02.C3): every ':' deleted, then one map, once. A value input's own map drops the
        // ids it lacks; the run's map passes them through (prompt log entry 229). An id the map sends to
        // nothing is dropped as well. FuSION 1 kept ids verbatim (S3.03), so hsa:3725 matched nothing.
        std::optional<core::GeneId> id;
        if (own_map) {
            id = map.lookup(id_text, interner);
        } else {
            id = map.canonical(id_text, interner);
        }
        if (id && interner.str(*id).empty()) id.reset();
        if (!id) {
            ++report.unmapped_lines;
            if (unmapped.emplace(id_text).second) report.unmapped_ids.emplace_back(id_text);
            continue;
        }
        // Repeated ids (04.C3, S3.08): kept once, at the first line's position, with every line.
        const auto [at, fresh] = impl->index.try_emplace(*id, static_cast<std::uint32_t>(impl->ids.size()));
        if (fresh) {
            impl->ids.push_back(*id);
            impl->names.emplace_back(interner.str(*id));
            per_gene.push_back(0);
        }
        ++per_gene[at->second];
        gene_of_line.push_back(at->second);
        values.push_back(value.value);
        numbers.push_back(number);
    }
    report.lines = number;

    // Group the lines by gene, in file order within each gene.
    const std::size_t genes = impl->ids.size();
    impl->first.assign(genes + 1, 0);
    for (std::size_t g = 0; g < genes; ++g) impl->first[g + 1] = impl->first[g] + per_gene[g];
    impl->line_values.resize(values.size());
    impl->line_numbers.resize(values.size());
    std::vector<std::uint32_t> next(impl->first.begin(), impl->first.end() - 1);
    for (std::size_t k = 0; k < values.size(); ++k) {
        const std::uint32_t slot = next[gene_of_line[k]]++;
        impl->line_values[slot] = values[k];
        impl->line_numbers[slot] = numbers[k];
    }
    // The representative (04.C3): the largest magnitude, the earlier line winning a tie.
    impl->representative.resize(genes);
    for (std::size_t g = 0; g < genes; ++g) {
        const std::uint32_t begin = impl->first[g], end = impl->first[g + 1];
        double best = impl->line_values[begin];
        for (std::uint32_t k = begin + 1; k < end; ++k)
            if (std::abs(impl->line_values[k]) > std::abs(best)) best = impl->line_values[k];
        impl->representative[g] = best;
        if (end - begin > 1)
            report.repeated.push_back(RepeatedId{
                impl->ids[g],
                std::vector<std::uint32_t>(impl->line_numbers.begin() + begin, impl->line_numbers.begin() + end),
                std::vector<double>(impl->line_values.begin() + begin, impl->line_values.begin() + end)});
    }

    // The report (S3.04, S3.05, S3.08): warnings, which never stop anything.
    if (!report.skipped.empty())
        ctx.log.warning("values.skipped_lines", detail::count(report.skipped.size(), "line", "lines") + " of " + shown +
                                                    " skipped: " + skipped_summary(report));
    if (!report.repeated.empty())
        ctx.log.warning("values.repeated_ids", detail::count(report.repeated.size(), "repeated id", "repeated ids") +
                                                   ", each kept once: " + repeated_summary(*impl));
    if (report.unmapped_lines > 0)
        ctx.log.warning("values.unmapped_ids", detail::count(report.unmapped_lines, "line", "lines") + " of " + shown +
                                                   " dropped: " +
                                                   detail::count(report.unmapped_ids.size(), "id is", "ids are") +
                                                   " not in " + map_name(input) + ": " + unmapped_summary(report));
    if (genes == 0) {
        std::string message;
        if (report.parsed > 0)
            message = "no id of " + shown + " is in " + map_name(input) + " (" +
                      detail::count(report.parsed, "line", "lines") + " with a number)";
        else if (table)
            message = "no line of " + shown + " holds an id in column " + detail::quoted(impl->id_column.value_or("")) +
                      " and a number in column " + detail::quoted(*input.value_column);
        else
            message = "no line of " + shown + " holds an id and a number; is it a table (value_column=...)?";
        throw core::Failure("values.empty", message, detail::where(shown));
    }

    core::Json event = core::Json::object();
    event["path"] = shown;
    event["layout"] = table ? "table" : "pairs";
    event["lines"] = report.lines;
    event["genes"] = genes;
    event["n_skipped"] = report.skipped.size();
    event["n_repeated"] = report.repeated.size();
    event["n_unmapped"] = report.unmapped_lines;
    ctx.events.emit("expression.read", std::move(event));
    return GeneValues(std::move(impl));
}

}  // namespace

GeneValues read_values(const std::filesystem::path& file, const ValueInput& input, const network::Maps& run_maps,
                       core::Interner& interner, core::Context& ctx) {
    const std::string bytes = detail::read_bytes(file);   // whole, in binary mode
    std::string path = detail::run_relative(file, ctx);
    if (!input.id_map) return parse(bytes, input, run_maps.ids, false, file, std::move(path), interner, ctx);
    // A value input's own map replaces the run's (1.12): the symbol map read backwards, or an S1.16
    // file, read from 09's checked copy.
    const network::IdMap own = *input.id_map == "symbols"
                                   ? network::IdMap::reverse_symbols(run_maps.symbols, ctx)
                                   : network::IdMap::read(ctx.files.input(core::path_from_utf8(*input.id_map)), ctx);
    return parse(bytes, input, own, true, file, std::move(path), interner, ctx);
}

GeneValues parse_values(std::string_view bytes, const ValueInput& input, const network::IdMap& map, bool own_map,
                        core::Interner& interner, core::Context& ctx) {
    return parse(bytes, input, map, own_map, input.path, core::to_utf8(input.path), interner, ctx);
}

// ---------------------------------------------------------------- ValueInputs (04.C9)

struct ValueInputs::Impl {
    std::deque<std::pair<ValueInput, GeneValues>> read;   // a deque keeps references valid
};

ValueInputs::ValueInputs() : impl_(std::make_unique<Impl>()) {}
ValueInputs::~ValueInputs() = default;
ValueInputs::ValueInputs(ValueInputs&&) noexcept = default;
ValueInputs& ValueInputs::operator=(ValueInputs&&) noexcept = default;

namespace {

// The reading arguments in which two inputs of one file differ, for values.read_two_ways.
std::string differences(const ValueInput& a, const ValueInput& b) {
    const auto shown = [](const std::optional<std::string>& value) {
        return value ? detail::quoted(*value) : std::string("none");
    };
    const auto rule = [](RepeatRule repeats) {
        return std::string(repeats == RepeatRule::max_abs ? "max_abs" : "classify_each");
    };
    std::vector<std::string> parts;
    if (a.id_column != b.id_column) parts.push_back("id_column " + shown(a.id_column) + " and " + shown(b.id_column));
    if (a.value_column != b.value_column)
        parts.push_back("value_column " + shown(a.value_column) + " and " + shown(b.value_column));
    if (a.id_map != b.id_map) parts.push_back("id_map " + shown(a.id_map) + " and " + shown(b.id_map));
    if (a.repeats != b.repeats) parts.push_back("repeats " + rule(a.repeats) + " and " + rule(b.repeats));
    std::string text;
    for (std::size_t i = 0; i < parts.size(); ++i) text += (i > 0 ? ", " : "") + parts[i];
    return text;
}

}  // namespace

const GeneValues& ValueInputs::read(const ValueInput& input, const std::filesystem::path& file,
                                    const network::Maps& maps, core::Interner& interner, core::Context& ctx) {
    for (const auto& [known, values] : impl_->read)
        if (known == input) return values;   // read once per run (04.C9, 1.13)
    for (const auto& [known, values] : impl_->read) {
        if (known.path != input.path) continue;
        // One file read with different arguments is two inputs: allowed, and named (04.C9).
        ctx.log.warning("values.read_two_ways", core::to_utf8(input.path) + " is read two ways: " +
                                                    differences(known, input));
        break;
    }
    GeneValues values = read_values(file, input, maps, interner, ctx);
    impl_->read.emplace_back(input, std::move(values));
    return impl_->read.back().second;
}

const GeneValues& ValueInputs::at(const ValueInput& input) const {
    for (const auto& [known, values] : impl_->read)
        if (known == input) return values;
    throw core::Failure("internal", "the value input " + core::to_utf8(input.path) + " was not read at the check");
}

}  // namespace fusion::expression
