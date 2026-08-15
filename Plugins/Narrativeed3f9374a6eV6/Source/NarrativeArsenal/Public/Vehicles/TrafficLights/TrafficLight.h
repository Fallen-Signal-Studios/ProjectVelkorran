// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Vehicles/Mass/TrafficLightIntersectionData.h"
#include "GameFramework/Actor.h"
#include "TrafficLight.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class NARRATIVEARSENAL_API ATrafficLight : public AActor
{
	GENERATED_BODY()

public:
	ATrafficLight();

	virtual void BeginPlay() override;

	// Gets the current period of this traffic light. If the value is 0, then the current period does not deal with this traffic light (we can assume red light), or that the cached side is not valid.
	UFUNCTION(BlueprintCallable, Category="TrafficLight")
	float GetPeriod(UPARAM(meta=(Bitmask, BitmaskEnum=EIntersectionSideRule)) uint8 Rule) const;

	// Caches the intersection side contained at the SideLocation. This should be used before running GetPeriod.
	void CacheIntersectionSide();

	// Gets called whenever the cached intersection gets a new period
	UFUNCTION(BlueprintNativeEvent, Category="TrafficLight")
	void OnPeriodUpdated();

	// The location of the intersection side to be used for this traffic light
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TrafficLight", meta=(MakeEditWidget))
	FVector SideLocation;

	// Determines the extent to query for an intersection side around SideLocation
	// Note that setting the query extent too small can create some false positives when querying for sides.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TrafficLight")
	FVector QueryExtent = FVector(3000.f);
	
	UPROPERTY()
	FTrafficIntersectionSideHandle CachedIntersectionSide = FTrafficIntersectionSideHandle();
};
