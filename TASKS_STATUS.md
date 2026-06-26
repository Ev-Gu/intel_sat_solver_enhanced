# Project Tasks Status (Jun 26, 2026)

Organized tracker from Yevgeny + Yam conversation.

## Done

| Task | Status | Notes |
|------|--------|-------|
| IPAMIR MrsBeaver/LSU loop bug | ✅ | Yevgeny `02435f3` |
| Main.cc exit 255 (`int` shadow) | ✅ | Yevgeny `02435f3` |
| Fuzzer scripts + tools pushed | ✅ | Bisan `838af2a` |
| IPAMIR compile fix (missing `;`) | ✅ | This session |
| Solver-internal timeout (`TOPOR_IPAMIR_TIME_LIMIT`) | ✅ | `ToporIpamir.cc` |
| `c timeo` on cost improvement (IPAMIR) | ✅ | `TOPOR_IPAMIR_VERBOSE=1` |
| Main LSU uses remaining timeout budget | ✅ | `Main.cc` |
| Fuzzer 2 passes internal timeout to ours | ✅ | `wcnf_ipamir_fuzz.py` |

## In progress / next

| Task | Owner | Notes |
|------|-------|-------|
| Rebuild IPAMIR drivers | You | `bash tools/build_ipamir_wcnf.sh` |
| Rerun Fuzzer 1 + 2 | You | Expect 0 bugs, some suboptimal |
| Performance vs MSE2022 IPAMIR solvers | You + Yevgeny | Per Yam: pick solvers + benchmark subset |
| NUWLS params like tt-open-wbo-inc | Yevgeny | Not 15s default if too much |
| Poster numbers for Lisa | You | After performance runs |
| Push timeout fixes to GitHub | Pending | After fuzzer verification |

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

# Fuzzer 2 (short test)
PYTHONUNBUFFERED=1 ./scripts/fuzz_maxsat_ipamir.sh -n 10 --nuwls-time 1 --timeout 120

# Fuzzer 1
PYTHONUNBUFFERED=1 ./scripts/fuzz_maxsat.sh
```
