// W7 of Phase 3 (docs/phase3/plan.md; section 12, "Merge then split"): FuSION 1's own merge of S2.22's
// bounded sets, with three exception sets each, checks both of 2.0's merge oracles "wherever no fix applies".
//
// The sets are bounded_sets.hpp's 50, the 32 listed shapes and 18 generated ones (seed 2022), less those
// that keep indirect relations or exclude ECrel ones and hold some: FuSION 1 always discards the first and
// keeps the second (S1.29, S2.11, S2.31). A set with such a setting and none of those relations is run, with
// a note. Each set run has a folder backend/tests/reference/synthetic/merge-<n>/:
//   set.json     the set's settings, files and ids, and its three runs: each exception set with its rule and
//                ids, and the batch FuSION 1 ran (start with the exception file, mff, wgx, exit);
//   inputs/      the KGML files, mff's list, one exception file per run and an empty map file;
//   f1/<run>/    FuSION 1's wgx output, merged.xml, and summary.json, written by
//                backend/tests/experiments/w7/run_merges.py through run_fusion1.py.
// The exception sets: none; de, each id of the set with probability one in four, as a list of differentially
// expressed genes would take it; bridging, every id that lies in two or more boxes, so that every bridge
// between boxes breaks. merge-sets.tsv lists all 50 sets, and why a set was left out or run with a note.
//
// The hidden test "[.w7-write]" writes the inputs. The test below renders them again and fails when a recorded
// file differs, so the suite's sets and the sets FuSION 1 ran cannot drift apart.
//
// Each run's FuSION 1 graph, read with f1::read_written_graph, is compared with exception_merge and with
// merge_direct unless an expected difference (section 12, "Expected differences") applies. Whether one
// applies is decided from the inputs alone, by replaying FuSION 1's read of each file far enough to see each
// fix's trigger (XMLParser.cc: process_node_with_merge_exception, process_edge; GraphManagerNew.cc:
// add_component_edges_for_complexes):
//   XD-03 (S2.08)  a collapsing box finds its key's node in its file without all of its ids;
//   XD-04 (S2.21)  an undirected edge's tag, set from its file's keys, is not the one the merged keys give;
//   XD-05 (S2.10)  a duplicate edge skipped in its file carried a larger score than the edge kept;
//   XD-06 (S2.09)  a file holds a complex but no edge, so its component edges have no first edge to copy;
//   XD-16 (S2.11)  an entry reached only through indirect relations is a group, or a box giving a multi-id
//                  unit (prompt log entry 368);
//   XD-22 (03.C5)  a one-file set, a single read in FuSION 1, holds an id in two nodes;
//   XD-29 (S2.17)  a set of more than one file in which one file's read holds an id in two nodes, which
//                  FuSION 1's fold can keep apart.
// A run under one of them is named and skipped: its comparison is still made, and reported in a message
// rather than checked, so that a trigger which skips a run FuSION 1 and the oracles agree on shows (XD-29 and
// this report: prompt log entries 360 and 361). Any other difference fails: it is a finding for the user.
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "fusion/testkit/adapt.hpp"
#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/fusion1.hpp"
#include "fusion/testkit/oracles.hpp"
#include "fusion/testkit/scratch.hpp"
#include "bounded_sets.hpp"
#include "support.hpp"

