// Test support for the merge module: a context whose files land in a scratch directory, a writer of
// synthetic KGML-format pathways with made-up ids (no KEGG data, S8.31), and a readable dump of a
// network's model fields for comparisons.
#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "fusion/core/core.hpp"
#include "fusion/merge/merge.hpp"
#include "fusion/network/network.hpp"
#include "fusion/testkit/generate.hpp"

namespace merge_test {

namespace fs = std::filesystem;
using fusion::merge::DocPtr;

// A fresh directory under the system's temporary folder, removed afterwards.
class Scratch {
public:
    Scratch() {
        static std::atomic<int> counter{0};
        dir_ = fs::temp_directory_path() /
               ("fusion-merge-test-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    const fs::path& dir() const noexcept { return dir_; }
    fs::path write(const std::string& name, const std::string& text) const {
        const fs::path p = dir_ / name;
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << text;
        return p;
    }
private:
    fs::path dir_;
};

inline std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

class Events final : public fusion::core::Events {
public:
    void emit(std::string_view type, fusion::core::Json fields) override {
        fields["type"] = std::string(type);
        list.push_back(std::move(fields));
    }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
    const fusion::core::Json* find(std::string_view type) const {
        for (const auto& e : list)
            if (e["type"] == type) return &e;
        return nullptr;
    }
    std::vector<fusion::core::Json> list;
};

class Lines final : public fusion::core::LogSink {
public:
    void write(fusion::core::Level, std::string_view code, std::string_view text) override {
        lines.push_back(code.empty() ? std::string(text) : std::string(code) + ": " + std::string(text));
    }
    std::vector<std::string> lines;
};

class NullJournal final : public fusion::core::Journal {
public:
    void append(const fusion::core::Json&) override {}
};

// The step's files: outputs in <scratch>/step, inputs read where they are named (no copies here).
class Files final : public fusion::core::Files {
public:
    explicit Files(fs::path step_dir) : dir_(std::move(step_dir)) { fs::create_directories(dir_); }
    fs::path input(const fs::path& as_named) override { return as_named; }
    fusion::core::OutputFile create(std::string_view name) override {
        const fs::path p = dir_ / fusion::core::path_from_utf8(name);
        fs::create_directories(p.parent_path());
        return fusion::core::OutputFile(p, [this](const fs::path& f, const fusion::core::Sha256&) { committed.push_back(f); });
    }
    fusion::core::OutputFile create_debug(std::string_view name) override { return create(name); }
    fusion::core::Journal& journal(std::string_view) override { return journal_; }
    std::span<const fusion::core::Json> resumed(std::string_view) const override { return {}; }
    fs::path step_dir() const override { return dir_; }
    std::vector<fs::path> committed;
private:
    fs::path dir_;
    NullJournal journal_;
};

// A whole context for one step.
struct Context {
    Scratch scratch;
    fusion::core::Log log{false};
    std::shared_ptr<Lines> console = std::make_shared<Lines>();
    Events events;
    fusion::core::Settings settings;
    Files files{scratch.dir() / "mff"};
    fusion::core::StopSource stop;
    fusion::core::Context ctx{log, events, stop.token(), settings, files, nullptr, "mff"};
    Context() { log.add_sink(console); }
};

// A synthetic KGML-format pathway (KGML's elements and attributes; made-up ids).
class Kgml {
public:
    explicit Kgml(std::string name, std::string title = {}) : name_(std::move(name)), title_(std::move(title)) {}
    Kgml& entry(int id, const std::string& ids, const std::string& type = "gene") {
        entries_.push_back("  <entry id=\"" + std::to_string(id) + "\" name=\"" + ids + "\" type=\"" + type + "\">\n"
                           "    <graphics name=\"x\"/>\n  </entry>\n");
        return *this;
    }
    Kgml& group(int id, const std::vector<int>& comps) {
        std::string e = "  <entry id=\"" + std::to_string(id) + "\" name=\"undefined\" type=\"group\">\n";
        for (int c : comps) e += "    <component id=\"" + std::to_string(c) + "\"/>\n";
        entries_.push_back(e + "  </entry>\n");
        return *this;
    }
    Kgml& rel(int e1, int e2, const std::vector<std::string>& subtypes, const std::string& type = "PPrel",
              const std::optional<std::string>& score = std::nullopt) {
        std::string r = "  <relation entry1=\"" + std::to_string(e1) + "\" entry2=\"" + std::to_string(e2) +
                        "\" type=\"" + type + "\"" + (score ? " experiment_score=\"" + *score + "\"" : std::string()) + ">\n";
        for (const auto& st : subtypes) r += "    <subtype name=\"" + st + "\" value=\"--&gt;\"/>\n";
        relations_.push_back(r + "  </relation>\n");
        return *this;
    }
    // The same pathway with its entries and relations in another order (entry ids stay).
    template <class Rng>
    Kgml shuffled(Rng& rng) const {
        Kgml k = *this;
        rng.shuffle(k.entries_.begin(), k.entries_.end());
        rng.shuffle(k.relations_.begin(), k.relations_.end());
        return k;
    }
    const std::string& name() const noexcept { return name_; }
    std::string text() const {
        std::string out = "<?xml version=\"1.0\"?>\n<pathway name=\"" + name_ + "\" org=\"hsa\" number=\"00000\" title=\"" +
                          title_ + "\">\n";
        for (const auto& e : entries_) out += e;
        for (const auto& r : relations_) out += r;
        return out + "</pathway>\n";
    }
private:
    std::string name_, title_;
    std::vector<std::string> entries_, relations_;
};

// Writes a pathway as <file> (default: its id after the colon, .xml) and reads it with section 02's reader.
inline DocPtr read(Context& c, const Kgml& k, std::string file = {}) {
    if (file.empty()) file = k.name().substr(k.name().find(':') + 1) + ".xml";
    const fs::path p = c.scratch.write("in/" + file, k.text());
    return fusion::network::read_kgml(p, fusion::network::IdMap::none(), c.ctx);
}

// The model fields of a network, one line each, in canonical order: what split and merge_direct must
// agree on (12.C14), readable when a test fails.
inline std::string dump(const fusion::network::Network& n) {
    using namespace fusion::network;
    const auto& in = n.interner();
    std::ostringstream o;
    for (NodeIdx i = 0; i < n.nodes().size(); ++i) {
        const Node& node = n.nodes()[i];
        o << "N " << n.node_key(i).value << " " << in.str(node.type) << " [";
        for (const auto m : n.members(i)) o << " " << in.str(m);
        o << " ] {";
        for (const auto c : n.components(i)) o << " " << n.node_key(c).value;
        o << " } f" << node.flags << "\n";
    }
    for (EdgeIdx e = 0; e < n.edges().size(); ++e) {
        const Edge& edge = n.edges()[e];
        o << "E " << n.node_key(edge.src).value << " -> " << n.node_key(edge.tgt).value << " " << in.str(edge.type)
          << " (";
        for (const auto s : n.subtypes(e)) o << " " << in.str(s);
        o << " ) <";
        for (const auto p : n.pathways(e)) o << " " << in.str(p);
        o << " > s" << edge.score << " f" << edge.flags << "\n";
    }
    return o.str();
}

// Each node's members, as text: "hsa1 hsa2" per node, sorted.
inline std::vector<std::string> member_sets(const fusion::network::Network& n) {
    std::vector<std::string> out;
    for (fusion::network::NodeIdx i = 0; i < n.nodes().size(); ++i) {
        std::string s;
        for (const auto m : n.members(i)) s += (s.empty() ? "" : " ") + std::string(n.interner().str(m));
        out.push_back(s);
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline std::optional<fusion::network::NodeIdx> node(const fusion::network::Network& n, const std::string& key) {
    return n.find_node(fusion::network::NodeKey{key});
}

inline const fusion::network::Edge* edge(const fusion::network::Network& n, const std::string& src,
                                         const std::string& tgt, const std::string& type,
                                         std::vector<std::string> subtypes) {
    const auto e = n.find_edge(fusion::network::EdgeKey{{src}, {tgt}, type, std::move(subtypes)});
    return e ? &n.edges()[*e] : nullptr;
}

inline std::vector<std::string> pathways(const fusion::network::Network& n, const fusion::network::Edge& e) {
    const auto idx = static_cast<fusion::network::EdgeIdx>(&e - n.edges().data());
    std::vector<std::string> out;
    for (const auto p : n.pathways(idx)) out.emplace_back(n.interner().str(p));
    return out;
}

inline bool has(std::uint16_t flags, fusion::network::EdgeFlag f) { return (flags & fusion::network::bit(f)) != 0; }
inline bool has(std::uint16_t flags, fusion::network::NodeFlag f) { return (flags & fusion::network::bit(f)) != 0; }

// ---- generated sets (the test kit's pathways(), 12.C9) ----

// Reads KGML texts as files <folder>/<n>.xml with section 02's reader, in the order given.
inline std::vector<DocPtr> read_texts(Context& c, const std::vector<std::string>& texts, const std::string& folder) {
    std::vector<DocPtr> docs;
    for (std::size_t k = 0; k < texts.size(); ++k) {
        const fs::path p = c.scratch.write("in/" + folder + "/" + std::to_string(k) + ".xml", texts[k]);
        docs.push_back(fusion::network::read_kgml(p, fusion::network::IdMap::none(), c.ctx));
    }
    return docs;
}

inline std::vector<std::string> texts_of(const std::vector<fusion::testkit::SyntheticPathway>& set) {
    std::vector<std::string> out;
    for (const auto& p : set) out.push_back(p.kgml);
    return out;
}

// The canonical ids a generated set names ("t:5" is "t5", S2.06), sorted.
inline std::vector<std::string> ids_of(const std::vector<std::string>& texts) {
    std::set<std::string> ids;
    for (const std::string& t : texts) {
        std::size_t at = 0;
        while ((at = t.find("<entry ", at)) != std::string::npos) {
            const std::size_t name = t.find(" name=\"", at) + 7;
            const std::string list = t.substr(name, t.find('"', name) - name);
            std::istringstream in(list);
            for (std::string id; in >> id;)
                if (id != "undefined") {
                    id.erase(std::remove(id.begin(), id.end(), ':'), id.end());
                    ids.insert(id);
                }
            at = name;
        }
    }
    return {ids.begin(), ids.end()};
}

// The same pathway with its entries and relations in another order; ids and entry ids stay (S2.17).
inline std::string shuffled_kgml(const std::string& text, fusion::core::Rng& rng) {
    std::vector<std::string> entries, relations;
    const std::size_t first = std::min(text.find("  <entry "), text.find("  <relation "));
    const std::size_t end = text.rfind("</pathway>");
    std::size_t at = first;
    while (at < end) {
        const bool entry = text.compare(at, 9, "  <entry ") == 0;
        const std::string close = entry ? "  </entry>\n" : "  </relation>\n";
        const std::size_t stop = text.find(close, at) + close.size();
        (entry ? entries : relations).push_back(text.substr(at, stop - at));
        at = stop;
    }
    rng.shuffle(entries.begin(), entries.end());
    rng.shuffle(relations.begin(), relations.end());
    std::string out = text.substr(0, first);
    for (const auto& e : entries) out += e;
    for (const auto& r : relations) out += r;
    return out + text.substr(end);
}

inline fusion::merge::ExceptionSet exceptions(std::initializer_list<const char*> ids,
                                              fusion::merge::Reason r = fusion::merge::Reason::not_merged) {
    fusion::merge::ExceptionSet e;
    for (const char* id : ids) e.add(std::string_view(id), r);
    return e;
}

}  // namespace merge_test
