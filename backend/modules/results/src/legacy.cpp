// results/src/legacy.cpp: the two-column file and FuSION 1's text formats (section 07, "FuSION 1 text
// formats"; E1, S5.24, S5.25, S5.27, cross-section item 1.21, 01.C11). FuSION 1's writers are
// Linux/src/main.cc:4920-5060 (PO.dat, the solution loop) and GraphManagerNew.cc:30848-31017
// (print_variables_from_z3_model) for the dumps, and GraphManagerNew.cc:29361-29829 for the limits file.
#include "fusion/results/legacy.hpp"

#include <algorithm>
#include <utility>

#include "text.hpp"

namespace fusion::results {

namespace {

const CaseResult* find_case(const SolveResult& r, Case kind) {
  for (const CaseResult& c : r.cases) {
    if (c.kind == kind) return &c;
  }
  return nullptr;
}

// FuSION 1's order of points: n descending, ties by e ascending (compare_pair_on_second_descending,
// main.cc:1002-1007, applied at main.cc:4921).
std::vector<const FrontierPoint*> legacy_order(const CaseResult& c) {
  std::vector<const FrontierPoint*> points;
  for (const FrontierPoint& p : c.points) points.push_back(&p);
  std::stable_sort(points.begin(), points.end(), [](const FrontierPoint* a, const FrontierPoint* b) {
    return a->at.n != b->at.n ? a->at.n > b->at.n : a->at.e < b->at.e;
  });
  return points;
}

std::string header_line(const CaseResult& c, const FrontierPoint& p) {
  // FuSION 1's parsers read fields 3 and 5 (plot_explanation.py:89-98; GraphManagerNew.cc sscanf), so
  // the uncertified suffix of S5.19 leaves them as they are.
  std::string line = "Current PO point: " + std::to_string(p.at.n) + " nodes, " + std::to_string(p.at.e) +
                     " edges relaxed";
  if (!p.certified) {
    std::string cells;
    for (const Point b : p.undecided_below) {
      if (!cells.empty()) cells += "; ";
      cells += point_text(b);
      for (const UndecidedCell& u : c.undecided) {
        if (u.at == b) cells += ", " + u.label;
      }
    }
    line += cells.empty() ? std::string(" (not certified)") : " (not certified: blocked by " + cells + ")";
  }
  return line + "\n";
}

// The representative's symbol, or the key when the map has none: FuSION 1's display name
// (kegg_hsa_id_to_display_name_map[rep id], GraphManagerNew.cc:30902-30905), without 07.C10's family
// size, so FuSION 1's own parsers and the user's scripts read the same text.
std::string symbol_of(const NodeRecord& n) {
  for (const Member& m : n.members) {
    if (m.id == n.key.value && !m.symbol.empty()) return m.symbol;
  }
  return n.key.value;
}

std::string_view expression_of(Observed o) {
  switch (o) {
    case Observed::up: return "up_reg";
    case Observed::down: return "down_reg";
    case Observed::none: break;
  }
  return "not_diff_expr";
}

class Lines {
 public:
  Lines(const SolveResult& r, const Witness& w) : r_(r), w_(w) {}

  bool present(NodeIx v) const { return has(w_.active, v) || has(w_.inactive, v); }

  // nid rep_id symbol expression present|not_present active|inactive|NA: six fields (S5.27). Column 1
  // holds the key instead of the session's node id (S4.14); an absent node's activity is NA.
  std::string node(NodeIx v) const {
    const NodeRecord& n = r_.tables.nodes[v];
    const bool is_present = present(v);
    std::string line = n.key.value + "\t" + n.key.value + "\t" + symbol_of(n) + "\t" +
                       std::string(expression_of(n.observed)) + "\t";
    line += is_present ? "present\t" : "not_present\t";
    line += !is_present ? "NA" : has(w_.active, v) ? "active" : "inactive";
    return line + "\n";
  }

