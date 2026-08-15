// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassPedRepresentationSubsystem.h"
#include "Engine/World.h"
#include "AI/Mass/Peds/MassPedSpawnerSubsystem.h"

void UMassPedRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMassPedSpawnerSubsystem::StaticClass());

	Super::Initialize(Collection);

	ActorSpawnerSubsystem = UWorld::GetSubsystem<UMassPedSpawnerSubsystem>(GetWorld());
}
