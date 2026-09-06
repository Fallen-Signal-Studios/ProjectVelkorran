// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterPolicy.h"
#include "Campaign/SovResourceSnapshotPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
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
#include "NarrativeGameplayTags.h"
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

	// Only resource-current aggregators with an independently reconstructable
	// constant additive magnitude are supported. Inhibited/conditional, captured
	// attribute, custom calculation and override/multiply effects fail closed.
	bool CurrentResourceOffsets(UAbilitySystemComponent* ASC, const TArray<FGameplayAttribute>& Attributes,
		TArray<float>& OutOffsets, bool& bHasModifiers)
	{
		OutOffsets.Init(0.f, Attributes.Num());
		bHasModifiers = false;
		for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle);
			if (!Active || !Active->Spec.Def || Active->Spec.GetPeriod() > 0.f) { continue; }
			const UGameplayEffect* Definition = Active->Spec.Def;
			for (int32 Index = 0; Index < Definition->Modifiers.Num(); ++Index)
			{
				const FGameplayModifierInfo& Modifier = Definition->Modifiers[Index];
				const int32 ResourceIndex = Attributes.IndexOfByKey(Modifier.Attribute);
				if (ResourceIndex == INDEX_NONE) { continue; }
				bHasModifiers = true;
				if (!Active->Spec.Modifiers.IsValidIndex(Index)
					|| !FMath::IsFinite(Active->Spec.GetPeriod()) || Active->Spec.GetPeriod() < 0.f
					|| Active->bIsInhibited || Modifier.ModifierOp != EGameplayModOp::Additive
					|| Modifier.ModifierMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::ScalableFloat
					|| !Modifier.SourceTags.IsEmpty() || !Modifier.TargetTags.IsEmpty()
					|| !Modifier.SourceTags.TagQuery.IsEmpty() || !Modifier.TargetTags.TagQuery.IsEmpty()
					|| !Definition->Executions.IsEmpty()) { return false; }
				const float Magnitude = Active->Spec.GetModifierMagnitude(Index, true);
				OutOffsets[ResourceIndex] += Magnitude;
				if (!FMath::IsFinite(Magnitude) || !FMath::IsFinite(OutOffsets[ResourceIndex])) { return false; }
			}
		}
		return true;
	}

	TArray<FGameplayAttribute> ResourceAttributes()
	{
		return { UNarrativeAttributeSetBase::GetShieldAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(),
			UNarrativeAttributeSetBase::GetPoiseAttribute(), UNarrativeAttributeSetBase::GetHealthAttribute(),
			UNarrativeAttributeSetBase::GetEchoAttribute() };
	}
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> RestoringResourceOwners;

}

