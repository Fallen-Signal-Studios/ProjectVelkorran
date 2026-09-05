// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassPedSpawnerSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "Engine/World.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "StructUtils/StructView.h"

ESpawnRequestStatus UMassPedSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView,
                                                         TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const
{
	UWorld* World = GetWorld();
	check(World);

	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<const FMassActorSpawnRequest>();
	FMassEntityManager* EntityManager = UE::Mass::Utils::GetEntityManager(World);
	if (!EntityManager || !EntityManager->IsEntityValid(SpawnRequest.MassAgent))
	{
		return ESpawnRequestStatus::Failed;
	}
	if (const auto* Participant = EntityManager->GetFragmentDataPtr<FNarrativeMassParticipantFragment>(SpawnRequest.MassAgent))
	{
		UObject* Owner = Participant->Owner.Get();
		const AActor* OwnerActor = Cast<AActor>(Owner);
		if (!Participant->HasValidIdentity() || !Owner || Owner->GetWorld() != World
			|| (OwnerActor && OwnerActor->IsActorBeingDestroyed())
			|| !Cast<INarrativeMassParticipantOwner>(Owner))
		{
			return ESpawnRequestStatus::Failed;
		}
		// Project-owned actors use the configured plain actor template. Never resolve a
		// GUID-name alias or call SpawnNPC for a participant representation.
		InOutSpawnParameters.Name = NAME_None;
		InOutSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FMassActorSpawnRequest ParticipantRequest = SpawnRequest;
		ParticipantRequest.Guid.Invalidate(); // The fragment owns stable identity, never the actor name.
		return Super::SpawnActor(FConstStructView::Make(ParticipantRequest), OutSpawnedActor, InOutSpawnParameters);
	}

	if (SpawnRequest.Guid.IsValid())
	{
		// offsetting `D` by 1 since `0` has special meaning for FNames
		InOutSpawnParameters.Name = FName(FString::Printf(TEXT("%s_%ud_%ud_%ud"), *SpawnRequest.Template->GetName(), SpawnRequest.Guid.A, SpawnRequest.Guid.B, SpawnRequest.Guid.C), SpawnRequest.Guid.D + 1);
		//InOutSpawnParameters.OverrideLevel = InOutSpawnParameters.OverrideLevel ? OverrideLevel : World->GetCurrentLevel();

		OutSpawnedActor = FindActorByName(InOutSpawnParameters.Name, InOutSpawnParameters.OverrideLevel ? InOutSpawnParameters.OverrideLevel : World->GetCurrentLevel());
		if (OutSpawnedActor)
		{
			OutSpawnedActor->SetActorEnableCollision(true);
			OutSpawnedActor->SetActorHiddenInGame(false);
			return ESpawnRequestStatus::Succeeded;
		}
	}
	
	InOutSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UNarrativeCharacterSubsystem* CharacterSubsystem = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	check(CharacterSubsystem)
	
	auto& PedFragment = EntityManager->GetFragmentDataChecked<FNarrativePedFragment>(SpawnRequest.MassAgent);
	auto& PedProperties = EntityManager->GetConstSharedFragmentDataChecked<FNarrativePedProperties>(SpawnRequest.MassAgent);
	
	OutSpawnedActor = CharacterSubsystem->SpawnNPC(PedProperties.NarrativePeds[PedFragment.NarrativePedIndex].LoadSynchronous(), SpawnRequest.Transform);

	return IsValid(OutSpawnedActor) ? ESpawnRequestStatus::Succeeded : ESpawnRequestStatus::Failed;
}
