// The scripted oracle (scripted_oracle.hpp).
#include "fusion/testkit/scripted_oracle.hpp"

#include <algorithm>

namespace fusion::testkit {

ScriptedOracle::ScriptedOracle(std::vector<solve::Point> staircase, std::map<solve::Point, solve::Outcome> overrides)
    : staircase_(std::move(staircase)), overrides_(std::move(overrides)) {}

bool ScriptedOracle::truth(solve::Point p) const {
  return std::any_of(staircase_.begin(), staircase_.end(), [&](solve::Point s) { return solve::below_or_equal(s, p); });
}

int ScriptedOracle::calls(solve::Point p) const {
  const auto it = calls_.find(p);
  return it == calls_.end() ? 0 : it->second;
}

solve::Answer ScriptedOracle::ask(solve::Point p, solve::Phase phase) {
  ++calls_[p];
  asked.emplace_back(p, phase);
  solve::Answer a{};
  a.point = p;
  a.phase = phase;
  a.source = solve::Source::z3;
  const auto it = overrides_.find(p);
  a.outcome = it != overrides_.end() ? it->second : truth(p) ? solve::Outcome::sat : solve::Outcome::unsat;
  if (a.outcome == solve::Outcome::timeout) a.reason = "timeout";
  if (a.outcome == solve::Outcome::undefined) a.reason = "unknown";
  if (a.outcome == solve::Outcome::sat) {
    a.model = model ? model(p).value_or(p) : p;
    solve::Witness w;
    w.counts = *a.model;
    witnesses.push_back(w);
    a.witness = static_cast<std::uint32_t>(witnesses.size() - 1);
  }
  return a;
}

}  // namespace fusion::testkit
