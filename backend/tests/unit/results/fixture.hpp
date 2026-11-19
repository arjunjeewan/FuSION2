// Test kit for the results module: a scripted catalogue, builders for 06's witnesses and answers, and
// a step context that writes into a temporary directory. Synthetic inputs only (S8.31).
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/results/analyse.hpp"
#include "fusion/results/model.hpp"

namespace fx {

using namespace fusion;
using results::Observed;

inline network::EdgeKey ek(const std::string& src, const std::string& tgt, std::vector<std::string> subtypes,
                           const std::string& type = "PPrel") {
  return network::EdgeKey{network::NodeKey{src}, network::NodeKey{tgt}, type, std::move(subtypes)};
}

inline solve::Point pt(int n, int e) {
  solve::Point p;
  p.n = n;
  p.e = e;
  return p;
}

// A catalogue scripted node by node and edge by edge.
class Catalogue final : public results::Catalogue {
 public:
  results::NodeRecord& node(const std::string& key, const std::string& label, Observed observed = Observed::none,
                            std::optional<double> lfc = std::nullopt) {
    results::NodeRecord r;
    r.key.value = key;
    r.label = label;
    r.observed = observed;
    r.members.push_back({key, label, lfc, observed});
    return nodes_[key] = std::move(r);
  }
  results::EdgeRecord& edge(const network::EdgeKey& key, std::vector<std::string> pathways = {}) {
    results::EdgeRecord r;
    r.key = key;
    r.type = key.type;
    r.subtypes = key.subtypes;
    r.effect = network::effect_of(key.subtypes);
    r.pathways = std::move(pathways);
    return edges_[key.text()] = std::move(r);
  }
  void gene(const std::string& gene, const std::string& node) { genes_[gene] = network::NodeKey{node}; }
  void pathway(const std::string& id, const std::string& name) { pathways_[id] = name; }

  std::optional<results::NodeRecord> node(const network::NodeKey& key) const override {
    const auto it = nodes_.find(key.value);
    if (it == nodes_.end()) return std::nullopt;
    return it->second;
  }
  std::optional<results::EdgeRecord> edge(const network::EdgeKey& key) const override {
    const auto it = edges_.find(key.text());
    if (it == edges_.end()) return std::nullopt;
    return it->second;
  }
  std::optional<network::NodeKey> node_of(std::string_view gene) const override {
    const auto it = genes_.find(std::string(gene));
    if (it == genes_.end()) return std::nullopt;
    return it->second;
  }
  std::string pathway_name(std::string_view id) const override {
    const auto it = pathways_.find(std::string(id));
    return it == pathways_.end() ? std::string(id) : it->second;
  }

