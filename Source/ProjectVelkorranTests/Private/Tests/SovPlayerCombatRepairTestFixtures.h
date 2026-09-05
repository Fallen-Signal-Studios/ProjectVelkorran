// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Abilities/SovGameplayAbility_Finisher.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "Items/NarrativeItem.h"
#include "Items/RangedWeaponItem.h"
#include "GAS/SovCombatTypes.h"
#include "SovPlayerCombatRepairTestFixtures.generated.h"
class UNarrativeAbilitySystemComponent;

/** Only authored configuration changes; all debit, ownership and collision paths are production. */
UCLASS(Transient, NotBlueprintable)
class USovRepairAmmo : public UNarrativeItem
{
    GENERATED_BODY()
public:
    USovRepairAmmo();
    bool bAllowRemoval=true;
    virtual bool CanBeRemoved_Implementation() const override { return bAllowRemoval; }
};
UCLASS(Transient, NotBlueprintable)
class USovRepairWeapon : public URangedWeaponItem
{
    GENERATED_BODY()
public:
    USovRepairWeapon();
    void SetLoaded(int32 Value) { WeaponClipState.AmmoInClip=Value; MarkDirtyForReplication(); }
    int32 RawLoaded() const { return WeaponClipState.AmmoInClip; }
};
UCLASS(Transient, NotBlueprintable)
class USovRepairEchoAbility : public USovGameplayAbility_EchoBase
{
    GENERATED_BODY()
public:
    USovRepairEchoAbility();
    int32 StartedCount=0;
    virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
};
UCLASS(Transient, NotBlueprintable)
class USovRepairJudgementAbility : public USovGameplayAbility_TarrikCinderJudgement
{
    GENERATED_BODY()
public:
    USovRepairJudgementAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovRepairStaccatoAbility : public USovGameplayAbility_SeleneStaccatoZero
{
    GENERATED_BODY()
public:
    void SetRetiredDamageOverride(TSubclassOf<UGameplayEffect> Class) { EmpoweredShotDamageEffectClass=Class; }
};
UCLASS(Transient, NotBlueprintable)
class USovPlayerCombatRepairProbe : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ASC;
    UPROPERTY() TObjectPtr<USovRepairWeapon> Weapon;
    UPROPERTY() TObjectPtr<USovGameplayAbility_TarrikCinderJudgement> Judgement;
    FGameplayAbilitySpecHandle Handle;
    bool bArmed=true;
    bool bNestedConsumeAccepted=false;
    bool bNestedReloadAccepted=false;
    bool bReactivationAccepted=false;
    bool bReactivateJudgement=false;
    UFUNCTION() void DuringAmmoMutation();
    UFUNCTION() void DuringEchoDebit(float OldEcho,float NewEcho,float Maximum);
    UFUNCTION() void DuringJudgementDamage(const FSovDamageResult& Result);
};

/** A real team-policy callback reenters GAS during target selection/reservation. */
UCLASS(Transient, NotBlueprintable)
class ASovRepairFinisherCharacter : public ASovAxiomRuntimeTestCharacter
{
    GENERATED_BODY()
public:
    ASovRepairFinisherCharacter(const FObjectInitializer& Initializer):Super(Initializer) {}
    FGameplayAbilitySpecHandle FinisherHandle;
    mutable int32 ReenterOnAttitudeCall=0;
    mutable bool bRestarted=false;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};
UCLASS(Transient, NotBlueprintable)
class USovRepairFinisherAbility : public USovGameplayAbility_Finisher
{
    GENERATED_BODY()
public:
    void LockEndForTest() { ++ScopeLockCount; }
    void RequestEndForTest() { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true); }
    void UnlockEndForTest();
};
