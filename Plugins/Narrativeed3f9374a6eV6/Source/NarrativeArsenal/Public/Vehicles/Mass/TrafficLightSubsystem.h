// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassSubsystemBase.h"
#include "TrafficLightIntersectionData.h"
#include "ZoneGraphAnnotationSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "HierarchicalHashGrid2D.h"
#include "MassVehicleSubsystem.h"
#include "TrafficLightSubsystem.generated.h"

class ATrafficLight;
typedef THierarchicalHashGrid2D<2, 2, FTrafficIntersectionSideHandle> FIntersectionSideHashGrid;

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficLight, Log, All);

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UTrafficLightSubsystem : public UMassTickableSubsystemBase
{
	GENERATED_BODY()

private:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	
	void PostZoneGraphDataAdded(const AZoneGraphData* ZoneGraphData);
	void PreZoneGraphDataRemoved(const AZoneGraphData* ZoneGraphData);
	void BuildLaneData(FTrafficLightData& LaneData, const FZoneGraphStorage& Storage);
	void UpdateRegisteredTrafficLights(const FTrafficLightIntersection& UpdatedIntersection);

public:
	void RegisterTrafficLight(ATrafficLight* TrafficLight);
	
protected:
	UPROPERTY(Transient)
	TObjectPtr<UZoneGraphSubsystem> ZoneGraphSubsystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UZoneGraphAnnotationSubsystem> ZoneGraphAnnotationSubsystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMassVehicleSubsystem> VehicleSubsystem = nullptr;
	
	FDelegateHandle OnPostZoneGraphDataAddedHandle;
	FDelegateHandle OnPreZoneGraphDataRemovedHandle;

public:
	TArray<FTrafficLightData> RegisteredLaneData;

	FIntersectionSideHashGrid IntersectionSidesGrid;

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<ATrafficLight>> RegisteredTrafficLights;
};