namespace {

namespace fs = std::filesystem;
namespace testkit = fusion::testkit;
using fusion::core::Json;
using merge_test::Bounded;

constexpr std::uint64_t kSetSeed = 2022;    // the generated sets' seed, as test_split.cpp's bounded layer has it
constexpr std::uint64_t kDeSeed = 2025;     // the de exception sets: core::Rng stream n of this seed for set n
const std::vector<std::string> kRuns = {"none", "de", "bridging"};

fs::path synthetic() { return testkit::source_dir() / "reference" / "synthetic"; }

std::string folder_name(std::size_t n) {
    std::string digits = std::to_string(n);
    return "merge-" + std::string(digits.size() < 2 ? 2 - digits.size() : 0, '0') + digits;
}

std::string join(const std::vector<std::string>& parts, const std::string& by) {
    std::string out;
    for (const auto& p : parts) out += (out.empty() ? "" : by) + p;
    return out;
}

bool contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

// The 50 sets at their fixed seed: FUSION_TEST_SEED never changes what FuSION 1 ran.
std::vector<Bounded> w7_sets() {
    std::vector<Bounded> sets = merge_test::bounded_sets();
    merge_test::add_generated_sets(sets, kSetSeed);
    return sets;
}

// ---- the KGML texts as FuSION 1 reads them: the generators' own layout, one element per line

struct KEntry {
    std::string type;
    std::vector<std::string> ids;     // ':' removed, sorted, unique (S2.06)
    std::vector<int> components;
};
struct KRelation {
    int entry1 = 0, entry2 = 0;
    std::string type;
    std::vector<std::string> subtypes;   // normalised, in order (S2.10)
    std::string score;                   // experiment_score as written; empty when absent
};
struct KDoc {
    std::string pathway;
    std::map<int, KEntry> entries;
    std::vector<KRelation> relations;
};

std::string attribute(const std::string& line, const std::string& name) {
    const std::string key = " " + name + "=\"";
    const auto at = line.find(key);
    if (at == std::string::npos) return {};
    const auto from = at + key.size();
    return line.substr(from, line.find('"', from) - from);
}

std::string normalise_subtype(const std::string& name) {   // S2.10
    if (name == "indirect effect") return "indirect";
    if (name == "missing interaction") return "missing";
    if (name == "state change") return "state";
    if (name == "binding/association") return "association";
    return name;
}

KDoc parse(const std::string& text) {
    KDoc doc;
    KEntry* entry = nullptr;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        const auto first = line.find_first_not_of(' ');
        if (first == std::string::npos) continue;
        const std::string element = line.substr(first, line.find_first_of(" />", first + 1) - first);
        if (element == "<pathway") {
            doc.pathway = attribute(line, "name");
        } else if (element == "<entry") {
            entry = &doc.entries[std::stoi(attribute(line, "id"))];
            entry->type = attribute(line, "type");
            std::set<std::string> ids;
            std::istringstream names(attribute(line, "name"));
            for (std::string id; names >> id;) {
                id.erase(std::remove(id.begin(), id.end(), ':'), id.end());
                ids.insert(id);
            }
            entry->ids.assign(ids.begin(), ids.end());
        } else if (element == "<component") {
            entry->components.push_back(std::stoi(attribute(line, "id")));
        } else if (element == "<relation") {
            doc.relations.push_back({std::stoi(attribute(line, "entry1")), std::stoi(attribute(line, "entry2")),
                                     attribute(line, "type"), {}, attribute(line, "experiment_score")});
        } else if (element == "<subtype") {
            doc.relations.back().subtypes.push_back(normalise_subtype(attribute(line, "name")));
        }
    }
    return doc;
}

// ---- the three exception sets

// Each id of the set, in byte order, with probability one in four.
std::vector<std::string> de_like(const std::vector<std::string>& ids, std::size_t n) {
    auto rng = fusion::core::Rng::stream(kDeSeed, n);
    std::vector<std::string> out;
    for (const auto& id : ids)
        if (rng.below(4) == 0) out.push_back(id);
    return out;
}

// Every id that lies in two or more boxes (non-group entries) of the set's files.
std::vector<std::string> bridging(const std::vector<std::string>& texts) {
    std::map<std::string, int> boxes;
    for (const auto& text : texts)
        for (const auto& [id, entry] : parse(text).entries)
            if (entry.components.empty() && entry.type != "group")
                for (const auto& gene : entry.ids) ++boxes[gene];
    std::vector<std::string> out;
    for (const auto& [gene, count] : boxes)
        if (count >= 2) out.push_back(gene);
    return out;
}

