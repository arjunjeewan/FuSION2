// fusion expression: classification (cudf), the observations, and the saved lists (section 04,
// "Classification", "Observations"; S3.01, S3.02, S3.06 to S3.13; 04.C4, 04.C5, 04.C11).
// FuSION 1's handler is Linux/src/main.cc:2629-2699; its list reader is
// Linux/src/GraphManagerNew.cc:18688-18708.
#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "internal.hpp"

namespace fusion::expression {

// S3.06: up when value >= up, down when value <= down, both inclusive, on doubles. FuSION 1 made the
// two tests on 32-bit floats (main.cc:2681-2688), so 1.0099999999 counted as up at 1.01.
Regulation regulation_of(double value, Thresholds thresholds) noexcept {
    if (value >= thresholds.up) return Regulation::up;
    if (value <= thresholds.down) return Regulation::down;
    return Regulation::none;
}

// ---------------------------------------------------------------- Observations (D1)

Observations::Observations() : impl_(std::make_shared<const Impl>()) {}
Observations::Observations(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
const Observations::Impl& Observations::impl() const noexcept { return *impl_; }

std::span<const core::GeneId> Observations::up() const noexcept {
    return std::span<const core::GeneId>(impl_->observed).first(impl_->n_up);
}
std::span<const core::GeneId> Observations::down() const noexcept {
    return std::span<const core::GeneId>(impl_->observed).subspan(impl_->n_up);
}
std::span<const core::GeneId> Observations::observed() const noexcept { return impl_->observed; }

Regulation Observations::regulation(core::GeneId id) const noexcept {
    const auto at = impl_->regulation.find(id);
    return at == impl_->regulation.end() ? Regulation::none : at->second;
}

bool Observations::empty() const noexcept { return impl_->observed.empty(); }
std::optional<Thresholds> Observations::thresholds() const noexcept { return impl_->thresholds; }
std::span<const core::GeneId> Observations::not_de_removed() const noexcept { return impl_->not_de_removed; }

namespace {

constexpr std::size_t console_names = 10;

std::string number(double value) { return core::format_number(value); }

// S3.02 and S3.07 are the command table's checks (01.C9); repeated here so no caller can classify
// with them broken. FuSION 1 took any stof prefix (main.cc:2644-2645) and wrote every gene between
// the thresholds into both lists when up <= down.
void check_thresholds(Thresholds thresholds) {
    const auto setting = [](const char* name) { return core::Where{std::string(), 0, std::string(name)}; };
    if (!std::isfinite(thresholds.up))
        throw core::Failure("batch.bad_value", "the up threshold is not a finite number", setting("up"));
    if (!std::isfinite(thresholds.down))
        throw core::Failure("batch.bad_value", "the down threshold is not a finite number", setting("down"));
    if (!(thresholds.down < thresholds.up))
        throw core::Failure("batch.bad_value",
                            "the up threshold " + number(thresholds.up) + " must be above the down threshold " +
                                number(thresholds.down),
                            setting("down"));
}

// "hsa5806 is up on lines 61, 62 (9.82, 9.82) and none on line 66 (1.95) (up >= 2, down <= -2)"
std::string conflict_message(const GeneValues& values, std::size_t gene, Thresholds thresholds, std::size_t others) {
    const std::span<const double> line_values = values.line_values(gene);
    const std::span<const std::uint32_t> line_numbers = values.line_numbers(gene);
    std::vector<Regulation> classes;   // in order of first appearance
    for (const double value : line_values) {
        const Regulation r = regulation_of(value, thresholds);
        if (std::find(classes.begin(), classes.end(), r) == classes.end()) classes.push_back(r);
    }
    std::string message = detail::utf8_text(values.impl().names[gene]) + " is ";
    for (std::size_t c = 0; c < classes.size(); ++c) {
        if (c > 0) message += c + 1 == classes.size() ? " and " : ", ";
        std::vector<std::uint32_t> lines;
        std::string shown_values;
        for (std::size_t k = 0; k < line_values.size(); ++k) {
            if (regulation_of(line_values[k], thresholds) != classes[c]) continue;
            lines.push_back(line_numbers[k]);
            shown_values += (shown_values.empty() ? "" : ", ") + number(line_values[k]);
        }
        message += std::string(detail::name(classes[c])) + (lines.size() == 1 ? " on line " : " on lines ") +
                   detail::join_numbers(lines, ", ") + " (" + shown_values + ")";
    }
    message += " (up >= " + number(thresholds.up) + ", down <= " + number(thresholds.down) + ")";
    if (others > 0) message += "; " + detail::count(others, "more repeated id conflicts", "more repeated ids conflict");
    return message;
}

}  // namespace

core::Outcome<Observations> classify(const GeneValues& values, const ClassifyRequest& request, core::Context& ctx) {
    const Thresholds thresholds = request.thresholds;
    check_thresholds(thresholds);
    const bool max_abs = values.input().repeats == RepeatRule::max_abs;

    auto impl = std::make_shared<Observations::Impl>();
    impl->thresholds = thresholds;
    impl->not_de_file = request.not_de_file;

    // Not DE (04.C4, S8.15): each listed id is set to none before its lines are classified, so the
    // override also settles a repeated id whose lines disagree.
    std::unordered_set<core::GeneId, core::HandleHash> not_de;   // lookups only
    for (const core::GeneId id : request.not_de) {
        if (!not_de.insert(id).second) continue;
        const std::optional<std::size_t> gene = values.find(id);
        if (!gene) {
            impl->not_de_unknown.push_back(id);
            continue;
        }
        bool changed = false;
        if (max_abs) {
            changed = regulation_of(values.value(*gene), thresholds) != Regulation::none;
        } else {
            for (const double value : values.line_values(*gene))
                changed = changed || regulation_of(value, thresholds) != Regulation::none;
        }
        (changed ? impl->not_de_removed : impl->not_de_unchanged).push_back(id);
    }

    // S3.06 on every line of every other gene (S3.08), or on the representative with repeats=max_abs
    // (04.C3). FuSION 1 wrote a gene once per line, into both lists when its lines disagreed
    // (main.cc:2677-2689); 2.0 writes each gene once and stops on a disagreement.
    std::vector<std::size_t> up, down;
    std::optional<std::size_t> conflict;
    std::size_t conflicts = 0;
    for (std::size_t g = 0; g < values.size(); ++g) {
        if (not_de.contains(values.id(g))) continue;
        Regulation r = Regulation::none;
        if (max_abs) {
            r = regulation_of(values.value(g), thresholds);
        } else {
            const std::span<const double> lines = values.line_values(g);
            r = regulation_of(lines.front(), thresholds);
            const bool split = std::any_of(lines.begin() + 1, lines.end(),
                                           [&](double v) { return regulation_of(v, thresholds) != r; });
            if (split) {
                if (!conflict) conflict = g;
                ++conflicts;
                continue;
            }
        }
        if (r == Regulation::up) up.push_back(g);
        if (r == Regulation::down) down.push_back(g);
    }
    if (conflict)
        throw core::Failure("observations.conflict", conflict_message(values, *conflict, thresholds, conflicts - 1),
                            detail::where(values.impl().path, values.line_numbers(*conflict).front()));

    const auto add = [&](std::size_t gene, Regulation r) {
        impl->observed.push_back(values.id(gene));
        impl->names.push_back(values.impl().names[gene]);
        impl->regulation.emplace(values.id(gene), r);
    };
    for (const std::size_t g : up) add(g, Regulation::up);
    impl->n_up = up.size();
    for (const std::size_t g : down) add(g, Regulation::down);

    if (!impl->not_de_unknown.empty()) {
        std::string message = detail::count(impl->not_de_unknown.size(), "Not DE id is", "Not DE ids are") +
                              " not in " + values.impl().path;
        if (request.interner) {
            message += ":";
            for (std::size_t i = 0; i < impl->not_de_unknown.size() && i < console_names; ++i)
                message += (i > 0 ? ", " : " ") + detail::utf8_text(request.interner->str(impl->not_de_unknown[i]));
            if (impl->not_de_unknown.size() > console_names)
                message += ", and " + detail::integer(impl->not_de_unknown.size() - console_names) + " more";
        }
        ctx.log.warning("not_de.unknown_ids", message);
    }

    core::Json event = core::Json::object();
    event["thresholds"] = detail::thresholds_json(thresholds);
    event["n_up"] = up.size();
    event["n_down"] = down.size();
    event["n_not_de_removed"] = impl->not_de_removed.size();
    ctx.events.emit("expression.classified", std::move(event));

    core::Outcome<Observations> outcome{Observations(std::move(impl)), std::nullopt};
    // 04.C5: no gene past the thresholds is a reported result; the two lists are still written, empty.
    if (outcome.value.empty())
        outcome.report = core::Report{"observations.none", "no gene passes up >= " + number(thresholds.up) +
                                                               " or down <= " + number(thresholds.down)};
    return outcome;
}

// ---------------------------------------------------------------- the saved step (S3.09, 0019)

namespace {

struct ListTarget {
    std::string argument;                       // "up_out"
    std::string name;                           // in the step directory
    std::optional<std::filesystem::path> also;  // an absolute path, written as well (2B.3)
};

ListTarget list_target(std::string argument, const std::string& given, std::string_view fallback) {
    const std::string text = given.empty() ? std::string(fallback) : given;
    const std::filesystem::path path = core::path_from_utf8(text);
    if (!path.is_absolute()) return ListTarget{std::move(argument), text, std::nullopt};
    if (!path.has_filename())
        throw core::Failure("batch.bad_value", argument + " names a folder, not a file: " + text,
                            core::Where{std::string(), 0, argument});
    return ListTarget{std::move(argument), core::to_utf8(path.filename()), path};
}

// Two names of one file in the step directory; case is ignored where the file system usually does.
bool same_name(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::string x = core::to_utf8(a.lexically_normal()), y = core::to_utf8(b.lexically_normal());
#if defined(_WIN32) || defined(__APPLE__)
    const auto lower = [](std::string& s) {   // ASCII only: no locale (09 "Determinism")
        for (char& c : s)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    };
    lower(x);
    lower(y);
#endif
    return x == y;
}

// By identity when both exist (exact on every file system), else by their canonical paths (2A.7).
bool same_file(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code error;
    if (std::filesystem::exists(a, error) && std::filesystem::exists(b, error))
        return std::filesystem::equivalent(a, b, error);
    const std::filesystem::path x = std::filesystem::weakly_canonical(a, error);
    const std::filesystem::path y = std::filesystem::weakly_canonical(b, error);
    return same_name(x, y);
}

// S3.10: FuSION 1 truncated both outputs before checking anything (main.cc:2650-2652), so one path for
// both lists, or a list over the input, destroyed data. 09's check refuses such clashes before the
// run; this repeats it at the step, by identity, before anything is written.
void check_clashes(const ListTarget& up, const ListTarget& down, const GeneValues& values) {
    if (same_name(core::path_from_utf8(up.name), core::path_from_utf8(down.name)))
        throw core::Failure("output.clash", "up_out and down_out are the same file " + up.name + " in the step folder");
    if (up.also && down.also && same_file(*up.also, *down.also))
        throw core::Failure("output.clash", "up_out and down_out are the same file " + core::to_utf8(*up.also));
    for (const ListTarget* target : {&up, &down}) {
        if (!target->also) continue;
        for (const std::filesystem::path& input : {values.impl().file, values.input().path})
            if (input.is_absolute() && same_file(*target->also, input))
                throw core::Failure("output.clash",
                                    target->argument + " is the value file " + core::to_utf8(input) + " itself");
    }
}

void write_list(core::Context& ctx, const ListTarget& target, const std::vector<std::string>& names,
                std::size_t begin, std::size_t end) {
    // S3.09: one id per line, LF, input order, no header; an empty list is a 0-byte file.
    std::string text;
    for (std::size_t i = begin; i < end; ++i) {
        text += names[i];
        text += '\n';
    }
    core::OutputFile file = ctx.files.create(target.name);
    if (target.also) file.also_at(*target.also);
    file.out().write(text.data(), static_cast<std::streamsize>(text.size()));
    file.commit();
}

std::string shown_target(const ListTarget& target, const core::Context& ctx) {
    const std::filesystem::path step = ctx.files.step_dir();
    std::string text = step.has_filename() ? core::to_utf8(step.filename()) + "/" + target.name : target.name;
    if (target.also) text += " (also " + core::to_utf8(*target.also) + ")";
    return text;
}

// S3.11's first console line: "inputs/x.tsv: 74 lines, 70 genes, 0 skipped, 3 repeated: ...".
std::string read_line(const GeneValues& values) {
    const GeneValues::Impl& impl = values.impl();
    const ValueReport& report = impl.report;
    std::string text = impl.path + ": " + detail::count(report.lines, "line", "lines") + ", " +
                       detail::count(values.size(), "gene", "genes") + ", " + detail::integer(report.skipped.size()) +
                       " skipped";
    for (std::size_t i = 0; i < report.skipped.size() && i < console_names; ++i)
        text += (i == 0 ? ": line " : ", ") + detail::integer(report.skipped[i].line) + " (" +
                std::string(detail::name(report.skipped[i].reason)) + ")";
    if (report.skipped.size() > console_names)
        text += ", and " + detail::integer(report.skipped.size() - console_names) + " more";
    text += ", " + detail::integer(report.repeated.size()) + " repeated";
    for (std::size_t i = 0; i < report.repeated.size() && i < console_names; ++i)
        text += (i == 0 ? ": " : ", ") + detail::utf8_text(impl.names[impl.index.at(report.repeated[i].id)]) + " (" +
                (i == 0 ? "lines " : "") + detail::join_numbers(report.repeated[i].lines, ", ") + ")";
    if (report.repeated.size() > console_names)
        text += ", and " + detail::integer(report.repeated.size() - console_names) + " more";
    if (report.unmapped_lines > 0)
        text += ", " + detail::integer(report.unmapped_lines) + " unmapped";
    return text;
}

}  // namespace

void save_observations(const Observations& observations, const GeneValues& values, const ListNames& names,
                       const core::Interner& interner, core::Context& ctx) {
    const Observations::Impl& impl = observations.impl();
    // Where the lists land (2B.3, 04.C11): a relative or omitted name in the step directory; an
    // absolute one there, under its own name, and at the path too. FuSION 1 wrote only the path.
    const ListTarget up = list_target("up_out", names.up, "up_reg");
    const ListTarget down = list_target("down_out", names.down, "down_reg");
    check_clashes(up, down, values);
    write_list(ctx, up, impl.names, 0, impl.n_up);
    if (up.also && down.also && same_file(*up.also, *down.also))   // again, just before the second
        throw core::Failure("output.clash", "up_out and down_out are the same file " + core::to_utf8(*down.also));
    write_list(ctx, down, impl.names, impl.n_up, impl.observed.size());

    const std::size_t n_up = impl.n_up, n_down = impl.observed.size() - impl.n_up;
    core::Json json = core::Json::object();
    json["command"] = "cudf";
    json["values"] = detail::values_json(values);
    json["read"] = detail::read_json(values);
    json["thresholds"] = detail::thresholds_json(impl.thresholds);
    json["not_de"] = detail::not_de_json(observations, interner);
    json["n_up"] = n_up;
    json["n_down"] = n_down;
    json["n_none"] = values.size() >= n_up + n_down ? values.size() - n_up - n_down : 0;
    json["lists"] = core::Json::object();   // the saved step's files (0019)
    json["lists"]["up"] = detail::utf8_text(up.name);
    json["lists"]["down"] = detail::utf8_text(down.name);
    detail::write_file(ctx, "cudf.json", detail::json_text(json));
    detail::save_debug_values(values, ctx);

    // S3.11: FuSION 1 printed nothing (main.cc:2680 is a commented-out trace).
    const std::string prefix = detail::console_prefix(ctx, "cudf") + ": ";
    ctx.log.info(prefix + read_line(values));
    std::string classes = prefix;
    if (impl.thresholds)
        classes += "up (>= " + number(impl.thresholds->up) + ") " + detail::count(n_up, "gene", "genes") +
                   ", down (<= " + number(impl.thresholds->down) + ") " + detail::count(n_down, "gene", "genes");
    else
        classes += "up " + detail::count(n_up, "gene", "genes") + ", down " + detail::count(n_down, "gene", "genes");
    ctx.log.info(classes + " -> " + shown_target(up, ctx) + ", " + shown_target(down, ctx));
    if (impl.not_de_file)
        ctx.log.info(prefix + "Not DE: " + detail::integer(impl.not_de_removed.size()) + " removed, " +
                     detail::integer(impl.not_de_unchanged.size()) + " already none, " +
                     detail::integer(impl.not_de_unknown.size()) + " not in the values");
}

// S3.12: whitespace-separated ids, so several per line, blank lines and \r all work, as FuSION 1's
// `ifs >> id` did (GraphManagerNew.cc:18699); repeats dropped, as its std::set<int> did. Each id
// passes the id rule (02.C3). A missing file is a failure (0021), where FuSION 1 printed a line and
// went on with an empty list (GraphManagerNew.cc:18693-18697).
std::vector<core::GeneId> read_id_list(const std::filesystem::path& file, const network::IdMap& map,
                                       core::Interner& interner) {
    const std::string bytes = detail::read_bytes(file);
    std::string_view text = bytes;
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    std::vector<core::GeneId> ids;
    std::unordered_set<core::GeneId, core::HandleHash> seen;   // lookups only
    for (std::size_t i = 0; i < text.size();) {
        while (i < text.size() && space(text[i])) ++i;
        const std::size_t begin = i;
        while (i < text.size() && !space(text[i])) ++i;
        if (i == begin) continue;
        const core::GeneId id = map.canonical(text.substr(begin, i - begin), interner);
        if (seen.insert(id).second) ids.push_back(id);
    }
    return ids;
}

Observations load_lists(const std::filesystem::path& up, const std::filesystem::path& down, const network::IdMap& map,
                        core::Interner& interner, core::Context& ctx) {
    const std::vector<core::GeneId> up_ids = read_id_list(up, map, interner);
    const std::vector<core::GeneId> down_ids = read_id_list(down, map, interner);
    auto impl = std::make_shared<Observations::Impl>();
    for (const core::GeneId id : up_ids) {
        impl->observed.push_back(id);
        impl->names.emplace_back(interner.str(id));
        impl->regulation.emplace(id, Regulation::up);
    }
    impl->n_up = up_ids.size();
    // An id in both lists is up, as the solve has always treated a node in both sets (S5.12;
    // GraphManagerNew.cc:28476 before 28498); FuSION 1 said nothing, 2.0 warns.
    std::vector<core::GeneId> overlap;
    for (const core::GeneId id : down_ids) {
        if (impl->regulation.contains(id)) {
            overlap.push_back(id);
            continue;
        }
        impl->observed.push_back(id);
        impl->names.emplace_back(interner.str(id));
        impl->regulation.emplace(id, Regulation::down);
    }
    if (!overlap.empty()) {
        std::string message = detail::count(overlap.size(), "id is", "ids are") + " in both " + core::to_utf8(up) +
                              " and " + core::to_utf8(down) + ", and each counts as up:";
        for (std::size_t i = 0; i < overlap.size() && i < console_names; ++i)
            message += (i > 0 ? ", " : " ") + detail::utf8_text(interner.str(overlap[i]));
        if (overlap.size() > console_names) message += ", and " + detail::integer(overlap.size() - console_names) + " more";
        ctx.log.warning("lists.overlap", message);
    }
    return Observations(std::move(impl));
}

// The preview's answer (04.C8, S8.14): cudf.json's fields with every gene's class, from the same
// classify() the step runs, so the preview is exactly what cudf will write.
core::Json cudf_answer(const GeneValues& values, const Observations& observations, const core::Interner& interner) {
    const Observations::Impl& impl = observations.impl();
    std::unordered_set<core::GeneId, core::HandleHash> not_de(impl.not_de_removed.begin(), impl.not_de_removed.end());
    not_de.insert(impl.not_de_unchanged.begin(), impl.not_de_unchanged.end());
    const std::size_t n_up = impl.n_up, n_down = impl.observed.size() - impl.n_up;
    core::Json genes = core::Json::array();
    for (std::size_t g = 0; g < values.size(); ++g) {
        core::Json gene = core::Json::object();
        gene["id"] = detail::utf8_text(values.impl().names[g]);
        gene["value"] = values.value(g);
        gene["class"] = detail::name(observations.regulation(values.id(g)));
        if (not_de.contains(values.id(g))) gene["not_de"] = true;
        genes.push_back(std::move(gene));
    }
    core::Json json = core::Json::object();
    json["ok"] = true;
    json["query"] = "cudf";
    json["values"] = detail::values_json(values);
    json["read"] = detail::read_json(values);
    json["thresholds"] = detail::thresholds_json(impl.thresholds);
    json["not_de"] = detail::not_de_json(observations, interner);
    json["n_up"] = n_up;
    json["n_down"] = n_down;
    json["n_none"] = values.size() >= n_up + n_down ? values.size() - n_up - n_down : 0;
    json["genes"] = std::move(genes);
    return json;
}

}  // namespace fusion::expression
