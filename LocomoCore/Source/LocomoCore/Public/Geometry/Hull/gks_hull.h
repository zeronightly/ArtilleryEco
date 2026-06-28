#pragma once
// GKS Hull — Graham-Kurzer Scan: linear-time 2D convex hull
//
// Original concept and radix sort by Jake Kurzer (JMK), circa 2008.
// Radix core derived from Herf Consulting LLC 2001 (MIT Licensed).
// Cleaned up and debugged for standalone C++20 by scaffold tooling.
//
// Key insight: Graham scan is O(n log n) because of the comparison sort
// on polar angles.  Replace that with a radix sort on the float angle keys
// and the bottleneck disappears.  Total: O(n) radix + O(n) scan = O(n).
//
// Radix engine: 6-pass LSD on 64-bit keys, 11-bit slices (2048-bucket
// histograms), SSE/prefetch on scatter.
// Each element packs sortable(pseudoangle) in the upper 32 bits (primary key)
// and the original point index in the lower 32 bits (tiebreaker).
//
// For small N (< 512), uses a sparse path: per-pass histogramming with
// branchless first-touch masking (bitmap + arithmetic mask eliminates
// the pre-zero pass entirely) and bitmap-guided sparse prefix sums.
//
// For large N, uses single-pass parallel histogramming (all 6 histograms
// built in one sweep) with full memset and dense prefix sums.
//
// Anchor selection uses the lowest-leftmost point (guaranteed on hull).
// Angles are computed via a pseudoangle: -dx / (|dx| + dy).  Since the
// anchor is the lowest-leftmost point, dy >= 0 for all others, making
// this monotonic over [0, pi] and thus sortable by radix — with no
// transcendental.  Colinear tiebreaking is handled by a post-sort fixup
// that sorts equal-angle runs by distance (nearest-first), which is
// free for non-degenerate inputs.

#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>
#include <bit>

// ── Prefetch portability ─────────────────────────────────────────────────

#if defined(_MSC_VER) || defined(__SSE__)
#include <xmmintrin.h>
#define GKS_PREFETCH(ptr) _mm_prefetch(reinterpret_cast<const char*>(ptr), _MM_HINT_T0)
#elif defined(__GNUC__) || defined(__clang__)
#define GKS_PREFETCH(ptr) __builtin_prefetch(ptr)
#else
#define GKS_PREFETCH(ptr) ((void)0)
#endif

namespace gks {

// ── Minimal 2D vector ────────────────────────────────────────────────────

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// ── Float-as-uint32 manipulation for radix sort ──────────────────────────

inline uint32_t float_flip(uint32_t f) {
    uint32_t mask = -static_cast<int32_t>(f >> 31) | 0x80000000u;
    return f ^ mask;
}

inline uint32_t float_unflip(uint32_t f) {
    uint32_t mask = ((f >> 31) - 1) | 0x80000000u;
    return f ^ mask;
}

inline uint32_t float_to_sortable(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return float_flip(bits);
}

// ── CCW orientation test ─────────────────────────────────────────────────

inline double ccw(Vec2 a, Vec2 b, Vec2 c) {
    return static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y)
         - static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
}

// ── HRadix: 6-pass LSD radix sort, 11-bit slices, 64-bit keys ───────────
//
// Adapted from Herf Consulting LLC 2001 (MIT Licensed), JMK 2008.
//
// Two paths based on element count:
//
// Sparse (N < 512):
//   Per-pass histogramming with branchless first-touch masking.  A 256-byte
//   bitmap tracks which histogram entries are live.  Each element does:
//     seen = (bitmap & bit) != 0;  bitmap |= bit;
//     hist[s] = (hist[s] & -seen) + 1;
//   This zeros on first touch and increments on subsequent — no separate
//   pre-zero pass, no branches.  Sparse prefix sum iterates only live
//   entries via countr_zero on the bitmap words.
//
// Full (N >= 512):
//   Single-pass parallel histogramming (all 6 histograms in one sweep).
//   49KB memset up front, dense prefix sums over all 2048 entries.
//   SSE prefetch on data reads and scatter writes.
//
// In both paths, 6 scatter passes ping-pong between data and buf.
// After 6 passes (even count), the result is in data[].

