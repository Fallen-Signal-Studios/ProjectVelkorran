// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassSpawner.h"
#include "MassStateTreeTypes.h"
#include "MassVehicleSpawner.generated.h"

class UTrafficIntersectionAnnotations;

UCLASS()
class AMassVehicleSpawner : public AMassSpawner
{
	GENERATED_BODY()

public:
	AMassVehicleSpawner();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Annotations")
	UTrafficIntersectionAnnotations* IntersectionAnnotations;

	// Sets the spawn count of the mass spawner. Will take effect next time entities are spawned with this spawner
	UFUNCTION(BlueprintCallable, Category = "Vehicle Spawner")
	void SetSpawnCount(int NewCount);
};
