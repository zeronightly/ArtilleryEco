// Boost.Geometry Index
//
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// ============================================================================
// VENDORED PATCH (Booster) — DETERMINISM
// The upstream implementation delegated to std::nth_element, whose arrangement of
// equal-key elements is STL-implementation-defined. That makes the packing/bulk
// tree structure diverge across platforms (MSVC STL / libstdc++ / libc++) whenever
// coincident coordinates exist — fatal for multi-platform rollback/lockstep.
// (Upstream even shipped a libstdc++-version #ifdef workaround here; see git blame.)
//
// Replaced with miniselect::median_of_ninthers_select — a self-contained,
// fully deterministic selection (Alexandrescu's median-of-ninthers) with NO fallback
// to std::nth_element/sort/heap and no randomization. Same code arranges equal keys
// identically on every platform.
//
// If re-extracting Boost with bcp, re-apply this patch. See thirdparty/DETERMINISM.md.
// ============================================================================

#ifndef BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_NTH_ELEMENT_HPP
#define BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_NTH_ELEMENT_HPP

#include <functional> // std::less

#include <miniselect/median_of_ninthers.h>

namespace boost { namespace geometry { namespace index { namespace detail {

template <typename RandomIt>
void nth_element(RandomIt first, RandomIt nth, RandomIt last)
{
    miniselect::median_of_ninthers_select(first, nth, last);
}

template <typename RandomIt, typename Compare>
void nth_element(RandomIt first, RandomIt nth, RandomIt last, Compare comp)
{
    miniselect::median_of_ninthers_select(first, nth, last, comp);
}

}}}} // namespace boost::geometry::index::detail

#endif // BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_NTH_ELEMENT_HPP
