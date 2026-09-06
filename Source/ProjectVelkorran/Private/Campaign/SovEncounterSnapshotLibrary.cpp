// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterPolicy.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "Components/SovEchoComponent.h"
#include "Exertion/SovExertionComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeSavableComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	bool IsCurrentSnapshotComponent(const UActorComponent* Component, const AActor* Owner, const FName Name)
	{
		if (!IsValid(Component) || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
			|| Component->GetOwner() != Owner || Component->GetFName() != Name)
		{
			return false;
		}
		TInlineComponentArray<UActorComponent*> Components(Owner);
		return Components.Contains(Component);
	}
}

bool FSovCombatResourceSnapshot::IsValid() const
{
	return SovEncounterPolicy::ValidResource(Health, MaxHealth)
		&& SovEncounterPolicy::ValidResource(Shield, MaxShield)
		&& SovEncounterPolicy::ValidResource(Stamina, MaxStamina)
		&& SovEncounterPolicy::ValidResource(Poise, MaxPoise)
		&& SovEncounterPolicy::ValidResource(Echo, MaxEcho);
}

bool FSovProtagonistSnapshot::IsValid() const
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	return SchemaVersion == 1 && (ProtagonistTag == Tags.Character_Player_Tarrik || ProtagonistTag == Tags.Character_Player_Selene)
		&& !PawnClass.IsNull() && !PlayerDefinition.IsNull() && Resources.IsValid() && PawnRecord.IsValid()
		&& Resources.Health > 0.f
		&& PawnRecord.ActorSoftClass.ToSoftObjectPath() == PawnClass.ToSoftObjectPath()
		&& !SkillTreeRecord.ComponentName.IsNone();
}

bool USovEncounterSnapshotLibrary::CaptureResources(UAbilitySystemComponent* ASC, FSovCombatResourceSnapshot& OutSnapshot)
{
	if (!IsValid(ASC) || !ASC->GetSet<UNarrativeAttributeSetBase>()) { return false; }
	FSovCombatResourceSnapshot Result;
#define SOV_CAPTURE_RESOURCE(Name) Result.Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::Get##Name##Attribute()); Result.Max##Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute());
	SOV_CAPTURE_RESOURCE(Health)
	SOV_CAPTURE_RESOURCE(Shield)
	SOV_CAPTURE_RESOURCE(Stamina)
	SOV_CAPTURE_RESOURCE(Poise)
	SOV_CAPTURE_RESOURCE(Echo)
#undef SOV_CAPTURE_RESOURCE
	if (!Result.IsValid()) { return false; }
	OutSnapshot = Result;
	return true;
}

bool USovEncounterSnapshotLibrary::RestoreResources(UAbilitySystemComponent* ASC, const FSovCombatResourceSnapshot& Snapshot)
{
	if (!IsValid(ASC) || !IsValid(ASC->GetOwnerActor()) || !ASC->GetOwnerActor()->HasAuthority()
		|| !ASC->GetSet<UNarrativeAttributeSetBase>() || !Snapshot.IsValid()) { return false; }
	AActor* Avatar = ASC->GetAvatarActor();
	USovShieldComponent* ShieldComponent = Avatar ? Avatar->FindComponentByClass<USovShieldComponent>() : nullptr;
	USovPoiseComponent* PoiseComponent = Avatar ? Avatar->FindComponentByClass<USovPoiseComponent>() : nullptr;
	TWeakObjectPtr<USovStatusComponent> StatusComponent = Avatar ? Avatar->FindComponentByClass<USovStatusComponent>() : nullptr;
	const bool bHadStatusComponent = StatusComponent.IsValid();
	if (ShieldComponent) { ShieldComponent->SetCheckpointRestoreInProgress(true); }
	if (PoiseComponent) { PoiseComponent->SetCheckpointRestoreInProgress(true); }
	ON_SCOPE_EXIT
	{
		if (IsValid(ShieldComponent)) { ShieldComponent->SetCheckpointRestoreInProgress(false); }
		if (IsValid(PoiseComponent)) { PoiseComponent->SetCheckpointRestoreInProgress(false); }
	};
	const auto StillOwnsAvatar = [&]() { return IsValid(ASC) && IsValid(Avatar) && ASC->GetAvatarActor() == Avatar; };
	// Generic Narrative NPCs initialize attributes on revive; campaign players retain
	// their existing maxima and grants. In either case restore explicit currents last.
	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC))
	{
		if (NarrativeASC->IsDead() && Snapshot.Health > 0.f) { NarrativeASC->Revive(); }
	}
	if (!StillOwnsAvatar()) { return false; }
#define SOV_RESTORE_RESOURCE(Name) { const float Desired = SovEncounterPolicy::ClampRestoredResource(Snapshot.Name, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute())); ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), Desired); if (!StillOwnsAvatar() || !FMath::IsNearlyEqual(ASC->GetNumericAttribute(UNarrativeAttributeSetBase::Get##Name##Attribute()), Desired, 0.01f)) { return false; } }
	SOV_RESTORE_RESOURCE(Shield)
	SOV_RESTORE_RESOURCE(Stamina)
	SOV_RESTORE_RESOURCE(Poise)
	SOV_RESTORE_RESOURCE(Health)
	SOV_RESTORE_RESOURCE(Echo)
