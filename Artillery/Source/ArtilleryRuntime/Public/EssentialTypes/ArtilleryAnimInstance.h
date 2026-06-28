// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FBarragePrimitive.h"
#include "Animation/AnimInstance.h"
#include "ArtilleryAnimInstance.generated.h"

class UBarrageColliderBase;

/**
 * Optional custom anim instance. Exists mainly to expose thread-safe artillery access for BP use
 */
UCLASS()
class ARTILLERYRUNTIME_API UArtilleryAnimInstance : public UAnimInstance 
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	
	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	FVector GetArtilleryOwnerVelocity() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSkeletonKey MyParentObjectKey;
	
	// @todo this is a shared ptr that can die with GC and invoke a dtor at a VERY bad time
	FBLet MyBarrageBody = nullptr;
};