inline void radix_sort_64(uint64_t* data, uint64_t* buf, uint32_t n) {
    if (n <= 1) return;

    constexpr uint32_t kHist = 2048;
    constexpr uint32_t kBitmapWords = kHist / 64; // 32

    if (n < 512) {
        // ── Sparse path ──────────────────────────────────────────────
        // Per-pass: branchless first-touch histogram (one data pass)
        //         + sparse prefix sum + scatter (one data pass).
        // No pre-zero pass.  Data stays in L1 at these sizes.

        uint32_t hist[kHist]; // one histogram, reused per pass
        constexpr int shifts[6] = { 0, 11, 22, 33, 44, 55 };

        for (int pass = 0; pass < 6; ++pass) {
            uint64_t* src = (pass & 1) ? buf  : data;
            uint64_t* dst = (pass & 1) ? data : buf;
            const int shift = shifts[pass];

            // Bitmap: 256 bytes zeroed, tracks which histogram entries are live
            uint64_t touched[kBitmapWords] = {};

            // Branchless first-touch histogram (single pass over data)
            //   seen=1 → mask=0xFFFFFFFF → hist[s] = (hist[s] & mask) + 1 = hist[s]+1
            //   seen=0 → mask=0x00000000 → hist[s] = (0) + 1 = 1
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t s = static_cast<uint32_t>((src[i] >> shift) & 0x7FF);
                uint64_t bit = 1ULL << (s & 63);
                uint32_t seen = (touched[s >> 6] & bit) != 0;
                touched[s >> 6] |= bit;
                hist[s] = (hist[s] & static_cast<uint32_t>(-seen)) + 1;
            }

            // Sparse prefix sum: iterate only live entries via bitmap
            uint32_t sum = 0;
            for (uint32_t w = 0; w < kBitmapWords; ++w) {
                uint64_t bits = touched[w];
                while (bits) {
                    uint32_t bit = std::countr_zero(bits);
                    uint32_t idx = w * 64 + bit;
                    uint32_t t = hist[idx] + sum;
                    hist[idx] = sum - 1;
                    sum = t;
                    bits &= bits - 1;
                }
            }

            // Scatter
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t s = static_cast<uint32_t>((src[i] >> shift) & 0x7FF);
                dst[++hist[s]] = src[i];
            }
        }

    } else {
        // ── Full path ────────────────────────────────────────────────
        uint32_t b0[kHist * 6];
        uint32_t* b1 = b0 + kHist;
        uint32_t* b2 = b1 + kHist;
        uint32_t* b3 = b2 + kHist;
        uint32_t* b4 = b3 + kHist;
        uint32_t* b5 = b4 + kHist;

        std::memset(b0, 0, sizeof(b0));

        // Single-pass histogramming with prefetch
        for (uint32_t i = 0; i < n; ++i) {
            GKS_PREFETCH(data + i + 64);
            uint64_t v = data[i];
            b0[ v        & 0x7FF]++;
            b1[(v >> 11) & 0x7FF]++;
            b2[(v >> 22) & 0x7FF]++;
            b3[(v >> 33) & 0x7FF]++;
            b4[(v >> 44) & 0x7FF]++;
            b5[ v >> 55         ]++;
        }

        // Parallel prefix sums (pre-decrement for scatter)
        uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
        for (uint32_t i = 0; i < kHist; ++i) {
            uint32_t t;
            t = b0[i]+s0; b0[i] = s0-1; s0 = t;
            t = b1[i]+s1; b1[i] = s1-1; s1 = t;
            t = b2[i]+s2; b2[i] = s2-1; s2 = t;
            t = b3[i]+s3; b3[i] = s3-1; s3 = t;
            t = b4[i]+s4; b4[i] = s4-1; s4 = t;
            t = b5[i]+s5; b5[i] = s5-1; s5 = t;
        }

        // 6 scatter passes (data <-> buf ping-pong)
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(data+i+128); buf[++b0[data[i] & 0x7FF]] = data[i]; }
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(buf+i+128);  data[++b1[(buf[i]>>11) & 0x7FF]] = buf[i]; }
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(data+i+128); buf[++b2[(data[i]>>22) & 0x7FF]] = data[i]; }
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(buf+i+128);  data[++b3[(buf[i]>>33) & 0x7FF]] = buf[i]; }
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(data+i+128); buf[++b4[(data[i]>>44) & 0x7FF]] = data[i]; }
        for (uint32_t i = 0; i < n; ++i) { GKS_PREFETCH(buf+i+128);  data[++b5[buf[i]>>55]] = buf[i]; }
    }
}

