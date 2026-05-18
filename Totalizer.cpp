#ifndef TOTALIZER_HPP
#define TOTALIZER_HPP

#include <vector>
#include <algorithm>

/**
 * Totalizer class for Cardinality Encoding.
 * Implements a unidirectional encoding suitable for Linear Search UNSAT (LSU).
 * This component is "pure" and does not interact with any SAT solver API directly.
 */
class Totalizer {
private:
    int& next_var;                          // Reference to the global variable counter
    std::vector<int> inputs;                // Input relaxation literals (r_i)
    std::vector<int> outputs;               // Root output literals of the unary sum tree
    std::vector<std::vector<int>> clauses;  // Generated CNF clauses

    // Generates a new variable index using the global counter
    int new_var() {
        return ++next_var;
    }

    // Stores a new clause in the internal buffer
    void add_to_internal_clauses(const std::vector<int>& lits) {
        clauses.push_back(lits);
    }

public:
    /**
     * @param nv Reference to the global variable counter.
     * @param in_lits The literals to be summed (relaxation variables).
     */
    Totalizer(int& nv, const std::vector<int>& in_lits)
        : next_var(nv), inputs(in_lits) {
    }

    // Builds the totalizer tree and generates the clauses
    void build() {
        if (inputs.empty()) return;
        outputs = emit_totalizer(inputs);
    }

    // Returns the set of all generated clauses to be added to the solver
    const std::vector<std::vector<int>>& get_clauses() const {
        return clauses;
    }

    /**
     * Returns the literal representing "at least k variables are true".
     * For a descending linear search, if a solution with weight W is found,
     * add a unit clause (-get_output_lit(W)) to the solver for the next iteration.
     */
    int get_output_lit(int k) const {
        if (k <= 0) return 0;
        if (k > (int)outputs.size()) return -1;
        return outputs[k - 1];
    }

private:
    // Recursive function to build the tree structure
    std::vector<int> emit_totalizer(const std::vector<int>& lits);

    // Merges two unary vectors into a single sum vector
    std::vector<int> merge(const std::vector<int>& lhs, const std::vector<int>& rhs);
};

// --- Implementation ---

inline std::vector<int> Totalizer::emit_totalizer(const std::vector<int>& lits) {
    if (lits.size() == 1) {
        return lits;
    }

    int mid = (int)lits.size() / 2;
    std::vector<int> lhs_lits(lits.begin(), lits.begin() + mid);
    std::vector<int> rhs_lits(lits.begin() + mid, lits.end());

    std::vector<int> left_outputs = emit_totalizer(lhs_lits);
    std::vector<int> right_outputs = emit_totalizer(rhs_lits);

    return merge(left_outputs, right_outputs);
}

inline std::vector<int> Totalizer::merge(const std::vector<int>& lhs,
    const std::vector<int>& rhs) {
    int n = (int)lhs.size();
    int m = (int)rhs.size();
    std::vector<int> result;
    result.reserve(n + m);

    for (int i = 0; i < n + m; i++) {
        result.push_back(new_var());
    }

    // Unidirectional encoding: (lhs_count >= i) AND (rhs_count >= j) => (total_count >= i+j)
    // CNF form: (NOT lhs[i-1] OR NOT rhs[j-1] OR result[i+j-1])
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= m; j++) {
            if (i == 0 && j == 0) continue;
            if (i + j > n + m) continue;

            int out_idx = i + j - 1;
            std::vector<int> clause;

            if (i > 0) clause.push_back(-lhs[i - 1]);
            if (j > 0) clause.push_back(-rhs[j - 1]);
            clause.push_back(result[out_idx]);

            add_to_internal_clauses(clause);
        }
    }

    return result;
}

#endif