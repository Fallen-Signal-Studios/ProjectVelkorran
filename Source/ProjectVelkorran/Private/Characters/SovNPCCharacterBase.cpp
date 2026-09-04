// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovNPCCharacterBase.h"

#include "Components/SovCombatSustainDropComponent.h"
#include "Components/SovDismembermentComponent.h"
#include "Misc/SecureHash.h"
#include "AI/NPCDefinition.h"
#include "Engine/World.h"

ASovNPCCharacterBase::ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DismembermentComponent = CreateDefaultSubobject<USovDismembermentComponent>(
		TEXT("SovDismembermentComponent"));
	CombatSustainDropComponent = CreateDefaultSubobject<USovCombatSustainDropComponent>(
		TEXT("SovCombatSustainDropComponent"));
}

FGuid ASovNPCCharacterBase::GetActorGUID_Implementation() const
{
	if (SpawnInfo.SpawnAssignedSaveGUID.IsValid()) { return SpawnInfo.SpawnAssignedSaveGUID; }
	if (NativeSaveGuid.IsValid()) { return NativeSaveGuid; }
	// Placed actors keep the same identity across reload. Dynamic encounter
	// actors receive an explicit saved GUID before FinishSpawning.
	FGuid Result;
	FString StablePath = GetPathName();
	// PIE package prefixes are session-specific and cannot be part of a save key.
	const FString PIEPrefix = GetWorld() ? GetWorld()->StreamingLevelsPrefix : FString();
	if (!PIEPrefix.IsEmpty()) { StablePath.ReplaceInline(*PIEPrefix, TEXT("")); }
	FGuid::ParseExact(FMD5::HashAnsiString(*StablePath), EGuidFormats::Digits, Result);
	return Result;
}

void ASovNPCCharacterBase::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	NativeSaveGuid = SavedGUID;
	SpawnInfo.SpawnAssignedSaveGUID = SavedGUID;
}

bool ASovNPCCharacterBase::ShouldRespawn_Implementation() const
{
	// A settlement owns its own NPC spawns. Encounter replacements likewise
	// remain owned by their director rather than a second dynamic save spawn.
	return !SpawnInfo.OwningSpawnerGUID.IsValid() && !bEncounterOwned;
}

void ASovNPCCharacterBase::PrepareForEncounterRestore(const FNPCSpawnInfo& SavedSpawnInfo, const FGuid& SavedGUID)
{
	check(!HasActorBegunPlay());
	bEncounterRestoreInitialization = true;
	bEncounterOwned = true;
	SpawnInfo = SavedSpawnInfo;
	SetActorGUID_Implementation(SavedGUID);
}

void ASovNPCCharacterBase::EnsureEncounterController()
{
	if (HasAuthority() && !GetController()) { SpawnDefaultController(); }
}

void ASovNPCCharacterBase::OnCharacterVisualInitialized()
{
	if (bEncounterRestoreInitialization)
	{
		// Narrative's normal path auto-loads the last world record. A retry owns
		// a different (entry) record and applies it only after initialization.
		if (!bEncounterSnapshotReady) { InitNewCharacter(GetNPCDefinition()); }
		ANarrativeCharacter::OnCharacterVisualInitialized();
	}
	else
	{
		Super::OnCharacterVisualInitialized();
	}
	bEncounterSnapshotReady = true;
}
