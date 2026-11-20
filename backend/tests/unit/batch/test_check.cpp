// The check of the whole batch before anything runs (decision 0021, 01.C12, 01.C16, 01.C17), the plan it
// builds (09's StepSpecs), and section 12's batch-corpus test on booking 1's frozen copy.
#include <algorithm>
#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "fusion/batch/batch.hpp"

using namespace fusion;

namespace {

const std::filesystem::path kBatch = "/data/exp/egfr.batch";

batch::Plan plan_of(std::string_view text, batch::CheckOptions options = {}) {
  core::CheckReport report;
  auto program = batch::parse(batch::source_from_text(kBatch, text), report);
  if (options.run_dir.empty()) options.run_dir = "/runs/r1";
  return batch::check(std::move(program), options);
}
batch::Plan plan_no_files(std::string_view text) {
  batch::CheckOptions options;
  options.inputs = false;
  options.tools = false;
  return plan_of(text, options);
}

std::vector<std::string> codes(const batch::Plan& plan, core::Severity severity) {
  std::vector<std::string> out;
  for (const auto& d : plan.report.diagnostics)
    if (d.severity == severity) out.push_back(d.code);
  return out;
}
std::vector<std::string> errors(const batch::Plan& plan) { return codes(plan, core::Severity::error); }
bool has(const batch::Plan& plan, std::string_view code, int line = 0) {
  return std::any_of(plan.report.diagnostics.begin(), plan.report.diagnostics.end(), [&](const core::Diagnostic& d) {
    return d.code == code && (line == 0 || d.line == static_cast<std::uint32_t>(line));
  });
}
std::string all_messages(const batch::Plan& plan) {
  std::ostringstream out;
  batch::print_diagnostics(out, plan.report, "egfr.batch");
  return out.str();
}

// A FuSION 1 batch shaped like the real one (S1.07's longest match, #LASTGRAPHNUM, positional lines).
constexpr std::string_view kFusion1 = R"(;; the real batch's structure, on made-up paths
let $WORK_DIR /w
let $WRITE_DIR $WORK_DIR/run
let $LFC /in/lfc.tsv
let $UP_REG_FILE $WRITE_DIR/up_reg
let $DOWN_REG_FILE $WRITE_DIR/down_reg
let $EXCEPTION_FILE $WRITE_DIR/exception_file.txt
let $SRC_NODE hsa1
let $TGT_NODE hsa22
let $CONNECT $WRITE_DIR/connect_pairs
let $EMPTY $WRITE_DIR/empty_file
let $LIST /in/list.txt
let $SHORTLIST $WRITE_DIR/gsea_kgml_files.txt
let $MERGED $WRITE_DIR/merged_master_graph.xml
let $PREFIX_W $WRITE_DIR/temp_w
let $PREFIX_WO $WRITE_DIR/temp_wo
exec mkdir -p $WRITE_DIR
cudf $LFC 1.01 -1.01 $UP_REG_FILE $DOWN_REG_FILE
exec cd $WRITE_DIR; echo $SRC_NODE > src_file; echo $TGT_NODE > tgt_file
exec echo $SRC_NODE $TGT_NODE > $CONNECT
exec cat $UP_REG_FILE $DOWN_REG_FILE > $EXCEPTION_FILE
exec touch $EMPTY
start $EMPTY $EXCEPTION_FILE /in/symbols.txt /in/names.txt
gsea $LFC $LIST fdr:0.25 $SHORTLIST
mff $SHORTLIST xml 1 n $EMPTY -1
size 0
wgx 0 $MERGED KEGG
exec mv edges_merged_filename $WRITE_DIR/merged_edges_file
rgx $MERGED y 1
let $MERGED_GRAPH_NUM #LASTGRAPHNUM
fb_rch 0 $SRC_NODE -1 $TGT_NODE -1 -1 10 0
pathz3 0 0 $CONNECT 0 0 8 8 10 $PREFIX_W $UP_REG_FILE $DOWN_REG_FILE $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $LFC 1 1 3600000 14400000
exec mv PO.dat $WRITE_DIR/po_w.dat
fb_rch $MERGED_GRAPH_NUM $SRC_NODE -1 $TGT_NODE -1 -1 10 0
pathz3 0 0 $CONNECT 0 0 8 8 10 $PREFIX_WO $UP_REG_FILE $DOWN_REG_FILE $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $EMPTY $LFC 1 1 3600000 14400000
pex $PREFIX_W_solutions.txt $PREFIX_W_relaxations.txt $PREFIX_WO_solutions.txt $PREFIX_WO_relaxations.txt $LFC /in/names.txt $SRC_NODE $TGT_NODE both all $WRITE_DIR/pex
exit
)";

