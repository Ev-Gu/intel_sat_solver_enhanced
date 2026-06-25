#!/usr/bin/env python3
"""
incr_fuzz.py - Incremental IPAMIR differential fuzzer.

It endlessly:
  1. generates a random *incremental* MaxSAT scenario (a deterministic list of
     IPAMIR operations: add hard clauses / declare soft literals / change
     weights / add assumptions / solve),
  2. replays the SAME scenario through two solvers via the IPAMIR API:
        - our solver           (bin/driver_ours)
        - UWrMaxSat 1.4 (ref)  (bin/driver_uwrmaxsat)   <- exact/complete oracle
  3. compares the result of every solve call,
  4. flags and saves any disagreement / crash / timeout.

Because UWrMaxSat is a complete (exact) solver, its objective is the true
optimum. Our solver is an anytime solver, so:
    - ours_obj == ref_obj  -> OK (our anytime solution is optimal)
    - ours_obj  < ref_obj  -> BUG  (impossible to beat the optimum)
    - ours_obj  > ref_obj  -> SUBOPTIMAL (allowed for anytime, recorded apart)
    - UNSAT vs SAT mismatch -> BUG
    - crash / timeout / ERROR state -> BUG

Stop it any time with Ctrl-C (or: pkill -INT -f incr_fuzz).
"""

import argparse
import os
import random
import subprocess
import sys
import time
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
DRIVER_OURS = os.path.join(HERE, "bin", "driver_ours")
DRIVER_REF = os.path.join(HERE, "bin", "driver_uwrmaxsat")


# --------------------------------------------------------------------------- #
# Scenario generation (model-independent so both solvers stay comparable)
# --------------------------------------------------------------------------- #
def gen_scenario(rng):
    """Return (scenario_text, n_solves)."""
    lines = []
    n_solves = 0

    n_vars = rng.randint(3, 12)
    weighted = rng.random() < 0.5
    soft_vars = rng.sample(range(1, n_vars + 1),
                           k=rng.randint(1, n_vars))

    def rand_lit():
        v = rng.randint(1, n_vars)
        return v if rng.random() < 0.5 else -v

    def rand_weight():
        return rng.randint(1, 1000) if weighted else 1

    def add_hard_clause():
        k = rng.randint(1, 3)
        lits = [rand_lit() for _ in range(k)]
        lines.append("h " + " ".join(str(l) for l in lits) + " 0")

    # --- initial instance ---
    for _ in range(rng.randint(1, n_vars + 2)):
        add_hard_clause()
    for v in soft_vars:
        lit = v if rng.random() < 0.7 else -v
        lines.append(f"s {lit} {rand_weight()}")

    lines.append("solve")
    n_solves += 1

    # --- incremental rounds ---
    for _ in range(rng.randint(2, 8)):
        # add hard clauses (monotonically constrains the problem)
        if rng.random() < 0.7:
            for _ in range(rng.randint(1, 2)):
                add_hard_clause()
        # change weights / declare new soft literals
        if rng.random() < 0.5:
            for _ in range(rng.randint(1, 3)):
                v = rng.randint(1, n_vars)
                lit = v if rng.random() < 0.7 else -v
                lines.append(f"s {lit} {rand_weight()}")
        # assumptions for this solve only
        if rng.random() < 0.5:
            for _ in range(rng.randint(1, 3)):
                lines.append(f"a {rand_lit()}")
        lines.append("solve")
        n_solves += 1

    return "\n".join(lines) + "\n", n_solves


# --------------------------------------------------------------------------- #
# Running a driver on a scenario
# --------------------------------------------------------------------------- #
class RunResult:
    def __init__(self):
        self.returncode = None
        self.timed_out = False
        self.solves = []        # list of (idx, status, obj)
        self.raw = ""


def run_driver(driver, scenario_path, timeout, extra_env=None):
    rr = RunResult()
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    try:
        p = subprocess.run([driver, scenario_path],
                           capture_output=True, text=True, timeout=timeout,
                           env=env)
        rr.returncode = p.returncode
        rr.raw = p.stdout + ("\n[stderr]\n" + p.stderr if p.stderr else "")
        for ln in p.stdout.splitlines():
            parts = ln.split()
            if len(parts) == 3:
                try:
                    rr.solves.append((int(parts[0]), int(parts[1]), int(parts[2])))
                except ValueError:
                    pass
    except subprocess.TimeoutExpired as e:
        rr.timed_out = True
        rr.raw = (e.stdout or "") if isinstance(e.stdout, str) else ""
    return rr


# --------------------------------------------------------------------------- #
# Comparison
# --------------------------------------------------------------------------- #
def is_sat(status):
    return status in (10, 30)


def compare(ref, ours):
    """Return (category, detail). category in {OK, BUG, SUBOPTIMAL}."""
    # crashes / timeouts
    if ours.timed_out:
        return "BUG", "ours TIMEOUT"
    if ref.timed_out:
        return "OK", "ref TIMEOUT (skipped)"      # oracle too slow: not our bug
    if ours.returncode is None or ours.returncode != 0:
        return "BUG", f"ours crashed (exit={ours.returncode})"
    if ref.returncode is None or ref.returncode != 0:
        return "OK", f"ref crashed (exit={ref.returncode}) (skipped)"

    if len(ours.solves) != len(ref.solves):
        return "BUG", (f"solve-count mismatch: ref={len(ref.solves)} "
                       f"ours={len(ours.solves)}")

    worst = "OK"
    detail = ""
    for (ri, rs, ro), (oi, os_, oo) in zip(ref.solves, ours.solves):
        if os_ == 40:
            return "BUG", f"step {oi}: ours returned ERROR (40)"
        if os_ == 0:
            return "BUG", f"step {oi}: ours returned 0 (interrupted unexpectedly)"
        r_sat, o_sat = is_sat(rs), is_sat(os_)
        if r_sat != o_sat:
            return "BUG", (f"step {oi}: feasibility mismatch "
                           f"ref={'SAT' if r_sat else 'UNSAT'} "
                           f"ours={'SAT' if o_sat else 'UNSAT'}")
        if r_sat and o_sat:
            if oo < ro:
                return "BUG", (f"step {oi}: ours obj {oo} < optimum {ro} "
                               f"(better than optimum is impossible)")
            if oo > ro:
                worst = "SUBOPTIMAL"
                detail = f"step {oi}: ours obj {oo} > optimum {ro}"
    return worst, detail


