# MSE2022 IPAMIR App: ipamirapp

Generated: 2026-06-26T11:51:51.176539Z

**App:** `ipamirapp` (MSE2022 incremental IPAMIR benchmark)
**Reference:** UWrMaxSat 1.4 IPAMIR
**Timeout:** 60s per app run
**TOPOR_IPAMIR_TIME_LIMIT:** 30s

## Results

| | Passed filter / ran | Avg wall time |
|--|---------------------|---------------|
| UWrMaxSat (phase 1) | 2/3 | 5.81s |
| IntelSatSolver (phase 2) | 2/2 | 5.60s |

### Per instance

| Input | Ref time | Ours time | Ours OK |
|-------|----------|-----------|---------|
| max_clq_150-0-447-1.clq.wcnf | 0.01s | 0.01s | yes |
| wqueens8_6.wcsp.dir.wcnf | 11.61s | 11.19s | yes |
