# IPAMIR test harness — setup

The actual harness (~750 MB: the upstream IPAMIR repo, its benchmark apps, and large
sample inputs) is **not committed** — it is re-clonable upstream content. This file explains
how to recreate it locally to run the benchmark applications against our solver.

## 1. Get the official IPAMIR repository
```bash
cd ipamir_test
git clone https://bitbucket.org/coreo-group/ipamir.git
rm -rf ipamir/.git        # so the parent repo can track wiring if desired
```

## 2. Add our solver wiring
The IntelSAT IPAMIR wiring is kept in the submission package; copy it in as a maxsat solver:
```bash
cp -r ../submission/IntelSatSolver ipamir/maxsat/IntelSatSolver
```
This directory builds `libipamirIntelSatSolver.a` (signature `IntelSatSolver`). It is fully
self-contained (bundles the solver source in `solver_src/`).

> Historical note: during development the wiring instead pointed at the solver tree four
> levels up (`SOLVERROOT = ../../../..`). The submission package supersedes that with a
> self-contained build, so prefer copying `submission/IntelSatSolver`.

## 3. Build a benchmark app against our solver
```bash
cd ipamir/app/ipamirapp          # or ipamirextenf, ipamiric3ref, ipamirbioptsat, ...
make IPAMIRSOLVER=IntelSatSolver
./ipamirapp inputs/<some_instance>.wcnf
```

## 4. Notes
- Some upstream apps need extra dependencies (e.g. `ipamirmlicseesaw` needs commercial CPLEX;
  `ipamiradaboost` has an upstream compile issue). These are app problems, not solver problems.
- For quick, dependency-free correctness checks use the in-repo tools instead:
  `tests/wcnf_solve.cc` + `tests/cost_crosscheck.py` (see `findings/code-changes.md` §7).
