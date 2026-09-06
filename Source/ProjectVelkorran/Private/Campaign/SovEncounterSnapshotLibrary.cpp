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
	// The caller may own this record in a collection that a GAS callback replaces.
	const FSovCombatResourceSnapshot StableSnapshot = Snapshot;
	if (!IsValid(ASC) || !IsValid(ASC->GetOwnerActor()) || !ASC->GetOwnerActor()->HasAuthority()
		|| !ASC->GetSet<UNarrativeAttributeSetBase>() || !StableSnapshot.IsValid()
		|| RestoringResourceOwners.Contains(ASC)) { return false; }
	TStrongObjectPtr<UAbilitySystemComponent> KeepASC(ASC);
	TStrongObjectPtr<AActor> KeepAvatar(ASC->GetAvatarActor());
	TStrongObjectPtr<const UNarrativeAttributeSetBase> KeepAttributes(ASC->GetSet<UNarrativeAttributeSetBase>());
	AActor* Avatar = KeepAvatar.Get();
	if (!IsValid(Avatar) || Avatar->IsActorBeingDestroyed()
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar) != ASC) { return false; }
	UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
	const uint64 ActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	const int32 ReadyEpoch = NarrativeASC ? NarrativeASC->GetCharacterReadyEpoch() : 0;
	const ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(Avatar);
	const int32 InitializationGeneration = Player ? Player->GetCharacterInitializationGeneration() : 0;
	uint64 LifeEpoch = KeepAttributes->GetCombatLifeEpoch();
	TWeakObjectPtr<USovShieldComponent> ShieldComponent = Avatar->FindComponentByClass<USovShieldComponent>();
	TWeakObjectPtr<USovPoiseComponent> PoiseComponent = Avatar->FindComponentByClass<USovPoiseComponent>();
	TWeakObjectPtr<USovStatusComponent> StatusComponent = Avatar->FindComponentByClass<USovStatusComponent>();
	const bool bHadShieldComponent = ShieldComponent.IsValid();
	const bool bHadPoiseComponent = PoiseComponent.IsValid();
	const bool bHadStatusComponent = StatusComponent.IsValid();
	const uint64 ShieldGeneration = ShieldComponent.IsValid() ? ShieldComponent->GetBindingGeneration() : 0;
	const uint64 PoiseGeneration = PoiseComponent.IsValid() ? PoiseComponent->GetBindingGeneration() : 0;
	RestoringResourceOwners.Add(ASC);
	bool bPassiveRestoreStarted = false;
	const auto BeginPassiveRestore = [&]()
	{
		if (bPassiveRestoreStarted) { return; }
		bPassiveRestoreStarted = true;
		if (ShieldComponent.IsValid()) { ShieldComponent->SetCheckpointRestoreInProgress(true, ShieldGeneration); }
		if (PoiseComponent.IsValid()) { PoiseComponent->SetCheckpointRestoreInProgress(true, PoiseGeneration); }
	};
	const auto EndPassiveRestore = [&]()
	{
		if (!bPassiveRestoreStarted) { return; }
		bPassiveRestoreStarted = false;
		if (ShieldComponent.IsValid()) { ShieldComponent->SetCheckpointRestoreInProgress(false, ShieldGeneration); }
		if (PoiseComponent.IsValid()) { PoiseComponent->SetCheckpointRestoreInProgress(false, PoiseGeneration); }
	};
	ON_SCOPE_EXIT
	{
		EndPassiveRestore();
		RestoringResourceOwners.Remove(ASC);
	};
	const auto StillOwnsAvatar = [&]()
	{
		return IsValid(ASC) && IsValid(Avatar) && !Avatar->IsActorBeingDestroyed()
			&& ASC->GetAvatarActor() == Avatar && ASC->GetSet<UNarrativeAttributeSetBase>() == KeepAttributes.Get()
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar) == ASC
			&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == ActorInfoEpoch
				&& NarrativeASC->GetCharacterReadyEpoch() == ReadyEpoch))
			&& (!Player || Player->GetCharacterInitializationGeneration() == InitializationGeneration)
			&& (bHadShieldComponent ? ShieldComponent.IsValid()
				&& Avatar->FindComponentByClass<USovShieldComponent>() == ShieldComponent.Get()
				&& ShieldComponent->GetBindingGeneration() == ShieldGeneration
				: Avatar->FindComponentByClass<USovShieldComponent>() == nullptr)
			&& (bHadPoiseComponent ? PoiseComponent.IsValid()
				&& Avatar->FindComponentByClass<USovPoiseComponent>() == PoiseComponent.Get()
				&& PoiseComponent->GetBindingGeneration() == PoiseGeneration
				: Avatar->FindComponentByClass<USovPoiseComponent>() == nullptr)
			&& (bHadStatusComponent ? StatusComponent.IsValid()
				&& Avatar->FindComponentByClass<USovStatusComponent>() == StatusComponent.Get()
				: Avatar->FindComponentByClass<USovStatusComponent>() == nullptr)
			&& KeepAttributes->GetCombatLifeEpoch() == LifeEpoch;
	};
	const TArray<FGameplayAttribute> Attributes = ResourceAttributes();
	const TArray<FGameplayAttribute> Maxima = {
		UNarrativeAttributeSetBase::GetMaxShieldAttribute(), UNarrativeAttributeSetBase::GetMaxStaminaAttribute(),
		UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), UNarrativeAttributeSetBase::GetMaxHealthAttribute(),
		UNarrativeAttributeSetBase::GetMaxEchoAttribute() };
	TArray<float> Offsets;
	bool bHasModifiers = false;
	// Preflight before revive or the first resource mutation.
	if (!CurrentResourceOffsets(ASC, Attributes, Offsets, bHasModifiers)
		|| (StableSnapshot.SchemaVersion == 1 && !SovResourceSnapshotPolicy::CanRestoreLegacy(bHasModifiers))) { return false; }
	if (NarrativeASC && NarrativeASC->IsDead() && StableSnapshot.Health > 0.f)
	{
		const bool bWasZero = KeepAttributes->GetHealth() <= 0.f;
		BeginPassiveRestore();
		NarrativeASC->Revive();
		if (bWasZero && KeepAttributes->GetHealth() > 0.f) { ++LifeEpoch; }
		if (!StillOwnsAvatar()) { return false; }
		if (!CurrentResourceOffsets(ASC, Attributes, Offsets, bHasModifiers)
			|| (StableSnapshot.SchemaVersion == 1 && bHasModifiers)) { return false; }
	}
	const float Currents[] = { StableSnapshot.Shield, StableSnapshot.Stamina, StableSnapshot.Poise, StableSnapshot.Health, StableSnapshot.Echo };
	const float Bases[] = { StableSnapshot.BaseShield, StableSnapshot.BaseStamina, StableSnapshot.BasePoise, StableSnapshot.BaseHealth, StableSnapshot.BaseEcho };
	const float SavedMaxima[] = { StableSnapshot.MaxShield, StableSnapshot.MaxStamina, StableSnapshot.MaxPoise, StableSnapshot.MaxHealth, StableSnapshot.MaxEcho };
	TArray<float> DesiredBases, ExpectedCurrents, ExpectedMaxima;
	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const float Maximum = ASC->GetNumericAttribute(Maxima[Index]);
		if (!FMath::IsFinite(Maximum) || Maximum < 0.f) { return false; }
		float Base = StableSnapshot.SchemaVersion == 1
			? SovEncounterPolicy::ClampRestoredResource(Currents[Index], Maximum)
			: static_cast<float>(SovResourceSnapshotPolicy::RestoredBase(Bases[Index], SavedMaxima[Index], Maximum));
		// A dead snapshot cannot become a living avatar because a transient
		// penalty expired or a grant was reconstructed during load.
		if (Index == 3 && StableSnapshot.Health == 0.f) { Base = FMath::Min(Base, -Offsets[Index]); }
		const float Expected = static_cast<float>(SovResourceSnapshotPolicy::Resolved(Base, Offsets[Index], Maximum));
		if (!FMath::IsFinite(Base) || !FMath::IsFinite(Expected)) { return false; }
		DesiredBases.Add(Base); ExpectedCurrents.Add(Expected); ExpectedMaxima.Add(Maximum);
	}
	BeginPassiveRestore();
	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		if (!StillOwnsAvatar()) { return false; }
		const bool bOwnRevive = Index == 3 && KeepAttributes->GetHealth() <= 0.f && ExpectedCurrents[Index] > 0.f;
		ASC->SetNumericAttributeBase(Attributes[Index], DesiredBases[Index]);
		if (bOwnRevive) { ++LifeEpoch; }
		if (!StillOwnsAvatar()
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(Attributes[Index]), DesiredBases[Index], .01f)
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attributes[Index]), ExpectedCurrents[Index], .01f)) { return false; }
	}
	if (USovExertionComponent* Exertion = Avatar->FindComponentByClass<USovExertionComponent>()) { Exertion->ResetForCheckpoint(); }
	if (!StillOwnsAvatar()) { return false; }
	if (USovHealthRechargeComponent* Health = Avatar->FindComponentByClass<USovHealthRechargeComponent>()) { Health->ResetForCheckpoint(); }
	if (!StillOwnsAvatar()) { return false; }
	if (USovShieldComponent* Shield = Avatar->FindComponentByClass<USovShieldComponent>()) { Shield->ResetForCheckpoint(); }
	if (!StillOwnsAvatar()) { return false; }
	if (USovPoiseComponent* Poise = Avatar->FindComponentByClass<USovPoiseComponent>()) { Poise->ResetForCheckpoint(); }
	if (!StillOwnsAvatar()) { return false; }
	if (USovEchoComponent* Echo = Avatar->FindComponentByClass<USovEchoComponent>()) { Echo->ResetCheckpointActivity(); }
	if (!StillOwnsAvatar()) { return false; }
	if (bHadStatusComponent)
	{
		// Preserve PR34's queued/readiness boundary. Persistent resource status
		// modifiers remain unsupported until generic Narrative resource saves migrate.
		if (!StatusComponent.IsValid() || Avatar->FindComponentByClass<USovStatusComponent>() != StatusComponent.Get()
			|| !StatusComponent->CompletePendingCheckpointRestore() || !StillOwnsAvatar()) { return false; }
	}
	// Hold release can notify gameplay; validate it before committing success,
	// keeping the same-ASC reentry/capture fence throughout both releases.
	EndPassiveRestore();
	if (!StillOwnsAvatar()) { return false; }
	// A later resource/status callback must not silently undo an earlier write.
	TArray<float> FinalOffsets;
	if (!CurrentResourceOffsets(ASC, Attributes, FinalOffsets, bHasModifiers)) { return false; }
	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		if (!FMath::IsNearlyEqual(FinalOffsets[Index], Offsets[Index], .01f)
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(Maxima[Index]), ExpectedMaxima[Index], .01f)
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttributeBase(Attributes[Index]), DesiredBases[Index], .01f)
			|| !FMath::IsNearlyEqual(ASC->GetNumericAttribute(Attributes[Index]), ExpectedCurrents[Index], .01f)) { return false; }
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
