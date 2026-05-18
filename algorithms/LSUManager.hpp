#pragma once

#include <vector>

// Forward declarations or inclusion of necessary header files only
#include "Totalizer.hpp"
#include "../Topor.hpp" 

/**
 * LSUManager implements the Linear SAT-UNSAT optimization loop.
 * It manages relaxation variables and uses the Totalizer to tighten the bound
 * based on the weights of found solutions.
 */
class LSUManager {
private:
    Topor::CTopor<>& solver;          // Reference to the Topor solver instance
    int& next_var;                    // Global variable counter

    std::vector<int> relaxation_vars; // r_i variables for each soft clause
    int best_weight;                  // Lowest weight (number of violated soft clauses)
    std::vector<int> best_model;      // Best assignment found so far

public:
    // Constructor - Keeping a short constructor with an initialization list 
    // in the header is standard practice for inline optimization.
    LSUManager(Topor::CTopor<>& s, int& nv, const std::vector<int>& existingRelaxVars);

    // Public API Methods - Declarations only (Implementation resides in the .cpp file)
    void add_hard_clause(const std::vector<int>& lits);
    void add_soft_clause(const std::vector<int>& lits);
    void run_optimization();

    // Short const Getter - Can safely remain inline in the header for performance reasons
    int get_best_weight() const { return best_weight; }

private:
    // Private Helper Methods - Declarations only
    int calculate_current_weight();
    void save_best_model();
};