#undef SOV_RESTORE_RESOURCE
	if (Avatar)
	{
		if (USovExertionComponent* Exertion = Avatar->FindComponentByClass<USovExertionComponent>()) { Exertion->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovHealthRechargeComponent* Health = Avatar->FindComponentByClass<USovHealthRechargeComponent>()) { Health->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovShieldComponent* Shield = Avatar->FindComponentByClass<USovShieldComponent>()) { Shield->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovPoiseComponent* Poise = Avatar->FindComponentByClass<USovPoiseComponent>()) { Poise->ResetForCheckpoint(); }
		if (!StillOwnsAvatar()) { return false; }
		if (USovEchoComponent* Echo = Avatar->FindComponentByClass<USovEchoComponent>())
		{
			Echo->RestoreEchoFromCheckpoint(Snapshot.Echo);
		}
		if (!StillOwnsAvatar()) { return false; }
		if (bHadStatusComponent)
		{
			// Component records queue status effects before resource loading. Commit
			// their saved state now, before encounter actors are unsuspended.
			if (!StatusComponent.IsValid()
				|| Avatar->FindComponentByClass<USovStatusComponent>() != StatusComponent.Get()
				|| !StatusComponent->CompletePendingCheckpointRestore()
				|| !StillOwnsAvatar()) { return false; }
		}
	}
	return StillOwnsAvatar();
}

bool USovEncounterSnapshotLibrary::CaptureComponent(UActorComponent* Component, FNarrativeSaveComponent& OutRecord)
{
	if (!IsValid(Component) || !Component->Implements<UNarrativeSavableComponent>()) { return false; }
	TStrongObjectPtr<UActorComponent> KeepComponent(Component);
	TStrongObjectPtr<AActor> KeepOwner(Component->GetOwner());
	FNarrativeSaveComponent Result;
	Result.ComponentName = Component->GetFName();
	const auto StillOwnsComponent = [&]()
	{
		return IsCurrentSnapshotComponent(Component, KeepOwner.Get(), Result.ComponentName);
	};
	if (!StillOwnsComponent()) { return false; }
	Result.ComponentClass = Component->GetClass();
	Result.bOptional = INarrativeSavableComponent::Execute_IsOptionalSaveRecord(Component);
	if (!StillOwnsComponent()) { return false; }
	const INarrativeSavableComponent* Policy = Cast<INarrativeSavableComponent>(Component);
	if (Policy)
	{
		Result.RestorePhase = Policy->GetSaveRestorePhase();
		if (!StillOwnsComponent()
			|| static_cast<uint8>(Result.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
	}
	INarrativeSavableComponent::Execute_PrepareForSave(Component);
	if (!StillOwnsComponent()) { return false; }
	FMemoryWriter Writer(Result.ByteData);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	Archive.ArIsSaveGame = true;
	Archive.ArNoDelta = true;
	Component->Serialize(Archive);
	if (Archive.IsError() || !StillOwnsComponent()) { return false; }
	if (Policy)
	{
		const bool bAccepted = Policy->ValidateSaveRecord(Result.ByteData);
		if (!bAccepted || !StillOwnsComponent()) { return false; }
	}
	OutRecord = MoveTemp(Result);
	return true;
}

bool USovEncounterSnapshotLibrary::RestoreComponent(UActorComponent* Component, const FNarrativeSaveComponent& Record)
{
	if (!IsValid(Component) || !Component->Implements<UNarrativeSavableComponent>()) { return false; }
	TStrongObjectPtr<UActorComponent> KeepComponent(Component);
	TStrongObjectPtr<AActor> KeepOwner(Component->GetOwner());
	// A callback may invalidate the caller's live snapshot collection.
	const FNarrativeSaveComponent StableRecord = Record;
	const auto StillOwnsComponent = [&]()
	{
		return IsCurrentSnapshotComponent(Component, KeepOwner.Get(), StableRecord.ComponentName)
			&& KeepOwner->HasAuthority();
	};
	if (!StillOwnsComponent() || StableRecord.ByteData.IsEmpty()
		|| static_cast<uint8>(StableRecord.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
	if (!StableRecord.ComponentClass.IsNull())
	{
		UClass* SavedClass = StableRecord.ComponentClass.LoadSynchronous();
		if (!StillOwnsComponent() || !SavedClass || !Component->IsA(SavedClass)) { return false; }
	}
	const INarrativeSavableComponent* Policy = Cast<INarrativeSavableComponent>(Component);
	if (Policy)
	{
		const bool bAccepted = Policy->ValidateSaveRecord(StableRecord.ByteData);
		if (!bAccepted || !StillOwnsComponent()) { return false; }
	}
	FMemoryReader Reader(StableRecord.ByteData);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;
	Archive.ArNoDelta = true;
	Component->Serialize(Archive);
	if (Archive.IsError() || !StillOwnsComponent()) { return false; }
	INarrativeSavableComponent::Execute_Load(Component);
	if (!StillOwnsComponent()) { return false; }
	const bool bAccepted = !Policy || Policy->WasSaveRecordLoadAccepted();
	return bAccepted && StillOwnsComponent();
}
