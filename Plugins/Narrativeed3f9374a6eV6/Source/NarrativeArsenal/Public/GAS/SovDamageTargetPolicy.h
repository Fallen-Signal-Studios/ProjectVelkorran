// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayEffectTypes.h"
#include "SovDamageTargetPolicy.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class USovDamageTargetPolicy : public UInterface
{
	GENERATED_BODY()
};

/**
 * The mirror of ISovDamageSourcePolicy, consulted on the actor being damaged rather than the one
 * dealing it, for rules that must hold whoever the attacker is.
 *
 * A source policy cannot express "this target cannot be killed yet": it would have to be implemented
 * on every possible attacker. Like the source policy, an implementation may only reduce resolved
 * damage, and the resolver clamps every result again afterwards.
 */
class NARRATIVEARSENAL_API ISovDamageTargetPolicy
{
	GENERATED_BODY()
public:
	/** Instigator may be null for environmental damage, which this policy still governs. */
	virtual bool LimitSovIncomingDamage(AActor* Instigator, const FGameplayEffectContextHandle& Context,
		float CurrentHealth, float& InOutShieldDamage, float& InOutHealthDamage, float& InOutPoiseDamage) const = 0;
};
