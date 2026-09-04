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
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Interacting);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Fatal);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_EchoAbility_Active);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Guard_Broken);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Poise_Broken);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Deflecting);
}

bool USovGameplayAbility_TarrikGuard::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return !bEndingGuardAbility
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void USovGameplayAbility_TarrikGuard::OnAvatarSet(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	BindGuardComponent(
		Avatar ? Avatar->FindComponentByClass<USovGuardComponent>() : nullptr);
}

void USovGameplayAbility_TarrikGuard::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	UnbindCancellationTags();
	UnbindGuardComponent();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void USovGameplayAbility_TarrikGuard::BindGuardComponent(
	USovGuardComponent* NewGuardComponent)
{
	if (GuardComponent != NewGuardComponent)
	{
		if (IsActive() && bGuardStarted)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
		UnbindGuardComponent();
		GuardComponent = NewGuardComponent;
	}

	if (IsValid(GuardComponent))
	{
		GuardComponent->OnGuardEnded.AddUniqueDynamic(this, &ThisClass::HandleGuardEnded);
		GuardComponent->OnGuardImpact.AddUniqueDynamic(
			this,
			&ThisClass::HandleGuardImpact);
		GuardComponent->OnPerfectDefense.AddUniqueDynamic(
			this,
			&ThisClass::HandlePerfectDefense);
		GuardComponent->OnGuardBroken.AddUniqueDynamic(
			this,
			&ThisClass::HandleGuardBroken);
		GuardComponent->OnCounterLanded.AddUniqueDynamic(
			this,
			&ThisClass::HandleCounterLanded);
	}
}

void USovGameplayAbility_TarrikGuard::UnbindGuardComponent()
{
	if (IsValid(GuardComponent))
	{
		GuardComponent->OnGuardEnded.RemoveDynamic(this, &ThisClass::HandleGuardEnded);
		GuardComponent->OnGuardImpact.RemoveDynamic(
			this,
			&ThisClass::HandleGuardImpact);
		GuardComponent->OnPerfectDefense.RemoveDynamic(
			this,
			&ThisClass::HandlePerfectDefense);
		GuardComponent->OnGuardBroken.RemoveDynamic(
			this,
			&ThisClass::HandleGuardBroken);
		GuardComponent->OnCounterLanded.RemoveDynamic(
			this,
			&ThisClass::HandleCounterLanded);
	}

	GuardComponent = nullptr;
}

bool USovGameplayAbility_TarrikGuard::ShouldRunLocalPresentation() const
{
	return CurrentActorInfo && CurrentActorInfo->IsLocallyControlled();
}

