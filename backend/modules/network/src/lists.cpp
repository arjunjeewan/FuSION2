// fusion network: input lists and the KGML directory (02.C12; S2.16, S8.31; items 2B.5, 2B.10, 2B.12,
// 1.24; 03.C6; this section's open question 6).
#include <algorithm>
#include <map>

#include "detail.hpp"

namespace fusion::network {

// 02.C12: kgml_dir when given; else, for a list a step wrote, the base it recorded (2B.10); else the
// list file's own folder. FuSION 1 resolved relative entries against the launch directory.
std::filesystem::path list_base(const std::filesystem::path& list, const std::optional<std::filesystem::path>& kgml_dir,
                                const std::optional<std::filesystem::path>& recorded) {
  if (kgml_dir) return *kgml_dir;
  if (recorded) return *recorded;
  std::filesystem::path parent = list.parent_path();
  return parent.empty() ? std::filesystem::path(".") : parent;
}

namespace {

// A bare pathway id ("hsa04010" or "path:hsa04010": no folder, no extension) names <base>/<id>.xml
// (2B.5, S8.31).
bool bare_pathway_id(std::string_view text) {
  if (text.find('/') != std::string_view::npos || text.find('\\') != std::string_view::npos) return false;
  return text.find('.') == std::string_view::npos;
}

std::filesystem::path resolve(std::string_view text, const std::filesystem::path& base) {
  if (bare_pathway_id(text)) {
    std::string id(text);
    if (id.rfind("path:", 0) == 0) id.erase(0, 5);
    return base / core::path_from_utf8(id + ".xml");
  }
  const std::filesystem::path p = core::path_from_utf8(text);
  return p.is_absolute() ? p : base / p;
}

// The pathway a listed KGML declares, from its root element (format detection's scan): only KGMLs
// are compared, since FuSION's own and FuSION 1's network files all carry the root name "KEGG".
std::optional<std::string> kgml_pathway(const std::filesystem::path& path) {
  const auto root = detail::scan_root(detail::read_head(path, 64 * 1024));
  if (!root || root->name != "pathway" || !root->attr("org") || root->attr("format")) return std::nullopt;
  auto name = root->attr("name");
  if (!name || name->empty()) return core::to_utf8(path.stem());
  return name;
}

}  // namespace

// One entry per line, trimmed, blank lines skipped (FuSION 1 aborted on a blank line or CRLF, S2.16).
// Every missing file is named in one failure (open question 6); a file listed twice is read once,
// with a warning; two KGMLs declaring one pathway id fail (1.24, 03.C6).
PathList read_input_list(const std::filesystem::path& list, const std::filesystem::path& base, core::Context& ctx) {
  const std::string bytes = detail::read_input(list);
  const std::string name = detail::display(list);
  PathList out{list, base, {}};
  std::vector<std::uint32_t> numbers;
  std::vector<std::string> missing;
  std::uint32_t first_missing = 0;
  detail::for_each_line(bytes, [&](std::string_view line, std::uint32_t number) {
    const auto text = detail::trim(line);
    if (text.empty()) return;
    ListLine l{std::string(text), resolve(text, base)};
    std::error_code ec;
    if (!std::filesystem::is_regular_file(l.path, ec)) {
      missing.push_back(detail::display(l.path) + " (line " + std::to_string(number) + ")");
      if (!first_missing) first_missing = number;
      return;
    }
    out.lines.push_back(std::move(l));
    numbers.push_back(number);
  });
  if (!missing.empty())
    throw core::Failure("input.missing", name + ": " + std::to_string(missing.size()) + " listed file" +
                        (missing.size() == 1 ? " is" : "s are") + " missing: " + detail::join(missing, ", "),
                        {name, first_missing, {}});

  // Duplicates by file identity, which also catches case-only differences on macOS and Windows;
  // only files of one size can be one file.
  std::vector<bool> drop(out.lines.size(), false);
  std::map<std::uintmax_t, std::vector<std::size_t>> by_size;
  for (std::size_t i = 0; i < out.lines.size(); ++i) {
    ctx.stop.throw_if_stopped();
    std::error_code ec;
    const auto size = std::filesystem::file_size(out.lines[i].path, ec);
    auto& same = by_size[ec ? 0 : size];
    for (std::size_t j : same)
      if (!drop[j] && std::filesystem::equivalent(out.lines[i].path, out.lines[j].path, ec) && !ec) {
        drop[i] = true;
        ctx.log.warning("list.duplicate_entry", name + ": line " + std::to_string(numbers[i]) + " names " +
                        detail::display(out.lines[i].path) + " again (line " + std::to_string(numbers[j]) + "); it is read once");
        break;
      }
    same.push_back(i);
  }
  std::vector<ListLine> kept;
  std::map<std::string, std::string> pathways;               // pathway id -> the file that declared it
  for (std::size_t i = 0; i < out.lines.size(); ++i) {
    if (drop[i]) continue;
    if (const auto pathway = kgml_pathway(out.lines[i].path)) {
      const auto [it, inserted] = pathways.emplace(*pathway, detail::display(out.lines[i].path));
      if (!inserted)
        throw core::Failure("list.duplicate_pathway", name + ": " + it->second + " and " + detail::display(out.lines[i].path) +
                            " both declare pathway " + *pathway, {name, numbers[i], {}});
    }
    kept.push_back(std::move(out.lines[i]));
  }
  out.lines = std::move(kept);
  return out;
}

}  // namespace fusion::network
