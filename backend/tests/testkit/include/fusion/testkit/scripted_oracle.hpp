// fusion/testkit/scripted_oracle.hpp: section 12's ScriptedOracle (fusion_testkit only): a monotone
// staircase of SAT points with overrides, driving section 06's Pareto search without Z3.
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "fusion/solve/solve.hpp"

namespace fusion::testkit {

class ScriptedOracle final : public solve::Oracle {
 public:
  explicit ScriptedOracle(std::vector<solve::Point> staircase,                 // minimal SAT points
                          std::map<solve::Point, solve::Outcome> overrides = {});   // e.g. timeout, undefined
  solve::Answer ask(solve::Point, solve::Phase) override;    // monotone, or the override
  int calls(solve::Point) const;                             // S5.20: never above 1
  bool truth(solve::Point) const;                            // SAT under the staircase
  std::function<std::optional<solve::Point>(solve::Point)> model;   // a SAT answer's model counts, if set
  std::vector<std::pair<solve::Point, solve::Phase>> asked;
  std::vector<solve::Witness> witnesses;                     // one per SAT answer, with its counts

 private:
  std::vector<solve::Point> staircase_;
  std::map<solve::Point, solve::Outcome> overrides_;
  std::map<solve::Point, int> calls_;
};

}  // namespace fusion::testkit
