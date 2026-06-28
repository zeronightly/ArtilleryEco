// SeqU64Prefix.h
// Prefix search on seq::radix_set<uint64_t> using arbitrary-width bit prefixes.
//
// The existing seq prefix()/prefix_range() API calls .find()/.data()/.size() on
// the key type, which is string-only and does not compile for uint64_t.  These
// free functions provide the same semantics using seq's lower_bound/upper_bound.
//
// prefix(s, val, k)       -> iterator to first key whose top k bits == top k bits of val
// prefix_range(s, val, k) -> subrange over all such keys (supports range-for)
//
// Works for any k in [0, 64]:
//   k == 0  -> all keys (same as [begin, end))
//   k == 64 -> exact match only

#pragma once

// seq/radix_map.hpp must already be included (via THIRD_PARTY_INCLUDES_START)
// before including this header.

#include <cstdint>
#include <ranges>

#include "radix_map.hpp"

using SeqSet64 = seq::radix_set<std::uint64_t>;

// ---------------------------------------------------------------------------
// prefix — first key whose top k_bits match val, or end()
// ---------------------------------------------------------------------------
inline SeqSet64::const_iterator
SeqU64Prefix(const SeqSet64& s, std::uint64_t val, unsigned k_bits)
{
    if (s.empty())    return s.end();
    if (k_bits == 0)  return s.begin();
    if (k_bits >= 64) {
        auto it = s.lower_bound(val);
        return (it != s.end() && *it == val) ? it : s.end();
    }

    const unsigned      shift = 64u - k_bits;
    const std::uint64_t lo    = (val >> shift) << shift;

    auto it = s.lower_bound(lo);
    // lower_bound may land in a higher prefix block if ours is empty
    return (it != s.end() && (*it >> shift) == (val >> shift)) ? it : s.end();
}

// ---------------------------------------------------------------------------
// prefix_range — [first, last) over all keys whose top k_bits match val
// ---------------------------------------------------------------------------
inline std::ranges::subrange<SeqSet64::const_iterator>
SeqU64prefix_range(const SeqSet64& s, std::uint64_t val, unsigned k_bits)
{
    if (k_bits == 0)  return {s.begin(), s.end()};
    if (k_bits >= 64) return {s.lower_bound(val), s.upper_bound(val)};

    const unsigned      shift = 64u - k_bits;
    const std::uint64_t lo    = (val >> shift) << shift;
    const std::uint64_t step  = std::uint64_t{1} << shift;
    const std::uint64_t hi    = lo + step;
    // hi wraps to 0 exactly when lo is the last possible prefix block
    // (e.g. k=4, top nibble = 0xF => lo=0xF000..., hi overflows to 0)

    const auto first = s.lower_bound(lo);
    const auto last  = (hi == 0) ? s.end() : s.lower_bound(hi);
    return {first, last};
}

