// fusion-test data: the data root against data-manifest.json (section 12, "Test data and the KEGG licence";
// the C3 note, C3.2 and C3.4). Each returns fusion-test's exit code.
//   verify   every manifest file, through the kit's hash cache: 0 all match, 1 one is missing or differs
//   init     copies each missing file from <from>/<its from path>, read only, hash-checked before it takes its
//            place; never overwrites a file that differs, and stops on one; then verifies
//   record   adds files the data root generated (W3's captures, W4's outputs, the reports) to the manifest,
//            each with the same `from`: a work item and its report, such as "W3 reports/w3/20261117T183723"
// 2 means the manifest or the root could not be read, or the arguments name nothing usable.
#pragma once

#include <filesystem>
#include <iosfwd>
#include <span>
#include <string>

namespace fusion::fusiontest::data {

int verify(const std::filesystem::path& root, const std::filesystem::path& manifest, std::ostream& out,
           std::ostream& err);
int init(const std::filesystem::path& from, const std::filesystem::path& root, const std::filesystem::path& manifest,
         std::ostream& out, std::ostream& err);
// Each path is a file or folder of the root: relative to it, as manifest paths are, or absolute inside it.
int record(const std::string& origin, std::span<const std::string> paths, const std::filesystem::path& root,
           const std::filesystem::path& manifest, std::ostream& out, std::ostream& err);

}  // namespace fusion::fusiontest::data
