// Copyright(C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
// 
// IntelSATAdapter - Bridge between IntelSAT and Local Search
// 
// This class provides an interface to use Local Search optimization
// with IntelSAT's data structures and conventions.

#pragma once

#include "LocalSearchTypes.hpp"
#include "LocalSearchEngine.hpp"
#include "ToporExternalTypes.hpp"
#include <string>

namespace Topor {
namespace LocalSearch {

/**
 * @brief Adapter class for integrating Local Search with IntelSAT
 * 
 * This class provides a bridge between IntelSAT's data structures and
 * the LocalSearchEngine. It handles:
 * - Converting IntelSAT's TLit format to Literal format
 * - Building clauses from IntelSAT's clause representation
 * - Running local search and returning results
 * 
 * Usage:
 *   IntelSATAdapter adapter;
 *   adapter.addHardClause({1, -2, 3});
 *   adapter.addSoftClause({-1, 2}, 5);  // weight = 5
 *   adapter.setInitialSolution(solution_from_sat);
 *   auto result = adapter.optimize();
 */
class IntelSATAdapter {
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param top_weight The weight value that indicates a hard clause
     */
    explicit IntelSATAdapter(int64_t top_weight = INT64_MAX);
    
    // ========================================================================
    // PROBLEM BUILDING
    // ========================================================================
    
    /**
     * @brief Reset the adapter for a new problem
     */
    void reset();
    
    /**
     * @brief Add a hard clause (must be satisfied)
     * 
     * @param literals Vector of literals (positive = true, negative = false)
     *                 Using IntelSAT's TLit convention
     */
    void addHardClause(const std::vector<int32_t>& literals);
    
    /**
     * @brief Add a soft clause (want to satisfy, has weight)
     * 
     * @param literals Vector of literals
     * @param weight Weight of the clause (cost if not satisfied)
     */
    void addSoftClause(const std::vector<int32_t>& literals, int64_t weight);
    
    // ========================================================================
    // INITIAL SOLUTION
    // ========================================================================
    
    /**
     * @brief Set the initial solution from a vector
     * 
     * @param solution Vector where index = variable, value = 0 or 1
     */
    void setInitialSolution(const std::vector<int>& solution);
    
    /**
     * @brief Set the initial solution from IntelSAT's TLit values
     * 
     * @param var_values Function that returns the value of a variable
     *                   true = 1, false = 0
     */
    template<typename F>
    void setInitialSolutionFromSolver(F&& var_values) {
        m_initial_solution.resize(m_num_vars + 1, 0);
        for (int v = 1; v <= m_num_vars; ++v) {
            m_initial_solution[v] = var_values(v) ? 1 : 0;
        }
    }
    
    /**
     * @brief Set initial solution from IntelSAT's TToporLitVal model
     * 
     * @param model Vector of TToporLitVal values from GetModel()
     */
    void setInitialSolutionFromModel(const std::vector<TToporLitVal>& model);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Configure the local search parameters
     */
    void setConfig(const LocalSearchConfig& config);
    
    /**
     * @brief Set timeout in seconds
     */
    void setTimeout(double seconds);
    
    // ========================================================================
    // OPTIMIZATION
    // ========================================================================
    
    /**
     * @brief Run the local search optimization
     * 
     * @return LocalSearchStats Statistics about the search
     */
    LocalSearchStats optimize();
    
    // ========================================================================
    // SOLUTION ACCESS
    // ========================================================================
    
    /**
     * @brief Get the best solution found
     * 
     * @return Vector where index = variable number, value = 0 or 1
     */
    const std::vector<int>& getBestSolution() const;
    
    /**
     * @brief Get the value of a specific variable in the best solution
     * 
     * @param var Variable number (1-based)
     * @return true if variable is assigned 1, false if 0
     */
    bool getVariableValue(int var) const;
    
    /**
     * @brief Get the cost of the best solution
     * 
     * @return Sum of weights of unsatisfied soft clauses
     */
    uint64_t getBestCost() const;
    
    /**
     * @brief Check if a feasible solution was found
     */
    bool hasFeasibleSolution() const;
    
    // ========================================================================
    // PROBLEM INFO
    // ========================================================================
    
    /**
     * @brief Get the number of variables
     */
    int getNumVars() const;
    
    /**
     * @brief Get the number of clauses
     */
    int getNumClauses() const;
    
    // ========================================================================
    // INTELSAT COMPATIBILITY
    // ========================================================================
    
    /**
     * @brief Export the best solution to IntelSAT's TToporLitVal format
     * 
     * @return Vector of TToporLitVal values compatible with IntelSAT
     */
    std::vector<TToporLitVal> exportSolutionToModel() const;
    
    /**
     * @brief Get the literal value for a specific literal (in IntelSAT format)
     * 
     * @param lit The literal in IntelSAT format (positive = true, negative = false)
     * @return TToporLitVal indicating the literal's value in the best solution
     */
    TToporLitVal getLitValue(int32_t lit) const;
    
    /**
     * @brief Validate the adapter configuration
     * 
     * @return true if valid, false otherwise
     */
    bool validate() const;
    
    /**
     * @brief Get statistics about the problem
     * 
     * @return String with problem statistics
     */
    std::string getStatistics() const;

private:
    int64_t m_top_weight;
    int m_num_vars;
    std::vector<Clause> m_clauses;
    std::vector<int> m_initial_solution;
    LocalSearchConfig m_config;
    LocalSearchEngine m_engine;
    bool m_engine_initialized;
};

} // namespace LocalSearch
} // namespace Topor

