#include "ToporIpamir.h"
#include "Topor.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <iostream>
#include "algorithms/Alg_nuwls.h"
#include "algorithms/LSU.hpp"
#include "algorithms/MrsBeaver.hpp"

using namespace std;
using namespace nuwls;
using namespace wmb;
using TLit = int32_t;

namespace Topor
{
    class CToporIpamirWrapper : public CTopor<>
    {
    public:

        int ValLit(int lit)
        {
            if (m_globalBestModel.empty()) return 0;

            int32_t absExt = std::abs(lit);
            auto it = m_ext2int.find(absExt);

            if (it == m_ext2int.end()) return 0;

            int32_t internalVar = it->second;

            if ((size_t)internalVar >= m_globalBestModel.size()) return 0;

            bool varTrue = (m_globalBestModel[internalVar] != 0);
            bool litSatisfied = (lit > 0) ? varTrue : !varTrue;

            return litSatisfied ? lit : -lit;
        }

        void AddHard(int lit_or_zero)
        {
            m_Result.valid = false;
            m_optimal = false;

            if (lit_or_zero == 0)
            {
                if (!m_CurrHardCls.empty())
                {
                    AddClause(m_CurrHardCls);
                    m_AllHardClauses.push_back(m_CurrHardCls);
                    m_CurrHardCls.clear();
                }
            }
            else
            {
                m_CurrHardCls.emplace_back(GetOrCreateInternalVar(lit_or_zero));
            }
        }

        void AddSoftLit(int lit, uint64_t weight)
        {
            m_Result.valid = false;
            m_optimal = false;

            int32_t internalLit = GetOrCreateInternalVar(lit);
            m_SoftLit2Weight[internalLit] = weight;

            if (m_lastWeight == 0) m_lastWeight = weight;
            if (weight != m_lastWeight) m_isWeighted = true;

            CreateInternalLit(std::abs(internalLit));
            FixPolarity(internalLit, false);
        }

        void Assume(int lit)
        {
            m_Result.valid = false;
            m_optimal = false;
            int32_t internalLit = GetOrCreateInternalVar(lit);

            if (m_Assump2Ind.find(internalLit) == m_Assump2Ind.end())
            {
                m_Assump2Ind.insert(make_pair(internalLit, m_CurrAssumps.size()));
                m_CurrAssumps.push_back(internalLit);
            }
        }
        int Solve()
        {
            const vector<int> assumpsForPost = m_CurrAssumps;
            solveStartTime = g_GlobalTimer.WallTimePassedSinceStartOrReset();

            TToporReturnVal trv = CTopor::Solve(m_CurrAssumps);

            m_PrevAssump2Ind = move(m_Assump2Ind);
            m_Assump2Ind.clear();
            m_CurrAssumps.clear();

            m_Result = TSolverResult();

            if (trv == Topor::TToporReturnVal::RET_SAT)
            {
                CaptureToporAssignment();
                ComputeObjFromAssignment();

                m_bestCost = m_Result.cost;
                m_globalBestModel = m_Result.assignment;
                if (m_bestCost == 0) m_optimal = true;
                if (!m_optimal) RunNuwlsPostSolve(assumpsForPost);
                if (!m_optimal) RunMbvAndLsuPostSolve(assumpsForPost);
                m_Result.is_optimal = m_optimal;
                return m_Result.is_optimal ? 30 : 10;
            }
            else if (trv == Topor::TToporReturnVal::RET_UNSAT)
            {
                return 20;
            }
            else
            {
                return 0;
            }
        }

        uint64_t ValObj() const
        {
            return m_bestCost;
        }

        void SetTerminate(void* state, int (*terminate)(void* state))
        {
            if (terminate == nullptr)
            {
                m_CurrTerminateFunc = nullptr;
                m_CurrTerminateState = nullptr;
                return;
            }

            m_CurrTerminateFunc = terminate;
            m_CurrTerminateState = state;

            auto StopTopor = [&]()
                {
                    if (m_CurrTerminateFunc && m_CurrTerminateFunc(m_CurrTerminateState))
                    {
                        return TStopTopor::VAL_STOP;
                    }
                    else
                    {
                        return TStopTopor::VAL_CONTINUE;
                    }
                };

            SetCbStopNow(StopTopor);
        }

