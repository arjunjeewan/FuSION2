// fusion-test's command line (options.hpp).
#include "options.hpp"

#include <ostream>

#include "fusion/core/core.hpp"
#include "fusion/testkit/data.hpp"

#ifndef FUSION_EXE
#error "fusion-test is built with FUSION_EXE, the build's staged fusion"
#endif

namespace fusion::fusiontest {

namespace fs = std::filesystem;

std::variant<Command, std::string> parse(std::span<const std::string> args) {
  Command c;
  if (args.empty()) return std::string("no subcommand: give data, e2e, ref or bench");
  const auto help = [](const std::string& a) { return a == "-h" || a == "--help"; };
  if (help(args[0])) {
    c.help = true;
    return c;
  }
  c.name = args[0];
  if (c.name != "data" && c.name != "e2e" && c.name != "ref" && c.name != "bench")
    return "unknown subcommand " + c.name + ": give data, e2e, ref or bench";
  if (c.name == "ref" || c.name == "bench") {   // C4 and C6 parse their own arguments
    c.rest.assign(args.begin() + 1, args.end());
    return c;
  }
  std::size_t i = 1;
  if (c.name == "data") {
    if (i < args.size() && help(args[i])) {
      c.help = true;
      return c;
    }
    if (i == args.size() || args[i].starts_with("-")) return std::string("data needs verify, init or record");
    c.action = args[i++];
    if (c.action != "verify" && c.action != "init" && c.action != "record")
      return "unknown data action " + c.action + ": give verify, init or record";
  }
  const std::string where = "fusion-test " + c.name + (c.action.empty() ? "" : " " + c.action);
  for (; i < args.size(); ++i) {
    const std::string& a = args[i];
    const auto value = [&]() -> std::optional<std::string> {
      if (i + 1 >= args.size() || args[i + 1].empty() || args[i + 1].starts_with("--")) return std::nullopt;
      return args[++i];
    };
    const auto once = [&](auto& into, auto convert) -> std::optional<std::string> {
      if (into) return a + " is given twice";
      const auto v = value();
      if (!v) return a + " needs a value";
      into = convert(*v);
      return std::nullopt;
    };
    const auto as_path = [](const std::string& v) { return core::path_from_utf8(v); };
    const auto as_text = [](const std::string& v) { return v; };
    std::optional<std::string> error;
    if (help(a)) c.help = true;
    else if (a == "--fusion") error = once(c.fusion, as_path);
    else if (a == "--data") error = once(c.data, as_path);
    else if (c.action == "init" && a == "--from") error = once(c.from, as_path);
    else if (c.action == "record" && a == "--origin") error = once(c.origin, as_text);
    else if (c.action == "record" && !a.starts_with("-")) c.paths.push_back(a);
    else if (c.name == "e2e" && (a == "--case" || a == "--update")) {
      const auto v = value();
      if (!v) error = a + " needs a case id";
      else (a == "--case" ? c.cases : c.update).push_back(*v);
    } else if (c.name == "e2e" && a == "--list") c.list = true;
    else if (c.name == "e2e" && a == "--keep") c.keep = true;
    else error = (a.starts_with("-") ? "unknown option " : "unexpected argument ") + a + " for " + where;
    if (error) return *error;
  }
  if (c.help) return c;
  if (c.action == "init" && !c.from) return std::string("data init needs --from <the folder holding FuSION 1's trees>");
  if (c.action == "record" && !c.origin) return std::string("data record needs --origin <work item and report>");
  if (c.action == "record" && c.paths.empty())
    return std::string("data record needs a file or folder of the data root");
  if (c.list && !c.update.empty()) return std::string("--list runs nothing, so it does not go with --update");
  return c;
}

void print_usage(std::ostream& out) {
  out << "Run as \"fusion-test <subcommand> ...\" (section 12; the C3 note)\n\n"
         "  fusion-test data verify [--data DIR]\n"
         "      check every file of data-manifest.json in the data root, through the hash cache\n"
         "  fusion-test data init --from DIR [--data DIR]\n"
         "      copy the root's missing files from DIR, the folder holding FuSION/ and the snapshot, then verify\n"
         "  fusion-test data record --origin TEXT PATH... [--data DIR]\n"
         "      add the files of each PATH of the data root (relative to it) to data-manifest.json\n"
         "  fusion-test e2e [--case ID]... [--update ID]... [--list] [--keep] [--fusion EXE] [--data DIR]\n"
         "      run the end-to-end cases; --update rewrites a case's expected/, --list prints the cases,\n"
         "      --keep keeps every case's scratch folder\n"
         "  fusion-test ref ...      the reference runner (C4)\n"
         "  fusion-test bench ...    the benchmarks (C6)\n\n"
         "  --fusion EXE   the fusion to run; default: the build's staged fusion\n"
         "  --data DIR     the data root; default: FUSION_TEST_DATA\n\n"
         "Exit codes: 0 no failure, 1 a difference or a failure, 2 the harness could not run, 4 everything skipped.\n";
}

fs::path fusion_exe(const Command& c) {
  return c.fusion ? fs::absolute(*c.fusion) : core::path_from_utf8(FUSION_EXE);
}

std::optional<fs::path> data_dir(const Command& c) {
  if (c.data) return fs::absolute(*c.data);
  const auto root = testkit::DataRoot::from_env();
  if (!root) return std::nullopt;
  return fs::absolute(root->root);
}

}  // namespace fusion::fusiontest
