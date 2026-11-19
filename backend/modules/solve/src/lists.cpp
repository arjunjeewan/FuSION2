// The solve's own input files: the node and edge lists (S5.05, S5.06) and the coexpression CSV
// (S5.15). Each is read once; binding the ids is bind.cpp's.
#include "fusion/solve/solve.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

#include "internal.hpp"

namespace fusion::solve {

namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw core::Failure("input.unreadable", "cannot read " + core::to_utf8(path), detail::where(core::to_utf8(path)));
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool blank(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

std::vector<std::string> split_blanks(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && blank(text[i])) ++i;
    const std::size_t start = i;
    while (i < text.size() && !blank(text[i])) ++i;
    if (i > start) out.emplace_back(text.substr(start, i - start));
  }
  return out;
}

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    out.push_back(line);
    if (end == text.size()) break;
    start = end + 1;
  }
  return out;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && blank(s.front())) s.remove_prefix(1);
  while (!s.empty() && blank(s.back())) s.remove_suffix(1);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
  return s;
}

std::vector<std::string_view> split_commas(std::string_view line) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t comma = line.find(',', start);
    out.push_back(trim(line.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start)));
    if (comma == std::string_view::npos) break;
    start = comma + 1;
  }
  return out;
}

// One coexpression cell: whitespace-separated ids, each with its ':' removed, as FuSION 1 tokenised
// StringDB's paralog clusters ("hsa:3108 hsa:3109"; GraphManagerNew.cc:28645-28658).
std::vector<std::string> cell_ids(std::string_view cell) {
  std::vector<std::string> out = split_blanks(cell);
  for (std::string& id : out) id.erase(std::remove(id.begin(), id.end(), ':'), id.end());
  out.erase(std::remove(out.begin(), out.end(), std::string()), out.end());
  return out;
}

}  // namespace

// S5.05: whitespace-separated ids, as FuSION 1's read_data_file_into_set_of_ints read them
// (GraphManagerNew.cc:18688-18708). An unreadable file is a failure (decision 0021), where FuSION 1
// printed a message and read nothing.
std::vector<std::string> read_node_list(const std::filesystem::path& path) {
  return split_blanks(read_file(path));
}

// S5.06: "<source> <target> <subtypes>", the subtypes joined by '_' as FuSION 1's labels joined them
// (GraphManagerNew.cc:18739-18819), or by ','. The field is required: FuSION 1 declared it outside its
// loop, so a two-token line reused the previous line's subtypes (S5.06).
EdgeLine parse_edge_line(std::string_view text, int line) {
  const std::vector<std::string> tokens = split_blanks(text);
  if (tokens.size() < 3)
    throw core::Failure("solve.edge_line_no_subtype",
                        "edge line " + std::to_string(line) + " has no subtype field: \"" + std::string(text) +
                            "\" (S5.06 requires <source> <target> <subtypes>)",
                        detail::where({}, static_cast<std::uint32_t>(line)));
  EdgeLine edge{tokens[0], tokens[1], {}, line};
  if (tokens[2] != "-") {
    std::string_view field = tokens[2];
    std::size_t start = 0;
    while (start <= field.size()) {
      const std::size_t end = std::min(field.find_first_of("_,", start), field.size());
      if (end > start) edge.subtypes.emplace_back(field.substr(start, end - start));
      start = end + 1;
    }
  }
  return edge;
}

std::vector<EdgeLine> read_edge_list(const std::filesystem::path& path) {
  const std::string text = read_file(path);
  std::vector<EdgeLine> out;
  int number = 0;
  for (std::string_view line : split_lines(text)) {
    ++number;
    if (split_blanks(line).empty()) continue;
    try {
      out.push_back(parse_edge_line(line, number));
    } catch (const core::Failure& f) {
      throw core::Failure(f.code(), core::to_utf8(path) + ": " + f.what(),
                          detail::where(core::to_utf8(path), static_cast<std::uint32_t>(number)));
    }
  }
  return out;
}

// S5.15's fix: columns found by their header names, not by position as FuSION 1 did (it took the
// third and fourth fields and the text after the fourth comma; GraphManagerNew.cc:28601-28680).
// Rows whose score is not a number in 04.C2's grammar are skipped and reported, where FuSION 1's
// stof skipped them silently.
CoexpressionTable CoexpressionTable::parse(std::string_view text, std::string source_name) {
  CoexpressionTable table;
  table.source_ = std::move(source_name);
  const std::vector<std::string_view> lines = split_lines(text);
  std::size_t first = 0;
  while (first < lines.size() && trim(lines[first]).empty()) ++first;
  if (first == lines.size())
    throw core::Failure("solve.coexpression_columns", table.source_ + " is empty (S5.15)",
                        detail::where(table.source_));
  const std::vector<std::string_view> header = split_commas(lines[first]);
  const auto column = [&](std::string_view wanted) -> std::size_t {
    const auto it = std::find(header.begin(), header.end(), wanted);
    if (it == header.end())
      throw core::Failure("solve.coexpression_columns",
                          table.source_ + " has no column " + std::string(wanted) +
                              "; it needs hsa_id_node1, hsa_id_node2 and coexpression_score (S5.15)",
                          detail::where(table.source_, static_cast<std::uint32_t>(first + 1)));
    return static_cast<std::size_t>(it - header.begin());
  };
  const std::size_t c1 = column("hsa_id_node1"), c2 = column("hsa_id_node2"), cs = column("coexpression_score");
  const std::size_t needed = std::max({c1, c2, cs}) + 1;
  for (std::size_t i = first + 1; i < lines.size(); ++i) {
    if (trim(lines[i]).empty()) continue;
    const auto line_number = static_cast<std::uint32_t>(i + 1);
    const std::vector<std::string_view> fields = split_commas(lines[i]);
    if (fields.size() < needed) {
      table.skipped_.push_back(line_number);
      continue;
    }
    const core::Decimal score = core::parse_decimal(fields[cs]);
    Row row{cell_ids(fields[c1]), cell_ids(fields[c2]), score.value, line_number};
    if (score.status != core::DecimalStatus::ok || row.ids1.empty() || row.ids2.empty()) {
      table.skipped_.push_back(line_number);
      continue;
    }
    table.rows_.push_back(std::move(row));
  }
  return table;
}

CoexpressionTable read_coexpression(const std::filesystem::path& path, core::Context& ctx) {
  CoexpressionTable table = CoexpressionTable::parse(read_file(path), core::to_utf8(path));
  const auto skipped = table.skipped_lines();
  if (!skipped.empty()) {
    std::string lines;
    for (std::size_t i = 0; i < skipped.size() && i < 10; ++i) lines += (i ? ", " : "") + std::to_string(skipped[i]);
    if (skipped.size() > 10) lines += ", ...";
    ctx.log.warning("solve.coexpression_rows_skipped",
                    std::to_string(skipped.size()) + " rows of " + table.source_name() +
                        " skipped, without both ids and a numeric score (lines " + lines + ")");
  }
  return table;
}

}  // namespace fusion::solve
