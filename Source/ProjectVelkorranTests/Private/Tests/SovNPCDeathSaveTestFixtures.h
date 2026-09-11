// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "SovNPCDeathSaveTestFixtures.generated.h"

UCLASS()
class USovNPCDeathSaveObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 DeathTransitions = 0;
	int32 DamageResults = 0;
	UFUNCTION(CallInEditor) void ObserveDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead)
	{ if (bDead) { ++DeathTransitions; } }
	UFUNCTION(CallInEditor) void ObserveDamage(const FSovDamageResult& Result) { ++DamageResults; }
};
