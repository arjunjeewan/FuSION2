// Variables: let, and '$' expansion by the longest defined key (S1.07, S1.08, 01.C2's "$$").
// FuSION 1 tried the keys in std::map byte order and took the first that was a prefix
// (Linux/src/main.cc:1197-1247), so $PLOT_FILE hid $PLOT_FILE_WO_GENE; S1.07 fixes this with the
// longest match. Keys sit in a byte trie, so a match costs the length of the key.
#include <algorithm>

#include "fusion/batch/batch.hpp"

namespace fusion::batch {

struct Variables::TrieNode {
  std::vector<std::pair<char, std::unique_ptr<TrieNode>>> children;   // few per node: a linear scan
  int value = -1;                                                     // index into order_

  TrieNode* child(char c) const noexcept {
    for (const auto& [k, node] : children)
      if (k == c) return node.get();
    return nullptr;
  }
};

Variables::Variables() : root_(std::make_unique<TrieNode>()) {}
Variables::~Variables() = default;
Variables::Variables(Variables&&) noexcept = default;
Variables& Variables::operator=(Variables&&) noexcept = default;

bool Variables::define(std::string key, std::string value, int line) {
  TrieNode* node = root_.get();
  for (const char c : key) {
    TrieNode* next = node->child(c);
    if (!next) {
      node->children.emplace_back(c, std::make_unique<TrieNode>());
      next = node->children.back().second.get();
    }
    node = next;
  }
  if (node->value >= 0) return false;                  // first definition wins; the caller reports it
  node->value = static_cast<int>(order_.size());
  order_.emplace_back(std::move(key), std::move(value), line);
  return true;
}

std::optional<int> Variables::defined_at(std::string_view key) const {
  const TrieNode* node = root_.get();
  for (const char c : key) {
    node = node->child(c);
    if (!node) return std::nullopt;
  }
  if (node->value < 0) return std::nullopt;
  return std::get<2>(order_[static_cast<std::size_t>(node->value)]);
}

namespace {
bool name_start(char c) noexcept { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
bool name_char(char c) noexcept { return name_start(c) || (c >= '0' && c <= '9'); }
}  // namespace

// One pass: each value is inserted once and never rescanned; a '$' that matches nothing passes through.
Variables::Expansion Variables::expand(std::string_view text, Mode mode) const {
  Expansion out;
  out.text.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c != '$') {
      out.text += c;
      ++i;
      continue;
    }
    if (i + 1 < text.size() && text[i + 1] == '$') {   // 01.C2: "$$" is one '$', except for the shell
      out.text += mode == Mode::argument ? "$" : "$$";
      i += 2;
      continue;
    }
    const TrieNode* node = root_.get();
    int best = -1;
    std::size_t best_length = 0;
    for (std::size_t j = i; j < text.size(); ++j) {
      node = node->child(text[j]);
      if (!node) break;
      if (node->value >= 0) {
        best = node->value;
        best_length = j - i + 1;
      }
    }
    if (best >= 0) {
      out.text += std::get<1>(order_[static_cast<std::size_t>(best)]);
      i += best_length;
      continue;
    }
    if (mode == Mode::argument && i + 1 < text.size() && name_start(text[i + 1])) {
      std::size_t k = i + 1;
      while (k < text.size() && name_char(text[k])) ++k;
      std::string name(text.substr(i, k - i));
      if (std::find(out.unmatched.begin(), out.unmatched.end(), name) == out.unmatched.end())
        out.unmatched.push_back(std::move(name));
    }
    out.text += '$';
    ++i;
  }
  return out;
}

std::vector<std::tuple<std::string, std::string, int>> Variables::in_order() const { return order_; }

bool valid_variable_key(std::string_view key) noexcept {
  if (key.size() < 2 || key[0] != '$' || !name_start(key[1])) return false;
  return std::all_of(key.begin() + 2, key.end(), name_char);
}

}  // namespace fusion::batch
