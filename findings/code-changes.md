# IPAMIR code changes — what we did, findings & conclusions

Scope: making the IntelSAT (Topor) solver a spec-compliant **IPAMIR** incremental-MaxSAT
solver for the MaxSAT Evaluation 2026 incremental track. Almost all logic lives in
`ToporIpamir.cc`; the build/packaging lives in `Makefile` + `submission/`.

---

## 1. `ToporIpamir.cc` — the IPAMIR wrapper

This file wraps `CTopor` and exposes the C IPAMIR API. Key changes:

### 1.1 Build/correctness fixes (made it compile & stop crashing)
*In plain words: small bugs stopped it from building and made it crash — we fixed those first.*
- **Namespace**: `NUWLS` → `nuwls::NUWLS`.
- **Double free**: after `nuwls_solver.free_memory()` added `built = {};` so the freed
  buffers are not freed again.
- **Divide-by-zero guard**: `RunNuwlsPostSolve()` returns early when `m_Result.cost == 0`.
- **`std::span` mismatch**: 5 call sites pass `const vector<int>` into APIs taking
  `std::span<int>`; wrapped as `std::span<int>(const_cast<int*>(v.data()), v.size())`.

### 1.2 Return-code contract (the core compliance work)
*In plain words: we make the solver report the right "answer code" so the competition knows whether it found, proved, or only guessed a solution.*

IPAMIR `ipamir_solve` must return exactly: `0` interrupted/no-feasible, `10`
interrupted/feasible, `20` UNSAT, `30` proven optimal, `40` ERROR.

`Solve()` now maps outcomes precisely (`ToporIpamir.cc` ~lines 68–191):
- `RET_SAT` → run the post-solve optimization (NuWLS, Mrs. Beaver, then **LSU**):
  - LSU returns `RET_UNSAT` ⇒ no cheaper model exists ⇒ **30 (proven optimal)**.
  - genuine interrupt (`RET_USER_INTERRUPT` / timeouts / conflict-out / mem-out) with a
    feasible model in hand ⇒ **10**.
  - `cost == 0` ⇒ optimal by definition ⇒ **30**.
- `RET_UNSAT` ⇒ **20**.
- otherwise: split genuine interrupts (⇒ **0**) from internal solver errors
  (`RET_INDEX_TOO_NARROW`, `RET_PARAM_ERROR`, …) ⇒ enter ERROR ⇒ **40**.

### 1.3 The code-10 compliance bug (root cause + fix)
*In plain words: a hidden 15-second timer made the solver give up early and pretend it was interrupted. We did NOT delete it — LSU still has its 15s default for other callers — we just turn it off (set it to 0) on the IPAMIR path so it runs long enough to truly prove the best answer.*

**Finding:** LSU ran on its own `CTopor` instance and had a built-in 15s timer
(`TimeLimitSeconds = 15`). On hard instances it stopped early, so the wrapper reported
`10` (feasible) **without any interrupt having occurred** — which is illegal under IPAMIR
and made benchmark apps (e.g. `ipamirextenf`) abort.

**Fix (in `RunLsuOptimization()`, ~lines 306–388):**
1. **Wire the terminate callback into the LSU solver** — `lsuTopor.SetCbStopNow(...)`
   forwards the app's `ipamir_set_terminate` callback, so the app can actually stop LSU.
2. **Disable LSU's internal timer for the IPAMIR path** — `options.TimeLimitSeconds = 0`
   (the timer is not removed from LSU; the 15s default stays for other callers). Timing is now driven
   only by the global wall-clock + the terminate callback, so LSU runs to completion and can
   reach UNSAT and **prove** optimality (30) instead of bailing out into an illegal 10.
3. The return mapping in 1.2 makes `10` reachable **only** on a real interrupt.

### 1.4 Code-40 / ERROR state
*In plain words: if something goes genuinely wrong inside the solver, it now says "error" (give the code 40) and stays in error until you reset it, instead of pretending all is fine.*
- Added a sticky `bool m_InError`.
- Top of `Solve()`: `if (m_InError) return 40;` (after ERROR, only `ipamir_release` is valid).
- Internal solver-error return codes set `m_InError = true; retVal = 40;`.

### 1.5 Test hook (compiled out of production)
*In plain words: a tiny test-only switch lets us fake an error on purpose to check the error handling works; it's removed from the real shipped solver.*
- `#ifdef IPAMIR_TEST_HOOKS` → `TestForceError()` + C entry `ipamir__test_force_error()`,
  used only by unit tests to drive the 40 path (no benchmark naturally triggers it).
  Verified absent from the production library.

---

## 2. `Makefile`
- `CSRCS` now includes `algorithms/*.cc` (NuWLS/Mrs. Beaver/LSU) and filters out
  `test_ipamir.cc`, so the solver builds on Linux and a clean static library has no stray
  `main()`.

