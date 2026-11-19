// Test support for the network module: a context that captures warnings and events, a scratch
// directory, the synthetic fixtures in data/ (made-up ids, no KEGG data, S8.31), and a one-file
// build that drives NetworkBuilder by section 02's rules, so they can be tested without 03's union.
#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "fusion/core/core.hpp"
#include "fusion/network/network.hpp"

namespace network_test {

namespace fs = std::filesystem;
namespace net = fusion::network;
namespace core = fusion::core;

inline fs::path data(const std::string& name) { return fs::path(FUSION_TEST_SOURCE_DIR) / "unit" / "network" / "data" / name; }
inline fs::path reference(const std::string& rel) { return fs::path(FUSION_TEST_SOURCE_DIR) / "reference" / rel; }

class Scratch {
 public:
  Scratch() {
    static std::atomic<int> counter{0};
    dir_ = fs::temp_directory_path() / ("fusion-network-test-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
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

class Events final : public core::Events {
 public:
  void emit(std::string_view type, core::Json fields) override {
    fields["type"] = std::string(type);
    list.push_back(std::move(fields));
  }
  void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
  const core::Json* find(std::string_view type) const {
    for (const auto& e : list)
      if (e["type"] == type) return &e;
    return nullptr;
  }
  std::vector<core::Json> list;
};

class Lines final : public core::LogSink {
 public:
  void write(core::Level level, std::string_view code, std::string_view text) override {
    if (level == core::Level::warning) warnings.emplace_back(code, text);
  }
  std::size_t count(std::string_view code) const {
    return static_cast<std::size_t>(std::count_if(warnings.begin(), warnings.end(), [&](const auto& w) { return w.first == code; }));
  }
  std::string text(std::string_view code) const {
    for (const auto& w : warnings)
      if (w.first == code) return w.second;
    return {};
  }
  std::vector<std::pair<std::string, std::string>> warnings;
};

class NullJournal final : public core::Journal {
 public:
  void append(const core::Json&) override {}
};

class Files final : public core::Files {
 public:
  explicit Files(fs::path dir) : dir_(std::move(dir)) {}
  fs::path input(const fs::path& as_named) override { return as_named; }
  core::OutputFile create(std::string_view name) override { return core::OutputFile(dir_ / core::path_from_utf8(name)); }
  core::OutputFile create_debug(std::string_view name) override { return create(name); }
  core::Journal& journal(std::string_view) override { return journal_; }
  std::span<const core::Json> resumed(std::string_view) const override { return {}; }
  fs::path step_dir() const override { return dir_; }

 private:
  fs::path dir_;
  NullJournal journal_;
};

// A whole context for one step, with its own scratch directory.
struct Context {
  Scratch scratch;
  core::Log log{false};
  std::shared_ptr<Lines> lines = std::make_shared<Lines>();
  Events events;
  core::Settings settings;
  Files files{scratch.dir()};
  core::StopSource stop;
  core::Context ctx{log, events, stop.token(), settings, files, nullptr, "rgx"};
  Context() { log.add_sink(lines); }
};

// ---- a one-file build by section 02's rules (S2.07 to S2.10), for testing the builder on
// documents. 03's merge also unions boxes that share an id; the fixtures here never need that.
inline std::shared_ptr<const net::Network> build_one(const net::PathwayDoc& doc, const net::BuildSettings& settings,
                                                     const net::IdSet& exceptions, core::Context& ctx,
                                                     std::shared_ptr<const net::PathwayDoc> embed = nullptr) {
  const auto& din = doc.interner();
  core::Interner in;
  net::NetworkInfo info;
  info.reader = doc.source().format == net::Format::edge_table ? "edges" : "kgml";
  info.settings = settings;
  info.sources.push_back(doc.source());
  info.exceptions.assign(exceptions.ids().begin(), exceptions.ids().end());
  info.id_map = doc.id_map();
  net::NetworkBuilder b(info, in);
  std::map<std::vector<std::string>, net::NodeIdx> boxes;     // members -> node
  std::map<std::uint32_t, std::vector<net::NodeIdx>> made;   // entry -> its nodes
  const core::Sym pathway = in.sym(doc.source().pathway);
  std::function<std::vector<net::NodeIdx>(std::uint32_t)> nodes_of = [&](std::uint32_t local) {
    if (auto it = made.find(local); it != made.end()) return it->second;
    const net::Entry& e = *doc.entry(local);
    std::vector<net::NodeIdx> out;
    if (din.str(e.type) == "group") {
      std::vector<std::vector<net::NodeIdx>> combos{{}};
      for (std::uint32_t c : doc.components(e)) {
        std::vector<std::vector<net::NodeIdx>> next;
        for (const auto& combo : combos)
          for (net::NodeIdx n : nodes_of(c)) {
            auto x = combo;
            x.push_back(n);
            next.push_back(std::move(x));
          }
        combos = std::move(next);
      }
      for (const auto& combo : combos) {
        const net::NodeIdx cx = b.add_complex(combo);
        b.add_component_edges(cx, pathway);
        if (std::find(out.begin(), out.end(), cx) == out.end()) out.push_back(cx);
      }
    } else {
      const net::EntrySplit split = net::split_entry(doc, e, exceptions, settings.split_threshold);
      std::vector<std::vector<std::string>> units;
      for (core::GeneId id : split.singles) units.push_back({std::string(din.str(id))});
      if (!split.box.empty()) {
        units.emplace_back();
        for (core::GeneId id : split.box) units.back().emplace_back(din.str(id));
      }
      for (const auto& unit : units) {
        auto [it, fresh] = boxes.try_emplace(unit, 0);
        if (fresh) {
          std::vector<core::GeneId> ids;
          for (const auto& m : unit) ids.push_back(in.id(m));
          it->second = b.add_node(in.sym(din.str(e.type)), ids);
          if (unit.size() == 1 && exceptions.contains(unit[0])) b.mark_exception(it->second);
        }
        out.push_back(it->second);
      }
    }
    made.emplace(local, out);
    return out;
  };
  for (std::uint32_t ri : net::kept_relations(doc, settings.filters)) {
    const net::Relation& r = doc.relations()[ri];
    std::vector<core::Sym> subs, paths;
    bool undirected = false;
    for (core::Sym s : doc.subtypes(r)) {
      const auto t = din.str(s);
      undirected = undirected || t == "association" || t == "dissociation" || t == "mapping";
      subs.push_back(in.sym(t));
    }
    for (core::Sym p : doc.pathways(r)) paths.push_back(in.sym(din.str(p)));
    const std::uint16_t flags = undirected ? net::bit(net::EdgeFlag::undirected) : 0;
    for (net::NodeIdx s : nodes_of(r.entry1))
      for (net::NodeIdx t : nodes_of(r.entry2)) {
        b.add_edge(s, t, in.sym(din.str(r.type)), subs, paths, r.score, flags);
        if (undirected && s != t) b.add_edge(t, s, in.sym(din.str(r.type)), subs, paths, r.score, flags);
      }
  }
  if (embed) {
    net::Provenance p;
    p.docs.push_back(std::move(embed));
    b.attach(std::move(p));
  }
  return std::make_shared<net::Network>(std::move(b).finish(ctx));
}

// A readable dump of a network's model, one line per node and edge.
inline std::vector<std::string> dump(const net::Network& n) {
  std::vector<std::string> out;
  const auto& in = n.interner();
  for (net::NodeIdx i = 0; i < n.nodes().size(); ++i) {
    std::string line = "node " + n.node_key(i).value + " " + std::string(in.str(n.nodes()[i].type)) + " [";
    for (core::GeneId m : n.members(i)) (line += in.str(m)) += ' ';
    line += "] comps [";
    for (net::NodeIdx c : n.components(i)) (line += n.node_key(c).value) += ' ';
    out.push_back(line + "] flags " + std::to_string(n.nodes()[i].flags));
  }
  for (net::EdgeIdx e = 0; e < n.edges().size(); ++e) {
    std::string line = "edge " + n.edge_key(e).text() + " pathways [";
    for (core::Sym p : n.pathways(e)) (line += in.str(p)) += ' ';
    const auto& edge = n.edges()[e];
    out.push_back(line + "] score " + std::to_string(edge.score) + " flags " + std::to_string(edge.flags));
  }
  return out;
}

inline bool has_flag(const net::Edge& e, net::EdgeFlag f) { return (e.flags & net::bit(f)) != 0; }
inline bool has_flag(const net::Node& n, net::NodeFlag f) { return (n.flags & net::bit(f)) != 0; }

}  // namespace network_test
