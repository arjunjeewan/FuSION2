// The merge-exception set (S1.17, S2.13, S8.22; section 03, "The exception set", 03.C1, 03.C2).
//
// FuSION 1 read the set from start's exception file, first token per line
// (Linux/src/GraphManagerNew.cc:8072-8110), which the batch built itself with
// `exec cat $UP_REG_FILE $DOWN_REG_FILE $SRC_FILE $TGT_FILE $HSA_IDS_NOT_TO_BE_MERGED_FILE` (S1.17). 2.0
// assembles it in memory from the run's inputs, and adds the FSC (S2.13) and the confirmed lists (03.C2).
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "fusion/merge/merge.hpp"

namespace fusion::merge {

namespace {

constexpr std::array<std::pair<Reason, std::string_view>, 9> reason_table{{
    {Reason::up, "up"},
    {Reason::down, "down"},
    {Reason::confirmed_up, "confirmed_up"},
    {Reason::confirmed_down, "confirmed_down"},
    {Reason::source, "source"},
    {Reason::target, "target"},
    {Reason::fsc, "fsc"},
    {Reason::not_merged, "not_merged"},
    {Reason::start_file, "start_file"},
}};

bool space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f'; }

// The first whitespace-separated token of each line; blank lines are skipped, LF and CRLF both end a
// line, and a UTF-8 byte-order mark is skipped (S1.17; section 03, "Portability").
std::vector<std::string> first_tokens(const std::filesystem::path& file) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        throw core::Failure("input.missing", "no such file: " + core::to_utf8(file), core::Where{core::to_utf8(file), 0, {}});
    std::ifstream in(file, std::ios::binary);
    if (!in) throw core::Failure("input.unreadable", "cannot read " + core::to_utf8(file), core::Where{core::to_utf8(file), 0, {}});
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) throw core::Failure("input.unreadable", "cannot read " + core::to_utf8(file), core::Where{core::to_utf8(file), 0, {}});
    std::string_view rest(text);
    if (rest.starts_with("\xEF\xBB\xBF")) rest.remove_prefix(3);
    std::vector<std::string> tokens;
    while (!rest.empty()) {
        const std::size_t end = rest.find('\n');
        std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
        std::size_t b = 0;
        while (b < line.size() && space(line[b])) ++b;
        std::size_t e = b;
        while (e < line.size() && !space(line[e])) ++e;
        if (e > b) tokens.emplace_back(line.substr(b, e - b));
    }
    return tokens;
}

}  // namespace

std::string_view reason_name(Reason r) noexcept {
    for (const auto& [reason, name] : reason_table)
        if (reason == r) return name;
    return {};
}

std::string reason_names(std::uint16_t reasons) {
    std::string out;
    for (const auto& [reason, name] : reason_table) {
        if ((reasons & static_cast<std::uint16_t>(reason)) == 0) continue;
        if (!out.empty()) out += ',';
        out += name;
    }
    return out;
}

// ---------------------------------------------------------------- ExceptionSet

ExceptionSet::ExceptionSet(const core::Interner& ids) : interner_(&ids) {}

bool ExceptionSet::insert(std::string_view id, Reason r) {
    auto it = reasons_.find(id);
    const bool fresh = it == reasons_.end();
    if (fresh) it = reasons_.emplace(std::string(id), std::uint16_t{0}).first;
    it->second = static_cast<std::uint16_t>(it->second | static_cast<std::uint16_t>(r));
    return fresh;
}

void ExceptionSet::rebuild() {
    std::vector<std::string> ids;
    ids.reserve(reasons_.size());
    for (const auto& entry : reasons_) ids.push_back(entry.first);
    ids_ = network::IdSet(std::move(ids));
}

void ExceptionSet::add(core::GeneId id, Reason r) {
    if (interner_ == nullptr)
        throw core::Failure("internal", "an exception id was given as a handle to a set with no interner");
    add(interner_->str(id), r);
}

void ExceptionSet::add(std::string_view id, Reason r) {
    if (insert(id, r)) rebuild();
}

bool ExceptionSet::contains(core::GeneId id) const noexcept {
    return interner_ != nullptr && contains(interner_->str(id));
}

bool ExceptionSet::contains(std::string_view id) const noexcept { return reasons_.find(id) != reasons_.end(); }

std::uint16_t ExceptionSet::reasons(core::GeneId id) const noexcept {
    return interner_ != nullptr ? reasons(interner_->str(id)) : std::uint16_t{0};
}

std::uint16_t ExceptionSet::reasons(std::string_view id) const noexcept {
    const auto it = reasons_.find(id);
    return it == reasons_.end() ? std::uint16_t{0} : it->second;
}

const network::IdSet& ExceptionSet::ids() const noexcept { return ids_; }

// SHA-256 of the ids in byte order, each ended by LF, for run.json (section 03, "Settings").
core::Sha256 ExceptionSet::digest() const {
    core::Sha256Builder b;
    for (const auto& entry : reasons_) {
        b.update(std::string_view(entry.first));
        b.update(std::string_view("\n"));
    }
    return b.finish();
}

std::size_t ExceptionSet::size() const noexcept { return reasons_.size(); }
bool ExceptionSet::empty() const noexcept { return reasons_.empty(); }

// ---------------------------------------------------------------- assembly (S8.22, 03.C1, 03.C2)

ExceptionSet assemble_exceptions(const ExceptionInputs& in, const network::IdMap& map, core::Context& ctx) {
    const bool handles = (in.observations != nullptr && !in.observations->empty()) || !in.sources.empty() ||
                         !in.targets.empty() || !in.fscs.empty();
    if (handles && in.interner == nullptr)
        throw core::Failure("internal", "the exception inputs name ids by handle but give no interner");
    core::Interner local;
    core::Interner& ids = in.interner != nullptr ? *in.interner : local;
    ExceptionSet set = in.interner != nullptr ? ExceptionSet(*in.interner) : ExceptionSet();

    const auto add_all = [&](std::span<const core::GeneId> list, Reason r) {
        for (const core::GeneId g : list) set.insert(ids.str(g), r);
    };
    // The up and down genes: the latest cudf's observations, after Not DE (04.C4, S8.15). FuSION 1 added
    // them through `exec cat` of up_reg and down_reg (S1.17).
    if (in.observations != nullptr) {
        add_all(in.observations->up(), Reason::up);
        add_all(in.observations->down(), Reason::down);
    }
    // The corridor's endpoints and the FSC (S2.13: the FSC joins by itself; 06.C10: one or more ids).
    add_all(in.sources, Reason::source);
    add_all(in.targets, Reason::target);
    add_all(in.fscs, Reason::fsc);
    // The files: first token per line, each id through the one id rule (02.C3), from the run's checked
    // copy (09.C3).
    const auto add_file = [&](const std::filesystem::path& named, Reason r) {
        for (const std::string& token : first_tokens(ctx.files.input(named))) {
            const core::GeneId g = map.canonical(token, ids);
            set.insert(ids.str(g), r);
        }
    };
    for (const auto& f : in.confirmed_up) add_file(f, Reason::confirmed_up);       // 03.C2
    for (const auto& f : in.confirmed_down) add_file(f, Reason::confirmed_down);
    if (in.not_merged) add_file(*in.not_merged, Reason::not_merged);               // hsa_ids_not_to_be_merged.txt
    if (in.start_file) add_file(*in.start_file, Reason::start_file);               // start's argument 2 (S1.15, 2B.4)
    set.rebuild();
    return set;
}

}  // namespace fusion::merge
