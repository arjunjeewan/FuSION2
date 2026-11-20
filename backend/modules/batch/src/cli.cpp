// The fusion command line (S1.01 to S1.03, 01.C14). FuSION 1 looked only at argv[1]: -b read argv[2]
// without checking argc, -d printed a message and fell into undefined behaviour, and any other first
// argument printed the usage on stdout with status 255 (Linux/src/main.cc:1046-1109). 2.0 parses every
// option, puts usage errors on stderr with status 2, and drops -d (S1.03).
#include <charconv>
#include <ostream>

#include "fusion/batch/cli.hpp"
#include "internal.hpp"

namespace fusion::cli {

namespace {

bool split_pair(std::string_view text, std::pair<std::string, std::string>& out) {
  const auto eq = text.find('=');
  if (eq == std::string_view::npos || eq == 0) return false;
  out = {std::string(text.substr(0, eq)), std::string(text.substr(eq + 1))};
  return true;
}

}  // namespace

std::variant<Options, std::string> parse(std::span<const std::string> args) {
  Options o;
  o.command_line.assign(args.begin(), args.end());
  if (args.size() <= 1) return std::string("no batch file: run as \"fusion -b <batch_file>\"");
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& a = args[i];
    const auto value = [&]() -> std::optional<std::string> {
      if (i + 1 >= args.size() || (args[i + 1].starts_with('-') && args[i + 1].size() > 1)) return std::nullopt;
      return args[++i];
    };
    const auto path_option = [&](std::optional<std::filesystem::path>& into) -> std::optional<std::string> {
      if (into) return a + " is given twice";
      const auto v = value();
      if (!v || v->empty()) return a + " needs a value";
      into = core::path_from_utf8(*v);
      return std::nullopt;
    };
    const auto text_option = [&](std::optional<std::string>& into) -> std::optional<std::string> {
      if (into) return a + " is given twice";
      const auto v = value();
      if (!v) return a + " needs a value";
      into = *v;
      return std::nullopt;
    };
    std::optional<std::string> error;
    if (a == "-b") error = path_option(o.batch);
    else if (a == "-h" || a == "--help") {
      if (i + 1 < args.size() && !args[i + 1].starts_with('-')) o.help = args[++i];
      else o.help = "";
    } else if (a == "--check") o.check = true;
    else if (a == "--json") o.json = true;
    else if (a == "--commands") o.commands = true;
    else if (a == "--query") o.query = true;
    else if (a == "--version") o.version = true;
    else if (a == "--debug") o.debug = true;
    else if (a == "--root") error = path_option(o.root);
    else if (a == "--run-dir") error = path_option(o.run_dir);
    else if (a == "--resume") error = path_option(o.resume);
    else if (a == "--stop") error = path_option(o.stop);
    else if (a == "--label") error = text_option(o.label);
    else if (a == "--log") error = text_option(o.log);
    else if (a == "--tag" || a == "--set") {
      const auto v = value();
      std::pair<std::string, std::string> kv;
      if (!v || !split_pair(*v, kv)) error = a + " needs <key>=<value>";
      else (a == "--tag" ? o.tags : o.settings).push_back(std::move(kv));
    } else if (a == "--jobs") {
      const auto v = value();
      if (!v) error = "--jobs needs a number or auto";
      else if (*v != "auto") {
        unsigned n = 0;
        const auto [p, ec] = std::from_chars(v->data(), v->data() + v->size(), n);
        if (ec != std::errc() || p != v->data() + v->size() || n == 0) error = "--jobs needs a positive number or auto";
        else o.jobs = n;
      }
    } else if (a == "--point") {
      const auto v = value();
      if (!v || v->empty()) error = "--point needs point numbers or labels";
      else {
        std::string_view rest = *v;
        while (!rest.empty()) {
          const auto comma = rest.find(',');
          o.points.emplace_back(rest.substr(0, comma));
          if (comma == std::string_view::npos) break;
          rest.remove_prefix(comma + 1);
        }
      }
    } else if (a == "-d") {
      error = "-d is not an option: the interactive prompt was dropped (S1.03)";
    } else {
      error = "unknown option " + a;
    }
    if (error) return *error;
  }
  // One thing to do (01.C14).
  const int modes = (o.batch || o.resume ? 1 : 0) + (o.help ? 1 : 0) + (o.commands ? 1 : 0) + (o.version ? 1 : 0) +
                    (o.stop ? 1 : 0) + (o.query ? 1 : 0);
  if (modes == 0) return std::string("nothing to do: give -b <batch_file>");
  if (modes > 1) return std::string("give one of -b, -h, --commands, --version, --stop and --query");
  if (o.check && !o.batch) return std::string("--check needs -b <batch_file>");
  if (o.json && !o.check && !o.version) return std::string("--json goes with --check or --version");
  if (o.run_dir && o.root) return std::string("--run-dir and --root exclude each other");
  return o;
}

void print_usage(std::ostream& out, bool with_listing) {
  out << "Run as \"fusion -b <batch_file>\"\n\n"
         "Options:\n"
         "  -b <file>              check, then run a batch file\n"
         "  -h [<command>]         this help, or one command's arguments\n"
         "  --check                check the batch file and its inputs; run nothing\n"
         "  --json                 with --check or --version: machine-readable output\n"
         "  --root <dir>           where the run directory is made (default: the batch file's folder)\n"
         "  --label <text>         the run directory's label (default: the batch file's name)\n"
         "  --log <name>           the run log's name\n"
         "  --set <key>=<value>    a run-level setting, over the batch's run line\n"
         "  --run-dir <dir>        run in a directory made for this run\n"
         "  --tag <key>=<value>    a label recorded in run.json\n"
         "  --resume <run dir>     a new run that continues a stopped one\n"
         "  --stop <run dir>       asks a running run to stop\n"
         "  --point <i,j,...>      only these sweep points\n"
         "  --jobs <n>|auto        the core budget for sweep points\n"
         "  --debug                debug records, traces and dumps\n"
         "  --query                one batch line on stdin, one JSON answer on stdout\n"
         "  --commands             the command table as JSON\n"
         "  --version              the versions\n";
  if (with_listing) {
    out << "\n";
    batch::command_table().print_listing(out);
  }
}

}  // namespace fusion::cli
