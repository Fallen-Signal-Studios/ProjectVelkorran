// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikGuard.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/SovGuardComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"

USovGameplayAbility_TarrikGuard::USovGameplayAbility_TarrikGuard()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_AltAttack;
}

void USovGameplayAbility_TarrikGuard::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !CommitAbility(Handle, ActorInfo, ActivationInfo))
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

	GuardComponent->OnGuardBroken.AddUniqueDynamic(this, &ThisClass::HandleGuardBroken);
	ReceiveGuardAbilityStarted();

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (InputReleaseTask)
	{
		InputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
}

void USovGameplayAbility_TarrikGuard::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (IsValid(GuardComponent))
	{
		GuardComponent->OnGuardBroken.RemoveDynamic(this, &ThisClass::HandleGuardBroken);
		GuardComponent->EndGuard();
	}

	InputReleaseTask = nullptr;
	GuardComponent = nullptr;
	ReceiveGuardAbilityEnded(bWasCancelled);
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
