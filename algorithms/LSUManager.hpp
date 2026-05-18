#pragma once

#include <iostream>
#include <vector>
#include <span>

#include "Totalizer.hpp"
#include "../Topor.hpp"

/**
 * LSUManager — Linear SAT-UNSAT for unweighted partial MaxSAT.
 *
 * Each soft clause (C) is relaxed to (C OR r_i). Violation cost = number of r_i assigned true.
 * A Totalizer encodes sum(r_i) <= k. Each SAT result prints "o <cost>"; the bound k is tightened
 * via assumptions on Totalizer outputs until UNSAT (then the last SAT model is optimal).
 *
 * Templated over TTopor so the same code works with Topor32 / Topor64 / Toporc from Main.cc.
 */
template <typename TTopor>
class LSUManager {
private:
    TTopor& solver;
    int& next_var;

    std::vector<int> relaxation_vars;
    int best_weight;
    std::vector<int> best_model;
    bool has_best;

public:
    LSUManager(TTopor& s, int& nv, const std::vector<int>& existingRelaxVars)
        : solver(s), next_var(nv), relaxation_vars(existingRelaxVars),
          best_weight(-1), has_best(false) {}

    void add_hard_clause(const std::vector<int>& lits) {
        std::vector<int> clause = lits;
        if (clause.empty() || clause.back() != 0) {
            clause.push_back(0);
        }
        solver.AddClause(std::span<int>(clause.data(), clause.size() - 1));
    }

    void add_soft_clause(const std::vector<int>& lits) {
        int r_i = ++next_var;
        relaxation_vars.push_back(r_i);

        std::vector<int> transformed_clause = lits;
        transformed_clause.push_back(r_i);
        transformed_clause.push_back(0);
        solver.AddClause(std::span<int>(transformed_clause.data(), transformed_clause.size() - 1));
    }

    void run_optimization() {
        if (relaxation_vars.empty()) {
            return;
        }

        Totalizer totalizer(next_var, relaxation_vars);
        totalizer.build();

        // Cardinality encoding is permanent; bound tightening is done with assumptions (incremental LSU).
        const auto& encoding_clauses = totalizer.get_clauses();
        for (const auto& clause : encoding_clauses) {
            if (clause.empty()) continue;
            solver.AddClause(std::span<int>(const_cast<int*>(clause.data()), clause.size()));
        }

        int current_bound = -1; // after SAT with cost w, next solve assumes sum(r_i) <= w-1

        while (true) {
            std::vector<int> assumps;
            if (current_bound >= 0) {
                int bound_lit = totalizer.get_output_lit(current_bound);
                if (bound_lit == -1) {
                    break;
                }
                // Falsify output literal at index current_bound => at most current_bound violations.
                assumps.push_back(-bound_lit);
            }

            Topor::TToporReturnVal res = assumps.empty()
                ? solver.Solve()
                : solver.Solve(std::span<int>(assumps));

            if (res == Topor::TToporReturnVal::RET_SAT) {
                save_best_model();
                int current_weight = calculate_current_weight();

                std::cout << "o " << current_weight << std::endl;

                if (current_weight == 0) {
                    break;
                }

                current_bound = current_weight;
            } else if (res == Topor::TToporReturnVal::RET_UNSAT) {
                // No model better than the last SAT model; last saved model is optimal.
                break;
            } else {
                break;
            }
        }
    }

    int get_best_weight() const { return best_weight; }
    const std::vector<int>& get_best_model() const { return best_model; }
    bool has_best_model() const { return has_best; }

private:
    int calculate_current_weight() {
        int weight = 0;
        for (int r_i : relaxation_vars) {
            if (solver.GetLitValue(r_i) == Topor::TToporLitVal::VAL_SATISFIED) {
                weight++;
            }
        }
        return weight;
    }

    void save_best_model() {
        best_model.clear();

        for (int i = 1; i <= next_var; ++i) {
            auto val = solver.GetLitValue(i);

            if (val == Topor::TToporLitVal::VAL_SATISFIED) {
                best_model.push_back(i);
            } else if (val == Topor::TToporLitVal::VAL_UNSATISFIED) {
                best_model.push_back(-i);
            }
        }
        best_weight = calculate_current_weight();
        has_best = true;
    }
};
