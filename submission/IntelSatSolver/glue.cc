// Glue for the IntelSAT IPAMIR static library.
//
// The solver's NuWLS local-search code (algorithms/Alg_nuwls.h) references the
// free function MainWallTimePassed(), normally provided by the command-line
// front-end Main.cc. Since the IPAMIR library must NOT contain a main() symbol
// (it would clash with the application's main), we drop Main.or from the archive
// and provide MainWallTimePassed() here instead.

#include <chrono>

static const std::chrono::steady_clock::time_point gGlueStartTime =
    std::chrono::steady_clock::now();

double MainWallTimePassed()
{
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - gGlueStartTime)
        .count();
}