// The same pipeline in 2.0's named form (S8.25, 01's "The frontend's batch files").
constexpr std::string_view kNamed = R"(let $IN /in
cudf values=$IN/lfc.tsv up=1.01 down=-1.01
gsea values=$IN/lfc.tsv candidates=$IN/list.txt selector=fdr:0.25 shortlist=gsea_kgml_files.txt
mff split_threshold=1 not_merged=$IN/not_merged.txt
fb_rch source=hsa1 target=hsa22 bound=10
pathz3 fsc=hsa11 node_max=8 edge_max=8 timeout=4h
plot_pareto
pex
)";

}  // namespace

TEST_CASE("batch: a FuSION 1 batch binds with one warning and the no-FSC note", "[batch][check]") {
  const auto plan = plan_no_files(kFusion1);
  INFO(all_messages(plan));
  CHECK(errors(plan).empty());
  CHECK(codes(plan, core::Severity::warning) == std::vector<std::string>{"batch.ignored_tokens"});   // mff's xml
  CHECK(codes(plan, core::Severity::note) == std::vector<std::string>{"batch.no_fsc"});              // 2A.9
  // Graph ids: mff makes 1, rgx 2, the corridors 3 and 4 (S2.01; FuSION 1 gave 84 for the merge).
  const auto& vars = plan.variables[0];
  const auto merged = std::find_if(vars.begin(), vars.end(), [](const auto& v) { return std::get<0>(v) == "$MERGED_GRAPH_NUM"; });
  REQUIRE(merged != vars.end());
  CHECK(std::get<1>(*merged) == "2");
  // Step ids follow the canonical names (09.C2): sz, not size.
  std::vector<std::string> ids;
  for (const auto& s : plan.steps[0]) ids.push_back(s.id);
  CHECK(ids == std::vector<std::string>{"exec", "cudf", "exec-2", "exec-3", "exec-4", "exec-5", "start", "gsea",
                                        "mff", "sz", "wgx", "exec-6", "rgx", "fb_rch", "pathz3", "exec-7",
                                        "fb_rch-2", "pathz3-2", "pex"});
}

