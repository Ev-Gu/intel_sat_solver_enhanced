// Copyright(C) 2026 Intel Corporation
// SPDX-License-Identifier: MIT
// 
// IntelSATAdapter - Implementation

#include "IntelSATAdapter.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace Topor {
namespace LocalSearch {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

IntelSATAdapter::IntelSATAdapter(int64_t top_weight) 
    : m_top_weight(top_weight)
    , m_num_vars(0)
    , m_engine_initialized(false)
{}

// ============================================================================
// PROBLEM BUILDING
// ============================================================================

void IntelSATAdapter::reset() {
    m_clauses.clear();
    m_num_vars = 0;
    m_initial_solution.clear();
    m_engine_initialized = false;
}

void IntelSATAdapter::addHardClause(const std::vector<int32_t>& literals) {
    Clause clause;
    clause.is_hard = true;
    clause.original_weight = m_top_weight;
    clause.current_weight = m_top_weight;
    
    int clause_idx = static_cast<int>(m_clauses.size());
    
    for (int32_t lit : literals) {
        if (lit == 0) break;  // End of clause marker
        
        Literal l;
        l.clause_num = clause_idx;
        l.var_num = std::abs(lit);
        l.sense = (lit > 0);
        
        clause.literals.push_back(l);
        
        // Track maximum variable
        m_num_vars = std::max(m_num_vars, l.var_num);
    }
    
    if (!clause.literals.empty()) {
        m_clauses.push_back(std::move(clause));
    }
}

void IntelSATAdapter::addSoftClause(const std::vector<int32_t>& literals, int64_t weight) {
    Clause clause;
    clause.is_hard = false;
    clause.original_weight = weight;
    clause.current_weight = weight;
    
    int clause_idx = static_cast<int>(m_clauses.size());
    
    for (int32_t lit : literals) {
        if (lit == 0) break;
        
        Literal l;
        l.clause_num = clause_idx;
        l.var_num = std::abs(lit);
        l.sense = (lit > 0);
        
        clause.literals.push_back(l);
        
        m_num_vars = std::max(m_num_vars, l.var_num);
    }
    
    if (!clause.literals.empty()) {
        m_clauses.push_back(std::move(clause));
    }
}

// ============================================================================
// INITIAL SOLUTION
// ============================================================================

void IntelSATAdapter::setInitialSolution(const std::vector<int>& solution) {
    m_initial_solution = solution;
}

void IntelSATAdapter::setInitialSolutionFromModel(const std::vector<TToporLitVal>& model) {
    m_initial_solution.resize(m_num_vars + 1, 0);
    
    // model[0] is unused, model[i] is the value of variable i
    for (size_t v = 1; v <= std::min(model.size() - 1, static_cast<size_t>(m_num_vars)); ++v) {
        switch (model[v]) {
            case TToporLitVal::VAL_SATISFIED:
                m_initial_solution[v] = 1;
                break;
            case TToporLitVal::VAL_UNSATISFIED:
                m_initial_solution[v] = 0;
                break;
            case TToporLitVal::VAL_UNASSIGNED:
            case TToporLitVal::VAL_DONT_CARE:
            default:
                // For unassigned variables, start with a random value
                m_initial_solution[v] = (std::rand() % 2);
                break;
        }
    }
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void IntelSATAdapter::setConfig(const LocalSearchConfig& config) {
    m_config = config;
}

void IntelSATAdapter::setTimeout(double seconds) {
    m_config.cutoff_time = seconds;
}

// ============================================================================
// OPTIMIZATION
// ============================================================================

LocalSearchStats IntelSATAdapter::optimize() {
    if (!m_engine_initialized) {
        m_engine.initialize(m_num_vars, m_clauses, m_top_weight, m_config);
        m_engine_initialized = true;
    }
    
    return m_engine.runSearch(m_initial_solution);
}

// ============================================================================
// SOLUTION ACCESS
// ============================================================================

const std::vector<int>& IntelSATAdapter::getBestSolution() const {
    return m_engine.getBestSolution();
}

bool IntelSATAdapter::getVariableValue(int var) const {
    const auto& sol = m_engine.getBestSolution();
    if (var >= 0 && var < static_cast<int>(sol.size())) {
        return sol[var] == 1;
    }
    return false;
}

uint64_t IntelSATAdapter::getBestCost() const {
    return m_engine.getBestCost();
}

bool IntelSATAdapter::hasFeasibleSolution() const {
    return m_engine.hasFeasibleSolution();
}

// ============================================================================
// PROBLEM INFO
// ============================================================================

int IntelSATAdapter::getNumVars() const {
    return m_num_vars;
}

int IntelSATAdapter::getNumClauses() const {
    return static_cast<int>(m_clauses.size());
}

// ============================================================================
// INTELSAT COMPATIBILITY
// ============================================================================

std::vector<TToporLitVal> IntelSATAdapter::exportSolutionToModel() const {
    const auto& sol = m_engine.getBestSolution();
    std::vector<TToporLitVal> model(sol.size(), TToporLitVal::VAL_UNASSIGNED);
    
    for (size_t v = 1; v < sol.size(); ++v) {
        model[v] = (sol[v] == 1) ? TToporLitVal::VAL_SATISFIED : TToporLitVal::VAL_UNSATISFIED;
    }
    
    return model;
}

TToporLitVal IntelSATAdapter::getLitValue(int32_t lit) const {
    if (lit == 0) return TToporLitVal::VAL_UNASSIGNED;
    
    int var = std::abs(lit);
    bool positive = (lit > 0);
    
    const auto& sol = m_engine.getBestSolution();
    if (var >= static_cast<int>(sol.size())) {
        return TToporLitVal::VAL_UNASSIGNED;
    }
    
    bool var_true = (sol[var] == 1);
    
    // If lit is positive, it's satisfied when var is true
    // If lit is negative, it's satisfied when var is false
    bool lit_satisfied = (positive == var_true);
    
    return lit_satisfied ? TToporLitVal::VAL_SATISFIED : TToporLitVal::VAL_UNSATISFIED;
}

bool IntelSATAdapter::validate() const {
    if (m_clauses.empty()) {
        std::cerr << "LocalSearch: Warning - No clauses added" << std::endl;
        return true;  // Empty problem is valid but trivial
    }
    
    for (size_t i = 0; i < m_clauses.size(); ++i) {
        const auto& clause = m_clauses[i];
        
        if (clause.literals.empty()) {
            std::cerr << "LocalSearch: Error - Clause " << i << " is empty" << std::endl;
            return false;
        }
        
        for (const auto& lit : clause.literals) {
            if (lit.var_num <= 0 || lit.var_num > m_num_vars) {
                std::cerr << "LocalSearch: Error - Invalid variable " 
                          << lit.var_num << " in clause " << i << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

std::string IntelSATAdapter::getStatistics() const {
    int hard_count = 0;
    int soft_count = 0;
    int64_t total_soft_weight = 0;
    
    for (const auto& clause : m_clauses) {
        if (clause.is_hard) {
            hard_count++;
        } else {
            soft_count++;
            total_soft_weight += clause.original_weight;
        }
    }
    
    return "LocalSearch Statistics:\n"
           "  Variables: " + std::to_string(m_num_vars) + "\n"
           "  Hard clauses: " + std::to_string(hard_count) + "\n"
           "  Soft clauses: " + std::to_string(soft_count) + "\n"
           "  Total soft weight: " + std::to_string(total_soft_weight) + "\n";
}

} // namespace LocalSearch
} // namespace Topor

