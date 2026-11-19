// The base merge, the split and reuse (S2.22; section 03, "Merge once, split afterwards", 03.C3).
// Each pathway list is merged once without the run's exceptions, the exception ids are split out
// afterwards, and runs on the same pathways share one base merge, held in memory by the interpreter's
// MergeStore (D1). FuSION 1 re-merged every list for every run, 3 s for 42 pathways and 122 to 126 s for
// 163 (Linux/src/GraphManagerNew.cc:10572-10621; S2.22).
#include <utility>

#include "engine.hpp"

namespace fusion::merge {

// ---------------------------------------------------------------- BaseMerge

BaseMerge::BaseMerge(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
BaseMerge::BaseMerge(BaseMerge&&) noexcept = default;
BaseMerge& BaseMerge::operator=(BaseMerge&&) noexcept = default;
BaseMerge::~BaseMerge() = default;

const MergeKey& BaseMerge::key() const noexcept { return impl_->key; }
const network::BuildSettings& BaseMerge::settings() const noexcept { return impl_->corpus->settings; }
std::span<const DocPtr> BaseMerge::docs() const noexcept { return impl_->docs; }
std::shared_ptr<const network::Network> BaseMerge::network() const noexcept { return impl_->network; }
std::uint32_t BaseMerge::box_count() const noexcept { return impl_->corpus->boxes; }
std::uint32_t BaseMerge::relation_count() const noexcept { return impl_->corpus->relations; }
const BaseMerge::Impl& BaseMerge::impl() const noexcept { return *impl_; }

std::span<const EntryRef> BaseMerge::boxes_of(network::NodeIdx n) const noexcept {
    const auto& t = impl_->tables;
    if (static_cast<std::size_t>(n) + 1 >= t.boxes_first.size()) return {};
    return std::span<const EntryRef>(t.boxes).subspan(t.boxes_first[n], t.boxes_first[n + 1] - t.boxes_first[n]);
}

std::shared_ptr<const BaseMerge> build_base_merge(std::vector<DocPtr> docs, const network::BuildSettings& s,
                                                  MergeKey key, core::Context& ctx) {
    auto corpus = detail::prepare(std::move(docs), s, &ctx.stop);
    auto impl = std::make_unique<BaseMerge::Impl>();
    impl->key = key;
    // The base merge has E = {}, base="true" and the rules version in its header (03.C4).
    impl->network = detail::build_direct(
        *corpus, ExceptionSet(), network::MergeHeader{key.hex(), true, static_cast<int>(rules_version)}, ctx,
        &impl->tables);
    for (const auto& d : corpus->docs) impl->docs.push_back(d.doc);
    impl->corpus = std::move(corpus);
    return std::make_shared<const BaseMerge>(std::move(impl));
}

// ---------------------------------------------------------------- the split and the direct merge

MergedNetwork split(const BaseMerge& base, const ExceptionSet& exceptions) {
    const BaseMerge::Impl& impl = base.impl();
    detail::QuietContext quiet;
    detail::SplitResult r =
        detail::build_split(*impl.corpus, *impl.network, impl.tables, exceptions,
                            network::MergeHeader{impl.key.hex(), false, static_cast<int>(rules_version)}, quiet.get());
    return MergedNetwork{std::move(r.network), exceptions, impl.key, r.stats, BaseSource::built};
}

std::shared_ptr<const network::Network> merge_direct(std::span<const DocPtr> docs, const network::BuildSettings& s,
                                                     const ExceptionSet& exceptions) {
    auto corpus = detail::prepare(std::vector<DocPtr>(docs.begin(), docs.end()), s, nullptr);
    detail::QuietContext quiet;
    return detail::build_direct(*corpus, exceptions, std::nullopt, quiet.get(), nullptr);
}

std::string_view source_name(BaseSource s) noexcept {
    switch (s) {
        case BaseSource::memory: return "memory";
        case BaseSource::store: return "store";
        case BaseSource::embedded: return "embedded";
        case BaseSource::built: return "built";
    }
    return "built";
}

core::Json step_details(const MergedNetwork& m) {
    core::Json d = core::Json::object();
    const bool legacy = m.network != nullptr && m.network->info().legacy;
    if (!legacy) {
        d["merge_key"] = m.base.hex();
        d["base_merge"] = std::string(source_name(m.source));
    }
    d["exceptions"] = m.exceptions.size();
    d["exceptions_sha256"] = m.exceptions.digest().hex();
    return d;
}

// ---------------------------------------------------------------- MergeStore (03.C3)

MergeStore::Hit MergeStore::get_or_build(std::span<const network::SourceRef> inputs, const network::BuildSettings& s,
                                         const network::IdMap& map, network::StepDocs& step_docs,
                                         core::Context& ctx) {
    const MergeKey key = merge_key(inputs, s, map);
    if (const auto it = memory_.find(key); it != memory_.end()) return Hit{it->second, Source::memory};
    // The shared store (03.C3, 03.C4) is a later iteration: ctx.cache is not read yet, and every miss
    // builds from the parse of the step's own inputs (02.C15, 09.C3; S7.19).
    std::vector<DocPtr> docs;
    docs.reserve(inputs.size());
    for (const network::SourceRef& in : inputs) {
        ctx.stop.throw_if_stopped();
        if (!in.path) throw core::Failure("input.missing", "no path for the input of pathway " + in.pathway);
        docs.push_back(step_docs.get(ctx.files.input(*in.path), map, ctx));
    }
    auto merge = build_base_merge(std::move(docs), s, key, ctx);
    memory_.emplace(key, merge);
    return Hit{std::move(merge), Source::built};
}

MergeStore::Hit MergeStore::get_or_build(std::vector<DocPtr> docs, const network::BuildSettings& s,
                                         core::Context& ctx) {
    const MergeKey key = detail::key_of_docs(docs, s);
    if (const auto it = memory_.find(key); it != memory_.end()) return Hit{it->second, Source::memory};
    auto merge = build_base_merge(std::move(docs), s, key, ctx);
    memory_.emplace(key, merge);
    return Hit{std::move(merge), Source::built};
}

MergeStore::Hit MergeStore::from_embedded(const network::Network& saved, const network::BuildSettings& s,
                                          core::Context& ctx) {
    const network::Provenance* p = saved.provenance();
    if (p == nullptr || p->docs.empty())
        throw core::Failure("merge.no_sources",
                            "the saved network embeds no sources, so it cannot be split exactly (03.C7)");
    Hit hit = get_or_build(std::vector<DocPtr>(p->docs.begin(), p->docs.end()), s, ctx);
    if (hit.source == Source::built) hit.source = Source::embedded;
    return hit;
}

std::size_t MergeStore::size() const noexcept { return memory_.size(); }

}  // namespace fusion::merge
