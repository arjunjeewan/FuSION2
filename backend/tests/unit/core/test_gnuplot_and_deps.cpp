// Section 12's core row: the gnuplot runner and the plot palette (2B.13), and the dependency check
// (S6.11, 2A.1). Section 11's test hooks: FUSION_GNUPLOT points at small stand-in programs, one
// without pngcairo, one that prints gnuplot's unknown-terminal warning and writes no file, and one
// that reports version 5.2.
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "fusion/core/core.hpp"
#include "fusion/core/deps.hpp"
#include "fusion/core/gnuplot.hpp"
#include "fusion/core/install.hpp"
#include "fusion/core/process.hpp"
#include "support.hpp"

using namespace fusion::core;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

// Answers the version probe like gnuplot 6.0 (on stderr), and otherwise copies its script to
// script.gp and its environment to env.txt, and draws plot.png, in its working directory.
std::string fake_gnuplot(const std::string& version_line, const std::string& terminals, bool draws = true) {
    return "if [ \"$1\" = -e ]; then printf '" + version_line + "\\n" + terminals + "\\n' >&2; exit 0; fi\n"
           "cat > script.gp\n"
           "env > env.txt\n" +
           (draws ? std::string("printf 'PNG' > plot.png\n")
                  : std::string("echo 'warning: unknown or ambiguous terminal type' >&2\n")) +
           "exit 0\n";
}

}  // namespace

TEST_CASE("core: the plot palette is plot_po.sh's (S6.23)", "[unit][core]") {
    const PlotPalette& p = plot_palette();
    CHECK(p.with_case == "#2a78d6");             // Linux/supporting_files/plot_po.sh:74
    CHECK(p.without_case == "#e34948");          // plot_po.sh:75
    CHECK(p.background == "#fcfcfb");            // plot_po.sh:56
    CHECK(p.grid == "#e1e0d9");                  // plot_po.sh:69
    CHECK(p.font == "Liberation Sans");          // plot_po.sh:56
    CHECK(&plot_palette() == &p);
}

TEST_CASE("core: run_gnuplot hands gnuplot the script on stdin and checks the image was written", "[unit][core]") {
    test::ScratchDir dir("gnuplot");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("6.0 5", " png pngcairo svg"));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    fs::create_directories(dir / "plot_pareto");
    GnuplotJob job;
    job.script = "set terminal pngcairo\n$data << EOD\n0 4\n1 2\nEOD\nplot $data\n";
    job.output = "plot.png";
    job.working_dir = dir / "plot_pareto";
    const GnuplotResult r = run_gnuplot(job);
    CHECK(r.exit_code == 0);
    CHECK(r.output_written);
    CHECK(r.program == program);
    CHECK(test::read_file(dir / "plot_pareto" / "script.gp") == job.script);
}

TEST_CASE("core: a gnuplot that warns, draws nothing and exits 0 is reported as not written", "[unit][core]") {
    test::ScratchDir dir("gnuplot-warns");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("6.0 5", " png", false));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    GnuplotJob job{.script = "set terminal nosuchterm\nplot x\n", .output = "plot.png", .working_dir = dir.path()};
    const GnuplotResult r = run_gnuplot(job);
    CHECK(r.exit_code == 0);
    CHECK_FALSE(r.output_written);
    CHECK_THAT(r.stderr_text, ContainsSubstring("unknown or ambiguous terminal type"));
}

TEST_CASE("core: a FUSION_GNUPLOT that names no file is reported, never silently replaced", "[unit][core]") {
    test::ScratchDir dir("gnuplot-missing");
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", (dir / "missing").string());
    const GnuplotResult r = run_gnuplot(GnuplotJob{.script = "plot x\n", .output = "plot.png", .working_dir = dir.path()});
    CHECK(r.exit_code == -1);
    CHECK_FALSE(r.output_written);
    CHECK(r.program.empty());
    CHECK_THAT(r.stderr_text, ContainsSubstring("FUSION_GNUPLOT"));
}

TEST_CASE("core: with the bundled fonts.conf, gnuplot runs with FONTCONFIG_FILE and PANGOCAIRO_BACKEND (11.C10)",
          "[unit][core]") {
    test::ScratchDir dir("gnuplot-fonts");
    const test::ScopedEnv home("FUSION_HOME", (dir / "home").string());
    if (install_dir().home != dir / "home") SKIP("the install directory was worked out earlier in this process");
    fs::create_directories(dir / "home" / "fonts");
    test::write_file(dir / "home" / "fonts" / "fonts.conf", "<fontconfig/>\n");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("6.0 5", " pngcairo"));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    const GnuplotResult r = run_gnuplot(GnuplotJob{.script = "plot x\n", .output = "plot.png", .working_dir = dir.path()});
    CHECK(r.output_written);
    const std::string env = test::read_file(dir / "env.txt");
    CHECK_THAT(env, ContainsSubstring("FONTCONFIG_FILE=" + (dir / "home" / "fonts" / "fonts.conf").string() + "\n"));
    CHECK_THAT(env, ContainsSubstring("PANGOCAIRO_BACKEND=fc\n"));
}

