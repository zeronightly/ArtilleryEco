
#pragma once

#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "MegafunkAnimUtilsTypes.generated.h"

static bool GbMegafunkUtilsAnySubsystemOnlineEnabled = false;
static FAutoConsoleVariableRef CVarMegafunkUtilsAnySubsystemEnabled(
	TEXT("MegafunkUtils.ExampleSkeletalMeshManager.Enabled"),
	GbMegafunkUtilsAnySubsystemOnlineEnabled,
	TEXT("If the any world subsystem built on this is enabled, you should flip this switch. Enabled by artillery, disabled by default."),
	ECVF_Default);
// This is intended to demonstrate how little data we actually need to get out of the anim evaluation steps
// Note these can be stored anywhere we want
// @Todo I will probably remove these in favor of users providing their own bone storage
USTRUCT()
struct FMegafunkUtilsAnimationEvaluationContainer {
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FTransform> ComponentSpaceTransforms;
	
	UPROPERTY()
	TArray<FTransform> BoneSpaceTransforms;
	
	UPROPERTY(VisibleAnywhere)
	FVector OutRootBoneLocationResult = FVector::ZeroVector;
	
	FBlendedHeapCurve Curve;
	UE::Anim::FMeshAttributeContainer Attributes;
};

