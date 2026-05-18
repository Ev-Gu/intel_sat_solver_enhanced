#include <iostream>
#include "algorithms/LSUManager.hpp"

// Constructor utilizing the member initialization list
LSUManager::LSUManager(Topor::CTopor<>& s, int& nv, const std::vector<int>& existingRelaxVars)
    : solver(s), next_var(nv), relaxation_vars(existingRelaxVars), best_weight(-1) {
}


// Adds a soft clause by transforming it: (C) -> (C OR r_i)
void LSUManager::add_soft_clause(const std::vector<int>& lits) {
    // 1. Generate a new relaxation variable and update the global tracker
    int r_i = ++next_var;
    relaxation_vars.push_back(r_i);

    // 2. Copy the original literals into a new transformed clause vector
    std::vector<int> transformed_clause = lits;

    // 3. Append the relaxation variable to the end (creating the logical OR relation)
    transformed_clause.push_back(r_i);

    // 4. Send the entire unified clause to the solver in a single call
    solver.AddClause(transformed_clause);
}

// Executes the descending linear search optimization loop
void LSUManager::run_optimization() {
    // 1. Initialize the Totalizer with all relaxation variables
    Totalizer totalizer(next_var, relaxation_vars);
    totalizer.build();

    // 2. Add all Totalizer encoding clauses directly to the solver
    const auto& encoding_clauses = totalizer.get_clauses();
    for (const auto& clause : encoding_clauses) {
        solver.AddClause(std::span<int>(const_cast<int*>(clause.data()), clause.size()));
    }

    // 3. The Linear Search SAT-UNSAT (LSU) Loop
    while (true) {
        Topor::TToporReturnVal res = solver.Solve();

        if (res == Topor::TToporReturnVal::RET_SAT) {
            // Found a satisfying assignment
            save_best_model();
            int current_weight = calculate_current_weight();

            std::cout << "o " << current_weight << std::endl;

            if (current_weight == 0) {
                // Optimum found (0 violations achieved)
                break;
            }

            // Tighten the bound: Sum(r_i) <= current_weight - 1
            // Force the (current_weight)-th output literal of the totalizer to FALSE
            int bound_lit = totalizer.get_output_lit(current_weight);
            if (bound_lit != -1) {
                // Pass the literal directly as a single argument (utilizing the variadic template)
                solver.AddClause(-bound_lit);
            }
            else {
                break;
            }
        }
        else if (res == Topor::TToporReturnVal::RET_UNSAT) {
            // No better solution exists, current best_model is optimal
            break;
        }
        else {
            // Solver encountered an error or a timeout occurred
            break;
        }
    }
}

// Extracts the assignment from the solver and counts active relaxation variables
int LSUManager::calculate_current_weight() {
    int weight = 0;
    for (int r_i : relaxation_vars) {
        if (solver.GetLitValue(r_i) == Topor::TToporLitVal::VAL_SATISFIED) {
            weight++;
        }
    }
    return weight;
}

// Stores the current satisfying model assignment from the solver
void LSUManager::save_best_model() {
    best_model.clear();

    // Iterating through all variables up to next_var to log their truth values
    for (int i = 1; i <= next_var; ++i) {
        auto val = solver.GetLitValue(i);

        // Corrected enum names based on Topor's specification
        if (val == Topor::TToporLitVal::VAL_SATISFIED) {
            best_model.push_back(i);
        }
        else if (val == Topor::TToporLitVal::VAL_UNSATISFIED) {
            best_model.push_back(-i);
        }
    }
    best_weight = calculate_current_weight();
}