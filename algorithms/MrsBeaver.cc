#include "MrsBeaver.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <random>

extern double MainWallTimePassed();

namespace wmb
{
    WMBResult RunMrsBeaver(
        bool isWeighted,
        std::vector<lsu::TWeightedRelaxLit> relaxLits,
        const std::vector<int32_t>& baseAssumps,
        uint64_t initialCost,
        const std::vector<int>& initialModel,
        const lsu::TSolveFn& solve,
        const lsu::TGetLitValueFn& getLitValue,
        const WMBOptions& options,
        bool forceLoop)
    {
        WMBResult res;
        res.bestCost = initialCost;
        res.bestModel01 = initialModel;
        res.skipCompletePhase = forceLoop;

        if (relaxLits.empty() || initialCost == 0) return res;

        // Sort descending by weight (Greedy approximation)
        std::sort(relaxLits.begin(), relaxLits.end(), 
            [](const lsu::TWeightedRelaxLit& a, const lsu::TWeightedRelaxLit& b) {
                return a.Weight > b.Weight;
            });

        auto start = std::chrono::steady_clock::now();
        int iteration = 0;
        std::mt19937 rng(1337);

        std::vector<int32_t> base;
        for (auto a : baseAssumps) {
            if (a != 0) base.push_back(a);
        }
        while (true)
        {
            iteration++;
            
            // Check time limit
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= options.timeLimitSeconds) break;

            // SSCP Check
            if (iteration > options.gtl && !res.skipCompletePhase) {
                // Heuristic: target count * max weight limits
                uint64_t expectedNodes = relaxLits.size() * res.bestCost;
                if (expectedNodes < options.gtThr) {
                    break; // Size is safe, hand off to LSU for the complete phase
                } else {
                    res.skipCompletePhase = true; // Size too big, keep looping WMB
                }
            }

            // OBV-BS Pass
            std::vector<int32_t> currentAssumps = base;
            uint64_t currentPassCost = 0;

            for (size_t i = 0; i < relaxLits.size(); ++i) {
                if (elapsed >= options.timeLimitSeconds) break;

                int32_t targetLit = relaxLits[i].Lit;
                uint64_t weight = relaxLits[i].Weight;

                // Test forcing the soft clause to be satisfied (-targetLit is usually the relaxation)
                currentAssumps.push_back(-targetLit); 
                
                Topor::TToporReturnVal ret = solve(currentAssumps);
                
                if (ret == Topor::TToporReturnVal::RET_SAT) {
                    // Lock it in, we can satisfy it
                    continue; 
                } else {
                    // Conflict or timeout. Revert and take the penalty.
                    currentAssumps.pop_back();
                    currentAssumps.push_back(targetLit);
                    currentPassCost += weight;
                }
                
                // Early exit if this pass is already worse than our best
                if (currentPassCost >= res.bestCost) break; 
            }

            // Update global best if we improved
            if (currentPassCost < res.bestCost) {
                res.bestCost = currentPassCost;
                for (int32_t v = 1; v < res.bestModel01.size(); ++v) {
                    res.bestModel01[v] = (getLitValue(v) == Topor::TToporLitVal::VAL_UNSATISFIED) ? 0 : 1;
                }
                std::cout << "o " << res.bestCost << std::endl;
                std::cout << "c timeo " << (unsigned)std::ceil(MainWallTimePassed()) << " " << res.bestCost << std::endl;
                if (res.bestCost == 0) break;
            }

            // Perturb for next iteration: Weighted shuffle
            std::shuffle(relaxLits.begin(), relaxLits.end(), rng);
            // Re-sort segments to roughly keep heavier weights near the top
            for(size_t i = 0; i < relaxLits.size(); i += 5) {
                size_t end = std::min(i + 5, relaxLits.size());
                std::sort(relaxLits.begin() + i, relaxLits.begin() + end, 
                    [](const auto& a, const auto& b) { return a.Weight > b.Weight; });
            }
        }

        return res;
    }
}