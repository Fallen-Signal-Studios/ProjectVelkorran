// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
class AActor;
namespace SovAimAssist
{
/** Per-release bounds, taken from the existing attack and its actual collision body. */
struct PROJECTVELKORRAN_API FProjectileLeadRequest
{
	FVector AimDirection = FVector::ForwardVector;
	FVector InitialVelocity = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	float Range = 5000.f;
	float MaximumFlightSeconds = .6f;
	float MaximumCorrectionDegrees = 8.f;
	float CollisionRadius = 0.f;
	float MaximumFlightSpeed = 0.f; // Zero matches ProjectileMovement's unlimited-speed policy.
	ECollisionChannel CollisionChannel = ECC_Visibility;
	FCollisionResponseContainer CollisionResponses = FCollisionResponseContainer::DefaultResponseContainer;
	bool bPreferHighArc = false;
};
/** Visible living hostile nearest the supplied aim direction. No hard-lock eligibility or target damage changes. */
PROJECTVELKORRAN_API bool FindVisibleTarget(AActor* Shooter, const FVector& Origin, const FVector& Direction,
	float Range, float ConeDegrees, AActor*& OutTarget, FVector& OutPoint);
/** Constant-speed, zero-gravity lead with a 0.6-second horizon and 8-degree correction limit. */
PROJECTVELKORRAN_API bool GetProjectileLead(AActor* Shooter, const FVector& Muzzle, const FVector& InitialDirection,
	float Speed, float Range, FVector& OutDirection);
/** Corrects only launch velocity. Rejected candidates preserve the exact supplied launch. */
PROJECTVELKORRAN_API bool GetBallisticProjectileLead(AActor* Shooter, const FVector& Muzzle,
	const FProjectileLeadRequest& Request, FVector& OutVelocity);
}
