// Copyright(C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
// 
// Local Search Types - Data structures for MaxSAT Local Search
// 
// This file contains all the basic data structures used by the Local Search module.
// It is separate from the algorithm implementation for clean dependency management.

#pragma once

#include <vector>
#include <cstdint>
#include <limits>

namespace Topor {
namespace LocalSearch {

// ============================================================================
// LITERAL
// ============================================================================

/**
 * @brief Represents a literal in a clause
 * 
 * A literal consists of a variable number and a sign (sense).
 * This is the core unit that makes up clauses in the MaxSAT formula.
 * 
 * Example:
 *   Variable 3 positive: Literal(clause_idx, 3, true)
 *   Variable 3 negative: Literal(clause_idx, 3, false)
 */
struct Literal {
    int32_t clause_num;    // Index of the clause this literal belongs to (0-based)
    int32_t var_num;       // Variable number (1-based, as per DIMACS convention)
    bool sense;            // true = positive literal, false = negative literal
    
    // Default constructor
    Literal() : clause_num(-1), var_num(0), sense(true) {}
    
    // Parameterized constructor
    Literal(int32_t c, int32_t v, bool s) : clause_num(c), var_num(v), sense(s) {}
};

// ============================================================================
// CLAUSE
// ============================================================================

/**
 * @brief Represents a clause in the MaxSAT formula
 * 
 * Contains the literals and weight information for a single clause.
 * 
 * In MaxSAT:
 *   - Hard clauses: MUST be satisfied (is_hard = true)
 *   - Soft clauses: WANT to satisfy, have a weight (is_hard = false)
 */
struct Clause {
    std::vector<Literal> literals;      // Literals in this clause
    int64_t original_weight;            // Original weight from the input
    int64_t current_weight;             // Dynamic weight (updated during search)
    bool is_hard;                       // true = hard clause, false = soft clause
    
    // Default constructor
    Clause() : original_weight(1), current_weight(1), is_hard(false) {}
};

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Configuration parameters for the Local Search algorithm
 * 
 * These parameters control the behavior of the SatLike algorithm.
 * Default values are tuned for general MaxSAT instances.
 */
struct LocalSearchConfig {
    // Search limits
    int64_t max_flips;                  // Maximum number of variable flips per try
    int64_t max_non_improve_flips;      // Max flips without improvement before restart
    int max_tries;                      // Maximum number of restart tries
    double cutoff_time;                 // Time limit in seconds
    
    // SatLike-specific parameters
    int h_inc;                          // Hard clause weight increment
    int softclause_weight_threshold;    // Threshold for soft clause weight updates
    double rdprob;                      // Random decision probability
    int hd_count_threshold;             // Threshold for BMS (Best from Multiple Samples)
    double rwprob;                      // Random walk probability
    double smooth_probability;          // Weight smoothing probability
    
    // Default constructor with tuned values
    LocalSearchConfig()
        : max_flips(10000000)
        , max_non_improve_flips(10000000)
        , max_tries(100000000)
        , cutoff_time(300.0)
        , h_inc(1)
        , softclause_weight_threshold(400)
        , rdprob(0.01)
        , hd_count_threshold(15)
        , rwprob(0.1)
        , smooth_probability(0.01)
    {}
};

// ============================================================================
// STATISTICS
// ============================================================================

/**
 * @brief Statistics collected during Local Search execution
 * 
 * This struct is returned after running the search to provide
 * information about the search progress and results.
 */
struct LocalSearchStats {
    int64_t total_flips;        // Total number of variable flips performed
    int tries;                  // Number of restart tries
    double best_solution_time;  // Time when best solution was found (seconds)
    bool found_feasible;        // Whether a feasible solution was found
    uint64_t best_cost;         // Cost of the best solution (sum of unsatisfied soft weights)
    
    // Default constructor
    LocalSearchStats()
        : total_flips(0)
        , tries(0)
        , best_solution_time(0.0)
        , found_feasible(false)
        , best_cost(std::numeric_limits<uint64_t>::max())
    {}
};

} // namespace LocalSearch
} // namespace Topor

