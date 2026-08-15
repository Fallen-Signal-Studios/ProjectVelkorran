// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/QuestRoadControls.h"

#include "ZoneGraphAnnotationSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicles/Mass/MassVehicleSpawner.h"
#include "Vehicles/Mass/RoadControlAnnotationsComponent.h"
#include "SaveSystemStatics.h"
#include "Vehicles/Mass/TrafficLightSubsystem.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuestRoadControls, Log, All);

// Sets default values
AQuestRoadControls::AQuestRoadControls()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scene Root"));
	SetRootComponent(SceneRoot);
	
	RoadControlAnnotationComponent = CreateDefaultSubobject<URoadControlAnnotationsComponent>(TEXT("Road Control Annotations"));

	USaveSystemStatics::CreateSaveGuid(RoadControlsSaveGUID);
}

FGuid AQuestRoadControls::GetActorGUID_Implementation() const
{
	return RoadControlsSaveGUID;
}

void AQuestRoadControls::SetActorGUID_Implementation(const FGuid& GUID)
{
	RoadControlsSaveGUID = GUID; 
}

bool AQuestRoadControls::IsActive() const
{
	return bIsActive;
}

void AQuestRoadControls::SetActive(bool bNewActive)
{
	// if the value is the same, we do nothing
	if (bNewActive == bIsActive) { return; }
	
	bIsActive = bNewActive;

	UZoneGraphSubsystem* ZoneGraph = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());
	UZoneGraphAnnotationSubsystem* ZoneGraphAnnotations = UWorld::GetSubsystem<UZoneGraphAnnotationSubsystem>(GetWorld());
	UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());

	check(ZoneGraph);
	check(ZoneGraphAnnotations);
	check(TrafficLightSubsystem);
		
	VehicleSpawner = Cast<AMassVehicleSpawner>(UGameplayStatics::GetActorOfClass(this, AMassVehicleSpawner::StaticClass()));
	if (VehicleSpawner)
	{
		// If we have turned active, cache the old spawn count
		if (bIsActive)
		{
			OldSpawnCount = VehicleSpawner->GetCount();
		}
		
		VehicleSpawner->SetSpawnCount(bIsActive ? NewSpawnCount : OldSpawnCount);
		VehicleSpawner->DoDespawning();
		VehicleSpawner->DoSpawning();
	}

	// Override appropriate intersections
	TArray<FTrafficIntersectionSideHandle> OverriddenHandles;
	OverriddenHandles.Reserve(IntersectionSideOverrides.Num());
	
	for (int i=0;i<IntersectionSideOverrides.Num();i++)
	{
		const FIntersectionSideOverride& IntersectionSideOverride = IntersectionSideOverrides[i];
		TArray<FTrafficIntersectionSideHandle> IntersectionSideHandles;
		FVector WorldSideOverrideLocation = GetActorTransform().TransformPosition(IntersectionSideOverride.IntersectionSideLocation);
		FBox QueryExtent = FBox::BuildAABB(WorldSideOverrideLocation, IntersectionSideQueryExtent);
		
		TrafficLightSubsystem->IntersectionSidesGrid.Query(QueryExtent, IntersectionSideHandles);
		if (IntersectionSideHandles.IsEmpty())
		{
#if ENABLE_VISUAL_LOG
			UE_VLOG_LOCATION(this, LogQuestRoadControls, Verbose, WorldSideOverrideLocation, 10.f, FColor::Red, TEXT("Side Query Location"));
			UE_VLOG_UELOG(this, LogQuestRoadControls, Error, TEXT("No valid intersection side found!"))
#endif 

			continue;
		}

		IntersectionSideHandles.Sort([&TrafficLightSubsystem, &WorldSideOverrideLocation](const FTrafficIntersectionSideHandle& Side1, const FTrafficIntersectionSideHandle& Side2)
		{
			const FVector& Side1Location = Side1.GetIntersectionSide(TrafficLightSubsystem).SideLocation;
			const FVector& Side2Location = Side2.GetIntersectionSide(TrafficLightSubsystem).SideLocation;

			return FVector::DistSquared(Side1Location, WorldSideOverrideLocation) < FVector::DistSquared(Side2Location, WorldSideOverrideLocation);
		});

		// We will assume index 0 is the closest intersection side
		FTrafficIntersectionSideHandle& IntersectionSideHandle = IntersectionSideHandles[0];
		IntersectionSideHandle.GetIntersection(TrafficLightSubsystem).SetOverrideIntersection(bNewActive, GetWorld());
		
		OverriddenHandles.Emplace(IntersectionSideHandle);
	}

	// Apply intersection override settings if we are active
	if (bNewActive)
	{
		for (int i=0;i<OverriddenHandles.Num();i++)
		{
			// Indexes will match up
			FTrafficIntersectionSideHandle& OverriddenHandle = OverriddenHandles[i];
			FTrafficIntersectionSide& IntersectionSide = OverriddenHandle.GetMutableIntersectionSide(TrafficLightSubsystem);
			FIntersectionSideOverride& IntersectionSideOverride = IntersectionSideOverrides[i];

			const FZoneGraphStorage* Storage = ZoneGraph->GetZoneGraphStorage(OverriddenHandle.ZoneGraphDataHandle);
			
			// Something probably went wrong - such as not being able to find an intersection side
			if (!OverriddenHandle.IsValid()) { continue; }
			
			const uint32 S = OverriddenHandle.SideIndex;
			const uint32 SLeft = (S + 1) % 4;
			const uint32 SOpposite = (S + 2) % 4;
			const uint32 SRight = (S + 3) % 4;

			TArray<FZoneGraphLaneHandle> Lanes;

			// Fetch lanes that we want to override
			if (EnumHasAnyFlags((EIntersectionSideRule)IntersectionSideOverride.Rule, EIntersectionSideRule::LeftOpen))
			{
				OverriddenHandle.GetIntersection(TrafficLightSubsystem).GetSidesConnectingLanes(S, SLeft, *Storage, Lanes);
			}
			if (EnumHasAnyFlags((EIntersectionSideRule)IntersectionSideOverride.Rule, EIntersectionSideRule::RightOpen))
			{
				OverriddenHandle.GetIntersection(TrafficLightSubsystem).GetSidesConnectingLanes(S, SRight, *Storage, Lanes);
			}
			if (EnumHasAnyFlags((EIntersectionSideRule)IntersectionSideOverride.Rule, EIntersectionSideRule::StraightOpen))
			{
				OverriddenHandle.GetIntersection(TrafficLightSubsystem).GetSidesConnectingLanes(S, SOpposite, *Storage, Lanes);
			}

			if (Lanes.IsEmpty())
			{
				UE_LOG(LogQuestRoadControls, Warning, TEXT("No lanes found for intersection side at %s. Unable to override intersection side."), *OverriddenHandle.GetIntersectionSide(TrafficLightSubsystem).SideLocation.ToCompactString());
				continue;
			}
#if ENABLE_VISUAL_LOG
			UE_VLOG_LOCATION(this, LogQuestRoadControls, Verbose, OverriddenHandle.GetIntersectionSide(TrafficLightSubsystem).SideLocation, 10.f, FColor::Red, TEXT("Overriden Side"));
#endif 
			IntersectionSide.SideOverride = IntersectionSideOverride.Rule;
			
			FTrafficPeriod Period;
			Period.Lanes = Lanes;
			Period.LanesCoveredMask = (EIntersectionSideRule)IntersectionSide.SideOverride;
			
			FTrafficPeriodEvent PeriodEvent = FTrafficPeriodEvent();
			PeriodEvent.Period = Period;
			PeriodEvent.State = ELaneState::Open;
				
			ZoneGraphAnnotations->SendEvent(PeriodEvent);
		}
	}
	else
	{
		for (const FTrafficIntersectionSideHandle& OverriddenHandle : OverriddenHandles)
		{
			OverriddenHandle.GetIntersection(TrafficLightSubsystem).SetOverrideIntersection(false, GetWorld());
		}
	}
	
}

