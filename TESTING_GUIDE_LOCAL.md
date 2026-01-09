# IntelSAT Testing Guide - Local Reference

**FOR LOCAL USE ONLY - DO NOT COMMIT TO GIT**

This document provides quick reference commands for running all IntelSAT tests in MSYS2 on your Windows system.

## Project Root Directory

```
/c/Users/gal13/OneDrive - Technion/Documents/Thechnion Fourth year/Demo-MAX-SAT/intel_sat_solver_enhanced
```

## Quick Navigation

From MSYS2, navigate to project root:
```bash
cd "/c/Users/gal13/OneDrive - Technion/Documents/Thechnion Fourth year/Demo-MAX-SAT/intel_sat_solver_enhanced"
```

---

## Prerequisites

### 1. Install tcsh (if not already installed)
```bash
pacman -S tcsh
```

### 2. Build IntelSAT (required before running tests)

**Build static release executable:**
```bash
make rs
```
This creates: `./intel_sat_solver_enhanced_static.exe` (use as `./intel_sat_solver_enhanced_static` in commands)

**Build IPAMIR library (for IPAMIR tests):**
```bash
make librh  # Shared release library
# OR
make libr   # Static release library
```

**Build fuzzers (for fuzzing tests):**
```bash
cd third_party/cnfuzzdd2013
tcsh make_fuzzers.csh
cd ../..
```

---

## Test Scripts

### 1. IPAMIR Incremental Test

**Purpose**: Tests incremental MaxSAT solving using IPAMIR API examples

**Command**:
```bash
tcsh scripts/test_ipamir_incremental.csh .
```

**What it does**:
- Compiles IPAMIR example programs
- Runs 3 example programs (incremental, assumptions, complex)
- Verifies incremental MaxSAT functionality

**Expected output**: All examples should compile and run successfully

---

### 2. Parsing Test (Single File)

**Purpose**: Tests if a CNF/WCNF file can be parsed correctly (without solving)

**Command**:
```bash
tcsh scripts/test_parsing_only.csh ./intel_sat_solver_enhanced_static path/to/file.cnf
```

**Example**:
```bash
tcsh scripts/test_parsing_only.csh ./intel_sat_solver_enhanced_static regression_instances/regr_1.cnf
```

**For MaxSAT (WCNF) files**:
```bash
tcsh scripts/test_parsing_only.csh ./intel_sat_solver_enhanced_static regression_instances/regr_wcnf_1.wcnf -M 1
```

---

### 3. Parsing Test (All Regression Instances)

**Purpose**: Tests parsing on all CNF/WCNF files in `regression_instances/`

**Command**:
```bash
tcsh scripts/test_parsing_on_regression.csh ./intel_sat_solver_enhanced_static
```

**What it does**:
- Tests all `.cnf` files in regression_instances/
- Tests all `.wcnf` files with `-M 1` flag

---

### 4. Output Format Test (Single File)

**Purpose**: Validates that solver output conforms to DIMACS format standards

**Command**:
```bash
tcsh scripts/test_output_format.csh ./intel_sat_solver_enhanced_static path/to/file.cnf
```

**Example (SAT)**:
```bash
tcsh scripts/test_output_format.csh ./intel_sat_solver_enhanced_static regression_instances/regr_1.cnf
```

**Example (MaxSAT)**:
```bash
tcsh scripts/test_output_format.csh ./intel_sat_solver_enhanced_static regression_instances/regr_wcnf_1.wcnf -M 1
```

**What it checks**:
- Result line format (`s SATISFIABLE` or `s UNSATISFIABLE`)
- Model line format (if SATISFIABLE)
- Objective line format (for MaxSAT)
- No invalid output lines

---

### 5. Output Format Test (All Regression Instances)

**Purpose**: Tests output format on all regression instances

**Command**:
```bash
tcsh scripts/test_output_format_on_regression.csh ./intel_sat_solver_enhanced_static
```

---

### 6. Run and Verify (Single File)

**Purpose**: Runs IntelSAT and verifies correctness using drat-trim and DiMoCheck

**Command**:
```bash
tcsh scripts/run_and_verify_intel_sat.csh ./intel_sat_solver_enhanced_static path/to/file.cnf
```

**Example (SAT)**:
```bash
tcsh scripts/run_and_verify_intel_sat.csh ./intel_sat_solver_enhanced_static regression_instances/regr_1.cnf
```

**Example (MaxSAT)**:
```bash
tcsh scripts/run_and_verify_intel_sat.csh ./intel_sat_solver_enhanced_static regression_instances/regr_wcnf_1.wcnf -M 1
```

**What it does**:
- Runs solver with proof generation enabled
- Verifies SAT results with DiMoCheck
- Verifies UNSAT results with drat-trim
- Verifies incremental queries separately

---

### 7. Run and Verify (All Regression Instances)

**Purpose**: Verifies all regression instances

**Command**:
```bash
tcsh scripts/run_and_verify_intel_sat_on_regression.csh ./intel_sat_solver_enhanced_static
```

**What it does**:
- Tests all `ddtbug_*.cnf` files (minimized bug test cases)
- Tests all `regr_*.cnf` files with different solver modes (0-8) and solver_mode values (0-1)

---

### 8. Fuzzing (CNF - SAT)

**Purpose**: Continuously generates random CNF formulas and tests the solver

**Command**:
```bash
tcsh scripts/fuzz_and_verify.csh ./intel_sat_solver_enhanced_static cnf
```

**Or** (default is CNF):
```bash
tcsh scripts/fuzz_and_verify.csh ./intel_sat_solver_enhanced_static
```

