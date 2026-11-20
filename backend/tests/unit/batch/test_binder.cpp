// Section 01's binder hooks: every positional form against its named equivalent, empty tokens, extra
// tokens, a missing -1, and type errors (decision 0018, 01.C1, S3.02, S4.01, S5.02).
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "fusion/batch/batch.hpp"

using namespace fusion;

namespace {

struct Bound {
  batch::BoundArgs args;
  core::CheckReport report;
  bool has(std::string_view code) const {
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [&](const core::Diagnostic& d) { return d.code == code; });
  }
  bool errors() const {
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [](const core::Diagnostic& d) { return d.severity == core::Severity::error; });
  }
  std::string message() const { return report.diagnostics.empty() ? "" : report.diagnostics.front().message; }
};

Bound bind_line(std::string_view text, const batch::Variables& vars = batch::Variables(),
                const std::filesystem::path& base = "/base") {
  Bound b;
  const auto line = batch::lex(5, text);
  const auto* spec = batch::command_table().find(line.tokens.front().text);
  REQUIRE(spec);
  b.args = batch::bind(*spec, line, vars, b.report, base);
  return b;
}

// The values a line binds, as (name, value text, has_value); the source is compared separately.
std::vector<std::tuple<std::string, std::string, bool>> values(const batch::BoundArgs& args) {
  std::vector<std::tuple<std::string, std::string, bool>> out;
  for (const auto& v : args.values)
    out.emplace_back(std::string(v.spec->name), v.has_value ? batch::value_text(v.value) : "", v.has_value);
  return out;
}

}  // namespace

TEST_CASE("batch: cudf: the positional form and the named form bind equal values (0018)", "[batch][binder]") {
  const auto positional = bind_line("cudf lfc.tsv 1.01 -1.01 /w/up_reg /w/down_reg");
  const auto named = bind_line("cudf down=-1.01 values=lfc.tsv up=1.01 down_out=/w/down_reg up_out=/w/up_reg");
  REQUIRE_FALSE(positional.errors());
  REQUIRE_FALSE(named.errors());
  CHECK(values(positional.args) == values(named.args));
  CHECK(positional.args.at("values").source == core::Source::positional);
  CHECK(named.args.at("values").source == core::Source::named);
  CHECK(std::get<std::string>(positional.args.at("values").value) == "/base/lfc.tsv");   // 01.C2
  CHECK(std::get<double>(positional.args.at("down").value) == -1.01);
  CHECK(positional.args.positional());
  CHECK_FALSE(named.args.positional());
  CHECK(named.args.at("repeats").source == core::Source::default_value);
  CHECK(std::get<std::string>(named.args.at("repeats").value) == "classify_each");
}

TEST_CASE("batch: cudf's outputs default to up_reg and down_reg when named (04.C11)", "[batch][binder]") {
  const auto b = bind_line("cudf values=x.tsv up=1 down=-1");
  REQUIRE_FALSE(b.errors());
  CHECK(std::get<std::string>(b.args.at("up_out").value) == "up_reg");
  CHECK_FALSE(b.args.at("up_out").given);
  CHECK(bind_line("cudf x.tsv 1 -1 up").has("batch.bad_argument"));   // positionally all five (S3.01)
}

