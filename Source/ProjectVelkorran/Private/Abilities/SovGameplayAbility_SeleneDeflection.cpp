// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneDeflection.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

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
	if (!Super::CanActivateAbility(
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
	Super::OnAvatarSet(ActorInfo, Spec);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	BindDeflectionComponent(
		Avatar ? Avatar->FindComponentByClass<USovDeflectionComponent>() : nullptr);
}

void USovGameplayAbility_SeleneDeflection::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
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
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	BindDeflectionComponent(
		Avatar ? Avatar->FindComponentByClass<USovDeflectionComponent>() : nullptr);
	if (!IsValid(DeflectionComponent)
		|| !DeflectionComponent->BeginDeflection())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bDeflectionStarted = true;
	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReceiveDeflectionAbilityStarted();
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
	UnbindCancellationTags();
	if (IsValid(DeflectionComponent) && bDeflectionStarted)
	{
		DeflectionComponent->EndDeflection();
	}

	RecoveryTask = nullptr;
	const bool bWasDeflectionStarted = bDeflectionStarted;
	bDeflectionStarted = false;
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
}

void USovGameplayAbility_SeleneDeflection::UnbindCancellationTags()
{
	if (BoundAbilitySystem)
	{
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
	DeadTagChangedHandle.Reset();
	FatalTagChangedHandle.Reset();
	PoiseBrokenTagChangedHandle.Reset();
	RagdollTagChangedHandle.Reset();
	SequencerTagChangedHandle.Reset();
}

void USovGameplayAbility_SeleneDeflection::HandleCancellationTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	if (NewCount > 0 && IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			true);
	}
}
