// Unit tests for the IPAMIR return-code contract of the IntelSAT wrapper.
//
// Covers the behavior fixed/added in ToporIpamir.cc:
//   - 30  : optimum found and proven
//   - 20  : unsatisfiable hard clauses
//   - 30  : trivially-satisfiable instance with optimum cost 0
//   - incremental solving with assumptions
//   - 40  : ERROR state (sticky) -- only reachable via the compile-gated test hook,
//           because no benchmark drives the solver into an internal error code.
//
// Build (manually; not part of the project Makefile):
//   FLAGS="-DSKIP_ZLIB -Wall -Wno-parentheses -std=c++20 -O3 -DNDEBUG -DIPAMIR_TEST_HOOKS"
//   g++ $FLAGS -c ToporIpamir.cc -o /tmp/ToporIpamir_hook.o
//   g++ $FLAGS -c tests/test_ipamir_codes.cc -o /tmp/test_codes.o
//   g++ /tmp/test_codes.o /tmp/ToporIpamir_hook.o (release .a) -lpthread -o /tmp/test_codes
//   /tmp/test_codes
//
// The hooked ToporIpamir object is linked explicitly so the release archive's
// (non-hooked) ToporIpamir.or is not pulled in.

#include <cstdint>
#include <cstdio>

extern "C" {
    const char* ipamir_signature();
    void*       ipamir_init();
    void        ipamir_release(void* solver);
    void        ipamir_add_hard(void* solver, int32_t lit_or_zero);
    void        ipamir_add_soft_lit(void* solver, int32_t lit, uint64_t weight);
    void        ipamir_assume(void* solver, int32_t lit);
    int         ipamir_solve(void* solver);
    uint64_t    ipamir_val_obj(void* solver);
    int32_t     ipamir_val_lit(void* solver, int32_t lit);
    void        ipamir_set_terminate(void* solver, void* state, int (*terminate)(void* state));
#ifdef IPAMIR_TEST_HOOKS
    void        ipamir__test_force_error(void* solver);
#endif
}

// The solver's NuWLS/LSU code references this; the command-line front-end normally
// provides it. For a standalone test we supply a trivial stub.
double MainWallTimePassed() { return 0.0; }

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* msg)
{
    if (cond) { ++g_pass; printf("  PASS: %s\n", msg); }
    else      { ++g_fail; printf("  FAIL: %s\n", msg); }
}

// Trivially satisfiable: one soft literal, no hard clauses. Optimum sets it false
// -> cost 0 -> proven optimal (30).
static void test_cost_zero()
{
    printf("[test] satisfiable, optimum cost 0 -> expect 30, obj 0\n");
    void* s = ipamir_init();
    ipamir_add_soft_lit(s, 1, 5);
    int r = ipamir_solve(s);
    check(r == 30, "return code 30");
    check(ipamir_val_obj(s) == 0, "objective 0");
    ipamir_release(s);
}

// Weighted: hard clause (a OR b), paying 3 for a and 5 for b. Optimum takes a -> cost 3.
static void test_weighted_opt()
{
    printf("[test] weighted, hard (a|b), w(a)=3 w(b)=5 -> expect 30, obj 3\n");
    void* s = ipamir_init();
    ipamir_add_soft_lit(s, 1, 3);
    ipamir_add_soft_lit(s, 2, 5);
    ipamir_add_hard(s, 1); ipamir_add_hard(s, 2); ipamir_add_hard(s, 0); // (1 OR 2)
    int r = ipamir_solve(s);
    check(r == 30, "return code 30");
    check(ipamir_val_obj(s) == 3, "objective 3");
    ipamir_release(s);
}

// Contradictory hard clauses (1) and (-1) -> no feasible solution -> 20.
static void test_unsat()
{
    printf("[test] hard (1) and (-1) -> expect 20\n");
    void* s = ipamir_init();
    ipamir_add_hard(s, 1);  ipamir_add_hard(s, 0);
    ipamir_add_hard(s, -1); ipamir_add_hard(s, 0);
    int r = ipamir_solve(s);
    check(r == 20, "return code 20");
    ipamir_release(s);
}

// Incremental: solve once (opt 3), then assume a=false, forcing b -> opt 5.
static void test_incremental()
{
    printf("[test] incremental + assumptions -> expect 30/obj3 then 30/obj5\n");
    void* s = ipamir_init();
    ipamir_add_soft_lit(s, 1, 3);
    ipamir_add_soft_lit(s, 2, 5);
    ipamir_add_hard(s, 1); ipamir_add_hard(s, 2); ipamir_add_hard(s, 0); // (1 OR 2)

    int r1 = ipamir_solve(s);
    check(r1 == 30, "first solve 30");
    check(ipamir_val_obj(s) == 3, "first objective 3");

    ipamir_assume(s, -1);              // force a (var 1) false -> must take b
    int r2 = ipamir_solve(s);
    check(r2 == 30, "second solve 30 (under assumption)");
    check(ipamir_val_obj(s) == 5, "second objective 5 (assumption forced)");
    ipamir_release(s);
}

#ifdef IPAMIR_TEST_HOOKS
// Force the ERROR state and verify ipamir_solve returns 40 and stays 40 (sticky).
static void test_error_40()
{
    printf("[test] forced ERROR -> expect 40, and sticky on repeat\n");
    void* s = ipamir_init();
    ipamir_add_soft_lit(s, 1, 1);
    ipamir__test_force_error(s);
    int r1 = ipamir_solve(s);
    check(r1 == 40, "return code 40 after forced error");
    int r2 = ipamir_solve(s);
    check(r2 == 40, "still 40 on next solve (sticky)");
    ipamir_release(s);
}
#endif

int main()
{
    printf("IPAMIR return-code tests for solver: %s\n\n", ipamir_signature());

    test_cost_zero();
    test_weighted_opt();
    test_unsat();
    test_incremental();
#ifdef IPAMIR_TEST_HOOKS
    test_error_40();
#else
    printf("[skip] code-40 test (build with -DIPAMIR_TEST_HOOKS to enable)\n");
#endif

    printf("\n==== %d passed, %d failed ====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
