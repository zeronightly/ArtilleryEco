// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Containers/CircularBuffer.h"

#include "ConservedVector.generated.h"

static constexpr uint32 CONSERVED_VECTIR_ATTRIBUTE_BUFFER_SIZE = 20;

/**
 * Conserved attributes record their last 20 changes.
 * Currently, this is for debug purposes, but we can use it with some additional features to provide a really expressive
 * model for rollback at a SUPER granular level if needed. 
 */
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FConservedVector
{
	GENERATED_BODY()
	virtual ~FConservedVector() = default;
	using Buff = TFixedCircular<FVector3d, CONSERVED_VECTIR_ATTRIBUTE_BUFFER_SIZE>;
	Buff CurrentHistory = Buff();


	virtual void SetCurrentValue(FVector3d NewValue) {
		CurrentHistory[CurrentHistory.GetNextIndex(CurrentHead)] = CurrentValue;
		CurrentValue = NewValue;
		++CurrentHead;
	};
	
	FVector3d CurrentValue = FVector3d::ZeroVector;
	uint64_t CurrentHead = 0;
};

