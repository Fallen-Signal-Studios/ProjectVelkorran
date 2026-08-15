// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphAnnotationComponent.h"
#include "ZoneGraphAnnotationTypes.h"
#include "RoadControlAnnotationsComponent.generated.h"

USTRUCT()
struct FRoadControlAnnotationEvent : public FZoneGraphAnnotationEventBase
{
	GENERATED_BODY()

	FRoadControlAnnotationEvent() = default;

	FRoadControlAnnotationEvent(bool bIsEnabled, TArray<FZoneGraphLaneHandle> InLanes, FZoneGraphTagMask InTagsToAdd) : bEnabled(bIsEnabled), Lanes(InLanes), TagsToAdd(InTagsToAdd) {};

	UPROPERTY(Category = "AnnotationEvent", EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(Category = "AnnotationEvent", EditAnywhere)
	TArray<FZoneGraphLaneHandle> Lanes;

	UPROPERTY(Category = "AnnotationEvent", EditAnywhere)
	FZoneGraphTagMask TagsToAdd = FZoneGraphTagMask::None;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API URoadControlAnnotationsComponent : public UZoneGraphAnnotationComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URoadControlAnnotationsComponent();

protected:

	virtual void HandleEvents(const FInstancedStructContainer& Events) override;

public:
	virtual void TickAnnotation(const float DeltaTime, FZoneGraphAnnotationTagContainer& AnnotationTagContainer) override;
	TArray<FRoadControlAnnotationEvent> StateChangeEvents;
	TArray<FZoneGraphLaneHandle> AffectedLanes;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy) override;
#endif
	
};
