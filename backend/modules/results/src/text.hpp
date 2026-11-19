// results/src/text.hpp: the text rules every writer of this module shares: numbers (S1.27), escaping for
// HTML, TSV (S6.08), DOT (S6.07) and inline JSON (07.C2), and deterministic JSON dumps (09.C16).
// Internal to the module.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "fusion/core/core.hpp"

namespace fusion::results::detail {

// A number for people and for FuSION 1's formats: whole values as integers ("700"), others in
// core::format_number's exact shortest form ("0.1"). Never depends on the locale.
std::string number_text(double);

std::string join(std::span<const std::string> parts, std::string_view separator);

// HTML text and attribute values: & < > " and ' escaped.
std::string html_escape(std::string_view);

// A TSV field (E1, S6.08): tab, CR, LF and every other control character become a space.
std::string tsv_field(std::string_view);

// A double-quoted DOT string (S6.07): \ and " escaped, control characters made spaces.
std::string dot_quote(std::string_view);

// JSON text that sits inside a <script> element (07.C2): every '<' written as the escape <, so no
// value can close the element. '<' can only occur inside strings, so the JSON stays the same value.
std::string json_for_html(std::string_view json);

// The part after the last '/' or '\'.
std::string base_name(std::string_view path);

// nlohmann/json's dump with invalid UTF-8 replaced, never thrown on: the same bytes on every platform.
std::string dump(const core::Json&, int indent = -1);

// The last `lines` lines of a tool's stderr, for a failure message (11).
std::string last_lines(std::string_view text, std::size_t lines);

// The bytes of a file a tool wrote; throws core::Failure io.read.
std::string read_file(const std::filesystem::path&);

}  // namespace fusion::results::detail
