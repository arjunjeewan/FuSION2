// fusion/testkit/adapt.hpp: module types as the kit's plain types (fusion_testkit only; it links
// libfusion). An adapter only reads a module's value and writes it in the kit's terms; the rules the
// oracles apply never come from here.
#pragma once

#include "fusion/corridor/corridor.hpp"
#include "fusion/network/network.hpp"
#include "fusion/solve/solve.hpp"
#include "fusion/testkit/canon.hpp"
#include "fusion/testkit/check.hpp"

namespace fusion::testkit::adapt {

CanonGraph canon(const network::Network&);
CanonGraph canon(const network::GraphView&);
CanonCorridor canon(const corridor::Corridor&);
// A case graph knows each node by its key only: a gene node's canonical key is its key, its one member;
// a complex keeps its key, with its (closed) component keys as members.
CanonCorridor canon(const solve::CaseGraph&);

// The case as the kit's ids: the bound node keys of every list, the observations' nodes, the FSC and its
// case, the families as S5.03 turns them on (a given flag decides, else the inputs given; a negative
// threshold is off), and the form (06.C16). Edge lists name CaseGraph edges, so they need the graph: the
// first form throws core::Failure when the spec has any.
CaseSpec spec(const solve::CaseSpec&, const solve::Settings&, solve::Case);
CaseSpec spec(const solve::CaseGraph&, const solve::CaseSpec&, const solve::Settings&, solve::Case);

Assignment assignment(const solve::Witness&);                          // keys as canon(CaseGraph) writes them
Assignment assignment(const corridor::Corridor&, const solve::Witness&);   // keys as canon(Corridor) writes them

}  // namespace fusion::testkit::adapt
