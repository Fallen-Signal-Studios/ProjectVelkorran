// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneDeflection.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Weapons/SovTransformingWeaponVisual.h"

USovGameplayAbility_SeleneDeflection::USovGameplayAbility_SeleneDeflection()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_AltAttack;

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	ActivationBlockedTags.AddTag(NarrativeTags.State_IsDead);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Busy);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Interacting);
	ActivationBlockedTags.AddTag(NarrativeTags.State_SequencerControlled);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_Equipping);
	ActivationBlockedTags.AddTag(SovTags.State_Fatal);
	ActivationBlockedTags.AddTag(SovTags.State_Guarding);
	ActivationBlockedTags.AddTag(SovTags.State_Guard_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_Poise_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_EchoAbility_Active);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Busy);

	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(SovTags.Ability_Defense_Selene_Deflection);
	SetAssetTags(AssetTags);
}

bool USovGameplayAbility_SeleneDeflection::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (bEndingDeflection || !FMath::IsFinite(DeflectionRecoveryDuration) || DeflectionRecoveryDuration < 0.f
		|| !Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (!AbilitySystem
		|| !Avatar
		|| AbilitySystem->GetAvatarActor() != Avatar
		|| !AbilitySystem->HasMatchingGameplayTag(
			SovTags.Character_Player_Selene)
		|| AbilitySystem->HasMatchingGameplayTag(
			SovTags.Character_Player_Tarrik)
		|| Avatar->FindComponentByClass<USovDeflectionComponent>() == nullptr)
	{
		return false;
	}

	// The ASC lives on PlayerState and may outlive a pawn. Reject a stale or
	// future sibling protagonist identity instead of treating Selene's tag as
	// sufficient on its own.
	FGameplayTagContainer OwnedTags;
	AbilitySystem->GetOwnedGameplayTags(OwnedTags);
	for (const FGameplayTag& OwnedTag : OwnedTags)
	{
		if (OwnedTag != SovTags.Character_Player
			&& OwnedTag != SovTags.Character_Player_Selene
			&& OwnedTag.MatchesTag(SovTags.Character_Player))
		{
			return false;
		}
	}

	return true;
}

void USovGameplayAbility_SeleneDeflection::OnAvatarSet(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
	Super::OnAvatarSet(ActorInfo, Spec);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	BindDeflectionComponent(
		Avatar ? Avatar->FindComponentByClass<USovDeflectionComponent>() : nullptr);
}

