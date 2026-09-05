// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_AxiomNullPulse.generated.h"

/** Native-only damage shell. Axiom supplies Shield-only routing on the spec. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovGameplayEffect_AxiomNullPulseDamage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_AxiomNullPulseDamage();
};

/** Independent duration instances retain earlier, longer suppression grants. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovGameplayEffect_AxiomShieldSuppression : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_AxiomShieldSuppression();
};

/** Device shutdown is separate from Shield recharge and never deals damage. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovGameplayEffect_AxiomDeviceDisable : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_AxiomDeviceDisable();
};
