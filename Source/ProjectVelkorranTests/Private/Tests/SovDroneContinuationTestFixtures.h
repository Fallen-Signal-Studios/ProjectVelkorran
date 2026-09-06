// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Presentation/SovReformationDroneGunshotPresentation.h"
#include "Presentation/SovReformationDroneSelfDestructPresentation.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovDroneContinuationTestFixtures.generated.h"

/** Hooks replace authored event bodies, not the real GAS/release/timer implementation. */
struct FSovDroneContinuationHooks
{
	TFunction<void()> OnStarted;
	TFunction<void()> OnRelease;
	TFunction<void()> OnWarning;
	TFunction<void()> OnSpawned;
	TFunction<void()> OnDetonated;
	int32 Started = 0;
	int32 Releases = 0;
	int32 Warnings = 0;
	int32 Ends = 0;
	int32 Spawns = 0;
	int32 Detonations = 0;
	void Dispatch(UFunction* Function);
	void Spawned();
	void Detonated();
};

/** Exercises the real virtual spec-construction boundary, after the native spec exists. */
UCLASS(Transient, NotBlueprintable)
class USovDroneContinuationASC : public UNarrativeAbilitySystemComponent
{
	GENERATED_BODY()
public:
	mutable TFunction<void()> OnMakeSpec;
	mutable int32 SpecsBeforeCallback = 0;
	virtual FGameplayEffectSpecHandle MakeOutgoingSpec(TSubclassOf<UGameplayEffect> EffectClass,
		float Level, FGameplayEffectContextHandle Context) const override;
};

UCLASS(Transient, NotBlueprintable)
class ASovDroneContinuationCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovDroneContinuationCharacter(const FObjectInitializer& ObjectInitializer);
};

UCLASS(Transient, NotBlueprintable)
class USovDroneContinuationGun : public USovGameplayAbility_ReformationDroneGunfire
{
	GENERATED_BODY()
public:
	USovDroneContinuationGun();
	FSovDroneContinuationHooks Hooks;
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
	void UseAutomaticRelease() { bAutoReleasePayload = true; PayloadReleaseDelay = .1f; }
	void LockEnd() { ++ScopeLockCount; }
	void UnlockEnd();
	void RequestEnd() { CancelDroneWeaponAbility(); }
	bool CanContinueForTest() const { return CanContinueWeaponPayload(); }
	void InvalidEnd() { EndAbility(FGameplayAbilitySpecHandle(), CurrentActorInfo, CurrentActivationInfo, true, true); }
};

UCLASS(Transient, NotBlueprintable)
class USovDroneContinuationRocket : public USovGameplayAbility_ReformationDroneRocketLauncher
{
	GENERATED_BODY()
public:
	USovDroneContinuationRocket();
	FSovDroneContinuationHooks Hooks;
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
};

UCLASS(Transient, NotBlueprintable)
class USovDroneContinuationExploder : public USovGameplayAbility_ReformationDroneSelfDestruct
{
	GENERATED_BODY()
public:
	USovDroneContinuationExploder();
	FSovDroneContinuationHooks Hooks;
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
	void DiscardDamageConfiguration() { DamageEffectClass = nullptr; AbilityIdentityTag = FGameplayTag(); DamageChannels.Reset(); }
};

UCLASS(Transient, NotBlueprintable)
class ASovDroneContinuationGunPresentation : public ASovReformationDroneGunshotPresentation
{
	GENERATED_BODY()
public:
	virtual void OnConstruction(const FTransform& Transform) override;
};

UCLASS(Transient, NotBlueprintable)
class ASovDroneContinuationRocketProjectile : public ASovReformationDroneRocketProjectile
{
	GENERATED_BODY()
public:
	virtual void OnConstruction(const FTransform& Transform) override;
};

UCLASS(Transient, NotBlueprintable)
class ASovDroneContinuationBlastPresentation : public ASovReformationDroneSelfDestructPresentation
{
	GENERATED_BODY()
public:
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
};