void USovGameplayAbility_SeleneDeflection::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
	StopDeflectionWeaponMontage();
	UnbindCancellationTags();
	UnbindDeflectionComponent();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void USovGameplayAbility_SeleneDeflection::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint64 Epoch = ++ActivationEpoch;
	ActionAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	ActionASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (Epoch != ActivationEpoch || !IsActive()) { return; }
	if (!ActorInfo || !OwnsActivation(Epoch))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	BindDeflectionComponent(
		Avatar ? Avatar->FindComponentByClass<USovDeflectionComponent>() : nullptr);
	if (!OwnsActivation(Epoch))
	{
		if (Epoch == ActivationEpoch && IsActive())
		{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); }
		return;
	}
	if (!IsValid(DeflectionComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Claim cleanup before the component's synchronous start/tag callbacks.
	bDeflectionStarted = true;
	OwnedDeflectionWindowEpoch = 0;
	ActiveDeflectionComponent = DeflectionComponent;
	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	const int32 OwnedBusy = ActivationOwnedTags.HasTag(FNarrativeGameplayTags::Get().State_Busy) ? 1 : 0;
	const bool bBegan = DeflectionComponent->BeginDeflectionInternal(OwnedBusy, &OwnedDeflectionWindowEpoch);
	if (Epoch != ActivationEpoch || !IsActive()) { return; }
	if (!bBegan || !OwnsActivation(Epoch))
	{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!OwnsActivation(Epoch))
	{
		if (Epoch == ActivationEpoch && IsActive())
		{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); }
		return;
	}

	PlayDeflectionWeaponMontage();
	if (!OwnsActivation(Epoch))
	{
		if (Epoch == ActivationEpoch && IsActive())
		{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); }
		return;
	}
	ReceiveDeflectionAbilityStarted();
	if (!OwnsActivation(Epoch))
	{
		if (Epoch == ActivationEpoch && IsActive())
		{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); }
		return;
	}
	if (DeflectionRecoveryDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		DeflectionRecoveryDuration);
	if (RecoveryTask)
	{
		RecoveryTask->OnFinish.AddDynamic(
			this,
			&ThisClass::HandleRecoveryFinished);
		RecoveryTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void USovGameplayAbility_SeleneDeflection::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (bEndingDeflection || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	if (ScopeLockCount > 0)
	{
		++ActivationEpoch;
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	TGuardValue<bool> Ending(bEndingDeflection, true);
	++ActivationEpoch;
	UnbindCancellationTags();
	const uint64 WindowToClose = OwnedDeflectionWindowEpoch;
	USovDeflectionComponent* ComponentToClose = ActiveDeflectionComponent.Get();
	ActiveDeflectionComponent.Reset();
	const bool bWasDeflectionStarted = bDeflectionStarted && WindowToClose != 0;
	bDeflectionStarted = false;
	OwnedDeflectionWindowEpoch = 0;
	ActionAvatar.Reset(); ActionASC.Reset();
	if (RecoveryTask)
	{
		RecoveryTask->OnFinish.RemoveDynamic(this, &ThisClass::HandleRecoveryFinished);
		RecoveryTask->EndTask(); RecoveryTask = nullptr;
	}
	if (bWasCancelled)
	{
		StopDeflectionWeaponMontage();
	}
	else
	{
		// A normal recovery end does not cut off a longer authored spin.
		ActiveDeflectionWeaponVisual = nullptr;
	}
	if (IsValid(ComponentToClose) && bWasDeflectionStarted)
	{
		ComponentToClose->EndOwnedDeflectionWindow(WindowToClose);
	}

	if (bWasDeflectionStarted)
	{
		ReceiveDeflectionAbilityEnded(bWasCancelled);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void USovGameplayAbility_SeleneDeflection::BindDeflectionComponent(
	USovDeflectionComponent* NewDeflectionComponent)
{
	if (DeflectionComponent != NewDeflectionComponent)
	{
		if (IsActive() && bDeflectionStarted)
		{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
		UnbindDeflectionComponent();
		DeflectionComponent = NewDeflectionComponent;
	}

	if (IsValid(DeflectionComponent))
	{
		DeflectionComponent->OnPerfectDeflection.AddUniqueDynamic(
			this,
			&ThisClass::HandlePerfectDeflection);
	}
}

void USovGameplayAbility_SeleneDeflection::UnbindDeflectionComponent()
{
	if (IsValid(DeflectionComponent))
	{
		DeflectionComponent->OnPerfectDeflection.RemoveDynamic(
			this,
			&ThisClass::HandlePerfectDeflection);
	}

	DeflectionComponent = nullptr;
}

bool USovGameplayAbility_SeleneDeflection::ShouldRunLocalPresentation() const
{
	return CurrentActorInfo && CurrentActorInfo->IsLocallyControlled();
}

void USovGameplayAbility_SeleneDeflection::PlayDeflectionWeaponMontage()
{
	ActiveDeflectionWeaponVisual = nullptr;
	ANarrativeCharacter* NarrativeCharacter = GetOwningNarrativeCharacter();
	ASovTransformingWeaponVisual* WeaponVisual = NarrativeCharacter
		? Cast<ASovTransformingWeaponVisual>(
			NarrativeCharacter->GetWieldedWeaponVisual(true))
		: nullptr;
	if (IsValid(WeaponVisual))
	{
		const uint64 Epoch = ActivationEpoch;
		// Montage events may cancel; make the exact visual available to EndAbility first.
		ActiveDeflectionWeaponVisual = WeaponVisual;
		const bool bPlayed = WeaponVisual->PlayDeflectionWeaponMontage();
		if (!bPlayed && Epoch == ActivationEpoch) { ActiveDeflectionWeaponVisual = nullptr; }
	}
}

void USovGameplayAbility_SeleneDeflection::StopDeflectionWeaponMontage()
{
	if (IsValid(ActiveDeflectionWeaponVisual))
	{
		ActiveDeflectionWeaponVisual->StopDeflectionWeaponMontage();
	}
	ActiveDeflectionWeaponVisual = nullptr;
}

void USovGameplayAbility_SeleneDeflection::HandlePerfectDeflection(
	const FSovDamageResult& Result)
{
	if (Result.DefenseKind == ESovDefenseKind::Deflection
		&& ShouldRunLocalPresentation())
	{
		ReceivePerfectDeflection(Result);
	}
}

void USovGameplayAbility_SeleneDeflection::HandleRecoveryFinished()
{
	if (IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			false);
	}
}

void USovGameplayAbility_SeleneDeflection::BindCancellationTags(
	UAbilitySystemComponent* AbilitySystem)
{
	UnbindCancellationTags();
	if (!AbilitySystem)
	{
		return;
	}

	BoundAbilitySystem = AbilitySystem;
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DeadTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(
			NarrativeTags.State_IsDead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	FatalTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(
			SovTags.State_Fatal,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	PoiseBrokenTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(
			SovTags.State_Poise_Broken,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	RagdollTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(
			NarrativeTags.State_Movement_Ragdoll,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	SequencerTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(
			NarrativeTags.State_SequencerControlled,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	const FGameplayTag Extra[] = {NarrativeTags.State_Interacting, NarrativeTags.State_Weapon_Equipping, NarrativeTags.State_Busy,
		SovTags.State_Guarding, SovTags.State_Guard_Broken, SovTags.State_EchoAbility_Active, SovTags.State_Status_Frozen};
	for (const auto& Tag : Extra)
	{
		AdditionalCancellationHandles.Add(Tag, BoundAbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleCancellationTagChanged));
	}
}

void USovGameplayAbility_SeleneDeflection::UnbindCancellationTags()
{
	if (BoundAbilitySystem)
	{
		for (const auto& Pair : AdditionalCancellationHandles)
		{ BoundAbilitySystem->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::AnyCountChange).Remove(Pair.Value); }
		const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
		const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				NarrativeTags.State_IsDead,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(DeadTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				SovTags.State_Fatal,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(FatalTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				SovTags.State_Poise_Broken,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(PoiseBrokenTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				NarrativeTags.State_Movement_Ragdoll,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(RagdollTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				NarrativeTags.State_SequencerControlled,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(SequencerTagChangedHandle);
	}

	BoundAbilitySystem = nullptr;
	AdditionalCancellationHandles.Reset();
	DeadTagChangedHandle.Reset();
	FatalTagChangedHandle.Reset();
	PoiseBrokenTagChangedHandle.Reset();
	RagdollTagChangedHandle.Reset();
	SequencerTagChangedHandle.Reset();
}

bool USovGameplayAbility_SeleneDeflection::OwnsActivation(const uint64 Epoch) const
{
	return Epoch == ActivationEpoch && IsActive() && !bEndingDeflection && ActionAvatar.IsValid() && ActionASC.IsValid()
		&& !ActionAvatar->IsActorBeingDestroyed() && CurrentActorInfo
		&& CurrentActorInfo->AvatarActor == ActionAvatar && CurrentActorInfo->AbilitySystemComponent == ActionASC
		&& ActionASC->GetAvatarActor() == ActionAvatar.Get();
}

void USovGameplayAbility_SeleneDeflection::HandleCancellationTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	const int32 OwnedCount = CallbackTag == FNarrativeGameplayTags::Get().State_Busy
		&& ActivationOwnedTags.HasTag(CallbackTag) ? 1 : 0;
	if (NewCount > OwnedCount && IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			true);
	}
}
