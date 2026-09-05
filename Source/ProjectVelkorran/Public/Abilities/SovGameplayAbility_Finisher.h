// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "SovGameplayAbility_Finisher.generated.h"
class USovFinisherTargetComponent;
class UAnimMontage;
class UAnimInstance;
class UAbilityTask_WaitGameplayEvent;
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_FinisherProtection : public UGameplayEffect
{
    GENERATED_BODY()
public: USovGameplayEffect_FinisherProtection();
};
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_FinisherDamage : public UGameplayEffect
{
    GENERATED_BODY()
public: USovGameplayEffect_FinisherDamage();
};
/** Server-owned finite Narrative combat action. Montage events can strike early; the native fallback always terminates. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayAbility_Finisher : public UNarrativeCombatAbility
{
    GENERATED_BODY()
public:
    USovGameplayAbility_Finisher();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
        FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;
    UFUNCTION(BlueprintPure, Category="Finisher") AActor* FindFinisherTarget() const;
    uint64 GetFinisherActivationEpoch() const { return ActionEpoch; }
    bool IsFinisherActivationCurrent(uint64 Epoch, const AActor* Attacker) const;
protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher") TObjectPtr<UAnimMontage> FinisherMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher") FName MontageSection;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="0.8", ClampMax="1.8")) float SequenceDuration=1.2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="50", ClampMax="300")) float MaximumDistance=220.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="0", ClampMax="75")) float MaximumFacingAngle=60.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="0")) float FallbackStrikeDamage=25.f;
    /** Optional authored exit offset from the player's current safe pose; activation never teleports across a hazard. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Finisher") FVector ExitOffset=FVector::ZeroVector;
    UFUNCTION(BlueprintImplementableEvent, Category="Finisher") void OnFinisherStarted(AActor* Target, bool bCinematicAligned);
    UFUNCTION(BlueprintImplementableEvent, Category="Finisher") void OnFinisherResolved(AActor* Target, bool bPhaseOutcome);
private:
    friend struct FSovFinisherOutcomeTestAccess;
    friend struct FSovPlayerCombatRepairTestAccess;
    bool SourceValid() const;
    bool OwnsAction(const FGuid& ExpectedLease) const;
    void FinishReservedAction(const FGuid& ExpectedLease);
    bool TargetInReach(AActor* Target) const;
    bool AlignmentSafe(AActor* Target) const;
    bool ApplyProtection();
    void Strike();
    void CheckAction();
    void FinishAction();
    UFUNCTION() void OnStrikeEvent(FGameplayEventData Payload);
    TWeakObjectPtr<AActor> ActionAvatar;
    TWeakObjectPtr<UAbilitySystemComponent> ActionASC;
    TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
    UPROPERTY(Transient) TObjectPtr<USovFinisherTargetComponent> TargetComponent;
    UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> StrikeTask;
    FGuid Lease;
    FActiveGameplayEffectHandle PlayerProtection;
    FActiveGameplayEffectHandle TargetProtection;
    FTimerHandle StrikeTimer, FinishTimer, CheckTimer;
    uint64 ActionEpoch=0;
    bool bEndPending=false;
    bool bEnding=false;
    bool bAligned=false;
    bool bStruck=false;
};
