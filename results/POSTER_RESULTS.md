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

## 2. Performance (MSE regression subset)

**Benchmark:** 40 instances from `MSE22Unique.csv` (MSE22 regression suite, certified optimums where noted)  
**Timeout:** 30 s per solver invocation  
**Script:** `./scripts/run_maxsat_performance.sh -n 40 -t 30`

### Batch (non-incremental, `-M 1`)

| Solver | Solved (SAT/optimum) | Matches certified optimum | Avg time when solved |
|--------|----------------------|---------------------------|----------------------|
| **IntelTopor** | 26 / 40 | 26 / 40 | 2.04 s |
| EvalMaxSAT 2022 | 38 / 40 | — | 0.01 s |

### IPAMIR (WCNF loader, single load + solve)

| Solver | Solved | Matches certified optimum | Avg time when solved |
|--------|--------|---------------------------|----------------------|
| **IntelTopor IPAMIR** | 38 / 40 | **31 / 40** | 10.35 s |
| UWrMaxSat 1.4 IPAMIR | 38 / 40 | *(loader reports `o 0` on many instances — not used for cost parity on this benchmark)* | — |

**Takeaway:** On this certified subset, IPAMIR path reaches optimum on **more instances (31) than batch (26)** within 30 s, but is slower per instance than EvalMaxSAT on batch mode.

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
3. On **MSE22 regression subset** (40 instances, 30 s): IPAMIR solves **31/40 to certified optimum** vs **26/40** batch mode; performance tuning vs MSE2022 incremental solvers is ongoing.
