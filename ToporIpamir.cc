#include "ToporIpamir.h"
#include "Topor.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

using namespace std;

namespace Topor
{
    class CToporIpamirWrapper : public CTopor<>
    {
    public:
        void AddHard(int lit_or_zero)
        {
            if (lit_or_zero == 0)
            {
                if (!m_CurrHardCls.empty())
                {
                    AddClause(m_CurrHardCls);
                    m_CurrHardCls.clear();
                }
            }
            else
            {
                m_CurrHardCls.emplace_back(lit_or_zero);
            }
        }

        void AddSoftLit(int lit, uint64_t weight)
        {
            // Store/override weight for soft literal
            m_SoftLit2Weight[lit] = weight;

            // Ensure the variable exists internally so val_lit/val_obj can query it
            const int v = lit > 0 ? lit : -lit;
            CreateInternalLit(v);
			FixPolarity(-lit, false);
        }

        void Assume(int lit)
        {
            if (m_Assump2Ind.find(lit) == m_Assump2Ind.end())
            {
                m_Assump2Ind.insert(make_pair(lit, m_CurrAssumps.size()));
                m_CurrAssumps.push_back(lit);
            }
        }

        int Solve()
        {
            TToporReturnVal trv = CTopor::Solve(m_CurrAssumps);

            // Keep previous assumption map for potential future extensions; clear current
            m_PrevAssump2Ind = move(m_Assump2Ind);
            m_Assump2Ind.clear();
            m_CurrAssumps.clear();

            // Compute objective value over the current model if SAT
            m_LastObj = 0;
            if (trv == Topor::TToporReturnVal::RET_SAT)
            {
                for (const auto& p : m_SoftLit2Weight)
                {
                    const int lit = p.first;
                    const uint64_t w = p.second;
                    const TToporLitVal lv = GetLitValue(lit);
                    if (lv == TToporLitVal::VAL_SATISFIED)
                    {
                        m_LastObj += w;
                    }
                }
				if (m_LastObj == 0) return 30; // all soft clauses satisfied with zero cost
                return 10; // feasible solution found
            }
            else if (trv == Topor::TToporReturnVal::RET_UNSAT)
            {
                return 20; // infeasible (no feasible solution)
            }
            else
            {
                return 0; // interrupted or other
            }
        }

        uint64_t ValObj() const
        {
            return m_LastObj;
        }

        int ValLit(int lit)
        {
            TToporLitVal tlv = GetLitValue(lit);
            switch (tlv)
            {
            case Topor::TToporLitVal::VAL_SATISFIED:
                return lit;
            case Topor::TToporLitVal::VAL_UNSATISFIED:
                return -lit;
            case Topor::TToporLitVal::VAL_DONT_CARE:
            case Topor::TToporLitVal::VAL_UNASSIGNED:
            default:
                return 0;
            }
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
        vector<int> m_CurrHardCls;
        vector<int> m_CurrAssumps;
        unordered_map<int, size_t> m_Assump2Ind;
        unordered_map<int, size_t> m_PrevAssump2Ind;

        unordered_map<int, uint64_t> m_SoftLit2Weight;
        uint64_t m_LastObj = 0;

        void* m_CurrTerminateState = nullptr;
        int (*m_CurrTerminateFunc)(void* state) = nullptr;
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