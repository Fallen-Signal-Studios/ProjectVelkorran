// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovNPCCharacterBase.h"

#include "Components/SovCombatSustainDropComponent.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/SecureHash.h"
#include "AI/NPCDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASovNPCCharacterBase::ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DismembermentComponent = CreateDefaultSubobject<USovDismembermentComponent>(
		TEXT("SovDismembermentComponent"));
	CombatSustainDropComponent = CreateDefaultSubobject<USovCombatSustainDropComponent>(
		TEXT("SovCombatSustainDropComponent"));
	StatusComponent = CreateDefaultSubobject<USovStatusComponent>(TEXT("SovStatusComponent"));
}

void ASovNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Narrative initializes an NPC's pawn-owned ASC during its BeginPlay. The
	// component also retains its OnASCInitialized fallback for unusual ordering.
	if (StatusComponent)
	{
		StatusComponent->InitializeWithAbilitySystem(
			GetNarrativeAbilitySystemComponent());
	}
	if (HasAuthority() && IsValid(AuthoredPlacedDefinition) && !GetNPCDefinition())
	{
		FString Error;
		if (InitializeAuthoredPlacedDefinition(Error)) { EnsureEncounterController(); }
		else { UE_LOG(LogTemp, Warning, TEXT("Placed encounter NPC %s: %s"), *GetPathName(), *Error); }
	}
}

bool ASovNPCCharacterBase::InitializeAuthoredPlacedDefinition(FString& Error)
{
	Error.Reset();
	// Deferred spawns and encounter restoration already supply their actual definition.
	// Never reapply it: that would duplicate definition-owned grants and appearance work.
	if (GetNPCDefinition()) { return true; }
	if (!HasAuthority() || !GetWorld() || !GetWorld()->IsGameWorld() || IsActorBeingDestroyed()
		|| bEncounterRestoreInitialization || !IsValid(AuthoredPlacedDefinition))
	{ Error = TEXT("A living authority-world placed NPC needs an authored definition; restore must supply its own definition."); return false; }
	UNPCDefinition* const Definition = AuthoredPlacedDefinition;
	UClass* const ExpectedClass = Definition->NPCClassPath.LoadSynchronous();
	if (!IsValid(this) || IsActorBeingDestroyed() || GetNPCDefinition() || AuthoredPlacedDefinition != Definition)
	{ Error = TEXT("Placed NPC ownership changed while loading its role class."); return false; }
	if (!ExpectedClass || !IsA(ExpectedClass))
	{ Error = TEXT("Placed NPC class must match or derive from its authored definition's NPCClassPath."); return false; }
	if (!Definition->bAllowMultipleInstances)
	{
		for (TActorIterator<ASovNPCCharacterBase> It(GetWorld()); It; ++It)
		{
			if (*It == this || It->IsActorBeingDestroyed()) { continue; }
			const UNPCDefinition* Other = It->GetNPCDefinition() ? It->GetNPCDefinition() : It->AuthoredPlacedDefinition.Get();
			if (Other == Definition || (Other && Other->NPCID == Definition->NPCID))
			{ Error = TEXT("A unique NPC definition is assigned to more than one live/placed character."); return false; }
		}
	}
	SetNPCDefinition(Definition);
	if (!IsValid(this) || IsActorBeingDestroyed() || GetNPCDefinition() != Definition)
	{ Error = TEXT("Placed NPC definition callback changed or retired the character."); return false; }
	return true;
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
