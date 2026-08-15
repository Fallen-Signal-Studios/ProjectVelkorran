// Fill out your copyright notice in the Description page of Project Settings.


#include "Spawners/ActorFactoryNPCDefinition.h"

#include "AI/NPCDefinition.h"
#include "Spawners/NPCSpawner.h"

UActorFactoryNPCDefinition::UActorFactoryNPCDefinition()
{
	DisplayName = FText::FromString("NPC Definition");
	NewActorClass = ANPCSpawner::StaticClass();
}

void UActorFactoryNPCDefinition::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	ANPCSpawner* Spawner = Cast<ANPCSpawner>(NewActor);
	if (!Spawner) { return; }

	// Add spawn component if the spawner doesnt already contain one (post spawn can get called more than once)
	if (!Spawner->FindComponentByClass<UNPCSpawnComponent>())
	{
		UNPCSpawnComponent* SpawnComponent = Spawner->CreateNPCSpawner();
		SpawnComponent->NPCToSpawn = Cast<UNPCDefinition>(Asset);
	}
}

bool UActorFactoryNPCDefinition::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid() && AssetData.IsInstanceOf(UNPCDefinition::StaticClass()))
	{
		return true;
	}
	else
	{
		OutErrorMsg = FText::FromString("Asset is not a NPCDefinition");
		return false;
	}
}