  // eid present|not_present subtypes src src_sym tgt tgt_sym src_present src_active tgt_present
  // "tgt_active [pathways]": eleven fields (S5.27). Column 1 holds the edge key; subtypes are joined
  // with '_' as FuSION 1 joined them (GraphManagerNew.cc:30989-30990), so its polarity split still works.
  std::string edge(EdgeIx e) const {
    const EdgeRecord& rec = r_.tables.edges[e];
    const auto end = [&](NodeIx v) {
      const NodeRecord& n = r_.tables.nodes[v];
      return n.key.value + "\t" + symbol_of(n);
    };
    const auto state = [&](NodeIx v, std::string_view side) {
      const std::string s(side);
      if (!present(v)) return s + "_not_present\t" + s + "_NA";
      return s + "_present\t" + s + (has(w_.active, v) ? "_active" : "_inactive");
    };
    std::string subtypes;
    for (std::size_t i = 0; i < rec.subtypes.size(); ++i) {
      if (i > 0) subtypes += "_";
      subtypes += rec.subtypes[i];
    }
    std::string pathways;
    for (std::size_t i = 0; i < rec.pathways.size(); ++i) {
      if (i > 0) pathways += " ";
      pathways += rec.pathways[i];
    }
    return rec.key.text() + "\t" + (has(w_.selected, e) ? "present" : "not_present") + "\t" + subtypes + "\t" +
           end(rec.source) + "\t" + end(rec.target) + "\t" + state(rec.source, "src") + "\t" +
           state(rec.target, "tgt") + " [" + pathways + "]\n";
  }

