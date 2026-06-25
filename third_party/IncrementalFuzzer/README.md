# Incremental IPAMIR Differential Fuzzer

This is the **second** fuzzer for the project. The existing
`third_party/MaxSAT-Fuzzer` tests the **non-incremental** `.wcnf` path. This one
tests the **incremental** path through the standard **IPAMIR** API
(`ipamir_init`, `ipamir_add_hard`, `ipamir_add_soft_lit`, `ipamir_assume`,
`ipamir_solve`, `ipamir_val_obj`, ...).

## What it does

It runs in an endless loop:

1. **Generates** a random *incremental scenario* — a deterministic list of
   IPAMIR operations (add hard clauses, declare/re-weight soft literals, add
   assumptions, and `solve` repeatedly on the evolving instance).
2. **Replays the exact same scenario** through two solvers via IPAMIR:
   - **our solver** — `IntelSatSolver` (`bin/driver_ours`)
   - **reference** — `UWrMaxSat 1.4` (`bin/driver_uwrmaxsat`), a complete /
     exact MaxSAT solver used as the ground-truth oracle.
3. **Compares the result of every `solve` call** between the two solvers.
4. **Flags and saves** any disagreement, crash, or timeout.

Because UWrMaxSat is exact, its objective value is the true optimum. Our solver
is anytime, so per solve step the comparison is:

| Situation                                  | Verdict       |
|--------------------------------------------|---------------|
| `ours_obj == optimum`                      | OK            |
| `ours_obj  > optimum` (still feasible)     | SUBOPTIMAL (allowed for anytime) |
| `ours_obj  < optimum`                      | **BUG** (impossible to beat optimum) |
| UNSAT vs SAT mismatch                      | **BUG**       |
| our solver crashes / hangs / returns ERROR | **BUG**       |

The scenarios are model-independent (mutations don't depend on the specific
solution found), so both solvers always face an identical instance at each step
and their optima must match.

## Components

| File                | Purpose                                                       |
|---------------------|--------------------------------------------------------------|
| `incr_driver.cc`    | Replays a scenario file through the linked IPAMIR solver, printing one result line per `solve`. |
| `build.sh`          | (Re)builds the two solver IPAMIR libraries and compiles the two drivers. |
| `incr_fuzz.py`      | The fuzzer: generates scenarios, runs both drivers, compares, logs faults. |
| `bin/`              | The compiled drivers `driver_ours` and `driver_uwrmaxsat`.   |
| `Logs/`             | One sub-folder per run with all output (see below).          |
| `ipamir.h`          | The IPAMIR header (copied from `third_party/ipamir`).        |

## Building (do this once, and after any solver code change)

**Prerequisite:** clone `third_party/ipamir` (see `build.sh` / team setup docs).

```bash
cd ~/bisan/third_party/IncrementalFuzzer
./build.sh
```

This runs `make libr LIB=IntelSatSolver` in the repo root (static release
**library** for IPAMIR) and links it into `bin/driver_ours`. UWrMaxSat is built
the same way for `bin/driver_uwrmaxsat`.

> **Note:** The non-incremental MaxSAT-Fuzzer uses a standalone executable built
> with `make rs EXEC=intel_sat_solver_enhanced` (static release binary), not
> `make r`. See `Scripts/intel_topor_maxsat.sh`.

## Running it

```bash
cd ~/bisan/third_party/IncrementalFuzzer
./incr_fuzz.py                      # endless run, default settings
```

Useful options:

```bash
./incr_fuzz.py -n 200               # run exactly 200 scenarios then stop
./incr_fuzz.py --timeout 12         # per-solver timeout per scenario (seconds)
./incr_fuzz.py --seed 1             # reproducible base seed
./incr_fuzz.py --nuwls-time 1       # cap our NUWLS local-search to 1s/solve (default)
./incr_fuzz.py --nuwls-time 0       # skip our local-search post-solve (fastest)
./incr_fuzz.py --save-suboptimal    # also save scenarios where we were suboptimal
```

> Tip: `--nuwls-time` only affects our solver (via the `TOPOR_NUWLS_TIME_LIMIT`
> environment variable). Without it, each incremental `solve` can run a ~15s
> local search, which is too slow for fuzzing.

While running it prints a periodic status line:

```
... 60 scenarios | bugs=0 suboptimal=2 | 0.4/s
```

## Stopping it

Press **Ctrl-C**, or from another shell:

```bash
pkill -INT -f incr_fuzz
```

On stop it writes a `FaultOverview.log` summary into the run's log folder.

## Folders it creates

Each run creates one directory `Logs/<date>-<pid>-incrfuzz/` containing:

| Folder / file        | Contents                                                       |
|----------------------|----------------------------------------------------------------|
| `FaultyScenarios/`   | The scenario files that triggered a **BUG** — the key files for reproduction. |
| `FaultLogs/`         | Full log per bug: the disagreement description, the repro commands, and both solvers' raw output. |
| `Suboptimal/`        | Scenarios where our solver returned a valid but non-optimal answer (only saved with `--save-suboptimal`). |
| `FaultOverview.log`  | Short summary of the whole run (written on exit).              |

As long as there are no bugs, `FaultyScenarios/` and `FaultLogs/` stay empty.

## Reproducing a saved bug

A saved scenario can be replayed directly through either driver:

```bash
./bin/driver_uwrmaxsat Logs/<run>/FaultyScenarios/bug_xxx.scenario   # oracle
TOPOR_NUWLS_TIME_LIMIT=1 ./bin/driver_ours Logs/<run>/FaultyScenarios/bug_xxx.scenario
```

Each driver prints one line per solve: `<index> <status> <objective>`
(`status`: 10=feasible, 20=UNSAT, 30=optimum, 40=error; `objective` is `-1`
when UNSAT).

## Scenario file format

Plain text, one operation per line:

```
h L1 L2 ... 0     add a hard clause (literals, then 0)
s LIT W           declare/update soft literal LIT with weight W
a LIT             add assumption LIT for the next solve only
solve             call ipamir_solve and emit one result line
c ...             comment
```
