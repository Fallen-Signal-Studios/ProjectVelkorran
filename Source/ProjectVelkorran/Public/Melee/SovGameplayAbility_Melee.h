// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "SovGameplayAbility_Melee.generated.h"
class USovMeleeAttackDefinition;
class USovAbilityTask_MeleeSweep;
class USovEchoAttackReceipt;
class UWeaponItem;
class USkeletalMeshComponent;
class UNarrativeAttributeSetBase;
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_MeleeDamage : public UGameplayEffect
{
    GENERATED_BODY()
public: USovGameplayEffect_MeleeDamage();
};
/** Finite native melee chain. Author data/montages and presentation; collision and payment remain native. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayAbility_Melee : public UNarrativeCombatAbility
{
    GENERATED_BODY()
public:
    USovGameplayAbility_Melee();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
        const FGameplayTagContainer* SourceTags=nullptr,const FGameplayTagContainer* TargetTags=nullptr,FGameplayTagContainer* Relevant=nullptr) const override;
    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,const FGameplayAbilityActivationInfo Activation) override;
    bool IsCurrentNodeHeavy() const;
protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
        const FGameplayAbilityActivationInfo Activation,const FGameplayEventData* Event) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
        const FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled) override;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Melee") TObjectPtr<USovMeleeAttackDefinition> AttackDefinition;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Melee") bool bAllowUnarmed=false;
    UFUNCTION(BlueprintNativeEvent,BlueprintPure,Category="Melee") USkeletalMeshComponent* ResolveMeleeTraceMesh() const;
    virtual USkeletalMeshComponent* ResolveMeleeTraceMesh_Implementation() const;
    UFUNCTION(BlueprintImplementableEvent,Category="Melee") void OnMeleeNodeStarted(int32 Node,bool bCharging);
    UFUNCTION(BlueprintImplementableEvent,Category="Melee") void OnMeleeImpact(const FHitResult& Hit,AActor* Target,bool bDamageAccepted);
    UFUNCTION(BlueprintImplementableEvent,Category="Melee") void OnMeleeEnvironmentContact(const FHitResult& Hit);
private:
    friend struct FSovMeleeRuntimeTestAccess;
    bool ContextValid() const;
    void BindInterruptions();
    void UnbindInterruptions();
    void HandleInterruption(FGameplayTag Tag,int32 Count);
    bool NodeGeometryValid() const;
    bool BeginNode(int32 Index);
    void ReleaseCharge();
    void StartAttackWindow();
    void ApplyAimCorrection();
    void FinishMelee();
    UFUNCTION() void OnContact(const FHitResult& Hit,AActor* Target);
    UFUNCTION() void OnEnvironment(const FHitResult& Hit,AActor* Target);
    UFUNCTION() void OnStep(float Elapsed);
    UFUNCTION() void OnNodeFinished();
    UFUNCTION() void OnTaskInvalidated();
    UPROPERTY(Transient) TObjectPtr<USovAbilityTask_MeleeSweep> SweepTask;
    UPROPERTY(Transient) TObjectPtr<USovEchoAttackReceipt> AttackReceipt;
    TWeakObjectPtr<UAbilitySystemComponent> ActionASC;
    TWeakObjectPtr<AActor> ActionAvatar;
    TWeakObjectPtr<UWeaponItem> ActionWeapon;
    TWeakObjectPtr<USkeletalMeshComponent> ActionMesh;
    FGuid InputWindow;
    FTimerHandle ChargeTimer;
    TMap<FGameplayTag,FDelegateHandle> InterruptionHandles;
    uint64 MeleeActivationEpoch=0;
    uint64 ActionActorInfoEpoch=0;
    uint64 ActionLifeEpoch=0;
    TWeakObjectPtr<const UNarrativeAttributeSetBase> ActionAttributes;
    bool bMeleeEndPending=false;
    bool bEndingMelee=false;
    int32 NodeIndex=INDEX_NONE;
    float ChargeStarted=0.f;
    float ChargeScalar=1.f;
    bool bCharging=false;
    bool bHitConfirmed=false;
    bool bStartedUnarmed=false;
};
