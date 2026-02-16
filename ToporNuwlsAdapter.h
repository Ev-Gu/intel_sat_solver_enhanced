#ifndef TOPOR_NUWLS_ADAPTER_H
#define TOPOR_NUWLS_ADAPTER_H

#include "Topor.hpp"
#include "algorithms/Alg_nuwls.h"
#include <vector>
#include <memory>
#include <iostream>

namespace ToporNuwls {

/**
 * RAII wrapper for NUWLS clause structures
 * Automatically cleans up allocated memory
 */
class NuwlsClauseData {
public:
    int nvars = 0;
    int nclauses = 0;
    unsigned long long topclauseweight = 0;
    clauselit **clause_lit = nullptr;
    int *clause_lit_count = nullptr;
    long long *clause_weight = nullptr;
    
    ~NuwlsClauseData() {
        cleanup();
    }
    
    void cleanup() {
        if (clause_lit) {
            for (int i = 0; i < nclauses; ++i) {
                delete[] clause_lit[i];
            }
            delete[] clause_lit;
            clause_lit = nullptr;
        }
        delete[] clause_lit_count;
        clause_lit_count = nullptr;
        delete[] clause_weight;
        clause_weight = nullptr;
    }
    
    // Prevent copying
    NuwlsClauseData(const NuwlsClauseData&) = delete;
    NuwlsClauseData& operator=(const NuwlsClauseData&) = delete;
    
    // Allow moving
    NuwlsClauseData() = default;
    NuwlsClauseData(NuwlsClauseData&& other) noexcept {
        *this = std::move(other);
    }
    
