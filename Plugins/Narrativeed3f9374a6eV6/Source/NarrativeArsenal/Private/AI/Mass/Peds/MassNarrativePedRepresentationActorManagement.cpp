// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"

#include "ArsenalStatics.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassAgentComponent.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "Character/NarrativeCharacterVisual.h"
#include "StructUtils/StructView.h"

EMassActorSpawnRequestAction UMassNarrativePedRepresentationActorManagement::OnPostActorSpawn(
	const FMassActorSpawnRequestHandle& SpawnRequestHandle, FConstStructView SpawnRequest,
	TSharedRef<FMassEntityManager> EntityManager) const
{
	const FMassActorSpawnRequest& MassActorSpawnRequest = SpawnRequest.Get<const FMassActorSpawnRequest>();
	if (!EntityManager->IsEntityValid(MassActorSpawnRequest.MassAgent))
	{
		return EMassActorSpawnRequestAction::Remove;
	}
	const bool bParticipantRepresentation = EntityManager->GetFragmentDataPtr<FNarrativeMassParticipantFragment>(MassActorSpawnRequest.MassAgent) != nullptr;
	auto RequestAction = Super::OnPostActorSpawn(SpawnRequestHandle, SpawnRequest, EntityManager);

	// This representation family also supports project-owned, non-NPC proxies.
	// Dispatch before either checked pedestrian fragment access or NPC initialization.
	if (bParticipantRepresentation)
	{
		AActor* Actor = MassActorSpawnRequest.SpawnedActor;
		if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
		{
			auto* Receipt = Actor->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>();
			if (!Receipt)
			{
				Receipt = NewObject<UNarrativeMassParticipantReceiptComponent>(Actor);
				Actor->AddInstanceComponent(Receipt);
				Receipt->RegisterComponent();
			}
			if (!Receipt->Bind(EntityManager, MassActorSpawnRequest.MassAgent)
				&& IsValid(Actor) && !Actor->IsActorBeingDestroyed() && !Receipt->IsCurrent(EntityManager.Get()))
			{
				Actor->SetActorEnableCollision(false);
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorTickEnabled(false);
			}
		}
		return RequestAction;
	}
	
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
	const TWeakObjectPtr<UNarrativeMassParticipantReceiptComponent> Receipt = Actor.FindComponentByClass<UNarrativeMassParticipantReceiptComponent>();
	if (Receipt.IsValid() && Receipt->GetEntity().Index != EntityIdx) { return; }
	const uint64 ExpectedReceiptEpoch = Receipt.IsValid() ? Receipt->GetBindingEpoch() : 0;
	const bool bPresentationOnly = Receipt.IsValid() && Receipt->GetParticipant().bPresentationOnly;
	const bool bCollisionAndTickEnabled = bEnabled && !bPresentationOnly;
	CommandBuffer.PushCommand<FMassDeferredSetCommand>([WeakActor, Agent, ExpectedEntity, Receipt,
		ExpectedReceiptEpoch, bEnabled, bCollisionAndTickEnabled](FMassEntityManager& EntityManager)
	{
		const auto StillAssociated = [WeakActor, Agent, ExpectedEntity, Receipt, ExpectedReceiptEpoch, &EntityManager]()
		{
			AActor* CurrentActor = WeakActor.Get();
			if (!IsValid(CurrentActor) || CurrentActor->IsActorBeingDestroyed() || Agent.IsStale() || Receipt.IsStale()) { return false; }
			auto* CurrentReceipt = CurrentActor->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>();
			if (CurrentReceipt != Receipt.Get()
				|| (CurrentReceipt && !CurrentReceipt->MatchesReceipt(EntityManager, ExpectedReceiptEpoch))) { return false; }
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
			if (ModifiedActor->GetActorEnableCollision() != bCollisionAndTickEnabled) { ModifiedActor->SetActorEnableCollision(bCollisionAndTickEnabled); }
			if (!StillAssociated()) { return; }
			if (!IsValid(ModifiedActor) || ModifiedActor->IsActorBeingDestroyed()) { continue; }
			if (ModifiedActor->IsHidden() == bEnabled) { ModifiedActor->SetActorHiddenInGame(!bEnabled); }
			if (!StillAssociated()) { return; }
			if (!IsValid(ModifiedActor) || ModifiedActor->IsActorBeingDestroyed()) { continue; }
			if (ModifiedActor->IsActorTickEnabled() != bCollisionAndTickEnabled) { ModifiedActor->SetActorTickEnabled(bCollisionAndTickEnabled); }
		}
	});
}
