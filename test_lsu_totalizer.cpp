// Test file for LSUManager with Totalizer
// Compile with: g++ -std=c++20 -DSKIP_ZLIB -o test_lsu_totalizer test_lsu_totalizer.cpp Topor.cc Topi.cc TopiAsg.cc TopiBacktrack.cc TopiBcp.cc TopiBitCompression.cc TopiCompression.cc TopiConflictAnalysis.cc TopiDebugPrinting.cc TopiDecision.cc TopiInprocess.cc TopiRestart.cc TopiStatistics.cc TopiVarScores.cc TopiWL.cc -lpthread

#include <iostream>
#include <vector>
#include <cassert>
#include "Topor.hpp"
#include "algorithms/LSUManager.hpp"

using namespace std;
using namespace Topor;

int main() {
    cout << "=== LSU Manager + Totalizer Test ===" << endl;
    
    // Test Case 1: Simple MaxSAT instance
    // Hard clauses: (1 v 2), (-1 v 3)
    // Soft clauses: (-2), (-3) with weights 1 each
    
    cout << "\nTest Case 1: Simple MaxSAT Problem" << endl;
    cout << "Hard clauses:" << endl;
    cout << "  (1 v 2)" << endl;
    cout << "  (-1 v 3)" << endl;
    cout << "Soft clauses with weights:" << endl;
    cout << "  (-2) weight=1" << endl;
    cout << "  (-3) weight=1" << endl;
    
    try {
        // Create Topor solver
        CTopor<int32_t, uint32_t, false> solver(10);
        int next_var = 3;  // We already have variables 1, 2, 3
        
        // Add hard clauses
        cout << "\nAdding hard clauses..." << endl;
        solver.AddClause({1, 2, 0});      // Hard: (1 v 2)
        solver.AddClause({-1, 3, 0});     // Hard: (-1 v 3)
        
        // Prepare soft clauses with relaxation variables
        cout << "Preparing soft clauses with relaxation variables..." << endl;
        vector<int> relaxation_vars;
        vector<int> soft_clause_1 = {-2};
        vector<int> soft_clause_2 = {-3};
        
        // Soft clause 1: (-2 v r1)
        int r1 = ++next_var;  // r1 = 4
        relaxation_vars.push_back(r1);
        cout << "Soft clause 1: (-2 v r" << r1 << ")" << endl;
        solver.AddClause({-2, r1, 0});
        
        // Soft clause 2: (-3 v r2)
        int r2 = ++next_var;  // r2 = 5
        relaxation_vars.push_back(r2);
        cout << "Soft clause 2: (-3 v r" << r2 << ")" << endl;
        solver.AddClause({-3, r2, 0});
        
        cout << "\nInitial Solve (before LSU)..." << endl;
        TToporReturnVal initial_res = solver.Solve();
        cout << "Result: " << (initial_res == TToporReturnVal::RET_SAT ? "SAT" : "UNSAT/OTHER") << endl;
        
        if (initial_res == TToporReturnVal::RET_SAT) {
            cout << "Solution found:" << endl;
            cout << "  Variable assignments:" << endl;
            for (int i = 1; i <= next_var; ++i) {
                TToporLitVal val = solver.GetLitValue(i);
                cout << "    Var " << i << " = " 
                     << (val == TToporLitVal::VAL_SATISFIED ? "TRUE" : 
                         val == TToporLitVal::VAL_UNSATISFIED ? "FALSE" : "UNASSIGNED") << endl;
            }
            
            // Check relaxation variable values
            cout << "  Relaxation variables status:" << endl;
            int weight = 0;
            for (int r_i : relaxation_vars) {
                TToporLitVal val = solver.GetLitValue(r_i);
                bool satisfied = (val == TToporLitVal::VAL_SATISFIED);
                if (satisfied) weight++;
                cout << "    r" << r_i << " = " 
                     << (satisfied ? "TRUE (soft clause VIOLATED)" : "FALSE (soft clause SATISFIED)") << endl;
            }
            cout << "  Current cost (violations): " << weight << endl;
        }
        
        cout << "\n=== Creating LSUManager ===" << endl;
        LSUManager lsu_manager(solver, next_var, relaxation_vars);
        cout << "LSUManager created successfully!" << endl;
        cout << "Number of relaxation variables: " << relaxation_vars.size() << endl;
        
        // Note: run_optimization() requires Totalizer to be properly implemented
        // For now, we just verify the manager is created and can be called
        cout << "\nLSUManager ready for optimization." << endl;
        cout << "Best weight before optimization: " << lsu_manager.get_best_weight() << endl;
        
        cout << "\n=== Test Case 1 PASSED ===" << endl;
        
    } catch (const exception& e) {
        cerr << "ERROR in Test Case 1: " << e.what() << endl;
        return 1;
    }
    
    // Test Case 2: Verify Totalizer can be created
    cout << "\n\nTest Case 2: Totalizer Creation" << endl;
    try {
        vector<int> test_relaxation_vars = {4, 5, 6};
        int test_next_var = 6;
        
        cout << "Creating Totalizer with " << test_relaxation_vars.size() << " relaxation variables..." << endl;
        Totalizer totalizer(test_next_var, test_relaxation_vars);
        totalizer.build();
        
        const auto& clauses = totalizer.get_clauses();
        cout << "Totalizer built successfully!" << endl;
        cout << "Number of encoding clauses: " << clauses.size() << endl;
        cout << "Sample clause details:" << endl;
        
        int sample_count = 0;
        for (const auto& clause : clauses) {
            if (sample_count++ < 3) {
                cout << "  Clause with " << clause.size() << " literals" << endl;
            }
        }
        
        cout << "\n=== Test Case 2 PASSED ===" << endl;
        
    } catch (const exception& e) {
        cerr << "ERROR in Test Case 2: " << e.what() << endl;
        return 1;
    }
    
    cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===" << endl;
    return 0;
}