    NuwlsClauseData& operator=(NuwlsClauseData&& other) noexcept {
        if (this != &other) {
            cleanup();
            nvars = other.nvars;
            nclauses = other.nclauses;
            topclauseweight = other.topclauseweight;
            clause_lit = other.clause_lit;
            clause_lit_count = other.clause_lit_count;
            clause_weight = other.clause_weight;
            
            other.clause_lit = nullptr;
            other.clause_lit_count = nullptr;
            other.clause_weight = nullptr;
        }
        return *this;
    }
};

/**
 * Build NUWLS data structures from vectors of clauses
 * Used by Main.cc for MaxSAT instances
 */
inline NuwlsClauseData BuildNuwlsFromClauses(
    const std::vector<std::vector<int>>& hardClauses,
    const std::vector<std::vector<int>>& softClauses,
    const std::vector<long long>& softWeights,
    int maxVar)
{
    NuwlsClauseData data;
    
    data.nvars = maxVar;
    data.nclauses = hardClauses.size() + softClauses.size();
    
    // Calculate top weight (sum of all soft weights + 1)
    data.topclauseweight = 1;
    for (auto w : softWeights) {
        data.topclauseweight += w;
    }
    
    // Allocate arrays
    data.clause_lit = new clauselit*[data.nclauses + 10];
    data.clause_lit_count = new int[data.nclauses + 10];
    data.clause_weight = new long long[data.nclauses + 10];
    
    int c = 0;
    
    // Add hard clauses
    for (const auto& clause : hardClauses) {
        data.clause_lit_count[c] = clause.size();
        data.clause_lit[c] = new clauselit[clause.size() + 1];
        
        for (size_t j = 0; j < clause.size(); ++j) {
            int lit = clause[j];
            data.clause_lit[c][j].var_num = abs(lit);
            data.clause_lit[c][j].sense = lit > 0 ? 1 : 0;
        }
        data.clause_lit[c][clause.size()].var_num = 0;
        data.clause_weight[c] = data.topclauseweight;
        c++;
    }
    
    // Add soft clauses
    for (size_t i = 0; i < softClauses.size(); ++i) {
        const auto& clause = softClauses[i];
        data.clause_lit_count[c] = clause.size();
        data.clause_lit[c] = new clauselit[clause.size() + 1];
        
        for (size_t j = 0; j < clause.size(); ++j) {
            int lit = clause[j];
            data.clause_lit[c][j].var_num = abs(lit);
            data.clause_lit[c][j].sense = lit > 0 ? 1 : 0;
        }
        data.clause_lit[c][clause.size()].var_num = 0;
        data.clause_weight[c] = softWeights[i];
        c++;
    }
    
    return data;
}

/**
 * Configuration for NUWLS invocation
 */
struct NuwlsConfig {
    int max_flips = -1;              // -1 means use NUWLS default
    int max_non_improve_flip = -1;   // -1 means use NUWLS default
    int time_limit_seconds = 15;
    bool weighted = true;
    int verbosity = 1;               // 0=silent, 1=normal, 2=verbose
};

/**
 * Result from NUWLS run
 */
struct NuwlsResult {
    bool improved = false;
    unsigned long long initial_cost = 0;
    unsigned long long final_cost = 0;
    std::vector<int> best_solution;  // 1-indexed (index 0 unused)
};

/**
 * Run NUWLS local search on a Topor instance
 * Template allows working with any Topor instantiation
 * 
 * @param topor - Topor solver instance (must have a SAT model)
 * @param data - NUWLS clause data structures
 * @param relaxVars - Relaxation variables with weights (for cost calculation)
 * @param config - NUWLS configuration
 * @return NuwlsResult containing improvement info and best solution
 */
template<typename TLit, typename TUInd, bool Compress>
NuwlsResult RunNuwlsOnTopor(
    Topor::CTopor<TLit, TUInd, Compress>& topor,
    NuwlsClauseData& data,
    const std::vector<std::pair<long long, TLit>>& relaxVars,
    const NuwlsConfig& config = NuwlsConfig())
{
    using namespace Topor;
    
    NuwlsResult result;
    
    if (config.verbosity > 0) {
        std::cout << "c Starting NUWLS local search" << std::endl;
        std::cout << "c NUWLS variables: " << data.nvars 
                  << ", clauses: " << data.nclauses << std::endl;
    }
    
    // Create and configure NUWLS solver
    NUWLS nuwls_solver;
    nuwls_solver.problem_weighted = config.weighted ? 1 : 0;
    
    nuwls_solver.build_instance(
        data.nvars, 
        data.nclauses, 
        data.topclauseweight,
        data.clause_lit, 
        data.clause_lit_count, 
        data.clause_weight
    );
    
    nuwls_solver.settings();
    
    // Override default settings if specified
    if (config.max_flips > 0) {
        nuwls_solver.max_flips = config.max_flips;
    }
    if (config.max_non_improve_flip > 0) {
        nuwls_solver.max_non_improve_flip = config.max_non_improve_flip;
    }
    
    // Initialize NUWLS with Topor's current model
    std::vector<int> init_solu(data.nvars + 1);
    for (int i = 1; i <= data.nvars; ++i) {
        auto val = topor.GetLitValue(i);
        init_solu[i] = (val == TToporLitVal::VAL_SATISFIED || 
                        val == TToporLitVal::VAL_DONT_CARE) ? 1 : 0;
    }
    
    nuwls_solver.init(init_solu);
    
    // Calculate initial cost from relaxation variables
    result.initial_cost = 0;
    for (const auto& rv : relaxVars) {
        long long weight = rv.first;
        TLit relax_var = rv.second;
        auto val = topor.GetLitValue(relax_var);
        if (val == TToporLitVal::VAL_SATISFIED) {
            result.initial_cost += weight;
        }
    }
    
    nuwls_solver.opt_unsat_weight = result.initial_cost;
    
    if (config.verbosity > 0) {
        std::cout << "o " << result.initial_cost << std::endl;
    }
    
    // Run NUWLS optimization loop
    start_timing();
    int time_limit = config.time_limit_seconds;
    
    for (int step = 1; step < nuwls_solver.max_flips; ++step) {
        if (nuwls_solver.hard_unsat_nb == 0) {
            nuwls_solver.local_soln_feasible = 1;
            
            if (nuwls_solver.soft_unsat_weight < nuwls_solver.opt_unsat_weight) {
                nuwls_solver.max_flips = step + nuwls_solver.max_non_improve_flip;
                time_limit = get_runtime() + config.time_limit_seconds;
                
                nuwls_solver.best_soln_feasible = 1;
                nuwls_solver.opt_unsat_weight = nuwls_solver.soft_unsat_weight;
                
                if (config.verbosity > 0) {
                    std::cout << "o " << nuwls_solver.opt_unsat_weight << std::endl;
                }
                
                if (nuwls_solver.opt_unsat_weight == 0) {
                    break;
                }
            }
        }
        
        int flipvar = nuwls_solver.pick_var();
        
        if (nuwls_solver.if_using_neighbor) {
            nuwls_solver.flip2(flipvar);
        } else {
            nuwls_solver.flip(flipvar);
        }
        
        nuwls_solver.time_stamp[flipvar] = step;
        
        if (step % 1000 == 0) {
            if (get_runtime() > time_limit) {
                if (config.verbosity > 1) {
                    std::cout << "c NUWLS time limit reached" << std::endl;
                }
                break;
            }
        }
    }
    
    // Extract results
    result.final_cost = nuwls_solver.opt_unsat_weight;
    result.improved = (result.final_cost < result.initial_cost);
    
    if (nuwls_solver.best_soln_feasible) {
        result.best_solution.resize(data.nvars + 1);
        for (int v = 1; v <= nuwls_solver.num_vars; ++v) {
            result.best_solution[v] = nuwls_solver.cur_soln[v];
        }
    }
    
    nuwls_solver.free_memory();
    
    if (config.verbosity > 0) {
        std::cout << "c NUWLS finished: " 
                  << (result.improved ? "IMPROVED" : "NO IMPROVEMENT")
                  << " (initial=" << result.initial_cost 
                  << ", final=" << result.final_cost << ")" << std::endl;
    }
    
    return result;
}

/**
 * Helper: Build NUWLS data for IPAMIR-style interface
 * where soft clauses are single literals with weights
 */
inline NuwlsClauseData BuildNuwlsFromIpamir(
    const std::vector<std::vector<int>>& hardClauses,
    const std::unordered_map<int, uint64_t>& softLitWeights,
    int maxVar)
{
    NuwlsClauseData data;
    
    data.nvars = maxVar;
    data.nclauses = hardClauses.size() + softLitWeights.size();
    
    // Calculate top weight
    data.topclauseweight = 1;
    for (const auto& p : softLitWeights) {
        data.topclauseweight += p.second;
    }
    
    // Allocate arrays
    data.clause_lit = new clauselit*[data.nclauses + 10];
    data.clause_lit_count = new int[data.nclauses + 10];
    data.clause_weight = new long long[data.nclauses + 10];
    
    int c = 0;
    
    // Add hard clauses
    for (const auto& clause : hardClauses) {
        data.clause_lit_count[c] = clause.size();
        data.clause_lit[c] = new clauselit[clause.size() + 1];
        
        for (size_t j = 0; j < clause.size(); ++j) {
            int lit = clause[j];
            data.clause_lit[c][j].var_num = abs(lit);
            data.clause_lit[c][j].sense = lit > 0 ? 1 : 0;
        }
        data.clause_lit[c][clause.size()].var_num = 0;
        data.clause_weight[c] = data.topclauseweight;
        c++;
    }
    
    // Add soft clauses (single literals)
    for (const auto& p : softLitWeights) {
        int lit = p.first;
        uint64_t weight = p.second;
        
        data.clause_lit_count[c] = 1;
        data.clause_lit[c] = new clauselit[2];
        data.clause_lit[c][0].var_num = abs(lit);
        data.clause_lit[c][0].sense = lit > 0 ? 1 : 0;
        data.clause_lit[c][1].var_num = 0;
        data.clause_weight[c] = weight;
        c++;
    }
    
    return data;
}

} // namespace ToporNuwls

#endif // TOPOR_NUWLS_ADAPTER_H