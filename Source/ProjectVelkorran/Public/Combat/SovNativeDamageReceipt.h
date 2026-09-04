// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/SovCombatTypes.h"
#include "SovNativeDamageReceipt.generated.h"

/** Synchronous typed result receipt uniquely identified by its effect source object. */
UCLASS()
class PROJECTVELKORRAN_API USovNativeDamageReceipt : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<AActor> ExpectedTarget;
	const FGameplayEffectContext* ExpectedContext = nullptr;
	bool bAcceptedControl = false;
	bool bAppliedDamage = false;
	bool bPoiseBroken = false;
	UFUNCTION()
	void ReceiveResult(const FSovDamageResult& Result);
};
