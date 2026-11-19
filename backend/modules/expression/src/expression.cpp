// fusion expression: the helpers the module's translation units share: text for JSON, TSV and the
// console, the "values" and "read" objects every JSON output carries, and the debug values file
// (section 04, "Files written", "Portability", "Determinism").
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include "internal.hpp"

namespace fusion::expression::detail {

std::string_view name(SkipReason reason) noexcept {
    switch (reason) {
        case SkipReason::missing_id: return "missing_id";
        case SkipReason::missing_value: return "missing_value";
        case SkipReason::bad_quote: return "bad_quote";
        case SkipReason::not_a_number: return "not_a_number";
        case SkipReason::not_finite: return "not_finite";
    }
    return "unknown";
}

std::string_view name(Regulation regulation) noexcept {
    switch (regulation) {
        case Regulation::up: return "up";
        case Regulation::down: return "down";
        case Regulation::none: return "none";
    }
    return "none";
}

std::string_view name(MatchGroup group) noexcept {
    switch (group) {
        case MatchGroup::not_in_network: return "not_in_network";
        case MatchGroup::outside_corridor: return "outside_corridor";
        case MatchGroup::in_corridor: return "in_corridor";
    }
    return "not_in_network";
}

namespace {

// The length of the valid UTF-8 sequence that starts at text[i], or 0 if none does
// (no overlong forms, no surrogates, nothing above U+10FFFF).
std::size_t sequence_length(std::string_view text, std::size_t i) noexcept {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80) return 1;
    std::size_t length = 0;
    std::uint32_t code = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        code = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        code = lead & 0x0Fu;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        code = lead & 0x07u;
    } else {
        return 0;
    }
    if (i + length > text.size()) return 0;
    for (std::size_t k = 1; k < length; ++k) {
        const auto next = static_cast<unsigned char>(text[i + k]);
        if ((next & 0xC0u) != 0x80u) return 0;
        code = (code << 6) | (next & 0x3Fu);
    }
    if (length == 3 && (code < 0x800 || (code >= 0xD800 && code <= 0xDFFF))) return 0;
    if (length == 4 && (code < 0x10000 || code > 0x10FFFF)) return 0;
    return length;
}

}  // namespace

std::string utf8_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t length = sequence_length(text, i);
        if (length == 0) {
            out += "\xEF\xBF\xBD";   // U+FFFD
            ++i;
        } else {
            out.append(text.substr(i, length));
            i += length;
        }
    }
    return out;
}

std::string excerpt(std::string_view line, std::size_t limit) {
    std::size_t end = 0;
    while (end < line.size()) {
        const std::size_t length = std::max<std::size_t>(sequence_length(line, end), 1);
        if (end + length > limit) break;
        end += length;
    }
    return std::string(line.substr(0, end));
}

std::string tsv_cell(std::string_view text) {
    std::string out(text);
    std::replace_if(out.begin(), out.end(), [](char c) { return c == '\t' || c == '\n' || c == '\r'; }, ' ');
    return out;
}

std::string integer(std::uint64_t value) {
    char buffer[24];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string(buffer, result.ptr);
}

std::string join_numbers(std::span<const std::uint32_t> numbers, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        if (i > 0) out += separator;
        out += integer(numbers[i]);
    }
    return out;
}

std::string count(std::uint64_t n, std::string_view one, std::string_view many) {
    return integer(n) + " " + std::string(n == 1 ? one : many);
}

std::string quoted(std::string_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out = "\"";
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '\t') {
            out += "\\t";
        } else if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (byte < 0x20 || byte == 0x7F) {
            out += "\\x";
            out += hex[byte >> 4];
            out += hex[byte & 0x0Fu];
        } else {
            out += c;
        }
    }
    out += '"';
    return utf8_text(out);
}

core::Where where(std::string file, std::uint32_t line) {
    return core::Where{std::move(file), line, std::string()};
}

