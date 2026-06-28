// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BarrageDeveloperSettings.generated.h"

/**
 *
 */
UCLASS(config = Game, defaultconfig)
class ARTILLERYRUNTIME_API UBarrageDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableDebugDraw = false;

	// Debug visualization colors and settings
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	FColor ShapeColor = FColor::Red;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	FColor ShapeColorServer = FColor::Green;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	FColor VelocityColor = FColor(0, 0, 255, 255);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	FColor GroundStateColor = FColor(255, 255, 0, 255);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	float VelocityScale = 0.1f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	float ShapeThickness = 8.0f;
};