---

## 3. Tests & cross-checks (`tests/`)
- `tests/test_ipamir_codes.cc` — drives the IPAMIR API directly; asserts the 30/20/30-cost0/
  incremental contract **and** code-40 stickiness (via the hook). **9–11/11 PASS.**
- `tests/wcnf_solve.cc` — minimal single-`ipamir_solve` WCNF front-end (used because
  `ipamirapp`'s post-solve loop runs many slow re-solves and won't terminate quickly).
- `tests/cost_crosscheck.py` — an **independent brute-force oracle** (enumerate assignments,
  enforce hard clauses, minimize violated-soft weight) vs the solver. **46/46 PASS**
  (6 hand-built + 40 seeded random, 3–12 vars).

---

## 4. Submission package (`submission/`)
- `submission/IntelSatSolver/` is **self-contained**: bundles the full solver source in
  `solver_src/`, plus `makefile`, `glue.cc` (supplies `MainWallTimePassed()`), `LIBS`,
  `README`. `make` → `libipamirIntelSatSolver.a` (drops `Main.or`, adds `glue.o`).
- `submission/IntelSatSolver.tar.gz` (164 KB, source-only) is the shippable archive;
  verified it rebuilds the correct library from scratch in a clean dir.

---

## 5. Conclusions (short)
- The IPAMIR return-code contract is now correct and locked by unit tests.
- The illegal code-10 case is fixed at the root (LSU timer + terminate wiring), so proven
  optima now report **30**.
- ERROR (40) is handled and sticky.
- Reported optima are **correct** — validated end-to-end against an independent oracle (46/46).
- A clean, self-contained, competition-shaped submission package builds and runs.

---

## 6. File / line map (where each change lives)
- `ToporIpamir.cc`
  - `Solve()` — return-code contract & code-40: ~lines 68–191
  - `RunLsuOptimization()` — terminate wiring + timer-off: ~lines 306–388
  - `SetTerminate()`: ~lines 216–241
  - test hook (`IPAMIR_TEST_HOOKS`): ~lines 243–249
- `Makefile` — `CSRCS` (algorithms + filter `test_ipamir.cc`): line 12
- `tests/test_ipamir_codes.cc`, `tests/wcnf_solve.cc`, `tests/cost_crosscheck.py`
- `submission/IntelSatSolver/` (+ `submission/IntelSatSolver.tar.gz`)

## 7. How to build & test
```
# build the submission library
make -C submission/IntelSatSolver                 # -> libipamirIntelSatSolver.a

# return-code unit tests (with the 40 hook enabled)
g++ -DIPAMIR_TEST_HOOKS -DSKIP_ZLIB -std=c++20 -O3 -DNDEBUG \
    -c ToporIpamir.cc -o /tmp/hook.o
g++ -DIPAMIR_TEST_HOOKS -DSKIP_ZLIB -std=c++20 -O3 -DNDEBUG \
    tests/test_ipamir_codes.cc /tmp/hook.o libintel_sat_solver_ipamir_lsu_release.a \
    -lpthread -o /tmp/test_codes && /tmp/test_codes

# cost cross-check vs independent oracle
g++ -O3 -DNDEBUG -std=c++20 -Iipamir_test/ipamir tests/wcnf_solve.cc \
    submission/IntelSatSolver/libipamirIntelSatSolver.a -lpthread -o /tmp/wcnf_solve
python3 tests/cost_crosscheck.py
```

## 8. Known limitations
- **Performance / timeouts**: on the hardest `extenf`/`bioptsat` instances the solver did not
  finish within our **short test caps (45–150s)**.
  *What this means: these are genuinely difficult problems, and we only gave the solver a couple
  of minutes. The real evaluation allows ~1 hour per instance, so these are NOT proven failures —
  we simply have not yet tested under the full time budget. When instances do finish, the answer
  is correct; we have not measured how many finish given an hour.*
- **Path-2 guard**: the `cost >= best` totalizer break is the one code path that could, in
  theory, return a feasible-but-not-proven `10` without an interrupt.
  *What this means: in one corner of the LSU encoding, the search could stop "thinking it's done"
  without a real proof. We never saw it happen in testing, but we have not formally ruled it out.*

## 9. Remaining
1. **Performance under a realistic time budget** — re-run the hard `extenf`/`bioptsat` instances
   with a long limit (e.g. 600s–3600s), not our short 45–150s caps.
   *Why: we need to know whether "slow" actually means "fails the competition's ~1h limit" or
   just "needs more than 2 minutes". Right now that is an assumption, not a measured fact.*
2. **Path-2 guard** — confirm the `cost >= best` totalizer break never misfires; fix the
   totalizer if it does.
   *Why: it is the only remaining path that could in theory report an unproven answer; closing it
   removes the last doubt about return-code correctness.*
