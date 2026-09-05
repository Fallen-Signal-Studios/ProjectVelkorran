// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Framework/SovPlayerState.h"
#include "Abilities/GameplayAbility.h"
#include "Progression/SovTechniqueComponent.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/PlayerDefinition.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/World.h"
#include "Misc/SecureHash.h"

ASovPlayerState::ASovPlayerState(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USovTechniqueComponent>(TEXT("SkillTreeComponent")))
{
	EnsureCampaignResourceSaveSelection();
}

void ASovPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !PlayerStateSaveGuid.IsValid()) { PlayerStateSaveGuid = GetActorGUID_Implementation(); }
	// Blueprint defaults may replace the constructor array. Merge required
	// currents without discarding any authored additional save attributes.
	EnsureCampaignResourceSaveSelection();
}

FGuid ASovPlayerState::GetActorGUID_Implementation() const
{
	if (PlayerStateSaveGuid.IsValid()) { return PlayerStateSaveGuid; }
	FGuid Result;
	FGuid::ParseExact(FMD5::HashAnsiString(*GetPathName()), EGuidFormats::Digits, Result);
	return Result;
}

void ASovPlayerState::SetCampaignFactions(const FGameplayTagContainer& Defaults)
{
	if (!HasAuthority()) { return; }
	Factions = Defaults;
	OnRep_Faction();
	ForceNetUpdate();
}

void ASovPlayerState::PrepareForSave_Implementation()
{
	if (!HasAuthority() || bRestoringProtagonist) { return; }
	if (ASovPlayerCharacterBase* Pawn = Cast<ASovPlayerCharacterBase>(GetPawn()))
	{
		FSovProtagonistSnapshot Snapshot;
		FString Error;
		if (CaptureProtagonistSnapshot(Pawn, Snapshot, Error)) { StoreProtagonistSnapshot(Snapshot); }
	}
}

void ASovPlayerState::EnsureCampaignResourceSaveSelection()
{
	if (!AbilitySystemComponent) { return; }
	AbilitySystemComponent->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetHealthAttribute());
	AbilitySystemComponent->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetShieldAttribute());
	AbilitySystemComponent->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetStaminaAttribute());
	AbilitySystemComponent->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetPoiseAttribute());
	AbilitySystemComponent->AttributesToSave.AddUnique(UNarrativeAttributeSetBase::GetEchoAttribute());
}

bool ASovPlayerState::CaptureProtagonistSnapshot(ASovPlayerCharacterBase* Pawn, FSovProtagonistSnapshot& OutSnapshot, FString& OutError)
{
	OutError.Reset();
	UNarrativeSaveSubsystem* Save = GetWorld() ? GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
	if (!HasAuthority() || bRestoringProtagonist || !IsValid(Pawn) || Pawn->GetPlayerState() != this || !AbilitySystemComponent || !Pawn->IsCharacterReady() || !Pawn->IsAlive()
		|| !IsValid(Pawn->GetPlayerDefinition()) || !Save || AbilitySystemComponent->GetAvatarActor() != Pawn)
	{
		OutError = TEXT("Capture requires the ready, possessed protagonist and Narrative save subsystem.");
		return false;
	}
	FSovProtagonistSnapshot Snapshot;
	Snapshot.ProtagonistTag = Pawn->GetProtagonistIdentityTag();
	Snapshot.PawnClass = Pawn->GetClass();
	Snapshot.PlayerDefinition = Pawn->GetPlayerDefinition();
	Snapshot.Factions = GetFactions();
	Snapshot.WieldEquipSlots = Pawn->GetWeaponWieldState().EquipSlots;
	Snapshot.WieldSlots = Pawn->GetWeaponWieldState().WieldSlots;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!Spec.Ability || Spec.RemoveAfterActivation) { continue; }
		FSovProtagonistAbilitySnapshot Grant; Grant.AbilityClass = Spec.Ability->GetClass(); Grant.Level = Spec.Level;
		Snapshot.GrantedAbilities.Add(Grant);
	}
	if (!USovEncounterSnapshotLibrary::CaptureResources(AbilitySystemComponent, Snapshot.Resources)
		|| !Save->CreateActorRecord(Pawn, Snapshot.PawnRecord)
		|| !USovEncounterSnapshotLibrary::CaptureComponent(SkillTreeComponent, Snapshot.SkillTreeRecord)
		|| !Snapshot.IsValid())
	{
		OutError = TEXT("Protagonist resources or Narrative component records could not be captured.");
		return false;
	}
	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

bool ASovPlayerState::StoreProtagonistSnapshot(const FSovProtagonistSnapshot& Snapshot)
{
	if (!HasAuthority() || bRestoringProtagonist || !Snapshot.IsValid()) { return false; }
	ProtagonistSnapshots.Add(Snapshot.ProtagonistTag, Snapshot);
	return true;
}

