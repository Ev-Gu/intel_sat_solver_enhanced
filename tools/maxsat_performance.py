#!/usr/bin/env python3
"""
Compare IntelTopor vs reference solvers on MSE regression WCNFs.

Batch path: IntelTopor vs EvalMaxSAT 2022.
IPAMIR path: ipamir_wcnf_ours vs ipamir_wcnf_uwrmaxsat (MSE2022 incremental oracle).

Designed for poster / evaluation summaries (subset + configurable timeout).
"""

import argparse
import csv
import os
import re
import subprocess
import sys
import time
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SUITE = os.path.join(REPO, "third_party", "MaxSATRegressionSuite")
TOPOR_BATCH = os.path.join(REPO, "third_party", "MaxSAT-Fuzzer", "Scripts", "intel_topor_maxsat.sh")
EVALMAXSAT = os.path.join(
    REPO, "third_party", "MaxSAT-Fuzzer", "MaxSATSolver", "MSE22", "EvalMaxSAT", "build", "EvalMaxSAT_bin"
)
IPAMIR_OURS = os.path.join(HERE, "bin", "ipamir_wcnf_ours")
IPAMIR_UWR = os.path.join(HERE, "bin", "ipamir_wcnf_uwrmaxsat")
SEARCH_DIRS = ("MSE22Unique", "MSE23Unique", "MSE22Big", "MSE23Big")


def resolve_wcnf(name):
    if os.path.sep in name:
        path = os.path.join(SUITE, name)
        if os.path.isfile(path):
            return path
    for sub in SEARCH_DIRS:
        path = os.path.join(SUITE, sub, name)
        if os.path.isfile(path):
            return path
    return None


def load_csv_instances(csv_path, max_instances):
    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(
            (ln for ln in f if not ln.startswith("c ") and ln.strip()),
            skipinitialspace=True,
        )
        for row in reader:
            name = row.get("WCNFFile", "").strip()
            if not name:
                continue
            path = resolve_wcnf(name)
            if path is None:
                continue
            rows.append(
                {
                    "file": name,
                    "path": path,
                    "best_o": row.get("BestOValue", "").strip() or None,
                    "certified": (row.get("CertifiedResult", "").strip() or "").upper() == "YES",
                }
            )
            if max_instances and len(rows) >= max_instances:
                break
    return rows


def parse_output(raw):
    last_o = None
    last_s = None
    for ln in raw.splitlines():
        ln = ln.strip()
        if ln.startswith("o "):
            try:
                last_o = int(ln.split()[1])
            except (IndexError, ValueError):
                pass
        elif ln.startswith("s "):
            last_s = ln[2:].strip()
    return last_s, last_o


def run_cmd(cmd, wcnf_path, timeout, env=None):
    t0 = time.monotonic()
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    try:
        p = subprocess.run(
            cmd + [wcnf_path],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=merged_env,
        )
        elapsed = time.monotonic() - t0
        raw = p.stdout + ("\n" + p.stderr if p.stderr else "")
        status, obj = parse_output(raw)
        return {
            "time": elapsed,
            "exit": p.returncode,
            "status": status,
            "obj": obj,
            "timeout": False,
            "raw_tail": "\n".join(raw.splitlines()[-8:]),
        }
    except subprocess.TimeoutExpired as e:
        elapsed = time.monotonic() - t0
        raw = (e.stdout or "") if isinstance(e.stdout, str) else ""
        status, obj = parse_output(raw)
        return {
            "time": elapsed,
            "exit": -1,
            "status": status or "TIMEOUT",
            "obj": obj,
            "timeout": True,
            "raw_tail": "",
        }


def status_ok(s):
    if not s:
        return False
    u = s.upper()
    return "OPTIMUM" in u or u == "SATISFIABLE"


def fmt_obj(o):
    return "" if o is None else str(o)


