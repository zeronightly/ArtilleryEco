#pragma once
#include "FixedMVReg.h"


template <typename  HasAddAndOrderingAndTrivial>
class TFixedMvdReg
{
	using T = HasAddAndOrderingAndTrivial;
	thread_local volatile T MyDelta = 0;
	FixedMVReg<T> A;

public:
	//you must insure that MyId is unique and replicas are properly constructed. this is left to the user.
	TFixedMvdReg(const volatile ::TFixedMvdReg<HasAddAndOrderingAndTrivial>::T& MyDelta, const FixedMVReg<T>& A, uint8_t MyID)
		: MyDelta(MyDelta),
		  A(A)
	{
		A.MyID = MyID;
	}

	T FinalizerAdd(T DeltaOnly)
	{
		MyDelta += DeltaOnly;
		A.Write(MyDelta);
		return A.ResolveAdditive();
	}
};