# MSE2022 performance benchmark (separate from regression suite)

This folder is **not** the MaxSAT regression / correctness suite.

| | Regression suite | This benchmark |
|---|------------------|----------------|
| Location | `third_party/MaxSATRegressionSuite/` | `benchmarks/mse2022/` |
| Purpose | Bug-finding, smoke tests, correctness | Yam ref-first **performance** comparison |
| Instances list | `MSE22Unique.csv` (in suite) | `instances.csv` (here) |
| WCNF files | Stored under regression suite tree | Resolved via `WCNF_ROOT` (read-only paths) |

## Configuration (Yevgeny, Jun 2026)

- **Phase 2:** IntelTopor **IPAMIR only** (no batch)
- **NUWLS:** disabled via `TOPOR_NUWLS_TIME_LIMIT=0` by default
- **WCNF timeout:** 3600s (MSE complete track)
- **IPAMIR app timeout:** 7200s (MSE incremental track)

## Run

```bash
./benchmarks/mse2022/run_bench.sh ref-first-wcnf --timeout 3600
./benchmarks/mse2022/run_bench.sh ipamir-app --timeout 7200
```

Results: `benchmarks/mse2022/results/`
