// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GameplayEffect.h"
#include "SovGameplayAbility_Exertion.generated.h"

class ASovPlayerCharacterBase;
class USovExertionComponent;
class UAbilityTask_WaitInputRelease;

/** Timed owned tags, with no competing movement or resource effect. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_ExertionWindow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_ExertionWindow();
};

/** Native capsule-swept evade. Optional presentation must use in-place animation. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayAbility_Evade : public UNarrativeGameplayAbility
{
	GENERATED_BODY()
public:
	USovGameplayAbility_Evade();
	/** Validate a transition without removing the current attack's Busy tag or spending either cost. */
	bool CanActivateAfterCombatCancel(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		class UNarrativeCombatAbility* CancellingAttack, float PendingCancelCost) const;
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Evade") void ReceiveEvadeStarted(FVector Direction, float Duration);
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Evade") void ReceiveEvadeEnded(bool bInterrupted);
private:
	bool ResolveEvade(const FGameplayAbilityActorInfo* ActorInfo, FVector& Direction, float& Distance) const;
	void MonitorEvade();
	void HandleInterruptTag(FGameplayTag Tag, int32 Count);
	bool HasValidSource() const;
	TWeakObjectPtr<ASovPlayerCharacterBase> EvadingCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> EvadingASC;
	FActiveGameplayEffectHandle InvulnerabilityHandle;
	FActiveGameplayEffectHandle BusyHandle;
	TMap<FGameplayTag, FDelegateHandle> InterruptHandles;
	FTimerHandle MonitorHandle;
	uint16 RootMotionSourceID = 0;
	bool bOwnsRootMotionSource = false;
	uint32 EvadeEpoch = 0;
	double EndWorldTime = 0.;
	bool bEnding = false;
	bool bPresented = false;
	bool bOwnedMovementRotation = false;
	bool bPreviousOrientToMovement = false;
	bool bPreviousControllerDesiredRotation = false;
};

/** Uses Narrative's replicated sprint intent; resource drain belongs to Exertion. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayAbility_Sprint : public UNarrativeGameplayAbility
{
	GENERATED_BODY()
public:
	USovGameplayAbility_Sprint();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void HandleReleased(float HeldTime);
	void MonitorSprint();
	TWeakObjectPtr<ASovPlayerCharacterBase> SprintingCharacter;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitInputRelease> ReleaseTask;
	FTimerHandle MonitorHandle;
	bool bEnding = false;
};
