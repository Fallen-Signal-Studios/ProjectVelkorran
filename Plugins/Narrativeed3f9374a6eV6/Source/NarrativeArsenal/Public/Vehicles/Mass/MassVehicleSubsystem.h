// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "HierarchicalHashGrid2D.h"
#include "MassEntityTypes.h"
#include "MassSubsystemBase.h"
#include "ZoneGraphTypes.h"
#include "MassExternalSubsystemTraits.h"
#include "MassEntityHandle.h"
#include "MassVehicleSubsystem.generated.h"

typedef THierarchicalHashGrid2D<2, 4, FMassEntityHandle> FVehicleObstacleHashGrid;

USTRUCT()
struct FVehicleLane
{
	GENERATED_BODY()

	FVehicleLane() = default;
	
	TArray<FMassEntityHandle> VehiclesInLane;

	int IncomingVehicles = 0;
};

/**
 * Handles general management of vehicles within the zonegraph
 */
UCLASS()
class NARRATIVEARSENAL_API UMassVehicleSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()

public:
	FVehicleObstacleHashGrid VehicleObstacles;

	void VehicleEnterLane(const FZoneGraphLaneHandle& Lane, const FMassEntityHandle& Vehicle);
	void VehicleLeaveLane(const FZoneGraphLaneHandle& Lane, const FMassEntityHandle& Vehicle);
	FMassEntityHandle GetTailVehicle(const FZoneGraphLaneHandle& Lane) const;
	float GetAvailableSpace(const FZoneGraphLaneHandle& Lane, bool bIncludeIncomingVehicles) const;
	void ClaimLaneSpot(const FZoneGraphLaneHandle& Lane);
	void UnclaimLaneSpot(const FZoneGraphLaneHandle& Lane);
	
	void DebugLane(const FZoneGraphLaneHandle& Lane);

protected:
	TMap<FZoneGraphLaneHandle, FVehicleLane> VehiclesInLanes;
};

template<>
struct TMassExternalSubsystemTraits<UMassVehicleSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};

