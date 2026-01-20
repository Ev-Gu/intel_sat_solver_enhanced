// Copyright(C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
// 
// LocalSearchEngine - Implementation

#include "LocalSearchEngine.hpp"
#include <stdexcept>
#include <random>
#include <algorithm>
#include <iostream>

namespace Topor {
namespace LocalSearch {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

LocalSearchEngine::LocalSearchEngine()
    : m_num_vars(0)
    , m_num_clauses(0)
    , m_num_hard_clauses(0)
    , m_num_soft_clauses(0)
    , m_top_weight(0)
    , m_total_soft_weight(0)
    , m_initialized(false)
    , m_best_solution_feasible(false)
    , m_best_cost(std::numeric_limits<uint64_t>::max())
    , m_hard_unsat_count(0)
    , m_soft_unsat_weight(0)
{}

// ============================================================================
// INITIALIZATION
// ============================================================================

void LocalSearchEngine::initialize(
    int num_vars,
    const std::vector<Clause>& clauses,
    int64_t top_weight,
    const LocalSearchConfig& config
) {
    m_num_vars = num_vars;
    m_num_clauses = static_cast<int>(clauses.size());
    m_top_weight = top_weight;
    m_config = config;
    
    // Count hard and soft clauses
    m_num_hard_clauses = 0;
    m_num_soft_clauses = 0;
    m_total_soft_weight = 0;
    
    for (const auto& clause : clauses) {
        if (clause.is_hard || clause.original_weight >= top_weight) {
            m_num_hard_clauses++;
        } else {
            m_num_soft_clauses++;
            m_total_soft_weight += clause.original_weight;
        }
    }
    
    // Allocate memory for all data structures
    allocateMemory(clauses);
    
    // Build variable-to-literal mapping
    buildVarLitMapping(clauses);
    
    // Initialize best solution tracking
    m_best_solution_feasible = false;
    m_best_cost = m_total_soft_weight + 1;
    
    m_initialized = true;
}

// ============================================================================
// MAIN SEARCH
// ============================================================================

LocalSearchStats LocalSearchEngine::runSearch(const std::vector<int>& initial_solution) {
    if (!m_initialized) {
        throw std::runtime_error("LocalSearchEngine not initialized");
    }
    
    LocalSearchStats stats;
    m_start_time = std::chrono::high_resolution_clock::now();
    
    // Adjust parameters based on problem characteristics
    tuneParameters();
    
    for (stats.tries = 1; stats.tries <= m_config.max_tries; ++stats.tries) {
        // Initialize for this try
        initializeTry(initial_solution);
        
        int64_t current_max_flips = m_config.max_flips;
        
        for (int64_t step = 1; step < current_max_flips; ++step) {
            stats.total_flips++;
            
            // Check if we found a better solution
            if (m_hard_unsat_count == 0) {
                if (m_soft_unsat_weight < m_best_cost || !m_best_solution_feasible) {
                    // Found a better or first feasible solution
                    current_max_flips = step + m_config.max_non_improve_flips;
                    
                    if (m_soft_unsat_weight < m_best_cost) {
                        m_best_solution_feasible = true;
                        m_best_cost = m_soft_unsat_weight;
                        stats.best_cost = m_best_cost;
                        stats.best_solution_time = getElapsedTime();
                        stats.found_feasible = true;
                        
                        // Save the current solution as best
                        m_best_solution = m_current_solution;
                        
                        std::cout << "c LS: Found solution with cost " 
                                 << m_best_cost << " at time " 
                                 << stats.best_solution_time << "s" << std::endl;
                    }
                    
                    // Check for optimal solution (cost = 0)
                    if (m_best_cost == 0) {
                        return stats;
                    }
                }
            }
            
            // Pick a variable to flip
            int flip_var = pickVariable();
            
            // Flip the variable
            flipVariable(flip_var);
            
            // Update timestamp
            m_time_stamp[flip_var] = step;
            
            // Check time limit
            if (getElapsedTime() > m_config.cutoff_time) {
                return stats;
            }
        }
    }
    
    return stats;
}

// ============================================================================
// SOLUTION ACCESS
// ============================================================================

const std::vector<int>& LocalSearchEngine::getBestSolution() const {
    return m_best_solution;
}

uint64_t LocalSearchEngine::getBestCost() const {
    return m_best_cost;
}

bool LocalSearchEngine::hasFeasibleSolution() const {
    return m_best_solution_feasible;
}

// ============================================================================
// MEMORY ALLOCATION
// ============================================================================

void LocalSearchEngine::allocateMemory(const std::vector<Clause>& clauses) {
    int var_size = m_num_vars + 1;
    int clause_size = m_num_clauses;
    
    // Variable information
    m_score.resize(var_size, 0);
    m_time_stamp.resize(var_size, 0);
    m_current_solution.resize(var_size, 0);
    m_best_solution.resize(var_size, 0);
    
    // Clause information
    m_clause_weight.resize(clause_size, 0);
    m_original_weight.resize(clause_size, 0);
    m_sat_count.resize(clause_size, 0);
    m_sat_var.resize(clause_size, 0);
    m_is_hard.resize(clause_size, false);
    m_clause_literals.resize(clause_size);
    
    // Copy clause data
    for (int c = 0; c < clause_size; ++c) {
        m_original_weight[c] = clauses[c].original_weight;
        m_is_hard[c] = clauses[c].is_hard || clauses[c].original_weight >= m_top_weight;
        m_clause_literals[c] = clauses[c].literals;
    }
    
    // Stacks for unsat clauses
    m_hard_unsat_stack.reserve(clause_size);
    m_soft_unsat_stack.reserve(clause_size);
    m_index_in_hard_unsat.resize(clause_size, -1);
    m_index_in_soft_unsat.resize(clause_size, -1);
    
    // Good variable stack
    m_goodvar_stack.reserve(var_size);
    m_in_goodvar_stack.resize(var_size, -1);
    
    // Variable-to-literal mapping (will be built later)
    m_var_literals.resize(var_size);
}

// ============================================================================
// BUILD VAR-LIT MAPPING
// ============================================================================

void LocalSearchEngine::buildVarLitMapping(const std::vector<Clause>& clauses) {
    // Count how many times each variable appears
    std::vector<int> var_count(m_num_vars + 1, 0);
    for (int c = 0; c < m_num_clauses; ++c) {
        for (const auto& lit : m_clause_literals[c]) {
            var_count[lit.var_num]++;
        }
    }
    
    // Allocate space for each variable's literal list
    for (int v = 1; v <= m_num_vars; ++v) {
        m_var_literals[v].reserve(var_count[v]);
    }
    
    // Fill in the literal lists
    for (int c = 0; c < m_num_clauses; ++c) {
        for (const auto& lit : m_clause_literals[c]) {
            m_var_literals[lit.var_num].push_back(lit);
        }
    }
}

// ============================================================================
// PARAMETER TUNING
// ============================================================================

void LocalSearchEngine::tuneParameters() {
    // Determine if the problem is weighted
    bool is_weighted = false;
    for (int c = 0; c < m_num_clauses; ++c) {
        if (!m_is_hard[c] && m_original_weight[c] != 1) {
            is_weighted = true;
            break;
        }
    }
    
    if (is_weighted) {
        // Weighted MaxSAT settings
        m_config.h_inc = 3;
        m_config.softclause_weight_threshold = 0;
        
        int64_t avg_weight = m_total_soft_weight / std::max(1, m_num_soft_clauses);
        if (avg_weight > 10000) {
            m_config.h_inc = 300;
            m_config.softclause_weight_threshold = 500;
        }
    } else {
        // Unweighted MaxSAT settings
        m_config.h_inc = 1;
        m_config.softclause_weight_threshold = (m_num_vars < 1100) ? 0 : 400;
    }
}

// ============================================================================
// TRY INITIALIZATION
// ============================================================================

void LocalSearchEngine::initializeTry(const std::vector<int>& initial_solution) {
    // Initialize clause weights (SatLike strategy)
    for (int c = 0; c < m_num_clauses; ++c) {
        if (m_is_hard[c]) {
            m_clause_weight[c] = 1;  // Hard clauses start with weight 1
        } else {
            // Soft clauses start with original weight (multiplied for emphasis)
            m_clause_weight[c] = m_original_weight[c] * 10;
        }
    }
    
    // Initialize solution
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    
    for (int v = 1; v <= m_num_vars; ++v) {
        if (!initial_solution.empty() && v < static_cast<int>(initial_solution.size())) {
            m_current_solution[v] = initial_solution[v];
            if (m_current_solution[v] != 0 && m_current_solution[v] != 1) {
                m_current_solution[v] = dis(gen);
            }
        } else {
            m_current_solution[v] = dis(gen);
        }
        m_time_stamp[v] = 0;
    }
    
    // Clear stacks
    m_hard_unsat_stack.clear();
    m_soft_unsat_stack.clear();
    m_goodvar_stack.clear();
    std::fill(m_index_in_hard_unsat.begin(), m_index_in_hard_unsat.end(), -1);
    std::fill(m_index_in_soft_unsat.begin(), m_index_in_soft_unsat.end(), -1);
    std::fill(m_in_goodvar_stack.begin(), m_in_goodvar_stack.end(), -1);
    
    // Calculate initial sat_count and unsat stacks
    m_hard_unsat_count = 0;
    m_soft_unsat_weight = 0;
    
    for (int c = 0; c < m_num_clauses; ++c) {
        m_sat_count[c] = 0;
        
        for (const auto& lit : m_clause_literals[c]) {
            if (m_current_solution[lit.var_num] == (lit.sense ? 1 : 0)) {
                m_sat_count[c]++;
                m_sat_var[c] = lit.var_num;
            }
        }
        
        if (m_sat_count[c] == 0) {
            addToUnsatStack(c);
        }
    }
    
    // Calculate initial scores
    std::fill(m_score.begin(), m_score.end(), 0);
    
    for (int v = 1; v <= m_num_vars; ++v) {
        for (const auto& lit : m_var_literals[v]) {
            int c = lit.clause_num;
            
            if (m_sat_count[c] == 0) {
                // Clause is unsatisfied, flipping this var would satisfy it
                m_score[v] += m_clause_weight[c];
            } else if (m_sat_count[c] == 1 && lit.sense == (m_current_solution[v] == 1)) {
                // This var is the only one satisfying the clause
                m_score[v] -= m_clause_weight[c];
            }
        }
    }
    
    // Initialize good variable stack
    for (int v = 1; v <= m_num_vars; ++v) {
        if (m_score[v] > 0) {
            m_in_goodvar_stack[v] = static_cast<int>(m_goodvar_stack.size());
            m_goodvar_stack.push_back(v);
        }
    }
}

// ============================================================================
// VARIABLE PICKING
// ============================================================================

int LocalSearchEngine::pickVariable() {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // If there are good variables (score > 0)
    if (!m_goodvar_stack.empty()) {
        // Random decision with small probability
        std::uniform_real_distribution<> prob_dis(0.0, 1.0);
        if (prob_dis(gen) < m_config.rdprob) {
            std::uniform_int_distribution<> idx_dis(0, static_cast<int>(m_goodvar_stack.size()) - 1);
            return m_goodvar_stack[idx_dis(gen)];
        }
        
        // BMS: Best from Multiple Samples
        int best_var = m_goodvar_stack[0];
        
        int sample_count = std::min(
            static_cast<int>(m_goodvar_stack.size()),
            m_config.hd_count_threshold
        );
        
        for (int i = 1; i < sample_count; ++i) {
            int v = m_goodvar_stack[i];
            if (m_score[v] > m_score[best_var]) {
                best_var = v;
            } else if (m_score[v] == m_score[best_var] && 
                      m_time_stamp[v] < m_time_stamp[best_var]) {
                best_var = v;
            }
        }
        
        return best_var;
    }
    
    // No good variables - we're at a local minimum
    // Update clause weights
    updateClauseWeights();
    
    // Select a random unsatisfied clause
    int selected_clause;
    if (!m_hard_unsat_stack.empty()) {
        std::uniform_int_distribution<> idx_dis(0, static_cast<int>(m_hard_unsat_stack.size()) - 1);
        selected_clause = m_hard_unsat_stack[idx_dis(gen)];
    } else {
        std::uniform_int_distribution<> idx_dis(0, static_cast<int>(m_soft_unsat_stack.size()) - 1);
        selected_clause = m_soft_unsat_stack[idx_dis(gen)];
    }
    
    // Random walk with small probability
    std::uniform_real_distribution<> prob_dis(0.0, 1.0);
    if (prob_dis(gen) < m_config.rwprob) {
        const auto& lits = m_clause_literals[selected_clause];
        std::uniform_int_distribution<> lit_dis(0, static_cast<int>(lits.size()) - 1);
        return lits[lit_dis(gen)].var_num;
    }
    
    // Pick the best variable from the selected clause
    const auto& lits = m_clause_literals[selected_clause];
    int best_var = lits[0].var_num;
    
    for (size_t i = 1; i < lits.size(); ++i) {
        int v = lits[i].var_num;
        if (m_score[v] > m_score[best_var]) {
            best_var = v;
        } else if (m_score[v] == m_score[best_var] && 
                  m_time_stamp[v] < m_time_stamp[best_var]) {
            best_var = v;
        }
    }
    
    return best_var;
}

// ============================================================================
// VARIABLE FLIPPING
// ============================================================================

void LocalSearchEngine::flipVariable(int flip_var) {
    // Store original score for final update
    int64_t original_score = m_score[flip_var];
    
    // Flip the variable
    m_current_solution[flip_var] = 1 - m_current_solution[flip_var];
    
    // Update all affected clauses
    for (const auto& lit : m_var_literals[flip_var]) {
        int c = lit.clause_num;
        
        // Check if this flip helped or hurt the clause
        bool now_satisfies = (m_current_solution[flip_var] == (lit.sense ? 1 : 0));
        
        if (now_satisfies) {
            // This literal now satisfies the clause
            m_sat_count[c]++;
            
            if (m_sat_count[c] == 1) {
                // Clause went from unsatisfied to satisfied
                m_sat_var[c] = flip_var;
                
                // Update scores: all vars in this clause lose potential gain
                for (const auto& other_lit : m_clause_literals[c]) {
                    m_score[other_lit.var_num] -= m_clause_weight[c];
                }
                
                removeFromUnsatStack(c);
            } else if (m_sat_count[c] == 2) {
                // The previously critical variable is no longer critical
                m_score[m_sat_var[c]] += m_clause_weight[c];
            }
        } else {
            // This literal no longer satisfies the clause
            m_sat_count[c]--;
            
            if (m_sat_count[c] == 0) {
                // Clause became unsatisfied
                for (const auto& other_lit : m_clause_literals[c]) {
                    m_score[other_lit.var_num] += m_clause_weight[c];
                }
                
                addToUnsatStack(c);
            } else if (m_sat_count[c] == 1) {
                // Find and record the new critical variable
                for (const auto& other_lit : m_clause_literals[c]) {
                    if (m_current_solution[other_lit.var_num] == (other_lit.sense ? 1 : 0)) {
                        m_sat_var[c] = other_lit.var_num;
                        m_score[other_lit.var_num] -= m_clause_weight[c];
                        break;
                    }
                }
            }
        }
    }
    
    // Update the score of the flipped variable
    m_score[flip_var] = -original_score;
    
    // Update good variable stack
    updateGoodvarStack(flip_var);
}

// ============================================================================
// CLAUSE WEIGHT UPDATES
// ============================================================================

void LocalSearchEngine::updateClauseWeights() {
    // Increase weights of unsatisfied hard clauses
    for (int c : m_hard_unsat_stack) {
        m_clause_weight[c] += m_config.h_inc;
        
        for (const auto& lit : m_clause_literals[c]) {
            m_score[lit.var_num] += m_config.h_inc;
            
            // Add to goodvar stack if score became positive
            if (m_score[lit.var_num] > 0 && m_in_goodvar_stack[lit.var_num] == -1) {
                m_in_goodvar_stack[lit.var_num] = static_cast<int>(m_goodvar_stack.size());
                m_goodvar_stack.push_back(lit.var_num);
            }
        }
    }
    
    // Increase weights of unsatisfied soft clauses (up to threshold)
    for (int c : m_soft_unsat_stack) {
        if (m_clause_weight[c] <= m_config.softclause_weight_threshold || 
            m_config.softclause_weight_threshold == 0) {
            m_clause_weight[c]++;
            
            for (const auto& lit : m_clause_literals[c]) {
                m_score[lit.var_num]++;
                
                if (m_score[lit.var_num] > 0 && m_in_goodvar_stack[lit.var_num] == -1) {
                    m_in_goodvar_stack[lit.var_num] = static_cast<int>(m_goodvar_stack.size());
                    m_goodvar_stack.push_back(lit.var_num);
                }
            }
        }
    }
}

// ============================================================================
// UNSAT STACK MANAGEMENT
// ============================================================================

void LocalSearchEngine::addToUnsatStack(int clause) {
    if (m_is_hard[clause]) {
        m_index_in_hard_unsat[clause] = static_cast<int>(m_hard_unsat_stack.size());
        m_hard_unsat_stack.push_back(clause);
        m_hard_unsat_count++;
    } else {
        m_index_in_soft_unsat[clause] = static_cast<int>(m_soft_unsat_stack.size());
        m_soft_unsat_stack.push_back(clause);
        m_soft_unsat_weight += m_original_weight[clause];
    }
}

void LocalSearchEngine::removeFromUnsatStack(int clause) {
    if (m_is_hard[clause]) {
        int index = m_index_in_hard_unsat[clause];
        int last = m_hard_unsat_stack.back();
        m_hard_unsat_stack[index] = last;
        m_index_in_hard_unsat[last] = index;
        m_hard_unsat_stack.pop_back();
        m_index_in_hard_unsat[clause] = -1;
        m_hard_unsat_count--;
    } else {
        int index = m_index_in_soft_unsat[clause];
        int last = m_soft_unsat_stack.back();
        m_soft_unsat_stack[index] = last;
        m_index_in_soft_unsat[last] = index;
        m_soft_unsat_stack.pop_back();
        m_index_in_soft_unsat[clause] = -1;
        m_soft_unsat_weight -= m_original_weight[clause];
    }
}

// ============================================================================
// GOOD VARIABLE STACK MANAGEMENT
// ============================================================================

void LocalSearchEngine::updateGoodvarStack(int flip_var) {
    // Remove variables that are no longer good
    for (int i = static_cast<int>(m_goodvar_stack.size()) - 1; i >= 0; --i) {
        int v = m_goodvar_stack[i];
        if (m_score[v] <= 0) {
            // Remove from stack
            int last = m_goodvar_stack.back();
            m_goodvar_stack[i] = last;
            m_in_goodvar_stack[last] = i;
            m_goodvar_stack.pop_back();
            m_in_goodvar_stack[v] = -1;
        }
    }
    
    // Add neighbors of flipped variable if they became good
    for (const auto& lit : m_var_literals[flip_var]) {
        for (const auto& other_lit : m_clause_literals[lit.clause_num]) {
            int v = other_lit.var_num;
            if (m_score[v] > 0 && m_in_goodvar_stack[v] == -1) {
                m_in_goodvar_stack[v] = static_cast<int>(m_goodvar_stack.size());
                m_goodvar_stack.push_back(v);
            }
        }
    }
}

// ============================================================================
// TIMING
// ============================================================================

double LocalSearchEngine::getElapsedTime() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(now - m_start_time).count();
}

} // namespace LocalSearch
} // namespace Topor

