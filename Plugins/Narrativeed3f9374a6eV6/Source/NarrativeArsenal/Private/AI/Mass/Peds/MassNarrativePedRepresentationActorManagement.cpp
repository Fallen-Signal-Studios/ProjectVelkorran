// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"

#include "ArsenalStatics.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "Character/NarrativeCharacterVisual.h"
#include "StructUtils/StructView.h"

EMassActorSpawnRequestAction UMassNarrativePedRepresentationActorManagement::OnPostActorSpawn(
	const FMassActorSpawnRequestHandle& SpawnRequestHandle, FConstStructView SpawnRequest,
	TSharedRef<FMassEntityManager> EntityManager) const
{
	const FMassActorSpawnRequest& MassActorSpawnRequest = SpawnRequest.Get<const FMassActorSpawnRequest>();
	auto RequestAction = Super::OnPostActorSpawn(SpawnRequestHandle, SpawnRequest, EntityManager);
	
	if (MassActorSpawnRequest.SpawnedActor)
	{
		auto& PedProperties = EntityManager->GetConstSharedFragmentDataChecked<FNarrativePedProperties>(MassActorSpawnRequest.MassAgent);
		auto& PedFragment = EntityManager->GetFragmentDataChecked<FNarrativePedFragment>(MassActorSpawnRequest.MassAgent);

		auto& NPCDefinition = PedProperties.NarrativePeds[PedFragment.NarrativePedIndex];

		if (auto NPCCharacter = Cast<ANarrativeNPCCharacter>(MassActorSpawnRequest.SpawnedActor))
		{
			NPCCharacter->SetRandomSeed(PedFragment.NarrativePedSeed);
			NPCCharacter->SetNPCDefinition(NPCDefinition.LoadSynchronous());
		}
	}
	return RequestAction;
}

void UMassNarrativePedRepresentationActorManagement::SetActorEnabled(const EMassActorEnabledType EnabledType,
	AActor& Actor, const int32 EntityIdx, FMassCommandBuffer& CommandBuffer) const
{
	const bool bEnabled = EnabledType != EMassActorEnabledType::Disabled;
	
	if (Actor.GetActorEnableCollision() != bEnabled)
	{
		// Deferring this as there is a callback internally that could end up doing things outside of the game thread and will fire checks(Chaos mostly)
		CommandBuffer.PushCommand<FMassDeferredSetCommand>([&Actor, bEnabled](FMassEntityManager&)
		{
			TArray<AActor*> ActorsToChange;
			Actor.GetAllChildActors(ActorsToChange);
			ActorsToChange.Emplace(&Actor);
			
			for (AActor* ModifiedActor : ActorsToChange)
			{
				ModifiedActor->SetActorEnableCollision(bEnabled);
				ModifiedActor->SetActorHiddenInGame(!bEnabled);
				ModifiedActor->SetActorTickEnabled(bEnabled);
			}
		});
	}
}
