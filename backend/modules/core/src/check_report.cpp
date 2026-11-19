// fusion core: the one type behind `fusion --check --json` (cross-section item 1.9; 09.C12, 01.C9).
// FuSION 1 checked nothing before a run: problems surfaced mid-run or never (spec S6.11,
// decision 0021).
#include <algorithm>
#include <tuple>

#include "fusion/core/core.hpp"

namespace fusion::core {

namespace {

std::string_view severity_name(Severity s) noexcept {
    switch (s) {
    case Severity::error: return "error";
    case Severity::warning: return "warning";
    case Severity::note: return "note";
    }
    return "error";
}

Json optional_text(const std::optional<std::string>& s) { return s ? Json(*s) : Json(nullptr); }

}  // namespace

bool CheckReport::ok() const noexcept {
    return std::none_of(diagnostics.begin(), diagnostics.end(),
                        [](const Diagnostic& d) { return d.severity == Severity::error; });
}

Json CheckReport::to_json() const {
    // Section 09's example, field for field. Diagnostics go out sorted by point (none first),
    // line and column, keeping the order found among equals.
    std::vector<const Diagnostic*> sorted;
    sorted.reserve(diagnostics.size());
    for (const Diagnostic& d : diagnostics) sorted.push_back(&d);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Diagnostic* a, const Diagnostic* b) {
        return std::tie(a->point, a->line, a->column) < std::tie(b->point, b->line, b->column);
    });

    Json out = Json::object();
    out["ok"] = ok();
    out["batch"] = to_utf8(batch);
    out["sha256"] = batch_sha256.hex();
    out["points"] = points;
    Json list = Json::array();
    for (const Diagnostic* d : sorted) {
        Json item = Json::object();
        item["severity"] = severity_name(d->severity);
        item["line"] = d->line;
        item["column"] = d->column;
        item["point"] = d->point ? Json(*d->point) : Json(nullptr);
        item["code"] = d->code;
        item["command"] = optional_text(d->command);
        item["setting"] = optional_text(d->setting);
        item["path"] = optional_text(d->path);
        item["message"] = d->message;
        list.push_back(std::move(item));
    }
    out["diagnostics"] = std::move(list);
    Json features_list = Json::array();
    for (const Feature& f : features) {
        Json item = Json::object();
        item["line"] = f.line;
        item["command"] = f.command;
        item["feature"] = f.feature;
        item["on"] = f.on;
        item["because"] = f.because;
        features_list.push_back(std::move(item));
    }
    out["features"] = std::move(features_list);
    return out;
}

}  // namespace fusion::core
