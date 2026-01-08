# IPAMIR Example Programs

This directory contains example programs demonstrating the use of the IPAMIR (Incremental Partial MaxSAT API) interface for incremental MaxSAT solving.

## Overview

IPAMIR is a C API for incremental MaxSAT solving, similar to IPASIR for incremental SAT solving. These examples demonstrate how to:

- Initialize and release IPAMIR solvers
- Add hard clauses incrementally
- Add soft literals with weights
- Use assumptions to harden soft clauses
- Solve incrementally (multiple solve calls)
- Query results and objective values

## Example Programs

### example1_incremental.cpp

Demonstrates basic incremental MaxSAT solving:
- Adding hard clauses
- Adding soft literals with weights
- Performing multiple solve calls with incremental modifications
- Querying objective values and variable assignments

### example2_assumptions.cpp

Demonstrates incremental solving with assumptions:
- Using assumptions to harden soft clauses incrementally
- Solving with different assumption sets
- Querying results after each solve with assumptions

### example3_complex.cpp

Demonstrates a more complex incremental scenario:
- Building a problem incrementally over multiple steps
- Multiple solve calls with various incremental modifications
- Comprehensive result checking and verification

## Building and Running

### Option 1: Use the test script (recommended)

The easiest way to build and run all examples is using the provided test script:

```bash
# From the root directory
scripts/test_ipamir_incremental.csh .
```

This script will:
1. Build the IPAMIR library if needed
2. Compile all example programs
3. Run each example and verify success

### Option 2: Manual compilation

First, build the IPAMIR library:

```bash
# From the root directory
make librh    # Build shared release library
# OR
make libr     # Build static release library
```

Then compile an example program:

For shared library:
```bash
g++ -std=c++20 -Wall -O2 -I.. -o example1_incremental example1_incremental.cpp -L.. -ltopor_shared_release -lpthread -Wl,-rpath,..
LD_LIBRARY_PATH=.. ./example1_incremental
```

For static library:
```bash
g++ -std=c++20 -Wall -O2 -I.. -o example1_incremental example1_incremental.cpp ../libtopor_release.a -lpthread
./example1_incremental
```

## IPAMIR API Overview

### Key Functions

- `ipamir_signature()` - Returns solver name and version
- `ipamir_init()` - Creates a new solver instance
- `ipamir_release(void* solver)` - Releases solver resources
- `ipamir_add_hard(void* solver, int32_t lit_or_zero)` - Adds a literal to the current hard clause (0 finalizes the clause)
- `ipamir_add_soft_lit(void* solver, int32_t lit, uint64_t weight)` - Declares a literal as soft with given weight
- `ipamir_assume(void* solver, int32_t lit)` - Adds an assumption for the next solve call
- `ipamir_solve(void* solver)` - Solves the current instance
- `ipamir_val_obj(void* solver)` - Returns the objective value of the current solution
- `ipamir_val_lit(void* solver, int32_t lit)` - Returns the truth value of a literal in the current solution

### Return Values from ipamir_solve

- `0` - Search interrupted, no feasible solution found
- `10` - Feasible solution found (state: SAT)
- `20` - No feasible solution exists (state: UNSAT)
- `30` - Optimal solution found (state: OPTIMAL)
- `40` - Error state

## Incremental Flow

The key concept in IPAMIR is **incremental solving**:

1. **Initialize** the solver once
2. **Add clauses and soft literals** incrementally
3. **Call solve** multiple times
4. After each solve, you can:
   - Add more hard clauses
   - Add or modify soft literals
   - Add assumptions for the next solve
   - Query results
5. **Release** the solver when done

The solver maintains state between solve calls, allowing efficient incremental solving without re-adding all clauses each time.

## Testing

To verify the IPAMIR incremental flow is working correctly, run:

```bash
scripts/test_ipamir_incremental.csh .
```

The script will compile and run all examples, verifying that each completes successfully.

## Notes

- All literals use DIMACS encoding (non-zero integers, negation represented by negative values)
- Literals must be in the range [INT32_MIN+1, INT32_MAX]
- Soft literals declared via `ipamir_add_soft_lit` represent unit soft clauses
- Non-unit soft clauses should be normalized by introducing an activation literal
- Assumptions are cleared after each `ipamir_solve` call
- The state of the solver is maintained between solve calls, enabling true incremental solving

