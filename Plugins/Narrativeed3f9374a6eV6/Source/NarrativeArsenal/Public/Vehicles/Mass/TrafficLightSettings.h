// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassSettings.h"
#include "ZoneGraphTypes.h"
#include "TrafficLightSettings.generated.h"

/**
 * 
 */
UCLASS(config = Mass, defaultconfig, meta = (DisplayName = "Traffic Light Behavior"))
class NARRATIVEARSENAL_API UTrafficLightSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	// Tag to mark zones as intersections
	UPROPERTY(EditAnywhere, Category = "TrafficLight", config)
	FZoneGraphTag IntersectionTag;

	// Tag to mark lanes as closed/inaccessible
	UPROPERTY(EditAnywhere, Category = "TrafficLight", config)
	FZoneGraphTag ClosedTag;

	// Time in advance to close a lane before swapping periods. This helps ensure that no vehicle tries to pass right before the light changes.
	UPROPERTY(EditAnywhere, Category = "TrafficLight", config)
	float LaneCloseAdvanceTime = 3.f;
};
