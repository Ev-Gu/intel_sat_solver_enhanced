/*
 * Example IPAMIR program demonstrating a more complex incremental MaxSAT scenario
 * This example shows:
 * 1. Building a problem incrementally (adding clauses and soft literals over time)
 * 2. Multiple solve calls with incremental modifications
 * 3. Comprehensive result checking
 */

#include "ToporIpamir.h"
#include <iostream>
#include <vector>

using namespace std;

void print_result(void* solver, int result, const string& description) {
    cout << "\n" << description << endl;
    cout << "Result: " << result << " ";
    if (result == 0) {
        cout << "(INTERRUPTED)" << endl;
    } else if (result == 10) {
        cout << "(SAT - feasible)" << endl;
        uint64_t obj = ipamir_val_obj(solver);
        cout << "Objective value: " << obj << endl;
    } else if (result == 20) {
        cout << "(UNSAT)" << endl;
    } else if (result == 30) {
        cout << "(OPTIMAL)" << endl;
        uint64_t obj = ipamir_val_obj(solver);
        cout << "Objective value: " << obj << endl;
    } else if (result == 40) {
        cout << "(ERROR)" << endl;
        return;
    }
    
    if (result == 10 || result == 30) {
        cout << "Variable assignments: ";
        for (int i = 1; i <= 5; i++) {
            int val = ipamir_val_lit(solver, i);
            cout << "x" << i << "=" << val << " ";
        }
        cout << endl;
    }
}

int main() {
    void* solver = ipamir_init();
    if (!solver) {
        cerr << "ERROR: Failed to initialize IPAMIR solver" << endl;
        return 1;
    }

    cout << "IPAMIR solver signature: " << ipamir_signature() << endl;
    cout << "Testing complex incremental MaxSAT scenario..." << endl;

    // Step 1: Add initial hard clauses
    cout << "\n--- Step 1: Adding initial hard clauses ---" << endl;
    
    // Clause 1: (x1 OR x2)
    ipamir_add_hard(solver, 1);
    ipamir_add_hard(solver, 2);
    ipamir_add_hard(solver, 0);
    
    // Clause 2: (-x2 OR x3)
    ipamir_add_hard(solver, -2);
    ipamir_add_hard(solver, 3);
    ipamir_add_hard(solver, 0);
    
    // Clause 3: (x3 OR x4)
    ipamir_add_hard(solver, 3);
    ipamir_add_hard(solver, 4);
    ipamir_add_hard(solver, 0);

    // Step 2: Add soft literals
    cout << "\n--- Step 2: Adding soft literals ---" << endl;
    ipamir_add_soft_lit(solver, 1, 15); // x1 = true costs 15
    ipamir_add_soft_lit(solver, 2, 10); // x2 = true costs 10
    ipamir_add_soft_lit(solver, 3, 8);  // x3 = true costs 8
    ipamir_add_soft_lit(solver, 4, 5);  // x4 = true costs 5

    // Step 3: First solve
    print_result(solver, ipamir_solve(solver), "=== First solve ===");

    // Step 4: Add more hard clauses incrementally
    cout << "\n--- Step 4: Adding more hard clauses ---" << endl;
    
    // Clause 4: (-x1 OR -x3)
    ipamir_add_hard(solver, -1);
    ipamir_add_hard(solver, -3);
    ipamir_add_hard(solver, 0);
    
    // Clause 5: (x4 OR x5)
    ipamir_add_hard(solver, 4);
    ipamir_add_hard(solver, 5);
    ipamir_add_hard(solver, 0);

    // Step 5: Add more soft literals
    ipamir_add_soft_lit(solver, 5, 20); // x5 = true costs 20

    // Step 6: Second solve
    print_result(solver, ipamir_solve(solver), "=== Second solve (with additional constraints) ===");

    // Step 7: Modify soft literal weights
    cout << "\n--- Step 7: Modifying soft literal weights ---" << endl;
    ipamir_add_soft_lit(solver, 1, 30); // Change weight of x1 to 30
    ipamir_add_soft_lit(solver, 4, 1);  // Change weight of x4 to 1

    // Step 8: Third solve
    print_result(solver, ipamir_solve(solver), "=== Third solve (with modified weights) ===");

    // Step 9: Add assumption (harden a soft clause)
    cout << "\n--- Step 9: Adding assumption to harden soft clause ---" << endl;
    ipamir_assume(solver, -1); // Hard constraint: x1 must be false

    // Step 10: Fourth solve with assumption
    print_result(solver, ipamir_solve(solver), "=== Fourth solve (with assumption -x1) ===");

    // Clean up
    ipamir_release(solver);
    
    cout << "\n=== Complex incremental test completed successfully ===" << endl;
    return 0;
}

