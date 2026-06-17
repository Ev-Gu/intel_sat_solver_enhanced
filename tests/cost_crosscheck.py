#!/usr/bin/env python3
"""
Cost-correctness cross-check for the IntelSAT IPAMIR solver.

For each small WCNF instance we compute the TRUE optimum with an independent
brute-force oracle (enumerate every assignment, require all hard clauses
satisfied, minimise the summed weight of violated soft clauses), then run the
official `ipamirapp` front-end -- which is linked against our solver -- and
compare its first reported result.

WCNF dialect accepted by ipamirapp: each clause line is
    <h|weight> lit lit ... 0
('c' = comment). No 'p' header line.

Exit code 0 == all instances matched the oracle.
"""

import itertools
import os
import re
import subprocess
import sys
import tempfile

# Single-solve front-end built from tests/wcnf_solve.cc, linked against the
# submission library. Override with the WCNF_SOLVE env var if needed.
SOLVER = os.environ.get("WCNF_SOLVE", "/tmp/wcnf_solve")


def parse_wcnf(text):
    hard, soft = [], []
    for line in text.splitlines():
        line = line.strip()
        if not line or line[0] == "c":
            continue
        toks = line.split()
        prefix, body = toks[0], toks[1:]
        assert body and body[-1] == "0", f"clause must end with 0: {line}"
        lits = [int(x) for x in body[:-1]]
        if prefix == "h":
            hard.append(lits)
        else:
            soft.append((int(prefix), lits))
    return hard, soft


def clause_sat(clause, assign):
    # assign: dict var->bool
    for lit in clause:
        v = abs(lit)
        val = assign[v]
        if (lit > 0 and val) or (lit < 0 and not val):
            return True
    return False


def brute_force_opt(text):
    """Return ('OPT', cost) or ('UNSAT', None)."""
    hard, soft = parse_wcnf(text)
    nvars = 0
    for cl in hard:
        for lit in cl:
            nvars = max(nvars, abs(lit))
    for _, cl in soft:
        for lit in cl:
            nvars = max(nvars, abs(lit))
    best = None
    for bits in itertools.product([False, True], repeat=nvars):
        assign = {i + 1: bits[i] for i in range(nvars)}
        if not all(clause_sat(cl, assign) for cl in hard):
            continue
        cost = sum(w for (w, cl) in soft if not clause_sat(cl, assign))
        if best is None or cost < best:
            best = cost
    if best is None:
        return ("UNSAT", None)
    return ("OPT", best)


def run_solver(text):
    """Run the single-solve front-end, return ('OPT', obj)/('UNSAT', None)."""
    with tempfile.NamedTemporaryFile("w", suffix=".wcnf", delete=False) as f:
        f.write(text)
        path = f.name
    try:
        out = subprocess.run(
            [SOLVER, path], capture_output=True, text=True, timeout=120
        ).stdout
    finally:
        os.unlink(path)
    for line in out.splitlines():
        if line.startswith("RESULT s UNSATISFIABLE"):
            return ("UNSAT", None)
        m = re.match(r"RESULT o (\d+)", line)
        if m:
            return ("OPT", int(m.group(1)))
    raise RuntimeError(f"no RESULT parsed from solver output:\n{out}")


# (name, wcnf-text) -- small enough to brute force.
INSTANCES = [
    ("weighted_opt3",
     "c hard (1|2); soft ~1 w3, ~2 w5, ~3 w1  => opt 3\n"
     "h 1 2 0\n3 -1 0\n5 -2 0\n1 -3 0\n"),
    ("unit_cover4",
     "c hard (1|2|3|4); soft ~1..~4 w1  => opt 1\n"
     "h 1 2 3 4 0\n1 -1 0\n1 -2 0\n1 -3 0\n1 -4 0\n"),
    ("cost0",
     "c hard (1); soft ~2 w7  => opt 0\n"
     "h 1 0\n7 -2 0\n"),
    ("unsat_hard",
     "c hard (1) and (~1)  => UNSAT\n"
     "h 1 0\nh -1 0\n"),
    ("weighted_mix",
     "c two hard, weighted softs (8 vars), brute-forced\n"
     "h 1 2 3 0\nh -2 4 0\n"
     "5 -1 0\n2 -2 0\n4 -3 0\n3 -4 0\n7 5 0\n1 -6 0\n6 7 8 0\n2 -8 0\n"),
    ("at_least_two",
     "c hard (1|2)&(2|3)&(1|3); soft ~1,~2,~3 weights 4,4,4 => opt 8\n"
     "h 1 2 0\nh 2 3 0\nh 1 3 0\n4 -1 0\n4 -2 0\n4 -3 0\n"),
]


def random_instances(n=40, seed=2026):
    """Seeded random weighted-MaxSAT instances, small enough to brute force."""
    import random
    rng = random.Random(seed)
    out = []
    for k in range(n):
        nv = rng.randint(3, 12)
        nhard = rng.randint(0, nv)
        nsoft = rng.randint(1, nv + 2)
        lines = [f"c random k={k} nv={nv} nhard={nhard} nsoft={nsoft}"]

        def rand_clause():
            size = rng.randint(1, 3)
            lits = set()
            while len(lits) < size:
                v = rng.randint(1, nv)
                lits.add(v if rng.random() < 0.5 else -v)
            return list(lits)

        for _ in range(nhard):
            lines.append("h " + " ".join(map(str, rand_clause())) + " 0")
        for _ in range(nsoft):
            w = rng.randint(1, 10)
            lines.append(f"{w} " + " ".join(map(str, rand_clause())) + " 0")
        out.append((f"rand_{k:02d}", "\n".join(lines) + "\n"))
    return out


def main():
    if not os.path.exists(SOLVER):
        print(f"ERROR: solver front-end not found at {SOLVER}", file=sys.stderr)
        return 2
    passed = failed = 0
    for name, text in INSTANCES + random_instances():
        oracle = brute_force_opt(text)
        got = run_solver(text)
        ok = oracle == got
        passed += ok
        failed += (not ok)
        print(f"[{'PASS' if ok else 'FAIL'}] {name:<14} "
              f"oracle={oracle}  solver={got}")
    print(f"\n==== {passed} passed, {failed} failed ====")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
