// results/src/text.cpp: the module's shared text rules (see text.hpp).
#include "text.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>

namespace fusion::results::detail {

std::string number_text(double value) {
  // Whole values print as integers, as the user's thresholds are written ("> 700"); the rest exactly
  // (S1.27). 2^53 bounds the doubles whose integer part is exact.
  if (std::isfinite(value) && value == std::trunc(value) && std::fabs(value) < 9007199254740992.0) {
    return std::to_string(static_cast<long long>(value));
  }
  return core::format_number(value);
}

std::string join(std::span<const std::string> parts, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) out += separator;
    out += parts[i];
  }
  return out;
}

std::string html_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

namespace {
bool is_control(char c) noexcept {
  const auto u = static_cast<unsigned char>(c);
  return u < 0x20 || u == 0x7f;
}
}  // namespace

std::string tsv_field(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (is_control(c)) c = ' ';
  }
  return out;
}

std::string dot_quote(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (is_control(c)) {
      out += ' ';
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}

std::string json_for_html(std::string_view json) {
  std::string out;
  out.reserve(json.size() + 16);
  for (const char c : json) {
    if (c == '<') {
      out += "\\u003c";
    } else {
      out += c;
    }
  }
  return out;
}

std::string base_name(std::string_view path) {
  const auto slash = path.find_last_of("/\\");
  return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string dump(const core::Json& json, int indent) {
  return json.dump(indent, ' ', false, core::Json::error_handler_t::replace);
}

std::string last_lines(std::string_view text, std::size_t lines) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.remove_suffix(1);
  std::size_t start = text.size();
  for (std::size_t seen = 0; start > 0; --start) {
    if (text[start - 1] == '\n' && ++seen == lines) break;
  }
  return std::string(text.substr(start));
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw core::Failure("io.read", "cannot read " + core::to_utf8(path));
  }
  std::ostringstream bytes;
  bytes << in.rdbuf();
  return std::move(bytes).str();
}

}  // namespace fusion::results::detail
