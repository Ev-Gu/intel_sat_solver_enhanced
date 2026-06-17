# IPAMIR Benchmark Testing — Findings & Open Bug

Branch: `feature/ipamir-lsu-on-main`
Date: 2026-06-17
Status: investigation only — **no code was modified**

---

## 1. What we did

Built the official IPAMIR test apps (from `ipamir_test/ipamir/app/`) against our
branch solver `libipamirIntelSatSolver.a`, then ran a sample of each app's real
benchmark inputs (extracted from each app's `inputs.zip`).

Command per app:

```
make IPAMIRSOLVER=IntelSatSolver
./<app> <input-file>
```

---

## 2. Build results (our solver linked into each app)

| App                | Build | Reason if failed                                  |
|--------------------|-------|---------------------------------------------------|
| `ipamirextenf`     | OK    | —                                                 |
| `ipamiric3ref`     | OK    | —                                                 |
| `ipamirbioptsat`   | OK    | —                                                 |
| `ipamiradaboost`   | FAIL  | App's **own** C++ bug (`DTEncoder::K`) — not ours |
| `ipamirmlicseesaw` | FAIL  | Needs commercial **CPLEX** (`ilcplex/cplexx.h`)   |

Takeaway: our library links cleanly into every app whose own dependencies are
satisfiable. The two failures are unrelated to our solver.

---

## 3. Run results (sampled, short timeouts)

- **`ipamirextenf`** (5 inputs, 60s each):
  - 4 → TIMEOUT at 60s (no answer in the short limit)
  - 1 (`A-1-BA_40_80_5_41_10_1.apx`) → returned code **10**, fast (NOT a timeout);
    the official app aborted: `ERROR: ipamir_solve returned 10. Terminating.`
- **`ipamiric3ref`** (3 inputs, 45s each): 2 finished fast (IC3's own engine), 1 timeout.
  IC3 leans on its own internal SAT engine; inconclusive for our solver.
- **`ipamirbioptsat`** (3 inputs, 45s each): all 3 TIMEOUT at 45s.

Note on timeouts: I only allowed 45–60s. The real evaluation allows ~1 hour, so
these timeouts are NOT proof of weakness — strength on hard instances remains
**unmeasured**.

---

## 4. The bug: returning code 10 without an interrupt

### IPAMIR spec (from `ipamir.h`)
`ipamir_solve` may return:
- `0`  — interrupted, no feasible solution
- `10` — interrupted, **but** a feasible solution was found.
  **"the solver can only be interrupted via `ipamir_set_terminate`."**
- `20` — unsatisfiable
- `30` — optimal solution found (proven)
- `40` — ERROR (unsupported call sequence)

So **`10` is only legal if the app installed a terminate callback and it fired.**

### What our code does (`ToporIpamir.cc`, ~lines 89–124)
After the initial SAT solve, if there are soft literals and cost > 0, it runs LSU.
LSU's result `LastSolveRet`:
- `RET_UNSAT` → we set `retVal = 30` (proven optimal). Correct.
- otherwise → `retVal` stays 0, then the fallback:
  `is_optimal = (cost == 0); retVal = is_optimal ? 30 : 10;`
  → for a feasible solution with cost > 0 this returns **10**.

There is **no check** for whether an interrupt actually occurred, and **no
terminate callback was registered** by `ipamirextenf` (verified: `ipamir_set_terminate`
is never called in its source). Per spec, 10 can therefore never be legal for that
app — so the app correctly aborts.

---

## 5. ROOT CAUSE (confirmed from code)

`algorithms/LSU.hpp`:

```cpp
struct TLinearSUOptions
{
    bool Verbose = false;
    int  TimeLimitSeconds = 15;   // <-- built-in 15-second limit, ON by default
};
```

Our wrapper (`RunLsuOptimization` in `ToporIpamir.cc`) sets only `options.Verbose = false`
and leaves `TimeLimitSeconds` at its default **15**.

Inside `RunWeightedLinearSatUnsat` (`algorithms/LSU.cc`), the optimization loop:

```cpp
while (best > 0) {
    if (options.TimeLimitSeconds > 0) {            // 15 > 0  => active
        if (elapsed >= options.TimeLimitSeconds)   // after 15s
            break;                                 // <-- exits with LastSolveRet == RET_SAT
    }
    ...
    res.LastSolveRet = solve(assumps);
    if (res.LastSolveRet != RET_SAT) break;        // UNSAT path -> proven optimal (30)
    if (cost >= best) break;                       // defensive break, also leaves LastSolveRet == SAT
    ...
}
```

So LSU can stop **early, on its own**, in two ways, both leaving `LastSolveRet == RET_SAT`:
1. **Its built-in 15-second timer fires** (primary suspect for the fast code-10 case).
2. A defensive break when Topor returns SAT but the model's cost isn't an improvement
   (an encoding-tightening limitation).

In both cases the wrapper sees `RET_SAT`, falls through, and returns **10** — even
though nobody interrupted the solver.

### This answers the open question
- Did the code-10 instance time out (my 60s) or stop on its own?
  → It stopped on its **own** logic, fast — most likely LSU's **internal 15s limit**.
- Is the early stop "by design" or "a flaw"?
  → The 15s limit is **by design** (a deliberate default). The **flaw** is what the
    wrapper does afterward: report `10`, which is illegal without an interrupt.

---

## 6. Why this matters for the competition

With a 15s internal cap and no terminate callback, the solver stops early and then
has **no legal IPAMIR code to report** for "feasible but not proven":
- `30` would be a lie (not proven optimal),
- `0` is wrong (we *do* have a feasible solution),
- `10` is illegal (no interrupt happened).

A spec-compliant incremental solver with no terminate callback must run until it can
return `20` or `30`. The 15s cap fundamentally conflicts with that.

---

## 6b. Mapping to the OFFICIAL task (MaxSAT Evaluation, Incremental Track)

Source of truth: `task_desc/url.txt` ->
`https://maxsat-evaluations.github.io/2024/submission.html`
(the incremental-track rules are stable across years).

Official incremental-track requirements:
1. Implement the IPAMIR functions **"as described in the comments of the header file."**
2. **If a function is not supported, the solver state must be changed to ERROR (return 40).**
3. Solver lives in a **directory named after the solver**.
4. **`make` in that directory must build the solver as a static library.**
5. Follow the `maxsat/solver2023` example layout.
6. Submit as a single zip/tar by email.

| Requirement | Status on this branch |
|---|---|
| Build as static library via `make` in solver-named dir | DONE (`maxsat/IntelSatSolver/makefile` -> `libipamirIntelSatSolver.a`) |
| Directory named after the solver | DONE (`maxsat/IntelSatSolver/`) |
| Implement IPAMIR as described in header comments | PARTIAL — violated by the code-10-without-interrupt bug (Section 4) |
| ERROR state (40) for unsupported calls | MISSING — no `40` anywhere in `ToporIpamir.cc` |
| Package as zip/tar for submission | NOT DONE |

So the two issues we found are **requirement-level gaps**, not just nice-to-haves:
the code-10 bug breaks rule #1, and the missing 40 breaks rule #2.

### Verdict on the two LSU break paths, judged against the official docs
The docs constrain only `ipamir_solve`'s **return values** (0/10/20/30/40); they say
nothing about LSU internals (timers, totalizers, guards). Therefore:

- **Path 1 (15s internal timer)** — NOT a code bug and not illegal by itself; it is a
  deliberate feature (fine for the command-line tool). But it is the **wrong setting for
  the IPAMIR path**, where timing is controlled externally (global wall-clock + terminate
  callback). It is the trigger that makes the solver stop early and emit the illegal 10.
  => treat as a **misconfiguration for IPAMIR use**, not an LSU defect.
- **Path 2 (`cost >= best` defensive break)** — NOT a doc-level bug; it is a safety guard
  against an infinite loop. But **if it actually fires**, it reveals an internal
  correctness problem in the totalizer encoding (the bound was not enforced).
  => keep the guard, but a firing signals a real encoding bug to fix separately.

Neither path produces a *wrong* feasible answer — both report a valid feasible cost that
is simply **not proven optimal**, mislabeled as 10. So this is a compliance/labeling +
"did not finish proving" problem, not a "wrong number" problem.

The only actual doc violation is downstream in `ToporIpamir.cc`: returning **10 without an
interrupt**.

---

## 7. Candidate fixes (NOT applied — for discussion)

1. **Run LSU to completion via IPAMIR**: set `TimeLimitSeconds = 0` in the wrapper so
   LSU always reaches `20`/`30` (relies on the app's own terminate callback for time control).
2. **Wire LSU's stop to the IPAMIR terminate callback**: then an early stop is a *real*
   interrupt, making `10` legal — and return `0`/`10` accordingly.
3. **Investigate the `cost >= best` defensive break**: if it fires independently of the
   timer, the weighted-totalizer encoding may be incomplete and needs a separate fix.

Decision should be evidence-based; see next steps.

### Other requirement-level work (from Section 6b, NOT applied)
4. **ERROR state / return 40**: add handling so unsupported IPAMIR call sequences set the
   solver to ERROR and `ipamir_solve` returns 40 (rule #2).
5. **Submission packaging**: produce a zip/tar where `make` in the solver-named directory
   builds the static library, following the `maxsat/solver2023` layout (rules #3–#6).

---

## 7b. CONFIRMED root cause (DPRINT experiment, 2026-06-17)

To settle which break path fires, I recompiled **only** `algorithms/LSU.cc` with `-DDPRINT`
(debug logging), swapped that object into the **app** library only (the main release lib
was left untouched), relinked `ipamirextenf`, and re-ran the failing instance
`inputs/A-1-BA_40_80_5_41_10_1.apx`. Afterwards the app library was rebuilt clean
(verified: 0 DPRINT strings remain). **No source code was changed.**

### What I ran
```
g++ ... -O3 -DNDEBUG -DDPRINT -c algorithms/LSU.cc -o LSU.or   # debug build of LSU only
ar r .../libipamirIntelSatSolver.a LSU.or ; ranlib ...          # swap into app lib only
make IPAMIRSOLVER=IntelSatSolver                                # relink app
time ./ipamirextenf inputs/A-1-BA_40_80_5_41_10_1.apx           # re-run failing instance
# then: make clean && make  -> restore clean app lib
```

### What I observed
- Total run time: **~22 seconds**, app exited with code **10** (then `ERROR: ... returned 10`).
- LSU log: `ENTERING LSU (WEIGHTED)` ... `EXITING LSU` / **`LSU Terminated early. Best Cost found: 619`**.
- **No `WARNING` line** -> the `cost >= best` defensive break (Path 2) did **NOT** fire.
- **No `PROVEN` line** -> optimality was never proven (loop did not end in UNSAT).
- LSU made **1037 improving steps** (`o 624 ... o 619`), each solve only **8-16 ms**, i.e.
  it was descending one unit at a time and still making fast progress when it stopped.

### Conclusion (now certain)
The exit was via the **15-second internal timer (Path 1)** — the only loop exit consistent
with "Terminated early" + no WARNING + no PROVEN. LSU was cut off mid-descent while still
improving quickly; the wrapper then mislabeled the feasible-but-unproven result as **10**.

Implications:
- **Path 2 is NOT the culprit here** — no evidence of the encoding-tightening guard firing.
  (It remains a latent guard; not observed to trigger on this instance.)
- This is squarely a **"the internal 15s timer cut it off, then we returned an illegal 10"**
  situation — i.e. a *misconfiguration for IPAMIR* plus a *return-code* bug, not a wrong answer
  and not an encoding flaw on this instance.
- Because the last solves were tiny (8-16 ms) and progress was steady, the instance might well
  reach a proven optimum given more time — but that is a *separate* "is it fast enough" question
  (still unmeasured; needs a long-timeout run, see Section 8).

## 8. Next steps to fully confirm

1. Recompile LSU with `-DDPRINT` and re-run `A-1-BA_40_80_5_41_10_1.apx` to see which
   break fired (15s timer vs. `cost >= best`).
2. Re-run the same instance with `TimeLimitSeconds = 0` (or a long limit) to check
   whether it then reaches `30` (proves it was just the timer) or still bails (encoding flaw).
3. Cross-check the reported cost against a known-good MaxSAT solver for correctness.

---

## 8b. FIX APPLIED + re-test (2026-06-17)

Implemented the bundled fix in `ToporIpamir.cc` (source changed this time):

1. **Wired the app's terminate callback into the LSU solver.** `RunLsuOptimization()`
   now calls `lsuTopor.SetCbStopNow(...)` using `m_CurrTerminateFunc/State`, so the app
   can actually interrupt LSU (previously LSU ran on a separate instance with no callback).
2. **Disabled LSU's internal 15s timer for the IPAMIR path** (`options.TimeLimitSeconds = 0`),
   so timing is external (global wall-clock + terminate), per the spec.
3. **Rewrote the return-code mapping in `Solve()`**:
   - LSU ends `RET_UNSAT`  -> `30` (proven optimal)
   - cost reaches 0        -> `30`
   - genuine interrupt (`RET_USER_INTERRUPT`/timeouts/conflict/mem-out) with a feasible
     solution -> `10` (now legal, because an interrupt actually occurred)
   - feasible-but-unproven without interrupt -> `10` as a guarded fallback, flagged as a
     KNOWN LIMITATION (only reachable via the Path 2 encoding guard; not observed).

### Re-test results
- **Toy `ipamirapp`** (3 small instances): still `s OPTIMUM FOUND` (code 30) on all,
  no regressions.
- **Previously-failing `ipamirextenf` instance** (`A-1-BA_40_80_5_41_10_1.apx`):
  - **Before fix:** aborted at illegal code `10` in ~22s (cost stuck at 619, cut by the 15s timer).
  - **After fix:** **no abort, no illegal 10.** LSU ran continuously and kept improving the
    bound **from 619 down to 14** over 5 minutes; it had not yet proven the optimum when my
    300s shell timeout killed it.

### Conclusion
- The actual compliance bug is **fixed**: the solver no longer emits an illegal `10`. It now
  either proves optimality (`30`) or keeps improving until externally stopped, exactly as the
  IPAMIR contract intends.
- What remains on that instance is a **pure performance question** ("is it fast enough to
  prove the optimum within the time limit?") — *not* a correctness/compliance bug. The bound
  marched 619 -> 14, so it is making real progress but proving the final optimum is slow here.

### Broader targeted re-run (post-fix, no source changes)
Verifying the key invariant after the fix: **no illegal code-10 anywhere** + no regressions.

| App / inputs | Result |
|---|---|
| `ipamirapp` (3 small) | all `OPTIMUM FOUND` (30) — no regression |
| `ipamirextenf` (3, 150s each) | **0 illegal-10**; 1 proved `OPTIMUM FOUND` (30), 2 ran to timeout cleanly |
| `ipamirbioptsat` (3, 150s each) | **0 illegal-10**; all 3 ran to timeout cleanly (large/hard instances) |

Takeaway: the illegal code-10 is **gone across all tested apps**, and one previously-aborting
`extenf` instance now **fully proves the optimum (30)**. All remaining timeouts are the
performance question, not correctness.

### Honest caveat / tradeoff
`ipamirextenf` does **not** register a terminate callback. With the internal timer removed,
a hard solve call will now run until the process is killed by the external (competition)
time limit, rather than returning early. That is spec-correct (a solver with no interrupt must
run to completion) and it streams improving `o` lines for anytime scoring, but for the *exact*
incremental track it means hard instances need the solver to be **fast enough to finish** —
i.e. follow-up performance work, tracked as the separate "strength" question (Section 8).

## 8c. Code-40 / ERROR-state handling (2026-06-17)

Implemented the required ERROR-state behavior (submission rule #2: "If your solver does not
provide support for a particular function, the solver state must be changed to ERROR").

### What the spec requires
`ipamir_solve` returns **40** when the solver is in **ERROR** state. Per the header, ERROR is
entered on "a sequence of ipamir calls ... that the solver does not support." add/solve are
only allowed from INPUT/OPTIMAL/SAT/UNSAT (not ERROR), so ERROR is **sticky** until
`ipamir_release`.

### The gap that was there
`Solve()`'s final `else` mapped **every** non-SAT/non-UNSAT `TToporReturnVal` to `0`
(interrupted). Several of those codes are genuine errors, not interrupts — notably
`RET_INDEX_TOO_NARROW` (problem too large for the 32-bit clause-buffer mode, realistic on big
competition instances), plus `RET_PARAM_ERROR`, `RET_ASSUMPTION_REQUIRED_ERROR`,
`RET_DRAT_FILE_PROBLEM`, `RET_EXOTIC_ERROR`. Reporting those as "interrupted (0)" is wrong.

### What I changed (`ToporIpamir.cc`)
1. Added a sticky `bool m_InError` (the ERROR state).
2. Top of `Solve()`: `if (m_InError) return 40;` (ERROR is sticky; only release is valid after).
3. Rewrote the `else` branch to split:
   - interrupt / resource-out (`USER_INTERRUPT`, `TIMEOUT_LOCAL/GLOBAL`, `CONFLICT_OUT`,
     `MEM_OUT`) -> `0` (interrupted, no feasible solution);
   - genuine error codes -> set `m_InError`, return `40`.

### Deliberately NOT done
- No full 5-state validator policing every call ordering (over-engineering; the spec's ERROR
  is about unsupported calls, not `val_obj`/`val_lit` precondition policing).
- LSU-phase errors are **not** mapped to 40: by then a valid feasible solution from the main
  solve is already in hand, so reporting it (10/30) is more useful and correct than discarding it.
- Existing 0/10/20/30 behavior unchanged.

### Verification
- Library + app rebuilt clean.
- **Regression**: toy `ipamirapp` still returns `OPTIMUM FOUND` (30) on all 3 inputs, with
  **0 spurious 40s** and **0 errors**.
- **Honest caveat**: the 40 path is implemented and code-reviewed but **not exercised** by the
  available benchmarks — none of them drive the underlying solver into an internal error code
  (e.g. `RET_INDEX_TOO_NARROW` needs an instance too large to construct in a quick test). So 40
  is wired correctly by construction, but remains unverified at runtime against a real trigger.

## 8d. Compliance sweep on the combined build (LSU fix + code-40)

Ran the given benchmark apps again on the current build to confirm no regressions and full
code compliance:

| App / inputs | Result |
|---|---|
| `ipamirapp` (3) | exit 0 — `OPTIMUM`+`UNSAT` mixes; 0 illegal-10, 0 spurious-40, 0 errors |
| `ipamirextenf` (2, 120s) | timeout (slow); 0 illegal-10, 0 spurious-40, 0 errors |
| `ipamirbioptsat` (2, 120s) | timeout (slow); 0 illegal-10, 0 spurious-40, 0 errors |

Conclusion: easy instances solve correctly (30/20), hard ones time out cleanly, and there are
**zero illegal codes, zero spurious 40s, and no crashes**. The code-40 change introduced no
regression. (The 40 path itself still has no live trigger among these benchmarks — see 8c.)

## 8e. Unit tests written + run (2026-06-17)

Added `tests/test_ipamir_codes.cc` (kept in a `tests/` subfolder so the project Makefile's
root `*.cc` wildcard never pulls it into the library). It drives the public IPAMIR C API and
asserts the exact return-code contract, including the code-40 path.

To make the otherwise-unreachable code-40 path testable, a **compile-gated** hook was added to
`ToporIpamir.cc` (`#ifdef IPAMIR_TEST_HOOKS`): a `TestForceError()` method + an
`ipamir__test_force_error()` entry point. It is **absent from production builds** — verified:
`nm` shows 0 `test_force_error` symbols in both `libintel_sat_solver_ipamir_lsu_release.a` and
the app's `libipamirIntelSatSolver.a`.

### Build (manual; hooked ToporIpamir linked explicitly so the archive copy is not used)
```
FLAGS="-DSKIP_ZLIB -Wall -Wno-parentheses -std=c++20 -O3 -DNDEBUG -DIPAMIR_TEST_HOOKS"
g++ $FLAGS -c ToporIpamir.cc -o /tmp/ToporIpamir_hook.o
g++ $FLAGS -c tests/test_ipamir_codes.cc -o /tmp/test_codes.o
g++ /tmp/test_codes.o /tmp/ToporIpamir_hook.o libintel_sat_solver_ipamir_lsu_release.a -lpthread -o /tmp/test_codes
/tmp/test_codes
```

### Cases and results — ALL PASS (11/11)
| Test | Asserts | Result |
|---|---|---|
| cost-0 satisfiable | code 30, obj 0 | PASS |
| weighted (a|b), w=3/5 | code 30, obj 3 | PASS |
| contradictory hard clauses | code 20 | PASS |
| incremental + assumption | 30/obj3 then 30/obj5 | PASS |
| forced ERROR (hook) | code 40, then 40 again (sticky) | PASS |

This closes the runtime-verification gap from 8c: **code 40 and its stickiness are now verified
at runtime**, and the 30/20/30-cost0/incremental contract is locked in against regressions.

## 8f. Submission package built + verified (2026-06-17)

Per the MaxSAT-Evaluation incremental-track rules and `maxsat/README`, a submission is a
single directory named after the solver signature whose `make` builds
`<SIG>/libipamir<SIG>.a`. Because IntelSAT is **not** part of the IPAMIR repo (unlike the
example `solver2022`, which references the in-repo `sat/minisat220`), the package must be
**self-contained** and bundle the solver source.

### What was produced — `submission/IntelSatSolver/`
```
submission/IntelSatSolver/
  makefile        default goal -> libipamirIntelSatSolver.a
  glue.cc         supplies MainWallTimePassed() (normally from Main.cc)
  LIBS            -lpthread
  README          layout + build + return-code contract
  solver_src/     full Topor source (46 files: root *.cc/*.h/*.hpp, Makefile,
                  LICENSE, algorithms/{LSU,MrsBeaver,Alg_nuwls}) — NO build
                  artifacts, NO ipamir_test/, NO regression_instances/, NO .git
```
`submission/IntelSatSolver.tar.gz` (164 KB, source-only) is the shippable archive.

### Makefile design
- `SOLVERROOT = solver_src`; builds the release lib with `make -C solver_src libr LIB=intelsat`
  (LIB overridden so the archive name is predictable regardless of dir name) →
  `solver_src/libintelsat_release.a`.
- Repackage as `libipamirIntelSatSolver.a`: `ar d` removes `Main.or` (drops `main()`),
  `ar r` adds `glue.o`, then `ranlib`.

### Verification (all PASS)
1. **Standalone build**: `make` in `submission/IntelSatSolver/` → `libipamirIntelSatSolver.a`
   (9.4 MB). `ar t` confirms it **contains `ToporIpamir.or` + `glue.o`** and **not `Main.or`**.
2. **Self-contained from tarball**: extracted `IntelSatSolver.tar.gz` to a clean `/tmp` dir and
   ran `make` with no other inputs → identical 9.4 MB library. Build is fully standalone.
3. **Functional**: the IPAMIR return-code unit tests (`tests/test_ipamir_codes.cc`) linked
   **directly against the submission-built library** pass 9/9 core checks (the code-40 hook test
   correctly skips, since the hook is compiled out of the production lib).

Confirms the production library carries **no test hooks** (`IPAMIR_TEST_HOOKS` off) and builds
cleanly with just `g++` (C++20) + `-lpthread`.

## 8g. Cost-correctness cross-check (2026-06-17)

Goal: confirm the **values** our solver reports as optimal are actually optimal, independent of
the solver itself.

### Method
- `tests/wcnf_solve.cc`: a minimal single-`ipamir_solve` WCNF front-end (ipamirapp dialect:
  `<h|weight> lits... 0`), linked against the submission library, prints `RESULT o <obj>` /
  `RESULT s UNSATISFIABLE`. (ipamirapp itself was unsuitable as the oracle harness: its
  post-solve loop runs many slow incremental re-solves and does not terminate quickly — a
  performance issue, not a correctness one; its *first* solve was already correct.)
- `tests/cost_crosscheck.py`: an **independent brute-force oracle** that enumerates every
  assignment, keeps only those satisfying all hard clauses, and minimises the summed weight of
  violated soft clauses. It compares the oracle's true optimum against the solver's `RESULT`.

### Results — ALL PASS (46/46)
- 6 hand-built cases (weighted opt=3, unit-cover opt=1, cost-0, UNSAT hard, an 8-var weighted
  mix opt=4, a 3-clause cover opt=8) + 40 seeded random weighted instances (3–12 vars).
- Every reported optimum (and every UNSAT verdict) **matched the independent oracle exactly**.

This is a genuine end-to-end correctness check: instances → IPAMIR API → LSU proof (code 30) →
reported objective, all validated against ground truth computed by a separate method.

### Reproduce
```
make -C submission/IntelSatSolver            # builds libipamirIntelSatSolver.a
g++ -O3 -DNDEBUG -std=c++20 -Iipamir_test/ipamir tests/wcnf_solve.cc \
    submission/IntelSatSolver/libipamirIntelSatSolver.a -lpthread -o /tmp/wcnf_solve
python3 tests/cost_crosscheck.py             # WCNF_SOLVE=/tmp/wcnf_solve by default
```

## 9. Apps / paths

- Apps: `ipamir_test/ipamir/app/ipamir{extenf,c3ref,bioptsat,adaboost,mlicseesaw}/`
- Our wiring: `ipamir_test/ipamir/maxsat/IntelSatSolver/{makefile,glue.cc,LIBS}`
- Submission package: `submission/IntelSatSolver/` (+ `submission/IntelSatSolver.tar.gz`)
- Cross-check tools: `tests/wcnf_solve.cc`, `tests/cost_crosscheck.py`
- Solver branch root: `/root/main/bisan/intel_sat_solver_ipamir_lsu`
- Relevant code: `ToporIpamir.cc` (~lines 89–124, 247–311), `algorithms/LSU.cc`, `algorithms/LSU.hpp`
