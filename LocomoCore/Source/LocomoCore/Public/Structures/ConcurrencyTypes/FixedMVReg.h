#pragma once

#ifdef _MSC_VER
#  pragma warning(disable: 4324) // structure padded due to alignas — intentional
#endif

#include <atomic>
#include <cstdint>
#include <immintrin.h>
#include <intrin.h>

// Multi-value register. Each replica writes only its own slot (version
// monotonically increases). 
template<typename V, int MaxReplicas = 32>
struct alignas(64) FixedMVReg
{
    struct Slot
    {
        V   Value{};
        int Version = 0; // 0 = never written
    };

    Slot    Entries[MaxReplicas];

    explicit FixedMVReg(uint8_t InID) {}

    void Write(const V& Val,uint8_t InID)
    {

        Entries[InID].Value = Val;
        std::atomic_signal_fence(std::memory_order_acq_rel); //enforces write order to ensure that version never goes up before val is written
        //this does a lot less than you might hope, because we're using the signal fence rather than a memory fence
        //but it's far far faster. We probably could use CL demote here. if I trusted it.
        ++Entries[InID].Version;
    }

    V ResolveAdditive() const
    {
        V Total = V{};
        for (int i = 0; i < MaxReplicas; ++i)
            if (Entries[i].Version > 0)
                Total += Entries[i].Value;
        return Total;
    }

    bool TryResolve(V& OutVal) const
    {
        int  Best  = 0;
        bool Found = false;
        for (int i = 0; i < MaxReplicas; ++i)
        {
            if (Entries[i].Version > Best)
            {
                Best   = Entries[i].Version;
                OutVal = Entries[i].Value;
                Found  = true;
            }
        }
        return Found;
    }
};