bool FSovCombatResourceSnapshot::IsValid() const
{
	if (SchemaVersion != 1 && SchemaVersion != 2) { return false; }
	const bool bCurrentsValid = SovEncounterPolicy::ValidResource(Health, MaxHealth)
		&& SovEncounterPolicy::ValidResource(Shield, MaxShield)
		&& SovEncounterPolicy::ValidResource(Stamina, MaxStamina)
		&& SovEncounterPolicy::ValidResource(Poise, MaxPoise)
		&& SovEncounterPolicy::ValidResource(Echo, MaxEcho);
	return bCurrentsValid && (SchemaVersion == 1 || (
		SovResourceSnapshotPolicy::ValidBase(BaseHealth, BaseMaxHealth)
		&& SovResourceSnapshotPolicy::ValidBase(BaseShield, BaseMaxShield)
		&& SovResourceSnapshotPolicy::ValidBase(BaseStamina, BaseMaxStamina)
		&& SovResourceSnapshotPolicy::ValidBase(BasePoise, BaseMaxPoise)
		&& SovResourceSnapshotPolicy::ValidBase(BaseEcho, BaseMaxEcho)));
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
	if (!IsValid(ASC) || !ASC->GetSet<UNarrativeAttributeSetBase>() || RestoringResourceOwners.Contains(ASC)) { return false; }
	TArray<float> Offsets;
	bool bHasModifiers = false;
	if (!CurrentResourceOffsets(ASC, ResourceAttributes(), Offsets, bHasModifiers)) { return false; }
	FSovCombatResourceSnapshot Result;
	Result.SchemaVersion = 2;
#define SOV_CAPTURE_RESOURCE(Name) Result.Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::Get##Name##Attribute()); Result.Max##Name = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute()); Result.Base##Name = ASC->GetNumericAttributeBase(UNarrativeAttributeSetBase::Get##Name##Attribute()); Result.BaseMax##Name = ASC->GetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute());
	SOV_CAPTURE_RESOURCE(Health)
	SOV_CAPTURE_RESOURCE(Shield)
	SOV_CAPTURE_RESOURCE(Stamina)
	SOV_CAPTURE_RESOURCE(Poise)
	SOV_CAPTURE_RESOURCE(Echo)
#undef SOV_CAPTURE_RESOURCE
	if (!Result.IsValid()) { return false; }
	const float Bases[] = { Result.BaseShield, Result.BaseStamina, Result.BasePoise, Result.BaseHealth, Result.BaseEcho };
	const float Currents[] = { Result.Shield, Result.Stamina, Result.Poise, Result.Health, Result.Echo };
	const float Maxima[] = { Result.MaxShield, Result.MaxStamina, Result.MaxPoise, Result.MaxHealth, Result.MaxEcho };
	for (int32 Index = 0; Index < Offsets.Num(); ++Index)
	{
		if (!FMath::IsNearlyEqual(Currents[Index], static_cast<float>(SovResourceSnapshotPolicy::Resolved(
			Bases[Index], Offsets[Index], Maxima[Index])), .01f)) { return false; }
	}
	OutSnapshot = Result;
	return true;
}

