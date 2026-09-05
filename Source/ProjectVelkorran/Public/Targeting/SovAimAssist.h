// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class AActor;
namespace SovAimAssist
{
/** Visible living hostile nearest the supplied aim direction. No hard-lock eligibility or target damage changes. */
PROJECTVELKORRAN_API bool FindVisibleTarget(AActor* Shooter, const FVector& Origin, const FVector& Direction,
	float Range, float ConeDegrees, AActor*& OutTarget, FVector& OutPoint);
/** Constant-speed, zero-gravity lead with a 0.6-second horizon and 8-degree correction limit. */
PROJECTVELKORRAN_API bool GetProjectileLead(AActor* Shooter, const FVector& Muzzle, const FVector& InitialDirection,
	float Speed, float Range, FVector& OutDirection);
}
