#pragma once

// ThiccGate.h
// 8192-bit CRC32C-backed approximate membership gate (1 KB), same-word two-probe.
//
// Hash layout — one CRC32 call, both probes in the SAME uint64 word (one load):
//
//   word = CRC32(key)[0..6]    (h & 127)          → 1 of 128 uint64 words
//   bit1 = CRC32(key)[26..31]  (h >> 26)           → bit position 1 (mask-free)
//   bit2 = CRC32(key)[7..12]  ((h >> 7) & 63)      → bit position 2
//
//   Add:   Gate[word] |= (1 << bit1) | (1 << bit2)
//   Maybe: both bits must be set — one load, two shifts, AND
//
// One load per lookup (was two). Bit extraction overlaps with the load.
// Theoretical FPR: (1 - e^(-2N/8192))^2 ≈ 4.7% at N=1000.
//
// IntersectInto / IntersectInPlace are pure bitwise AND over the flat array —
// AND intersection fully compatible with any other ThiccGate of the same size.
//
// Requires SSE4.2 for CRC32.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

#include "CoreTypes.h"

//large gate perf is on the order of 1ns for all ops other than intersect.
struct alignas(64) FLargeGate
{
    static constexpr uint32 GateBits      = 8192;
    static constexpr uint32 PayloadBytes  = GateBits / 8; // 1024
    static constexpr uint32 NumWords      = 128;

    uint64 Gate[128];

    FORCEINLINE void Clear() noexcept
    {
        std::memset(Gate, 0, PayloadBytes);
    }

    FORCEINLINE void Add(uint64 key) noexcept
    {
        uint32 h = static_cast<uint32>(_mm_crc32_u64(0, key));
        Gate[h & 127u] |= (uint64(1) << (h >> 26))
            | (uint64(1) << ((h >> 7) & 63u));
    }

    FORCEINLINE bool ApproxFind(uint64 key) const noexcept
    {
        uint32 h = static_cast<uint32>(_mm_crc32_u64(0, key));
        const uint64 w = Gate[h & 127u];
        return ((w >> (h >> 26)) & (w >> ((h >> 7) & 63u))) & 1u;
    }

    // Returns a 4-bit mask: bit i set iff keys[i] MAY be in the gate.
    // All four CRC32s + mask builds issued before loads for maximum pipelining.
    FORCEINLINE uint32 ApproxFindBatch4(
        uint64 k0, uint64 k1, uint64 k2, uint64 k3) const noexcept
    {
        const uint32 h0 = static_cast<uint32>(_mm_crc32_u64(0, k0));
        const uint32 h1 = static_cast<uint32>(_mm_crc32_u64(0, k1));
        const uint32 h2 = static_cast<uint32>(_mm_crc32_u64(0, k2));
        const uint32 h3 = static_cast<uint32>(_mm_crc32_u64(0, k3));

        const uint64 w0 = Gate[h0 & 127u];
        const uint64 w1 = Gate[h1 & 127u];
        const uint64 w2 = Gate[h2 & 127u];
        const uint64 w3 = Gate[h3 & 127u];

        const uint32 r0 = ((w0 >> (h0 >> 26)) & (w0 >> ((h0 >> 7) & 63u))) & 1u;
        const uint32 r1 = ((w1 >> (h1 >> 26)) & (w1 >> ((h1 >> 7) & 63u))) & 1u;
        const uint32 r2 = ((w2 >> (h2 >> 26)) & (w2 >> ((h2 >> 7) & 63u))) & 1u;
        const uint32 r3 = ((w3 >> (h3 >> 26)) & (w3 >> ((h3 >> 7) & 63u))) & 1u;

        return r0 | (r1 << 1) | (r2 << 2) | (r3 << 3);
    }

    FORCEINLINE void IntersectInto(const FLargeGate& other,
                                   FLargeGate& out) const noexcept
    {
        const __m256i* a = reinterpret_cast<const __m256i*>(Gate);
        const __m256i* b = reinterpret_cast<const __m256i*>(other.Gate);
        __m256i*       r = reinterpret_cast<__m256i*>(out.Gate);
        for (int i = 0; i < 32; ++i)
            _mm256_store_si256(r + i, _mm256_and_si256(_mm256_load_si256(a + i),
                                                        _mm256_load_si256(b + i)));
    }

    FORCEINLINE void IntersectInPlace(const FLargeGate& other) noexcept
    {
        const __m256i* b = reinterpret_cast<const __m256i*>(other.Gate);
        __m256i*       a = reinterpret_cast<__m256i*>(Gate);
        for (int i = 0; i < 32; ++i)
            _mm256_store_si256(a + i, _mm256_and_si256(_mm256_load_si256(a + i),
                                                        _mm256_load_si256(b + i)));
    }
    
};

static_assert(sizeof(FLargeGate)  == 1024, "ThiccGate size changed");
static_assert(alignof(FLargeGate) ==   64, "ThiccGate must be 64-byte aligned");
