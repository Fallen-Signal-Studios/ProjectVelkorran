// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "Weapons/SovTransformingWeaponVisual.h"
#include "Components/SovWeakPointComponent.h"
#include "SovEchoResourceTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovEchoHeavyTestAbility : public UNarrativeCombatAbility
{
	GENERATED_BODY()
public:
	USovEchoHeavyTestAbility();
	void Finish();
};
UCLASS(Transient, NotBlueprintable)
class USovEchoReadyTestSignature : public USovGameplayAbility_SeleneDispatch
{
	GENERATED_BODY()
public:
	USovEchoReadyTestSignature();
};

UCLASS(Transient, NotBlueprintable)
class USovEchoSummonedTestSignature : public USovGameplayAbility_SeleneDispatch
{
	GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable)
class ASovEchoVisualTestActor : public ASovTransformingWeaponVisual
{
	GENERATED_BODY()
public:
	void SetTestCharacter(ANarrativeCharacter* Character);
};

UCLASS(Transient, NotBlueprintable)
class USovEchoUnbrokenTestWeakPoint : public USovWeakPointComponent
{
	GENERATED_BODY()
public:
	USovEchoUnbrokenTestWeakPoint();
};

UCLASS(Transient, NotBlueprintable)
class USovEchoCallbackTestObserver : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<USovEchoHeavyTestAbility> SourceAbility;
	TWeakObjectPtr<USovEchoComponent> Echo;
	bool bSpendOnNextChange = false;
	UFUNCTION() void EndSource(const FSovDamageResult& Result);
	UFUNCTION() void HandleEchoChanged(float OldEcho, float NewEcho, float MaxEcho);
};

UCLASS(Transient, NotBlueprintable)
class USovEchoPeriodicTestEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovEchoPeriodicTestEffect();
};
