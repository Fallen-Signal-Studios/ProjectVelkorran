// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/SovCombatTypes.h"
#include "SovGameplayAbility_TarrikGuard.generated.h"

class UAbilityTask_WaitInputRelease;
class UAbilitySystemComponent;
class USovGuardComponent;

/** Hold-to-guard GAS entry point for Tarrik's first defense vertical slice. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikGuard : public UNarrativeGameplayAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikGuard();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleGuardBroken(const FSovDamageResult& Result);

	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Guard", meta = (DisplayName = "Guard Ability Started"))
	void ReceiveGuardAbilityStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Guard", meta = (DisplayName = "Guard Ability Ended"))
	void ReceiveGuardAbilityEnded(bool bWasCancelled);

private:
	UPROPERTY(Transient)
	TObjectPtr<USovGuardComponent> GuardComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	FDelegateHandle DeadTagChangedHandle;
	FDelegateHandle PoiseBrokenTagChangedHandle;
	FDelegateHandle SequencerTagChangedHandle;
	bool bGuardStarted = false;
};
