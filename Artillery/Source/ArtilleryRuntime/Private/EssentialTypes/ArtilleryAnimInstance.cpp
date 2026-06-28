// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtilleryAnimInstance.h"

#include "FBarragePrimitive.h"
#include "MegafunkUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsTypes/BarrageColliderBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArtilleryAnimInstance)

void UArtilleryAnimInstance::NativeInitializeAnimation() 
{
	Super::NativeInitializeAnimation();
	if (USkeletalMeshComponent* SkelMeshComp = GetSkelMeshComponentUnchecked())
	{
		for (USceneComponent* CurrentSceneComp = SkelMeshComp; IsValid(CurrentSceneComp); CurrentSceneComp = CurrentSceneComp->GetAttachParent()) 
		{
			//this is a back-up!
			if (UBarrageColliderBase* BarrageComponentPtr = Cast<UBarrageColliderBase>(CurrentSceneComp)) {
				MyBarrageBody = BarrageComponentPtr->MyBarrageBody;
				break;
			}
		}
	}
}

//fun story, seems like this is never called and I don't know why. it's right there in the damn graph.
//or it is called and my breakpoints don't work for it and it always returns a zero vector. I'm pretty damn sure barrage body is defined though.
FVector UArtilleryAnimInstance::GetArtilleryOwnerVelocity() const 
{
	if (MyBarrageBody)
	{
		const FVector3f Velocity = FBarragePrimitive::GetVelocity(MyBarrageBody); //to self: consider reverting this just to see if it works.
		return FVector(Velocity);
	}
	//determinism hazard - this may only fire in some situations. it's a read only call, so it should be fine. there's no order change that results.
	return FVector::ZeroVector;
}