TEST_CASE("batch: fb_rch: -1 lists, and edges_to_target by number or name (S4.01, 05.C8)", "[batch][binder]") {
  const auto positional = bind_line("fb_rch 0 hsa1956 -1 hsa3576 hsa3577 -1 -1 10 1");
  const auto named = bind_line(
      "fb_rch graph=0 source=hsa1956 target=hsa3576,hsa3577 exclude= bound=10 edges_to_target=activation");
  REQUIRE_FALSE(positional.errors());
  REQUIRE_FALSE(named.errors());
  CHECK(std::get<std::vector<std::string>>(positional.args.at("target").value) ==
        std::vector<std::string>{"hsa3576", "hsa3577"});
  CHECK(std::get<std::string>(positional.args.at("edges_to_target").value) == "activation");
  CHECK(batch::value_text(named.args.at("target").value) == batch::value_text(positional.args.at("target").value));
  CHECK(std::get<std::string>(named.args.at("edges_to_target").value) == "activation");
  CHECK(std::get<std::vector<std::string>>(positional.args.at("exclude").value).empty());
  CHECK(std::get<std::vector<std::string>>(positional.args.at("skip_subtypes").value) ==
        std::vector<std::string>{"component", "association", "dissociation", "mapping"});

  const auto missing = bind_line("fb_rch 0 hsa1956 -1 hsa3576 10 0");
  CHECK(missing.has("batch.bad_argument"));
  CHECK(missing.message().find("-1 terminator") != std::string::npos);
  CHECK(bind_line("fb_rch graph=0 source=a target=b bound=10 edges_to_target=1").has("batch.bad_value"));
}

TEST_CASE("batch: pathz3: FuSION 1's 28 tokens and its 5 optional ones (S5.01, S5.02)", "[batch][binder]") {
  const std::string base28 =
      "pathz3 0 0 pairs 0 0 8 7 10 /w/temp_w up down en an ee ae act inact cu cd rn nrn re nre lfc.tsv 1 3 "
      "3600000 14400000";
  const auto b28 = bind_line(base28);
  REQUIRE_FALSE(b28.errors());
  CHECK(std::get<std::int64_t>(b28.args.at("edge_min").value) == 0);
  CHECK(std::get<std::int64_t>(b28.args.at("node_max").value) == 7);   // FuSION 1: <e_lo> <n_lo> <e_hi> <n_hi>
  CHECK(std::get<std::int64_t>(b28.args.at("edge_max").value) == 8);
  CHECK(std::get<std::string>(b28.args.at("legacy_prefix").value) == "/w/temp_w");
  CHECK(std::get<std::int64_t>(b28.args.at("solutions").value) == 3);          // explore (06.C11)
  CHECK(std::get<std::int64_t>(b28.args.at("incremental_timeout").value) == 3'600'000);   // bare ms
  CHECK(std::get<std::int64_t>(b28.args.at("timeout").value) == 14'400'000);
  CHECK_FALSE(b28.args.at("coexpression_csv").given);
  CHECK(std::get<std::int64_t>(b28.args.at("seed").value) == 42);

  const auto b33 = bind_line(base28 + " coexp.csv 700 800 1 7");
  REQUIRE_FALSE(b33.errors());
  CHECK(std::get<double>(b33.args.at("coexpression_threshold").value) == 700);
  CHECK(std::get<bool>(b33.args.at("constraints").value));
  CHECK(std::get<std::int64_t>(b33.args.at("seed").value) == 7);

  // S5.02: an empty token from an empty variable keeps only that slot's default.
  batch::Variables vars;
  vars.define("$EMPTY", "", 1);
  const auto empties = bind_line(base28 + " $EMPTY $EMPTY 800", vars);
  REQUIRE_FALSE(empties.errors());
  CHECK_FALSE(empties.args.at("coexpression_csv").given);
  CHECK_FALSE(empties.args.at("coexpression_threshold").given);
  CHECK(std::get<double>(empties.args.at("frozen_threshold").value) == 800);

  CHECK(bind_line(base28 + " a 1 2 0 42 extra").has("batch.bad_argument"));
  CHECK(bind_line("pathz3 0 0 pairs").has("batch.bad_argument"));
}

