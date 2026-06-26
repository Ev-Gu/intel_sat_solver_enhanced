#!/usr/bin/env python3
"""
wcnf_ipamir_fuzz.py — Fuzzer 2: differential IPAMIR testing on WCNF files.

Uses the same random WCNF generator as Fuzzer 1 (wcnfuzz), then feeds each
instance to two IPAMIR loaders (Yevgeny's WCNF→IPAMIR approach):

  - our solver      (tools/bin/ipamir_wcnf_ours)
  - UWrMaxSat 1.4   (tools/bin/ipamir_wcnf_uwrmaxsat)  exact oracle

Compares status (SAT/UNSAT) and objective after each solve in the file.

Stop with Ctrl-C (or: pkill -INT -f wcnf_ipamir_fuzz).
"""

import argparse
import os
import random
import subprocess
import sys
import time
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(HERE)
WCNFUZZ = os.path.join(REPO_ROOT, "third_party", "MaxSAT-Fuzzer", "Fuzzer", "wcnfuzz", "wcnfuzz")
DRIVER_OURS = os.path.join(HERE, "bin", "ipamir_wcnf_ours")
DRIVER_REF = os.path.join(HERE, "bin", "ipamir_wcnf_uwrmaxsat")


class RunResult:
    def __init__(self):
        self.returncode = None
        self.timed_out = False
        self.results = []  # list of (status_line, obj or None)
        self.raw = ""


def parse_mse_output(raw):
    """Return list of (s-line text, objective or None) per solve."""
    out = []
    pending_o = None
    for ln in raw.splitlines():
        ln = ln.strip()
        if ln.startswith("o "):
            try:
                pending_o = int(ln.split()[1])
            except (IndexError, ValueError):
                pending_o = None
        elif ln.startswith("s "):
            out.append((ln[2:].strip(), pending_o))
            pending_o = None
    return out


def is_sat_status(status):
    s = status.upper()
    return "UNSAT" not in s and s not in ("UNKNOWN", "ERROR", "")


def run_driver(driver, wcnf_path, timeout, extra_env=None):
    rr = RunResult()
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)
    try:
        p = subprocess.run(
            [driver, wcnf_path],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env,
        )
        rr.returncode = p.returncode
        rr.raw = p.stdout + ("\n[stderr]\n" + p.stderr if p.stderr else "")
        rr.results = parse_mse_output(rr.raw)
    except subprocess.TimeoutExpired as e:
        rr.timed_out = True
        rr.raw = (e.stdout or "") if isinstance(e.stdout, str) else ""
        rr.results = parse_mse_output(rr.raw)
    return rr


def compare(ref, ours):
    """Return (category, detail). category in {OK, BUG, SUBOPTIMAL}."""
    if ours.timed_out:
        return "BUG", "ours TIMEOUT"
    if ref.timed_out:
        return "OK", "ref TIMEOUT (skipped)"
    if ours.returncode not in (0, None):
        return "BUG", f"ours crashed (exit={ours.returncode})"
    if ref.returncode not in (0, None):
        return "OK", f"ref crashed (exit={ref.returncode}) (skipped)"

    if not ref.results and not ours.results:
        return "BUG", "no s-line output from either solver"
    if len(ours.results) != len(ref.results):
        return "BUG", (
            f"solve-count mismatch: ref={len(ref.results)} ours={len(ours.results)}"
        )

    worst = "OK"
    detail = ""
    for i, ((rs, ro), (os_, oo)) in enumerate(zip(ref.results, ours.results)):
        r_sat, o_sat = is_sat_status(rs), is_sat_status(os_)
        if r_sat != o_sat:
            return "BUG", (
                f"solve {i}: feasibility mismatch "
                f"ref={'SAT' if r_sat else 'UNSAT'} ({rs}) "
                f"ours={'SAT' if o_sat else 'UNSAT'} ({os_})"
            )
        if r_sat and o_sat and ro is not None and oo is not None:
            if oo < ro:
                return "BUG", (
                    f"solve {i}: ours obj {oo} < optimum {ro} (impossible)"
                )
            if oo > ro:
                worst = "SUBOPTIMAL"
                detail = f"solve {i}: ours obj {oo} > optimum {ro}"
    return worst, detail


def generate_wcnf(wcnfuzz_bin, seed, out_path):
    p = subprocess.run(
        [wcnfuzz_bin, "--wcnf", str(seed)],
        capture_output=True,
        text=True,
    )
    if p.returncode != 0:
        raise RuntimeError(f"wcnfuzz failed: {p.stderr[:500]}")
    body = p.stdout
    if not body.strip():
        raise RuntimeError("wcnfuzz produced empty output")
    with open(out_path, "w") as f:
        f.write(f"c seed={seed}\n")
        f.write(body)
        if not body.endswith("\n"):
            f.write("\n")


