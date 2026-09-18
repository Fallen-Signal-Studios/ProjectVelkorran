// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "SovPlayerCharacterBase.generated.h"

/** Project-owned player base that makes lifecycle components part of readiness. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerCharacterBase : public ANarrativePlayerCharacter, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer);

	/** Campaign controller owns save/default inventory application before readiness. Set before possession. */
	bool PrepareCampaignInitialization(class UPlayerDefinition* Definition);
	bool IsCampaignDataReadyToApply() const;
	bool CompleteCampaignDataInitialization(bool bGrantDefaultInventory);
	void FailCampaignInitialization();
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual bool ShouldRespawn_Implementation() const override { return false; }
	/** Campaign PlayerData and protagonist snapshots explicitly own this pawn's record. */
	virtual bool ShouldSaveWorldRecord() const override { return false; }
	virtual bool ShouldResetAttributesOnRevive() const override { return false; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Components") class USovFatalRecoveryComponent* GetRecoveryComponent() const { return RecoveryComponent; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Components") class USovTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }
	/** The single owner of who is driving the camera. Systems claim it; the authored rig applies it. */
	UFUNCTION(BlueprintPure, Category="Sovereign|Components") class USovCameraControlComponent* GetCameraControlComponent() const { return CameraControlComponent; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovEchoComponent* GetEchoComponent() const { return EchoComponent; }

	UFUNCTION(BlueprintPure, Category="Sovereign|Components")
	class USovExertionComponent* GetExertionComponent() const { return ExertionComponent; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Components")
	class USovFieldRecoveryComponent* GetFieldRecoveryComponent() const { return FieldRecoveryComponent; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Components")
	class USovResonanceComponent* GetResonanceComponent() const { return ResonanceComponent; }

	/** Dormant until an explicitly mission-permitted source makes real authority contact. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovCorruptionComponent* GetCorruptionComponent() const { return CorruptionComponent; }

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
	class USovStatusComponent* GetStatusComponent() const { return StatusComponent; }

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
	virtual void OnCharacterVisualInitialized() override;
	virtual class UNarrativeSaveWithCreatorData* GetCharacterCreatorData() const override;
	virtual void HandleAbilitySystemReady(UNarrativeAbilitySystemComponent* ReadyAbilitySystem) override;
	virtual bool AreAdditionalCharacterSystemsReady() const override;
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovEchoComponent> EchoComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sovereign|Components") TObjectPtr<class USovExertionComponent> ExertionComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sovereign|Components") TObjectPtr<class USovFieldRecoveryComponent> FieldRecoveryComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sovereign|Components") TObjectPtr<class USovResonanceComponent> ResonanceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovCorruptionComponent> CorruptionComponent;

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

	/** Authoritative transient combat-status owner shared by both protagonists. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovStatusComponent> StatusComponent;

	/** Optional compatibility slot owned only by ASovTarrikCharacter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovGuardComponent> GuardComponent;

private:
	UPROPERTY(VisibleAnywhere, Category="Sovereign|Components") TObjectPtr<class USovFatalRecoveryComponent> RecoveryComponent;
	UPROPERTY(VisibleAnywhere, Category="Sovereign|Components") TObjectPtr<class USovTargetingComponent> TargetingComponent;
	UPROPERTY(VisibleAnywhere, Category="Sovereign|Components") TObjectPtr<class USovCameraControlComponent> CameraControlComponent;
	UPROPERTY(SaveGame) FGuid CampaignSaveGuid;
	bool bCampaignManagedInitialization = false;
	bool bCampaignInitializationFailed = false;
};
