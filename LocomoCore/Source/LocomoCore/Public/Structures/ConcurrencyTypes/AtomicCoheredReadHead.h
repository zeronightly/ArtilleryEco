#pragma once

#include <algorithm>
#include <intrin.h>

#include "UObject/ObjectMacros.h" // this gives vision to the lil grouping mechanism.


//staleness minimizing cohered reader
struct FAtomicArrayGrouping
{
	//we might wanna actually loop this sucker.
    short IdTracker = -1;
    short GenerateMyID()
    {
        return _InterlockedIncrement16(&IdTracker);
    }
};

//TODO: DETERMINISM RISK
// These use rdtsc which checks CPU op count. to make them truly deterministic, we will need to rationalize against local CPU
// to get a reasonably consistent band. as long as we're within 1-2%, resim will be stable. Why do it this way? it's about 40x faster.
// Staleness-minimizing cohered reader.
// Contract:
// - Fixed writer-owned heads; one writer per ID.
// - Writer publishes one value and one packed band/check word.
// - Reader accepts values from the leading observed band or the immediately prior band.
// - The top 16 bits of TSC carry a validator copied from the published double bits.
// - No C++ synchronization guarantee is claimed here; this is a low-level, platform-specific publication surface.
struct FTimedAtomicArray
{
    static constexpr uint32_t MaxWriters = 6;
    // 2^19 ticks. At 5 GHz, this is about .2 ms per band.
    static constexpr uint32_t BandShift = 19;

    struct alignas(32) Heads
    {// Preserve 32 B/head so the 8-writer array is 256 B and each cache line holds two heads.
        std::atomic<std::pair<double, uint64>> ValAndStamp;
    };
    alignas(64)
    std::atomic<uint64> CurrentVersionLeft;
    alignas(64)
    std::atomic<uint64> CurrentVersionRight;
    
    alignas(64) Heads BandedHeads[MaxWriters];
    static_assert(sizeof(Heads) == 32, "FTimeBandedArray::Heads must stay 32 bytes");
    static_assert(alignof(Heads) == 32, "FTimeBandedArray::Heads must stay 32-byte aligned");
    
    FORCEINLINE void Write(const double& In, const uint8& MyID)
    {
        Heads& Head = BandedHeads[MyID];
        atomic_signal_fence(std::memory_order_acquire);
        Head.ValAndStamp.store({In, __rdtsc() >> BandShift});
    }

    FORCEINLINE double Candidate(const Heads& Head, uint64& Newest) const
    {
        std::atomic<std::pair<double, unsigned long long>>::_TVal atomunit = Head.ValAndStamp.load();
        Newest = std::max(Newest, atomunit.second);

        if (atomunit.second+1 < Newest)
        {
            return DOUBLE_BIG_NUMBER;
        }

        return atomunit.first;
    }

    // this is a min-tournament, similar to a sorting network.
    // I couldn't find a way to early out in case of just one candidate that was actually faster.
    FORCEINLINE double Read() const
    {
        uint64 Newest = 0;

        double V0 = Candidate(BandedHeads[0], Newest);
        double V1 = Candidate(BandedHeads[1], Newest);
        double V2 = Candidate(BandedHeads[2], Newest);
        double V3 = Candidate(BandedHeads[3], Newest);
        double V4 = Candidate(BandedHeads[4], Newest);
        double V5 = Candidate(BandedHeads[5], Newest);

        if (V1 < V0) V0 = V1;
        if (V3 < V2) V2 = V3;
        if (V5 < V4) V4 = V5;

        if (V2 < V0) V0 = V2;
        if (V4 < V0) V0 = V4;

        return V0;
    }
};
