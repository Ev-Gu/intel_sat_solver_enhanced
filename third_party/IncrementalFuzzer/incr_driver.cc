// incr_driver.cc - IPAMIR incremental scenario replayer.
//
// Reads a "scenario" (a deterministic list of incremental MaxSAT operations)
// and replays it through whatever IPAMIR solver this driver is linked against.
// For every "solve" operation it prints exactly one result line to stdout, so
// that the same scenario run through two different solvers can be compared
// line-by-line by the fuzzer.
//
// The scenario is read from the file given as argv[1] (or from stdin if no
// argument is given). Supported lines (whitespace separated):
//
//   h L1 L2 ... 0     add a hard clause (literals terminated by 0)
//   s LIT W           declare/update soft literal LIT with weight W
//   a LIT             add assumption LIT for the *next* solve only
//   solve             call ipamir_solve and emit one result line
//   c ...             comment, ignored
//
// Output, one line per "solve" (index starts at 0):
//
//   <idx> <status> <obj>
//
// where <status> is the raw ipamir_solve() return code (0/10/20/30/40) and
// <obj> is ipamir_val_obj() when status is 10 or 30, otherwise -1.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

extern "C" {
#include "ipamir.h"
}

int main(int argc, char** argv)
{
    std::istream* in = &std::cin;
    std::ifstream fin;
    if (argc >= 2) {
        fin.open(argv[1]);
        if (!fin) {
            std::fprintf(stderr, "ERROR: cannot open scenario file %s\n", argv[1]);
            return 2;
        }
        in = &fin;
    }

    void* soph = ipamir_init();
    if (!soph) {
        std::fprintf(stderr, "ERROR: ipamir_init returned null\n");
        return 2;
    }

    std::string line;
    long solve_idx = 0;

    while (std::getline(*in, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string op;
        iss >> op;
        if (op.empty() || op == "c") continue;

        if (op == "h") {
            int32_t lit;
            // includes the terminating 0
            while (iss >> lit) {
                ipamir_add_hard(soph, lit);
                if (lit == 0) break;
            }
        } else if (op == "s") {
            int32_t lit;
            uint64_t w;
            if (iss >> lit >> w) {
                ipamir_add_soft_lit(soph, lit, w);
            }
        } else if (op == "a") {
            int32_t lit;
            if (iss >> lit) {
                ipamir_assume(soph, lit);
            }
        } else if (op == "solve") {
            int status = ipamir_solve(soph);
            long long obj = -1;
            if (status == 10 || status == 30) {
                obj = (long long)ipamir_val_obj(soph);
            }
            std::printf("%ld %d %lld\n", solve_idx, status, obj);
            std::fflush(stdout);
            ++solve_idx;
        } else {
            // unknown op: ignore but note on stderr
            std::fprintf(stderr, "WARN: unknown op '%s'\n", op.c_str());
        }
    }

    ipamir_release(soph);
    return 0;
}
