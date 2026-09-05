// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayEffectTypes.h"
#include "SovDamageSourcePolicy.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class USovDamageSourcePolicy : public UInterface
{
	GENERATED_BODY()
};
/** Native component policy may only reduce resolved damage; the resolver clamps every result again. */
class NARRATIVEARSENAL_API ISovDamageSourcePolicy
{
	GENERATED_BODY()
public:
	virtual bool LimitSovDamage(AActor* Target, const FGameplayEffectContextHandle& Context,
		float& InOutShieldDamage, float& InOutHealthDamage, float& InOutPoiseDamage) const = 0;
};
