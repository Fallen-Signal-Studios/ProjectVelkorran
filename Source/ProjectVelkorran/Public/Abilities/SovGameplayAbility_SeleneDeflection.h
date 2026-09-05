// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/SovCombatTypes.h"
#include "SovGameplayAbility_SeleneDeflection.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitDelay;
class ASovTransformingWeaponVisual;
class USovDeflectionComponent;

/** Tap defense that opens Selene's brief precision-Deflection window. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneDeflection
	: public UNarrativeGameplayAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneDeflection();

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

	/** Keeps the ability instance alive briefly after the 0.11 s defense window for authored recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Deflection|Tuning", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DeflectionRecoveryDuration = 0.35f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Deflection", meta = (DisplayName = "Deflection Ability Started"))
	void ReceiveDeflectionAbilityStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Deflection", meta = (DisplayName = "Deflection Ability Ended"))
	void ReceiveDeflectionAbilityEnded(bool bWasCancelled);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Deflection|Presentation", meta = (DisplayName = "Perfect Deflection"))
	void ReceivePerfectDeflection(const FSovDamageResult& Result);

private:
	void BindDeflectionComponent(USovDeflectionComponent* NewDeflectionComponent);
	void UnbindDeflectionComponent();
	bool ShouldRunLocalPresentation() const;
	void PlayDeflectionWeaponMontage();
	void StopDeflectionWeaponMontage();

	UFUNCTION()
	void HandlePerfectDeflection(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleRecoveryFinished();

	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<USovDeflectionComponent> DeflectionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> RecoveryTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	/** Exact visual that accepted this activation, retained for cancellation cleanup. */
	UPROPERTY(Transient)
	TObjectPtr<ASovTransformingWeaponVisual> ActiveDeflectionWeaponVisual;

	FDelegateHandle DeadTagChangedHandle;
	FDelegateHandle FatalTagChangedHandle;
	FDelegateHandle PoiseBrokenTagChangedHandle;
	FDelegateHandle RagdollTagChangedHandle;
	FDelegateHandle SequencerTagChangedHandle;
	TWeakObjectPtr<USovDeflectionComponent> ActiveDeflectionComponent;
	uint32 ActivationEpoch = 0;
	bool bEndingDeflectionAbility = false;
	bool bDeflectionStarted = false;
};
