// fusion run: input copies and the generic checks before a run (09.C3, decision 0021, 2A.7, 09.C12).
//
// FuSION 1 checked no input before it ran: each command opened its files when its line came, and a
// missing file was found hours into a run, or not at all (spec S8.30; decision 0021). Inputs were
// read in place, so a file edited during a run of up to 52 h changed what later lines read, and a
// finished run could not be redone once its inputs moved (09.C3). Here every input is copied into
// the run's inputs/ and hashed in the same read, the check runs on the copies before the first
// step, and steps read only the copies, so the recorded hash is of the bytes parsed (1.13).
#include <algorithm>
#include <format>
#include <fstream>

#include "internal.hpp"

namespace fusion::run {

namespace detail {

namespace {

std::string lower_ascii(std::string text) {
    for (char& c : text)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return text;
}

// Two paths name one file: by identity when both exist (a hard link, a symbolic link, a case-only
// difference on macOS and Windows), else by their normal text (2A.7).
bool same_file(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::exists(a, ec) && fs::exists(b, ec)) {
        const bool same = fs::equivalent(a, b, ec);
        if (!ec) return same;
    }
    return a == b;
}

}  // namespace

std::optional<Problem> probe_input(const fs::path& file) {
    std::error_code ec;
    const fs::file_status status = fs::status(file, ec);
    if (status.type() == fs::file_type::not_found)
        return Problem{"input.missing", core::to_utf8(file) + " does not exist"};
    if (ec) return Problem{"input.unreadable", "cannot read " + core::to_utf8(file) + ": " + ec.message()};
    if (!fs::is_regular_file(status)) return Problem{"input.not_file", core::to_utf8(file) + " is not a regular file"};
    std::ifstream in(file, std::ios::binary);
    if (!in) return Problem{"input.unreadable", "cannot read " + core::to_utf8(file)};
    return std::nullopt;
}

Inputs::Inputs(fs::path run_dir, fs::path base) : run_dir_(std::move(run_dir)), base_(std::move(base)) {}

std::optional<InputCopy> Inputs::find(const fs::path& resolved) const {
    const auto it = by_original_.find(core::to_utf8(resolved));
    if (it == by_original_.end()) return std::nullopt;
    return it->second;
}

std::string Inputs::unique_name(const fs::path& folder, const fs::path& file_name) {
    std::string stem = core::to_utf8(file_name.stem());
    const std::string ext = core::to_utf8(file_name.extension());
    if (stem.empty() && ext.empty()) stem = "input";
    for (int n = 1;; ++n) {
        const std::string name = n == 1 ? stem + ext : std::format("{}-{}{}", stem, n, ext);
        const std::string rel = core::to_utf8(folder / core::path_from_utf8(name));
        std::error_code ec;
        if (names_.contains(lower_ascii(rel)) || fs::exists(run_dir_ / core::path_from_utf8(rel), ec)) continue;
        names_.insert(lower_ascii(rel));
        return rel;
    }
}

std::optional<InputCopy> Inputs::take(const fs::path& resolved, const fs::path& folder, bool fresh,
                                      std::optional<Problem>& problem) {
    problem.reset();
    const std::string key = core::to_utf8(resolved);
    const auto known = by_original_.find(key);
    if (known != by_original_.end() && !fresh) return known->second;
    if (auto p = probe_input(resolved)) {
        problem = std::move(p);
        return std::nullopt;
    }
    std::ifstream in(resolved, std::ios::binary);
    if (!in) {
        problem = Problem{"input.unreadable", "cannot read " + core::to_utf8(resolved)};
        return std::nullopt;
    }

    InputCopy copy;
    copy.original = resolved;
    core::Sha256Builder hash;
    std::vector<char> buffer(std::size_t{1} << 20);
    const bool in_place = is_inside(resolved, run_dir_);
    std::optional<core::OutputFile> out;
    std::string rel;
    if (in_place) {
        // The frontend's batch and override files, and earlier steps' outputs, are hashed where they lie.
        copy.copy = resolved;
        copy.copy_rel = relative_utf8(resolved, run_dir_);
    } else {
        rel = unique_name(folder, resolved.filename());
        copy.copy = run_dir_ / core::path_from_utf8(rel);
        copy.copy_rel = rel;
        out.emplace(copy.copy);
    }
    // One read: every chunk is hashed and written to the copy (09.C3).
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto n = static_cast<std::size_t>(in.gcount());
        if (n == 0) break;
        hash.update(std::string_view(buffer.data(), n));
        if (out) out->out().write(buffer.data(), static_cast<std::streamsize>(n));
        copy.size += n;
    }
    if (in.bad()) {
        if (!rel.empty()) names_.erase(lower_ascii(rel));   // the uncommitted copy's temp file is removed
        problem = Problem{"input.unreadable", "cannot read " + core::to_utf8(resolved) + ": the read failed"};
        return std::nullopt;
    }
    copy.sha256 = hash.finish();
    if (out) {
        if (known != by_original_.end() && known->second.sha256 == copy.sha256) {
            names_.erase(lower_ascii(rel));      // unchanged since its last copy: keep that one
            return known->second;
        }
        out->commit();
    }
    ++count_;
    bytes_ += copy.size;
    by_original_[key] = copy;
    return copy;
}

