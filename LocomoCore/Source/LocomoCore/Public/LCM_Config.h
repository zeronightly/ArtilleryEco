#pragma once
#include "CoreMinimal.h"
#include "KeyedConcept.h"
#include "ORDIN.h"
#include "Engine/Engine.h"
#include "Subsystems/WorldSubsystem.h"

THIRD_PARTY_INCLUDES_START
#include "immintrin.h"

#include <memory>
#include "Memory/sseutil.h"
#include "Memory/aligned_allocator.h"
#define NO_BLAZE true
#define NOT_THREADSAFE true
#ifndef NOSVML
	#define NOSVML true
#endif
#ifndef NO_SLEEF
	#define NO_SLEEF true
#endif
#ifndef __SSE2__
	#define __SSE2__ true
#endif
#ifndef __AVX2__
	#define __AVX2__ true
#endif
#ifndef __AVX__
	#define __AVX__ true
#endif

#define LCM_USE_SSE4_2 true
//HI! you MAY want to change this. Might. Maybe. You know. If you value your life, hope, or sanity.

#if defined(_MSC_VER) && !defined(__clang__)
#include "Fallbacks/uint128.h"
using __uint128_t=Fortran::common::uint128_t;
#endif
//you could get this working...
//SKETCHALLOC sse::AlignedAllocator<ValueType, AllocatorAlignment>;

#define SKETCHALLOC AlignedAllocator<ValueType, AllocatorAlignment>
THIRD_PARTY_INCLUDES_END

#include "LCM_Config.generated.h"

UCLASS()
class UWorldAwareCommons : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
public:
	//these are the same for the default, but as you begin to do more...
	static constexpr double Hz = 1/128;
	static constexpr double Dt = 1/128;
	
	//anything less than step cubed is substrate and artillery just expects it to Be There. Useful for little bobbins like this.
	
	constexpr static int32 OrdinateSeqKey = ORDIN::E_D_C::LocomoCommons;
	static UWorldAwareCommons* Get(UWorld& World)
	{
		return World.GetSubsystem<UWorldAwareCommons>();
	}
	
	static UWorldAwareCommons* Get(UObject* WorldContextObject) 
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (ensure(World)) 
		{
			return UWorldAwareCommons::Get(*World);
		}

		return nullptr;
	}
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UArtillerySkeletalMeshDispatch, STATGROUP_Tickables); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
	}

	virtual bool RegistrationImplementation() override
	{
		return true;
	}
};