    protected:
        struct TSolverResult
        {
            vector<int> assignment;
            bool is_optimal = false;
            uint64_t cost = 0;
            bool valid = false;
        };

        static bool IsLitSatisfiedByAssignment(int lit, const vector<int>& assignment)
        {
            const int v = lit > 0 ? lit : -lit;
            if (v <= 0 || v >= (int)assignment.size()) return false;
            return lit > 0 ? assignment[v] == 1 : assignment[v] == 0;
        }

        void CaptureToporAssignment()
        {
            m_Result.assignment.assign(m_internalMaxVar + 1, 0);

            for (int v = 1; v <= m_internalMaxVar; ++v)
            {
                const TToporLitVal lv = GetLitValue(v);
                if (lv == TToporLitVal::VAL_SATISFIED) m_Result.assignment[v] = 1;
                else m_Result.assignment[v] = 0;
            }

            m_Result.valid = true;
        }

        void ComputeObjFromAssignment()
        {
            uint64_t c = 0;
            for (const auto& p : m_SoftLit2Weight)
            {
                if (!IsLitSatisfiedByAssignment(p.first, m_Result.assignment))
                {
                    c += p.second;
                }
            }
            m_Result.cost = c;
        }

        void RunMbvAndLsuPostSolve(const vector<int>& assumpsForPost) {
            bool skipLSU = false;
            double elapsedGlobal;
            double elapsedLocal;
            double remainingLocal;

            std::vector<lsu::TWeightedRelaxLit> wr;
            for (const auto& kv : m_SoftLit2Weight) {
                wr.push_back({ -kv.first, kv.second });
            }

            std::vector<int32_t> internalAssumps;
            internalAssumps.reserve(assumpsForPost.size());

            for (int a : assumpsForPost) {
                if (a != 0) {
                    internalAssumps.push_back(a);
                }
            }
            do {
                skipLSU = false;

                elapsedGlobal = g_GlobalTimer.WallTimePassedSinceStartOrReset();
                elapsedLocal = elapsedGlobal - solveStartTime;

                remainingLocal = max(0.0, (double)m_lsuTimeLimit - elapsedLocal);

                if (m_lsuTimeLimit > 0 && remainingLocal <= 0) {
                    skipLSU = true;
                    break;
                }

                if (wmbOptions.enable && !m_optimal)
                {
                    wmb::WMBOptions localWmbOptions = wmbOptions;
                    if (m_lsuTimeLimit > 0) {
                        localWmbOptions.timeLimitSeconds = min(60, (int)remainingLocal);
                    }

                    auto wmbSolveCb = [&](const std::vector<int32_t>& a) {
                        auto ret = CTopor::Solve(std::span<int32_t>(const_cast<int32_t*>(a.data()), a.size()),
                            std::make_pair(numeric_limits<double>::max(), false),
                            wmbOptions.conflictThreshold);

                        // IMMEDIATE GLOBAL UPDATE: Intercept SAT models during WMB
                        if (ret == Topor::TToporReturnVal::RET_SAT) {
                            CaptureToporAssignment();
                            ComputeObjFromAssignment();
                            if (m_Result.cost < m_bestCost) {
                                m_bestCost = m_Result.cost;
                                m_globalBestModel = m_Result.assignment;
                                if (m_bestCost == 0) m_optimal = true;
                            }
                        }
                        return ret;
                        };

                    auto getValCb = [&](int32_t l) { return CTopor::GetLitValue(l); };

                    wmb::WMBResult wmbRes = wmb::RunMrsBeaver(
                        m_isWeighted, wr, internalAssumps,
                        m_bestCost, m_globalBestModel, wmbSolveCb, getValCb, localWmbOptions, false
                    );

                    // Sync final WMB status just in case
                    if (wmbRes.bestCost < m_bestCost) {
                        m_bestCost = wmbRes.bestCost;
                        m_globalBestModel = std::move(wmbRes.bestModel01);
                        if (m_bestCost == 0) m_optimal = true;
                    }

                    if (wmbRes.skipCompletePhase || wmbRes.timedOut) {
                        skipLSU = true;
                    }
                }

                // --- LSU COMPLETE PHASE ---
                if (m_enableLSU && !skipLSU && !m_optimal)
                {
                    lsu::TLinearSUOptions opt;
                    opt.Verbose = (m_lsuVerbosity != 0);
                    opt.TimeLimitSeconds = m_lsuTimeLimit > 0 ? (int)remainingLocal : 0;

                    auto addClauseCb = [&](const std::vector<int32_t>& c) {
                        std::vector<int32_t> clauseWithAct = c;
                        CTopor::AddClause(std::span<int32_t>(clauseWithAct.data(), clauseWithAct.size()));
                        };

                    auto solveCb = [&](const std::vector<int32_t>& a) {
                        std::vector<int32_t> fullAssumps = a;
                        auto ret = CTopor::Solve(std::span<int32_t>(fullAssumps.data(), fullAssumps.size()));

                        // IMMEDIATE GLOBAL UPDATE: Intercept SAT models during LSU
                        if (ret == Topor::TToporReturnVal::RET_SAT) {
                            CaptureToporAssignment();
                            ComputeObjFromAssignment();
                            if (m_Result.cost < m_bestCost) {
                                m_bestCost = m_Result.cost;
                                m_globalBestModel = m_Result.assignment;
                                if (m_bestCost == 0) m_optimal = true;
                            }
                        }
                        return ret;
                        };

                    auto getValCb = [&](int32_t l) { return CTopor::GetLitValue(l); };

                    lsu::TLinearSUResult lsuRes;

                    try {
                        if (!m_isWeighted) {
                            std::vector<int32_t> ur;
                            for (const auto& w_lit : wr) ur.push_back(w_lit.Lit);
                            uint64_t commonWeight = (m_lastWeight > 0) ? (uint64_t)m_lastWeight : 1;

                            lsuRes = lsu::RunUnweightedLinearSatUnsat(
                                ur, internalAssumps, m_internalMaxVar + 1, m_internalMaxVar,
                                m_bestCost, commonWeight, addClauseCb, solveCb, getValCb, opt);
                        }
                        else {
                            lsuRes = lsu::RunWeightedLinearSatUnsat(
                                wr, internalAssumps, m_internalMaxVar + 1, m_internalMaxVar,
                                m_bestCost, addClauseCb, solveCb, getValCb, opt);
                        }

                        if (lsuRes.NextFreeVar > m_internalMaxVar) {
                            m_internalMaxVar = lsuRes.NextFreeVar;
                            if ((size_t)m_internalMaxVar >= m_int2ext.size()) m_int2ext.resize(m_internalMaxVar + 1, 0);
                        }

                        // Sync final LSU status just in case
                        if (lsuRes.Improved && lsuRes.BestCost < m_bestCost) {
                            m_bestCost = lsuRes.BestCost;
                            m_globalBestModel = std::move(lsuRes.BestModel01);
                        }

                        if (lsuRes.LastSolveRet == Topor::TToporReturnVal::RET_MEM_OUT) {
                            break;
                        }
                        else if (lsuRes.LastSolveRet == Topor::TToporReturnVal::RET_UNSAT || m_bestCost == 0) {
                            m_optimal = true;
                        }
                    }
                    catch (const std::bad_alloc& e) {
                        break;
                    }
                }
            } while (skipLSU && !m_optimal);
        }

