# Fuzzer 2 Summary (Jun 26, 2026)

**Command:** `./scripts/fuzz_maxsat_ipamir.sh -n 20 --nuwls-time 1 --timeout 120`

| Metric | Value |
|--------|-------|
| Instances | 20 |
| **Bugs (SAT/UNSAT mismatch vs UWrMaxSat)** | **0** |
| Suboptimal (ours worse cost, both SAT) | 12 |
| Skipped (generator fail) | 0 |
| Runtime | ~389 s (~0.05 inst/s) |
| Log dir | `tools/Logs/2026-06-26-052239-10692-wcnfipamirfuzz` |

**Interpretation (Yevgeny's target):** With internal timeout (`TOPOR_IPAMIR_TIME_LIMIT`) and `c timeo` logging, correctness holds; remaining gap is **suboptimality** under tight NUWLS budget (1s), not wrong answers.