std::string member_message(std::string_view argument, const fs::path& list, const Problem& problem) {
    return std::format("{}: a file listed in {}: {}", argument, core::to_utf8(list), problem.message);
}

core::Diagnostic diagnostic(core::Severity severity, const StepSpec& step, std::string_view argument,
                            std::string code, std::string message, const fs::path& path) {
    core::Diagnostic d;
    d.severity = severity;
    d.line = step.line;
    d.code = std::move(code);
    d.message = std::move(message);
    if (!step.command.empty()) d.command = step.command;
    if (!argument.empty()) d.setting = std::string(argument);
    if (!path.empty()) d.path = core::to_utf8(path);
    return d;
}

void sort_diagnostics(std::vector<core::Diagnostic>& list) {
    std::stable_sort(list.begin(), list.end(), [](const core::Diagnostic& a, const core::Diagnostic& b) {
        const auto key = [](const core::Diagnostic& d) { return std::tuple(d.point.value_or(0), d.line, d.column); };
        return key(a) < key(b);
    });
}

void check_outputs(std::span<const StepSpec> steps, const fs::path& base, std::vector<core::Diagnostic>& out) {
    struct Absolute { fs::path path; const StepSpec* step; };
    std::vector<Absolute> absolutes;
    for (const StepSpec& step : steps) {
        // Inside one step directory: a relative output by its name, an absolute one by its file name,
        // since its copy is kept there too (2B.3).
        std::map<std::string, fs::path> names;
        for (const fs::path& output : step.outputs) {
            if (output.empty()) continue;
            const std::string name =
                core::to_utf8(output.is_absolute() ? output.filename() : output.lexically_normal());
            if (const auto [it, added] = names.emplace(name, output); !added)
                out.push_back(diagnostic(core::Severity::error, step, {}, "output.clash",
                                         std::format("{}: {} and {} are one file in its step directory", step.command,
                                                     core::to_utf8(it->second), core::to_utf8(output)),
                                         output));
            if (!output.is_absolute()) continue;
            const fs::path path = normal(output, base);
            for (const Absolute& other : absolutes)
                if (same_file(path, other.path))
                    out.push_back(diagnostic(core::Severity::error, step, {}, "output.clash",
                                             std::format("{}: {} is also written by line {}", step.command,
                                                         core::to_utf8(path), other.step->line),
                                             path));
            absolutes.push_back({path, &step});
        }
    }
    // An output may not be an input, unless the batch makes that input for a later step (01.C12).
    for (const StepSpec& step : steps)
        for (const InputSpec& input : step.inputs) {
            if (input.path.empty() || input.made_by_line) continue;
            const fs::path path = normal(input.path, base);
            for (const Absolute& output : absolutes)
                if (same_file(path, output.path))
                    out.push_back(diagnostic(core::Severity::error, step, input.argument, "output.clash",
                                             std::format("{}: the input {} is written by line {}", step.command,
                                                         core::to_utf8(path), output.step->line),
                                             path));
        }
}

}  // namespace detail

// 09.C12: every check a real run makes on its inputs, on the originals, with no run directory. It
// copies nothing and records nothing. Relative paths resolve against the current directory, so the
// caller passes them resolved against the batch file's folder (01.C2).
core::CheckReport Run::check_only(std::span<const StepSpec> steps) {
    namespace fs = std::filesystem;
    core::CheckReport report;
    std::error_code ec;
    const fs::path base = fs::current_path(ec);
    for (const StepSpec& step : steps)
        for (const InputSpec& input : step.inputs) {
            if (input.made_by_line) continue;    // checked just before its step (01.C12)
            if (input.path.empty()) {
                if (input.required)
                    report.diagnostics.push_back(detail::diagnostic(core::Severity::error, step, input.argument,
                                                                    "input.missing",
                                                                    "the required input " + input.argument +
                                                                        " is not given",
                                                                    {}));
                continue;                        // an optional input left out turns its feature off (0021)
            }
            const fs::path path = detail::normal(input.path, base);
            if (auto problem = detail::probe_input(path)) {
                report.diagnostics.push_back(detail::diagnostic(core::Severity::error, step, input.argument,
                                                                problem->code,
                                                                input.argument + ": " + problem->message, path));
                continue;
            }
            if (!input.is_list || !input.members) continue;
            std::vector<fs::path> members;
            try {
                members = input.members(path);
            } catch (const core::Failure& failure) {
                report.diagnostics.push_back(detail::diagnostic(core::Severity::error, step, input.argument,
                                                                failure.code(), failure.what(), path));
            }
            for (const fs::path& member : members) {
                const fs::path resolved = detail::normal(member, path.parent_path());
                if (auto problem = detail::probe_input(resolved))
                    report.diagnostics.push_back(detail::diagnostic(
                        core::Severity::error, step, input.argument, "input.list_member",
                        detail::member_message(input.argument, path, *problem), resolved));
            }
        }
    detail::check_outputs(steps, base, report.diagnostics);
    detail::sort_diagnostics(report.diagnostics);
    return report;
}

}  // namespace fusion::run
