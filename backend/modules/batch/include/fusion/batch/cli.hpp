// fusion/batch/cli.hpp: the fusion executable's command line (section 01, the outline's fourth block,
// Choice 01.C14). Section 01 puts these in the executable's main.cpp; they live in the batch module so the
// tests can call them, and backend/cli/main.cpp stays a thin wrapper (B1).
#pragma once

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fusion::cli {

struct Options {
  std::optional<std::filesystem::path> batch;                 // -b
  std::optional<std::string> help;                            // -h [command]; "" = listing
  bool check = false, json = false, commands = false, query = false;
  bool version = false, debug = false;
  std::optional<std::filesystem::path> root, run_dir, resume, stop;
  std::optional<std::string> label, log;
  std::vector<std::pair<std::string, std::string>> tags;      // --tag key=value
  std::vector<std::pair<std::string, std::string>> settings;  // --set key=value
  std::optional<unsigned> jobs;                               // --jobs N; nullopt = auto
  std::vector<std::string> points;                            // --point
  std::vector<std::string> command_line;                      // Added: argv as given, for run.json
  std::function<void()> startup;                              // Added: the startup check before the batch
                                                              // is read; empty: solve::verify_z3 (11)
};
std::variant<Options, std::string> parse(std::span<const std::string> utf8_args);  // string: usage error
int run(const Options&);

// Added: the same, with the streams and stdin given, so the tests can run the executable's paths.
struct Streams { std::istream& in; std::ostream& out; std::ostream& err; };
int run(const Options&, Streams);
void print_usage(std::ostream&, bool with_listing);          // S1.02's text and the listing
int main_utf8(std::span<const std::string> utf8_args, Streams);   // parse, then run; usage errors exit 2

// Exit statuses (01.C14).
inline constexpr int exit_ok = 0, exit_failed = 1, exit_usage = 2, exit_stopped = 3, exit_interrupted = 130;

}  // namespace fusion::cli