TEST_CASE("batch: the plan: inputs the batch makes, echoes and normalised lines (01.C12, S1.10)", "[batch][check]") {
  const auto plan = plan_no_files(kFusion1);
  const auto& steps = plan.steps[0];
  const auto find = [&](std::string_view id) {
    return *std::find_if(steps.begin(), steps.end(), [&](const run::StepSpec& s) { return s.id == id; });
  };
  // start's exception file is made by line 21's exec cat; the symbol maps are not made by the batch.
  const auto start = find("start");
  REQUIRE(start.inputs.size() == 4);
  CHECK(start.inputs[1].argument == "exception_file");
  CHECK(start.inputs[1].made_by_line == std::optional<std::uint32_t>(21));
  CHECK(start.inputs[0].made_by_line == std::optional<std::uint32_t>(22));   // touch $EMPTY
  CHECK_FALSE(start.inputs[2].made_by_line);
  // rgx reads the file wgx declared (01.C13); pathz3's up list is cudf's absolute output.
  CHECK(find("rgx").inputs.at(0).made_by_line == std::optional<std::uint32_t>(27));
  const auto z3 = find("pathz3");
  const auto up = std::find_if(z3.inputs.begin(), z3.inputs.end(), [](const run::InputSpec& i) { return i.argument == "up"; });
  REQUIRE(up != z3.inputs.end());
  CHECK(up->made_by_line == std::optional<std::uint32_t>(18));
  CHECK(find("cudf").normalised ==
        "cudf values=/in/lfc.tsv up=1.01 down=-1.01 up_out=/w/run/up_reg down_out=/w/run/down_reg");
  CHECK(find("exec-2").normalised == "exec cd /w/run; echo hsa1 > src_file; echo hsa22 > tgt_file");
  // The echo: each expanded token followed by one space (S1.10).
  const auto& lines = plan.lines[0];
  const auto cudf = std::find_if(lines.begin(), lines.end(), [](const batch::PlannedLine& l) { return l.line == 18; });
  REQUIRE(cudf != lines.end());
  CHECK(cudf->echo == "cudf /in/lfc.tsv 1.01 -1.01 /w/run/up_reg /w/run/down_reg ");
  const auto size = std::find_if(lines.begin(), lines.end(), [](const batch::PlannedLine& l) { return l.line == 26; });
  CHECK(size->echo == "size 0 ");
  // Settings carry their sources (E1).
  const auto* settings = find("fb_rch").settings;
  REQUIRE(settings);
  CHECK(settings->at("bound").source == core::Source::positional);
  CHECK(std::get<std::int64_t>(*settings->at("graph").value) == 2);   // 0 is the latest graph, rgx's
  CHECK(settings->at("skip_subtypes").source == core::Source::default_value);
  CHECK(std::get<std::int64_t>(*find("fb_rch-2").settings->at("graph").value) == 2);
  CHECK(std::get<std::int64_t>(*find("pathz3-2").settings->at("graph").value) == 4);
}

