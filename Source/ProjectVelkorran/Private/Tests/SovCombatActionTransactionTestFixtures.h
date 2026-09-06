// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovCombatActionTransactionTestFixtures.generated.h"

class UNarrativeAbilitySystemComponent;

/** Authored values and callback probes only; payment and lifecycle remain production. */
UCLASS(Transient, NotBlueprintable)
class USovCombatActionTransactionEchoAbility : public USovGameplayAbility_EchoBase
{
    GENERATED_BODY()
public:
    USovCombatActionTransactionEchoAbility();
    int32 StartedCount=0;
    int32 CommittedCount=0;
    int32 EndedCount=0;
    bool bRestartDuringStarted=false;
    bool bRestartAccepted=false;
    virtual void ProcessEvent(UFunction* Function,void* Parameters) override;
    void LockEndForTest() { ++ScopeLockCount; }
    void RequestEndForTest() { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true); }
    void UnlockEndForTest();
    bool OwnsExecutionForTest() const { return IsCurrentEchoExecutionValid(); }
};

enum class ESovCombatActionDebitMutation : uint8 { Cancel, ReplaceAvatar, RestoreOriginalAvatar, Freeze, ZeroHealth, RestoreLife };

UCLASS(Transient, NotBlueprintable)
class USovCombatActionTransactionProbe : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ASC;
    UPROPERTY() TObjectPtr<AActor> ReplacementAvatar;
    FGameplayAbilitySpecHandle Handle;
    ESovCombatActionDebitMutation Mutation=ESovCombatActionDebitMutation::Cancel;
    bool bArmed=true;
    UFUNCTION(CallInEditor) void DuringEchoDebit(float OldEcho,float NewEcho,float Maximum);
};

UCLASS(Transient, NotBlueprintable)
class USovCombatActionTransactionMeleeAbility : public USovMeleeRuntimeTestAbility
{
    GENERATED_BODY()
public:
    mutable bool bRestartDuringMesh=true;
    mutable bool bRestartAccepted=false;
    int32 NodeStartedCount=0;
    virtual USkeletalMeshComponent* ResolveMeleeTraceMesh_Implementation() const override;
    virtual void ProcessEvent(UFunction* Function,void* Parameters) override;
};

UCLASS(Transient, NotBlueprintable)
class ASovCombatActionTransactionTeamCharacter : public ASovAxiomRuntimeTestCharacter
{
    GENERATED_BODY()
public:
    ASovCombatActionTransactionTeamCharacter(const FObjectInitializer& Initializer):Super(Initializer) {}
    FGameplayAbilitySpecHandle Handle;
    mutable bool bCancelDuringAttitude=false;
    mutable bool bQueriedAttitude=false;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};
