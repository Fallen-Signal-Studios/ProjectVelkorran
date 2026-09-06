// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "SovReadinessRuntimeTestFixtures.generated.h"

/** Uses the production ASC subscription and finalization after staging content prerequisites. */
UCLASS(Transient, NotBlueprintable)
class ASovReadinessRuntimeTestPawn : public ASovHandoffRuntimeTestPawn
{
	GENERATED_BODY()
public:
	ASovReadinessRuntimeTestPawn(const FObjectInitializer& Initializer) : Super(Initializer) {}
	void BindProductionReadiness() { TryInitializePlayerCharacter(); }
	void RetryReadiness(int32 Epoch) { HandleAbilitySystemReadyEpochChanged(Epoch); TryFinalizeCharacterReadiness(); }
	bool HasAuthorityReadyGate() const { return bAuthoritativeCharacterReady; }
};

UCLASS(Transient, NotBlueprintable)
class USovReadinessRuntimeProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ASC;
	TArray<int32> Epochs;
	int32 ReadyCount = 0;
	int32 ReadyTrueCount = 0;
	int32 ReadyFalseCount = 0;
	bool bObservedUncommittedEpoch = false;
	TFunction<void(int32)> OnEpoch;
	TFunction<void()> OnReady;
	TFunction<void(bool)> OnChanged;
	UFUNCTION() void EpochChanged(int32 Epoch)
	{
		Epochs.Add(Epoch);
		bObservedUncommittedEpoch |= !ASC || ASC->GetCharacterReadyEpoch() != Epoch;
		if (OnEpoch) { OnEpoch(Epoch); }
	}
	UFUNCTION() void Ready(ANarrativePlayerCharacter* Character)
	{
		++ReadyCount;
		if (OnReady) { OnReady(); }
	}
	UFUNCTION() void Changed(ANarrativePlayerCharacter* Character, bool bReady)
	{
		if (bReady) { ++ReadyTrueCount; } else { ++ReadyFalseCount; }
		if (OnChanged) { OnChanged(bReady); }
	}
};
