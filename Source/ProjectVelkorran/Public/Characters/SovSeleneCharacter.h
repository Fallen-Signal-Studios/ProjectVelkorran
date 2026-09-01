// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "SovSeleneCharacter.generated.h"

class UNarrativeAbilitySystemComponent;
class USovDeflectionComponent;
class USovSeleneEchoGenerationComponent;

/** Selene's concrete player class and boundary for precision/disruption systems. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovSeleneCharacter : public ASovPlayerCharacterBase
{
	GENERATED_BODY()

public:
	ASovSeleneCharacter(const FObjectInitializer& ObjectInitializer);

	virtual FGameplayTag GetProtagonistIdentityTag() const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	USovDeflectionComponent* GetDeflectionComponent() const
	{
		return DeflectionComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	USovSeleneEchoGenerationComponent* GetSeleneEchoGenerationComponent() const
	{
		return SeleneEchoGenerationComponent;
	}

protected:
	virtual void HandleAbilitySystemReady(
		UNarrativeAbilitySystemComponent* ReadyAbilitySystem) override;
	virtual bool AreAdditionalCharacterSystemsReady() const override;

	/** Selene-only, short precision-defense window. Never present on Tarrik. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<USovDeflectionComponent> DeflectionComponent;

	/** Converts Selene's authored precision results into the shared Echo resource. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<USovSeleneEchoGenerationComponent> SeleneEchoGenerationComponent;
};
