// fusion-test's command line and its exit codes (the C3 note; section 12, "Files, settings, events and
// errors"): the parse in process, then the built executable as a child.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/core/process.hpp"
#include "fusion/testkit/scratch.hpp"
#include "options.hpp"

#ifndef FUSION_TEST_EXE
#error "test_fusion_test is built with FUSION_TEST_EXE, the built fusion-test"
#endif

using namespace fusion::fusiontest;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

Command command(std::vector<std::string> args) {
  const auto p = parse(args);
  INFO((std::holds_alternative<std::string>(p) ? std::get<std::string>(p) : std::string()));
  REQUIRE(std::holds_alternative<Command>(p));
  return std::get<Command>(p);
}

std::string usage_error(std::vector<std::string> args) {
  const auto p = parse(args);
  REQUIRE(std::holds_alternative<std::string>(p));
  return std::get<std::string>(p);
}

// Sets an environment variable for one scope, then puts the old value back.
class EnvVar {
 public:
  EnvVar(const char* name, std::optional<std::string> value) : name_(name) {
    if (const char* old = std::getenv(name)) old_ = old;
    set(value);
  }
  ~EnvVar() { set(old_); }
  EnvVar(const EnvVar&) = delete;
  EnvVar& operator=(const EnvVar&) = delete;

 private:
  void set(const std::optional<std::string>& value) {
#ifdef _WIN32
    _putenv_s(name_, value ? value->c_str() : "");
#else
    if (value) ::setenv(name_, value->c_str(), 1);
    else ::unsetenv(name_);
#endif
  }
  const char* name_;
  std::optional<std::string> old_;
};

fusion::core::ProcessResult fusion_test(std::vector<std::string> args,
                                        std::vector<std::pair<std::string, std::string>> env = {}) {
  fusion::core::ProcessSpec spec;
  spec.program = fusion::core::path_from_utf8(FUSION_TEST_EXE);
  spec.args = std::move(args);
  spec.env_set = std::move(env);
  spec.timeout = std::chrono::seconds(60);
  return fusion::core::run_process(spec);
}

}  // namespace

TEST_CASE("fusion-test: the command line of data, e2e, ref and bench", "[unit][fusion-test]") {
  const Command verify = command({"data", "verify", "--data", "/d"});
  CHECK(verify.name == "data");
  CHECK(verify.action == "verify");
  CHECK(verify.data == fs::path("/d"));
  const Command init = command({"data", "init", "--from", "/arjun", "--data", "/d"});
  CHECK(init.from == fs::path("/arjun"));
  const Command record = command({"data", "record", "--origin", "W3 reports/w3/20261117T183723",
                                  "reference/a/f1/capture", "reports/w3/20261117T183723"});
  CHECK(record.origin == "W3 reports/w3/20261117T183723");
  CHECK(record.paths == std::vector<std::string>{"reference/a/f1/capture", "reports/w3/20261117T183723"});
  const Command e2e = command({"e2e", "--case", "a", "--update", "b", "--case", "c", "--keep", "--fusion", "/f"});
  CHECK(e2e.cases == std::vector<std::string>{"a", "c"});
  CHECK(e2e.update == std::vector<std::string>{"b"});
  CHECK(e2e.keep);
  CHECK_FALSE(e2e.list);
  CHECK(fusion_exe(e2e) == fs::path("/f"));
  CHECK(command({"e2e", "--list"}).list);
  // ref and bench take their own arguments (C4, C6).
  CHECK(command({"ref", "--tier", "t2", "--whatever"}).rest == std::vector<std::string>{"--tier", "t2", "--whatever"});
  CHECK(command({"bench"}).name == "bench");
  CHECK(command({"-h"}).help);
  CHECK(command({"data", "--help"}).help);
  CHECK(command({"e2e", "-h"}).help);
}