 private:
  std::map<std::string, results::NodeRecord> nodes_;
  std::map<std::string, results::EdgeRecord> edges_;
  std::map<std::string, network::NodeKey> genes_;
  std::map<std::string, std::string> pathways_;
};

// 06's witness, with its counts from its own relaxed lists.
struct W {
  std::vector<std::string> active, inactive;
  std::vector<network::EdgeKey> selected;
  std::vector<std::pair<std::string, bool>> relaxed_nodes;          // key, present
  std::vector<std::pair<network::EdgeKey, bool>> relaxed_edges;     // key, selected
  solve::Witness make() const {
    solve::Witness w;
    for (const auto& k : active) w.active.push_back(network::NodeKey{k});
    for (const auto& k : inactive) w.inactive.push_back(network::NodeKey{k});
    w.selected = selected;
    for (const auto& [k, present] : relaxed_nodes) w.relaxed_nodes.emplace_back(network::NodeKey{k}, present);
    w.relaxed_edges = relaxed_edges;
    w.counts = pt(static_cast<int>(relaxed_nodes.size()), static_cast<int>(relaxed_edges.size()));
    return w;
  }
};

inline solve::Answer answer(solve::Point at, solve::Phase phase, solve::Outcome outcome,
                            std::optional<std::uint32_t> witness = std::nullopt,
                            solve::Source source = solve::Source::z3, double cpu = 0.5, std::string reason = {}) {
  solve::Answer a;
  a.point = at;
  a.phase = phase;
  a.source = source;
  a.outcome = outcome;
  a.cpu_s = cpu;
  a.wall_s = cpu;
  a.reason = std::move(reason);
  a.witness = witness;
  return a;
}

// A case with one frontier point per witness, each certified unless listed.
inline solve::CaseResult frontier_case(solve::Case kind, std::vector<W> witnesses, std::vector<std::size_t> uncertified = {},
                                       solve::CaseStatus status = solve::CaseStatus::complete) {
  solve::CaseResult c{};
  c.kase = kind;
  c.status = status;
  for (std::size_t i = 0; i < witnesses.size(); ++i) {
    solve::Witness w = witnesses[i].make();
    solve::FrontierPoint fp;
    fp.point = w.counts;
    fp.certified = std::find(uncertified.begin(), uncertified.end(), i) == uncertified.end();
    fp.witness = static_cast<std::uint32_t>(i);
    c.log.push_back(answer(w.counts, solve::Phase::diagonal, solve::Outcome::sat, static_cast<std::uint32_t>(i)));
    c.witnesses.push_back(std::move(w));
    c.frontier.points.push_back(fp);
  }
  return c;
}

inline std::string read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

inline std::filesystem::path source_dir() { return std::filesystem::path(FUSION_TEST_SOURCE_DIR); }

// A step context writing into a fresh temporary directory, removed afterwards.
class Step {
 public:
  explicit Step(std::string step = "pathz3", std::vector<core::Setting> settings = {})
      : name_(std::move(step)), settings_(std::move(settings)), log_(false), sink_(std::make_shared<Sink>()) {
    static std::atomic<int> counter{0};
    root_ = std::filesystem::temp_directory_path() /
            ("fusion-results-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
             std::to_string(counter++));
    std::filesystem::create_directories(root_ / name_);
    files_.dir = root_ / name_;
    log_.add_sink(sink_);
  }
  ~Step() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  Step(const Step&) = delete;
  Step& operator=(const Step&) = delete;

  core::Context context() { return core::Context{log_, events_, core::StopToken{}, settings_, files_, nullptr, name_}; }
  const std::filesystem::path& root() const { return root_; }
  std::filesystem::path dir() const { return root_ / name_; }
  const std::vector<std::string>& lines() const { return sink_->lines; }
  const std::vector<std::pair<std::string, core::Json>>& events() const { return events_.emitted; }
  const std::vector<std::filesystem::path>& committed() const { return files_.committed; }
  std::string file(const std::string& relative) const { return read(dir() / relative); }

 private:
  struct Sink final : core::LogSink {
    std::vector<std::string> lines;
    void write(core::Level, std::string_view, std::string_view text) override { lines.emplace_back(text); }
  };
  struct Events final : core::Events {
    std::vector<std::pair<std::string, core::Json>> emitted;
    void emit(std::string_view type, core::Json fields) override { emitted.emplace_back(std::string(type), std::move(fields)); }
    void progress(std::uint64_t, std::uint64_t, std::string_view) override {}
  };
  struct Files final : core::Files {
    std::filesystem::path dir;
    std::vector<std::filesystem::path> committed;
    std::filesystem::path input(const std::filesystem::path& p) override { return p; }
    core::OutputFile create(std::string_view name) override {
      return core::OutputFile(dir / std::filesystem::path(std::string(name)),
                              [this](const std::filesystem::path& p, const core::Sha256&) { committed.push_back(p); });
    }
    core::OutputFile create_debug(std::string_view name) override { return create(name); }
    core::Journal& journal(std::string_view) override { throw core::Failure("internal", "no journal in tests"); }
    std::span<const core::Json> resumed(std::string_view) const override { return {}; }
    std::filesystem::path step_dir() const override { return dir; }
  };

  std::string name_;
  core::Settings settings_;
  core::Log log_;
  std::shared_ptr<Sink> sink_;
  Events events_;
  Files files_;
  std::filesystem::path root_;
};

}  // namespace fx
