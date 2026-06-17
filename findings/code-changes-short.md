# IPAMIR code changes — short version

Goal: make IntelSAT (Topor) a spec-compliant IPAMIR incremental-MaxSAT solver. Nearly all
logic is in `ToporIpamir.cc`.

## What we changed
- **Compile/crash fixes** (`ToporIpamir.cc`): `nuwls::NUWLS` namespace; double-free fix
  (`built = {};`); divide-by-zero guard (cost 0); `std::span` casts at 5 sites.
- **Return codes** (`Solve()`): correct IPAMIR mapping — `30` proven optimal (LSU reaches
  UNSAT), `20` UNSAT, `10` only on a real interrupt with a feasible model, `0` interrupt /
  no model, `40` internal error.
- **code-10 bug fix** (`RunLsuOptimization()`): LSU's built-in 15s timer made it stop early
  and illegally report `10`. Fix = wire the app terminate callback into LSU's solver +
  override `TimeLimitSeconds = 0` **on the IPAMIR path only** (the 15s default stays in LSU for
  other callers), so LSU runs to completion and **proves** optimality (30).
- **code-40 ERROR**: sticky `m_InError`; `Solve()` returns `40` and only `ipamir_release`
  is valid afterwards.
- **Test hook**: `IPAMIR_TEST_HOOKS` force-error path; compiled out of production.
- **`Makefile`**: build `algorithms/*.cc`, exclude `test_ipamir.cc`.

## Verification
- `tests/test_ipamir_codes.cc`: return-code + 40-stickiness — **PASS (11/11)**.
- `tests/cost_crosscheck.py` + `tests/wcnf_solve.cc`: optima vs independent brute-force
  oracle — **PASS (46/46)**.

## Packaging
- `submission/IntelSatSolver/` self-contained (`make` → `libipamirIntelSatSolver.a`,
  bundled source + glue), `submission/IntelSatSolver.tar.gz` — rebuilds cleanly.

## Where it lives (file/line map)
- `ToporIpamir.cc`: `Solve()` (codes + 40) ~68–191; `RunLsuOptimization()` (terminate +
  timer-off) ~306–388; `SetTerminate()` ~216–241; test hook ~243–249.
- `Makefile` line 12 (`CSRCS`); `tests/` (3 files); `submission/`.

## Build & test
```
make -C submission/IntelSatSolver                 # -> libipamirIntelSatSolver.a
# return-code tests (40 hook on): compile ToporIpamir.cc + test_ipamir_codes.cc with
#   -DIPAMIR_TEST_HOOKS, link libintel_sat_solver_ipamir_lsu_release.a  -> run
# cost cross-check: build tests/wcnf_solve.cc vs lib, then python3 tests/cost_crosscheck.py
```
(Full commands in `code-changes.md` §7.)

## Conclusion
Return codes correct & tested; code-10 fixed at the root; code-40 handled; reported optima
verified correct; submission package builds & runs.

## Known limitations
- **Timeouts on hard instances**: hardest `extenf`/`bioptsat` didn't finish in our short test
  caps (45–150s). These are genuinely tough problems and we only gave ~2 min; the real
  evaluation allows ~1 hour, so this is NOT a proven failure — just untested at full time.
  Answers are correct when they do finish.
- **Path-2 `cost >= best` guard**: one LSU code path could in theory stop without a real proof
  and report an unproven `10`. Never observed in testing, but not formally ruled out.

## Remaining
1. **Test under a realistic time budget** — re-run hard `extenf`/`bioptsat` with a long limit
   (600s–3600s), not our short 45–150s caps, to learn if "slow" really means "fails the ~1h
   competition limit" or just "needs more than 2 min". Currently an assumption, not measured.
2. **Confirm/fix the Path-2 guard** — verify the `cost >= best` break never misfires (the only
   path that could report an unproven answer); fix the totalizer if it does.
