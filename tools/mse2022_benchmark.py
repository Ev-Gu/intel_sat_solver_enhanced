#!/usr/bin/env python3
"""
Yam-style MSE2022 performance benchmark (NOT the regression correctness suite).

Modes:
  ref-first-wcnf  — EvalMaxSAT filters WCNFs, then IntelTopor IPAMIR only
  ipamir-app      — UWrMaxSat filters ipamirapp inputs, then IntelSatSolver

Config lives under benchmarks/mse2022/ (separate from MaxSATRegressionSuite).
WCNF files are read from WCNF_ROOT (default: third_party/MaxSATRegressionSuite).
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
BENCHMARK_DIR = os.path.join(REPO, "benchmarks", "mse2022")
WCNF_ROOT = os.path.join(REPO, "third_party", "MaxSATRegressionSuite")
IPAMIR = os.path.join(REPO, "third_party", "ipamir")
EVAL = os.path.join(
    REPO, "third_party", "MaxSAT-Fuzzer", "MaxSATSolver", "MSE22", "EvalMaxSAT", "build", "EvalMaxSAT_bin"
)
IPAMIR_OURS = os.path.join(HERE, "bin", "ipamir_wcnf_ours")
SEARCH_DIRS = ("MSE22Unique", "MSE23Unique", "MSE22Big", "MSE23Big")
DEFAULT_CSV = os.path.join(BENCHMARK_DIR, "instances.csv")
DEFAULT_OUT = os.path.join(BENCHMARK_DIR, "results")


def wcnf_root():
    return os.environ.get("WCNF_ROOT", WCNF_ROOT)


def resolve_wcnf(name):
    root = wcnf_root()
    if os.path.sep in name:
        path = os.path.join(root, name)
        if os.path.isfile(path):
            return path
    for sub in SEARCH_DIRS:
        path = os.path.join(root, sub, name)
        if os.path.isfile(path):
            return path
    return None


def ipamir_solver_env(timeout, nuwls_time):
    """Env for our IPAMIR WCNF loader / IntelSatSolver app (NUWLS off when nuwls_time=0)."""
    return {
        "TOPOR_IPAMIR_TIME_LIMIT": str(max(1, timeout - 1)),
        "TOPOR_IPAMIR_VERBOSE": "0",
        "TOPOR_NUWLS_TIME_LIMIT": str(nuwls_time),
    }


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


def fmt_obj(obj):
    return "" if obj is None else str(obj)


def matches_certified(best_o, obj):
    return bool(best_o and obj is not None and str(obj) == str(best_o))


def log_solver_line(solver_label, inst_name, r):
    obj = fmt_obj(r.get("obj"))
    status = r.get("status") or ("TIMEOUT" if r.get("timeout") else "—")
    print(
        f"    [{solver_label}] {inst_name}: {r['time']:.2f}s  status={status}  o={obj or '—'}",
        flush=True,
    )


def write_csv(path, fieldnames, rows):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        for row in rows:
            w.writerow(row)


def write_wcnf_detail_csvs(out_dir, results, phase1_all, timeout, nuwls_time):
    """Wide + long per-instance tables for WCNF ref-first (EvalMaxSAT + IPAMIR only)."""
    wide_path = os.path.join(out_dir, "wcnf_ref_first_instances.csv")
    long_path = os.path.join(out_dir, "wcnf_ref_first_by_solver.csv")
    phase1_path = os.path.join(out_dir, "wcnf_eval_filter_all.csv")

    wide_fields = [
        "instance",
        "certified_best_o",
        "eval_time_s",
        "eval_status",
        "eval_best_o",
        "eval_solved",
        "eval_timeout",
        "ipamir_time_s",
        "ipamir_status",
        "ipamir_best_o",
        "ipamir_solved",
        "ipamir_matches_certified",
        "ipamir_timeout",
        "timeout_sec",
        "topor_nuwls_time_limit",
    ]
    wide_rows = []
    long_rows = []
    for r in results:
        inst = r["file"]
        best_o = r.get("best_o") or ""
        ref = r.get("ref", {})
        ipamir = r["ipamir"]
        wide_rows.append(
            {
                "instance": inst,
                "certified_best_o": best_o,
                "eval_time_s": f"{ref.get('time', 0):.3f}",
                "eval_status": ref.get("status") or "",
                "eval_best_o": fmt_obj(ref.get("obj")),
                "eval_solved": ref.get("ok", False),
                "eval_timeout": ref.get("timeout", False),
                "ipamir_time_s": f"{ipamir['time']:.3f}",
                "ipamir_status": ipamir.get("status") or "",
                "ipamir_best_o": fmt_obj(ipamir.get("obj")),
                "ipamir_solved": ipamir.get("ok", False),
                "ipamir_matches_certified": matches_certified(best_o, ipamir.get("obj")),
                "ipamir_timeout": ipamir.get("timeout", False),
                "timeout_sec": timeout,
                "topor_nuwls_time_limit": nuwls_time,
            }
        )
        for solver, label in (
            (ref, "EvalMaxSAT_2022"),
            (ipamir, "IntelTopor_IPAMIR"),
        ):
            long_rows.append(
                {
                    "track": "wcnf",
                    "instance": inst,
                    "solver": label,
                    "time_s": f"{solver['time']:.3f}",
                    "status": solver.get("status") or "",
                    "best_o": fmt_obj(solver.get("obj")),
                    "solved": solver.get("ok", False),
                    "timeout": solver.get("timeout", False),
                    "exit_code": solver.get("exit", ""),
                    "certified_best_o": best_o,
                    "matches_certified": matches_certified(best_o, solver.get("obj")),
                    "timeout_sec": timeout,
                    "topor_nuwls_time_limit": nuwls_time,
                }
            )

    write_csv(wide_path, wide_fields, wide_rows)

    long_fields = [
        "track",
        "instance",
        "solver",
        "time_s",
        "status",
        "best_o",
        "solved",
        "timeout",
        "exit_code",
        "certified_best_o",
        "matches_certified",
        "timeout_sec",
        "topor_nuwls_time_limit",
    ]
    write_csv(long_path, long_fields, long_rows)

    if phase1_all:
        p1_fields = [
            "instance",
            "certified_best_o",
            "eval_time_s",
            "eval_status",
            "eval_best_o",
            "eval_solved",
            "eval_timeout",
            "passed_filter",
            "timeout_sec",
        ]
        p1_rows = []
        for row in phase1_all:
            ref = row["eval"]
            p1_rows.append(
                {
                    "instance": row["file"],
                    "certified_best_o": row.get("best_o") or "",
                    "eval_time_s": f"{ref['time']:.3f}",
                    "eval_status": ref.get("status") or "",
                    "eval_best_o": fmt_obj(ref.get("obj")),
                    "eval_solved": ref.get("ok", False),
                    "eval_timeout": ref.get("timeout", False),
                    "passed_filter": ref.get("ok", False),
                    "timeout_sec": timeout,
                }
            )
        write_csv(phase1_path, p1_fields, p1_rows)

    return wide_path, long_path, phase1_path if phase1_all else None


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


def summarize_ref_first(instances, filtered, results, timeout, csv_path, out_dir, nuwls_time):
    ip_ok = sum(1 for r in results if r["ipamir"]["ok"])
    ip_cert = sum(
        1
        for r in results
        if r["ipamir"]["ok"] and r["best_o"] and str(r["ipamir"]["obj"]) == r["best_o"]
    )
    ip_times = [r["ipamir"]["time"] for r in results if r["ipamir"]["ok"]]
    ref_times = [r["ref"]["time"] for r in results]

    lines = [
        f"**Mode:** ref-first WCNF (Yam: reference filters, then our IPAMIR only)",
        f"**Benchmark config:** `{os.path.relpath(BENCHMARK_DIR, REPO)}/` (not regression suite)",
        f"**Instances CSV:** `{os.path.relpath(csv_path, REPO)}`",
        f"**WCNF file root:** `{os.path.relpath(wcnf_root(), REPO)}/` (read-only paths)",
        f"**Timeout:** {timeout}s",
        f"**TOPOR_NUWLS_TIME_LIMIT:** {nuwls_time} (0 = NUWLS disabled)",
        f"**Phase 2 completed:** {len(results)}/{len(filtered)} instances",
        "",
        "## Phase 1 — EvalMaxSAT filter",
        "",
        f"| Scanned | Passed filter |",
        f"|---------|---------------|",
        f"| {len(instances)} | {len(filtered)} |",
        "",
        "## Phase 2 — IntelTopor IPAMIR on filtered set only",
        "",
        "| Path | Solved | Matches certified optimum | Avg time (solved) |",
        "|------|--------|---------------------------|-------------------|",
        f"| IPAMIR | {ip_ok}/{len(results)} | {ip_cert}/{len(results)} | "
        f"{(sum(ip_times)/len(ip_times) if ip_times else 0):.2f}s |",
        f"| EvalMaxSAT (phase 1) | {len(results)}/{len(results)} | — | "
        f"{(sum(ref_times)/len(ref_times) if ref_times else 0):.2f}s |",
        "",
        "## Interpretation",
        "",
        "- Only instances **EvalMaxSAT solved within timeout** are compared (fair subset).",
        "- Batch path is **not** run (IPAMIR-only benchmark).",
        "- NUWLS local search: **off** when `TOPOR_NUWLS_TIME_LIMIT=0`.",
    ]
    wide, long, p1 = write_wcnf_detail_csvs(out_dir, results, None, timeout, nuwls_time)
    lines.extend(
        [
            "",
            "## Per-instance detail (CSV)",
            "",
            f"- Wide (one row per instance): `{os.path.relpath(wide, REPO)}`",
            f"- Long (one row per solver × instance): `{os.path.relpath(long, REPO)}`",
        ]
    )
    if p1:
        lines.append(f"- Phase-1 EvalMaxSAT on all scanned: `{os.path.relpath(p1, REPO)}`")
    out_md = os.path.join(out_dir, "MSE2022_REF_FIRST_WCNF.md")
    write_summary(out_md, "MSE Ref-First Benchmark (WCNF)", lines)
    return out_md


def cmd_ref_first_wcnf(args):
    if not os.path.isfile(EVAL) or not os.path.isfile(IPAMIR_OURS):
        print(f"Missing EvalMaxSAT or {IPAMIR_OURS}", file=sys.stderr)
        return 1

    ckpt_file = checkpoint_path(args.out_dir)
    ckpt = load_checkpoint(ckpt_file) if args.resume else None
    phase1_all = None
    nuwls_time = args.nuwls_time

    ckpt_ok = (
        ckpt
        and ckpt.get("csv") == args.csv
        and ckpt.get("timeout") == args.timeout
        and ckpt.get("nuwls_time") == nuwls_time
    )

    if ckpt_ok:
        instances = ckpt["instances"]
        filtered = ckpt["filtered"]
        results = ckpt.get("results", [])
        phase1_all = ckpt.get("phase1_all")
        done_files = {r["file"] for r in results}
        print(f"Resuming: phase1 {len(filtered)}/{len(instances)}, phase2 {len(results)}/{len(filtered)} done", flush=True)
    else:
        if args.resume and ckpt:
            print("Checkpoint mismatch (csv/timeout/nuwls changed); starting fresh.", flush=True)
        instances = load_csv_instances(args.csv, args.max_instances)
        if not instances:
            print("No instances.", file=sys.stderr)
            return 1

        print(
            f"Phase 1: EvalMaxSAT filter ({len(instances)} instances, {args.timeout}s)...",
            flush=True,
        )
        print(f"  TOPOR_NUWLS_TIME_LIMIT={nuwls_time} (phase 2 IPAMIR only)", flush=True)
        filtered = []
        phase1_all = []
        for i, inst in enumerate(instances, 1):
            print(f"  Phase 1 [{i}/{len(instances)}] {inst['file']}", flush=True)
            r = run_cmd([EVAL], inst["path"], args.timeout)
            log_solver_line("EvalMaxSAT", inst["file"], r)
            phase1_all.append({**inst, "eval": r})
            if r["ok"]:
                filtered.append({**inst, "ref": r})
            if i % 20 == 0:
                print(f"  ... {i}/{len(instances)} scanned, {len(filtered)} passed filter", flush=True)
        write_wcnf_detail_csvs(args.out_dir, [], phase1_all, args.timeout, nuwls_time)
        print(f"Phase 1 done: {len(filtered)}/{len(instances)} solved by EvalMaxSAT\n", flush=True)
        results = []
        done_files = set()
        save_checkpoint(
            ckpt_file,
            {
                "csv": args.csv,
                "timeout": args.timeout,
                "nuwls_time": nuwls_time,
                "instances": instances,
                "filtered": filtered,
                "phase1_all": phase1_all,
                "results": results,
            },
        )

    ip_env = ipamir_solver_env(args.timeout, nuwls_time)
    pending = [inst for inst in filtered if inst["file"] not in done_files]
    for i, inst in enumerate(pending, len(results) + 1):
        print(f"Phase 2 [{i}/{len(filtered)}] {inst['file']}", flush=True)
        ipamir = run_cmd([IPAMIR_OURS], inst["path"], args.timeout + 5, ip_env)
        log_solver_line("IntelTopor_IPAMIR", inst["file"], ipamir)
        results.append({**inst, "ipamir": ipamir})
        write_wcnf_detail_csvs(args.out_dir, results, phase1_all, args.timeout, nuwls_time)
        save_checkpoint(
            ckpt_file,
            {
                "csv": args.csv,
                "timeout": args.timeout,
                "nuwls_time": nuwls_time,
                "instances": instances,
                "filtered": filtered,
                "phase1_all": phase1_all,
                "results": results,
            },
        )

    out_md = summarize_ref_first(
        instances, filtered, results, args.timeout, args.csv, args.out_dir, nuwls_time
    )
    wide = os.path.join(args.out_dir, "wcnf_ref_first_instances.csv")
    print(f"\nWrote {out_md}")
    print(f"Per-instance CSV: {wide}")
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
        raw = p.stdout + ("\n" + p.stderr if p.stderr else "")
        status, obj = parse_output(raw)
        ok = status and ("OPTIMUM" in status.upper() or status.upper() == "SATISFIABLE")
        ok = ok and p.returncode in (0, 10, 30) and "ERROR" not in raw
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


def write_ipamir_detail_csvs(out_dir, app_name, results, phase1_all, timeout):
    """Wide + long per-instance tables for IPAMIR incremental app track."""
    wide_path = os.path.join(out_dir, f"ipamir_app_{app_name}_instances.csv")
    long_path = os.path.join(out_dir, f"ipamir_app_{app_name}_by_solver.csv")
    phase1_path = os.path.join(out_dir, f"ipamir_app_{app_name}_filter_all.csv")

    wide_fields = [
        "instance",
        "uwr_time_s",
        "uwr_status",
        "uwr_best_o",
        "uwr_solved",
        "uwr_timeout",
        "intel_time_s",
        "intel_status",
        "intel_best_o",
        "intel_solved",
        "intel_timeout",
        "intel_matches_uwr",
        "timeout_sec",
    ]
    wide_rows = []
    long_rows = []
    for r in results:
        inst = os.path.basename(r["path"])
        ref = r["ref"]
        ours = r["ours"]
        wide_rows.append(
            {
                "instance": inst,
                "uwr_time_s": f"{ref['time']:.3f}",
                "uwr_status": ref.get("status") or "",
                "uwr_best_o": fmt_obj(ref.get("obj")),
                "uwr_solved": ref.get("ok", False),
                "uwr_timeout": ref.get("timeout", False),
                "intel_time_s": f"{ours['time']:.3f}",
                "intel_status": ours.get("status") or "",
                "intel_best_o": fmt_obj(ours.get("obj")),
                "intel_solved": ours.get("ok", False),
                "intel_timeout": ours.get("timeout", False),
                "intel_matches_uwr": ref.get("obj") is not None
                and ours.get("obj") is not None
                and str(ref.get("obj")) == str(ours.get("obj")),
                "timeout_sec": timeout,
            }
        )
        for solver, label in ((ref, "UWrMaxSat14"), (ours, "IntelSatSolver")):
            long_rows.append(
                {
                    "track": "ipamir_app",
                    "app": app_name,
                    "instance": inst,
                    "solver": label,
                    "time_s": f"{solver['time']:.3f}",
                    "status": solver.get("status") or "",
                    "best_o": fmt_obj(solver.get("obj")),
                    "solved": solver.get("ok", False),
                    "timeout": solver.get("timeout", False),
                    "exit_code": solver.get("exit", ""),
                    "timeout_sec": timeout,
                }
            )

    write_csv(wide_path, wide_fields, wide_rows)

    long_fields = [
        "track",
        "app",
        "instance",
        "solver",
        "time_s",
        "status",
        "best_o",
        "solved",
        "timeout",
        "exit_code",
        "timeout_sec",
    ]
    write_csv(long_path, long_fields, long_rows)

    if phase1_all:
        p1_fields = [
            "instance",
            "uwr_time_s",
            "uwr_status",
            "uwr_best_o",
            "uwr_solved",
            "uwr_timeout",
            "passed_filter",
            "timeout_sec",
        ]
        p1_rows = []
        for row in phase1_all:
            ref = row["ref"]
            p1_rows.append(
                {
                    "instance": os.path.basename(row["path"]),
                    "uwr_time_s": f"{ref['time']:.3f}",
                    "uwr_status": ref.get("status") or "",
                    "uwr_best_o": fmt_obj(ref.get("obj")),
                    "uwr_solved": ref.get("ok", False),
                    "uwr_timeout": ref.get("timeout", False),
                    "passed_filter": ref.get("ok", False),
                    "timeout_sec": timeout,
                }
            )
        write_csv(phase1_path, p1_fields, p1_rows)

    return wide_path, long_path, phase1_path if phase1_all else None


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

    ip_env = ipamir_solver_env(args.timeout, args.nuwls_time)

    print(f"Phase 1: UWrMaxSat filter ({len(inputs)} inputs)...", flush=True)
    print(f"  TOPOR_NUWLS_TIME_LIMIT={args.nuwls_time} (phase 2 IntelSatSolver)", flush=True)
    filtered = []
    phase1_all = []
    for i, inp in enumerate(inputs, 1):
        name = os.path.basename(inp)
        print(f"  Phase 1 [{i}/{len(inputs)}] {name}", flush=True)
        r = run_app(ref_bin, inp, args.timeout)
        log_solver_line("UWrMaxSat14", name, r)
        phase1_all.append({"path": inp, "ref": r})
        if r["ok"]:
            filtered.append({"path": inp, "ref": r})
    write_ipamir_detail_csvs(args.out_dir, args.app, [], phase1_all, args.timeout)

    print(f"Phase 1: {len(filtered)}/{len(inputs)} passed\n", flush=True)
    rows = []
    for i, inp in enumerate(filtered, 1):
        path = inp["path"]
        name = os.path.basename(path)
        print(f"Phase 2 [{i}/{len(filtered)}] {name}", flush=True)
        ours = run_app(ours_bin, path, args.timeout, ip_env)
        log_solver_line("IntelSatSolver", name, ours)
        rows.append({"path": path, "ref": inp["ref"], "ours": ours})
        write_ipamir_detail_csvs(args.out_dir, args.app, rows, phase1_all, args.timeout)

    ref_ok = len(filtered)
    ours_ok = sum(1 for r in rows if r["ours"]["ok"])
    ref_t = [r["ref"]["time"] for r in rows]
    ours_t = [r["ours"]["time"] for r in rows if r["ours"]["ok"]]

    lines = [
        f"**App:** `{args.app}` (MSE2022 incremental IPAMIR benchmark)",
        f"**Benchmark config:** `{os.path.relpath(BENCHMARK_DIR, REPO)}/`",
        f"**Reference:** UWrMaxSat 1.4 IPAMIR",
        f"**Timeout:** {args.timeout}s per app run",
        f"**TOPOR_IPAMIR_TIME_LIMIT:** {ip_env['TOPOR_IPAMIR_TIME_LIMIT']}s",
        f"**TOPOR_NUWLS_TIME_LIMIT:** {args.nuwls_time} (0 = NUWLS disabled)",
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
        "| Input | Ref time | Ref o | Ours time | Ours o | Ours OK |",
        "|-------|----------|-------|-----------|--------|---------|",
    ]
    for r in rows:
        name = os.path.basename(r["path"])
        ref_o = fmt_obj(r["ref"].get("obj"))
        ours_o = fmt_obj(r["ours"].get("obj"))
        lines.append(
            f"| {name} | {r['ref']['time']:.2f}s | {ref_o or '—'} | "
            f"{r['ours']['time']:.2f}s | {ours_o or '—'} | "
            f"{'yes' if r['ours']['ok'] else 'no'} |"
        )

    wide, long, p1 = write_ipamir_detail_csvs(args.out_dir, args.app, rows, phase1_all, args.timeout)
    lines.extend(
        [
            "",
            "## Per-instance detail (CSV)",
            "",
            f"- Wide (one row per instance): `{os.path.relpath(wide, REPO)}`",
            f"- Long (one row per solver × instance): `{os.path.relpath(long, REPO)}`",
        ]
    )
    if p1:
        lines.append(f"- Phase-1 UWrMaxSat on all inputs: `{os.path.relpath(p1, REPO)}`")

    out_md = os.path.join(args.out_dir, f"MSE2022_IPAMIR_APP_{args.app}.md")
    write_summary(out_md, f"MSE2022 IPAMIR App: {args.app}", lines)
    print(f"Wrote {out_md}")
    print(f"Per-instance CSV: {wide}")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Yam-style ref-first MSE2022 benchmarks")
    sub = ap.add_subparsers(dest="mode", required=True)

    w = sub.add_parser("ref-first-wcnf", help="EvalMaxSAT filters WCNFs; our IPAMIR only")
    w.add_argument("--csv", default=DEFAULT_CSV)
    w.add_argument("--max-instances", type=int, default=0, help="0 = all in CSV")
    w.add_argument("--timeout", type=int, default=3600)
    w.add_argument(
        "--nuwls-time",
        type=int,
        default=0,
        help="TOPOR_NUWLS_TIME_LIMIT for IPAMIR (0=disable NUWLS, default)",
    )
    w.add_argument("--resume", action="store_true", help="Resume from checkpoint if interrupted")
    w.add_argument("--out-dir", default=DEFAULT_OUT)

    a = sub.add_parser("ipamir-app", help="MSE2022 IPAMIR incremental application")
    a.add_argument("--app", default="ipamirapp")
    a.add_argument("--max-instances", type=int, default=0)
    a.add_argument("--timeout", type=int, default=7200)
    a.add_argument(
        "--nuwls-time",
        type=int,
        default=0,
        help="TOPOR_NUWLS_TIME_LIMIT for IntelSatSolver (0=disable NUWLS, default)",
    )
    a.add_argument("--out-dir", default=DEFAULT_OUT)

    args = ap.parse_args()
    if args.mode == "ref-first-wcnf":
        return cmd_ref_first_wcnf(args)
    return cmd_ipamir_app(args)


if __name__ == "__main__":
    sys.exit(main())