// ── GKS Hull — Graham-Kurzer Scan ────────────────────────────────────────
//
// Returns the convex hull vertices in counter-clockwise order.
// Input points are not modified.
//
// Complexity: O(n) expected for fixed-width float keys.
//
// Each point is packed into a 64-bit key: upper 32 bits = sortable
// pseudoangle, lower 32 bits = original index.  The 6-pass radix sort
// orders primarily by pseudoangle; equal-angle ties are broken by a
// post-sort fixup that orders runs nearest-first by distance (free for
// non-degenerate inputs).

inline std::vector<Vec2> compute_hull(const std::vector<Vec2>& points) {
    const size_t n = points.size();

    if (n < 3) {
        return points;
    }

    // ── Step 1: Find lowest-leftmost point (guaranteed on hull) ─────
    size_t anchor_idx = 0;
    for (size_t i = 1; i < n; ++i) {
        if (points[i].y < points[anchor_idx].y ||
            (points[i].y == points[anchor_idx].y &&
             points[i].x < points[anchor_idx].x)) {
            anchor_idx = i;
        }
    }
    Vec2 anchor = points[anchor_idx];

    const uint32_t m = static_cast<uint32_t>(n - 1);

    // ── Step 2: Pack (sortable_angle | index) into 64-bit keys ──────
    // Stack buffers for small N (avoids heap allocation entirely).
    // For large N, heap-allocate.  Threshold matches radix sparse path.
    constexpr uint32_t kStackMax = 512;
    uint64_t local_keys[kStackMax], local_buf[kStackMax];
    Vec2 local_hull[kStackMax + 1];
    std::vector<uint64_t> heap_keys, heap_buf;
    std::vector<Vec2> heap_hull;

    uint64_t* keys;
    uint64_t* buf;
    Vec2* hull;
    if (m <= kStackMax) {
        keys = local_keys;
        buf  = local_buf;
        hull = local_hull;
    } else {
        heap_keys.resize(m);
        heap_buf.resize(m);
        heap_hull.resize(n);
        keys = heap_keys.data();
        buf  = heap_buf.data();
        hull = heap_hull.data();
    }

    uint32_t k = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) {
        if (i == static_cast<uint32_t>(anchor_idx)) continue;
        float dx = points[i].x - anchor.x;
        float dy = points[i].y - anchor.y;
        // Pseudoangle: -dx / (|dx| + dy), monotonic over [0, π]
        // since anchor is lowest-leftmost, dy >= 0 always holds.
        // Coincident points (dx=dy=0) produce NaN → sorts last → scan pops them.
        float pseudo = -dx / (std::abs(dx) + dy);
        uint32_t sortable = float_to_sortable(pseudo);
        keys[k] = (static_cast<uint64_t>(sortable) << 32) | i;
        ++k;
    }

    // ── Step 3: Radix sort (O(n), 6 passes) ─────────────────────────
    radix_sort_64(keys, buf, m);

    // ── Step 3b: Colinear fixup ─────────────────────────────────────
    // For runs of equal angle (same upper 32 bits), sort nearest-first
    // by distance from anchor.  In practice these runs are tiny or
    // nonexistent, so this is effectively free.
    for (uint32_t i = 0; i < m; ) {
        uint32_t j = i + 1;
        uint32_t angle_hi = static_cast<uint32_t>(keys[i] >> 32);
        while (j < m && static_cast<uint32_t>(keys[j] >> 32) == angle_hi)
            ++j;
        if (j - i > 1) {
            std::sort(keys + i, keys + j,
                [&](uint64_t a, uint64_t b) {
                    uint32_t ia = static_cast<uint32_t>(a & 0xFFFFFFFF);
                    uint32_t ib = static_cast<uint32_t>(b & 0xFFFFFFFF);
                    float dxa = points[ia].x - anchor.x;
                    float dya = points[ia].y - anchor.y;
                    float dxb = points[ib].x - anchor.x;
                    float dyb = points[ib].y - anchor.y;
                    return (dxa*dxa + dya*dya) < (dxb*dxb + dyb*dyb);
                });
        }
        i = j;
    }

    // ── Step 4: Graham scan on raw array (no vector overhead) ────────
    uint32_t hull_top = 0;
    hull[hull_top++] = anchor;

    for (uint32_t i = 0; i < m; ++i) {
        uint32_t idx = static_cast<uint32_t>(keys[i] & 0xFFFFFFFF);
        Vec2 pt = points[idx];
        while (hull_top >= 2 &&
               ccw(hull[hull_top - 2], hull[hull_top - 1], pt) <= 0.0) {
            hull_top--;
        }
        hull[hull_top++] = pt;
    }

    return std::vector<Vec2>(hull, hull + hull_top);
}

} // namespace gks
