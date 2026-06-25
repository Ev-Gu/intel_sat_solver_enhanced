# Differential Fuzzing Setup

Two fuzzers validate IntelTopor / IntelSatSolver on this branch.

## Build the solver (once)

From the **repository root**:

```bash
# Static release executable — used by the non-incremental WCNF fuzzer
make rs EXEC=intel_sat_solver_enhanced

# Static release library — used by the incremental IPAMIR fuzzer (via build.sh)
make libr LIB=IntelSatSolver
```

Team standard: **`make rs`** (static release), not `make r` (dynamic `_release`).

---

## Fuzzer 1 — Non-incremental (`.wcnf`)

**Directory:** `third_party/MaxSAT-Fuzzer/`

| File | Role |
|------|------|
| `runwcnfuzz.py` | Main fuzzer loop |
| `compare.py` | Runs two solvers, compares output |
| `config.py` | Solver paths (IntelTopor + EvalMaxSAT) |
| `Scripts/intel_topor_maxsat.sh` | Runs `intel_sat_solver_enhanced_static` on WCNF |
| `Scripts/install_evalmaxsat.sh` | Clone + build EvalMaxSAT 2022 reference |
| `Fuzzer/wcnfuzz/wcnfuzz.c` | Random WCNF generator |

**Setup:**

```bash
bash third_party/MaxSAT-Fuzzer/Scripts/install_evalmaxsat.sh
cd third_party/MaxSAT-Fuzzer/Fuzzer/wcnfuzz && make
```

**Run:**

```bash
cd third_party/MaxSAT-Fuzzer
./runwcnfuzz.py -t 4 --timeout 30 --upperBound 4611686018427387904
```

---

## Fuzzer 2 — Incremental (IPAMIR)

**Directory:** `third_party/IncrementalFuzzer/`

| File | Role |
|------|------|
| `incr_fuzz.py` | Generates scenarios, compares our solver vs UWrMaxSat |
| `incr_driver.cc` | Replays a scenario through any linked IPAMIR library |
| `build.sh` | Builds static IPAMIR libs + `bin/driver_ours` / `driver_uwrmaxsat` |
| `Scripts/install_ipamir.sh` | Clone IPAMIR repo + apply UWrMaxSat compile patches |
| `patches/uwrmaxsat-build.patch` | Fixes for UWrMaxSat + cominisatps on modern g++ |

**Setup:**

```bash
bash third_party/IncrementalFuzzer/Scripts/install_ipamir.sh
cd third_party/IncrementalFuzzer && ./build.sh
```

**Run:**

```bash
cd third_party/IncrementalFuzzer
./incr_fuzz.py
```

Bug scenarios are saved under `Logs/<run>/FaultyScenarios/`.