// Called when the game starts or when spawned
void AQuestRoadControls::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoActivate)
	{
		SetActive(true);
	}

	// Disable collision for all box components added to this actor
	TArray<UBoxComponent*> BoxComponents;
	GetComponents(BoxComponents);

	for (UBoxComponent* BoxComponent : BoxComponents)
	{
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Since we no longer want to tag certain lanes for controlling traffic, this will be commented out. But this may still be useful in the future
	/*
	// Cache all lanes within bounding boxes
	TArray<UBoxComponent*> BoxComponents;
	GetComponents<UBoxComponent>(BoxComponents);
	
	for (const UBoxComponent* Component : BoxComponents)
	{
		auto Bounds = Component->CalcBounds(Component->GetComponentTransform());

		TArray<FZoneGraphLaneHandle> Lanes;
		FBox Box = FBox::BuildAABB(Bounds.Origin, Bounds.BoxExtent);
		ZoneGraph->FindOverlappingLanes(Box, FZoneGraphTagFilter(AnyTags, AllTags, NotTags), Lanes);
		UE_VLOG_BOX(this, LogQuestRoadControls, Log, Box, FColor::Red, TEXT("Lane Zone"));

		CachedLanes.Append(Lanes);
	}
	
	FRoadControlAnnotationEvent RoadEvent(true, CachedLanes, TagsToAdd);
	ZoneGraphAnnotations->SendEvent(RoadEvent);
	*/
}

void AQuestRoadControls::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// We need to make sure that the spawner does not use this quest road controls bounds
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (VehicleSpawner)
		{
			SetActive(false);
		}
	}

	Super::EndPlay(EndPlayReason);

	/*
	FRoadControlAnnotationEvent RoadEvent(false, CachedLanes, TagsToAdd);
	ZoneGraphAnnotations->SendEvent(RoadEvent);
	*/
}

