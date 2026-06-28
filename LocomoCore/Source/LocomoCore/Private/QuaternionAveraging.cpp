#include "Geometry/QuaternionAveraging.h"
#include "Eigen/Dense"

// Eigen Vector4f layout mirrors FQuat: { X=0, Y=1, Z=2, W=3 }

static Eigen::Vector4f QuatToVec(const FQuat& Q)
{
	return Eigen::Vector4f(Q.X, Q.Y, Q.Z, Q.W);
}

static FQuat VecToQuat(const Eigen::Vector4f& V)
{
	return FQuat(V(0), V(1), V(2), V(3)).GetNormalized();
}

FQuat AvgQuaternionMarkley(TArrayView<const FQuat> Quaternions)
{
	// Form the symmetric accumulator matrix
	Eigen::Matrix4f A = Eigen::Matrix4f::Zero();
	const int32 M = Quaternions.Num();

	for (const FQuat& Quat : Quaternions)
	{
		Eigen::Vector4f Q = QuatToVec(Quat);

		if (Q(3) < 0.f) // handle the antipodal configuration (W < 0)
			Q = -Q;

		A += Q * Q.transpose(); // rank-1 update (outer product)
	}

	A /= static_cast<float>(M);

	// Eigenvectors are sorted ascending; last column is the largest
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix4f> Solver(A);
	return VecToQuat(Solver.eigenvectors().col(3));
}

FQuat WAvgQuaternionMarkley(TArrayView<const FQuat> Quaternions, TArrayView<const float> Weights)
{
	check(Quaternions.Num() == Weights.Num());

	// Form the symmetric accumulator matrix
	Eigen::Matrix4f A = Eigen::Matrix4f::Zero();
	float WSum = 0.f;

	for (int32 i = 0; i < Quaternions.Num(); ++i)
	{
		Eigen::Vector4f Q = QuatToVec(Quaternions[i]);

		if (Q(3) < 0.f) // handle the antipodal configuration (W < 0)
			Q = -Q;

		const float Wi = Weights[i];
		A += Wi * (Q * Q.transpose()); // weighted rank-1 update (outer product)
		WSum += Wi;
	}

	A /= WSum;

	// Eigenvectors are sorted ascending; last column is the largest
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix4f> Solver(A);
	return VecToQuat(Solver.eigenvectors().col(3));
}
