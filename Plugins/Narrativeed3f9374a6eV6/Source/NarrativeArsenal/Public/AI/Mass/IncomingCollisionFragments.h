// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"
#include "MassEntityHandle.h"
#include "IncomingCollisionFragments.generated.h"

// Stores data related to the closest entity that we will collide with
USTRUCT()
struct FIncomingCollisionFragment : public FMassFragment
{
	GENERATED_BODY()

	FIncomingCollisionFragment() = default;

	UPROPERTY()
	float TimeUntilCollision = FLT_MAX;
	
	float DistanceToObstacle = FLT_MAX;
	
	FMassEntityHandle IncomingEntity = FMassEntityHandle();

	FMassEntityHandle LastEventEntity = FMassEntityHandle();

	void Reset()
	{
		TimeUntilCollision = FLT_MAX;
		DistanceToObstacle = FLT_MAX;
		IncomingEntity.Reset();
	}
};

// General settings for managing incoming collision
USTRUCT()
struct FCollisionDetectionSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// The min speed to consider a colliding entity.
	UPROPERTY(EditAnywhere, Category = "Collision Detection")
	float MinSpeed = 100.f;

	// Average speed that this entity will move at. This will help improve collision detection by keeping entity velocity constant
	UPROPERTY(EditAnywhere, Category = "Collision Detection")
	float EntityAverageSpeed = 100.f;

	// The radius multiplier for incoming dangerous obstacles. This gives entities more room to react to incoming collisions
	UPROPERTY(EditAnywhere, Category = "Collision Detection")
	float ObstacleRadiusMultiplier = 3.f;

	// The range we check for incoming obstacles
	UPROPERTY(EditAnywhere, Category = "Collision Detection")
	float ObstacleQueryRange = 2000.f;

	// Time in seconds to trigger IncomingDangerousEntity() on entities when there is an incoming collision
	UPROPERTY(EditAnywhere, Category = "Collision Detection")
	float TimeToCollisionEvent = 1.f;
};