# --------------------------------------------------------------------------- #
# Main loop
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(description="Incremental IPAMIR differential fuzzer")
    ap.add_argument("-n", "--num", type=int, default=0,
                    help="number of scenarios to run (0 = endless)")
    ap.add_argument("--timeout", type=float, default=20.0,
                    help="per-solver timeout in seconds (default 20)")
    ap.add_argument("--seed", type=int, default=None,
                    help="base RNG seed (default: random)")
    ap.add_argument("--save-suboptimal", action="store_true",
                    help="also save scenarios where our solver was suboptimal")
    ap.add_argument("--nuwls-time", type=int, default=1,
                    help="cap our solver's NUWLS local-search budget per solve, "
                         "in seconds (0 = skip local search). Default 1.")
    args = ap.parse_args()

    ours_env = {"TOPOR_NUWLS_TIME_LIMIT": str(args.nuwls_time)}

    for d in (DRIVER_OURS, DRIVER_REF):
        if not os.path.isfile(d):
            sys.exit(f"ERROR: missing driver {d}\n  -> run ./build.sh first")

    base_seed = args.seed if args.seed is not None else random.randrange(2**31)
    stamp = datetime.now().strftime("%Y-%m-%d-%H%M%S")
    run_dir = os.path.join(HERE, "Logs", f"{stamp}-{os.getpid()}-incrfuzz")
    faults_dir = os.path.join(run_dir, "FaultyScenarios")
    faultlogs_dir = os.path.join(run_dir, "FaultLogs")
    subopt_dir = os.path.join(run_dir, "Suboptimal")
    overview = os.path.join(run_dir, "FaultOverview.log")
    for d in (faults_dir, faultlogs_dir, subopt_dir):
        os.makedirs(d, exist_ok=True)

    scen_path = os.path.join(run_dir, "current.scenario")

    n_run = n_bugs = n_subopt = 0
    t0 = time.time()
    print(f"Incremental IPAMIR fuzzer | base_seed={base_seed} | logs: {run_dir}")
    print("Ctrl-C to stop.\n")

    try:
        i = 0
        while args.num == 0 or i < args.num:
            seed = base_seed + i
            rng = random.Random(seed)
            scenario, n_solves = gen_scenario(rng)
            with open(scen_path, "w") as f:
                f.write(f"c seed={seed} solves={n_solves}\n")
                f.write(scenario)

            ref = run_driver(DRIVER_REF, scen_path, args.timeout)
            ours = run_driver(DRIVER_OURS, scen_path, args.timeout, extra_env=ours_env)
            cat, detail = compare(ref, ours)
            n_run += 1

            if cat == "BUG":
                n_bugs += 1
                base = f"bug_{stamp}_seed{seed}"
                with open(os.path.join(faults_dir, base + ".scenario"), "w") as f:
                    f.write(f"c seed={seed}\nc {detail}\n{scenario}")
                with open(os.path.join(faultlogs_dir, base + ".log"), "w") as f:
                    f.write(f"BUG: {detail}\nseed={seed}\n\n")
                    f.write(f"repro:\n  {DRIVER_REF} <scenario>\n  {DRIVER_OURS} <scenario>\n\n")
                    f.write("=== reference (UWrMaxSat) ===\n" + ref.raw + "\n")
                    f.write("=== ours (IntelSatSolver) ===\n" + ours.raw + "\n")
                print(f"  [BUG #{n_bugs}] seed={seed}: {detail}")
            elif cat == "SUBOPTIMAL":
                n_subopt += 1
                if args.save_suboptimal:
                    base = f"subopt_{stamp}_seed{seed}"
                    with open(os.path.join(subopt_dir, base + ".scenario"), "w") as f:
                        f.write(f"c seed={seed}\nc {detail}\n{scenario}")

            if n_run % 20 == 0:
                el = time.time() - t0
                rate = n_run / el if el > 0 else 0
                print(f"  ... {n_run} scenarios | bugs={n_bugs} "
                      f"suboptimal={n_subopt} | {rate:.1f}/s")
            i += 1
    except KeyboardInterrupt:
        print("\nInterrupted.")

    el = time.time() - t0
    summary = (
        "=== Incremental IPAMIR Fuzzer - Overview ===\n"
        f"base_seed         : {base_seed}\n"
        f"scenarios run     : {n_run}\n"
        f"BUGS found        : {n_bugs}\n"
        f"suboptimal (ours) : {n_subopt}\n"
        f"runtime (s)       : {el:.1f}\n"
        f"faulty scenarios  : {faults_dir}\n"
    )
    with open(overview, "w") as f:
        f.write(summary)
    print("\n" + summary)


if __name__ == "__main__":
    main()
