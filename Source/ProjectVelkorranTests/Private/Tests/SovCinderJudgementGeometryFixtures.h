// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovCinderJudgementGeometryFixtures.generated.h"

/** Authored test values only. GAS activation, payment and traces are production. */
UCLASS(Transient, NotBlueprintable)
class USovCinderJudgementGeometryAbility : public USovGameplayAbility_TarrikCinderJudgement
{
	GENERATED_BODY()
public:
	USovCinderJudgementGeometryAbility();
	void SetMuzzleForTest(FVector Offset) { FallbackMuzzleOffset = Offset; }
	void SetRadiusForTest(float Radius) { TraceRadius = Radius; }
	void SetMaximumRangeBlastForTest(bool bEnabled) { bExplodeAtMaximumRange = bEnabled; }
	FGameplayAbilitySpecHandle SpecForTest() const { return CurrentSpecHandle; }
	UPROPERTY() TObjectPtr<AWeaponVisual> TestVisual;
	virtual AWeaponVisual* GetAbilityWeaponVisual() const override;
};

/** Socket provider only; production validates and selects socket vs fallback. */
UCLASS(Transient, NotBlueprintable)
class USovCinderJudgementGeometryMesh : public USkeletalMeshComponent
{
	GENERATED_BODY()
public:
	FTransform TestSocket = FTransform::Identity;
	virtual bool DoesSocketExist(FName Name) const override { return Name == TEXT("Muzzle"); }
	virtual FTransform GetSocketTransform(FName Name, ERelativeTransformSpace Space = RTS_World) const override
	{ return TestSocket; }
};

UCLASS(Transient, NotBlueprintable)
class ASovCinderJudgementGeometryCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovCinderJudgementGeometryCharacter(const FObjectInitializer& Initializer) : Super(Initializer) {}
	bool bInvalidEye = false;
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
};

enum class ESovJudgementDamageMutation : uint8 { Cancel, Restart, AvatarABA, LifeABA };

/** Synchronous damage observer; production source ownership guards must retire old work. */
UCLASS(Transient, NotBlueprintable)
class USovCinderJudgementDamageProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> SourceASC;
	UPROPERTY() TObjectPtr<AActor> Replacement;
	FGameplayAbilitySpecHandle Handle;
	ESovJudgementDamageMutation Mutation = ESovJudgementDamageMutation::Cancel;
	int32 TriggerAfterHit = 1;
	int32 HitCount = 0;
	bool bArmed = true;
	bool bRestartAccepted = false;
	UFUNCTION(CallInEditor) void DuringDamage(const FSovDamageResult& Result);
};
