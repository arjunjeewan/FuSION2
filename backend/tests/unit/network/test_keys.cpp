// Stable keys, node kinds, subtype names and the one polarity rule (S4.14, 02.C11, S2.10, S6.14, S5.13).
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace network_test;
using net::Effect;
using net::EndState;

TEST_CASE("network: an edge key prints as src|tgt|type|subtypes and parses back (02.C11)", "[unit][network]") {
  const net::EdgeKey key{{"t2"}, {"c_s_t2_t6_c_e"}, "PPrel", {"activation", "phosphorylation"}};
  CHECK(key.text() == "t2|c_s_t2_t6_c_e|PPrel|activation,phosphorylation");
  CHECK(net::EdgeKey::parse(key.text()) == key);
  const net::EdgeKey bare{{"a"}, {"b"}, "PCrel", {}};
  CHECK(bare.text() == "a|b|PCrel|");
  CHECK(net::EdgeKey::parse("a|b|PCrel|") == bare);
  for (const char* bad : {"a|b|PPrel", "a|b|PPrel|x|y", "|b|PPrel|x", "a||PPrel|x", "a|b||x"}) {
    INFO(bad);
    CHECK_THROWS_AS(net::EdgeKey::parse(bad), core::Failure);
  }
  // Byte order: the key's ordering equals the canonical edge order (02.C10).
  CHECK(net::EdgeKey{{"t1"}, {"t9"}, "PPrel", {}} < net::EdgeKey{{"t10"}, {"t1"}, "PPrel", {}});
}

TEST_CASE("network: node kinds follow the entry type (item 2B.1)", "[unit][network]") {
  CHECK(net::kind_of("gene") == net::NodeKind::gene);
  CHECK(net::kind_of("group") == net::NodeKind::complex);
  CHECK(net::kind_of("compound") == net::NodeKind::compound);
  CHECK(net::kind_of("map") == net::NodeKind::other);
  CHECK(net::kind_of("ortholog") == net::NodeKind::other);
  CHECK(net::kind_of("Gene") == net::NodeKind::other);
}

TEST_CASE("network: subtype names are normalised by S2.10's table and kept otherwise", "[unit][network]") {
  CHECK(net::normalise_subtype("indirect effect") == "indirect");
  CHECK(net::normalise_subtype("missing interaction") == "missing");
  CHECK(net::normalise_subtype("state change") == "state");
  CHECK(net::normalise_subtype("binding/association") == "association");
  CHECK(net::normalise_subtype("activation") == "activation");
  CHECK(net::normalise_subtype("hidden compound") == "hidden compound");
  CHECK(net::normalise_subtype("Indirect effect") == "Indirect effect");
}

TEST_CASE("network: effect_of: inhibition iff inhibition or repression (S5.13, S6.14)", "[unit][network]") {
  using V = std::vector<std::string>;
  CHECK(net::effect_of(V{"inhibition", "dephosphorylation"}) == Effect::inhibition);
  CHECK(net::effect_of(V{"repression"}) == Effect::inhibition);
  CHECK(net::effect_of(V{}) == Effect::activation);
  CHECK(net::effect_of(V{"activation", "phosphorylation"}) == Effect::activation);
  CHECK(net::effect_of(V{"component"}) == Effect::activation);
  CHECK(net::effect_of(V{"association"}) == Effect::activation);
  CHECK(net::effect_of(V{"indirect"}) == Effect::activation);
  CHECK(net::effect_of(V{"expression"}) == Effect::activation);
}

TEST_CASE("network: is_relaxed is FuSION 1's rule over the whole 3 x 3 x 2 table (S6.14)", "[unit][network]") {
  // Linux/src/GraphManagerNew.cc:28550-28594: inhibition relaxed when both ends are present and both
  // active or both inactive; activation relaxed when both are present and not both active.
  const EndState states[] = {EndState::absent, EndState::inactive, EndState::active};
  int relaxed = 0;
  for (Effect effect : {Effect::activation, Effect::inhibition})
    for (EndState s : states)
      for (EndState t : states) {
        bool expected = false;
        if (s != EndState::absent && t != EndState::absent) {
          const bool sa = s == EndState::active, ta = t == EndState::active;
          expected = effect == Effect::inhibition ? (sa && ta) || (!sa && !ta) : (sa && !ta) || (!sa && ta) || (!sa && !ta);
        }
        CHECK(net::is_relaxed(effect, s, t) == expected);
        relaxed += expected ? 1 : 0;
      }
  CHECK(relaxed == 5);                                      // 3 activation rows and 2 inhibition rows
  CHECK_FALSE(net::is_relaxed(Effect::activation, EndState::active, EndState::active));
  CHECK(net::is_relaxed(Effect::inhibition, EndState::inactive, EndState::inactive));
  CHECK_FALSE(net::is_relaxed(Effect::inhibition, EndState::active, EndState::absent));
}
