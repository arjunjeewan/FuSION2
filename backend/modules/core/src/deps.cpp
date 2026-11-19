// fusion core: the dependency check before a run (S6.11; section 11, "The dependency check before
// a run"; 11.C20, cross-section items 1.27, 2A.1 and 2B.15).
//
// FuSION 1 checked no helper before a run: a missing gnuplot, pyvis or dot showed up only when its
// step ran, possibly after a solve of hours (spec S6.11; plot_po.sh's needs in S6.22, pex's in
// S6.10). Here every command line declares the tools it needs, and they are probed together, once
// each, before the first step.
#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>

#include "fusion/core/core.hpp"
#include "fusion/core/deps.hpp"
#include "fusion/core/install.hpp"
#include "fusion/core/process.hpp"
#include "tools.hpp"

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace fusion::core {

namespace detail {

Located locate_tool(const char* override_variable, const std::filesystem::path& bundled, std::string_view name) {
    namespace fs = std::filesystem;
    Located found;
    std::error_code ec;
    if (const char* value = std::getenv(override_variable); value != nullptr && *value != '\0') {
        const fs::path p = path_from_utf8(value);
        if (fs::is_regular_file(p, ec)) {
            found.program = p;
            found.origin = override_variable;
        } else {
            found.tried = std::string(override_variable) + "=" + value + " is missing";
        }
        return found;
    }
    std::string tried = std::string(override_variable) + " unset";
    if (!bundled.empty()) {
        if (fs::is_regular_file(bundled, ec)) {
            found.program = bundled;
            found.origin = "bundled";
            return found;
        }
        tried += "; " + to_utf8(bundled) + " missing";
    }
    if (auto on_path = find_on_path(name)) {
        found.program = *on_path;
        found.origin = "PATH";
        return found;
    }
    found.tried = tried + "; not on PATH";
    return found;
}

}  // namespace detail

namespace deps {

namespace {

namespace fs = std::filesystem;

struct ToolFacts {                               // what probing one tool found; cached per process
    bool found = false;
    fs::path path;
    std::string version;
    std::string origin;
    std::vector<std::string> available;          // gnuplot: its terminals
    std::string reason;                          // why it was not found
};

std::vector<std::string> words(std::string_view text) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) ++i;
        const std::size_t start = i;
        while (i < text.size() && !(text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) ++i;
        if (i > start) out.emplace_back(text.substr(start, i - start));
    }
    return out;
}

std::vector<unsigned long> version_parts(std::string_view version) {
    std::vector<unsigned long> parts;
    std::size_t i = 0;
    while (i < version.size()) {
        unsigned long value = 0;
        while (i < version.size() && version[i] >= '0' && version[i] <= '9')
            value = value * 10 + static_cast<unsigned long>(version[i++] - '0');
        parts.push_back(value);
        while (i < version.size() && version[i] != '.') ++i;   // "0-rc1" counts as 0
        if (i < version.size()) ++i;
    }
    return parts;
}

bool at_least(std::string_view have, std::string_view need) {
    const auto a = version_parts(have), b = version_parts(need);
    for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        const unsigned long x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x > y;
    }
    return true;
}

ProcessResult ask(const fs::path& program, std::vector<std::string> args) {
    ProcessSpec spec;
    spec.program = program;
    spec.args = std::move(args);
    spec.own_group = true;
    spec.timeout = std::chrono::seconds(20);
    return run_process(spec);
}

