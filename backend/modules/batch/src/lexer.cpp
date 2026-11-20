// The batch file and its lines: reading (S1.01, 01.C19) and lexing (S1.05, S1.06, 01.C2).
// FuSION 1 read every line with getline and split it with strtok on spaces only
// (Linux/src/main.cc:1059-1067 and 1151-1181); 2.0 splits on runs of spaces and tabs, drops one '\r',
// sizes the token list to the line, and adds double quotes (S1.05's decision).
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

#include "fusion/batch/batch.hpp"

namespace fusion::batch {
namespace {

bool blank_char(char c) noexcept { return c == ' ' || c == '\t'; }
bool lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
bool digit(char c) noexcept { return c >= '0' && c <= '9'; }

// One UTF-8 code point at s[i]; returns its length, or 0 when the bytes are not valid UTF-8
// (overlong forms, surrogates and values past U+10FFFF included).
std::size_t utf8_length(std::string_view s, std::size_t i) noexcept {
  const auto b0 = static_cast<unsigned char>(s[i]);
  if (b0 < 0x80) return 1;
  std::size_t n = 0;
  std::uint32_t cp = 0;
  if ((b0 & 0xE0) == 0xC0) { n = 2; cp = b0 & 0x1F; }
  else if ((b0 & 0xF0) == 0xE0) { n = 3; cp = b0 & 0x0F; }
  else if ((b0 & 0xF8) == 0xF0) { n = 4; cp = b0 & 0x07; }
  else return 0;
  if (i + n > s.size()) return 0;
  for (std::size_t k = 1; k < n; ++k) {
    const auto b = static_cast<unsigned char>(s[i + k]);
    if ((b & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (b & 0x3F);
  }
  if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000)) return 0;   // overlong
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
  return n;
}

bool valid_utf8(std::string_view s) noexcept {
  for (std::size_t i = 0; i < s.size();) {
    const std::size_t n = utf8_length(s, i);
    if (n == 0) return false;
    i += n;
  }
  return true;
}

}  // namespace

SourceFile source_from_text(std::filesystem::path path, std::string_view bytes) {
  SourceFile source;
  source.path = std::move(path);
  source.sha256 = core::Sha256::of(bytes);
  if (bytes.starts_with("\xEF\xBB\xBF")) bytes.remove_prefix(3);   // a leading BOM is skipped (01.C19)
  std::size_t start = 0;
  while (start < bytes.size()) {
    std::size_t end = bytes.find('\n', start);
    const bool last = end == std::string_view::npos;
    if (last) end = bytes.size();
    std::string_view line = bytes.substr(start, end - start);
    if (line.ends_with('\r')) line.remove_suffix(1);              // one '\r' before '\n' or at the end
    if (!source.invalid_utf8_line && !valid_utf8(line))
      source.invalid_utf8_line = static_cast<int>(source.lines.size()) + 1;
    source.lines.emplace_back(line);
    start = last ? bytes.size() : end + 1;
  }
  return source;
}

// S1.01: FuSION 1 printed "Could not open file" and went on into undefined behaviour
// (Linux/src/main.cc:1062-1064); 2.0 stops with a usage error, exit status 2.
SourceFile read_source(const std::filesystem::path& file) {
  if (file.empty()) throw core::UsageError("usage.no_batch", "no batch file named after -b");
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(file, ec);
  const auto status = std::filesystem::status(file, ec);
  if (ec || !std::filesystem::exists(status))
    throw core::UsageError("usage.batch_missing", "batch file " + core::to_utf8(file) + " does not exist");
  if (!std::filesystem::is_regular_file(status))
    throw core::UsageError("usage.batch_unreadable", "batch file " + core::to_utf8(file) + " is not a file");
  std::ifstream in(file, std::ios::binary);
  if (!in) throw core::UsageError("usage.batch_unreadable", "cannot read batch file " + core::to_utf8(file));
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) throw core::UsageError("usage.batch_unreadable", "cannot read batch file " + core::to_utf8(file));
  SourceFile source = source_from_text(ec ? file : absolute, bytes);
  bool any_command = false;
  for (std::size_t i = 0; i < source.lines.size() && !any_command; ++i)
    any_command = lex(static_cast<int>(i) + 1, source.lines[i]).kind == LineKind::command;
  if (!any_command)
    throw core::UsageError("usage.batch_empty", "batch file " + core::to_utf8(file) + " holds no command");
  return source;
}

// S1.05, S1.06 and 01.C2: runs of spaces and tabs separate tokens; '"' opens a quoted section anywhere in
// a token, '""' inside it is one '"', the backslash is ordinary, and nothing is expanded here.
Line lex(int number, std::string_view text) {
  Line line;
  line.number = number;
  std::size_t i = 0;
  const std::size_t n = text.size();
  while (i < n && blank_char(text[i])) ++i;
  if (i == n) return line;                                        // blank: spaces and tabs only (S1.06)
  if (text.substr(i).starts_with(";;")) {                          // the first token starts with ";;"
    line.kind = LineKind::comment;
    return line;
  }
  line.kind = LineKind::command;
  std::size_t first_end = std::string_view::npos;
  while (i < n) {
    while (i < n && blank_char(text[i])) ++i;
    if (i == n) break;
    Token token;
    token.column = static_cast<int>(i) + 1;
    bool in_quote = false;
    bool name_possible = true;                                     // still inside [a-z][a-z0-9_]*
    bool key_possible = true;                                      // still inside [a-z][a-z0-9_.]*
    int quote_column = 0;
    while (i < n) {
      const char c = text[i];
      if (in_quote) {
        if (c == '"') {
          if (i + 1 < n && text[i + 1] == '"') { token.text += '"'; i += 2; }
          else { in_quote = false; ++i; }
        } else {
          token.text += c;
          ++i;
        }
        continue;
      }
      if (blank_char(c)) break;
      if (c == '"') {
        in_quote = true;
        token.quoted = true;
        quote_column = static_cast<int>(i) + 1;
        name_possible = key_possible = false;                     // a quoted '=' never makes a name
        ++i;
        continue;
      }
      if (c == '=' && !token.key_end && !token.text.empty()) {
        if (key_possible) token.key_end = token.text.size();
        if (name_possible && !token.name_end) token.name_end = token.text.size();
        name_possible = key_possible = false;
      } else if (!token.key_end) {
        const bool first = token.text.empty();
        const bool name_char = first ? lower(c) : (lower(c) || digit(c) || c == '_');
        if (!name_char) name_possible = false;
        if (!(name_char || (!first && c == '.'))) key_possible = false;
      }
      token.text += c;
      ++i;
    }
    if (in_quote && !line.unterminated_quote) line.unterminated_quote = quote_column;
    if (first_end == std::string_view::npos) first_end = i;
    line.tokens.push_back(std::move(token));
  }
  std::size_t r = first_end;
  while (r < n && blank_char(text[r])) ++r;
  line.rest = std::string(text.substr(r));
  return line;
}

}  // namespace fusion::batch
