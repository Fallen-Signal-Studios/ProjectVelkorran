// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "Components/SovCommandLinkComponent.h"
#include "GAS/SovCombatTypes.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "SovAxiomRuntimeTestFixtures.generated.h"

class USovEchoComponent;
class USovDeflectionComponent;
class USovSeleneEchoGenerationComponent;

/** Content-free actor fixture. Uses the real Narrative ASC and damage attributes. */
UCLASS(Transient, NotBlueprintable)
class ASovAxiomRuntimeTestCharacter : public ANarrativeCharacter
{
	GENERATED_BODY()
public:
	ASovAxiomRuntimeTestCharacter(const FObjectInitializer& ObjectInitializer);
	void InitializeTestCombat(int32 InTeam);
	UWeaponItem* SetTestWeapon();
	void RemoveTestWeapon();
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual FGameplayTagContainer GetFactions() const override;
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;

	UPROPERTY()
	TObjectPtr<USovEchoComponent> TestEcho;
	UPROPERTY()
	TObjectPtr<USovDeflectionComponent> TestDeflection;
	UPROPERTY()
	TObjectPtr<USovSeleneEchoGenerationComponent> TestEchoGeneration;
	int32 TestTeam = 0;
	bool bTestNeutral = false;
	int32 ResolvedHitCount = 0;
	FSovDamageResult LastDamageResult;
	TWeakObjectPtr<USovGameplayAbility_SeleneAxiomNullPulse> ReentrantPulse;
	bool bReentrantReleaseAccepted = false;
	bool bCancelPulseOnDamage = false;
	FVector TestEyeOffset = FVector::ZeroVector;

	UFUNCTION()
	void RecordDamage(const FSovDamageResult& Result);
};

/** Concrete child with a real item-class allowlist, no Blueprint payload. */
UCLASS(Transient, NotBlueprintable)
class USovAxiomRuntimeTestAbility : public USovGameplayAbility_SeleneAxiomNullPulse
{
	GENERATED_BODY()
public:
	USovAxiomRuntimeTestAbility();
};

/** Supplies an authored ID without changing the production component contract. */
UCLASS(Transient, NotBlueprintable)
class USovAxiomRuntimeTestCommandLink : public USovCommandLinkComponent
{
	GENERATED_BODY()
public:
	USovAxiomRuntimeTestCommandLink();
};

/** An authored wielded item for the real source/wielded-weapon validation. */
UCLASS(Transient, NotBlueprintable)
class USovAxiomRuntimeTestWeapon : public UWeaponItem
{
	GENERATED_BODY()
public:
	USovAxiomRuntimeTestWeapon();
};
