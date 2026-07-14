// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FArtilleryGun.h"
#include "Public/GameplayTags.h"
#include "FTArcingProjectile.h"

#include "FGunWeeArtillery.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FGunWeeArtillery : public FArtilleryGun
{
	GENERATED_BODY()

public:
	FGunWeeArtillery(const FGunKey& KeyFromDispatch, int FireRateIn, float RangeIn, float ApogeeModifierIn, UArtilleryDispatch* Dispatch)
	{
		MyGunKey = KeyFromDispatch;
		Firerate = FireRateIn;
		Range = RangeIn;
		ApogeeModifier = ApogeeModifierIn;
	}
	
	//Dispatch will get assigned during gun bake, see GetGun from Artillery Dispatch
	FGunWeeArtillery() : FGunWeeArtillery(
		Default,
		240,
		0.f,
		0.f, nullptr) {}

	virtual bool Initialize(
		const FGunKey& KeyFromDispatch,
		const bool MyCodeWillHandleKeys,
		UArtilleryPerActorAbilityMinimum* PF = nullptr,
		UArtilleryPerActorAbilityMinimum* PFC = nullptr,
		UArtilleryPerActorAbilityMinimum* F = nullptr,
		UArtilleryPerActorAbilityMinimum* FC = nullptr,
		UArtilleryPerActorAbilityMinimum* PtF = nullptr,
		UArtilleryPerActorAbilityMinimum* PtFc = nullptr,
		UArtilleryPerActorAbilityMinimum* FFC = nullptr) override
	{
		ARTGUN_MACROAUTOINIT(MyCodeWillHandleKeys);
		return true;
	}
	
	virtual void PreFireGun(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const EventBufferInfo FireAction = EventBufferInfo::Default(),
		const FGameplayEventData* TriggerEventData = nullptr,
		bool RerunDueToReconcile = false,
		int DallyFramesToOmit = 0, bool VerifiedFrame = false) override
	{
		AttrPtr CooldownRemainingPtr = MyDispatch->GetAttrib(MyGunKey, COOLDOWN_REMAINING);
		if (!CooldownRemainingPtr.IsValid() || CooldownRemainingPtr->GetCurrentValue() > 0.f)
		{
			// Cooldown not up yet!
			return;
		}
		
		TWeakObjectPtr<AActor> Me = MyTransformDispatch->GetAActorByObjectKey(MyProbableOwner);
		if(Me.Get())
		{
			FireGun(Fired, 0, ActorInfo, ActivationInfo, false, TriggerEventData, Handle);
		}
	}

	virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle) override
	{
		UArtilleryProjectileDispatch* ProjectileDispatch = MyDispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
		if (ProjectileDispatch)
		{
			FVector TargetLocation = UArtilleryLibrary::GetLocalPlayer_UNSAFE(MyDispatch)->GetActorLocation();
			FVector StartLocation = FiringPointComponent->GetComponentLocation();
			TArray<FGameplayTag> ProjectileTags;
			ProjectileTags.Add(TAG_EnemyProjectile);
			FSkeletonKey MissileKey = ProjectileDispatch->QueueProjectileInstance(
				TEXT("Shell"), MyGunKey, StartLocation, FVector::Zero(), 1.6f, Layers::ENEMYPROJECTILE, &ProjectileTags);
			StructureFullTL_Dispatch(MyDispatch,
				ProjectileArc,TL_ArcingProjectile,FTArcingProjectile, MissileKey, TargetLocation, StartLocation, 180, 8000.f);
			MyDispatch->RequestAddTicklite(ProjectileArc, Early);
		}
		
		PostFireGun(Fired, 0, ActorInfo, ActivationInfo, false, TriggerEventData, Handle);
	}

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle) override
	{
		AttrMapPtr MyAttribs = MyDispatch->GetAttribMap(MyGunKey);
		AttrPtr CooldownPtr = MyAttribs->FindRef(COOLDOWN);
		AttrPtr CooldownRemainingPtr = MyAttribs->FindRef(COOLDOWN_REMAINING);
		if (CooldownPtr.IsValid() && CooldownRemainingPtr.IsValid())
		{
			CooldownRemainingPtr->SetCurrentValue(CooldownPtr->GetCurrentValue());
		}
	}

private:
	float Range;
	float ApogeeModifier;
	
	static const inline FGunKey Default = FGunKey("WeeArtillery");
};
