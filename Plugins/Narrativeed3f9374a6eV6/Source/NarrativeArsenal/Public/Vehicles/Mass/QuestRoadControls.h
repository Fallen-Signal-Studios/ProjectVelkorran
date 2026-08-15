// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "TrafficLightIntersectionData.h"
#include "QuestRoadControls.generated.h"

enum class EIntersectionSideRule : uint8;
class AMassVehicleSpawner;
class AMassSpawner;
class URoadControlAnnotationsComponent;
class UBoxComponent;

USTRUCT()
struct FIntersectionSideOverride
{
	GENERATED_BODY()
	
	FIntersectionSideOverride() = default;

	// The location that will be used to query for nearby intersection sides
	UPROPERTY(VisibleAnywhere, Category = "Side Override", meta=(MakeEditWidget))
	FVector IntersectionSideLocation = FVector::ZeroVector;

	// The override that we want to apply to the specified intersection side
	UPROPERTY(EditAnywhere, Category = "Side Override", meta = (Bitmask, BitmaskEnum = "/Script/NarrativeArsenal.EIntersectionSideRule"))
	uint8 Rule = (uint8)EIntersectionSideRule::AllClosed;
};

/**
 * Controller for adjusting zonegraph lanes tags and other features at runtime.
 */
UCLASS(Blueprintable)
class NARRATIVEARSENAL_API AQuestRoadControls : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AQuestRoadControls();

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& GUID) override;

	// Returns whether the road controls actor is actively managing mass vehicle spawning
	UFUNCTION(BlueprintCallable, Category = "Road Controls")
	virtual bool IsActive() const;

	// Sets this quest road controls as active, this will automatically respawn mass vehicles
	UFUNCTION(BlueprintCallable, Category = "Road Controls")
	virtual void SetActive(bool bNewActive);

	// Defines the intersection sides that will be overriden when the quest road controls actor is active
	UPROPERTY(EditAnywhere, Category = "Road Controls|Intersection")
	TArray<FIntersectionSideOverride> IntersectionSideOverrides;

	// The query extent when searching for nearby intersection sides to override
	UPROPERTY(EditAnywhere, Category = "Road Controls|Intersection")
	FVector IntersectionSideQueryExtent = FVector(1000.f);
	
	// Determines whether the road controls will automatically respawn vehicles on begin play
	UPROPERTY(EditAnywhere, Category = "Road Controls")
	bool bAutoActivate = true;

	// Defines how many mass vehicles should be spawned within the box bounds
	UPROPERTY(EditAnywhere, Category = "Road Controls|Spawning")
	int32 NewSpawnCount = 5;

	UPROPERTY(BlueprintReadOnly, Category = "Road Controls")
	FGuid RoadControlsSaveGUID;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category="Road Controls|AreaShape")
	URoadControlAnnotationsComponent* RoadControlAnnotationComponent;

	TArray<FZoneGraphLaneHandle> CachedLanes;

	// Tags that will be added to the overlapping lanes
	UPROPERTY(Category = "Road Controls|Annotation", EditAnywhere)
	FZoneGraphTagMask TagsToAdd = FZoneGraphTagMask::None;

	UPROPERTY(Category = "Road Controls|Zone", EditAnywhere)
	FZoneGraphTagMask AnyTags = FZoneGraphTagMask::None;

	UPROPERTY(Category = "Road Controls|Zone", EditAnywhere)
	FZoneGraphTagMask AllTags = FZoneGraphTagMask::None;

	UPROPERTY(Category = "Road Controls|Zone", EditAnywhere)
	FZoneGraphTagMask NotTags = FZoneGraphTagMask::None;

	UPROPERTY()
	AMassVehicleSpawner* VehicleSpawner;

	// Lets mass spawner know whether this quest road control is active even before it gets destroyed
	bool bIsActive = false;

	// Stores the original spawn count in the mass spawner
	int OldSpawnCount = 0;
};
