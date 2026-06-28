// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoordinateUtils.h"

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif 
#include "CoreMinimal.h"
#include "DrawDebugHelpers.h"
#include <Jolt/Jolt.h>
#include <Jolt/Math/Real.h>
#include "Chaos/DebugDrawQueue.h"
#include "ImGuiConfig.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DebugRenderer.generated.h"


//this can be swapped in for the jolt simple debug renderer, but also provides a little encapsulated way to do decent drag from, say, ticklites.
UCLASS()
class BARRAGE_API UDeferredDebugRenderer : public UObject
{
	GENERATED_BODY()
public:
	UDeferredDebugRenderer()
	{
	};

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	
	//should prolly be variadic
	//this draws from jolt...
	virtual void DrawLine(FVector& From, FVector& To, FColor& InColor)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("Not Set!"));
			return;
		}
		FVector DebugFrom{From.X, From.Z, From.Y};
		FVector DebugTo{To.X, To.Z, To.Y};
		auto& quickbind = Chaos::FDebugDrawQueue::GetInstance();

		quickbind.DrawDebugLine(
			From, To, InColor, true, 3);
	};

	virtual void DrawLine(JPH::Vec3& From, JPH::Vec3& To, FColor& InColor)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("Not Set!"));
			return;
		}
		auto DebugFrom = CoordinateUtils::FromJoltCoordinatesFakePrecision(From);
		auto DebugTo = CoordinateUtils::FromJoltCoordinatesFakePrecision(To);
		auto& quickbind = Chaos::FDebugDrawQueue::GetInstance();

		quickbind.DrawDebugLine(
			DebugFrom, DebugTo, InColor, true, 3);
	};
	
	virtual void DrawTriangle(FVector& Vec1, FVector& Vec2, FVector& Vec3, FColor& InColor)
	{
		auto& quickbind = Chaos::FDebugDrawQueue::GetInstance();

		quickbind.DrawDebugLine(
			Vec1, Vec2, InColor, true, 0.25);
		quickbind.DrawDebugLine(
			Vec2, Vec3, InColor, true, 0.25);
		quickbind.DrawDebugLine(
			Vec3, Vec1, InColor, true, 0.25);
	};
	UWorld* World = nullptr;
};
