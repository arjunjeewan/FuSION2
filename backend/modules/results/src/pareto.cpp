// results/src/pareto.cpp: the built-in Pareto plot step, plot_pareto (section 07, "Pareto plot step";
// S6.22 to S6.24, 07.C6, F1). It keeps the look of FuSION 1's plot_po.sh
// (Linux/supporting_files/plot_po.sh:34-78) and draws through core's gnuplot runner (2B.13).
#include "fusion/results/pareto.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "fusion/core/gnuplot.hpp"
#include "text.hpp"

namespace fusion::results {

namespace {

struct Style { std::string colour; int filled, open; double size, halo; };

// plot_po.sh:74-75 for the first two curves; a grey third curve for a free case beside in and out.
std::vector<Style> styles(const core::PlotPalette& palette) {
  return {{std::string(palette.with_case), 7, 6, 1.5, 2.3},
          {std::string(palette.without_case), 5, 4, 1.4, 2.2},
          {"#8a8a8a", 9, 8, 1.6, 2.4},
          {"#52514e", 13, 12, 1.5, 2.3}};
}

struct Curve {
  const SolveResult* solve = nullptr;
  const CaseResult* c = nullptr;
  std::size_t style = 0;
  bool markers_only = false;                    // a points request (06.C9): no frontier to join
  std::vector<std::pair<Point, bool>> points;   // with certified
};

std::string label_text(const SolveResult& r, NodeIx i) { return r.tables.nodes.at(i).label; }

bool partial(CaseStatus s) { return s == CaseStatus::stopped || s == CaseStatus::budget_exhausted; }

std::vector<Curve> curves_of(std::span<const SolveResult* const> solves) {
  std::vector<Curve> curves;
  for (const SolveResult* r : solves) {
    for (const CaseResult& c : r->cases) {
      Curve curve;
      curve.solve = r;
      curve.c = &c;
      if (c.points.empty() && !c.requested_points.empty()) {
        curve.markers_only = true;
        for (const RequestedPoint& q : c.requested_points) {
          if (q.answer == solve::Outcome::sat) curve.points.emplace_back(q.at, true);
        }
      } else {
        for (const FrontierPoint& p : c.points) curve.points.emplace_back(p.at, p.certified);
      }
      std::sort(curve.points.begin(), curve.points.end(), [](const auto& a, const auto& b) {
        return a.first.n != b.first.n ? a.first.n < b.first.n : a.first.e < b.first.e;
      });
      curves.push_back(std::move(curve));
    }
  }
  // Blue circles for in, red squares for out (S6.23); a solve without an FSC gives one blue free curve,
  // and a free case beside in and out is grey (S6.22). With several solves, curves take the styles in
  // order, as plot_po.sh's two files did.
  const auto max_style = styles(core::plot_palette()).size() - 1;
  if (solves.size() == 1) {
    bool has_in_or_out = false;
    for (const Curve& c : curves) has_in_or_out = has_in_or_out || c.c->kind != Case::free;
    for (Curve& c : curves) {
      c.style = c.c->kind == Case::in ? 0 : c.c->kind == Case::out ? 1 : has_in_or_out ? 2 : 0;
    }
  } else {
    for (std::size_t i = 0; i < curves.size(); ++i) curves[i].style = std::min(i, max_style);
  }
  return curves;
}

std::string number(double v) { return detail::number_text(v); }

}  // namespace

double dodge_offset(std::int32_t edge_upper) noexcept {
  // S6.24: plot_po.sh's 0.10 (plot_po.sh:32) was tuned for the bound of 8 every call passed; the plot
  // area is fixed by explicit margins, so the same on-screen offset is 0.10 * (e_ub + 1) / 9.
  return 0.10 * static_cast<double>(edge_upper + 1) / 9.0;
}

std::string gnuplot_text(std::string_view label) {
  // Enhanced text's control characters, escaped for a double-quoted gnuplot string, and the quote.
  std::string out;
  for (const char c : label) {
    switch (c) {
      case '_': case '^': case '{': case '}': case '@': case '&': case '~':
        out += "\\\\";
        out += c;
        break;
      case '\\': out += "\\\\\\\\"; break;
      case '"': out += "\\\""; break;
      default:
        out += static_cast<unsigned char>(c) < 0x20 || c == 0x7f ? ' ' : c;
    }
  }
  return out;
}

std::string gnuplot_script(std::span<const SolveResult* const> solves, const ParetoOptions& options) {
  const core::PlotPalette& palette = core::plot_palette();
  const auto style_table = styles(palette);
  std::vector<Curve> curves = curves_of(solves);
  const std::string font = gnuplot_text(options.font);

  // Axes use the requested box, [-0.5, ub + 0.5], so plots in a sweep share their scales (S6.23).
  int n_ub = 0, e_ub = 0;
  for (const Curve& c : curves) {
    n_ub = std::max(n_ub, c.c->requested.n);
    e_ub = std::max(e_ub, c.c->requested.e);
    for (const auto& [p, certified] : c.points) {
      n_ub = std::max(n_ub, p.n);
      e_ub = std::max(e_ub, p.e);
    }
  }

  // S6.24: a point on both of the first two curves moves up on the first and down on the second.
  const double off = dodge_offset(e_ub);
  std::set<std::pair<int, int>> on_first, on_second;
  for (const Curve& c : curves) {
    for (const auto& [p, certified] : c.points) {
      if (c.style == 0) on_first.emplace(p.n, p.e);
      if (c.style == 1) on_second.emplace(p.n, p.e);
    }
  }
  const auto y_of = [&](const Curve& c, Point p) {
    const bool both = on_first.contains({p.n, p.e}) && on_second.contains({p.n, p.e});
    if (!both || c.style > 1) return number(p.e);
    return number(static_cast<double>(p.e) + (c.style == 0 ? off : -off));
  };

  std::string s;
  s += "# FuSION 2.0 Pareto plot (section 07; S6.22 to S6.24). In this directory,\n"
       "# \"gnuplot pareto.gp\" draws pareto.png again; the data are inline.\n";
  s += "set encoding utf8\n";
  s += "set terminal pngcairo enhanced size 980,690 font \"" + font + ",13\" background rgb \"" +
       std::string(palette.background) + "\"\n";
  s += "set output 'pareto.png'\n";
  s += "set lmargin at screen 0.085; set rmargin at screen 0.975; set bmargin at screen 0.105; "
       "set tmargin at screen 0.865\n";

  // Title and subtitle from the run's own labels and settings (S6.22, S6.23).
  std::string title = "Pareto frontier";
  std::string subtitle;
  if (!solves.empty()) {
    const SolveResult& r = *solves.front();
    title = gnuplot_text(label_text(r, r.header.source)) + " → " + gnuplot_text(label_text(r, r.header.target)) +
            "     ·     Pareto frontier";
    std::vector<std::string> parts;
    if (!r.header.fsc.empty()) {
      std::vector<std::string> labels, keys;
      for (const NodeIx f : r.header.fsc) {
        labels.push_back(gnuplot_text(label_text(r, f)));
        keys.push_back(gnuplot_text(r.tables.nodes.at(f).key.value));
      }
      parts.push_back("FSC " + detail::join(labels, ", ") + " (" + detail::join(keys, ", ") + ")");
    }
    parts.push_back("bound " + std::to_string(r.header.bound));
    // Only the constraint families the run used, with the solver's strict test (S5.03, S5.15, S5.16);
    // plot_po.sh printed "≥" and both thresholds whatever the run used (plot_po.sh:35).
    std::vector<std::string> families;
    if (r.header.coexpression_threshold) families.push_back("coexpression > " + number(*r.header.coexpression_threshold));
    if (r.header.frozen_threshold) families.push_back("frozen > " + number(*r.header.frozen_threshold));
    if (!families.empty()) parts.push_back(detail::join(families, ", "));
    parts.push_back("n ≤ " + std::to_string(n_ub) + ", e ≤ " + std::to_string(e_ub));
    subtitle = detail::join(parts, " · ");
  }
  s += "set title \"{/:Bold " + title + "}\\n{/*0.72 " + subtitle + "}\" tc rgb \"#0b0b0b\" font \"" + font +
       ",16\" offset 0,-0.4\n";
  s += "set xlabel \"Node relaxations\" tc rgb \"#52514e\" font \"" + font + ",12.5\" offset 0,0.4\n";
  s += "set ylabel \"Edge relaxations\" tc rgb \"#52514e\" font \"" + font + ",12.5\" offset 1.2,0\n";
  s += "set xrange [-0.5:" + std::to_string(n_ub) + ".5]\n";
  s += "set yrange [-0.5:" + std::to_string(e_ub) + ".5]\n";
  s += "set xtics 1 nomirror out scale 0.4 tc rgb \"#898781\" font \"" + font + ",11\"\n";
  s += "set ytics 1 nomirror out scale 0.4 tc rgb \"#898781\" font \"" + font + ",11\"\n";
  s += "set border 3 lc rgb \"#c3c2b7\" lw 1.4\n";
  s += "set grid xtics ytics back lc rgb \"" + std::string(palette.grid) + "\" lw 1.0 lt 1\n";
  s += "set tics front\n";
  s += "set key top right reverse Left samplen 1.4 spacing 1.35 font \"" + font +
       ",12\" tc rgb \"#52514e\" box lc rgb \"" + std::string(palette.grid) + "\" lw 1.0 opaque\n";
  std::set<std::size_t> used;
  for (const Curve& c : curves) used.insert(c.style);
  for (const std::size_t i : used) {
    const Style& st = style_table[i];
    s += "set style line " + std::to_string(11 + i) + " lc rgb \"" + st.colour + "\" lw 2.4 pt " +
         std::to_string(st.filled) + " ps " + number(st.size) + "\n";
    // 07.C6: an uncertified point gets the open marker of its curve's shape and colour.
    s += "set style line " + std::to_string(21 + i) + " lc rgb \"" + st.colour + "\" lw 2.4 pt " +
         std::to_string(st.open) + " ps " + number(st.size) + "\n";
  }

  // Data blocks inline (gnuplot 5): no temporary files, as plot_po.sh's mktemp and awk needed.
  std::vector<std::string> halos, lines, markers, keys;
  bool any_open = false;
  for (std::size_t i = 0; i < curves.size(); ++i) {
    const Curve& c = curves[i];
    const Style& st = style_table[c.style];
    const std::string block = "$c" + std::to_string(i);
    const std::string ls = std::to_string(11 + c.style);
    std::string all, certified, open;
    for (const auto& [p, ok] : c.points) {
      const std::string row = std::to_string(p.n) + "\t" + y_of(c, p) + "\n";
      all += row;
      (ok ? certified : open) += row;
    }
    if (!all.empty()) {
      s += block + " << EOD\n" + all + "EOD\n";
      halos.push_back(block + " u 1:2 w points pt " + std::to_string(st.filled) + " ps " + number(st.halo) +
                      " lc rgb \"" + std::string(palette.background) + "\" notitle");
      if (!c.markers_only) lines.push_back(block + " u 1:2 w lines ls " + ls + " notitle");
    }
    if (!certified.empty()) {
      s += block + "_certified << EOD\n" + certified + "EOD\n";
      markers.push_back(block + "_certified u 1:2 w points ls " + ls + " notitle");
    }
    if (!open.empty()) {
      s += block + "_open << EOD\n" + open + "EOD\n";
      markers.push_back(block + "_open u 1:2 w points ls " + std::to_string(21 + c.style) + " notitle");
      any_open = true;
    }
    // The key: one entry per case. A case without a curve shows its status in the display words
    // (S6.21, S6.23: "no Pareto point within the bounds"), as plot_po.sh's empty-file entry did.
    std::string title_words = c.c->label;
    if (solves.size() > 1 && c.c->kind != Case::free) title_words = c.solve->header.name + ": " + title_words;
    if (c.points.empty()) {
      title_words += ": " + c.c->status_text;
      keys.push_back("keyentry w points ls " + ls + " title \"" + gnuplot_text(title_words) + "\"");
      continue;
    }
    if (partial(c.c->status)) title_words += " (partial)";
    if (c.markers_only) title_words += " (requested points)";
    keys.push_back("keyentry w " + std::string(c.markers_only ? "points" : "linespoints") + " ls " + ls +
                   " title \"" + gnuplot_text(title_words) + "\"");
  }
  if (any_open) {
    keys.push_back("keyentry w points pt 6 ps 1.5 lc rgb \"#52514e\" title \"open marker: not certified\"");
  }
  std::vector<std::string> plot = halos;
  plot.insert(plot.end(), lines.begin(), lines.end());
  plot.insert(plot.end(), markers.begin(), markers.end());
  plot.insert(plot.end(), keys.begin(), keys.end());
  s += "plot " + detail::join(plot, ", \\\n     ") + "\n";
  return s;
}

void plot_pareto(std::span<const SolveResult* const> solves, const ParetoOptions& options, core::Context& ctx) {
  const std::string script = gnuplot_script(solves, options);
  {
    // The script is kept, so the plot can be drawn again without a re-solve (section 07).
    core::OutputFile gp = ctx.files.create("pareto.gp");
    gp.out() << script;
    gp.commit();
  }
  core::OutputFile png = ctx.files.create("pareto.png");
  // gnuplot writes the OutputFile's own temporary file, by its relative name in the step directory,
  // so no path reaches its string rules; commit() then renames and hashes it (09, 2A.6).
  const std::filesystem::path temp = png.temp_path();
  const std::string temp_name = core::to_utf8(temp.filename());
  std::string run_script = script;
  const std::string output_line = "set output 'pareto.png'\n";
  if (const auto at = run_script.find(output_line); at != std::string::npos) {
    run_script.replace(at, output_line.size(), "set output '" + temp_name + "'\n");
  }
  core::GnuplotJob job;
  job.script = std::move(run_script);
  job.output = temp.filename();
  job.working_dir = temp.parent_path();
  const core::GnuplotResult result = core::run_gnuplot(job);
  if (result.exit_code != 0 || !result.output_written) {
    std::string what = result.program.empty() ? std::string("gnuplot was not found")
                       : result.exit_code != 0 ? "gnuplot exited with status " + std::to_string(result.exit_code)
                                               : std::string("gnuplot wrote no image");
    const std::string tail = detail::last_lines(result.stderr_text, 20);
    if (!tail.empty()) what += ": " + tail;
    throw core::Failure("results.gnuplot_failed", "plot_pareto: " + what);
  }
  png.commit();

  std::vector<std::string> parts;
  std::size_t uncertified = 0;
  for (const SolveResult* r : solves) {
    for (const CaseResult& c : r->cases) {
      std::size_t k = c.points.size();
      if (c.points.empty()) {
        k = static_cast<std::size_t>(std::count_if(c.requested_points.begin(), c.requested_points.end(),
                                                   [](const RequestedPoint& q) { return q.answer == solve::Outcome::sat; }));
      }
      parts.push_back(c.label + " " + std::to_string(k) + (k == 1 ? " point" : " points"));
      for (const FrontierPoint& p : c.points) uncertified += p.certified ? 0 : 1;
    }
  }
  if (uncertified > 0) parts.push_back(std::to_string(uncertified) + " not certified");
  ctx.log.info("plot_pareto: " + std::string(ctx.step) + "/pareto.png (" + detail::join(parts, ", ") + ")");
}

}  // namespace fusion::results
