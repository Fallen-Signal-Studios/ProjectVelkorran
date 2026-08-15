// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "VehiclePIDController.generated.h"

struct FPIDSettings;

USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FVehiclePIDController
{
	GENERATED_BODY()

	float Tick(float Goal, float Actual, const FPIDSettings& Params);

	void ResetErrorIntegral()
	{
		ErrorIntegral = 0.0f;
	}

private:

	UPROPERTY(Transient)
	float ErrorIntegral = 0.0f;

	UPROPERTY(Transient)
	float LastError = 0.0f;
};