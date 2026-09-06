// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "GAS/SovCombatTypes.h"
#include "SovCombatInterruptionTestFixtures.generated.h"

class UNarrativeAbilitySystemComponent;

/** Authored values/callback mutations only. All release logic is production. */
UCLASS(Transient, NotBlueprintable)
class USovCombatDroneRocketTestAbility : public USovGameplayAbility_ReformationDroneRocketLauncher
{
	GENERATED_BODY()
public:
	USovCombatDroneRocketTestAbility();
	bool bDisableDuringRelease = false;
	bool bRestartDuringRelease = false;
	bool bRestartAccepted = false;
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
};

UCLASS(Transient, NotBlueprintable)
class USovCombatDroneGunTestAbility : public USovGameplayAbility_ReformationDroneGunfire
{
	GENERATED_BODY()
public:
	USovCombatDroneGunTestAbility();
};

UCLASS(Transient, NotBlueprintable)
class USovCombatDroneSuicideTestAbility : public USovGameplayAbility_ReformationDroneSelfDestruct
{
	GENERATED_BODY()
public:
	USovCombatDroneSuicideTestAbility();
};

UCLASS(Transient, NotBlueprintable)
class USovCombatJudgementTestAbility : public USovGameplayAbility_TarrikCinderJudgement
{
	GENERATED_BODY()
public:
	USovCombatJudgementTestAbility();
};

UCLASS(Transient, NotBlueprintable)
class USovCombatInterruptionDamageProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> SourceASC;
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> TargetASC;
	UPROPERTY() TObjectPtr<AActor> OriginalAvatar;
	UPROPERTY() TObjectPtr<AActor> ReplacementAvatar;
	bool bHealTarget = false;
	bool bRebindSource = false;
	bool bRebindTarget = false;
	bool bReviveTarget = false;
	bool bRestoreOriginalAvatar = false;
	bool bCancelSource = false;
	bool bArmed = true;
	UFUNCTION(CallInEditor) void ReceiveDamage(const FSovDamageResult& Result);
};
