// Copyright(C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
// 
// LocalSearchEngine - The main Local Search algorithm for MaxSAT
// 
// This class implements the SatLike local search algorithm adapted for
// integration with IntelSAT.

#pragma once

#include "LocalSearchTypes.hpp"
#include <chrono>

namespace Topor {
namespace LocalSearch {

/**
 * @brief The main Local Search engine for MaxSAT optimization
 * 
 * This class implements the SatLike local search algorithm adapted for
 * integration with IntelSAT. It optimizes MaxSAT formulas by iteratively
 * flipping variables to minimize the total weight of unsatisfied soft clauses.
 * 
 * Key concepts:
 * - score[v]: The improvement in objective if variable v is flipped
 *   score = make - break, where:
 *   - make = weight of clauses that become satisfied after flip
 *   - break = weight of clauses that become unsatisfied after flip
 * 
 * - flip: Changing a variable's value from 0 to 1 or vice versa
 * 
 * - goodvar_stack: Variables with positive score (beneficial to flip)
 * 
 * Usage:
 *   LocalSearchEngine engine;
 *   engine.initialize(num_vars, clauses, top_weight, config);
 *   auto stats = engine.runSearch(initial_solution);
 *   auto best = engine.getBestSolution();
 */
class LocalSearchEngine {
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    LocalSearchEngine();
    
    // ========================================================================
    // MAIN INTERFACE
    // ========================================================================
    
    /**
     * @brief Initialize the engine with a MaxSAT formula
     * 
     * @param num_vars Number of variables in the formula
     * @param clauses Vector of clauses (both hard and soft)
     * @param top_weight Weight value that indicates a hard clause
     * @param config Configuration parameters
     */
    void initialize(
        int num_vars,
        const std::vector<Clause>& clauses,
        int64_t top_weight,
        const LocalSearchConfig& config = LocalSearchConfig()
    );
    
    /**
     * @brief Run the local search optimization
     * 
     * @param initial_solution Initial variable assignment (optional)
     *        Vector index = variable number, value = 0 or 1
     * @return LocalSearchStats Statistics about the search
     */
    LocalSearchStats runSearch(const std::vector<int>& initial_solution = {});
    
    /**
     * @brief Get the best solution found
     * 
     * @return Vector where index = variable number, value = 0 or 1
     */
    const std::vector<int>& getBestSolution() const;
    
    /**
     * @brief Get the cost of the best solution
     */
    uint64_t getBestCost() const;
    
    /**
     * @brief Check if a feasible solution was found
     */
    bool hasFeasibleSolution() const;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    void allocateMemory(const std::vector<Clause>& clauses);
    void buildVarLitMapping(const std::vector<Clause>& clauses);
    void tuneParameters();
    void initializeTry(const std::vector<int>& initial_solution);
    int pickVariable();
    void flipVariable(int flip_var);
    void updateClauseWeights();
    void addToUnsatStack(int clause);
    void removeFromUnsatStack(int clause);
    void updateGoodvarStack(int flip_var);
    double getElapsedTime() const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // Problem size
    int m_num_vars;
    int m_num_clauses;
    int m_num_hard_clauses;
    int m_num_soft_clauses;
    int64_t m_top_weight;
    int64_t m_total_soft_weight;
    
    // Configuration
    LocalSearchConfig m_config;
    bool m_initialized;
    
    // Clause data
    std::vector<std::vector<Literal>> m_clause_literals;
    std::vector<int64_t> m_clause_weight;
    std::vector<int64_t> m_original_weight;
    std::vector<bool> m_is_hard;
    std::vector<int> m_sat_count;
    std::vector<int> m_sat_var;
    
    // Variable data
    std::vector<std::vector<Literal>> m_var_literals;
    std::vector<int64_t> m_score;
    std::vector<int64_t> m_time_stamp;
    
    // Solution
    std::vector<int> m_current_solution;
    std::vector<int> m_best_solution;
    bool m_best_solution_feasible;
    uint64_t m_best_cost;
    
    // Unsat clause stacks
    std::vector<int> m_hard_unsat_stack;
    std::vector<int> m_soft_unsat_stack;
    std::vector<int> m_index_in_hard_unsat;
    std::vector<int> m_index_in_soft_unsat;
    int m_hard_unsat_count;
    uint64_t m_soft_unsat_weight;
    
    // Good variable stack
    std::vector<int> m_goodvar_stack;
    std::vector<int> m_in_goodvar_stack;
    
    // Timing
    std::chrono::high_resolution_clock::time_point m_start_time;
};

} // namespace LocalSearch
} // namespace Topor

