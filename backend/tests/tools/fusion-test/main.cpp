// fusion-test, the driver of section 12's tests (12.C2, 12.C8; the C3 note): subcommand dispatch and the exit
// codes 0 (no failure), 1 (a difference or a failure), 2 (the harness could not run) and 4 (everything skipped).
// ref is the reference runner (C4); bench comes with C6.
#include <iostream>
#include <string>
#include <vector>

#include "data.hpp"
#include "e2e/engine.hpp"
#include "fusion/core/core.hpp"
#include "fusion/testkit/data.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"
#include "ref/ref.hpp"

namespace {

using namespace fusion::fusiontest;
namespace fs = std::filesystem;

int dispatch(const Command& c) {
  if (c.name == "ref") return fusion::reftest::command(c.rest, std::cout, std::cerr);
  if (c.name == "bench") {
    std::cerr << "error: fusion-test bench is not built yet: the benchmarks come with C6 (docs/phase3/plan.md)\n";
    return exit_harness;
  }
  const std::optional<fs::path> data = data_dir(c);
  if (c.name == "data") {
    if (!data) {
      std::cerr << "error: no data root: set FUSION_TEST_DATA or give --data <folder>\n";
      return exit_harness;
    }
    const fs::path manifest = fusion::testkit::manifest_path();
    if (c.action == "verify") return data::verify(*data, manifest, std::cout, std::cerr);
    if (c.action == "init") return data::init(fs::absolute(*c.from), *data, manifest, std::cout, std::cerr);
    return data::record(*c.origin, c.paths, *data, manifest, std::cout, std::cerr);
  }
  e2e::Settings s;
  s.fusion = fusion_exe(c);
  if (data) s.data = fusion::testkit::DataRoot{*data};
  s.root = fusion::testkit::source_dir() / "e2e";
  s.keep = c.keep;
  if (c.list) return e2e::list(s, c.cases, std::cout, std::cerr);
  return e2e::run(s, c.cases, c.update, std::cout, std::cerr);
}

int main_utf8(const std::vector<std::string>& args) {
  try {
    const auto parsed = parse(args);
    if (const auto* message = std::get_if<std::string>(&parsed)) {
      std::cerr << "error: " << *message << "\n\n";
      print_usage(std::cerr);
      return exit_harness;
    }
    const Command& c = std::get<Command>(parsed);
    if (c.help) {
      print_usage(std::cout);
      return exit_ok;
    }
    return dispatch(c);
  } catch (const fusion::core::Failure& f) {
    std::cerr << "error: " << f.what() << " [" << f.code() << "]\n";
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
  }
  return exit_harness;
}

}  // namespace

#ifdef _WIN32
// Windows: arguments to UTF-8, as fusion's own main does.
int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    const auto u8 = std::filesystem::path(argv[i]).u8string();
    args.emplace_back(u8.begin(), u8.end());
  }
  return main_utf8(args);
}
#else
int main(int argc, char** argv) { return main_utf8(std::vector<std::string>(argv + 1, argv + argc)); }
#endif
