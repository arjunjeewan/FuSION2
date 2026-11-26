// The test kit's own small helpers: files, lines, tokens and sorted sets. Internal to the kit.
#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fusion::testkit::detail {

std::string slurp(const std::filesystem::path&);                       // throws core::Failure
void write_text(const std::filesystem::path&, std::string_view text);   // creates parent folders
std::vector<std::string> lines_of(std::string_view text);               // without '\n' or a final '\r'
std::vector<std::string> words(std::string_view text);                  // split on spaces, tabs, CR, LF
std::vector<std::string> split(std::string_view text, char separator);  // keeps empty fields
std::string join(const std::vector<std::string>& items, std::string_view separator);
std::vector<int> integers(std::string_view line);                       // every run of digits, in order
std::optional<double> leading_double(std::string_view text);            // stof's reading: a numeric prefix
std::string strip_colons(std::string_view id);                          // the id rule: ':' removed
std::string trim(std::string_view text);

template <class T>
void sort_unique(std::vector<T>& items) {
  std::sort(items.begin(), items.end());
  items.erase(std::unique(items.begin(), items.end()), items.end());
}

[[noreturn]] void fail(const std::string& message);                    // core::Failure("testkit.error")

}  // namespace fusion::testkit::detail
