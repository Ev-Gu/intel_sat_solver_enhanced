// Minimal single-solve WCNF front-end for cost-correctness cross-checking.
//
// Reads a WCNF file in the ipamirapp dialect (each clause line is
//   <h|weight> lit lit ... 0 ; 'c' = comment, no 'p' header), performs exactly
// ONE ipamir_solve, and prints:
//     o <obj>           on a proven optimum (return 30)
//     s UNSATISFIABLE   on UNSAT (return 20)
//     c RET <code>      otherwise
//
// Soft clauses are normalised exactly as ipamirapp does: a unit soft clause
// (w, {l}) becomes soft literal -l with weight w; a longer soft clause gets a
// fresh blocking variable b, hard clause (b -> clause), soft literal b.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "ipamir.h"
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: wcnf_solve <file.wcnf>\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 2;
    }

    std::vector<std::vector<int32_t>> hard;
    std::vector<std::pair<uint64_t, std::vector<int32_t>>> soft;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c') continue;
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        std::vector<int32_t> clause;
        int32_t lit;
        while (iss >> lit) clause.push_back(lit);
        if (clause.empty() || clause.back() != 0) continue;
        clause.pop_back();
        if (prefix == "h")
            hard.push_back(clause);
        else
            soft.push_back({std::stoull(prefix), clause});
    }

    int32_t n_vars = 0;
    for (auto& c : hard)
        for (auto l : c) n_vars = std::max(n_vars, std::abs(l));
    for (auto& [w, c] : soft)
        for (auto l : c) n_vars = std::max(n_vars, std::abs(l));

    void* s = ipamir_init();
    for (auto& c : hard) {
        for (auto l : c) ipamir_add_hard(s, l);
        ipamir_add_hard(s, 0);
    }
    int32_t bvar = 0;
    for (auto& [w, c] : soft) {
        if (c.size() == 1) {
            ipamir_add_soft_lit(s, -c[0], w);
        } else {
            int32_t b = n_vars + (++bvar);
            ipamir_add_hard(s, b);
            for (auto l : c) ipamir_add_hard(s, l);
            ipamir_add_hard(s, 0);
            ipamir_add_soft_lit(s, b, w);
        }
    }

    int res = ipamir_solve(s);
    if (res == 30)
        std::cout << "RESULT o " << ipamir_val_obj(s) << "\n";
    else if (res == 20)
        std::cout << "RESULT s UNSATISFIABLE\n";
    else
        std::cout << "RESULT c RET " << res << "\n";
    ipamir_release(s);
    return 0;
}