std::vector<std::string> exceptions_of(const Bounded& set, std::size_t n, const std::string& run) {
    if (run == "de") return de_like(merge_test::ids_of(set.texts), n);
    if (run == "bridging") return bridging(set.texts);
    return {};
}

// ---- what FuSION 1 can run, and the files of a set's folder

// The set's indirect and ECrel relations, which the filters FuSION 1 lacks act on.
std::pair<int, int> filtered_relations(const Bounded& set) {
    int indirect = 0, ecrel = 0;
    for (const auto& text : set.texts)
        for (const auto& r : parse(text).relations) {
            indirect += contains(r.subtypes, "indirect") ? 1 : 0;
            ecrel += r.type == "ECrel" ? 1 : 0;
        }
    return {indirect, ecrel};
}

bool keeps_indirect(const Bounded& set) { return set.settings.filters.indirect != fusion::network::IndirectMode::discard; }
bool excludes_ecrel(const Bounded& set) { return set.settings.filters.ecrel != fusion::network::EcrelMode::keep; }

// Why FuSION 1 cannot run the set as the suite does, or empty: it always discards indirect relations and
// keeps ECrel ones (S1.29, S2.11, S2.31), so a set that keeps or excludes some is left out.
std::string left_out(const Bounded& set) {
    const auto [indirect, ecrel] = filtered_relations(set);
    std::vector<std::string> why;
    if (keeps_indirect(set) && indirect > 0) why.push_back("keeps indirect relations; FuSION 1 always discards them (S2.11, S1.29)");
    if (excludes_ecrel(set) && ecrel > 0) why.push_back("excludes ECrel relations; FuSION 1 always keeps them (S2.31)");
    return join(why, "; ");
}

// Why a set with such a setting is run all the same: it holds none of the relations the setting acts on.
std::string run_anyway(const Bounded& set) {
    const auto [indirect, ecrel] = filtered_relations(set);
    std::vector<std::string> why;
    if (keeps_indirect(set) && indirect == 0)
        why.push_back("keeps indirect relations, which FuSION 1 always discards; the set has none, so the setting changes nothing");
    if (excludes_ecrel(set) && ecrel == 0)
        why.push_back("excludes ECrel relations, which FuSION 1 always keeps; the set has none, so the setting changes nothing");
    return join(why, "; ");
}

std::string file_of(const std::string& text) {
    const std::string pathway = parse(text).pathway;
    return pathway.substr(pathway.find(':') + 1) + ".xml";
}

std::vector<std::string> batch(const Bounded& set, const std::string& run) {
    return {"start inputs/empty_file inputs/exceptions-" + run + ".txt inputs/empty_file inputs/empty_file",
            "mff inputs/list.txt xml " + std::to_string(set.settings.split_threshold) + " n inputs/empty_file -1",
            "wgx 0 merged.xml KEGG", "exit"};
}

std::string lines(const std::vector<std::string>& list) {
    std::string out;
    for (const auto& l : list) out += l + "\n";
    return out;
}

