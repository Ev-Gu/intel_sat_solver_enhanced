/*
 * Example IPAMIR program demonstrating incremental MaxSAT solving
 * This example shows how to:
 * 1. Initialize the solver
 * 2. Add hard clauses
 * 3. Add soft literals with weights
 * 4. Solve incrementally (multiple solve calls)
 * 5. Query results and objective values
 */

#include "ToporIpamir.h"
#include <iostream>
#include <cassert>

using namespace std;

int main() {
    void* solver = ipamir_init();
    if (!solver) {
        cerr << "ERROR: Failed to initialize IPAMIR solver" << endl;
        return 1;
    }

    cout << "IPAMIR solver signature: " << ipamir_signature() << endl;
    cout << "Testing incremental MaxSAT solving..." << endl;

    // Example 1: Simple MaxSAT problem with incremental solving
    // Hard clause: (x1 OR x2)
    ipamir_add_hard(solver, 1);
    ipamir_add_hard(solver, 2);
    ipamir_add_hard(solver, 0);

    // Soft literals with weights
    // We want x1 to be false (cost 5 if true)
    ipamir_add_soft_lit(solver, 1, 5);
    // We want x2 to be false (cost 3 if true)
    ipamir_add_soft_lit(solver, 2, 3);

    // First solve: find optimal solution
    cout << "\n=== First solve ===" << endl;
    int result1 = ipamir_solve(solver);
    cout << "Result: " << result1 << " ";
    if (result1 == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result1 == 30) {
        cout << "(OPTIMAL)" << endl;
        uint64_t obj1 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj1 << endl;
        int val_x1 = ipamir_val_lit(solver, 1);
        int val_x2 = ipamir_val_lit(solver, 2);
        cout << "x1 = " << val_x1 << ", x2 = " << val_x2 << endl;
    } else if (result1 == 10) {
        cout << "(SAT - feasible)" << endl;
        uint64_t obj1 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj1 << endl;
    }

    // Incremental modification: add another hard clause
    // Hard clause: (-x1 OR -x2)
    ipamir_add_hard(solver, -1);
    ipamir_add_hard(solver, -2);
    ipamir_add_hard(solver, 0);

    // Second solve: solve with the new constraint
    cout << "\n=== Second solve (with additional hard clause) ===" << endl;
    int result2 = ipamir_solve(solver);
    cout << "Result: " << result2 << " ";
    if (result2 == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result2 == 30) {
        cout << "(OPTIMAL)" << endl;
        uint64_t obj2 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj2 << endl;
        int val_x1 = ipamir_val_lit(solver, 1);
        int val_x2 = ipamir_val_lit(solver, 2);
        cout << "x1 = " << val_x1 << ", x2 = " << val_x2 << endl;
    } else if (result2 == 10) {
        cout << "(SAT - feasible)" << endl;
        uint64_t obj2 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj2 << endl;
    }

    // Incremental modification: change soft literal weight
    ipamir_add_soft_lit(solver, 1, 10); // Change weight of x1 to 10

    // Third solve: solve with updated weights
    cout << "\n=== Third solve (with updated soft literal weight) ===" << endl;
    int result3 = ipamir_solve(solver);
    cout << "Result: " << result3 << " ";
    if (result3 == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result3 == 30) {
        cout << "(OPTIMAL)" << endl;
        uint64_t obj3 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj3 << endl;
        int val_x1 = ipamir_val_lit(solver, 1);
        int val_x2 = ipamir_val_lit(solver, 2);
        cout << "x1 = " << val_x1 << ", x2 = " << val_x2 << endl;
    } else if (result3 == 10) {
        cout << "(SAT - feasible)" << endl;
        uint64_t obj3 = ipamir_val_obj(solver);
        cout << "Objective value: " << obj3 << endl;
    }

    // Clean up
    ipamir_release(solver);
    
    cout << "\n=== Test completed successfully ===" << endl;
    return 0;
}

