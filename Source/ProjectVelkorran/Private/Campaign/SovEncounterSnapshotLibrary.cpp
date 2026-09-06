// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterPolicy.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
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
#include "NarrativeGameplayTags.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// Game-thread transaction admission; this is not a second resource store.
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> ResourceRestoresInProgress;

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
	// Callbacks may mutate the caller's storage or attempt another restore. Freeze
	// the request and reserve this ASC before publishing any resource/death event.
	const FSovCombatResourceSnapshot FrozenSnapshot = Snapshot;
	if (!IsInGameThread() || !IsValid(ASC) || !FrozenSnapshot.IsValid()) { return false; }
	const TWeakObjectPtr<UAbilitySystemComponent> RestoreKey(ASC);
	if (ResourceRestoresInProgress.Contains(RestoreKey)) { return false; }
	TStrongObjectPtr<UAbilitySystemComponent> KeepASC(ASC);
	TStrongObjectPtr<AActor> Owner(ASC->GetOwnerActor());
	TStrongObjectPtr<AActor> Avatar(ASC->GetAvatarActor());
	TStrongObjectPtr<const UNarrativeAttributeSetBase> Attributes(ASC->GetSet<UNarrativeAttributeSetBase>());
	if (!Owner.IsValid() || !Avatar.IsValid() || !Attributes.IsValid()
		|| !Owner->HasAuthority() || !Avatar->HasAuthority()) { return false; }
	auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
	const uint64 ActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	uint64 AcceptedLifeEpoch = Attributes->GetCombatLifeEpoch();
	bool bMayAdvanceLife = Attributes->GetHealth() <= 0.f && FrozenSnapshot.Health > 0.f;
	TStrongObjectPtr<USovShieldComponent> Shield(Avatar->FindComponentByClass<USovShieldComponent>());
	TStrongObjectPtr<USovPoiseComponent> Poise(Avatar->FindComponentByClass<USovPoiseComponent>());
	TStrongObjectPtr<USovExertionComponent> Exertion(Avatar->FindComponentByClass<USovExertionComponent>());
	TStrongObjectPtr<USovHealthRechargeComponent> Health(Avatar->FindComponentByClass<USovHealthRechargeComponent>());
	TStrongObjectPtr<USovEchoComponent> Echo(Avatar->FindComponentByClass<USovEchoComponent>());
	TStrongObjectPtr<USovStatusComponent> Status(Avatar->FindComponentByClass<USovStatusComponent>());
	uint64 ShieldRestoreGeneration = 0;
	uint64 PoiseRestoreGeneration = 0;
	const auto StillOwnsStorage = [&]()
	{
		return IsValid(ASC) && IsValid(Owner.Get()) && IsValid(Avatar.Get()) && IsValid(Attributes.Get())
			&& !Owner->IsActorBeingDestroyed() && !Avatar->IsActorBeingDestroyed()
			&& Owner->HasAuthority() && Avatar->HasAuthority()
			&& ASC->GetOwnerActor() == Owner.Get() && ASC->GetAvatarActor() == Avatar.Get()
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar.Get()) == ASC
			&& ASC->GetSet<UNarrativeAttributeSetBase>() == Attributes.Get()
			&& Attributes->GetOwningAbilitySystemComponent() == ASC
			&& (!NarrativeASC || NarrativeASC->GetCombatActorInfoEpoch() == ActorInfoEpoch)
			&& Avatar->FindComponentByClass<USovShieldComponent>() == Shield.Get()
			&& Avatar->FindComponentByClass<USovPoiseComponent>() == Poise.Get()
			&& Avatar->FindComponentByClass<USovExertionComponent>() == Exertion.Get()
			&& Avatar->FindComponentByClass<USovHealthRechargeComponent>() == Health.Get()
			&& Avatar->FindComponentByClass<USovEchoComponent>() == Echo.Get()
			&& Avatar->FindComponentByClass<USovStatusComponent>() == Status.Get()
			&& (!ShieldRestoreGeneration || (IsValid(Shield.Get()) && Shield->GetCheckpointRestoreGeneration() == ShieldRestoreGeneration))
			&& (!PoiseRestoreGeneration || (IsValid(Poise.Get()) && Poise->GetCheckpointRestoreGeneration() == PoiseRestoreGeneration));
	};
	const auto StillOwnsLife = [&]()
	{
		return StillOwnsStorage() && Attributes->GetCombatLifeEpoch() == AcceptedLifeEpoch
			&& FMath::IsFinite(Attributes->GetHealth())
			&& (FrozenSnapshot.Health <= 0.f || ((bMayAdvanceLife || Attributes->GetHealth() > 0.f)
				&& (!NarrativeASC || !NarrativeASC->IsDead())
				&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
				&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)));
	};
	// A single 0 -> positive transition is permitted only at the explicit revive
	// or Health write. An unrelated callback cannot consume that permission.
	const auto AcceptControlledRevival = [&]()
	{
		if (!StillOwnsStorage()) { return false; }
		const uint64 CurrentLife = Attributes->GetCombatLifeEpoch();
		if (CurrentLife != AcceptedLifeEpoch)
		{
			if (!bMayAdvanceLife || CurrentLife != AcceptedLifeEpoch + 1 || Attributes->GetHealth() <= 0.f) { return false; }
			AcceptedLifeEpoch = CurrentLife;
			bMayAdvanceLife = false;
		}
		return StillOwnsLife();
	};
	if (!StillOwnsStorage()) { return false; }
	ResourceRestoresInProgress.Add(RestoreKey);
	bool bCommitted = false;
	ON_SCOPE_EXIT
	{
		// Retire precisely our barrier even when ownership changed. Generation
		// matching preserves a newer barrier; abort never resumes passive work.
		if (!bCommitted)
		{
			if (IsValid(Shield.Get()) && ShieldRestoreGeneration) { Shield->EndCheckpointRestore(ShieldRestoreGeneration, false); }
			if (IsValid(Poise.Get()) && PoiseRestoreGeneration) { Poise->EndCheckpointRestore(PoiseRestoreGeneration, false); }
		}
		ResourceRestoresInProgress.Remove(RestoreKey);
	};
	if (Shield.IsValid()) { Shield->SetCheckpointRestoreInProgress(true); ShieldRestoreGeneration = Shield->GetCheckpointRestoreGeneration(); }
	if (Poise.IsValid()) { Poise->SetCheckpointRestoreInProgress(true); PoiseRestoreGeneration = Poise->GetCheckpointRestoreGeneration(); }
	if (NarrativeASC && NarrativeASC->IsDead() && FrozenSnapshot.Health > 0.f) { NarrativeASC->Revive(); }
	if (!AcceptControlledRevival()) { return false; }

	// NPC revival may legitimately initialize authored maxima. Freeze those values
	// after revival; later callbacks cannot silently retune half the transaction.
	FSovCombatResourceSnapshot Desired = FrozenSnapshot;
