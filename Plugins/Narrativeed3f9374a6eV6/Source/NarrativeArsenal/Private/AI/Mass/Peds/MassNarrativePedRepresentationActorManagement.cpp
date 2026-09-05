// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"

#include "ArsenalStatics.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassAgentComponent.h"
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
	
	// Always enqueue: the actor's present state does not include earlier queued commands.
	// A disable followed by enable before a flush must leave the actor enabled.
	const TWeakObjectPtr<AActor> WeakActor = &Actor;
	const TWeakObjectPtr<UMassAgentComponent> Agent = Actor.FindComponentByClass<UMassAgentComponent>();
	const FMassEntityHandle ExpectedEntity = Agent.IsValid() ? Agent->GetEntityHandle() : FMassEntityHandle();
	CommandBuffer.PushCommand<FMassDeferredSetCommand>([WeakActor, Agent, ExpectedEntity, bEnabled](FMassEntityManager&)
	{
		const auto StillAssociated = [WeakActor, Agent, ExpectedEntity]()
		{
			AActor* CurrentActor = WeakActor.Get();
			if (!IsValid(CurrentActor) || CurrentActor->IsActorBeingDestroyed() || Agent.IsStale()) { return false; }
			UMassAgentComponent* CurrentAgent = CurrentActor->FindComponentByClass<UMassAgentComponent>();
			return CurrentAgent == Agent.Get() && (!CurrentAgent || CurrentAgent->GetEntityHandle() == ExpectedEntity);
		};
		if (!StillAssociated()) { return; }
		AActor* LiveActor = WeakActor.Get();
		TArray<AActor*> ActorsToChange;
		LiveActor->GetAllChildActors(ActorsToChange);
		ActorsToChange.Emplace(LiveActor);
		for (AActor* ModifiedActor : ActorsToChange)
		{
			if (!StillAssociated()) { return; }
			if (!IsValid(ModifiedActor) || ModifiedActor->IsActorBeingDestroyed()) { continue; }
			if (ModifiedActor->GetActorEnableCollision() != bEnabled) { ModifiedActor->SetActorEnableCollision(bEnabled); }
			if (!StillAssociated()) { return; }
			if (!IsValid(ModifiedActor) || ModifiedActor->IsActorBeingDestroyed()) { continue; }
			if (ModifiedActor->IsHidden() == bEnabled) { ModifiedActor->SetActorHiddenInGame(!bEnabled); }
			if (!IsValid(ModifiedActor) || ModifiedActor->IsActorBeingDestroyed()) { continue; }
			if (ModifiedActor->IsActorTickEnabled() != bEnabled) { ModifiedActor->SetActorTickEnabled(bEnabled); }
		}
	});
}