        void RunNuwlsPostSolve(const vector<int>& assumpsForPost)
        {
            if (m_internalMaxVar <= 0) return;
            if (m_AllHardClauses.empty() && m_SoftLit2Weight.empty()) return;

            vector<pair<uint64_t, vector<int>>> softClauses;
            softClauses.reserve(m_SoftLit2Weight.size());

            unsigned long long sumW = 0;
            bool isWeighted = false;
            for (const auto& p : m_SoftLit2Weight)
            {
                softClauses.push_back({ p.second, vector<int>{ p.first } });
                sumW += (unsigned long long)p.second;
                if (p.second != 1) isWeighted = true;
            }

            const unsigned long long topClauseWeight = sumW + 1ULL;

            auto built = nuwls::SanitizeAndBuildNuwlsInstance(
                m_internalMaxVar,
                topClauseWeight,
                m_AllHardClauses,
                softClauses,
                &assumpsForPost);

            if (built.numClauses > 0)
            {
                NUWLS nuwls_solver;
                nuwls_solver.problem_weighted = isWeighted ? 1 : 0;
                nuwls_solver.build_instance(
                    built.numVars,
                    built.numClauses,
                    built.topClauseWeight,
                    built.clauseLit,
                    built.clauseLitCount,
                    built.clauseWeight);
                nuwls_solver.settings();

                // IMMEDIATE GLOBAL UPDATE: Initialize directly from the global state
                nuwls_solver.init(m_globalBestModel);
                unsigned long long nuwlsCost = static_cast<unsigned long long>(m_bestCost);

                nuwls_solver.RunLocalSearch(m_globalBestModel, nuwlsCost, 0);

                if (nuwlsCost < m_bestCost) {
                    m_bestCost = static_cast<uint64_t>(nuwlsCost);
                }
                if (m_bestCost == 0) m_optimal = true;

                nuwls_solver.free_memory();
            }
        }