TEST_CASE("core: the system gnuplot draws a PNG through the runner", "[unit][core][gnuplot]") {
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", std::nullopt);
    if (!find_on_path("gnuplot")) SKIP("no gnuplot on PATH");
    test::ScratchDir dir("gnuplot-real");
    const PlotPalette& p = plot_palette();
    GnuplotJob job;
    job.script = "set terminal pngcairo size 200,150 background rgb '" + std::string(p.background) +
                 "'\nset output 'pareto.png.tmp'\n$d << EOD\n0 4\n1 2\n3 1\nEOD\n"
                 "plot $d using 1:2 with linespoints lc rgb '" + std::string(p.with_case) + "' notitle\n";
    job.output = "pareto.png.tmp";
    job.working_dir = dir.path();
    const GnuplotResult r = run_gnuplot(job);
    INFO(r.stderr_text);
    CHECK(r.exit_code == 0);
    CHECK(r.output_written);
    CHECK(test::read_file(dir / "pareto.png.tmp").substr(1, 3) == "PNG");
}

TEST_CASE("core: the check merges each tool's requirements and finds gnuplot's version and terminals", "[unit][core]") {
    test::ScratchDir dir("deps-ok");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("6.0 5", " block png pngcairo svg"));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    deps::Registry registry;
    registry.require({"gnuplot", deps::Kind::program, "5.4", {"pngcairo"}, "plot_pareto (line 262)"});
    registry.require({"sh", deps::Kind::program, "", {}, "exec (line 151)"});
    registry.require({"gnuplot", deps::Kind::program, "5.0", {"png"}, "exec (line 263)"});
    const auto probes = registry.check();
    REQUIRE(probes.size() == 2);                 // one per id, sorted by id
    const deps::Probe& g = probes[0];
    CHECK(g.id == "gnuplot");
    CHECK(g.ok);
    CHECK(g.version == "6.0.5");
    CHECK(g.origin == "FUSION_GNUPLOT");
    CHECK(g.path == program);
    CHECK(g.features == std::vector<std::string>{"pngcairo", "png"});
    CHECK(g.missing.empty());
    CHECK(g.needed_by == std::vector<std::string>{"plot_pareto (line 262)", "exec (line 263)"});
    CHECK(probes[1].id == "sh");
    CHECK(probes[1].ok);
    CHECK(probes[1].path == "/bin/sh");
}

TEST_CASE("core: a gnuplot without pngcairo fails the check, naming the terminal (deps.missing)", "[unit][core]") {
    test::ScratchDir dir("deps-no-cairo");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("6.0 0", " dumb png svg"));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    deps::Registry registry;
    registry.require({"gnuplot", deps::Kind::program, "5.4", {"pngcairo"}, "plot_pareto (line 262)"});
    const auto probes = registry.check();
    REQUIRE(probes.size() == 1);
    CHECK_FALSE(probes[0].ok);
    CHECK(probes[0].missing == std::vector<std::string>{"pngcairo"});
    CHECK_THAT(probes[0].message, ContainsSubstring("plot_pareto (line 262)") && ContainsSubstring("pngcairo"));
}

TEST_CASE("core: a gnuplot older than the minimum fails the check", "[unit][core]") {
    test::ScratchDir dir("deps-old");
    const fs::path program = test::write_script(dir / "gnuplot", fake_gnuplot("5.2 8", " pngcairo"));
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", program.string());
    deps::Registry registry;
    registry.require({"gnuplot", deps::Kind::program, "5.4", {"pngcairo"}, "plot_pareto (line 262)"});
    const auto probes = registry.check();
    REQUIRE(probes.size() == 1);
    CHECK_FALSE(probes[0].ok);
    CHECK(probes[0].version == "5.2.8");
    CHECK_THAT(probes[0].message, ContainsSubstring("5.4 or later"));
}

TEST_CASE("core: a missing tool fails the check, saying where it looked", "[unit][core]") {
    test::ScratchDir dir("deps-missing");
    const test::ScopedEnv override_gnuplot("FUSION_GNUPLOT", (dir / "nowhere" / "gnuplot").string());
    deps::Registry registry;
    registry.require({"gnuplot", deps::Kind::program, "5.4", {"pngcairo"}, "plot_pareto (line 262)"});
    registry.require({"no-such-tool-fusion-test", deps::Kind::program, "", {}, "exec (line 3)"});
    const auto probes = registry.check();
    REQUIRE(probes.size() == 2);
    CHECK_FALSE(probes[0].ok);
    CHECK_THAT(probes[0].message, ContainsSubstring("none found") && ContainsSubstring("FUSION_GNUPLOT"));
    CHECK(probes[0].missing.front() == "gnuplot");
    CHECK_FALSE(probes[1].ok);
    CHECK(deps::Registry().check().empty());
}

TEST_CASE("core: the bundled Liberation Sans satisfies the font requirement (11.C10)", "[unit][core]") {
    test::ScratchDir dir("deps-font");
    const test::ScopedEnv home("FUSION_HOME", (dir / "home").string());
    if (install_dir().home != dir / "home") SKIP("the install directory was worked out earlier in this process");
    fs::create_directories(dir / "home" / "fonts");
    test::write_file(dir / "home" / "fonts" / "LiberationSans-Regular.ttf", "font");
    test::write_file(dir / "home" / "fonts" / "LiberationSans-Bold.ttf", "font");
    deps::Registry registry;
    registry.require({"font:Liberation Sans", deps::Kind::font, "", {}, "plot_pareto (line 262)"});
    const auto probes = registry.check();
    REQUIRE(probes.size() == 1);
    CHECK(probes[0].ok);
    CHECK(probes[0].origin == "bundled");
    CHECK(probes[0].path == dir / "home" / "fonts");
}
