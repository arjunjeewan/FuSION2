// The fusion executable (B1): a thin wrapper around libfusion's command line (section 01, 01.C14).
// Everything it does is in fusion::cli, so the tests run the same paths.
#include <iostream>
#include <string>
#include <vector>

#include "fusion/batch/cli.hpp"
#include "fusion/run/run.hpp"

#ifdef _WIN32
#include <filesystem>

// Windows: arguments to UTF-8 (11's platform notes); the console's code page is section 11's.
int wmain(int argc, wchar_t** argv) {
  fusion::run::install_signal_handlers();   // 09.C8: Ctrl-C, Ctrl-Break and closing ask for a stop
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    const auto u8 = std::filesystem::path(argv[i]).u8string();
    args.emplace_back(u8.begin(), u8.end());
  }
  return fusion::cli::main_utf8(args, {std::cin, std::cout, std::cerr});
}
#else
int main(int argc, char** argv) {
  fusion::run::install_signal_handlers();   // 09.C8: Ctrl-C and SIGTERM ask for a stop; SIGHUP is ignored
  const std::vector<std::string> args(argv, argv + argc);
  return fusion::cli::main_utf8(args, {std::cin, std::cout, std::cerr});
}
#endif