// Section 11: `gnuplot -e "print GPVAL_VERSION, GPVAL_PATCHLEVEL; print GPVAL_TERMINALS"` prints,
// on stderr, "6.0 0" and the terminal names. Trying `set terminal` would prove nothing: an unknown
// terminal is only a warning, with exit status 0.
ToolFacts ask_gnuplot(const detail::Located& located) {
    ToolFacts facts;
    facts.path = located.program;
    facts.origin = located.origin;
    const ProcessResult r = ask(located.program, {"-e", "print GPVAL_VERSION, GPVAL_PATCHLEVEL; print GPVAL_TERMINALS"});
    if (r.exit_code != 0) {
        facts.reason = to_utf8(located.program) + " did not answer (" +
                       (r.error.empty() ? "exit status " + std::to_string(r.exit_code) : r.error) + ")";
        return facts;
    }
    const std::string_view out = r.stderr_text;
    const std::size_t end_of_first = out.find('\n');
    const auto first = words(out.substr(0, end_of_first));
    if (first.empty()) {
        facts.reason = to_utf8(located.program) + " did not report its version";
        return facts;
    }
    facts.version = first[0] + (first.size() > 1 ? "." + first[1] : std::string());
    if (end_of_first != std::string_view::npos) facts.available = words(out.substr(end_of_first + 1));
    facts.found = true;
    return facts;
}

// Graphviz is never bundled (07.C5). `dot -V` prints "dot - graphviz version 14.1.2 (20251207.0)"
// on stderr.
ToolFacts ask_dot(const detail::Located& located) {
    ToolFacts facts;
    facts.path = located.program;
    facts.origin = located.origin;
    const ProcessResult r = ask(located.program, {"-V"});
    const auto said = words(r.stderr_text + " " + r.stdout_text);
    for (std::size_t i = 0; i + 1 < said.size(); ++i)
        if (said[i] == "version") facts.version = said[i + 1];
    facts.found = r.exit_code == 0;
    if (!facts.found) facts.reason = to_utf8(located.program) + " did not answer";
    return facts;
}

// exec lines run in a POSIX shell (01.C10): /bin/sh on Linux and macOS. busybox-w32 on Windows
// comes with the Windows build.
ToolFacts probe_shell() {
    ToolFacts facts;
#if defined(_WIN32)
    facts.reason = "the bundled busybox-w32 shell is not available yet";
#else
    facts.path = "/bin/sh";
    facts.origin = "system";
    facts.found = ::access("/bin/sh", X_OK) == 0;
    if (!facts.found) facts.reason = "/bin/sh is missing";
#endif
    return facts;
}

// 11.C10: the bundled Regular and Bold files in the home's fonts/, else what fontconfig resolves
// for the family, where fc-match exists (macOS ships none).
ToolFacts probe_font(const std::string& family, const fs::path& fonts) {
    ToolFacts facts;
    std::string stem;
    for (const char c : family)
        if (c != ' ') stem.push_back(c);
    std::error_code ec;
    if (fs::is_regular_file(fonts / (stem + "-Regular.ttf"), ec) && fs::is_regular_file(fonts / (stem + "-Bold.ttf"), ec)) {
        facts.found = true;
        facts.path = fonts;
        facts.origin = "bundled";
        return facts;
    }
    if (const auto fc_match = find_on_path("fc-match")) {
        const ProcessResult r = ask(*fc_match, {"-f", "%{family[0]}", family + ":bold"});
        if (r.exit_code == 0 && r.stdout_text == family) {
            facts.found = true;
            facts.origin = "system";
            return facts;
        }
        facts.reason = "fontconfig resolves it to " + (r.stdout_text.empty() ? std::string("nothing") : r.stdout_text);
        return facts;
    }
    facts.reason = "no bundled copy in " + to_utf8(fonts) + ", and no fc-match to ask";
    return facts;
}

