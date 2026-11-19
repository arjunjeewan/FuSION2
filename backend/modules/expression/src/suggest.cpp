// fusion expression: threshold suggestion, its report and its histogram (section 04, "Threshold
// suggestion"; S3.15 to S3.19; 04.C6, 04.C7). The port of
// Linux/supporting_files/suggest_thresholds.py, which FuSION 1 never called (decision 0013).
#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>

#include "fusion/core/gnuplot.hpp"
#include "internal.hpp"

namespace fusion::expression {

namespace {

std::string number(double value) { return core::format_number(value); }

double median_of_sorted(const std::vector<double>& sorted) {
    const std::size_t n = sorted.size(), middle = n / 2;
    return n % 2 == 1 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
}

// Python's round(t, 4): the correctly rounded decimal with four places, read back as a double.
double round4(double value) {
    char buffer[400];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, 4);
    const double rounded = core::parse_decimal(std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer))).value;
    return rounded == 0 ? 0.0 : rounded;
}

// Python's format(t, "+g"): six significant digits, with the sign.
std::string signed_general(double value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::general, 6);
    std::string text(buffer, result.ptr);
    return text.front() == '-' ? text : "+" + text;
}

// A gnuplot string in single quotes, where '' is one quote and backslashes are plain (04 "Portability").
std::string single_quoted(std::string_view text) {
    std::string out = "'";
    for (const char c : text) out += c == '\'' ? std::string("''") : std::string(1, c);
    return out + "'";
}

// The palette suggest_thresholds.py shares with plot_po.sh (suggest_thresholds.py:51-61; S3.19). The
// background, grid, cut colours and font come from the plot palette core holds for this plot and the
// Pareto plot (2B.13); the inks it does not hold are the script's own.
constexpr std::string_view ink = "#0b0b0b", label = "#52514e", tic = "#898781", border = "#c3c2b7",
                           bar_fill = "#dcdbd3", bar_edge = "#a8a79d";

std::string script(const Suggestion& suggestion, std::optional<Thresholds> current,
                   std::optional<std::string_view> output) {
    const core::PlotPalette& palette = core::plot_palette();
    const std::string_view background = palette.background, grid = palette.grid, up_colour = palette.with_case,
                           down_colour = palette.without_case, font = palette.font;
    const Distribution& d = suggestion.distribution;
    const Histogram& histogram = suggestion.histogram;
    const bool one_bin = histogram.counts.size() == 1;
    // S3.19's fix: all values equal give one bin over [v - 0.5, v + 0.5], never an empty x range.
    const double x_low = one_bin ? histogram.lo : d.min - histogram.width;
    const double x_high = one_bin ? histogram.lo + histogram.width : d.max + histogram.width;
    std::string subtitle = detail::integer(d.n) + " genes  \xC2\xB7  dotted lines = candidate cuts";
    if (current) subtitle += "  \xC2\xB7  solid = in use";
    const auto rgb = [](std::string_view colour) { return "rgb \"" + std::string(colour) + "\""; };
    const auto font_at = [font](std::string_view size) {
        return "font \"" + std::string(font) + "," + std::string(size) + "\"";
    };

    std::string s;
    s += "set encoding utf8\n";
    s += "set terminal pngcairo enhanced size 980,620 " + font_at("13") + " background " + rgb(background) + "\n";
    if (output) s += "set output " + single_quoted(*output) + "\n";
    s += "\n";
    s += "set title \"{/:Bold Log-fold-change distribution}\\n{/*0.72 " + subtitle + "}\" tc " + rgb(ink) + " " +
         font_at("16") + " offset 0,-0.4\n";
    s += "set xlabel \"log fold change\" tc " + rgb(label) + " " + font_at("12.5") + " offset 0,0.4\n";
    s += "set ylabel \"genes per bin (log scale)\" tc " + rgb(label) + " " + font_at("12.5") + " offset 1.2,0\n";
    s += "\n";
    s += "set xrange [" + number(x_low) + ":" + number(x_high) + "]\n";
    s += "set logscale y\n";
    s += "set yrange [0.7:*]\n";
    s += "set xtics nomirror out scale 0.4 tc " + rgb(tic) + " " + font_at("11") + "\n";
    s += "set ytics nomirror out scale 0.4 tc " + rgb(tic) + " " + font_at("11") + "\n";
    s += "\n";
    s += "set border 3 lc " + rgb(border) + " lw 1.4\n";
    s += "set grid ytics back lc " + rgb(grid) + " lw 1.0 lt 1\n";
    s += "set tics front\n";
    s += "unset key\n";
    s += "\n";
    s += "set style fill solid 1.0 border lc " + rgb(bar_edge) + "\n";
    s += "set boxwidth " + number(histogram.width * 0.92) + "\n";
    s += "\n";
    // Every candidate cut but current, dotted, merged after rounding to four decimals
    // (suggest_thresholds.py:164-168).
    std::vector<double> guides;
    for (const CandidateRow& row : suggestion.rows) {
        if (row.criterion == "current") continue;
        guides.push_back(round4(row.up));
        guides.push_back(round4(row.down));
    }
    std::sort(guides.begin(), guides.end());
    guides.erase(std::unique(guides.begin(), guides.end()), guides.end());
    for (const double t : guides)
        s += "set arrow from " + number(t) + ",graph 0 to " + number(t) + ",graph 0.92 nohead lc " + rgb(tic) +
             " lw 1.0 dt 3 front\n";
    // The cuts in use, solid blue and red (suggest_thresholds.py:169-175).
    if (current) {
        const std::pair<double, std::pair<std::string_view, std::string_view>> cuts[] = {
            {current->up, {up_colour, "up"}}, {current->down, {down_colour, "down"}}};
        for (const auto& [t, style] : cuts) {
            s += "set arrow from " + number(t) + ",graph 0 to " + number(t) + ",graph 0.86 nohead lc " +
                 rgb(style.first) + " lw 2.4 front\n";
            s += "set label \"" + std::string(style.second) + " " + signed_general(t) + "\" at " + number(t) +
                 ",graph 0.90 center tc " + rgb(style.first) + " " + font_at("11") + " front\n";
        }
    }
    s += "\n";
    // The bins inline, as a data block, so no temporary file is made; empty bins are left out, since
    // the y axis is logarithmic (suggest_thresholds.py:181-186).
    s += "$hist << EOD\n";
    for (std::size_t i = 0; i < histogram.counts.size(); ++i) {
        if (histogram.counts[i] == 0) continue;
        s += number(histogram.lo + (static_cast<double>(i) + 0.5) * histogram.width) + "\t" +
             detail::integer(histogram.counts[i]) + "\n";
    }
    s += "EOD\n";
    s += "plot $hist u 1:2 w boxes lc " + rgb(bar_fill) + "\n";
    return s;
}

