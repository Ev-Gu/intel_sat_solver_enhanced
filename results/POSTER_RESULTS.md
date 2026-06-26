# IntelTopor MaxSAT — Results for Poster (Lisa)

**Date:** Jun 26, 2026  
**Branch:** `New-Basic-Corectness-Validation`  
**Contact:** Bisan / Yevgeny

---

## 1. Correctness (incremental IPAMIR)

Differential fuzzer compares **IntelTopor IPAMIR** vs **UWrMaxSat 1.4 IPAMIR** on random WCNFs with incremental solve rounds.

| Run | Instances | Bugs | Suboptimal |
|-----|-----------|------|------------|
| Smoke (5) | 5 | 0 | 4 |
| Extended (20) | 20 | **0** | 12 |

- Internal per-solve timeout: `TOPOR_IPAMIR_TIME_LIMIT` (env)
- Progress logging: `c timeo <time> <cost>` when cost improves
- **No SAT/UNSAT or hard-clause correctness bugs** in extended run after Yevgeny's fixes + timeout work

---

## 2. Performance — ref-first benchmark (Yam method, **final run**)

**Benchmark:** `MSE22Unique.csv` — only instances **EvalMaxSAT solved within 60s** (141 of 148)  
**Timeout:** 60 s per solver invocation  
**Script:** `./scripts/run_mse2022_bench.sh ref-first-wcnf --timeout 60`  
**Full report:** `results/MSE2022_REF_FIRST_WCNF.md`

| Path | Solved | Matches certified optimum | Avg time (solved) |
|------|--------|---------------------------|-------------------|
| **IntelTopor batch (-M 1)** | **78 / 141** | **78 / 141** | 1.27 s |
| **IntelTopor IPAMIR** | **140 / 141** | **99 / 141** | 26.54 s |
| EvalMaxSAT 2022 (filter) | 141 / 141 | — | ~0 s |

**Takeaway:** On the fair subset (what the reference solved), **IPAMIR solves far more instances than batch** (140 vs 78) within 60s; batch is faster when it succeeds. Official MSE uses 7200s per instance — this run uses 60s for poster/dev (per Yam).

### Earlier smoke (40 instances, 30s — superseded for poster)

See `results/PERFORMANCE_SUMMARY.md` if needed; use ref-first numbers above for Lisa.

---

## 3. MSE2022 incremental track (next step, per Yam)

- Full **incremental applications** from the [IPAMIR repository](https://bitbucket.org/coreo-group/ipamir/) (`app/*`) should be built with `IPAMIRSOLVER=IntelSatSolver` and compared to TT-Open-WBO-Inc / other MSE2022 incremental solvers.
- Competition timeout is 7200 s; use a **subset** or reduced timeout for development runs.
- NUWLS tuning to match **tt-open-wbo-inc** parameters — coordinate with Yevgeny.

---

## 4. Infrastructure delivered

| Component | Purpose |
|-----------|---------|
| `scripts/fuzz_maxsat.sh` | Fuzzer 1: batch vs EvalMaxSAT |
| `scripts/fuzz_maxsat_ipamir.sh` | Fuzzer 2: IPAMIR vs UWrMaxSat |
| `scripts/run_maxsat_performance.sh` | Regression performance comparison |
| `scripts/run_maxsat_regression.sh` | MSE regression correctness |
| `TASKS_STATUS.md` | Task tracker |

---

## 5. Suggested poster bullets

1. **Incremental MaxSAT (IPAMIR)** integrated into IntelTopor with differential fuzzing vs UWrMaxSat — **0 correctness bugs** in 20-instance extended fuzz run.
2. **Internal timeout + `c timeo`** align IPAMIR behavior with competition-style anytime output.
3. **Ref-first benchmark (141 instances, 60s):** IPAMIR **140/141 solved**, **99/141 certified optimum** vs batch **78/141** — IPAMIR path much stronger on EvalMaxSAT-feasible subset.