 private:
  template <class T>
  static bool has(const std::vector<T>& sorted, T v) {
    return std::binary_search(sorted.begin(), sorted.end(), v);
  }
  const SolveResult& r_;
  const Witness& w_;
};

// One dump: per point its header, then per recorded witness "Solution k", a node block and an edge
// block, as main.cc:4975-5060 writes them. Only recorded witnesses are written, so there is no empty
// "Solution 2"; "No more solutions. Obtained unsat" follows only a complete enumeration (S5.26).
template <class NodesOf, class EdgesOf>
std::string dump(const SolveResult& r, Case kind, NodesOf nodes_of, EdgesOf edges_of) {
  const CaseResult* c = find_case(r, kind);
  if (c == nullptr) return {};
  std::string out;
  for (const FrontierPoint* p : legacy_order(*c)) {
    out += header_line(*c, *p);
    for (std::size_t k = 0; k < p->witnesses.size(); ++k) {
      const Witness& w = p->witnesses[k];
      const Lines lines(r, w);
      out += "Solution " + std::to_string(k + 1) + "\n";
      for (const NodeIx v : nodes_of(w)) out += lines.node(v);
      out += "\n";
      for (const EdgeIx e : edges_of(w)) out += lines.edge(e);
      out += "\n\n";
    }
    if (p->enumeration == Enumeration::complete) out += "No more solutions. Obtained unsat\n";
    out += "\n";
  }
  return out;
}

template <class T>
std::vector<T> merged(const std::vector<T>& a, const std::vector<T>& b) {
  std::vector<T> out = a;
  out.insert(out.end(), b.begin(), b.end());
  std::sort(out.begin(), out.end());
  return out;
}

std::string_view step_name(solve::Phase phase) {
  // The labels FuSION 1's TIMEOUT and UNDEFINED lines give each step (GraphManagerNew.cc:29481, 29632,
  // 29745); a points request (06.C9) asks without a search.
  switch (phase) {
    case solve::Phase::diagonal: return "diagonal-loop";
    case solve::Phase::smallest_n: return "smallest-n";
    case solve::Phase::smallest_e: return "smallest-e";
    case solve::Phase::point: return "point";
    case solve::Phase::extra: return "extra";
  }
  return "search";
}

std::string unknown_line(const solve::Answer& a, std::string_view where) {
  // S5.19's two labels, as FuSION 1 words them (GraphManagerNew.cc:29481, 29487), points as (e, n).
  const std::string e = std::to_string(a.point.e), n = std::to_string(a.point.n);
  const std::string step(step_name(a.phase));
  if (a.outcome == solve::Outcome::timeout) {
    return "TIMEOUT (" + step + " " + std::string(where) + ") at point e=" + e + " n=" + n + " (z3 reason: " +
           a.reason + "). Consider increasing the solver timeout ($CONSTR_SOLVER_TIMEOUT1/$CONSTR_SOLVER_TIMEOUT2 "
           "in the batch file).\n";
  }
  return "UNDEFINED (" + step + " " + std::string(where) + "; z3::unknown, reason: " + a.reason + ") at point e=" +
         e + " n=" + n + "\n";
}

std::string_view result_word(solve::Outcome o) {
  switch (o) {
    case solve::Outcome::sat: return "sat";
    case solve::Outcome::unsat: return "unsat";
    default: return "unknown";            // z3::check_result's third value, as FuSION 1 printed it
  }
}

}  // namespace

std::string two_column(const CaseResult& c) {
  // S5.25 as 06.C20 amends it: one n<TAB>e line per point, n descending as FuSION 1 wrote PO.dat
  // (main.cc:4920-4955), without its dominance filter, which never fired; empty with no point (S5.26).
  std::string out;
  for (const FrontierPoint* p : legacy_order(c)) {
    out += std::to_string(p->at.n) + "\t" + std::to_string(p->at.e) + "\n";
  }
  return out;
}

std::string legacy_solutions(const SolveResult& r, Case kind) {
  // Every present node and every selected edge (GraphManagerNew.cc:30902-30905, 30998-31003).
  return dump(r, kind, [](const Witness& w) { return merged(w.active, w.inactive); },
              [](const Witness& w) { return w.selected; });
}

std::string legacy_relaxations(const SolveResult& r, Case kind) {
  // Every relaxed node, present or not, and every relaxed edge, selected or not
  // (GraphManagerNew.cc:30907-30910, 31005-31009).
  return dump(r, kind, [](const Witness& w) { return merged(w.relaxed_present, w.relaxed_absent); },
              [](const Witness& w) { return merged(w.relaxed_selected, w.relaxed_unselected); });
}

std::string legacy_limits(const solve::CaseResult& c) {
  // FuSION 1's limits file traced its search in free text (S5.24). The user's compare_sweep.py and
  // compare_candidates.py read only its checks: "Relax bounds[ (node call)| (edge call)]: <e> <n>",
  // "Time taken (sec): <cpu>", the TIMEOUT and UNDEFINED lines and sat, unsat or unknown, and count
  // the result lines after cache hits ("Point <e> <n> already sat") too. Those are written from 06's
  // answers in order; the extra-solution checks were never in the file (main.cc:4998-5058).
  std::string out;
  for (const solve::Answer& a : c.log) {
    if (a.phase == solve::Phase::extra) continue;
    const std::string e = std::to_string(a.point.e), n = std::to_string(a.point.n);
    if (a.source != solve::Source::z3) {
      // Answered without Z3: FuSION 1's cache-hit lines (GraphManagerNew.cc:29423-29432).
      out += "Point " + e + " " + n + " already " + std::string(result_word(a.outcome)) + "\n";
      out += std::string(result_word(a.outcome)) + "\n";
      continue;
    }
    switch (a.phase) {
      case solve::Phase::smallest_n: out += "Relax bounds (node call): "; break;
      case solve::Phase::smallest_e: out += "Relax bounds (edge call): "; break;
      default: out += "Relax bounds: "; break;
    }
    out += e + " " + n + "\n";
    out += "Time taken (sec): " + core::format_number(a.cpu_s) + "\n";
    const bool labelled = a.outcome == solve::Outcome::timeout || a.outcome == solve::Outcome::undefined;
    if (labelled) out += unknown_line(a, "cache");
    out += std::string(result_word(a.outcome)) + "\n";
    if (labelled) out += unknown_line(a, "steering");
  }
  return out;
}

void write_legacy(const SolveResult& r, const solve::Results& results, std::string_view legacy_prefix,
                  core::Context& ctx) {
  // Cross-section item 1.21: with legacy_prefix P, <P>_solutions.txt and <P>_relaxations.txt (with
  // several cases <P>_<case>_…) in pathz3/legacy/, and at P's own place too when P is absolute (2B.3).
  // The limits rendering goes beside them, under the same stem.
  const std::string base = detail::base_name(legacy_prefix);
  const std::filesystem::path stem_path = core::path_from_utf8(legacy_prefix);
  const bool absolute = stem_path.is_absolute();
  std::vector<const CaseResult*> cases;
  for (const CaseResult& c : r.cases) {
    if (c.status != CaseStatus::not_requested) cases.push_back(&c);
  }
  const bool several = cases.size() > 1;
  for (const CaseResult* c : cases) {
    const std::string suffix = several ? "_" + std::string(case_name(c->kind)) : std::string();
    const std::pair<std::string_view, std::string> files[] = {
        {"_solutions.txt", legacy_solutions(r, c->kind)},
        {"_relaxations.txt", legacy_relaxations(r, c->kind)}};
    for (const auto& [ending, text] : files) {
      core::OutputFile file = ctx.files.create("legacy/" + base + suffix + std::string(ending));
      if (!text.empty()) file.out() << text;
      if (absolute) {
        std::filesystem::path at = stem_path;
        at += core::path_from_utf8(suffix + std::string(ending));
        file.also_at(at);
      }
      file.commit();
    }
    for (const solve::CaseResult& sc : results.cases) {
      if (sc.kase != c->kind) continue;
      const std::string limits = legacy_limits(sc);
      core::OutputFile file = ctx.files.create("legacy/" + base + suffix + "_limits_timefile.txt");
      if (!limits.empty()) file.out() << limits;
      file.commit();
    }
  }
}

}  // namespace fusion::results
