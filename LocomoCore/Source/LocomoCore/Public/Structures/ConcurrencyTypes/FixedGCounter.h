#pragma once

#ifdef _MSC_VER
#  pragma warning(disable: 4324) // structure padded due to alignas — intentional
#endif

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <type_traits>

// Grow-only counter. Each replica owns one slot; join takes element-wise max.
// V must be a signed 32-bit integer; join uses _mm256_max_epi32.
template<typename V = int, int MaxReplicas = 64>
struct alignas(64) FixedGCounter
{
    static_assert(std::is_integral_v<V> && std::is_signed_v<V> && sizeof(V) == 4,
        "V must be a signed 32-bit integer (join uses _mm256_max_epi32)");

    V       Counts[MaxReplicas];
    uint8_t MyID;

    explicit FixedGCounter(uint8_t InID) : MyID(InID)
    {
        std::memset(Counts, 0, sizeof(Counts));
    }

    void Inc(V Amount = 1) { Counts[MyID] += Amount; }
    V Local()        const { return Counts[MyID]; }

    V Read() const
    {
        constexpr int PrefetchDistance = 8;
        V Sum = V{};
        for (int i = 0; i < MaxReplicas; ++i)
        {
            if (i + PrefetchDistance < MaxReplicas)
                _mm_prefetch(reinterpret_cast<const char*>(&Counts[i + PrefetchDistance]), _MM_HINT_T0);
            Sum += Counts[i];
        }
        return Sum;
    }

    // AVX2: 8 ints/iter.  SSE4.1 tail: 4 ints.  Scalar remainder.
    void Join(const FixedGCounter& O)
    {
        int i = 0;
        for (; i + 8 <= MaxReplicas; i += 8)
        {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(O.Counts + i));
            __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(Counts + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(Counts + i), _mm256_max_epi32(a, b));
        }
        for (; i + 4 <= MaxReplicas; i += 4)
        {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(O.Counts + i));
            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Counts + i));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(Counts + i), _mm_max_epi32(a, b));
        }
        for (; i < MaxReplicas; ++i)
            if (O.Counts[i] > Counts[i]) Counts[i] = O.Counts[i];
    }
};
