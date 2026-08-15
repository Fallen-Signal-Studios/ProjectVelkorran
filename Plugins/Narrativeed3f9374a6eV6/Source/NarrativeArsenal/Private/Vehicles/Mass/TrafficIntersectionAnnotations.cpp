// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/TrafficIntersectionAnnotations.h"

#include "ZoneGraphRenderingUtilities.h"
#include "Vehicles/Mass/TrafficLightSettings.h"
#include "Vehicles/Mass/TrafficLightSubsystem.h"


// Sets default values for this component's properties
UTrafficIntersectionAnnotations::UTrafficIntersectionAnnotations()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UTrafficIntersectionAnnotations::HandleEvents(const FInstancedStructContainer& Events)
{
	for (FConstStructView Event : Events)
	{
		if (const FTrafficPeriodEvent* StateChangeEvent = Event.GetPtr<const FTrafficPeriodEvent>())
		{
			PeriodEvents.Add(*StateChangeEvent);
		}
	}
}

void UTrafficIntersectionAnnotations::TickAnnotation(const float DeltaTime,
	FZoneGraphAnnotationTagContainer& AnnotationTagContainer)
{
	if (!CloseLaneTag.IsValid())
	{
		return;
	}

	// Process events
	for (const FTrafficPeriodEvent& PeriodEvent : PeriodEvents) 
	{
		for (const FZoneGraphLaneHandle& Lane : PeriodEvent.Period.Lanes)
		{
			TArrayView<FZoneGraphTagMask> LaneTags = AnnotationTagContainer.GetMutableAnnotationTagsForData(Lane.DataHandle);
			FZoneGraphTagMask& LaneTagMask = LaneTags[Lane.Index];

			switch (PeriodEvent.State)
			{
			case ELaneState::Closed:
				LaneTagMask.Add(CloseLaneTag);
				break;
			case ELaneState::Open:
				LaneTagMask.Remove(CloseLaneTag);
				break;
			default:
				break;
			}
		}
	}
	
	PeriodEvents.Reset();

#if UE_ENABLE_DEBUG_DRAWING
	if (bEnableDebugDrawing)
	{
		MarkRenderStateDirty();
	}
#endif // UE_ENABLE_DEBUG_DRAWING
}

void UTrafficIntersectionAnnotations::PostSubsystemsInitialized()
{
	Super::PostSubsystemsInitialized();

	TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());
}

#if UE_ENABLE_DEBUG_DRAWING
void UTrafficIntersectionAnnotations::DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy)
{
	UZoneGraphSubsystem* ZoneGraph = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());
	UZoneGraphAnnotationSubsystem* ZoneGraphAnnotationSubsystem = UWorld::GetSubsystem<UZoneGraphAnnotationSubsystem>(GetWorld());
	const UTrafficLightSettings* TrafficSettings = GetDefault<UTrafficLightSettings>();

	static const FVector ZOffset(0, 0, 35.0f);
	static const FLinearColor ClosedColor(FColor(255, 61, 0));
	static const FLinearColor OpenColor(FColor(61, 255, 0));

	for (const FTrafficLightData& RegisteredLaneData : TrafficLightSubsystem->RegisteredLaneData)
	{
		const FZoneGraphStorage* ZoneStorage = ZoneGraph->GetZoneGraphStorage(RegisteredLaneData.DataHandle);
		if (ZoneStorage == nullptr)
		{
			continue;
		}

		for (const FTrafficLightIntersection& Intersection : RegisteredLaneData.Intersections)
		{
			for (const FZoneGraphLaneHandle& Lane : Intersection.GetLanes())
			{
				auto Tags = ZoneGraphAnnotationSubsystem->GetAnnotationTags(Lane);

				if (Tags.Contains(CloseLaneTag))
				{
					UE::ZoneGraph::RenderingUtilities::AppendLane(DebugProxy, *ZoneStorage, Lane, ClosedColor.ToFColor(/*sRGB*/true), 4.0f, ZOffset);
				}
				else if (Tags.Contains(TrafficSettings->IntersectionTag))
				{
					// Only display open lane on intersection (current use case)
					UE::ZoneGraph::RenderingUtilities::AppendLane(DebugProxy, *ZoneStorage, Lane, OpenColor.ToFColor(/*sRGB*/true), 4.0f, ZOffset);
				}
			}
		}
	}
}
#endif
