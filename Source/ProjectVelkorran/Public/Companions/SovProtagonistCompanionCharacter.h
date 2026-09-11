// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "GameplayAbilitySpecHandle.h"
#include "Campaign/SovEncounterTypes.h"
#include "SovProtagonistCompanionCharacter.generated.h"
class USovCompanionComponent;
class USovGuardComponent;
class USovDeflectionComponent;
class USovEchoComponent;
class USovShieldComponent;
class USovPoiseComponent;
class UNarrativeAbilitySystemComponent;
class UGameplayAbility;
class UCharacterMovementComponent;

USTRUCT()
struct FSovCompanionKitGrant
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TSubclassOf<UGameplayAbility> Ability;
	UPROPERTY(SaveGame) int32 Level = 1;
};

USTRUCT()
struct FSovCompanionProxySnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName MissionId;
	UPROPERTY(SaveGame) FGameplayTag Identity;
	UPROPERTY(SaveGame) FName CompanionId;
	UPROPERTY(SaveGame) TArray<FSovCompanionKitGrant> Grants;
	UPROPERTY(SaveGame) FSovCombatResourceSnapshot Resources;
	UPROPERTY(SaveGame) FNarrativeActorRecord ActorRecord;
};

/** A separate, mission-owned ASC for the non-controlled protagonist. No player ledger is mutated. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovProtagonistCompanionCharacter : public ASovNPCCharacterBase
{
	GENERATED_BODY()
public:
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Companions; }
	virtual bool ShouldSaveWorldRecord() const override { return false; }
	ASovProtagonistCompanionCharacter(const FObjectInitializer& Initializer);
	bool PrepareProxy(FGameplayTag Identity, FName CompanionId, const UNarrativeAbilitySystemComponent* OutgoingASC,
		const TArray<TSubclassOf<UGameplayAbility>>& CuratedClasses, FString& Reason);
	/** True only once visual/NPC initialization and the copied kit/resources have completed. */
	bool CompleteProxyInitialization();
	void SetProxyStaged(bool bStaged);
	bool CaptureProxySnapshot(FName MissionId, FSovCompanionProxySnapshot& Snapshot, FString& Reason);
	bool PrepareProxyFromSnapshot(const FSovCompanionProxySnapshot& Snapshot, const TArray<TSubclassOf<UGameplayAbility>>& Curated, FString& Reason);
	UFUNCTION(BlueprintPure, Category="Companion") FGameplayTag GetCompanionIdentity() const { return CompanionIdentity; }
	UFUNCTION(BlueprintPure, Category="Companion") USovCompanionComponent* GetCompanionComponent() const { return Companion; }
	virtual bool ShouldRespawn_Implementation() const override { return false; }
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetOwner(AActor* NewOwner) override;
protected:
	virtual void BeginPlay() override;
	virtual void OnCharacterVisualInitialized() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovCompanionComponent> Companion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovGuardComponent> Guard;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovDeflectionComponent> Deflection;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovEchoComponent> Echo;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovShieldComponent> Shield;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovPoiseComponent> Poise;
private:
	friend struct FSovConvergenceTestAccess;
	/** Intercepts only APawn's first engine owner assignment during a normal AI possession change. */
	TWeakObjectPtr<AActor> PossessionCampaignOwner;
	TWeakObjectPtr<AActor> PossessionEngineOwner;
	bool bPreserveNextPawnOwnerWrite = false;
	UPROPERTY() FGameplayTag CompanionIdentity;
	UPROPERTY(Transient) TArray<FSovCompanionKitGrant> CopiedGrants;
	TArray<FGameplayAbilitySpecHandle> OwnedKitHandles;
	float SavedHealth = 0.f, SavedMaxHealth = 0.f;
	float SavedShield = 0.f, SavedMaxShield = 0.f;
	float SavedStamina = 0.f, SavedMaxStamina = 0.f;
	float SavedEcho = 0.f, SavedMaxEcho = 0.f;
	float SavedPoise = 0.f, SavedMaxPoise = 0.f;
	bool bPrepared = false;
	bool bProxyInitialized = false;
	bool bApplyingKit = false;
	bool bStagedForTransition = true;
	/** The collision-free startup interval owns only this component's tick suspension. */
	TWeakObjectPtr<UCharacterMovementComponent> StagedMovement;
	bool bMovementTickWasEnabled = false;
	bool bMovementTickStartedEnabled = false;
	bool bMovementAutoUpdatedTick = false;
	uint64 ProxyStagingEpoch = 0;
};
