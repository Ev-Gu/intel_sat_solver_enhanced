/*
 * Example IPAMIR program demonstrating incremental MaxSAT solving with assumptions
 * This example shows how to:
 * 1. Use assumptions to harden soft clauses incrementally
 * 2. Solve with different assumption sets
 * 3. Query results after each solve
 */

#include "ToporIpamir.h"
#include <iostream>

using namespace std;

int main() {
    void* solver = ipamir_init();
    if (!solver) {
        cerr << "ERROR: Failed to initialize IPAMIR solver" << endl;
        return 1;
    }

    cout << "IPAMIR solver signature: " << ipamir_signature() << endl;
    cout << "Testing incremental MaxSAT solving with assumptions..." << endl;

    // Hard clause: (x1 OR x2)
    ipamir_add_hard(solver, 1);
    ipamir_add_hard(solver, 2);
    ipamir_add_hard(solver, 0);

    // Hard clause: (x1 OR x3)
    ipamir_add_hard(solver, 1);
    ipamir_add_hard(solver, 3);
    ipamir_add_hard(solver, 0);

    // Soft literals with weights
    ipamir_add_soft_lit(solver, 1, 10); // x1 = true costs 10
    ipamir_add_soft_lit(solver, 2, 5);  // x2 = true costs 5
    ipamir_add_soft_lit(solver, 3, 3);  // x3 = true costs 3

    // First solve: without assumptions
    cout << "\n=== First solve (no assumptions) ===" << endl;
    int result1 = ipamir_solve(solver);
    cout << "Result: " << result1 << " ";
    if (result1 == 20) {
        cout << "(UNSAT)" << endl;
        ipamir_release(solver);
        return 1;
    } else if (result1 == 30 || result1 == 10) {
        cout << (result1 == 30 ? "(OPTIMAL)" : "(SAT - feasible)") << endl;
        uint64_t obj1 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj1 << endl;
        cout << "x1 = " << ipamir_val_lit(solver, 1) 
             << ", x2 = " << ipamir_val_lit(solver, 2)
             << ", x3 = " << ipamir_val_lit(solver, 3) << endl;
    }

    // Second solve: with assumption -x1 (harden soft clause for x1)
    cout << "\n=== Second solve (assumption: -x1, hardening soft clause) ===" << endl;
    ipamir_assume(solver, -1); // Assume x1 is false
    int result2 = ipamir_solve(solver);
    cout << "Result: " << result2 << " ";
    if (result2 == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result2 == 30 || result2 == 10) {
        cout << (result2 == 30 ? "(OPTIMAL)" : "(SAT - feasible)") << endl;
        uint64_t obj2 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj2 << endl;
        cout << "x1 = " << ipamir_val_lit(solver, 1) 
             << ", x2 = " << ipamir_val_lit(solver, 2)
             << ", x3 = " << ipamir_val_lit(solver, 3) << endl;
        // Note: x1 should be false due to assumption
        if (ipamir_val_lit(solver, 1) != -1) {
            cerr << "WARNING: Expected x1 = -1 (false) due to assumption" << endl;
        }
    }

    // Third solve: with assumptions -x1 and -x2
    cout << "\n=== Third solve (assumptions: -x1, -x2) ===" << endl;
    ipamir_assume(solver, -1); // Assume x1 is false
    ipamir_assume(solver, -2); // Assume x2 is false
    int result3 = ipamir_solve(solver);
    cout << "Result: " << result3 << " ";
    if (result3 == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result3 == 30 || result3 == 10) {
        cout << (result3 == 30 ? "(OPTIMAL)" : "(SAT - feasible)") << endl;
        uint64_t obj3 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj3 << endl;
        cout << "x1 = " << ipamir_val_lit(solver, 1) 
             << ", x2 = " << ipamir_val_lit(solver, 2)
             << ", x3 = " << ipamir_val_lit(solver, 3) << endl;
    }

    // Clean up
    ipamir_release(solver);
    
    cout << "\n=== Test completed successfully ===" << endl;
    return 0;
}

