# MaxSAT regression instances

This directory mirrors `regression_instances/` (SAT) for **MaxSAT**.

## Quick smoke tests (committed here)

Small `.wcnf` files for a fast local check (modern format: use `h` for hard clauses, no `p wcnf` line).

```bash
# From repo root, after scripts/build_maxsat_tools.sh:
./scripts/compare_wcnf_batch_vs_ipamir.sh maxsat_regression_instances/*.wcnf
bash third_party/MaxSAT-Fuzzer/Scripts/intel_topor_maxsat.sh maxsat_regression_instances/smoke_1.wcnf
```

## Full MSE regression suite

The official suite lives under `third_party/MaxSATRegressionSuite/` (not committed — large download).

```bash
cd third_party/MaxSATRegressionSuite
./install
cd ../..
./scripts/run_maxsat_regression.sh
```

See `third_party/MaxSATRegressionSuite/README.md` for details.