// The folder's files, by path relative to merge-<n>/.
std::map<std::string, std::string> folder_files(const Bounded& set, std::size_t n) {
    std::map<std::string, std::string> out;
    std::vector<std::string> files, list;
    for (const auto& text : set.texts) {
        files.push_back(file_of(text));
        list.push_back("inputs/" + files.back());
        out["inputs/" + files.back()] = text;
    }
    out["inputs/list.txt"] = lines(list);
    out["inputs/empty_file"] = "";
    Json doc;
    doc["set"] = n;
    doc["name"] = set.name;
    doc["source"] = n < merge_test::bounded_sets().size()
                        ? "bounded_sets(), backend/tests/unit/merge/bounded_sets.hpp"
                        : "add_generated_sets(seed " + std::to_string(kSetSeed) + "), backend/tests/unit/merge/bounded_sets.hpp";
    doc["threshold"] = set.settings.split_threshold;
    doc["filters"] = {{"indirect", keeps_indirect(set) ? "keep" : "discard"}, {"ecrel", excludes_ecrel(set) ? "exclude" : "keep"},
                      {"maplink", set.settings.filters.maplink == fusion::network::MaplinkMode::skip ? "skip" : "keep"}};
    if (const std::string note = run_anyway(set); !note.empty()) doc["note"] = note;
    doc["files"] = files;
    doc["ids"] = merge_test::ids_of(set.texts);
    Json runs = Json::array();
    for (const auto& run : kRuns) {
        const std::vector<std::string> e = exceptions_of(set, n, run);
        out["inputs/exceptions-" + run + ".txt"] = lines(e);
        Json r;
        r["exception_set"] = run;
        r["rule"] = run == "none" ? "no ids"
                    : run == "de" ? "each id of the set, in byte order, with probability one in four: core::Rng::stream(" +
                                        std::to_string(kDeSeed) + ", " + std::to_string(n) + ").below(4) == 0"
                                  : "every id that lies in two or more boxes (non-group entries) of the set's files";
        r["exceptions"] = "inputs/exceptions-" + run + ".txt";
        r["ids"] = e;
        r["batch"] = batch(set, run);
        r["output"] = "f1/" + run + "/merged.xml";
        runs.push_back(r);
    }
    doc["runs"] = runs;
    out["set.json"] = doc.dump(2) + "\n";
    return out;
}

// merge-sets.tsv: every set, with its folder, or why it was left out or is run despite a setting.
std::string sets_index(const std::vector<Bounded>& sets) {
    std::string out = "set\tname\tthreshold\tfilters\tfiles\tids\tindirect relations\tECrel relations\tfolder\tnote\n";
    for (std::size_t n = 0; n < sets.size(); ++n) {
        const Bounded& set = sets[n];
        const auto [indirect, ecrel] = filtered_relations(set);
        std::vector<std::string> filters;
        if (keeps_indirect(set)) filters.push_back("indirect kept");
        if (excludes_ecrel(set)) filters.push_back("ECrel excluded");
        const std::string why = left_out(set), note = run_anyway(set);
        out += std::to_string(n) + "\t" + set.name + "\t" + std::to_string(set.settings.split_threshold) + "\t" +
               (filters.empty() ? "default" : join(filters, ", ")) + "\t" + std::to_string(set.texts.size()) + "\t" +
               std::to_string(merge_test::ids_of(set.texts).size()) + "\t" + std::to_string(indirect) + "\t" +
               std::to_string(ecrel) + "\t" + (why.empty() ? folder_name(n) : "-") + "\t" +
               (!why.empty() ? "left out: " + why : note.empty() ? "-" : "run: " + note) + "\n";
    }
    return out;
}

std::optional<std::string> slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// ---- FuSION 1's read of one file, replayed far enough to see each fix's trigger

struct F1Node {
    std::string key;                  // FuSION 1's rep id
    std::vector<std::string> ids;     // all_id
    bool complex = false;
    std::vector<int> components;      // a complex's component nodes, in FuSION 1's order
};
struct F1Edge {
    int src = 0, tgt = 0;
    std::string type;
    std::vector<std::string> subtypes;   // without dummy_u_to_d
    bool tag = false;
    double score = 0;
    bool component = false;
};

