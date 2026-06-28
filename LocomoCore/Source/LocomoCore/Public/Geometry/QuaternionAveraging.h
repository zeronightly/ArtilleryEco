#pragma once

#include "CoreMinimal.h"

// Markley, F. Landis, Yang Cheng, John Lucas Crassidis, and Yaakov Oshman.
// "Averaging quaternions." Journal of Guidance, Control, and Dynamics 30,
// no. 4 (2007): 1193-1197.
//
// Original MATLAB implementation by Tolga Birdal.
// Converted to Unreal Engine types.

// Returns the average of the given quaternions using the eigenvector method.
LOCOMOCORE_API FQuat AvgQuaternionMarkley(TArrayView<const FQuat> Quaternions);

// Returns the weighted average of the given quaternions using the eigenvector method.
// Weights and Quaternions must have the same length.
LOCOMOCORE_API FQuat WAvgQuaternionMarkley(TArrayView<const FQuat> Quaternions, TArrayView<const float> Weights);
