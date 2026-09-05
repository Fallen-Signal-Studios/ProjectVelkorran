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
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void OnAvatarSet(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	virtual void OnRemoveAbility(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

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
	void HandleGuardEnded();

	UFUNCTION()
	void HandleGuardImpact(const FSovDamageResult& Result);

	UFUNCTION()
	void HandlePerfectDefense(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleGuardBroken(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleCounterLanded(const FSovDamageResult& Result);

	void BindGuardComponent(USovGuardComponent* NewGuardComponent);
	void UnbindGuardComponent();
	bool ShouldRunLocalPresentation() const;

	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Guard", meta = (DisplayName = "Guard Ability Started"))
	void ReceiveGuardAbilityStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Guard", meta = (DisplayName = "Guard Ability Ended"))
	void ReceiveGuardAbilityEnded(bool bWasCancelled);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Guard|Presentation", meta = (DisplayName = "Guard Impact"))
	void ReceiveGuardImpact(const FSovDamageResult& Result);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Guard|Presentation", meta = (DisplayName = "Perfect Defense"))
	void ReceivePerfectDefense(const FSovDamageResult& Result);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Guard|Presentation", meta = (DisplayName = "Guard Broken"))
	void ReceiveGuardBroken(const FSovDamageResult& Result);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Guard|Presentation", meta = (DisplayName = "Counter Landed"))
	void ReceiveCounterLanded(const FSovDamageResult& Result);

private:
	UPROPERTY(Transient)
	TObjectPtr<USovGuardComponent> GuardComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	TMap<FGameplayTag, FDelegateHandle> CancellationTagHandles;
	FDelegateHandle BusyTagChangedHandle;
	uint32 ActivationEpoch = 0;
	bool bEndingGuardAbility = false;
	bool bGuardStarted = false;
};
