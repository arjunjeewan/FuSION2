// fusion-test's command line (section 12, "Repository layout" and "Files, settings, events and errors";
// the C3 note): the data, e2e, ref and bench subcommands, the data root and the fusion to run.
#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace fusion::fusiontest {

// Section 12's exit codes: 0 no failure; 1 an unexpected difference, a failed prediction, a stale entry or a
// golden mismatch; 2 the harness could not run; 4 everything skipped, which CTest registers as the skip code.
inline constexpr int exit_ok = 0, exit_failed = 1, exit_harness = 2, exit_skipped = 4;

struct Command {
  std::string name;                                    // data, e2e, ref or bench; empty with help alone
  std::string action;                                  // data: verify, init or record
  bool help = false;                                   // -h, --help
  std::optional<std::filesystem::path> fusion, data;  // --fusion EXE, --data DIR
  std::optional<std::filesystem::path> from;           // data init --from DIR: the folder holding FuSION 1's trees
  std::optional<std::string> origin;                   // data record --origin TEXT
  std::vector<std::string> paths;                      // data record PATH...
  std::vector<std::string> cases, update;              // e2e --case ID, --update ID (each repeatable)
  bool list = false, keep = false;                     // e2e --list, --keep
  std::vector<std::string> rest;                       // ref and bench: their arguments, for C4 and C6
};

// The arguments after the program's name. A string is a usage error, exit code 2.
std::variant<Command, std::string> parse(std::span<const std::string> args);
void print_usage(std::ostream&);

std::filesystem::path fusion_exe(const Command&);                     // --fusion, else the build's staged fusion
std::optional<std::filesystem::path> data_dir(const Command&);        // --data, else FUSION_TEST_DATA

}  // namespace fusion::fusiontest