std::string read_bytes(const std::filesystem::path& file) {
    const std::string shown = core::to_utf8(file);
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(file, error);
    if (status.type() == std::filesystem::file_type::not_found)
        throw core::Failure("input.missing", "cannot read " + shown + ": no such file", where(shown));
    if (error) throw core::Failure("input.unreadable", "cannot read " + shown + ": " + error.message(), where(shown));
    if (!std::filesystem::is_regular_file(status))
        throw core::Failure("input.not_file", "cannot read " + shown + ": not a regular file", where(shown));
    std::ifstream in(file, std::ios::binary);
    if (!in) throw core::Failure("input.unreadable", "cannot read " + shown, where(shown));
    std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad()) throw core::Failure("input.unreadable", "cannot read " + shown, where(shown));
    return bytes;
}

std::string console_prefix(const core::Context& ctx, std::string_view command) {
    return std::string(ctx.step.empty() ? command : ctx.step);
}

std::string run_relative(const std::filesystem::path& file, const core::Context& ctx) {
    // A step directory sits directly in its run directory (09.C2).
    const std::filesystem::path step = ctx.files.step_dir();
    if (!step.empty() && step.has_parent_path()) {
        const std::filesystem::path relative = file.lexically_relative(step.parent_path());
        if (!relative.empty() && *relative.begin() != std::filesystem::path(".."))
            return core::to_utf8(relative);
    }
    return core::to_utf8(file);
}

core::Json values_json(const GeneValues& values) {
    const GeneValues::Impl& impl = values.impl();
    const auto optional_text = [](const std::optional<std::string>& text) {
        return text ? core::Json(utf8_text(*text)) : core::Json(nullptr);
    };
    std::string_view separator = "whitespace";
    if (impl.separator == Separator::tab) separator = "tab";
    if (impl.separator == Separator::comma) separator = "comma";
    if (impl.separator == Separator::spaces) separator = "spaces";
    core::Json json = core::Json::object();
    json["path"] = utf8_text(impl.path);
    json["bytes"] = impl.report.bytes;
    json["sha256"] = impl.report.sha256.hex();
    json["layout"] = impl.layout == Layout::pairs ? "pairs" : "table";
    json["separator"] = separator;
    json["id_column"] = optional_text(impl.id_column);
    json["value_column"] = optional_text(impl.input.value_column);
    json["id_map"] = optional_text(impl.input.id_map);
    json["repeats"] = impl.input.repeats == RepeatRule::max_abs ? "max_abs" : "classify_each";
    return json;
}

core::Json read_json(const GeneValues& values) {
    const GeneValues::Impl& impl = values.impl();
    const ValueReport& report = impl.report;
    core::Json skipped = core::Json::array();
    for (const SkippedLine& line : report.skipped) {
        core::Json item = core::Json::object();
        item["line"] = line.line;
        item["reason"] = name(line.reason);
        item["text"] = utf8_text(line.text);
        skipped.push_back(std::move(item));
    }
    core::Json repeated = core::Json::array();
    for (const RepeatedId& id : report.repeated) {
        core::Json item = core::Json::object();
        item["id"] = utf8_text(impl.names[impl.index.at(id.id)]);
        item["lines"] = id.lines;
        item["values"] = id.values;
        repeated.push_back(std::move(item));
    }
    core::Json unmapped_ids = core::Json::array();
    for (const std::string& id : report.unmapped_ids) unmapped_ids.push_back(utf8_text(id));
    core::Json json = core::Json::object();
    json["lines"] = report.lines;
    json["header"] = report.header;
    json["blank"] = report.blank;
    json["parsed"] = report.parsed;
    json["genes"] = impl.ids.size();
    json["skipped"] = std::move(skipped);
    json["repeated"] = std::move(repeated);
    json["unmapped"] = core::Json::object();
    json["unmapped"]["lines"] = report.unmapped_lines;
    json["unmapped"]["ids"] = std::move(unmapped_ids);
    return json;
}

core::Json thresholds_json(const std::optional<Thresholds>& thresholds) {
    if (!thresholds) return nullptr;
    core::Json json = core::Json::object();
    json["up"] = thresholds->up;
    json["down"] = thresholds->down;
    return json;
}

