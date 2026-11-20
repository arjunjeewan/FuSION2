// fusion/batch/batch.hpp: the batch language and its interpreter (section 01, "C++ interface outline").
// The four blocks are section 01's outline. Additions are marked "Added": what the outline leaves to the
// implementation (private state, the handlers' table, check options). Two outline names changed because
// they do not compile: RuleKind::requires (a C++20 keyword) and core::Outcome<void> (see core::Outcome<void>).
//
// The language (source, lexer, variables, command table, binder, planner and check) lives in translation
// units that call no step module, so test_batch links before the step modules do. The step modules'
// types appear here only as forward declarations; the handlers include their headers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "fusion/core/core.hpp"
#include "fusion/core/deps.hpp"
#include "fusion/run/run.hpp"

namespace fusion::network { class Network; struct Maps; }
namespace fusion::expression { class Observations; }
namespace fusion::corridor { class Corridor; }
namespace fusion::enrichment { struct Shortlist; }
namespace fusion::solve { struct Results; }

namespace fusion::batch {

// --- source and lexing (S1.01, S1.05, S1.06, S1.14) ---
struct SourceFile {
  std::filesystem::path path;            // absolute
  core::Sha256 sha256;                   // of the bytes read
  std::vector<std::string> lines;        // split on '\n', one '\r' dropped, BOM skipped
  std::optional<int> invalid_utf8_line;  // Added: the first line with an invalid byte sequence (01.C19)
};
SourceFile read_source(const std::filesystem::path& file);    // throws core::UsageError (exit 2)
SourceFile source_from_text(std::filesystem::path path, std::string_view bytes);   // Added: the same rules
                                                                                    // on bytes in memory

struct Token {
  std::string text;                      // quotes removed, '$' not yet expanded
  bool quoted = false;                   // some part was inside "..."
  std::optional<std::size_t> name_end;   // unquoted '=' after [a-z][a-z0-9_]*
  int column = 0;
  std::optional<std::size_t> key_end;    // Added: unquoted '=' after [a-z][a-z0-9_.]*, for run-line keys
};
enum class LineKind { blank, comment, command };
struct Line {
  int number = 0;                        // 1-based
  LineKind kind = LineKind::blank;
  std::vector<Token> tokens;             // all tokens; unused once the expanded first token is "exec"
  std::string rest;                      // the text after the first token, as written; used for exec
  std::optional<int> unterminated_quote; // column; an error unless the line turns out to be exec
};
Line lex(int number, std::string_view text);   // knows no variables: exec is decided after expansion

// --- variables (S1.07 to S1.09) ---
class Variables {
 public:
  Variables();                           // Added: the trie's root lives in the .cpp
  ~Variables();
  Variables(Variables&&) noexcept;
  Variables& operator=(Variables&&) noexcept;
  bool define(std::string key, std::string value, int line);  // false: already defined
  enum class Mode { argument, exec };   // exec: "$$" kept for the shell, no warnings
  struct Expansion { std::string text; std::vector<std::string> unmatched; };
  Expansion expand(std::string_view text, Mode mode) const;   // longest match, one pass
  std::vector<std::tuple<std::string, std::string, int>> in_order() const;  // for run.json
  std::optional<int> defined_at(std::string_view key) const;   // Added: the line of the definition
 private:
  struct TrieNode;
  std::unique_ptr<TrieNode> root_;
  std::vector<std::tuple<std::string, std::string, int>> order_;   // Added
};
bool valid_variable_key(std::string_view key) noexcept;   // Added: '$' [A-Za-z_] [A-Za-z0-9_]* (S1.08)
inline constexpr std::string_view reserved_prefix = "$FUSION_";   // Added: 01.C18

// --- the command table and binding ---
enum class ValueKind { maps, network, corridor, observations, shortlist, results };
std::string_view kind_name(ValueKind) noexcept;   // Added: "network", "corridor", ...

struct ArgSpec {
  std::string_view name;                 // the bare name on a command line, snake_case
  std::string_view setting;              // registry key "<module>.<name>": its core::SettingSpec
                                         // gives the core::Type, need, default, range and help (row 1.16)
  bool list = false;                     // several values: commas when named, -1 slots when positional
  std::optional<ValueKind> takes = std::nullopt;   // names an in-memory value; the latest by default (01.C16)
  bool positional_only = false;          // Added: a FuSION 1 slot with no name (start's exception file,
                                         // pathz3's mode, pex's files); never bound by name
};
enum class SlotKind { one, list, ignored, optional_tail,
                      ignored_list };    // Added: tokens up to "-1", dropped with a warning (mff's maps)
struct Slot {
  SlotKind kind;
  std::string_view arg;                  // empty for ignored slots
  std::string_view none_token;           // "0" or "-1" where FuSION 1 used one
};
struct PositionalForm {
  std::string_view origin;               // "FuSION 1" or "2.0"
  std::span<const Slot> slots;
  bool (*fits)(std::span<const std::string> tokens);   // e.g. mff: tokens[2] == "xml"
  std::string_view warning;              // printed when this form is used
};
struct Effects {
  std::span<const ValueKind> needs = {};   // must exist, or be loaded, before the line
  std::optional<ValueKind> produces = std::nullopt;
  bool creates_graph = false;            // takes the next graph id
  bool needs_run_dir = true;             // false: allowed in --query
};
enum class RuleKind { less, less_or_equal, requires_, excludes };   // "requires" is a C++20 keyword
struct Rule { RuleKind kind; std::string_view a, b; };   // e.g. less(down, up), S3.07

struct BoundArgs;  struct Workspace;  class CheckContext;

struct CommandSpec {
  std::string_view name = {};
  std::span<const std::string_view> aliases = {};
  std::string_view owner = {};           // C1 module
  std::string_view summary = {};
  std::span<const ArgSpec> args = {};
  std::span<const PositionalForm> forms = {};
  Effects effects = {};
  std::span<const Rule> rules = {};               // enforced, and exported by --commands
  std::span<const core::deps::Requirement> tools = {};   // 11's tool needs (rows 2A.1, 2B.15); a checker
                                                  // adds a need that depends on arguments
  core::Outcome<void> (*run)(const BoundArgs&, Workspace&, core::Context&) = nullptr;   // a Report skips
                                                                            // the point's later lines.
                                                  // Null in the language's table: the interpreter takes
                                                  // each handler from Handlers (below), so the language
                                                  // links without the step modules
  void (*check)(const BoundArgs&, CheckContext&) = nullptr;
  std::string_view dropped = {};         // non-empty: dropped in 2.0 (upfx, S2.29); the error's reason
  std::span<const std::string_view> run_settings = {};   // Added: run-level keys the step reads (network.indirect)
  bool step = true;                      // Added: false for let, sweep, run and exit, which make no step
};
class CommandTable {
 public:
  explicit CommandTable(std::span<const CommandSpec> specs);   // Added
  const CommandSpec* find(std::string_view name_or_alias) const;  // exact (S1.11)
  std::span<const CommandSpec> all() const;
  void print_listing(std::ostream&) const;                       // help, -h, unknown command
  void print_command(std::ostream&, const CommandSpec&) const;   // -h <command>
  nlohmann::json describe() const;      // --commands: arguments with registry defaults, rules,
                                        // tools and run-level settings
 private:
  std::span<const CommandSpec> specs_;
};
const CommandTable& command_table();

// Added: the settings registry that the table points into (row 1.16). Each argument's key names its
// owner's module; the specs sit here, beside the table, until the modules export their own tables.
std::span<const core::SettingSpec> registry();
const core::SettingSpec* find_setting(std::string_view key) noexcept;   // "<module>.<name>"
std::span<const std::string_view> run_level_keys();                     // what a run line may set by key

struct BoundValue {
  const ArgSpec* spec = nullptr;
  core::Source source;                   // default, positional or named (E1)
  bool given = false;                    // false: left out, or given empty
  std::string text;                      // after expansion, for the echo and run.json
  core::Value value;
  int column = 0;                        // Added: where it was written, for diagnostics
  bool has_value = false;                // Added: false for an optional argument left out (0021)
};
struct BoundArgs {
  const CommandSpec* command = nullptr;
  int line = 0;
  std::vector<BoundValue> values;        // one per ArgSpec, in table order
  const BoundValue& at(std::string_view name) const;
  const PositionalForm* form = nullptr;  // Added: the positional form used; null for named-only lines
  bool positional() const noexcept { return form != nullptr; }   // Added
  std::string normalised() const;        // Added: the named form for run.json (row 1.14)
};
BoundArgs bind(const CommandSpec&, const Line&, const Variables&, core::CheckReport&);
BoundArgs bind(const CommandSpec&, const Line&, const Variables&, core::CheckReport&,
               const std::filesystem::path& base);   // Added: relative input paths resolve here (01.C2)
std::optional<core::Value> parse_value(const core::SettingSpec&, std::string_view text, core::Source,
                                       std::string& error);   // Added: one token of its type
std::string value_text(const core::Value&);   // Added: canonical text, for messages and the named form
std::string quote_value(std::string_view);    // Added: 01.C2's quoting, for the named form

// --- planning, checking and running ---
struct Point {
  int index = 1;                         // 1-based, product order
  std::string label;                     // "fdr0.25"; empty without a sweep
  std::string dir;                       // "points/01-fdr0.25"; empty without a sweep
  std::vector<std::pair<std::string, std::string>> values;   // "$FDR" -> "0.25"
};
struct Program {
  SourceFile source;
  std::vector<Line> lines;
  std::vector<Point> points;             // one unlabelled point without sweep lines
};
Program parse(SourceFile, core::CheckReport&);   // 09's report type; diagnostics per row 1.9

struct BatchFacts {                      // the look-ahead (Choice 01.C17)
  std::vector<std::string> sources, targets, fscs;
  std::vector<std::filesystem::path> confirmed_up, confirmed_down;   // 03.C2
};
class CheckContext {
 public:
  void input(const BoundValue& path);            // a run::InputSpec; made_by_line when produced (01.C12)
  void output(const std::filesystem::path&);     // later lines may read it; clashes are 09's
  void require(core::deps::Requirement);         // e.g. dot for pex static=svg (07)
  void feature(std::string_view name, bool on, std::string_view because);   // 0021
  const BatchFacts& facts() const;
  core::CheckReport& report();
  // Added: what a checker needs beyond the outline.
  struct State;                                  // the simulation of one point (src/check_state.hpp)
  CheckContext(State& state, const BoundArgs& args) : state_(state), args_(args) {}
  void error(std::string_view code, std::string message, std::string_view setting = {}, std::string_view path = {});
  void warning(std::string_view code, std::string message, std::string_view setting = {},
               std::string_view path = {});
  void note(std::string_view code, std::string message, std::string_view setting = {}, std::string_view path = {});
  State& state() noexcept { return state_; }
  const BoundArgs& args() const noexcept { return args_; }
 private:
  State& state_;
  const BoundArgs& args_;
};

struct PlannedLine {                             // Added: one line of one point, ready to run
  int line = 0;
  enum class Kind { skip, step, exit } kind = Kind::skip;
  std::string echo;                              // the expanded tokens, each followed by one space (S1.10)
  bool exec = false;
  std::string exec_text;                         // exec: the rest of the line, expanded (S1.14)
  BoundArgs args;                                // command lines
  std::size_t step = 0;                          // index into Plan::steps[point]
};
struct CheckOptions {                            // Added
  bool inputs = true;                            // stat every input (the corpus test turns this off)
  bool tools = true;                             // probe the tools the lines need
  std::string run_dir;                           // $FUSION_RUN_DIR; empty: "<run dir>" in the check
  std::string home;                              // $FUSION_HOME
  std::vector<std::pair<std::string, std::string>> settings;   // fusion --set key=value (row 2B.2)
  std::vector<int> only_points;
};
struct RunLine {                                 // Added: the run line's values (01.C7, row 2B.2)
  int line = 0;
  std::optional<std::string> root, label, log;
  std::vector<std::pair<std::string, std::string>> settings;   // keys, as written
};
struct Plan {
  Program program;
  std::vector<BatchFacts> facts;                 // one per point
  std::vector<std::vector<run::StepSpec>> steps; // one list per point: 09's plan (ids, lines,
                                                 // normalised lines, settings, inputs)
  core::CheckReport report;                      // 01's diagnostics; 09 adds the input checks
  // Added
  std::vector<std::vector<PlannedLine>> lines;   // one list per point
  std::vector<std::vector<std::unique_ptr<core::Settings>>> settings;   // what steps[p][i].settings points at
  std::vector<std::vector<std::tuple<std::string, std::string, int>>> variables;   // per point, for run.json
  RunLine run_line;
  std::vector<core::Setting> run_settings;       // run-level settings with their sources (E1)
  std::vector<core::deps::Probe> tools;          // the check's probes of the tools the lines need (11)
};
Plan check(Program, std::span<const int> only_points = {});   // tools through core::deps::Registry (11)
Plan check(Program, const CheckOptions&);                      // Added
void print_diagnostics(std::ostream& err, const core::CheckReport&, std::string_view batch_name);   // Added:
                                                 // "error: egfr.batch:160: ... [input.missing]" (09.C14)

using GraphEntry = std::variant<std::shared_ptr<const network::Network>,
                                std::shared_ptr<const corridor::Corridor>>;
class GraphStore {
 public:
  int add(GraphEntry entry);             // next id
  const GraphEntry& get(int id) const;   // 0 = latest (S2.01); throws core::Failure
  int latest() const;                    // #LASTGRAPHNUM (S1.09); 0 when there is none
  std::size_t size() const noexcept { return entries_.size(); }   // Added
 private:
  std::vector<GraphEntry> entries_;
};
using Remembered = std::variant<std::shared_ptr<const network::Network>,
                                std::shared_ptr<const expression::Observations>,
                                std::shared_ptr<const enrichment::Shortlist>>;
struct FileIdentity {                    // Added: st_dev and st_ino (01's platform notes)
  std::uint64_t device = 0, inode = 0;
  auto operator<=>(const FileIdentity&) const = default;
};
std::optional<FileIdentity> file_identity(const std::filesystem::path&);   // Added
class WrittenFiles {                     // Choice 01.C13
 public:
  std::optional<Remembered> get(const std::filesystem::path&) const;   // same identity and SHA-256
  void put(const std::filesystem::path&, Remembered value);
 private:
  struct Entry { FileIdentity id; core::Sha256 sha256; Remembered value; };
  std::vector<Entry> entries_;           // Added
};
struct StepState;                        // Added: what the handlers keep between lines (src/handlers/)
struct Workspace {
  std::shared_ptr<const network::Maps> maps;                     // start, or the shipped maps
  GraphStore graphs;                                             // networks and corridors
  std::shared_ptr<const expression::Observations> observations;  // cudf
  std::shared_ptr<const enrichment::Shortlist> shortlist;        // gsea (08's type)
  std::map<std::string, std::shared_ptr<const solve::Results>> solves;   // by solve name or prefix;
                                                 // each handed to results::publish_solve (row 1.5)
  BatchFacts facts;                              // this point's look-ahead
  WrittenFiles written;
  // Added
  std::filesystem::path batch_dir;               // relative inputs and exec lines (01.C2)
  std::filesystem::path home;                    // $FUSION_HOME: the bundled files and tools (S1.20)
  std::string latest_solve;                      // the name of the latest solve (01.C16)
  bool debug = false;                            // --debug (01.C8)
  std::shared_ptr<StepState> state;              // created by the handlers on first use
  core::Json details = core::Json::object();     // what a handler adds to its step's run.json entry
                                                 // (09's details): the interpreter records it, then clears it
};

// Added: the handlers' side of the table (01.C9). The interpreter calls a command's handler through it, so
// the language never names a step module. step_handlers() lives with the handlers and calls the modules.
using Handler = core::Outcome<void> (*)(const BoundArgs&, Workspace&, core::Context&);
struct LegacyMove;
struct Handlers {
  std::span<const std::pair<std::string_view, Handler>> commands;   // by canonical name
  void (*legacy_move)(const LegacyMove&, const Workspace&, core::Context&) = nullptr;
  void (*finish)(Workspace&, run::Run&) = nullptr;   // the run's end: bindings.tsv (02)
  void (*start)(run::Run&, const Plan&) = nullptr;   // the run's start: run.json's versions and tools (11)
  Handler find(std::string_view command) const;
};
const Handlers& step_handlers();

class Interpreter {
 public:
  Interpreter(const Plan&, int point, run::Run&);   // run.log(), events() and stop() are 09's
  Interpreter(const Plan&, int point, run::Run&, const Handlers&);   // Added: tests pass their own
  run::RunStatus run();                  // 09's statuses: ok, failed, stopped
  const std::vector<core::Report>& reports() const noexcept { return reports_; }   // Added
  const Workspace& workspace() const noexcept { return workspace_; }               // Added
  void set_home(const std::string& home) { workspace_.home = core::path_from_utf8(home); }   // Added
 private:
  core::Outcome<void> run_exec(const PlannedLine&, core::Context&);   // Added: S1.14, 01.C11, 01.C20
  const Plan& plan_;
  int point_;
  run::Run& run_;
  const Handlers& handlers_;
  Workspace workspace_;
  std::vector<core::Report> reports_;
};
struct RunOptions {                      // becomes 09's run::Options; jobs and points stay here
  std::optional<std::filesystem::path> root, run_dir, parent;   // run_dir: adopt; parent: --resume
  std::optional<std::string> label, log;
  std::map<std::string, std::string> tags;                      // --tag, 09's labels
  std::vector<std::pair<std::string, std::string>> settings;    // --set (row 2B.2)
  bool debug = false;
  unsigned jobs = 0;                     // the coordinator's core budget; 0 = auto
  std::vector<std::string> points;       // --point
  std::vector<std::string> command_line;
  std::function<void(int)> on_grace_expired;   // set by the executable: std::_Exit(3) (09)
  std::vector<std::shared_ptr<core::LogSink>> sinks;   // Added: the console (09's Options::sinks)
  std::string home;                      // Added: $FUSION_HOME, with '/' (01.C18)
};
run::Options run_options(const Plan&, const RunOptions&);   // Added: the label, root and log (01.C7)
int run_batch(const Plan&, const RunOptions&, core::Log&);   // returns the exit status
int run_batch(const Plan&, const RunOptions&, core::Log&, const Handlers&);   // Added
int run_query(std::string_view line, std::ostream& json_out, core::Log& stderr_log);   // --query

// --- exec, FuSION 1's moves, and the executable ---
struct ShellResult { int status = 0; bool stopped = false; };
class Shell {
 public:
  static std::optional<Shell> find(const std::filesystem::path& home);   // /bin/sh; busybox on Windows
  std::string describe() const;          // path and version, for run.json
  ShellResult run(std::string_view text, const std::filesystem::path& cwd,
                  core::Log& log,        // output to a file (ProcessSpec::Output::file), then
                                         // every line through the log (row 1.15)
                  const core::StopToken& stop) const;   // 11's core::Process in its own group;
                                         // stdin null; env_set PATH with the bundled tools
                                         // directory appended (11.C20)
 private:
  std::filesystem::path path_;           // Added
  std::filesystem::path tools_dir_;      // Added: appended to PATH
};
bool is_window_plot(std::string_view exec_text);   // gnuplot -p without set term (Choice 01.C20)
bool display_available();                // DISPLAY or WAYLAND_DISPLAY on Linux; not SSH on macOS

enum class LegacyName { po_dat, limits_timefile, edges_merged, fb_rch, fb_rch_back, mylog };
struct LegacyMove { LegacyName name; std::filesystem::path dest; };
std::optional<LegacyMove> match_legacy_move(std::string_view exec_text);   // Choice 01.C11
void perform_legacy_move(const LegacyMove&, const Workspace&, core::Context&);   // PO.dat through 07's
                                                    // two_column, limits_timefile.txt through its legacy_limits
std::string_view legacy_file_name(LegacyName) noexcept;   // Added: "PO.dat", ...

}  // namespace fusion::batch