def main():
    ap = argparse.ArgumentParser(
        description="WCNF IPAMIR differential fuzzer (ours vs UWrMaxSat)"
    )
    ap.add_argument("-n", "--num", type=int, default=0, help="instances (0=endless)")
    ap.add_argument("--timeout", type=float, default=60.0, help="per-solver seconds")
    ap.add_argument("--seed", type=int, default=None, help="base RNG seed")
    ap.add_argument(
        "--wcnfuzz",
        default=WCNFUZZ,
        help="path to wcnfuzz binary",
    )
    ap.add_argument(
        "--nuwls-time",
        type=int,
        default=1,
        help="TOPOR_NUWLS_TIME_LIMIT for our solver (0=skip NUWLS)",
    )
    ap.add_argument("--save-suboptimal", action="store_true")
    args = ap.parse_args()

    if not os.path.isfile(args.wcnfuzz):
        sys.exit(
            f"ERROR: missing {args.wcnfuzz}\n"
            "  -> make -C third_party/MaxSAT-Fuzzer/Fuzzer/wcnfuzz"
        )
    wcnfuzz_bin = args.wcnfuzz

    for d in (DRIVER_OURS, DRIVER_REF):
        if not os.path.isfile(d):
            sys.exit(f"ERROR: missing {d}\n  -> bash tools/build_ipamir_wcnf.sh")

    ours_env = {
        "TOPOR_NUWLS_TIME_LIMIT": str(args.nuwls_time),
        # Solver-internal budget (seconds); stop gracefully before subprocess kill.
        "TOPOR_IPAMIR_TIME_LIMIT": str(max(1, int(args.timeout) - 1)),
        "TOPOR_IPAMIR_VERBOSE": "1",
    }

    base_seed = args.seed if args.seed is not None else random.randrange(2**31)
    stamp = datetime.now().strftime("%Y-%m-%d-%H%M%S")
    run_dir = os.path.join(HERE, "Logs", f"{stamp}-{os.getpid()}-wcnfipamirfuzz")
    faults_dir = os.path.join(run_dir, "FaultyWCNFs")
    faultlogs_dir = os.path.join(run_dir, "FaultLogs")
    subopt_dir = os.path.join(run_dir, "Suboptimal")
    overview = os.path.join(run_dir, "FaultOverview.log")
    for d in (faults_dir, faultlogs_dir, subopt_dir):
        os.makedirs(d, exist_ok=True)

    wcnf_path = os.path.join(run_dir, "current.wcnf")

    n_run = n_bugs = n_subopt = n_skip = 0
    t0 = time.time()
    print(
        f"WCNF IPAMIR fuzzer (ours vs UWrMaxSat) | base_seed={base_seed} | logs: {run_dir}",
        flush=True,
    )
    print("Ctrl-C to stop.\n", flush=True)

    try:
        i = 0
        while args.num == 0 or i < args.num:
            seed = base_seed + i
            try:
                generate_wcnf(wcnfuzz_bin, seed, wcnf_path)
            except RuntimeError as e:
                print(f"  [skip] seed={seed}: {e}", flush=True)
                n_skip += 1
                i += 1
                continue

            ref = run_driver(DRIVER_REF, wcnf_path, args.timeout)
            ours = run_driver(DRIVER_OURS, wcnf_path, args.timeout, extra_env=ours_env)
            cat, detail = compare(ref, ours)
            n_run += 1

            if cat == "BUG":
                n_bugs += 1
                base = f"bug_{stamp}_seed{seed}"
                with open(os.path.join(faults_dir, base + ".wcnf"), "w") as f:
                    with open(wcnf_path) as src:
                        f.write(src.read())
                with open(os.path.join(faultlogs_dir, base + ".log"), "w") as f:
                    f.write(f"BUG: {detail}\nseed={seed}\n\n")
                    f.write(f"repro:\n  {DRIVER_REF} {base}.wcnf\n  {DRIVER_OURS} {base}.wcnf\n\n")
                    f.write("=== reference (UWrMaxSat) ===\n" + ref.raw + "\n")
                    f.write("=== ours (IntelSatSolver) ===\n" + ours.raw + "\n")
                print(f"  [BUG #{n_bugs}] seed={seed}: {detail}", flush=True)
            elif cat == "SUBOPTIMAL":
                n_subopt += 1
                if args.save_suboptimal:
                    base = f"subopt_{stamp}_seed{seed}"
                    with open(os.path.join(subopt_dir, base + ".wcnf"), "w") as f:
                        with open(wcnf_path) as src:
                            f.write(src.read())

            if n_run % 10 == 0:
                el = time.time() - t0
                rate = n_run / el if el > 0 else 0
                print(
                    f"  ... {n_run} instances | bugs={n_bugs} "
                    f"suboptimal={n_subopt} skipped={n_skip} | {rate:.2f}/s",
                    flush=True,
                )
            i += 1
    except KeyboardInterrupt:
        print("\nInterrupted.", flush=True)

    el = time.time() - t0
    summary = (
        "=== WCNF IPAMIR Fuzzer - Overview ===\n"
        f"base_seed         : {base_seed}\n"
        f"instances run     : {n_run}\n"
        f"skipped (gen fail): {n_skip}\n"
        f"BUGS found        : {n_bugs}\n"
        f"suboptimal (ours) : {n_subopt}\n"
        f"runtime (s)       : {el:.1f}\n"
        f"faulty WCNFs      : {faults_dir}\n"
    )
    with open(overview, "w") as f:
        f.write(summary)
    print("\n" + summary, flush=True)


if __name__ == "__main__":
    main()