core::Json not_de_json(const Observations& observations, const core::Interner& interner) {
    const Observations::Impl& impl = observations.impl();
    const auto ids = [&](const std::vector<core::GeneId>& list) {
        core::Json array = core::Json::array();
        for (const core::GeneId id : list) array.push_back(utf8_text(interner.str(id)));
        return array;
    };
    core::Json json = core::Json::object();
    json["path"] = impl.not_de_file ? core::Json(utf8_text(core::to_utf8(*impl.not_de_file))) : core::Json(nullptr);
    json["removed"] = ids(impl.not_de_removed);
    json["unchanged"] = ids(impl.not_de_unchanged);
    json["unknown"] = ids(impl.not_de_unknown);
    return json;
}

core::Json suggestion_fields(const Suggestion& suggestion) {
    const Distribution& d = suggestion.distribution;
    core::Json distribution = core::Json::object();
    distribution["n"] = d.n;
    distribution["min"] = d.min;
    distribution["max"] = d.max;
    distribution["mean"] = d.mean;
    distribution["sd"] = d.sd;
    distribution["median"] = d.median;
    distribution["mad"] = d.mad;
    distribution["mad_scaled"] = d.mad * mad_to_sd;
    // S3.15: how many times further one tail reaches, when neither extreme is 0
    // (Linux/supporting_files/suggest_thresholds.py:130-135).
    if (std::min(std::abs(d.min), std::abs(d.max)) > 0) {
        const bool up_wider = std::abs(d.max) >= std::abs(d.min);
        distribution["wider_tail"] = up_wider ? "up" : "down";
        distribution["tail_ratio"] = up_wider ? std::abs(d.max) / std::abs(d.min) : std::abs(d.min) / std::abs(d.max);
    } else {
        distribution["wider_tail"] = nullptr;
        distribution["tail_ratio"] = nullptr;
    }
    core::Json rows = core::Json::array();
    for (const CandidateRow& row : suggestion.rows) {
        core::Json item = core::Json::object();
        item["criterion"] = row.criterion;
        item["up"] = row.up;
        item["down"] = row.down;
        item["n_up"] = row.n_up;
        item["n_down"] = row.n_down;
        item["n_de"] = row.n_up + row.n_down;
        item["conflicts"] = row.conflicts;
        rows.push_back(std::move(item));
    }
    core::Json histogram = core::Json::object();
    histogram["lo"] = suggestion.histogram.lo;
    histogram["width"] = suggestion.histogram.width;
    histogram["counts"] = suggestion.histogram.counts;
    core::Json json = core::Json::object();
    json["distribution"] = std::move(distribution);
    json["rows"] = std::move(rows);
    json["histogram"] = std::move(histogram);
    return json;
}

void save_debug_values(const GeneValues& values, core::Context& ctx) {
    if (!ctx.log.debug_on()) return;   // S1.29: debug outputs only under the debug option
    const GeneValues::Impl& impl = values.impl();
    std::string text = "id\tvalue\tlines\n";
    for (std::size_t i = 0; i < impl.ids.size(); ++i) {
        text += tsv_cell(impl.names[i]);
        text += '\t';
        text += core::format_number(impl.representative[i]);
        text += '\t';
        text += join_numbers(values.line_numbers(i), ",");
        text += '\n';
    }
    core::OutputFile file = ctx.files.create_debug("values.tsv");
    file.out().write(text.data(), static_cast<std::streamsize>(text.size()));
    file.commit();
}

void write_file(core::Context& ctx, std::string_view name, std::string_view text) {
    core::OutputFile file = ctx.files.create(name);
    file.out().write(text.data(), static_cast<std::streamsize>(text.size()));
    file.commit();
}

std::string json_text(const core::Json& json) {
    return json.dump(2, ' ', false, core::Json::error_handler_t::replace) + "\n";
}

}  // namespace fusion::expression::detail