// The table's columns, padded for the console.
std::string padded(std::string_view text, std::size_t width, bool right) {
    const std::string fill(width > text.size() ? width - text.size() : 0, ' ');
    return right ? fill + std::string(text) : std::string(text) + fill;
}

std::optional<std::string> setting_text(const core::Context& ctx, std::string_view name) {
    try {
        return ctx.settings.find<std::string>(name);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace

Suggestion suggest_thresholds(const GeneValues& values, std::optional<Thresholds> current) {
    // S3.16, S3.18: the representatives, one per gene, as cudf writes them; FuSION 1 counted a
    // repeated id once per line (suggest_thresholds.py:67-81, 239), 74 rows for the TNFα file's 70 genes.
    const std::size_t n = values.size();
    std::vector<double> sorted(n);
    for (std::size_t i = 0; i < n; ++i) sorted[i] = values.value(i) + 0.0;   // -0.0 as 0.0: a total order
    std::sort(sorted.begin(), sorted.end());

    Suggestion suggestion;
    Distribution& d = suggestion.distribution;
    d.n = n;
    if (n == 0) return suggestion;   // the readers never return an empty GeneValues (values.empty)
    d.min = sorted.front();
    d.max = sorted.back();
    // Summed in ascending order in double (04 "Determinism"), which gives section 04's digits;
    // Python's statistics module summed exactly.
    double sum = 0;
    for (const double v : sorted) sum += v;
    d.mean = sum / static_cast<double>(n);
    double squares = 0;
    for (const double v : sorted) {
        const double deviation = v - d.mean;
        squares += deviation * deviation;
    }
    d.sd = std::sqrt(squares / static_cast<double>(n));
    d.median = median_of_sorted(sorted);
    std::vector<double> deviations(n);
    for (std::size_t i = 0; i < n; ++i) deviations[i] = std::abs(sorted[i] - d.median);
    std::sort(deviations.begin(), deviations.end());
    d.mad = median_of_sorted(deviations);

    // The repeated ids, for the conflicts column (04.C6): with repeats=max_abs none can conflict.
    std::vector<std::size_t> repeated;
    if (values.input().repeats == RepeatRule::classify_each)
        for (std::size_t g = 0; g < n; ++g)
            if (values.line_values(g).size() > 1) repeated.push_back(g);

    const auto add = [&](std::string criterion, double up, double down) {
        CandidateRow row;
        row.criterion = std::move(criterion);
        row.up = up;
        row.down = down;
        // cudf's rule, >= up and <= down (S3.06), by binary search on the sorted values.
        row.n_up = n - static_cast<std::size_t>(std::lower_bound(sorted.begin(), sorted.end(), up) - sorted.begin());
        row.n_down = static_cast<std::size_t>(std::upper_bound(sorted.begin(), sorted.end(), down) - sorted.begin());
        for (const std::size_t g : repeated) {
            const std::span<const double> lines = values.line_values(g);
            const auto side = [&](double v) { return std::pair<bool, bool>(v >= up, v <= down); };
            if (std::any_of(lines.begin() + 1, lines.end(), [&](double v) { return side(v) != side(lines.front()); }))
                ++row.conflicts;
        }
        suggestion.rows.push_back(std::move(row));
    };

    // S3.17: the seven criteria (suggest_thresholds.py:99-115), with the same arithmetic, plus
    // ±k·SD about 0 for k = 2 and 3, the rule the batches use (prompt log entry 055).
    if (current) add("current", current->up, current->down);
    const auto quantile = [&](double p) {   // suggest_thresholds.py:84-91
        const double position = p * static_cast<double>(n - 1);
        const auto low = static_cast<std::size_t>(position);
        const std::size_t high = std::min(low + 1, n - 1);
        const double fraction = position - static_cast<double>(low);
        return sorted[low] * (1 - fraction) + sorted[high] * fraction;
    };
    using Labelled = std::pair<std::string_view, double>;
    constexpr Labelled percents[] = {{"1", 1.0}, {"2.5", 2.5}, {"5", 5.0}};
    constexpr Labelled multiples[] = {{"2", 2.0}, {"3", 3.0}};
    for (const auto& [shown, percent] : percents) {
        const double p = percent / 100.0;
        add("quantile " + std::string(shown) + "%", quantile(1 - p), quantile(p));
    }
    const double scaled = d.mad * mad_to_sd;
    for (const auto& [shown, k] : multiples)
        add("MAD k=" + std::string(shown), d.median + k * scaled, d.median - k * scaled);
    for (const auto& [shown, k] : multiples)
        add("SD k=" + std::string(shown), d.mean + k * d.sd, d.mean - k * d.sd);
    for (const auto& [shown, k] : multiples)
        add("SD about 0 k=" + std::string(shown), k * d.sd, -(k * d.sd));

    // S3.19, 04.C7: 80 equal bins over [min, max] (suggest_thresholds.py:156-161).
    Histogram& histogram = suggestion.histogram;
    if (d.max > d.min) {
        histogram.lo = d.min;
        histogram.width = (d.max - d.min) / static_cast<double>(histogram_bins);
        histogram.counts.assign(histogram_bins, 0);
        for (const double v : sorted) {
            const auto bin = static_cast<std::size_t>((v - histogram.lo) / histogram.width);
            ++histogram.counts[std::min<std::size_t>(bin, histogram_bins - 1)];
        }
    } else {
        histogram.lo = d.min - 0.5;
        histogram.width = 1.0;
        histogram.counts.assign(1, static_cast<std::uint32_t>(n));
    }
    return suggestion;
}

void save_suggestion(const Suggestion& suggestion, const GeneValues& values, core::Context& ctx) {
    const Distribution& d = suggestion.distribution;
    // thresholds.tsv: the candidate rows, numbers exact (1.20, 4A.7).
    std::string tsv = "criterion\tup\tdown\tn_up\tn_down\tn_de\tconflicts\n";
    for (const CandidateRow& row : suggestion.rows)
        tsv += row.criterion + "\t" + number(row.up) + "\t" + number(row.down) + "\t" + detail::integer(row.n_up) +
               "\t" + detail::integer(row.n_down) + "\t" + detail::integer(row.n_up + row.n_down) + "\t" +
               detail::integer(row.conflicts) + "\n";
    detail::write_file(ctx, "thresholds.tsv", tsv);

    core::Json json = core::Json::object();
    json["command"] = "suggest_thresholds";
    json["values"] = detail::values_json(values);
    json["read"] = detail::read_json(values);
    const core::Json fields = detail::suggestion_fields(suggestion);
    for (const auto& [key, field] : fields.items()) json[key] = field;
    detail::write_file(ctx, "thresholds.json", detail::json_text(json));
    detail::save_debug_values(values, ctx);

    // S3.15's report, its four parts, with every number exact so a row copied into cudf gives
    // exactly its counts (S1.27, 4A.7); FuSION 1 rounded to three or four places
    // (suggest_thresholds.py:118-146).
    const ValueReport& report = values.report();
    const std::string prefix = detail::console_prefix(ctx, "suggest_thresholds") + ": ";
    ctx.log.info(prefix + values.impl().path + ": " + detail::count(d.n, "gene", "genes") + " (" +
                 detail::count(report.skipped.size(), "line", "lines") + " skipped, " +
                 detail::count(report.repeated.size(), "id", "ids") + " repeated)");
    ctx.log.info("  distribution");
    ctx.log.info("    min / max      " + number(d.min) + " / " + number(d.max));
    ctx.log.info("    mean / sd      " + number(d.mean) + " / " + number(d.sd));
    ctx.log.info("    median / MAD   " + number(d.median) + " / " + number(d.mad) + "   (MAD*" + number(mad_to_sd) +
                 " = " + number(d.mad * mad_to_sd) + ")");
    if (std::min(std::abs(d.min), std::abs(d.max)) > 0) {
        const bool up_wider = std::abs(d.max) >= std::abs(d.min);
        const double ratio = up_wider ? std::abs(d.max) / std::abs(d.min) : std::abs(d.min) / std::abs(d.max);
        ctx.log.info(std::string("    tail extents   ") + (up_wider ? "up" : "down") + " tail reaches " + number(ratio) +
                     "x further than the other");
    }
    ctx.log.info("  candidate thresholds");
    std::vector<std::vector<std::string>> table{{"criterion", "up", "down", "#up", "#down", "#DE", "%DE", "conflicts"}};
    for (const CandidateRow& row : suggestion.rows) {
        const std::size_t de = row.n_up + row.n_down;
        table.push_back({row.criterion, number(row.up), number(row.down), detail::integer(row.n_up),
                         detail::integer(row.n_down), detail::integer(de),
                         number(100.0 * static_cast<double>(de) / static_cast<double>(d.n)),
                         detail::integer(row.conflicts)});
    }
    std::vector<std::size_t> widths(table.front().size(), 0);
    for (const auto& line : table)
        for (std::size_t c = 0; c < line.size(); ++c) widths[c] = std::max(widths[c], line[c].size());
    for (const auto& line : table) {
        std::string text = "    ";
        for (std::size_t c = 0; c < line.size(); ++c) text += (c > 0 ? "  " : "") + padded(line[c], widths[c], c > 0);
        ctx.log.info(text);
    }
    ctx.log.info("  quantile cuts each tail at a fixed % of genes  -> up/down sets equal in SIZE");
    ctx.log.info("  MAD / SD cut a fixed distance from the centre -> up/down sets equal in EFFECT SIZE");
    ctx.log.info("  MAD is the outlier-resistant counterpart of SD; on skewed data it cuts tighter.");

    core::Json event = core::Json::object();
    event["genes"] = d.n;
    event["n_rows"] = suggestion.rows.size();
    const std::optional<std::string> plot = setting_text(ctx, "plot");
    event["plot"] = plot ? core::Json(detail::utf8_text(*plot)) : core::Json(nullptr);
    ctx.events.emit("expression.suggested", std::move(event));
}

std::string histogram_script(const Suggestion& suggestion, std::optional<Thresholds> current) {
    return script(suggestion, current, std::nullopt);
}

std::string histogram_script(const Suggestion& suggestion, std::optional<Thresholds> current, std::string_view output) {
    return script(suggestion, current, output);
}

core::Json suggestion_answer(const Suggestion& suggestion, const GeneValues& values) {
    core::Json json = core::Json::object();
    json["ok"] = true;
    json["query"] = "suggest_thresholds";
    json["values"] = detail::values_json(values);
    json["read"] = detail::read_json(values);
    const core::Json fields = detail::suggestion_fields(suggestion);
    for (const auto& [key, field] : fields.items()) json[key] = field;
    return json;
}

}  // namespace fusion::expression