def write_summary(out_path, meta, rows):
    def solved(r, key):
        return status_ok(r[key]["status"])

    n = len(rows)
    batch_ours = sum(1 for r in rows if solved(r, "batch_ours"))
    batch_ref = sum(1 for r in rows if solved(r, "batch_ref"))
    ip_ours = sum(1 for r in rows if solved(r, "ipamir_ours"))
    ip_ref = sum(1 for r in rows if solved(r, "ipamir_ref"))

    def avg_time(key, pred):
        times = [r[key]["time"] for r in rows if pred(r, key)]
        return sum(times) / len(times) if times else 0.0

    def parity(a_key, b_key):
        ok = 0
        for r in rows:
            if not solved(r, a_key) or not solved(r, b_key):
                continue
            if r[a_key]["obj"] == r[b_key]["obj"]:
                ok += 1
        return ok

    lines = [
        "# MaxSAT Performance Summary",
        "",
        f"Generated: {meta['timestamp']}",
        f"Instances: {n} from `{meta['csv']}`",
        f"Timeout: {meta['timeout']}s per solver run",
        f"IPAMIR internal limit: TOPOR_IPAMIR_TIME_LIMIT={meta['ipamir_limit']}",
        "",
        "## Batch (WCNF, non-incremental)",
        "",
        "| Solver | Solved | Avg time (solved) |",
        "|--------|--------|-------------------|",
        f"| IntelTopor | {batch_ours}/{n} | {avg_time('batch_ours', solved):.2f}s |",
        f"| EvalMaxSAT 2022 | {batch_ref}/{n} | {avg_time('batch_ref', solved):.2f}s |",
        "",
        "## IPAMIR (WCNF loader, incremental API)",
        "",
        "| Solver | Solved | Avg time (solved) |",
        "|--------|--------|-------------------|",
        f"| IntelTopor IPAMIR | {ip_ours}/{n} | {avg_time('ipamir_ours', solved):.2f}s |",
        f"| UWrMaxSat 1.4 IPAMIR | {ip_ref}/{n} | {avg_time('ipamir_ref', solved):.2f}s |",
        "",
        "## Correctness vs references",
        "",
        f"- Batch objective match (both solved): {parity('batch_ours', 'batch_ref')} instances",
        f"- IPAMIR objective match (both solved): {parity('ipamir_ours', 'ipamir_ref')} instances",
        "",
        "## Notes for poster (Lisa / MSE2022 incremental track)",
        "",
        "- Regression subset: MSE22+23 unique instances (certified where noted in CSV).",
        "- Full MSE2022 incremental *applications* (IPAMIR repo `app/`) need separate driver builds per Yam.",
        "- UWrMaxSat 1.4 IPAMIR is the exact oracle used in Fuzzer 2; EvalMaxSAT is batch reference.",
        "- Competition uses 7200s; this run uses a shorter timeout for a practical subset benchmark.",
        "",
    ]
    with open(out_path, "w") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description="MaxSAT performance comparison on regression WCNFs")
    ap.add_argument("--csv", default=os.path.join(SUITE, "MSE22Unique.csv"))
    ap.add_argument("--max-instances", type=int, default=40)
    ap.add_argument("--timeout", type=int, default=30)
    ap.add_argument("--skip-batch", action="store_true")
    ap.add_argument("--skip-ipamir", action="store_true")
    ap.add_argument("--out-dir", default=os.path.join(REPO, "results"))
    args = ap.parse_args()

    checks = []
    if not args.skip_batch:
        checks += [(TOPOR_BATCH, "IntelTopor batch"), (EVALMAXSAT, "EvalMaxSAT")]
    if not args.skip_ipamir:
        checks += [(IPAMIR_OURS, "ipamir_wcnf_ours"), (IPAMIR_UWR, "ipamir_wcnf_uwrmaxsat")]
    for path, label in checks:
        if not os.path.isfile(path):
            print(f"Missing: {path} ({label})", file=sys.stderr)
            return 1
        if not path.endswith(".sh") and not os.access(path, os.X_OK):
            print(f"Not executable: {path} ({label})", file=sys.stderr)
            return 1

    instances = load_csv_instances(args.csv, args.max_instances)
    if not instances:
        print("No instances found. Run: cd third_party/MaxSATRegressionSuite && ./install", file=sys.stderr)
        return 1

    os.makedirs(args.out_dir, exist_ok=True)
    stamp = datetime.utcnow().strftime("%Y%m%d_%H%M%S")
    csv_out = os.path.join(args.out_dir, f"performance_{stamp}.csv")
    md_out = os.path.join(args.out_dir, "PERFORMANCE_SUMMARY.md")

    ipamir_limit = max(1, args.timeout - 1)
    ipamir_env = {
        "TOPOR_IPAMIR_TIME_LIMIT": str(ipamir_limit),
        "TOPOR_IPAMIR_VERBOSE": "0",
    }

    fieldnames = [
        "file",
        "best_o",
        "certified",
        "batch_ours_time",
        "batch_ours_status",
        "batch_ours_obj",
        "batch_ref_time",
        "batch_ref_status",
        "batch_ref_obj",
        "ipamir_ours_time",
        "ipamir_ours_status",
        "ipamir_ours_obj",
        "ipamir_ref_time",
        "ipamir_ref_status",
        "ipamir_ref_obj",
    ]

    results = []
    for i, inst in enumerate(instances, 1):
        print(f"[{i}/{len(instances)}] {inst['file']}", flush=True)
        row = {
            "file": inst["file"],
            "best_o": inst["best_o"] or "",
            "certified": inst["certified"],
            "batch_ours": {},
            "batch_ref": {},
            "ipamir_ours": {},
            "ipamir_ref": {},
        }
        if not args.skip_batch:
            row["batch_ours"] = run_cmd(["bash", TOPOR_BATCH], inst["path"], args.timeout)
            row["batch_ref"] = run_cmd([EVALMAXSAT], inst["path"], args.timeout)
        if not args.skip_ipamir:
            row["ipamir_ours"] = run_cmd([IPAMIR_OURS], inst["path"], args.timeout + 5, ipamir_env)
            row["ipamir_ref"] = run_cmd([IPAMIR_UWR], inst["path"], args.timeout + 5)

        flat = {
            "file": row["file"],
            "best_o": row["best_o"],
            "certified": row["certified"],
        }
        for prefix in ("batch_ours", "batch_ref", "ipamir_ours", "ipamir_ref"):
            r = row[prefix]
            flat[f"{prefix}_time"] = f"{r.get('time', 0):.3f}" if r else ""
            flat[f"{prefix}_status"] = r.get("status", "") if r else ""
            flat[f"{prefix}_obj"] = fmt_obj(r.get("obj")) if r else ""
        results.append(row)
        with open(csv_out, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fieldnames)
            w.writeheader()
            for rr in results:
                w.writerow(
                    {
                        "file": rr["file"],
                        "best_o": rr["best_o"],
                        "certified": rr["certified"],
                        **{
                            f"{p}_time": f"{rr[p].get('time', 0):.3f}" if rr[p] else ""
                            for p in ("batch_ours", "batch_ref", "ipamir_ours", "ipamir_ref")
                        },
                        **{
                            f"{p}_status": rr[p].get("status", "") if rr[p] else ""
                            for p in ("batch_ours", "batch_ref", "ipamir_ours", "ipamir_ref")
                        },
                        **{
                            f"{p}_obj": fmt_obj(rr[p].get("obj")) if rr[p] else ""
                            for p in ("batch_ours", "batch_ref", "ipamir_ours", "ipamir_ref")
                        },
                    }
                )

    meta = {
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "csv": os.path.relpath(args.csv, REPO),
        "timeout": args.timeout,
        "ipamir_limit": ipamir_limit,
    }
    write_summary(md_out, meta, results)
    print(f"\nWrote {csv_out}\nWrote {md_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
