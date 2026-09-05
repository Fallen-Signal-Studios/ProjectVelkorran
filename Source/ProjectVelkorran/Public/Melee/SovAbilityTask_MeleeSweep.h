// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "SovAbilityTask_MeleeSweep.generated.h"
class USkeletalMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovMeleeContact,const FHitResult&,Hit,AActor*,DamageTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovMeleeStep,float,ElapsedSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovMeleeFinished);
/** Swept collision follows actual socket transforms. Each task is one attack node and owns one hit ledger. */
UCLASS()
class PROJECTVELKORRAN_API USovAbilityTask_MeleeSweep : public UAbilityTask
{
    GENERATED_BODY()
public:
    USovAbilityTask_MeleeSweep();
    UPROPERTY(BlueprintAssignable) FSovMeleeContact OnContact;
    UPROPERTY(BlueprintAssignable) FSovMeleeContact OnEnvironmentContact;
    UPROPERTY(BlueprintAssignable) FSovMeleeStep OnStep;
    UPROPERTY(BlueprintAssignable) FSovMeleeFinished OnFinished;
    UPROPERTY(BlueprintAssignable) FSovMeleeFinished OnInvalidated;
    UFUNCTION(BlueprintCallable,Category="Ability|Tasks",meta=(HidePin="OwningAbility",DefaultToSelf="OwningAbility",BlueprintInternalUseOnly="true"))
    static USovAbilityTask_MeleeSweep* SweepMeleeSockets(UGameplayAbility* OwningAbility,USkeletalMeshComponent* TraceMesh,const FSovMeleeAttackNode& Node);
    virtual void Activate() override;
    virtual void TickTask(float DeltaTime) override;
protected:
    virtual void OnDestroy(bool bAbilityEnded) override;
private:
    bool ContextValid() const;
    void StopInvalid();
    void Sweep(float FromAlpha,float ToAlpha,const FTransform& CurrentTransform,const FVector& LocalStart,const FVector& LocalEnd);
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TWeakObjectPtr<AActor> Source;
    FGuid AttackId;
    FSovMeleeAttackNode Definition;
    FTransform PreviousTransform;
    FVector PreviousLocalStart,PreviousLocalEnd;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TSet<TWeakObjectPtr<UPrimitiveComponent>> EnvironmentContacts;
    float Elapsed=0.f;
    float StartedAt=0.f;
    bool bStopped=false;
};
