// The kit's small helpers (util.hpp).
#include "util.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <iterator>

#include "fusion/core/core.hpp"

namespace fusion::testkit::detail {

void fail(const std::string& message) { throw core::Failure("testkit.error", message); }

std::string slurp(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw core::Failure("testkit.unreadable", "cannot read " + core::to_utf8(path));
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path& path, std::string_view text) {
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!out) throw core::Failure("testkit.unwritable", "cannot write " + core::to_utf8(path));
}

std::vector<std::string> lines_of(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string line(text.substr(start, end - start));
    if (!line.empty() && line.back() == '\r') line.pop_back();
    out.push_back(std::move(line));
    start = end + 1;
  }
  return out;
}

std::vector<std::string> words(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (i < text.size()) {
    while (i < text.size() && space(text[i])) ++i;
    const std::size_t start = i;
    while (i < text.size() && !space(text[i])) ++i;
    if (i > start) out.emplace_back(text.substr(start, i - start));
  }
  return out;
}

std::vector<std::string> split(std::string_view text, char separator) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t end = text.find(separator, start);
    if (end == std::string_view::npos) {
      out.emplace_back(text.substr(start));
      return out;
    }
    out.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
}

std::string join(const std::vector<std::string>& items, std::string_view separator) {
  std::string out;
  for (std::size_t k = 0; k < items.size(); ++k) {
    if (k) out += separator;
    out += items[k];
  }
  return out;
}

std::vector<int> integers(std::string_view line) {
  std::vector<int> out;
  std::size_t i = 0;
  while (i < line.size()) {
    const bool negative = line[i] == '-' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1]));
    if (std::isdigit(static_cast<unsigned char>(line[i])) || negative) {
      int value = 0;
      const auto r = std::from_chars(line.data() + i, line.data() + line.size(), value);
      out.push_back(value);
      i = static_cast<std::size_t>(r.ptr - line.data());
    } else {
      ++i;
    }
  }
  return out;
}

std::optional<double> leading_double(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
  std::size_t end = i;
  if (end < text.size() && (text[end] == '+' || text[end] == '-')) ++end;
  bool digits = false;
  while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end, digits = true;
  if (end < text.size() && text[end] == '.') {
    ++end;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end, digits = true;
  }
  if (!digits) return std::nullopt;
  if (end < text.size() && (text[end] == 'e' || text[end] == 'E')) {
    std::size_t exp = end + 1;
    if (exp < text.size() && (text[exp] == '+' || text[exp] == '-')) ++exp;
    if (exp < text.size() && std::isdigit(static_cast<unsigned char>(text[exp]))) {
      while (exp < text.size() && std::isdigit(static_cast<unsigned char>(text[exp]))) ++exp;
      end = exp;
    }
  }
  const core::Decimal d = core::parse_decimal(text.substr(i, end - i));
  if (d.status != core::DecimalStatus::ok) return std::nullopt;
  return d.value;
}

std::string strip_colons(std::string_view id) {
  std::string out;
  for (char c : id)
    if (c != ':') out += c;
  return out;
}

std::string trim(std::string_view text) {
  std::size_t a = 0, b = text.size();
  while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
  return std::string(text.substr(a, b - a));
}

}  // namespace fusion::testkit::detail
