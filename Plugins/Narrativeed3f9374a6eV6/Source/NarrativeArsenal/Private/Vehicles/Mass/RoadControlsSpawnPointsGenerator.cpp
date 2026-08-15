// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/RoadControlsSpawnPointsGenerator.h"

#include "MassCommonUtils.h"
#include "MassGameplaySettings.h"
#include "MassSpawnLocationProcessor.h"
#include "ZoneGraphSubsystem.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Vehicles/Mass/MassVehicle.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "Vehicles/Mass/VehicleFragments.h"
#include "VisualLogger/VisualLogger.h"
#include "Engine/World.h"

void URoadControlsSpawnPointsGenerator::Generate(UObject& QueryOwner,
                                                 TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count,
                                                 FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	// @note Since GeneratePointsForZoneGraphData is not virtual, we have to override this function to get our requested functionality
	// If this changes in the future, we should consider updating this logic
	
	if (Count <= 0)
	{
		FinishedGeneratingSpawnPointsDelegate.Execute(TArray<FMassEntitySpawnDataGeneratorResult>());
		return;
	}
	
	const UZoneGraphSubsystem* ZoneGraph = UWorld::GetSubsystem<UZoneGraphSubsystem>(QueryOwner.GetWorld());
	if (ZoneGraph == nullptr)
	{
#if ENABLE_VISUAL_LOG
		UE_VLOG_UELOG(&QueryOwner, LogMassVehicles, Error, TEXT("No zone graph subsystem found in world"));
#endif 
		return;
	}

	TArray<FVector> Locations;
	
	const FRandomStream RandomStream(UE::Mass::Utils::OverrideRandomSeedForTesting(GetRandomSelectionSeed()));
	const TConstArrayView<FRegisteredZoneGraphData> RegisteredZoneGraphs = ZoneGraph->GetRegisteredZoneGraphData();
	if (RegisteredZoneGraphs.IsEmpty())
	{
#if ENABLE_VISUAL_LOG
		UE_VLOG_UELOG(&QueryOwner, LogMassVehicles, Error, TEXT("No zone graphs found"));
#endif 
		return;
	}

	for (const FRegisteredZoneGraphData& Registered : RegisteredZoneGraphs)
	{
		if (Registered.bInUse && Registered.ZoneGraphData)
		{
			GeneratePointsForZoneGraphData(*Registered.ZoneGraphData, Locations, RandomStream);

			// Filter locations based on whether they are within bounds
			if (auto RoadControls = Cast<AQuestRoadControls>(UGameplayStatics::GetActorOfClass(&QueryOwner, AQuestRoadControls::StaticClass())))
			{
				if (RoadControls->IsActive())
				{
					TArray<UBoxComponent*> BoxComponents;
					RoadControls->GetComponents<UBoxComponent>(BoxComponents);

					// Filter locations based on whether they are within the box areas defined in QuestRoadControls
					Locations = Locations.FilterByPredicate([&BoxComponents, this](const FVector& InLocation)
					{
						bool bIsInBox = false;
						for (UBoxComponent* BoxComponent : BoxComponents)
						{
							if (UKismetMathLibrary::IsPointInBoxWithTransform(InLocation, BoxComponent->GetComponentTransform(), BoxComponent->GetUnscaledBoxExtent()))
							{
								bIsInBox = true;
								break;
							}
						}
						return bIsInBox;
					});
				}
			}
		}
	}

	if (Locations.IsEmpty())
	{
#if ENABLE_VISUAL_LOG
		UE_VLOG_UELOG(&QueryOwner, LogMassVehicles, Error, TEXT("No locations found on zone graphs"));
#endif 
		return;
	}

	// Randomize them
	for (int32 I = 0; I < Locations.Num(); ++I)
	{
		const int32 J = RandomStream.RandHelper(Locations.Num());
		Locations.Swap(I, J);
	}

	// If we generated too many, shrink it.
	if (Locations.Num() > Count)
	{
		Locations.SetNum(Count);
	}

	// If there are not enough spawn points, clamp and log error
	if (Count > Locations.Num())
	{
		UE_LOG(LogMassVehicle, Error, TEXT("%hs: Unable to generate enough points for requested entities! Clamping entities spawned from %d to %d"), __FUNCTION__, Count, Locations.Num());
		Count = FMath::Min(Locations.Num(), Count);
	}

	// Build array of entity types to spawn.
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(Count, EntityTypes, Results);

	const int32 LocationCount = Locations.Num();
	int32 LocationIndex = 0;

	// Distribute points amongst the entities to spawn.
	for (FMassEntitySpawnDataGeneratorResult& Result : Results)
	{
		// @todo: Make separate processors and pass the ZoneGraph locations directly.
		Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
		Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
		FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

		Transforms.Transforms.Reserve(Result.NumEntities);
		for (int i = 0; i < Result.NumEntities; i++)
		{
			FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
			Transform.SetLocation(Locations[LocationIndex % LocationCount]);
			LocationIndex++;
		}
	}

#if ENABLE_VISUAL_LOG
	UE_VLOG(this, LogMassVehicles, Log, TEXT("Spawning at %d locations"), LocationIndex);
	if (GetDefault<UMassGameplaySettings>()->bLogSpawnLocations)
	{
		FVisualLogger::Get().ExecuteOnLastEntryForObject(this, [LocationIndex, &Results](FVisualLogEntry& LogEntry)
		{
			FVisualLogShapeElement Element(TEXT(""), FColor::Orange, /*Thickness*/20, LogMassVehicles.GetCategoryName());

			Element.Points.Reserve(LocationIndex);
			for (const FMassEntitySpawnDataGeneratorResult& Result : Results)
			{
				const FMassTransformsSpawnData& Transforms = Result.SpawnData.Get<FMassTransformsSpawnData>();
				for (int i = 0; i < Result.NumEntities; i++)
				{
					Element.Points.Add(Transforms.Transforms[i].GetLocation());
				}
			}
			
			Element.Type = EVisualLoggerShapeElement::SinglePoint;
			Element.Verbosity = ELogVerbosity::Display;
			LogEntry.AddElement(Element);
		});
	}
#endif // ENABLE_VISUAL_LOG

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}
