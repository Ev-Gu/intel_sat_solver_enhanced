## IntelSAT Testing and Debugging Scripts

This directory contains scripts for testing, fuzzing, and delta-debugging IntelSAT.

### Usage  
Run any script without parameters to see its usage instructions.

### Available Scripts  

- **`run_and_verify_intel_sat.csh`**  
  Runs and verifies IntelSAT on a given instance (including incremental instances).  
  Every generated clause and the solution (if any) are verified.  

- **`run_and_verify_intel_sat_on_regression.csh`**  
  Runs and verifies IntelSAT on the regression instances located in `regression_instances`.  
  
- **`test_parsing_only.csh`**  
  Tests parsing correctness (not solving) for a single instance.  
  Verifies that the input file is parsed correctly without actually solving it.  
  
- **`test_parsing_on_regression.csh`**  
  Tests parsing correctness on all regression instances located in `regression_instances`.  
  Runs `test_parsing_only.csh` on each `.cnf` and `.wcnf` file.  
  
- **`delta_debug_intel_sat.csh`**  
  Performs delta debugging on IntelSAT in case of a failure (i.e., finds a small instance where the failure still occurs).

- **`delta_debug_intel_sat_till_fixed_point.csh`**  
  Runs `delta_debug_intel_sat.csh` iteratively until a fixed point is reached.  

- **`fuzz_and_verify.csh`**  
  Fuzzes incremental instances and verifies IntelSAT.  

- **`fuzz_and_verify_parallel.csh`**  
  Fuzzes incremental instances and verifies IntelSAT in parallel.

- **`test_ipamir_incremental.csh`**  
  Tests incremental flow using IPAMIR example programs.  
  Compiles and runs IPAMIR (Incremental Partial MaxSAT API) example programs to verify  
  incremental MaxSAT solving functionality. This script demonstrates how to use the IPAMIR  
  interface for incremental solving with multiple solve calls, assumptions, and querying results.

---

## Running Tests on MSYS2

This section provides step-by-step instructions for running the test scripts in an MSYS2 environment on Windows.

### Prerequisites

1. **Install MSYS2**: If not already installed, download and install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)

2. **Install tcsh**: The scripts require `tcsh` shell. Install it using:
   ```bash
   pacman -S tcsh
   ```

3. **Build IntelSAT**: Ensure IntelSAT is built before running tests. From the project root:
   ```bash
   make librh  # For shared release library
   # OR
   make libr   # For static release library
   ```

4. **Build IntelSAT executable** (required for run_and_verify and parsing tests):
   ```bash
   make rs     # For static release executable
   ```

### Test Scripts for MSYS2

#### 1. IPAMIR Incremental Test (`test_ipamir_incremental.csh`)

This test compiles and runs IPAMIR example programs to verify incremental MaxSAT solving.

**Location**: From the project root directory

**Command**:
```bash
tcsh scripts/test_ipamir_incremental.csh .
```

**What it does**:
- Automatically finds or builds the IPAMIR shared library
- Compiles example programs from `ipamir_examples/` directory
- Runs three test examples demonstrating incremental solving
- Verifies that all examples complete successfully

**Expected output**:
```
=========================================
Testing IPAMIR Incremental Flow
=========================================
...
Compiled 3 example(s) successfully
...
=========================================
All IPAMIR incremental flow tests PASSED
=========================================
```

**Troubleshooting**:
- If you get "No such file or directory" errors, ensure you're running from the project root
- If compilation fails, check that `g++` is available: `which g++`
- The script handles paths with spaces automatically

---

#### 2. Parsing Test (`test_parsing_only.csh`)

This test verifies that input files are parsed correctly without actually solving them.

**Location**: From the project root directory

**Basic usage** (single file):
```bash
tcsh scripts/test_parsing_only.csh intel_sat_solver_test_static regression_instances/regr_1.cnf
```

**Parameters**:
- `intel_sat_solver_test_static`: Path to the IntelSAT executable (adjust if using a different build)
- `regression_instances/regr_1.cnf`: Path to the input file to test

**Running on all regression instances**:
```bash
tcsh scripts/test_parsing_on_regression.csh intel_sat_solver_test_static
```

This automatically runs the parsing test on all `.cnf` and `.wcnf` files in the `regression_instances/` directory.

**Expected output**:
```
Testing parsing of: regression_instances/regr_1.cnf
Parsing test completed successfully
```

---

#### 3. Run and Verify Test (`run_and_verify_intel_sat.csh`)

This test runs IntelSAT on an instance and verifies the results using drat-trim and DiMoCheck.

**Location**: From the project root directory

**Basic usage** (single file):
```bash
tcsh scripts/run_and_verify_intel_sat.csh intel_sat_solver_test_static regression_instances/regr_1.cnf
```

**Parameters**:
- `intel_sat_solver_test_static`: Path to the IntelSAT executable
- `regression_instances/regr_1.cnf`: Path to the input file
- Optional: Additional solver parameters can be added after the file path

**Example with parameters**:
```bash
tcsh scripts/run_and_verify_intel_sat.csh intel_sat_solver_test_static regression_instances/regr_1.cnf /mode/value 0
```

**Running on all regression instances**:
```bash
tcsh scripts/run_and_verify_intel_sat_on_regression.csh intel_sat_solver_test_static
```

This runs the verification test on all regression instances with various solver modes.

**Expected output**:
```
The command-line intel_sat_solver_test_static regression_instances/regr_1.cnf
...
Ok
```

---

### Common Issues and Solutions

**Issue: "tcsh: command not found"**
- **Solution**: Install tcsh: `pacman -S tcsh`

**Issue: "No such file or directory" when running scripts**
- **Solution**: Ensure you're in the project root directory. Check with `pwd` and navigate if needed: `cd /c/path/to/intel_sat_solver_enhanced`

**Issue: "Library not found" errors**
- **Solution**: Build the library first: `make librh` (for shared) or `make libr` (for static)

**Issue: "Executable not found"**
- **Solution**: Build the executable: `make rs` (for static release)

**Issue: Paths with spaces causing errors**
- **Solution**: The scripts handle paths with spaces automatically. If you encounter issues, ensure you're using `tcsh` (not `csh`) and that you're using the latest version of the scripts.

**Issue: "Permission denied" errors**
- **Solution**: Ensure executables have execute permissions. In MSYS2, you may need to rebuild or check file permissions.

---

### Quick Start Summary

For a quick test run in MSYS2:

```bash
# 1. Navigate to project root
cd /c/path/to/intel_sat_solver_enhanced

# 2. Build IntelSAT (if not already built)
make librh    # For IPAMIR test
make rs       # For run_and_verify and parsing tests

# 3. Run IPAMIR test
tcsh scripts/test_ipamir_incremental.csh .

# 4. Run parsing test on a single file
tcsh scripts/test_parsing_only.csh intel_sat_solver_test_static regression_instances/regr_1.cnf

# 5. Run verification test on a single file
tcsh scripts/run_and_verify_intel_sat.csh intel_sat_solver_test_static regression_instances/regr_1.cnf
```

All scripts will exit with code 0 on success and non-zero on failure, making them suitable for use in automated testing pipelines.  
