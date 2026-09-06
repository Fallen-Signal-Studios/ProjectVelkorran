// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "SovTarrikCharacter.generated.h"

/** Tarrik's concrete player class and owner of his pressure/guard systems. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovTarrikCharacter : public ASovPlayerCharacterBase
{
	GENERATED_BODY()

public:
	ASovTarrikCharacter(const FObjectInitializer& ObjectInitializer);

	virtual FGameplayTag GetProtagonistIdentityTag() const override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void HandleAbilitySystemReady(
		UNarrativeAbilitySystemComponent* ReadyAbilitySystem) override;
	virtual bool AreAdditionalCharacterSystemsReady() const override;
	virtual void OnRep_WieldState(const FWeaponWieldState& OldWieldState) override;
};