bool ASovPlayerState::FindProtagonistSnapshot(FGameplayTag Protagonist, FSovProtagonistSnapshot& OutSnapshot) const
{
	if (const FSovProtagonistSnapshot* Found = ProtagonistSnapshots.Find(Protagonist))
	{
		if (Found->IsValid()) { OutSnapshot = *Found; return true; }
	}
	return false;
}

bool ASovPlayerState::RestoreProtagonistSnapshot(ASovPlayerCharacterBase* Pawn, const FSovProtagonistSnapshot& Snapshot, bool bRestoreTransform, FString& OutError)
{
	OutError.Reset();
	UNarrativeSaveSubsystem* Save = GetWorld() ? GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
	if (!HasAuthority() || bRestoringProtagonist || !IsValid(Pawn) || Pawn->GetPlayerState() != this || !AbilitySystemComponent
		|| (!Pawn->IsCharacterReady() && !Pawn->IsCampaignDataReadyToApply())
		|| !Snapshot.IsValid() || !Save || AbilitySystemComponent->GetAvatarActor() != Pawn
		|| Snapshot.ProtagonistTag != Pawn->GetProtagonistIdentityTag()
		|| Snapshot.PawnClass.Get() != Pawn->GetClass() || Snapshot.PlayerDefinition.Get() != Pawn->GetPlayerDefinition())
	{
		OutError = TEXT("Restore requires an initialized matching protagonist class/definition; no state was applied.");
		return false;
	}
	TGuardValue<bool> RestoreGuard(bRestoringProtagonist, true);
	UNarrativeAbilitySystemComponent* OriginalASC = AbilitySystemComponent;
	const auto StillOwnsPawn = [&]()
	{
		if (!IsValid(this) || !IsValid(Pawn) || !IsValid(OriginalASC) || AbilitySystemComponent != OriginalASC
			|| Pawn->GetPlayerState() != this || OriginalASC->GetAvatarActor() != Pawn)
		{
			OutError = TEXT("Protagonist ownership changed during restore; the retained origin snapshot remains available.");
			return false;
		}
		return true;
	};
	// Managed new pawns have no input yet. Preserve their activate-on-grant
	// defaults; the campaign controller already tore down the outgoing avatar.
	if (Pawn->IsCharacterReady()) { OriginalASC->CancelAllAbilities(); }
	if (!StillOwnsPawn()) { return false; }
	Pawn->SetWieldState(FWeaponWieldState());
	if (!StillOwnsPawn()) { return false; }
	FNarrativeActorRecord PawnRecord = Snapshot.PawnRecord;
	// PlayerState ASC is not in PawnRecord; resource restoration is explicit.
	if (!bRestoreTransform) { PawnRecord.Transform = FTransform::Identity; PawnRecord.bHasTransform = false; }
	if (!Save->LoadActorFromRecord(Pawn, PawnRecord))
	{
		OutError = TEXT("Narrative rejected the pawn record; the retained origin snapshot remains available."); return false;
	}
	if (!StillOwnsPawn()) { return false; }
	FWeaponWieldState RestoredWields;
	RestoredWields.EquipSlots = Snapshot.WieldEquipSlots;
	RestoredWields.WieldSlots = Snapshot.WieldSlots;
	Pawn->SetWieldState(RestoredWields);
	if (!StillOwnsPawn()) { return false; }
	if (bRestoreTransform) { Pawn->SetActorTransform(Snapshot.PawnRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics); }
	if (!StillOwnsPawn()) { return false; }
	if (!USovEncounterSnapshotLibrary::RestoreComponent(SkillTreeComponent, Snapshot.SkillTreeRecord))
	{
		OutError = TEXT("Skill-tree record failed after pawn restore. The caller must recover from its retained origin snapshot.");
		return false;
	}
	if (!StillOwnsPawn()) { return false; }
	if (const USovTechniqueComponent* Techniques = Cast<USovTechniqueComponent>(SkillTreeComponent))
	{
		if (!Techniques->IsTechniqueStateValid())
		{
			OutError = TEXT("Saved Technique progression is incompatible with current authoring."); return false;
		}
	}
	// SetFactions rejects empty containers, so restore our own saved property and
	// use the inherited presentation notification for either populated or empty state.
	SetCampaignFactions(Snapshot.Factions);
	if (!StillOwnsPawn()) { return false; }
	if (!USovEncounterSnapshotLibrary::RestoreResources(OriginalASC, Snapshot.Resources))
	{
		OutError = TEXT("Resources failed after pawn restore. The caller must recover from its retained origin snapshot.");
		return false;
	}
	if (!StillOwnsPawn()) { return false; }
	ForceNetUpdate();
	return true;
}
