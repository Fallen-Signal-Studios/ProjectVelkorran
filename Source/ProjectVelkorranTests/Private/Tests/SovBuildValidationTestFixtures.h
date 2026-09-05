// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Sovereign/SovGameplayTags.h"
#include "SovBuildValidationTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovEchoValidationTestAbility : public USovGameplayAbility_EchoBase
{
	GENERATED_BODY()
public:
	USovEchoValidationTestAbility()
	{
		RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Tarrik;
		EchoSpendTag = FSovGameplayTags::Get().Echo_Source_WeakPointBreak;
		bRequiresAllowedWeapon = false;
	}
	void SetRawCost(float Value) { EchoCost = Value; }
	void SetRawThreshold(float Value) { MinimumEchoRequired = Value; }
	void RequireAuthoredWeapon(bool Value) { bRequiresAllowedWeapon = Value; }
	void SetTestIdentity(FGameplayTag Value) { RequiredCharacterTag = Value; }
	bool bPayloadConfigured = true;
protected:
	virtual bool HasRequiredPayloadConfiguration() const override { return bPayloadConfigured; }
};
