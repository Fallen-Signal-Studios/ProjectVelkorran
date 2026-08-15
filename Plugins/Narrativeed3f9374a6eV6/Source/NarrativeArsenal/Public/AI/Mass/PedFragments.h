// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"
#include "PedFragments.generated.h"

// Marks an entity as 'dangerous'. This can mean various things, but the main idea is that we generally don't want to be near it.
USTRUCT()
struct FDangerousEntity : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FDangerDetectionSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// The min speed to consider a dangerous entity.
	UPROPERTY(EditAnywhere, Category = "Ped Danger")
	float MinSpeed = 100.f;

	// The radius multiplier for incoming dangerous obstacles. This gives peds more room to react to incoming dangerous obstacles
	UPROPERTY(EditAnywhere, Category = "Ped Danger")
	float ObstacleRadiusMultiplier = 3.f;

	// The range we check for incoming dangerous obstacles
	UPROPERTY(EditAnywhere, Category = "Ped Danger")
	float ObstacleQueryExtent = 2000.f;

	// Time in seconds to trigger IncomingDangerousEntity() on ped NPCs when there is an incoming dangerous vehicle
	UPROPERTY(EditAnywhere, Category = "Ped Danger")
	float TimeToCollisionEvent = 1.f;
};
