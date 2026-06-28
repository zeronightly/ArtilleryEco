#pragma once

#include "CoreTypes.h"
#include <immintrin.h>

//single probe smaller variant of large gate. faster, but more prone to a higher FPR
//ultimately, something of a dead end, but it's faster and 512b. Unfortunately, only really
//scales to a 200-300 elements before FPR becomes really rough.
struct alignas(64) FSmallGate
{
    static constexpr uint32 GateBits = 4096;
    static constexpr uint32 PayloadBytes = GateBits / 8;
    static constexpr uint32 Width_Of = PayloadBytes / sizeof(uint64);
    uint64 Gate[Width_Of];

    FORCEINLINE void Clear() noexcept
    {
        std::memset(Gate, 0, PayloadBytes);
    }
    
    FORCEINLINE void Add(uint64 key) noexcept
    {
        const uint64 h = _mm_crc32_u64(0, key);
        Gate[h & 63u] |= uint64(1) << (h >> 26);
    }

    FORCEINLINE bool ApproxFind(uint64 key) const noexcept
    {
        const uint64 h = _mm_crc32_u64(0, key);
        return ((Gate[h & 63u] >> (h >> 26)) & 1u) != 0;
    }

    FORCEINLINE void IntersectInto(const FSmallGate& other,
                                   FSmallGate& out) const noexcept
    {
        const __m256i* a = reinterpret_cast<const __m256i*>(Gate);
        const __m256i* b = reinterpret_cast<const __m256i*>(other.Gate);
        __m256i* r = reinterpret_cast<__m256i*>(out.Gate);
        for (int i = 0; i < 16; ++i)
        {
            _mm256_store_si256(r + i, _mm256_and_si256(_mm256_load_si256(a + i),
                                                       _mm256_load_si256(b + i)));
        }
    }

    FORCEINLINE void IntersectInPlace(const FSmallGate& other) noexcept
    {
        const __m256i* b = reinterpret_cast<const __m256i*>(other.Gate);
        __m256i* a = reinterpret_cast<__m256i*>(Gate);
        for (int i = 0; i < 16; ++i)
        {
            _mm256_store_si256(a + i, _mm256_and_si256(_mm256_load_si256(a + i),
                                                       _mm256_load_si256(b + i)));
        }
    }
    // Returns a 4-bit mask: bit i set iff keys[i] MAY be in the gate.
    // Issues all four CRC32s before any bit-array access so the CPU pipelines
    // them in consecutive issue slots.
    FORCEINLINE uint32 ApproxFindBatch4(
        uint64 k0, uint64 k1, uint64 k2, uint64 k3) const noexcept
    {
        const uint32 h0 = static_cast<uint32>(_mm_crc32_u64(0, k0));
        const uint32 h1 = static_cast<uint32>(_mm_crc32_u64(0, k1));
        const uint32 h2 = static_cast<uint32>(_mm_crc32_u64(0, k2));
        const uint32 h3 = static_cast<uint32>(_mm_crc32_u64(0, k3));
        const uint32 w0 = h0 & 63u, b0 = (h0 >> 6) & 63u;
        const uint32 w1 = h1 & 63u, b1 = (h1 >> 6) & 63u;
        const uint32 w2 = h2 & 63u, b2 = (h2 >> 6) & 63u;
        const uint32 w3 = h3 & 63u, b3 = (h3 >> 6) & 63u;
        return  ((Gate[w0] >> b0) & 1u)
              | (((Gate[w1] >> b1) & 1u) << 1)
              | (((Gate[w2] >> b2) & 1u) << 2)
              | (((Gate[w3] >> b3) & 1u) << 3);
    }
};

static_assert(sizeof(FSmallGate) == 512, "TinyGate4096Single size changed");
static_assert(alignof(FSmallGate) == 64, "TinyGate4096Single must be 64-byte aligned");
