#!/usr/bin/env python3
"""
Yam-style benchmark: run reference solver first, keep instances it solves,
then compare IntelTopor on that filtered set only.

Modes:
  ref-first-wcnf  — MSE regression WCNFs, EvalMaxSAT filters, IntelTopor batch + IPAMIR
  ipamir-app      — MSE2022 incremental app (ipamirapp), UWrMaxSat filters, IntelSatSolver
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SUITE = os.path.join(REPO, "third_party", "MaxSATRegressionSuite")
IPAMIR = os.path.join(REPO, "third_party", "ipamir")
EVAL = os.path.join(
    REPO, "third_party", "MaxSAT-Fuzzer", "MaxSATSolver", "MSE22", "EvalMaxSAT", "build", "EvalMaxSAT_bin"
)
TOPOR_BATCH = os.path.join(REPO, "third_party", "MaxSAT-Fuzzer", "Scripts", "intel_topor_maxsat.sh")
IPAMIR_OURS = os.path.join(HERE, "bin", "ipamir_wcnf_ours")
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


def run_cmd(cmd, arg, timeout, env=None):
    t0 = time.monotonic()
    merged = os.environ.copy()
    if env:
        merged.update(env)
    try:
        p = subprocess.run(
            cmd + [arg],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=merged,
        )
        elapsed = time.monotonic() - t0
        raw = p.stdout + ("\n" + p.stderr if p.stderr else "")
        status, obj = parse_output(raw)
        ok = status and ("OPTIMUM" in status.upper() or status.upper() == "SATISFIABLE")
        ok = ok and p.returncode in (0, 10, 30)
        return {
            "time": elapsed,
            "exit": p.returncode,
            "status": status,
            "obj": obj,
            "ok": ok,
            "timeout": False,
        }
    except subprocess.TimeoutExpired:
        return {
            "time": timeout,
            "exit": -1,
            "status": "TIMEOUT",
            "obj": None,
            "ok": False,
            "timeout": True,
        }


def write_summary(path, title, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(f"# {title}\n\n")
        f.write(f"Generated: {datetime.now(timezone.utc).isoformat()}\n\n")
        f.write("\n".join(lines))
        f.write("\n")


def checkpoint_path(out_dir):
    return os.path.join(out_dir, "ref_first_checkpoint.json")


def save_checkpoint(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, path)


def load_checkpoint(path):
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return json.load(f)


def summarize_ref_first(instances, filtered, results, timeout, csv_path, out_dir):
    batch_ok = sum(1 for r in results if r["batch"]["ok"])
    ip_ok = sum(1 for r in results if r["ipamir"]["ok"])
    batch_cert = sum(
        1
        for r in results
        if r["batch"]["ok"] and r["best_o"] and str(r["batch"]["obj"]) == r["best_o"]
    )
    ip_cert = sum(
        1
        for r in results
        if r["ipamir"]["ok"] and r["best_o"] and str(r["ipamir"]["obj"]) == r["best_o"]
    )
    batch_times = [r["batch"]["time"] for r in results if r["batch"]["ok"]]
    ip_times = [r["ipamir"]["time"] for r in results if r["ipamir"]["ok"]]
    ref_times = [r["ref"]["time"] for r in results]

    lines = [
        f"**Mode:** ref-first WCNF (Yam: run what reference solved first)",
        f"**CSV:** `{os.path.relpath(csv_path, REPO)}`",
        f"**Timeout:** {timeout}s",
        f"**Phase 2 completed:** {len(results)}/{len(filtered)} instances",
        "",
        "## Phase 1 — EvalMaxSAT filter",
        "",
        f"| Scanned | Passed filter |",
        f"|---------|---------------|",
        f"| {len(instances)} | {len(filtered)} |",
        "",
        "## Phase 2 — IntelTopor on filtered set only",
        "",
        "| Path | Solved | Matches certified optimum | Avg time (solved) |",
        "|------|--------|---------------------------|-------------------|",
        f"| Batch (-M 1) | {batch_ok}/{len(results)} | {batch_cert}/{len(results)} | "
        f"{(sum(batch_times)/len(batch_times) if batch_times else 0):.2f}s |",
        f"| IPAMIR | {ip_ok}/{len(results)} | {ip_cert}/{len(results)} | "
        f"{(sum(ip_times)/len(ip_times) if ip_times else 0):.2f}s |",
        f"| EvalMaxSAT (phase 1) | {len(results)}/{len(results)} | — | "
        f"{(sum(ref_times)/len(ref_times) if ref_times else 0):.2f}s |",
        "",
        "## Interpretation",
        "",
        "- Only instances **EvalMaxSAT solved within timeout** are compared (fair subset).",
        "- Official MSE uses 7200s; this run uses a dev timeout for practical iteration.",
    ]
    out_md = os.path.join(out_dir, "MSE2022_REF_FIRST_WCNF.md")
    write_summary(out_md, "MSE Ref-First Benchmark (WCNF)", lines)
    return out_md


def cmd_ref_first_wcnf(args):
    for p in (EVAL, TOPOR_BATCH, IPAMIR_OURS):
        if not os.path.isfile(p):
            print(f"Missing: {p}", file=sys.stderr)
            return 1

    ckpt_file = checkpoint_path(args.out_dir)
    ckpt = load_checkpoint(ckpt_file) if args.resume else None

    if ckpt and ckpt.get("csv") == args.csv and ckpt.get("timeout") == args.timeout:
        instances = ckpt["instances"]
        filtered = ckpt["filtered"]
        results = ckpt.get("results", [])
        done_files = {r["file"] for r in results}
        print(f"Resuming: phase1 {len(filtered)}/{len(instances)}, phase2 {len(results)}/{len(filtered)} done", flush=True)
    else:
        if args.resume and ckpt:
            print("Checkpoint mismatch (csv/timeout changed); starting fresh.", flush=True)
        instances = load_csv_instances(args.csv, args.max_instances)
        if not instances:
            print("No instances.", file=sys.stderr)
            return 1

        print(f"Phase 1: EvalMaxSAT filter ({len(instances)} instances, {args.timeout}s)...", flush=True)
        filtered = []
        for i, inst in enumerate(instances, 1):
            r = run_cmd([EVAL], inst["path"], args.timeout)
            if r["ok"]:
                filtered.append({**inst, "ref": r})
            if i % 20 == 0:
                print(f"  ... {i}/{len(instances)} scanned, {len(filtered)} passed filter", flush=True)
        print(f"Phase 1 done: {len(filtered)}/{len(instances)} solved by EvalMaxSAT\n", flush=True)
        results = []
        done_files = set()
        save_checkpoint(
            ckpt_file,
            {
                "csv": args.csv,
                "timeout": args.timeout,
                "instances": instances,
                "filtered": filtered,
                "results": results,
            },
        )

    ip_env = {
        "TOPOR_IPAMIR_TIME_LIMIT": str(max(1, args.timeout - 1)),
        "TOPOR_IPAMIR_VERBOSE": "0",
    }
    pending = [inst for inst in filtered if inst["file"] not in done_files]
    for i, inst in enumerate(pending, len(results) + 1):
        print(f"Phase 2 [{i}/{len(filtered)}] {inst['file']}", flush=True)
        batch = run_cmd(["bash", TOPOR_BATCH], inst["path"], args.timeout)
        ipamir = run_cmd([IPAMIR_OURS], inst["path"], args.timeout + 5, ip_env)
        results.append({**inst, "batch": batch, "ipamir": ipamir})
        save_checkpoint(
            ckpt_file,
            {
                "csv": args.csv,
                "timeout": args.timeout,
                "instances": instances,
                "filtered": filtered,
                "results": results,
            },
        )

    out_md = summarize_ref_first(instances, filtered, results, args.timeout, args.csv, args.out_dir)
    print(f"\nWrote {out_md}")
    if os.path.isfile(ckpt_file):
        os.remove(ckpt_file)
    return 0


def build_ipamir_app(app_dir, solver):
    app_name = os.path.basename(app_dir.rstrip("/"))
    subprocess.run(["make", "clean"], cwd=app_dir, env={**os.environ, "IPAMIRSOLVER": solver},
                   capture_output=True)
    p = subprocess.run(["make"], cwd=app_dir, env={**os.environ, "IPAMIRSOLVER": solver},
                       capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError(f"build failed {app_name}/{solver}:\n{p.stderr[-500:]}")
    return os.path.join(app_dir, app_name)


def run_app(binary, input_path, timeout, env=None):
    t0 = time.monotonic()
    merged = os.environ.copy()
    if env:
        merged.update(env)
    try:
        p = subprocess.run(
            [binary, input_path],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=merged,
        )
        elapsed = time.monotonic() - t0
        ok = p.returncode == 0 and "ERROR" not in p.stdout
        return {"time": elapsed, "exit": p.returncode, "ok": ok, "timeout": False,
                "lines": len(p.stdout.splitlines())}
    except subprocess.TimeoutExpired:
        return {"time": timeout, "exit": -1, "ok": False, "timeout": True, "lines": 0}


def cmd_ipamir_app(args):
    app_dir = os.path.join(IPAMIR, "app", args.app)
    inputs_dir = os.path.join(app_dir, "inputs")
    if not os.path.isdir(inputs_dir):
        print(f"No inputs/: {inputs_dir}", file=sys.stderr)
        return 1

    inputs = sorted(
        os.path.join(inputs_dir, f)
        for f in os.listdir(inputs_dir)
        if os.path.isfile(os.path.join(inputs_dir, f))
    )
    if args.max_instances:
        inputs = inputs[: args.max_instances]

    print(f"Building {args.app} with uwrmaxsat14...", flush=True)
    ref_bin = build_ipamir_app(app_dir, "uwrmaxsat14")
    print(f"Building {args.app} with IntelSatSolver...", flush=True)
    ours_bin = build_ipamir_app(app_dir, "IntelSatSolver")

    ip_env = {
        "TOPOR_IPAMIR_TIME_LIMIT": str(max(1, args.timeout // 2)),
        "TOPOR_IPAMIR_VERBOSE": "0",
    }

    print(f"Phase 1: UWrMaxSat filter ({len(inputs)} inputs)...", flush=True)
    filtered = []
    for inp in inputs:
        r = run_app(ref_bin, inp, args.timeout)
        if r["ok"]:
            filtered.append({"path": inp, "ref": r})

    print(f"Phase 1: {len(filtered)}/{len(inputs)} passed\n", flush=True)
    rows = []
    for inp in filtered:
        path = inp["path"]
        ours = run_app(ours_bin, path, args.timeout, ip_env)
        rows.append({"path": path, "ref": inp["ref"], "ours": ours})

    ref_ok = len(filtered)
    ours_ok = sum(1 for r in rows if r["ours"]["ok"])
    ref_t = [r["ref"]["time"] for r in rows]
    ours_t = [r["ours"]["time"] for r in rows if r["ours"]["ok"]]

    lines = [
        f"**App:** `{args.app}` (MSE2022 incremental IPAMIR benchmark)",
        f"**Reference:** UWrMaxSat 1.4 IPAMIR",
        f"**Timeout:** {args.timeout}s per app run",
        f"**TOPOR_IPAMIR_TIME_LIMIT:** {ip_env['TOPOR_IPAMIR_TIME_LIMIT']}s",
        "",
        "## Results",
        "",
        f"| | Passed filter / ran | Avg wall time |",
        f"|--|---------------------|---------------|",
        f"| UWrMaxSat (phase 1) | {ref_ok}/{len(inputs)} | "
        f"{(sum(ref_t)/len(ref_t) if ref_t else 0):.2f}s |",
        f"| IntelSatSolver (phase 2) | {ours_ok}/{ref_ok} | "
        f"{(sum(ours_t)/len(ours_t) if ours_t else 0):.2f}s |",
        "",
        "### Per instance",
        "",
        "| Input | Ref time | Ours time | Ours OK |",
        "|-------|----------|-----------|---------|",
    ]
    for r in rows:
        name = os.path.basename(r["path"])
        lines.append(
            f"| {name} | {r['ref']['time']:.2f}s | {r['ours']['time']:.2f}s | "
            f"{'yes' if r['ours']['ok'] else 'no'} |"
        )

    out_md = os.path.join(args.out_dir, f"MSE2022_IPAMIR_APP_{args.app}.md")
    write_summary(out_md, f"MSE2022 IPAMIR App: {args.app}", lines)
    print(f"Wrote {out_md}")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Yam-style ref-first MSE2022 benchmarks")
    sub = ap.add_subparsers(dest="mode", required=True)

    w = sub.add_parser("ref-first-wcnf", help="EvalMaxSAT filters regression WCNFs")
    w.add_argument("--csv", default=os.path.join(SUITE, "MSE22Unique.csv"))
    w.add_argument("--max-instances", type=int, default=0, help="0 = all in CSV")
    w.add_argument("--timeout", type=int, default=60)
    w.add_argument("--resume", action="store_true", help="Resume from checkpoint if interrupted")
    w.add_argument("--out-dir", default=os.path.join(REPO, "results"))

    a = sub.add_parser("ipamir-app", help="MSE2022 IPAMIR incremental application")
    a.add_argument("--app", default="ipamirapp")
    a.add_argument("--max-instances", type=int, default=0)
    a.add_argument("--timeout", type=int, default=60)
    a.add_argument("--out-dir", default=os.path.join(REPO, "results"))

    args = ap.parse_args()
    if args.mode == "ref-first-wcnf":
        return cmd_ref_first_wcnf(args)
    return cmd_ipamir_app(args)


if __name__ == "__main__":
    sys.exit(main())