class Fusion1Read {
public:
    Fusion1Read(const KDoc& doc, const std::set<std::string>& e, int threshold) : doc_(doc), e_(e), threshold_(threshold) {
        for (const KRelation& r : doc_.relations) {
            const bool indirect = contains(r.subtypes, "indirect");
            kept_ = !indirect;
            const std::set<int> from = node_set(r.entry1), to = node_set(r.entry2);
            if (indirect) continue;   // dropped after its entries became nodes (XMLParser.cc:682-683, 1481-1490)
            const bool undirected = contains(r.subtypes, "association") || contains(r.subtypes, "dissociation") ||
                                    contains(r.subtypes, "mapping");
            const fusion::core::Decimal d = fusion::core::parse_decimal(r.score);
            const double score = d.status == fusion::core::DecimalStatus::ok ? d.value : 0;
            for (int s : from)
                for (int t : to) {
                    attempt(s, t, r.type, r.subtypes, score, undirected && key(s) > key(t), false);
                    if (undirected) attempt(t, s, r.type, r.subtypes, score, key(t) > key(s), false);
                }
        }
        // Two-way component edges after the relations, with the pathways of the file's first edge
        // (GraphManagerNew.cc:3981-4071), which a file without edges does not have.
        const bool any_edge = !edges_.empty();
        for (std::size_t v = 0; v < nodes_.size(); ++v) {
            if (!nodes_[v].complex) continue;
            if (!any_edge) {
                complex_without_edges_ = true;
                break;
            }
            const int c = static_cast<int>(v);
            for (int part : nodes_[v].components) {
                attempt(part, c, "PPrel", {"component"}, 0, true, true);
                attempt(c, part, "PPrel", {"component"}, 0, true, true);
            }
        }
    }

    const std::vector<F1Node>& nodes() const { return nodes_; }
    const std::vector<F1Edge>& edges() const { return edges_; }
    int lost() const { return lost_; }                                         // S2.08's lost ids
    bool skipped_larger_score() const { return skipped_larger_score_; }        // S2.10
    bool complex_without_edges() const { return complex_without_edges_; }      // S2.09
    // S2.11: an entry reached only through indirect relations that is a group, or a box giving a multi-id
    // unit (it collapses, with two or more ids outside E); a one-id box, or one of one-id units, changes nothing.
    bool reached_only_indirectly() const {
        for (int id : reached_all_) {
            if (reached_kept_.contains(id)) continue;
            const KEntry& entry = doc_.entries.at(id);
            if (!entry.components.empty()) return true;
            const bool collapse = threshold_ >= 0 && entry.ids.size() > static_cast<std::size_t>(threshold_);
            const auto outside = std::count_if(entry.ids.begin(), entry.ids.end(),
                                               [this](const std::string& gene) { return !e_.contains(gene); });
            if (collapse && outside >= 2) return true;
        }
        return false;
    }
    bool id_in_two_nodes() const {
        std::map<std::string, int> holders;
        for (const F1Node& n : nodes_)
            if (!n.complex)
                for (const auto& id : n.ids)
                    if (++holders[id] > 1) return true;
        return false;
    }

private:
    const std::string& key(int v) const { return nodes_[static_cast<std::size_t>(v)].key; }

    // The entry's nodes (process_node_with_merge_exception, XMLParser.cc:875-1125).
    std::set<int> node_set(int id) {
        reached_all_.insert(id);
        if (kept_) reached_kept_.insert(id);
        const KEntry& entry = doc_.entries.at(id);
        std::set<int> out;
        if (!entry.components.empty()) {   // a group: one complex per combination (cartestian_product, 42-62)
            std::vector<std::set<int>> combos;
            for (int c : entry.components) {
                const std::set<int> part = node_set(c);
                if (combos.empty()) {
                    for (int n : part) combos.push_back({n});
                    continue;
                }
                std::vector<std::set<int>> next;
                for (int n : part)
                    for (const auto& combo : combos) {
                        auto x = combo;
                        x.insert(n);
                        next.push_back(std::move(x));
                    }
                combos = std::move(next);
            }
            for (const auto& combo : combos) {
                const std::vector<int> parts(combo.begin(), combo.end());
                const auto [it, fresh] = complex_by_parts_.try_emplace(parts, static_cast<int>(nodes_.size()));
                if (fresh) {
                    std::set<std::string> keys;
                    for (int p : parts) keys.insert(key(p));
                    F1Node n;
                    n.key = "c_s_" + join({keys.begin(), keys.end()}, "_") + "_c_e";
                    n.complex = true;
                    n.components = parts;
                    nodes_.push_back(std::move(n));
                }
                out.insert(it->second);
            }
            return out;
        }
        // A box (S2.07): exception ids alone; the rest under min(S \ E) when |S| > thres, else alone.
        const bool collapse = threshold_ >= 0 && entry.ids.size() > static_cast<std::size_t>(threshold_);
        std::vector<std::string> rest;
        for (const auto& gene : entry.ids)
            if (!e_.contains(gene)) rest.push_back(gene);
        std::set<std::string> reps;
        for (const auto& gene : entry.ids) reps.insert(e_.contains(gene) || !collapse ? gene : rest.front());
        for (const std::string& rep : reps) {
            const auto [it, fresh] = node_by_key_.try_emplace(rep, static_cast<int>(nodes_.size()));
            if (fresh) {
                F1Node n;
                n.key = rep;
                n.ids = e_.contains(rep) || !collapse ? std::vector<std::string>{rep} : rest;
                nodes_.push_back(std::move(n));
            } else if (!e_.contains(rep) && collapse) {   // S2.08: an existing node gains none of the box's ids
                for (const auto& gene : rest)
                    if (!contains(nodes_[static_cast<std::size_t>(it->second)].ids, gene)) ++lost_;
            }
            out.insert(it->second);
        }
        return out;
    }