TEST_CASE("batch: named durations need a unit; positional slots take milliseconds (01.C1)", "[batch][binder]") {
  const auto ok = bind_line("pathz3 fsc=hsa3725 timeout=4h incremental_timeout=50min budget=0 node_max=7");
  REQUIRE_FALSE(ok.errors());
  CHECK(std::get<std::int64_t>(ok.args.at("timeout").value) == 14'400'000);
  CHECK(std::get<std::int64_t>(ok.args.at("incremental_timeout").value) == 3'000'000);
  CHECK(std::get<std::int64_t>(ok.args.at("budget").value) == 0);
  CHECK(std::get<std::vector<std::string>>(ok.args.at("fsc").value) == std::vector<std::string>{"hsa3725"});
  CHECK(bind_line("pathz3 timeout=3600").has("batch.bad_value"));
  CHECK(bind_line("pathz3 timeout=4 h").has("batch.bad_argument"));
  CHECK(bind_line("pathz3 timeout=4hours").has("batch.bad_value"));
  CHECK(std::get<std::int64_t>(bind_line("pathz3 timeout=90s").args.at("timeout").value) == 90'000);
  CHECK(std::get<std::int64_t>(bind_line("pathz3 timeout=500ms").args.at("timeout").value) == 500);
  CHECK(std::get<std::int64_t>(bind_line("pathz3 timeout=1d").args.at("timeout").value) == 86'400'000);
}

TEST_CASE("batch: type errors: a whole token of its type (S3.02, 04.C2)", "[batch][binder]") {
  for (const char* bad : {"1.01x", "nan", "0x1p0", "1,5", "inf", "1e", "--1"})
    CHECK(bind_line(std::string("cudf x.tsv ") + bad + " -1 u d").has("batch.bad_value"));
  CHECK(bind_line(R"(cudf x.tsv "" -1 u d)").has("batch.bad_argument"));   // empty: not given, but required
  CHECK_FALSE(bind_line("cudf x.tsv +1.5e0 -.5 u d").errors());
  CHECK(bind_line("fb_rch 0 a -1 b -1 -1 1.5 0").has("batch.bad_value"));   // a bound is a whole number
  CHECK(bind_line("fb_rch 0 a -1 b -1 -1 -2 0").has("batch.bad_value"));    // >= 0
  CHECK(bind_line("fb_rch 0 a -1 b -1 -1 10 7").has("batch.bad_value"));    // 0, 1 or 2
  CHECK(bind_line("gsea permutations=0 values=v candidates=c selector=fdr:0.25 shortlist=s").has("batch.bad_value"));
  CHECK(bind_line("rgx file.xml maybe 1").has("batch.bad_value"));
  CHECK_FALSE(bind_line("rgx file.xml Y 1").errors());   // FuSION 1 took Y as well as y
}

TEST_CASE("batch: binding errors name what went wrong (01.C1)", "[batch][binder]") {
  const auto unknown = bind_line("cudf values=x up=1 down=-1 upp=2");
  REQUIRE(unknown.has("batch.bad_argument"));
  CHECK(unknown.message().find("unknown argument 'upp'") != std::string::npos);
  CHECK(unknown.message().find("values, up, down") != std::string::npos);   // the names are listed

  CHECK(bind_line("cudf values=x up=1 down=-1 up=2").message().find("given twice") != std::string::npos);
  CHECK(bind_line("cudf x 1 -1 u d up=2").message().find("already given by position") != std::string::npos);
  CHECK(bind_line("cudf values=x 1 -1").message().find("after a named one") != std::string::npos);
  CHECK(bind_line("cudf values=x up=1").message().find("missing argument 'down'") != std::string::npos);
  // Positional-only slots have no name (pathz3's mode, start's exception file).
  CHECK(bind_line("pathz3 mode=0").has("batch.bad_argument"));
  CHECK(bind_line("plot_pareto x").has("batch.bad_argument"));   // takes no positional arguments
}

TEST_CASE("batch: mff: FuSION 1's form warns about the ignored tokens (S2.16)", "[batch][binder]") {
  const auto old = bind_line("mff list.txt xml 1 n map.txt -1");
  CHECK_FALSE(old.errors());
  REQUIRE(old.has("batch.ignored_tokens"));
  CHECK(std::get<std::int64_t>(old.args.at("split_threshold").value) == 1);
  CHECK_FALSE(std::get<bool>(old.args.at("tool_written").value));
  REQUIRE(old.args.form);
  CHECK(old.args.form->origin == "FuSION 1");

  const auto modern = bind_line("mff list.txt 1 y");
  CHECK_FALSE(modern.errors());
  CHECK_FALSE(modern.has("batch.ignored_tokens"));
  CHECK(std::get<bool>(modern.args.at("tool_written").value));
  CHECK(modern.args.form->origin == "2.0");

  CHECK(bind_line("mff list.txt xml 1 n map.txt").has("batch.bad_argument"));   // its map list needs -1
}

TEST_CASE("batch: start: -1 and an empty token mean no file (S1.15)", "[batch][binder]") {
  batch::Variables vars;
  vars.define("$NONE", "", 1);
  const auto b = bind_line("start $NONE -1 sym.txt names.txt", vars);
  REQUIRE_FALSE(b.errors());
  CHECK_FALSE(b.args.at("id_map").given);
  CHECK_FALSE(b.args.at("exception_file").given);
  CHECK(std::get<std::string>(b.args.at("symbols").value) == "/base/sym.txt");
}

TEST_CASE("batch: gsea: four or five positions; keep 0 is none (S7.01, S7.14)", "[batch][binder]") {
  const auto four = bind_line("gsea lfc.tsv list.txt fdr:0.25 out.txt");
  CHECK_FALSE(four.errors());
  CHECK_FALSE(four.args.at("keep").given);
  CHECK_FALSE(bind_line("gsea lfc.tsv list.txt fdr:0.25 out.txt 0").args.at("keep").given);
  const auto five = bind_line("gsea lfc.tsv list.txt fdr:0.25 out.txt hsa1,hsa2");
  CHECK(five.args.at("keep").given);
  CHECK(bind_line("gsea lfc.tsv list.txt").has("batch.bad_argument"));
}

TEST_CASE("batch: the normalised named form, and 01.C2's quoting", "[batch][binder]") {
  batch::Variables vars;
  vars.define("$D", "/data dir", 1);
  const auto b = bind_line("cudf $D/lfc.tsv 1.01 -1.01 /w/up /w/down", vars);
  CHECK(b.args.normalised() == R"(cudf values="/data dir/lfc.tsv" up=1.01 down=-1.01 up_out=/w/up down_out=/w/down)");
  CHECK(batch::quote_value("a$b") == "a$$b");
  CHECK(batch::quote_value("") == R"("")");
  CHECK(batch::quote_value(R"(say "x")") == R"("say ""x""")");
}

TEST_CASE("batch: parse_value: booleans, choices and ranges", "[batch][binder]") {
  const auto* spec = batch::find_setting("corridor.edges_to_target");
  REQUIRE(spec);
  std::string why;
  CHECK(batch::parse_value(*spec, "2", core::Source::positional, why) == core::Value(std::string("expression")));
  CHECK_FALSE(batch::parse_value(*spec, "2", core::Source::named, why));
  const auto* seed = batch::find_setting("solve.seed");
  REQUIRE(seed);
  CHECK_FALSE(batch::parse_value(*seed, "-1", core::Source::named, why));
  CHECK(why.find(">= 0") != std::string::npos);
  CHECK_FALSE(batch::find_setting("nothing.here"));
  CHECK_FALSE(batch::find_setting("corridor"));
}

TEST_CASE("batch: every argument and run-level key names a registered setting (row 1.16)", "[batch][binder]") {
  for (const auto& command : batch::command_table().all()) {
    for (const auto& arg : command.args) {
      INFO(command.name << " " << arg.name << " " << arg.setting);
      CHECK(batch::find_setting(arg.setting));
    }
    for (const auto key : command.run_settings) {
      INFO(command.name << " " << key);
      CHECK(batch::find_setting(key));
    }
  }
  for (const auto key : batch::run_level_keys()) {
    INFO(key);
    CHECK(batch::find_setting(key));
  }
  // The modules' own tables win (corridor, expression, solve, run): 09 keeps corridor.bound must_match.
  const auto* bound = batch::find_setting("corridor.bound");
  REQUIRE(bound);
  CHECK(bound->on_resume == core::OnResume::must_match);
  CHECK(batch::find_setting("corridor.observations")->need == core::Need::computed);
}
