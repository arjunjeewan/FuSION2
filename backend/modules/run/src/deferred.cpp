// fusion run: saved-step fingerprints (09.C5), and the parts of section 09 that come after the
// first version: the registry of a run's saved steps (09.C5, decision 0019) and the shared store
// (09.C13). Decision 0038 builds them in a later iteration; until then the declarations stay and
// behave safely: a run with a parent or a load from a run directory fails with a clear message, and
// the store is off, which never changes a result (decision 0023).
//
// FuSION 1 had neither: its users re-ran whole batches, or re-solved single points by hand in new
// directories, checking by hand that the graphs and corridors were byte-identical (09.C5's Why).
#include "internal.hpp"

namespace fusion::run {

std::string step_fingerprint(std::string_view command, const core::Settings& settings,
                             const std::map<std::string, core::Sha256>& inputs) {
    // The command, every result-changing must_match setting by key, and each input's SHA-256 by role;
    // core::fingerprint adds the build id and hashes the canonical form (09, "Saved steps").
    core::Json chosen = core::Json::object();
    for (const core::Setting& setting : settings.all()) {
        if (!setting.spec || !setting.spec->affects_results || setting.spec->on_resume != core::OnResume::must_match)
            continue;
        chosen[core::key(*setting.spec)] = setting.value ? core::to_json(*setting.value) : core::Json(nullptr);
    }
    core::Json by_role = core::Json::object();
    for (const auto& [role, sha] : inputs) by_role[role] = sha.hex();
    core::Json fields = core::Json::object();
    fields["command"] = command;
    fields["inputs"] = std::move(by_role);
    fields["settings"] = std::move(chosen);
    return core::fingerprint(fields);
}

// ---------------------------------------------------------------- Registry (09.C5, later)

struct Registry::Impl {};

Registry::Registry(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Registry::Registry(Registry&&) noexcept = default;
Registry& Registry::operator=(Registry&&) noexcept = default;
Registry::~Registry() = default;

Registry Registry::open(const std::filesystem::path& run_dir) {
    throw core::Failure("run.resume_unavailable",
                        "loading the saved steps of " + core::to_utf8(run_dir) +
                            " (09.C5, decision 0019) comes in a later version of FuSION 2.0");
}

// open() makes no Registry yet, so these are never reached.
RunStatus Registry::status() const { return RunStatus::failed; }
bool Registry::live() const { return false; }
std::vector<Artefact> Registry::find(std::string_view, std::string_view) const { return {}; }
std::vector<Artefact> Registry::match(std::string_view, std::string_view) const { return {}; }
std::filesystem::path Registry::verified(const Artefact& artefact) const {
    throw core::Failure("artefact.changed", "no registry is open for " + artefact.path);
}

// ---------------------------------------------------------------- Store (09.C13, later)

Store::Store(std::filesystem::path root) : root_(std::move(root)) {}
Store::Store(Store&&) noexcept = default;
Store& Store::operator=(Store&&) noexcept = default;
Store::~Store() = default;

std::optional<Store> Store::open(const Options&) {
    return std::nullopt;                         // the store is off: every step computes its own output
}

std::filesystem::path Store::dir(std::string_view kind) {
    throw core::Failure("internal", "the shared store (09.C13) comes in a later version: " + std::string(kind));
}

bool Store::publish(const std::filesystem::path&, const std::filesystem::path& final_path) {
    throw core::Failure("internal", "the shared store (09.C13) comes in a later version: " + core::to_utf8(final_path));
}

}  // namespace fusion::run