    // One edge, unless the file has one of the same ends, type and subtypes, with or without the tag
    // (process_edge, XMLParser.cc:1493-1501); a skipped duplicate's score is lost.
    void attempt(int s, int t, const std::string& type, const std::vector<std::string>& subtypes, double score, bool tag,
                 bool component) {
        for (const F1Edge& e : edges_)
            if (e.src == s && e.tgt == t && e.type == type && e.subtypes == subtypes) {
                if (score > e.score) skipped_larger_score_ = true;
                return;
            }
        edges_.push_back({s, t, type, subtypes, tag, score, component});
    }

    const KDoc& doc_;
    const std::set<std::string>& e_;
    int threshold_;
    bool kept_ = true;
    std::vector<F1Node> nodes_;
    std::map<std::string, int> node_by_key_;
    std::map<std::vector<int>, int> complex_by_parts_;
    std::vector<F1Edge> edges_;
    std::set<int> reached_all_, reached_kept_;
    int lost_ = 0;
    bool skipped_larger_score_ = false, complex_without_edges_ = false;
};

// The expected differences that apply to a run, from its inputs and the merged keys exception_merge gives.
std::vector<std::string> expected_differences(const Bounded& set, const std::set<std::string>& e,
                                              const testkit::CanonGraph& oracle) {
    std::map<std::string, std::string> merged;   // id -> the module key of its merged node
    for (const auto& n : oracle.nodes)
        if (n.type != "group")
            for (const auto& m : n.members) merged[m] = testkit::module_key(n);
    bool xd03 = false, xd04 = false, xd05 = false, xd06 = false, xd16 = false, xd22 = false, xd29 = false;
    for (const auto& text : set.texts) {
        const KDoc doc = parse(text);
        const Fusion1Read f1(doc, e, set.settings.split_threshold);
        xd03 = xd03 || f1.lost() > 0;
        xd05 = xd05 || f1.skipped_larger_score();
        xd06 = xd06 || f1.complex_without_edges();
        xd16 = xd16 || f1.reached_only_indirectly();
        xd22 = xd22 || (set.texts.size() == 1 && f1.id_in_two_nodes());
        xd29 = xd29 || (set.texts.size() > 1 && f1.id_in_two_nodes());
        std::vector<std::string> merged_key(f1.nodes().size());
        for (std::size_t v = 0; v < f1.nodes().size(); ++v) {   // components come before their complexes
            const F1Node& n = f1.nodes()[v];
            if (!n.complex) {
                const auto it = merged.find(n.key);
                merged_key[v] = it == merged.end() ? std::string() : it->second;
                continue;
            }
            std::set<std::string> parts;
            for (int p : n.components) parts.insert(merged_key[static_cast<std::size_t>(p)]);
            merged_key[v] = "c_s_" + join({parts.begin(), parts.end()}, "_") + "_c_e";
        }
        for (const F1Edge& edge : f1.edges()) {
            if (edge.component) continue;
            if (!contains(edge.subtypes, "association") && !contains(edge.subtypes, "dissociation") &&
                !contains(edge.subtypes, "mapping"))
                continue;
            const std::string& s = merged_key[static_cast<std::size_t>(edge.src)];
            const std::string& t = merged_key[static_cast<std::size_t>(edge.tgt)];
            if (s.empty() || t.empty()) {   // a kept relation's ends are always merged: the replay is wrong
                FAIL_CHECK(doc.pathway << ": an undirected edge's end is in no merged node");
                xd04 = true;
                continue;
            }
            xd04 = xd04 || edge.tag != (s != t && t < s);
        }
    }
    std::vector<std::string> out;
    if (xd03) out.push_back("XD-03");
    if (xd04) out.push_back("XD-04");
    if (xd05) out.push_back("XD-05");
    if (xd06) out.push_back("XD-06");
    if (xd16) out.push_back("XD-16");
    if (xd22) out.push_back("XD-22");
    if (xd29) out.push_back("XD-29");
    return out;
}

