# Solver Correctness Fix Log

This document tracks correctness, memory-safety, and IPAMIR-behavior fixes made during the final solver review.

Each fix documents:
- what was wrong,
- where it appeared,
- why it was dangerous,
- what was changed,
- why the new behavior matches the project goals,
- and how the fix was validated.

Baseline:
- Branch: feature/lsu-memory-preflight
- Remote: origin/feature/ipamir-incremental-maxsat
- Reviewed commit: 69b0e68 removed prints
- Baseline build: succeeded, 0 errors, 9 warnings from algorithms/Alg_nuwls.h

---

## FIX-001: Correct ValLit return semantics

File: ToporIpamir.cc

Approximate area:
CToporIpamirWrapper::ValLit

Original problem:
ValLit returned the stored internal variable assignment directly, usually 0 or 1.

Risk if unchanged:
A literal-value API must answer the value of the requested literal, including its sign. Returning only the variable assignment means ValLit(x) and ValLit(-x) can return the same value. This can corrupt external model reporting and validation.

Fix:
Map the external variable to the internal variable, read the stored best assignment, apply the sign of the requested literal, and return the requested literal if it is satisfied. Otherwise return its negation.

Why the fix is correct:
The result is now literal-oriented instead of variable-oriented. This preserves correct behavior for both positive and negative literals.

Implementation:
Changed ValLit so it computes varTrue from m_globalBestModel, then computes litSatisfied according to the sign of the requested literal, and returns either lit or -lit.

Validation:
The project must build after the change. A later tiny-model test should verify that ValLit(x) and ValLit(-x) report opposite literal satisfaction for the same assignment.

Status: Done

---

## FIX-002: Remove double mapping of assumptions

File: ToporIpamir.cc

Approximate area:
Assume
RunMbvAndLsuPostSolve

Original problem:
Assume already maps external literals to internal literals before storing them in m_CurrAssumps. RunMbvAndLsuPostSolve then maps assumpsForPost again through GetOrCreateInternalVar.

Risk if unchanged:
Post-solvers may solve under different assumptions than the main Topor solve. This is a silent correctness bug.

Fix:
Treat assumpsForPost as already internal and copy or filter it without remapping.

Status: Planned

---

## FIX-003: Pass localWmbOptions into MrsBeaver

File: ToporIpamir.cc

Approximate area:
RunMbvAndLsuPostSolve

Original problem:
localWmbOptions is created and modified, but RunMrsBeaver receives wmbOptions instead.

Risk if unchanged:
The intended local timeout is ignored.

Fix:
Pass localWmbOptions to RunMrsBeaver.

Status: Planned

---

## FIX-004: Replace unsafe reinterpret_cast around NUWLS cost

File: ToporIpamir.cc

Approximate area:
RunNuwlsPostSolve

Original problem:
m_bestCost is passed to NUWLS through reinterpret_cast<unsigned long long&>.

Risk if unchanged:
This relies on unsafe type aliasing and is not robust.

Fix:
Copy m_bestCost to a local unsigned long long, pass it to NUWLS, then copy it back only if improved.

Status: Planned

---

## FIX-005: Prevent repeated MrsBeaver loop without improvement

File: ToporIpamir.cc

Approximate area:
RunMbvAndLsuPostSolve

Original problem:
The loop can repeat MrsBeaver when skipCompletePhase or timedOut is set, even if bestCost did not improve.

Risk if unchanged:
The solver may waste remaining time in a no-progress loop instead of making a meaningful handoff decision.

Fix:
Track costBeforeWmb and avoid repeating the loop if no improvement occurred.

Status: Planned

---

## FIX-006: Verify soft-literal objective semantics

File: ToporIpamir.cc

Approximate area:
ComputeObjFromAssignment
RunNuwlsPostSolve
RunMbvAndLsuPostSolve

Original problem:
The current code appears to interpret soft literals as soft clauses, meaning cost is paid when the literal is not satisfied. This must be verified against the intended project and API convention.

Risk if unchanged:
If the intended convention is opposite, the solver optimizes the wrong objective.

Fix:
Confirm the intended convention and align objective computation, NUWLS construction, and MrsBeaver or LSU relaxation construction.

Status: Needs verification before editing

---

## FIX-007: Decide whether LSU clauses may remain in the main Topor instance

File: ToporIpamir.cc

Approximate area:
LSU addClause callback

Original problem:
LSU appears to add clauses directly into the main CTopor instance. The name clauseWithAct suggests activation support, but no activation literal is added.

Risk if unchanged:
Old LSU constraints may remain active across future solve calls and break incremental behavior.

Fix:
Decide whether LSU is terminal for this wrapper, whether LSU should run on a separate Topor instance, or whether activation literals are required.

Status: Needs design decision
