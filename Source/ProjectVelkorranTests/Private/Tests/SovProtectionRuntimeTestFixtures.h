// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "SovProtectionRuntimeTestFixtures.generated.h"
class ASovAxiomRuntimeTestCharacter;

/** Authored timing only: the tested trace, receipt and damage producer remain native. */
UCLASS(Transient, NotBlueprintable)
class USovProtectionRuntimeGunfire : public USovGameplayAbility_ReformationDroneGunfire
{
	GENERATED_BODY()
public:
	USovProtectionRuntimeGunfire();
	void Finish();
	void SetBurstSize(int32 Count) { BurstShotCount = Count; }
};

UCLASS(Transient, NotBlueprintable)
class USovProtectionRuntimeObserver : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ASovAxiomRuntimeTestCharacter> Source;
	TWeakObjectPtr<ASovAxiomRuntimeTestCharacter> Target;
	TWeakObjectPtr<USovProtectionRuntimeGunfire> CancelOnAward;
	bool bNestOnNextTargetResult = false;
	int32 ProtectionAwardCount = 0;
	float ProtectionAwardTotal = 0.f;
	TArray<FSovDamageResult> SourceResults;
	UFUNCTION() void TargetResolved(const FSovDamageResult& Result);
	UFUNCTION() void SourceResolved(const FSovDamageResult& Result);
	UFUNCTION() void EchoAwarded(float Amount, float NewEcho, ESovTarrikEchoAwardType Type, AActor* Protected);
};