bool USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(UAbilitySystemComponent* ASC, FSovCombatResourceSnapshot& InOutSnapshot)
{
	if (!IsValid(ASC) || !ASC->GetSet<UNarrativeAttributeSetBase>() || !InOutSnapshot.IsValid()
		|| RestoringResourceOwners.Contains(ASC)) { return false; }
	TArray<float> Offsets;
	bool bHasModifiers = false;
	if (!CurrentResourceOffsets(ASC, ResourceAttributes(), Offsets, bHasModifiers)) { return false; }
	FSovCombatResourceSnapshot Result = InOutSnapshot;
	Result.SchemaVersion = 2;
	int32 Index = 0;
#define SOV_REBASE_RESOURCE(Name) \
	if (!FMath::IsNearlyEqual(Result.Max##Name, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMax##Name##Attribute()), .01f)) { return false; } \
	if (InOutSnapshot.SchemaVersion == 1 || !FMath::IsNearlyEqual(static_cast<float>(SovResourceSnapshotPolicy::Resolved(Result.Base##Name, Offsets[Index], Result.Max##Name)), Result.Name, .01f)) { Result.Base##Name = Result.Name - Offsets[Index]; } \
	Result.BaseMax##Name = ASC->GetNumericAttributeBase(UNarrativeAttributeSetBase::GetMax##Name##Attribute()); ++Index;
	SOV_REBASE_RESOURCE(Shield)
	SOV_REBASE_RESOURCE(Stamina)
	SOV_REBASE_RESOURCE(Poise)
	SOV_REBASE_RESOURCE(Health)
	SOV_REBASE_RESOURCE(Echo)
#undef SOV_REBASE_RESOURCE
	if (!Result.IsValid()) { return false; }
	InOutSnapshot = Result;
	return true;
}

bool USovEncounterSnapshotLibrary::RestoreResources(UAbilitySystemComponent* ASC, const FSovCombatResourceSnapshot& Snapshot)
{
	// Callbacks may mutate the caller's storage or attempt another restore. Freeze
	// the request and reserve this ASC before publishing any resource/death event.
	const FSovCombatResourceSnapshot FrozenSnapshot = Snapshot;
	if (!IsInGameThread() || !IsValid(ASC) || !FrozenSnapshot.IsValid()) { return false; }
	const TWeakObjectPtr<UAbilitySystemComponent> RestoreKey(ASC);
	if (RestoringResourceOwners.Contains(RestoreKey)) { return false; }
	TStrongObjectPtr<UAbilitySystemComponent> KeepASC(ASC);
	TStrongObjectPtr<AActor> Owner(ASC->GetOwnerActor());
	TStrongObjectPtr<AActor> Avatar(ASC->GetAvatarActor());
	TStrongObjectPtr<const UNarrativeAttributeSetBase> Attributes(ASC->GetSet<UNarrativeAttributeSetBase>());
	if (!Owner.IsValid() || !Avatar.IsValid() || !Attributes.IsValid()
		|| !Owner->HasAuthority() || !Avatar->HasAuthority()) { return false; }
	auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
	const uint64 ActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	const int32 ReadyEpoch = NarrativeASC ? NarrativeASC->GetCharacterReadyEpoch() : 0;
	const auto* Player = Cast<ANarrativePlayerCharacter>(Avatar.Get());
	const int32 InitializationGeneration = Player ? Player->GetCharacterInitializationGeneration() : 0;
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
			&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == ActorInfoEpoch
				&& NarrativeASC->GetCharacterReadyEpoch() == ReadyEpoch))
			&& (!Player || Player->GetCharacterInitializationGeneration() == InitializationGeneration)
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
	RestoringResourceOwners.Add(RestoreKey);
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
		RestoringResourceOwners.Remove(RestoreKey);
	};
	if (Shield.IsValid()) { Shield->SetCheckpointRestoreInProgress(true); ShieldRestoreGeneration = Shield->GetCheckpointRestoreGeneration(); }
	if (Poise.IsValid()) { Poise->SetCheckpointRestoreInProgress(true); PoiseRestoreGeneration = Poise->GetCheckpointRestoreGeneration(); }
	TArray<float> PreflightOffsets; bool bPreflightModifiers = false;
	if (!CurrentResourceOffsets(ASC, ResourceAttributes(), PreflightOffsets, bPreflightModifiers)
		|| (FrozenSnapshot.SchemaVersion == 1 && bPreflightModifiers)) { return false; }
	if (NarrativeASC && NarrativeASC->IsDead() && FrozenSnapshot.Health > 0.f) { NarrativeASC->Revive(); }
	if (!AcceptControlledRevival()) { return false; }

	const TArray<FGameplayAttribute> ResourceFields = ResourceAttributes();
	const TArray<FGameplayAttribute> Maxima = {
		UNarrativeAttributeSetBase::GetMaxShieldAttribute(), UNarrativeAttributeSetBase::GetMaxStaminaAttribute(),
		UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), UNarrativeAttributeSetBase::GetMaxHealthAttribute(),
		UNarrativeAttributeSetBase::GetMaxEchoAttribute() };
	TArray<float> Offsets; bool bHasModifiers = false;
	if (!CurrentResourceOffsets(ASC, ResourceFields, Offsets, bHasModifiers)
		|| (FrozenSnapshot.SchemaVersion == 1 && !SovResourceSnapshotPolicy::CanRestoreLegacy(bHasModifiers))) { return false; }
	const float Currents[] = { FrozenSnapshot.Shield, FrozenSnapshot.Stamina, FrozenSnapshot.Poise, FrozenSnapshot.Health, FrozenSnapshot.Echo };
	const float Bases[] = { FrozenSnapshot.BaseShield, FrozenSnapshot.BaseStamina, FrozenSnapshot.BasePoise, FrozenSnapshot.BaseHealth, FrozenSnapshot.BaseEcho };
	const float SavedMaxima[] = { FrozenSnapshot.MaxShield, FrozenSnapshot.MaxStamina, FrozenSnapshot.MaxPoise, FrozenSnapshot.MaxHealth, FrozenSnapshot.MaxEcho };
	TArray<float> DesiredBases, ExpectedCurrents, ExpectedMaxima;
	for (int32 Index = 0; Index < ResourceFields.Num(); ++Index)
	{
		const float Maximum = ASC->GetNumericAttribute(Maxima[Index]);
		if (!FMath::IsFinite(Maximum) || Maximum < 0.f) { return false; }
		float Base = FrozenSnapshot.SchemaVersion == 1
			? SovEncounterPolicy::ClampRestoredResource(Currents[Index], Maximum)
			: static_cast<float>(SovResourceSnapshotPolicy::RestoredBase(Bases[Index], SavedMaxima[Index], Maximum));
		// A dead snapshot cannot become a living avatar because a transient
		// penalty expired or a grant was reconstructed during load.
		if (Index == 3 && FrozenSnapshot.Health == 0.f) { Base = FMath::Min(Base, -Offsets[Index]); }
		const float Expected = static_cast<float>(SovResourceSnapshotPolicy::Resolved(Base, Offsets[Index], Maximum));
		if (!FMath::IsFinite(Base) || !FMath::IsFinite(Expected)) { return false; }
		DesiredBases.Add(Base); ExpectedCurrents.Add(Expected); ExpectedMaxima.Add(Maximum);
	}
	if (FrozenSnapshot.Health > 0.f && ExpectedCurrents[3] <= 0.f) { return false; }
	for (int32 Index = 0; Index < ResourceFields.Num(); ++Index)
	{
		if (!StillOwnsLife()) { return false; }
		ASC->SetNumericAttributeBase(ResourceFields[Index], DesiredBases[Index]);
		if (!(Index == 3 ? AcceptControlledRevival() : StillOwnsLife())
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(ResourceFields[Index]), DesiredBases[Index], .01f)
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(ResourceFields[Index]), ExpectedCurrents[Index], .01f)) { return false; }
	}
	if (Exertion.IsValid()) { Exertion->ResetForCheckpoint(); }
	if (!StillOwnsLife()) { return false; }
	if (Health.IsValid()) { Health->ResetForCheckpoint(); }
	if (!StillOwnsLife()) { return false; }
	if (Shield.IsValid()) { Shield->ResetForCheckpoint(); if (!Shield->IsCheckpointStateReconciled()) { return false; } }
	if (!StillOwnsLife()) { return false; }
	if (Poise.IsValid()) { Poise->ResetForCheckpoint(); if (!Poise->IsCheckpointStateReconciled()) { return false; } }
	if (!StillOwnsLife()) { return false; }
	if (Echo.IsValid()) { Echo->ResetCheckpointActivity(); }
	if (!StillOwnsLife()) { return false; }
	// Retain PR34's status safety restriction alongside the explicit v2 resource bases.
	if (Status.IsValid() && !Status->CompletePendingCheckpointRestore()) { return false; }
	if (!StillOwnsLife()) { return false; }
	const auto VerifyResourceVector = [&]()
	{
		TArray<float> FinalOffsets; bool bFinalModifiers = false;
		if (!CurrentResourceOffsets(ASC, ResourceFields, FinalOffsets, bFinalModifiers)) { return false; }
		for (int32 Index = 0; Index < ResourceFields.Num(); ++Index)
		{
			if (!FMath::IsNearlyEqual(FinalOffsets[Index], Offsets[Index], .01f)
				|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(Maxima[Index]), ExpectedMaxima[Index], .01f)
				|| !FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(ResourceFields[Index]), DesiredBases[Index], .01f)
				|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(ResourceFields[Index]), ExpectedCurrents[Index], .01f)) { return false; }
		}
		return true;
	};
	if (!VerifyResourceVector()) { return false; }
	// Release is part of the transaction: zero-duration Poise recovery can publish
	// callbacks immediately. Recheck after each release, not after a bool return
	// value has already been evaluated by a scope-exit guard.
	if (Shield.IsValid() && !Shield->EndCheckpointRestore(ShieldRestoreGeneration, true)) { return false; }
	if (!StillOwnsLife()) { return false; }
	if (Poise.IsValid() && !Poise->EndCheckpointRestore(PoiseRestoreGeneration, true)) { return false; }
	if (!StillOwnsLife()) { return false; }
	if (!VerifyResourceVector()) { return false; }
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
