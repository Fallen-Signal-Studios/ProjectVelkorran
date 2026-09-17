// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovNPCCharacterBase.h"
#include "Presentation/SovBloodFeedbackComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Character/NarrativeCharacterVisual.h"

#include "Components/SovCombatSustainDropComponent.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovStatusComponent.h"
#include "Targeting/SovTargetingComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/SecureHash.h"
#include "AI/NPCDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASovNPCCharacterBase::ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    CreateDefaultSubobject<USovBloodFeedbackComponent>(TEXT("SovBloodFeedback"));
	DismembermentComponent = CreateDefaultSubobject<USovDismembermentComponent>(
		TEXT("SovDismembermentComponent"));
	CombatSustainDropComponent = CreateDefaultSubobject<USovCombatSustainDropComponent>(
		TEXT("SovCombatSustainDropComponent"));
	StatusComponent = CreateDefaultSubobject<USovStatusComponent>(TEXT("SovStatusComponent"));
}

void ASovNPCCharacterBase::BeginPlay()
{
	// NPC Super::BeginPlay initializes attributes, startup effects and default
	// abilities from GetNPCDefinition(). Match a normal deferred Narrative spawn:
	// publish the placed fallback before that one-time startup pipeline executes.
	// A spawner or restore definition already supplied before BeginPlay still wins.
	bool bInitializePlacedController = false;
	if (HasAuthority() && IsValid(AuthoredPlacedDefinition) && !GetNPCDefinition())
	{
		FString Error;
		bInitializePlacedController = InitializeAuthoredPlacedDefinition(Error);
		if (!bInitializePlacedController)
		{
			UE_LOG(LogTemp, Warning, TEXT("Placed encounter NPC %s: %s"), *GetPathName(), *Error);
		}
	}

	Super::BeginPlay();

	// Declared combat roles are lockable threats. AddUnique keeps an authored tag idempotent.
	if (bPermitsHardLock) { Tags.AddUnique(USovTargetingComponent::HardLockPermissionTag()); }

	// Narrative initializes an NPC's pawn-owned ASC during its BeginPlay. The
	// component also retains its OnASCInitialized fallback for unusual ordering.
	if (StatusComponent)
	{
		StatusComponent->InitializeWithAbilitySystem(
			GetNarrativeAbilitySystemComponent());
	}
	if (bInitializePlacedController && IsValid(this) && !IsActorBeingDestroyed())
	{
		EnsureEncounterController();
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
	// Normal Narrative spawners publish this home transform as part of their
	// spawn metadata. A directly authored fallback has no spawner to do that:
	// capture its real placement before definition/activity callbacks can use it.
	// Existing spawner metadata (including an intentional origin home), explicit
	// transforms, overrides and durable actor identities remain externally owned.
	if (!SpawnInfo.OwningSpawnerGUID.IsValid() && SpawnInfo.SpawnName.IsNone()
		&& !SpawnInfo.OwningSpawn.IsValid() && SpawnInfo.SpawnTransform.Equals(FTransform::Identity, 0.f))
	{
		const FTransform Placement = GetActorTransform();
		if (Placement.ContainsNaN())
		{ Error = TEXT("Placed NPC needs a finite home transform before definition initialization."); return false; }
		SpawnInfo.SpawnTransform = Placement;
	}
	// Repeatable Narrative Blueprint NPCs return this spawn metadata directly
	// from their GetActorGUID override. A placed actor has no spawner to publish
	// it, so bridge the existing native stable identity before definition callbacks.
	// Keep supplied/save identities, and do not re-enter the Blueprint query.
	if (!SpawnInfo.SpawnAssignedSaveGUID.IsValid())
	{
		const FGuid PlacedIdentity = ASovNPCCharacterBase::GetActorGUID_Implementation();
		if (!PlacedIdentity.IsValid())
		{ Error = TEXT("Placed NPC needs a valid native identity before definition initialization."); return false; }
		SpawnInfo.SpawnAssignedSaveGUID = PlacedIdentity;
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
	if (!IsValid(this) || IsActorBeingDestroyed()) { return; }
	if (!IsValid(GetCharacterVisual()))
	{
		ANarrativeCharacter::OnCharacterVisualInitialized();
		return;
	}
	const TWeakObjectPtr<ANarrativeCharacterVisual> InitialVisual(GetCharacterVisual());
	const auto IsCurrentVisual = [this, InitialVisual]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && InitialVisual.IsValid()
			&& !InitialVisual->IsActorBeingDestroyed() && GetCharacterVisual() == InitialVisual.Get();
	};
	if (!IsCurrentVisual()) { return; }
	TStrongObjectPtr<ANarrativeCharacter> KeepCharacter(this);
	TStrongObjectPtr<ANarrativeCharacterVisual> KeepVisual(InitialVisual.Get());
	if (bEncounterRestoreInitialization)
	{
		// Narrative's normal path auto-loads the last world record. A retry owns
		// a different (entry) record and applies it only after initialization.
		if (!bEncounterSnapshotReady)
		{
			InitNewCharacter(GetNPCDefinition());
			if (!IsCurrentVisual()) { return; }
		}
		ANarrativeCharacter::OnCharacterVisualInitialized();
	}
	else
	{
		Super::OnCharacterVisualInitialized();
	}
	// A restore or notification may have retired this pawn/body pair.
	if (IsCurrentVisual()) { bEncounterSnapshotReady = true; }
}
