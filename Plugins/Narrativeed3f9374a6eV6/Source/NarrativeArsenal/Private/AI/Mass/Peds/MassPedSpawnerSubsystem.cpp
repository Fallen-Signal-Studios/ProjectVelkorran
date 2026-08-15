// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassPedSpawnerSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "Engine/World.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"

ESpawnRequestStatus UMassPedSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView,
                                                         TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const
{
	UWorld* World = GetWorld();
	check(World);

	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<const FMassActorSpawnRequest>();

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
	
	FMassEntityManager* EntityManager = UE::Mass::Utils::GetEntityManager(World);
	auto& PedFragment = EntityManager->GetFragmentDataChecked<FNarrativePedFragment>(SpawnRequest.MassAgent);
	auto& PedProperties = EntityManager->GetConstSharedFragmentDataChecked<FNarrativePedProperties>(SpawnRequest.MassAgent);
	
	OutSpawnedActor = CharacterSubsystem->SpawnNPC(PedProperties.NarrativePeds[PedFragment.NarrativePedIndex].LoadSynchronous(), SpawnRequest.Transform);

	return IsValid(OutSpawnedActor) ? ESpawnRequestStatus::Succeeded : ESpawnRequestStatus::Failed;
}