        vector<int> m_CurrHardCls;
        vector<int> m_CurrAssumps;
        unordered_map<int, size_t> m_Assump2Ind;
        unordered_map<int, size_t> m_PrevAssump2Ind;
        double solveStartTime;
        wmb::WMBOptions wmbOptions;

        unordered_map<int, uint64_t> m_SoftLit2Weight;

        TSolverResult m_Result;

        void* m_CurrTerminateState = nullptr;
        int (*m_CurrTerminateFunc)(void* state) = nullptr;

        uint64_t m_bestCost = std::numeric_limits<uint64_t>::max();
        bool m_optimal = false;
        bool m_isWeighted = false;
        uint64_t m_lastWeight = 0;
        std::vector<int> m_globalBestModel;

        bool m_enableLSU = true;
        int m_lsuVerbosity = 1;
        int m_lsuTimeLimit = 120;

        vector<vector<int>> m_AllHardClauses;

        std::unordered_map<int32_t, int32_t> m_ext2int;
        std::vector<int32_t> m_int2ext;
        int32_t m_internalMaxVar = 0;

        int32_t GetOrCreateInternalVar(int32_t extVar)
        {
            int32_t absExt = std::abs(extVar);
            auto it = m_ext2int.find(absExt);
            if (it != m_ext2int.end()) {
                return extVar > 0 ? it->second : -it->second;
            }

            m_internalMaxVar++;
            m_ext2int[absExt] = m_internalMaxVar;

            if ((size_t)m_internalMaxVar >= m_int2ext.size()) {
                m_int2ext.resize(m_internalMaxVar + 1, 0);
            }
            m_int2ext[m_internalMaxVar] = absExt;

            return extVar > 0 ? m_internalMaxVar : -m_internalMaxVar;
        }
    };
}

using namespace Topor;

extern "C" {

    const char* ipamir_signature()
    {
        return "IntelSatSolver";
    }

    void* ipamir_init()
    {
        CToporIpamirWrapper* tw = new CToporIpamirWrapper();
        return (void*)tw;
    }

    void ipamir_release(void* solver)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        delete tw;
    }

    void ipamir_add_hard(void* solver, int32_t lit_or_zero)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        tw->AddHard(lit_or_zero);
    }

    void ipamir_add_soft_lit(void* solver, int32_t lit, uint64_t weight)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        tw->AddSoftLit(lit, weight);
    }

    void ipamir_assume(void* solver, int32_t lit)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        tw->Assume(lit);
    }

    int ipamir_solve(void* solver)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        return tw->Solve();
    }

    uint64_t ipamir_val_obj(void* solver)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        return tw->ValObj();
    }

    int32_t ipamir_val_lit(void* solver, int32_t lit)
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        return tw->ValLit(lit);
    }

    void ipamir_set_terminate(void* solver, void* state, int (*terminate)(void* state))
    {
        CToporIpamirWrapper* tw = reinterpret_cast<CToporIpamirWrapper*>(solver);
        tw->SetTerminate(state, terminate);
    }
}