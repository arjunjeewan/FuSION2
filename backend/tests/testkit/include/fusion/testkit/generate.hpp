// fusion/testkit/generate.hpp: seeded generators for property tests (12.C9). They draw only from
// core::Rng (std::mt19937_64 with core's own distributions, 09.C15), never from <random>'s
// distributions, so a seed gives the same inputs on every platform.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "fusion/core/core.hpp"
#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/check.hpp"

namespace fusion::testkit {

// FUSION_TEST_SEED when set (the weekly run), else the test's fixed seed (12.C9).
std::uint64_t test_seed(std::uint64_t fixed);

// ---- synthetic pathways in KGML format, with made-up ids ("t:1", compounds "cpd:x1"; S8.31)
struct PathwayShape {
  int entries = 30, relations = 40, max_box = 6, groups = 3;
  double shared_id = 0.2, undirected = 0.15, indirect = 0.05, ecrel = 0.03, duplicate = 0.05;
};
struct SyntheticPathway { std::string id; std::string kgml; };   // id: the root's name, "path:t00001"
// `entries` boxes of 1 to max_box ids (now and then a compound), `groups` groups over earlier entries (a
// group of a group too) and `relations` relations among them. Ids come from a pool of `entries` ids that
// every pathway shares, with probability shared_id, else from a pool of `entries` ids of the pathway's
// own, so boxes overlap within and across pathways and a set of `count` pathways holds at most
// entries * (count + 1) ids. undirected, indirect and ecrel are the shares of such relations, duplicate the
// share of relations repeated exactly.
std::vector<SyntheticPathway> pathways(core::Rng&, int count, const PathwayShape&);

// A two-column value file "<id>\t<value>" with values in [-3, 3] at two decimals; tie_share of the
// lines repeat an earlier value. LF line ends, or CRLF.
std::string value_file(core::Rng&, std::span<const std::string> ids, double tie_share, bool crlf);

// ---- tiny solve cases (section 12's solve row: 6 nodes, 8 edges per case)
struct TinyCase { CanonCorridor corridor; CaseSpec spec; };
// A corridor of exactly `nodes` nodes ("S", "T", "N1", ...; at times one of them a complex "{Ni,Nj}") and
// `edges` edges on walks of at most `bound` edges, as walk_corridor() keeps them, with random scores and
// inhibitions; and a case on it: observations, lists, edge lists, coexpressed pairs, a frozen threshold,
// an FSC with its case, and the named or positional form.
TinyCase tiny_case(core::Rng&, int nodes, int edges, int bound);

}  // namespace fusion::testkit