void USovGameplayAbility_TarrikGuard::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint32 Epoch = ++ActivationEpoch;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (Epoch != ActivationEpoch || !IsActive()) { return; }

	if (!ActorInfo)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	BindGuardComponent(
		Avatar ? Avatar->FindComponentByClass<USovGuardComponent>() : nullptr);
	if (Epoch != ActivationEpoch || !IsActive()) { return; }
	if (!IsValid(GuardComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// BeginGuard broadcasts synchronously. Claim the lifecycle before that event
	// so cancellation from Blueprint can unwind both GAS and component state.
	bGuardStarted = true;
	const int32 OwnedBusy = ActivationOwnedTags.HasTag(FNarrativeGameplayTags::Get().State_Busy) ? 1 : 0;
	const bool bBeganGuard = GuardComponent->BeginGuardInternal(OwnedBusy);
	if (Epoch != ActivationEpoch || !IsActive()) { return; }
	if (!bBeganGuard)
	{
		bGuardStarted = false;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	if (Epoch != ActivationEpoch || !IsActive()) { return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Epoch != ActivationEpoch || !IsActive()) { return; }

	ReceiveGuardAbilityStarted();
	if (Epoch != ActivationEpoch || !IsActive()) { return; }

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
	if (bEndingGuardAbility || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	if (ScopeLockCount > 0)
	{
		// Stop native activation continuation even when GAS must defer teardown.
		++ActivationEpoch;
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	TGuardValue<bool> EndingGuardAbility(bEndingGuardAbility, true);
	++ActivationEpoch;
	UnbindCancellationTags();
	const bool bWasGuardStarted = bGuardStarted;
	bGuardStarted = false;
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.RemoveDynamic(this, &ThisClass::HandleInputReleased);
		InputReleaseTask->EndTask();
		InputReleaseTask = nullptr;
	}
	if (IsValid(GuardComponent) && bWasGuardStarted)
	{
		GuardComponent->EndGuard();
	}

	if (bWasGuardStarted)
	{
		ReceiveGuardAbilityEnded(bWasCancelled);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USovGameplayAbility_TarrikGuard::HandleGuardEnded()
{
	if (bGuardStarted && IsActive() && !bEndingGuardAbility)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void USovGameplayAbility_TarrikGuard::HandleInputReleased(const float TimeHeld)
{
	static_cast<void>(TimeHeld);
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void USovGameplayAbility_TarrikGuard::HandleGuardImpact(
	const FSovDamageResult& Result)
{
	if (ShouldRunLocalPresentation())
	{
		ReceiveGuardImpact(Result);
	}
}

void USovGameplayAbility_TarrikGuard::HandlePerfectDefense(
	const FSovDamageResult& Result)
{
	if (ShouldRunLocalPresentation())
	{
		ReceivePerfectDefense(Result);
	}
}

void USovGameplayAbility_TarrikGuard::HandleGuardBroken(
	const FSovDamageResult& Result)
{
	const uint32 Epoch = ActivationEpoch;
	if (ShouldRunLocalPresentation())
	{
		ReceiveGuardBroken(Result);
	}

	if (Epoch == ActivationEpoch && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void USovGameplayAbility_TarrikGuard::HandleCounterLanded(
	const FSovDamageResult& Result)
{
	if (ShouldRunLocalPresentation())
	{
		ReceiveCounterLanded(Result);
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
	FGameplayTagContainer Tags;
	Tags.AddTag(NarrativeTags.State_IsDead);
	Tags.AddTag(NarrativeTags.State_Interacting);
	Tags.AddTag(NarrativeTags.State_SequencerControlled);
	Tags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	Tags.AddTag(SovTags.State_Fatal);
	Tags.AddTag(SovTags.State_Poise_Broken);
	Tags.AddTag(SovTags.State_Guard_Broken);
	Tags.AddTag(SovTags.State_EchoAbility_Active);
	Tags.AddTag(SovTags.State_Deflecting);
	for (const FGameplayTag& Tag : Tags)
	{
		CancellationTagHandles.Add(Tag, BoundAbilitySystem
			->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCancellationTagChanged));
	}
	BusyTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(NarrativeTags.State_Busy, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
	const int32 OwnedBusy = ActivationOwnedTags.HasTag(NarrativeTags.State_Busy) ? 1 : 0;
	if (BoundAbilitySystem->HasAnyMatchingGameplayTags(Tags)
		|| BoundAbilitySystem->GetGameplayTagCount(NarrativeTags.State_Busy) > OwnedBusy)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void USovGameplayAbility_TarrikGuard::UnbindCancellationTags()
{
	if (IsValid(BoundAbilitySystem))
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : CancellationTagHandles)
		{
			BoundAbilitySystem->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::NewOrRemoved)
				.Remove(Entry.Value);
		}
		BoundAbilitySystem
			->RegisterGameplayTagEvent(FNarrativeGameplayTags::Get().State_Busy, EGameplayTagEventType::AnyCountChange)
			.Remove(BusyTagChangedHandle);
	}

	BoundAbilitySystem = nullptr;
	CancellationTagHandles.Reset();
	BusyTagChangedHandle.Reset();
}

void USovGameplayAbility_TarrikGuard::HandleCancellationTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	const FGameplayTag BusyTag = FNarrativeGameplayTags::Get().State_Busy;
	const int32 OwnedBusy = ActivationOwnedTags.HasTag(BusyTag) ? 1 : 0;
	const int32 AllowedCount = CallbackTag == BusyTag ? OwnedBusy : 0;
	if (NewCount > AllowedCount && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