**What it does**:
- Generates random CNF formulas using `cnfuzz_incr`
- Tests each formula with verification
- If bug found: delta debugging minimizes it and saves to `regression_instances/ddtbug_*.cnf`
- Runs forever until bug found or manually stopped (Ctrl+C)

---

### 9. Fuzzing (WCNF - MaxSAT)

**Purpose**: Continuously generates random WCNF (MaxSAT) formulas and tests the solver

**Command**:
```bash
tcsh scripts/fuzz_and_verify.csh ./intel_sat_solver_enhanced_static wcnf
```

**What it does**:
- Generates random WCNF formulas using `wcnfuzz_incr`
- Tests each formula with MaxSAT mode (`-M 1`)
- If bug found: delta debugging minimizes it and saves to `regression_instances/ddtbug_*.cnf`
- Runs forever until bug found or manually stopped (Ctrl+C)

**Note**: This is the newly implemented WCNF fuzzing!

---

### 10. Fuzzing (Both CNF and WCNF)

**Purpose**: Alternates between CNF and WCNF fuzzing

**Command**:
```bash
tcsh scripts/fuzz_and_verify.csh ./intel_sat_solver_enhanced_static both
```

---

### 11. Parallel Fuzzing

**Purpose**: Runs multiple fuzzing threads in parallel

**Command**:
```bash
tcsh scripts/fuzz_and_verify_parallel.csh ./intel_sat_solver_enhanced_static 9
```

**Parameters**:
- First: executable path
- Second: number of threads (recommended: 9 to test all solver modes)
- Additional: any extra solver parameters

**What it does**:
- Spawns multiple fuzzing processes in parallel
- Each thread tests different solver modes
- Periodically prints status of all threads

---

### 12. Performance Baseline Evaluation

**Purpose**: Establishes baseline runtime and quality metrics on selected benchmarks

**Command**:
```bash
tcsh scripts/performance_baseline.csh ./intel_sat_solver_enhanced_static
```

**With custom output file**:
```bash
tcsh scripts/performance_baseline.csh ./intel_sat_solver_enhanced_static baseline_2026.txt
```

**What it does**:
- Runs solver on a representative set of benchmarks (mix of CNF and WCNF files)
- Measures wall-clock runtime for each benchmark
- Extracts quality metrics:
  - SAT result (SATISFIABLE/UNSATISFIABLE/TIMEOUT)
  - Objective value (for MaxSAT instances)
- Saves results to a structured output file (default: `baseline_results.txt`)
- Reports summary statistics (total tests, passed, failed)

**Output format**:
```
Benchmark | Type | Result | Objective | Runtime(s) | Status
```

**Use cases**:
- Establish performance baseline before making optimizations
- Compare performance after code changes
- Validate solver behavior on standard benchmarks
- Track runtime and quality metrics over time

---

## Common Test Workflows

### Performance Baseline Evaluation
```bash
# 1. Build the solver
make rs

# 2. Run baseline evaluation
tcsh scripts/performance_baseline.csh ./intel_sat_solver_enhanced_static

# 3. Review results
cat baseline_results.txt
```

### Quick Test Everything
```bash
# 1. Build everything
make rs
cd third_party/cnfuzzdd2013 && tcsh make_fuzzers.csh && cd ../..

# 2. Test IPAMIR
tcsh scripts/test_ipamir_incremental.csh .

# 3. Test parsing on all regression instances
tcsh scripts/test_parsing_on_regression.csh ./intel_sat_solver_enhanced_static

# 4. Test output format on all regression instances
tcsh scripts/test_output_format_on_regression.csh ./intel_sat_solver_enhanced_static

# 5. Verify all regression instances
tcsh scripts/run_and_verify_intel_sat_on_regression.csh ./intel_sat_solver_enhanced_static
```

### Test New WCNF Fuzzing
```bash
# Build fuzzers
cd third_party/cnfuzzdd2013
tcsh make_fuzzers.csh
cd ../..

# Run WCNF fuzzing
tcsh scripts/fuzz_and_verify.csh ./intel_sat_solver_enhanced_static wcnf
```

### Test Single File Manually
```bash
# Test a specific file
tcsh scripts/run_and_verify_intel_sat.csh ./intel_sat_solver_enhanced_static regression_instances/regr_wcnf_1.wcnf -M 1
```

---

## File Locations

- **Executable**: `./intel_sat_solver_enhanced_static` (or `.exe`)
- **Scripts**: `scripts/*.csh`
- **Regression instances**: `regression_instances/*.cnf` and `regression_instances/*.wcnf`
- **Fuzzers**: `third_party/cnfuzzdd2013/cnfuzz_incr`, `wcnfuzz_incr`
- **IPAMIR examples**: `ipamir_examples/example*.cpp`
- **Baseline results**: `baseline_results.txt` (generated by performance_baseline.csh)

---

## Troubleshooting

### "Command not found: tcsh"
```bash
pacman -S tcsh
```

### "intel_sat_executable doesn't exist"
Make sure you built the executable:
```bash
make rs
```

### "wcnfuzz_incr not found"
Build the fuzzers:
```bash
cd third_party/cnfuzzdd2013
tcsh make_fuzzers.csh
cd ../..
```

### Path issues in MSYS2
Always use `./intel_sat_solver_enhanced_static` (with `./`) not just the name, as it may not be in PATH.

---

## Notes

- All scripts use `tcsh` shell (not `bash`)
- Use `./intel_sat_solver_enhanced_static` for the executable (relative path)
- For MaxSAT/WCNF files, add `-M 1` flag
- Fuzzing runs indefinitely until stopped (Ctrl+C) or bug found
- Delta debugging can be slow (tests each clause removal individually)

---

**Last Updated**: January 9, 2026

