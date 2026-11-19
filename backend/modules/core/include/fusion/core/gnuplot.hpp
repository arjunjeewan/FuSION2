// fusion/core/gnuplot.hpp: the one gnuplot runner and the plot palette, shared by section 07's
// plot_pareto and section 04's threshold histogram (cross-section item 2B.13; 11.C9, 11.C10).
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

namespace fusion::core {

struct PlotPalette {                  // plot_po.sh's look (S6.23)
    std::string_view with_case = "#2a78d6", without_case = "#e34948";
    std::string_view background = "#fcfcfb", grid = "#e1e0d9";
    std::string_view font = "Liberation Sans";
};
const PlotPalette& plot_palette() noexcept;

struct GnuplotJob {
    std::string script{};                  // the whole script, data inline (gnuplot 5 data blocks)
    std::filesystem::path output{};        // the PNG it must write; relative to working_dir
    std::filesystem::path working_dir{};
    std::chrono::milliseconds timeout{120000};   // a gnuplot that hangs is stopped
};

struct GnuplotResult {
    int exit_code = -1;                    // -1: not found, could not start, or stopped
    bool output_written = false;           // the file exists and is non-empty
    std::string stderr_text{};             // warnings included, such as an unknown terminal
    std::filesystem::path program{};       // the gnuplot that ran; empty if none was found
};

// Finds gnuplot in C20's order (FUSION_GNUPLOT, the bundled tools/gnuplot/bin/gnuplot, PATH), sets
// FONTCONFIG_FILE and PANGOCAIRO_BACKEND when the home holds fonts/fonts.conf (C10), runs it through
// run_process with the script on stdin, and reports; the caller raises a failed plot as its own
// failure (04's expression.gnuplot_failed, 07's results.gnuplot_failed).
GnuplotResult run_gnuplot(const GnuplotJob&);

}  // namespace fusion::core
