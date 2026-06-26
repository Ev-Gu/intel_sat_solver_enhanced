# MaxSAT Performance Summary

Generated: 2026-06-26T09:35:35.442013Z
Instances: 40 from `third_party/MaxSATRegressionSuite/MSE22Unique.csv`
Timeout: 30s per solver run
IPAMIR internal limit: TOPOR_IPAMIR_TIME_LIMIT=29

## Batch (WCNF, non-incremental)

| Solver | Solved | Avg time (solved) |
|--------|--------|-------------------|
| IntelTopor | 26/40 | 2.04s |
| EvalMaxSAT 2022 | 38/40 | 0.01s |

## IPAMIR (WCNF loader, incremental API)

| Solver | Solved | Avg time (solved) |
|--------|--------|-------------------|
| IntelTopor IPAMIR | 38/40 | 10.35s |
| UWrMaxSat 1.4 IPAMIR | 38/40 | 0.00s |

## Correctness vs references

- Batch objective match (both solved vs EvalMaxSAT): 26 instances (all matched)
- IPAMIR vs certified `BestOValue` in CSV: IntelTopor **31/40**
- Batch vs certified `BestOValue`: IntelTopor **26/40**
- Note: UWrMaxSat WCNF loader often prints `o 0` on these regression files; use fuzzer oracle for IPAMIR cost parity, not this loader on static WCNFs.

## Notes for poster (Lisa / MSE2022 incremental track)

- Regression subset: MSE22+23 unique instances (certified where noted in CSV).
- Full MSE2022 incremental *applications* (IPAMIR repo `app/`) need separate driver builds per Yam.
- UWrMaxSat 1.4 IPAMIR is the exact oracle used in Fuzzer 2; EvalMaxSAT is batch reference.
- Competition uses 7200s; this run uses a shorter timeout for a practical subset benchmark.