fusion::merge::ExceptionSet exception_set(const std::set<std::string>& ids) {
    fusion::merge::ExceptionSet e;
    for (const auto& id : ids) e.add(std::string_view(id), fusion::merge::Reason::up);
    return e;
}

}  // namespace

TEST_CASE("W7: write the bounded sets as FuSION 1 inputs", "[.w7-write]") {
    // Writes each folder's set.json and inputs/ and merge-sets.tsv. A file that exists is kept when it holds
    // the same bytes; one that differs stops the writer, since FuSION 1's recorded runs read it.
    const std::vector<Bounded> sets = w7_sets();
    REQUIRE(sets.size() == 50);
    std::map<fs::path, std::string> files{{synthetic() / "merge-sets.tsv", sets_index(sets)}};
    for (std::size_t n = 0; n < sets.size(); ++n)
        if (left_out(sets[n]).empty())
            for (const auto& [rel, text] : folder_files(sets[n], n)) files[synthetic() / folder_name(n) / rel] = text;
    int written = 0;
    for (const auto& [path, text] : files) {
        const std::optional<std::string> old = slurp(path);
        INFO(fusion::core::to_utf8(path));
        if (old) {
            const bool same = *old == text;
            REQUIRE(same);
            continue;
        }
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
        const bool done = slurp(path) == text;
        REQUIRE(done);
        ++written;
    }
    WARN("W7 inputs: " << written << " of " << files.size() << " files written under " << fusion::core::to_utf8(synthetic()));
}

