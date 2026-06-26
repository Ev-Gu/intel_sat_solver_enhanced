# Project Tasks Status (Jun 26, 2026)

Organized tracker from Yevgeny + Yam conversation.

## Done

| Task | Status | Notes |
|------|--------|-------|
| IPAMIR MrsBeaver/LSU loop bug | ✅ | Yevgeny `02435f3` |
| Main.cc exit 255 (`int` shadow) | ✅ | Yevgeny `02435f3` |
| Fuzzer scripts + tools pushed | ✅ | Bisan `838af2a` |
| IPAMIR compile fix (missing `;`) | ✅ | Phase 1 |
| Solver-internal timeout (`TOPOR_IPAMIR_TIME_LIMIT`) | ✅ | `ToporIpamir.cc` |
| `c timeo` on cost improvement (IPAMIR) | ✅ | `TOPOR_IPAMIR_VERBOSE=1` |
| Main LSU uses remaining timeout budget | ✅ | `Main.cc` |
| Fuzzer 2 passes internal timeout to ours | ✅ | `wcnf_ipamir_fuzz.py` |
| Timeout fixes pushed | ✅ | Bisan `f3546c7` |
| Fuzzer 2 extended run (20 inst) | ✅ | **0 bugs**, 12 suboptimal |
| Performance script + MSE22 subset run | ✅ | `scripts/run_maxsat_performance.sh` |
| Poster summary for Lisa | ✅ | `results/POSTER_RESULTS.md` |

## Remaining / with Yevgeny

| Task | Owner | Notes |
|------|-------|-------|
| MSE2022 incremental *apps* (IPAMIR repo) | You + Yevgeny | Build `app/*` with `IPAMIRSOLVER=IntelSatSolver` |
| NUWLS params like tt-open-wbo-inc | Yevgeny | Reduce suboptimality under time budget |
| Longer fuzzer runs (overnight) | You | Fuzzer 1 + 2 |
| Full regression suite (148+ instances) | You | `./scripts/run_maxsat_regression.sh` |

## Environment variables (IPAMIR)

| Variable | Meaning |
|----------|---------|
| `TOPOR_IPAMIR_TIME_LIMIT` | Per-`ipamir_solve` wall seconds (0 = no limit) |
| `TOPOR_IPAMIR_VERBOSE` | Print `c timeo` on improving cost (default: on if LSU verbose) |
| `TOPOR_NUWLS_TIME_LIMIT` | NUWLS budget inside our solver (fuzzer sets via `--nuwls-time`) |

## Run commands

```bash
cd /root/bisan
./scripts/build_maxsat_tools.sh
bash tools/build_ipamir_wcnf.sh

# Fuzzer 2
PYTHONUNBUFFERED=1 ./scripts/fuzz_maxsat_ipamir.sh -n 20 --nuwls-time 1 --timeout 120

# Performance (MSE22 subset)
./scripts/run_maxsat_performance.sh -n 40 -t 30

# Regression correctness
./scripts/run_maxsat_regression.sh --folder MSE22Unique/
```

## Latest results (Jun 26)

| Metric | Result |
|--------|--------|
| Fuzzer 2 bugs (n=20) | 0 |
| Fuzzer 2 suboptimal | 12 |
| Batch optimum (40 inst, 30s) | 26/40 |
| IPAMIR optimum vs certified | 31/40 |

See `results/POSTER_RESULTS.md` for Lisa.
