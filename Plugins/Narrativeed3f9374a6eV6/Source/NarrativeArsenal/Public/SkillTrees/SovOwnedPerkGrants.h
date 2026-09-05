// Copyright Narrative Tools 2024.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "SovOwnedPerkGrants.generated.h"

/** Native perk implementations report exact owned handles across reentrant grant callbacks. */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class NARRATIVEARSENAL_API USovOwnedPerkGrants : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVEARSENAL_API ISovOwnedPerkGrants
{
	GENERATED_BODY()
public:
	virtual void GetOwnedPerkGrants(TArray<FGameplayAbilitySpecHandle>& OutAbilities,
		TArray<FActiveGameplayEffectHandle>& OutEffects) const = 0;
};
