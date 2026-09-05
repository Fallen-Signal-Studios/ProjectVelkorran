// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/SovCombatTypes.h"
#include "SovGameplayAbility_FieldRecovery.generated.h"
class USovFieldRecoveryComponent;
class UNarrativeAbilitySystemComponent;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayAbility_FieldRecovery : public UNarrativeGameplayAbility
{
	GENERATED_BODY()
public:
	USovGameplayAbility_FieldRecovery();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Field Recovery") TObjectPtr<UAnimMontage> RecoveryMontage;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* RelevantTags = nullptr) const override;
protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled) override;
	UFUNCTION(BlueprintImplementableEvent, Category="Field Recovery") void OnRecoveryStarted(float Duration);
	UFUNCTION(BlueprintImplementableEvent, Category="Field Recovery") void OnRecoveryFinished(bool bHealed, float HealthRestored);
private:
	friend struct FSovFieldRecoveryTestAccess;
	friend class USovFieldRecoveryComponent;
	bool ContextValid() const;
	void Poll();
	void HandleInterrupt(FGameplayTag Tag, int32 Count);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	UFUNCTION() void HandleDamage(const FSovDamageResult& Result);
	UFUNCTION() void HandleMontageInterrupted();
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	TWeakObjectPtr<USovFieldRecoveryComponent> Recovery;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<AActor> BoundAvatar;
	TWeakObjectPtr<AController> BoundController;
	TMap<FGameplayTag, FDelegateHandle> InterruptHandles;
	FDelegateHandle HealthChangedHandle;
	FTimerHandle PollTimer;
	FGuid ChargeUse;
	uint64 Epoch = 0;
	uint64 RecoveryStateEpoch = 0;
	int32 ReadyEpoch = 0;
	bool bEnding = false;
	bool bEndPending = false;
	bool bPresented = false;
	bool bCompleted = false;
	float CompletedHeal = 0.f;
};
