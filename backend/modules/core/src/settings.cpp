// fusion core: the settings registry (section 09, "The settings registry"; cross-section item 1.16).
//
// FuSION 1 fixed most result-changing behaviour at compile time (Linux/Makefile:10, spec S1.29)
// and kept the rest in positional tokens parsed in place (Linux/src/main.cc:4521-4554). Here every
// setting of a step is a Setting with its spec, value and source, recorded in run.json (E1).
#include <algorithm>
#include <type_traits>

#include "fusion/core/core.hpp"

namespace fusion::core {

std::string key(const SettingSpec& spec) {
    std::string k;
    k.reserve(spec.module.size() + 1 + spec.name.size());
    k.append(spec.module).append(".").append(spec.name);
    return k;
}

Json to_json(const Value& value) {
    return std::visit(
        [](const auto& v) -> Json {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                Json list = Json::array();
                for (const auto& item : v) list.push_back(item);
                return list;
            } else {
                return Json(v);
            }
        },
        value);
}

std::string_view source_name(Source source) noexcept {
    switch (source) {
    case Source::default_value: return "default";
    case Source::positional: return "positional";
    case Source::named: return "named";
    case Source::set_option: return "set";
    case Source::run_line: return "run_line";
    }
    return "default";
}

Settings::Settings(std::vector<Setting> settings) : settings_(std::move(settings)) {
    // "One step's settings, sorted by key", in byte order (09's determinism rules).
    std::stable_sort(settings_.begin(), settings_.end(), [](const Setting& a, const Setting& b) {
        return stable_compare(key(*a.spec), key(*b.spec)) < 0;
    });
}

const Setting* Settings::find_setting(std::string_view name) const noexcept {
    for (const Setting& s : settings_)
        if (s.spec->name == name) return &s;
    return nullptr;
}

const Setting& Settings::at(std::string_view name) const {
    if (const Setting* s = find_setting(name)) return *s;
    throw Failure("setting.unknown", "no setting named '" + std::string(name) + "' in this step",
                  Where{{}, 0, std::string(name)});
}

void Settings::throw_left_out(std::string_view name) {
    throw Failure("setting.left_out", "the setting '" + std::string(name) + "' was left out",
                  Where{{}, 0, std::string(name)});
}

void Settings::set_effective(std::string_view name, Value value, std::string reason) {
    const Setting& found = at(name);
    Setting& s = settings_[static_cast<std::size_t>(&found - settings_.data())];
    s.effective = std::move(value);
    s.reason = std::move(reason);
}

Json to_json(const Settings& settings) {
    // run.json's "settings" list (09, "run.json"): key, value, source, and effective with its
    // reason when the step changed the value (S4.16). An optional setting left out has no value
    // (decision 0021), recorded as null.
    Json list = Json::array();
    for (const Setting& s : settings.all()) {
        Json item = Json::object();
        item["key"] = key(*s.spec);
        item["value"] = s.value ? to_json(*s.value) : Json(nullptr);
        item["source"] = source_name(s.source);
        if (s.effective) {
            item["effective"] = to_json(*s.effective);
            item["reason"] = s.reason;
        }
        list.push_back(std::move(item));
    }
    return list;
}

}  // namespace fusion::core