TEST_CASE("batch: the named form binds, and takes the latest value of each kind (01.C16)", "[batch][check]") {
  const auto plan = plan_no_files(kNamed);
  INFO(all_messages(plan));
  CHECK(errors(plan).empty());
  CHECK_FALSE(has(plan, "batch.no_fsc"));   // pathz3 names fsc=, which the look-ahead gives mff (01.C17)
  REQUIRE(plan.facts.size() == 1);
  CHECK(plan.facts[0].fscs == std::vector<std::string>{"hsa11"});
  CHECK(plan.facts[0].sources == std::vector<std::string>{"hsa1"});
  CHECK(plan.facts[0].targets == std::vector<std::string>{"hsa22"});
  const auto& steps = plan.steps[0];
  CHECK(std::get<std::int64_t>(*steps[3].settings->at("graph").value) == 1);   // fb_rch on mff's network
  CHECK(std::get<std::int64_t>(*steps[4].settings->at("graph").value) == 2);   // pathz3 on the corridor
  CHECK(steps[4].settings->at("timeout").source == core::Source::named);
  CHECK(std::get<std::int64_t>(*steps[4].settings->at("timeout").value) == 14'400'000);
  // mff before any gsea has no list (01.C16); pathz3 before any corridor has nothing to solve.
  CHECK(has(plan_no_files("mff split_threshold=1\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("cudf values=x up=1 down=-1\npathz3 fsc=a\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("pex\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("sz 0\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("rgx x.xml y 1\nsz 3\n"), "batch.bad_argument"));
}

TEST_CASE("batch: let: literal keys, redefinitions, reserved names and #LASTGRAPHNUM (S1.08, S1.09)", "[batch][check]") {
  CHECK(has(plan_no_files("let $X 1\nlet $X 2\nexit\n"), "batch.redefined", 2));
  CHECK(all_messages(plan_no_files("let $X 1\nlet $X 2\n")).find("already defined at line 1") != std::string::npos);
  CHECK(has(plan_no_files("let X 1\n"), "batch.bad_value"));
  CHECK(has(plan_no_files("let $FUSION_HOME /x\n"), "batch.bad_value"));
  CHECK(has(plan_no_files("let $X a b\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("let\n"), "batch.bad_argument"));
  CHECK(has(plan_no_files("let $G #LASTGRAPHNUM\n"), "batch.bad_value"));
  // The key is literal: with $X defined, "let $X_Y" defines $X_Y, which FuSION 1 expanded first.
  const auto literal = plan_no_files("let $X 1\nlet $X_Y 2\nsz $X_Y\n");
  CHECK_FALSE(has(literal, "batch.redefined"));
  // A missing value is empty; an undefined $NAME warns outside exec, not inside (S1.07).
  const auto warn = plan_no_files("let $E\nexec echo $NOPE $E\nhelp $NOPE\n");
  CHECK(codes(warn, core::Severity::warning) == std::vector<std::string>{"batch.undefined_variable"});
}

TEST_CASE("batch: commands: unknown, dropped, exit and the end of the file (S1.11, S1.13, S2.29)", "[batch][check]") {
  const auto unknown = plan_no_files("frobnicate 1\n");
  CHECK(has(unknown, "batch.unknown_command", 1));
  CHECK(all_messages(unknown).find("the commands are help, let") != std::string::npos);
  const auto upfx = plan_no_files("unique_prefix x\n");
  CHECK(has(upfx, "batch.unknown_command"));
  CHECK(all_messages(upfx).find("upfx was dropped in FuSION 2.0") != std::string::npos);
  CHECK(has(plan_no_files("Exit\n"), "batch.unknown_command"));   // exact and case-sensitive (S1.11)
  // A variable may name the command: every token is expanded before dispatch (S1.07).
  CHECK(errors(plan_no_files("let $CMD help\n$CMD\n")).empty());
  // exit: its extra tokens and the lines after it are ignored with a warning (S1.13).
  const auto exit = plan_no_files("exit now\nfrobnicate\n");
  CHECK(codes(exit, core::Severity::warning) == std::vector<std::string>{"batch.ignored_tokens", "batch.ignored_tokens"});
  CHECK(errors(exit).empty());
  CHECK(exit.lines[0].back().kind == batch::PlannedLine::Kind::skip);
  CHECK(has(plan_no_files("cudf values=\"x up=1 down=-1\n"), "batch.bad_value"));   // unterminated quote
  CHECK(errors(plan_no_files("exec echo \"unterminated\n")).empty());                 // exec bypasses quoting
  CHECK(has(plan_no_files("sweep $X 1 2\n"), "batch.unknown_command"));
  CHECK(has(plan_no_files("labels\n"), "batch.unknown_command"));                     // --query only (2B.6)
}

TEST_CASE("batch: cross-argument rules and value checks (S3.07, S5.28, 05.C7, 06.C10)", "[batch][check]") {
  const auto equal = plan_no_files("cudf values=x up=0.0 down=0.0\n");
  CHECK(has(equal, "batch.bad_value"));
  CHECK(all_messages(equal).find("down (0.0) must be below up (0.0)") != std::string::npos);
  CHECK(errors(plan_no_files("cudf values=x up=1 down=-1\n")).empty());
  CHECK(has(plan_no_files("sth values=x current_up=1\n"), "batch.bad_argument"));
  const std::string head = "rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 10 0\n";
  const std::string z3 = "pathz3 0 0 p 0 0 8 8 10 /w/t up down e e e e e e e e e e e e lfc 1 1 1000 1000\n";
  CHECK(errors(plan_no_files(head + z3)).empty());
  CHECK(has(plan_no_files(head + "pathz3 0 1 p 0 0 8 8 10 /w/t up down e e e e e e e e e e e e lfc 1 1 1000 1000\n"),
            "batch.bad_value"));   // mode 1 (S5.28)
  const auto bound = plan_no_files(head + "pathz3 0 0 p 0 0 8 8 7 /w/t up down e e e e e e e e e e e e lfc 1 1 1000 1000\n");
  CHECK(all_messages(bound).find("differs from the corridor's bound 10") != std::string::npos);   // 05.C7
  CHECK(has(plan_no_files(head + "pathz3 node_min=5 node_max=2\n"), "batch.bad_value"));
  CHECK(has(plan_no_files(head + "pathz3 cases=in\n"), "batch.bad_argument"));   // in needs an FSC
  CHECK(errors(plan_no_files(head + "pathz3 cases=free\n")).empty());
  CHECK(has(plan_no_files("rgx g.xml y 1\npathz3 graph=1\n"), "batch.bad_argument"));   // a network, not a corridor
  // pex: positional files bind by prefix (07.C9); src and tgt must be the solve's (07.C12).
  const std::string pex_ok = "pex /w/t_solutions.txt /w/t_relaxations.txt 0 0 lfc names a b incl all /w/pex\n";
  CHECK(errors(plan_no_files(head + z3 + pex_ok)).empty());
  CHECK(has(plan_no_files(head + z3 + "pex /w/x_solutions.txt 0 0 0 lfc names a b incl all /w/pex\n"),
            "batch.unmatched_file"));
  CHECK(has(plan_no_files(head + z3 + "pex /w/t_solutions.txt 0 0 0 lfc names a c incl all /w/pex\n"),
            "batch.bad_value"));
  CHECK(has(plan_no_files(head + z3 + "pex cases=both\n"), "batch.bad_value"));   // positional only (1.6)
  CHECK(has(plan_no_files(head + z3 + "plot_pareto solve=nothing\n"), "batch.bad_argument"));
  CHECK(errors(plan_no_files(head + z3 + "plot_pareto solve=/w/t\n")).empty());
}

TEST_CASE("batch: mff warns when it runs before any cudf (03.C1)", "[batch][check]") {
  const auto plan = plan_no_files("mff l.txt 1 n\n");
  CHECK(has(plan, "batch.mff_before_cudf"));
  CHECK(has(plan, "batch.no_fsc"));
  CHECK_FALSE(has(plan_no_files("cudf values=x up=1 down=-1\nmff l.txt 1 n fsc=hsa1\n"), "batch.mff_before_cudf"));
}

TEST_CASE("batch: the run line, --set and a step's own argument (01.C7, 01.C8, entry 229)", "[batch][check]") {
  const std::string text =
      "run root=/runs label=egfr corridor.skip_subtypes=component network.indirect=keep\n"
      "rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 10 0\nfb_rch 1 a -1 b -1 -1 10 0\n"
      "fb_rch graph=1 source=a target=b bound=3 skip_subtypes=association\n";
  const auto from_run_line = plan_no_files(text);
  INFO(all_messages(from_run_line));
  REQUIRE(errors(from_run_line).empty());
  CHECK(from_run_line.run_line.root == std::optional<std::string>("/runs"));
  CHECK(from_run_line.run_line.label == std::optional<std::string>("egfr"));
  const auto& steps = from_run_line.steps[0];
  CHECK(steps[1].settings->at("skip_subtypes").source == core::Source::run_line);
  CHECK(std::get<std::vector<std::string>>(*steps[1].settings->at("skip_subtypes").value) ==
        std::vector<std::string>{"component"});
  CHECK(steps[3].settings->at("skip_subtypes").source == core::Source::named);
  CHECK(steps[0].settings->at("indirect").source == core::Source::run_line);   // rgx reads network.indirect

  batch::CheckOptions options;
  options.inputs = options.tools = false;
  options.settings = {{"corridor.skip_subtypes", "mapping"}};
  const auto with_set = plan_of(text, options);
  CHECK(with_set.steps[0][1].settings->at("skip_subtypes").source == core::Source::set_option);
  CHECK(with_set.steps[0][3].settings->at("skip_subtypes").source == core::Source::named);
  const auto indirect = std::find_if(with_set.run_settings.begin(), with_set.run_settings.end(),
                                     [](const core::Setting& s) { return s.spec->name == "indirect"; });
  REQUIRE(indirect != with_set.run_settings.end());
  CHECK(indirect->source == core::Source::run_line);

  batch::CheckOptions bad_set;
  bad_set.inputs = bad_set.tools = false;
  bad_set.settings = {{"solve.seed", "3"}};
  CHECK(has(plan_of("sz 0\n", bad_set), "batch.bad_argument"));                   // not run-level
  bad_set.settings = {{"network.indirect", "maybe"}};
  CHECK(has(plan_of("sz 0\n", bad_set), "batch.bad_value"));
  bad_set.settings = {{"run.debug", "true"}};
  CHECK(has(plan_of("sz 0\n", bad_set), "batch.bad_argument"));                   // --debug only
  CHECK(has(plan_no_files("run debug=true\n"), "batch.bad_argument"));            // 01.C8
  CHECK(has(plan_no_files("sz 0\nrun label=x\n"), "batch.bad_argument"));         // before the first step
  CHECK(has(plan_no_files("run solve.seed=3\n"), "batch.bad_argument"));          // not a run-level key
  CHECK(has(plan_no_files("run network.indirect=sometimes\n"), "batch.bad_value"));
  CHECK(has(plan_no_files("run label\n"), "batch.bad_argument"));
}

TEST_CASE("batch: inputs are checked before anything runs (decision 0021)", "[batch][check]") {
  const auto dir = std::filesystem::temp_directory_path() / "fusion_batch_check_inputs";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "sub");
  { std::ofstream(dir / "lfc.tsv") << "hsa1 1.5\n"; }
  { std::ofstream(dir / "list.txt") << "a.xml\n\nhsa2\n# comment\n"; }
  { std::ofstream(dir / "a.xml") << "<pathway/>\n"; }
  core::CheckReport report;
  auto program = batch::parse(batch::source_from_text(dir / "x.batch",
      "cudf values=lfc.tsv up=1 down=-1 not_de=missing.txt\n"
      "cudf values=sub up=1 down=-1\n"
      "gsea values=lfc.tsv candidates=list.txt selector=fdr:0.25 shortlist=s.txt\n"
      "exec touch made.txt\n"
      "cudf values=made.txt up=1 down=-1\n"
      "rgx /nowhere/g.xml y 1\n"), report);
  batch::CheckOptions options;
  options.tools = false;
  const auto plan = batch::check(std::move(program), options);
  INFO(all_messages(plan));
  CHECK(has(plan, "input.missing", 1));       // a named optional input that is missing (0021)
  CHECK(has(plan, "input.not_file", 2));
  CHECK(has(plan, "input.list_member", 3));   // hsa2 names hsa2.xml, which is missing (02.C12)
  CHECK_FALSE(has(plan, "input.missing", 5)); // made by line 4's exec: checked before its step (01.C12)
  CHECK(has(plan, "input.missing", 6));
  CHECK(plan.steps[0][4].inputs.at(0).made_by_line == std::optional<std::uint32_t>(4));
  CHECK(plan.steps[0][0].inputs.at(0).path == dir / "lfc.tsv");   // relative to the batch's directory (01.C2)
  std::filesystem::remove_all(dir);
}

TEST_CASE("batch: outputs that clash (row 2A.7)", "[batch][check]") {
  CHECK(has(plan_no_files("cudf values=x up=1 down=-1 up_out=/w/a down_out=/w/a\n"), "output.clash"));
  CHECK(has(plan_no_files("cudf values=/w/v up=1 down=-1 up_out=/w/v down_out=/w/d\n"), "output.clash"));
  CHECK(has(plan_no_files("rgx g.xml y 1\nwgx 0 /w/o.xml KEGG\nwgx 0 /w/o.xml KEGG\n"), "output.clash"));
  CHECK_FALSE(has(plan_no_files("cudf values=x up=1 down=-1\ncudf values=x up=2 down=-1\n"), "output.clash"));
}

TEST_CASE("batch: features the inputs turn on are reported (decision 0021)", "[batch][check]") {
  const auto plan = plan_no_files(
      "rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 10 0\n"
      "pathz3 fsc=c coexpression_csv=/c.csv coexpression_threshold=700 frozen_threshold=800\n");
  REQUIRE(plan.report.features.size() == 2);
  CHECK(plan.report.features[0].feature == "coexpression");
  CHECK(plan.report.features[0].on);
  CHECK(plan.report.features[0].line == 3);
  CHECK(plan.report.features[1].feature == "frozen");
  const auto off = plan_no_files("rgx g.xml y 1\nfb_rch 0 a -1 b -1 -1 10 0\n"
                                 "pathz3 fsc=c coexpression_csv=/c.csv coexpression_threshold=7 constraints=0\n");
  REQUIRE(off.report.features.size() == 1);
  CHECK_FALSE(off.report.features[0].on);
}

TEST_CASE("batch: diagnostics are sorted, and print in 09.C14's form", "[batch][check]") {
  const auto plan = plan_no_files("exit\n");
  CHECK(errors(plan).empty());
  const auto bad = plan_no_files("bogus\nlet $X 1\nlet $X 2\n");
  REQUIRE(bad.report.diagnostics.size() == 2);
  CHECK(bad.report.diagnostics[0].line == 1);
  CHECK(bad.report.diagnostics[1].line == 3);
  CHECK(all_messages(bad).starts_with("error: egfr.batch:1: unknown command 'bogus'"));
  CHECK(all_messages(bad).find("[batch.redefined]\n") != std::string::npos);
  const auto json = bad.report.to_json();
  CHECK(json["ok"] == false);
  CHECK(json["diagnostics"].size() == 2);
}

// Section 12's batch row: every batch file of booking 1's frozen copy binds with no error, its input
// checks off; output_script.txt fails S3.07's check; advtempscript.txt (placeholders) is excluded.
TEST_CASE("batch: the FuSION 1 batch corpus binds (12, batch row)", "[batch][check][corpus]") {
  const std::filesystem::path root = std::filesystem::path(FUSION_TEST_SOURCE_DIR) / "fixtures" / "fusion1-batches";
  if (!std::filesystem::exists(root)) SKIP("booking 1's frozen corpus is not in this checkout");
  std::vector<std::filesystem::path> files;
  for (const auto& e : std::filesystem::recursive_directory_iterator(root))
    if (e.is_regular_file() && (e.path().extension() == ".batch" || e.path().filename() == "script.txt"))
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  REQUIRE(files.size() >= 100);
  std::size_t mff_warnings = 0, mff_lines = 0;
  for (const auto& file : files) {
    std::ifstream in(file, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    core::CheckReport report;
    batch::CheckOptions options;
    options.inputs = options.tools = false;
    const auto plan = batch::check(batch::parse(batch::source_from_text(file, bytes), report), options);
    INFO(file.string() << "\n" << all_messages(plan));
    CHECK(errors(plan).empty());
    for (const auto& d : plan.report.diagnostics)
      if (d.code == "batch.ignored_tokens" && d.command == std::optional<std::string>("mff")) ++mff_warnings;
    for (const auto& line : plan.lines[0])
      if (line.args.command && line.args.command->name == "mff") ++mff_lines;
  }
  CHECK(mff_warnings == mff_lines);   // one warning per FuSION 1 mff line (S2.16)

  std::ifstream in(root / "Linux" / "output_script.txt", std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  core::CheckReport report;
  batch::CheckOptions options;
  options.inputs = options.tools = false;
  const auto gui = batch::check(batch::parse(batch::source_from_text(root / "Linux" / "output_script.txt", bytes), report),
                                options);
  CHECK(std::any_of(gui.report.diagnostics.begin(), gui.report.diagnostics.end(), [](const core::Diagnostic& d) {
    return d.code == "batch.bad_value" && d.command == std::optional<std::string>("cudf");
  }));
}
