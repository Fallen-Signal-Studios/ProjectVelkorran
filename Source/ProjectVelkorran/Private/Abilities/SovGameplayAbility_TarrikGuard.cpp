// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikGuard.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Components/SovGuardComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovGameplayAbility_TarrikGuard::USovGameplayAbility_TarrikGuard()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_AltAttack;

	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_IsDead);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Fatal);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Guard_Broken);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Poise_Broken);
}

void USovGameplayAbility_TarrikGuard::ActivateAbility(
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
	GuardComponent = Avatar ? Avatar->FindComponentByClass<USovGuardComponent>() : nullptr;
	if (!IsValid(GuardComponent) || !GuardComponent->BeginGuard())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bGuardStarted = true;
	GuardComponent->OnGuardBroken.AddUniqueDynamic(this, &ThisClass::HandleGuardBroken);
	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReceiveGuardAbilityStarted();

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void USovGameplayAbility_TarrikGuard::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	UnbindCancellationTags();
	if (IsValid(GuardComponent))
	{
		GuardComponent->OnGuardBroken.RemoveDynamic(this, &ThisClass::HandleGuardBroken);
		if (bGuardStarted)
		{
			GuardComponent->EndGuard();
		}
	}

	InputReleaseTask = nullptr;
	GuardComponent = nullptr;
	const bool bWasGuardStarted = bGuardStarted;
	bGuardStarted = false;
	if (bWasGuardStarted)
	{
		ReceiveGuardAbilityEnded(bWasCancelled);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USovGameplayAbility_TarrikGuard::HandleInputReleased(const float TimeHeld)
{
	static_cast<void>(TimeHeld);
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void USovGameplayAbility_TarrikGuard::HandleGuardBroken(const FSovDamageResult& Result)
{
	static_cast<void>(Result);
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void USovGameplayAbility_TarrikGuard::BindCancellationTags(
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
		->RegisterGameplayTagEvent(NarrativeTags.State_IsDead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	PoiseBrokenTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(SovTags.State_Poise_Broken, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	SequencerTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(NarrativeTags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
}

void USovGameplayAbility_TarrikGuard::UnbindCancellationTags()
{
	if (BoundAbilitySystem)
	{
		const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
		const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
		BoundAbilitySystem
			->RegisterGameplayTagEvent(NarrativeTags.State_IsDead, EGameplayTagEventType::NewOrRemoved)
			.Remove(DeadTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(SovTags.State_Poise_Broken, EGameplayTagEventType::NewOrRemoved)
			.Remove(PoiseBrokenTagChangedHandle);
		BoundAbilitySystem
			->RegisterGameplayTagEvent(NarrativeTags.State_SequencerControlled, EGameplayTagEventType::NewOrRemoved)
			.Remove(SequencerTagChangedHandle);
	}

	BoundAbilitySystem = nullptr;
	DeadTagChangedHandle.Reset();
	PoiseBrokenTagChangedHandle.Reset();
	SequencerTagChangedHandle.Reset();
}

void USovGameplayAbility_TarrikGuard::HandleCancellationTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	if (NewCount > 0 && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
