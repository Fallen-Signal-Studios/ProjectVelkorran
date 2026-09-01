// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
	class USovTarrikEchoGenerationComponent* GetTarrikEchoGenerationComponent() const
	{
		return TarrikEchoGenerationComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovShieldComponent* GetShieldComponent() const { return ShieldComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovHealthRechargeComponent* GetHealthRechargeComponent() const { return HealthRechargeComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovPoiseComponent* GetPoiseComponent() const { return PoiseComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovGuardComponent* GetGuardComponent() const { return GuardComponent; }

	/** Exact project identity owned by the concrete protagonist class. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Identity")
	virtual FGameplayTag GetProtagonistIdentityTag() const;

	/**
	 * Called by the PreCMCTick component immediately before Character Movement updates.
	 * Blueprint subclasses can override this to update movement and rotation in the required tick order.
	 * Subclasses without an override safely use the generated no-op implementation.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Sovereign|Movement", meta = (DisplayName = "Sov Pre CMC Tick"))
	void SovPreCMCTick();

protected:
	virtual void HandleAbilitySystemReady(UNarrativeAbilitySystemComponent* ReadyAbilitySystem) override;
	virtual bool AreAdditionalCharacterSystemsReady() const override;
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovEchoComponent> EchoComponent;

	/**
	 * Optional compatibility slot owned only by ASovTarrikCharacter.
	 * It remains declared here so existing Blueprint getter/property references survive migration.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovTarrikEchoGenerationComponent> TarrikEchoGenerationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovShieldComponent> ShieldComponent;

	/** Player-only delayed Health recharge. NPC bases do not construct this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovHealthRechargeComponent> HealthRechargeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovPoiseComponent> PoiseComponent;

	/** Optional compatibility slot owned only by ASovTarrikCharacter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovGuardComponent> GuardComponent;
};
