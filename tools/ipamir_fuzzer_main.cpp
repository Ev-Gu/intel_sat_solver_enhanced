#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <cmath>
#include <algorithm>

extern "C" {
    void* ipamir_init();
    void ipamir_release(void* solver);
    void ipamir_add_hard(void* solver, int32_t lit_or_zero);
    void ipamir_add_soft_lit(void* solver, int32_t lit, uint64_t weight);
    void ipamir_assume(void* solver, int32_t lit); // Added assume signature
    int ipamir_solve(void* solver);
    uint64_t ipamir_val_obj(void* solver);
    int32_t ipamir_val_lit(void* solver, int32_t lit);
}

// Helper to check if a string is a valid positive number
bool is_number(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

// Helper to print the solution cleanly
void print_solution(int status, void* solver, int32_t print_max_var) {
    if (status == 10 || status == 30) {
        std::cout << "o " << ipamir_val_obj(solver) << "\n";
        std::cout << "s " << (status == 30 ? "OPTIMUM FOUND" : "SATISFIABLE") << "\n";

        std::cout << "v";
        for (int32_t i = 1; i <= print_max_var; ++i) {
            int32_t val = ipamir_val_lit(solver, i)>0 ? 1 : 0;
            std::cout << " " << val;
        }
        std::cout << "\n";
    }
    else if (status == 20) {
        std::cout << "s UNSATISFIABLE\n";
    }
    else {
        std::cout << "s UNKNOWN\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <instance.wcnf>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << "\n";
        return 1;
    }

    int32_t max_var = 0;
    uint64_t top_weight = std::numeric_limits<uint64_t>::max();
    std::string line;

    // PASS 1: Get Max Var
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == '\r') continue;

        std::istringstream iss(line);
        std::string token;
        if (!(iss >> token)) continue;

        if (token == "p") {
            std::string format;
            int vars, clauses;
            iss >> format >> vars >> clauses;
            if (iss >> top_weight) {} // Successfully parsed old format top_weight
        }
        else if (token == "s") {
            continue; // Skip 's' lines for max_var calculation
        }
        else {
            int32_t lit;
            while (iss >> lit && lit != 0) {
                max_var = std::max(max_var, std::abs(lit));
            }
        }
    }

    // Cache the original max_var to avoid printing internal relaxation variables later
    const int32_t original_max_var = max_var;

    file.clear();
    file.seekg(0);

    void* solver = ipamir_init();
    if (!solver) {
        std::cerr << "Error: Failed to init solver.\n";
        return 1;
    }

    std::cout << "c Parsed max variable: " << original_max_var << "\n";
    if (top_weight == std::numeric_limits<uint64_t>::max()) {
        std::cout << "c Top weight: None (Modern WCNF format detected)\n";
    }
    else {
        std::cout << "c Top weight: " << top_weight << "\n";
    }
    std::cout << "c Feeding instance to IPAMIR...\n";

    bool solved_via_s_line = false;

    // PASS 2: Parse and feed to IPAMIR
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == 'c' || line[0] == 'p' || line[0] == '\r') continue;

        std::istringstream iss(line);
        std::string first_token;
        if (!(iss >> first_token)) continue; // Safely skip empty tokens

        // Handle 's' line (solve under assumptions)
        if (first_token == "s") {
            int32_t lit;
            while (iss >> lit && lit != 0) {
                ipamir_assume(solver, lit);
            }

            std::cout << "c Solving (triggered by 's' line)...\n";
            int status = ipamir_solve(solver);
            print_solution(status, solver, original_max_var);

            solved_via_s_line = true;
            continue;
        }

        bool is_hard = false;
        uint64_t weight = 0;

        if (first_token == "h") {
            is_hard = true;
        }
        else if (is_number(first_token)) {
            weight = std::stoull(first_token);
            if (weight >= top_weight) is_hard = true;
        }
        else {
            continue; // Skip malformed lines
        }

        std::vector<int32_t> clause;
        int32_t lit;
        while (iss >> lit && lit != 0) {
            clause.push_back(lit);
        }

        if (is_hard) {
            for (int32_t l : clause) ipamir_add_hard(solver, l);
            ipamir_add_hard(solver, 0);
        }
        else {
            max_var++;
            int32_t relax_var = max_var;

            for (int32_t l : clause) ipamir_add_hard(solver, l);
            ipamir_add_hard(solver, relax_var);
            ipamir_add_hard(solver, 0);

            ipamir_add_soft_lit(solver, -relax_var, weight);
        }
    }

    // Default solve if the file did not contain any 's' lines
    if (!solved_via_s_line) {
        std::cout << "c Solving (default end-of-file)...\n";
        int status = ipamir_solve(solver);
        print_solution(status, solver, original_max_var);
    }

    ipamir_release(solver);
    return 0;
}