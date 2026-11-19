// fusion core: the gnuplot runner and the plot palette (cross-section item 2B.13; 11.C9, 11.C10,
// 11.C20, 1.27).
//
// FuSION 1 drew its Pareto plot with plot_po.sh, named by an absolute path in each batch and run
// through exec (S6.22), with its colours and font in the script
// (Linux/supporting_files/plot_po.sh:11-12, 48, 56, 69, 74-75; S6.23). Here the palette is one
// value both plots share, and gnuplot runs without a shell, found the same way everywhere.
#include "fusion/core/gnuplot.hpp"

#include "fusion/core/core.hpp"
#include "fusion/core/install.hpp"
#include "fusion/core/process.hpp"
#include "tools.hpp"

namespace fusion::core {

const PlotPalette& plot_palette() noexcept {
    static const PlotPalette palette;            // plot_po.sh's blue and red pair, halo and grid
    return palette;
}

GnuplotResult run_gnuplot(const GnuplotJob& job) {
    namespace fs = std::filesystem;
    GnuplotResult result;
    const detail::Located gnuplot =
        detail::locate_tool("FUSION_GNUPLOT", install_dir().tools_dir / "gnuplot" / "bin" / "gnuplot", "gnuplot");
    if (gnuplot.program.empty()) {
        result.stderr_text = "gnuplot not found (" + gnuplot.tried + ")";
        return result;
    }
    result.program = gnuplot.program;

    ProcessSpec spec;
    spec.program = gnuplot.program;
    spec.working_dir = job.working_dir;
    spec.input = ProcessSpec::Input::text;
    spec.stdin_text = job.script;
    spec.output = ProcessSpec::Output::capture;
    spec.own_group = true;
    spec.timeout = job.timeout;
    // 11.C10: the bundled fonts.conf lists only the bundled Liberation Sans, so the same font
    // files draw every plot; PANGOCAIRO_BACKEND=fc makes pango use fontconfig on every platform.
    const fs::path fonts_conf = install_dir().home / "fonts" / "fonts.conf";
    std::error_code ec;
    if (fs::is_regular_file(fonts_conf, ec)) {
        spec.env_set.emplace_back("FONTCONFIG_FILE", to_utf8(fonts_conf));
        spec.env_set.emplace_back("PANGOCAIRO_BACKEND", "fc");
    }

    const ProcessResult run = run_process(spec);
    result.exit_code = run.exit_code;
    result.stderr_text = run.stderr_text;
    if (!run.error.empty()) result.stderr_text = run.error;
    else if (run.timed_out) result.stderr_text += "gnuplot did not finish within the time allowed\n";

    // A gnuplot without the asked terminal warns, draws nothing and exits 0 (section 11, measured),
    // so the file itself is checked too.
    const fs::path output =
        job.output.is_absolute() || job.working_dir.empty() ? job.output : job.working_dir / job.output;
    const bool is_file = fs::is_regular_file(output, ec);
    result.output_written = is_file && fs::file_size(output, ec) > 0 && !ec;
    return result;
}

}  // namespace fusion::core
