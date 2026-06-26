# MSE Ref-First Benchmark (WCNF)

Generated: 2026-06-26T15:38:24.984582+00:00

**Mode:** ref-first WCNF (Yam: run what reference solved first)
**CSV:** `third_party/MaxSATRegressionSuite/MSE22Unique.csv`
**Timeout:** 60s
**Phase 2 completed:** 141/141 instances

## Phase 1 — EvalMaxSAT filter

| Scanned | Passed filter |
|---------|---------------|
| 148 | 141 |

## Phase 2 — IntelTopor on filtered set only

| Path | Solved | Matches certified optimum | Avg time (solved) |
|------|--------|---------------------------|-------------------|
| Batch (-M 1) | 78/141 | 78/141 | 1.27s |
| IPAMIR | 140/141 | 99/141 | 26.54s |
| EvalMaxSAT (phase 1) | 141/141 | — | 0.00s |

## Interpretation

- Only instances **EvalMaxSAT solved within timeout** are compared (fair subset).
- Official MSE uses 7200s; this run uses a dev timeout for practical iteration.