TEST_CASE("W7: FuSION 1's merge of the bounded sets against exception_merge and merge_direct",
          "[merge][ref][S2.22][12.C14]") {
    const std::vector<Bounded> sets = w7_sets();
    REQUIRE(sets.size() == 50);
    {
        INFO("merge-sets.tsv differs from the suite's sets: rerun W7 (test_merge \"[.w7-write]\", then run_merges.py)");
        const bool same = slurp(synthetic() / "merge-sets.tsv") == sets_index(sets);
        CHECK(same);
    }
    merge_test::Context c;
    std::map<std::string, std::pair<int, int>> per_xd;   // runs skipped under it, and how many of them differ
    std::string skipped;
    int runs = 0, compared = 0, equal = 0, skipped_differ = 0;
    for (std::size_t n = 0; n < sets.size(); ++n) {
        const Bounded& set = sets[n];
        const fs::path folder = synthetic() / folder_name(n);
        if (!left_out(set).empty()) {
            CHECK_FALSE(fs::exists(folder));
            continue;
        }
        INFO(folder_name(n) << " (" << set.name << ")");
        for (const auto& text : set.texts)
            for (const auto& r : parse(text).relations) REQUIRE(r.type != "maplink");   // XD-26 would apply
        for (const auto& [rel, text] : folder_files(set, n)) {
            INFO(rel << " differs from the suite's set: rerun W7 (test_merge \"[.w7-write]\", then run_merges.py)");
            const bool same = slurp(folder / rel) == text;
            CHECK(same);
        }
        const auto docs = merge_test::read_texts(c, set.texts, folder_name(n));
        for (const auto& run : kRuns) {
            ++runs;
            const std::vector<std::string> ids = exceptions_of(set, n, run);
            const std::set<std::string> e(ids.begin(), ids.end());
            INFO("exception set " << run << ": {" << join(ids, " ") << "}, threshold " << set.settings.split_threshold
                                  << "; reproduce with " << folder_name(n) << "/inputs/ and its set.json");
            const fs::path out = folder / "f1" / run;
            const std::optional<std::string> summary = slurp(out / "summary.json");
            REQUIRE(summary.has_value());   // every run was made
            testkit::MergeOracleSettings o;
            o.threshold = set.settings.split_threshold;
            o.keep_indirect = keeps_indirect(set);
            o.keep_ecrel = !excludes_ecrel(set);
            const testkit::CanonGraph oracle = testkit::exception_merge_texts(set.texts, e, o);
            const std::vector<std::string> xds = expected_differences(set, e, oracle);
            const Json facts = Json::parse(*summary);
            INFO("FuSION 1 exited " << facts.value("exit", -1));
            const bool written = fs::exists(out / "merged.xml");
            testkit::GraphDiff to_oracle, to_direct;
            if (written) {
                const testkit::CanonGraph fusion1 = testkit::f1::read_written_graph(out / "merged.xml");
                const testkit::CanonGraph direct =
                    testkit::adapt::canon(*fusion::merge::merge_direct(docs, set.settings, exception_set(e)));
                to_oracle = testkit::diff(fusion1, oracle);
                to_direct = testkit::diff(fusion1, direct);
            }
            if (!xds.empty()) {
                // Skipped: the comparison is reported, never checked, so that a trigger which skips a run
                // FuSION 1 and the oracles agree on shows.
                const bool differs = !written || !to_oracle.empty() || !to_direct.empty();
                for (const auto& xd : xds) {
                    ++per_xd[xd].first;
                    per_xd[xd].second += differs ? 1 : 0;
                }
                skipped_differ += differs ? 1 : 0;
                skipped += "\n  " + folder_name(n) + " " + run + ": " + join(xds, ", ") +
                           (!written ? " (no merged.xml)" : differs ? " (differs)" : " (equal)");
                continue;
            }
            ++compared;
            if (!written) {
                FAIL_CHECK("FuSION 1 wrote no merged.xml");
                continue;
            }
            {
                INFO("FuSION 1 (a) against exception_merge (b):\n" << to_oracle.text());
                CHECK(to_oracle.empty());
            }
            {
                INFO("FuSION 1 (a) against merge_direct (b):\n" << to_direct.text());
                CHECK(to_direct.empty());
            }
            equal += to_oracle.empty() && to_direct.empty() ? 1 : 0;
        }
    }
    std::string counts;
    for (const auto& [xd, k] : per_xd)
        counts += (counts.empty() ? "" : ", ") + xd + " " + std::to_string(k.first) + " (" + std::to_string(k.second) +
                  " differ)";
    WARN("W7: " << runs << " runs; " << compared << " compared, " << equal << " equal to both oracles; "
                << runs - compared << " skipped under an expected difference, " << skipped_differ
                << " of them differing from an oracle (" << (counts.empty() ? "none" : counts)
                << "; a run can fall under several):" << skipped);
    CHECK(runs == 3 * static_cast<int>(std::count_if(sets.begin(), sets.end(),
                                                     [](const Bounded& b) { return left_out(b).empty(); })));
}