#define SOV_FREEZE_RESOURCE(Name) Desired.Max##Name = Attributes->GetMax##Name(); Desired.Name = SovEncounterPolicy::ClampRestoredResource(FrozenSnapshot.Name, Desired.Max##Name);
	SOV_FREEZE_RESOURCE(Health) SOV_FREEZE_RESOURCE(Shield) SOV_FREEZE_RESOURCE(Stamina) SOV_FREEZE_RESOURCE(Poise) SOV_FREEZE_RESOURCE(Echo)
#undef SOV_FREEZE_RESOURCE
	if (!Desired.IsValid() || (FrozenSnapshot.Health > 0.f && Desired.Health <= 0.f)) { return false; }
#define SOV_RESTORE_RESOURCE(Name) ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute(), Desired.Name); if (!StillOwnsLife() || !FMath::IsNearlyEqual(Attributes->Get##Name(), Desired.Name, 0.01f)) { return false; }
	SOV_RESTORE_RESOURCE(Shield)
	SOV_RESTORE_RESOURCE(Stamina)
	SOV_RESTORE_RESOURCE(Poise)
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), Desired.Health);
	if (!AcceptControlledRevival() || !FMath::IsNearlyEqual(Attributes->GetHealth(), Desired.Health, 0.01f)) { return false; }
	SOV_RESTORE_RESOURCE(Echo)
#undef SOV_RESTORE_RESOURCE
	if (Exertion.IsValid()) { Exertion->ResetForCheckpoint(); }
	if (!StillOwnsLife()) { return false; }
	if (Health.IsValid()) { Health->ResetForCheckpoint(); }
	if (!StillOwnsLife()) { return false; }
	if (Shield.IsValid()) { Shield->ResetForCheckpoint(); if (!Shield->IsCheckpointStateReconciled()) { return false; } }
	if (!StillOwnsLife()) { return false; }
	if (Poise.IsValid()) { Poise->ResetForCheckpoint(); if (!Poise->IsCheckpointStateReconciled()) { return false; } }
	if (!StillOwnsLife()) { return false; }
	if (Echo.IsValid()) { Echo->RestoreEchoFromCheckpoint(Desired.Echo); }
	if (!StillOwnsLife()) { return false; }
	// PR34's generic-status modifier checks and queued restore barrier remain the
	// authority; no resolved-current/base-value save format conversion is added.
	if (Status.IsValid() && !Status->CompletePendingCheckpointRestore()) { return false; }
	if (!StillOwnsLife()) { return false; }
#define SOV_VERIFY_RESOURCE(Name) if (!FMath::IsNearlyEqual(Attributes->Get##Name(), Desired.Name, 0.01f) || !FMath::IsNearlyEqual(Attributes->GetMax##Name(), Desired.Max##Name, 0.01f)) { return false; }
	SOV_VERIFY_RESOURCE(Health) SOV_VERIFY_RESOURCE(Shield) SOV_VERIFY_RESOURCE(Stamina) SOV_VERIFY_RESOURCE(Poise) SOV_VERIFY_RESOURCE(Echo)
#undef SOV_VERIFY_RESOURCE
	// Release is part of the transaction: zero-duration Poise recovery can publish
	// callbacks immediately. Recheck after each release, not after a bool return
	// value has already been evaluated by a scope-exit guard.
	if (Shield.IsValid() && !Shield->EndCheckpointRestore(ShieldRestoreGeneration, true)) { return false; }
	if (!StillOwnsLife()) { return false; }
	if (Poise.IsValid() && !Poise->EndCheckpointRestore(PoiseRestoreGeneration, true)) { return false; }
	if (!StillOwnsLife()) { return false; }
	bCommitted = true;
	return true;
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
