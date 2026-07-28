// Booster — SpatialIndex3D, "whole enchilada" single-include.
//
// Include THIS (instead of spatial_index3d.h) when you'd rather pull the implementation straight into
// your own translation unit than compile Private/spatial_index3d.cpp as a separate TU. Convenient for
// a header-only drop-in or a single-file consumer.
//
// Rules:
//   * Include this in EXACTLY ONE .cpp of your target. It drags the impl's anonymous namespace into
//     that TU; including it in two TUs (or including it AND also building spatial_index3d.cpp) is an
//     ODR / duplicate-symbol error. Everywhere else, include the plain facade spatial_index3d.h.
//   * The impl is pulled by bare name via the include path (the local convention -- see the mapbench's
//     `#include "interleave_index.cpp"`), so the directory holding spatial_index3d.cpp (src/ here,
//     the module's Private/ under Unreal) must be on the include path.
//   * Requires AVX2 (the query kernel), same as the separate-TU build.
#pragma once

#include "booster/spatial_index3d.h"   // Public facade
#include "spatial_index3d.cpp"          // Private impl, resolved via the include path