// Where a tool is costs a getenv and a stat or two, and is worked out at every check; what running
// it says is asked once per process for each place it is found ("probes each id once"), so an
// override changed in between is probed afresh.
const ToolFacts& facts_for(const std::string& id, Kind kind) {
    detail::Located where;
    fs::path fonts;
    if (kind == Kind::font) {
        fonts = install_dir().home / "fonts";
        where.program = fonts;
    } else if (id == "gnuplot") {
        where = detail::locate_tool("FUSION_GNUPLOT", install_dir().tools_dir / "gnuplot" / "bin" / "gnuplot", "gnuplot");
    } else if (id == "dot") {
        where = detail::locate_tool("FUSION_DOT", {}, "dot");
    } else if (id != "sh") {
        if (const auto found = find_on_path(id)) {
            where.program = *found;
            where.origin = "PATH";
        } else {
            where.tried = "not on PATH";
        }
    }

    static std::mutex mutex;
    static std::map<std::string, ToolFacts> cache;
    std::lock_guard lock(mutex);
    const std::string key = id + '\n' + to_utf8(where.program);
    if (auto it = cache.find(key); it != cache.end()) return it->second;
    ToolFacts facts;
    if (kind == Kind::font) {
        facts = probe_font(id.starts_with("font:") ? id.substr(5) : id, fonts);
    } else if (id == "sh") {
        facts = probe_shell();
    } else if (where.program.empty()) {
        facts.reason = where.tried;
    } else if (id == "gnuplot") {
        facts = ask_gnuplot(where);
    } else if (id == "dot") {
        facts = ask_dot(where);
    } else {
        facts.found = true;
        facts.path = where.program;
        facts.origin = where.origin;
    }
    return cache.emplace(key, std::move(facts)).first->second;
}

std::string joined(const std::vector<std::string>& items, std::string_view separator) {
    std::string out;
    for (const std::string& item : items) {
        if (!out.empty()) out.append(separator);
        out.append(item);
    }
    return out;
}

struct Merged {
    Kind kind = Kind::program;
    std::string min_version;
    std::vector<std::string> features;
    std::vector<std::string> needed_by;
};

}  // namespace

void Registry::require(Requirement r) { requirements_.push_back(std::move(r)); }

std::vector<Probe> Registry::check() const {
    std::map<std::string, Merged> by_id;         // sorted by id, in byte order
    for (const Requirement& r : requirements_) {
        Merged& m = by_id[r.id];
        m.kind = r.kind;
        if (!r.min_version.empty() && (m.min_version.empty() || !at_least(m.min_version, r.min_version)))
            m.min_version = r.min_version;       // the highest minimum wins
        for (const std::string& f : r.features)
            if (std::find(m.features.begin(), m.features.end(), f) == m.features.end()) m.features.push_back(f);
        if (!r.requested_by.empty()) m.needed_by.push_back(r.requested_by);
    }

    std::vector<Probe> probes;
    for (const auto& [id, m] : by_id) {
        const ToolFacts& facts = facts_for(id, m.kind);
        Probe p;
        p.id = id;
        p.path = facts.path;
        p.version = facts.version;
        p.origin = facts.origin;
        p.needed_by = m.needed_by;
        for (const std::string& f : m.features) {
            if (std::find(facts.available.begin(), facts.available.end(), f) != facts.available.end())
                p.features.push_back(f);
            else
                p.missing.push_back(f);
        }
        const std::string who = m.needed_by.empty() ? std::string("this batch") : joined(m.needed_by, ", ");
        std::string what = m.kind == Kind::font ? id.substr(id.starts_with("font:") ? 5 : 0) : id;
        if (!m.min_version.empty()) what += " " + m.min_version + " or later";
        if (!m.features.empty()) what += " with " + joined(m.features, ", ");
        if (!facts.found) {
            p.missing.insert(p.missing.begin(), id);
            p.message = m.kind == Kind::font
                            ? who + " draws with " + what + ", which is not available (" + facts.reason +
                                  "); gnuplot will substitute another font"
                            : who + " needs " + what + "; none found (" + facts.reason + ")";
        } else if (!m.min_version.empty() && !at_least(facts.version, m.min_version)) {
            p.message = who + " needs " + what + "; " + to_utf8(facts.path) + " is version " + facts.version;
        } else if (!p.missing.empty()) {
            p.message = who + " needs " + what + "; " + to_utf8(facts.path) + " lacks " + joined(p.missing, ", ");
        } else {
            p.ok = true;
        }
        probes.push_back(std::move(p));
    }
    return probes;
}

}  // namespace deps

}  // namespace fusion::core