TEST_CASE("fusion-test: usage errors name what is wrong", "[unit][fusion-test]") {
  CHECK_THAT(usage_error({}), ContainsSubstring("no subcommand"));
  CHECK_THAT(usage_error({"golden"}), ContainsSubstring("unknown subcommand golden"));
  CHECK_THAT(usage_error({"data"}), ContainsSubstring("verify, init or record"));
  CHECK_THAT(usage_error({"data", "copy"}), ContainsSubstring("unknown data action copy"));
  CHECK_THAT(usage_error({"data", "init"}), ContainsSubstring("needs --from"));
  CHECK_THAT(usage_error({"data", "record", "reports/w3"}), ContainsSubstring("needs --origin"));
  CHECK_THAT(usage_error({"data", "record", "--origin", "W3 x"}), ContainsSubstring("a file or folder"));
  CHECK_THAT(usage_error({"data", "verify", "--data"}), ContainsSubstring("--data needs a value"));
  CHECK_THAT(usage_error({"data", "verify", "--data", "/a", "--data", "/b"}), ContainsSubstring("given twice"));
  CHECK_THAT(usage_error({"data", "verify", "--from", "/a"}), ContainsSubstring("unknown option --from"));
  CHECK_THAT(usage_error({"data", "verify", "extra"}), ContainsSubstring("unexpected argument extra"));
  CHECK_THAT(usage_error({"e2e", "--case"}), ContainsSubstring("--case needs a case id"));
  CHECK_THAT(usage_error({"e2e", "--case", "--keep"}), ContainsSubstring("--case needs a case id"));
  CHECK_THAT(usage_error({"e2e", "--list", "--update", "a"}), ContainsSubstring("does not go with --update"));
  CHECK_THAT(usage_error({"e2e", "--origin", "x"}), ContainsSubstring("unknown option --origin for fusion-test e2e"));
}

TEST_CASE("fusion-test: --fusion defaults to the staged fusion, --data to FUSION_TEST_DATA", "[unit][fusion-test]") {
  const Command plain = command({"e2e"});
  CHECK(fusion_exe(plain) == fusion::core::path_from_utf8(FUSION_EXE));
  CHECK(fusion_exe(plain).parent_path().filename() == "bin");   // stage/bin/fusion, beside the staged home
  {
    const EnvVar env("FUSION_TEST_DATA", std::nullopt);
    CHECK_FALSE(data_dir(plain));
  }
  {
    const EnvVar env("FUSION_TEST_DATA", "");
    CHECK_FALSE(data_dir(plain));
  }
  const EnvVar env("FUSION_TEST_DATA", "/somewhere/data-root");
  CHECK(data_dir(plain) == fs::path("/somewhere/data-root"));
  CHECK(data_dir(command({"e2e", "--data", "/other"})) == fs::path("/other"));
  CHECK(fusion_exe(command({"e2e", "--fusion", "relative/fusion"})).is_absolute());
}

TEST_CASE("fusion-test: the executable's exit codes are section 12's 0, 1, 2 and 4", "[unit][fusion-test]") {
  const auto help = fusion_test({"-h"});
  CHECK(help.exit_code == exit_ok);
  CHECK_THAT(help.stdout_text, ContainsSubstring("fusion-test data verify"));
  const auto none = fusion_test({});
  CHECK(none.exit_code == exit_harness);   // a usage error: the harness could not run
  CHECK_THAT(none.stderr_text, ContainsSubstring("no subcommand"));
  for (const char* later : {"ref", "bench"}) {
    const auto stub = fusion_test({later, "--tier", "t1"});
    CHECK(stub.exit_code == exit_harness);
    CHECK_THAT(stub.stderr_text, ContainsSubstring(std::string(later) == "ref" ? "comes with C4" : "come with C6"));
  }
  const auto no_root = fusion_test({"data", "verify"}, {{"FUSION_TEST_DATA", ""}});
  CHECK(no_root.exit_code == exit_harness);
  CHECK_THAT(no_root.stderr_text, ContainsSubstring("no data root"));
  // An empty toy root against the repository's manifest: every file is missing, a difference (1). Nothing is
  // hashed, so the real root is never read and the toy root keeps no cache.
  fusion::testkit::ScratchDir empty("empty-root");
  const auto missing = fusion_test({"data", "verify", "--data", fusion::core::to_utf8(empty.path())});
  CHECK(missing.exit_code == exit_failed);
  CHECK_THAT(missing.stdout_text, ContainsSubstring("missing"));
  CHECK(fs::is_empty(empty.path()));
  const auto unknown_case = fusion_test({"e2e", "--case", "no-such-case"});
  CHECK(unknown_case.exit_code == exit_harness);
  CHECK_THAT(unknown_case.stdout_text, ContainsSubstring("no case no-such-case"));
}
