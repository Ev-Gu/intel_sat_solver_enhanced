# Differential MaxSAT Testing

MaxSAT testing is **separate** from SAT (see `scripts/README.md`).

## Two fuzzers

| | Fuzzer 1 | Fuzzer 2 |
|---|----------|----------|
| **Script** | `./scripts/fuzz_maxsat.sh` | `./scripts/fuzz_maxsat_ipamir.sh` |
| **Input** | random `.wcnf` | random `.wcnf` (same generator) |
| **Path** | batch (`-M 1`) | IPAMIR (Yevgeny WCNF loader) |
| **Compare** | Ours vs **EvalMaxSAT** | Ours vs **UWrMaxSat** |

Extra check (not a fuzzer): `./scripts/compare_wcnf_batch_vs_ipamir.sh` — batch vs IPAMIR on **our** solver only.

## Build (once)

```bash
./scripts/build_maxsat_tools.sh
```

## Run

```bash
# Fuzzer 1 — batch WCNF
./scripts/fuzz_maxsat.sh

# Fuzzer 2 — IPAMIR WCNF (ours vs UWrMaxSat)
PYTHONUNBUFFERED=1 ./scripts/fuzz_maxsat_ipamir.sh -n 10 --nuwls-time 1

# Batch vs IPAMIR consistency (our solver)
./scripts/compare_wcnf_batch_vs_ipamir.sh maxsat_regression_instances/*.wcnf

# MSE regression (after ./install in MaxSATRegressionSuite)
./scripts/run_maxsat_regression.sh
```

## Layout (mirrors SAT)

| SAT | MaxSAT |
|-----|--------|
| `scripts/fuzz_and_verify.csh` | `scripts/fuzz_maxsat.sh` + `scripts/fuzz_maxsat_ipamir.sh` |
| `scripts/run_and_verify_intel_sat_on_regression.csh` | `scripts/run_maxsat_regression.sh` |
| `regression_instances/*.cnf` | `maxsat_regression_instances/` + `MaxSATRegressionSuite/` |
| `third_party/cnfuzzdd2013/` | `third_party/MaxSAT-Fuzzer/` |
| — | `tools/ipamir_wcnf_*.cpp` (Yevgeny WCNF→IPAMIR loader) |

## Components

- **`third_party/MaxSAT-Fuzzer/`** — WCNF generator + fuzzer 1
- **`tools/wcnf_ipamir_fuzz.py`** — fuzzer 2 loop
- **`tools/bin/ipamir_wcnf_ours`** / **`ipamir_wcnf_uwrmaxsat`** — Yevgeny loader × two libraries
- **`third_party/MaxSATRegressionSuite/`** — MSE regression

Bug WCNFs from fuzzer 2: `tools/Logs/<run>/FaultyWCNFs/`
