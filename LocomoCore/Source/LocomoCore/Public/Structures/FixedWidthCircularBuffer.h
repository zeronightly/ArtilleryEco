// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

/**
 * Template for circular buffers.
 *
 * The size of the buffer is rounded up to the next power of two in order speed up indexing
 * operations using a simple bit mask instead of the commonly used modulus operator that may
 * be slow on some platforms.
 */
template<typename InElementType, uint32 Width> class TFixedCircular
{
public:
	using ElementType = InElementType;

	/**
	 * Creates and initializes a new instance of the TFixedCircular class.
	 *
	 */
	[[nodiscard]] explicit TFixedCircular()
	{
	}

public:

	/**
	 * Returns the mutable element at the specified index.
	 *
	 * @param Index The index of the element to return.
	 */
	[[nodiscard]] UE_FORCEINLINE_HINT ElementType& operator[](uint32 Index)
	{
		return Elements[Index & IndexMask];
	}

	/**
	 * Returns the immutable element at the specified index.
	 *
	 * @param Index The index of the element to return.
	 */
	[[nodiscard]] UE_FORCEINLINE_HINT const ElementType& operator[](uint32 Index) const
	{
		return Elements[Index & IndexMask];
	}
	
public:

	/**
	 * Returns the number of elements that the buffer can hold.
	 *
	 * @return Buffer capacity.
	 */
	[[nodiscard]] UE_FORCEINLINE_HINT uint32 Capacity() const
	{
		return Width;
	}

	/**
	 * Calculates the index that follows the given index.
	 *
	 * @param CurrentIndex The current index.
	 * @return The next index.
	 */
	[[nodiscard]] UE_FORCEINLINE_HINT uint32 GetNextIndex(uint32 CurrentIndex) const
	{
		return ((CurrentIndex + 1) & IndexMask);
	}

	/**
	 * Calculates the index previous to the given index.
	 *
	 * @param CurrentIndex The current index.
	 * @return The previous index.
	 */
	[[nodiscard]] UE_FORCEINLINE_HINT uint32 GetPreviousIndex(uint32 CurrentIndex) const
	{
		return ((CurrentIndex - 1) & IndexMask);
	}

private:

	/** Holds the mask for indexing the buffer's elements. */
	static constexpr uint32 IndexMask = Width - 1; // static is safe because width is a template parameter.

	/** Holds the buffer's elements. */
	ElementType Elements[Width];
};
