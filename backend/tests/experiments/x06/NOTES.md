# X06 · What Z3's `:solver2_timeout` and `:timeout` do

Phase 3 plan, experiment X06; spec stage 5, open question 1; section 06, 06.C2 and open question 8. This note reads the source of Z3 4.8.12, the version FuSION 1 links (`libz3-4 4.8.12-3.1build1`), at the tag `z3-4.8.12`. Z3 paths below are under `src/` at that tag; FuSION 1's lines are the snapshot's. `run_x06.py` checks the reading on the full run's dump, and writes its results to the data root's `reports/x06/`.

## The solver FuSION 1 builds

- `z3::solver s(c, "QF_BV")` (`Linux/src/main.cc:4722`) calls `Z3_mk_solver_for_logic` (`/usr/include/z3++.h:2564`), which builds a combined solver (`api/api_solver.cpp:203`; `tactic/portfolio/smt_strategic_solver.cpp:162-166`):
  - solver 1, non-incremental: the QF_BV tactic run as a solver (`smt_strategic_solver.cpp:55-56`, `164`);
  - solver 2, incremental: for QF_BV the incremental SAT solver, not the SMT kernel, because `hi_div0` is true by default (`smt_strategic_solver.cpp:120-121`; `params/bv_rewriter_params.pyg:8`).
- FuSION 1 sets `:solver2_timeout` to the batch's first timeout and `:timeout` to its second, 1 h and 4 h in every b10 run (`main.cc:4730-4731`, `4736`). Z3 drops the leading colon (`util/params.cpp:30-31`).
- `solver2_timeout` belongs to the combined solver (`solver/combined_solver.cpp:88-93`). Its default is no limit (`solver/combined_solver_params.pyg:4`).
- `timeout` does not. The API reads it from the solver's parameters and puts one timer around the whole check (`api/api_solver.cpp:573-575`, `584`). The CLI's `-t:` does the same in `cmd_context/cmd_context.cpp:1600` and `1642-1649`.

## How the two interact (`solver/combined_solver.cpp`)

1. The combined solver starts non-incremental (`122`). A push, or an assertion after a check, makes it incremental (`41-44`, `161-166`, `175-180`). FuSION 1 pushes before every check, so every check is incremental. A dump run on its own has no push, goes straight to solver 1 (`250-252`), and ignores `solver2_timeout`.
2. In incremental mode with `solver2_timeout` set, solver 2 runs under its own timer of that length (`230-236`). When that timer fires, it cancels solver 2 (`82-85`). The combined solver then always prints `solver 2 failed, trying solver1` and starts solver 1 from scratch (`243-252`), with none of solver 2's learned state.
3. The overall timer runs from the start of the check, so solver 1 gets only what is left: 3 h of FuSION 1's 4 h.
4. If solver 2 returns unknown before its own timer fires, the combined solver falls back only when `use_solver1_when_undef()` holds (`243`), which by default means a quantifier-free problem (`106-115`; `combined_solver_params.pyg:6`). FuSION 1's problems are quantifier-free. So when the overall timer fires first, while solver 2 runs, line `243` still sends the check to solver 1, which then stops on the same cancel. The branch without a `solver2_timeout` tests for the cancel (`226`) and returns without falling back.
5. After a timeout the answer is unknown, and the API sets the reason `timeout` on both solvers (`api/api_solver.cpp:605-607`; `solver/check_sat_result.cpp:27-28`; `combined_solver.cpp:335-338`).
6. `reason_unknown()` asks the solver that ran last (`328-333`), and `pop` does not change which one that was (`182-187`).
7. The combined solver writes all these messages at verbosity 15 (`27`). `-v:10` shows none of them, so `run_x06.py` uses `-v:15`.

## Why FuSION 1's 4 h checks said `unknown`

A 4 h check falls back to solver 1 after 1 h, and the 4 h timer then stops solver 1. FuSION 1 reads the reason after `pop`: in the smallest-e step that wrote X06's dump, `check` at `Linux/src/GraphManagerNew.cc:29725`, `pop` at `29729` and the reason at `29741` (in the diagonal step, `29461`, `29465` and `29477`). The `pop` reaches solver 1's `pop_core` (`solver/solver_na2as.cpp:92-96`), which drops the result that held `timeout` (`solver/tactic2solver.cpp:149-157`). Without a result, solver 1 reports `unknown` (`tactic2solver.cpp:279-284`). Read before `pop`, the reason is `timeout`.

This confirms section 06's answer (06.C2, open question 8). A check that ended inside solver 2 would keep its reason across `pop`: the incremental SAT solver keeps its reason string (`sat/sat_solver/inc_sat_solver.cpp:274-297`, `537-539`). FuSION 1's comment at `GraphManagerNew.cc:29339-29340` has the hand-off backwards: after `:solver2_timeout`, the check goes from the incremental solver to solver 1.

## What run_x06.py checks

The CLI cannot show the reason after `pop`, because its `pop` forgets the last check (`cmd_context.cpp:1564-1565`). A later `get-info :reason-unknown` prints `state of the most recent check-sat command is not known` (`850-853`). The runs check the rest on the full run's without-FSC check at (n 7, e 0), which FuSION 1 left unknown after 14,396.2 s. Expected from the source:

| Run | Input | `solver2_timeout` | `timeout` | Expected |
|---|---|---|---|---|
| a | pushed bounds | 10 s | 40 s | `using solver 2 (with timeout)`; about 10 s later `solver 2 failed, trying solver1` and `using solver 1`; unknown about 40 s after the check starts; reason `timeout` before `pop` |
| b | pushed bounds | 40 s | 10 s | `using solver 2 (with timeout)`; about 10 s later `solver 2 failed, trying solver1` and `using solver 1` all the same (point 4), then unknown soon after; reason `timeout` |
| c | pushed bounds | none | 20 s | `using solver 2 (without a timeout)`, no fallback (`226`); unknown after about 20 s; reason `timeout` |
| d | the dump as written | 10 s | 20 s | `using solver 1` only (point 1); unknown after about 20 s; reason `timeout` |

The first `combined-solver` line marks the start of the check. In runs a to c the SAT solver bit-blasts the formula at the first push (`inc_sat_solver.cpp:250-259`), before the check and outside both timers.
