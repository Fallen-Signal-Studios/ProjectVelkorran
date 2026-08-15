// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/RoadControlAnnotationsComponent.h"

#include "ZoneGraphAnnotationSubsystem.h"
#include "ZoneGraphRenderingUtilities.h"
#include "ZoneGraphSubsystem.h"
#include "StructUtils/StructView.h"


// Sets default values for this component's properties
URoadControlAnnotationsComponent::URoadControlAnnotationsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void URoadControlAnnotationsComponent::HandleEvents(const FInstancedStructContainer& Events)
{
	for (FConstStructView Event : Events)
	{
		if (const FRoadControlAnnotationEvent* const StateChangeEvent = Event.GetPtr<const FRoadControlAnnotationEvent>())
		{
			StateChangeEvents.Add(*StateChangeEvent);
		}
	}
}

void URoadControlAnnotationsComponent::TickAnnotation(const float DeltaTime,
	FZoneGraphAnnotationTagContainer& AnnotationTagContainer)
{
	for (const FRoadControlAnnotationEvent& Event : StateChangeEvents)
	{
		if (Event.bEnabled)
		{
			AffectedLanes.Reserve(Event.Lanes.Num());
		}
		
		for (const FZoneGraphLaneHandle& Lane : Event.Lanes)
		{
			TArrayView<FZoneGraphTagMask> LaneTags = AnnotationTagContainer.GetMutableAnnotationTagsForData(Lane.DataHandle);
			FZoneGraphTagMask& LaneTagMask = LaneTags[Lane.Index];
			
			if (Event.bEnabled)
			{
				AffectedLanes.Emplace(Lane);
				LaneTagMask.Add(Event.TagsToAdd);
			}
			else
			{
				AffectedLanes.RemoveSwap(Lane);
				LaneTagMask.Remove(Event.TagsToAdd);
			}
		}
		
	}
	
	StateChangeEvents.Reset();

#if UE_ENABLE_DEBUG_DRAWING
	if (bEnableDebugDrawing)
	{
		MarkRenderStateDirty();
	}
#endif // UE_ENABLE_DEBUG_DRAWING
}

#if UE_ENABLE_DEBUG_DRAWING
void URoadControlAnnotationsComponent::DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy)
{
	UZoneGraphSubsystem* ZoneGraph = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());
	UZoneGraphAnnotationSubsystem* ZoneGraphAnnotationSubsystem = UWorld::GetSubsystem<UZoneGraphAnnotationSubsystem>(GetWorld());

	static const FVector ZOffset(0, 0, 35.0f);
	static const FLinearColor OpenColor(FColor(61, 255, 0));
	
	for (const FZoneGraphLaneHandle& AffectedLane : AffectedLanes) 
	{
		const FZoneGraphStorage* ZoneStorage = ZoneGraph->GetZoneGraphStorage(AffectedLane.DataHandle);
		if (ZoneStorage == nullptr)
		{
			continue;
		}
		
		auto Tags = ZoneGraphAnnotationSubsystem->GetAnnotationTags(AffectedLane);
		UE::ZoneGraph::RenderingUtilities::AppendLane(DebugProxy, *ZoneStorage, AffectedLane, OpenColor.ToFColor(/*sRGB*/true), 4.0f, ZOffset);
	}
}
#endif

