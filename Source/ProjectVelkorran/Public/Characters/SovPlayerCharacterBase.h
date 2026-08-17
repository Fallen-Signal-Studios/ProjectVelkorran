// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "SovPlayerCharacterBase.generated.h"

/** Project-owned player base that makes lifecycle components part of readiness. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerCharacterBase : public ANarrativePlayerCharacter
{
	GENERATED_BODY()

public:
	ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovEchoComponent* GetEchoComponent() const { return EchoComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovShieldComponent* GetShieldComponent() const { return ShieldComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovPoiseComponent* GetPoiseComponent() const { return PoiseComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovGuardComponent* GetGuardComponent() const { return GuardComponent; }

protected:
	virtual void HandleAbilitySystemReady(UNarrativeAbilitySystemComponent* ReadyAbilitySystem) override;
	virtual bool AreAdditionalCharacterSystemsReady() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovEchoComponent> EchoComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovShieldComponent> ShieldComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovPoiseComponent> PoiseComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovGuardComponent> GuardComponent;
};
