// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Structures/FixedWidthCircularBuffer.h"
#include "Structures/ConcurrencyTypes/TimeCoheredReadHead.h"

#include "ConservedAttribute.generated.h"

/**
 * Conserved attributes record their last 128 changes.
 * Currently, this is for debug purposes, but we can use it with some additional features to provide a really expressive
 * model for rollback at a SUPER granular level if needed. 
 */
//TODO: do we need to break the GAS dependency? It's forcing a lot of unneeded stuff.
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FConservedAttributeData
{
	GENERATED_BODY()
	
	using Buff = TFixedCircular<double, 32>;
	virtual ~FConservedAttributeData() = default;
	Buff CurrentHistory = Buff();
	Buff BaseHistory = Buff();
	
	virtual void SetCurrentValue(double NewValue) {
		CurrentHistory[CurrentHistory.GetNextIndex(heads.CurrentHead)] = state.CurrentValue.Read();
		thread_local auto THREAD_Key = Keys.GenerateMyID();
		state.CurrentValue.Write(NewValue, THREAD_Key);
		++heads.CurrentHead;
	};

	virtual double GetCurrentValue() {
		return state.CurrentValue.Read();
	};
	
	virtual void AddToCurrentValue(double AddValue)
	{
		SetCurrentValue(state.CurrentValue.Read() + AddValue);
	}
	
	virtual double GetBaseValue() {
		return state.BaseValue;
	};
	
	virtual void SetBaseValue(float NewValue) {
		SetBaseValue(static_cast<double>(NewValue));
	};

	virtual void SetBaseValue(double NewValue) {
		BaseHistory[BaseHistory.GetNextIndex(heads.BaseHead)] = state.BaseValue;
		state.BaseValue = NewValue;
		++heads.BaseHead;
	};

	double GetPriorValue()
	{
		return CurrentHistory[CurrentHistory.GetNextIndex(heads.CurrentHead-2)]; // ugh
	}
	
	double operator*(FConservedAttributeData const& rhs) 
	{ 
		return state.CurrentValue.Read() * rhs.state.CurrentValue.Read(); // this is a double op.
	};
	
	double operator*(int const& rhs) 
	{ 
		return state.CurrentValue.Read() * rhs; // this is a double op.
	}
	
	double operator*(uint64 const& rhs) 
	{ 
		return state.CurrentValue.Read() * rhs; // this is a double op.
	}
	
protected:
	
	struct indexes
	{
		long long volatile CurrentHead = 0;
		long long BaseHead = 0;
	};
	
	struct values
	{
		FTimedArray CurrentValue;
		double BaseValue = 0;
	};
	values state;
	indexes heads;
	FArrayGrouping Keys;
};

